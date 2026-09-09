#define _POSIX_C_SOURCE 200809L
#include "program.h"
#include "graph_io.h"
#include "source_io.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Publish only a completely written image; the temporary file shares its
 * destination directory so rename does not cross filesystems. */
static int save_image(const char *path, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots)
{
	static const char suffix[] = ".tmp.XXXXXX";
	size_t length = strlen(path);
	if (length > SIZE_MAX - sizeof(suffix)) return -1;
	char *temporary = malloc(length + sizeof(suffix));
	if (!temporary) return -1;
	memcpy(temporary, path, length);
	memcpy(temporary + length, suffix, sizeof(suffix));
	int status = -1;
	int descriptor = mkstemp(temporary);
	if (descriptor < 0) { free(temporary); return -1; }
	FILE *file = fdopen(descriptor, "wb");
	if (file) {
		status = pg_sources_write(file, synthesis, count, roots);
		if (fclose(file)) status = -1;
	} else close(descriptor);
	if (!status) status = rename(temporary, path);
	if (status) unlink(temporary);
	free(temporary);
	return status;
}

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

static int import_provider(struct pg_program *program, const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) return -1;
	char *text = NULL;
	size_t length;
	int status = read_source(file, &text, &length);
	if (fclose(file)) status = -1;
	struct pg_parser parser = {0};
	struct pg_synthesis_job *module = status ? NULL : pg_program_source(program,
		program->scope, text, length, &parser);
	free(text);
	if (parser.error) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", path, parser.error_token.line,
			parser.error_token.column, parser.error);
		return 1;
	}
	if (!module) return -1;
	const struct pg_source_scope *bindings = pg_program_exports(program,
		pg_synthesis_root(&program->synthesis), module);
	if (!bindings) return -1;
	program->scope = pg_synthesis_import_scope(&program->synthesis, program->scope, bindings);
	return program->scope ? 0 : -1;
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

static int report(struct pg_program *program, struct pg_synthesis_job *job)
{
	const char *status;
	int result;
	switch (pg_synthesis_status(job)) {
	case PG_SYNTHESIS_DONE: status = "done"; result = 0; break;
	case PG_SYNTHESIS_REJECTED: status = "rejected"; result = 1; break;
	case PG_SYNTHESIS_PENDING: status = "pending"; result = 3; break;
	case PG_SYNTHESIS_UNSUPPORTED: status = "unsupported"; result = 4; break;
	default: status = "error"; result = 2; break;
	}
	printf("%s steps=%" PRIu64 "\n", status, program->synthesis.steps);
	return result;
}

static int repl(struct pg_program *program, uint64_t budget,
	size_t count, struct pg_synthesis_job *const *roots)
{
	char *line = NULL;
	size_t capacity = 0;
	if (!count || count > SIZE_MAX / sizeof(*roots)) return 2;
	size_t root_capacity = count;
	struct pg_synthesis_job **retained = malloc(count * sizeof(*retained));
	if (!retained) return 2;
	memcpy(retained, roots, count * sizeof(*retained));
	roots = retained;
	int result = 0;
	for (;;) {
		if (isatty(STDIN_FILENO)) { fputs("pointer> ", stdout); fflush(stdout); }
		if (getline(&line, &capacity, stdin) < 0) break;
		char *command = line + strspn(line, " \t\r\n");
		if (!*command) continue;
		if (*command != ':') {
			const struct pg_source_scope *ambient;
			const struct pg_syntax *syntax;
			if (pg_synthesis_source_input(&program->synthesis, program->root, &ambient, &syntax)) {
				fputs("selected root has no source scope\n", stderr);
				continue;
			}
			const struct pg_source_scope *scope = pg_program_exports(program, ambient, program->root);
			if (!scope) { fputs("selected root is not a source module\n", stderr); continue; }
			struct pg_parser parser;
			struct pg_synthesis_job *root = pg_program_source(program, scope, command, strlen(command), &parser);
			if (!root) {
				fprintf(stderr, "<interactive>:%zu:%zu: %s\n", parser.error_token.line,
					parser.error_token.column, parser.error ? parser.error : "cannot prepare source");
				continue;
			}
			if (count == root_capacity) {
				if (root_capacity > SIZE_MAX / sizeof(*retained) / 2) { result = 2; break; }
				size_t next = root_capacity * 2;
				struct pg_synthesis_job **grown = realloc(retained, next * sizeof(*retained));
				if (!grown) { result = 2; break; }
				retained = grown; roots = retained; root_capacity = next;
			}
			retained[count++] = root;
			program->root = root;
			pg_synthesis_advance(&program->synthesis, budget);
			report(program, root);
			continue;
		}
		char *argument = command + strcspn(command, " \t\r\n");
		if (*argument) *argument++ = 0;
		argument += strspn(argument, " \t\r\n");
		size_t length = strlen(argument);
		while (length && strchr(" \t\r\n", argument[length - 1])) argument[--length] = 0;
		if (!*command) continue;
		if (!strcmp(command, ":quit") && !*argument) break;
		if (!strcmp(command, ":status") && !*argument) { report(program, program->root); continue; }
		if (!strcmp(command, ":root")) {
			uint64_t index;
			if (steps_argument(argument, &index) || !index || index > count) {
				fprintf(stderr, "root index must be in 1..%zu\n", count);
				continue;
			}
			program->root = roots[index - 1];
			report(program, program->root);
			continue;
		}
		if (!strcmp(command, ":solve")) {
			uint64_t steps = budget;
			if (*argument && steps_argument(argument, &steps)) { fputs("invalid step budget\n", stderr); continue; }
			pg_synthesis_advance(&program->synthesis, steps);
			report(program, program->root);
			continue;
		}
		if (!strcmp(command, ":save") && *argument) {
			if (save_image(argument, &program->synthesis, count, roots)) fputs("cannot save input image\n", stderr);
			else puts("saved");
			continue;
		}
		if ((!strcmp(command, ":nf") || !strcmp(command, ":whnf")) && *argument) {
			if (argument[length - 1] == ';') argument[--length] = 0;
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = argument, .length = length};
			if (pg_synthesis_status(program->root) != PG_SYNTHESIS_DONE) { report(program, program->root); continue; }
			struct pg_synthesis_job *definition = pg_synthesis_definition(program->root, name);
			if (!definition) { fputs("definition not found\n", stderr); continue; }
			struct pg_synthesis_job *job = pg_program_normalize(program,
				pg_synthesis_result(definition), !strcmp(command, ":nf"));
			if (!job) { fputs("unsupported selected definition\n", stderr); continue; }
			pg_synthesis_advance(&program->synthesis, budget);
			if (!report(program, job) && pg_graph_print(stdout, pg_evidence_subject(pg_synthesis_result(job))->core))
				fputs("cannot print result\n", stderr);
			continue;
		}
		fputs("expected :solve [N], :status, :root N, :whnf NAME, :nf NAME, :save FILE.a or :quit\n", stderr);
	}
	free(line);
	free(retained);
	return ferror(stdin) ? 2 : result;
}

int main(int argc, char **argv)
{
	uint64_t budget = 100000;
	uint64_t root_index = 0;
	enum pg_definition_policy policy = PG_DEFINITION_IMPLICIT_THUNK;
	const char *path = NULL;
	const char *selected = NULL;
	const char *save = NULL;
	const char *imports = NULL;
	int nf = 0, load = 0, interactive = 0;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--steps")) {
			if (++i == argc || steps_argument(argv[i], &budget) != 0) goto usage;
		} else if (!strcmp(argv[i], "--root")) {
			if (root_index || ++i == argc || steps_argument(argv[i], &root_index) || !root_index) goto usage;
		} else if (!strcmp(argv[i], "--nf") || !strcmp(argv[i], "--whnf")) {
			if (selected) goto usage;
			nf = !strcmp(argv[i], "--nf");
			if (++i == argc || !*argv[i]) goto usage;
			selected = argv[i];
		} else if (!strcmp(argv[i], "--imports")) {
			if (imports || ++i == argc || !*argv[i] || !strcmp(argv[i], "-")) goto usage;
			imports = argv[i];
		} else if (!strcmp(argv[i], "--load")) load = 1;
		else if (!strcmp(argv[i], "--repl")) interactive = 1;
		else if (!strcmp(argv[i], "--save")) {
			if (save || ++i == argc || !*argv[i] || !strcmp(argv[i], "-")) goto usage;
			save = argv[i];
		} else if (!strcmp(argv[i], "--strict-thunks")) policy = PG_DEFINITION_EXPLICIT_THUNK;
		else if (!strcmp(argv[i], "--help")) {
			puts("usage: pointer-check [--steps N] [--strict-thunks] [--imports FILE.p|--load [--root N]] [--save FILE.a] [--whnf NAME|--nf NAME] INPUT|-\n"
				"Checks with the pointer-core solver; does not execute host effects.\n"
				"--imports FILE.p supplies exported symbols to explicit source imports.\n"
				"--repl keeps the loaded Program for :solve, :whnf, :nf, :status, :root, :save, :quit.\n"
				"--load reads an image (limit 1000000); its stored thunk policy applies.\n"
				"--root N selects a loaded root (1-based, default 1); save retains every root.\n"
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
	if (!path || (load && (policy == PG_DEFINITION_EXPLICIT_THUNK || imports)) || (root_index && !load)) goto usage;
	if (interactive && !strcmp(path, "-")) goto usage;
	if (!root_index) root_index = 1;
	FILE *file = !strcmp(path, "-") ? stdin : fopen(path, "rb");
	if (!file) { fprintf(stderr, "%s: cannot open input\n", path); return 2; }
	struct pg_program *program;
	size_t count = 1;
	struct pg_synthesis_job *const *roots = NULL;
	if (load) {
		program = pg_sources_read(file, 1000000, &count, &roots);
		if (program && root_index > count) {
			fprintf(stderr, "%s: root index out of range: %" PRIu64 " (count %zu)\n", path, root_index, count);
			pg_program_destroy(program);
			if (file != stdin) fclose(file);
			return 2;
		}
		if (program) program->root = roots[root_index - 1];
	} else {
		char *source = NULL;
		size_t length = 0;
		program = read_source(file, &source, &length) ? NULL : pg_program_allocate(policy);
		if (program && imports) {
			int status = import_provider(program, imports);
			if (status) {
				if (status < 0) fprintf(stderr, "%s: cannot prepare import provider\n", imports);
				pg_program_destroy(program);
				free(source);
				if (file != stdin) fclose(file);
				return status < 0 ? 2 : 1;
			}
		}
		if (program) program->root = pg_program_source(program, program->scope, source, length, &program->parser);
		free(source);
	}
	if (file != stdin) fclose(file);
	if (!program) { fprintf(stderr, "%s: cannot read or initialize input\n", path); return 2; }
	if (!roots) roots = &program->root;
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
			job = pg_program_normalize(program, proof, nf);
			if (!job) {
				fputs("unsupported selected definition\n", stderr);
				pg_program_destroy(program);
				return 4;
			}
			uint64_t remaining = program->synthesis.steps < budget ? budget - program->synthesis.steps : 0;
			pg_synthesis_advance(&program->synthesis, remaining);
		}
		result = report(program, job);
		if (selected && !result && pg_graph_print(stdout, pg_evidence_subject(pg_synthesis_result(job))->core)) result = 2;
		if (save) {
			if (save_image(save, &program->synthesis, count, roots)) {
				fprintf(stderr, "%s: cannot save input image\n", save); result = 2;
			}
		}
		if (interactive) result = repl(program, budget, count, roots);
	}
	pg_program_destroy(program);
	return result;
usage:
	fputs("usage: pointer-check [--steps N] [--strict-thunks] [--imports FILE.p|--load [--root N]] [--save FILE.a] [--whnf NAME|--nf NAME] INPUT|-\n", stderr);
	return 2;
}
