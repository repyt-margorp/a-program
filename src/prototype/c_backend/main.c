#define _POSIX_C_SOURCE 200809L
#include "emit.h"
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

static int publish(const char *path, const struct pg_occurrence *root)
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
	if (pg_c_emit(output, root, &error)) {
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
	uint64_t budget = 1000000;
	size_t limit = PG_ARTIFACT_DEFAULT_LIMIT;
	int index = 1;
	while (index + 1 < argc) {
		if (!strcmp(argv[index], "--steps")) {
			if (number(argv[index + 1], &budget)) goto usage;
		} else if (!strcmp(argv[index], "--image-limit")) {
			if (pg_artifact_limit_argument(argv[index + 1], &limit)) goto usage;
		} else break;
		index += 2;
	}
	if (argc - index != 3) goto usage;
	const char *input = argv[index], *entry = argv[index + 1], *output = argv[index + 2];
	struct stat source_stat, target_stat;
	if (!stat(input, &source_stat) && !stat(output, &target_stat)
		&& source_stat.st_dev == target_stat.st_dev && source_stat.st_ino == target_stat.st_ino) {
		fputs("C export: output must not replace the input artifact\n", stderr);
		return 2;
	}
	FILE *file = fopen(input, "rb");
	if (!file) { perror(input); return 2; }
	size_t count = 0;
	struct pg_synthesis_job *const *roots = NULL;
	struct pg_program *program = pg_artifact_read_file(file, limit, &count, &roots);
	fclose(file);
	if (!program) { fputs("C export: cannot read artifact\n", stderr); return 2; }
	int status = 2;
	if (!count) { fputs("C export: artifact has no root\n", stderr); goto done; }
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = entry, .length = strlen(entry)};
	struct pg_synthesis_job *selected = pg_program_select_name(program, roots[0], name);
	if (!selected) { status = 1; goto done; }
	pg_synthesis_advance(&program->synthesis, budget);
	fprintf(stderr, "C export: reconstruction steps=%" PRIu64 " budget=%" PRIu64 "\n",
		program->synthesis.steps, budget);
	switch (pg_synthesis_status(selected)) {
	case PG_SYNTHESIS_PENDING: status = 3; break;
	case PG_SYNTHESIS_REJECTED: status = 1; break;
	case PG_SYNTHESIS_UNSUPPORTED: status = 4; break;
	case PG_SYNTHESIS_ERROR: status = 2; break;
	case PG_SYNTHESIS_DONE: {
		const struct pg_evidence *proof = pg_synthesis_result(selected);
		if (!pg_evidence_owned_by(proof, &program->typing)) break;
		status = publish(output, pg_evidence_subject(proof));
		break;
	}
	}
done:
	pg_program_destroy(program);
	return status;
usage:
	fputs("usage: a-to-c [--steps N] [--image-limit N|none] INPUT.a ENTRY OUTPUT.c\n", stderr);
	return 2;
}
