#include "source_io.h"
#include "syntax_io.h"
#include "dag.h"
#include "wire.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include <string.h>

static const char magic[8] = "APGSRC\7";
enum environment_kind { ROOT, NAME, MODULE, NAMESPACE, IMPORTS, DEFINITIONS, BINDING };

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
	if (input.binding) {
		const struct pg_object *binder;
		if (pg_synthesis_binding_input(synthesis, input.binding, &output->parent, &output->syntax, &binder)) return -1;
		output->kind = BINDING; output->rule = pg_synthesis_allocation_origin(input.binding);
		return 0;
	}
	if (input.definitions) {
		output->kind = DEFINITIONS; output->definitions = input.definitions;
		return 0;
	}
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

static int producer_child(void *owner, const void *key, size_t index, const void **result)
{
	const struct pg_synthesis *const *synthesis = owner;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job *term, *type;
	if (index >= 2 || pg_synthesis_source_expect_input(*synthesis, key, &scope, &term, &type)) return 0;
	*result = index ? type : term;
	return 1;
}

static uint64_t id(const struct pg_dag *dag, const void *key)
{
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? node->id : 0;
}

struct origin_collection {
	const struct pg_synthesis *synthesis;
	struct pg_dag *scopes, *syntax, *rules, *origins;
	struct pg_dag objects, terms;
	struct pg_index candidates;
	struct pg_declaration_io *codec;
	const struct pg_dag_node *last_scope;
};

struct origin_candidate {
	struct pg_index_entry index;
	const struct pg_object *object;
	struct pg_synthesis_job *job;
};

static int index_origin(void *owner, struct pg_synthesis_job *job)
{
	struct origin_collection *c = owner;
	const struct pg_object *object = pg_synthesis_allocation_object(job);
	if (!object) return -1;
	uint64_t hash = (uintptr_t)object;
	for (struct pg_index_entry *entry = pg_index_candidates(&c->candidates, hash); entry; entry = entry->next) {
		const struct origin_candidate *candidate = (const void *)entry;
		if (candidate->object == object && candidate->job == job) return 0;
	}
	struct origin_candidate *candidate = pg_alloc(&c->objects.storage, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->object = object; candidate->job = job;
	return pg_index_insert(&c->candidates, &candidate->index, hash);
}

static int collect_origin(void *owner, struct pg_synthesis_job *job)
{
	struct origin_collection *c = owner;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	const struct pg_object *binder = NULL;
	if (pg_synthesis_source_input(c->synthesis, job, &scope, &syntax)) {
		if (pg_synthesis_binding_input(c->synthesis, job, &scope, &syntax, &binder)) return -1;
		scope = pg_synthesis_binding_scope(job);
	}
	if (!id(c->syntax, syntax)) return 0;
	/* Only lexical descendants of selected source roots belong to the image. */
	const struct pg_source_scope *parent = scope;
	while (!id(c->scopes, parent)) {
		struct pg_source_environment input;
		if (pg_synthesis_environment_input(c->synthesis, parent, &input)) return -1;
		const struct pg_syntax *site = input.definitions;
		if (input.binding) {
			const struct pg_object *binder;
			if (pg_synthesis_binding_input(c->synthesis, input.binding, &input.parent, &site, &binder)) return -1;
		}
		if (!site || !id(c->syntax, site)) return 0;
		parent = input.parent;
		if (!parent) return 0;
	}
	if (pg_dag_add(c->scopes, scope) || pg_dag_add(c->rules, pg_synthesis_allocation_origin(job))
		|| (!binder && pg_dag_add(c->origins, job))) return -1;
	return 0;
}

static int retain_dependencies(void *owner, const struct pg_derivation_input *input,
	const struct pg_effect_inference *work)
{
	struct origin_collection *c = owner;
	const struct pg_dag_node *previous = c->objects.last;
	const struct pg_term *slots[PG_DERIVATION_TERM_SLOTS];
	const struct pg_term *const *terms = slots;
	size_t count = PG_DERIVATION_TERM_SLOTS, equations;
	if (input) {
		if (pg_derivation_input_terms(&c->terms.storage, input, slots)) return -1;
	} else if (pg_effect_inference_pack(work, &c->terms.storage, &equations, &count, &terms)) return -1;
	for (size_t i = 0; i < count; ++i)
		if (terms[i] && pg_dag_add(&c->terms, terms[i])) return -1;
	for (const struct pg_dag_node *node = previous ? previous->next : c->objects.first; node; node = node->next) {
		uint64_t hash = (uintptr_t)node->key;
		for (struct pg_index_entry *entry = pg_index_candidates(&c->candidates, hash); entry; entry = entry->next) {
			const struct origin_candidate *candidate = (const void *)entry;
			if (candidate->object == node->key && collect_origin(c, candidate->job)) return -1;
		}
	}
	for (const struct pg_dag_node *node = c->last_scope ? c->last_scope->next : c->scopes->first;
		node; c->last_scope = node, node = node->next) {
		struct environment input;
		if (environment(c->synthesis, node->key, &input)) return -1;
		if (input.syntax && pg_dag_add(c->syntax, input.syntax)) return -1;
		if (input.definitions && pg_dag_add(c->syntax, input.definitions)) return -1;
		if (input.rule && pg_dag_add(c->rules, input.rule)) return -1;
	}
	return 0;
}

int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots)
{
	if (!file || !synthesis || (count && !roots)) return -1;
	struct pg_dag scopes = {0}, syntax = {0}, rules = {0}, origins = {0}, producers = {0};
	struct pg_effect_inference effects = {0};
	struct pg_declaration_io codec = {0};
	struct origin_collection collection = {.synthesis = synthesis, .scopes = &scopes,
		.syntax = &syntax, .rules = &rules, .origins = &origins, .codec = &codec};
	int status = -1;
	/* Callbacks only inspect synthesis; no solver entry is invoked. */
	if (pg_dag_init(&scopes, child, &synthesis) || pg_dag_init(&syntax, pg_syntax_child, NULL)
		|| pg_dag_init(&producers, producer_child, &synthesis)
		|| pg_dag_init(&rules, NULL, NULL) || pg_dag_init(&origins, NULL, NULL)
		|| pg_dag_init(&collection.objects, NULL, NULL) || pg_index_init(&collection.candidates)
		|| pg_effect_inference_init(&effects, &rules.storage)
		|| pg_declaration_io_init(&codec, synthesis->typing, synthesis->classifiers)) goto done;
	if (pg_graph_dependencies_init(&collection.terms, &collection.objects, &pg_declaration_graph_codec, &codec)) goto done;
	for (size_t i = 0; i < count; ++i) if (!roots[i] || pg_dag_add(&producers, roots[i])) goto done;
	for (const struct pg_dag_node *node = producers.first; node; node = node->next) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term, *definitions;
		struct pg_synthesis_job *left, *right;
		if (!pg_synthesis_source_expect_input(synthesis, node->key, &scope, &left, &right)) {
			if (pg_dag_add(&scopes, scope)) goto done;
			continue;
		}
		if (source_input(synthesis, node->key, &scope, &term, &definitions)) {
			if (pg_dag_add(&rules, node->key)) goto done;
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
	collection.last_scope = scopes.last;
	if (pg_synthesis_visit_source_allocations(synthesis, index_origin, &collection)) goto done;
	const struct pg_derivation_input *const *derivations;
	if (pg_synthesis_export_rule_closure(synthesis, &rules, &rules.storage, &effects, 1,
		retain_dependencies, &collection, &derivations)) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, synthesis->definition_policy)
		|| pg_wire_write_u64(file, scopes.count) || pg_wire_write_u64(file, count)
		|| pg_wire_write_u64(file, origins.count) || pg_wire_write_u64(file, producers.count)) goto done;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		uint64_t words[] = {input.kind, id(&scopes, input.parent), id(&scopes, input.target),
			id(&syntax, input.syntax), id(&syntax, input.definitions), input.name.kind, input.name.length, id(&rules, input.rule)};
		for (size_t i = 0; i < 8; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
		if (input.name.length && fwrite(input.name.text, 1, input.name.length, file) != input.name.length) goto done;
	}
	for (size_t i = 0; i < count; ++i) if (pg_wire_write_u64(file, id(&producers, roots[i]))) goto done;
	for (const struct pg_dag_node *node = producers.first; node; node = node->next) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term, *definitions;
		struct pg_synthesis_job *left, *right;
		uint64_t words[6] = {0};
		if (!pg_synthesis_source_expect_input(synthesis, node->key, &scope, &left, &right)) {
			words[0] = id(&scopes, scope);
			words[4] = id(&producers, left); words[5] = id(&producers, right);
		} else {
			(void)source_input(synthesis, node->key, &scope, &term, &definitions);
			words[0] = id(&scopes, scope); words[1] = id(&syntax, term);
			words[2] = id(&syntax, definitions); words[3] = id(&rules, node->key);
		}
		for (size_t i = 0; i < 6; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
	}
	for (const struct pg_dag_node *node = origins.first; node; node = node->next) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term;
		if (pg_synthesis_source_input(synthesis, node->key, &scope, &term)) goto done;
		if (pg_wire_write_u64(file, id(&scopes, scope)) || pg_wire_write_u64(file, id(&syntax, term))
			|| pg_wire_write_u64(file, id(&rules, pg_synthesis_allocation_origin(node->key)))) goto done;
	}
	if (syntax.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_syntax **terms = pg_alloc(&syntax.storage, syntax.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = syntax.first; node; node = node->next) terms[node->id - 1] = node->key;
	if (pg_syntax_write(file, syntax.count, terms)) goto done;
	status = pg_derivation_inputs_write_inference(file, rules.count, derivations, &effects, &pg_declaration_graph_codec, &codec);
done:
	pg_index_destroy(&collection.candidates); pg_dag_destroy(&collection.terms); pg_dag_destroy(&collection.objects);
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects); pg_dag_destroy(&rules); pg_dag_destroy(&origins);
	pg_dag_destroy(&syntax); pg_dag_destroy(&scopes); pg_dag_destroy(&producers);
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
	uint64_t policy, n, nr, no, np;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &policy) || policy > PG_DEFINITION_EXPLICIT_THUNK) return NULL;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr) || pg_wire_read_u64(file, &no)
		|| pg_wire_read_u64(file, &np)) return NULL;
	if (n > limit || nr > limit - n || limit > SIZE_MAX / sizeof(struct record)) return NULL;
	if (no > (limit - n - nr) / 3) return NULL;
	if (np > (limit - n - nr - 3 * no) / 6) return NULL;
	struct pg_program *program = pg_program_allocate((enum pg_definition_policy)policy);
	if (!program) return NULL;
	struct pg_declaration_io codec = {0};
	if (pg_declaration_io_init(&codec, &program->typing, &program->classifiers)) goto fail;
	struct pg_graph *graph = &program->graph;
	struct record *records = pg_alloc(graph, (size_t)n * sizeof(*records));
	const struct pg_source_scope **scopes = pg_alloc(graph, (size_t)n * sizeof(*scopes));
	uint64_t *ids = pg_alloc(graph, (size_t)np * 6 * sizeof(*ids));
	uint64_t *selections = pg_alloc(graph, (size_t)nr * sizeof(*selections));
	uint64_t *origin_ids = pg_alloc(graph, (size_t)no * 3 * sizeof(*origin_ids));
	struct pg_synthesis_job **jobs = pg_alloc(graph, (size_t)nr * sizeof(*jobs));
	struct pg_synthesis_job **producers = pg_alloc(graph, (size_t)np * sizeof(*producers));
	if (!records || !scopes || !ids || !jobs || !origin_ids || !selections || !producers) goto fail;
	size_t remaining = limit - (size_t)n - (size_t)nr - 3 * (size_t)no - 6 * (size_t)np;
	for (size_t i = 0; i < n; ++i) {
		uint64_t w[8];
		for (size_t j = 0; j < 8; ++j) if (pg_wire_read_u64(file, &w[j])) goto fail;
		if (w[0] > BINDING || w[1] > i || w[2] > i || w[5] > PG_TOKEN_ERROR || w[6] > remaining) goto fail;
		char *name = pg_alloc(graph, (size_t)w[6]);
		if (!name || fread(name, 1, (size_t)w[6], file) != w[6]) goto fail;
		remaining -= (size_t)w[6];
		records[i] = (struct record){w[0], w[1], w[2], w[3], w[4], w[7],
			{.kind = w[5], .text = name, .length = w[6], .text_length = w[6]}};
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &selections[i]) || !selections[i] || selections[i] > np) goto fail;
	for (size_t i = 0; i < 6 * np; ++i) if (pg_wire_read_u64(file, &ids[i])) goto fail;
	for (size_t i = 0; i < 3 * no; ++i) if (pg_wire_read_u64(file, &origin_ids[i])) goto fail;
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
		if (r->kind == BINDING) {
			if (!parent || target || !r->syntax || !r->rule || r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_binding_scope(pg_synthesis_restore_binding(s, parent,
				terms[r->syntax - 1], rules[r->rule - 1]));
			if (!scopes[i]) goto fail;
			continue;
		}
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
		if (r->kind == DEFINITIONS) {
			if (!parent || target || r->syntax || !r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_definition_scope(s, parent, terms[r->definitions - 1]);
			if (!scopes[i]) goto fail;
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
	for (size_t i = 0; i < no; ++i) {
		uint64_t scope = origin_ids[3 * i], syntax = origin_ids[3 * i + 1], rule = origin_ids[3 * i + 2];
		if (!scope || scope > n || !syntax || syntax > nt || !rule || rule > nd) goto fail;
		if (!pg_synthesis_restore_declaration(&program->synthesis, scopes[scope - 1], terms[syntax - 1],
			rules[rule - 1])) goto fail;
	}
	for (size_t i = 0; i < np; ++i) {
		uint64_t scope = ids[6 * i], syntax = ids[6 * i + 1], definitions = ids[6 * i + 2], rule = ids[6 * i + 3];
		uint64_t left = ids[6 * i + 4], right = ids[6 * i + 5];
		if (left || right) {
			if (!left || left > i || !right || right > i || !scope || scope > n || syntax || definitions || rule) goto fail;
			producers[i] = pg_synthesis_source_expect(&program->synthesis, scopes[scope - 1],
				producers[left - 1], producers[right - 1]);
			if (!producers[i]) goto fail;
			continue;
		}
		if (rule) {
			if (rule > nd || scope || syntax || definitions) goto fail;
			producers[i] = rules[rule - 1];
			continue;
		}
		if (!scope || scope > n || !syntax || syntax > nt || definitions > nt) goto fail;
		producers[i] = definitions ? pg_synthesis_definition_request(&program->synthesis,
			scopes[scope - 1], terms[definitions - 1], terms[syntax - 1])
			: pg_synthesis_request(&program->synthesis, scopes[scope - 1], terms[syntax - 1]);
		if (!producers[i]) goto fail;
	}
	for (size_t i = 0; i < nr; ++i) jobs[i] = producers[selections[i] - 1];
	program->root = nr ? jobs[0] : NULL;
	*count = (size_t)nr; *roots = jobs;
	pg_declaration_io_destroy(&codec);
	return program;
fail:
	pg_declaration_io_destroy(&codec);
	pg_program_destroy(program);
	return NULL;
}
