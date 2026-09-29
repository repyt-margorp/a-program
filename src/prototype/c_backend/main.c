#define _POSIX_C_SOURCE 200809L
#include "emit.h"
#include "link/plan.h"
#include "artifact/file.h"
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int number(const char *text, uint64_t *result)
{
	if (!text || *text < '0' || *text > '9') return -1;
	char *end;
	errno = 0;
	uintmax_t value = strtoumax(text, &end, 10);
	if (errno || *end || value > UINT64_MAX) return -1;
	*result = value;
	return 0;
}

static int publish(const char *path, const struct pg_c_link_plan *plan)
{
	size_t length = strlen(path);
	if (length > SIZE_MAX - 12) return 2;
	char *temporary = malloc(length + 12);
	if (!temporary) return 2;
	snprintf(temporary, length + 12, "%s.tmp.XXXXXX", path);
	int fd = mkstemp(temporary), status = 2;
	if (fd < 0) goto done;
	FILE *output = fdopen(fd, "w");
	if (!output) { close(fd); goto done; }
	const char *error;
	if (pg_c_emit_exports(output, plan->count, plan->exports, plan->entry, &error)) {
		fprintf(stderr, "C export: cannot lower entry: %s\n", error);
		status = 4;
		fclose(output);
		goto done;
	}
	if (fclose(output)) goto done;
	if (rename(temporary, path)) goto done;
	status = 0;
done:
	if (status) unlink(temporary);
	free(temporary);
	return status;
}

int main(int argc, char **argv)
{
	uint64_t budget = 1000000, validation_limit = UINT64_MAX;
	size_t limit = PG_ARTIFACT_DEFAULT_LIMIT;
	int index = 1, trust_image = 0, tools = 0;
	const char *script = NULL, *cc = "cc", *ar = "ar";
	while (index < argc) {
		if (!strcmp(argv[index], "--trust-image")) {
			trust_image = 1;
			++index;
			continue;
		}
		if (index + 1 == argc) break;
		if (!strcmp(argv[index], "--steps")) {
			if (number(argv[index + 1], &budget)) goto usage;
		} else if (!strcmp(argv[index], "--revalidate-limit")) {
			if (number(argv[index + 1], &validation_limit)) goto usage;
		} else if (!strcmp(argv[index], "--image-limit")) {
			if (pg_artifact_limit_argument(argv[index + 1], &limit)) goto usage;
		} else if (!strcmp(argv[index], "--link")) {
			if (script) goto usage;
			script = argv[index + 1];
		} else if (!strcmp(argv[index], "--cc")) {
			cc = argv[index + 1];
			tools = 1;
		} else if (!strcmp(argv[index], "--ar")) {
			ar = argv[index + 1];
			tools = 1;
		} else break;
		index += 2;
	}
	if (argc - index != (script ? 1 : 3)) goto usage;
	if (tools && !script) goto usage;
	struct pg_c_link_plan plan = {.entry = 0, .count = 1};
	struct pg_c_export single = {"entry", NULL};
	const char *single_name = script ? NULL : argv[index + 1];
	const char *output = script ? argv[index] : argv[index + 2];
	if (script) {
		plan = (struct pg_c_link_plan){0};
		size_t line;
		const char *error;
		if (pg_c_link_read(&plan, script, &line, &error)) {
			fprintf(stderr, "%s:%zu: %s\n", script, line, error);
			pg_c_link_destroy(&plan);
			return 2;
		}
	} else {
		plan.artifact = argv[index]; plan.names = &single_name; plan.exports = &single;
	}
	int status = 2;
	const char *input = plan.artifact;
	struct stat source_stat, target_stat;
	if (!stat(input, &source_stat) && !stat(output, &target_stat)
		&& source_stat.st_dev == target_stat.st_dev && source_stat.st_ino == target_stat.st_ino) {
		fputs("C export: output must not replace the input artifact\n", stderr);
		goto cleanup;
	}
	FILE *file = fopen(input, "rb");
	if (!file) { perror(input); goto cleanup; }
	size_t count = 0;
	struct pg_synthesis_job *const *roots = NULL;
	struct pg_program *program = pg_artifact_read_file(file, limit, &count, &roots);
	fclose(file);
	if (!program) { fputs("C export: cannot read artifact\n", stderr); goto cleanup; }
	if (!count) { fputs("C export: artifact has no root\n", stderr); goto done; }
	uint64_t spent = 0;
	for (size_t i = 0; i < plan.count + plan.enum_count; ++i) {
		const char *selected_name = i < plan.count ? plan.names[i] : plan.enum_names[i - plan.count];
		struct pg_c_export *target = i < plan.count ? &plan.exports[i] : &plan.enums[i - plan.count];
		struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = selected_name, .length = strlen(selected_name)};
		if (trust_image) {
			int available = pg_artifact_trusted_export(program, roots[0], name, &target->subject);
			if (available != 1) {
				fputs("C export: no complete saved module/export; trust does not complete pending work\n", stderr);
				status = available < 0 ? 2 : 3;
				goto checked;
			}
			continue;
		}
		struct pg_synthesis_job *selected = pg_program_select_name(program, roots[0], name);
		if (!selected) { status = 1; goto checked; }
		uint64_t used = 0;
		enum pg_synthesis_status state = pg_artifact_revalidate(program, selected,
			budget - spent, validation_limit - spent, &used);
		spent += used;
		switch (state) {
		case PG_SYNTHESIS_PENDING: status = 3; goto checked;
		case PG_SYNTHESIS_REJECTED: status = 1; goto checked;
		case PG_SYNTHESIS_UNSUPPORTED: status = 4; goto checked;
		case PG_SYNTHESIS_ERROR: status = 2; goto checked;
		case PG_SYNTHESIS_DONE: break;
		}
		const struct pg_evidence *proof = pg_synthesis_result(selected);
		if (!pg_evidence_owned_by(proof, &program->typing)) goto checked;
		target->subject = pg_evidence_subject(proof);
	}
	status = script ? pg_c_link_publish(&plan, output, cc, ar, script, trust_image, spent) : publish(output, &plan);
checked:
	if (trust_image)
		fputs("C export: user-trusted saved completion; no authentication or revalidation; steps=0\n", stderr);
	else fprintf(stderr, "C export: reconstruction steps=%" PRIu64 " budget=%" PRIu64
		" validation_limit=%" PRIu64 " remaining=%" PRIu64 "\n", spent, budget, validation_limit, budget - spent);
done:
	pg_program_destroy(program);
cleanup:
	pg_c_link_destroy(&plan);
	return status;
usage:
	fputs("usage: a-to-c [--trust-image] [--steps N] [--revalidate-limit N] [--image-limit N|none]\n"
		"       INPUT.a ENTRY OUTPUT.c\n"
		"       --link SCRIPT.aplink [--cc TOOL] [--ar TOOL] NEW_OUTPUT_DIRECTORY\n", stderr);
	return 2;
}
