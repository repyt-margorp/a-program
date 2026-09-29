#define _XOPEN_SOURCE 700
#include "plan.h"
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

static int receipt(const char *path, const struct pg_c_link_plan *plan, const char *script,
	const char *cc, const char *ar, int trusted, uint64_t spent)
{
	FILE *file = fopen(path, "w");
	if (!file) return -1;
	static const char *const products[] = {"source", "object", "archive", "executable"};
	fputs("{\n  \"artifact\": ", file); json_string(file, plan->artifact);
	fputs(",\n  \"link_script\": ", file); json_string(file, script);
	fputs(",\n  \"native_script\": ", file); json_string(file, plan->native_script);
	fputs(",\n  \"product\": ", file); json_string(file, products[plan->product]);
	fputs(",\n  \"target\": \"host-c11\",\n  \"abi\": \"isolated_v1\",\n  \"runtime_abi\": 2,\n  \"cc\": ", file);
	json_string(file, cc);
	fputs(",\n  \"ar\": ", file); json_string(file, ar);
	fprintf(file, ",\n  \"admission\": \"%s\",\n  \"validation_steps\": %" PRIu64 ",\n  \"exports\": [\n",
		trusted ? "user-trusted-unauthenticated" : "ordinary-solve", spent);
	for (size_t i = 0; i < plan->count; ++i) {
		fputs("    {\"source\": ", file); json_string(file, plan->names[i]);
		fprintf(file, ", \"symbol\": \"ap_export_%s\"}%s\n", plan->exports[i].alias, i + 1 < plan->count ? "," : "");
	}
	fputs("  ],\n  \"entry\": ", file);
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
	enum { SOURCE, HEADER, RECEIPT, RUNTIME, RUNTIME_HEADER, OBJECT, RUNTIME_OBJECT, ARCHIVE, EXECUTABLE, FILES };
	static const char *const names[] = {"component.c", "component.h", "link.json", "runtime.c", "runtime.h",
		"component.o", "runtime.o", "library.a", "program"};
	char *paths[FILES] = {0};
	int status = 2;
	for (size_t i = 0; i < FILES; ++i) if (!(paths[i] = path_join(staging, names[i]))) goto done;
	FILE *file = fopen(paths[SOURCE], "w");
	if (!file) goto done;
	const char *error;
	int emitted = pg_c_emit_exports(file, plan->count, plan->exports, plan->entry, &error);
	int closed = fclose(file);
	if (emitted) { fprintf(stderr, "C link: cannot lower exports: %s\n", error); status = 4; goto done; }
	if (closed) goto done;
	file = fopen(paths[HEADER], "w");
	if (!file) goto done;
	emitted = pg_c_emit_header(file, plan->count, plan->exports);
	closed = fclose(file);
	if (emitted || closed) goto done;
	if (copy_runtime(names[RUNTIME], paths[RUNTIME]) || copy_runtime(names[RUNTIME_HEADER], paths[RUNTIME_HEADER])) goto done;
	if (plan->product != PG_C_SOURCE) {
		char *compile[] = {(char *)cc, "-std=c11", "-O2", "-c", paths[SOURCE], "-o", paths[OBJECT], NULL};
		if (run(compile)) goto done;
		compile[4] = paths[RUNTIME]; compile[6] = paths[RUNTIME_OBJECT];
		if (run(compile)) goto done;
	}
	if (plan->product == PG_C_ARCHIVE) {
		char *archive[] = {(char *)ar, "rcs", paths[ARCHIVE], paths[OBJECT], paths[RUNTIME_OBJECT], NULL};
		if (run(archive)) goto done;
	}
	if (plan->product == PG_C_EXECUTABLE) {
		char *link[] = {(char *)cc, paths[OBJECT], paths[RUNTIME_OBJECT], "-o", paths[EXECUTABLE], NULL, NULL, NULL, NULL, NULL};
		char *native = NULL;
		if (plan->native_script) {
			native = realpath(plan->native_script, NULL);
			if (!native) { perror(plan->native_script); goto done; }
			link[5] = "-Xlinker"; link[6] = "-T"; link[7] = "-Xlinker"; link[8] = native;
		}
		int linked = run(link);
		free(native);
		if (linked) goto done;
	}
	if (receipt(paths[RECEIPT], plan, script, cc, ar, trusted, spent)) goto done;
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
