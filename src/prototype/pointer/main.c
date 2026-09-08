#include "program.h"
#include "graph_io.h"
#include "source_io.h"

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
	const char *selected = NULL;
	const char *save = NULL;
	int nf = 0, load = 0;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--steps")) {
			if (++i == argc || steps_argument(argv[i], &budget) != 0) goto usage;
		} else if (!strcmp(argv[i], "--nf") || !strcmp(argv[i], "--whnf")) {
			if (selected) goto usage;
			nf = !strcmp(argv[i], "--nf");
			if (++i == argc || !*argv[i]) goto usage;
			selected = argv[i];
		} else if (!strcmp(argv[i], "--load")) load = 1;
		else if (!strcmp(argv[i], "--save")) {
			if (save || ++i == argc || !*argv[i] || !strcmp(argv[i], "-")) goto usage;
			save = argv[i];
		} else if (!strcmp(argv[i], "--strict-thunks")) policy = PG_DEFINITION_EXPLICIT_THUNK;
		else if (!strcmp(argv[i], "--help")) {
			puts("usage: pointer-check [--steps N] [--strict-thunks] [--load] [--save FILE.a] [--whnf NAME|--nf NAME] INPUT|-\n"
				"Checks with the pointer-core solver; does not execute host effects.\n"
				"--load reads a single-root image (limit 1000000); its stored thunk policy applies.\n"
				"--save stores RECOMPUTE inputs, including pending/rejected inputs, not progress.\n"
				"WHNF/NF select a definition, force a stored thunk once, and print its pure Core DAG.\n"
				"Exit: 0 done, 1 rejected/syntax, 2 input/internal error, 3 pending, 4 unsupported.\n"
				"Steps bound solver transitions, not parsing time or individual rule cost.");
			return 0;
		} else {
			if (path || (argv[i][0] == '-' && strcmp(argv[i], "-"))) goto usage;
			path = argv[i];
		}
	}
	if (!path || (load && policy == PG_DEFINITION_EXPLICIT_THUNK)) goto usage;
	FILE *file = !strcmp(path, "-") ? stdin : fopen(path, "rb");
	if (!file) { fprintf(stderr, "%s: cannot open input\n", path); return 2; }
	struct pg_program *program;
	if (load) {
		size_t count;
		struct pg_synthesis_job *const *roots;
		program = pg_sources_read(file, 1000000, &count, &roots);
		if (program && count != 1) { pg_program_destroy(program); program = NULL; }
	} else {
		char *source = NULL;
		size_t length = 0;
		program = read_source(file, &source, &length) ? NULL : pg_program_create(source, length, policy);
		free(source);
	}
	if (file != stdin) fclose(file);
	if (!program) { fprintf(stderr, "%s: cannot read or initialize input\n", path); return 2; }
	int result;
	if (!program->root) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", path, program->parser.error_token.line,
			program->parser.error_token.column, program->parser.error ? program->parser.error : "parse failed");
		result = 1;
	} else {
		pg_synthesis_advance(&program->synthesis, budget);
		struct pg_synthesis_job *job = program->root;
		if (selected && pg_synthesis_status(job) == PG_SYNTHESIS_DONE) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = selected, .length = strlen(selected)};
			struct pg_synthesis_job *definition = pg_synthesis_definition(job, name);
			if (!definition) {
				fprintf(stderr, "%s: definition not found: %s\n", path, selected);
				pg_program_destroy(program);
				return 1;
			}
			const struct pg_evidence *proof = pg_synthesis_result(definition);
			const struct pg_term *content;
			if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
				pg_thunk_type_view(pg_evidence_classifier(proof), &content))
				proof = pg_prove_force(&program->typing, proof);
			const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
			job = nf ? pg_synthesis_nf(&program->synthesis, context, proof)
				: pg_synthesis_normalize(&program->synthesis, context, proof);
			if (!job) {
				fputs("unsupported selected definition\n", stderr);
				pg_program_destroy(program);
				return 4;
			}
			uint64_t remaining = program->synthesis.steps < budget ? budget - program->synthesis.steps : 0;
			pg_synthesis_advance(&program->synthesis, remaining);
		}
		const char *status;
		switch (pg_synthesis_status(job)) {
		case PG_SYNTHESIS_DONE: status = "done"; result = 0; break;
		case PG_SYNTHESIS_REJECTED: status = "rejected"; result = 1; break;
		case PG_SYNTHESIS_PENDING: status = "pending"; result = 3; break;
		case PG_SYNTHESIS_UNSUPPORTED: status = "unsupported"; result = 4; break;
		default: status = "error"; result = 2; break;
		}
		printf("%s steps=%" PRIu64 "\n", status, program->synthesis.steps);
		if (selected && !result && pg_graph_print(stdout, pg_evidence_subject(pg_synthesis_result(job))->core)) result = 2;
		if (save) {
			FILE *image = fopen(save, "wb");
			int failed = !image;
			if (image) {
				failed = pg_sources_write(image, &program->synthesis, 1, &program->root);
				if (fclose(image)) failed = 1;
			}
			if (failed) { fprintf(stderr, "%s: cannot save input image\n", save); result = 2; }
		}
	}
	pg_program_destroy(program);
	return result;
usage:
	fputs("usage: pointer-check [--steps N] [--strict-thunks] [--load] [--save FILE.a] [--whnf NAME|--nf NAME] INPUT|-\n", stderr);
	return 2;
}
