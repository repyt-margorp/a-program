#include "source_io.h"
#include "derivation.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct pg_synthesis_job *parse(struct pg_program *program,
	const struct pg_source_scope *scope, const char *text)
{
	struct pg_parser parser;
	struct pg_synthesis_job *job = pg_program_source(program, scope, text, strlen(text), &parser);
	assert(job && !parser.error);
	return job;
}

static void definition_boundaries(void)
{
	const char *texts[] = {"good:=@; bad:=missing;", "good:=@; a:=b; b:=a;", "good:=@; good:=@;"};
	for (size_t i = 0; i < 3; ++i) {
		struct pg_program *original = pg_program_create(texts[i], strlen(texts[i]), PG_DEFINITION_EXPLICIT_THUNK);
		assert(original && original->root);
		const struct pg_source_scope *scope;
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&original->synthesis, original->root, &scope, &syntax));
		const struct pg_source_scope *local = pg_synthesis_definition_scope(&original->synthesis, scope, syntax);
		assert(local && !original->synthesis.steps);
		assert(local == pg_synthesis_definition_scope(&original->synthesis, scope, syntax));
		struct pg_source_environment environment;
		assert(!pg_synthesis_environment_input(&original->synthesis, local, &environment));
		assert(environment.parent == scope && environment.definitions == syntax);
		struct pg_synthesis_job *roots[] = {original->root,
			pg_synthesis_definition_request(&original->synthesis, scope, syntax, syntax->items[0].expression),
			parse(original, local, "{{ main:=good; }}.main")};
		assert(roots[1]);
		FILE *file = tmpfile();
		assert(file && !pg_sources_write(file, &original->synthesis, 3, roots));
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *restored;
		struct pg_program *loaded = pg_sources_read(file, 10000, &count, &restored);
		assert(loaded && count == 3 && !loaded->synthesis.steps);
		struct pg_program *programs[] = {original, loaded};
		for (size_t j = 0; j < 2; ++j) {
			struct pg_synthesis *s = &programs[j]->synthesis;
			struct pg_synthesis_job *const *selected = j ? restored : roots;
			while (s->ready) { assert(s->steps < 10000); pg_synthesis_advance(s, 1); }
			assert(pg_synthesis_status(selected[0]) == (i == 1 ? PG_SYNTHESIS_PENDING : PG_SYNTHESIS_REJECTED));
			assert(pg_synthesis_status(selected[1]) == (i == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
			assert(pg_synthesis_status(selected[2]) == (i == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		}
		assert(!fclose(file));
		pg_program_destroy(loaded);
		pg_program_destroy(original);
	}
}

static void rule_environments(void)
{
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *context = pg_synthesis_evidence(&p->synthesis, pg_prove_empty_context(&p->typing));
	struct pg_derivation_input universe = {.rule = PG_UNIVERSE_FORM, .count = 1};
	struct pg_synthesis_job *type = pg_synthesis_rule(&p->synthesis, &universe, &context, NULL, NULL);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "T", .length = 1};
	const struct pg_source_scope *scope = pg_synthesis_name_job(&p->synthesis, p->scope, name, type);
	assert(scope);
	struct pg_synthesis_job *consumer = parse(p, scope, "{{ main:=&(\\x:T => x); }}.main");
	const struct pg_effect_row *empty = pg_effect_row(&p->graph, 0, NULL);
	struct pg_effect_equation *equation = pg_effect_equation(&p->imported_effects, empty);
	struct pg_derivation_input carrier = {.rule = PG_RETURN_TYPE_FORM, .count = 1};
	struct pg_synthesis_job *result = pg_synthesis_rule(&p->synthesis, &carrier, &type, &p->imported_effects, equation);
	struct pg_derivation_input invalid = {.rule = PG_APP_ELIM};
	struct pg_synthesis_job *bad = pg_synthesis_derivation(&p->synthesis, &invalid);
	struct pg_synthesis_job *selected[] = {consumer, type, result, bad, type};
	FILE *file = tmpfile();
	assert(file && pg_sources_write(file, &p->synthesis, 5, selected) == -1 && ftell(file) == 0);
	pg_effect_inference_seal(&p->imported_effects);
	assert(!pg_sources_write(file, &p->synthesis, 5, selected));
	assert(!p->synthesis.steps && !pg_synthesis_result(type));
	pg_program_destroy(p);
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	p = pg_sources_read(file, 10000, &count, &roots);
	assert(p && count == 5 && roots[1] == roots[4] && !p->synthesis.steps);
	for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 1); }
	for (size_t i = 0; i < count; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 3 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	const struct pg_effect_row *row;
	const struct pg_term *value;
	assert(pg_effect_type_view(pg_evidence_subject(pg_synthesis_result(roots[2]))->core, &row, &value));
	assert(!pg_effect_count(row) && value == pg_universe(&p->classifiers, 0));
	assert(!fclose(file));
	pg_program_destroy(p);
}

static void write_sources(FILE *file)
{
	const char text[] = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	const struct pg_source_scope *provider_scope;
	const struct pg_syntax *provider;
	assert(!pg_synthesis_source_input(&p->synthesis, p->root, &provider_scope, &provider));
	struct pg_synthesis_job *reserved = pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider->items[0].expression);
	assert(reserved == pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider->items[0].expression));
	struct pg_synthesis_job *missing = pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider);
	assert(reserved && missing && !pg_synthesis_result(reserved));
	struct pg_token lib = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *namespace = pg_synthesis_module_namespace(&p->synthesis, p->scope, lib, p->root);
	assert(namespace);
	struct pg_synthesis_job *client = parse(p, namespace, "{{ main:=lib.id; }}.main");
	struct pg_token alias = {.kind = PG_TOKEN_IDENT, .text = "alias", .length = 5};
	const struct pg_source_scope *exports = pg_synthesis_name_job(&p->synthesis, p->scope, alias, reserved);
	const struct pg_source_scope *imports = pg_synthesis_import_scope(&p->synthesis, p->scope, exports);
	struct pg_synthesis_job *consumer = parse(p, imports, "{{ import alias; main:=alias; }}.main");
	struct pg_token public = {.kind = PG_TOKEN_IDENT, .text = "public", .length = 6};
	const struct pg_source_scope *published = pg_synthesis_namespace(&p->synthesis, p->scope, public, exports);
	struct pg_synthesis_job *member = parse(p, published, "{{ main:=public.alias; }}.main");
	struct pg_synthesis_job *roots[] = {client, consumer, p->root, client, member, reserved, missing};
	size_t jobs = p->synthesis.jobs.count, scopes = p->synthesis.scopes.count, proofs = p->typing.proofs.count;
	assert(!pg_sources_write(file, &p->synthesis, 7, roots));
	assert(!p->synthesis.steps && p->synthesis.jobs.count == jobs && p->synthesis.scopes.count == scopes);
	assert(p->typing.proofs.count == proofs);
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *definition;
	while (!(definition = pg_synthesis_definition(p->root, id))) {
		assert(p->synthesis.steps < 1000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	assert(!pg_synthesis_result(definition));
	assert(definition == reserved);
	const struct pg_source_scope *ambient;
	const struct pg_syntax *definitions, *expression;
	assert(!pg_synthesis_definition_input(&p->synthesis, definition, &ambient, &definitions, &expression));
	assert(ambient == provider_scope && definitions == provider && expression == provider->items[0].expression);
	/* The source view is independent of accepted/failed progress state. */
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 64); }
	for (size_t i = 0; i < 7; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 6 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	assert(pg_synthesis_definition(p->root, id) == definition);
	assert(!pg_synthesis_definition_input(&p->synthesis, definition, &ambient, &definitions, &expression));
	assert(ambient == provider_scope && definitions == provider && expression == provider->items[0].expression);
	FILE *accepted = tmpfile();
	assert(accepted && !pg_sources_write(accepted, &p->synthesis, 7, roots));
	rewind(file); rewind(accepted);
	int a, b;
	do { a = fgetc(file); b = fgetc(accepted); assert(a == b); } while (a != EOF);
	assert(!ferror(file) && !ferror(accepted) && !fclose(accepted));
	FILE *retained = tmpfile();
	assert(retained);
	struct pg_synthesis_job *evidence = pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(client));
	assert(!pg_sources_write(retained, &p->synthesis, 1, &evidence));
	assert(!fclose(retained));
	pg_program_destroy(p);
}

static void read_sources(FILE *file, uint64_t chunk)
{
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
	assert(p && count == 7 && p->root == roots[0] && roots[0] == roots[3]);
	assert(!p->synthesis.steps && !p->parser.reader.input);
	for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
	for (size_t i = 0; i < count; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 6 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[1]));
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[4]));
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	assert(pg_synthesis_definition(roots[2], id) == roots[5]);
	assert(pg_synthesis_result(roots[5]) == pg_synthesis_result(roots[0]));
	printf("source image: shared modules, explicit imports and ordinary Solve passed (%llu steps)\n", (unsigned long long)p->synthesis.steps);
	pg_program_destroy(p);
}

static void nominal_sources(FILE *file, int writing, uint64_t chunk, int origins)
{
	if (writing) {
		const char text[] = "D:=@{z:*;}; E:=@{z:*;}; d:=D.z; e:=E.z;";
		struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
		assert(p && p->root);
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 64); }
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const char *names[] = {"D", "E", "d", "e"};
		struct pg_synthesis_job *values[4];
		const struct pg_source_scope *scope = p->scope;
		for (size_t i = 0; i < 4; ++i) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = names[i], .length = 1};
			const struct pg_evidence *proof = pg_synthesis_result(pg_synthesis_definition(p->root, name));
			assert(proof);
			values[i] = pg_synthesis_evidence(&p->synthesis, proof);
			scope = pg_synthesis_name_job(&p->synthesis, scope, name, values[i]);
			assert(scope);
		}
		const struct pg_evidence *type = pg_synthesis_result(values[0]);
		const struct pg_operation_declaration *declaration = pg_operation_declaration(&p->typing, type, type);
		const struct pg_evidence *function = pg_prove_operation_function(&p->typing, &p->classifiers, declaration);
		assert(function);
		struct pg_synthesis_job *operation = pg_synthesis_evidence(&p->synthesis, function);
		scope = pg_synthesis_name_job(&p->synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "ask", .length = 3}, operation);
		assert(scope);
		struct pg_synthesis_job *roots[] = {
			parse(p, scope, "{{ main:=d; main::D; }}.main"),
			parse(p, scope, "{{ main:=e; main::D; }}.main"),
			parse(p, scope, "{{ main:=&(ask d); }}.main"), values[0], values[1], values[2], values[3], operation, p->root};
		assert(!pg_sources_write(file, &p->synthesis, origins ? 9 : 8, roots));
		pg_program_destroy(p);
	} else {
		size_t count;
		struct pg_synthesis_job *const *roots;
		struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == (origins ? 9u : 8u) && !p->synthesis.steps);
		if (origins) {
			FILE *pending = tmpfile();
			assert(pending && !pg_sources_write(pending, &p->synthesis, count, roots));
			assert(!p->synthesis.steps);
			pg_program_destroy(p);
			rewind(pending);
			p = pg_sources_read(pending, 10000, &count, &roots);
			assert(p && count == 9 && !p->synthesis.steps);
			assert(!fclose(pending));
		}
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
		for (size_t i = 0; i < count; ++i)
			assert(pg_synthesis_status(roots[i]) == (i == 1 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[5]));
		const struct pg_term *d = pg_evidence_subject(pg_synthesis_result(roots[3]))->core;
		const struct pg_term *e = pg_evidence_subject(pg_synthesis_result(roots[4]))->core;
		assert(d != e);
		assert(pg_evidence_classifier(pg_synthesis_result(roots[5])) == d);
		assert(pg_evidence_classifier(pg_synthesis_result(roots[6])) == e);
		const struct pg_term *computation, *value;
		const struct pg_effect_row *effects;
		assert(pg_thunk_type_view(pg_evidence_classifier(pg_synthesis_result(roots[2])), &computation));
		assert(pg_effect_type_view(computation, &effects, &value));
		assert(pg_effect_count(effects) == 1 && value == d);
		if (origins) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "D", .length = 1};
			const struct pg_evidence *source_type = pg_synthesis_result(pg_synthesis_definition(roots[8], name));
			assert(source_type);
			if (pg_evidence_subject(source_type)->core != d) {
				fputs("source image: source and retained evidence split one nominal declaration\n", stderr);
				pg_program_destroy(p);
				exit(1);
			}
		}
		pg_program_destroy(p);
		puts("source image: nominal external names, distinct declarations and operation wrapper passed");
	}
}

static int find_declaration(void *owner, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job **found = owner;
	assert(!*found);
	*found = job;
	return 0;
}

static void parameter_origins(void)
{
	const char *text = "Box:=&(\\A:@=>\\B:@=>@{mk:A->B->*;});";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	while (p->synthesis.ready) { assert(p->synthesis.steps < 20000); pg_synthesis_advance(&p->synthesis, 1); }
	assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *declaration = NULL;
	assert(!pg_synthesis_visit_declarations(&p->synthesis, find_declaration, &declaration));
	assert(declaration && pg_synthesis_result(declaration));
	struct pg_synthesis_job *selected[] = {p->root,
		pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(declaration))};
	struct pg_synthesis_job *const *roots = selected;
	size_t count = 2;
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 50000, &count, &roots);
		assert(p && count == 2 && !p->synthesis.steps);
		assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
		assert(!fclose(file));
	}
	while (p->synthesis.ready) { assert(p->synthesis.steps < 30000); pg_synthesis_advance(&p->synthesis, 1); }
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
	declaration = NULL;
	assert(!pg_synthesis_visit_declarations(&p->synthesis, find_declaration, &declaration));
	assert(declaration && pg_synthesis_result(declaration) == pg_synthesis_result(roots[1]));
	pg_program_destroy(p);
	puts("source image: dependent parameter scopes retain declaration identity through unsolved resave");
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int origins = !strncmp(argv[1], "origin-", 7);
	int nominal = origins || !strncmp(argv[1], "nominal-", 8);
	int writing = !strcmp(argv[1], "write") || !strcmp(argv[1], "nominal-write") || !strcmp(argv[1], "origin-write");
	if (writing) { definition_boundaries(); rule_environments(); parameter_origins(); }
	FILE *file = fopen(argv[2], writing ? "w+b" : "rb");
	assert(file);
	if (nominal) nominal_sources(file, writing, !strcmp(argv[1], "nominal-read-bulk") ? 64 : 1, origins);
	else if (writing) write_sources(file);
	else read_sources(file, !strcmp(argv[1], "read-bulk") ? 64 : 1);
	assert(!fclose(file));
	return 0;
}
