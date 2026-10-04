#include "synthesis.h"
#include "synthesis_work.h"
#include "synthesis_effect.h"
#include "synthesis_source.h"
#include "synthesis_schema.h"
#include "synthesis_handler.h"
#include "synthesis_conversion.h"
#include "host.h"
#include "computation.h"
#include "iadt.h"
#include "action.h"
#include "derivation.h"
#include "dag.h"
#include "effect_inference.h"
#include "function_graph.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct source_metadata {
	struct pg_index_entry index;
	const struct pg_occurrence *subject;
	struct pg_synthesis_job *source;
	struct pg_synthesis_job *match;
};

struct source_binding {
	struct pg_index_entry index;
	struct pg_source_binding input;
};

enum source_reference_kind { SOURCE_ALLOCATION, SOURCE_BINDING, ALLOCATION_ORIGIN, SOURCE_ENVIRONMENT };

struct source_reference_entry {
	struct pg_index_entry index;
	const void *key;
	enum source_reference_kind kind;
	const void *input;
};

static int register_source_reference(struct pg_synthesis *synthesis, const void *key,
	enum source_reference_kind kind, const void *input, const void *selector)
{
	/* The unary marker indexes binders with source environments, not a chosen
	 * environment. Exact parent/binder edges still select every matching use. */
	if (kind == SOURCE_ENVIRONMENT && !input)
		for (struct pg_index_entry *candidate = pg_index_candidates(&synthesis->source_references, (uintptr_t)key);
			candidate; candidate = candidate->next) {
			const struct source_reference_entry *found = (const void *)candidate;
			if (found->key == key && found->kind == kind && !found->input) return 0;
		}
	/* The owning request publishes each edge once. Distinct lexical uses of
	 * one address are not duplicate requests to search through here. */
	struct source_reference_entry *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return -1;
	entry->key = key; entry->kind = kind; entry->input = input;
	uint64_t hash = (uintptr_t)key;
	if (selector) hash = hash * UINT64_C(1099511628211) ^ (uintptr_t)selector;
	return pg_index_insert(&synthesis->source_references, &entry->index, hash);
}

int pg_synthesis_visit_source_references(const struct pg_synthesis *synthesis, const void *key,
	int (*allocation)(void *, struct pg_synthesis_job *),
	int (*binding)(void *, const struct pg_source_binding *),
	int (*environment_binder)(void *, const struct pg_object *), void *owner)
{
	if (!synthesis || !key) return -1;
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_references, (uintptr_t)key);
		entry; entry = entry->next) {
		const struct source_reference_entry *input = (const void *)entry;
		if (input->key != key) continue;
		switch (input->kind) {
		case SOURCE_ENVIRONMENT:
			if (!input->input && environment_binder && environment_binder(owner, key)) return -1;
			break;
		case SOURCE_BINDING:
			if (binding && binding(owner, input->input)) return -1;
			break;
		case SOURCE_ALLOCATION: case ALLOCATION_ORIGIN:
			if (allocation && allocation(owner, (void *)input->input)) return -1;
			break;
		}
	}
	return 0;
}

struct block_name {
	struct pg_index_entry index;
	struct pg_token name;
	struct pg_synthesis_job *producer;
	struct pg_synthesis_job *imported;
};
struct definition_state {
	struct pg_index names;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job **entries;
	size_t indexed, activated, next;
};
struct definition_work {
	struct pg_synthesis_job *body;
	int active;
};
struct motive_demand {
	struct motive_demand *next;
	const struct pg_source_scope *scope;
	const struct pg_object *field;
	struct pg_synthesis_job *callee, *domain;
	struct pg_synthesis_input normalized;
	const struct pg_evidence *solution;
	size_t count;
	struct pg_synthesis_job *arguments[];
};
struct motive_scan {
	const struct pg_syntax *syntax;
	const struct pg_source_scope *scope;
	size_t item;
};
struct motive_frontier {
	size_t count, capacity;
	struct motive_scan items[];
};
struct motive_lambda {
	const struct pg_evidence *outer, *inner;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	struct motive_lambda *parent;
};
struct motive_result {
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	size_t next;
	struct pg_synthesis_job *input, *callee;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	struct pg_synthesis_job *telescope, *nested;
	struct motive_lambda *lambdas;
	size_t nested_checked;
};
struct match_index_path {
	const struct pg_evidence *left, *right, *path;
	struct pg_synthesis_job *normal[2];
};
struct match_branch {
	const struct pg_source_scope *scope;
	const struct pg_evidence *fields, *pattern;
	const struct pg_syntax *clause;
	struct pg_synthesis_job *body;
	struct pg_synthesis_job *adapted, *refined;
	struct pg_pending *type_job;
	struct pg_synthesis_job *converted;
	const struct pg_evidence *function;
	struct motive_demand *demands;
	struct motive_frontier *scan;
	int scan_started;
	struct motive_result *result;
	struct match_index_path *paths, *contradiction;
	size_t path_checked;
	size_t field_count;
	int needs_ih;
};
struct source_case_field {
	struct pg_token name;
	size_t graph_value;
};
struct source_case_layout {
	size_t count;
	struct source_case_field *fields;
};
struct pg_synthesis_graph_names {
	const struct pg_synthesis_job *origin;
	struct source_case_layout cases[];
};
struct match_generalization {
	const struct pg_evidence *motive_context, *generic_indices, *map, *back;
	struct pg_synthesis_input pair;
	size_t count, next, copied, first, end;
	int active, replacing;
	const struct pg_evidence *extensions[];
};
struct match_state {
	struct pg_inductive_instance instance;
	const struct pg_evidence *result_constraint;
	const struct pg_source_scope *labels;
	const struct pg_evidence *motive;
	const struct pg_evidence *motive_context, *generic_context;
	struct pg_synthesis_job *motive_job;
	struct pg_synthesis_job *explicit_type_job;
	const struct pg_effect_row *motive_effects;
	enum pg_totality motive_totality;
	const struct pg_evidence *generalization, *specialization;
	size_t generalized_count;
	int scoped;
	struct match_generalization *generalizing;
	const struct pg_evidence *path_context, *path_result;
	const struct pg_evidence *candidate;
	size_t candidate_next, candidate_checked;
	const struct pg_evidence **path_extensions;
	size_t path_count, path_scoped;
	size_t count, selected, next, collected, effect_checked, demanded, checked, prepared, typed, result_checked, validated;
	int induction, uses_scrutinee, has_demands, type_cases, recursive_motive_done;
	struct match_branch *branches;
};
static void match_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void reference_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void definitions_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void graph_reference_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_input graph_reference_output(const struct pg_synthesis_job *);
static int source_context_wait(struct pg_synthesis *, struct pg_synthesis_job *);
static int atomic_rule_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void source_unsupported_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void source_match_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static void source_match_destroy(struct pg_synthesis_job *);
static void source_reference_start(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection source_reference_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_projection source_module_projection(const struct pg_synthesis_job *);
static const struct pg_source_scope *input_exports(const struct pg_synthesis *, struct pg_synthesis_input);
static const struct pg_term *structure_result(const struct pg_synthesis_job *);
static void term_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void classifier_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void type_structure_step(struct pg_synthesis *, struct pg_synthesis_job *);
struct structure_work {
	const struct pg_term *type_structure;
	struct pg_synthesis_structure left;
	struct pg_synthesis_reduction normalizing;
};
#define STRUCTURE_WORK(name, advance_fn) static const struct pg_synthesis_work_class name[1] = {{ \
	.pending = {&pg_synthesis_pending_ops}, \
	.size = sizeof(struct structure_work), .advance = advance_fn, .structure = structure_result}}
STRUCTURE_WORK(TERM_STRUCTURE_JOB, term_structure_step);
STRUCTURE_WORK(CLASSIFIER_STRUCTURE_JOB, classifier_structure_step);
STRUCTURE_WORK(TYPE_STRUCTURE_JOB, type_structure_step);
#undef STRUCTURE_WORK
struct match_work {
	struct pg_synthesis_job *left;
	struct pg_synthesis_job *right;
	const struct pg_source_scope *inner;
	const struct pg_evidence *checking_term;
	struct pg_synthesis_job *value_job;
	struct match_state *match;
	const struct pg_synthesis_sequence_frame *match_frame;
	const struct pg_match_allocation *match_allocation;
};
struct reference_work {
	struct pg_synthesis_job *left, *value_job;
	const struct pg_object *binder;
	struct pg_source_context_allocation *context_allocation;
};
struct module_work {
	struct pg_synthesis_job *left, *right;
};
struct graph_reference_work {
	struct pg_synthesis_job *value_job;
	struct pg_function_source_cursor *function_source;
};
static const struct pg_synthesis_work_class SOURCE_MATCH[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct match_work),
	.source_key = 1, .advance = match_step, .completed = source_match_completed,
	.destroy = source_match_destroy}};
static const struct pg_synthesis_work_class SOURCE_REFERENCE[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct reference_work),
	.source_key = 1, .start = source_reference_start, .advance = reference_step,
	.project = source_reference_projection, .output = pg_synthesis_projected_output}};
static const struct pg_synthesis_work_class SOURCE_MODULE[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct module_work),
	.source_key = 1, .start = source_reference_start, .advance = definitions_step,
	.project = source_module_projection, .output = pg_synthesis_projected_output}};
static const struct pg_synthesis_work_class SOURCE_GRAPH_REFERENCE[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct graph_reference_work),
	.source_key = 1, .advance = graph_reference_step, .output = graph_reference_output}};
static const struct pg_synthesis_work_class SOURCE_UNSUPPORTED[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .source_key = 1, .advance = source_unsupported_step}};

struct induction_branch_work {
	struct pg_synthesis_job *fields, *body, *abstraction;
	const struct pg_source_scope *scope;
};
static void induction_branch_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_input induction_branch_output(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class INDUCTION_BRANCH_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct induction_branch_work),
	.advance = induction_branch_step, .output = induction_branch_output}};

static void definition_start(struct pg_synthesis *, struct pg_synthesis_job *);
static void definition_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void definition_scope_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void definition_scope_destroy(struct pg_synthesis_job *);
static const struct pg_synthesis_work_class DEFINITION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct definition_work),
	.start = definition_start, .advance = definition_step}};
static const struct pg_synthesis_work_class DEFINITION_SCOPE_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct definition_state),
	.advance = definition_scope_step, .destroy = definition_scope_destroy}};

static struct definition_work *definition_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, DEFINITION_JOB);
}

static struct definition_state *definition_state(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, DEFINITION_SCOPE_JOB);
}

struct source_expect_work {
	struct pg_synthesis_job *returned, *check;
};
static void source_expect_step(struct pg_synthesis *, struct pg_synthesis_job *);
static struct pg_synthesis_projection source_expect_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_input source_expect_output(const struct pg_synthesis_job *);
static const struct pg_synthesis_work_class SOURCE_EXPECT_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct source_expect_work),
	.advance = source_expect_step, .output = source_expect_output}};
static const struct pg_synthesis_work_class BINDING_EXPECT_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops}, .size = sizeof(struct source_expect_work),
	.advance = source_expect_step, .project = source_expect_projection, .output = source_expect_output}};

static struct match_work *match_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, SOURCE_MATCH);
}
static struct reference_work *reference_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, SOURCE_REFERENCE);
}
static struct module_work *module_work(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, SOURCE_MODULE);
}
/* These owners borrow their lexical input; the request index is its authority. */
static const struct pg_source_scope *source_scope(const struct pg_synthesis_job *job)
{
	return job->inputs[0];
}
static const struct pg_syntax *source_syntax(const struct pg_synthesis_job *job)
{
	return job->inputs[1];
}

/* Result equations can arrive before scrutinee normalization. Keep the input
 * on its Match owner; allocate the constructor cases only after discovery. */
static struct match_state *match_state(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	if (!local->match) local->match = pg_alloc(synthesis->typing->graph, sizeof(*local->match));
	return local->match;
}

static const struct pg_evidence *match_function_input(const void *owner, size_t i)
{
	const struct match_state *state = owner;
	return state->branches[i].function;
}

int pg_synthesis_is_match(const struct pg_synthesis_job *job)
{
	return match_work(job) != NULL;
}

const struct pg_evidence *pg_synthesis_match_result_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *input)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key || !pg_synthesis_is_match(job)
		|| !pg_evidence_owned_by(input, synthesis->typing) || pg_evidence_judgement(input) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	struct match_state *state = match_state(synthesis, job);
	if (!state) return NULL;
	if (!state->result_constraint) {
		state->result_constraint = input;
		pg_synthesis_enqueue(synthesis, job);
	}
	return state->result_constraint;
}

struct constructor_callable pg_synthesis_callable(const struct pg_synthesis *synthesis, const struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return (struct constructor_callable){0};
	const struct constructor_callable *callable = pg_synthesis_application_callable(job);
	return callable ? *callable : pg_synthesis_constructor_callable(synthesis, job);
}

static const struct pg_source_scope *source_exports(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	const struct definition_work *definition = definition_work(job);
	if (definition) return job->status == PG_SYNTHESIS_DONE ? source_exports(synthesis, definition->body) : NULL;
	if (job && (pg_synthesis_work_role(job) == SOURCE_EXPECT_JOB || pg_synthesis_work_role(job) == BINDING_EXPECT_JOB))
		return source_exports(synthesis, pg_pending_job(pg_synthesis_work_dependency(job, 1).pending));
	const struct module_work *module = module_work(job);
	if (module) return job->status == PG_SYNTHESIS_DONE ? source_exports(synthesis, source_module_projection(job).rule) : NULL;
	const struct reference_work *reference = reference_work(job);
	if (!reference) {
		const struct pg_source_scope *exports = pg_synthesis_declaration_exports(job);
		if (!exports) {
			const struct pg_evidence *declaration;
			const struct pg_synthesis_graph_names *names;
			if (pg_synthesis_graph_source(job, &declaration, &exports, &names) != 1) return NULL;
		}
		return exports;
	}
	if (reference->left) return source_exports(synthesis, reference->left);
	return reference->value_job ? input_exports(synthesis,
		pg_synthesis_rule_input(synthesis, reference->value_job, 1)) : NULL;
}

const struct pg_evidence *pg_synthesis_scope_context(const struct pg_source_scope *scope)
{
	const struct pg_evidence *proof = scope ? pg_synthesis_input_result(scope->context) : NULL;
	return proof && pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT ? proof : NULL;
}

int pg_synthesis_scope_wait(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_source_scope *scope)
{
	return scope && scope->registration ? pg_synthesis_await(synthesis, job, scope->registration) : 0;
}

int pg_synthesis_init(struct pg_synthesis *synthesis, struct pg_typing *typing,
	struct pg_whnf_work *normalization,
	enum pg_definition_policy definition_policy)
{
	memset(synthesis, 0, sizeof(*synthesis));
	if (normalization->graph != typing->graph) return -1;
	if ((unsigned)definition_policy > PG_DEFINITION_EXPLICIT_THUNK) return -1;
	synthesis->typing = typing;
	synthesis->normalization = normalization;
	synthesis->definition_policy = definition_policy;
	if (pg_index_init(&synthesis->jobs) != 0) return -1;
	if (pg_index_init(&synthesis->rule_inputs) == 0 && pg_index_init(&synthesis->scopes) == 0 && pg_index_init(&synthesis->source_metadata) == 0
		&& pg_index_init(&synthesis->source_bindings) == 0 && pg_index_init(&synthesis->source_references) == 0) {
		synthesis->owner_key = pg_alloc(typing->graph, 1);
		if (synthesis->owner_key) return 0;
	}
	pg_synthesis_destroy(synthesis);
	return -1;
}

static void definition_scope_destroy(struct pg_synthesis_job *job)
{
	pg_index_destroy(&definition_state(job)->names);
}

void pg_synthesis_destroy(struct pg_synthesis *synthesis)
{
	pg_synthesis_work_destroy(synthesis);
	pg_index_destroy(&synthesis->rule_inputs);
	pg_index_destroy(&synthesis->scopes);
	pg_index_destroy(&synthesis->source_metadata);
	pg_index_destroy(&synthesis->source_bindings);
	pg_index_destroy(&synthesis->source_references);
	memset(synthesis, 0, sizeof(*synthesis));
}

static struct source_metadata *source_metadata(const struct pg_synthesis *synthesis,
	const struct pg_occurrence *subject)
{
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_metadata, (uintptr_t)subject);
		entry; entry = entry->next) {
		struct source_metadata *metadata = (void *)entry;
		if (metadata->subject == subject) return metadata;
	}
	return NULL;
}

static struct source_metadata *register_source_metadata(struct pg_synthesis *synthesis,
	const struct pg_occurrence *subject)
{
	struct source_metadata *metadata = source_metadata(synthesis, subject);
	if (metadata || !subject) return metadata;
	metadata = pg_alloc(synthesis->typing->graph, sizeof(*metadata));
	if (!metadata) return NULL;
	metadata->subject = subject;
	if (pg_index_insert(&synthesis->source_metadata, &metadata->index, (uintptr_t)subject)) return NULL;
	return metadata;
}

static const struct pg_source_scope *metadata_exports(const struct pg_synthesis *synthesis,
	const struct source_metadata *origin)
{
	return origin ? source_exports(synthesis, origin->source) : NULL;
}

static const struct pg_synthesis_graph_names *metadata_graph_names(const struct source_metadata *origin)
{
	const struct pg_evidence *declaration;
	const struct pg_source_scope *exports;
	const struct pg_synthesis_graph_names *names;
	return origin && pg_synthesis_graph_source(origin->source, &declaration, &exports, &names) == 1 ? names : NULL;
}

const struct source_constructor *pg_synthesis_constructor_source(const struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, const struct pg_object *constructor)
{
	const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(formation));
	const struct source_constructor *constructors = origin ? pg_synthesis_declaration_members(origin->source) : NULL;
	if (!constructors) return NULL;
	size_t ordinal;
	const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
	return pg_data_constructor_position(layout, constructor, &ordinal) ? &constructors[ordinal] : NULL;
}

int pg_synthesis_source_metadata(struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, struct pg_synthesis_job *source)
{
	if (!synthesis || !source || source->pending.owner != synthesis->owner_key
		|| !pg_evidence_owned_by(formation, synthesis->typing)) return -1;
	const struct pg_source_scope *exports = pg_synthesis_declaration_exports(source);
	if (exports) {
		if (source->result != formation) return -1;
	} else {
		const struct pg_evidence *declaration;
		const struct pg_synthesis_graph_names *names;
		if (pg_synthesis_graph_source(source, &declaration, &exports, &names) != 1
			|| declaration != formation || !exports) return -1;
	}
	struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(formation));
	if (!origin) return -1;
	origin->source = source;
	return 0;
}

static uint64_t name_hash(struct pg_token name);
static int same_name(struct pg_token left, struct pg_token right);

static const void *source_name_target(struct pg_synthesis_input input)
{
	/* Checked names share their typed use, not erased Core. Pending identity
	 * stays with its producer even after that producer completes. */
	const struct pg_occurrence *subject = input.checked ? pg_evidence_subject(input.checked) : NULL;
	return subject ? (const void *)subject : input.pending;
}

const struct pg_source_scope *pg_synthesis_intern_scope(struct pg_synthesis *synthesis, struct pg_source_scope input)
{
	if (!pg_synthesis_input_owned(synthesis, input.context)) return NULL;
	if (input.definitions && (input.definitions->pending.owner != synthesis->owner_key
		|| pg_synthesis_work_role(input.definitions) != DEFINITION_SCOPE_JOB)) return NULL;
	input.context = input.context;
	if (input.value.checked || input.value.pending) {
		if (!pg_synthesis_input_owned(synthesis, input.value)) return NULL;
		input.value = input.value;
	}
	if (!input.effect_owner && input.parent) input.effect_owner = input.parent->effect_owner;
	/* A derived dependency, not a second registration state or an intern key. */
	input.registration = input.definitions ? input.definitions
		: input.parent ? input.parent->registration : NULL;
	/* Special roots carry their spelling in kind, not borrowed text. */
	if (input.name.kind == '#' || input.name.kind == '*')
		input.name = (struct pg_token){.kind = input.name.kind};
	if (input.name.length && !input.name.text) return NULL;
	uint64_t hash = name_hash(input.name) ^ (unsigned)input.name.kind;
	hash = (hash ^ input.association) * UINT64_C(1099511628211);
	const void *pointers[] = {input.parent, input.context.checked, input.context.pending, input.binder, input.associated_binder, input.definitions,
		source_name_target(input.value), input.exports, input.module, input.imports, input.effect_owner, input.clause};
	for (size_t i = 0; i < sizeof(pointers) / sizeof(*pointers); ++i)
		hash = (hash ^ (uintptr_t)pointers[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->scopes, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		const struct pg_source_scope *scope = (const struct pg_source_scope *)entry;
		if (scope->parent != input.parent) continue;
		if (scope->context.checked != input.context.checked || scope->context.pending != input.context.pending) continue;
		if (scope->binder != input.binder) continue;
		if (scope->associated_binder != input.associated_binder) continue;
		if (scope->association != input.association) continue;
		if (scope->definitions != input.definitions) continue;
		if (source_name_target(scope->value) != source_name_target(input.value)) continue;
		if (scope->exports != input.exports) continue;
		if (scope->module != input.module) continue;
		if (scope->imports != input.imports) continue;
		if (scope->effect_owner != input.effect_owner) continue;
		if (scope->clause != input.clause) continue;
		if (scope->name.kind != input.name.kind) continue;
		if (same_name(scope->name, input.name)) return scope;
	}
	struct pg_source_scope *scope = pg_alloc(synthesis->typing->graph, sizeof(*scope));
	if (!scope) return NULL;
	*scope = input;
	scope->owner = synthesis->owner_key;
	if (scope->binder && scope->parent) {
		if (register_source_reference(synthesis, scope->parent, SOURCE_ENVIRONMENT, scope, scope->binder)) return NULL;
		if (register_source_reference(synthesis, scope->binder, SOURCE_ENVIRONMENT, NULL, NULL)) return NULL;
	}
	return pg_index_insert(&synthesis->scopes, &scope->index, hash) == 0 ? scope : NULL;
}

const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis)
{
	return pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.context =
		{.checked = pg_prove_empty_context(synthesis->typing)}});
}

int pg_synthesis_source_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax)
{
	if (!synthesis || !job || !scope || !syntax) return -1;
	if (job->pending.owner != synthesis->owner_key) return -1;
	if (!pg_synthesis_work_role(job)->source_key)
		return pg_synthesis_handler_source_input(job, scope, syntax);
	*scope = job->inputs[0]; *syntax = job->inputs[1];
	return 0;
}

static const struct pg_induction_allocation *source_induction_allocation(const struct pg_synthesis_job *job)
{
	const struct match_work *local = match_work(job);
	if (local && local->match_allocation) return &local->match_allocation->induction;
	return pg_evidence_induction_allocation(pg_synthesis_result(job));
}

const struct pg_match_allocation *pg_synthesis_match_allocation(
	struct pg_synthesis_input input, struct pg_graph *storage)
{
	if (input.checked && input.pending) return NULL;
	const struct match_work *local = match_work(pg_pending_job(input.pending));
	if (local && local->match_allocation) return local->match_allocation;
	if (!storage) return NULL;
	const struct pg_evidence *proof = pg_synthesis_input_result(input);
	const struct pg_induction_allocation *induction = pg_evidence_induction_allocation(proof);
	if (!induction || induction->count > (SIZE_MAX - sizeof(struct pg_match_allocation)) / sizeof(void *)) return NULL;
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	if (subject->operand_count != induction->count + 3) return NULL;
	struct pg_match_allocation *allocation = pg_alloc(storage,
		sizeof(*allocation) + induction->count * sizeof(*allocation->branches));
	if (!allocation) return NULL;
	allocation->induction = *induction;
	allocation->prefix = subject->context;
	allocation->motive = subject->operands[induction->count + 1]->context;
	for (size_t i = 0; i < induction->count; ++i) {
		size_t count;
		if (pg_context_extension_size(induction->clauses[i], subject->context, &count)) return NULL;
		const struct pg_occurrence *branch = subject->operands[i + 1];
		while (count--) {
			branch = pg_occurrence_scoped_input(branch, 0);
			if (!branch) return NULL;
		}
		allocation->branches[i] = branch->context;
	}
	return allocation;
}

int pg_synthesis_visit_source_allocations(const struct pg_synthesis *synthesis,
	int (*visit)(void *, struct pg_synthesis_job *), void *owner)
{
	if (!synthesis || !visit) return -1;
	for (size_t i = 0; i < synthesis->source_references.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->source_references.buckets[i]; entry; entry = entry->next) {
			const struct source_reference_entry *input = (const void *)entry;
			if (input->kind != SOURCE_ALLOCATION) continue;
			struct pg_synthesis_job *job = (void *)input->input;
			const struct pg_object *object = pg_synthesis_allocation_object(synthesis, job);
			if (!object) continue;
			if (visit(owner, job)) return -1;
		}
	return 0;
}

int pg_synthesis_definition_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **definitions, const struct pg_syntax **expression)
{
	if (!synthesis || !job || !scope || !definitions || !expression) return -1;
	if (job->pending.owner != synthesis->owner_key || pg_synthesis_work_role(job) != DEFINITION_JOB) return -1;
	const struct pg_synthesis_job *registration = job->inputs[0];
	*scope = registration->inputs[0]; *definitions = registration->inputs[1];
	*expression = job->inputs[1];
	return 0;
}

const struct pg_source_scope *pg_synthesis_prepared_environment(const struct pg_synthesis_job *job)
{
	if (!job) return NULL;
	if (pg_synthesis_work_role(job) == DEFINITION_JOB) job = job->inputs[0];
	else if (module_work(job)) job = module_work(job)->right;
	if (!job) return NULL;
	const struct definition_state *definitions = definition_state(job);
	if (definitions) return definitions->scope;
	return pg_synthesis_handler_environment(job);
}

int pg_synthesis_environment_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_source_environment *input)
{
	if (!synthesis || !scope || !input || scope->owner != synthesis->owner_key) return -1;
	if (pg_synthesis_handler_environment_input(scope, input)) return 0;
	if (scope->binder) {
		struct pg_synthesis_job *binding = pg_pending_job(scope->context.pending);
		const struct pg_source_scope *bound_scope = pg_synthesis_binding_scope(binding);
		if (bound_scope) {
			if (bound_scope != scope || scope->associated_binder) return -1;
			*input = (struct pg_source_environment){.parent = scope->parent, .binding = binding, .binder = scope->binder};
			return 0;
		}
		const struct pg_source_scope *field = NULL;
		if (scope->associated_binder) {
			field = scope->parent;
			while (field && field->binder != scope->associated_binder) field = field->parent;
			if (!field) return -1;
		}
		*input = (struct pg_source_environment){.parent = scope->parent, .name = scope->name,
			.context = pg_synthesis_scope_context_input(binding),
			.binder = scope->binder, .associated = field, .association = scope->association};
		if (!input->context.checked && !input->context.pending) input->context = scope->context;
		return 0;
	}
	const struct pg_evidence *context = pg_synthesis_scope_context(scope);
	if (scope->parent) {
		if (scope->context.checked != scope->parent->context.checked ||
			scope->context.pending != scope->parent->context.pending) return -1;
	} else if (!context || pg_evidence_context(context)) return -1;
	*input = (struct pg_source_environment){.parent = scope->parent, .exports = scope->exports,
		.imports = scope->imports, .name = scope->name, .value = scope->value, .module = scope->module,
		.definitions = scope->definitions ? scope->definitions->inputs[1] : NULL,
		.registration = scope->definitions ? scope->registration : NULL};
	return 0;
}

int pg_synthesis_visit_binding_environments(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_object *binder,
	int (*visit)(void *, const struct pg_source_scope *), void *owner)
{
	if (!synthesis || !parent || !binder || !visit) return -1;
	uint64_t hash = (uintptr_t)parent * UINT64_C(1099511628211) ^ (uintptr_t)binder;
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_references, hash);
		entry; entry = entry->next) {
		const struct source_reference_entry *reference = (const void *)entry;
		if (entry->hash != hash || reference->kind != SOURCE_ENVIRONMENT || reference->key != parent) continue;
		const struct pg_source_scope *scope = reference->input;
		if (scope->binder == binder && visit(owner, scope)) return -1;
	}
	return 0;
}

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, struct pg_synthesis_input context)
{
	return pg_synthesis_scope_bind(synthesis, parent, name, binder, context, NULL, NULL, PG_SOURCE_UNASSOCIATED);
}

static void definition_start(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	/* Registration activates definitions after all local names are indexed. */
	(void)synthesis;
	(void)job;
}

static struct pg_synthesis_job *request_job(struct pg_synthesis *synthesis,
	const struct pg_synthesis_work_class *role, const void *first, const void *second)
{
	const void *inputs[] = {first, second};
	return pg_synthesis_work_request(synthesis, role, 2, inputs);
}

static struct pg_synthesis_job *request_role(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax, const struct pg_synthesis_work_class *role)
{
	if (!scope || scope->owner != synthesis->owner_key || !syntax) return NULL;
	return request_job(synthesis, role, scope, syntax);
}

static void source_reference_start(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (source_syntax(job)->kind == PG_SYNTAX_QUALIFIED)
		if (register_source_reference(synthesis, source_scope(job), SOURCE_ALLOCATION, job, NULL))
			{ pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	pg_synthesis_enqueue(synthesis, job);
}

static int literal_header(struct pg_synthesis *synthesis, struct pg_token token,
	struct pg_derivation_input *input)
{
	*input = (struct pg_derivation_input){.rule = PG_UNIVERSE_FORM, .count = 1};
	if (token.kind == '@') return 0;
	if (token.kind != PG_TOKEN_INT && token.kind != PG_TOKEN_TEXT) return -1;
	const struct pg_object *type = pg_host_type(token.kind == PG_TOKEN_INT ? "Int32" : "Text");
	int out_of_range = token.kind == PG_TOKEN_INT && (token.integer < INT32_MIN || token.integer > INT32_MAX);
	const struct pg_object *literal = token.kind == PG_TOKEN_INT
		? pg_host_integer(synthesis->typing->graph, type, token.integer)
		: pg_host_literal(synthesis->typing->graph, type, token.text_length, (const unsigned char *)token.text);
	if (!literal && !out_of_range) return -1;
	input->rule = PG_HOST_VALUE_INTRO;
	input->parameters.constant = literal;
	return 0;
}

static struct pg_synthesis_job *literal_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_token token)
{
	if (!synthesis || !scope || scope->owner != synthesis->owner_key) return NULL;
	struct pg_derivation_input intro;
	if (literal_header(synthesis, token, &intro)) return NULL;
	if (intro.rule == PG_UNIVERSE_FORM)
		return pg_synthesis_rule_inputs(synthesis, &intro, &scope->context, NULL, NULL);
	const struct pg_object *type = pg_host_type(token.kind == PG_TOKEN_INT ? "Int32" : "Text");
	struct pg_derivation_input formation = {.rule = PG_HOST_TYPE_FORM, .count = 1, .parameters.constant = type};
	struct pg_synthesis_job *formed = pg_synthesis_rule_inputs(synthesis, &formation, &scope->context, NULL, NULL);
	if (!formed) return NULL;
	/* An absent out-of-range literal is rejected by the same ordinary rule;
	 * constructing this request is not a checked host value. */
	return pg_synthesis_rule_inputs(synthesis, &intro,
		&(struct pg_synthesis_input){.pending = pg_synthesis_pending(formed)}, NULL, NULL);
}

struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!syntax) return NULL;
	if (syntax->kind == PG_SYNTAX_ATOM && (syntax->token.kind == '@'
		|| syntax->token.kind == PG_TOKEN_INT || syntax->token.kind == PG_TOKEN_TEXT))
		return literal_request(synthesis, scope, syntax->token);
	if (syntax->kind == PG_SYNTAX_APPLICATION) return pg_synthesis_source_application(synthesis, scope, syntax);
	if (syntax->kind == PG_SYNTAX_LAMBDA) return pg_synthesis_source_lambda(synthesis, scope, syntax);
	if (syntax->kind == PG_SYNTAX_PI) return pg_synthesis_source_pi(synthesis, scope, syntax);
	if (syntax->kind == PG_SYNTAX_QUOTE) return pg_synthesis_source_quote(synthesis, scope, syntax);
	if (syntax->kind == PG_SYNTAX_DECLARATION) return pg_synthesis_source_declaration(synthesis, scope, syntax);
	if (pg_synthesis_block_syntax(syntax)) return pg_synthesis_source_block(synthesis, scope, syntax);
	if (syntax->item_count > 1 && pg_synthesis_handler_syntax(syntax))
		return pg_synthesis_handler(synthesis, scope, (struct pg_synthesis_input){0}, syntax);
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		struct pg_synthesis_job *term = pg_synthesis_request(synthesis, scope, syntax->left);
		struct pg_synthesis_job *type = pg_synthesis_request(synthesis, scope, syntax->right);
		return pg_synthesis_source_expect(synthesis, scope,
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(term)},
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(type)});
	}
	const struct pg_synthesis_work_class *role = SOURCE_UNSUPPORTED;
	switch (syntax->kind) {
	case PG_SYNTAX_ELIMINATION:
		if (pg_synthesis_handler_syntax(syntax)) return pg_synthesis_return_handler(synthesis, scope, syntax);
		role = SOURCE_MATCH;
		break;
	case PG_SYNTAX_DEFINITIONS: role = SOURCE_MODULE; break;
	case PG_SYNTAX_GRAPH_REFERENCE: role = SOURCE_GRAPH_REFERENCE; break;
	case PG_SYNTAX_QUALIFIED:
		role = syntax->left->kind == PG_SYNTAX_DEFINITIONS ? SOURCE_MODULE : SOURCE_REFERENCE;
		break;
	case PG_SYNTAX_ATOM: case PG_SYNTAX_IMPORT: role = SOURCE_REFERENCE; break;
	default: break;
	}
	return request_role(synthesis, scope, syntax, role);
}

int pg_synthesis_member_allocation(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_context **prefix, const struct pg_context **fields)
{
	const struct reference_work *local = reference_work(job);
	if (!job || job->pending.owner != synthesis->owner_key || pg_synthesis_work_role(job) != SOURCE_REFERENCE
		|| source_syntax(job)->kind != PG_SYNTAX_QUALIFIED || !prefix || !fields) return -1;
	if (local->context_allocation) {
		*prefix = local->context_allocation->prefix;
		*fields = local->context_allocation->end;
		return 0;
	}
	struct pg_constructor_input input;
	if (pg_synthesis_constructor_input(synthesis, local->left, &input) || !input.allocated) return -1;
	*prefix = input.prefix; *fields = input.fields;
	return 0;
}

const struct pg_object *pg_synthesis_allocation_object(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return NULL;
	const struct pg_data_declaration *schema_input = pg_synthesis_schema_allocation(job);
	if (schema_input) return pg_data_declaration_family(schema_input);
	const struct pg_data_declaration *declaration = pg_synthesis_declaration_allocation(job);
	if (declaration) return pg_data_declaration_family(declaration);
	if (reference_work(job) && source_syntax(job)->kind == PG_SYNTAX_QUALIFIED) {
		const struct pg_context *prefix, *fields;
		return pg_synthesis_member_allocation(synthesis, job, &prefix, &fields) || fields == prefix
			? NULL : fields->binder;
	}
	if (match_work(job)) {
		const struct pg_induction_allocation *allocation = source_induction_allocation(job);
		if (allocation) return allocation->self;
	}
	return NULL;
}

/* Address discovery does not wait for imported evidence to be accepted. */
int pg_synthesis_register_source_allocation(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(synthesis, job, &scope, &syntax)) return 0;
	struct match_work *local = match_work(job);
	if (syntax->kind != PG_SYNTAX_DECLARATION && syntax->kind != PG_SYNTAX_ELIMINATION) return 0;
	const struct pg_object *object = pg_synthesis_allocation_object(synthesis, job);
	if (!object) return 0;
	const struct pg_data_declaration *declaration = pg_data_declaration_view(object);
	const struct pg_context *context = declaration ? pg_data_declaration_parameters(declaration)
		: local && local->match_allocation ? local->match_allocation->prefix : pg_evidence_context(pg_synthesis_result(job));
	if (register_source_reference(synthesis, scope, SOURCE_ALLOCATION, job, NULL)) return -1;
	/* An erased allocation can retain its defining input, not every later
	 * alias. Resolve this immutable scope relation once, at registration. */
	if (!scope->binder) return 0;
	if (!pg_context_lookup(context, scope->binder)) return 0;
	if (register_source_reference(synthesis, object, ALLOCATION_ORIGIN, job, NULL)) return -1;
	return declaration ? register_source_reference(synthesis,
		pg_data_matcher(pg_data_declaration_layout(declaration)), ALLOCATION_ORIGIN, job, NULL) : 0;
}

struct pg_synthesis_job *pg_synthesis_restore_elimination(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_match_allocation *allocation)
{
	if (!syntax || syntax->kind != PG_SYNTAX_ELIMINATION) return NULL;
	if (pg_synthesis_handler_syntax(syntax)) return NULL;
	if (!allocation || !allocation->motive || allocation->induction.count != syntax->item_count) return NULL;
	size_t count = allocation->induction.count;
	if (count > (SIZE_MAX - sizeof(*allocation)) / (2 * sizeof(void *))) return NULL;
	if (count && !allocation->induction.clauses) return NULL;
	const struct pg_object *binders[] = {allocation->induction.self, allocation->induction.argument, allocation->induction.recursion};
	for (size_t i = 0; i < 3; ++i) if (!binders[i] || binders[i]->kind != PG_BINDER) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job) return NULL;
	if (match_work(job)->match_allocation) {
		const struct pg_match_allocation *old = match_work(job)->match_allocation;
		if (old->prefix != allocation->prefix || old->motive != allocation->motive ||
			!pg_induction_allocation_equal(&old->induction, &allocation->induction)) return NULL;
		for (size_t i = 0; i < count; ++i) if (old->branches[i] != allocation->branches[i]) return NULL;
		return job;
	}
	if (job->result || match_work(job)->value_job || match_work(job)->match) return NULL;
	struct pg_match_allocation *saved = pg_alloc(synthesis->typing->graph, sizeof(*saved) + 2 * count * sizeof(void *));
	if (!saved) return NULL;
	*saved = *allocation;
	const struct pg_context **clauses = saved->branches + count;
	for (size_t i = 0; i < count; ++i) {
		saved->branches[i] = allocation->branches[i];
		clauses[i] = allocation->induction.clauses[i];
	}
	saved->induction.clauses = clauses;
	match_work(job)->match_allocation = saved;
	return pg_synthesis_register_source_allocation(synthesis, job) ? NULL : job;
}

int pg_synthesis_context_allocation_at(struct pg_synthesis *synthesis,
	struct pg_source_context_allocation **slot, const struct pg_context *prefix,
	const struct pg_context *end, int started)
{
	size_t count;
	if (pg_context_extension_size(end, prefix, &count)
		|| count > (SIZE_MAX - sizeof(struct pg_source_context_allocation)) / sizeof(void *)) return -1;
	struct pg_source_context_allocation *allocation = *slot;
	if (allocation) {
		return allocation->prefix == prefix && allocation->end == end ? 0 : -1;
	} else {
		if (started) return -1;
		allocation = pg_alloc(synthesis->typing->graph, sizeof(*allocation) + count * sizeof(*allocation->contexts));
		if (!allocation) return -1;
		allocation->prefix = prefix; allocation->end = end; allocation->count = count;
		const struct pg_context *context = end;
		for (size_t i = count; i; --i, context = context->parent) allocation->contexts[i - 1] = context;
		*slot = allocation;
	}
	return 0;
}

struct pg_synthesis_job *pg_synthesis_member_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *fields)
{
	if (!syntax || syntax->kind != PG_SYNTAX_QUALIFIED || fields == prefix) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!reference_work(job) || pg_synthesis_context_allocation_at(synthesis, &reference_work(job)->context_allocation,
		prefix, fields, reference_work(job)->left != NULL)) return NULL;
	return job;
}

struct pg_source_binding_cursor pg_synthesis_source_binding_scope(const struct pg_source_binding *input)
{
	if (!input) return (struct pg_source_binding_cursor){0};
	/* Only a nonempty native address retains this one existing scope owner.
	 * Empty addresses need no tail; raw arrays keep their independent lifetime. */
	if (input->scope_count && !input->scope) {
		const struct source_binding *entry = (const void *)((const unsigned char *)input - offsetof(struct source_binding, input));
		if (input->syntax)
			return (struct pg_source_binding_cursor){.source = *(const struct pg_source_scope *const *)(entry + 1)};
		return (struct pg_source_binding_cursor){.context = *(const struct pg_context *const *)(entry + 1)};
	}
	return (struct pg_source_binding_cursor){.binders = input->scope, .count = input->scope_count};
}

/* Read one lexical address without copying its source telescope. */
int pg_synthesis_source_binding_next(struct pg_source_binding_cursor *cursor, const struct pg_object **binder)
{
	if (cursor->count) {
		*binder = *cursor->binders++;
		--cursor->count;
		return 1;
	}
	if (cursor->context) {
		*binder = cursor->context->binder;
		cursor->context = cursor->context->parent;
		return 1;
	}
	while (cursor->source && !cursor->source->binder) cursor->source = cursor->source->parent;
	if (!cursor->source) return 0;
	*binder = cursor->source->binder;
	cursor->source = cursor->source->parent;
	return 1;
}

static int binding_scope_local(const struct pg_synthesis *synthesis, const struct pg_source_scope *scope)
{
	for (; scope; scope = scope->parent) {
		const struct pg_index_entry *entry = pg_index_candidates(&synthesis->scopes, scope->index.hash);
		for (; entry; entry = entry->next)
			if (entry == &scope->index) break;
		if (!entry) return 0;
	}
	return 1;
}

static const struct pg_source_binding *source_binding_intern(struct pg_synthesis *synthesis,
	const struct pg_source_binding *input, struct pg_source_binding_cursor initial)
{
	if (!synthesis || !input) return NULL;
	if (input->syntax) {
		if (input->constructor) return NULL;
		switch (input->syntax->kind) {
		case PG_SYNTAX_APPLICATION: break;
		case PG_SYNTAX_BLOCK:
			if (input->slot >= input->syntax->item_count) return NULL;
			if (!input->syntax->items[input->slot].name.length) return NULL;
			break;
		case PG_SYNTAX_ELIMINATION:
			if (pg_synthesis_handler_syntax(input->syntax)) return NULL;
			if (input->slot) return NULL;
			break;
		case PG_SYNTAX_LAMBDA:
		case PG_SYNTAX_PI:
			if (input->slot) return NULL;
			break;
		case PG_SYNTAX_CLAUSE:
			if (!input->syntax->left) return NULL;
			if (pg_synthesis_return_clause(input->syntax)) {
				if (input->syntax->item_count != 1 || input->slot) return NULL;
			} else if (input->syntax->item_count != 2 || input->slot >= 3) return NULL;
			break;
		default: return NULL;
		}
	} else {
		const struct pg_data_layout *layout;
		size_t position, arity;
		if (!pg_data_constructor_view(input->constructor, &layout, &position, &arity) || input->slot >= arity) return NULL;
	}
	if (input->scope_count > (SIZE_MAX - sizeof(struct source_binding)) / sizeof(*input->scope)) return NULL;
	if (initial.source) {
		if (initial.source->owner != synthesis->owner_key || input->scope || input->constructor) return NULL;
	} else if (input->scope_count && !input->scope) return NULL;
	if (input->binder && input->binder->kind != PG_BINDER) return NULL;
	uint64_t hash = (uintptr_t)input->syntax ^ (uintptr_t)input->constructor ^ input->slot;
	struct pg_source_binding_cursor cursor = initial;
	const struct pg_object *binder;
	size_t count = 0;
	while (pg_synthesis_source_binding_next(&cursor, &binder)) {
		if (!binder || binder->kind != PG_BINDER || binder == input->binder) return NULL;
		if (++count > (SIZE_MAX - sizeof(struct source_binding)) / sizeof(*input->scope)) return NULL;
		hash = (hash ^ (uintptr_t)binder) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_bindings, hash); entry; entry = entry->next) {
		const struct pg_source_binding *found = &((const struct source_binding *)entry)->input;
		if (entry->hash != hash || found->syntax != input->syntax || found->slot != input->slot) continue;
		if (found->constructor != input->constructor) continue;
		if (found->scope_count != count) continue;
		cursor = initial;
		struct pg_source_binding_cursor saved = pg_synthesis_source_binding_scope(found);
		const struct pg_object *prior;
		size_t i = 0;
		while (pg_synthesis_source_binding_next(&cursor, &binder)
			&& pg_synthesis_source_binding_next(&saved, &prior) && prior == binder) ++i;
		if (i != count) continue;
		return !input->binder || input->binder == found->binder ? found : NULL;
	}
	/* A local head stamp does not retain a caller-owned ancestor. */
	const struct pg_source_scope *source = initial.source && binding_scope_local(synthesis, initial.source)
		? initial.source : NULL;
	const struct pg_context *context = initial.context;
	for (const struct pg_context *parent = context; parent; parent = parent->parent) {
		if (pg_context_owned_by(parent, synthesis->typing)) continue;
		context = NULL;
		break;
	}
	int borrowed = source || context;
	size_t retained = borrowed ? (count != 0) : count;
	struct source_binding *entry = pg_alloc(synthesis->typing->graph,
		sizeof(*entry) + retained * sizeof(*input->scope));
	if (!entry) return NULL;
	const struct pg_object **scope = (void *)(entry + 1);
	cursor = initial;
	if (source && count) *(const struct pg_source_scope **)(entry + 1) = source;
	else if (context && count) *(const struct pg_context **)(entry + 1) = context;
	else {
		for (size_t i = 0; i < retained; ++i) {
			if (!pg_synthesis_source_binding_next(&cursor, &binder)) return NULL;
			scope[i] = binder;
		}
	}
	entry->input = *input;
	entry->input.scope = !borrowed && retained ? scope : NULL;
	entry->input.scope_count = count;
	if (!entry->input.binder) entry->input.binder = pg_binder(synthesis->typing->graph);
	if (!entry->input.binder || pg_index_insert(&synthesis->source_bindings, &entry->index, hash)) return NULL;
	if (register_source_reference(synthesis, entry->input.binder, SOURCE_BINDING, &entry->input, NULL)) return NULL;
	return &entry->input;
}

const struct pg_source_binding *pg_synthesis_source_binding(struct pg_synthesis *synthesis,
	const struct pg_source_binding *input)
{
	if (!input) return NULL;
	/* An omitted nonempty array is meaningful only on an existing local owner.
	 * Reject raw copies or foreign canonical addresses before reading its tail. */
	if (input->scope_count && !input->scope) {
		if (!synthesis) return NULL;
		for (const struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_references, (uintptr_t)input->binder);
			entry; entry = entry->next) {
			const struct source_reference_entry *reference = (const void *)entry;
			if (reference->key == input->binder && reference->kind == SOURCE_BINDING && reference->input == input) return input;
		}
		return NULL;
	}
	return source_binding_intern(synthesis, input,
		(struct pg_source_binding_cursor){.binders = input->scope, .count = input->scope_count});
}

const struct pg_object *pg_synthesis_constructor_binder(struct pg_synthesis *synthesis,
	const struct pg_context *context, const struct pg_object *constructor, size_t field)
{
	const struct pg_source_binding *binding = source_binding_intern(synthesis,
		&(struct pg_source_binding){.slot = field, .constructor = constructor},
		(struct pg_source_binding_cursor){.context = context});
	return binding ? binding->binder : NULL;
}

const struct pg_object *pg_synthesis_source_binder(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax, size_t slot,
	const struct pg_object *binder)
{
	struct pg_source_binding input = {.syntax = syntax, .slot = slot, .binder = binder};
	const struct pg_source_binding *binding = source_binding_intern(synthesis, &input,
		(struct pg_source_binding_cursor){.source = scope});
	return binding ? binding->binder : NULL;
}

int pg_synthesis_visit_source_bindings(const struct pg_synthesis *synthesis,
	int (*visit)(void *, const struct pg_source_binding *), void *owner)
{
	if (!synthesis || !visit) return -1;
	for (size_t i = 0; i < synthesis->source_bindings.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->source_bindings.buckets[i]; entry; entry = entry->next)
			if (visit(owner, &((const struct source_binding *)entry)->input)) return -1;
	return 0;
}

static int type_structure_rule(enum pg_evidence_rule rule)
{
	if (pg_synthesis_function_type_rule(rule)) return 1;
	if (pg_synthesis_cbpv_type_rule(rule)) return 1;
	switch (rule) {
	case PG_UNIVERSE_FORM: case PG_HOST_TYPE_FORM:
	case PG_TYPE_FROM_VALUE: return 1;
	default: return 0;
	}
}

struct pg_synthesis_structure pg_synthesis_structure_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input, const struct pg_synthesis_work_class *role, int classifier, int type)
{
	if (!pg_synthesis_input_owned(synthesis, input)) return (struct pg_synthesis_structure){0};
	const void *key[] = {input.checked, input.pending};
	struct pg_synthesis_job *existing = pg_synthesis_work_find(synthesis, role, 2, key);
	if (existing) return (struct pg_synthesis_structure){.pending = existing};
	const struct pg_evidence *proof = pg_synthesis_input_result(input);
	if (proof) {
		const struct pg_occurrence *subject = pg_evidence_subject(proof);
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (type && kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) subject = NULL;
		return (struct pg_synthesis_structure){.term = !subject ? NULL : classifier ? subject->classifier : subject->core};
	}
	return (struct pg_synthesis_structure){.pending = pg_synthesis_work_request(synthesis, role, 2, key)};
}

struct pg_synthesis_structure pg_synthesis_type_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation)
{
	return pg_synthesis_type_structure_input(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(formation)});
}

static struct pg_synthesis_structure structural_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input, int classifier, int type)
{
	if (!pg_synthesis_input_owned(synthesis, input)) return (struct pg_synthesis_structure){0};
	/* These rules change a judgement/scope, not Core. Borrow the actual
	 * input query iteratively; descriptive readiness does not admit the rule. */
	for (;;) {
		struct pg_synthesis_job *producer = pg_pending_job(input.pending);
		const struct pg_derivation_input *rule = pg_synthesis_plain_derivation(producer);
		if (!rule) {
			/* Assertion and classifier conversion keep subject Core unchanged.
			 * The operand owns its shape; these checks still own admission. */
			if (!classifier && !type) {
				struct pg_synthesis_input operand = pg_synthesis_classifier_input(producer);
				if (!operand.checked && !operand.pending) operand = pg_synthesis_expect_input(producer);
				if (pg_synthesis_input_owned(synthesis, operand)) {
					input = operand;
					continue;
				}
			}
			/* Classifier formation already has the operand's shape owner.
			 * Borrow it; formation still checks its own Context and judgement. */
			if (!type) break;
			struct pg_synthesis_input operand = pg_synthesis_classifier_formation_input(synthesis, input.pending);
			if (!pg_synthesis_input_owned(synthesis, operand)) break;
			input = operand;
			classifier = 1;
			type = 0;
			continue;
		}
		if (pg_synthesis_input_result(input)) break;
		size_t ordinal;
		if (rule->rule == PG_CONTEXT_PROJECTION && rule->count == 2) ordinal = 1;
		else if (rule->rule == PG_TYPE_FROM_VALUE && rule->count == 1) {
			ordinal = 0;
			type = 0;
		} else if (rule->rule == PG_VALUE_FROM_TYPE && rule->count == 1 && !type) {
			ordinal = 0;
			type = !classifier;
		} else if (classifier && rule->count == 1 &&
			(rule->rule == PG_HOST_VALUE_INTRO || rule->rule == PG_HOST_FUNCTION_INTRO)) {
			ordinal = 0;
			classifier = 0;
			type = 1;
		} else if (classifier && rule->rule == PG_VARIABLE && rule->count == 1) {
			return pg_synthesis_declared_type(synthesis,
				pg_synthesis_rule_input(synthesis, producer, 0), rule->parameters.binder);
		} else break;
		input = pg_synthesis_rule_input(synthesis, producer, ordinal);
	}
	const struct pg_derivation_input *rule = pg_synthesis_plain_derivation(pg_pending_job(input.pending));
	if (type && rule && type_structure_rule(rule->rule)) type = 0;
	if (!type && rule && !pg_synthesis_input_result(input)) {
		if (rule->rule == PG_UNIVERSE_FORM) {
			uint64_t level = rule->parameters.level;
			if (!classifier || level != UINT64_MAX)
				return (struct pg_synthesis_structure){.term = pg_universe(synthesis->typing->graph, level + !!classifier)};
		} else if (!classifier) {
			switch (rule->rule) {
			case PG_HOST_TYPE_FORM: case PG_HOST_VALUE_INTRO: case PG_HOST_FUNCTION_INTRO:
				return (struct pg_synthesis_structure){.term = pg_reference(synthesis->typing->graph, rule->parameters.constant)};
			case PG_VARIABLE:
				return (struct pg_synthesis_structure){.term = pg_reference(synthesis->typing->graph, rule->parameters.binder)};
			default: break;
			}
		}
	}
	if (type) {
		const struct pg_synthesis_work_class *role = pg_synthesis_binding_type_class(pg_pending_job(input.pending));
		return pg_synthesis_structure_input(synthesis, input, role ? role : TYPE_STRUCTURE_JOB, 0, 1);
	}
	struct pg_synthesis_structure result;
	if (pg_synthesis_function_structure_input(synthesis, input, classifier, &result)) return result;
	if (pg_synthesis_cbpv_structure_input(synthesis, input, classifier, &result)) return result;
	return pg_synthesis_structure_input(synthesis, input,
		classifier ? CLASSIFIER_STRUCTURE_JOB : TERM_STRUCTURE_JOB, classifier, 0);
}

struct pg_synthesis_structure pg_synthesis_type_structure_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input formation)
{
	return structural_input(synthesis, formation, 0, 1);
}

static const struct pg_term *structure_result(const struct pg_synthesis_job *job)
{
	const struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	return local->type_structure;
}

int pg_synthesis_structure_valid(struct pg_synthesis_structure input)
{
	return input.term != NULL || input.pending != NULL;
}

enum pg_synthesis_status pg_synthesis_structure_status(struct pg_synthesis_structure input)
{
	return input.term ? PG_SYNTHESIS_DONE : input.pending ? input.pending->status : PG_SYNTHESIS_UNSUPPORTED;
}

int pg_synthesis_await_structure(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_structure input)
{
	if (input.term) return 0;
	if (input.pending) return pg_synthesis_await(synthesis, job, input.pending);
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
	return 1;
}

const struct pg_term *pg_synthesis_type_structure_result(struct pg_synthesis_structure input)
{
	if (input.term) return input.term;
	const struct pg_synthesis_job *job = input.pending;
	if (!job || job->status != PG_SYNTHESIS_DONE || !pg_synthesis_work_role(job)->structure) return NULL;
	return pg_synthesis_work_role(job)->structure(job);
}

struct pg_synthesis_structure pg_synthesis_classifier_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term)
{
	return pg_synthesis_classifier_structure_input(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(term)});
}

struct pg_synthesis_structure pg_synthesis_classifier_structure_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input term)
{
	return structural_input(synthesis, term, 1, 0);
}

struct pg_synthesis_structure pg_synthesis_term_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term)
{
	return pg_synthesis_term_structure_input(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(term)});
}

struct pg_synthesis_structure pg_synthesis_term_structure_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input term)
{
	return structural_input(synthesis, term, 0, 0);
}

const struct pg_source_scope *pg_synthesis_bind_hypothesis(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_object *field,
	const struct pg_object *binder, struct pg_synthesis_input context)
{
	if (!field) return NULL;
	return pg_synthesis_scope_bind(synthesis, parent, (struct pg_token){0}, binder, context, field, NULL, PG_SOURCE_HYPOTHESIS);
}

const struct pg_source_scope *pg_synthesis_bind_graph(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_object *value,
	const struct pg_object *binder, struct pg_synthesis_input context)
{
	if (!value) return NULL;
	return pg_synthesis_scope_bind(synthesis, parent, (struct pg_token){0}, binder, context, value, NULL, PG_SOURCE_GRAPH);
}

const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_input value)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!pg_synthesis_input_owned(synthesis, value)) return NULL;
	if (value.checked) {
		if (!pg_evidence_subject(value.checked)) return NULL;
		const struct pg_evidence *context = pg_synthesis_scope_context(parent);
		if (context) {
			size_t count;
			if (pg_context_extension_size(pg_evidence_context(context),
				pg_evidence_context(value.checked), &count)) return NULL;
		} else if (!parent->context.pending || pg_synthesis_pending_status(parent->context.pending) != PG_SYNTHESIS_PENDING) return NULL;
	}
	return pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.name = name, .context = parent->context, .value = value});
}

const struct pg_source_scope *pg_synthesis_import_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_source_scope *bindings)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (!bindings || bindings->owner != synthesis->owner_key) return NULL;
	if (!pg_synthesis_scope_context(bindings) || pg_evidence_context(pg_synthesis_scope_context(bindings))) return NULL;
	return pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.context = parent->context, .imports = bindings});
}

static const struct pg_source_scope *publish_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports, struct pg_synthesis_job *module)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (name.kind != '#') {
		if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	}
	if (module) {
		const struct pg_syntax *syntax = source_syntax(module);
		if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
		module = pg_synthesis_request(synthesis, source_scope(module), syntax);
		if (!module) return NULL;
	}
	return pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.name = name, .context = parent->context, .exports = exports, .module = module});
}

const struct pg_source_scope *pg_synthesis_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports)
{
	if (!exports || exports->owner != synthesis->owner_key) return NULL;
	if (!pg_synthesis_scope_context(exports) || pg_evidence_context(pg_synthesis_scope_context(exports))) return NULL;
	return publish_namespace(synthesis, parent, name, exports, NULL);
}

const struct pg_source_scope *pg_synthesis_module_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *module)
{
	if (!module || module->pending.owner != synthesis->owner_key || pg_synthesis_work_role(module) != SOURCE_MODULE) return NULL;
	if (!pg_synthesis_scope_context(source_scope(module)) || pg_evidence_context(pg_synthesis_scope_context(source_scope(module)))) return NULL;
	const struct pg_syntax *syntax = source_syntax(module);
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	return publish_namespace(synthesis, parent, name, NULL, module);
}

static struct pg_synthesis_job *source_expect_request(struct pg_synthesis *synthesis, const struct pg_synthesis_work_class *role,
	const struct pg_source_scope *scope, struct pg_synthesis_input term,
	struct pg_synthesis_input type)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!pg_synthesis_input_owned(synthesis, term)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, type)) return NULL;
	const void *inputs[] = {scope, term.checked, term.pending, type.checked, type.pending};
	return pg_synthesis_work_request(synthesis, role, 5, inputs);
}

struct pg_synthesis_job *pg_synthesis_source_expect(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_input term,
	struct pg_synthesis_input type)
{
	return source_expect_request(synthesis, SOURCE_EXPECT_JOB, scope, term, type);
}

struct pg_synthesis_job *pg_synthesis_binding_expect(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_input term,
	struct pg_synthesis_input type)
{
	return source_expect_request(synthesis, BINDING_EXPECT_JOB, scope, term, type);
}

int pg_synthesis_source_expect_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	struct pg_synthesis_input *term, struct pg_synthesis_input *type)
{
	if (!synthesis || !job || !scope || !term || !type) return -1;
	if (job->pending.owner != synthesis->owner_key || pg_synthesis_work_role(job) != SOURCE_EXPECT_JOB) return -1;
	*scope = job->inputs[0];
	*term = pg_synthesis_work_dependency(job, 1);
	*type = pg_synthesis_work_dependency(job, 3);
	return 0;
}

struct pg_synthesis_job *pg_synthesis_induction_branch(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *generalization, const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(formation, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(parameters, synthesis->typing)) return NULL;
	if (pg_evidence_context(parameters) != pg_evidence_context(pg_synthesis_scope_context(scope))) return NULL;
	if (!pg_evidence_owned_by(motive_context, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(motive, synthesis->typing)) return NULL;
	if (generalization && (!pg_evidence_owned_by(generalization, synthesis->typing) ||
		pg_evidence_judgement(generalization) != PG_JUDGEMENT_SUBSTITUTION)) return NULL;
	if (generalization && pg_evidence_context(pg_evidence_premise(generalization, 0)) !=
		pg_evidence_context(pg_synthesis_scope_context(scope))) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, formation, constructor, parameters, motive_context, motive, clause, generalization};
	return pg_synthesis_work_request(synthesis, INDUCTION_BRANCH_JOB, 8, inputs);
}

struct pg_synthesis_input pg_synthesis_abstract(struct pg_synthesis *synthesis,
	const struct pg_evidence *prefix, const struct pg_evidence *context,
	struct pg_synthesis_input body)
{
	if (!pg_synthesis_input_owned(synthesis, body)) return (struct pg_synthesis_input){0};
	if (!pg_evidence_owned_by(prefix, synthesis->typing) || pg_evidence_judgement(prefix) != PG_JUDGEMENT_CONTEXT) return (struct pg_synthesis_input){0};
	if (!pg_evidence_owned_by(context, synthesis->typing) || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return (struct pg_synthesis_input){0};
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(prefix), &count)) return (struct pg_synthesis_input){0};
	if (!count) return pg_synthesis_body(synthesis, body, (struct pg_synthesis_input){.checked = context});
	for (size_t i = 0; i < count; ++i) {
		struct pg_synthesis_job *lambda = pg_synthesis_lambda_body(synthesis,
			(struct pg_synthesis_input){.checked = context}, body);
		if (!lambda) return (struct pg_synthesis_input){0};
		body = (struct pg_synthesis_input){.pending = pg_synthesis_pending(lambda)};
		context = pg_context_parent_input(synthesis->typing, context);
	}
	return body;
}

int pg_synthesis_typed_input(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(context, synthesis->typing) || !pg_evidence_owned_by(proof, synthesis->typing)) return 0;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	return pg_evidence_context(context) == pg_evidence_context(proof);
}

static void source_match_destroy(struct pg_synthesis_job *job)
{
	struct match_state *state = match_work(job)->match;
	if (!state || !state->branches) return;
	for (size_t i = 0; i < state->count; ++i) {
		struct match_branch *branch = &state->branches[i];
		free(branch->scan);
		branch->scan = NULL;
	}
}

static void source_match_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	int first_finish)
{
	struct match_work *local = match_work(job);
	if (job->status == PG_SYNTHESIS_DONE && local->match && job->result) {
		struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(job->result));
		if (!origin) job->status = PG_SYNTHESIS_ERROR;
		else if (!origin->match) origin->match = job;
	}
	/* Imported allocations were registered when attached, before Solve. */
	if (first_finish && !local->match_allocation)
		if (pg_synthesis_register_source_allocation(synthesis, job)) job->status = PG_SYNTHESIS_ERROR;
	source_match_destroy(job);
}

static void depend(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child)
{
	pg_synthesis_subscribe(synthesis, parent, child, 0);
}

static struct pg_synthesis_job *definition_registration(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis->owner_key || !syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, DEFINITION_SCOPE_JOB, scope, syntax);
	if (!job) return NULL;
	struct definition_state *state = definition_state(job);
	if (job->status == PG_SYNTHESIS_ERROR) return NULL;
	if (state->scope) return job;
	if (pg_index_init(&state->names) != 0) goto fail;
	if (syntax->item_count > SIZE_MAX / sizeof(*state->entries)) goto fail;
	state->entries = pg_alloc(synthesis->typing->graph, syntax->item_count * sizeof(*state->entries));
	state->scope = pg_synthesis_intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
		.context = scope->context, .definitions = job});
	if (!state->scope || !state->entries) goto fail;
	return job;
fail:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return NULL;
}

struct pg_synthesis_job *pg_synthesis_prepare_module(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *root)
{
	if (!synthesis || !root || root->pending.owner != synthesis->owner_key || pg_synthesis_work_role(root) != SOURCE_MODULE) return NULL;
	const struct pg_syntax *syntax = source_syntax(root);
	if (!syntax) return NULL;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	if (module_work(root)->right) return module_work(root)->right;
	struct pg_synthesis_job *registration = definition_registration(synthesis, source_scope(root), syntax);
	if (!registration) return NULL;
	module_work(root)->right = registration;
	module_work(root)->left = registration;
	depend(synthesis, root, registration);
	return registration;
}

static int definition_input(struct pg_synthesis_job *registration, size_t index,
	struct pg_synthesis_job *producer)
{
	const struct pg_syntax *syntax = registration->inputs[1];
	struct definition_state *state = definition_state(registration);
	if (!producer || index >= syntax->item_count) return -1;
	if (state->entries[index]) return state->entries[index] == producer ? 0 : -1;
	state->entries[index] = producer;
	return 0;
}

int pg_synthesis_retain_definition_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *root, size_t index, struct pg_synthesis_job *producer)
{
	if (!root || root->pending.owner != synthesis->owner_key) return -1;
	if (!producer || producer->pending.owner != synthesis->owner_key) return -1;
	struct pg_synthesis_job *registration = pg_synthesis_work_role(root) == DEFINITION_SCOPE_JOB
		? root : pg_synthesis_prepare_module(synthesis, root);
	if (!registration) return -1;
	return definition_input(registration, index, producer);
}

const struct pg_source_scope *pg_synthesis_definition_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions)
{
	struct pg_synthesis_job *job = definition_registration(synthesis, scope, definitions);
	return job && job->status != PG_SYNTHESIS_ERROR ? definition_state(job)->scope : NULL;
}

struct pg_synthesis_job *pg_synthesis_definition_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions,
	const struct pg_syntax *expression)
{
	if (!definitions || definitions->kind != PG_SYNTAX_DEFINITIONS || !expression) return NULL;
	struct pg_synthesis_job *registration = definition_registration(synthesis, scope, definitions);
	if (!registration) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, DEFINITION_JOB, registration, expression);
	if (job && job->status == PG_SYNTHESIS_PENDING && !definition_work(job)->active && !job->dependency)
		depend(synthesis, job, registration);
	return job;
}

static const struct pg_evidence *value_type(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION_TYPE)
		return pg_prove_thunk_type(synthesis->typing, proof);
	return pg_prove_value_type(synthesis->typing, proof);
}


static const struct pg_evidence *value(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE_TYPE)
		return pg_prove_type_value(synthesis->typing, proof);
	return pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE ? proof : NULL;
}

static uint64_t name_hash(struct pg_token name)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	for (size_t i = 0; i < name.length; ++i) hash = (hash ^ (unsigned char)name.text[i]) * UINT64_C(1099511628211);
	return hash;
}

static struct block_name *lookup_name(const struct pg_index *names, struct pg_token name)
{
	uint64_t hash = name_hash(name);
	for (struct pg_index_entry *entry = pg_index_candidates(names, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct block_name *found = (struct block_name *)entry;
		if (found->name.length != name.length) continue;
		if (memcmp(found->name.text, name.text, name.length) == 0) return found;
	}
	return NULL;
}

struct source_reference {
	const struct pg_object *binder;
	struct pg_synthesis_input value;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	struct pg_synthesis_input context;
};

static struct pg_synthesis_input lookup_definition(const struct definition_state *state, struct pg_token token)
{
	struct block_name *name = lookup_name(&state->names, token);
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(name ? (name->producer ? name->producer : name->imported) : NULL)};
}

static struct source_reference lookup_scope(const struct pg_source_scope *scope, struct pg_token token)
{
	for (; scope; scope = scope->parent) {
		if (scope->definitions && token.kind == PG_TOKEN_IDENT) {
			struct pg_synthesis_input value = lookup_definition(definition_state(scope->definitions), token);
			if (value.pending) return (struct source_reference){.value = value, .context = scope->context};
		}
		if (scope->name.kind != token.kind) continue;
		/* Punctuation tokens carry their spelling in kind, not text. */
		if (token.kind != '#' && token.kind != '*') {
			if (scope->name.length != token.length) continue;
			if (memcmp(scope->name.text, token.text, token.length) != 0) continue;
		}
		return (struct source_reference){scope->binder, scope->value, scope->exports, scope->module, scope->context};
	}
	return (struct source_reference){0};
}

struct pg_synthesis_input pg_synthesis_named_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_token name)
{
	if (!synthesis || !scope || scope->owner != synthesis->owner_key) return (struct pg_synthesis_input){0};
	if (name.kind != PG_TOKEN_IDENT || !name.text) return (struct pg_synthesis_input){0};
	return lookup_scope(scope, name).value;
}

struct pg_synthesis_job *pg_synthesis_source_origin(const struct pg_synthesis_job *producer)
{
	if (!producer) return NULL;
	if (pg_synthesis_work_role(producer) == DEFINITION_JOB) return definition_work(producer)->body;
	if (pg_synthesis_work_role(producer) == SOURCE_EXPECT_JOB)
		return pg_pending_job(pg_synthesis_work_dependency(producer, 1).pending);
	struct pg_synthesis_job *operand = pg_synthesis_quote_operand(producer);
	if (operand) return operand;
	const struct module_work *module = module_work(producer);
	if (module) return source_module_projection(producer).rule;
	const struct reference_work *local = reference_work(producer);
	if (!local) return NULL;
	const struct pg_syntax *syntax = source_syntax(producer);
	switch (syntax->kind) {
	case PG_SYNTAX_ATOM:
		if (local->left) return local->left;
		if (syntax->token.kind != PG_TOKEN_IDENT) return NULL;
		for (const struct pg_source_scope *scope = source_scope(producer); scope; scope = scope->parent) {
			const struct pg_synthesis_job *registration = scope->definitions;
			if (registration && definition_state(registration)->indexed
				< ((const struct pg_syntax *)registration->inputs[1])->item_count) return NULL;
		}
		return pg_pending_job(lookup_scope(source_scope(producer), syntax->token).value.pending);
	case PG_SYNTAX_IMPORT:
		return local->left;
	case PG_SYNTAX_QUALIFIED:
		return local->left;
	default: return NULL;
	}
}


struct marker_shadow {
	struct pg_token name;
	const struct marker_shadow *parent;
};
struct marker_task {
	const struct pg_syntax *syntax;
	const struct marker_shadow *shadow;
	struct marker_task *next;
};

static int marker_push(struct pg_graph *arena, struct marker_task **tasks,
	const struct pg_syntax *syntax, const struct marker_shadow *shadow)
{
	if (!syntax) return 0;
	struct marker_task *task = pg_alloc(arena, sizeof(*task));
	if (!task) return -1;
	*task = (struct marker_task){syntax, shadow, *tasks};
	*tasks = task;
	return 0;
}

static const struct marker_shadow *marker_bind(struct pg_graph *arena,
	const struct marker_shadow *parent, struct pg_token name)
{
	struct marker_shadow *shadow = pg_alloc(arena, sizeof(*shadow));
	if (shadow) *shadow = (struct marker_shadow){name, parent};
	return shadow;
}

static const struct pg_object *associated_binder(const struct pg_source_scope *scope,
	const struct pg_object *value, enum pg_source_association association)
{
	for (; scope; scope = scope->parent)
		if (scope->association == association && scope->associated_binder == value) return scope->binder;
	return NULL;
}

static const struct pg_object *hypothesis_field(const struct pg_source_scope *scope,
	struct pg_token name)
{
	const struct pg_object *value = lookup_scope(scope, name).binder;
	if (!value) return NULL;
	const struct pg_object *graph = associated_binder(scope, value, PG_SOURCE_GRAPH);
	return graph ? graph : value;
}

/* Lexical dependency discovery only. The normal binder resolver and kernel
 * still check every use. Nested pattern/Lambda/block binders shadow names;
 * declarations introduce their own Self marker. No classifier is guessed. */
static int branch_dependencies(const struct pg_source_scope *scope,
	const struct pg_context *prefix, const struct pg_syntax *body,
	const struct pg_object *scrutinee, int *uses_scrutinee)
{
	int self = lookup_scope(scope, (struct pg_token){.kind = '*'}).binder != NULL;
	struct pg_graph arena = {0};
	struct marker_task *tasks = NULL;
	int result = -1, needs_ih = 0;
	if (marker_push(&arena, &tasks, body, NULL)) goto done;
	while (tasks) {
		const struct pg_syntax *syntax = tasks->syntax;
		const struct marker_shadow *shadow = tasks->shadow;
		tasks = tasks->next;
		size_t count = syntax->item_count;
		if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_BLOCK) {
			count = pg_synthesis_block_end(syntax);
			syntax = syntax->left;
		}
		if (syntax->kind == PG_SYNTAX_DECLARATION) continue;
		if (syntax->kind == PG_SYNTAX_QUALIFIED) {
			/* The member is resolved in its owner, not the ambient scope. */
			if (marker_push(&arena, &tasks, syntax->left, shadow)) goto done;
			continue;
		}
		if (scrutinee && syntax->kind == PG_SYNTAX_ATOM && syntax->token.kind == PG_TOKEN_IDENT) {
			const struct marker_shadow *bound = shadow;
			while (bound && !same_name(bound->name, syntax->token)) bound = bound->parent;
			if (!bound && lookup_scope(scope, syntax->token).binder == scrutinee) *uses_scrutinee = 1;
		}
		if (!self && syntax->kind == PG_SYNTAX_APPLICATION && syntax->left->kind == PG_SYNTAX_ATOM &&
			syntax->left->token.kind == '*' && syntax->right->kind == PG_SYNTAX_ATOM) {
			struct pg_token name = syntax->right->token;
			const struct marker_shadow *bound = shadow;
			while (bound && !same_name(bound->name, name)) bound = bound->parent;
			if (!bound) {
				const struct pg_object *field = hypothesis_field(scope, name);
				for (const struct pg_context *context = pg_evidence_context(pg_synthesis_scope_context(scope));
					context && context != prefix; context = context->parent) {
					if (context->binder == field) {
						needs_ih = 1;
						if (!scrutinee) { result = 1; goto done; }
					}
				}
			}
		}
		if (syntax->kind == PG_SYNTAX_LAMBDA || syntax->kind == PG_SYNTAX_PI) {
			struct pg_token name = syntax->kind == PG_SYNTAX_LAMBDA ? syntax->token
				: syntax->left->kind == PG_SYNTAX_BINDER ? syntax->left->token : (struct pg_token){0};
			const struct marker_shadow *inner = shadow;
			if (name.kind == PG_TOKEN_IDENT) {
				inner = marker_bind(&arena, shadow, name);
				if (!inner) goto done;
			}
			if (marker_push(&arena, &tasks, syntax->left, shadow)) goto done;
			if (marker_push(&arena, &tasks, syntax->right, inner)) goto done;
			continue;
		}
		if (syntax->kind == PG_SYNTAX_ELIMINATION) {
			if (marker_push(&arena, &tasks, syntax->left, shadow)) goto done;
			int handler = pg_synthesis_handler_syntax(syntax);
			for (size_t i = 0; i < syntax->item_count; ++i) {
				const struct pg_syntax *clause = syntax->items[i].expression;
				const struct marker_shadow *inner = shadow;
				for (size_t j = 0; j < clause->item_count; ++j) {
					struct pg_token name = clause->items[j].operation == PG_TOKEN_ASSIGN
						? clause->items[j].expression->token : clause->items[j].name;
					inner = marker_bind(&arena, inner, name);
					if (!inner) goto done;
				}
				if (clause->left->kind != PG_SYNTAX_ATOM || handler)
					if (marker_push(&arena, &tasks, clause->left, shadow)) goto done;
				if (marker_push(&arena, &tasks, clause->right, inner)) goto done;
			}
			continue;
		}
		if (syntax->kind == PG_SYNTAX_DEFINITIONS) {
			for (size_t i = 0; i < syntax->item_count; ++i) {
				shadow = marker_bind(&arena, shadow, syntax->items[i].name);
				if (!shadow) goto done;
			}
		}
		if (marker_push(&arena, &tasks, syntax->left, shadow)) goto done;
		if (marker_push(&arena, &tasks, syntax->right, shadow)) goto done;
		for (size_t i = 0; i < count; ++i) {
			if (marker_push(&arena, &tasks, syntax->items[i].expression, shadow)) goto done;
			if (marker_push(&arena, &tasks, syntax->items[i].annotation, shadow)) goto done;
			if (syntax->kind == PG_SYNTAX_BLOCK && syntax->items[i].name.kind == PG_TOKEN_IDENT) {
				shadow = marker_bind(&arena, shadow, syntax->items[i].name);
				if (!shadow) goto done;
			}
		}
	}
	result = needs_ih;
done:
	pg_graph_destroy(&arena);
	return result;
}

static int branch_needs_ih(const struct pg_source_scope *scope,
	const struct pg_context *prefix, const struct pg_syntax *body)
{
	return branch_dependencies(scope, prefix, body, NULL, NULL);
}

int pg_synthesis_star_application(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_APPLICATION || syntax->left->kind != PG_SYNTAX_ATOM) return 0;
	return syntax->left->token.kind == '*';
}

int pg_synthesis_hypothesis_syntax(const struct pg_syntax *syntax)
{
	return pg_synthesis_star_application(syntax) && syntax->right->kind == PG_SYNTAX_ATOM;
}

/* Collect APP domain constraints on IH applications in their lexical scopes.
 * Lambda domains use the ordinary pending binding producer. An annotation
 * never supplies a demand, and no incomplete branch is accepted as evidence. */
static int motive_scan_push(struct match_branch *branch,
	const struct pg_syntax *syntax, const struct pg_source_scope *scope)
{
	/* Reuse retired traversal slots; only the active frontier must survive. */
	if (!branch->scan || branch->scan->count == branch->scan->capacity) {
		size_t count = branch->scan ? branch->scan->count : 0;
		size_t old_capacity = branch->scan ? branch->scan->capacity : 0;
		size_t capacity = old_capacity ? old_capacity * 2 : 4;
		if (capacity < old_capacity || capacity > (SIZE_MAX - sizeof(*branch->scan)) / sizeof(*branch->scan->items)) return -1;
		struct motive_frontier *scan = realloc(branch->scan, sizeof(*scan) + capacity * sizeof(*scan->items));
		if (!scan) return -1;
		scan->count = count;
		scan->capacity = capacity;
		branch->scan = scan;
	}
	branch->scan->items[branch->scan->count++] = (struct motive_scan){.syntax = syntax, .scope = scope};
	return 0;
}

/* 1 suspends, 0 completes, -1 is an allocation/input failure. Block scopes
 * belong to the source producer; constraint collection never recreates them. */
static int motive_demands(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct match_branch *branch, const struct pg_context *prefix)
{
	if (!branch->scan_started) {
		if (motive_scan_push(branch, branch->clause->right, branch->scope)) return -1;
		branch->scan_started = 1;
	}
	if (!branch->scan || !branch->scan->count) {
		free(branch->scan);
		branch->scan = NULL;
		return 0;
	}
	struct motive_scan *scan = &branch->scan->items[branch->scan->count - 1];
	const struct pg_syntax *syntax = scan->syntax;
	const struct pg_source_scope *scope = scan->scope;
	const struct pg_syntax *block = pg_synthesis_block_syntax(syntax);
	if (block && scan->item < pg_synthesis_block_end(syntax)) {
		struct pg_synthesis_job *producer = pg_synthesis_request(synthesis, scope, syntax);
		if (!producer) return -1;
		const struct pg_source_scope *visited = pg_synthesis_block_scope(producer, scan->item);
		if (!visited) {
			if (producer->status == PG_SYNTHESIS_PENDING) {
				if (pg_synthesis_dependency(producer)) depend(synthesis, job, (void *)pg_synthesis_dependency(producer));
				else pg_synthesis_enqueue(synthesis, job);
				return 1;
			}
			--branch->scan->count;
		} else {
			size_t i = scan->item++;
			if (motive_scan_push(branch, block->items[i].expression, visited)) return -1;
		}
		pg_synthesis_enqueue(synthesis, job); return 1;
	}
	--branch->scan->count;
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		if (motive_scan_push(branch, syntax->left, scope)) return -1;
	} else if (syntax->kind == PG_SYNTAX_LAMBDA) {
		struct pg_synthesis_job *binding = pg_synthesis_binding(synthesis, scope, syntax);
		const struct pg_source_scope *inner = pg_synthesis_binding_scope(binding);
		if (!inner) return -1;
		if (motive_scan_push(branch, syntax->right, inner)) return -1;
	} else if (syntax->kind == PG_SYNTAX_APPLICATION) {
		const struct pg_syntax *head = syntax->right;
		size_t count = 0;
		while (head->kind == PG_SYNTAX_APPLICATION && !pg_synthesis_hypothesis_syntax(head)) {
			++count;
			head = head->left;
		}
		if (pg_synthesis_hypothesis_syntax(head)) {
			const struct pg_object *field = hypothesis_field(scope, head->right->token);
			const struct pg_context *fields = pg_evidence_context(pg_synthesis_scope_context(branch->scope));
			while (fields && fields != prefix && fields->binder != field) fields = fields->parent;
			if (fields && fields != prefix) {
				if (count > (SIZE_MAX - sizeof(struct motive_demand)) / sizeof(struct pg_synthesis_job *)) return -1;
				struct motive_demand *demand = pg_alloc(synthesis->typing->graph,
					sizeof(*demand) + count * sizeof(*demand->arguments));
				if (!demand) return -1;
				demand->scope = scope;
				demand->field = field;
				demand->count = count;
				demand->callee = pg_synthesis_request(synthesis, scope, syntax->left);
				if (!demand->callee) return -1;
				const struct pg_syntax *application = syntax->right;
				for (size_t i = count; i; --i, application = application->left) {
					demand->arguments[i - 1] = pg_synthesis_request(synthesis, scope, application->right);
					if (!demand->arguments[i - 1]) return -1;
				}
				demand->next = branch->demands;
				branch->demands = demand;
			}
		}
		if (motive_scan_push(branch, syntax->left, scope)) return -1;
		if (motive_scan_push(branch, syntax->right, scope)) return -1;
	}
	pg_synthesis_enqueue(synthesis, job); return 1;
}

int pg_synthesis_source_star(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(synthesis, job, &scope, &syntax)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1;
	}
	if (!pg_synthesis_star_application(syntax)) return 0;
	if (lookup_scope(scope, (struct pg_token){.kind = '*'}).binder) return 0;
	if (!pg_synthesis_hypothesis_syntax(syntax)) goto rejected;
	const struct pg_object *field = hypothesis_field(scope, syntax->right->token);
	if (!field) goto rejected;
	const struct pg_object *binder = associated_binder(scope, field, PG_SOURCE_HYPOTHESIS);
	if (!binder) goto rejected;
	const struct pg_evidence *value = pg_prove_variable(synthesis->typing, pg_synthesis_scope_context(scope), binder);
	job->result = pg_prove_force(synthesis->typing, value);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return 1;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return 1;
}

static struct pg_synthesis_input source_reference_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input input)
{
	/* Storage quotation does not change the meaning of a source definition. */
	struct pg_synthesis_job *producer = pg_pending_job(input.pending);
	if (producer && pg_synthesis_work_role(producer) == DEFINITION_JOB && definition_work(producer)->body &&
		pg_evidence_judgement(pg_synthesis_result(definition_work(producer)->body)) == PG_JUDGEMENT_COMPUTATION)
		return (struct pg_synthesis_input){.pending =
			pg_synthesis_pending(pg_synthesis_plain_rule_inputs(synthesis, PG_FORCE_ELIM, NULL, 1, &input))};
	return input;
}

static const struct pg_source_scope *input_exports(const struct pg_synthesis *synthesis,
	struct pg_synthesis_input input)
{
	const struct pg_source_scope *exports = source_exports(synthesis, pg_pending_job(input.pending));
	if (exports) return exports;
	const struct pg_evidence *proof = pg_synthesis_input_result(input);
	if (!proof) return NULL;
	const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(proof));
	return metadata_exports(synthesis, origin);
}

static struct pg_synthesis_job *prepare_reference_rule(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct source_reference reference)
{
	struct reference_work *local = reference_work(job);
	if (reference.value.checked || reference.value.pending) {
		local->left = pg_pending_job(reference.value.pending);
		struct pg_synthesis_input term = source_reference_input(synthesis, reference.value);
		local->value_job = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_input[]){source_scope(job)->context, term});
		return local->value_job;
	}
	local->binder = reference.binder;
	local->left = pg_synthesis_plain_rule_inputs(synthesis, PG_VARIABLE, reference.binder, 1, &source_scope(job)->context);
	return local->left;
}

static enum pg_synthesis_status resolve_member(struct pg_synthesis *synthesis,
	struct source_reference *reference,
	struct pg_token token, struct pg_pending **dependency)
{
	if (reference->exports) {
		*reference = lookup_scope(reference->exports, token);
		return PG_SYNTHESIS_DONE;
	}
	if (reference->module) {
		struct pg_synthesis_job *module = reference->module;
		struct pg_synthesis_job *registration = definition_registration(synthesis, source_scope(module), source_syntax(module));
		if (!registration) return PG_SYNTHESIS_ERROR;
		*dependency = pg_synthesis_pending(registration);
		if (registration->status != PG_SYNTHESIS_DONE) return registration->status;
		struct block_name *member = lookup_name(&definition_state(registration)->names, token);
		if (!member || !member->producer) return PG_SYNTHESIS_REJECTED;
		*dependency = pg_synthesis_pending(module);
		if (module->status != PG_SYNTHESIS_DONE) return module->status;
		*reference = (struct source_reference){.value = {.pending = pg_synthesis_pending(member->producer)},
			.context = definition_state(registration)->scope->context};
		return PG_SYNTHESIS_DONE;
	}
	if (reference->value.checked || reference->value.pending) {
		/* Resolve in the defining context; the use-site rule checks projection. */
		struct pg_synthesis_input scope = reference->context;
		if (!pg_synthesis_input_owned(synthesis, scope)) return PG_SYNTHESIS_ERROR;
		struct pg_synthesis_input input = reference->value;
		*dependency = input.pending;
		if (input.pending && pg_synthesis_pending_status(input.pending) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(input.pending);
		const struct pg_source_scope *exports = input_exports(synthesis, input);
		if (exports) {
			*reference = lookup_scope(exports, token);
			return PG_SYNTHESIS_DONE;
		}
		input = source_reference_input(synthesis, input);
		if (!input.checked && !input.pending) return PG_SYNTHESIS_ERROR;
		*dependency = input.pending;
		if (input.pending && pg_synthesis_pending_status(input.pending) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(input.pending);
		struct pg_synthesis_job *producer;
		*dependency = scope.pending;
		if (scope.pending && pg_synthesis_pending_status(scope.pending) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(scope.pending);
		const struct pg_evidence *context = pg_synthesis_input_result(scope);
		const struct pg_evidence *proof = pg_prove_projection(synthesis->typing, context, pg_synthesis_input_result(input));
		if (!proof) return PG_SYNTHESIS_UNSUPPORTED;
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION) {
			producer = pg_synthesis_return(synthesis, context, proof);
			if (!producer) return PG_SYNTHESIS_ERROR;
			*dependency = pg_synthesis_pending(producer);
			if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
			proof = producer->result;
		}
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_TYPE_FAMILY) {
			producer = pg_synthesis_normalize(synthesis, context, proof);
			if (!producer) return PG_SYNTHESIS_ERROR;
			*dependency = pg_synthesis_pending(producer);
			if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
			proof = producer->result;
		} else proof = value_type(synthesis, proof);
		struct pg_pending *instance_job = pg_synthesis_inductive_instance(synthesis,
			(struct pg_synthesis_input){.checked = proof});
		if (!instance_job) return PG_SYNTHESIS_UNSUPPORTED;
		*dependency = instance_job;
		if (pg_synthesis_pending_status(instance_job) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(instance_job);
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) return PG_SYNTHESIS_ERROR;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(instance.formation));
		exports = metadata_exports(synthesis, origin);
		if (!exports) return PG_SYNTHESIS_UNSUPPORTED;
		struct source_reference member = lookup_scope(exports, token);
		if (!member.value.pending) return PG_SYNTHESIS_REJECTED;
		struct pg_constructor_input constructor_input;
		if (pg_synthesis_constructor_input(synthesis, pg_pending_job(member.value.pending), &constructor_input)) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = (struct source_reference){.value = {.pending = pg_synthesis_pending(pg_synthesis_constructor_value(synthesis,
			(struct pg_synthesis_input){.checked = instance.formation}, constructor_input.constructor, (struct pg_synthesis_input){.checked = instance.parameters}))}, .context = scope};
		return reference->value.pending ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR;
	}
	/* Nominal members require a typed declaration, never an older namespace. */
	return reference->binder ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_REJECTED;
}

static enum pg_synthesis_status resolve_reference(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct source_reference *reference, struct pg_pending **dependency)
{
	if (syntax->kind == PG_SYNTAX_IMPORT) {
		while (scope && !scope->imports) scope = scope->parent;
		if (!scope) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = lookup_scope(scope->imports, syntax->left->token);
		if (reference->value.checked || reference->value.pending) return PG_SYNTHESIS_DONE;
		return reference->exports || reference->module ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_REJECTED;
	}
	const struct pg_syntax *root = syntax;
	size_t count = 0;
	while (root->kind == PG_SYNTAX_QUALIFIED) { ++count; root = root->left; }
	if (root->kind == PG_SYNTAX_ATOM) {
		if (root->token.kind != PG_TOKEN_IDENT && root->token.kind != '#') return PG_SYNTHESIS_UNSUPPORTED;
		*reference = lookup_scope(scope, root->token);
	} else {
		if (!count) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = (struct source_reference){.value = {.pending = pg_synthesis_pending(pg_synthesis_request(synthesis, scope, root))},
			.context = scope->context};
		if (!reference->value.pending) return PG_SYNTHESIS_ERROR;
	}
	if (!count) return PG_SYNTHESIS_DONE;
	if (count > SIZE_MAX / sizeof(const struct pg_syntax *)) return PG_SYNTHESIS_ERROR;
	const struct pg_syntax **path = malloc(count * sizeof(*path));
	if (!path) return PG_SYNTHESIS_ERROR;
	for (size_t i = count; i; --i, syntax = syntax->left) path[i - 1] = syntax->right;
	enum pg_synthesis_status status = PG_SYNTHESIS_DONE;
	for (size_t i = 0; i < count; ++i) {
		status = resolve_member(synthesis, reference, path[i]->token, dependency);
		if (status != PG_SYNTHESIS_DONE) break;
	}
	free(path);
	return status;
}

int pg_synthesis_graph_exports(struct pg_synthesis *synthesis,
	struct pg_function_graph_work *work, const struct pg_source_scope **output,
	struct pg_synthesis_graph_names **names_output)
{
	struct pg_synthesis_graph_names *names_input = *names_output;
	const struct pg_evidence *declaration = pg_function_graph_declaration(work);
	const struct pg_evidence *context = pg_context_parent_input(synthesis->typing, pg_evidence_premise(declaration, 0));
	const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing, context, context);
	const struct pg_data_layout *target = pg_data_schema_layout(pg_evidence_inductive_schema(declaration));
	const struct pg_source_scope *exports = pg_synthesis_intern_scope(synthesis,
		(struct pg_source_scope){.context = {.checked = context}});
	struct pg_graph temporary = {0};
	struct pg_index names = {0}, aliases = {0};
	int result = -1;
	if (!exports || pg_index_init(&names) || pg_index_init(&aliases)) goto done;
	/* Canonical names belong to this generated declaration, not to a source
	 * constructor that may occur at several leaves. Reserve them first. */
	for (size_t index = 0; index < pg_data_layout_count(target); ++index) {
		char text[5 + 3 * sizeof(size_t)];
		int length = snprintf(text, sizeof(text), "case%zu", index);
		if (length < 0 || (size_t)length >= sizeof(text)) goto done;
		char *spelling = pg_alloc(synthesis->typing->graph, (size_t)length + 1);
		struct block_name *name = pg_alloc(&temporary, sizeof(*name));
		if (!spelling || !name) goto done;
		memcpy(spelling, text, (size_t)length + 1);
		name->name = (struct pg_token){.kind = PG_TOKEN_IDENT, .text = spelling, .length = (size_t)length};
		name->producer = pg_synthesis_constructor_value(synthesis, (struct pg_synthesis_input){.checked = declaration},
			pg_data_constructor(target, index), (struct pg_synthesis_input){.checked = parameters});
		if (pg_index_insert(&names, &name->index, name_hash(name->name))) goto done;
		exports = pg_synthesis_name(synthesis, exports, name->name, (struct pg_synthesis_input){.pending = pg_synthesis_pending(name->producer)});
		if (!exports) goto done;
	}
	for (size_t index = 0; index < pg_data_layout_count(target); ++index) {
		struct pg_function_graph_case_source source;
		if (!pg_function_graph_case_source(work, index, &source)) goto done;
		if (!source.formation) continue;
		if (source.refined) names_input = NULL;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(source.formation));
		const struct pg_source_scope *source_exports = metadata_exports(synthesis, origin);
		if (!source_exports) goto done;
		const struct pg_object *constructor = pg_data_constructor(target, index);
		struct pg_synthesis_job *producer = pg_synthesis_constructor_value(synthesis, (struct pg_synthesis_input){.checked = declaration}, constructor, (struct pg_synthesis_input){.checked = parameters});
		for (const struct pg_source_scope *name = source_exports; name; name = name->parent) {
			struct pg_constructor_input input;
			if (pg_synthesis_constructor_input(synthesis, pg_pending_job(name->value.pending), &input) || input.constructor != source.constructor) continue;
			if (lookup_name(&names, name->name)) continue;
			struct block_name *alias = lookup_name(&aliases, name->name);
			if (alias) {
				if (alias->producer != producer) alias->producer = NULL;
				continue;
			}
			alias = pg_alloc(&temporary, sizeof(*alias));
			if (!alias) goto done;
			alias->name = name->name; alias->producer = producer;
			if (pg_index_insert(&aliases, &alias->index, name_hash(alias->name))) goto done;
		}
	}
	for (size_t i = 0; i < aliases.capacity; ++i) {
		for (struct pg_index_entry *entry = aliases.buckets[i]; entry; entry = entry->next) {
			const struct block_name *alias = (const struct block_name *)entry;
			if (!alias->producer) continue;
			exports = pg_synthesis_name(synthesis, exports, alias->name, (struct pg_synthesis_input){.pending = pg_synthesis_pending(alias->producer)});
			if (!exports) goto done;
		}
	}
	*output = exports; *names_output = names_input;
	result = 0;
done:
	pg_index_destroy(&names); pg_index_destroy(&aliases);
	pg_graph_destroy(&temporary);
	return result;
}

/* Source call slots are an interface layout, not an execution schedule.
 * Lexical shadowing and block cutoffs use the same marker walker as IH scope
 * analysis. The backend validates every slot against its typed call plan. */
int pg_synthesis_graph_order(struct pg_synthesis *synthesis, struct pg_function_graph_work *work,
	const struct pg_synthesis_job *origin, struct pg_synthesis_graph_names **names_input)
{
	if (!match_work(origin) || !match_work(origin)->match) return 0;
	if (!pg_function_graph_case_input(work)) return 0;
	struct pg_graph temporary = {0};
	size_t count = match_work(origin)->match->count;
	size_t trailing = pg_function_graph_trailing_arity(work);
	size_t generalized = match_work(origin)->match->generalized_count;
	if (generalized > trailing) return -1;
	if (count > SIZE_MAX / sizeof(struct pg_function_graph_order)) return -1;
	struct pg_function_graph_order *orders = pg_alloc(&temporary, count * sizeof(*orders));
	int result = -1;
	if (count && !orders) goto done;
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_graph_names)) / sizeof(struct source_case_layout)) goto done;
	struct pg_synthesis_graph_names *names = pg_alloc(synthesis->typing->graph,
		sizeof(*names) + count * sizeof(*names->cases));
	if (!names) goto done;
	names->origin = origin;
	for (size_t i = 0; i < count; ++i) {
		const struct match_branch *branch = &match_work(origin)->match->branches[i];
		const struct pg_syntax *clause = branch->clause;
		if (!clause) goto done;
		/* Named source patterns require their resolved field layout here too. */
		if (clause->item_count && clause->items[0].operation) goto done;
		const struct pg_syntax *body = clause->right;
		const struct marker_shadow *argument_shadow = NULL;
		for (size_t argument = generalized; argument < trailing; ++argument) {
			if (body->kind != PG_SYNTAX_LAMBDA) goto done;
			argument_shadow = marker_bind(&temporary, argument_shadow, body->token);
			if (!argument_shadow) goto done;
			body = body->right;
		}
		const struct pg_syntax *top = pg_synthesis_block_syntax(body);
		size_t groups = top ? pg_synthesis_block_end(body) : 1;
		if (clause->item_count > SIZE_MAX / sizeof(size_t)) goto done;
		size_t *positions = pg_alloc(&temporary, clause->item_count * sizeof(*positions));
		size_t *last_group = pg_alloc(&temporary, clause->item_count * sizeof(*last_group));
		if (clause->item_count && (!positions || !last_group)) goto done;
		for (size_t field = 0; field < clause->item_count; ++field) {
			last_group[field] = SIZE_MAX;
			const struct pg_object *binder = lookup_scope(branch->scope, clause->items[field].name).binder;
			const struct pg_context *context = pg_evidence_context(branch->fields);
			size_t position = branch->field_count;
			while (position && context->binder != binder) { --position; context = context->parent; }
			if (!position) goto done;
			positions[field] = position - 1;
		}
		struct marker_task *tasks = NULL;
		struct slot { size_t field; struct pg_token name; struct slot *next; };
		struct slot *slots = NULL, **tail = &slots;
		const struct marker_shadow *top_shadow = argument_shadow;
		for (size_t group = 0; group < groups; ++group) {
			const struct pg_syntax *expression = top ? top->items[group].expression : body;
			const struct pg_syntax *result_call = expression;
			while (result_call->kind == PG_SYNTAX_EXPECT) result_call = result_call->left;
			while (result_call->kind == PG_SYNTAX_APPLICATION && !pg_synthesis_hypothesis_syntax(result_call))
				result_call = result_call->left;
			if (marker_push(&temporary, &tasks, expression, top_shadow)) goto done;
			while (tasks) {
				const struct pg_syntax *syntax = tasks->syntax;
				const struct marker_shadow *shadow = tasks->shadow;
				tasks = tasks->next;
				if (pg_synthesis_hypothesis_syntax(syntax)) {
					struct pg_token name = syntax->right->token;
					const struct marker_shadow *bound = shadow;
					while (bound && !same_name(bound->name, name)) bound = bound->parent;
					if (bound) continue;
					size_t field = 0;
					while (field < clause->item_count && !same_name(clause->items[field].name, name)) ++field;
					if (field == clause->item_count) continue;
					/* Field identity alone cannot distinguish reordered occurrences
					 * of the same call inside one application. Require an explicit
					 * sequencing boundary until occurrence provenance is retained. */
					if (last_group[field] != SIZE_MAX && (!top || last_group[field] == group)) goto done;
					last_group[field] = group;
					struct slot *slot = pg_alloc(&temporary, sizeof(*slot));
					if (!slot || orders[i].count == SIZE_MAX) goto done;
					slot->field = positions[field];
					if (top && syntax == result_call) slot->name = top->items[group].name;
					*tail = slot; tail = &slot->next;
					++orders[i].count;
					continue;
				}
				const struct pg_syntax *block = pg_synthesis_block_syntax(syntax);
				if (block) {
					struct marker_task *items = NULL;
					for (size_t item = 0; item < pg_synthesis_block_end(syntax); ++item) {
						if (marker_push(&temporary, &items, block->items[item].expression, shadow)) goto done;
						if (block->items[item].name.kind == PG_TOKEN_IDENT) {
							shadow = marker_bind(&temporary, shadow, block->items[item].name);
							if (!shadow) goto done;
						}
					}
					while (items) {
						struct marker_task *item = items;
						items = item->next;
						item->next = tasks; tasks = item;
					}
					continue;
				}
				if (syntax->kind == PG_SYNTAX_APPLICATION) {
					if (marker_push(&temporary, &tasks, syntax->right, shadow)) goto done;
					if (marker_push(&temporary, &tasks, syntax->left, shadow)) goto done;
				} else if (syntax->kind == PG_SYNTAX_EXPECT) {
					if (marker_push(&temporary, &tasks, syntax->left, shadow)) goto done;
				}
			}
			if (top && top->items[group].name.kind == PG_TOKEN_IDENT) {
				top_shadow = marker_bind(&temporary, top_shadow, top->items[group].name);
				if (!top_shadow) goto done;
			}
		}
		if (orders[i].count > SIZE_MAX / sizeof(size_t)) goto done;
		size_t *fields = pg_alloc(&temporary, orders[i].count * sizeof(*fields));
		if (orders[i].count && !fields) goto done;
		struct source_case_layout *layout = &names->cases[i];
		if (trailing > SIZE_MAX - branch->field_count) goto done;
		if (orders[i].count > (SIZE_MAX - branch->field_count - trailing) / 2) goto done;
		layout->count = branch->field_count + trailing + 2 * orders[i].count;
		if (layout->count > SIZE_MAX / sizeof(*layout->fields)) goto done;
		layout->fields = pg_alloc(synthesis->typing->graph, layout->count * sizeof(*layout->fields));
		if (layout->count && !layout->fields) goto done;
		for (size_t field = 0; field < clause->item_count; ++field)
			layout->fields[positions[field]].name = clause->items[field].name;
		for (size_t slot = 0; slots; ++slot, slots = slots->next) {
			fields[slot] = slots->field;
			size_t value = branch->field_count + trailing + 2 * slot;
			layout->fields[value].name = slots->name;
			layout->fields[value + 1].graph_value = value + 1;
		}
		orders[i].fields = fields;
	}
	result = pg_function_graph_source_order(work, count, orders);
	if (!result) *names_input = names;
done:
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_synthesis_job *pg_synthesis_function_graph_request(struct pg_synthesis *synthesis,
	const struct pg_evidence *function)
{
	if (!synthesis || !pg_evidence_owned_by(function, synthesis->typing)) return NULL;
	const struct pg_occurrence *subject = pg_evidence_subject(function), *body = subject;
	if (!subject) return NULL;
	while (body->core->kind == PG_LAMBDA && body->operand_count == 1) body = body->operands[0];
	const struct source_metadata *origin = source_metadata(synthesis, body);
	return pg_synthesis_function_graph(synthesis, function, origin ? origin->match : NULL);
}

static struct pg_synthesis_input graph_reference_output(const struct pg_synthesis_job *job)
{
	const struct graph_reference_work *local = pg_synthesis_work_state(job, SOURCE_GRAPH_REFERENCE);
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(local->value_job)};
}

static void graph_reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (source_context_wait(synthesis, job)) return;
	struct graph_reference_work *local = pg_synthesis_work_state(job, SOURCE_GRAPH_REFERENCE);
	if (!local->value_job) {
		if (!local->function_source) {
			struct source_reference reference;
			struct pg_pending *dependency = NULL;
			const struct pg_syntax *name = source_syntax(job)->left;
			enum pg_synthesis_status status = resolve_reference(synthesis, source_scope(job), name, &reference, &dependency);
			if (status == PG_SYNTHESIS_PENDING) { pg_synthesis_await_pending(synthesis, job, dependency); return; }
			if (status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, status); return; }
			if (reference.binder) {
				const struct pg_object *binder = associated_binder(source_scope(job), reference.binder, PG_SOURCE_GRAPH);
				if (!binder) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				job->result = pg_prove_variable(synthesis->typing, pg_synthesis_scope_context(source_scope(job)), binder);
				pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
				return;
			}
			if (!reference.value.checked && !reference.value.pending) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			if (pg_synthesis_await_input(synthesis, job, reference.value)) return;
			local->function_source = pg_alloc(synthesis->typing->graph, sizeof(*local->function_source));
			if (!local->function_source) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			local->function_source->function = pg_synthesis_input_result(reference.value);
		}
		int status = pg_function_source_advance(synthesis->typing, local->function_source);
		if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
		if (status < 0) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *graph = pg_synthesis_function_graph_request(synthesis, local->function_source->function);
		if (!graph) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_synthesis_input premises[] = {source_scope(job)->context, {.pending = pg_synthesis_pending(graph)}};
		local->value_job = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
	}
	pg_synthesis_forward(synthesis, job, local->value_job);
}

static enum pg_synthesis_status resolve_source_reference(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct source_reference *reference,
	struct pg_pending **dependency)
{
	struct reference_work *local = reference_work(job);
	enum pg_synthesis_status status = resolve_reference(synthesis, source_scope(job), source_syntax(job), reference, dependency);
	if (status != PG_SYNTHESIS_DONE || !local->context_allocation) return status;
	struct pg_constructor_input input;
	if (pg_synthesis_constructor_input(synthesis, pg_pending_job(reference->value.pending), &input)) return PG_SYNTHESIS_REJECTED;
	struct pg_synthesis_input inputs[] = {input.formation, input.parameters};
	for (size_t i = 0; i < 2; ++i) {
		*dependency = inputs[i].pending;
		if (*dependency && pg_synthesis_pending_status(*dependency) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(*dependency);
	}
	const struct pg_evidence *formation = pg_synthesis_input_result(input.formation);
	const struct pg_evidence *parameters = pg_synthesis_input_result(input.parameters);
	const struct pg_context *prefix = pg_evidence_context(pg_evidence_premise(parameters, 1));
	if (!pg_context_same_allocation_shape(local->context_allocation->prefix, prefix)) return PG_SYNTHESIS_REJECTED;
	const struct pg_context *fields = local->context_allocation->end;
	const struct pg_evidence *declared = pg_data_schema_fields(
		pg_evidence_inductive_schema(formation), input.constructor);
	size_t count;
	if (!declared || pg_context_extension_size(pg_evidence_context(declared),
		pg_evidence_context(pg_evidence_premise(formation, 0)), &count)
		|| count != local->context_allocation->count) return PG_SYNTHESIS_REJECTED;
	if (!input.allocated && !pg_synthesis_constructor_scope_at(synthesis, input.formation, input.constructor,
		input.parameters, local->context_allocation->prefix, fields)) return PG_SYNTHESIS_REJECTED;
	/* A pending allocation is not the checked field structure. Compare against
	 * the shared worker's synthesized result, never another use's saved types. */
	struct pg_synthesis_input scope = pg_synthesis_constructor_fields(pg_pending_job(reference->value.pending));
	*dependency = scope.pending;
	if (*dependency && pg_synthesis_pending_status(*dependency) != PG_SYNTHESIS_DONE) return pg_synthesis_pending_status(*dependency);
	return pg_context_same_allocation_shape(pg_evidence_context_map(pg_synthesis_input_result(scope))->destination, fields)
		? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED;
}

static void reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	/* Lexical discovery precedes row checking, but never name registration. */
	if (pg_synthesis_scope_wait(synthesis, job, source_scope(job))) return;
	if (atomic_rule_step(synthesis, job)) return;
	if (source_context_wait(synthesis, job)) return;
	struct reference_work *local = reference_work(job);
	if (local->left) {
		if (local->left->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->left->status); return; }
		const struct pg_evidence *proof = pg_synthesis_result(local->left);
		if (!proof || !pg_evidence_subject(proof)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (source_syntax(job)->kind == PG_SYNTAX_IMPORT && pg_evidence_context(proof)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct source_reference reference = {.value = {.pending = pg_synthesis_pending(local->left)}};
		pg_synthesis_forward(synthesis, job, prepare_reference_rule(synthesis, job, reference));
		return;
	}
	struct pg_token token = source_syntax(job)->token;
	if (source_syntax(job)->kind == PG_SYNTAX_ATOM && token.kind == '*') {
		/* An explicitly supplied type assumption stays in the proof context.
		 * This is not recursive declaration admission or an IH lookup. */
		struct source_reference reference = lookup_scope(source_scope(job), token);
		if (!reference.binder) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		job->result = pg_prove_variable(synthesis->typing, pg_synthesis_scope_context(source_scope(job)), reference.binder);
		if (job->result && pg_evidence_judgement(job->result) != PG_JUDGEMENT_TYPE_FAMILY)
			job->result = pg_prove_value_type(synthesis->typing, job->result);
		if (!job->result) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	} else {
		struct source_reference reference;
		struct pg_pending *dependency = NULL;
		enum pg_synthesis_status status = resolve_source_reference(synthesis, job, &reference, &dependency);
		if (status == PG_SYNTHESIS_PENDING) { pg_synthesis_await_pending(synthesis, job, dependency); return; }
		if (status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, status); return; }
		if (reference.value.checked) {
			if (source_syntax(job)->kind == PG_SYNTAX_IMPORT && pg_evidence_context(reference.value.checked)) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			pg_synthesis_forward(synthesis, job, prepare_reference_rule(synthesis, job, reference));
			return;
		}
		if (reference.value.pending) {
			local->left = pg_pending_job(reference.value.pending);
			depend(synthesis, job, local->left);
			return;
		}
		if (reference.binder)
			job->result = pg_prove_variable(synthesis->typing, pg_synthesis_scope_context(source_scope(job)), reference.binder);
		if (!job->result) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

enum pg_comparison_status pg_synthesis_probe_advance(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_comparison *work)
{
	enum pg_comparison_status status = pg_comparison_advance(work, 1);
	if (status == PG_COMPARISON_PENDING) pg_synthesis_enqueue(synthesis, job);
	if (status == PG_COMPARISON_ERROR) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return status;
}

enum pg_comparison_status pg_synthesis_independence(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_comparison *work, const struct pg_term *term, const struct pg_object *binder)
{
	if (!work->state && pg_independence_init(work, term, binder)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
		return PG_COMPARISON_ERROR;
	}
	return pg_synthesis_probe_advance(synthesis, job, work);
}


static int same_name(struct pg_token left, struct pg_token right)
{
	if (left.length != right.length) return 0;
	return !left.length || memcmp(left.text, right.text, left.length) == 0;
}

int pg_synthesis_register_name(struct pg_synthesis *synthesis, struct pg_index *names, struct pg_token name)
{
	if (!name.length) return 0;
	if (lookup_name(names, name)) return 1;
	struct block_name *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return -1;
	entry->name = name;
	return pg_index_insert(names, &entry->index, name_hash(name));
}


static void definition_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct definition_work *local = definition_work(job);
	struct pg_synthesis_job *registration = (void *)job->inputs[0];
	const struct pg_source_scope *scope = definition_state(registration)->scope;
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (pg_synthesis_await_input(synthesis, job, scope->context)) return;
	if (!pg_synthesis_scope_context(scope)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (!local->active) {
		if (registration->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, registration);
		else pg_synthesis_finish(synthesis, job, registration->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : registration->status);
		return;
	}
	if (!local->body) {
		local->body = pg_synthesis_request(synthesis, scope, job->inputs[1]);
		depend(synthesis, job, local->body);
		return;
	}
	if (pg_synthesis_await(synthesis, job, local->body)) return;
	const struct pg_evidence *result = pg_synthesis_result(local->body);
	if (pg_evidence_judgement(result) == PG_JUDGEMENT_COMPUTATION) {
		if (synthesis->definition_policy == PG_DEFINITION_EXPLICIT_THUNK) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		result = pg_prove_thunk(synthesis->typing, result);
	}
	job->result = result;
	pg_synthesis_finish(synthesis, job, result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void source_expect_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct source_expect_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	const struct pg_source_scope *scope = job->inputs[0];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	if (pg_synthesis_await_input(synthesis, job, scope->context)) return;
	if (!pg_synthesis_scope_context(scope)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (local->check) { pg_synthesis_forward(synthesis, job, local->check); return; }
	struct pg_synthesis_input parts[] = {pg_synthesis_work_dependency(job, 1), pg_synthesis_work_dependency(job, 3)};
	for (size_t i = 0; i < 2; ++i) {
		if (pg_synthesis_await_input(synthesis, job, parts[i])) return;
	}
	struct pg_synthesis_input term = source_reference_input(synthesis, parts[0]);
	if (!pg_synthesis_input_owned(synthesis, term)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (pg_synthesis_work_role(job) == BINDING_EXPECT_JOB) {
		term = pg_synthesis_normalize_classifier(synthesis, scope->context, term);
		if (!pg_synthesis_input_owned(synthesis, term)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (pg_synthesis_await_input(synthesis, job, term)) return;
	const struct pg_evidence *left = pg_prove_projection(synthesis->typing,
		pg_synthesis_scope_context(scope), pg_synthesis_input_result(term));
	const struct pg_evidence *right = pg_prove_projection(synthesis->typing,
		pg_synthesis_scope_context(scope), pg_synthesis_input_result(parts[1]));
	if (!left || !right) goto rejected;
	right = pg_synthesis_type_input(synthesis, job, &local->returned, pg_synthesis_scope_context(scope), right);
	if (!right) return;
	if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE_TYPE) left = value(synthesis, left);
	if (!left) goto rejected;
	if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) right = value_type(synthesis, right);
	else {
		enum pg_totality totality = PG_TOTALITY_TOTAL;
		const struct pg_effect_row *effects = pg_effect_row(synthesis->typing->graph, 0, NULL);
		const struct pg_term *content;
		/* A binding annotation checks the result, not the effects needed
		 * to obtain it. Function results use the same U(Pi) as binders. */
		int result_binding = pg_synthesis_work_role(job) == BINDING_EXPECT_JOB &&
			pg_computation_type_view(pg_evidence_classifier(left), &totality, &effects, &content);
		if (result_binding || pg_evidence_judgement(right) != PG_JUDGEMENT_COMPUTATION_TYPE)
			right = pg_prove_computation_type(synthesis->typing,
				totality, effects, value_type(synthesis, right));
	}
	if (!right) goto rejected;
	local->check = pg_synthesis_expect_inputs(synthesis,
		(struct pg_synthesis_input){.checked = left}, (struct pg_synthesis_input){.checked = right});
	pg_synthesis_forward(synthesis, job, local->check);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

int pg_synthesis_definition_frontier(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *registration, struct pg_definition_frontier *view)
{
	if (!synthesis || !registration || !view || registration->pending.owner != synthesis->owner_key
		|| pg_synthesis_work_role(registration) != DEFINITION_SCOPE_JOB) return -1;
	const struct definition_state *state = definition_state(registration);
	const struct pg_syntax *syntax = registration->inputs[1];
	*view = (struct pg_definition_frontier){.count = syntax->item_count, .indexed = state->indexed,
		.activated = state->activated, .position = state->next, .entries = state->entries,
		.complete = registration->status == PG_SYNTHESIS_DONE};
	return 0;
}

int pg_synthesis_definition_resume(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *registration, const struct pg_definition_frontier *view)
{
	struct pg_definition_frontier current;
	if (!view || pg_synthesis_definition_frontier(synthesis, registration, &current)) return -1;
	if (registration->status != PG_SYNTHESIS_PENDING) return -1;
	if (current.indexed || current.activated || current.complete || view->count != current.count) return -1;
	if (view->indexed > view->count || view->activated > view->indexed) return -1;
	if (view->position > view->activated || (view->position && !view->complete)) return -1;
	if (view->activated && view->indexed != view->count) return -1;
	if (view->count && !view->entries) return -1;
	const struct pg_source_scope *scope = registration->inputs[0];
	const struct pg_syntax *syntax = registration->inputs[1];
	struct definition_state *state = definition_state(registration);
	if ((unsigned)view->complete > 1) return -1;
	if (view->complete) {
		if (view->indexed != view->count || view->activated != view->count) return -1;
		if (scope->registration && scope->registration->status != PG_SYNTHESIS_DONE) return -1;
	}
	struct pg_index names;
	if (pg_index_init(&names)) return -1;
	/* Validate the transported recipe links, then rebuild a disposable index.
	 * No request, subscription, checked result, or cursor is published here. */
	for (size_t i = 0; i < view->count; ++i) {
		struct pg_synthesis_job *producer = view->entries[i];
		const struct pg_syntax_item *item = &syntax->items[i];
		if (state->entries[i] && state->entries[i] != producer) goto fail;
		int registered = i < view->indexed && item->operation != PG_TOKEN_EXPECT;
		if (!producer) {
			if (registered || i < view->activated) goto fail;
			continue;
		}
		if (producer->pending.owner != synthesis->owner_key) goto fail;
		const struct pg_source_scope *incoming;
		const struct pg_syntax *expression;
		switch (item->operation) {
		case PG_TOKEN_ASSIGN:
			if (pg_synthesis_work_role(producer) != DEFINITION_JOB || producer->inputs[0] != registration
				|| producer->inputs[1] != item->expression) goto fail;
			if (producer->status != PG_SYNTHESIS_PENDING) goto fail;
			if (definition_work(producer)->active || definition_work(producer)->body) goto fail;
			break;
		case PG_SYNTAX_IMPORT:
			if (pg_synthesis_source_input(synthesis, producer, &incoming, &expression) || incoming != scope) goto fail;
			/* Repeated imports share the first name's source recipe. */
			struct block_name *prior = lookup_name(&names, item->name);
			if (prior && prior->imported) {
				if (producer != prior->imported) goto fail;
			} else if (expression != item->expression) goto fail;
			break;
		case PG_TOKEN_EXPECT:
			if (pg_synthesis_work_role(producer) != SOURCE_EXPECT_JOB || producer->inputs[0] != state->scope) goto fail;
			struct pg_synthesis_job *type = pg_pending_job(pg_synthesis_work_dependency(producer, 3).pending);
			struct pg_derivation_input header;
			if (item->expression->kind == PG_SYNTAX_ATOM && !literal_header(synthesis, item->expression->token, &header)) {
				/* This checks the closed source recipe, not its typing. Ordinary
				 * checking and SOURCE_EXPECT's projection validate its context. */
				if (!pg_synthesis_rule_header_matches(type, &header)) goto fail;
			} else {
				if (pg_synthesis_source_input(synthesis, type, &incoming, &expression)) goto fail;
				if (incoming != state->scope || expression != item->expression) goto fail;
			}
			break;
		default: goto fail;
		}
		if (!registered) continue;
		if (pg_synthesis_register_name(synthesis, &names, item->name) < 0) goto fail;
		struct block_name *name = lookup_name(&names, item->name);
		if (!name) goto fail;
		if (item->operation == PG_TOKEN_ASSIGN) {
			if (name->producer) goto fail;
			name->producer = producer;
		} else {
			if (name->imported && name->imported != producer) goto fail;
			name->imported = producer;
		}
	}
	for (size_t i = 0; i < view->activated; ++i) {
		const struct pg_syntax_item *item = &syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) continue;
		struct block_name *name = lookup_name(&names, item->name);
		struct pg_synthesis_input term = name
			? (struct pg_synthesis_input){.pending = pg_synthesis_pending(name->producer ? name->producer : name->imported)}
			: lookup_scope(scope, item->name).value;
		struct pg_synthesis_input expected = pg_synthesis_work_dependency(view->entries[i], 1);
		if (!term.checked && !term.pending) goto fail;
		if (term.checked != expected.checked || term.pending != expected.pending) goto fail;
	}
	pg_index_destroy(&state->names);
	state->names = names;
	for (size_t i = 0; i < view->count; ++i) {
		state->entries[i] = view->entries[i];
		if (i >= view->indexed || syntax->items[i].operation != PG_TOKEN_ASSIGN) continue;
		if (i < view->activated) definition_work(state->entries[i])->active = 1;
	}
	state->indexed = view->indexed;
	state->activated = view->activated;
	state->next = view->position;
	/* This is only completed name registration. No evidence or entry result is
	 * imported, and startup subscriptions are replaced by the checkpoint owner. */
	if (view->complete) registration->status = PG_SYNTHESIS_DONE;
	return 0;
fail:
	pg_index_destroy(&names);
	return -1;
}

static void definition_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = job->inputs[0];
	const struct pg_syntax *syntax = job->inputs[1];
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return;
	struct definition_state *state = definition_state(job);
	/* Local definitions remain dormant until registration completes. Import
	 * references use the immutable incoming provider scope, never this index. */
	if (state->indexed < syntax->item_count) {
		size_t i = state->indexed;
		const struct pg_syntax_item *item = &syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) {
			if (item->operation != PG_TOKEN_ASSIGN && item->operation != PG_SYNTAX_IMPORT) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
			}
			if (pg_synthesis_register_name(synthesis, &state->names, item->name) < 0) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
			struct block_name *name = lookup_name(&state->names, item->name);
			if (!name) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (item->operation == PG_SYNTAX_IMPORT) {
				if (!name->imported) name->imported = pg_synthesis_request(synthesis, scope, item->expression);
				if (!name->imported) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (definition_input(job, i, name->imported)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			} else {
				if (name->producer) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				name->producer = request_job(synthesis, DEFINITION_JOB, job, item->expression);
				if (!name->producer) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (definition_input(job, i, name->producer)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			}
		}
		++state->indexed;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (state->activated < syntax->item_count) {
		size_t i = state->activated;
		const struct pg_syntax_item *item = &syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) {
			struct pg_synthesis_job *producer = state->entries[i];
			if (!producer) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (pg_synthesis_work_role(producer) == DEFINITION_JOB && !definition_work(producer)->active) {
				definition_work(producer)->active = 1;
				if (!producer->dependency) pg_synthesis_enqueue(synthesis, producer);
			}
		} else {
			struct pg_synthesis_input term = lookup_scope(state->scope, item->name).value;
			if (!term.checked && !term.pending) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			struct pg_synthesis_job *type = pg_synthesis_request(synthesis, state->scope, item->expression);
			if (!type) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_synthesis_job *check = pg_synthesis_source_expect(synthesis, state->scope, term, (struct pg_synthesis_input){.pending = pg_synthesis_pending(type)});
			if (!check) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (definition_input(job, i, check)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		}
		++state->activated;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
}

int pg_synthesis_definition_body(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *definition, struct pg_synthesis_job **body)
{
	if (!synthesis || !definition || !body || definition->pending.owner != synthesis->owner_key
		|| pg_synthesis_work_role(definition) != DEFINITION_JOB) return -1;
	const struct definition_work *local = definition_work(definition);
	if (!local->active) return -1;
	*body = local->body;
	return 0;
}

int pg_synthesis_definition_resume_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *definition, struct pg_synthesis_job *body)
{
	struct pg_synthesis_job *previous;
	if (!definition || definition->status != PG_SYNTHESIS_PENDING) return -1;
	if (pg_synthesis_definition_body(synthesis, definition, &previous) || previous) return -1;
	if (!body || body->pending.owner != synthesis->owner_key) return -1;
	const struct pg_source_scope *scope = definition_state(definition->inputs[0])->scope;
	/* Direct literal rules need no source wrapper. The existing factory's
	 * exact key establishes the same body identity for every expression. */
	if (pg_synthesis_request(synthesis, scope, definition->inputs[1]) != body) return -1;
	definition_work(definition)->body = body;
	return 0;
}

int pg_synthesis_module_frontier(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *module, struct pg_module_frontier *view)
{
	if (!synthesis || !module || !view || module->pending.owner != synthesis->owner_key
		|| pg_synthesis_work_role(module) != SOURCE_MODULE) return -1;
	const struct module_work *local = module_work(module);
	const struct pg_syntax *syntax = source_syntax(module);
	if (!syntax) return -1;
	int selected = syntax->kind == PG_SYNTAX_QUALIFIED;
	if (selected) syntax = syntax->left;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS || !local->right
		|| pg_synthesis_work_role(local->right) != DEFINITION_SCOPE_JOB) return -1;
	size_t position = selected ? local->left != local->right : definition_state(local->right)->next;
	struct pg_synthesis_job *previous = local->left;
	if (!selected && position && previous == local->right)
		previous = definition_state(local->right)->entries[position - 1];
	*view = (struct pg_module_frontier){position, previous};
	return 0;
}

int pg_synthesis_module_resume(struct pg_synthesis *synthesis, struct pg_synthesis_job *module,
	const struct pg_module_frontier *view)
{
	struct pg_module_frontier current;
	if (!module || module->status != PG_SYNTHESIS_PENDING) return -1;
	if (!view || pg_synthesis_module_frontier(synthesis, module, &current) || current.position) return -1;
	struct module_work *local = module_work(module);
	if (local->left != local->right) return -1;
	struct definition_state *state = definition_state(local->right);
	int selected = source_syntax(module)->kind == PG_SYNTAX_QUALIFIED;
	const struct pg_syntax *syntax = local->right->inputs[1];
	if (view->position > (selected ? 1 : syntax->item_count)) return -1;
	struct pg_synthesis_job *previous = view->previous;
	if (!previous || previous->pending.owner != synthesis->owner_key) return -1;
	if (!view->position) {
		if (previous != local->right) return -1;
	} else {
		if (local->right->status != PG_SYNTHESIS_DONE) return -1;
		if (selected) {
			if (pg_synthesis_work_role(previous) != SOURCE_MODULE || previous->inputs[0] != source_scope(module)
				|| previous->inputs[1] != source_syntax(module)->left) return -1;
			if (!lookup_definition(state, source_syntax(module)->right->token).pending) return -1;
		} else {
			if (previous != state->entries[view->position - 1]) return -1;
			/* A cursor cannot hide an unchecked or failed earlier sibling. The
			 * current entry remains a normal dependency, even if it has failed. */
			for (size_t i = 0; i + 1 < view->position; ++i)
				if (!state->entries[i] || state->entries[i]->status != PG_SYNTHESIS_DONE) return -1;
		}
	}
	local->left = previous;
	if (!selected) state->next = view->position;
	return 0;
}

static void definitions_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (source_context_wait(synthesis, job)) return;
	struct module_work *local = module_work(job);
	const struct pg_syntax *syntax = source_syntax(job);
	int selected = syntax->kind == PG_SYNTAX_QUALIFIED;
	if (selected) syntax = syntax->left;
	if (!local->right) {
		if (!pg_synthesis_prepare_module(synthesis, job)) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
		return;
	}
	if (pg_synthesis_await(synthesis, job, local->left)) return;
	struct definition_state *state = definition_state(local->right);
	/* A restored ordinal describes source progress, not checked children.
	 * Reuse the canonical prefix owners through ordinary Solve before skipping. */
	if (!selected && state->next && local->left == local->right) {
		for (size_t i = 0; i < state->next; ++i)
			if (pg_synthesis_await(synthesis, job, state->entries[i])) return;
		local->left = state->entries[state->next - 1];
	}
	if (selected) {
		if (local->left == local->right) {
			if (!lookup_definition(state, source_syntax(job)->right->token).pending) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			/* Selection cannot hide an invalid or pending unselected entry. */
			local->left = pg_synthesis_request(synthesis, source_scope(job), syntax);
			depend(synthesis, job, local->left);
			return;
		}
	} else if (state->next < syntax->item_count) {
		local->left = state->entries[state->next++];
		depend(synthesis, job, local->left);
		return;
	}
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
}


/* Resolve source selectors against the generated telescope, then use the
 * same lexical field bindings in ordinary and inductive branch synthesis. */
static enum pg_synthesis_status case_field_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_syntax *clause, size_t count,
	const struct pg_evidence *const *extensions, const struct pg_source_scope **result)
{
	int named = clause && clause->item_count && clause->items[0].operation;
	size_t hidden = 0, ordinal;
	const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
	if (!pg_data_constructor_position(layout, constructor, &ordinal)) return PG_SYNTHESIS_REJECTED;
	const struct source_constructor *calling = pg_synthesis_constructor_source(synthesis, formation, constructor);
	if (calling) hidden = calling->implicit_count;
	if (hidden > count) return PG_SYNTHESIS_ERROR;
	if (clause && !named && count - hidden != clause->item_count) return PG_SYNTHESIS_REJECTED;
	const struct pg_syntax_item **bindings = NULL;
	const struct source_case_layout *source = NULL;
	enum pg_synthesis_status status = PG_SYNTHESIS_REJECTED;
	if (named) {
		if (count > SIZE_MAX / sizeof(*bindings)) return PG_SYNTHESIS_ERROR;
		bindings = calloc(count, sizeof(*bindings));
		if (count && !bindings) return PG_SYNTHESIS_ERROR;
		const struct pg_synthesis_graph_names *names = metadata_graph_names(source_metadata(synthesis, pg_evidence_subject(formation)));
		if (!names) { status = PG_SYNTHESIS_UNSUPPORTED; goto done; }
		source = &names->cases[ordinal];
		if (source->count != count) goto done;
		for (size_t i = 0; i < clause->item_count; ++i) {
			const struct pg_syntax_item *item = &clause->items[i];
			if (item->operation != PG_TOKEN_ASSIGN || !item->expression || item->expression->kind != PG_SYNTAX_ATOM) goto done;
			struct pg_token local_name = item->expression->token, source_selector = item->name;
			if (local_name.kind != PG_TOKEN_IDENT || source_selector.kind != PG_TOKEN_IDENT) goto done;
			for (size_t j = 0; j < i; ++j)
				if (same_name(local_name, clause->items[j].expression->token)) goto done;
			size_t selected = count;
			for (size_t field = 0; field < count; ++field) {
				if (source->fields[field].name.kind != PG_TOKEN_IDENT || !same_name(source->fields[field].name, source_selector)) continue;
				if (selected != count) goto done;
				selected = field;
			}
			if (selected == count) {
				const struct pg_syntax *body = match_work(names->origin)->match->branches[ordinal].clause->right;
				const struct pg_syntax *block = pg_synthesis_block_syntax(body);
				/* A derived block result is not the result of a nested call.
				 * Its naming requires a checked expression projection, not an alias. */
				if (block) for (size_t j = 0; j < pg_synthesis_block_end(body); ++j)
					if (block->items[j].name.kind == PG_TOKEN_IDENT && same_name(block->items[j].name, source_selector))
						status = PG_SYNTHESIS_UNSUPPORTED;
				goto done;
			}
			if (bindings[selected]) goto done;
			bindings[selected] = item;
		}
	} else if (clause) for (size_t i = 0; i < count - hidden; ++i) {
		if (clause->items[i].operation) goto done;
	}
	const struct pg_source_scope *scope = parent;
	for (size_t i = 0; scope && i < count; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		const struct pg_object *value = NULL;
		size_t graph_value = source ? source->fields[i].graph_value : 0;
		if (graph_value) {
			if (graph_value > i) goto done;
			value = pg_evidence_context(extensions[graph_value - 1])->binder;
		}
		struct pg_token name = named ? bindings[i] ? bindings[i]->expression->token : (struct pg_token){0}
			: clause && i >= hidden ? clause->items[i - hidden].name : (struct pg_token){0};
		scope = pg_synthesis_bind(synthesis, scope, name, binder, (struct pg_synthesis_input){.checked = extensions[i]});
		if (scope && value) {
			struct pg_source_scope associated = *scope;
			associated.association = PG_SOURCE_GRAPH;
			associated.associated_binder = value;
			scope = pg_synthesis_intern_scope(synthesis, associated);
		}
	}
	*result = scope;
	status = scope ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR;
done:
	free(bindings);
	return status;
}

static const struct pg_evidence *constructor_pattern(struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_object *constructor,
	const struct pg_evidence *fields, size_t count, const struct pg_evidence *context);
static const struct pg_source_scope *generalized_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *original, const struct pg_source_scope *scope,
	const struct pg_evidence *generalization, const struct pg_evidence *pattern);
static const struct pg_source_scope *mapped_source_scope(struct pg_synthesis *,
	const struct pg_source_scope *, const struct pg_source_scope *, const struct pg_evidence *);

static void induction_branch_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct induction_branch_work *local = pg_synthesis_work_state(job, INDUCTION_BRANCH_JOB);
	const struct pg_source_scope *parent = job->inputs[0];
	const struct pg_syntax *clause = job->inputs[6];
	if (pg_synthesis_scope_wait(synthesis, job, parent)) return;
	if (pg_synthesis_await_input(synthesis, job, parent->context)) return;
	if (!pg_synthesis_scope_context(parent)) goto error;
	if (!local->body) {
		const struct pg_evidence *formation = job->inputs[1], *parameters = job->inputs[3];
		if (!local->fields) local->fields = pg_synthesis_induction_scope(synthesis,
			(struct pg_synthesis_input){.checked = formation}, job->inputs[2], (struct pg_synthesis_input){.checked = parameters},
			(struct pg_synthesis_input){.checked = job->inputs[4]}, (struct pg_synthesis_input){.checked = job->inputs[5]});
		if (!local->fields) goto error;
		if (pg_synthesis_await(synthesis, job, local->fields)) return;
		const struct pg_evidence *map = pg_synthesis_result(local->fields);
		size_t fields = pg_evidence_context_map(map)->count - pg_evidence_context_map(parameters)->count - 1, total;
		const struct pg_evidence *context = pg_evidence_premise(map, 1);
		if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(parameters), &total)) goto error;
		if (total > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		struct pg_graph temporary = {0};
		const struct pg_evidence **extensions = pg_alloc(&temporary, total * sizeof(*extensions));
		unsigned char *recursive = pg_alloc(&temporary, fields);
		if ((total && !extensions) || (fields && !recursive)) { pg_graph_destroy(&temporary); goto error; }
		for (size_t i = total; i; --i, context = pg_context_parent_input(synthesis->typing, context)) extensions[i - 1] = context;
		const struct pg_source_scope *scope = parent;
		enum pg_synthesis_status status = case_field_scope(synthesis, scope, formation, job->inputs[2],
			clause, fields, extensions, &scope);
		if (status != PG_SYNTHESIS_DONE) { pg_graph_destroy(&temporary); pg_synthesis_finish(synthesis, job, status); return; }
		/* Match IHs to original field binders, not their surface spelling. */
		const struct pg_object *self = pg_evidence_context(pg_evidence_premise(formation, 0))->binder;
		const struct pg_context *source_fields = pg_evidence_context(pg_evidence_premise(map, 0));
		size_t next = fields;
		for (size_t i = fields; i; --i, source_fields = source_fields->parent)
			recursive[i - 1] = pg_data_recursive_field(source_fields->declared_type, self) == 1;
		for (size_t i = 0; scope && i < fields; ++i) {
			if (!recursive[i]) continue;
			if (next == total) { scope = NULL; break; }
			scope = pg_synthesis_bind_hypothesis(synthesis, scope, pg_evidence_context(extensions[i])->binder,
				pg_evidence_context(extensions[next])->binder, (struct pg_synthesis_input){.checked = extensions[next]});
			++next;
		}
		if (next != total) scope = NULL;
		if (scope && job->inputs[7]) {
			if (pg_synthesis_await_input(synthesis, job, scope->context)) {
				pg_graph_destroy(&temporary); return;
			}
			const struct pg_evidence *pattern = constructor_pattern(synthesis, formation, parameters,
				job->inputs[4], job->inputs[2], fields ? extensions[fields - 1] : context, fields,
				pg_synthesis_scope_context(scope));
			scope = generalized_scope(synthesis, parent, scope, job->inputs[7], pattern);
		}
		pg_graph_destroy(&temporary);
		if (!scope) goto error;
		local->scope = scope;
		local->body = pg_synthesis_request(synthesis, scope, clause->right);
		if (!local->body) goto error;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (!local->abstraction) local->abstraction = pg_pending_job(pg_synthesis_abstract(synthesis,
		pg_synthesis_scope_context(parent), pg_synthesis_scope_context(local->scope),
		(struct pg_synthesis_input){.pending = pg_synthesis_pending(local->body)}).pending);
	if (!local->abstraction) goto error;
	pg_synthesis_forward(synthesis, job, local->abstraction);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static struct pg_synthesis_job *projected_image(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *image)
{
	struct pg_synthesis_input premises[] = {{.checked = context}, {.checked = image}};
	return pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
}

/* Source binders belong to the branch's typed Lambda edges, not the separate
 * recursive-erasure allocation. Ordinary Solve still checks the source body. */
static int source_match_branch_context(const struct pg_synthesis_job *job,
	size_t ordinal, size_t count, const struct pg_context **context)
{
	const struct match_work *local = match_work(job);
	const struct pg_match_allocation *allocation = local->match_allocation;
	if (!allocation || ordinal >= allocation->induction.count) return -1;
	const struct pg_context *branch = allocation->branches[ordinal];
	size_t total;
	if (pg_context_extension_size(branch, allocation->prefix, &total) || count > total) return -1;
	while (total > count) { branch = branch->parent; --total; }
	*context = branch;
	return 0;
}

static const struct pg_evidence *match_motive_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	if (!state->generic_context) {
		const struct pg_evidence *context;
		if (local->match_allocation) {
			context = pg_prove_inductive_motive_context_at(synthesis->typing,
				state->instance.formation, state->instance.parameters, local->match_allocation->motive);
		} else context = pg_prove_inductive_motive_context(synthesis->typing, state->instance.formation,
				state->instance.parameters, pg_binder(synthesis->typing->graph));
		if (!context) return NULL;
		if (!pg_inductive_motive_context_valid(synthesis->typing,
			state->instance.formation, state->instance.parameters, context)) return NULL;
		state->generic_context = context;
	}
	return state->generic_context;
}

static void match_explicit_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	const struct pg_syntax *motive = source_syntax(job)->right;
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	if (!mc) goto unsupported;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(mc), pg_evidence_context(context), &count)) goto unsupported;
	if (count != motive->item_count) goto rejected;
	const struct pg_evidence *target = state->generalization ? pg_evidence_premise(state->generalization, 1) : mc;
	if (!state->explicit_type_job) {
		const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
		if (!extensions) goto error;
		const struct pg_evidence *extension = mc;
		for (size_t i = count; i; --i, extension = pg_context_parent_input(synthesis->typing, extension))
			extensions[i - 1] = extension;
		const struct pg_source_scope *scope = local->inner;
		for (size_t i = 0; scope && i < count; ++i)
			scope = pg_synthesis_bind(synthesis, scope, state->generalization ? (struct pg_token){0} : motive->items[i].name,
				pg_evidence_context(extensions[i])->binder, (struct pg_synthesis_input){.checked = extensions[i]});
		if (scope && state->generalization) {
			scope = mapped_source_scope(synthesis, local->inner, scope, state->generalization);
			/* Explicit motive names shadow transported ambient aliases. */
			for (size_t i = 0; scope && i < count; ++i)
				scope = pg_synthesis_name(synthesis, scope, motive->items[i].name,
					(struct pg_synthesis_input){.checked = pg_prove_variable(synthesis->typing, target, pg_evidence_context(extensions[i])->binder)});
		}
		free(extensions);
		if (!scope) goto unsupported;
		state->explicit_type_job = pg_synthesis_request(synthesis, scope, motive->right);
		if (!state->explicit_type_job) goto error;
	}
	if (pg_synthesis_await(synthesis, job, state->explicit_type_job)) return;
	const struct pg_evidence *type = pg_synthesis_type_input(synthesis, job, &local->value_job, target, pg_synthesis_result(state->explicit_type_job));
	if (!type) return;
	if (pg_evidence_context(type) != pg_evidence_context(target)) goto rejected;
	const struct pg_effect_row *effects = state->motive_effects;
	if (!effects) effects = pg_effect_row(synthesis->typing->graph, 0, NULL);
	/* A motive describes the branch computation, not a Lambda parameter.
	 * Preserve Pi; only a value-type result needs the implicit RETURN layer. */
	const struct pg_evidence *candidate = type;
	if (pg_evidence_judgement(type) != PG_JUDGEMENT_COMPUTATION_TYPE) {
		type = pg_prove_value_type(synthesis->typing, type);
		if (!type) goto rejected;
		candidate = pg_prove_computation_type(synthesis->typing,
			state->motive_totality, effects, type);
	}
	if (!candidate) goto unsupported;
	const struct pg_evidence *extension = target;
	for (size_t i = state->generalized_count; candidate && i; --i, extension = pg_context_parent_input(synthesis->typing, extension))
		candidate = pg_prove_pi(synthesis->typing, extension, candidate);
	if (state->path_context) {
		state->path_result = pg_prove_projection(synthesis->typing, state->path_context, candidate);
		candidate = state->path_result;
		for (size_t i = state->path_count; candidate && i; --i)
			candidate = pg_prove_pi(synthesis->typing, state->path_extensions[i - 1], candidate);
	}
	if (!candidate) goto unsupported;
	state->motive = candidate;
	state->motive_context = mc;
	pg_synthesis_enqueue(synthesis, job);
	return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static const struct pg_evidence *constructor_pattern(struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_object *constructor,
	const struct pg_evidence *source_fields, size_t count, const struct pg_evidence *context)
{
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *instantiated = pg_prove_substitution_compose(typing, parameters,
		pg_prove_substitution_projection(typing, pg_evidence_premise(parameters, 1), context));
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **fields = malloc(count * sizeof(*fields));
	if (count && !fields) return NULL;
	const struct pg_context *field = pg_evidence_context(source_fields);
	for (size_t i = count; i; --i, field = field->parent) fields[i - 1] = pg_prove_variable(typing, context, field->binder);
	const struct pg_evidence *value = pg_prove_constructor(typing, formation, constructor, instantiated, count, fields);
	free(fields);
	return pg_prove_inductive_motive_substitution(typing,
		formation, parameters, motive_context, context, value);
}

static const struct pg_evidence *match_constructor_pattern(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t ordinal, const struct pg_evidence *context)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[ordinal];
	return constructor_pattern(synthesis, state->instance.formation, state->instance.parameters,
		match_motive_context(synthesis, job), pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal),
		branch->fields, branch->field_count, context);
}

/* Propose a motive from an independent call result or nested Match branch,
 * abstracting enclosing Lambda binders. No proof of the incomplete source is
 * published: ordinary induction must subsequently check the complete branch. */
static void match_result_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->result_checked];
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *prefix = pg_synthesis_scope_context(local->inner);
	if (!branch->needs_ih) goto skip;
	if (!branch->result) {
		branch->result = pg_alloc(typing->graph, sizeof(*branch->result));
		if (!branch->result) goto error;
		*branch->result = (struct motive_result){.scope = branch->scope, .syntax = branch->clause->right,
			.effects = pg_effect_row(typing->graph, 0, NULL), .totality = PG_TOTALITY_TOTAL};
		if (!branch->result->effects) goto error;
	}
	struct motive_result *result = branch->result;
	struct pg_synthesis_input context_input = result->scope->context;
	if (context_input.pending && pg_synthesis_pending_status(context_input.pending) == PG_SYNTHESIS_PENDING) {
		depend(synthesis, job, pg_pending_job(context_input.pending)); return;
	}
	const struct pg_evidence *context = pg_synthesis_input_result(context_input);
	if (!context) goto skip;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	const struct pg_term *content;
	const struct pg_syntax *syntax = result->syntax;
	const struct pg_evidence *type = NULL;
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		result->syntax = syntax->left;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (syntax->kind == PG_SYNTAX_LAMBDA) {
		if (!result->telescope) result->telescope = pg_synthesis_telescope(synthesis, result->scope, syntax);
		if (!result->telescope) goto error;
		if (result->telescope->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->telescope); return; }
		if (result->telescope->status != PG_SYNTHESIS_DONE) goto skip;
		struct motive_lambda *frame = pg_alloc(typing->graph, sizeof(*frame));
		if (!frame) goto error;
		*frame = (struct motive_lambda){context, pg_synthesis_result(result->telescope), result->effects, result->totality, result->lambdas};
		result->lambdas = frame;
		result->scope = pg_synthesis_telescope_scope(result->telescope);
		result->syntax = pg_synthesis_telescope_body(result->telescope);
		result->effects = pg_effect_row(typing->graph, 0, NULL);
		result->totality = PG_TOTALITY_TOTAL;
		result->telescope = NULL;
		if (!result->effects) goto error;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (pg_synthesis_block_syntax(syntax)) {
		size_t end = syntax->kind == PG_SYNTAX_QUALIFIED ? pg_synthesis_block_end(syntax) : syntax->item_count;
		if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
		if (!end) goto skip;
		const struct pg_syntax_item *item = &syntax->items[result->next];
		if (result->next + 1 == end) {
			result->syntax = item->expression;
			result->next = 0;
			pg_synthesis_enqueue(synthesis, job); return;
		}
		if (!result->input) {
			int recursive = branch_needs_ih(result->scope, pg_evidence_context(prefix), item->expression);
			if (recursive < 0) goto error;
			if (recursive) goto skip;
			struct pg_synthesis_job *input = pg_synthesis_request(synthesis, result->scope, item->expression);
			if (item->name.length && pg_synthesis_prepare_value_argument(synthesis, job, context_input, &input)) return;
			struct pg_synthesis_input body = pg_synthesis_body(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(input)}, (struct pg_synthesis_input){0});
			result->input = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, context_input, body).pending);
			if (!result->input) goto error;
		}
		if (result->input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->input); return; }
		if (result->input->status != PG_SYNTHESIS_DONE) goto skip;
		if (pg_computation_type_view(pg_evidence_classifier(result->input->result), &totality, &effects, &content)) {
			result->effects = pg_effect_union(typing->graph, result->effects, effects);
			if (totality < result->totality) result->totality = totality;
		}
		if (!result->effects) goto error;
		if (item->name.length) {
			const struct pg_object *binder = pg_binder(typing->graph);
			struct pg_synthesis_job *bound = pg_synthesis_result_context(synthesis, context_input, (struct pg_synthesis_input){.pending = pg_synthesis_pending(result->input)}, binder);
			result->scope = pg_synthesis_bind(synthesis, result->scope, item->name, binder, (struct pg_synthesis_input){.pending = pg_synthesis_pending(bound)});
			if (!result->scope) goto error;
		}
		result->input = NULL;
		++result->next;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (syntax->kind == PG_SYNTAX_ELIMINATION && !pg_synthesis_handler_syntax(syntax)) {
		if (!result->nested) result->nested = pg_synthesis_request(synthesis, result->scope, syntax);
		if (!result->nested) goto error;
		if (result->nested->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->nested); return; }
		if (result->nested->status == PG_SYNTHESIS_ERROR) goto error;
		struct match_state *nested = match_work(result->nested)->match;
		if (!nested || nested->next != nested->count || match_work(result->nested)->match_frame) goto skip;
		if (result->nested_checked == nested->count) goto skip;
		struct match_branch *part = &nested->branches[result->nested_checked];
		if (!part->body || pg_synthesis_status(part->body) != PG_SYNTHESIS_DONE) {
			++result->nested_checked;
			pg_synthesis_enqueue(synthesis, job); return;
		}
		const struct pg_evidence *fields = pg_synthesis_scope_context(part->scope);
		struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(part->body)}).pending);
		struct pg_pending *formation = pg_synthesis_classifier_in(synthesis, (struct pg_synthesis_input){.checked = fields}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
		if (!formation) goto error;
		if (pg_synthesis_wait_pending(synthesis, job, formation)) return;
		if (pg_synthesis_pending_status(formation) == PG_SYNTHESIS_ERROR) goto error;
		if (pg_synthesis_pending_status(formation) == PG_SYNTHESIS_DONE) {
			/* A nested case proposes a family at its scrutinee, not a result
			 * pinned to this constructor. The complete branches still check it. */
			const struct pg_evidence *pattern = match_constructor_pattern(synthesis,
				result->nested, result->nested_checked, fields);
			type = pg_prove_pattern_type(typing, context, pattern, pg_pending_result(formation));
			const struct pg_evidence *actual = pg_prove_inductive_motive_substitution(typing,
				nested->instance.formation, nested->instance.parameters,
				match_motive_context(synthesis, result->nested), context, match_work(result->nested)->checking_term);
			type = pg_prove_reindex(typing, actual, type);
		}
		if (!type) {
			++result->nested_checked;
			pg_synthesis_enqueue(synthesis, job); return;
		}
		goto result_type;
	}
	if (syntax->kind != PG_SYNTAX_APPLICATION || pg_synthesis_hypothesis_syntax(syntax)) goto skip;
	if (!result->callee) {
		int recursive = branch_needs_ih(result->scope, pg_evidence_context(prefix), syntax->left);
		if (recursive < 0) goto error;
		if (recursive) goto skip;
		struct pg_synthesis_job *callee = pg_synthesis_request(synthesis, result->scope, syntax->left);
		result->callee = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, context_input, (struct pg_synthesis_input){.pending = pg_synthesis_pending(callee)}).pending);
		if (!result->callee) goto error;
	}
	if (result->callee->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->callee); return; }
	if (result->callee->status != PG_SYNTHESIS_DONE) goto skip;
	type = pg_prove_classifier(typing, context, result->callee->result);
	while (type) {
		const struct pg_term *term = pg_evidence_subject(type)->core;
		if (pg_thunk_type_view(term, &content)) type = pg_prove_thunk_content(typing, type);
		else if (pg_computation_type_view(term, &totality, &effects, &content)) {
			result->effects = pg_effect_union(typing->graph, result->effects, effects);
			if (totality < result->totality) result->totality = totality;
			if (!result->effects) goto error;
			type = pg_prove_return_content(typing, type);
		} else break;
	}
	type = pg_prove_pi_constant_codomain(typing, type);
	if (!type) goto skip;
result_type:
	if (pg_computation_type_view(pg_evidence_subject(type)->core, &totality, &effects, &content)) {
		result->effects = pg_effect_union(typing->graph, result->effects, effects);
		if (totality < result->totality) result->totality = totality;
		if (!result->effects) goto error;
		type = pg_prove_computation_type(typing, result->totality,
			result->effects, pg_prove_return_content(typing, type));
	} else if (pg_effect_count(result->effects) || result->totality != PG_TOTALITY_TOTAL) goto skip;
	if (!type) goto skip;
	for (struct motive_lambda *frame = result->lambdas; frame; frame = frame->parent) {
		while (context && pg_evidence_context(context) != pg_evidence_context(frame->inner)) {
			type = pg_prove_pi_constant_codomain(typing, pg_prove_pi(typing, context, type));
			context = pg_context_parent_input(typing, context);
		}
		while (context && pg_evidence_context(context) != pg_evidence_context(frame->outer)) {
			type = pg_prove_pi(typing, context, type);
			context = pg_context_parent_input(typing, context);
		}
		if (!type || !context || pg_effect_count(frame->effects) || frame->totality != PG_TOTALITY_TOTAL) goto skip;
	}
	const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
	if (!motive_context) goto skip;
	const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, state->result_checked, context);
	const struct pg_evidence *candidate = pg_prove_pattern_type(typing, prefix, pattern, type);
	if (candidate && state->generalization) {
		const struct pg_evidence *extension = pg_evidence_premise(state->generalization, 1);
		candidate = pg_prove_projection(typing, extension, candidate);
		for (size_t i = state->generalized_count; candidate && i; --i, extension = pg_context_parent_input(typing, extension))
			candidate = pg_prove_pi(typing, extension, candidate);
	}
	if (candidate) {
		state->motive = candidate;
		state->motive_context = motive_context;
	}
skip:
	++state->result_checked;
	pg_synthesis_enqueue(synthesis, job); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}


static void match_demand_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->demanded];
	struct motive_demand *demand = branch->demands;
	if (!demand) { ++state->demanded; pg_synthesis_enqueue(synthesis, job); return; }
	if (pg_synthesis_await_input(synthesis, job, demand->scope->context)) return;
	const struct pg_evidence *context = pg_synthesis_scope_context(demand->scope);
	if (!pg_synthesis_input_owned(synthesis, demand->normalized)) {
		const struct pg_source_scope *input_scope;
		const struct pg_syntax *input_syntax;
		if (pg_synthesis_source_input(synthesis, demand->callee, &input_scope, &input_syntax)) goto error;
		int recursive = branch_needs_ih(demand->scope, pg_evidence_context(pg_synthesis_scope_context(local->inner)), input_syntax);
		if (recursive < 0) goto error;
		if (recursive) goto skip;
		if (pg_synthesis_await(synthesis, job, demand->callee)) return;
		/* An unresolved implicit prefix is not the written argument's domain.
		 * Let branch equations propose the motive; the complete constructor
		 * call then recovers indices from the checked IH result type. */
		struct pg_synthesis_job *origin = pg_synthesis_callable_source(synthesis, demand->callee);
		if (pg_synthesis_callable(synthesis, origin).source) goto skip;
		const struct pg_evidence *callee = pg_synthesis_result(demand->callee);
		if (pg_evidence_judgement(callee) == PG_JUDGEMENT_VALUE) callee = pg_prove_force(synthesis->typing, callee);
		if (!callee) goto skip;
		demand->normalized = pg_synthesis_normalize_classifier(synthesis,
			(struct pg_synthesis_input){.checked = context}, (struct pg_synthesis_input){.checked = callee});
		if (!pg_synthesis_input_owned(synthesis, demand->normalized)) goto error;
	}
	if (pg_synthesis_await_input(synthesis, job, demand->normalized)) return;
	const struct pg_term *argument_type, *result_type;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_classifier(pg_synthesis_input_result(demand->normalized)), &argument_type, &binder, &result_type)) goto skip;
	if (!demand->domain) demand->domain = pg_synthesis_application_domain(synthesis,
		(struct pg_synthesis_input){.checked = context}, demand->normalized);
	if (!demand->domain) goto error;
	if (pg_synthesis_await(synthesis, job, demand->domain)) return;
	if (!demand->solution) {
		for (size_t i = 0; i < demand->count; ++i) {
			struct pg_synthesis_job *argument = demand->arguments[i];
			const struct pg_source_scope *input_scope;
			const struct pg_syntax *input_syntax;
			if (pg_synthesis_source_input(synthesis, argument, &input_scope, &input_syntax)) goto error;
			int recursive = branch_needs_ih(demand->scope, pg_evidence_context(pg_synthesis_scope_context(local->inner)), input_syntax);
			if (recursive < 0) goto error;
			if (recursive) goto skip;
			if (argument->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, argument); return; }
			if (argument->status != PG_SYNTHESIS_DONE) goto skip;
			if (pg_evidence_judgement(pg_synthesis_result(argument)) != PG_JUDGEMENT_VALUE) goto skip;
		}
		const struct pg_evidence *domain = pg_synthesis_result(demand->domain);
		const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
		if (!motive_context) goto skip;
		const struct pg_evidence *field = pg_prove_variable(synthesis->typing, context, demand->field);
		const struct pg_evidence *pattern = pg_prove_inductive_motive_substitution(synthesis->typing,
			state->instance.formation, state->instance.parameters,
			motive_context, context, field);
		if (!pattern) goto skip;
		const struct pg_evidence *prefix = pg_synthesis_scope_context(local->inner);
		for (size_t i = 0; i < demand->count; ++i) {
			const struct pg_evidence *argument = pg_synthesis_result(demand->arguments[i]);
			const struct pg_evidence *argument_type = pg_prove_classifier(synthesis->typing, context, argument);
			const struct pg_evidence *pulled = pg_prove_pattern_type(synthesis->typing,
				prefix, pattern, pg_prove_return_type(synthesis->typing, argument_type));
			const struct pg_evidence *extension = pg_prove_context_extension(synthesis->typing,
				pg_evidence_premise(pattern, 0), pg_binder(synthesis->typing->graph), pg_prove_return_content(synthesis->typing, pulled));
			pattern = pg_prove_substitution_pair(synthesis->typing, pattern, extension, argument);
			if (!pattern) goto skip;
		}
		const struct pg_evidence *type = pg_prove_computation_type(synthesis->typing,
			state->motive_totality, state->motive_effects, domain);
		demand->solution = pg_prove_pattern_type(synthesis->typing,
			prefix, pattern, type);
		const struct pg_evidence *extension = pg_evidence_premise(pattern, 0);
		for (size_t i = demand->count; demand->solution && i; --i, extension = pg_context_parent_input(synthesis->typing, extension))
			demand->solution = pg_prove_pi(synthesis->typing, extension, demand->solution);
		if (!demand->solution) goto skip;
		if (!state->motive) {
			state->motive = demand->solution;
			state->motive_context = motive_context;
		}
	}
	struct pg_synthesis_job *comparison = pg_synthesis_compare_terms(synthesis,
		pg_evidence_subject(state->motive)->core, pg_evidence_subject(demand->solution)->core);
	if (!comparison) goto error;
	if (pg_synthesis_await(synthesis, job, comparison)) return;
skip:
	branch->demands = demand->next;
	pg_synthesis_enqueue(synthesis, job);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* An APP domain constrains the IH's value result, not its effects. Seed the
 * candidate row from independently synthesized branches. Full branch checking
 * still has to establish that the candidate covers every recursive branch. */
static void match_effects_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->effect_checked];
	if (!state->motive_effects) state->motive_effects = pg_effect_row(synthesis->typing->graph, 0, NULL);
	if (!state->motive_effects) goto error;
	if (!branch->needs_ih) {
		const struct pg_evidence *context = pg_synthesis_scope_context(branch->scope);
		struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, context, context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
		if (!body) goto error;
		if (pg_synthesis_await(synthesis, job, body)) return;
		const struct pg_effect_row *effects;
		const struct pg_term *result;
		enum pg_totality totality;
		if (pg_computation_type_view(pg_evidence_classifier(pg_synthesis_result(body)), &totality, &effects, &result)) {
			state->motive_effects = pg_effect_union(synthesis->typing->graph, state->motive_effects, effects);
			if (totality < state->motive_totality) state->motive_totality = totality;
			if (!state->motive_effects) goto error;
		}
	}
	++state->effect_checked;
	pg_synthesis_enqueue(synthesis, job);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static const struct pg_evidence *match_result_type(struct pg_synthesis *synthesis,
	const struct match_state *state, const struct pg_evidence *type)
{
	if (!type) return NULL;
	const struct pg_effect_row *effects;
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_computation_type_view(pg_evidence_subject(type)->core, &totality, &effects, &content)) return type;
	return pg_prove_computation_type(synthesis->typing,
		state->motive_totality, state->motive_effects, pg_prove_return_content(synthesis->typing, type));
}

static const struct pg_evidence *match_candidate_type(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t ordinal, const struct pg_evidence *type)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct pg_typing *typing = synthesis->typing;
	struct match_branch *branch = &state->branches[ordinal];
	const struct pg_evidence *fields = pg_synthesis_scope_context(branch->scope);
	/* Copied ambient inputs remain motive arguments; hidden paths may be
	 * removed only by checked constant-codomain formation. */
	while (type && pg_evidence_context(fields) != pg_evidence_context(branch->fields)) {
		if (!state->generalization && !state->path_context) break;
		type = pg_prove_pi(typing, fields, type);
		if (state->path_context) type = pg_prove_pi_constant_codomain(typing, type);
		fields = pg_context_parent_input(typing, fields);
	}
	const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, ordinal, fields);
	return pg_prove_pattern_type(typing, pg_synthesis_scope_context(local->inner), pattern, type);
}

static struct pg_synthesis_job *match_induction_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t ordinal, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal);
	struct pg_synthesis_input formation = {.checked = state->instance.formation};
	struct pg_synthesis_input parameters = {.checked = state->instance.parameters};
	struct pg_synthesis_input context = {.checked = motive_context};
	struct pg_synthesis_input type = {.checked = motive};
	if (!local->match_allocation)
		return pg_synthesis_induction_scope(synthesis, formation, constructor, parameters, context, type);
	const struct pg_evidence *fields = pg_data_schema_fields(state->instance.schema, constructor);
	const struct pg_object *self = pg_evidence_context(pg_evidence_premise(state->instance.formation, 0))->binder;
	size_t count = state->branches[ordinal].field_count, total = count;
	const struct pg_context *field = pg_evidence_context(fields);
	for (size_t i = 0; i < count; ++i, field = field->parent) {
		int recursive = pg_data_recursive_field(field->declared_type, self);
		if (recursive < 0 || (recursive && total == SIZE_MAX)) return NULL;
		total += recursive != 0;
	}
	const struct pg_context *prefix, *end;
	if (source_match_branch_context(job, ordinal, count, &prefix) ||
		source_match_branch_context(job, ordinal, total, &end)) return NULL;
	return pg_synthesis_induction_scope_at(synthesis, formation, constructor, parameters, context, type,
		prefix, end);
}

static struct pg_synthesis_job *match_induction_source(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t ordinal, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal);
	return pg_synthesis_induction_branch(synthesis, local->inner,
		state->instance.formation, constructor, state->instance.parameters,
		motive_context, motive, state->generalization, state->branches[ordinal].clause);
}

/* A base equation can suggest M(xs), not merely the constant M(nil).
 * Every suggested family is checked against complete induction branches;
 * unsuccessful proposals never replace the ordinary constant-motive path. */
static void match_recursive_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	if (!mc) goto done;
	if (!state->candidate) {
		if (state->candidate_next == state->count) goto done;
		struct match_branch *branch = &state->branches[state->candidate_next];
		if (branch->needs_ih || branch->contradiction) goto next;
		const struct pg_evidence *fields = pg_synthesis_scope_context(branch->scope);
		if (!branch->type_job) {
			struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
			branch->type_job = pg_synthesis_classifier_in(synthesis, (struct pg_synthesis_input){.checked = fields}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
		}
		if (!branch->type_job) goto error;
		if (pg_synthesis_wait_pending(synthesis, job, branch->type_job)) return;
		if (pg_synthesis_pending_status(branch->type_job) == PG_SYNTHESIS_ERROR) goto error;
		if (pg_synthesis_pending_status(branch->type_job) != PG_SYNTHESIS_DONE) goto next;
		const struct pg_evidence *candidate = match_candidate_type(synthesis, job,
			state->candidate_next, pg_pending_result(branch->type_job));
		if (!candidate) goto next;
		/* Indexed motives may depend on a family index without mentioning the
		 * scrutinee itself. Test the complete motive telescope, not its tail. */
		const struct pg_context *prefix = pg_evidence_context(pg_synthesis_scope_context(local->inner));
		const struct pg_context *parameter = pg_evidence_context(mc);
		for (; parameter != prefix; parameter = parameter->parent) {
			if (!parameter) goto error;
			int independent = pg_term_independent(pg_evidence_subject(candidate)->core, parameter->binder);
			if (independent < 0) goto error;
			if (!independent) break;
		}
		if (parameter == prefix) goto next;
		state->candidate = candidate;
		state->candidate_checked = 0;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (state->candidate_checked == state->count) {
		state->motive = state->candidate;
		state->motive_context = mc;
		state->checked = state->prepared = state->validated = state->count;
		for (size_t i = 0; i < state->count; ++i) {
			struct match_branch *branch = &state->branches[i];
			branch->function = pg_synthesis_result(branch->converted);
			if (branch->needs_ih) branch->body = branch->adapted;
			branch->adapted = branch->converted = NULL;
		}
		goto done;
	}
	size_t ordinal = state->candidate_checked;
	struct match_branch *branch = &state->branches[ordinal];
	struct pg_synthesis_job *scope = match_induction_scope(synthesis, job, ordinal, mc, state->candidate);
	if (!scope) goto next;
	if (scope->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, scope); return; }
	if (scope->status == PG_SYNTHESIS_ERROR) goto error;
	if (scope->status != PG_SYNTHESIS_DONE) goto next;
	if (!branch->adapted) branch->adapted = match_induction_source(synthesis, job, ordinal, mc, state->candidate);
	if (!branch->adapted) goto next;
	if (!branch->converted) {
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal);
		const struct pg_evidence *expected = pg_prove_match_branch_type(typing,
			state->instance.formation, constructor, state->instance.parameters, mc, state->candidate, scope->result);
		if (!expected) goto next;
		branch->converted = pg_synthesis_expect_inputs(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->adapted)},
			(struct pg_synthesis_input){.checked = expected});
	}
	if (!branch->converted) goto error;
	if (pg_synthesis_wait_pending(synthesis, job, pg_synthesis_pending(branch->converted))) return;
	if (pg_synthesis_status(branch->converted) == PG_SYNTHESIS_ERROR) goto error;
	if (pg_synthesis_status(branch->converted) != PG_SYNTHESIS_DONE) goto next;
	++state->candidate_checked;
	pg_synthesis_enqueue(synthesis, job); return;
next:
	state->candidate = NULL;
	for (size_t i = 0; i < state->count; ++i) state->branches[i].adapted = state->branches[i].converted = NULL;
	++state->candidate_next;
	pg_synthesis_enqueue(synthesis, job); return;
done:
	state->recursive_motive_done = 1;
	state->candidate = NULL; state->candidate_next = state->candidate_checked = 0;
	pg_synthesis_enqueue(synthesis, job); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* A branch proposes a family; it does not establish the other branch
 * equations. Check every reachable body before committing to that motive. */
static void match_candidate_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
	if (!state->candidate) {
		struct match_branch *branch = &state->branches[state->candidate_next];
		if (!branch->contradiction) {
			const struct pg_evidence *type = match_result_type(synthesis, state, pg_pending_result(branch->type_job));
			if (!branch->refined) state->candidate = match_candidate_type(synthesis, job, state->candidate_next, type);
			if (!state->candidate && state->path_context) {
				if (!branch->refined) {
					const struct pg_evidence *fields = pg_synthesis_scope_context(branch->scope);
					struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
					struct pg_synthesis_job *quoted = pg_synthesis_plain_rule_inputs(synthesis, PG_THUNK_INTRO, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
					if (!quoted) goto error;
					branch->refined = pg_synthesis_index_result(synthesis, branch->scope->context,
						(struct pg_synthesis_input){.pending = pg_synthesis_pending(quoted)}, (struct pg_synthesis_input){.checked = context});
				}
				if (!branch->refined) goto error;
				if (branch->refined->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->refined); return; }
				if (branch->refined->status == PG_SYNTHESIS_ERROR) goto error;
				if (branch->refined->status == PG_SYNTHESIS_DONE) {
					type = pg_prove_classifier(typing, pg_synthesis_scope_context(branch->scope), branch->refined->result);
					type = match_result_type(synthesis, state, pg_prove_thunk_content(typing, type));
					state->candidate = match_candidate_type(synthesis, job, state->candidate_next, type);
				}
			}
		}
		++state->candidate_next;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (state->candidate_checked == state->count) {
		state->motive_context = match_motive_context(synthesis, job);
		state->motive = state->candidate;
		if (state->path_context) {
			state->path_result = pg_prove_projection(typing, state->path_context, state->motive);
			state->motive = state->path_result;
			for (size_t i = state->path_count; state->motive && i; --i)
				state->motive = pg_prove_pi(typing, state->path_extensions[i - 1], state->motive);
		} else state->prepared = state->count;
		if (!state->motive) goto unsupported;
		for (size_t i = 0; i < state->count; ++i) state->branches[i].converted = NULL;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	struct match_branch *branch = &state->branches[state->candidate_checked];
	if (branch->contradiction) {
		++state->candidate_checked;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (!branch->converted) {
		const struct pg_evidence *body_context = pg_synthesis_scope_context(branch->scope);
		const struct pg_evidence *fields = state->generalization ? branch->fields : body_context;
		const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, state->candidate_checked, fields);
		const struct pg_evidence *target = pg_prove_reindex(typing, pattern, state->candidate);
		if (!target) goto next_candidate;
		struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, body_context, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
		if (state->path_context) {
			/* Transport U(C), then force, without running C during synthesis. */
			target = pg_prove_thunk_type(typing, target);
			body = pg_synthesis_plain_rule_inputs(synthesis, PG_THUNK_INTRO, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
			body = pg_synthesis_index_transport(synthesis, branch->scope->context,
				(struct pg_synthesis_input){.pending = pg_synthesis_pending(body)}, (struct pg_synthesis_input){.checked = target});
			body = pg_synthesis_plain_rule_inputs(synthesis, PG_FORCE_ELIM, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
		} else body = pg_synthesis_expect_inputs(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)},
			(struct pg_synthesis_input){.checked = target});
		branch->converted = pg_pending_job(pg_synthesis_abstract(synthesis, context, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)}).pending);
		if (!branch->converted) goto error;
	}
	if (pg_synthesis_wait_pending(synthesis, job, pg_synthesis_pending(branch->converted))) return;
	if (pg_synthesis_status(branch->converted) == PG_SYNTHESIS_ERROR) goto error;
	if (pg_synthesis_status(branch->converted) != PG_SYNTHESIS_DONE) goto next_candidate;
	branch->function = pg_synthesis_result(branch->converted);
	++state->candidate_checked;
	pg_synthesis_enqueue(synthesis, job); return;
next_candidate:
	state->candidate = NULL; state->candidate_checked = 0;
	for (size_t i = 0; i < state->count; ++i) state->branches[i].converted = NULL;
	pg_synthesis_enqueue(synthesis, job); return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_dependent_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
	if (state->typed < state->count) {
		struct match_branch *branch = &state->branches[state->typed];
		if (branch->contradiction) { ++state->typed; pg_synthesis_enqueue(synthesis, job); return; }
		const struct pg_evidence *fields = pg_synthesis_scope_context(branch->scope);
		if (!branch->type_job) {
			struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
			branch->type_job = pg_synthesis_classifier_in(synthesis, (struct pg_synthesis_input){.checked = fields}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)});
			if (!branch->type_job) goto error;
		}
		if (pg_synthesis_wait_pending(synthesis, job, branch->type_job)) return;
		if (pg_synthesis_pending_status(branch->type_job) != PG_SYNTHESIS_DONE) goto unsupported;
		const struct pg_effect_row *effects;
		const struct pg_term *value_type;
		enum pg_totality totality;
		if (pg_computation_type_view(pg_evidence_subject(pg_pending_result(branch->type_job))->core, &totality, &effects, &value_type)) {
			if (totality < state->motive_totality) state->motive_totality = totality;
			state->motive_effects = state->motive_effects
				? pg_effect_union(synthesis->typing->graph, state->motive_effects, effects) : effects;
			if (!state->motive_effects) goto error;
		}
		++state->typed;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	if (!mc) goto unsupported;
	if (state->candidate || state->candidate_next < state->count) { match_candidate_step(synthesis, job); return; }
	if (state->path_context) goto unsupported;
	const struct pg_evidence *projection = pg_prove_substitution_projection(synthesis->typing, context, mc);
	const struct pg_evidence *parameters = pg_prove_substitution_compose(synthesis->typing, state->instance.parameters, projection);
	const struct pg_evidence **families = malloc(state->count * sizeof(*families));
	if (!families) goto error;
	for (size_t i = 0; i < state->count; ++i) {
		struct match_branch *branch = &state->branches[i];
		const struct pg_evidence *type = pg_prove_return_content(synthesis->typing, pg_pending_result(branch->type_job));
		for (const struct pg_evidence *scope = pg_synthesis_scope_context(branch->scope);
			type && pg_evidence_context(scope) != pg_evidence_context(context); scope = pg_context_parent_input(synthesis->typing, scope))
			type = pg_prove_family_abstraction(synthesis->typing, scope, type);
		families[i] = pg_prove_projection(synthesis->typing, mc, type);
	}
	const struct pg_evidence *scrutinee = pg_prove_variable(synthesis->typing, mc, pg_evidence_context(mc)->binder);
	const struct pg_evidence *type = pg_prove_type_case(synthesis->typing,
		state->instance.formation, parameters, scrutinee, state->count, (struct pg_evidence_inputs){.owner = families});
	free(families);
	state->motive = pg_prove_computation_type(synthesis->typing,
		state->motive_totality, state->motive_effects, type);
	if (!state->motive) goto unsupported;
	state->motive_context = mc;
	pg_synthesis_enqueue(synthesis, job);
	return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_dependent_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->prepared];
	if (!branch->converted) {
		const struct pg_evidence *context = pg_synthesis_scope_context(local->inner), *fields = pg_synthesis_scope_context(branch->scope);
		struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_abstract(synthesis, fields, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
		const struct pg_evidence *result_type = match_result_type(synthesis, state, pg_pending_result(branch->type_job));
		body = pg_synthesis_expect_inputs(synthesis, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)},
			(struct pg_synthesis_input){.checked = result_type});
		struct pg_synthesis_job *function = pg_pending_job(pg_synthesis_abstract(synthesis, context, fields, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)}).pending);
		branch->converted = function;
		if (!branch->converted) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (pg_synthesis_await(synthesis, job, branch->converted)) return;
	branch->function = pg_synthesis_result(branch->converted);
	branch->converted = NULL;
	++state->prepared;
	pg_synthesis_enqueue(synthesis, job);
}

static void match_validate_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->validated];
	if (!branch->converted) {
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), state->validated);
		struct pg_synthesis_job *scope = state->induction
			? match_induction_scope(synthesis, job, state->validated, state->motive_context, state->motive)
			: pg_synthesis_constructor_scope(synthesis, (struct pg_synthesis_input){.checked = state->instance.formation},
				constructor, (struct pg_synthesis_input){.checked = state->instance.parameters});
		if (pg_synthesis_await(synthesis, job, scope)) return;
		const struct pg_evidence *expected = pg_prove_match_branch_type(synthesis->typing,
			state->instance.formation, constructor, state->instance.parameters, state->motive_context, state->motive, scope->result);
		if (!expected) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		branch->converted = pg_synthesis_expect_inputs(synthesis, (struct pg_synthesis_input){.checked = branch->function},
			(struct pg_synthesis_input){.checked = expected});
		if (!branch->converted) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (pg_synthesis_await(synthesis, job, branch->converted)) return;
	branch->function = pg_synthesis_result(branch->converted);
	++state->validated;
	pg_synthesis_enqueue(synthesis, job);
}

/* Abstract the ambient telescope after distinct variable indices/scrutinee. The
 * specialization is a checked section: copied inputs are supplied once after
 * elimination, not silently retyped at each constructor. */
static int match_generalize(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
	const struct pg_term *subject = pg_evidence_subject(local->right->result)->core;
	struct match_generalization *work = state->generalizing;
	if (work) goto advance;
	if (subject->kind != PG_REFERENCE || subject->as.reference->kind != PG_BINDER) return 0;
	/* The index substitution also contains the declaration's Self image. */
	size_t first = pg_evidence_context_map(state->instance.parameters)->count + 1;
	size_t end = state->instance.indices ? pg_evidence_context_map(state->instance.indices)->count : first;
	if (first == end && !(state->induction && state->uses_scrutinee)) {
		/* An existing IH may deliberately keep the ambient arguments fixed.
		 * Do not change its function domain just because a capture is dependent. */
		if (state->induction) return 0;
		const struct pg_context *captured = pg_evidence_context(context);
		while (captured && captured->binder != subject->as.reference) {
			int independent = pg_term_independent(captured->declared_type, subject->as.reference);
			if (independent < 0) goto error;
			if (!independent) break;
			captured = captured->parent;
		}
		if (!captured || captured->binder == subject->as.reference) return 0;
	}
	for (size_t i = first; i < end; ++i) {
		const struct pg_term *index = pg_evidence_context_map(state->instance.indices)->images[i]->core;
		if (index->kind != PG_REFERENCE || index->as.reference->kind != PG_BINDER) return 0;
		for (size_t j = first; j < i; ++j)
			if (pg_evidence_context_map(state->instance.indices)->images[j]->core == index) return 0;
	}
	/* Local definition blocks and handlers need their own scoped transport;
	 * do not leave their captured environment at the old indices. */
	for (const struct pg_source_scope *scope = local->inner; scope; scope = scope->parent) {
		if (scope->effect_owner) return 0;
		if (pg_evidence_context(pg_synthesis_scope_context(scope))) {
			if (scope->definitions || scope->module) return 0;
		}
	}
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	struct pg_inductive_instance generic;
	if (!mc || !pg_inductive_instance(typing, pg_context_declared_input(typing, mc), &generic)) return 0;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), NULL, &count)) return 0;
	if (count > (SIZE_MAX - sizeof(*work)) / sizeof(*work->extensions)) goto error;
	work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->extensions));
	if (!work) goto error;
	work->motive_context = mc; work->generic_indices = generic.indices;
	work->count = count; work->first = first; work->end = end;
	const struct pg_evidence *extension = context;
	for (size_t i = count; i; --i, extension = pg_context_parent_input(typing, extension)) work->extensions[i - 1] = extension;
	work->map = pg_prove_substitution_projection(typing, extension, mc);
	work->back = pg_prove_inductive_motive_substitution(typing,
		state->instance.formation, state->instance.parameters, mc, context, local->right->result);
	if (!work->map || !work->back) return 0;
	state->generalizing = work;
advance:
	if (work->next == work->count) {
		state->generalization = work->map;
		state->specialization = work->back;
		state->generalized_count = work->copied;
		return 0;
	}
	if (!work->pair.checked && !work->pair.pending) {
		extension = work->extensions[work->next];
		const struct pg_object *binder = pg_evidence_context(extension)->binder;
		const struct pg_evidence *image = NULL;
		for (size_t j = work->first; j < work->end; ++j) {
			const struct pg_term *index = pg_evidence_context_map(state->instance.indices)->images[j]->core;
			if (index->as.reference == binder) {
				image = pg_prove_projection(typing, work->motive_context, pg_substitution_image_at(typing, work->generic_indices, j));
				work->active = 1;
			}
		}
		if (binder == subject->as.reference) {
			image = pg_prove_variable(typing, work->motive_context, pg_evidence_context(work->motive_context)->binder);
			work->active = 1;
		}
		if (!work->active) image = pg_prove_variable(typing, work->motive_context, binder);
		work->replacing = image != NULL;
		if (image) work->pair = pg_synthesis_substitution_pair(synthesis, work->map, extension,
			pg_prove_projection(typing, pg_evidence_premise(work->map, 1), image));
		else {
			work->map = pg_prove_substitution_lift(typing, work->map, extension, pg_binder(typing->graph));
			if (!work->map) return 0;
			work->pair = pg_synthesis_substitution_pair(synthesis, work->back, pg_evidence_premise(work->map, 1),
				pg_prove_variable(typing, context, binder));
			++work->copied;
		}
		if (!work->pair.checked && !work->pair.pending) return 0;
	}
	if (work->pair.pending) {
		if (pg_synthesis_pending_status(work->pair.pending) == PG_SYNTHESIS_PENDING) { depend(synthesis, job, pg_pending_job(work->pair.pending)); return 1; }
		if (pg_synthesis_pending_status(work->pair.pending) == PG_SYNTHESIS_ERROR) goto error;
		if (pg_synthesis_pending_status(work->pair.pending) != PG_SYNTHESIS_DONE) return 0;
	}
	if (work->replacing) work->map = pg_synthesis_input_result(work->pair);
	else work->back = pg_synthesis_input_result(work->pair);
	work->pair = (struct pg_synthesis_input){0};
	++work->next;
	pg_synthesis_enqueue(synthesis, job);
	return 1;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 1;
}

static const struct pg_source_scope *generalized_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *original, const struct pg_source_scope *scope,
	const struct pg_evidence *generalization, const struct pg_evidence *pattern)
{
	struct pg_typing *typing = synthesis->typing;
	if (!pattern || !generalization || !scope) return NULL;
	const struct pg_evidence *extension = pg_evidence_premise(generalization, 1);
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(extension),
		pg_evidence_context(pg_evidence_premise(pattern, 0)), &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
	if (count && !extensions) return NULL;
	for (size_t i = count; i; --i, extension = pg_context_parent_input(typing, extension)) extensions[i - 1] = extension;
	for (size_t i = 0; pattern && i < count; ++i)
		pattern = pg_prove_substitution_lift(typing, pattern, extensions[i], pg_binder(typing->graph));
	free(extensions);
	const struct pg_evidence *map = pg_prove_substitution_compose(typing, generalization, pattern);
	return mapped_source_scope(synthesis, original, scope, map);
}

static const struct pg_source_scope *mapped_source_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *original, const struct pg_source_scope *scope,
	const struct pg_evidence *map)
{
	struct pg_typing *typing = synthesis->typing;
	if (!map || !scope) return NULL;
	const struct pg_evidence *extension = pg_evidence_premise(map, 1);
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(extension),
		pg_evidence_context(pg_synthesis_scope_context(scope)), &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
	if (count && !extensions) return NULL;
	for (size_t i = count; i; --i, extension = pg_context_parent_input(typing, extension)) extensions[i - 1] = extension;
	for (size_t i = 0; scope && i < count; ++i) {
		const struct pg_evidence *destination = extensions[i];
		const struct pg_source_scope *old = original;
		for (; old; old = old->parent) {
			if (!old->binder) continue;
			const struct pg_evidence *image = pg_substitution_image(typing, map, old->binder);
			const struct pg_term *term = image ? pg_evidence_subject(image)->core : NULL;
			if (term && term->kind == PG_REFERENCE && term->as.reference == pg_evidence_context(destination)->binder) break;
		}
		scope = pg_synthesis_bind(synthesis, scope, old ? old->name : (struct pg_token){0},
			pg_evidence_context(destination)->binder, (struct pg_synthesis_input){.checked = destination});
		if (scope && old && old->associated_binder) {
			const struct pg_evidence *associated = pg_substitution_image(typing, map, old->associated_binder);
			const struct pg_term *field = associated ? pg_evidence_subject(associated)->core : NULL;
			if (!field || field->kind != PG_REFERENCE || field->as.reference->kind != PG_BINDER) { scope = NULL; break; }
			struct pg_source_scope linked = *scope;
			linked.associated_binder = field->as.reference;
			linked.association = old->association; linked.clause = old->clause;
			scope = pg_synthesis_intern_scope(synthesis, linked);
		}
		if (!scope) break;
	}
	free(extensions);
	if (!scope) return NULL;
	/* One checked map governs aliases, copied variables and their IH/graph
	 * associations. A constructor image is a value, not a conversion axiom. */
	for (const struct pg_source_scope *old = original; old; old = old->parent) {
		if (!old->name.length) continue;
		const struct pg_evidence *image;
		struct pg_synthesis_input value;
		if (old->binder) {
			if (lookup_scope(scope, old->name).binder != old->binder) continue;
			image = pg_substitution_image(typing, map, old->binder);
			if (!image) continue;
			const struct pg_term *term = pg_evidence_subject(image)->core;
			if (term->kind == PG_REFERENCE && term->as.reference == old->binder) continue;
			value = (struct pg_synthesis_input){.checked = image};
		} else {
			if (!old->value.checked && !old->value.pending) continue;
			if (!pg_evidence_context(pg_synthesis_scope_context(old))) continue;
			if (source_name_target(lookup_scope(scope, old->name).value) != source_name_target(old->value)) continue;
			/* Context action applies to the eventual subject, not completion state. */
			struct pg_synthesis_job *producer = pg_synthesis_plain_rule_inputs(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_input[]){original->context, old->value});
			value = pg_synthesis_reindex_input(synthesis,
				(struct pg_synthesis_input){.checked = map}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(producer)});
		}
		scope = pg_synthesis_name(synthesis, scope, old->name, value);
		if (!scope) return NULL;
	}
	return scope;
}

/* A rigid index is not a unification assignment. Keep the equality between
 * the actual index and the constructor index as an explicit motive argument. */
static int match_index_context(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	if (state->path_context || state->induction || state->generalization || !state->instance.indices) return 0;
	size_t first = pg_evidence_context_map(state->instance.parameters)->count + 1;
	size_t end = pg_evidence_context_map(state->instance.indices)->count;
	int rigid = 0;
	for (size_t i = first; i < end; ++i) {
		const struct pg_term *index = pg_evidence_context_map(state->instance.indices)->images[i]->core;
		if (index->kind != PG_REFERENCE || index->as.reference->kind != PG_BINDER) rigid = 1;
	}
	if (!rigid) return 0;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	struct pg_inductive_instance generic;
	if (!mc || !pg_inductive_instance(typing, pg_context_declared_input(typing, mc), &generic)) return -1;
	const struct pg_evidence *left = pg_prove_substitution_compose(typing, state->instance.indices,
		pg_prove_substitution_projection(typing, context, mc));
	const struct pg_evidence *right = pg_prove_substitution_compose(typing, generic.indices,
		pg_prove_substitution_projection(typing, pg_evidence_premise(generic.indices, 1), mc));
	size_t count = end - first;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return -1;
	const struct pg_object **binders = malloc(count * sizeof(*binders));
	const struct pg_evidence **paths = malloc(count * sizeof(*paths));
	if (!binders || !paths) { free(binders); free(paths); return -1; }
	for (size_t i = 0; i < count; ++i) binders[i] = pg_binder(typing->graph);
	const struct pg_evidence *pc = pg_identity_substitution_context(typing, left, right, count, binders, paths);
	free(binders); free(paths);
	if (!pc) return -1;
	state->path_extensions = pg_alloc(typing->graph, count * sizeof(*state->path_extensions));
	if (!state->path_extensions) return -1;
	state->path_context = pc; state->path_count = count;
	for (size_t i = count; i; --i, pc = pg_context_parent_input(typing, pc)) state->path_extensions[i - 1] = pc;
	return 0;
}

static int match_index_scope(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, size_t ordinal)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[ordinal];
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, ordinal, branch->fields);
	for (size_t i = 0; pattern && i < state->path_count; ++i) {
		pattern = pg_prove_substitution_lift(typing, pattern, state->path_extensions[i], pg_binder(typing->graph));
		if (!pattern) return -1;
		const struct pg_evidence *context = pg_evidence_premise(pattern, 1);
		branch->scope = pg_synthesis_bind(synthesis, branch->scope, (struct pg_token){0},
			pg_evidence_context(context)->binder, (struct pg_synthesis_input){.checked = context});
		if (!branch->scope) return -1;
	}
	if (!pattern || state->path_count > SIZE_MAX / sizeof(*branch->paths)) return -1;
	branch->pattern = pattern;
	branch->paths = pg_alloc(typing->graph, state->path_count * sizeof(*branch->paths));
	if (!branch->paths) return -1;
	const struct pg_evidence *context = pg_synthesis_scope_context(branch->scope);
	struct pg_inductive_instance generic;
	if (!pg_inductive_instance(typing, pg_context_declared_input(typing, state->generic_context), &generic)) return -1;
	size_t first = pg_evidence_context_map(state->instance.parameters)->count + 1;
	const struct pg_evidence *indices = pg_prove_substitution_compose(typing, generic.indices,
		pg_prove_substitution_projection(typing, pg_evidence_premise(generic.indices, 1), state->path_context));
	indices = pg_prove_substitution_compose(typing, indices, pattern);
	if (!indices) return -1;
	for (size_t i = 0; i < state->path_count; ++i) {
		struct match_index_path *path = &branch->paths[i];
		path->left = pg_prove_projection(typing, context, pg_substitution_image_at(typing, state->instance.indices, first + i));
		path->right = pg_substitution_image_at(typing, indices, first + i);
		path->path = pg_substitution_image(typing, pattern, pg_evidence_context(state->path_extensions[i])->binder);
		if (!path->left || !path->right || !path->path) return -1;
	}
	return 0;
}

/* Head comparison selects a possible refutation only. Acceptance still needs
 * a transport derivation using the corresponding, scoped Identity witness. */
static void match_index_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->path_scoped];
	if (!branch->paths && match_index_scope(synthesis, job, state->path_scoped)) goto unsupported;
	if (!branch->contradiction && branch->path_checked < state->path_count) {
		struct match_index_path *path = &branch->paths[branch->path_checked];
		const struct pg_evidence *ends[] = {path->left, path->right};
		const struct pg_object *heads[2] = {0};
		for (size_t i = 0; i < 2; ++i) {
			if (!path->normal[i]) path->normal[i] = pg_synthesis_reduction_request(synthesis,
				branch->scope->context,
				(struct pg_synthesis_input){.checked = ends[i]}, PG_REDUCTION_WHNF, 0);
			if (!path->normal[i]) goto error;
			if (pg_synthesis_await(synthesis, job, path->normal[i])) return;
			const struct pg_term *head = pg_evidence_subject(path->normal[i]->result)->core;
			while (head->kind == PG_APPLICATION) head = head->as.application.function;
			if (head->kind == PG_REFERENCE) heads[i] = head->as.reference;
		}
		struct pg_inductive_instance instance;
		const struct pg_evidence *type = pg_prove_classifier(synthesis->typing,
			pg_synthesis_scope_context(branch->scope), path->normal[0]->result);
		if (heads[0] && heads[1] && heads[0] != heads[1] && pg_inductive_instance(synthesis->typing, type, &instance)) {
			size_t positions[2];
			const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
			if (pg_data_constructor_position(layout, heads[0], &positions[0]) &&
				pg_data_constructor_position(layout, heads[1], &positions[1])) branch->contradiction = path;
		}
		++branch->path_checked;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	++state->path_scoped;
	pg_synthesis_enqueue(synthesis, job); return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_refuted_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_work *local = match_work(job);
	struct match_state *state = local->match;
	struct match_branch *branch = &state->branches[state->prepared];
	if (!branch->contradiction) { ++state->prepared; pg_synthesis_enqueue(synthesis, job); return; }
	if (!branch->adapted) {
		struct match_index_path *path = branch->contradiction;
		/* Ex falso at U(C), then force, works for every computation carrier,
		 * including raw Pi. The unreachable source body does not choose C. */
		const struct pg_evidence *type = pg_prove_reindex(synthesis->typing, branch->pattern, state->path_result);
		type = pg_prove_thunk_type(synthesis->typing, type);
		if (!type) goto unsupported;
		branch->body = pg_synthesis_disjoint_transport(synthesis, branch->scope->context,
			(struct pg_synthesis_input){.checked = path->left}, (struct pg_synthesis_input){.checked = path->right},
			(struct pg_synthesis_input){.checked = path->path}, (struct pg_synthesis_input){.checked = path->left},
			(struct pg_synthesis_input){.checked = type});
		branch->body = pg_synthesis_plain_rule_inputs(synthesis, PG_FORCE_ELIM, NULL, 1, &(struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)});
		branch->adapted = pg_pending_job(pg_synthesis_abstract(synthesis, pg_synthesis_scope_context(local->inner),
			pg_synthesis_scope_context(branch->scope), (struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
		if (!branch->adapted) goto error;
	}
	if (pg_synthesis_await(synthesis, job, branch->adapted)) return;
	branch->function = pg_synthesis_result(branch->adapted);
	++state->prepared;
	pg_synthesis_enqueue(synthesis, job); return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (source_context_wait(synthesis, job)) return;
	struct match_work *local = match_work(job);
	if (job->result) goto complete;
	if (!local->left) {
		local->left = pg_synthesis_request(synthesis, source_scope(job), source_syntax(job)->left);
		depend(synthesis, job, local->left);
		return;
	}
	if (local->left->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->left->status); return; }
	if (!local->inner) {
		local->checking_term = pg_synthesis_result(local->left);
		if (local->checking_term && pg_evidence_judgement(local->checking_term) == PG_JUDGEMENT_COMPUTATION) {
			if (!local->match_frame) {
				struct pg_synthesis_sequence_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
				if (!frame) goto error;
				*frame = (struct pg_synthesis_sequence_frame){.input = local->left, .binds = 1,
					.context = {.pending = pg_synthesis_pending(pg_synthesis_result_context(synthesis, source_scope(job)->context,
						(struct pg_synthesis_input){.pending = pg_synthesis_pending(local->left)}, pg_synthesis_source_binder(synthesis, source_scope(job), source_syntax(job), 0, NULL)))}};
				if (!frame->context.pending) goto error;
				local->match_frame = frame;
			}
			struct pg_synthesis_job *extension = pg_pending_job(local->match_frame->context.pending);
			if (pg_synthesis_await(synthesis, job, extension)) return;
			const struct pg_object *binder = pg_evidence_context(extension->result)->binder;
			local->inner = pg_synthesis_bind(synthesis, source_scope(job), (struct pg_token){0}, binder, (struct pg_synthesis_input){.checked = extension->result});
			if (!local->inner) goto error;
			local->checking_term = pg_prove_variable(synthesis->typing, extension->result, binder);
		} else local->inner = source_scope(job);
	}
	const struct pg_evidence *context = pg_synthesis_scope_context(local->inner), *scrutinee = local->checking_term;
	if (!scrutinee || pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE) goto unsupported;
	if (!local->right) local->right = pg_pending_job(pg_synthesis_normalize_classifier(synthesis, (struct pg_synthesis_input){.checked = context}, (struct pg_synthesis_input){.checked = scrutinee}).pending);
	if (!local->right) goto error;
	if (pg_synthesis_await(synthesis, job, local->right)) return;
	scrutinee = local->right->result;
	if (!local->match || !local->match->instance.schema) {
		struct pg_pending *classifier = pg_synthesis_classifier_in(synthesis,
			(struct pg_synthesis_input){.checked = context}, (struct pg_synthesis_input){.checked = scrutinee});
		if (!classifier) goto error;
		if (pg_synthesis_await_pending(synthesis, job, classifier)) return;
		struct pg_pending *instance_job = pg_synthesis_inductive_instance(synthesis,
			(struct pg_synthesis_input){.checked = pg_pending_result(classifier)});
		if (!instance_job) goto error;
		if (pg_synthesis_await_pending(synthesis, job, instance_job)) return;
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) goto error;
		size_t count = pg_data_constructor_count(instance.schema);
		if (count < source_syntax(job)->item_count) goto rejected;
		if (!count) goto unsupported;
		if (count > SIZE_MAX / sizeof(struct match_branch)) goto error;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(instance.formation));
		const struct pg_source_scope *exports = metadata_exports(synthesis, origin);
		if (!exports) goto unsupported;
		struct match_state *state = match_state(synthesis, job);
		if (!state) goto error;
		state->branches = pg_alloc(synthesis->typing->graph, count * sizeof(*state->branches));
		if (!state->branches) goto error;
		local->match->instance = instance;
		local->match->motive_totality = PG_TOTALITY_TOTAL;
		local->match->count = count;
		local->match->labels = exports;
	}
	struct match_state *state = local->match;
	const struct pg_data_layout *layout = pg_data_schema_layout(state->instance.schema);
	if (state->selected < source_syntax(job)->item_count) {
		const struct pg_syntax *clause = source_syntax(job)->items[state->selected].expression;
		struct source_reference label = {0};
		if (clause->left->kind == PG_SYNTAX_ATOM)
			label = lookup_scope(state->labels, clause->left->token);
		else {
			struct pg_pending *dependency = NULL;
			enum pg_synthesis_status status = resolve_reference(synthesis, local->inner, clause->left, &label, &dependency);
			if (status == PG_SYNTHESIS_PENDING) { pg_synthesis_await_pending(synthesis, job, dependency); return; }
			if (status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, status); return; }
		}
		if (!label.value.pending) goto rejected;
		struct pg_constructor_input input;
		if (pg_synthesis_constructor_input(synthesis, pg_pending_job(label.value.pending), &input)) goto unsupported;
		const struct pg_object *constructor = input.constructor;
		size_t ordinal;
		if (!pg_data_constructor_position(layout, constructor, &ordinal)) goto rejected;
		if (state->branches[ordinal].clause) goto rejected;
		state->branches[ordinal].clause = clause;
		++state->selected;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	/* Scope every constructor, including omitted clauses. Only a checked
	 * contradiction can later replace an absent body with elimination. */
	if (state->next < state->count) {
		size_t ordinal = state->next;
		struct match_branch *branch = &state->branches[ordinal];
		const struct pg_syntax *clause = branch->clause;
		const struct pg_object *constructor = pg_data_constructor(layout, ordinal);
		const struct pg_evidence *schema_fields = pg_data_schema_fields(state->instance.schema, constructor);
		size_t field_count;
		if (pg_context_extension_size(pg_evidence_context(schema_fields),
			pg_evidence_context(pg_evidence_premise(state->instance.formation, 0)), &field_count)) goto error;
		if (local->match_allocation) {
			const struct pg_context *saved;
			if (source_match_branch_context(job, ordinal, field_count, &saved) ||
				!pg_synthesis_constructor_scope_at(synthesis,
				(struct pg_synthesis_input){.checked = state->instance.formation}, constructor,
				(struct pg_synthesis_input){.checked = state->instance.parameters},
				local->match_allocation->prefix, saved)) goto rejected;
		}
		struct pg_synthesis_job *scope_job = pg_synthesis_constructor_scope(synthesis,
			(struct pg_synthesis_input){.checked = state->instance.formation}, constructor,
			(struct pg_synthesis_input){.checked = state->instance.parameters});
		if (!scope_job) goto error;
		if (pg_synthesis_await(synthesis, job, scope_job)) return;
		const struct pg_evidence *map = scope_job->result;
		const struct pg_evidence *fields = pg_evidence_premise(map, 1);
		size_t count;
		if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(context), &count)) goto error;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
		if (count && !extensions) goto error;
		for (size_t i = count; i; --i, fields = pg_context_parent_input(synthesis->typing, fields)) extensions[i - 1] = fields;
		const struct pg_source_scope *scope = local->inner;
		enum pg_synthesis_status status = case_field_scope(synthesis, scope, state->instance.formation,
			constructor, clause, count, extensions, &scope);
		if (status != PG_SYNTHESIS_DONE) { free(extensions); pg_synthesis_finish(synthesis, job, status); return; }
		free(extensions);
		if (!scope) goto error;
		if (pg_synthesis_await_input(synthesis, job, scope->context)) return;
		branch->scope = scope;
		branch->fields = pg_synthesis_scope_context(scope);
		branch->field_count = count;
		const struct pg_term *subject = pg_evidence_subject(scrutinee)->core;
		const struct pg_object *binder = subject->kind == PG_REFERENCE ? subject->as.reference : NULL;
		branch->needs_ih = clause ? branch_dependencies(scope, pg_evidence_context(context), clause->right,
			binder, &state->uses_scrutinee) : 0;
		if (branch->needs_ih < 0) goto error;
		if (branch->needs_ih) {
			state->induction = 1;
		}
		++state->next;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (!state->scoped) {
		if (match_generalize(synthesis, job)) return;
		if (match_index_context(synthesis, job)) goto unsupported;
		if (state->path_scoped < state->count && state->path_context) { match_index_step(synthesis, job); return; }
		for (size_t i = 0; i < state->count; ++i) {
			struct match_branch *branch = &state->branches[i];
			if (branch->contradiction) continue;
			if (!branch->clause) goto rejected;
			if (state->generalization) branch->scope = generalized_scope(synthesis, local->inner, branch->scope,
				state->generalization, match_constructor_pattern(synthesis, job, i, pg_synthesis_scope_context(branch->scope)));
			if (!branch->scope) goto unsupported;
			if (branch->needs_ih) continue;
			branch->body = pg_synthesis_request(synthesis, branch->scope, branch->clause->right);
			if (!branch->body) goto error;
		}
		state->scoped = 1;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (state->collected < state->count) {
		struct match_branch *branch = &state->branches[state->collected];
		if (branch->needs_ih && !branch->contradiction) {
			int collecting = motive_demands(synthesis, job, branch, pg_evidence_context(context));
			if (collecting < 0) goto error;
			if (collecting) return;
			if (branch->demands) state->has_demands = 1;
		}
		++state->collected;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	if (state->has_demands && state->effect_checked < state->count) { match_effects_step(synthesis, job); return; }
	if (state->demanded < state->count) { match_demand_step(synthesis, job); return; }
	if (source_syntax(job)->right && !state->motive) { match_explicit_motive_step(synthesis, job); return; }
	/* A base case alone does not establish a constant recursive motive.
	 * First use independent result contracts from recursive branches; the
	 * ordinary branch checks must still validate every resulting equation. */
	if (!state->motive && state->result_checked < state->count) { match_result_step(synthesis, job); return; }
	if (state->induction && !state->motive && !state->recursive_motive_done) {
		match_recursive_motive_step(synthesis, job); return;
	}
	if (state->checked < state->count) {
		struct match_branch *branch = &state->branches[state->checked];
		if (branch->needs_ih || branch->contradiction) {
			++state->checked;
			pg_synthesis_enqueue(synthesis, job);
			return;
		}
		/* Retain constructor-index information before choosing a constant
		 * result. Refuted branches above cannot propose a motive. */
		if (state->path_context) state->type_cases = 1;
		const struct pg_evidence *fields = pg_synthesis_scope_context(branch->scope);
		struct pg_synthesis_job *function = pg_pending_job(pg_synthesis_abstract(synthesis, context, fields,
			(struct pg_synthesis_input){.pending = pg_synthesis_pending(branch->body)}).pending);
		if (!function) goto error;
		struct pg_pending *candidate = pg_synthesis_pending(function);
		if (!state->motive_context && !state->type_cases) {
			size_t count;
			if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(context), &count)) goto error;
			candidate = pg_synthesis_constant_result(synthesis, (struct pg_synthesis_input){.checked = context},
				(struct pg_synthesis_input){.pending = pg_synthesis_pending(function)}, count);
		}
		if (!candidate) goto error;
		if (pg_synthesis_wait_pending(synthesis, job, candidate)) return;
		enum pg_synthesis_status status = pg_synthesis_pending_status(candidate);
		/* A nonconstant classifier rejects this strategy, not the branch term. */
		if (!state->motive_context && !state->type_cases && status == PG_SYNTHESIS_REJECTED)
			status = PG_SYNTHESIS_UNSUPPORTED;
		if (status == PG_SYNTHESIS_UNSUPPORTED && !state->induction && !state->type_cases) {
			state->type_cases = 1;
			pg_synthesis_enqueue(synthesis, job);
			return;
		}
		if (status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, status); return; }
		branch->function = pg_synthesis_result(function);
		if (!state->motive_context && !state->type_cases) {
			const struct pg_evidence *type = pg_pending_result(candidate);
			if (!state->motive) state->motive = type;
			else {
				struct pg_synthesis_job *comparison = pg_synthesis_compare_terms(synthesis,
					pg_evidence_subject(state->motive)->core, pg_evidence_subject(type)->core);
				if (!comparison) goto error;
				if (comparison->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, comparison); return; }
				if (comparison->status == PG_SYNTHESIS_ERROR) goto error;
				if (comparison->status != PG_SYNTHESIS_DONE) {
					if (state->induction) goto unsupported;
					state->type_cases = 1;
				}
			}
		}
		++state->checked;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (state->type_cases && !state->motive_context) { match_dependent_motive_step(synthesis, job); return; }
	if (!state->motive && state->path_context) {
		for (size_t i = 0; i < state->count; ++i)
			if (!state->branches[i].contradiction) goto unsupported;
		/* No runnable continuation until an application equation supplies the
		 * carrier. Its publisher wakes this owner, not a result-slot Job. */
		if (!state->result_constraint) return;
		state->motive = pg_prove_projection(synthesis->typing, context, state->result_constraint);
		if (!state->motive || pg_evidence_judgement(state->motive) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
	}
	if (!state->motive) goto unsupported;
	if (!state->motive_context) {
		if (!state->motive_job) {
			const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
			if (!motive_context) goto unsupported;
			const struct pg_evidence *target = state->generalization
				? pg_evidence_premise(state->generalization, 1) : motive_context;
			if (state->path_context) target = state->path_context;
			state->motive_job = projected_image(synthesis, target, state->motive);
			if (!state->motive_job) goto error;
		}
		if (pg_synthesis_await(synthesis, job, state->motive_job)) return;
		state->motive_context = state->generic_context;
		state->motive = pg_synthesis_result(state->motive_job);
		if (state->path_context) state->path_result = state->motive;
		const struct pg_evidence *extension = state->generalization ? pg_evidence_premise(state->generalization, 1) : NULL;
		for (size_t i = state->generalized_count; i; --i, extension = pg_context_parent_input(synthesis->typing, extension))
			state->motive = pg_prove_pi(synthesis->typing, extension, state->motive);
		for (size_t i = state->path_count; state->motive && i; --i)
			state->motive = pg_prove_pi(synthesis->typing, state->path_extensions[i - 1], state->motive);
		if (!state->motive) goto unsupported;
	}
	if (state->path_context && state->prepared < state->count) { match_refuted_branch(synthesis, job); return; }
	if (state->type_cases && state->prepared < state->count) { match_dependent_branch(synthesis, job); return; }
	if (state->induction && state->prepared < state->count) {
		struct match_branch *branch = &state->branches[state->prepared];
		struct pg_synthesis_job *scope = match_induction_scope(synthesis, job,
			state->prepared, state->motive_context, state->motive);
		if (!scope) goto error;
		if (pg_synthesis_await(synthesis, job, scope)) return;
		if (branch->needs_ih) {
			if (!branch->body) branch->body = match_induction_source(synthesis, job,
				state->prepared, state->motive_context, state->motive);
			if (!branch->body) goto error;
			if (pg_synthesis_await(synthesis, job, branch->body)) return;
			branch->function = pg_synthesis_result(branch->body);
		} else {
			if (!branch->adapted) {
				const struct pg_evidence *map = scope->result, *destination = pg_evidence_premise(map, 1);
				struct pg_synthesis_job *body = projected_image(synthesis, destination, branch->function);
				for (size_t i = pg_evidence_context_map(state->instance.parameters)->count + 1; i < pg_evidence_context_map(map)->count; ++i)
					body = pg_synthesis_application(synthesis, (struct pg_synthesis_input){.checked = destination}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(projected_image(synthesis, destination, pg_substitution_image_at(synthesis->typing, map, i)))});
				branch->adapted = pg_pending_job(pg_synthesis_abstract(synthesis, context, destination, (struct pg_synthesis_input){.pending = pg_synthesis_pending(body)}).pending);
				if (!branch->adapted) goto error;
			}
			if (pg_synthesis_await(synthesis, job, branch->adapted)) return;
			branch->function = pg_synthesis_result(branch->adapted);
		}
		if (!branch->function) goto unsupported;
		++state->prepared;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (state->validated < state->count) { match_validate_branch(synthesis, job); return; }
	const struct pg_induction_allocation *allocation = source_induction_allocation(job);
	if (allocation && !state->induction) goto rejected;
	struct pg_induction_allocation generated = {0};
	const struct pg_context **clauses = NULL;
	enum pg_synthesis_status elimination_status = PG_SYNTHESIS_UNSUPPORTED;
	const struct pg_evidence_inputs branches = {.owner = state, .at = match_function_input};
	if (state->induction && !allocation) {
		clauses = malloc(state->count * sizeof(*clauses));
		if (!clauses) { elimination_status = PG_SYNTHESIS_ERROR; goto elimination_done; }
		for (size_t i = 0; i < state->count; ++i) {
			struct pg_synthesis_job *scope = match_induction_scope(synthesis, job, i, state->motive_context, state->motive);
			if (!scope) { elimination_status = PG_SYNTHESIS_ERROR; goto elimination_done; }
			if (scope->status != PG_SYNTHESIS_DONE) {
				elimination_status = scope->status;
				if (scope->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, scope);
				goto elimination_done;
			}
			clauses[i] = pg_evidence_context(scope->result);
		}
		/* Field/IH scopes were checked during branch preparation. Only the
		 * recursive erasure's three private binders still need allocation. */
		generated = (struct pg_induction_allocation){
			.recursion = pg_binder(synthesis->typing->graph), .argument = pg_binder(synthesis->typing->graph),
			.self = pg_binder(synthesis->typing->graph), .count = state->count, .clauses = clauses};
		allocation = &generated;
	}
	job->result = allocation
		? pg_prove_induction_at(synthesis->typing, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches, allocation)
		: pg_prove_match(synthesis->typing, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches);
elimination_done:
	free(clauses);
	if (!job->result) {
		if (elimination_status != PG_SYNTHESIS_PENDING) pg_synthesis_finish(synthesis, job, elimination_status);
		return;
	}
complete:
	/* Instantiate the elimination, then close its computed scrutinee. Retain
	 * one producer chain so suspension cannot skip either operation. */
	if (!local->value_job) {
		struct match_state *state = local->match;
		const struct pg_evidence *context = pg_synthesis_scope_context(local->inner);
		struct pg_synthesis_input result = {.checked = job->result};
		if (state && state->path_context) {
			size_t first = pg_evidence_context_map(state->instance.parameters)->count + 1;
			for (size_t i = 0; i < state->path_count; ++i) {
				const struct pg_evidence *value = pg_substitution_image_at(synthesis->typing, state->instance.indices, first + i);
				const struct pg_evidence *type = pg_prove_classifier(synthesis->typing, context, value);
				const struct pg_evidence *path = pg_prove_reflexivity(synthesis->typing, type, value);
				result = (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_application(synthesis,
					(struct pg_synthesis_input){.checked = context}, result,
					(struct pg_synthesis_input){.checked = path}))};
			}
		}
		if (state && state->specialization) {
			size_t end = pg_evidence_context_map(state->specialization)->count;
			for (size_t i = end - state->generalized_count; i < end; ++i)
				result = (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_application(synthesis,
					(struct pg_synthesis_input){.checked = context}, result,
					(struct pg_synthesis_input){.checked = pg_substitution_image_at(synthesis->typing, state->specialization, i)}))};
		}
		if (local->match_frame) {
			const struct pg_synthesis_sequence_frame *frame = local->match_frame;
			struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis,
				frame->context, result);
			result = (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_sequence(synthesis,
				pg_synthesis_rule_input(synthesis, pg_pending_job(frame->context.pending), 0), (struct pg_synthesis_input){.pending = pg_synthesis_pending(frame->input)}, (struct pg_synthesis_input){.pending = pg_synthesis_pending(continuation)}))};
		}
		if (!pg_synthesis_input_owned(synthesis, result)) goto error;
		local->value_job = pg_pending_job(result.pending);
	}
	if (local->value_job) {
		if (pg_synthesis_await(synthesis, job, local->value_job)) return;
		job->result = pg_synthesis_result(local->value_job);
	}
	local->match_frame = NULL;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
unsupported:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void forward_structure(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (!pg_synthesis_structure_valid(local->left)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (pg_synthesis_await_structure(synthesis, job, local->left)) return;
	local->type_structure = pg_synthesis_type_structure_result(local->left);
	pg_synthesis_finish(synthesis, job, pg_synthesis_structure_status(local->left));
}

/* Accepted typed data is the structural authority. Pending recipes are only
 * needed before that data exists, for example to close effect equations. */
static int accepted_structure(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_input producer)
{
	struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	const struct pg_evidence *proof = pg_synthesis_input_result(producer);
	if (!proof) return 0;
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	if (pg_synthesis_work_role(job) == TYPE_STRUCTURE_JOB) {
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) subject = NULL;
	}
	local->type_structure = !subject ? NULL : pg_synthesis_work_role(job) == CLASSIFIER_STRUCTURE_JOB ? subject->classifier : subject->core;
	pg_synthesis_finish(synthesis, job, local->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
	return 1;
}

/* A known term producer can be projected before acceptance; namespace-only
 * producers still need name resolution rather than a typing rule. */
static int named_term_ready(struct pg_synthesis_input input)
{
	if (input.checked) return pg_evidence_subject(input.checked) != NULL;
	const struct pg_synthesis_job *producer = pg_pending_job(input.pending);
	if (!producer) return 0;
	enum pg_synthesis_status status = pg_synthesis_pending_status(input.pending);
	if (status != PG_SYNTHESIS_PENDING)
		return status == PG_SYNTHESIS_DONE && pg_synthesis_input_result(input) != NULL;
	if (pg_synthesis_work_role(producer) == DEFINITION_JOB) return 0;
	return pg_synthesis_input_value_kind(input) >= 0;
}

/* Project the existing rule and, when requested, its preparation state.
 * A ready namespace can have no rule. This does not inspect acceptance or
 * copy a child's dependency; subscription and publication use the same view. */
static struct pg_synthesis_job *source_rule(const struct pg_synthesis_job *producer,
	int *preparing)
{
	struct pg_synthesis_projection view = pg_synthesis_work_project(producer);
	if (preparing) *preparing = view.preparing;
	return view.rule;
}

static struct pg_synthesis_projection source_reference_projection(const struct pg_synthesis_job *job)
{
	const struct reference_work *local = reference_work(job);
	const struct pg_syntax *syntax = source_syntax(job);
	struct pg_synthesis_job *rule = NULL;
	int waiting = 0;
	if (syntax->kind == PG_SYNTAX_QUALIFIED || syntax->kind == PG_SYNTAX_IMPORT) {
		rule = local->value_job;
		waiting = !rule;
	} else if (syntax->kind == PG_SYNTAX_ATOM) {
		rule = local->binder ? local->left : local->value_job;
		if (job->status == PG_SYNTHESIS_PENDING && syntax->token.kind == PG_TOKEN_IDENT
			&& !local->value_job && !local->binder) {
			struct source_reference reference = lookup_scope(source_scope(job), syntax->token);
			waiting = reference.binder ? 1 : named_term_ready(reference.value);
			if (reference.value.pending && pg_synthesis_pending_status(reference.value.pending) == PG_SYNTHESIS_PENDING) waiting = 1;
		}
	}
	return (struct pg_synthesis_projection){rule, job->status == PG_SYNTHESIS_PENDING && waiting, -1};
}

static struct pg_synthesis_projection source_module_projection(const struct pg_synthesis_job *job)
{
	const struct module_work *local = module_work(job);
	const struct pg_syntax *syntax = source_syntax(job);
	struct pg_synthesis_job *rule = NULL;
	if (syntax->kind == PG_SYNTAX_QUALIFIED && local->right && local->left != local->right)
		rule = pg_pending_job(lookup_definition(definition_state(local->right), syntax->right->token).pending);
	return (struct pg_synthesis_projection){.rule = rule, .value_kind = -1};
}

static struct pg_synthesis_projection source_expect_projection(const struct pg_synthesis_job *job)
{
	/* The operand supplies structure; the annotation remains a post-check. */
	return (struct pg_synthesis_projection){.rule = pg_pending_job(pg_synthesis_work_dependency(job, 1).pending), .value_kind = -1};
}

static struct pg_synthesis_input source_expect_output(const struct pg_synthesis_job *job)
{
	const struct source_expect_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(local->check)};
}

static struct pg_synthesis_input induction_branch_output(const struct pg_synthesis_job *job)
{
	const struct induction_branch_work *local = pg_synthesis_work_state(job, INDUCTION_BRANCH_JOB);
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(local->abstraction)};
}

int pg_synthesis_await_preparation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *producer, struct pg_synthesis_job **rule)
{
	int preparing;
	struct pg_synthesis_job *prepared = source_rule(producer, &preparing);
	if (rule) *rule = prepared;
	if (!preparing) return 0;
	pg_synthesis_subscribe(synthesis, job, producer, 1);
	return 1;
}

/* 2 is a type used as a value, 1 a value, 0 computation, -1 unknown.
 * Accepted judgements supersede recipes; pending adapters expose their rule. */
int pg_synthesis_input_kind(struct pg_synthesis_input dependency,
	const struct pg_derivation_input **construction)
{
	if (construction) *construction = NULL;
	const struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	if (!producer && !dependency.checked) return -1;
	const struct pg_synthesis_job *rule = producer;
	const struct pg_evidence *result = dependency.checked;
	while (!result && !pg_synthesis_result(rule)) {
		const struct pg_synthesis_job *prepared = source_rule(rule, NULL);
		if (prepared) { rule = prepared; continue; }
		struct pg_synthesis_input classified = pg_synthesis_classifier_input(rule);
		if (classified.checked) { result = classified.checked; break; }
		if (classified.pending) { rule = pg_pending_job(classified.pending); continue; }
		const struct pg_derivation_input *input = pg_synthesis_plain_derivation(rule);
		if (input && input->rule == PG_CONTEXT_PROJECTION && input->count == 2) {
			struct pg_synthesis_input premise = pg_synthesis_work_dependency(rule, 5);
			if (premise.checked) { result = premise.checked; break; }
			if (!premise.pending) break;
			rule = pg_pending_job(premise.pending); continue;
		}
		break;
	}
	if (construction) *construction = pg_synthesis_plain_derivation(rule);
	if (!result) result = pg_synthesis_result(rule);
	if (!result) result = rule->result ? rule->result : producer->result;
	if (result) {
		enum pg_evidence_judgement judgement = pg_evidence_judgement(result);
		if (judgement == PG_JUDGEMENT_VALUE_TYPE) return 2;
		if (judgement == PG_JUDGEMENT_VALUE) return 1;
		if (judgement == PG_JUDGEMENT_COMPUTATION) return 0;
	}
	if (!rule) return -1;
	return pg_synthesis_work_project(rule).value_kind;
}

int pg_synthesis_input_value_kind(struct pg_synthesis_input dependency)
{
	return pg_synthesis_input_kind(dependency, NULL);
}

const struct pg_object *pg_synthesis_context_binder(const struct pg_synthesis_job *context)
{
	return pg_synthesis_input_context_binder((struct pg_synthesis_input){.pending = pg_synthesis_pending((void *)context)});
}

const struct pg_object *pg_synthesis_input_context_binder(struct pg_synthesis_input input)
{
	const struct pg_synthesis_job *context = pg_pending_job(input.pending);
	const struct pg_evidence *proof = pg_synthesis_input_result(input);
	if (proof) {
		const struct pg_context *scope = pg_evidence_context(proof);
		return pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT && scope ? scope->binder : NULL;
	}
	const struct pg_object *binder = pg_synthesis_binding_binder(context);
	if (binder) return binder;
	binder = pg_synthesis_pi_scope_binder(context);
	if (binder) return binder;
	const struct pg_derivation_input *rule = pg_synthesis_plain_derivation(context);
	if (rule) {
		if (rule->rule == PG_CONTEXT_EXTEND || rule->rule == PG_CONTEXT_FAMILY_EXTEND)
			return rule->parameters.binder;
	}
	return NULL;
}

static void term_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	if (accepted_structure(synthesis, job, dependency)) return;
	const struct pg_derivation_input *input = pg_synthesis_plain_derivation(producer);
	if (!input) {
		if (!pg_synthesis_structure_valid(local->left)) {
			struct pg_synthesis_job *prepared;
			if (pg_synthesis_await_preparation(synthesis, job, producer, &prepared)) return;
			if (!prepared) goto unsupported;
			local->left = pg_synthesis_term_structure(synthesis, prepared);
		}
		forward_structure(synthesis, job);
		return;
	}
	goto unsupported;
unsupported:
	if (pg_synthesis_await_input(synthesis, job, dependency)) return;
	if (!accepted_structure(synthesis, job, dependency)) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static void classifier_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	if (accepted_structure(synthesis, job, dependency)) return;
	struct pg_synthesis_input classified = pg_synthesis_classifier_input(producer);
	if (pg_synthesis_structure_valid(local->left)) goto consume;
	struct pg_synthesis_job *prepared;
	if (pg_synthesis_await_preparation(synthesis, job, producer, &prepared)) return;
	if (classified.checked || classified.pending) {
		local->left = pg_synthesis_classifier_structure_input(synthesis, classified);
		goto consume;
	}
	struct pg_synthesis_input expected = pg_synthesis_index_transport_target(producer);
	if (!expected.checked && !expected.pending) expected = pg_synthesis_expect_target(producer);
	if (expected.checked) {
		enum pg_evidence_judgement kind = pg_evidence_judgement(expected.checked);
		const struct pg_occurrence *subject = pg_evidence_subject(expected.checked);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) subject = NULL;
		local->type_structure = subject ? subject->core : NULL;
		pg_synthesis_finish(synthesis, job, local->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (expected.pending) {
		local->left = pg_synthesis_type_structure(synthesis, pg_pending_job(expected.pending));
		goto consume;
	}
	if (prepared) {
		local->left = pg_synthesis_classifier_structure(synthesis, prepared);
		goto consume;
	}
	if (!pg_synthesis_structure_valid(local->left)) goto accepted_classifier;
consume:
	{
		if (pg_synthesis_await_structure(synthesis, job, local->left)) return;
		const struct pg_term *type = pg_synthesis_type_structure_result(local->left);
		if (classified.checked || classified.pending) {
			const struct pg_reduction_certificate *receipt = pg_synthesis_reduction_advance(synthesis, job, &local->normalizing, type, PG_REDUCTION_WHNF);
			if (!receipt) return;
			type = pg_reduction_target(receipt);
		}
		local->type_structure = type;
		pg_synthesis_finish(synthesis, job, type ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
accepted_classifier:
	if (pg_synthesis_await_input(synthesis, job, dependency)) return;
	if (!accepted_structure(synthesis, job, dependency)) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static void type_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct structure_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	struct pg_synthesis_input dependency = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_job *producer = pg_pending_job(dependency.pending);
	if (accepted_structure(synthesis, job, dependency)) return;
	if (pg_synthesis_structure_valid(local->left)) goto forward;
	if (!producer) {
		if (pg_synthesis_await_input(synthesis, job, dependency)) return;
		pg_synthesis_enqueue(synthesis, job); return;
	}
	struct pg_synthesis_job *prepared;
	if (pg_synthesis_await_preparation(synthesis, job, producer, &prepared)) return;
	if (prepared) {
		local->left = pg_synthesis_type_structure(synthesis, prepared);
		goto forward;
	}
	if (pg_synthesis_await_input(synthesis, job, dependency)) return;
	if (!accepted_structure(synthesis, job, dependency)) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
	return;
forward:
	forward_structure(synthesis, job);
}

int pg_synthesis_reference_frontier(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_reference_frontier *view)
{
	if (!synthesis || !job || !view || job->pending.owner != synthesis->owner_key) return -1;
	if (pg_synthesis_work_role(job) != SOURCE_REFERENCE) return 0;
	const struct reference_work *local = reference_work(job);
	if (source_syntax(job)->kind != PG_SYNTAX_ATOM || source_syntax(job)->token.kind != PG_TOKEN_IDENT) return 0;
	if (local->left && !local->binder && !local->value_job) return 0;
	struct pg_reference_frontier result = {.rule = local->binder ? local->left : local->value_job};
	if (!local->binder && result.rule)
		result.value = lookup_scope(source_scope(job), source_syntax(job)->token).value;
	if (job->status == PG_SYNTHESIS_DONE && !result.rule) return 0;
	*view = result;
	return 1;
}

int pg_synthesis_reference_resume(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_reference_frontier *view)
{
	struct pg_reference_frontier current;
	if (!view || pg_synthesis_reference_frontier(synthesis, job, &current) != 1) return -1;
	if (job->status != PG_SYNTHESIS_PENDING || current.value.checked || current.value.pending || current.rule) return -1;
	if (!view->rule) return view->value.checked || view->value.pending ? -1 : 0;
	if (view->rule->pending.owner != synthesis->owner_key) return -1;
	if (source_scope(job)->registration && source_scope(job)->registration->status != PG_SYNTHESIS_DONE) return -1;
	struct source_reference reference = lookup_scope(source_scope(job), source_syntax(job)->token);
	if (reference.value.checked != view->value.checked || reference.value.pending != view->value.pending) return -1;
	if (reference.value.checked || reference.value.pending) {
		if (!named_term_ready(reference.value)) return -1;
		const struct pg_evidence *proof = pg_synthesis_input_result(reference.value);
		if (proof && !pg_evidence_subject(proof)) return -1;
	} else if (!reference.binder) return -1;
	return prepare_reference_rule(synthesis, job, reference) == view->rule ? 0 : -1;
}

static int atomic_rule_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct reference_work *local = reference_work(job);
	int qualified = source_syntax(job)->kind == PG_SYNTAX_QUALIFIED;
	if (!qualified && source_syntax(job)->kind != PG_SYNTAX_ATOM) return 0;
	if (!qualified && source_syntax(job)->token.kind != PG_TOKEN_IDENT) return 0;
	if (local->value_job) {
		pg_synthesis_forward(synthesis, job, local->value_job); return 1;
	}
	if (!local->binder) {
		if (local->left) return 0;
		struct source_reference reference;
		struct pg_pending *dependency = NULL;
		enum pg_synthesis_status status = resolve_source_reference(synthesis, job, &reference, &dependency);
		if (status == PG_SYNTHESIS_PENDING) { pg_synthesis_await_pending(synthesis, job, dependency); return 1; }
		if (status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, status); return 1; }
		if (named_term_ready(reference.value)) {
			const struct pg_evidence *proof = pg_synthesis_input_result(reference.value);
			if (proof && !pg_evidence_subject(proof)) {
				pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return 1;
			}
			pg_synthesis_forward(synthesis, job, prepare_reference_rule(synthesis, job, reference));
			return 1;
		}
		/* A pending named producer, not the enclosing context, determines when
		 * its polarity becomes available. Subscribe before waiting on the row. */
		if (reference.value.pending && pg_synthesis_pending_status(reference.value.pending) == PG_SYNTHESIS_PENDING) {
			depend(synthesis, job, pg_pending_job(reference.value.pending));
			return 1;
		}
		if (reference.value.pending) {
			pg_synthesis_finish(synthesis, job, pg_synthesis_pending_status(reference.value.pending) == PG_SYNTHESIS_DONE
				? PG_SYNTHESIS_UNSUPPORTED : pg_synthesis_pending_status(reference.value.pending));
			return 1;
		}
		if (!reference.binder) {
			if (reference.exports || reference.module) return 0;
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
			return 1;
		}
		prepare_reference_rule(synthesis, job, reference);
	}
	pg_synthesis_forward(synthesis, job, local->left);
	return 1;
}

static const struct pg_syntax *sequence_binding_source(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope)
{
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->source_references, (uintptr_t)scope->binder);
		entry; entry = entry->next) {
		const struct source_reference_entry *reference = (const void *)entry;
		if (reference->key != scope->binder || reference->kind != SOURCE_BINDING) continue;
		const struct pg_source_binding *binding = reference->input;
		if (!binding->syntax || binding->syntax->kind != PG_SYNTAX_BLOCK) continue;
		const struct pg_syntax_item *item = &binding->syntax->items[binding->slot];
		if (!same_name(scope->name, item->name)) continue;
		struct pg_source_binding_cursor cursor = {.source = scope->parent};
		struct pg_source_binding_cursor saved = pg_synthesis_source_binding_scope(binding);
		const struct pg_object *binder, *prior;
		size_t i = 0;
		for (; i < binding->scope_count; ++i)
			if (!pg_synthesis_source_binding_next(&cursor, &binder)
				|| !pg_synthesis_source_binding_next(&saved, &prior) || prior != binder) break;
		if (i == binding->scope_count && !pg_synthesis_source_binding_next(&cursor, &binder)) return item->expression;
	}
	return NULL;
}

static struct pg_synthesis_job *constructor_callable_origin(struct pg_synthesis *synthesis, struct pg_synthesis_job *producer)
{
	if (!producer) return NULL;
	struct pg_synthesis_input classified = pg_synthesis_classifier_input(producer);
	if (classified.checked || classified.pending) return pg_pending_job(classified.pending);
	struct pg_synthesis_job *body = pg_pending_job(pg_synthesis_body_input(producer).pending);
	if (body) return body;
	if (pg_synthesis_work_role(producer) == BINDING_EXPECT_JOB) return pg_pending_job(pg_synthesis_work_dependency(producer, 1).pending);
	const struct pg_derivation_input *rule = pg_synthesis_plain_derivation(producer);
	if (rule) {
		switch (rule->rule) {
		case PG_THUNK_INTRO: case PG_FORCE_ELIM: case PG_RETURN_INTRO:
			return pg_pending_job(pg_synthesis_rule_input(synthesis, producer, 0).pending);
		case PG_CONTEXT_PROJECTION: return pg_pending_job(pg_synthesis_rule_input(synthesis, producer, 1).pending);
		default: return NULL;
		}
	}
	if (reference_work(producer) && reference_work(producer)->binder && source_syntax(producer)->kind == PG_SYNTAX_ATOM) {
		const struct pg_source_scope *scope = source_scope(producer);
		while (scope && scope->binder != reference_work(producer)->binder) scope = scope->parent;
		if (!scope) return NULL;
		/* Calling convention is lexical input, not a classifier query result.
		 * Lambda/pattern parameters retain their explicit Pi contract. */
		const struct pg_syntax *source = sequence_binding_source(synthesis, scope);
		return source ? pg_synthesis_request(synthesis, scope->parent, source) : NULL;
	}
	return pg_synthesis_source_origin(producer);
}

struct pg_synthesis_job *pg_synthesis_callable_source(struct pg_synthesis *synthesis, struct pg_synthesis_job *producer)
{
	struct pg_synthesis_job *slow = producer, *fast = producer;
	while (slow) {
		if (pg_synthesis_callable(synthesis, slow).source) return slow;
		struct pg_synthesis_input fields = pg_synthesis_constructor_fields(slow);
		if (fields.checked || fields.pending) return slow;
		slow = constructor_callable_origin(synthesis, slow);
		fast = constructor_callable_origin(synthesis, constructor_callable_origin(synthesis, fast));
		if (slow && slow == fast) return NULL;
	}
	return NULL;
}

static int source_context_wait(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_source_scope *scope = source_scope(job);
	if (pg_synthesis_scope_wait(synthesis, job, scope)) return 1;
	if (pg_synthesis_await_input(synthesis, job, scope->context)) return 1;
	if (!pg_synthesis_scope_context(scope)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
		return 1;
	}
	return 0;
}

static void source_unsupported_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (source_context_wait(synthesis, job)) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name)
{
	const struct module_work *local = module_work(root);
	if (local && local->right && pg_synthesis_work_role(local->right) == DEFINITION_SCOPE_JOB) root = local->right;
	const struct definition_state *state = definition_state(root);
	if (!state || !name.length || !name.text) return NULL;
	struct block_name *entry = lookup_name(&state->names, name);
	return entry ? entry->producer : NULL;
}

int pg_synthesis_definition_entry(const struct pg_synthesis_job *root, size_t index,
	const struct pg_syntax_item **item, struct pg_synthesis_job **producer)
{
	if (!root || !item || !producer) return -1;
	if (pg_synthesis_work_role(root) != SOURCE_MODULE && pg_synthesis_work_role(root) != DEFINITION_SCOPE_JOB) return -1;
	const struct pg_syntax *syntax = root->inputs[1];
	if (syntax && syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return -1;
	if (index >= syntax->item_count) return 0;
	const struct pg_synthesis_job *registration = pg_synthesis_work_role(root) == DEFINITION_SCOPE_JOB ? root : module_work(root)->right;
	const struct definition_state *state = registration && pg_synthesis_work_role(registration) == DEFINITION_SCOPE_JOB
		? definition_state(registration) : NULL;
	*item = &syntax->items[index];
	*producer = state ? state->entries[index] : NULL;
	return 1;
}
