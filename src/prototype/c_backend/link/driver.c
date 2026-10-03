#define _XOPEN_SOURCE 700
#include "plan.h"
#include "../lower/scalar.h"
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PG_C_BACKEND_DIRECTORY
#error PG_C_BACKEND_DIRECTORY_must_name_the_runtime_source_directory
#endif

static char *path_join(const char *directory, const char *name)
{
	size_t a = strlen(directory), b = strlen(name);
	if (a > SIZE_MAX - b - 2) return NULL;
	char *path = malloc(a + b + 2);
	if (path) snprintf(path, a + b + 2, "%s/%s", directory, name);
	return path;
}

static int copy_runtime(const char *name, const char *destination)
{
	char *source = path_join(PG_C_BACKEND_DIRECTORY, name);
	if (!source) return -1;
	FILE *in = fopen(source, "rb"), *out = NULL;
	free(source);
	int status = -1;
	if (!in) return -1;
	out = fopen(destination, "wb");
	if (!out) goto done;
	unsigned char buffer[8192];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), in)))
		if (fwrite(buffer, 1, n, out) != n) goto done;
	status = ferror(in) ? -1 : 0;
done:
	if (out && fclose(out)) status = -1;
	fclose(in);
	return status;
}

static int run(char *const *arguments)
{
	pid_t process = fork();
	if (process < 0) return -1;
	if (!process) {
		execvp(arguments[0], arguments);
		perror(arguments[0]);
		_exit(127);
	}
	int status;
	while (waitpid(process, &status, 0) < 0) if (errno != EINTR) return -1;
	if (WIFEXITED(status) && !WEXITSTATUS(status)) return 0;
	fprintf(stderr, "C link: native tool failed: %s\n", arguments[0]);
	return -1;
}

static void json_string(FILE *file, const char *value)
{
	if (!value) { fputs("null", file); return; }
	fputc('"', file);
	for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
		if (*p == '"' || *p == '\\') fputc('\\', file);
		if (*p < 32) fprintf(file, "\\u%04x", *p);
		else fputc(*p, file);
	}
	fputc('"', file);
}

/* Public names belong to the selected target ABI. Unmatched helper names do
	* not add definitions; local:* hides runtime and private implementation names. */
static int shared_symbols(const char *path, const struct pg_c_link_plan *plan)
{
	FILE *file = fopen(path, "w");
	if (!file) return -1;
	fputs("{\n\tglobal:\n", file);
	for (size_t i = 0; i < plan->count; ++i)
		fprintf(file, "\t\tap_export_%s;\n", plan->exports[i].alias);
	if (plan->lowering == PG_C_NATIVE_DIRECT) {
		for (size_t i = 0; i < plan->natural_count; ++i)
			fprintf(file, "\t\tap_arena_%s_destroy;\n", plan->naturals[i].alias);
		for (size_t i = 0; i < plan->data_count; ++i) {
			const char *alias = plan->data[i].alias;
			fprintf(file, "\t\tap_arena_%s_destroy;\n\t\tap_from_%s;\n\t\tap_copy_%s;\n", alias, alias, alias);
		}
	}
	fputs("\tlocal: *;\n};\n", file);
	int status = ferror(file) ? -1 : 0;
	if (fclose(file)) status = -1;
	return status;
}

static int receipt(const char *path, const struct pg_c_link_plan *plan, const char *script,
	const char *cc, const char *ar, int trusted, uint64_t spent, const struct pg_c_native_contract *contract)
{
	int recursive = contract->recursive;
	FILE *file = fopen(path, "w");
	if (!file) return -1;
	static const char *const products[] = {"source", "object", "archive", "executable", "shared"};
	fputs("{\n  \"artifact\": ", file); json_string(file, plan->artifact);
	fputs(",\n  \"link_script\": ", file); json_string(file, script);
	fputs(",\n  \"native_script\": ", file); json_string(file, plan->native_script);
	fputs(",\n  \"product\": ", file); json_string(file, products[plan->product]);
	if (plan->product == PG_C_SHARED)
		fputs(",\n  \"shared_library\":\"library.so\",\n  \"shared_visibility\":\"selected-public-api\",\n  \"shared_toolchain\":\"elf-version-script\"", file);
	fputs(",\n  \"target\": \"host-c11\",\n  \"abi\": ", file); json_string(file, pg_c_abi_name(plan->lowering));
	fputs(",\n  \"lowering\": ", file); json_string(file, pg_c_lowering_name(plan->lowering));
	fputs(",\n  \"fallback\": \"reject\",\n  \"runtime_abi\": ", file);
	fputs(plan->lowering == PG_C_STRUCTURAL ? "2" : "null", file);
	fputs(",\n  \"transformations\": ", file);
	if (plan->lowering == PG_C_STRUCTURAL) fputs("[\"structural-closures\"]", file);
	else {
		fputs("[\"fixed-width-arithmetic\",\"pure-sequencing\",\"shared-direct-calls\",\"capture-lifting\"", file);
		if (plan->enum_count) fputs(",\"nullary-enum32\",\"conditional-match\"", file);
		if (contract->natural) fputs(",\"checked-nat32\",\"conditional-match\"", file);
		if (plan->data_count) fputs(",\"fieldful-tagged-values\",\"conditional-match\"", file);
		if (contract->value_fields) fputs(",\"nested-value-fields\"", file);
		if (recursive) fputs(",\"single-tail-nodes\",\"direct-recursive-match\",\"known-ih-thunks\",\"arena-construction\"", file);
		if (contract->copy_out) fputs(",\"finite-list-copy-out\",\"array-to-list-copy\"", file);
		fputc(']', file);
	}
	fputs(",\n  \"cc\": ", file);
	json_string(file, cc);
	fputs(",\n  \"ar\": ", file); json_string(file, ar);
	fprintf(file, ",\n  \"admission\": \"%s\",\n  \"validation_steps\": %" PRIu64 ",\n  \"exports\": [\n",
		trusted ? "user-trusted-unauthenticated" : "ordinary-solve", spent);
	for (size_t i = 0; i < plan->count; ++i) {
		fputs("    {\"source\": ", file); json_string(file, plan->names[i]);
		fprintf(file, ", \"symbol\": \"ap_export_%s\"}%s\n", plan->exports[i].alias, i + 1 < plan->count ? "," : "");
	}
	fputs("  ],\n  \"enum32\": [", file);
	for (size_t i = 0; i < plan->enum_count; ++i) {
		if (i) fputs(", ", file);
		fputs("{\"source\": ", file); json_string(file, plan->enum_names[i]);
		fputs(", \"alias\": ", file); json_string(file, plan->enums[i].alias); fputc('}', file);
	}
	fputs("],\n  \"data\": [", file);
	for (size_t i = 0; i < plan->data_count; ++i) {
		if (i) fputs(", ", file);
		fputs("{\"source\": ", file); json_string(file, plan->data_names[i]);
		fputs(", \"alias\": ", file); json_string(file, plan->data[i].alias);
		fputs(", \"selection\": ", file);
		json_string(file, plan->data_from_value && plan->data_from_value[i] ? "value-classifier" : "value-type");
		fputc('}', file);
	}
	fputs("],\n  \"data_contract\": ", file);
	if (plan->data_count) {
		fputs("{\"ownership\":", file);
		json_string(file, recursive ? "borrowed-inputs-and-caller-arena" : "value-copy");
		fputs(",\"fields\":\"int32-int64-selected-enum32", file);
		if (contract->natural) fputs("-selected-nat32", file);
		if (contract->value_fields) fputs("-selected-value-data", file);
		if (recursive) fputs("-single-self-tail", file);
		fputs("\",\"invalid_input\":2,", file);
		fputs(recursive ? "\"allocation_failure\":3,\"depth_limit\":4,\"failure_output\":\"unchanged\",\"allocation_rollback\":true}" :
			"\"invalid_input_output\":\"unchanged\"}", file);
	} else fputs("null", file);
	fputs(",\n  \"nat32\": [", file);
	for (size_t i = 0; i < plan->natural_count; ++i) {
		if (i) fputs(", ", file);
		fputs("{\"source\": ", file); json_string(file, plan->natural_names[i]);
		fputs(", \"alias\": ", file); json_string(file, plan->naturals[i].alias); fputc('}', file);
	}
	fputs("],\n  \"natural_overflow\": ", file); fputs(contract->natural ? "5" : "null", file);
	fputs(",\n  \"natural_depth_limit\": ", file); fputs(contract->natural ? "4" : "null", file);
	fputs(",\n  \"list_copy_out\": ", file);
	fputs(contract->copy_out ? "{\"capacity_failure\":6,\"failure_buffer\":\"unchanged\",\"failure_length\":\"unchanged\",\"overlap\":\"forbidden\"}" : "null", file);
	fputs(",\n  \"list_copy_in\": ", file);
	fputs(contract->copy_out ? "{\"ownership\":\"arena\",\"allocation_failure\":3,\"length_overflow\":6,\"failure_output\":\"unchanged\",\"failure_allocations\":\"rollback-new\",\"overlap\":\"forbidden\"}" : "null", file);
	fputs(",\n  \"entry\": ", file);
	json_string(file, plan->entry == SIZE_MAX ? NULL : plan->exports[plan->entry].alias);
	fputs("\n}\n", file);
	int status = ferror(file) ? -1 : 0;
	if (fclose(file)) status = -1;
	return status;
}

int pg_c_link_publish(const struct pg_c_link_plan *plan, const char *directory,
	const char *cc, const char *ar, const char *script, int trusted, uint64_t spent)
{
	struct stat existing;
	if (!lstat(directory, &existing) || errno != ENOENT) {
		fputs("C link: output directory must not already exist\n", stderr);
		return 2;
	}
	size_t length = strlen(directory);
	if (length > SIZE_MAX - 12) return 2;
	char *staging = malloc(length + 12);
	if (!staging) return 2;
	snprintf(staging, length + 12, "%s.tmp.XXXXXX", directory);
	if (!mkdtemp(staging)) { free(staging); return 2; }
	/* Native tools must receive file operands, even for a relative output name
	 * beginning with '-'. Resolve only downstream paths, never the input graph. */
	char *absolute = realpath(staging, NULL);
	if (!absolute) { rmdir(staging); free(staging); return 2; }
	free(staging); staging = absolute;
	enum { SOURCE, HEADER, RECEIPT, RUNTIME, RUNTIME_HEADER, OBJECT, RUNTIME_OBJECT, ARCHIVE, EXECUTABLE, SHARED, SYMBOLS, FILES };
	static const char *const names[] = {"component.c", "component.h", "link.json", "runtime.c", "runtime.h",
		"component.o", "runtime.o", "library.a", "program", "library.so", "symbols.map"};
	char *paths[FILES] = {0};
	int status = 2;
	for (size_t i = 0; i < FILES; ++i) if (!(paths[i] = path_join(staging, names[i]))) goto done;
	FILE *file = fopen(paths[SOURCE], "w");
	if (!file) goto done;
	FILE *header = fopen(paths[HEADER], "w");
	if (!header) { fclose(file); goto done; }
	const char *error;
	int runtime = plan->lowering == PG_C_STRUCTURAL;
	int emitted;
	struct pg_c_native_contract contract = {0};
	if (runtime) {
		emitted = pg_c_emit_exports(file, plan->count, plan->exports, plan->entry, &error);
		if (!emitted) emitted = pg_c_emit_header(header, plan->count, plan->exports);
	} else if (plan->lowering == PG_C_NATIVE_DIRECT) {
		emitted = pg_c_emit_native_profile(file, header, plan->count, plan->exports, plan->entry,
			plan->enum_count, plan->enums, plan->natural_count, plan->naturals,
			plan->data_count, plan->data, &contract, &error);
	} else emitted = pg_c_emit_scalar(file, header, plan->count, plan->exports, plan->entry, &error);
	/* Save stream errors before closing handles; they are I/O failures even
	 * when the emitter also returns its unsupported-lowering sentinel. */
	int failed = ferror(file) || ferror(header);
	int closed = fclose(file), header_closed = fclose(header);
	if (failed || closed || header_closed) goto done;
	if (emitted) { fprintf(stderr, "C link: cannot lower exports: %s\n", error); status = 4; goto done; }
	if (runtime && (copy_runtime(names[RUNTIME], paths[RUNTIME]) || copy_runtime(names[RUNTIME_HEADER], paths[RUNTIME_HEADER]))) goto done;
	if (plan->product != PG_C_SOURCE) {
		char *compile[] = {(char *)cc, "-std=c11", "-O2", "-c", paths[SOURCE], "-o", paths[OBJECT],
			plan->product == PG_C_SHARED ? "-fPIC" : NULL, NULL};
		if (run(compile)) goto done;
		if (runtime) {
			compile[4] = paths[RUNTIME]; compile[6] = paths[RUNTIME_OBJECT];
			if (run(compile)) goto done;
		}
	}
	if (plan->product == PG_C_ARCHIVE) {
		char *archive[] = {(char *)ar, "rcs", paths[ARCHIVE], paths[OBJECT], runtime ? paths[RUNTIME_OBJECT] : NULL, NULL};
		if (run(archive)) goto done;
	}
	if (plan->product == PG_C_SHARED) {
		if (shared_symbols(paths[SYMBOLS], plan)) goto done;
		char *link[] = {(char *)cc, "-shared", paths[OBJECT], "-o", paths[SHARED],
			"-Xlinker", "--version-script", "-Xlinker", paths[SYMBOLS],
			runtime ? paths[RUNTIME_OBJECT] : NULL, NULL};
		if (run(link)) goto done;
	}
	if (plan->product == PG_C_EXECUTABLE) {
		char *link[10] = {(char *)cc, paths[OBJECT]};
		size_t n = 2;
		if (runtime) link[n++] = paths[RUNTIME_OBJECT];
		link[n++] = "-o"; link[n++] = paths[EXECUTABLE];
		char *native = NULL;
		if (plan->native_script) {
			native = realpath(plan->native_script, NULL);
			if (!native) { perror(plan->native_script); goto done; }
			link[n++] = "-Xlinker"; link[n++] = "-T"; link[n++] = "-Xlinker"; link[n++] = native;
		}
		int linked = run(link);
		free(native);
		if (linked) goto done;
	}
	if (receipt(paths[RECEIPT], plan, script, cc, ar, trusted, spent, &contract)) goto done;
	/* Do not overwrite an existing product. Like the other CLI publications,
	 * this command requires exclusive ownership of its output path. */
	if (!lstat(directory, &existing) || errno != ENOENT) goto done;
	if (rename(staging, directory)) goto done;
	status = 0;
done:
	if (status == 2) fprintf(stderr, "C link: could not publish %s\n", directory);
	for (size_t i = 0; i < FILES; ++i) {
		if (status && paths[i]) unlink(paths[i]);
		free(paths[i]);
	}
	if (status) rmdir(staging);
	free(staging);
	return status;
}
