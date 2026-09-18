#include "source_io.h"
#include "iadt.h"
#include "syntax_io.h"
#include "dag.h"
#include "wire.h"
#include "derivation_io.h"
#include "declaration_io.h"
#include "retained_io.h"
#include "context_payload.h"
#include <string.h>

static const char magic[8] = "APGSRC\76";
static const char retained_magic[8] = "APGSRC\77";
enum environment_kind { ROOT, NAME, MODULE, NAMESPACE, IMPORTS, DEFINITIONS, BINDING, CONTEXT_BINDING, HANDLER_SCOPE, HANDLER_BINDING, GRAPH_BINDING };

struct environment {
	enum environment_kind kind;
	struct pg_token name;
	const struct pg_source_scope *parent, *target;
	const struct pg_syntax *syntax, *definitions;
	struct pg_synthesis_job *rule;
	struct pg_synthesis_job *producer;
	const struct pg_object *binder;
	struct pg_handler_clause_input allocation;
	unsigned slot;
};

/* One physical transport for named wrappers; their typing rules stay distinct. */
struct callable_input {
	struct pg_synthesis_job *left, *right;
	const struct pg_object *reference;
	const struct pg_context *prefix, *fields;
	int allocated, operation;
};

static int callable_input(const struct pg_synthesis *synthesis, const struct pg_synthesis_job *job,
	struct callable_input *input)
{
	struct pg_constructor_input constructor;
	if (!pg_synthesis_constructor_input(synthesis, job, &constructor)) {
		*input = (struct callable_input){constructor.formation, constructor.parameters, constructor.constructor,
			constructor.prefix, constructor.fields, constructor.allocated, 0};
		return 0;
	}
	struct pg_operation_input operation;
	if (pg_synthesis_operation_input(synthesis, job, &operation)) return -1;
	*input = (struct callable_input){operation.payload, operation.response, operation.label,
		NULL, operation.allocation, operation.allocation != NULL, 1};
	return 0;
}

static int allocation_input(const struct pg_synthesis *synthesis, const struct pg_synthesis_job *job,
	const struct pg_context **prefix, const struct pg_context **end, const struct pg_object **reference)
{
	if (!pg_synthesis_member_allocation(synthesis, job, prefix, end)) {
		if (*prefix == *end) return -1;
		*reference = (*end)->binder;
		return 0;
	}
	struct callable_input input;
	if (callable_input(synthesis, job, &input)) return -1;
	*prefix = input.prefix; *end = input.fields; *reference = input.reference;
	return 0;
}

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
	struct pg_handler_binding_input binding;
	int has_binding = pg_synthesis_handler_binding_input(synthesis, scope, &binding);
	if (has_binding < 0) return -1;
	if (has_binding) {
		output->kind = HANDLER_BINDING;
		output->target = binding.parent;
		output->syntax = binding.clause;
		output->definitions = binding.handler;
		output->producer = binding.allocation.operation;
		output->allocation = binding.allocation;
		output->slot = binding.slot;
		return 0;
	}
	if (input.handler) {
		output->kind = HANDLER_SCOPE; output->syntax = input.handler;
		return 0;
	}
	if (input.context) {
		output->kind = input.association == PG_SOURCE_GRAPH ? GRAPH_BINDING : CONTEXT_BINDING;
		output->rule = input.context;
		output->target = input.associated;
		return 0;
	}
	if (input.binding) {
		if (pg_synthesis_binding_input(synthesis, input.binding, &output->parent, &output->syntax, &output->binder)) return -1;
		output->kind = BINDING;
		return 0;
	}
	if (input.definitions) {
		output->kind = DEFINITIONS; output->definitions = input.definitions;
		return 0;
	}
	struct pg_synthesis_job *job = input.producer ? input.producer : input.module;
	if (job) {
		output->kind = input.producer ? NAME : MODULE;
		output->producer = job;
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
	struct callable_input callable;
	enum pg_reduction_kind kind;
	if (!callable_input(*synthesis, key, &callable)) {
		if (index >= 2) return 0;
		*result = index ? callable.right : callable.left;
		return 1;
	}
	if (!pg_synthesis_normalization_input(*synthesis, key, &term, &type, &kind, NULL)) {
		if (index >= 2) return 0;
		*result = index ? type : term;
		return 1;
	}
	if (!pg_synthesis_source_expect_input(*synthesis, key, &scope, &term, &type)) {
		if (index >= 2) return 0;
		*result = index ? type : term;
		return 1;
	}
	const struct pg_syntax_item *item;
	if (pg_synthesis_definition_entry(key, index, &item, &term) != 1) return 0;
	*result = term;
	return term ? 1 : 2;
}

static uint64_t id(const struct pg_dag *dag, const void *key)
{
	const struct pg_dag_node *node = pg_dag_find(dag, key);
	return node ? node->id : 0;
}

struct source_member {
	struct source_member *next;
	const struct pg_synthesis_job *declaration;
	size_t index;
	struct pg_constructor_allocation allocation;
};

struct origin_collection {
	const struct pg_synthesis *synthesis;
	struct pg_dag *scopes, *syntax, *rules, *origins, *producers, *allocations, *bindings, *matches, *declarations;
	struct pg_dag objects, terms, contexts;
	struct pg_index candidates;
	struct pg_declaration_io *codec;
	const struct pg_dag_node *last_scope, *last_producer, *last_syntax, *last_object;
	struct source_member *members;
	size_t member_count;
};

static int index_scope_origin(void *owner, struct pg_synthesis_job *job);
static int collect_candidates(struct origin_collection *c, const void *key);

static int collect_allocation(struct origin_collection *c, const struct pg_context *prefix,
	const struct pg_context *fields, const struct pg_object *constructor)
{
	if (pg_context_collect(&c->terms, &c->contexts, prefix) ||
		pg_context_collect(&c->terms, &c->contexts, fields)) return -1;
	return pg_dag_add(&c->terms, pg_reference(&c->rules->storage, constructor));
}

static int collect_member_use(struct origin_collection *c, struct pg_synthesis_job *job,
	const struct pg_context *prefix, const struct pg_context *fields)
{
	if (id(c->allocations, job)) return 0;
	if (!fields || prefix == fields || pg_dag_add(c->origins, job) || pg_dag_add(c->allocations, job)) return -1;
	return collect_allocation(c, prefix, fields, fields->binder);
}

/* Drain newly discovered immutable inputs once, including edges from lexical
 * names to prepared producers and back to their defining scopes. */
static int collect_inputs(struct origin_collection *c)
{
	for (;;) {
		const struct pg_dag_node *producer = c->last_producer ? c->last_producer->next : c->producers->first;
		const struct pg_dag_node *scope_node = c->last_scope ? c->last_scope->next : c->scopes->first;
		if (!producer && !scope_node) return 0;
		for (; producer; c->last_producer = producer, producer = producer->next) {
			const struct pg_source_scope *scope;
			const struct pg_syntax *term, *definitions;
			struct pg_synthesis_job *left, *right;
			struct callable_input callable;
			enum pg_reduction_kind kind;
			if (!callable_input(c->synthesis, producer->key, &callable)) {
				if (pg_dag_add(c->allocations, producer->key)) return -1;
				if (collect_allocation(c, callable.prefix, callable.fields, callable.reference)) return -1;
				continue;
			}
			if (!pg_synthesis_normalization_input(c->synthesis, producer->key, &left, &right, &kind, NULL))
				continue;
			if (!pg_synthesis_source_expect_input(c->synthesis, producer->key, &scope, &left, &right)) {
				if (pg_dag_add(c->scopes, scope)) return -1;
			} else if (source_input(c->synthesis, producer->key, &scope, &term, &definitions)) {
				if (pg_dag_add(c->rules, producer->key)) return -1;
			} else {
				if (pg_dag_add(c->scopes, scope) || pg_dag_add(c->syntax, term)) return -1;
				if (definitions && pg_dag_add(c->syntax, definitions)) return -1;
				const struct pg_context *prefix, *fields;
				if (!pg_synthesis_member_allocation(c->synthesis, producer->key, &prefix, &fields) && fields != prefix)
					if (collect_member_use(c, (void *)producer->key, prefix, fields)) return -1;
				if (!pg_synthesis_source_input(c->synthesis, producer->key, &scope, &term)
					&& pg_syntax_constructors(term)) {
					for (size_t i = 0; i < pg_syntax_constructors(term)->item_count; ++i) {
						struct pg_constructor_allocation allocation;
						int available = pg_synthesis_declaration_member_input(c->synthesis, producer->key, i, &allocation);
						if (available < 0) return -1;
						if (!available) continue;
						struct source_member *member = pg_alloc(&c->rules->storage, sizeof(*member));
						if (!member || c->member_count == SIZE_MAX) return -1;
						*member = (struct source_member){c->members, producer->key, i, allocation};
						c->members = member; ++c->member_count;
						if (collect_allocation(c, allocation.prefix, allocation.fields, allocation.constructor)) return -1;
					}
				}
			}
		}
		/* Producer traversal can have appended the first scope. */
		scope_node = c->last_scope ? c->last_scope->next : c->scopes->first;
		for (; scope_node; c->last_scope = scope_node, scope_node = scope_node->next) {
			struct environment input;
			if (environment(c->synthesis, scope_node->key, &input)) return -1;
			if (collect_candidates(c, scope_node->key)) return -1;
			if (pg_synthesis_visit_source_references(c->synthesis, scope_node->key, index_scope_origin, NULL, c)) return -1;
			if (input.syntax && pg_dag_add(c->syntax, input.syntax)) return -1;
			if (input.definitions && pg_dag_add(c->syntax, input.definitions)) return -1;
			if (input.rule && pg_dag_add(c->rules, input.rule)) return -1;
			if (input.producer && pg_dag_add(c->producers, input.producer)) return -1;
			if (input.binder && pg_dag_add(&c->terms, pg_reference(&c->rules->storage, input.binder))) return -1;
			if (input.kind == HANDLER_BINDING) {
				const struct pg_object *binders[] = {input.allocation.payload, input.allocation.resume, input.allocation.response};
				for (size_t i = 0; i < 3; ++i)
					if (pg_dag_add(&c->terms, pg_reference(&c->rules->storage, binders[i]))) return -1;
			}
		}
	}
}

struct origin_candidate {
	struct origin_candidate *next;
	struct pg_synthesis_job *job;
	const struct pg_source_binding *binding;
};

struct origin_dependency {
	struct pg_index_entry index;
	const void *key;
	struct origin_candidate *first;
};

static struct origin_dependency *origin_dependency(struct origin_collection *c, const void *key)
{
	for (struct pg_index_entry *entry = pg_index_candidates(&c->candidates, (uintptr_t)key); entry; entry = entry->next) {
		struct origin_dependency *dependency = (void *)entry;
		if (dependency->key == key) return dependency;
	}
	return NULL;
}

static int collect_binding(struct origin_collection *c, const struct pg_source_binding *input);
static int collect_origin(void *owner, struct pg_synthesis_job *job);

static int index_origin_reference(struct origin_collection *c, struct pg_synthesis_job *job,
	const struct pg_source_binding *binding, const void *key)
{
	if (!key) return -1;
	const struct pg_dag_node *reached = pg_dag_find(&c->objects, key);
	if (reached && c->last_object && reached->id <= c->last_object->id)
		return binding ? collect_binding(c, binding) : collect_origin(c, job);
	/* Distinct input edges may await the same syntax; no global rescan is needed. */
	struct origin_dependency *dependency = origin_dependency(c, key);
	if (!dependency) {
		dependency = pg_alloc(&c->objects.storage, sizeof(*dependency));
		if (!dependency) return -1;
		dependency->key = key;
		if (pg_index_insert(&c->candidates, &dependency->index, (uintptr_t)key)) return -1;
	}
	struct origin_candidate *candidate = pg_alloc(&c->objects.storage, sizeof(*candidate));
	if (!candidate) return -1;
	*candidate = (struct origin_candidate){dependency->first, job, binding};
	dependency->first = candidate;
	return 0;
}

static int index_origin(void *owner, struct pg_synthesis_job *job)
{
	struct origin_collection *c = owner;
	const struct pg_object *object = pg_synthesis_allocation_object(c->synthesis, job);
	if (index_origin_reference(c, job, NULL, object)) return -1;
	/* Computation endpoints may retain only the layout, not the type family. */
	const struct pg_data_declaration *declaration = pg_data_declaration_view(object);
	return declaration ? index_origin_reference(c, job, NULL,
		pg_data_matcher(pg_data_declaration_layout(declaration))) : 0;
}

static int index_scope_origin(void *owner, struct pg_synthesis_job *job)
{
	struct origin_collection *c = owner;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(c->synthesis, job, &scope, &syntax)) return -1;
	const struct pg_dag_node *node = pg_dag_find(c->syntax, syntax);
	if (node && c->last_syntax && node->id <= c->last_syntax->id) return index_origin(c, job);
	/* Keep allocation discovery in syntax-frontier order, including inert saves. */
	return index_origin_reference(c, job, NULL, syntax);
}

static int index_binding(void *owner, const struct pg_source_binding *input)
{
	struct origin_collection *c = owner;
	if (!input->syntax || id(c->syntax, input->syntax)) return collect_binding(c, input);
	/* The binder is reached first; wait for its source syntax, not Solve. */
	return index_origin_reference(c, NULL, input, input->syntax);
}

static int collect_binding(struct origin_collection *c, const struct pg_source_binding *input)
{
	if (input->syntax && !id(c->syntax, input->syntax)) return 0;
	if (pg_dag_add(c->bindings, input)) return -1;
	if (input->constructor && pg_dag_add(&c->terms, pg_reference(&c->rules->storage, input->constructor))) return -1;
	if (pg_dag_add(&c->terms, pg_reference(&c->rules->storage, input->binder))) return -1;
	for (size_t i = 0; i < input->scope_count; ++i)
		if (pg_dag_add(&c->terms, pg_reference(&c->rules->storage, input->scope[i]))) return -1;
	return 0;
}

static int collect_origin(void *owner, struct pg_synthesis_job *job)
{
	struct origin_collection *c = owner;
	if (id(c->origins, job)) return 0;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(c->synthesis, job, &scope, &syntax)) return -1;
	if (!id(c->syntax, syntax)) return index_origin_reference(c, job, NULL, syntax);
	/* Only lexical descendants of selected source roots belong to the image. */
	const struct pg_source_scope *parent = scope;
	while (!id(c->scopes, parent)) {
		struct pg_source_environment input;
		if (pg_synthesis_environment_input(c->synthesis, parent, &input)) return -1;
		const struct pg_syntax *site = input.handler ? input.handler : input.definitions;
		if (input.binding) {
			const struct pg_object *binder;
			if (pg_synthesis_binding_input(c->synthesis, input.binding, &input.parent, &site, &binder)) return -1;
		}
		if (!input.context) {
			if (!site || !id(c->syntax, site)) {
				if (site && index_origin_reference(c, job, NULL, site)) return -1;
				return index_origin_reference(c, job, NULL, parent);
			}
		}
		parent = input.parent;
		if (!parent) return 0;
	}
	if (pg_dag_add(c->scopes, scope) || pg_dag_add(c->origins, job) || pg_dag_add(c->producers, job)) return -1;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) {
		const struct pg_context *prefix, *fields;
		if (pg_synthesis_member_allocation(c->synthesis, job, &prefix, &fields)) return -1;
		return collect_member_use(c, job, prefix, fields);
	}
	if (syntax->kind == PG_SYNTAX_ELIMINATION) {
		const struct pg_match_allocation *input = pg_synthesis_match_allocation(job, &c->rules->storage);
		if (!input || pg_dag_add(c->matches, job)) return -1;
		const struct pg_induction_allocation *a = &input->induction;
		if (collect_allocation(c, input->prefix, input->motive, a->self)) return -1;
		if (pg_dag_add(&c->terms, pg_reference(&c->rules->storage, a->recursion)) ||
			pg_dag_add(&c->terms, pg_reference(&c->rules->storage, a->argument))) return -1;
		for (size_t i = 0; i < a->count; ++i)
			if (collect_allocation(c, input->branches[i], a->clauses[i], a->self)) return -1;
		return 0;
	}
	const struct pg_object *family = pg_synthesis_allocation_object(c->synthesis, job);
	if (!pg_data_declaration_view(family) || pg_dag_add(c->declarations, family)
		|| pg_dag_add(&c->terms, pg_reference(&c->rules->storage, family))) return -1;
	return 0;
}

static int collect_candidates(struct origin_collection *c, const void *key)
{
	struct origin_dependency *dependency = origin_dependency(c, key);
	if (!dependency) return 0;
	struct origin_candidate *first = dependency->first;
	dependency->first = NULL;
	/* Callbacks may grow/rehash the index; the detached waiter list is stable. */
	for (const struct origin_candidate *candidate = first; candidate; candidate = candidate->next) {
		int status;
		if (candidate->binding) status = collect_binding(c, candidate->binding);
		else {
			const struct pg_source_scope *scope;
			const struct pg_syntax *syntax;
			if (pg_synthesis_source_input(c->synthesis, candidate->job, &scope, &syntax)) return -1;
			status = key == syntax ? index_origin(c, candidate->job) : collect_origin(c, candidate->job);
		}
		if (status) return -1;
	}
	return 0;
}

static int retain_objects(struct origin_collection *c)
{
	if (collect_inputs(c)) return -1;
	for (;;) {
		for (const struct pg_dag_node *node = c->last_syntax ? c->last_syntax->next : c->syntax->first;
			node; c->last_syntax = node, node = node->next) {
			if (collect_candidates(c, node->key)) return -1;
		}
		for (const struct pg_dag_node *node = c->last_object ? c->last_object->next : c->objects.first;
			node; node = node->next) {
			c->last_object = node;
			if (pg_synthesis_visit_source_references(c->synthesis, node->key, NULL, index_binding, c)) return -1;
			if (collect_candidates(c, node->key)) return -1;
		}
		if (collect_inputs(c)) return -1;
		if (c->last_object == c->objects.last && c->last_syntax == c->syntax->last) return 0;
	}
}

static int retain_dependencies(void *owner, const struct pg_derivation_input *input,
	const struct pg_effect_inference *work)
{
	struct origin_collection *c = owner;
	if (input) {
		if (pg_derivation_input_collect(&c->terms, &c->contexts, input)) return -1;
	} else {
		const struct pg_term *const *terms;
		size_t count, equations;
		if (pg_effect_inference_pack(work, &c->terms.storage, &equations, &count, &terms)) return -1;
		for (size_t i = 0; i < count; ++i)
			if (terms[i] && pg_dag_add(&c->terms, terms[i])) return -1;
	}
	return retain_objects(c);
}

int pg_sources_write_retained(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots, const struct pg_reduction_archive *reductions)
{
	if (!file || !synthesis || (count && !roots)) return -1;
	struct pg_dag scopes = {0}, syntax = {0}, rules = {0}, origins = {0}, producers = {0}, allocations = {0};
	struct pg_dag bindings = {0}, matches = {0}, declarations = {0};
	struct pg_effect_inference effects = {0};
	struct pg_declaration_io codec = {0};
	struct origin_collection collection = {.synthesis = synthesis, .scopes = &scopes,
		.syntax = &syntax, .rules = &rules, .origins = &origins, .producers = &producers,
		.allocations = &allocations, .bindings = &bindings, .matches = &matches,
		.declarations = &declarations, .codec = &codec};
	int status = -1;
	/* Callbacks only inspect synthesis; no solver entry is invoked. */
	if (pg_dag_init(&scopes, child, &synthesis) || pg_dag_init(&syntax, pg_syntax_child, NULL)
		|| pg_dag_init(&producers, producer_child, &synthesis)
		|| pg_dag_init(&rules, NULL, NULL) || pg_dag_init(&origins, NULL, NULL) || pg_dag_init(&allocations, NULL, NULL)
		|| pg_dag_init(&bindings, NULL, NULL) || pg_dag_init(&matches, NULL, NULL)
		|| pg_dag_init(&declarations, NULL, NULL)
		|| pg_graph_init(&rules.storage)
		|| pg_dag_init(&collection.objects, NULL, NULL) || pg_index_init(&collection.candidates)
		|| pg_dag_init(&collection.contexts, pg_context_dependency, NULL)
		|| pg_effect_inference_init(&effects, &rules.storage)
		|| pg_declaration_io_init(&codec, synthesis->typing)) goto done;
	if (pg_graph_dependencies_init(&collection.terms, &collection.objects, &pg_declaration_graph_codec, &codec)) goto done;
	for (size_t i = 0; i < count; ++i) if (!roots[i] || pg_dag_add(&producers, roots[i])) goto done;
	if (retain_objects(&collection)) goto done;
	if (reductions) {
		if (pg_reduction_archive_collect(&collection.terms, reductions) || retain_objects(&collection)) goto done;
	}
	const struct pg_derivation_input *const *derivations;
	if (pg_synthesis_export_rule_closure(synthesis, &rules, &rules.storage, &effects, 1,
		retain_dependencies, &collection, &derivations)) goto done;
	if (matches.count > SIZE_MAX / (3 * sizeof(void *))) goto done;
	const struct pg_match_allocation **match_inputs = pg_alloc(&rules.storage, matches.count * sizeof(*match_inputs));
	if (!match_inputs) goto done;
	size_t match_context_count = 0;
	for (const struct pg_dag_node *node = matches.first; node; node = node->next) {
		const struct pg_match_allocation *input = pg_synthesis_match_allocation(node->key, &rules.storage);
		if (!input || SIZE_MAX - match_context_count < 2 ||
			input->induction.count > (SIZE_MAX - match_context_count - 2) / 2) goto done;
		match_inputs[node->id - 1] = input;
		match_context_count += 2 + 2 * input->induction.count;
	}
	if (collection.member_count > SIZE_MAX - allocations.count) goto done;
	size_t allocation_count = allocations.count + collection.member_count;
	if (allocation_count > SIZE_MAX / (2 * sizeof(void *))) goto done;
	size_t binding_reference_count = 0;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		size_t extra = input.kind == HANDLER_BINDING ? 3 : input.kind == BINDING;
		if (extra > SIZE_MAX - binding_reference_count) goto done;
		binding_reference_count += extra;
	}
	if (binding_reference_count > SIZE_MAX - allocation_count) goto done;
	size_t reference_count = allocation_count + binding_reference_count;
	for (const struct pg_dag_node *node = bindings.first; node; node = node->next) {
		const struct pg_source_binding *input = node->key;
		size_t extra = input->constructor ? 2 : 1;
		if (extra > SIZE_MAX - reference_count || input->scope_count > SIZE_MAX - reference_count - extra) goto done;
		reference_count += input->scope_count + extra;
	}
	if (matches.count > (SIZE_MAX - reference_count) / 3) goto done;
	reference_count += 3 * matches.count;
	if (declarations.count > SIZE_MAX - reference_count) goto done;
	reference_count += declarations.count;
	if (reference_count > SIZE_MAX / sizeof(void *) || match_context_count > SIZE_MAX - 2 * allocation_count) goto done;
	size_t context_count = 2 * allocation_count + match_context_count;
	if (context_count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_context **contexts = pg_alloc(&rules.storage, context_count * sizeof(*contexts));
	const struct pg_term **references = pg_alloc(&rules.storage, reference_count * sizeof(*references));
	if (!contexts || !references) goto done;
	for (const struct pg_dag_node *node = allocations.first; node; node = node->next) {
		const struct pg_object *reference;
		if (allocation_input(synthesis, node->key,
			&contexts[2 * (node->id - 1)], &contexts[2 * (node->id - 1) + 1], &reference)) goto done;
		references[node->id - 1] = pg_reference(&rules.storage, reference);
	}
	size_t slot = allocations.count;
	for (const struct source_member *member = collection.members; member; member = member->next, ++slot) {
		contexts[2 * slot] = member->allocation.prefix;
		contexts[2 * slot + 1] = member->allocation.fields;
		references[slot] = pg_reference(&rules.storage, member->allocation.constructor);
	}
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		if (input.kind == BINDING) references[slot++] = pg_reference(&rules.storage, input.binder);
		if (input.kind != HANDLER_BINDING) continue;
		const struct pg_object *binders[] = {input.allocation.payload, input.allocation.resume, input.allocation.response};
		for (size_t i = 0; i < 3; ++i) references[slot++] = pg_reference(&rules.storage, binders[i]);
	}
	for (const struct pg_dag_node *node = bindings.first; node; node = node->next) {
		const struct pg_source_binding *input = node->key;
		for (size_t i = 0; i < input->scope_count; ++i) references[slot++] = pg_reference(&rules.storage, input->scope[i]);
		references[slot++] = pg_reference(&rules.storage, input->binder);
		if (input->constructor) references[slot++] = pg_reference(&rules.storage, input->constructor);
	}
	size_t context_slot = 2 * allocation_count;
	for (size_t i = 0; i < matches.count; ++i) {
		const struct pg_match_allocation *input = match_inputs[i];
		const struct pg_induction_allocation *a = &input->induction;
		contexts[context_slot++] = input->prefix;
		contexts[context_slot++] = input->motive;
		for (size_t j = 0; j < a->count; ++j) {
			contexts[context_slot++] = input->branches[j];
			contexts[context_slot++] = a->clauses[j];
		}
		references[slot++] = pg_reference(&rules.storage, a->self);
		references[slot++] = pg_reference(&rules.storage, a->recursion);
		references[slot++] = pg_reference(&rules.storage, a->argument);
	}
	for (const struct pg_dag_node *node = declarations.first; node; node = node->next)
		references[slot++] = pg_reference(&rules.storage, node->key);
	struct pg_derivation_payload payload;
	if (pg_contexts_pack(&rules.storage, context_count, contexts, reference_count, references,
		&payload.metadata_count, &payload.metadata, &payload.count, &payload.terms)) goto done;
	size_t entry_count = 0;
	for (const struct pg_dag_node *node = producers.first; node; node = node->next) {
		const struct pg_syntax_item *item;
		struct pg_synthesis_job *producer;
		for (size_t i = 0; pg_synthesis_definition_entry(node->key, i, &item, &producer) == 1; ++i)
			if (producer) ++entry_count;
	}
	if (fwrite(reductions ? retained_magic : magic, 1, 8, file) != 8 || pg_wire_write_u64(file, synthesis->definition_policy)
		|| pg_wire_write_u64(file, scopes.count) || pg_wire_write_u64(file, count)
		|| pg_wire_write_u64(file, origins.count) || pg_wire_write_u64(file, producers.count)
		|| pg_wire_write_u64(file, entry_count)) goto done;
	for (const struct pg_dag_node *node = scopes.first; node; node = node->next) {
		struct environment input;
		if (environment(synthesis, node->key, &input)) goto done;
		uint64_t words[] = {input.kind, id(&scopes, input.parent), id(&scopes, input.target),
			id(&syntax, input.syntax), id(&syntax, input.definitions), input.name.kind, input.name.length,
			input.producer && input.kind != HANDLER_BINDING ? id(&producers, input.producer) : id(&rules, input.rule),
			input.kind == HANDLER_BINDING ? id(&producers, input.producer) : 0, input.slot};
		for (size_t i = 0; i < 10; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
		if (input.name.length && fwrite(input.name.text, 1, input.name.length, file) != input.name.length) goto done;
	}
	for (size_t i = 0; i < count; ++i) if (pg_wire_write_u64(file, id(&producers, roots[i]))) goto done;
	for (const struct pg_dag_node *node = producers.first; node; node = node->next) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term, *definitions;
		struct pg_synthesis_job *left, *right;
		uint64_t words[6] = {0};
		enum pg_reduction_kind kind;
		int force;
		struct callable_input callable;
		if (!callable_input(synthesis, node->key, &callable)) {
			words[1] = callable.operation ? 6 : 5; words[2] = id(&allocations, node->key); words[3] = callable.allocated;
			words[4] = id(&producers, callable.left); words[5] = id(&producers, callable.right);
		} else if (!pg_synthesis_normalization_input(synthesis, node->key, &left, &right, &kind, &force)) {
			/* No scope: syntax slot carries the request mode, not an AST ID. */
			words[1] = kind == PG_REDUCTION_NF ? 2 : 1;
			if (force) words[1] += 2;
			words[4] = id(&producers, left); words[5] = id(&producers, right);
		} else if (!pg_synthesis_source_expect_input(synthesis, node->key, &scope, &left, &right)) {
			words[0] = id(&scopes, scope);
			words[4] = id(&producers, left); words[5] = id(&producers, right);
		} else {
			if (source_input(synthesis, node->key, &scope, &term, &definitions)) {
				words[3] = id(&rules, node->key);
			} else {
				words[0] = id(&scopes, scope); words[1] = id(&syntax, term);
				words[2] = id(&syntax, definitions);
			}
		}
		for (size_t i = 0; i < 6; ++i) if (pg_wire_write_u64(file, words[i])) goto done;
	}
	for (const struct pg_dag_node *node = producers.first; node; node = node->next) {
		const struct pg_syntax_item *item;
		struct pg_synthesis_job *producer;
		for (size_t i = 0; pg_synthesis_definition_entry(node->key, i, &item, &producer) == 1; ++i) {
			if (!producer) continue;
			if (pg_wire_write_u64(file, node->id) || pg_wire_write_u64(file, i)
				|| pg_wire_write_u64(file, id(&producers, producer))) goto done;
		}
	}
	for (const struct pg_dag_node *node = origins.first; node; node = node->next) {
		const struct pg_source_scope *scope;
		const struct pg_syntax *term;
		if (pg_synthesis_source_input(synthesis, node->key, &scope, &term)) goto done;
		uint64_t allocation = term->kind == PG_SYNTAX_QUALIFIED ? id(&allocations, node->key)
			: term->kind == PG_SYNTAX_ELIMINATION ? id(&matches, node->key)
			: id(&declarations, pg_synthesis_allocation_object(synthesis, node->key));
		if (pg_wire_write_u64(file, id(&scopes, scope)) || pg_wire_write_u64(file, id(&syntax, term))
			|| pg_wire_write_u64(file, allocation)) goto done;
	}
	if (syntax.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_syntax **terms = pg_alloc(&syntax.storage, syntax.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = syntax.first; node; node = node->next) terms[node->id - 1] = node->key;
	if (pg_syntax_write(file, syntax.count, terms)) goto done;
	if (pg_wire_write_u64(file, bindings.count)) goto done;
	for (const struct pg_dag_node *node = bindings.first; node; node = node->next) {
		const struct pg_source_binding *input = node->key;
		if (pg_wire_write_u64(file, id(&syntax, input->syntax)) || pg_wire_write_u64(file, input->scope_count)
			|| pg_wire_write_u64(file, input->slot)) goto done;
	}
	if (pg_wire_write_u64(file, matches.count)) goto done;
	for (size_t i = 0; i < matches.count; ++i)
		if (pg_wire_write_u64(file, match_inputs[i]->induction.count)) goto done;
	if (pg_wire_write_u64(file, declarations.count)) goto done;
	if (pg_wire_write_u64(file, payload.metadata_count)) goto done;
	for (size_t i = 0; i < payload.metadata_count; ++i)
		if (pg_wire_write_u64(file, payload.metadata[i])) goto done;
	if (pg_wire_write_u64(file, collection.member_count)) goto done;
	for (const struct source_member *member = collection.members; member; member = member->next)
		if (pg_wire_write_u64(file, id(&producers, member->declaration)) || pg_wire_write_u64(file, member->index)) goto done;
	status = pg_retained_write(file, rules.count, derivations, &effects, reductions,
		payload.count, payload.terms, &pg_declaration_graph_codec, &codec);
done:
	pg_dag_destroy(&collection.contexts);
	pg_index_destroy(&collection.candidates); pg_dag_destroy(&collection.terms); pg_dag_destroy(&collection.objects);
	pg_declaration_io_destroy(&codec);
	pg_effect_inference_destroy(&effects); pg_dag_destroy(&rules); pg_dag_destroy(&origins);
	pg_dag_destroy(&bindings); pg_dag_destroy(&matches); pg_dag_destroy(&declarations);
	pg_dag_destroy(&syntax); pg_dag_destroy(&scopes); pg_dag_destroy(&producers); pg_dag_destroy(&allocations);
	return status;
}

int pg_sources_write(FILE *file, const struct pg_synthesis *synthesis,
	size_t count, struct pg_synthesis_job *const *roots)
{
	return pg_sources_write_retained(file, synthesis, count, roots, NULL);
}

struct record {
	uint64_t kind, parent, target, syntax, definitions, rule;
	struct pg_token name;
	uint64_t operation, slot;
	size_t allocation;
};

struct restore_node { size_t index; int scope; };
struct restore_order {
	const struct record *records;
	const uint64_t *ids;
	struct restore_node *nodes;
	size_t scopes, producers;
};

static int restore_child(void *owner, const void *key, size_t index, const void **result)
{
	if (index == 3) return 0;
	const struct restore_order *order = owner;
	const struct restore_node *node = key;
	uint64_t dependency;
	int scope;
	if (node->scope) {
		const struct record *r = &order->records[node->index];
		scope = index < 2;
		dependency = index == 0 ? r->parent : index == 1 ? r->target
			: (r->kind == NAME || r->kind == MODULE) ? r->rule : r->operation;
	} else {
		const uint64_t *r = &order->ids[6 * node->index];
		scope = index == 0;
		dependency = r[index ? index + 3 : 0];
	}
	if (!dependency) return 2;
	if (dependency > (scope ? order->scopes : order->producers)) return -1;
	*result = &order->nodes[(scope ? 0 : order->scopes) + (size_t)dependency - 1];
	return 1;
}

struct pg_program *pg_sources_read(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots)
{
	if (!file || !count || !roots) return NULL;
	char header[8];
	uint64_t policy, n, nr, no, np, ne;
	if (fread(header, 1, 8, file) != 8) return NULL;
	int retained = !memcmp(header, retained_magic, 8);
	if (!retained && memcmp(header, magic, 8)) return NULL;
	if (pg_wire_read_u64(file, &policy) || policy > PG_DEFINITION_EXPLICIT_THUNK) return NULL;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr) || pg_wire_read_u64(file, &no)
		|| pg_wire_read_u64(file, &np) || pg_wire_read_u64(file, &ne)) return NULL;
	if (n > limit || nr > limit - n || limit > SIZE_MAX / sizeof(struct record)) return NULL;
	if (no > (limit - n - nr) / 3) return NULL;
	if (np > (limit - n - nr - 3 * no) / 6) return NULL;
	if (ne > (limit - n - nr - 3 * no - 6 * np) / 3) return NULL;
	struct pg_program *program = pg_program_allocate((enum pg_definition_policy)policy);
	if (!program) return NULL;
	struct pg_declaration_io codec = {0};
	struct pg_dag order = {0};
	if (pg_declaration_io_init(&codec, &program->typing)) goto fail;
	struct pg_graph *graph = &program->graph;
	struct record *records = pg_alloc(graph, (size_t)n * sizeof(*records));
	const struct pg_source_scope **scopes = pg_alloc(graph, (size_t)n * sizeof(*scopes));
	uint64_t *ids = pg_alloc(graph, (size_t)np * 6 * sizeof(*ids));
	uint64_t *selections = pg_alloc(graph, (size_t)nr * sizeof(*selections));
	uint64_t *origin_ids = pg_alloc(graph, (size_t)no * 3 * sizeof(*origin_ids));
	uint64_t *entries = pg_alloc(graph, (size_t)ne * 3 * sizeof(*entries));
	struct pg_synthesis_job **jobs = pg_alloc(graph, (size_t)nr * sizeof(*jobs));
	struct pg_synthesis_job **producers = pg_alloc(graph, (size_t)np * sizeof(*producers));
	if (!records || !scopes || !ids || !jobs || !origin_ids || !selections || !producers || !entries) goto fail;
	size_t remaining = limit - (size_t)n - (size_t)nr - 3 * (size_t)no - 6 * (size_t)np - 3 * (size_t)ne;
	size_t binding_reference_count = 0;
	for (size_t i = 0; i < n; ++i) {
		uint64_t w[10];
		for (size_t j = 0; j < 10; ++j) if (pg_wire_read_u64(file, &w[j])) goto fail;
		if (w[0] > GRAPH_BINDING || w[1] > i || w[2] > i || w[5] > PG_TOKEN_ERROR || w[6] > remaining) goto fail;
		if (w[0] != HANDLER_BINDING && (w[8] || w[9])) goto fail;
		if (w[0] == HANDLER_BINDING && (!w[8] || w[8] > np || w[9] > 1)) goto fail;
		char *name = pg_alloc(graph, (size_t)w[6]);
		if (!name || fread(name, 1, (size_t)w[6], file) != w[6]) goto fail;
		remaining -= (size_t)w[6];
		records[i] = (struct record){w[0], w[1], w[2], w[3], w[4], w[7],
			{.kind = w[5], .text = name, .length = w[6], .text_length = w[6]}, w[8], w[9], binding_reference_count};
		size_t extra = w[0] == HANDLER_BINDING ? 3 : w[0] == BINDING;
		if (extra > SIZE_MAX - binding_reference_count) goto fail;
		binding_reference_count += extra;
	}
	for (size_t i = 0; i < nr; ++i)
		if (pg_wire_read_u64(file, &selections[i]) || !selections[i] || selections[i] > np) goto fail;
	for (size_t i = 0; i < 6 * np; ++i) if (pg_wire_read_u64(file, &ids[i])) goto fail;
	for (size_t i = 0; i < 3 * ne; ++i) if (pg_wire_read_u64(file, &entries[i])) goto fail;
	for (size_t i = 0; i < 3 * no; ++i) if (pg_wire_read_u64(file, &origin_ids[i])) goto fail;
	size_t nt;
	const struct pg_syntax *const *terms;
	if (pg_syntax_read(file, graph, limit, &nt, &terms) || pg_syntax_validate(nt, terms)) goto fail;
	uint64_t source_binding_count;
	if (pg_wire_read_u64(file, &source_binding_count) || source_binding_count > limit / 3
		|| source_binding_count > SIZE_MAX / (3 * sizeof(uint64_t))) goto fail;
	uint64_t *source_bindings = pg_alloc(graph, 3 * (size_t)source_binding_count * sizeof(*source_bindings));
	if (!source_bindings) goto fail;
	size_t source_reference_count = 0;
	for (size_t i = 0; i < source_binding_count; ++i) {
		uint64_t *words = &source_bindings[3 * i];
		for (size_t j = 0; j < 3; ++j) if (pg_wire_read_u64(file, &words[j])) goto fail;
		size_t extra = words[0] ? 1 : 2;
		if (words[0] > nt || extra > limit - source_reference_count || words[2] > SIZE_MAX) goto fail;
		if (words[1] > limit - source_reference_count - extra) goto fail;
		source_reference_count += (size_t)words[1] + extra;
	}
	uint64_t match_count;
	if (pg_wire_read_u64(file, &match_count) || match_count > limit / 3 || match_count > SIZE_MAX / sizeof(void *)) goto fail;
	struct pg_match_allocation **matches = pg_alloc(graph, (size_t)match_count * sizeof(*matches));
	if (!matches) goto fail;
	size_t match_context_count = 0;
	for (size_t i = 0; i < match_count; ++i) {
		uint64_t branches;
		if (pg_wire_read_u64(file, &branches) || limit - match_context_count < 2 ||
			branches > (limit - match_context_count - 2) / 2 ||
			branches > (SIZE_MAX - sizeof(**matches)) / (2 * sizeof(void *))) goto fail;
		matches[i] = pg_alloc(graph, sizeof(**matches) + 2 * (size_t)branches * sizeof(void *));
		if (!matches[i]) goto fail;
		matches[i]->induction.count = (size_t)branches;
		match_context_count += 2 + 2 * (size_t)branches;
	}
	uint64_t declaration_count;
	if (pg_wire_read_u64(file, &declaration_count) || declaration_count > limit || declaration_count > SIZE_MAX) goto fail;
	uint64_t metadata_count;
	if (pg_wire_read_u64(file, &metadata_count) || metadata_count > limit
		|| metadata_count > SIZE_MAX / sizeof(uint64_t)) goto fail;
	uint64_t *metadata = pg_alloc(graph, (size_t)metadata_count * sizeof(*metadata));
	if (!metadata) goto fail;
	for (size_t i = 0; i < metadata_count; ++i) if (pg_wire_read_u64(file, &metadata[i])) goto fail;
	uint64_t member_count;
	if (pg_wire_read_u64(file, &member_count) || member_count > limit || member_count > SIZE_MAX / (2 * sizeof(uint64_t))) goto fail;
	uint64_t *members = pg_alloc(graph, 2 * (size_t)member_count * sizeof(*members));
	if (!members) goto fail;
	for (size_t i = 0; i < 2 * member_count; ++i) if (pg_wire_read_u64(file, &members[i])) goto fail;
	size_t nd;
	size_t input_count = 0;
	const struct pg_term *const *input_terms;
	const struct pg_derivation_input *const *derivations;
	int input_status = pg_retained_read(file, &program->typing, limit, limit, &program->imported_effects,
		&pg_declaration_graph_codec, &codec, &nd, &derivations, &program->retained_reductions, &input_count, &input_terms);
	if (input_status) goto fail;
	if (retained != (program->retained_reductions != NULL)) goto fail;
	size_t context_count, reference_count;
	const struct pg_context *const *contexts;
	const struct pg_term *const *references;
	if (pg_contexts_unpack(&program->typing, (size_t)metadata_count, metadata, input_count, input_terms,
		&context_count, &contexts, &reference_count, &references)) goto fail;
	if (source_reference_count > reference_count) goto fail;
	if (binding_reference_count > reference_count - source_reference_count) goto fail;
	size_t allocation_count = reference_count - source_reference_count - binding_reference_count;
	if (declaration_count > allocation_count) goto fail;
	allocation_count -= (size_t)declaration_count;
	if (match_count > allocation_count / 3) goto fail;
	allocation_count -= 3 * (size_t)match_count;
	if (member_count > allocation_count || allocation_count - member_count > np
		|| match_context_count > context_count || allocation_count > SIZE_MAX / 2 ||
		context_count - match_context_count != 2 * allocation_count) goto fail;
	size_t context_slot = 2 * allocation_count;
	size_t declaration_slot = reference_count - (size_t)declaration_count;
	size_t match_slot = declaration_slot - 3 * (size_t)match_count;
	for (size_t i = 0; i < match_count; ++i) {
		struct pg_match_allocation *input = matches[i];
		input->prefix = contexts[context_slot++];
		input->motive = contexts[context_slot++];
		const struct pg_context **clauses = input->branches + input->induction.count;
		for (size_t j = 0; j < input->induction.count; ++j) {
			input->branches[j] = contexts[context_slot++];
			clauses[j] = contexts[context_slot++];
		}
		input->induction.clauses = clauses;
		for (size_t j = 0; j < 3; ++j)
			if (references[match_slot + j]->kind != PG_REFERENCE) goto fail;
		input->induction.self = references[match_slot++]->as.reference;
		input->induction.recursion = references[match_slot++]->as.reference;
		input->induction.argument = references[match_slot++]->as.reference;
	}
	size_t use_count = allocation_count - (size_t)member_count;
	size_t source_offset = allocation_count + binding_reference_count;
	if (source_reference_count > SIZE_MAX / sizeof(void *)) goto fail;
	const struct pg_object **binding_objects = pg_alloc(graph, source_reference_count * sizeof(*binding_objects));
	if (!binding_objects) goto fail;
	for (size_t i = 0; i < source_reference_count; ++i) {
		const struct pg_term *term = references[source_offset + i];
		if (term->kind != PG_REFERENCE) goto fail;
		binding_objects[i] = term->as.reference;
	}
	source_offset = 0;
	for (size_t i = 0; i < source_binding_count; ++i) {
		const uint64_t *words = &source_bindings[3 * i];
		struct pg_source_binding input = {words[0] ? terms[words[0] - 1] : NULL, (size_t)words[1], (size_t)words[2],
			binding_objects + source_offset, binding_objects[source_offset + words[1]],
			words[0] ? NULL : binding_objects[source_offset + words[1] + 1]};
		if (!pg_synthesis_source_binding(&program->synthesis, &input)) goto fail;
		source_offset += input.scope_count + (input.constructor ? 2 : 1);
	}
	if (fgetc(file) != EOF || ferror(file)) goto fail;
	if (nd > SIZE_MAX / sizeof(void *)) goto fail;
	struct pg_synthesis_job **rules = pg_alloc(graph, nd * sizeof(*rules));
	if (!rules) goto fail;
	pg_effect_inference_seal(&program->imported_effects);
	for (size_t i = 0; i < nd; ++i) {
		rules[i] = pg_synthesis_derivation_inference(&program->synthesis, derivations[i], &program->imported_effects);
		if (!rules[i]) goto fail;
	}
	struct restore_order dependencies = {records, ids, NULL, (size_t)n, (size_t)np};
	if (pg_dag_init(&order, restore_child, &dependencies)) goto fail;
	struct restore_node *nodes = pg_alloc(&order.storage, ((size_t)n + (size_t)np) * sizeof(*nodes));
	if (!nodes) goto fail;
	dependencies.nodes = nodes;
	for (size_t i = 0; i < n + np; ++i)
		nodes[i] = (struct restore_node){i < n ? i : i - (size_t)n, i < n};
	for (size_t i = 0; i < n + np; ++i) if (pg_dag_add(&order, &nodes[i])) goto fail;
	for (const struct pg_dag_node *entry = order.first; entry; entry = entry->next) {
		const struct restore_node *node = entry->key;
		size_t i = node->index;
		if (!node->scope) {
			uint64_t scope = ids[6 * i], syntax = ids[6 * i + 1], definitions = ids[6 * i + 2], rule = ids[6 * i + 3];
			uint64_t left = ids[6 * i + 4], right = ids[6 * i + 5];
			if (!scope && (syntax == 5 || syntax == 6)) {
				if (!left || left > i || !right || right > i || !definitions || definitions > use_count || rule > 1) goto fail;
				const struct pg_term *reference = references[definitions - 1];
				if (reference->kind != PG_REFERENCE) goto fail;
				const struct pg_context *prefix = contexts[2 * (definitions - 1)], *fields = contexts[2 * (definitions - 1) + 1];
				if (!rule && (prefix || fields)) goto fail;
				if (syntax == 6) {
					if (prefix) goto fail;
					producers[i] = rule ? pg_synthesis_operation_at(&program->synthesis, reference->as.reference,
						producers[left - 1], producers[right - 1], fields)
						: pg_synthesis_operation_jobs(&program->synthesis, reference->as.reference,
							producers[left - 1], producers[right - 1]);
				} else {
					if (rule && !pg_synthesis_constructor_scope_at(&program->synthesis,
						producers[left - 1], reference->as.reference, producers[right - 1], prefix, fields)) goto fail;
					producers[i] = pg_synthesis_constructor_value_jobs(&program->synthesis,
						producers[left - 1], reference->as.reference, producers[right - 1]);
				}
			} else if (left || right) {
				if (!left || left > i || !right || right > i || definitions || rule) goto fail;
				if (!scope) {
					if (syntax < 1 || syntax > 4) goto fail;
					enum pg_reduction_kind kind = syntax % 2 ? PG_REDUCTION_WHNF : PG_REDUCTION_NF;
					producers[i] = syntax > 2 ? pg_synthesis_evaluate_jobs(&program->synthesis,
						producers[left - 1], producers[right - 1], kind)
						: pg_synthesis_normalize_jobs(&program->synthesis, producers[left - 1], producers[right - 1], kind);
				} else {
					if (scope > n || syntax) goto fail;
					producers[i] = pg_synthesis_source_expect(&program->synthesis, scopes[scope - 1],
						producers[left - 1], producers[right - 1]);
				}
			} else if (rule) {
				if (rule > nd || scope || syntax || definitions) goto fail;
				producers[i] = rules[rule - 1];
			} else {
				if (!scope || scope > n || !syntax || syntax > nt || definitions > nt) goto fail;
				producers[i] = definitions ? pg_synthesis_definition_request(&program->synthesis,
					scopes[scope - 1], terms[definitions - 1], terms[syntax - 1])
					: pg_synthesis_request(&program->synthesis, scopes[scope - 1], terms[syntax - 1]);
			}
			if (!producers[i]) goto fail;
			continue;
		}
		const struct record *r = &records[i];
		if (r->syntax > nt || r->definitions > nt) goto fail;
		const struct pg_source_scope *parent = r->parent ? scopes[r->parent - 1] : NULL;
		const struct pg_source_scope *target = r->target ? scopes[r->target - 1] : NULL;
		struct pg_synthesis *s = &program->synthesis;
		if (r->kind == HANDLER_BINDING) {
			if (!parent || !target || !r->syntax || !r->definitions || r->rule) goto fail;
			const struct pg_term *const *binders = &references[allocation_count + r->allocation];
			for (size_t j = 0; j < 3; ++j)
				if (binders[j]->kind != PG_REFERENCE || binders[j]->as.reference->kind != PG_BINDER) goto fail;
			struct pg_handler_binding_input input = {.parent = target, .handler = terms[r->definitions - 1],
				.clause = terms[r->syntax - 1], .slot = (unsigned)r->slot,
				.allocation = {producers[r->operation - 1], binders[0]->as.reference,
					binders[1]->as.reference, binders[2]->as.reference}};
			scopes[i] = pg_synthesis_restore_handler_binding(s, &input);
			struct pg_source_environment restored;
			if (!scopes[i] || pg_synthesis_environment_input(s, scopes[i], &restored)) goto fail;
			if (restored.parent != parent || restored.name.kind != r->name.kind || restored.name.length != r->name.length) goto fail;
			if (r->name.length && memcmp(restored.name.text, r->name.text, r->name.length)) goto fail;
			continue;
		}
		if (r->kind == HANDLER_SCOPE) {
			if (!parent || target || !r->syntax || r->rule || r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_handler_scope(s, parent, terms[r->syntax - 1]);
			if (!scopes[i]) goto fail;
			continue;
		}
		if (r->kind == CONTEXT_BINDING || r->kind == GRAPH_BINDING) {
			if (!parent || !r->rule || r->rule > nd || r->syntax || r->definitions) goto fail;
			const struct pg_derivation_input *input = derivations[r->rule - 1];
			if (!input->parameters.binder) goto fail;
			if (input->rule != PG_CONTEXT_EXTEND && input->rule != PG_CONTEXT_FAMILY_EXTEND) goto fail;
			if (r->kind == GRAPH_BINDING && !target) goto fail;
			if (target) {
				if (input->rule != PG_CONTEXT_EXTEND) goto fail;
				struct pg_source_environment field;
				if (r->name.kind || r->name.length || pg_synthesis_environment_input(s, target, &field)) goto fail;
				scopes[i] = r->kind == GRAPH_BINDING
					? pg_synthesis_bind_graph(s, parent, field.binder, input->parameters.binder, rules[r->rule - 1])
					: pg_synthesis_bind_hypothesis(s, parent, field.binder, input->parameters.binder, rules[r->rule - 1]);
			} else scopes[i] = pg_synthesis_bind_context(s, parent, r->name,
				input->parameters.binder, rules[r->rule - 1]);
			if (!scopes[i]) goto fail;
			continue;
		}
		if (r->kind == BINDING) {
			if (!parent || target || !r->syntax || r->rule || r->definitions || r->name.kind || r->name.length) goto fail;
			const struct pg_term *binder = references[allocation_count + r->allocation];
			if (binder->kind != PG_REFERENCE) goto fail;
			scopes[i] = pg_synthesis_binding_scope(pg_synthesis_binding_at(s, parent,
				terms[r->syntax - 1], binder->as.reference));
			if (!scopes[i]) goto fail;
			continue;
		}
		if (r->kind == NAME || r->kind == MODULE) {
			if (!r->rule || r->rule > np || !parent || target || r->syntax || r->definitions) goto fail;
			struct pg_synthesis_job *producer = producers[r->rule - 1];
			scopes[i] = r->kind == NAME ? pg_synthesis_name_job(s, parent, r->name, producer)
				: pg_synthesis_module_namespace(s, parent, r->name, producer);
			if (!scopes[i]) goto fail;
			continue;
		}
		if (r->rule) goto fail;
		if (r->kind == ROOT) {
			if (parent || target || r->syntax || r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_root(s);
			continue;
		}
		if (r->kind == DEFINITIONS) {
			if (!parent || target || r->syntax || !r->definitions || r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_definition_scope(s, parent, terms[r->definitions - 1]);
			if (!scopes[i]) goto fail;
			continue;
		}
		if (!parent || !target) goto fail;
		if (r->syntax || r->definitions) goto fail;
		if (r->kind == IMPORTS) {
			if (r->name.kind || r->name.length) goto fail;
			scopes[i] = pg_synthesis_import_scope(s, parent, target);
		} else scopes[i] = pg_synthesis_namespace(s, parent, r->name, target);
		if (!scopes[i]) goto fail;
	}
	for (size_t i = 0; i < ne; ++i) {
		uint64_t module = entries[3 * i], index = entries[3 * i + 1], producer = entries[3 * i + 2];
		if (!module || module > np || !producer || producer >= module || index > SIZE_MAX) goto fail;
		if (pg_synthesis_retain_definition_input(&program->synthesis, producers[module - 1],
			(size_t)index, producers[producer - 1])) goto fail;
	}
	for (size_t i = 0; i < no; ++i) {
		uint64_t scope = origin_ids[3 * i], syntax = origin_ids[3 * i + 1], rule = origin_ids[3 * i + 2];
		if (!scope || scope > n || !syntax || syntax > nt || !rule) goto fail;
		const struct pg_syntax *site = terms[syntax - 1];
		if (site->kind == PG_SYNTAX_QUALIFIED) {
			if (rule > use_count) goto fail;
			const struct pg_context *prefix = contexts[2 * (rule - 1)], *fields = contexts[2 * (rule - 1) + 1];
			const struct pg_term *reference = references[rule - 1];
			if (!fields || prefix == fields || reference->kind != PG_REFERENCE || reference->as.reference != fields->binder) goto fail;
			if (!pg_synthesis_member_at(&program->synthesis, scopes[scope - 1], site, prefix, fields)) goto fail;
			continue;
		}
		if (site->kind == PG_SYNTAX_ELIMINATION) {
			if (rule > match_count || !pg_synthesis_restore_elimination(&program->synthesis,
				scopes[scope - 1], site, matches[rule - 1])) goto fail;
		} else {
			if (site->kind != PG_SYNTAX_DECLARATION || rule > declaration_count) goto fail;
			const struct pg_term *reference = references[declaration_slot + rule - 1];
			if (reference->kind != PG_REFERENCE) goto fail;
			const struct pg_data_declaration *declaration = pg_data_declaration_view(reference->as.reference);
			if (!declaration || !pg_synthesis_declaration_at(&program->synthesis,
				scopes[scope - 1], site, declaration)) goto fail;
		}
	}
	for (size_t i = 0; i < member_count; ++i) {
		uint64_t producer = members[2 * i], index = members[2 * i + 1];
		if (!producer || producer > np || index > SIZE_MAX) goto fail;
		size_t slot = use_count + i;
		if (references[slot]->kind != PG_REFERENCE) goto fail;
		struct pg_constructor_allocation allocation = {
			references[slot]->as.reference, contexts[2 * slot], contexts[2 * slot + 1]};
		if (pg_synthesis_declaration_member_at(&program->synthesis, producers[producer - 1], (size_t)index, &allocation)) goto fail;
	}
	for (size_t i = 0; i < nr; ++i) jobs[i] = producers[selections[i] - 1];
	program->root = nr ? jobs[0] : NULL;
	*count = (size_t)nr; *roots = jobs;
	pg_declaration_io_destroy(&codec);
	pg_dag_destroy(&order);
	return program;
fail:
	pg_dag_destroy(&order);
	pg_declaration_io_destroy(&codec);
	pg_program_destroy(program);
	return NULL;
}
