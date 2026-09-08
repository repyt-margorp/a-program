#include "source_io.h"
#include "syntax_io.h"
#include "dag.h"
#include "wire.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include <string.h>

static const char magic[8] = "APGSRC\3";
enum environment_kind { ROOT, NAME, MODULE, NAMESPACE, IMPORTS };

struct environment {
	enum environment_kind kind;
	struct pg_token name;
	const struct pg_source_scope *parent, *target;
	const struct pg_syntax *syntax, *definitions;
	struct pg_synthesis_job *rule;
};

static int source_input(const struct pg_synthesis *synthesis, const struct pg_synthesis_job *job,
	const struct pg_source_scope **scope, const struct pg_syntax **syntax, const struct pg_syntax **definitions)
{
	*definitions = NULL;
	*scope = NULL; *syntax = NULL;
	if (!pg_synthesis_source_input(synthesis, job, scope, syntax)) return 0;
	return pg_synthesis_definition_input(synthesis, job, scope, definitions, syntax);
}

static int environment(const struct pg_synthesis *synthesis, const struct pg_source_scope *scope,
	struct environment *output)
{
	struct pg_source_environment input;
	if (pg_synthesis_environment_input(synthesis, scope, &input)) return -1;
	*output = (struct environment){.parent = input.parent, .name = input.name};
	struct pg_synthesis_job *job = input.producer ? input.producer : input.module;
	if (job) {
		output->kind = input.producer ? NAME : MODULE;
		if (!source_input(synthesis, job, &output->target, &output->syntax, &output->definitions)) return 0;
		if (input.module) return -1;
		output->rule = job;
		return 0;
	}
	if (input.exports) { output->kind = NAMESPACE; output->target = input.exports; }
	else if (input.imports) { output->kind = IMPORTS; output->target = input.imports; }
	else if (input.parent) return -1;
	return 0;
}

static int child(void *owner, const void *key, size_t index, const void **result)
{
	if (index == 2) return 0;
	struct environment input;
	const struct pg_synthesis *const *synthesis = owner;
	if (environment(*synthesis, key, &input)) return -1;
	*result = index ? input.target : input.parent;
	return *result ? 1 : 2;
}

static uint64_t id(const struct pg_dag *dag, const void *key)
{
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? node->id : 0;
}

int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots)
{
	if (!file || !synthesis || (count && !roots)) return -1;
	struct pg_dag scopes = {0}, syntax = {0}, rules = {0};
	struct pg_effect_inference effects = {0};
	struct pg_declaration_io codec = {0};
	int status = -1;
	/* Callbacks only inspect synthesis; no solver entry is invoked. */
	if (pg_dag_init(&scopes, child, &synthesis) || pg_dag_init(&syntax, NULL, NULL)
		|| pg_dag_init(&rules, NULL, NULL) || pg_effect_inference_init(&effects, &rules.storage)
		|| pg_declaration_io_init(&codec, synthesis->typing, synthesis->classifiers)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term, *definitions;
		if (source_input(synthesis, roots[i], &scope, &term, &definitions)) {
			if (pg_dag_add(&rules, roots[i])) goto done;
			continue;
		}
		if (pg_dag_add(&scopes, scope) || pg_dag_add(&syntax, term)) goto done;
		if (definitions && pg_dag_add(&syntax, definitions)) goto done;
	}
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		if (input.syntax && pg_dag_add(&syntax, input.syntax)) goto done;
		if (input.definitions && pg_dag_add(&syntax, input.definitions)) goto done;
		if (input.rule && pg_dag_add(&rules, input.rule)) goto done;
	}
	if (rules.count > SIZE_MAX / sizeof(void *)) goto done;
	struct pg_synthesis_job **producers = pg_alloc(&rules.storage, rules.count * sizeof(*producers));
	if (!producers) goto done;
	for (const struct pg_dag_node *node = rules.first; node; node = node->next) producers[node->id - 1] = (void *)node->key;
	const struct pg_derivation_input *const *derivations;
	if (pg_synthesis_export_rules(synthesis, rules.count, producers, &rules.storage, &effects, 1, &derivations)) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, synthesis->definition_policy)
		|| pg_wire_write_u64(file, scopes.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		uint64_t words[] = {input.kind, id(&scopes, input.parent), id(&scopes, input.target),
			id(&syntax, input.syntax), id(&syntax, input.definitions), input.name.kind, input.name.length, id(&rules, input.rule)};
		for (size_t i = 0; i < 8; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
		if (input.name.length && fwrite(input.name.text, 1, input.name.length, file) != input.name.length) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term, *definitions;
		(void)source_input(synthesis, roots[i], &scope, &term, &definitions);
		if (pg_wire_write_u64(file, id(&scopes, scope)) || pg_wire_write_u64(file, id(&syntax, term))
			|| pg_wire_write_u64(file, id(&syntax, definitions)) || pg_wire_write_u64(file, id(&rules, roots[i]))) goto done;
	}
	if (syntax.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_syntax **terms = pg_alloc(&syntax.storage, syntax.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = syntax.first; node; node = node->next) terms[node->id - 1] = node->key;
	if (pg_syntax_write(file, syntax.count, terms)) goto done;
	status = pg_derivation_inputs_write_inference(file, rules.count, derivations, &effects, &pg_declaration_graph_codec, &codec);
done:
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects); pg_dag_destroy(&rules);
	pg_dag_destroy(&syntax); pg_dag_destroy(&scopes);
	return status;
}

struct record {
	uint64_t kind, parent, target, syntax, definitions, rule;
	struct pg_token name;
};

struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots)
{
	if (!file || !count || !roots) return NULL;
	char header[8];
	uint64_t policy, n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &policy) || policy > PG_DEFINITION_EXPLICIT_THUNK) return NULL;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return NULL;
	if (n > limit || nr > (limit - n) / 4 || limit > SIZE_MAX / sizeof(struct record)) return NULL;
	struct pg_program *program = pg_program_allocate((enum pg_definition_policy)policy);
	if (!program) return NULL;
	struct pg_declaration_io codec = {0};
	if (pg_declaration_io_init(&codec, &program->typing, &program->classifiers)) goto fail;
	struct pg_graph *graph = &program->graph;
	struct record *records = pg_alloc(graph, (size_t)n * sizeof(*records));
	const struct pg_source_scope **scopes = pg_alloc(graph, (size_t)n * sizeof(*scopes));
	uint64_t *ids = pg_alloc(graph, (size_t)nr * 4 * sizeof(*ids));
	struct pg_synthesis_job **jobs = pg_alloc(graph, (size_t)nr * sizeof(*jobs));
	if (!records || !scopes || !ids || !jobs) goto fail;
	size_t remaining = limit - (size_t)n - 4 * (size_t)nr;
	for (size_t i = 0; i < n; ++i) {
		uint64_t w[8];
		for (size_t j = 0; j < 8; ++j) if (pg_wire_read_u64(file, &w[j])) goto fail;
		if (w[0] > IMPORTS || w[1] > i || w[2] > i || w[5] > PG_TOKEN_ERROR || w[6] > remaining) goto fail;
		char *name = pg_alloc(graph, (size_t)w[6]);
		if (!name || fread(name, 1, (size_t)w[6], file) != w[6]) goto fail;
		remaining -= (size_t)w[6];
		records[i] = (struct record){w[0], w[1], w[2], w[3], w[4], w[7],
			{.kind = w[5], .text = name, .length = w[6], .text_length = w[6]}};
	}
	for (size_t i = 0; i < 4 * nr; ++i) if (pg_wire_read_u64(file, &ids[i])) goto fail;
	size_t nt;
	const struct pg_syntax *const *terms;
	if (pg_syntax_read(file, graph, limit, &nt, &terms) || pg_syntax_validate(nt, terms)) goto fail;
	size_t nd;
	const struct pg_derivation_input *const *derivations;
	if (pg_derivations_read_inference(file, graph, limit, limit, &program->imported_effects,
		&pg_declaration_graph_codec, &codec, &nd, &derivations)) goto fail;
	if (fgetc(file) != EOF || ferror(file)) goto fail;
	if (nd > SIZE_MAX / sizeof(void *)) goto fail;
	struct pg_synthesis_job **rules = pg_alloc(graph, nd * sizeof(*rules));
	if (!rules) goto fail;
	pg_effect_inference_seal(&program->imported_effects);
	for (size_t i = 0; i < nd; ++i) {
		rules[i] = pg_synthesis_derivation_inference(&program->synthesis, derivations[i], &program->imported_effects);
		if (!rules[i]) goto fail;
	}
	for (size_t i = 0; i < n; ++i) {
		const struct record *r = &records[i];
		if (r->syntax > nt || r->definitions > nt || r->rule > nd) goto fail;
		const struct pg_source_scope *parent = r->parent ? scopes[r->parent - 1] : NULL;
		const struct pg_source_scope *target = r->target ? scopes[r->target - 1] : NULL;
		struct pg_synthesis *s = &program->synthesis;
		if (r->rule) {
			if (r->kind != NAME || !parent || target || r->syntax || r->definitions) goto fail;
			scopes[i] = pg_synthesis_name_job(s, parent, r->name, rules[r->rule - 1]);
			if (!scopes[i]) goto fail;
			continue;
		}
		if (r->kind == ROOT) {
			if (parent || target || r->syntax || r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = program->scope;
			continue;
		}
		if (!parent || !target) goto fail;
		if (r->kind == NAME || r->kind == MODULE) {
			if (!r->syntax) goto fail;
			struct pg_synthesis_job *job = r->definitions
				? pg_synthesis_definition_request(s, target, terms[r->definitions - 1], terms[r->syntax - 1])
				: pg_synthesis_request(s, target, terms[r->syntax - 1]);
			scopes[i] = r->kind == NAME ? pg_synthesis_name_job(s, parent, r->name, job)
				: pg_synthesis_module_namespace(s, parent, r->name, job);
		} else {
			if (r->syntax || r->definitions) goto fail;
			if (r->kind == IMPORTS) {
				if (r->name.kind || r->name.length) goto fail;
				scopes[i] = pg_synthesis_import_scope(s, parent, target);
			} else scopes[i] = pg_synthesis_namespace(s, parent, r->name, target);
		}
		if (!scopes[i]) goto fail;
	}
	for (size_t i = 0; i < nr; ++i) {
		uint64_t scope = ids[4 * i], syntax = ids[4 * i + 1], definitions = ids[4 * i + 2], rule = ids[4 * i + 3];
		if (rule) {
			if (rule > nd || scope || syntax || definitions) goto fail;
			jobs[i] = rules[rule - 1];
			continue;
		}
		if (!scope || scope > n || !syntax || syntax > nt || definitions > nt) goto fail;
		jobs[i] = definitions ? pg_synthesis_definition_request(&program->synthesis,
			scopes[scope - 1], terms[definitions - 1], terms[syntax - 1])
			: pg_synthesis_request(&program->synthesis, scopes[scope - 1], terms[syntax - 1]);
		if (!jobs[i]) goto fail;
	}
	program->root = nr ? jobs[0] : NULL;
	*count = (size_t)nr; *roots = jobs;
	pg_declaration_io_destroy(&codec);
	return program;
fail:
	pg_declaration_io_destroy(&codec);
	pg_program_destroy(program);
	return NULL;
}
