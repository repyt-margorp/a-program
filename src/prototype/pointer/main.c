#define _POSIX_C_SOURCE 200809L
#include "program.h"
#include "graph_io.h"
#include "source_io.h"
#include "computation_io.h"
#include "execution.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct root_list {
	/* Ordered user selections; aliases are allowed, jobs remain interned. */
	struct pg_synthesis_job **items;
	size_t count, capacity;
};

static int retain_root(struct root_list *roots, struct pg_synthesis_job *job)
{
	if (!job) return -1;
	if (roots->count == roots->capacity) {
		if (roots->capacity > SIZE_MAX / sizeof(*roots->items) / 2) return -1;
		size_t capacity = roots->capacity ? roots->capacity * 2 : 8;
		struct pg_synthesis_job **items = realloc(roots->items, capacity * sizeof(*items));
		if (!items) return -1;
		roots->items = items; roots->capacity = capacity;
	}
	roots->items[roots->count++] = job;
	return 0;
}

/* Publish only a completely written image; the temporary file shares its
 * destination directory so rename does not cross filesystems. */
static int save_image(const char *path, const struct pg_program *program,
	size_t count, struct pg_synthesis_job *const *roots, int retain_reductions)
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
		struct pg_graph temporary_graph = {0};
		const struct pg_reduction_archive *reductions = retain_reductions
			? pg_reduction_archive_snapshot(&temporary_graph, &program->evaluation, program->retained_reductions) : NULL;
		if (!retain_reductions || reductions)
			status = pg_sources_write_retained(file, &program->synthesis, count, roots, reductions);
		pg_graph_destroy(&temporary_graph);
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

static int report(FILE *output, struct pg_program *program, struct pg_synthesis_job *job)
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
	fprintf(output, "%s steps=%" PRIu64 "\n", status, program->synthesis.steps);
	if (result == 3 && !program->synthesis.ready) {
		fflush(output);
		fputs("pending: no runnable synthesis work; increasing the step budget alone will not advance this Program\n", stderr);
	}
	return result;
}

static int run(struct pg_program *program, const struct pg_evidence *proof, uint64_t budget)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE) {
		const struct pg_term *content;
		proof = pg_thunk_type_view(pg_evidence_classifier(proof), &content)
			? pg_prove_force(&program->typing, proof)
			: pg_prove_return(&program->typing, proof);
	}
	struct pg_execution execution;
	int result = 4;
	const char *status = "unsupported entry: expected a closed returning computation";
	if (!pg_execution_init(&execution, &program->typing, proof, stdout)) {
		switch (pg_execution_advance(&execution, budget)) {
		case PG_EXECUTION_DONE: status = "done"; result = 0; break;
		case PG_EXECUTION_PENDING: status = "pending"; result = 3; break;
		case PG_EXECUTION_UNHANDLED: status = "unhandled operation"; break;
		case PG_EXECUTION_STUCK: status = "stuck computation"; break;
		case PG_EXECUTION_IO_ERROR: status = "output error (not retried)"; result = 2; break;
		case PG_EXECUTION_ERROR: status = "internal error"; result = 2; break;
		}
	}
	fprintf(stderr, "run: %s steps=%" PRIu64 "\n", status, execution.steps);
	pg_execution_destroy(&execution);
	return result;
}

static int repl(struct pg_program *program, uint64_t budget,
	struct root_list *roots, int retain_reductions)
{
	char *line = NULL;
	size_t capacity = 0;
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
			if (retain_root(roots, root)) { result = 2; break; }
			program->root = root;
			pg_synthesis_advance(&program->synthesis, budget);
			report(stdout, program, root);
			continue;
		}
		char *argument = command + strcspn(command, " \t\r\n");
		if (*argument) *argument++ = 0;
		argument += strspn(argument, " \t\r\n");
		size_t length = strlen(argument);
		while (length && strchr(" \t\r\n", argument[length - 1])) argument[--length] = 0;
		if (!*command) continue;
		if (!strcmp(command, ":quit") && !*argument) break;
		if (!strcmp(command, ":status") && !*argument) { report(stdout, program, program->root); continue; }
		if (!strcmp(command, ":root")) {
			uint64_t index;
			if (steps_argument(argument, &index) || !index || index > roots->count) {
				fprintf(stderr, "root index must be in 1..%zu\n", roots->count);
				continue;
			}
			program->root = roots->items[index - 1];
			report(stdout, program, program->root);
			continue;
		}
		if (!strcmp(command, ":solve")) {
			uint64_t steps = budget;
			if (*argument && steps_argument(argument, &steps)) { fputs("invalid step budget\n", stderr); continue; }
			pg_synthesis_advance(&program->synthesis, steps);
			report(stdout, program, program->root);
			continue;
		}
		if (!strcmp(command, ":save") && *argument) {
			if (save_image(argument, program, roots->count, roots->items, retain_reductions)) fputs("cannot save input image\n", stderr);
			else puts("saved");
			continue;
		}
		if ((!strcmp(command, ":nf") || !strcmp(command, ":whnf")) && *argument) {
			if (argument[length - 1] == ';') argument[--length] = 0;
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = argument, .length = length};
			struct pg_synthesis_job *job = pg_program_evaluate_name(program, program->root, name, !strcmp(command, ":nf"));
			if (!job) { fputs("definition not found\n", stderr); continue; }
			if (retain_root(roots, job)) { result = 2; break; }
			pg_synthesis_advance(&program->synthesis, budget);
			if (!report(stdout, program, job) && pg_graph_print(stdout, pg_evidence_subject(pg_synthesis_result(job))->core))
				fputs("cannot print result\n", stderr);
			continue;
		}
		fputs("expected :solve [N], :status, :root N, :whnf NAME, :nf NAME, :save FILE.a or :quit\n", stderr);
	}
	free(line);
	return ferror(stdin) ? 2 : result;
}

int main(int argc, char **argv)
{
	uint64_t budget = 100000;
	uint64_t run_budget = 100000;
	uint64_t root_index = 0;
	enum pg_definition_policy policy = PG_DEFINITION_IMPLICIT_THUNK;
	const char *path = NULL;
	const char *selected = NULL;
	const char *save = NULL;
	const char *imports = NULL;
	int nf = 0, load = 0, interactive = 0, retain_reductions = 0, legacy_intrinsic_dot = 0;
	int execute = 0, run_steps = 0;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--steps")) {
			if (++i == argc || steps_argument(argv[i], &budget) != 0) goto usage;
		} else if (!strcmp(argv[i], "--run-steps")) {
			if (run_steps || ++i == argc || steps_argument(argv[i], &run_budget)) goto usage;
			run_steps = 1;
		} else if (!strcmp(argv[i], "--root")) {
			if (root_index || ++i == argc || steps_argument(argv[i], &root_index) || !root_index) goto usage;
		} else if (!strcmp(argv[i], "--nf") || !strcmp(argv[i], "--whnf") || !strcmp(argv[i], "--run")) {
			if (selected) goto usage;
			nf = !strcmp(argv[i], "--nf");
			execute = !strcmp(argv[i], "--run");
			if (++i == argc || !*argv[i]) goto usage;
			selected = argv[i];
		} else if (!strcmp(argv[i], "--imports")) {
			if (imports || ++i == argc || !*argv[i] || !strcmp(argv[i], "-")) goto usage;
			imports = argv[i];
		} else if (!strcmp(argv[i], "--load")) load = 1;
		else if (!strcmp(argv[i], "--repl")) interactive = 1;
		else if (!strcmp(argv[i], "--retain-reductions")) retain_reductions = 1;
		else if (!strcmp(argv[i], "--save")) {
			if (save || ++i == argc || !*argv[i] || !strcmp(argv[i], "-")) goto usage;
			save = argv[i];
		} else if (!strcmp(argv[i], "--strict-thunks")) policy = PG_DEFINITION_EXPLICIT_THUNK;
		else if (!strcmp(argv[i], "--legacy-intrinsic-dot")) legacy_intrinsic_dot = 1;
		else if (!strcmp(argv[i], "--help")) {
			puts("usage: pointer-check [--steps N] [--strict-thunks] [--legacy-intrinsic-dot] [--imports FILE.p|--load [--root N]] [--save FILE.a] [--retain-reductions] [--whnf NAME|--nf NAME|--run NAME [--run-steps N]] INPUT|-\n"
				"Checks with the pointer-core solver; host effects execute only with --run.\n"
				"--run selects a checked definition, forces a stored thunk once, and runs it with fresh effect state.\n"
				"Print writes exact Text bytes without a newline. Run diagnostics go to stderr.\n"
				"--run-steps bounds evaluator/host-dispatch transitions (default 100000), not I/O time or byte count.\n"
				"A saved image is not an effect receipt; another --run starts over and can repeat output.\n"
				"#Name is standard; --legacy-intrinsic-dot also accepts #.Name in source/imports/REPL.\n"
				"--imports FILE.p supplies exported symbols to explicit source imports.\n"
				"--repl keeps the loaded Program for :solve, :whnf, :nf, :status, :root, :save, :quit.\n"
				"--load reads an image (limit 1000000); its stored thunk policy applies.\n"
				"--root N selects a loaded root (1-based, default 1); save retains every root.\n"
				"Created WHNF/NF requests append roots without replacing the selected source module.\n"
				"--save defaults to RECOMPUTE inputs, including pending/rejected inputs.\n"
				"--retain-reductions also saves completed reductions and partial NF records, including imported ones.\n"
				"Retained records are not trusted on load; ordinary source Solve still recomputes. This is not full CHECKPOINT.\n"
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
	if (execute && interactive) goto usage;
	if (run_steps && !execute) goto usage;
	if (retain_reductions && !save && !interactive) goto usage;
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
		if (program) program->allow_legacy_intrinsic_dot = legacy_intrinsic_dot;
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
	program->allow_legacy_intrinsic_dot = legacy_intrinsic_dot;
	if (!roots) roots = &program->root;
	struct root_list retained = {0};
	int result;
	if (!program->root) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", path, program->parser.error_token.line,
			program->parser.error_token.column, program->parser.error ? program->parser.error : "parse failed");
		result = 1;
	} else {
		for (size_t i = 0; i < count; ++i)
			if (retain_root(&retained, roots[i])) { result = 2; goto done; }
		struct pg_synthesis_job *job = program->root;
		if (selected) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = selected, .length = strlen(selected)};
			job = execute ? pg_program_select_name(program, job, name)
				: pg_program_evaluate_name(program, job, name, nf);
			if (!job) {
				fprintf(stderr, "%s: definition not found: %s\n", path, selected);
				result = 1; goto done;
			}
			if (retain_root(&retained, job)) { result = 2; goto done; }
		}
		pg_synthesis_advance(&program->synthesis, budget);
		result = report(execute ? stderr : stdout, program, job);
		if (selected && !execute && !result && pg_graph_print(stdout, pg_evidence_subject(pg_synthesis_result(job))->core)) result = 2;
		if (save) {
			if (save_image(save, program, retained.count, retained.items, retain_reductions)) {
				fprintf(stderr, "%s: cannot save input image\n", save); result = 2;
			}
		}
		if (execute && !result) result = run(program, pg_synthesis_result(job), run_budget);
		if (interactive) result = repl(program, budget, &retained, retain_reductions);
	}
done:
	free(retained.items);
	pg_program_destroy(program);
	return result;
usage:
	fputs("usage: pointer-check [--steps N] [--strict-thunks] [--legacy-intrinsic-dot] [--imports FILE.p|--load [--root N]] [--save FILE.a] [--retain-reductions] [--whnf NAME|--nf NAME|--run NAME [--run-steps N]] INPUT|-\n", stderr);
	return 2;
}
