#include "program.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_source(FILE *file, char **source, size_t *length)
{
	size_t capacity = 4096;
	char *buffer = malloc(capacity);
	if (!buffer) return -1;
	*length = 0;
	for (;;) {
		if (*length == capacity) {
			if (capacity > SIZE_MAX / 2) break;
			size_t next = capacity * 2;
			char *grown = realloc(buffer, next);
			if (!grown) break;
			buffer = grown;
			capacity = next;
		}
		size_t read = fread(buffer + *length, 1, capacity - *length, file);
		*length += read;
		if (ferror(file)) break;
		if (feof(file)) { *source = buffer; return 0; }
		if (!read) break;
	}
	free(buffer);
	return -1;
}

static int steps_argument(const char *text, uint64_t *steps)
{
	if (*text < '0' || *text > '9') return -1;
	errno = 0;
	char *end;
	uintmax_t value = strtoumax(text, &end, 10);
	if (errno || *end || value > UINT64_MAX) return -1;
	*steps = (uint64_t)value;
	return 0;
}

int main(int argc, char **argv)
{
	uint64_t budget = 100000;
	enum pg_definition_policy policy = PG_DEFINITION_IMPLICIT_THUNK;
	const char *path = NULL;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--steps")) {
			if (++i == argc || steps_argument(argv[i], &budget) != 0) goto usage;
		} else if (!strcmp(argv[i], "--strict-thunks")) policy = PG_DEFINITION_EXPLICIT_THUNK;
		else if (!strcmp(argv[i], "--help")) {
			puts("usage: pointer-check [--steps N] [--strict-thunks] SOURCE.p|-\n"
				"Checks with the pointer-core solver; does not execute host effects.\n"
				"Exit: 0 done, 1 rejected/syntax, 2 input/internal error, 3 pending, 4 unsupported.\n"
				"Steps bound solver transitions, not parsing time or individual rule cost.");
			return 0;
		} else {
			if (path || (argv[i][0] == '-' && strcmp(argv[i], "-"))) goto usage;
			path = argv[i];
		}
	}
	if (!path) goto usage;
	FILE *file = !strcmp(path, "-") ? stdin : fopen(path, "rb");
	if (!file) { fprintf(stderr, "%s: cannot open source\n", path); return 2; }
	char *source = NULL;
	size_t length = 0;
	int read = read_source(file, &source, &length);
	if (file != stdin) fclose(file);
	if (read != 0) { fprintf(stderr, "%s: cannot read source\n", path); return 2; }
	struct pg_program *program = pg_program_create(source, length, policy);
	free(source);
	if (!program) { fputs("cannot initialize program\n", stderr); return 2; }
	int result;
	if (!program->root) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", path, program->parser.error_token.line,
			program->parser.error_token.column, program->parser.error ? program->parser.error : "parse failed");
		result = 1;
	} else {
		pg_synthesis_advance(&program->synthesis, budget);
		const char *status;
		switch (pg_synthesis_status(program->root)) {
		case PG_SYNTHESIS_DONE: status = "done"; result = 0; break;
		case PG_SYNTHESIS_REJECTED: status = "rejected"; result = 1; break;
		case PG_SYNTHESIS_PENDING: status = "pending"; result = 3; break;
		case PG_SYNTHESIS_UNSUPPORTED: status = "unsupported"; result = 4; break;
		default: status = "error"; result = 2; break;
		}
		printf("%s steps=%" PRIu64 "\n", status, program->synthesis.steps);
	}
	pg_program_destroy(program);
	return result;
usage:
	fputs("usage: pointer-check [--steps N] [--strict-thunks] SOURCE.p|-\n", stderr);
	return 2;
}
