#include "program.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void solve(struct pg_program *program, uint64_t budget)
{
	for (size_t turns = 0; pg_synthesis_status(program->root) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 10000);
		pg_synthesis_advance(&program->synthesis, budget);
	}
}

static void modules(void)
{
	const char *library = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *program = pg_program_create(library, strlen(library), PG_DEFINITION_EXPLICIT_THUNK);
	assert(program && program->root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *scope = pg_synthesis_module_namespace(&program->synthesis,
		program->scope, name, program->root);
	struct pg_parser parser;
	char client[] = "{{ main:=lib.id; }}.main";
	struct pg_synthesis_job *root = pg_program_source(program, scope, client, strlen(client), &parser);
	assert(root && !parser.error && !program->synthesis.steps);
	memset(client, '?', sizeof(client));
	for (size_t i = 0; pg_synthesis_status(root) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&program->synthesis, 1);
	}
	assert(pg_synthesis_status(root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *exported = pg_synthesis_definition(program->root, id);
	assert(exported && pg_synthesis_result(exported) == pg_synthesis_result(root));
	const struct pg_evidence *accepted = pg_synthesis_result(root);
	assert(!pg_program_source(program, scope, "bad:=", 5, &parser));
	assert(parser.error);
	assert(pg_synthesis_result(root) == accepted && !program->parser.error);
	pg_program_destroy(program);
}

static void execute_example(const char *path, const char *type_name,
	const char *base_name, const char *step_name, size_t count, uint64_t budget)
{
	FILE *file = fopen(path, "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long size = ftell(file);
	assert(size >= 0 && !fseek(file, 0, SEEK_SET));
	char *source = malloc((size_t)size + 1);
	assert(source && fread(source, 1, (size_t)size, file) == (size_t)size);
	assert(!fclose(file));
	struct pg_program *program = pg_program_create(source, (size_t)size, PG_DEFINITION_IMPLICIT_THUNK);
	free(source);
	assert(program && program->root);
	solve(program, budget);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	const struct pg_evidence *main = pg_synthesis_result(pg_synthesis_definition(program->root, name));
	assert(main);
	if (pg_evidence_judgement(main) == PG_JUDGEMENT_VALUE) main = pg_prove_force(&program->typing, main);
	assert(main && pg_evidence_judgement(main) == PG_JUDGEMENT_COMPUTATION);
	const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
	struct pg_synthesis_job *nf = pg_synthesis_nf(&program->synthesis, context, main);
	assert(nf);
	for (size_t turns = 0; pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 100000);
		pg_synthesis_advance(&program->synthesis, budget);
	}
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *result = pg_prove_return_value(&program->typing, pg_synthesis_result(nf));
	assert(result);
	name.text = type_name; name.length = strlen(type_name);
	struct pg_synthesis_job *type_job = pg_synthesis_definition(program->root, name);
	const struct pg_evidence *type = pg_synthesis_result(type_job);
	assert(type && pg_evidence_classifier(result) == pg_evidence_subject(type)->core);
	/* Resolve labels in this declaration, never hard-code constructor ordinals
	 * or accept an erased shape belonging to a different nominal type. */
	const struct pg_source_scope *scope = pg_synthesis_name_job(&program->synthesis,
		program->scope, name, type_job);
	char expression[256];
	int length = snprintf(expression, sizeof(expression), "expected:=%s.%s;", type_name, base_name);
	assert(length > 0 && (size_t)length < sizeof(expression));
	struct pg_parser parser;
	struct pg_synthesis_job *expected_job = pg_program_source(program, scope, expression, (size_t)length, &parser);
	assert(expected_job);
	pg_synthesis_advance(&program->synthesis, 100000);
	assert(pg_synthesis_status(expected_job) == PG_SYNTHESIS_DONE);
	name.text = "expected"; name.length = 8;
	const struct pg_evidence *expected = pg_synthesis_result(pg_synthesis_definition(expected_job, name));
	assert(expected && pg_evidence_judgement(expected) == PG_JUDGEMENT_VALUE);
	const struct pg_term *core = pg_evidence_subject(expected)->core;
	if (count) {
		length = snprintf(expression, sizeof(expression), "step:=%s.%s;", type_name, step_name);
		assert(length > 0 && (size_t)length < sizeof(expression));
		struct pg_synthesis_job *step_job = pg_program_source(program, scope, expression, (size_t)length, &parser);
		assert(step_job);
		pg_synthesis_advance(&program->synthesis, 100000);
		assert(pg_synthesis_status(step_job) == PG_SYNTHESIS_DONE);
		/* Applying the checked constructor, rather than decoding Nat numerals,
		 * keeps the expected result within the same source declaration. */
		name.text = "step"; name.length = 4;
		const struct pg_evidence *step = pg_prove_force(&program->typing,
			pg_synthesis_result(pg_synthesis_definition(step_job, name)));
		assert(step);
		for (size_t i = 0; i < count; ++i) {
			struct pg_synthesis_job *value = pg_synthesis_return(&program->synthesis, context,
				pg_prove_application(&program->typing, step, expected));
			assert(value);
			pg_synthesis_advance(&program->synthesis, 100000);
			assert(pg_synthesis_status(value) == PG_SYNTHESIS_DONE);
			expected = pg_synthesis_result(value);
		}
		core = pg_evidence_subject(expected)->core;
	}
	assert(pg_evidence_subject(result)->core == core);
	printf("executed %s: %s.%s + %zu %s, chunk=%llu\n", path,
		type_name, base_name, count, step_name, (unsigned long long)budget);
	pg_program_destroy(program);
}

int main(int argc, char **argv)
{
	if (argc != 1) {
		assert(argc == 6);
		char *end;
		unsigned long count = strtoul(argv[5], &end, 10);
		assert(*argv[5] && !*end && count < 1000);
		execute_example(argv[1], argv[2], argv[3], argv[4], count, 1);
		execute_example(argv[1], argv[2], argv[3], argv[4], count, 10000);
		return 0;
	}
	modules();
	char source[] = "{{ id := &(\\A:@ => \\x:A => x); id :: (A:@)->A->A; }}.id";
	struct pg_program *split = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	struct pg_program *whole = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	assert(split && whole && split->root && whole->root);
	memset(source, '?', sizeof(source));
	assert(!split->synthesis.steps && !whole->synthesis.steps);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(split->root));
	pg_synthesis_advance(&split->synthesis, 0);
	assert(!split->synthesis.steps);
	solve(split, 1);
	solve(whole, 10000);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whole->root) == PG_SYNTHESIS_DONE);
	assert(split->synthesis.steps == whole->synthesis.steps);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(split->root))->core,
		pg_evidence_subject(pg_synthesis_result(whole->root))->core) == 1);
	uint64_t steps = split->synthesis.steps;
	pg_synthesis_advance(&split->synthesis, 10000);
	assert(split->synthesis.steps == steps);
	const struct pg_evidence *proof = pg_synthesis_result(split->root);
	assert(!pg_program_normalize(NULL, proof, 0));
	assert(!pg_program_normalize(split, NULL, 0));
	assert(!pg_program_normalize(whole, proof, 0));
	const struct pg_evidence *empty = pg_prove_empty_context(&split->typing);
	const struct pg_evidence *universe = pg_prove_universe(&split->typing, &split->classifiers, empty, 0);
	const struct pg_object *binder = pg_binder(&split->graph);
	const struct pg_evidence *open = pg_prove_context_extension(&split->typing, empty, binder, universe);
	const struct pg_evidence *variable = pg_prove_variable(&split->typing, open, binder);
	assert(variable && !pg_program_normalize(split, variable, 0));
	struct pg_synthesis_job *whnf = pg_program_normalize(split, proof, 0);
	struct pg_synthesis_job *nf = pg_program_normalize(split, proof, 1);
	assert(whnf && nf && split->synthesis.steps == steps);
	assert(whnf == pg_program_normalize(split, proof, 0));
	assert(nf == pg_program_normalize(split, proof, 1));
	for (size_t i = 0; pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&split->synthesis, 1);
	}
	assert(pg_synthesis_status(whnf) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(pg_synthesis_result(nf)) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_evidence_subject(pg_synthesis_result(whnf))->core->kind == PG_LAMBDA);
	assert(pg_program_normalize(split, proof, 1) == nf);
	pg_program_destroy(split);
	pg_program_destroy(whole);
	const char *invalid = "main := ";
	struct pg_program *bad = pg_program_create(invalid, strlen(invalid), PG_DEFINITION_EXPLICIT_THUNK);
	assert(bad && !bad->root && bad->parser.error);
	assert(!bad->synthesis.steps);
	pg_program_destroy(bad);
	struct pg_program *pending = pg_program_create("x:=x;", 5, PG_DEFINITION_EXPLICIT_THUNK);
	assert(pending && pending->root);
	pg_synthesis_advance(&pending->synthesis, 100);
	assert(pg_synthesis_status(pending->root) == PG_SYNTHESIS_PENDING);
	pg_program_destroy(pending);
	assert(!pg_program_create(NULL, 1, PG_DEFINITION_EXPLICIT_THUNK));
	assert(!pg_program_create("", 0, (enum pg_definition_policy)99));
	pg_program_destroy(NULL);
	puts("program: owned source, unresolved root, split-budget solve and pending destruction passed");
	return 0;
}
