#include "synthesis.h"
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
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *match, *graph;
	const struct source_constructor *constructors;
	int packet;
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

struct pg_source_scope {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder;
	const struct pg_object *associated_binder;
	enum pg_source_association association;
	struct pg_synthesis_job *context_job;
	struct definition_state *definitions;
	struct pg_synthesis_job *registration;
	struct pg_synthesis_job *producer;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	const struct pg_source_scope *imports;
	struct handler_state *effect_owner;
	struct pg_synthesis_job *clause;
};
struct waiter {
	struct pg_synthesis_job *parent;
	struct pg_synthesis_job *child;
	struct waiter *next;
	int preparation;
};
struct block_name {
	struct pg_index_entry index;
	struct pg_token name;
	struct pg_synthesis_job *producer;
	struct pg_synthesis_job *imported;
};
struct definition_state {
	struct pg_index names;
	struct pg_synthesis_job *registration;
	const struct pg_syntax *syntax;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job **entries;
	size_t count, indexed, activated, next;
};
struct block_frame {
	struct pg_synthesis_job *input, *context;
	const struct block_frame *parent;
	int binds;
};
struct block_state {
	const struct pg_syntax *syntax;
	size_t next, end;
	const struct pg_source_scope *scope;
	const struct pg_source_scope **scopes;
	const struct block_frame *frames;
	struct pg_synthesis_job *tail;
	struct pg_index names;
};
struct application_state {
	struct pg_synthesis_job *context, *callee, *argument, *tail;
	struct pg_synthesis_job *constraint;
	struct constructor_application *constructor;
	const struct block_frame *frames;
	size_t binding_count;
};
struct index_progress {
	const struct pg_context *field;
	struct pg_comparison comparison;
	size_t counts[2];
	/* 0 starts a candidate; 1/2/3 inspect target/old/new support. */
	unsigned next;
};
struct transport_scope {
	const struct pg_evidence *map, *left, *right, *extended;
	struct pg_typed_query *query;
	struct pg_context_lift *lift;
	struct pg_occurrence_action *action;
	struct pg_synthesis_job **branches;
	size_t count, next, scoped;
	const struct pg_evidence *extensions[];
};
struct index_scope {
	const struct pg_evidence *prefix, *map, *domain;
	const struct pg_evidence **values;
	struct pg_typed_query *query;
	struct transport_scope *boundary;
	size_t count, next, value_count;
	const struct pg_evidence *fields[];
};
struct index_transport_state {
	const struct pg_evidence *cursor, *path, *endpoints[2];
	struct pg_synthesis_job *normal[2], *candidates[2], *checks[2];
	struct index_scope *scopes[2];
	size_t candidate_next, building;
	int direct_checked, normalized_checked, constructor_checked;
	struct index_progress progress;
};
struct substitution_entry {
	const struct pg_evidence *extension;
	struct pg_synthesis_job *image;
};
struct substitution_state {
	const struct pg_evidence *map;
	size_t count, next;
	struct substitution_entry entries[];
};
struct constructor_index_path {
	const struct pg_object *binder;
	size_t field, index;
};
struct source_constructor {
	const struct pg_syntax *telescope;
	struct pg_synthesis_job *producer;
	size_t implicit_count;
	const struct constructor_index_path *indices;
};
/* Source calling convention only. The callable itself is an ordinary checked
 * Lambda/Pi term, with its remaining implicit fields still quantified. */
struct constructor_callable {
	const struct source_constructor *source;
	size_t field, count;
	size_t indices[];
};
struct constructor_application {
	const struct constructor_callable *input;
	struct constructor_callable *output;
	struct pg_synthesis_job *context, *function, *instance;
	struct pg_synthesis_job **scopes;
	size_t next, count;
	size_t arity, supplied;
	struct pg_synthesis_job **arguments;
};
struct declaration_state {
	struct pg_index names;
	const struct pg_syntax *constructors;
	struct source_constructor *members;
	size_t indexed, checked;
};
struct motive_demand {
	struct motive_demand *next;
	const struct pg_source_scope *scope;
	const struct pg_object *field;
	struct pg_synthesis_job *callee, *normalized, *domain;
	const struct pg_evidence *solution;
	size_t count;
	struct pg_synthesis_job *arguments[];
};
struct motive_scan {
	const struct pg_syntax *syntax;
	const struct pg_source_scope *scope;
	struct motive_scan *next;
	size_t item;
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
	struct pg_synthesis_job *type_job, *converted;
	const struct pg_evidence *function;
	struct motive_demand *demands;
	struct motive_scan *scan;
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
struct match_generalization {
	const struct pg_evidence *motive_context, *generic_indices, *map, *back;
	struct pg_synthesis_job *pair;
	size_t count, next, copied, first, end;
	int active, replacing;
	const struct pg_evidence *extensions[];
};
struct match_state {
	struct pg_inductive_instance instance;
	const struct pg_source_scope *labels;
	const struct pg_evidence *motive;
	const struct pg_evidence *motive_context, *generic_context;
	struct pg_synthesis_job *motive_job;
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
	int induction, uses_scrutinee, has_demands, type_cases, packet, recursive_motive_done;
	struct match_branch branches[];
};
struct family_state {
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations;
	const struct pg_evidence **paths;
	size_t count, common, next;
};
struct source_handler_clause {
	const struct pg_synthesis_job *operation;
	struct pg_synthesis_job *body;
};
struct handler_state {
	struct pg_synthesis_job *source;
	size_t scanned, count;
	struct pg_effect_inference effects;
	struct pg_effect_equation *equation;
	struct pg_synthesis_job *carrier, *collection;
	const struct pg_effect_row *handled;
	const struct pg_object **labels;
	size_t collected;
	struct handler_state *effect_owner;
	const struct pg_source_scope *scope;
	/* Only the owner counts handlers whose equation edges are incomplete. */
	size_t registering;
	int registered;
	enum pg_synthesis_status failure;
	struct source_handler_clause clauses[];
};
struct derivation_state {
	size_t next;
	const struct pg_evidence **premises;
	struct pg_comparison endpoint;
	const struct pg_reduction_certificate *reduction;
};
struct derivation_input_state {
	size_t next;
	struct pg_synthesis_job *premises[];
};
struct fold_structure_state {
	size_t next;
	struct pg_operation_clause clauses[];
};
struct effect_substitution_state {
	size_t next;
	struct pg_binding_value *bindings;
	struct pg_substitution *work;
	const struct pg_term *result;
};
enum job_role { LIFT_JOB, FUNCTION_WITNESS_JOB, PI_SCOPE_JOB, DERIVATION_INPUT_JOB, INDUCTIVE_INSTANCE_JOB, INDUCTION_SCOPE_JOB, CONSTRUCTOR_SCOPE_JOB, ROW_CONTRIBUTION_JOB, SEQUENCE_JOB, EFFECT_CONTRIBUTION_JOB, BODY_JOB, CLASSIFIER_FORMATION_JOB, DOMAIN_JOB, TERM_STRUCTURE_JOB, DECLARED_TYPE_JOB, CLASSIFIER_STRUCTURE_JOB, TYPE_STRUCTURE_JOB, EXPRESSION_JOB, DEFINITION_JOB, DEFINITION_SCOPE_JOB, EVIDENCE_JOB, RETURN_JOB, THUNK_JOB, NORMALIZATION_JOB, NF_JOB,
	RESULT_TYPE_JOB, CLASSIFIER_CONSTRAINT_JOB, BINDING_EXPECT_JOB,
	REFLEXIVITY_JOB, CLASSIFIER_JOB, FAMILY_ACTION_JOB, FORMATION_JOB, FACE_JOB, EXPECT_JOB, SOURCE_EXPECT_JOB, INSTANCE_JOB, CONVERSION_JOB, DATA_CASE_JOB, REINDEX_JOB, PAIR_JOB, SUBSTITUTION_JOB, BINDING_JOB, TELESCOPE_JOB, TELESCOPE_STRUCTURE_JOB, DATA_RESULT_JOB, DATA_SCHEMA_JOB, CONSTRUCTOR_JOB, CONSTRUCTOR_VALUE_JOB, INDUCTION_BRANCH_JOB, CONSTANT_MOTIVE_JOB, DERIVATION_JOB, OPERATION_JOB, OPERATION_REFERENCE_JOB, EFFECT_INFERENCE_JOB, HANDLER_RETURN_JOB, HANDLER_CLAUSE_JOB, HANDLER_JOB, SCOPE_CONTEXT_JOB, EFFECT_SUBSTITUTION_JOB, FAMILY_FUNCTION_JOB, FAMILY_CONTRACT_JOB, FUNCTION_GRAPH_JOB, CONSTRUCTOR_TRANSPORT_JOB, INDEX_TRANSPORT_JOB, INDEX_RESULT_JOB };
enum { APPLICATION_RULE_READY = 6 };
struct context_allocation {
	const struct pg_context *prefix, *end;
	size_t count, next;
	const struct pg_context *contexts[];
};
struct pg_synthesis_job {
	struct pg_index_entry index;
	const void *owner;
	enum job_role role;
	size_t input_count;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	const struct pg_syntax *tail;
	enum pg_synthesis_status status;
	unsigned stage;
	struct pg_synthesis_job *next;
	struct waiter *waiters;
	struct waiter *dependency;
	struct pg_synthesis_job *left;
	struct pg_synthesis_job *right;
	const struct pg_source_scope *inner;
	const struct pg_evidence *domain;
	const struct pg_object *binder;
	const struct pg_evidence *result;
	const struct pg_evidence *checking_term;
	const struct pg_evidence *checking_type;
	struct pg_synthesis_job *value_job;
	const struct pg_term *type_structure;
	struct pg_substitution *structural_substitution;
	const struct pg_evidence *function;
	const struct pg_conversion_certificate *certificate;
	const struct pg_inductive_instance *inductive_instance;
	/* The immutable role selects private work, not the accepted result. */
	union {
		struct pg_conversion comparison;
		struct pg_occurrence_action *reindex;
		struct pg_function_graph_work function_graph;
		struct pg_identity_face_work *face;
		struct pg_identity_formation_work *formation;
		struct {
			union { struct pg_whnf_job *whnf; struct pg_nf_job *nf; } normalizing;
			/* Derivation checking may normalize while retaining its premises. */
			struct derivation_state *derivation;
		};
		struct index_transport_state *index_transport;
		struct transport_scope *transport_scope;
		struct substitution_state *substitution;
		struct family_state *family;
		struct derivation_input_state *derivation_input;
		struct fold_structure_state *fold_structure;
		struct effect_substitution_state *effect_substitution;
		struct {
			struct pg_function_source_cursor *function_source;
			struct block_state *block;
			struct application_state *application;
			struct match_state *match;
		};
		struct definition_state *definitions;
		struct declaration_state *declaration;
		struct handler_state *handler;
	};
	const struct block_frame *match_frame;
	const struct pg_data_schema *schema;
	const struct pg_operation_declaration *operation;
	const struct pg_data_declaration *nominal_input;
	const struct constructor_callable *callable;
	const struct pg_match_allocation *match_allocation;
	struct source_case_layout *case_layouts;
	struct context_allocation *context_allocation;
	struct pg_constructor_allocation *member_allocations;
	const struct pg_source_scope *exports;
	struct pg_handler_clause_input *handler_clause;
	struct pg_synthesis_job *handler_owner;
	const void *inputs[];
};

static void enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	job->next = NULL;
	if (synthesis->ready_tail) synthesis->ready_tail->next = job;
	else synthesis->ready = job;
	synthesis->ready_tail = job;
}

static const struct pg_evidence *source_context(const struct pg_source_scope *scope)
{
	if (!scope || scope->context_job->status != PG_SYNTHESIS_DONE) return NULL;
	const struct pg_evidence *proof = scope->context_job->result;
	return proof && pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT ? proof : NULL;
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
	if (pg_index_init(&synthesis->scopes) == 0 && pg_index_init(&synthesis->source_metadata) == 0
		&& pg_index_init(&synthesis->source_bindings) == 0 && pg_index_init(&synthesis->source_references) == 0) {
		synthesis->owner_key = pg_alloc(typing->graph, 1);
		if (synthesis->owner_key) return 0;
	}
	pg_synthesis_destroy(synthesis);
	return -1;
}

void pg_synthesis_destroy(struct pg_synthesis *synthesis)
{
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
			switch (job->role) {
			case CONVERSION_JOB: pg_conversion_destroy(&job->comparison); break;
			case FUNCTION_GRAPH_JOB: pg_function_graph_destroy(&job->function_graph); break;
			case FACE_JOB: pg_identity_face_destroy(job->face); break;
			case FORMATION_JOB: pg_identity_formation_destroy(job->formation); break;
			case INDEX_TRANSPORT_JOB: case INDEX_RESULT_JOB:
				if (job->index_transport) pg_comparison_destroy(&job->index_transport->progress.comparison);
				break;
			case EFFECT_SUBSTITUTION_JOB:
				if (job->effect_substitution) free(job->effect_substitution->bindings);
				break;
			case DERIVATION_JOB:
				if (job->derivation) pg_comparison_destroy(&job->derivation->endpoint);
				break;
			case EXPRESSION_JOB:
				if (job->block) pg_index_destroy(&job->block->names);
				break;
			case HANDLER_JOB:
				if (job->handler && job->handler->effect_owner == job->handler)
					pg_effect_inference_destroy(&job->handler->effects);
				break;
			case DEFINITION_SCOPE_JOB:
				if (job->definitions) pg_index_destroy(&job->definitions->names);
				break;
			case DATA_SCHEMA_JOB:
				if (job->declaration) pg_index_destroy(&job->declaration->names);
				break;
			default: break; /* Other work is borrowed from its graph owner. */
			}
		}
	pg_index_destroy(&synthesis->jobs);
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

static uint64_t name_hash(struct pg_token name);
static int same_name(struct pg_token left, struct pg_token right);
static int handler_syntax(const struct pg_syntax *syntax);
static int return_clause(const struct pg_syntax *clause);
static struct pg_synthesis_job *plain_rule(struct pg_synthesis *synthesis,
	enum pg_evidence_rule rule, const struct pg_object *binder, size_t count,
	struct pg_synthesis_job *const *premises);

static const void *source_name_target(const struct pg_synthesis_job *producer)
{
	/* Evidence inputs are immutable, already checked references. A pending
	 * producer remains its own obligation even if another producer succeeds. */
	if (producer && producer->role == EVIDENCE_JOB) {
		const struct pg_occurrence *subject = pg_evidence_subject(producer->inputs[0]);
		if (subject) return subject;
	}
	return producer;
}

static const struct pg_source_scope *intern_scope(struct pg_synthesis *synthesis, struct pg_source_scope input)
{
	if (!input.context_job || input.context_job->owner != synthesis->owner_key) return NULL;
	if (!input.effect_owner && input.parent) input.effect_owner = input.parent->effect_owner;
	/* A derived dependency, not a second registration state or an intern key. */
	input.registration = input.definitions ? input.definitions->registration
		: input.parent ? input.parent->registration : NULL;
	/* Special roots carry their spelling in kind, not borrowed text. */
	if (input.name.kind == '#' || input.name.kind == '*')
		input.name = (struct pg_token){.kind = input.name.kind};
	if (input.name.length && !input.name.text) return NULL;
	uint64_t hash = name_hash(input.name) ^ (unsigned)input.name.kind;
	hash = (hash ^ input.association) * UINT64_C(1099511628211);
	const void *pointers[] = {input.parent, input.context_job, input.binder, input.associated_binder, input.definitions,
		source_name_target(input.producer), input.exports, input.module, input.imports, input.effect_owner, input.clause};
	for (size_t i = 0; i < sizeof(pointers) / sizeof(*pointers); ++i)
		hash = (hash ^ (uintptr_t)pointers[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->scopes, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		const struct pg_source_scope *scope = (const struct pg_source_scope *)entry;
		if (scope->parent != input.parent) continue;
		if (scope->context_job != input.context_job) continue;
		if (scope->binder != input.binder) continue;
		if (scope->associated_binder != input.associated_binder) continue;
		if (scope->association != input.association) continue;
		if (scope->definitions != input.definitions) continue;
		if (source_name_target(scope->producer) != source_name_target(input.producer)) continue;
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
	return intern_scope(synthesis, (struct pg_source_scope){.context_job =
		pg_synthesis_evidence(synthesis, pg_prove_empty_context(synthesis->typing))});
}

int pg_synthesis_source_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax)
{
	if (!synthesis || !job || !scope || !syntax) return -1;
	if (job->owner != synthesis->owner_key || job->role != EXPRESSION_JOB) return -1;
	*scope = job->scope; *syntax = job->syntax;
	return 0;
}

static const struct pg_induction_allocation *source_induction_allocation(const struct pg_synthesis_job *job)
{
	if (job->match_allocation) return &job->match_allocation->induction;
	return pg_evidence_induction_allocation(pg_synthesis_result(job));
}

const struct pg_match_allocation *pg_synthesis_match_allocation(
	const struct pg_synthesis_job *job, struct pg_graph *storage)
{
	if (!job) return NULL;
	if (job->match_allocation) return job->match_allocation;
	if (!storage) return NULL;
	const struct pg_induction_allocation *induction = source_induction_allocation(job);
	if (!induction || induction->count > (SIZE_MAX - sizeof(struct pg_match_allocation)) / sizeof(void *)) return NULL;
	const struct pg_occurrence *subject = pg_evidence_subject(pg_synthesis_result(job));
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
	if (job->owner != synthesis->owner_key || job->role != DEFINITION_JOB) return -1;
	const struct pg_synthesis_job *registration = job->inputs[0];
	*scope = registration->scope; *definitions = registration->syntax;
	*expression = job->inputs[1];
	return 0;
}

const struct pg_source_scope *pg_synthesis_prepared_environment(const struct pg_synthesis_job *job)
{
	if (!job) return NULL;
	if (job->role == DEFINITION_JOB) job = job->inputs[0];
	else if (job->role == EXPRESSION_JOB) {
		const struct pg_synthesis_job *definitions = job->right;
		job = definitions && definitions->role == DEFINITION_SCOPE_JOB ? definitions : job->value_job;
	}
	if (!job) return NULL;
	if (job->role == DEFINITION_SCOPE_JOB && job->definitions) return job->definitions->scope;
	if (job->role == HANDLER_JOB && job->handler) return job->handler->scope;
	return NULL;
}

int pg_synthesis_environment_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_source_environment *input)
{
	if (!synthesis || !scope || !input || scope->owner != synthesis->owner_key) return -1;
	if (scope->effect_owner && scope->effect_owner->scope == scope) {
		*input = (struct pg_source_environment){.parent = scope->parent,
			.handler = scope->effect_owner->source->syntax};
		return 0;
	}
	if (scope->binder) {
		struct pg_synthesis_job *binding = scope->context_job;
		if (binding->role == BINDING_JOB) {
			if (binding->inner != scope || scope->associated_binder) return -1;
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
			.context = binding->role == SCOPE_CONTEXT_JOB ? (void *)binding->inputs[2] : binding,
			.binder = scope->binder, .associated = field, .association = scope->association};
		return 0;
	}
	const struct pg_evidence *context = source_context(scope);
	if (scope->parent) {
		if (scope->context_job != scope->parent->context_job) return -1;
	} else if (!context || pg_evidence_context(context)) return -1;
	*input = (struct pg_source_environment){.parent = scope->parent, .exports = scope->exports,
		.imports = scope->imports, .name = scope->name, .producer = scope->producer, .module = scope->module,
		.definitions = scope->definitions ? scope->definitions->syntax : NULL};
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

static int binding_context(struct pg_synthesis *synthesis, const struct pg_source_scope *parent,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!parent || parent->owner != synthesis->owner_key ||
		!pg_evidence_owned_by(extended_context, synthesis->typing)) return 0;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return 0;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!source_context(parent)) return 0;
	if (!context || context->parent != pg_evidence_context(source_context(parent))) return 0;
	return context->binder == binder;
}

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!binding_context(synthesis, parent, binder, extended_context)) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context_job = pg_synthesis_evidence(synthesis, extended_context)});
}

enum { RULE_KEY_FIELDS = 18 };

static void rule_key(const struct pg_derivation_input *input, uint64_t *key)
{
	const uint64_t fields[RULE_KEY_FIELDS] = {input->rule,
		(uintptr_t)input->parameters.binder, (uintptr_t)input->parameters.effects,
		input->parameters.level, input->parameters.direction,
		(uintptr_t)input->parameters.conversion, (uintptr_t)input->parameters.reduction,
		(uintptr_t)input->parameters.operation_label,
		(uintptr_t)input->parameters.handler,
		(uintptr_t)input->parameters.declaration,
		(uintptr_t)input->parameters.constructor,
		(uintptr_t)input->parameters.induction,
		(uintptr_t)input->source, (uintptr_t)input->target, input->reduction_kind, input->count,
		input->parameters.totality, (uintptr_t)input->parameters.constant};
	memcpy(key, fields, sizeof(fields));
}

/* Read the caller's fixed prefix and dependency array without assembling a
 * temporary key. Stored requests still own one flat immutable input array. */
static const void *request_operand(size_t prefix, const void *const *inputs,
	struct pg_synthesis_job *const *jobs, size_t index)
{
	return index < prefix ? inputs[index] : jobs[index - prefix];
}

static struct pg_synthesis_job *request_inputs_with_jobs(struct pg_synthesis *synthesis,
	enum job_role role, size_t prefix, const void *const *inputs,
	size_t job_count, struct pg_synthesis_job *const *jobs)
{
	if (job_count > SIZE_MAX - prefix) return NULL;
	size_t count = prefix + job_count;
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_job)) / sizeof(*inputs)) return NULL;
	uint64_t hash = ((unsigned)role ^ count) * UINT64_C(1099511628211);
	int producer_rule = role == DERIVATION_JOB;
	uint64_t key[RULE_KEY_FIELDS];
	if (producer_rule) {
		rule_key(inputs[0], key);
		for (size_t i = 0; i < RULE_KEY_FIELDS; ++i) hash = (hash ^ key[i]) * UINT64_C(1099511628211);
	}
	for (size_t i = producer_rule; i < count; ++i)
		hash = (hash ^ (uintptr_t)request_operand(prefix, inputs, jobs, i)) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (job->role != role) continue;
		if (job->input_count != count) continue;
		if (producer_rule) {
			uint64_t candidate[RULE_KEY_FIELDS];
			rule_key(job->inputs[0], candidate);
			if (memcmp(key, candidate, sizeof(key))) continue;
		}
		size_t i = producer_rule;
		while (i < count && job->inputs[i] == request_operand(prefix, inputs, jobs, i)) ++i;
		if (i == count) return job;
	}
	struct pg_synthesis_job *job = pg_alloc(synthesis->typing->graph, sizeof(*job) + count * sizeof(*inputs));
	if (!job) return NULL;
	job->owner = synthesis->owner_key;
	job->role = role;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = request_operand(prefix, inputs, jobs, i);
	if (producer_rule) {
		struct pg_derivation_input *input = pg_alloc(synthesis->typing->graph, sizeof(*input));
		if (!input) return NULL;
		*input = *(const struct pg_derivation_input *)inputs[0];
		job->inputs[0] = input;
	}
	if (pg_index_insert(&synthesis->jobs, &job->index, hash) != 0) return NULL;
	if (role == EVIDENCE_JOB) {
		job->result = inputs[0];
		job->status = PG_SYNTHESIS_DONE;
	} else if (role != DEFINITION_JOB) {
		enqueue(synthesis, job);
	}
	return job;
}

static struct pg_synthesis_job *request_inputs(struct pg_synthesis *synthesis,
	enum job_role role, size_t count, const void *const *inputs)
{
	return request_inputs_with_jobs(synthesis, role, count, inputs, 0, NULL);
}

static struct pg_synthesis_job *request_job(struct pg_synthesis *synthesis,
	enum job_role role, const void *first, const void *second)
{
	const void *inputs[] = {first, second};
	return request_inputs(synthesis, role, 2, inputs);
}

static struct pg_synthesis_job *request_role(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax, enum job_role role)
{
	if (!scope || scope->owner != synthesis->owner_key || !syntax) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, role, scope, syntax);
	if (job && !job->syntax && role == EXPRESSION_JOB && syntax->kind == PG_SYNTAX_QUALIFIED) {
		if (register_source_reference(synthesis, scope, SOURCE_ALLOCATION, job, NULL)) return NULL;
	}
	if (job) { job->scope = scope; job->syntax = syntax; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_operation(struct pg_synthesis *synthesis,
	const struct pg_operation_declaration *declaration)
{
	if (!declaration) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_operation_jobs(synthesis, pg_operation_label(declaration),
		pg_synthesis_evidence(synthesis, pg_operation_payload_type(declaration)),
		pg_synthesis_evidence(synthesis, pg_operation_response_type(declaration)));
	if (job && pg_evidence_owned_by(pg_operation_payload_type(declaration), synthesis->typing)
		&& pg_evidence_owned_by(pg_operation_response_type(declaration), synthesis->typing)) job->operation = declaration;
	return job;
}

struct pg_synthesis_job *pg_synthesis_operation_jobs(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_job *payload, struct pg_synthesis_job *response)
{
	const struct pg_term *a, *b;
	if (!payload || !response || payload->owner != synthesis->owner_key || response->owner != synthesis->owner_key) return NULL;
	if (!pg_operation_label_types(label, &a, &b)) return NULL;
	const void *inputs[] = {label, payload, response};
	return request_inputs(synthesis, OPERATION_JOB, 3, inputs);
}

int pg_synthesis_operation_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_operation_input *input)
{
	if (!job || !input || job->owner != synthesis->owner_key || job->role != OPERATION_JOB) return -1;
	*input = (struct pg_operation_input){job->inputs[0], (void *)job->inputs[1], (void *)job->inputs[2],
		job->context_allocation ? job->context_allocation->end : NULL};
	return 0;
}

struct pg_synthesis_job *pg_synthesis_handler_return(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *input,
	const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, input, clause};
	struct pg_synthesis_job *job = request_inputs(synthesis, HANDLER_RETURN_JOB, 3, inputs);
	if (job && !job->left) {
		job->scope = scope;
		job->syntax = clause;
		struct pg_synthesis_job *body = request_job(synthesis, BODY_JOB, input, NULL);
		if (!body) return NULL;
		struct pg_derivation_input projection = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
		struct pg_synthesis_job *premises[] = {scope->context_job, body};
		job->left = pg_synthesis_rule(synthesis, &projection, premises, NULL, NULL);
		if (!job->left) return NULL;
	}
	return job;
}

struct pg_synthesis_job *pg_synthesis_effect_inference(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work)
{
	if (!work || work->rows != synthesis->typing->graph) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, EFFECT_INFERENCE_JOB, work, NULL);
	if (job && job->stage && (work->sealed || work->failed)) {
		job->stage = 0;
		enqueue(synthesis, job);
	}
	return job;
}

struct pg_synthesis_job *pg_synthesis_effect_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, struct pg_synthesis_job *formation)
{
	if (!work || work->rows != synthesis->typing->graph || !mask) return NULL;
	if (!pg_effect_equation_parameter(work, target)) return NULL;
	struct pg_synthesis_job *structure = pg_synthesis_type_structure(synthesis, formation);
	if (!structure) return NULL;
	const void *inputs[] = {work, target, mask, structure};
	return request_inputs(synthesis, EFFECT_CONTRIBUTION_JOB, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_row_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, const struct pg_term *row)
{
	if (!work || work->rows != synthesis->typing->graph || !mask || !row) return NULL;
	if (!pg_effect_equation_parameter(work, target)) return NULL;
	const void *inputs[] = {work, target, mask, row};
	return request_inputs(synthesis, ROW_CONTRIBUTION_JOB, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_effect_substitution(struct pg_synthesis *synthesis,
	const struct pg_term *term, struct pg_effect_inference *work, size_t count,
	const struct pg_effect_equation *const *equations)
{
	if (!term || (count && !equations) || count > SIZE_MAX / sizeof(void *) - 2) return NULL;
	struct pg_synthesis_job *effects = pg_synthesis_effect_inference(synthesis, work);
	if (!effects) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_effect_equation_parameter(work, equations[i])) return NULL;
	struct pg_graph temporary = {0};
	const void **inputs = pg_alloc(&temporary, (count + 2) * sizeof(*inputs));
	if (!inputs) return NULL;
	inputs[0] = term; inputs[1] = effects;
	for (size_t i = 0; i < count; ++i) inputs[i + 2] = equations[i];
	struct pg_synthesis_job *job = request_inputs(synthesis, EFFECT_SUBSTITUTION_JOB, count + 2, inputs);
	pg_graph_destroy(&temporary);
	return job;
}

const struct pg_term *pg_synthesis_effect_substitution_result(const struct pg_synthesis_job *job)
{
	if (!job || job->role != EFFECT_SUBSTITUTION_JOB || job->status != PG_SYNTHESIS_DONE) return NULL;
	return job->effect_substitution->result;
}

struct pg_synthesis_job *pg_synthesis_handler_clause(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!carrier || carrier->owner != synthesis->owner_key) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, carrier, clause};
	struct pg_synthesis_job *job = request_inputs(synthesis, HANDLER_CLAUSE_JOB, 3, inputs);
	if (job) { job->scope = scope; job->syntax = clause; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_constant_result(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *callable, size_t parameters)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!callable || callable->owner != synthesis->owner_key) return NULL;
	struct pg_synthesis_job *result = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, callable);
	if (parameters > SIZE_MAX / sizeof(struct pg_synthesis_job *)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **scopes = pg_alloc(&temporary, parameters * sizeof(*scopes));
	if (parameters && !scopes) { pg_graph_destroy(&temporary); return NULL; }
	struct pg_derivation_input project = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
	struct pg_derivation_input apply = {.rule = PG_PI_CODOMAIN, .count = 2};
	struct pg_derivation_input pi = {.rule = PG_PI_FORM, .count = 2};
	struct pg_derivation_input codomain = {.rule = PG_PI_CONSTANT_CODOMAIN, .count = 1};
	/* Open the whole dependent telescope before discharging it in reverse.
	 * A scope request owns its binder even while its domain is still pending. */
	for (size_t i = 0; result && i < parameters; ++i) {
		struct pg_synthesis_job *scope = request_job(synthesis, PI_SCOPE_JOB, context, result);
		if (!scope) { result = NULL; break; }
		if (!scope->binder) scope->binder = pg_binder(synthesis->typing->graph);
		if (!scope->binder) { result = NULL; break; }
		scopes[i] = scope;
		struct pg_derivation_input variable = {.rule = PG_VARIABLE, .parameters.binder = scope->binder, .count = 1};
		struct pg_synthesis_job *premises[] = {scope, result};
		premises[0] = pg_synthesis_rule(synthesis, &project, premises, NULL, NULL);
		premises[1] = pg_synthesis_rule(synthesis, &variable, &scope, NULL, NULL);
		result = pg_synthesis_rule(synthesis, &apply, premises, NULL, NULL);
		context = scope;
	}
	for (size_t i = parameters; result && i; --i) {
		struct pg_synthesis_job *premises[] = {scopes[i - 1], result};
		result = pg_synthesis_rule(synthesis, &pi, premises, NULL, NULL);
		result = pg_synthesis_rule(synthesis, &codomain, &result, NULL, NULL);
	}
	pg_graph_destroy(&temporary);
	return result;
}

struct pg_synthesis_job *pg_synthesis_handler_carrier(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *returned,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation)
{
	if (!equation) return NULL;
	struct pg_synthesis_job *result = pg_synthesis_constant_result(synthesis, context, returned, 1);
	struct pg_derivation_input content = {.rule = PG_RETURN_CONTENT, .count = 1};
	result = pg_synthesis_rule(synthesis, &content, &result, NULL, NULL);
	struct pg_derivation_input carrier = {.rule = PG_RETURN_TYPE_FORM, .count = 1};
	return pg_synthesis_rule(synthesis, &carrier, &result, work, equation);
}

struct pg_synthesis_job *pg_synthesis_handler(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *syntax)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (carrier && carrier->owner != synthesis->owner_key) return NULL;
	if (!syntax || syntax->kind != PG_SYNTAX_ELIMINATION) return NULL;
	const void *inputs[] = {scope, carrier, syntax};
	struct pg_synthesis_job *job = request_inputs(synthesis, HANDLER_JOB, 3, inputs);
	if (job) { job->scope = scope; job->syntax = syntax; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_operation_reference(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer)
{
	if (!producer || producer->owner != synthesis->owner_key) return NULL;
	return request_job(synthesis, OPERATION_REFERENCE_JOB, producer, NULL);
}

struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return request_role(synthesis, scope, syntax, EXPRESSION_JOB);
}

static int register_source_allocation(struct pg_synthesis *synthesis, struct pg_synthesis_job *job);

static struct pg_synthesis_job *attach_nominal(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_data_declaration *allocation)
{
	if (!job || !allocation) return job;
	if (job->nominal_input) return job->nominal_input == allocation ? job : NULL;
	if (job->left || job->domain)
		return job->schema && pg_data_schema_declaration(job->schema) == allocation ? job : NULL;
	job->nominal_input = allocation;
	return register_source_allocation(synthesis, job) ? NULL : job;
}

struct pg_synthesis_job *pg_synthesis_declaration_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_data_declaration *allocation)
{
	if (!syntax || syntax->kind != PG_SYNTAX_DECLARATION) return NULL;
	return attach_nominal(synthesis, pg_synthesis_request(synthesis, scope, syntax), allocation);
}

int pg_synthesis_member_allocation(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_context **prefix, const struct pg_context **fields)
{
	if (!job || job->owner != synthesis->owner_key || job->role != EXPRESSION_JOB
		|| job->syntax->kind != PG_SYNTAX_QUALIFIED || !prefix || !fields) return -1;
	if (job->context_allocation) {
		*prefix = job->context_allocation->prefix;
		*fields = job->context_allocation->end;
		return 0;
	}
	struct pg_constructor_input input;
	if (pg_synthesis_constructor_input(synthesis, job->left, &input) || !input.allocated) return -1;
	*prefix = input.prefix; *fields = input.fields;
	return 0;
}

const struct pg_object *pg_synthesis_allocation_object(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->owner != synthesis->owner_key) return NULL;
	if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_QUALIFIED) {
		const struct pg_context *prefix, *fields;
		return pg_synthesis_member_allocation(synthesis, job, &prefix, &fields) || fields == prefix
			? NULL : fields->binder;
	}
	if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_ELIMINATION) {
		const struct pg_induction_allocation *allocation = source_induction_allocation(job);
		if (allocation) return allocation->self;
	}
	const struct pg_data_declaration *declaration = job->nominal_input;
	if (!declaration && job->schema) declaration = pg_data_schema_declaration(job->schema);
	return declaration ? pg_data_declaration_family(declaration) : NULL;
}

/* Address discovery does not wait for imported evidence to be accepted. */
static int register_source_allocation(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB) return 0;
	if (job->syntax->kind != PG_SYNTAX_DECLARATION && job->syntax->kind != PG_SYNTAX_ELIMINATION) return 0;
	const struct pg_object *object = pg_synthesis_allocation_object(synthesis, job);
	if (!object) return 0;
	const struct pg_data_declaration *declaration = pg_data_declaration_view(object);
	const struct pg_context *context = declaration ? pg_data_declaration_parameters(declaration)
		: job->match_allocation ? job->match_allocation->prefix : pg_evidence_context(pg_synthesis_result(job));
	if (register_source_reference(synthesis, job->scope, SOURCE_ALLOCATION, job, NULL)) return -1;
	/* An erased allocation can retain its defining input, not every later
	 * alias. Resolve this immutable scope relation once, at registration. */
	if (!job->scope->binder) return 0;
	if (!pg_context_lookup(context, job->scope->binder)) return 0;
	if (register_source_reference(synthesis, object, ALLOCATION_ORIGIN, job, NULL)) return -1;
	return declaration ? register_source_reference(synthesis,
		pg_data_matcher(pg_data_declaration_layout(declaration)), ALLOCATION_ORIGIN, job, NULL) : 0;
}

struct pg_synthesis_job *pg_synthesis_restore_elimination(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_match_allocation *allocation)
{
	if (!syntax || syntax->kind != PG_SYNTAX_ELIMINATION) return NULL;
	if (handler_syntax(syntax)) return NULL;
	if (!allocation || !allocation->motive || allocation->induction.count != syntax->item_count) return NULL;
	size_t count = allocation->induction.count;
	if (count > (SIZE_MAX - sizeof(*allocation)) / (2 * sizeof(void *))) return NULL;
	if (count && !allocation->induction.clauses) return NULL;
	const struct pg_object *binders[] = {allocation->induction.self, allocation->induction.argument, allocation->induction.recursion};
	for (size_t i = 0; i < 3; ++i) if (!binders[i] || binders[i]->kind != PG_BINDER) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job) return NULL;
	if (job->match_allocation) {
		const struct pg_match_allocation *old = job->match_allocation;
		if (old->prefix != allocation->prefix || old->motive != allocation->motive ||
			!pg_induction_allocation_equal(&old->induction, &allocation->induction)) return NULL;
		for (size_t i = 0; i < count; ++i) if (old->branches[i] != allocation->branches[i]) return NULL;
		return job;
	}
	if (job->result || job->value_job || job->match) return NULL;
	struct pg_match_allocation *saved = pg_alloc(synthesis->typing->graph, sizeof(*saved) + 2 * count * sizeof(void *));
	if (!saved) return NULL;
	*saved = *allocation;
	const struct pg_context **clauses = saved->branches + count;
	for (size_t i = 0; i < count; ++i) {
		saved->branches[i] = allocation->branches[i];
		clauses[i] = allocation->induction.clauses[i];
	}
	saved->induction.clauses = clauses;
	job->match_allocation = saved;
	return register_source_allocation(synthesis, job) ? NULL : job;
}

struct pg_synthesis_job *pg_synthesis_telescope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return request_role(synthesis, scope, syntax, TELESCOPE_JOB);
}

struct pg_synthesis_job *pg_synthesis_telescope_structure(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return request_role(synthesis, scope, syntax, TELESCOPE_STRUCTURE_JOB);
}

static int context_allocation_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_context *prefix,
	const struct pg_context *end, int started)
{
	size_t count;
	if (pg_context_extension_size(end, prefix, &count)
		|| count > (SIZE_MAX - sizeof(struct context_allocation)) / sizeof(void *)) return -1;
	struct context_allocation *allocation = job->context_allocation;
	if (allocation) {
		return allocation->prefix == prefix && allocation->end == end ? 0 : -1;
	} else {
		if (started) return -1;
		allocation = pg_alloc(synthesis->typing->graph, sizeof(*allocation) + count * sizeof(*allocation->contexts));
		if (!allocation) return -1;
		allocation->prefix = prefix; allocation->end = end; allocation->count = count;
		const struct pg_context *context = end;
		for (size_t i = count; i; --i, context = context->parent) allocation->contexts[i - 1] = context;
		job->context_allocation = allocation;
	}
	return 0;
}

static int same_context_binders(const struct pg_context *left, const struct pg_context *right)
{
	for (; left != right; left = left->parent, right = right->parent)
		if (!left || !right || left->binder != right->binder) return 0;
	return 1;
}

struct pg_synthesis_job *pg_synthesis_member_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *fields)
{
	if (!syntax || syntax->kind != PG_SYNTAX_QUALIFIED || fields == prefix) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job || context_allocation_at(synthesis, job, prefix, fields, job->left != NULL)) return NULL;
	return job;
}

struct pg_synthesis_job *pg_synthesis_operation_at(struct pg_synthesis *synthesis,
	const struct pg_object *label, struct pg_synthesis_job *payload, struct pg_synthesis_job *response,
	const struct pg_context *allocation)
{
	struct pg_synthesis_job *job = pg_synthesis_operation_jobs(synthesis, label, payload, response);
	size_t count;
	if (!job || pg_context_extension_size(allocation, NULL, &count) || count != 2) return NULL;
	return context_allocation_at(synthesis, job, NULL, allocation, job->left != NULL) ? NULL : job;
}

struct pg_synthesis_job *pg_synthesis_telescope_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *structure = pg_synthesis_telescope_structure(synthesis, scope, syntax);
	if (!structure || context_allocation_at(synthesis, structure, prefix, end, structure->inner != NULL)) return NULL;
	return pg_synthesis_telescope(synthesis, scope, syntax);
}

struct binding_cursor {
	const struct pg_source_scope *source;
	const struct pg_context *context;
	const struct pg_object *const *binders;
	size_t count;
};

/* Read one lexical address without copying its source telescope. */
static int binding_next(struct binding_cursor *cursor, const struct pg_object **binder)
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

static const struct pg_source_binding *source_binding_intern(struct pg_synthesis *synthesis,
	const struct pg_source_binding *input, struct binding_cursor initial)
{
	if (!synthesis || !input) return NULL;
	if (input->syntax) {
		if (input->constructor) return NULL;
		switch (input->syntax->kind) {
		case PG_SYNTAX_APPLICATION: break;
		case PG_SYNTAX_ELIMINATION:
			if (handler_syntax(input->syntax)) return NULL;
			if (input->slot) return NULL;
			break;
		case PG_SYNTAX_LAMBDA:
		case PG_SYNTAX_PI:
			if (input->slot) return NULL;
			break;
		case PG_SYNTAX_CLAUSE:
			if (!input->syntax->left) return NULL;
			if (return_clause(input->syntax)) {
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
	if (input->scope_count && !input->scope) return NULL;
	if (input->binder && input->binder->kind != PG_BINDER) return NULL;
	uint64_t hash = (uintptr_t)input->syntax ^ (uintptr_t)input->constructor ^ input->slot;
	struct binding_cursor cursor = initial;
	const struct pg_object *binder;
	size_t count = 0;
	while (binding_next(&cursor, &binder)) {
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
		size_t i = 0;
		while (binding_next(&cursor, &binder) && found->scope[i] == binder) ++i;
		if (i != count) continue;
		return !input->binder || input->binder == found->binder ? found : NULL;
	}
	struct source_binding *entry = pg_alloc(synthesis->typing->graph,
		sizeof(*entry) + count * sizeof(*input->scope));
	if (!entry) return NULL;
	const struct pg_object **scope = (void *)(entry + 1);
	cursor = initial;
	for (size_t i = 0; binding_next(&cursor, &binder); ++i) scope[i] = binder;
	entry->input = *input;
	entry->input.scope = scope;
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
	return source_binding_intern(synthesis, input,
		(struct binding_cursor){.binders = input->scope, .count = input->scope_count});
}

static const struct pg_object *source_binder(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax, size_t slot,
	const struct pg_object *binder)
{
	struct pg_source_binding input = {.syntax = syntax, .slot = slot, .binder = binder};
	const struct pg_source_binding *binding = source_binding_intern(synthesis, &input,
		(struct binding_cursor){.source = scope});
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

struct pg_synthesis_job *pg_synthesis_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return pg_synthesis_binding_at(synthesis, scope, syntax, NULL);
}

struct pg_synthesis_job *pg_synthesis_binding_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_object *binder)
{
	if (!syntax || (syntax->kind != PG_SYNTAX_LAMBDA && syntax->kind != PG_SYNTAX_PI)) return NULL;
	if (binder && binder->kind != PG_BINDER) return NULL;
	struct pg_synthesis_job *job = request_role(synthesis, scope, syntax, BINDING_JOB);
	if (!job) return NULL;
	if (job->binder && binder && job->binder != binder) return NULL;
	if (!job->binder) job->binder = source_binder(synthesis, scope, syntax, 0, binder);
	if (!job->binder) return NULL;
	if (!job->left) {
		const struct pg_syntax *domain = syntax->left;
		if (syntax->kind == PG_SYNTAX_PI && domain->kind == PG_SYNTAX_BINDER) domain = domain->left;
		job->left = pg_synthesis_request(synthesis, scope, domain);
		if (!job->left) return NULL;
	}
	if (!job->right) {
		job->right = request_job(synthesis, DOMAIN_JOB, scope, job->left);
		if (!job->right) return NULL;
		job->right->scope = scope;
		job->right->left = job->left;
	}
	if (!job->inner) {
		struct pg_token name = syntax->token;
		if (syntax->kind == PG_SYNTAX_PI)
			name = syntax->left->kind == PG_SYNTAX_BINDER ? syntax->left->token : (struct pg_token){0};
		job->inner = intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
			.name = name, .binder = job->binder, .context_job = job});
	}
	return job->inner ? job : NULL;
}

int pg_synthesis_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	const struct pg_syntax **syntax, const struct pg_object **binder)
{
	if (!synthesis || !job || !scope || !syntax || !binder) return -1;
	if (job->owner != synthesis->owner_key || job->role != BINDING_JOB || !job->binder) return -1;
	*scope = job->scope; *syntax = job->syntax; *binder = job->binder;
	return 0;
}

const struct pg_object *pg_synthesis_binding_binder(const struct pg_synthesis_job *job)
{
	return job && job->role == BINDING_JOB ? job->binder : NULL;
}

const struct pg_source_scope *pg_synthesis_binding_scope(const struct pg_synthesis_job *job)
{
	return job && job->role == BINDING_JOB ? job->inner : NULL;
}

const struct pg_source_scope *pg_synthesis_telescope_scope(const struct pg_synthesis_job *job)
{
	if (!job || job->status != PG_SYNTHESIS_DONE) return NULL;
	if (job->role != TELESCOPE_JOB && job->role != TELESCOPE_STRUCTURE_JOB) return NULL;
	return job->inner;
}

const struct pg_syntax *pg_synthesis_telescope_body(const struct pg_synthesis_job *job)
{
	return pg_synthesis_telescope_scope(job) ? job->tail : NULL;
}

struct pg_synthesis_job *pg_synthesis_evidence(struct pg_synthesis *synthesis,
	const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(proof, synthesis->typing)) return NULL;
	const void *inputs[] = {proof};
	return request_inputs(synthesis, EVIDENCE_JOB, 1, inputs);
}

static int type_structure_rule(enum pg_evidence_rule rule)
{
	switch (rule) {
	case PG_UNIVERSE_FORM: case PG_HOST_TYPE_FORM:
	case PG_RETURN_TYPE_FORM: case PG_THUNK_TYPE_FORM: case PG_PI_FORM:
	case PG_PI_DOMAIN: case PG_PI_CODOMAIN: case PG_RETURN_CONTENT:
	case PG_TYPE_FROM_VALUE: case PG_PI_CONSTANT_CODOMAIN: return 1;
	default: return 0;
	}
}

struct pg_synthesis_job *pg_synthesis_type_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation)
{
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	if (formation->role == DERIVATION_JOB &&
		type_structure_rule(((const struct pg_derivation_input *)formation->inputs[0])->rule))
		return pg_synthesis_term_structure(synthesis, formation);
	return request_job(synthesis, TYPE_STRUCTURE_JOB, formation, NULL);
}

const struct pg_term *pg_synthesis_type_structure_result(const struct pg_synthesis_job *job)
{
	if (!job || job->status != PG_SYNTHESIS_DONE) return NULL;
	switch (job->role) {
	case TYPE_STRUCTURE_JOB: case CLASSIFIER_STRUCTURE_JOB: case DECLARED_TYPE_JOB: case TERM_STRUCTURE_JOB:
		return job->type_structure;
	default: return NULL;
	}
}

struct pg_synthesis_job *pg_synthesis_classifier_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term)
{
	if (!term || term->owner != synthesis->owner_key) return NULL;
	return request_job(synthesis, CLASSIFIER_STRUCTURE_JOB, term, NULL);
}

struct pg_synthesis_job *pg_synthesis_term_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term)
{
	if (!term || term->owner != synthesis->owner_key) return NULL;
	return request_job(synthesis, TERM_STRUCTURE_JOB, term, NULL);
}

struct pg_synthesis_job *pg_synthesis_derivation(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input)
{
	return pg_synthesis_derivation_inference(synthesis, input, NULL);
}

struct pg_synthesis_job *pg_synthesis_derivation_inference(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct pg_effect_inference *work)
{
	if (!input) return NULL;
	const void *inputs[] = {input, work};
	return request_inputs(synthesis, DERIVATION_INPUT_JOB, 2, inputs);
}

static const struct pg_source_scope *bind_context(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, struct pg_synthesis_job *context, const struct pg_object *field,
	struct pg_synthesis_job *clause, enum pg_source_association association)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (!binder || binder->kind != PG_BINDER) return NULL;
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (field && field->kind != PG_BINDER) return NULL;
	const void *inputs[] = {parent, binder, context, field};
	struct pg_synthesis_job *checked = request_inputs(synthesis, SCOPE_CONTEXT_JOB, 4, inputs);
	if (!checked) return NULL;
	checked->scope = parent;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context_job = checked, .associated_binder = field,
		.association = association, .clause = clause});
}

const struct pg_source_scope *pg_synthesis_bind_context(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, struct pg_synthesis_job *context)
{
	return bind_context(synthesis, parent, name, binder, context, NULL, NULL, PG_SOURCE_UNASSOCIATED);
}

const struct pg_source_scope *pg_synthesis_bind_hypothesis(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_object *field,
	const struct pg_object *binder, struct pg_synthesis_job *context)
{
	if (!field) return NULL;
	return bind_context(synthesis, parent, (struct pg_token){0}, binder, context, field, NULL, PG_SOURCE_HYPOTHESIS);
}

const struct pg_source_scope *pg_synthesis_bind_graph(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_object *value,
	const struct pg_object *binder, struct pg_synthesis_job *context)
{
	if (!value) return NULL;
	return bind_context(synthesis, parent, (struct pg_token){0}, binder, context, value, NULL, PG_SOURCE_GRAPH);
}

struct pg_synthesis_job *pg_synthesis_rule(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct pg_synthesis_job *const *premises,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation)
{
	if (!input || (input->count && !premises)) return NULL;
	if (input->effect_parameter) return NULL;
	if (input->count > SIZE_MAX / sizeof(void *) - 3) return NULL;
	struct pg_synthesis_job *effects = NULL;
	if (work || equation) {
		if (!work || !equation || input->parameters.effects) return NULL;
		if (input->rule != PG_RETURN_TYPE_FORM) return NULL;
		if (!pg_effect_equation_parameter(work, equation)) return NULL;
		effects = pg_synthesis_effect_inference(synthesis, work);
		if (!effects) return NULL;
	}
	for (size_t i = 0; i < input->count; ++i)
		if (!premises[i] || premises[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {input, effects, equation};
	return request_inputs_with_jobs(synthesis, DERIVATION_JOB, 3, inputs, input->count, premises);
}

const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_evidence *proof)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!pg_evidence_owned_by(proof, synthesis->typing) || !pg_evidence_subject(proof)) return NULL;
	const struct pg_evidence *context = source_context(parent);
	if (!pg_evidence_owned_by(context, synthesis->typing) || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(proof), &count)) return NULL;
	return pg_synthesis_name_job(synthesis, parent, name, pg_synthesis_evidence(synthesis, proof));
}

const struct pg_source_scope *pg_synthesis_name_job(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *producer)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!producer || producer->owner != synthesis->owner_key) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.name = name, .context_job = parent->context_job, .producer = producer});
}

const struct pg_source_scope *pg_synthesis_import_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_source_scope *bindings)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (!bindings || bindings->owner != synthesis->owner_key) return NULL;
	if (!source_context(bindings) || pg_evidence_context(source_context(bindings))) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.context_job = parent->context_job, .imports = bindings});
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
		const struct pg_syntax *syntax = module->syntax;
		if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
		module = pg_synthesis_request(synthesis, module->scope, syntax);
		if (!module) return NULL;
	}
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.name = name, .context_job = parent->context_job, .exports = exports, .module = module});
}

const struct pg_source_scope *pg_synthesis_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports)
{
	if (!exports || exports->owner != synthesis->owner_key) return NULL;
	if (!source_context(exports) || pg_evidence_context(source_context(exports))) return NULL;
	return publish_namespace(synthesis, parent, name, exports, NULL);
}

const struct pg_source_scope *pg_synthesis_module_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *module)
{
	if (!module || module->owner != synthesis->owner_key || module->role != EXPRESSION_JOB) return NULL;
	if (!source_context(module->scope) || pg_evidence_context(source_context(module->scope))) return NULL;
	const struct pg_syntax *syntax = module->syntax;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	return publish_namespace(synthesis, parent, name, NULL, module);
}

struct pg_synthesis_job *pg_synthesis_reflexivity(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *input)
{
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!context || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, REFLEXIVITY_JOB, context, input);
	if (job) job->left = input;
	return job;
}

struct pg_synthesis_job *pg_synthesis_family_action_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	struct pg_synthesis_job *const *paths)
{
	if (!input || input->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(right_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(left_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (pg_evidence_judgement(right_substitution) != PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (count && !paths) return NULL;
	if (count > SIZE_MAX / sizeof(const void *) - 3) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!paths[i] || paths[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {left_substitution, right_substitution, input};
	struct pg_synthesis_job *job = request_inputs_with_jobs(synthesis, FAMILY_ACTION_JOB, 3, inputs, count, paths);
	if (job) job->left = input;
	return job;
}

struct pg_synthesis_job *pg_synthesis_family_action(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *input, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths)
{
	if (count && !paths) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_evidence_owned_by(paths[i], synthesis->typing)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **jobs = pg_alloc(&temporary, count * sizeof(*jobs));
	if (count && !jobs) { pg_graph_destroy(&temporary); return NULL; }
	for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_evidence(synthesis, paths[i]);
	struct pg_synthesis_job *result = pg_synthesis_family_action_jobs(synthesis,
		input, left_substitution, right_substitution, count, jobs);
	pg_graph_destroy(&temporary);
	return result;
}

static int reindex_inputs(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing)) return 0;
	if (!pg_evidence_owned_by(proof, synthesis->typing)) return 0;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	return pg_evidence_context(proof) == pg_evidence_context(pg_evidence_premise(substitution, 0));
}

struct pg_synthesis_job *pg_synthesis_reindex_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *substitution, struct pg_synthesis_job *proof)
{
	if (!substitution || substitution->owner != synthesis->owner_key) return NULL;
	if (!proof || proof->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {substitution, proof};
	return request_inputs(synthesis, REINDEX_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_expect(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term, struct pg_synthesis_job *type)
{
	if (!term || term->owner != synthesis->owner_key) return NULL;
	if (!type || type->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {term, type};
	return request_inputs(synthesis, EXPECT_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_family_transport_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *family, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	struct pg_synthesis_job *const *paths, struct pg_synthesis_job *value,
	enum pg_identity_direction direction)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	if (!family || family->owner != synthesis->owner_key) return NULL;
	if (!value || value->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(left_substitution, synthesis->typing)) return NULL;
	if (pg_evidence_rule(left_substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	struct pg_derivation_input as_value = {.rule = PG_VALUE_FROM_TYPE, .count = 1};
	struct pg_synthesis_job *type_value = pg_synthesis_rule(synthesis, &as_value, &family, NULL, NULL);
	struct pg_synthesis_job *action = pg_synthesis_family_action_jobs(synthesis,
		type_value, left_substitution, right_substitution, count, paths);
	if (!action) return NULL;
	struct pg_synthesis_job *context = pg_synthesis_evidence(synthesis,
		pg_evidence_premise(left_substitution, 1));
	action = pg_synthesis_normalize_classifier_jobs(synthesis, context, action);
	if (!action) return NULL;
	struct pg_derivation_input endpoints[] = {
		{.rule = PG_IDENTITY_LEFT_TYPE, .count = 1},
		{.rule = PG_IDENTITY_RIGHT_TYPE, .count = 1}
	};
	struct pg_synthesis_job *left = pg_synthesis_rule(synthesis, &endpoints[0], &action, NULL, NULL);
	struct pg_synthesis_job *right = pg_synthesis_rule(synthesis, &endpoints[1], &action, NULL, NULL);
	struct pg_synthesis_job *checked = pg_synthesis_expect(synthesis, value,
		direction == PG_IDENTITY_RIGHT ? left : right);
	struct pg_derivation_input transport = {.rule = PG_IDENTITY_TRANSPORT,
		.count = 3, .parameters.direction = direction};
	struct pg_synthesis_job *premises[] = {
		direction == PG_IDENTITY_RIGHT ? right : left, action, checked
	};
	return pg_synthesis_rule(synthesis, &transport, premises, NULL, NULL);
}

static struct pg_synthesis_job *constructor_transport_request(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	struct pg_synthesis_job *value, struct pg_synthesis_job *target_type,
	const struct pg_object *field)
{
	if (!synthesis) return NULL;
	const void *inputs[] = {context, left, right, path, value, target_type, field};
	for (size_t i = 0; i < 6; ++i) {
		const struct pg_synthesis_job *input = inputs[i];
		if (!input || input->owner != synthesis->owner_key) return NULL;
	}
	return request_inputs(synthesis, CONSTRUCTOR_TRANSPORT_JOB, 7, inputs);
}

struct pg_synthesis_job *pg_synthesis_disjoint_transport(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	struct pg_synthesis_job *value, struct pg_synthesis_job *target_type)
{
	return constructor_transport_request(synthesis, context, left, right, path, value, target_type, NULL);
}

struct pg_synthesis_job *pg_synthesis_constructor_field_identity(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *left,
	struct pg_synthesis_job *right, struct pg_synthesis_job *path,
	const struct pg_object *field, struct pg_synthesis_job *left_field,
	struct pg_synthesis_job *right_field)
{
	if (!field || field->kind != PG_BINDER) return NULL;
	return constructor_transport_request(synthesis, context, left, right, path, left_field, right_field, field);
}

static struct pg_synthesis_job *source_expect_request(struct pg_synthesis *synthesis, enum job_role role,
	const struct pg_source_scope *scope, struct pg_synthesis_job *term,
	struct pg_synthesis_job *type)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!term || term->owner != synthesis->owner_key) return NULL;
	if (!type || type->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {scope, term, type};
	struct pg_synthesis_job *job = request_inputs(synthesis, role, 3, inputs);
	if (job) { job->scope = scope; job->left = term; job->right = type; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_source_expect(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *term,
	struct pg_synthesis_job *type)
{
	return source_expect_request(synthesis, SOURCE_EXPECT_JOB, scope, term, type);
}

int pg_synthesis_source_expect_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, const struct pg_source_scope **scope,
	struct pg_synthesis_job **term, struct pg_synthesis_job **type)
{
	if (!synthesis || !job || !scope || !term || !type) return -1;
	if (job->owner != synthesis->owner_key || job->role != SOURCE_EXPECT_JOB) return -1;
	*scope = job->inputs[0]; *term = (void *)job->inputs[1]; *type = (void *)job->inputs[2];
	return 0;
}

struct pg_synthesis_job *pg_synthesis_application(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	return pg_synthesis_application_jobs(synthesis, pg_synthesis_evidence(synthesis, context), function, argument);
}

static struct pg_synthesis_job *application_domain(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *callee)
{
	struct pg_synthesis_job *formation = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, callee);
	if (!formation) return NULL;
	struct pg_derivation_input input = {.rule = PG_PI_DOMAIN, .count = 1};
	return pg_synthesis_rule(synthesis, &input, &formation, NULL, NULL);
}

struct pg_synthesis_job *pg_synthesis_application_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *function, struct pg_synthesis_job *argument)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!function || function->owner != synthesis->owner_key) return NULL;
	if (!argument || argument->owner != synthesis->owner_key) return NULL;
	struct pg_synthesis_job *callee = pg_synthesis_normalize_classifier_jobs(synthesis, context, function);
	if (!callee) return NULL;
	struct pg_synthesis_job *domain = application_domain(synthesis, context, callee);
	struct pg_synthesis_job *checked = pg_synthesis_expect(synthesis, argument, domain);
	if (!checked) return NULL;
	struct pg_derivation_input input = {.rule = PG_APP_ELIM, .count = 2};
	struct pg_synthesis_job *premises[] = {callee, checked};
	return pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
}

struct pg_synthesis_job *pg_synthesis_lambda_body(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context,
	struct pg_synthesis_job *body)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!body || body->owner != synthesis->owner_key) return NULL;
	body = request_job(synthesis, BODY_JOB, body, context);
	if (!body) return NULL;
	struct pg_synthesis_job *codomain = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, body);
	if (!codomain) return NULL;
	struct pg_derivation_input pi_input = {.rule = PG_PI_FORM, .count = 2};
	struct pg_synthesis_job *premises[] = {context, codomain};
	struct pg_synthesis_job *pi = pg_synthesis_rule(synthesis, &pi_input, premises, NULL, NULL);
	if (!pi) return NULL;
	struct pg_derivation_input lambda_input = {.rule = PG_LAMBDA_INTRO, .count = 2};
	struct pg_synthesis_job *lambda_premises[] = {pi, body};
	return pg_synthesis_rule(synthesis, &lambda_input, lambda_premises, NULL, NULL);
}

struct pg_synthesis_job *pg_synthesis_result_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *computation,
	const struct pg_object *binder)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!computation || computation->owner != synthesis->owner_key || !binder) return NULL;
	struct pg_synthesis_job *formation = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, computation);
	if (!formation) return NULL;
	struct pg_derivation_input content_input = {.rule = PG_RETURN_CONTENT, .count = 1};
	struct pg_synthesis_job *domain = pg_synthesis_rule(synthesis, &content_input, &formation, NULL, NULL);
	if (!domain) return NULL;
	struct pg_derivation_input extension = {.rule = PG_CONTEXT_EXTEND, .parameters.binder = binder, .count = 2};
	struct pg_synthesis_job *premises[] = {context, domain};
	return pg_synthesis_rule(synthesis, &extension, premises, NULL, NULL);
}

struct pg_synthesis_job *pg_synthesis_identity_instance(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *family,
	struct pg_synthesis_job *left, struct pg_synthesis_job *right)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!family || family->owner != synthesis->owner_key) return NULL;
	if (!left || left->owner != synthesis->owner_key) return NULL;
	if (!right || right->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, family, left, right};
	return request_inputs(synthesis, INSTANCE_JOB, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_reindex(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!reindex_inputs(synthesis, substitution, proof)) return NULL;
	return pg_synthesis_reindex_jobs(synthesis, pg_synthesis_evidence(synthesis, substitution),
		pg_synthesis_evidence(synthesis, proof));
}

struct pg_synthesis_job *pg_synthesis_substitution_pair(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_evidence *image)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(extension, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(image, synthesis->typing)) return NULL;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_rule(extension) == PG_CONTEXT_FAMILY_EXTEND) {
		const struct pg_evidence *result = pg_prove_substitution_pair(synthesis->typing, substitution, extension, image);
		return pg_synthesis_evidence(synthesis, result);
	}
	if (pg_evidence_rule(extension) != PG_CONTEXT_EXTEND) return NULL;
	if (pg_evidence_judgement(image) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_context(substitution) != pg_evidence_context(image)) return NULL;
	if (pg_evidence_context(pg_evidence_premise(substitution, 0)) != pg_evidence_context(extension)->parent) return NULL;
	const void *inputs[] = {substitution, extension, image};
	return request_inputs(synthesis, PAIR_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_data_schema(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration)
{
	return pg_synthesis_data_schema_at(synthesis, parameters, declaration, NULL);
}

struct pg_synthesis_job *pg_synthesis_data_schema_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parameters, const struct pg_syntax *declaration,
	const struct pg_data_declaration *allocation)
{
	if (!declaration || declaration->kind != PG_SYNTAX_DECLARATION) return NULL;
	return attach_nominal(synthesis, request_role(synthesis, parameters, declaration, DATA_SCHEMA_JOB), allocation);
}

const struct pg_data_schema *pg_synthesis_schema_result(const struct pg_synthesis_job *job)
{
	if (!job || job->role != DATA_SCHEMA_JOB || job->status != PG_SYNTHESIS_DONE) return NULL;
	return job->schema;
}

struct pg_synthesis_job *pg_synthesis_data_result(struct pg_synthesis *synthesis,
	const struct pg_source_scope *fields, const struct pg_evidence *parameters,
	const struct pg_evidence *indices, const struct pg_syntax *result)
{
	if (!fields || fields->owner != synthesis->owner_key || !result) return NULL;
	if (!pg_evidence_owned_by(parameters, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(indices, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(parameters) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (pg_evidence_judgement(indices) != PG_JUDGEMENT_CONTEXT) return NULL;
	const void *inputs[] = {fields, parameters, indices, result};
	struct pg_synthesis_job *job = request_inputs(synthesis, DATA_RESULT_JOB, 4, inputs);
	if (job) { job->scope = fields; job->syntax = result; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_data_case(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *body, const struct pg_data_schema *schema,
	const struct pg_object *constructor, const struct pg_evidence *motive)
{
	if (!body || body->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(motive, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_evidence *result = pg_data_schema_result(schema, constructor);
	if (!pg_evidence_owned_by(result, synthesis->typing)) return NULL;
	if (pg_evidence_context(motive) != pg_evidence_context(pg_evidence_premise(result, 0))) return NULL;
	const void *inputs[] = {schema, constructor, motive, body};
	struct pg_synthesis_job *job = request_inputs(synthesis, DATA_CASE_JOB, 4, inputs);
	if (job) job->left = body;
	return job;
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
	if (pg_evidence_context(parameters) != pg_evidence_context(source_context(scope))) return NULL;
	if (!pg_evidence_owned_by(motive_context, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(motive, synthesis->typing)) return NULL;
	if (generalization && (!pg_evidence_owned_by(generalization, synthesis->typing) ||
		pg_evidence_judgement(generalization) != PG_JUDGEMENT_SUBSTITUTION)) return NULL;
	if (generalization && pg_evidence_context(pg_evidence_premise(generalization, 0)) !=
		pg_evidence_context(source_context(scope))) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, formation, constructor, parameters, motive_context, motive, clause, generalization};
	struct pg_synthesis_job *job = request_inputs(synthesis, INDUCTION_BRANCH_JOB, 8, inputs);
	if (job) { job->scope = scope; job->syntax = clause; }
	return job;
}

struct pg_synthesis_job *pg_synthesis_abstract(struct pg_synthesis *synthesis,
	const struct pg_evidence *prefix, const struct pg_evidence *context,
	struct pg_synthesis_job *body)
{
	if (!body || body->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(prefix, synthesis->typing) || pg_evidence_judgement(prefix) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_evidence_owned_by(context, synthesis->typing) || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(prefix), &count)) return NULL;
	if (!count) return request_job(synthesis, BODY_JOB, body, pg_synthesis_evidence(synthesis, context));
	for (size_t i = 0; body && i < count; ++i) {
		body = pg_synthesis_lambda_body(synthesis,
			pg_synthesis_evidence(synthesis, context), body);
		context = pg_evidence_premise(context, 0);
	}
	return body;
}

struct pg_synthesis_job *pg_synthesis_constant_motive(struct pg_synthesis *synthesis,
	const struct pg_evidence *destination, const struct pg_evidence *fields,
	struct pg_synthesis_job *body)
{
	if (!body || body->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(destination, synthesis->typing) || pg_evidence_judgement(destination) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!pg_evidence_owned_by(fields, synthesis->typing) || pg_evidence_judgement(fields) != PG_JUDGEMENT_CONTEXT) return NULL;
	const void *inputs[] = {destination, fields, body};
	return request_inputs(synthesis, CONSTANT_MOTIVE_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_substitution(struct pg_synthesis *synthesis,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, struct pg_synthesis_job *const *images)
{
	if (!pg_evidence_owned_by(source, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(destination, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(source) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (pg_evidence_judgement(destination) != PG_JUDGEMENT_CONTEXT) return NULL;
	size_t arity;
	if (pg_context_extension_size(pg_evidence_context(source), NULL, &arity) != 0) return NULL;
	if (arity != count || (count && !images)) return NULL;
	return pg_synthesis_substitution_jobs(synthesis, pg_synthesis_evidence(synthesis, source),
		pg_synthesis_evidence(synthesis, destination), count, images);
}

struct pg_synthesis_job *pg_synthesis_substitution_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *source, struct pg_synthesis_job *destination,
	size_t count, struct pg_synthesis_job *const *images)
{
	if (!source || source->owner != synthesis->owner_key) return NULL;
	if (!destination || destination->owner != synthesis->owner_key) return NULL;
	if (count && !images) return NULL;
	if (count > SIZE_MAX / sizeof(const void *) - 2) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!images[i] || images[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {source, destination};
	return request_inputs_with_jobs(synthesis, SUBSTITUTION_JOB, 2, inputs, count, images);
}

struct pg_synthesis_job *pg_synthesis_substitution_lift(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing) || pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(extension, synthesis->typing)) return NULL;
	enum pg_evidence_rule rule = pg_evidence_rule(extension);
	if (rule != PG_CONTEXT_EXTEND && rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (!binder || pg_evidence_context(extension)->parent != pg_evidence_context(pg_evidence_premise(substitution, 0))) return NULL;
	const void *inputs[] = {substitution, extension, binder};
	return request_inputs(synthesis, LIFT_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_inductive_instance(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *type)
{
	if (!type || type->owner != synthesis->owner_key) return NULL;
	if (type->status == PG_SYNTHESIS_DONE) type = pg_synthesis_evidence(synthesis, type->result);
	return type ? request_job(synthesis, INDUCTIVE_INSTANCE_JOB, type, NULL) : NULL;
}

int pg_synthesis_inductive_instance_result(const struct pg_synthesis_job *job,
	struct pg_inductive_instance *output)
{
	if (!output || !job || job->role != INDUCTIVE_INSTANCE_JOB || job->status != PG_SYNTHESIS_DONE) return 0;
	*output = *job->inductive_instance;
	return 1;
}

struct pg_synthesis_job *pg_synthesis_constructor_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters)
{
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	if (!parameters || parameters->owner != synthesis->owner_key || !constructor) return NULL;
	const void *inputs[] = {formation, parameters, constructor};
	return request_inputs(synthesis, CONSTRUCTOR_SCOPE_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_constructor_scope_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *job = pg_synthesis_constructor_scope(synthesis, formation, constructor, parameters);
	if (!job || context_allocation_at(synthesis, job, prefix, end, job->substitution != NULL)) return NULL;
	return job;
}

struct pg_synthesis_job *pg_synthesis_constructor_value_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters)
{
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	if (!parameters || parameters->owner != synthesis->owner_key || !constructor) return NULL;
	const void *inputs[] = {formation, constructor, parameters};
	struct pg_synthesis_job *job = request_inputs(synthesis, CONSTRUCTOR_VALUE_JOB, 3, inputs);
	if (!job) return NULL;
	if (!job->left) job->left = pg_synthesis_constructor_scope(synthesis, formation, constructor, parameters);
	return job->left ? job : NULL;
}

int pg_synthesis_constructor_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_constructor_input *input)
{
	if (!job || !input || job->owner != synthesis->owner_key || job->role != CONSTRUCTOR_VALUE_JOB) return -1;
	*input = (struct pg_constructor_input){.formation = (void *)job->inputs[0],
		.parameters = (void *)job->inputs[2], .constructor = job->inputs[1]};
	const struct pg_synthesis_job *scope = job->left;
	if (scope->context_allocation) {
		input->allocated = 1;
		input->prefix = scope->context_allocation->prefix;
		input->fields = scope->context_allocation->end;
	} else if (scope->status == PG_SYNTHESIS_DONE) {
		input->allocated = 1;
		input->prefix = pg_evidence_context_map(input->parameters->result)->destination;
		input->fields = pg_evidence_context_map(scope->result)->destination;
	}
	return 0;
}

struct pg_synthesis_job *pg_synthesis_constructor_value(struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters)
{
	return pg_synthesis_constructor_value_jobs(synthesis, pg_synthesis_evidence(synthesis, formation),
		constructor, pg_synthesis_evidence(synthesis, parameters));
}

struct pg_synthesis_job *pg_synthesis_constructor_value_at(struct pg_synthesis *synthesis,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, const struct pg_context *prefix, const struct pg_context *end)
{
	struct pg_synthesis_job *scope = pg_synthesis_constructor_scope_at(synthesis,
		pg_synthesis_evidence(synthesis, formation), constructor,
		pg_synthesis_evidence(synthesis, parameters), prefix, end);
	if (!scope) return NULL;
	return pg_synthesis_constructor_value(synthesis, formation, constructor, parameters);
}

struct pg_synthesis_job *pg_synthesis_induction_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, struct pg_synthesis_job *motive_context,
	struct pg_synthesis_job *motive)
{
	struct pg_synthesis_job *producers[] = {formation, parameters, motive_context, motive};
	for (size_t i = 0; i < 4; ++i)
		if (!producers[i] || producers[i]->owner != synthesis->owner_key) return NULL;
	if (!constructor) return NULL;
	const void *inputs[] = {formation, parameters, motive_context, motive, constructor};
	return request_inputs(synthesis, INDUCTION_SCOPE_JOB, 5, inputs);
}

struct pg_synthesis_job *pg_synthesis_induction_scope_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation, const struct pg_object *constructor,
	struct pg_synthesis_job *parameters, struct pg_synthesis_job *motive_context,
	struct pg_synthesis_job *motive, const struct pg_context *fields, const struct pg_context *end)
{
	struct pg_synthesis_job *job = pg_synthesis_induction_scope(synthesis, formation, constructor, parameters, motive_context, motive);
	if (!job || context_allocation_at(synthesis, job, fields, end, job->substitution != NULL)) return NULL;
	return job;
}

static int typed_input(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(context, synthesis->typing) || !pg_evidence_owned_by(proof, synthesis->typing)) return 0;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	return pg_evidence_context(context) == pg_evidence_context(proof);
}

static struct pg_synthesis_job *request_typed(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof, enum job_role role)
{
	if (!typed_input(synthesis, context, proof)) return NULL;
	return request_job(synthesis, role, context, proof);
}

static struct pg_synthesis_job *request_evaluation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof, enum job_role role)
{
	if (!proof) return NULL;
	enum pg_evidence_judgement judgement = role == THUNK_JOB ? PG_JUDGEMENT_VALUE : PG_JUDGEMENT_COMPUTATION;
	if (pg_evidence_judgement(proof) != judgement) return NULL;
	return request_typed(synthesis, context, proof, role);
}

static int endpoint_selector(const struct pg_dimension_map *face)
{
	if (!face || face->target <= face->source || face->target - face->source != 1) return 0;
	if (!face->coordinates) return 0;
	if (face->coordinates[0].kind == PG_AXIS) return 0;
	size_t axes = 0, fixed = 0;
	for (size_t i = 0; i < face->target; ++i) {
		switch (face->coordinates[i].kind) {
		case PG_AXIS:
			if (face->coordinates[i].axis != axes++) return 0;
			break;
		case PG_ENDPOINT_ZERO: case PG_ENDPOINT_ONE:
			if (fixed++) return 0;
			break;
		default: return 0;
		}
	}
	return fixed == 1 && axes == face->source;
}

struct pg_synthesis_job *pg_synthesis_identity_endpoint(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!endpoint_selector(face)) return NULL;
	return pg_synthesis_identity_face(synthesis, context, formation, face);
}

struct pg_synthesis_job *pg_synthesis_identity_formation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer)
{
	if (!producer || producer->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {producer};
	return request_inputs(synthesis, FORMATION_JOB, 1, inputs);
}

struct pg_synthesis_job *pg_synthesis_identity_face_job(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *formation,
	const struct pg_dimension_map *face)
{
	if (!face || face->source >= face->target || !face->coordinates) return NULL;
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, formation, face};
	return request_inputs(synthesis, FACE_JOB, 3, inputs);
}

static int face_formation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return 0;
	if (!pg_evidence_owned_by(formation, synthesis->typing)) return 0;
	if (pg_evidence_context(context) != pg_evidence_context(formation)) return 0;
	switch (pg_evidence_judgement(formation)) {
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: return 1;
	default: return 0;
	}
}

struct pg_synthesis_job *pg_synthesis_permutation_source_face(struct pg_synthesis *synthesis,
	struct pg_dimensions *dimensions, const struct pg_evidence *context,
	struct pg_synthesis_job *formation, const struct pg_dimension_map *permutation,
	const struct pg_dimension_map *target_face, const struct pg_dimension_map **intrinsic)
{
	if (!dimensions || dimensions->graph != synthesis->typing->graph) return NULL;
	if (!intrinsic || !permutation || !target_face) return NULL;
	if (permutation->source != permutation->target) return NULL;
	if (target_face->source >= target_face->target) return NULL;
	permutation = pg_dimension_face(dimensions, permutation);
	if (!permutation) return NULL;
	const struct pg_dimension_map *composed = pg_dimension_compose(dimensions, permutation, target_face);
	const struct pg_dimension_map *ordered, *orientation;
	if (pg_dimension_face_factor(dimensions, composed, &ordered, &orientation) != 0) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_identity_face_job(synthesis, context, formation, ordered);
	if (!job) return NULL;
	*intrinsic = orientation;
	return job;
}

struct pg_synthesis_job *pg_synthesis_identity_face(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *formation,
	const struct pg_dimension_map *face)
{
	if (!face_formation(synthesis, context, formation)) return NULL;
	return pg_synthesis_identity_face_job(synthesis, context, pg_synthesis_evidence(synthesis, formation), face);
}

struct pg_synthesis_job *pg_synthesis_return(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *computation)
{
	return request_evaluation(synthesis, context, computation, RETURN_JOB);
}

struct pg_synthesis_job *pg_synthesis_unthunk(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *value)
{
	return request_evaluation(synthesis, context, value, THUNK_JOB);
}

struct pg_synthesis_job *pg_synthesis_normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!typed_input(synthesis, context, proof)) return NULL;
	return pg_synthesis_normalize_jobs(synthesis, pg_synthesis_evidence(synthesis, context),
		pg_synthesis_evidence(synthesis, proof), PG_REDUCTION_WHNF);
}

struct pg_synthesis_job *pg_synthesis_nf(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!typed_input(synthesis, context, proof)) return NULL;
	return pg_synthesis_normalize_jobs(synthesis, pg_synthesis_evidence(synthesis, context),
		pg_synthesis_evidence(synthesis, proof), PG_REDUCTION_NF);
}

static struct pg_synthesis_job *normalization_request(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *proof,
	enum pg_reduction_kind kind, int force)
{
	if (!synthesis || !context || !proof) return NULL;
	if (context->owner != synthesis->owner_key || proof->owner != synthesis->owner_key) return NULL;
	if (kind != PG_REDUCTION_WHNF && kind != PG_REDUCTION_NF) return NULL;
	const void *inputs[] = {context, proof, force ? &pg_force_operation : NULL};
	return request_inputs(synthesis, kind == PG_REDUCTION_NF ? NF_JOB : NORMALIZATION_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_normalize_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *proof, enum pg_reduction_kind kind)
{
	return normalization_request(synthesis, context, proof, kind, 0);
}

struct pg_synthesis_job *pg_synthesis_evaluate_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *proof, enum pg_reduction_kind kind)
{
	return normalization_request(synthesis, context, proof, kind, 1);
}

int pg_synthesis_normalization_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_synthesis_job **context,
	struct pg_synthesis_job **proof, enum pg_reduction_kind *kind, int *force)
{
	if (!synthesis || !job || !context || !proof || !kind) return -1;
	if (job->owner != synthesis->owner_key) return -1;
	if (job->role != NORMALIZATION_JOB && job->role != NF_JOB) return -1;
	*context = (void *)job->inputs[0];
	*proof = (void *)job->inputs[1];
	*kind = job->role == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF;
	if (force) *force = job->inputs[2] != NULL;
	return 0;
}

struct pg_synthesis_job *pg_synthesis_normalize_classifier(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!typed_input(synthesis, context, proof)) return NULL;
	switch (pg_evidence_judgement(proof)) {
	case PG_JUDGEMENT_TYPE_FAMILY:
		/* A pending callee may resolve to a family, not a CBPV computation. */
		return pg_synthesis_evidence(synthesis, proof);
	case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION:
		return pg_synthesis_normalize_classifier_jobs(synthesis,
			pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, proof));
	default: return NULL;
	}
}

struct pg_synthesis_job *pg_synthesis_normalize_classifier_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *proof)
{
	if (!context || context->owner != synthesis->owner_key) return NULL;
	if (!proof || proof->owner != synthesis->owner_key) return NULL;
	return request_job(synthesis, CLASSIFIER_JOB, context, proof);
}

static void wake(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int preparation)
{
	struct waiter **link = &job->waiters;
	while (*link) {
		struct waiter *waiter = *link;
		if (preparation && !waiter->preparation) { link = &waiter->next; continue; }
		*link = waiter->next;
		waiter->parent->dependency = NULL;
		enqueue(synthesis, waiter->parent);
	}
}

static void finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	if (status == PG_SYNTHESIS_DONE && job->role == EXPRESSION_JOB && job->match && job->result) {
		struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(job->result));
		if (!origin) status = PG_SYNTHESIS_ERROR;
		else if (!origin->match) origin->match = job;
	}
	if (status > PG_SYNTHESIS_DONE && job->role == HANDLER_JOB && job->handler && job->handler->effect_owner) {
		struct handler_state *owner = job->handler->effect_owner;
		if (!owner->effects.sealed && !owner->failure) {
			owner->failure = status;
			owner->effects.failed = 1;
			pg_synthesis_effect_inference(synthesis, &owner->effects);
		}
	}
	int first_finish = job->status == PG_SYNTHESIS_PENDING;
	job->status = status;
	/* Imported allocations were registered when attached, before Solve. */
	if (first_finish && !job->nominal_input && !job->match_allocation)
		if (register_source_allocation(synthesis, job)) job->status = PG_SYNTHESIS_ERROR;
	wake(synthesis, job, 0);
}

/* Subscribe exactly once at a stage transition. Completed dependencies need
 * no subscription; pending dependencies wake their consumers on completion. */
static void subscribe(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child, int preparation)
{
	if (!child) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	if (child->status != PG_SYNTHESIS_PENDING) {
		enqueue(synthesis, parent);
		return;
	}
	struct waiter *waiter = pg_alloc(synthesis->typing->graph, sizeof(*waiter));
	if (!waiter) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	*waiter = (struct waiter){parent, child, child->waiters, preparation};
	child->waiters = waiter;
	parent->dependency = waiter;
}

static void depend(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child)
{
	subscribe(synthesis, parent, child, 0);
}

/* Zero means the dependency is done. Otherwise suspend or propagate its
 * failure without interpreting its payload or changing the caller's stage. */
static inline int await_dependency(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *dependency)
{
	if (!dependency) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	if (dependency->status == PG_SYNTHESIS_DONE) return 0;
	if (dependency->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, dependency);
	else finish(synthesis, job, dependency->status);
	return 1;
}

static struct pg_synthesis_job *definition_registration(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	struct pg_synthesis_job *job = request_role(synthesis, scope, syntax, DEFINITION_SCOPE_JOB);
	if (!job || job->definitions) return job;
	struct definition_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
	if (!state) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL; }
	job->definitions = state;
	state->registration = job;
	if (pg_index_init(&state->names) != 0) goto fail;
	if (syntax->item_count > SIZE_MAX / sizeof(*state->entries)) goto fail;
	state->entries = pg_alloc(synthesis->typing->graph, syntax->item_count * sizeof(*state->entries));
	state->scope = intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
		.context_job = scope->context_job, .definitions = state});
	if (!state->scope || !state->entries) goto fail;
	state->count = syntax->item_count;
	state->syntax = syntax;
	return job;
fail:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return NULL;
}

static struct pg_synthesis_job *prepare_module(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *root)
{
	if (root->right) return root->right;
	const struct pg_syntax *syntax = root->syntax;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	struct pg_synthesis_job *registration = definition_registration(synthesis, root->scope, syntax);
	if (!registration) return NULL;
	root->right = registration;
	root->left = registration;
	depend(synthesis, root, registration);
	return registration;
}

static int definition_input(struct definition_state *state, size_t index,
	struct pg_synthesis_job *producer)
{
	if (!producer || index >= state->count) return -1;
	if (state->entries[index]) return state->entries[index] == producer ? 0 : -1;
	state->entries[index] = producer;
	return 0;
}

int pg_synthesis_retain_definition_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *root, size_t index, struct pg_synthesis_job *producer)
{
	if (!root || root->owner != synthesis->owner_key || root->role != EXPRESSION_JOB) return -1;
	if (!producer || producer->owner != synthesis->owner_key) return -1;
	const struct pg_syntax *syntax = root->syntax;
	if (syntax && syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS || index >= syntax->item_count) return -1;
	struct pg_synthesis_job *registration = prepare_module(synthesis, root);
	if (!registration) return -1;
	return definition_input(registration->definitions, index, producer);
}

const struct pg_source_scope *pg_synthesis_definition_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions)
{
	struct pg_synthesis_job *job = definition_registration(synthesis, scope, definitions);
	return job && job->status != PG_SYNTHESIS_ERROR ? job->definitions->scope : NULL;
}

struct pg_synthesis_job *pg_synthesis_definition_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *definitions,
	const struct pg_syntax *expression)
{
	if (!definitions || definitions->kind != PG_SYNTAX_DEFINITIONS || !expression) return NULL;
	struct pg_synthesis_job *registration = definition_registration(synthesis, scope, definitions);
	if (!registration) return NULL;
	struct pg_synthesis_job *job = request_job(synthesis, DEFINITION_JOB, registration, expression);
	if (job && !job->syntax) {
		job->syntax = expression;
		depend(synthesis, job, registration);
	}
	return job;
}

/* Forward proof-result jobs only; schema/namespace outputs have other payloads. */
static int forward_proof(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *canonical)
{
	if (!canonical) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	if (canonical == job) return 0;
	if (canonical->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, canonical);
	else {
		job->result = canonical->result;
		finish(synthesis, job, canonical->status);
	}
	return 1;
}

static const struct pg_evidence *value_type(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION_TYPE)
		return pg_prove_thunk_type(synthesis->typing, proof);
	return pg_prove_value_type(synthesis->typing, proof);
}

/* Type positions may advance pure computations or use their checked total
 * result. Expected types never supply missing synthesis. */
static const struct pg_evidence *type_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *context,
	const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) != PG_JUDGEMENT_COMPUTATION) return proof;
	if (!job->value_job) {
		job->value_job = pg_synthesis_return(synthesis, context, proof);
		depend(synthesis, job, job->value_job);
		return NULL;
	}
	struct pg_synthesis_job *producer = job->value_job;
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return NULL; }
	job->value_job = NULL;
	return producer->result;
}

static const struct pg_evidence *compare(struct pg_synthesis *synthesis, struct pg_synthesis_job *job);

static void domain_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->right) {
		if (await_dependency(synthesis, job, job->left)) return;
		const struct pg_evidence *input = type_input(synthesis, job, source_context(job->scope), job->left->result);
		if (!input) return;
		input = value_type(synthesis, input);
		if (!input) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		/* Share the annotation's checked WHNF with all context consumers,
		 * rather than exposing a family alias's nominal head only at Match. */
		job->right = pg_synthesis_normalize(synthesis, source_context(job->scope), input);
	}
	forward_proof(synthesis, job, job->right);
}

/* In binder annotations, an arrow telescope ending in @ declares a logical
 * family, not a promise to extract a type from an arbitrary computation.
 * Inspect the synthesized annotation, so an alias has the same contract. */
static int family_parameter_annotation(const struct pg_term *type)
{
	if (!pg_thunk_type_view(type, &type)) return 0;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	size_t count = 0;
	while (pg_pi_view(type, &domain, &binder, &body)) {
		++count;
		type = body;
	}
	uint64_t level;
	enum pg_totality totality;
	return count && pg_pure_computation_type_view(type, &totality, &body) && pg_universe_level(body, &level);
}

static int logical_family_signature(const struct pg_term *type)
{
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (!pg_pi_view(type, &domain, &binder, &body)) return 0;
	do { type = body; } while (pg_pi_view(type, &domain, &binder, &body));
	uint64_t level;
	return pg_universe_level(type, &level);
}

static const struct pg_term *family_parameter_structure(struct pg_graph *graph, const struct pg_term *type)
{
	struct parameter { const struct pg_term *domain; const struct pg_object *binder; struct parameter *next; };
	struct pg_graph temporary = {0};
	struct parameter *parameters = NULL;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	const struct pg_term *result = NULL;
	if (!pg_thunk_type_view(type, &type)) goto done;
	while (pg_pi_view(type, &domain, &binder, &body)) {
		struct parameter *next = pg_alloc(&temporary, sizeof(*next));
		if (!next) goto done;
		*next = (struct parameter){domain, binder, parameters};
		parameters = next;
		type = body;
	}
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(type, &totality, &result)) goto done;
	while (parameters && result) {
		result = pg_pi(graph, parameters->domain, parameters->binder, result);
		parameters = parameters->next;
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *family_parameter_context(struct pg_synthesis *synthesis,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *annotation)
{
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *type = pg_prove_thunk_content(typing, annotation);
	const struct pg_evidence *indices = parent;
	const struct pg_term *domain, *body;
	const struct pg_object *argument;
	while (type && pg_pi_view(pg_evidence_subject(type)->core, &domain, &argument, &body)) {
		indices = pg_prove_context_extension(typing, indices, argument, pg_prove_pi_domain(typing, type));
		if (!indices) return NULL;
		type = pg_prove_pi_codomain(typing, pg_prove_projection(typing, indices, type),
			pg_prove_variable(typing, indices, argument));
	}
	return pg_prove_family_context_extension(typing, parent, binder, indices,
		pg_prove_return_content(typing, type));
}

static void binding_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->syntax->binder_marker) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (await_dependency(synthesis, job, job->right)) return;
	job->domain = job->right->result;
	job->result = family_parameter_annotation(pg_evidence_subject(job->domain)->core)
		? family_parameter_context(synthesis, source_context(job->scope), job->binder, job->domain)
		: pg_prove_context_extension(synthesis->typing, source_context(job->scope), job->binder, job->domain);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void telescope_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->inner) { job->inner = job->scope; job->tail = job->syntax; }
	if (job->tail->kind != job->syntax->kind ||
		(job->tail->kind != PG_SYNTAX_LAMBDA && job->tail->kind != PG_SYNTAX_PI)) {
		finish(synthesis, job, job->context_allocation && job->context_allocation->next != job->context_allocation->count
			? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE);
		return;
	}
	const struct pg_object *binder = NULL;
	if (job->context_allocation) {
		struct context_allocation *allocation = job->context_allocation;
		if (allocation->next == allocation->count) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		binder = allocation->contexts[allocation->next++]->binder;
	}
	struct pg_synthesis_job *binding = pg_synthesis_binding_at(synthesis, job->inner, job->tail, binder);
	if (!binding) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	job->inner = binding->inner;
	job->tail = job->tail->right;
	enqueue(synthesis, job);
}

static void telescope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->left = pg_synthesis_telescope_structure(synthesis, job->scope, job->syntax);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	job->inner = job->left->inner;
	job->tail = job->left->tail;
	struct pg_synthesis_job *context = job->inner->context_job;
	if (await_dependency(synthesis, job, context)) return;
	job->result = source_context(job->inner);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void classifier_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (void *)job->inputs[i];
			if (await_dependency(synthesis, job, input)) return;
		}
		const struct pg_synthesis_job *context = job->inputs[0], *term = job->inputs[1];
		struct pg_synthesis_job *canonical = pg_synthesis_normalize_classifier(synthesis, context->result, term->result);
		if (!canonical) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (forward_proof(synthesis, job, canonical)) return;
		job->checking_term = term->result;
		if (!job->right) job->right = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, term);
		if (await_dependency(synthesis, job, job->right)) return;
		job->left = pg_synthesis_normalize(synthesis, context->result, job->right->result);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	job->checking_type = job->left->result;
	if (pg_evidence_classifier(job->checking_term) == pg_evidence_subject(job->checking_type)->core)
		job->result = job->checking_term;
	else job->result = compare(synthesis, job);
	if (job->result) finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static const struct pg_evidence *contents(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *proof)
{
	return job->role == THUNK_JOB ? pg_prove_thunk_computation(synthesis->typing, proof)
		: pg_prove_return_value(synthesis->typing, proof);
}

static void contents_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->left = pg_synthesis_normalize(synthesis, job->inputs[0], job->inputs[1]);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (!job->stage) {
		const struct pg_evidence *term = job->left->result;
		job->result = contents(synthesis, job, term);
		if (job->result) { finish(synthesis, job, PG_SYNTHESIS_DONE); return; }
		job->left = pg_synthesis_normalize_classifier(synthesis, job->inputs[0], term);
		job->stage = 1;
		depend(synthesis, job, job->left);
		return;
	}
	job->result = contents(synthesis, job, job->left->result);
	if (!job->result && job->role == RETURN_JOB) {
		job->result = pg_prove_total_pure_value(synthesis->typing, job->left->result);
	}
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
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
	struct pg_synthesis_job *producer;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	struct pg_synthesis_job *context;
};

static struct pg_synthesis_job *lookup_definition(const struct definition_state *state, struct pg_token token)
{
	struct block_name *name = lookup_name(&state->names, token);
	if (!name) return NULL;
	return name->producer ? name->producer : name->imported;
}

static struct source_reference lookup_scope(const struct pg_source_scope *scope, struct pg_token token)
{
	for (; scope; scope = scope->parent) {
		if (scope->definitions && token.kind == PG_TOKEN_IDENT) {
			struct pg_synthesis_job *producer = lookup_definition(scope->definitions, token);
			if (producer) return (struct source_reference){.producer = producer, .context = scope->context_job};
		}
		if (scope->name.kind != token.kind) continue;
		/* Punctuation tokens carry their spelling in kind, not text. */
		if (token.kind != '#' && token.kind != '*') {
			if (scope->name.length != token.length) continue;
			if (memcmp(scope->name.text, token.text, token.length) != 0) continue;
		}
		return (struct source_reference){scope->binder, scope->producer, scope->exports, scope->module, scope->context_job};
	}
	return (struct source_reference){0};
}

struct pg_synthesis_job *pg_synthesis_named_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_token name)
{
	if (!synthesis || !scope || scope->owner != synthesis->owner_key) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text) return NULL;
	return lookup_scope(scope, name).producer;
}

static struct pg_synthesis_job *operation_origin(const struct pg_synthesis_job *producer)
{
	if (!producer) return NULL;
	if (producer->role == OPERATION_REFERENCE_JOB) return producer->right ? producer->right : (void *)producer->inputs[0];
	if (producer->role == DEFINITION_JOB) return producer->left;
	if (producer->role != EXPRESSION_JOB) return NULL;
	const struct pg_syntax *syntax = producer->syntax;
	switch (syntax->kind) {
	case PG_SYNTAX_ATOM:
		if (producer->left) return producer->left;
		if (syntax->token.kind != PG_TOKEN_IDENT) return NULL;
		for (const struct pg_source_scope *scope = producer->scope; scope; scope = scope->parent)
			if (scope->definitions && scope->definitions->indexed < scope->definitions->count) return NULL;
		return lookup_scope(producer->scope, syntax->token).producer;
	case PG_SYNTAX_IMPORT: case PG_SYNTAX_QUOTE: case PG_SYNTAX_EXPECT:
		return producer->left;
	case PG_SYNTAX_QUALIFIED:
		if (syntax->left->kind == PG_SYNTAX_DEFINITIONS) return producer->value_job;
		return syntax->left->kind != PG_SYNTAX_BLOCK ? producer->left : NULL;
	default: return NULL;
	}
}

static const struct pg_synthesis_job *operation_reference_origin(const struct pg_synthesis_job *job)
{
	if (!job || job->role != OPERATION_REFERENCE_JOB) return NULL;
	if (job->status != PG_SYNTHESIS_PENDING && job->status != PG_SYNTHESIS_DONE) return NULL;
	const struct pg_synthesis_job *slow = job, *fast = job;
	while (slow) {
		if (slow->status != PG_SYNTHESIS_PENDING && slow->status != PG_SYNTHESIS_DONE) return NULL;
		if (slow->role == OPERATION_JOB) return slow;
		slow = operation_origin(slow);
		fast = operation_origin(operation_origin(fast));
		if (slow && slow == fast && slow->role != OPERATION_JOB) return NULL;
	}
	return NULL;
}

const struct pg_operation_declaration *pg_synthesis_operation_declaration(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *origin = operation_reference_origin(job);
	return origin ? origin->operation : NULL;
}

int pg_synthesis_operation_reference_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *reference, struct pg_operation_input *input)
{
	if (!synthesis || !reference || reference->owner != synthesis->owner_key) return -1;
	return pg_synthesis_operation_input(synthesis, operation_reference_origin(reference), input);
}

static const struct pg_syntax *block_syntax(const struct pg_syntax *syntax)
{
	if (syntax->kind == PG_SYNTAX_BLOCK) return syntax;
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_BLOCK) return syntax->left;
	return NULL;
}

static size_t block_end(const struct pg_syntax *syntax)
{
	if (syntax->kind == PG_SYNTAX_BLOCK) return syntax->item_count;
	const struct pg_syntax *block = syntax->left;
	for (size_t i = 0; i < block->item_count; ++i) {
		struct pg_token name = block->items[i].name;
		if (name.length && same_name(name, syntax->right->token)) return i + 1;
	}
	return 0;
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
			count = block_end(syntax);
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
				for (const struct pg_context *context = pg_evidence_context(source_context(scope));
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
			int handler = handler_syntax(syntax);
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

static int hypothesis_syntax(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_APPLICATION || syntax->left->kind != PG_SYNTAX_ATOM) return 0;
	return syntax->left->token.kind == '*' && syntax->right->kind == PG_SYNTAX_ATOM;
}

/* Collect APP domain constraints on IH applications in their lexical scopes.
 * Lambda domains use the ordinary pending binding producer. An annotation
 * never supplies a demand, and no incomplete branch is accepted as evidence. */
static int motive_scan_push(struct pg_synthesis *synthesis, struct match_branch *branch,
	const struct pg_syntax *syntax, const struct pg_source_scope *scope)
{
	struct motive_scan *scan = pg_alloc(synthesis->typing->graph, sizeof(*scan));
	if (!scan) return -1;
	*scan = (struct motive_scan){.syntax = syntax, .scope = scope, .next = branch->scan};
	branch->scan = scan;
	return 0;
}

/* 1 suspends, 0 completes, -1 is an allocation/input failure. Block scopes
 * belong to the source producer; constraint collection never recreates them. */
static int motive_demands(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct match_branch *branch, const struct pg_context *prefix)
{
	if (!branch->scan_started) {
		if (motive_scan_push(synthesis, branch, branch->clause->right, branch->scope)) return -1;
		branch->scan_started = 1;
	}
	struct motive_scan *scan = branch->scan;
	if (!scan) return 0;
	const struct pg_syntax *syntax = scan->syntax;
	const struct pg_source_scope *scope = scan->scope;
	const struct pg_syntax *block = block_syntax(syntax);
	if (block && scan->item < block_end(syntax)) {
		struct pg_synthesis_job *producer = pg_synthesis_request(synthesis, scope, syntax);
		if (!producer) return -1;
		struct block_state *state = producer->block;
		if (!state || !state->scopes || !state->scopes[scan->item]) {
			if (producer->status == PG_SYNTHESIS_PENDING) {
				if (producer->dependency) depend(synthesis, job, producer->dependency->child);
				else enqueue(synthesis, job);
				return 1;
			}
			branch->scan = scan->next;
		} else {
			size_t i = scan->item++;
			if (motive_scan_push(synthesis, branch, block->items[i].expression, state->scopes[i])) return -1;
		}
		enqueue(synthesis, job); return 1;
	}
	branch->scan = scan->next;
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		if (motive_scan_push(synthesis, branch, syntax->left, scope)) return -1;
	} else if (syntax->kind == PG_SYNTAX_LAMBDA) {
		struct pg_synthesis_job *binding = pg_synthesis_binding(synthesis, scope, syntax);
		if (!binding || !binding->inner) return -1;
		if (motive_scan_push(synthesis, branch, syntax->right, binding->inner)) return -1;
	} else if (syntax->kind == PG_SYNTAX_APPLICATION) {
		const struct pg_syntax *head = syntax->right;
		size_t count = 0;
		while (head->kind == PG_SYNTAX_APPLICATION && !hypothesis_syntax(head)) {
			++count;
			head = head->left;
		}
		if (hypothesis_syntax(head)) {
			const struct pg_object *field = hypothesis_field(scope, head->right->token);
			const struct pg_context *fields = pg_evidence_context(source_context(branch->scope));
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
		if (motive_scan_push(synthesis, branch, syntax->left, scope)) return -1;
		if (motive_scan_push(synthesis, branch, syntax->right, scope)) return -1;
	}
	enqueue(synthesis, job); return 1;
}

static int hypothesis_reference(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
	if (!hypothesis_syntax(syntax)) return 0;
	if (lookup_scope(job->scope, (struct pg_token){.kind = '*'}).binder) return 0;
	const struct pg_object *field = hypothesis_field(job->scope, syntax->right->token);
	if (!field) return 0;
	const struct pg_object *binder = associated_binder(job->scope, field, PG_SOURCE_HYPOTHESIS);
	if (!binder) return 0;
	const struct pg_evidence *value = pg_prove_variable(synthesis->typing, source_context(job->scope), binder);
	job->result = pg_prove_force(synthesis->typing, value);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return 1;
}

static struct pg_synthesis_job *source_reference_producer(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer)
{
	/* Storage quotation does not change the meaning of a source definition. */
	if (producer->role == DEFINITION_JOB && producer->left &&
		pg_evidence_judgement(producer->left->result) == PG_JUDGEMENT_COMPUTATION)
		return plain_rule(synthesis, PG_FORCE_ELIM, NULL, 1, &producer);
	return producer;
}

static enum pg_synthesis_status resolve_member(struct pg_synthesis *synthesis,
	struct source_reference *reference,
	struct pg_token token, struct pg_synthesis_job **dependency)
{
	if (reference->exports) {
		*reference = lookup_scope(reference->exports, token);
		return PG_SYNTHESIS_DONE;
	}
	if (reference->module) {
		struct pg_synthesis_job *module = reference->module;
		struct pg_synthesis_job *registration = definition_registration(synthesis, module->scope, module->syntax);
		if (!registration) return PG_SYNTHESIS_ERROR;
		*dependency = registration;
		if (registration->status != PG_SYNTHESIS_DONE) return registration->status;
		struct block_name *member = lookup_name(&registration->definitions->names, token);
		if (!member || !member->producer) return PG_SYNTHESIS_REJECTED;
		*dependency = module;
		if (module->status != PG_SYNTHESIS_DONE) return module->status;
		*reference = (struct source_reference){.producer = member->producer,
			.context = registration->definitions->scope->context_job};
		return PG_SYNTHESIS_DONE;
	}
	if (reference->producer) {
		/* Resolve in the defining context. The final reference rule projects
		 * the selected member into its use site after that context is checked. */
		struct pg_synthesis_job *context_job = reference->context;
		if (!context_job) return PG_SYNTHESIS_ERROR;
		struct pg_synthesis_job *producer = reference->producer;
		*dependency = producer;
		if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
		if (producer->exports) {
			*reference = lookup_scope(producer->exports, token);
			return PG_SYNTHESIS_DONE;
		}
		producer = source_reference_producer(synthesis, producer);
		if (!producer) return PG_SYNTHESIS_ERROR;
		*dependency = producer;
		if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
		*dependency = context_job;
		if (context_job->status != PG_SYNTHESIS_DONE) return context_job->status;
		const struct pg_evidence *context = context_job->result;
		const struct pg_evidence *proof = pg_prove_projection(synthesis->typing, context, producer->result);
		if (!proof) return PG_SYNTHESIS_UNSUPPORTED;
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION) {
			producer = pg_synthesis_return(synthesis, context, proof);
			if (!producer) return PG_SYNTHESIS_ERROR;
			*dependency = producer;
			if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
			proof = producer->result;
		}
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_TYPE_FAMILY) {
			producer = pg_synthesis_normalize(synthesis, context, proof);
			if (!producer) return PG_SYNTHESIS_ERROR;
			*dependency = producer;
			if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
			proof = producer->result;
		} else proof = value_type(synthesis, proof);
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis,
			pg_synthesis_evidence(synthesis, proof));
		if (!instance_job) return PG_SYNTHESIS_UNSUPPORTED;
		*dependency = instance_job;
		if (instance_job->status != PG_SYNTHESIS_DONE) return instance_job->status;
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) return PG_SYNTHESIS_ERROR;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(instance.formation));
		if (!origin || !origin->exports) return PG_SYNTHESIS_UNSUPPORTED;
		struct source_reference member = lookup_scope(origin->exports, token);
		if (!member.producer) return PG_SYNTHESIS_REJECTED;
		if (member.producer->role != CONSTRUCTOR_VALUE_JOB) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = (struct source_reference){.producer = pg_synthesis_constructor_value(synthesis,
			instance.formation, member.producer->inputs[1], instance.parameters), .context = context_job};
		return reference->producer ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR;
	}
	/* Nominal members require a typed declaration, never an older namespace. */
	return reference->binder || reference->producer ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_REJECTED;
}

static enum pg_synthesis_status resolve_reference(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct source_reference *reference, struct pg_synthesis_job **dependency)
{
	if (syntax->kind == PG_SYNTAX_IMPORT) {
		while (scope && !scope->imports) scope = scope->parent;
		if (!scope) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = lookup_scope(scope->imports, syntax->left->token);
		if (reference->producer) return PG_SYNTHESIS_DONE;
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
		*reference = (struct source_reference){.producer = pg_synthesis_request(synthesis, scope, root),
			.context = scope->context_job};
		if (!reference->producer) return PG_SYNTHESIS_ERROR;
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

static int function_graph_exports(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *declaration = pg_function_graph_declaration(&job->function_graph);
	const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(declaration, 0), 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing, context, context);
	const struct pg_data_layout *target = pg_data_schema_layout(pg_evidence_inductive_schema(declaration));
	const struct pg_source_scope *exports = intern_scope(synthesis,
		(struct pg_source_scope){.context_job = pg_synthesis_evidence(synthesis, context)});
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
		name->producer = pg_synthesis_constructor_value(synthesis, declaration,
			pg_data_constructor(target, index), parameters);
		if (pg_index_insert(&names, &name->index, name_hash(name->name))) goto done;
		exports = pg_synthesis_name_job(synthesis, exports, name->name, name->producer);
		if (!exports) goto done;
	}
	for (size_t index = 0; index < pg_data_layout_count(target); ++index) {
		struct pg_function_graph_case_source source;
		if (!pg_function_graph_case_source(&job->function_graph, index, &source)) goto done;
		if (!source.formation) continue;
		if (source.refined) job->case_layouts = NULL;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(source.formation));
		if (!origin || !origin->exports) goto done;
		const struct pg_object *constructor = pg_data_constructor(target, index);
		struct pg_synthesis_job *producer = pg_synthesis_constructor_value(synthesis, declaration, constructor, parameters);
		for (const struct pg_source_scope *name = origin->exports; name; name = name->parent) {
			if (!name->producer || name->producer->role != CONSTRUCTOR_VALUE_JOB ||
				name->producer->inputs[1] != source.constructor) continue;
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
			exports = pg_synthesis_name_job(synthesis, exports, alias->name, alias->producer);
			if (!exports) goto done;
		}
	}
	struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(declaration));
	if (!origin) goto done;
	origin->exports = exports;
	origin->graph = job;
	result = 0;
done:
	pg_index_destroy(&names); pg_index_destroy(&aliases);
	pg_graph_destroy(&temporary);
	return result;
}

/* Source call slots are an interface layout, not an execution schedule.
 * Lexical shadowing and block cutoffs use the same marker walker as IH scope
 * analysis. The backend validates every slot against its typed call plan. */
static int function_graph_order(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *origin)
{
	if (!origin || !origin->match) return 0;
	if (!pg_function_graph_case_input(&job->function_graph)) return 0;
	struct pg_graph temporary = {0};
	size_t count = origin->match->count;
	size_t trailing = pg_function_graph_trailing_arity(&job->function_graph);
	size_t generalized = origin->match->generalized_count;
	if (generalized > trailing) return -1;
	if (count > SIZE_MAX / sizeof(struct pg_function_graph_order)) return -1;
	struct pg_function_graph_order *orders = pg_alloc(&temporary, count * sizeof(*orders));
	int result = -1;
	if (count && !orders) goto done;
	job->case_layouts = pg_alloc(synthesis->typing->graph, count * sizeof(*job->case_layouts));
	if (count && !job->case_layouts) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct match_branch *branch = &origin->match->branches[i];
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
		const struct pg_syntax *top = block_syntax(body);
		size_t groups = top ? block_end(body) : 1;
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
			while (result_call->kind == PG_SYNTAX_APPLICATION && !hypothesis_syntax(result_call))
				result_call = result_call->left;
			if (marker_push(&temporary, &tasks, expression, top_shadow)) goto done;
			while (tasks) {
				const struct pg_syntax *syntax = tasks->syntax;
				const struct marker_shadow *shadow = tasks->shadow;
				tasks = tasks->next;
				if (hypothesis_syntax(syntax)) {
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
				const struct pg_syntax *block = block_syntax(syntax);
				if (block) {
					struct marker_task *items = NULL;
					for (size_t item = 0; item < block_end(syntax); ++item) {
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
		struct source_case_layout *layout = &job->case_layouts[i];
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
	result = pg_function_graph_source_order(&job->function_graph, count, orders);
done:
	pg_graph_destroy(&temporary);
	return result;
}

static struct pg_synthesis_job *function_graph_request(struct pg_synthesis *synthesis,
	const struct pg_evidence *function)
{
	const struct pg_occurrence *subject = pg_evidence_subject(function), *body = subject;
	while (body->core->kind == PG_LAMBDA && body->operand_count == 1) body = body->operands[0];
	const struct source_metadata *origin = source_metadata(synthesis, body);
	return request_job(synthesis, FUNCTION_GRAPH_JOB, subject, origin ? origin->match : NULL);
}

static void function_graph_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->function_graph.state) {
		const struct pg_evidence *function = pg_prove_structural_subject(synthesis->typing, job->inputs[0]);
		if (pg_function_graph_init(&job->function_graph,
			synthesis->typing, synthesis->normalization, function)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	if (!job->case_layouts && pg_function_graph_prepared(&job->function_graph) &&
		function_graph_order(synthesis, job, (void *)job->inputs[1])) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	enum pg_function_graph_status status = pg_function_graph_advance(&job->function_graph, 1);
	const struct pg_evidence *required = pg_function_graph_dependency(&job->function_graph);
	if (required) {
		struct pg_synthesis_job *graph = function_graph_request(synthesis, required);
		if (!graph || graph == job) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *witness = request_job(synthesis, FUNCTION_WITNESS_JOB, graph, NULL);
		if (await_dependency(synthesis, job, witness)) return;
		if (pg_function_graph_supply(&job->function_graph, &graph->function_graph)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		enqueue(synthesis, job);
		return;
	}
	if (status == PG_FUNCTION_GRAPH_PENDING) { enqueue(synthesis, job); return; }
	job->result = pg_function_graph_formation(&job->function_graph);
	if (status == PG_FUNCTION_GRAPH_DONE && function_graph_exports(synthesis, job)) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	finish(synthesis, job, status == PG_FUNCTION_GRAPH_DONE ? PG_SYNTHESIS_DONE :
		status == PG_FUNCTION_GRAPH_UNSUPPORTED ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_ERROR);
}

static void function_witness_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *graph = (struct pg_synthesis_job *)job->inputs[0];
	if (await_dependency(synthesis, job, graph)) return;
	enum pg_function_graph_status status = pg_function_graph_witness_advance(&graph->function_graph, 1);
	if (status == PG_FUNCTION_GRAPH_PENDING) { enqueue(synthesis, job); return; }
	if (status != PG_FUNCTION_GRAPH_DONE) {
		finish(synthesis, job, status == PG_FUNCTION_GRAPH_UNSUPPORTED ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_ERROR);
		return;
	}
	const struct pg_evidence *packet = pg_function_graph_packet(&graph->function_graph);
	const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(packet, 0), 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing, context, context);
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(pg_evidence_inductive_schema(packet)), 0);
	const struct pg_source_scope *exports = intern_scope(synthesis,
		(struct pg_source_scope){.context_job = pg_synthesis_evidence(synthesis, context)});
	exports = pg_synthesis_name_job(synthesis, exports,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "returned", .length = 8},
		pg_synthesis_constructor_value(synthesis, packet, constructor, parameters));
	struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(packet));
	if (!origin || !exports) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	origin->exports = exports;
	job->result = pg_function_graph_witness(&graph->function_graph);
	origin->packet = 1;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void graph_reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int witness)
{
	if (!job->value_job) {
		if (!job->function_source) {
			struct source_reference reference;
			struct pg_synthesis_job *dependency = NULL;
			const struct pg_syntax *name = witness ? job->syntax->right : job->syntax->left;
			enum pg_synthesis_status status = resolve_reference(synthesis, job->scope, name, &reference, &dependency);
			if (status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, dependency); return; }
			if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return; }
			if (!witness && reference.binder) {
				const struct pg_object *binder = associated_binder(job->scope, reference.binder, PG_SOURCE_GRAPH);
				if (!binder) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				job->result = pg_prove_variable(synthesis->typing, source_context(job->scope), binder);
				finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
				return;
			}
			if (!reference.producer) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			if (await_dependency(synthesis, job, reference.producer)) return;
			job->function_source = pg_alloc(synthesis->typing->graph, sizeof(*job->function_source));
			if (!job->function_source) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			job->function_source->function = reference.producer->result;
		}
		int status = pg_function_source_advance(synthesis->typing, job->function_source);
		if (!status) { enqueue(synthesis, job); return; }
		if (status < 0) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *graph = function_graph_request(synthesis, job->function_source->function);
		if (!graph) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (witness) graph = request_job(synthesis, FUNCTION_WITNESS_JOB, graph, NULL);
		struct pg_synthesis_job *premises[] = {job->scope->context_job, graph};
		job->value_job = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
	}
	forward_proof(synthesis, job, job->value_job);
}

static enum pg_synthesis_status resolve_source_reference(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct source_reference *reference,
	struct pg_synthesis_job **dependency)
{
	enum pg_synthesis_status status = resolve_reference(synthesis, job->scope, job->syntax, reference, dependency);
	if (status != PG_SYNTHESIS_DONE || !job->context_allocation) return status;
	struct pg_constructor_input input;
	if (pg_synthesis_constructor_input(synthesis, reference->producer, &input)) return PG_SYNTHESIS_REJECTED;
	*dependency = input.parameters;
	if ((*dependency)->status != PG_SYNTHESIS_DONE) return (*dependency)->status;
	const struct pg_context *prefix = pg_evidence_context(pg_evidence_premise(input.parameters->result, 1));
	if (job->context_allocation->prefix != prefix) return PG_SYNTHESIS_REJECTED;
	const struct pg_context *fields = job->context_allocation->end;
	if (input.allocated) {
		/* Declaration and use may retain the same binders through different
		 * checked field-type derivations. Only allocation is shared here. */
		const struct pg_context *existing = input.fields;
		while (existing != input.prefix && fields != prefix) {
			if (!existing || !fields || existing->binder != fields->binder) return PG_SYNTHESIS_REJECTED;
			existing = existing->parent; fields = fields->parent;
		}
		return existing == input.prefix && fields == prefix && input.prefix == prefix
			? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED;
	}
	return pg_synthesis_constructor_scope_at(synthesis, input.formation, input.constructor, input.parameters,
		prefix, fields) ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED;
}

static void reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *proof = job->left->result;
		if (!proof || !pg_evidence_subject(proof)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (job->syntax->kind == PG_SYNTAX_IMPORT && pg_evidence_context(proof)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct pg_synthesis_job *term = source_reference_producer(synthesis, job->left);
		job->value_job = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){job->scope->context_job, term});
		job->exports = job->left->exports;
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	struct pg_token token = job->syntax->token;
	if (job->syntax->kind == PG_SYNTAX_ATOM && token.kind == '*') {
		/* An explicitly supplied type assumption stays in the proof context.
		 * This is not recursive declaration admission or an IH lookup. */
		struct source_reference reference = lookup_scope(job->scope, token);
		if (!reference.binder) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		job->result = pg_prove_variable(synthesis->typing, source_context(job->scope), reference.binder);
		if (job->result && pg_evidence_judgement(job->result) != PG_JUDGEMENT_TYPE_FAMILY)
			job->result = pg_prove_value_type(synthesis->typing, job->result);
		if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	} else {
		struct source_reference reference;
		struct pg_synthesis_job *dependency = NULL;
		enum pg_synthesis_status status = resolve_source_reference(synthesis, job, &reference, &dependency);
		if (status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, dependency); return; }
		if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return; }
		if (reference.producer) {
			job->left = reference.producer;
			depend(synthesis, job, job->left);
			return;
		}
		if (reference.binder)
			job->result = pg_prove_variable(synthesis->typing, source_context(job->scope), reference.binder);
		if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void conversion_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->comparison.state) {
		if (pg_conversion_init(&job->comparison, synthesis->normalization,
			job->inputs[0], job->inputs[1]) != 0) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	enum pg_conversion_status status = pg_conversion_advance(&job->comparison, 1);
	if (status == PG_CONVERSION_PENDING) {
		enqueue(synthesis, job);
		return;
	}
	job->certificate = pg_conversion_certificate(&job->comparison);
	pg_conversion_destroy(&job->comparison);
	if (status == PG_CONVERSION_DIFFERENT) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	finish(synthesis, job, status == PG_CONVERSION_EQUAL ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static const struct pg_evidence *compare(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *comparison = request_job(synthesis, CONVERSION_JOB,
		pg_evidence_classifier(job->checking_term), pg_evidence_subject(job->checking_type)->core);
	if (await_dependency(synthesis, job, comparison)) return NULL;
	const struct pg_evidence *result = pg_prove_conversion(synthesis->typing,
		job->checking_term, job->checking_type, comparison->certificate);
	if (!result) finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return result;
}

static const struct pg_evidence *post_check_type(struct pg_synthesis *synthesis,
	const struct pg_evidence *term, const struct pg_evidence *target)
{
	if (pg_evidence_judgement(term) != PG_JUDGEMENT_COMPUTATION) return target;
	const struct pg_effect_row *source_row, *target_row;
	const struct pg_term *source_value, *target_value;
	enum pg_totality source_grade, target_grade;
	if (!pg_computation_type_view(pg_evidence_classifier(term), &source_grade, &source_row, &source_value)) return target;
	if (!pg_computation_type_view(pg_evidence_subject(target)->core, &target_grade, &target_row, &target_value)) return target;
	if (source_row == target_row && source_grade == target_grade) return target;
	if (source_grade < target_grade) return target;
	if (pg_effect_subset(source_row, target_row) != 1) return target;
	/* Compare result types at the source contract before weakening. Conversion
	 * remains symmetric and does not silently become effect subtyping. */
	return pg_prove_computation_type(synthesis->typing,
		source_grade, source_row, pg_prove_return_content(synthesis->typing, target));
}

static void expect_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->checking_term) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (struct pg_synthesis_job *)job->inputs[i];
			if (await_dependency(synthesis, job, input)) return;
		}
		const struct pg_evidence *term = ((const struct pg_synthesis_job *)job->inputs[0])->result;
		const struct pg_evidence *type = ((const struct pg_synthesis_job *)job->inputs[1])->result;
		if (!pg_evidence_owned_by(term, synthesis->typing)) goto rejected;
		if (!pg_evidence_owned_by(type, synthesis->typing)) goto rejected;
		if (pg_evidence_context(term) != pg_evidence_context(type)) goto rejected;
		switch (pg_evidence_judgement(term)) {
		case PG_JUDGEMENT_VALUE:
			if (pg_evidence_judgement(type) != PG_JUDGEMENT_VALUE_TYPE) goto rejected;
			break;
		case PG_JUDGEMENT_COMPUTATION:
			if (pg_evidence_judgement(type) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
			break;
		default: goto rejected;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_expect(synthesis,
			pg_synthesis_evidence(synthesis, term), pg_synthesis_evidence(synthesis, type));
		if (forward_proof(synthesis, job, canonical)) return;
		job->checking_term = term;
		job->checking_type = post_check_type(synthesis, term, type);
		if (!job->checking_type) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	job->result = compare(synthesis, job);
	const struct pg_evidence *target = ((const struct pg_synthesis_job *)job->inputs[1])->result;
	if (job->result && job->checking_type != target) {
		job->result = pg_prove_effect_subsumption(synthesis->typing, job->result, target);
		if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (job->result) finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

struct pg_synthesis_job *pg_synthesis_sequence(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *input,
	struct pg_synthesis_job *continuation)
{
	struct pg_synthesis_job *producers[] = {context, input, continuation};
	for (size_t i = 0; i < 3; ++i)
		if (!producers[i] || producers[i]->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, input, continuation};
	return request_inputs(synthesis, SEQUENCE_JOB, 3, inputs);
}

static int source_value_kind(const struct pg_synthesis_job *producer);
static int await_source_preparation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *producer, struct pg_synthesis_job **rule);

/* Sufficient structural conditions for choosing FOLD before acceptance.
 * Other cases still need checked conversion or finite-return inversion. */
static int fixed_sequence_fold(const struct pg_term *input, const struct pg_term *continuation)
{
	const struct pg_term *row, *value, *domain, *codomain, *following, *result;
	const struct pg_object *binder;
	enum pg_totality first, next;
	if (!pg_computation_type_spine_view(input, &first, &row, &value)) return 0;
	if (!pg_pi_view(continuation, &domain, &binder, &codomain)) return 0;
	if (pg_alpha_equal(domain, value) != 1 || pg_term_independent(codomain, binder) != 1) return 0;
	if (pg_computation_type_spine_view(codomain, &next, &following, &result)) return 1;
	const struct pg_effect_row *closed = pg_effect_row_view(row);
	return first == PG_TOTALITY_TOTAL && closed && !pg_effect_count(closed);
}

/* The compatibility policy quotes functions at value boundaries. Returning
 * computations keep their ordinary sequencing; no result value is assumed. */
static int prepare_value_argument(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *context, struct pg_synthesis_job **input)
{
	if (synthesis->definition_policy != PG_DEFINITION_IMPLICIT_THUNK) return 0;
	if (await_source_preparation(synthesis, job, *input, NULL)) return 1;
	struct pg_synthesis_job *normalized = pg_synthesis_normalize_classifier_jobs(synthesis, context, *input);
	struct pg_synthesis_job *shape = pg_synthesis_classifier_structure(synthesis, normalized);
	if (await_dependency(synthesis, job, shape)) return 1;
	const struct pg_term *type = pg_synthesis_type_structure_result(shape), *domain, *body;
	const struct pg_object *binder;
	if (pg_pi_view(type, &domain, &binder, &body) && !logical_family_signature(type)) {
		*input = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &normalized);
		if (!*input) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	}
	return 0;
}

static void sequence_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->value_job && !job->right) {
		struct pg_synthesis_job *argument = (void *)job->inputs[1];
		if (await_source_preparation(synthesis, job, argument, NULL)) return;
		int kind = source_value_kind(argument);
		if (kind < 0) {
			if (argument->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, argument); return; }
			finish(synthesis, job, argument->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : argument->status);
			return;
		}
		if (kind > 0) {
			if (kind == 2) {
				struct pg_derivation_input input = {.rule = PG_VALUE_FROM_TYPE, .count = 1};
				argument = pg_synthesis_rule(synthesis, &input, &argument, NULL, NULL);
			}
			job->value_job = pg_synthesis_application_jobs(synthesis, (void *)job->inputs[0],
				(void *)job->inputs[2], argument);
			if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		} else {
			job->left = pg_synthesis_normalize_classifier_jobs(synthesis, (void *)job->inputs[0], argument);
			if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_synthesis_job *premises[] = {job->left, (void *)job->inputs[2]};
			struct pg_derivation_input input = {.rule = PG_FOLD_ELIM, .count = 2};
			job->right = pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
			if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
	}
	if (job->value_job && job->value_job != job->right) { forward_proof(synthesis, job, job->value_job); return; }
	if (!job->value_job) {
		struct pg_synthesis_job *shapes[] = {
			pg_synthesis_classifier_structure(synthesis, job->left),
			pg_synthesis_classifier_structure(synthesis, (void *)job->inputs[2])};
		for (size_t i = 0; i < 2; ++i) {
			if (!shapes[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (shapes[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, shapes[i]); return; }
			if (shapes[i]->status == PG_SYNTHESIS_ERROR) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		if (fixed_sequence_fold(pg_synthesis_type_structure_result(shapes[0]), pg_synthesis_type_structure_result(shapes[1])))
			job->value_job = job->right;
	}
	for (size_t i = 0; i < 3; ++i) {
		struct pg_synthesis_job *input = (void *)job->inputs[i];
		if (await_dependency(synthesis, job, input)) return;
	}
	const struct pg_evidence *context = ((const struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_evidence *input = ((const struct pg_synthesis_job *)job->inputs[1])->result;
	const struct pg_evidence *continuation = ((const struct pg_synthesis_job *)job->inputs[2])->result;
	if (!context || pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT || !input || !continuation) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (pg_evidence_context(input) != pg_evidence_context(context) ||
		pg_evidence_context(continuation) != pg_evidence_context(context)) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (pg_evidence_judgement(input) != PG_JUDGEMENT_COMPUTATION) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (await_dependency(synthesis, job, job->left)) return;
	input = job->left->result;
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->value_job || job->right->status != PG_SYNTHESIS_REJECTED) {
		job->value_job = job->right;
		forward_proof(synthesis, job, job->value_job); return;
	}
	const struct pg_term *content, *domain, *body;
	const struct pg_object *binder;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	const struct pg_term *codomain = pg_pi_constant_codomain(pg_evidence_classifier(continuation));
	if (pg_computation_type_view(pg_evidence_classifier(input), &totality, &effects, &content) &&
		pg_effect_count(effects) && pg_pi_view(codomain, &domain, &binder, &body)) {
		/* Preserve the effectful prefix outside the returned function.
		 * The source adapter uses only checked APP/THUNK/RETURN/FOLD. */
		binder = pg_binder(synthesis->typing->graph);
		struct pg_synthesis_job *extended = pg_synthesis_result_context(synthesis,
			(void *)job->inputs[0], job->left, binder);
		struct pg_synthesis_job *variable = plain_rule(synthesis, PG_VARIABLE, binder, 1, &extended);
		struct pg_synthesis_job *premises[] = {extended, (void *)job->inputs[2]};
		struct pg_synthesis_job *projected = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
		struct pg_synthesis_job *applied = pg_synthesis_application_jobs(synthesis, extended, projected, variable);
		struct pg_synthesis_job *quoted = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &applied);
		premises[0] = job->left;
		premises[1] = pg_synthesis_lambda_body(synthesis, extended, quoted);
		job->value_job = plain_rule(synthesis, PG_FOLD_ELIM, NULL, 2, premises);
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	struct pg_synthesis_job *argument = pg_synthesis_return(synthesis, context, input);
	job->value_job = pg_synthesis_application(synthesis, context,
		(void *)job->inputs[2], argument);
	forward_proof(synthesis, job, job->value_job);
}

static struct pg_synthesis_job *rule_premise(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index);

static int same_name(struct pg_token left, struct pg_token right)
{
	if (left.length != right.length) return 0;
	return !left.length || memcmp(left.text, right.text, left.length) == 0;
}

static const struct pg_evidence *classifier_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_evidence *context, const struct pg_evidence *input)
{
	if (!job->value_job) {
		job->value_job = pg_synthesis_normalize_classifier(synthesis, context, input);
		depend(synthesis, job, job->value_job);
		return NULL;
	}
	struct pg_synthesis_job *producer = job->value_job;
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return NULL; }
	job->value_job = NULL;
	return producer->result;
}

static void instance_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *context = job->inputs[0];
	if (!job->function) {
		const struct pg_evidence *values[3];
		for (size_t i = 0; i < 3; ++i) {
			struct pg_synthesis_job *producer = (void *)job->inputs[i + 1];
			if (await_dependency(synthesis, job, producer)) return;
			values[i] = producer->result;
			if (!pg_evidence_owned_by(values[i], synthesis->typing)) goto rejected;
			if (pg_evidence_judgement(values[i]) != PG_JUDGEMENT_VALUE) goto rejected;
			if (pg_evidence_context(values[i]) != pg_evidence_context(context)) goto rejected;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_instance(synthesis, context,
			pg_synthesis_evidence(synthesis, values[0]), pg_synthesis_evidence(synthesis, values[1]),
			pg_synthesis_evidence(synthesis, values[2]));
		if (forward_proof(synthesis, job, canonical)) return;
		job->function = values[0];
	}
	if (!job->stage) {
		const struct pg_evidence *family = classifier_input(synthesis, job, context, job->function);
		if (!family) return;
		job->function = family;
		job->stage = 1;
	}
	if (!job->value_job) {
		enum pg_evidence_rule side = job->stage == 1 ? PG_IDENTITY_LEFT_TYPE : PG_IDENTITY_RIGHT_TYPE;
		const struct pg_evidence *type = pg_prove_identity_endpoint_type(synthesis->typing,
			job->function, side);
		if (!type) goto rejected;
		job->value_job = pg_synthesis_expect(synthesis, (void *)job->inputs[job->stage + 1],
			pg_synthesis_evidence(synthesis, type));
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (await_dependency(synthesis, job, job->value_job)) return;
	if (job->stage == 1) {
		job->checking_term = job->value_job->result;
		job->value_job = NULL;
		job->stage = 2;
		enqueue(synthesis, job);
		return;
	}
	job->result = pg_prove_identity_instance(synthesis->typing,
		job->function, job->checking_term, job->value_job->result);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static int register_name(struct pg_synthesis *synthesis, struct pg_index *names, struct pg_token name)
{
	if (!name.length) return 0;
	if (lookup_name(names, name)) return 1;
	struct block_name *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	if (!entry) return -1;
	entry->name = name;
	return pg_index_insert(names, &entry->index, name_hash(name));
}

static void block_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->block) {
		struct block_state *block = pg_alloc(synthesis->typing->graph, sizeof(*block));
		if (!block) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->block = block;
		block->syntax = job->syntax;
		block->scope = job->scope;
		if (pg_index_init(&block->names) != 0) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->syntax->kind == PG_SYNTAX_QUALIFIED) {
			block->syntax = job->syntax->left;
			block->end = block_end(job->syntax);
			if (!block->end) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		} else block->end = block->syntax->item_count;
		if (!block->end) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (block->end > SIZE_MAX / sizeof(*block->scopes)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		block->scopes = pg_alloc(synthesis->typing->graph, block->end * sizeof(*block->scopes));
		if (!block->scopes) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct block_state *block = job->block;
	if (block->tail) {
		if (!block->frames) {
			forward_proof(synthesis, job, block->tail);
			return;
		}
		const struct block_frame *frame = block->frames;
		struct pg_synthesis_job *context = frame->context;
		if (!frame->binds) {
			if (await_source_preparation(synthesis, job, frame->input, NULL)) return;
			if (source_value_kind(frame->input) != 0) {
				if (await_dependency(synthesis, job, frame->input)) return;
				if (value(synthesis, frame->input->result)) {
					block->frames = frame->parent;
					enqueue(synthesis, job);
					return;
				}
			}
			struct pg_synthesis_job *input = pg_synthesis_normalize_classifier_jobs(synthesis, context, frame->input);
			context = pg_synthesis_result_context(synthesis, context, input, pg_binder(synthesis->typing->graph));
			struct pg_derivation_input projection = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
			struct pg_synthesis_job *premises[] = {context, block->tail};
			block->tail = pg_synthesis_rule(synthesis, &projection, premises, NULL, NULL);
		}
		if (!context || !block->tail) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, context, block->tail);
		block->tail = pg_synthesis_sequence(synthesis, rule_premise(synthesis, context, 0),
			frame->input, continuation);
		if (!block->tail) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		block->frames = frame->parent;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_syntax_item *item = &block->syntax->items[block->next];
	block->scopes[block->next] = block->scope;
	struct pg_synthesis_job *input = pg_synthesis_request(synthesis, block->scope, item->expression);
	if (!input) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (item->annotation) {
		input = source_expect_request(synthesis, BINDING_EXPECT_JOB, block->scope, input,
			pg_synthesis_request(synthesis, block->scope, item->annotation));
	}
	if (!input) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (item->name.length && block->next + 1 < block->end)
		if (prepare_value_argument(synthesis, job, block->scope->context_job, &input)) return;
	int name_status = register_name(synthesis, &block->names, item->name);
	if (name_status) { finish(synthesis, job, name_status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
	++block->next;
	struct pg_synthesis_job *normalized = NULL;
	if (block->next == block->end || item->name.length) {
		struct pg_synthesis_job *body = request_job(synthesis, BODY_JOB, input, NULL);
		normalized = pg_synthesis_normalize_classifier_jobs(synthesis, block->scope->context_job, body);
		if (!normalized) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (block->next == block->end) block->tail = normalized;
	else {
		struct block_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
		if (!frame) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		*frame = (struct block_frame){input, block->scope->context_job, block->frames, item->name.length != 0};
		block->frames = frame;
		if (frame->binds) {
			const struct pg_object *binder = pg_binder(synthesis->typing->graph);
			frame->context = pg_synthesis_result_context(synthesis, frame->context, normalized, binder);
			block->scope = pg_synthesis_bind_context(synthesis, block->scope, item->name, binder, frame->context);
			if (!block->scope) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
	}
	enqueue(synthesis, job);
}

static void definition_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->stage) {
		struct pg_synthesis_job *registration = (void *)job->inputs[0];
		if (registration->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, registration);
		else finish(synthesis, job, registration->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : registration->status);
		return;
	}
	if (!job->left) {
		job->left = pg_synthesis_request(synthesis, job->scope, job->syntax);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *result = job->left->result;
	if (pg_evidence_judgement(result) == PG_JUDGEMENT_COMPUTATION) {
		if (synthesis->definition_policy == PG_DEFINITION_EXPLICIT_THUNK) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		result = pg_prove_thunk(synthesis->typing, result);
	}
	job->result = result;
	job->exports = job->left->exports;
	finish(synthesis, job, result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void source_expect_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *parts[] = {job->left, job->right};
	for (size_t i = 0; i < 2; ++i) {
		if (await_dependency(synthesis, job, parts[i])) return;
	}
	if (!job->checking_term) {
		struct pg_synthesis_job *term = source_reference_producer(synthesis, job->left);
		if (!term) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->role == BINDING_EXPECT_JOB) {
			term = pg_synthesis_normalize_classifier_jobs(synthesis, job->scope->context_job, term);
			if (!term) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		if (await_dependency(synthesis, job, term)) return;
		const struct pg_evidence *left = pg_prove_projection(synthesis->typing,
			source_context(job->scope), term->result);
		const struct pg_evidence *right = type_input(synthesis, job, source_context(job->scope), job->right->result);
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
			int result_binding = job->role == BINDING_EXPECT_JOB &&
				pg_computation_type_view(pg_evidence_classifier(left), &totality, &effects, &content);
			if (result_binding || pg_evidence_judgement(right) != PG_JUDGEMENT_COMPUTATION_TYPE)
				right = pg_prove_computation_type(synthesis->typing,
					totality, effects, value_type(synthesis, right));
		}
		if (!right) goto rejected;
		job->checking_term = left;
		job->checking_type = right;
	}
	if (!job->value_job) {
		job->value_job = pg_synthesis_expect(synthesis,
			pg_synthesis_evidence(synthesis, job->checking_term),
			pg_synthesis_evidence(synthesis, job->checking_type));
	}
	job->exports = job->left->exports;
	forward_proof(synthesis, job, job->value_job);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static void definition_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct definition_state *state = job->definitions;
	/* Local definitions remain dormant until registration completes. Import
	 * references use the immutable incoming provider scope, never this index. */
	if (state->indexed < state->count) {
		size_t i = state->indexed++;
		const struct pg_syntax_item *item = &state->syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) {
			if (item->operation != PG_TOKEN_ASSIGN && item->operation != PG_SYNTAX_IMPORT) {
				finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
			}
			if (register_name(synthesis, &state->names, item->name) < 0) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
			struct block_name *name = lookup_name(&state->names, item->name);
			if (!name) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (item->operation == PG_SYNTAX_IMPORT) {
				if (!name->imported) name->imported = pg_synthesis_request(synthesis, job->scope, item->expression);
				if (!name->imported) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (definition_input(state, i, name->imported)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			} else {
				if (name->producer) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				name->producer = request_job(synthesis, DEFINITION_JOB, job, item->expression);
				if (name->producer) {
					name->producer->scope = state->scope;
					name->producer->syntax = item->expression;
				}
				if (!name->producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (definition_input(state, i, name->producer)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			}
		}
		enqueue(synthesis, job);
		return;
	}
	if (state->activated < state->count) {
		size_t i = state->activated++;
		const struct pg_syntax_item *item = &state->syntax->items[i];
		if (item->operation != PG_TOKEN_EXPECT) {
			struct pg_synthesis_job *producer = state->entries[i];
			if (!producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (producer->role == DEFINITION_JOB && !producer->stage) {
				producer->stage = 1;
				if (!producer->dependency) enqueue(synthesis, producer);
			}
		} else {
			struct pg_synthesis_job *term = lookup_scope(state->scope, item->name).producer;
			if (!term) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			struct pg_synthesis_job *type = pg_synthesis_request(synthesis, state->scope, item->expression);
			if (!type) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_synthesis_job *check = pg_synthesis_source_expect(synthesis, state->scope, term, type);
			if (!check) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (definition_input(state, i, check)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		}
		enqueue(synthesis, job);
		return;
	}
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void definitions_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
	int selected = syntax->kind == PG_SYNTAX_QUALIFIED;
	if (selected) syntax = syntax->left;
	if (!job->right) {
		if (!prepare_module(synthesis, job)) finish(synthesis, job, PG_SYNTHESIS_ERROR);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) {
		finish(synthesis, job, job->left->status); return;
	}
	struct definition_state *state = job->right->definitions;
	if (selected) {
		if (!job->stage) {
			job->value_job = lookup_definition(state, job->syntax->right->token);
			if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			/* Selection cannot hide an invalid or pending unselected entry. */
			job->left = pg_synthesis_request(synthesis, job->scope, syntax);
			job->stage = 1;
			depend(synthesis, job, job->left);
			return;
		}
		job->result = job->value_job->result;
		job->exports = job->value_job->exports;
	} else if (state->next < state->count) {
		job->left = state->entries[state->next++];
		depend(synthesis, job, job->left);
		return;
	}
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void reindex_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->reindex) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (struct pg_synthesis_job *)job->inputs[i];
			if (await_dependency(synthesis, job, input)) return;
		}
		const struct pg_evidence *substitution = ((const struct pg_synthesis_job *)job->inputs[0])->result;
		const struct pg_evidence *proof = ((const struct pg_synthesis_job *)job->inputs[1])->result;
		if (!reindex_inputs(synthesis, substitution, proof)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		/* All producer paths converge on the accepted evidence tuple before
		 * allocating traversal state. Distinct producers may prove the same map. */
		struct pg_synthesis_job *canonical = pg_synthesis_reindex(synthesis, substitution, proof);
		if (forward_proof(synthesis, job, canonical)) return;
		job->reindex = pg_occurrence_action_request(synthesis->typing,
			pg_evidence_context_map(substitution), pg_evidence_subject(proof));
		if (!job->reindex) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	switch (pg_occurrence_action_advance(job->reindex, 1)) {
	case PG_SUBSTITUTION_PENDING:
		enqueue(synthesis, job);
		return;
	case PG_SUBSTITUTION_ERROR:
		finish(synthesis, job, PG_SYNTHESIS_ERROR);
		break;
	case PG_SUBSTITUTION_DONE:
		job->result = pg_prove_reindex(synthesis->typing,
			((const struct pg_synthesis_job *)job->inputs[0])->result,
			((const struct pg_synthesis_job *)job->inputs[1])->result);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		break;
	}
}

static void pair_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->checking_term = job->inputs[2];
		job->left = pg_synthesis_reindex(synthesis, job->inputs[0], pg_evidence_premise(job->inputs[1], 1));
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	job->checking_type = job->left->result;
	const struct pg_evidence *checked = compare(synthesis, job);
	if (!checked) return;
	job->result = pg_prove_substitution_pair(synthesis->typing, job->inputs[0], job->inputs[1], checked);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void constructor_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		if (job->syntax->kind == PG_SYNTAX_LAMBDA) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		job->left = pg_synthesis_telescope(synthesis, job->scope, job->syntax);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	struct pg_synthesis_job *indices = (struct pg_synthesis_job *)job->inputs[1];
	if (await_dependency(synthesis, job, indices)) return;
	if (!job->right) {
		job->right = pg_synthesis_data_result(synthesis, pg_synthesis_telescope_scope(job->left),
			source_context(job->scope), indices->result, pg_synthesis_telescope_body(job->left));
		depend(synthesis, job, job->right);
		return;
	}
	job->result = job->right->result;
	finish(synthesis, job, job->right->status);
}

/* Recomputed field annotations can have fresh binders inside their types.
 * Keep the saved declaration immutable, and transport the independently
 * synthesized result through a checked variable substitution instead. */
static const struct pg_evidence *schema_result_context(struct pg_typing *typing,
	const struct pg_evidence *result, const struct pg_context *target)
{
	const struct pg_evidence *source = pg_evidence_premise(result, 1);
	if (pg_evidence_context(source) == target) return result;
	return pg_prove_substitution_compose(typing, result,
		pg_prove_telescope_correspondence(typing, source, pg_prove_context_alpha(typing, source, target)));
}

/* Compile direct typed index projections once per constructor. These paths
 * describe source argument insertion; they never authorize a kernel rule. */
static enum pg_synthesis_status constructor_index_paths(struct pg_synthesis *synthesis,
	const struct pg_context *prefix, const struct pg_context *fields,
	struct source_constructor *member)
{
	if (!member->implicit_count) return PG_SYNTHESIS_DONE;
	size_t count;
	if (pg_context_extension_size(fields, prefix, &count) || member->implicit_count > count)
		return PG_SYNTHESIS_ERROR;
	if (count > SIZE_MAX / sizeof(const struct pg_context *) ||
		member->implicit_count > SIZE_MAX / sizeof(struct constructor_index_path)) return PG_SYNTHESIS_ERROR;
	struct pg_graph scratch = {0};
	const struct pg_context **ordered = pg_alloc(&scratch, count * sizeof(*ordered));
	struct constructor_index_path *paths = pg_alloc(synthesis->typing->graph,
		member->implicit_count * sizeof(*paths));
	if (!ordered || !paths) { pg_graph_destroy(&scratch); return PG_SYNTHESIS_ERROR; }
	for (size_t i = count; i; --i, fields = fields->parent) ordered[i - 1] = fields;
	for (size_t i = 0; i < member->implicit_count; ++i)
		paths[i] = (struct constructor_index_path){.binder = ordered[i]->binder, .field = SIZE_MAX};
	for (size_t field = member->implicit_count; field < count; ++field) {
		const struct pg_term *type = ordered[field]->declared_type, *head = type;
		size_t arity = 0, index_count;
		while (head->kind == PG_APPLICATION) { ++arity; head = head->as.application.function; }
		if (head->kind != PG_REFERENCE) continue;
		const struct pg_data_declaration *family = pg_data_declaration_view(head->as.reference);
		if (family) {
			if (pg_context_extension_size(pg_data_declaration_indices(family),
				pg_data_declaration_parameters(family), &index_count)) continue;
		} else if (prefix && prefix->indices && head->as.reference == prefix->binder) {
			if (pg_context_extension_size(prefix->indices, prefix->parent, &index_count)) continue;
		} else continue;
		if (index_count > arity) continue;
		for (size_t offset = 0; offset < index_count; ++offset, type = type->as.application.function) {
			const struct pg_term *argument = type->as.application.argument;
			if (argument->kind != PG_REFERENCE) continue;
			for (size_t i = 0; i < member->implicit_count; ++i) {
				if (paths[i].binder != argument->as.reference || paths[i].field != SIZE_MAX) continue;
				paths[i].field = field;
				paths[i].index = index_count - offset - 1;
			}
		}
	}
	enum pg_synthesis_status status = PG_SYNTHESIS_DONE;
	for (size_t i = 0; i < member->implicit_count; ++i)
		if (paths[i].field == SIZE_MAX) status = PG_SYNTHESIS_REJECTED;
	pg_graph_destroy(&scratch);
	member->indices = paths;
	return status;
}

int pg_synthesis_visit_rejected_index_paths(const struct pg_synthesis *synthesis,
	int (*visit)(void *, struct pg_token, struct pg_token, size_t, size_t), void *owner)
{
	if (!synthesis || !visit) return -1;
	for (size_t bucket = 0; bucket < synthesis->jobs.capacity; ++bucket)
		for (const struct pg_index_entry *entry = synthesis->jobs.buckets[bucket]; entry; entry = entry->next) {
			const struct pg_synthesis_job *job = (const void *)entry;
			if (job->role != DATA_SCHEMA_JOB || job->status != PG_SYNTHESIS_REJECTED) continue;
			const struct declaration_state *state = job->declaration;
			if (!state || state->checked == state->constructors->item_count) continue;
			const struct source_constructor *member = &state->members[state->checked];
			if (!member->indices) continue;
			const struct pg_syntax *telescope = member->telescope;
			for (size_t i = 0; i < member->implicit_count; ++i, telescope = telescope->right) {
				const struct constructor_index_path *path = &member->indices[i];
				size_t field = path->field == SIZE_MAX ? SIZE_MAX : path->field - member->implicit_count;
				int status = visit(owner, state->constructors->items[state->checked].name,
					telescope->token, field, path->index);
				if (status) return status;
			}
		}
	return 0;
}

static void data_schema_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->left = job->nominal_input ? pg_synthesis_telescope_at(synthesis, job->scope, job->syntax->left,
			pg_data_declaration_parameters(job->nominal_input), pg_data_declaration_indices(job->nominal_input))
			: pg_synthesis_telescope(synthesis, job->scope, job->syntax->left);
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->right = pg_synthesis_telescope_structure(synthesis, job->scope, job->syntax->left);
		depend(synthesis, job, job->right);
		return;
	}
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	if (!job->declaration) {
		const struct pg_syntax *constructors = pg_synthesis_telescope_body(job->right);
		if (constructors->kind != PG_SYNTAX_CONSTRUCTORS) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		struct declaration_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
		if (!state) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->declaration = state;
		state->constructors = constructors;
		if (pg_index_init(&state->names) != 0) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		size_t count = constructors->item_count;
		if (count > SIZE_MAX / sizeof(*state->members)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		state->members = pg_alloc(synthesis->typing->graph, count * sizeof(*state->members));
		if (count && !state->members) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct declaration_state *state = job->declaration;
	if (state->indexed < state->constructors->item_count) {
		size_t i = state->indexed++;
		const struct pg_syntax_item *item = &state->constructors->items[i];
		struct source_constructor *member = &state->members[i];
		member->telescope = pg_syntax_constructor_telescope(synthesis->typing->graph,
			job->syntax, i, &member->implicit_count);
		if (!member->telescope) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->nominal_input) {
			const struct pg_context *prefix = pg_data_declaration_parameters(job->nominal_input);
			if (i >= pg_data_layout_count(pg_data_declaration_layout(job->nominal_input))) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			if (!pg_synthesis_telescope_at(synthesis, job->scope, member->telescope, prefix,
				pg_data_declaration_fields(job->nominal_input, i))) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
		}
		int status = register_name(synthesis, &state->names, item->name);
		if (status) { finish(synthesis, job, status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
		const void *inputs[] = {job->scope, job->left, member->telescope};
		struct pg_synthesis_job *producer = request_inputs(synthesis, CONSTRUCTOR_JOB, 3, inputs);
		if (!producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		producer->scope = job->scope;
		producer->syntax = member->telescope;
		member->producer = producer;
		enqueue(synthesis, job);
		return;
	}
	if (state->checked == state->constructors->item_count) {
		if (await_dependency(synthesis, job, job->left)) return;
		const struct pg_data_signature *signature = pg_data_signature(synthesis->typing, source_context(job->scope), job->left->result);
		if (!signature) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_graph temporary = {0};
		const struct pg_evidence **results = NULL;
		if (state->checked <= SIZE_MAX / sizeof(*results))
			results = pg_alloc(&temporary, state->checked * sizeof(*results));
		if (state->checked && !results) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		for (size_t i = 0; i < state->checked; ++i) {
			results[i] = state->members[i].producer->result;
			if (job->nominal_input) results[i] = schema_result_context(synthesis->typing, results[i],
				pg_data_declaration_fields(job->nominal_input, i));
		}
		job->schema = job->nominal_input
			? pg_data_schema_check(synthesis->typing, job->nominal_input, signature, state->checked, results)
			: pg_data_schema(synthesis->typing, signature, state->checked, results);
		pg_graph_destroy(&temporary);
		finish(synthesis, job, job->schema ? PG_SYNTHESIS_DONE
			: job->nominal_input ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
		return;
	}
	struct source_constructor *member = &state->members[state->checked];
	struct pg_synthesis_job *producer = member->producer;
	if (await_dependency(synthesis, job, producer)) return;
	enum pg_synthesis_status recovery = constructor_index_paths(synthesis,
		pg_evidence_context(source_context(job->scope)),
		pg_evidence_context(pg_evidence_premise(producer->result, 1)), member);
	if (recovery != PG_SYNTHESIS_DONE) { finish(synthesis, job, recovery); return; }
	++state->checked;
	enqueue(synthesis, job);
}

static void constructor_value_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *formation_job = (void *)job->inputs[0], *parameter_job = (void *)job->inputs[2];
	const struct pg_object *constructor = job->inputs[1];
	if (await_dependency(synthesis, job, job->left)) return;
	const struct pg_evidence *formation = formation_job->result, *parameters = parameter_job->result;
	if (!job->callable) {
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(formation));
		size_t ordinal;
		const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
		if (origin && origin->constructors && pg_data_constructor_position(layout, constructor, &ordinal)) {
			const struct source_constructor *source = &origin->constructors[ordinal];
			if (source->implicit_count) {
				if (source->implicit_count > (SIZE_MAX - sizeof(struct constructor_callable)) / sizeof(size_t)) goto error;
				struct constructor_callable *callable = pg_alloc(synthesis->typing->graph,
					sizeof(*callable) + source->implicit_count * sizeof(*callable->indices));
				if (!callable) goto error;
				*callable = (struct constructor_callable){.source = source,
					.field = source->implicit_count, .count = source->implicit_count};
				for (size_t i = 0; i < callable->count; ++i) callable->indices[i] = i;
				job->callable = callable;
			}
		}
	}
	const struct pg_evidence *map = job->left->result, *context = pg_evidence_premise(map, 1);
	size_t prefix = pg_evidence_premise_count(parameters) - 2;
	if (!job->right) {
		if (prefix > SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
		struct pg_synthesis_job **images = malloc(prefix * sizeof(*images));
		if (prefix && !images) goto error;
		for (size_t i = 0; i < prefix; ++i) images[i] = pg_synthesis_evidence(synthesis, pg_evidence_premise(map, i + 2));
		job->right = pg_synthesis_substitution(synthesis, pg_evidence_premise(parameters, 0), context, prefix, images);
		free(images);
		if (!job->right) goto error;
	}
	if (await_dependency(synthesis, job, job->right)) return;
	if (!job->value_job) {
		size_t count = pg_evidence_premise_count(map) - prefix - 3;
		const struct pg_evidence *const *fields = pg_evidence_premises(map) + prefix + 3;
		const struct pg_evidence *body = pg_prove_constructor(synthesis->typing, formation, constructor, job->right->result, count, fields);
		if (!body) goto error;
		if (!count) { job->result = body; finish(synthesis, job, PG_SYNTHESIS_DONE); return; }
		job->value_job = pg_synthesis_abstract(synthesis, pg_evidence_premise(parameters, 1), context, pg_synthesis_evidence(synthesis, body));
		if (!job->value_job) goto error;
	}
	/* Nullary constructors are values; field-bearing ones are raw functions. */
	forward_proof(synthesis, job, job->value_job);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static int declaration_member_valid(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index)
{
	return synthesis && job && job->owner == synthesis->owner_key
		&& job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_DECLARATION
		&& pg_syntax_constructors(job->syntax) && index < pg_syntax_constructors(job->syntax)->item_count;
}

int pg_synthesis_declaration_member_at(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t index, const struct pg_constructor_allocation *input)
{
	if (!declaration_member_valid(synthesis, job, index) || !input || !input->constructor) return -1;
	size_t fields;
	if (pg_context_extension_size(input->fields, input->prefix, &fields)) return -1;
	if (job->member_allocations && job->member_allocations[index].constructor) {
		const struct pg_constructor_allocation *previous = &job->member_allocations[index];
		return previous->constructor == input->constructor && previous->prefix == input->prefix
			&& previous->fields == input->fields ? 0 : -1;
	}
	if (job->exports || job->status != PG_SYNTHESIS_PENDING) return -1;
	if (!job->member_allocations) {
		size_t count = pg_syntax_constructors(job->syntax)->item_count;
		if (count > SIZE_MAX / sizeof(*job->member_allocations)) return -1;
		job->member_allocations = pg_alloc(synthesis->typing->graph, count * sizeof(*job->member_allocations));
		if (!job->member_allocations) return -1;
	}
	job->member_allocations[index] = *input;
	return 0;
}

int pg_synthesis_declaration_member_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index, struct pg_constructor_allocation *input)
{
	if (!declaration_member_valid(synthesis, job, index) || !input) return -1;
	if (job->member_allocations && job->member_allocations[index].constructor) {
		*input = job->member_allocations[index];
		return 1;
	}
	struct source_reference member = lookup_scope(job->exports, pg_syntax_constructors(job->syntax)->items[index].name);
	struct pg_constructor_input constructor;
	if (pg_synthesis_constructor_input(synthesis, member.producer, &constructor) || !constructor.allocated) return 0;
	*input = (struct pg_constructor_allocation){constructor.constructor, constructor.prefix, constructor.fields};
	return 1;
}

/* Each universe candidate owns a distinct conditional Self context. Raising
 * the bound never mutates an accepted assumption or publishes a provisional
 * family. Failure to construct a candidate is not universe inconsistency. */
static void declaration_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	int indexed = job->syntax->left->kind != PG_SYNTAX_CONSTRUCTORS;
	if (indexed) {
		if (!job->right && job->nominal_input) {
			const struct pg_context *self = pg_data_declaration_parameters(job->nominal_input);
			const struct pg_context *prefix = pg_evidence_context(source_context(job->scope));
			if (!self || self->parent != prefix || !self->indices) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			job->right = pg_synthesis_telescope_at(synthesis, job->scope, job->syntax->left, prefix, self->indices);
		} else if (!job->right) job->right = pg_synthesis_telescope(synthesis, job->scope, job->syntax->left);
		if (await_dependency(synthesis, job, job->right)) return;
	}
	if (!job->domain)
		job->domain = pg_prove_universe(synthesis->typing, source_context(job->scope), 0);
	if (!job->left) {
		const struct pg_data_declaration *allocation = NULL;
		if (job->nominal_input) {
			const struct pg_context *parameters = pg_data_declaration_parameters(job->nominal_input);
			uint64_t candidate, stored;
			const struct pg_term *tail = parameters ? parameters->declared_type : NULL, *domain, *body;
			const struct pg_object *binder;
			if (indexed) while (pg_pi_view(tail, &domain, &binder, &body)) tail = body;
			if (!parameters || parameters->parent != pg_evidence_context(source_context(job->scope))
				|| !pg_universe_level(tail, &stored)
				|| !job->domain || !pg_universe_level(pg_evidence_subject(job->domain)->core, &candidate)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			if (candidate == stored) allocation = job->nominal_input;
			job->binder = allocation ? parameters->binder : pg_binder(synthesis->typing->graph);
		} else job->binder = pg_binder(synthesis->typing->graph);
		const struct pg_evidence *self = indexed
			? pg_prove_family_context_extension(synthesis->typing, source_context(job->scope), job->binder,
				job->right->result, pg_prove_projection(synthesis->typing, job->right->result, job->domain))
			: pg_prove_context_extension(synthesis->typing, source_context(job->scope), job->binder, job->domain);
		job->inner = pg_synthesis_bind(synthesis, job->scope, (struct pg_token){.kind = '*'}, job->binder, self);
		job->left = pg_synthesis_data_schema_at(synthesis, job->inner, job->syntax, allocation);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) {
		finish(synthesis, job, job->left->status); return;
	}
	const struct pg_data_schema *schema = pg_synthesis_schema_result(job->left);
	uint64_t candidate, bound;
	if (!job->domain || !pg_universe_level(pg_evidence_subject(job->domain)->core, &candidate)) {
		finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
	}
	/* Schema storage is conditional; logical-family fields do not yet have
	 * a value-field universe/positivity rule and cannot be admitted. */
	if (pg_data_schema_field_level(schema, &bound)) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (bound > candidate) {
		job->domain = pg_prove_universe(synthesis->typing, source_context(job->scope), bound);
		if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->left = NULL;
		enqueue(synthesis, job);
		return;
	}
	if (job->nominal_input && pg_data_schema_declaration(schema) != job->nominal_input) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	job->result = pg_prove_inductive_type(synthesis->typing, schema);
	if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	job->schema = schema;
	/* Export only declared members, never fall back to enclosing lexical names. */
	job->exports = intern_scope(synthesis, (struct pg_source_scope){.context_job = job->scope->context_job});
	const struct pg_syntax *constructors = indexed ? pg_synthesis_telescope_body(job->right) : job->syntax->left;
	for (size_t i = 0; job->exports && i < constructors->item_count; ++i) {
		const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing,
			source_context(job->scope), source_context(job->scope));
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(schema), i);
		const struct pg_constructor_allocation *allocation = job->member_allocations ? &job->member_allocations[i] : NULL;
		if (allocation && allocation->constructor) {
			if (allocation->constructor != constructor ||
				!pg_synthesis_constructor_scope_at(synthesis, pg_synthesis_evidence(synthesis, job->result),
					constructor, pg_synthesis_evidence(synthesis, parameters), allocation->prefix, allocation->fields)) {
				job->exports = NULL;
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
		}
		struct pg_synthesis_job *member = pg_synthesis_constructor_value(synthesis,
			job->result, constructor, parameters);
		job->exports = pg_synthesis_name_job(synthesis, job->exports, constructors->items[i].name, member);
	}
	struct source_metadata *origin = register_source_metadata(synthesis, pg_evidence_subject(job->result));
	if (!origin) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	origin->exports = job->exports;
	origin->constructors = job->left->declaration->members;
	finish(synthesis, job, job->exports ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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
	const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(formation));
	const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
	if (!pg_data_constructor_position(layout, constructor, &ordinal)) return PG_SYNTHESIS_REJECTED;
	if (origin && origin->constructors) hidden = origin->constructors[ordinal].implicit_count;
	if (hidden > count) return PG_SYNTHESIS_ERROR;
	if (clause && !named && count - hidden != clause->item_count) return PG_SYNTHESIS_REJECTED;
	if (count > SIZE_MAX / sizeof(struct source_case_field)) return PG_SYNTHESIS_ERROR;
	struct source_case_field *bindings = calloc(count, sizeof(*bindings));
	if (count && !bindings) return PG_SYNTHESIS_ERROR;
	enum pg_synthesis_status status = PG_SYNTHESIS_REJECTED;
	if (named) {
		struct pg_synthesis_job *graph = origin ? origin->graph : NULL;
		if (!graph || !graph->case_layouts) { status = PG_SYNTHESIS_UNSUPPORTED; goto done; }
		const struct source_case_layout *source = &graph->case_layouts[ordinal];
		if (source->count != count) goto done;
		for (size_t i = 0; i < count; ++i) bindings[i].graph_value = source->fields[i].graph_value;
		for (size_t i = 0; i < clause->item_count; ++i) {
			const struct pg_syntax_item *item = &clause->items[i];
			if (item->operation != PG_TOKEN_ASSIGN || !item->expression || item->expression->kind != PG_SYNTAX_ATOM) goto done;
			struct pg_token alias = item->expression->token;
			if (alias.kind != PG_TOKEN_IDENT) goto done;
			for (size_t j = 0; j < i; ++j)
				if (same_name(alias, clause->items[j].expression->token)) goto done;
			size_t selected = count;
			for (size_t field = 0; field < count; ++field) {
				if (source->fields[field].name.kind != PG_TOKEN_IDENT || !same_name(source->fields[field].name, item->name)) continue;
				if (selected != count) goto done;
				selected = field;
			}
			if (selected == count) {
				struct pg_synthesis_job *origin = (void *)graph->inputs[1];
				const struct pg_syntax *body = origin->match->branches[ordinal].clause->right;
				const struct pg_syntax *block = block_syntax(body);
				/* A derived block result is not the result of a nested call.
				 * Its naming requires a checked expression projection, not an alias. */
				if (block) for (size_t j = 0; j < block_end(body); ++j)
					if (block->items[j].name.kind == PG_TOKEN_IDENT && same_name(block->items[j].name, item->name))
						status = PG_SYNTHESIS_UNSUPPORTED;
				goto done;
			}
			if (bindings[selected].name.kind) goto done;
			bindings[selected].name = alias;
		}
	} else if (clause) for (size_t i = 0; i < count - hidden; ++i) {
		if (clause->items[i].operation) goto done;
		bindings[i + hidden].name = clause->items[i].name;
	}
	const struct pg_source_scope *scope = parent;
	for (size_t i = 0; scope && i < count; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		const struct pg_object *value = NULL;
		if (bindings[i].graph_value) {
			if (bindings[i].graph_value > i) goto done;
			value = pg_evidence_context(extensions[bindings[i].graph_value - 1])->binder;
		}
		scope = pg_synthesis_bind(synthesis, scope, bindings[i].name, binder, extensions[i]);
		if (scope && value) {
			struct pg_source_scope associated = *scope;
			associated.association = PG_SOURCE_GRAPH;
			associated.associated_binder = value;
			scope = intern_scope(synthesis, associated);
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

static void induction_branch_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		const struct pg_evidence *formation = job->inputs[1], *parameters = job->inputs[3];
		if (!job->right) job->right = pg_synthesis_induction_scope(synthesis,
			pg_synthesis_evidence(synthesis, formation), job->inputs[2], pg_synthesis_evidence(synthesis, parameters),
			pg_synthesis_evidence(synthesis, job->inputs[4]), pg_synthesis_evidence(synthesis, job->inputs[5]));
		if (!job->right) goto error;
		if (await_dependency(synthesis, job, job->right)) return;
		const struct pg_evidence *map = job->right->result;
		size_t fields = pg_evidence_premise_count(map) - pg_evidence_premise_count(parameters) - 1, total;
		const struct pg_evidence *context = pg_evidence_premise(map, 1);
		if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(parameters), &total)) goto error;
		if (total > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		struct pg_graph temporary = {0};
		const struct pg_evidence **extensions = pg_alloc(&temporary, total * sizeof(*extensions));
		unsigned char *recursive = pg_alloc(&temporary, fields);
		if ((total && !extensions) || (fields && !recursive)) { pg_graph_destroy(&temporary); goto error; }
		for (size_t i = total; i; --i, context = pg_evidence_premise(context, 0)) extensions[i - 1] = context;
		const struct pg_source_scope *scope = job->scope;
		enum pg_synthesis_status status = case_field_scope(synthesis, scope, formation, job->inputs[2],
			job->syntax, fields, extensions, &scope);
		if (status != PG_SYNTHESIS_DONE) { pg_graph_destroy(&temporary); finish(synthesis, job, status); return; }
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
				pg_evidence_context(extensions[next])->binder, pg_synthesis_evidence(synthesis, extensions[next]));
			++next;
		}
		if (next != total) scope = NULL;
		if (scope && job->inputs[7]) {
			if (scope->context_job->status == PG_SYNTHESIS_PENDING) {
				pg_graph_destroy(&temporary); depend(synthesis, job, scope->context_job); return;
			}
			if (scope->context_job->status != PG_SYNTHESIS_DONE) {
				pg_graph_destroy(&temporary); finish(synthesis, job, scope->context_job->status); return;
			}
			const struct pg_evidence *pattern = constructor_pattern(synthesis, formation, parameters,
				job->inputs[4], job->inputs[2], fields ? extensions[fields - 1] : context, fields,
				source_context(scope));
			scope = generalized_scope(synthesis, job->scope, scope, job->inputs[7], pattern);
		}
		pg_graph_destroy(&temporary);
		if (!scope) goto error;
		job->inner = scope;
		job->left = pg_synthesis_request(synthesis, scope, job->syntax->right);
		if (!job->left) goto error;
		enqueue(synthesis, job);
		return;
	}
	if (!job->value_job) job->value_job = pg_synthesis_abstract(synthesis,
		source_context(job->scope), source_context(job->inner), job->left);
	if (!job->value_job) goto error;
	forward_proof(synthesis, job, job->value_job);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void constant_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *destination = job->inputs[0];
	if (!job->left) {
		const struct pg_evidence *fields = job->inputs[1];
		size_t count;
		if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(destination), &count)) goto unsupported;
		job->left = pg_synthesis_abstract(synthesis, destination, fields, (void *)job->inputs[2]);
		job->right = pg_synthesis_constant_result(synthesis, pg_synthesis_evidence(synthesis, destination), job->left, count);
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (await_dependency(synthesis, job, job->left)) return;
	job->function = job->left->result;
	if (job->right->status == PG_SYNTHESIS_REJECTED) goto unsupported;
	forward_proof(synthesis, job, job->right);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static int return_clause(const struct pg_syntax *clause)
{
	if (!clause || !clause->left) return 0;
	const struct pg_syntax *head = clause->left;
	if (head->kind != PG_SYNTAX_QUALIFIED) return 0;
	if (!head->left || !head->right) return 0;
	if (head->left->kind != PG_SYNTAX_ATOM) return 0;
	struct pg_token root = head->left->token, name = head->right->token;
	return root.kind == '#' &&
		name.length == 6 && !memcmp(name.text, "return", 6);
}

static int handler_syntax(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_ELIMINATION || !syntax->item_count) return 0;
	for (size_t i = 0; i < syntax->item_count; ++i)
		if (return_clause(syntax->items[i].expression)) return 1;
	return 0;
}

static void handler_return_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *clause = job->syntax;
	if (!return_clause(clause)) goto rejected;
	if (clause->item_count != 1) goto rejected;
	if (clause->items[0].operation) goto rejected;
	if (!job->inner) {
		if (!job->binder) job->binder = source_binder(synthesis, job->scope, clause, 0, NULL);
		if (!job->binder) goto error;
		struct pg_synthesis_job *context = pg_synthesis_result_context(synthesis,
			job->scope->context_job, job->left, job->binder);
		if (!context) goto error;
		job->inner = pg_synthesis_bind_context(synthesis, job->scope, clause->items[0].name,
			job->binder, context);
		if (!job->inner) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->right = pg_synthesis_request(synthesis, job->inner, clause->right);
		job->value_job = pg_synthesis_lambda_body(synthesis,
			context, job->right);
	}
	if (!job->value_job) goto error;
	if (await_dependency(synthesis, job, job->value_job)) return;
	job->result = job->value_job->result;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void return_handler_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->value_job) {
		job->left = pg_synthesis_request(synthesis, job->scope, job->syntax->left);
		job->right = pg_synthesis_handler_return(synthesis, job->scope, job->left,
			job->syntax->items[0].expression);
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->value_job = pg_synthesis_sequence(synthesis, job->scope->context_job,
			job->right->left, job->right);
	}
	forward_proof(synthesis, job, job->value_job);
}

static struct pg_synthesis_job *plain_rule(struct pg_synthesis *synthesis,
	enum pg_evidence_rule rule, const struct pg_object *binder, size_t count,
	struct pg_synthesis_job *const *premises)
{
	struct pg_derivation_input input = {.rule = rule, .parameters.binder = binder, .count = count};
	return pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
}

static struct pg_synthesis_job *handler_rule(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *body, struct pg_synthesis_job *returned,
	struct pg_synthesis_job *carrier, size_t count, const struct source_handler_clause *clauses)
{
	body = request_job(synthesis, BODY_JOB, body, NULL);
	if (!count) {
		struct pg_synthesis_job *fold = plain_rule(synthesis, PG_FOLD_ELIM, NULL, 2,
			(struct pg_synthesis_job *[]){body, returned});
		return plain_rule(synthesis, PG_EFFECT_SUBSUMPTION, NULL, 2,
			(struct pg_synthesis_job *[]){fold, carrier});
	}
	if (count > (SIZE_MAX / sizeof(struct pg_synthesis_job *) - 3) / 3) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job **premises = pg_alloc(&temporary, (3 + 3 * count) * sizeof(*premises));
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	struct pg_synthesis_job *result = NULL;
	if (!premises || !labels) goto done;
	premises[0] = body; premises[1] = returned; premises[2] = carrier;
	for (size_t i = 0; i < count; ++i) {
		struct pg_operation_input signature;
		if (pg_synthesis_operation_input(synthesis, clauses[i].operation, &signature)) goto done;
		labels[i] = signature.label;
		premises[3 + 3 * i] = signature.payload;
		premises[4 + 3 * i] = signature.response;
		premises[5 + 3 * i] = clauses[i].body;
	}
	struct pg_derivation_input input = {.rule = PG_HANDLER_ELIM, .count = 3 + 3 * count,
		.parameters.handler = pg_handler_signature(synthesis->typing->graph, count, labels)};
	if (input.parameters.handler) result = pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

static void operation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		for (size_t i = 1; i < 3; ++i) {
			struct pg_synthesis_job *signature = (void *)job->inputs[i];
			if (await_dependency(synthesis, job, signature)) return;
		}
		const struct pg_operation_declaration *operation = pg_operation_declaration_at(synthesis->typing,
			job->inputs[0], pg_synthesis_result(job->inputs[1]), pg_synthesis_result(job->inputs[2]));
		if (!operation) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		job->operation = operation;
		struct pg_synthesis_job *payload = pg_synthesis_evidence(synthesis, pg_operation_payload_type(operation));
		struct pg_synthesis_job *response = pg_synthesis_evidence(synthesis, pg_operation_response_type(operation));
		struct pg_synthesis_job *empty = plain_rule(synthesis, PG_CONTEXT_EMPTY, NULL, 0, NULL);
		if (!job->context_allocation) {
			const struct pg_context *allocation = pg_context_bind(synthesis->typing, NULL,
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_payload_type(operation))->core, PG_JUDGEMENT_VALUE);
			if (allocation) allocation = pg_context_bind(synthesis->typing, allocation,
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_response_type(operation))->core, PG_JUDGEMENT_VALUE);
			if (!allocation || context_allocation_at(synthesis, job, NULL, allocation, 0)) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
		}
		const struct pg_object *a = job->context_allocation->contexts[0]->binder;
		const struct pg_object *b = job->context_allocation->contexts[1]->binder;
		struct pg_synthesis_job *scope = plain_rule(synthesis, PG_CONTEXT_EXTEND, a, 2,
			(struct pg_synthesis_job *[]){empty, payload});
		struct pg_synthesis_job *domain = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){scope, response});
		struct pg_synthesis_job *response_scope = plain_rule(synthesis, PG_CONTEXT_EXTEND, b, 2,
			(struct pg_synthesis_job *[]){scope, domain});
		struct pg_synthesis_job *value = plain_rule(synthesis, PG_VARIABLE, b, 1, &response_scope);
		struct pg_derivation_input return_rule = {.rule = PG_RETURN_INTRO, .count = 1, .parameters.totality = PG_TOTALITY_TOTAL};
		struct pg_synthesis_job *returned = pg_synthesis_rule(synthesis, &return_rule, &value, NULL, NULL);
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, response_scope, returned);
		struct pg_synthesis_job *argument = plain_rule(synthesis, PG_VARIABLE, a, 1, &scope);
		struct pg_derivation_input input = {.rule = PG_REQUEST_INTRO, .count = 4, .parameters.operation_label = pg_operation_label(operation)};
		struct pg_synthesis_job *request = pg_synthesis_rule(synthesis, &input,
			(struct pg_synthesis_job *[]){payload, response, argument, continuation}, NULL, NULL);
		job->left = pg_synthesis_lambda_body(synthesis, scope, request);
	}
	forward_proof(synthesis, job, job->left);
}

struct pg_synthesis_job *pg_synthesis_handler_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *context, struct pg_synthesis_job *carrier,
	struct pg_synthesis_job *payload_type, struct pg_synthesis_job *response_type, const struct pg_object *payload,
	const struct pg_object *resume, const struct pg_object *response)
{
	if (!synthesis) return NULL;
	struct pg_synthesis_job *inputs[] = {context, carrier, payload_type, response_type};
	for (size_t i = 0; i < 4; ++i)
		if (!inputs[i] || inputs[i]->owner != synthesis->owner_key) return NULL;
	const struct pg_object *binders[] = {payload, resume, response};
	for (size_t i = 0; i < 3; ++i)
		if (!binders[i] || binders[i]->kind != PG_BINDER) return NULL;
	struct pg_synthesis_job *a = payload_type, *b = response_type;
	b = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, b});
	struct pg_synthesis_job *response_context = plain_rule(synthesis, PG_CONTEXT_EXTEND,
		response, 2, (struct pg_synthesis_job *[]){context, b});
	struct pg_synthesis_job *codomain = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
		(struct pg_synthesis_job *[]){response_context, carrier});
	struct pg_synthesis_job *pi = plain_rule(synthesis, PG_PI_FORM, NULL, 2,
		(struct pg_synthesis_job *[]){response_context, codomain});
	struct pg_synthesis_job *delayed = plain_rule(synthesis, PG_THUNK_TYPE_FORM, NULL, 1, &pi);
	a = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, a});
	struct pg_synthesis_job *payload_context = plain_rule(synthesis, PG_CONTEXT_EXTEND, payload, 2,
		(struct pg_synthesis_job *[]){context, a});
	delayed = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
		(struct pg_synthesis_job *[]){payload_context, delayed});
	return plain_rule(synthesis, PG_CONTEXT_EXTEND, resume, 2,
		(struct pg_synthesis_job *[]){payload_context, delayed});
}

static int prepare_handler_clause(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_handler_clause_input *input)
{
	const struct pg_syntax *clause = job->syntax;
	if (clause->item_count != 2 || return_clause(clause)) return PG_SYNTHESIS_REJECTED;
	if (clause->items[0].operation || clause->items[1].operation) return PG_SYNTHESIS_REJECTED;
	struct pg_operation_input signature;
	if (!input || pg_synthesis_operation_input(synthesis, input->operation, &signature)) return PG_SYNTHESIS_REJECTED;
	if (!input->payload || input->payload->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (!input->resume || input->resume->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (!input->response || input->response->kind != PG_BINDER) return PG_SYNTHESIS_REJECTED;
	if (job->handler_clause) {
		const struct pg_handler_clause_input *previous = job->handler_clause;
		if (previous->operation != input->operation || previous->payload != input->payload ||
			previous->resume != input->resume || previous->response != input->response) return PG_SYNTHESIS_REJECTED;
		if (job->inner) return 0;
	} else {
		if (job->status != PG_SYNTHESIS_PENDING) return PG_SYNTHESIS_REJECTED;
		job->handler_clause = pg_alloc(synthesis->typing->graph, sizeof(*input));
		if (!job->handler_clause) return PG_SYNTHESIS_ERROR;
		*job->handler_clause = *input;
	}
	struct pg_synthesis_job *context = pg_synthesis_handler_context(synthesis, job->scope->context_job,
		(void *)job->inputs[1], signature.payload, signature.response, input->payload, input->resume, input->response);
	if (!context) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis_job *payload_context = rule_premise(synthesis, context, 0);
	const struct pg_source_scope *scope = bind_context(synthesis, job->scope,
		clause->items[0].name, input->payload, payload_context, NULL, job, PG_SOURCE_UNASSOCIATED);
	job->inner = bind_context(synthesis, scope, clause->items[1].name, input->resume, context, NULL, job, PG_SOURCE_UNASSOCIATED);
	if (!job->inner) return PG_SYNTHESIS_ERROR;
	job->right = pg_synthesis_request(synthesis, job->inner, clause->right);
	struct pg_synthesis_job *inner_lambda = pg_synthesis_lambda_body(synthesis, context, job->right);
	job->value_job = pg_synthesis_lambda_body(synthesis, payload_context, inner_lambda);
	return job->value_job ? 0 : PG_SYNTHESIS_ERROR;
}

struct pg_synthesis_job *pg_synthesis_handler_clause_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *carrier,
	const struct pg_syntax *clause, const struct pg_handler_clause_input *input)
{
	struct pg_synthesis_job *job = pg_synthesis_handler_clause(synthesis, scope, carrier, clause);
	return job && !prepare_handler_clause(synthesis, job, input) ? job : NULL;
}

const struct pg_source_scope *pg_synthesis_handler_clause_scope(const struct pg_synthesis_job *job)
{
	return job && job->role == HANDLER_CLAUSE_JOB ? job->inner : NULL;
}

int pg_synthesis_handler_clause_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_handler_clause_input *input)
{
	if (!synthesis || !job || !input || job->owner != synthesis->owner_key) return -1;
	if (job->role != HANDLER_CLAUSE_JOB || !job->handler_clause) return -1;
	*input = *job->handler_clause;
	return 0;
}

static void handler_clause_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *clause = job->syntax;
	if (clause->item_count != 2 || return_clause(clause)) goto rejected;
	if (clause->items[0].operation || clause->items[1].operation) goto rejected;
	if (!job->left) {
		job->left = pg_synthesis_operation_reference(synthesis,
			pg_synthesis_request(synthesis, job->scope, clause->left));
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	{
		struct pg_synthesis_job *operation = (void *)operation_reference_origin(job->left);
		if (!operation) {
			if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
			finish(synthesis, job, job->left->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : job->left->status); return;
		}
		struct pg_handler_clause_input input = {.operation = operation};
		if (job->handler_clause) {
			input = *job->handler_clause;
			input.operation = operation;
		} else {
			input.payload = source_binder(synthesis, job->scope, clause, 0, NULL);
			input.resume = source_binder(synthesis, job->scope, clause, 1, NULL);
			input.response = source_binder(synthesis, job->scope, clause, 2, NULL);
		}
		if (!input.payload || !input.resume || !input.response) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		int status = prepare_handler_clause(synthesis, job, &input);
		if (status) { finish(synthesis, job, status); return; }
	}
	if (await_dependency(synthesis, job, job->value_job)) return;
	if (await_dependency(synthesis, job, job->left)) return;
	const struct pg_effect_row *effects;
	const struct pg_term *value;
	if (!carrier->result || pg_evidence_judgement(carrier->result) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
	if (pg_evidence_context(carrier->result) != pg_evidence_context(source_context(job->scope))) goto rejected;
	if (!pg_effect_type_view(pg_evidence_subject(carrier->result)->core, &effects, &value)) goto rejected;
	job->result = job->value_job->result;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static int prepare_handler(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int independent)
{
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	size_t count = job->syntax->item_count;
	if (job->handler) return !independent || job->handler->effect_owner == job->handler ? 0 : -1;
	if (count > (SIZE_MAX - sizeof(*job->handler)) / sizeof(struct source_handler_clause)) return -1;
	struct handler_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state) + count * sizeof(struct source_handler_clause));
	if (!state) return -1;
	state->source = job;
	state->scope = job->scope;
	if (!carrier) {
		state->labels = pg_alloc(synthesis->typing->graph, count * sizeof(*state->labels));
		if (!state->labels) return -1;
		struct handler_state *owner = job->scope->effect_owner;
		if (independent || !owner || owner->effects.sealed) {
			owner = state;
			if (pg_effect_inference_init(&owner->effects, synthesis->typing->graph)) return -1;
		}
		state->effect_owner = owner;
		if (job->scope->effect_owner != owner)
			state->scope = intern_scope(synthesis, (struct pg_source_scope){
				.parent = job->scope, .context_job = job->scope->context_job, .effect_owner = owner});
		if (!state->scope) {
			if (owner == state) pg_effect_inference_destroy(&state->effects);
			return -1;
		}
		++owner->registering;
	}
	job->handler = state;
	return 0;
}

const struct pg_source_scope *pg_synthesis_handler_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *job = pg_synthesis_handler(synthesis, parent, NULL, syntax);
	if (!job || prepare_handler(synthesis, job, 1)) return NULL;
	return job->handler->scope;
}

/* Reserve dependencies, not answers. The return body and effect equation are
 * the same producers subsequently advanced by the ordinary handler worker. */
static int prepare_handler_carrier(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct handler_state *state = job->handler;
	if (!job->right) {
		size_t selected = SIZE_MAX;
		for (size_t i = 0; i < job->syntax->item_count; ++i) {
			if (!return_clause(job->syntax->items[i].expression)) continue;
			if (selected != SIZE_MAX) return PG_SYNTHESIS_REJECTED;
			selected = i;
		}
		if (selected == SIZE_MAX) return PG_SYNTHESIS_REJECTED;
		job->left = pg_synthesis_request(synthesis, state->scope, job->syntax->left);
		if (!job->left) return PG_SYNTHESIS_ERROR;
		job->right = pg_synthesis_handler_return(synthesis, state->scope, job->left,
			job->syntax->items[selected].expression);
		if (!job->right) return PG_SYNTHESIS_ERROR;
	}
	if (!job->inputs[1] && !state->carrier) {
		struct handler_state *owner = state->effect_owner;
		const struct pg_effect_row *empty = pg_effect_row(synthesis->typing->graph, 0, NULL);
		if (!state->equation) state->equation = pg_effect_equation(&owner->effects, empty);
		if (!state->equation) return PG_SYNTHESIS_ERROR;
		state->carrier = pg_synthesis_handler_carrier(synthesis, state->scope->context_job, job->right,
			&owner->effects, state->equation);
		if (!state->carrier) return PG_SYNTHESIS_ERROR;
	}
	return 0;
}

struct pg_synthesis_job *pg_synthesis_source_handler_carrier(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *job = pg_synthesis_handler(synthesis, parent, NULL, syntax);
	if (!job || prepare_handler(synthesis, job, 0) || prepare_handler_carrier(synthesis, job)) return NULL;
	return job->handler->carrier;
}

static struct pg_synthesis_job *source_handler_clause_job(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *handler, struct pg_synthesis_job *carrier, const struct pg_syntax *syntax)
{
	struct pg_synthesis_job *clause = pg_synthesis_handler_clause(synthesis, handler->handler->scope, carrier, syntax);
	if (!clause || (clause->handler_owner && clause->handler_owner != handler)) return NULL;
	clause->handler_owner = handler;
	return clause;
}

int pg_synthesis_handler_binding_input(const struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_handler_binding_input *input)
{
	if (!synthesis || !scope || !input || scope->owner != synthesis->owner_key) return -1;
	const struct pg_synthesis_job *clause = scope->clause;
	if (!clause || !clause->handler_owner || clause->handler_owner->inputs[1]) return 0;
	const struct pg_synthesis_job *handler = clause->handler_owner;
	if (!clause->inner || !clause->handler_clause) return -1;
	unsigned slot;
	if (scope == clause->inner) slot = 1;
	else if (scope == clause->inner->parent) slot = 0;
	else return -1;
	*input = (struct pg_handler_binding_input){.parent = handler->scope, .handler = handler->syntax,
		.clause = clause->syntax, .allocation = *clause->handler_clause, .slot = slot};
	return 1;
}

const struct pg_source_scope *pg_synthesis_restore_handler_binding(struct pg_synthesis *synthesis,
	const struct pg_handler_binding_input *input)
{
	if (!input || input->slot > 1 || !input->handler || !input->clause) return NULL;
	if (input->handler->kind != PG_SYNTAX_ELIMINATION) return NULL;
	size_t index;
	for (index = 0; index < input->handler->item_count; ++index)
		if (input->handler->items[index].expression == input->clause) break;
	if (index == input->handler->item_count || return_clause(input->clause)) return NULL;
	struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(synthesis, input->parent, input->handler);
	if (!carrier) return NULL;
	struct pg_synthesis_job *handler = pg_synthesis_handler(synthesis, input->parent, NULL, input->handler);
	struct pg_synthesis_job *clause = source_handler_clause_job(synthesis, handler, carrier, input->clause);
	if (!clause) return NULL;
	if (prepare_handler_clause(synthesis, clause, &input->allocation)) return NULL;
	return input->slot ? clause->inner : clause->inner->parent;
}

static void handler_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	size_t count = job->syntax->item_count;
	if (prepare_handler(synthesis, job, 0)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	int preparation = prepare_handler_carrier(synthesis, job);
	if (preparation) { finish(synthesis, job, preparation); return; }
	struct handler_state *state = job->handler;
	const struct pg_source_scope *scope = state->scope;
	struct handler_state *owner = state->effect_owner;
	if (owner && owner->failure) { finish(synthesis, job, owner->failure); return; }
	if (state->scanned < count) {
		const struct pg_syntax *clause = job->syntax->items[state->scanned].expression;
		if (!return_clause(clause)) {
			struct pg_synthesis_job *operation = pg_synthesis_operation_reference(synthesis,
				pg_synthesis_request(synthesis, scope, clause->left));
			if (!operation) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_operation_input signature;
			const struct pg_synthesis_job *origin = operation_reference_origin(operation);
			if (pg_synthesis_operation_input(synthesis, origin, &signature)) {
				if (operation->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, operation); return; }
				finish(synthesis, job, operation->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : operation->status);
				return;
			}
			struct pg_synthesis_job *body = source_handler_clause_job(synthesis, job,
				carrier ? carrier : state->carrier, clause);
			if (!body) { finish(synthesis, job, carrier ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_REJECTED); return; }
			if (!carrier) state->labels[state->count] = signature.label;
			state->clauses[state->count++] = (struct source_handler_clause){origin, body};
		}
		++state->scanned;
		enqueue(synthesis, job);
		return;
	}
	if (!job->right) goto rejected;
	if (!carrier) {
		const struct pg_effect_row *empty = pg_effect_row(synthesis->typing->graph, 0, NULL);
		if (!state->handled) {
			state->handled = pg_effect_row(synthesis->typing->graph, state->count, state->labels);
			if (!state->handled) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (pg_effect_count(state->handled) != state->count) goto rejected;
		}
		carrier = state->carrier;
		if (state->collected < state->count + 2) {
			if (!state->collection) {
				struct pg_synthesis_job *body;
				size_t parameters;
				if (!state->collected) {
					body = request_job(synthesis, BODY_JOB, job->left, NULL);
					parameters = 0;
				} else if (state->collected == 1) { body = job->right; parameters = 1; }
				else {
					body = state->clauses[state->collected - 2].body;
					parameters = 2;
				}
				struct pg_synthesis_job *formation = pg_synthesis_constant_result(synthesis, scope->context_job, body, parameters);
				state->collection = pg_synthesis_effect_contribution(synthesis, &owner->effects, state->equation,
					state->collected ? empty : state->handled, formation);
			}
			if (await_dependency(synthesis, job, state->collection)) return;
			state->collection = NULL;
			++state->collected;
			enqueue(synthesis, job);
			return;
		}
		if (!state->registered) {
			state->registered = 1;
			if (!--owner->registering) {
				pg_effect_inference_seal(&owner->effects);
				if (!pg_synthesis_effect_inference(synthesis, &owner->effects)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			}
		}
	}
	if (!job->value_job) job->value_job = handler_rule(synthesis, job->left, job->right,
		carrier, state->count, state->clauses);
	forward_proof(synthesis, job, job->value_job);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static struct pg_synthesis_job *projected_image(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *image)
{
	struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, image)};
	struct pg_derivation_input input = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
	return pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
}

/* Source binders belong to the branch's typed Lambda edges, not the separate
 * recursive-erasure allocation. Ordinary Solve still checks the source body. */
static int source_match_branch_context(const struct pg_synthesis_job *job,
	size_t ordinal, size_t count, const struct pg_context **context)
{
	const struct pg_match_allocation *allocation = job->match_allocation;
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
	struct match_state *state = job->match;
	if (!state->generic_context) {
		const struct pg_evidence *context;
		if (job->match_allocation) {
			context = pg_prove_inductive_motive_context_at(synthesis->typing,
				state->instance.formation, state->instance.parameters, job->match_allocation->motive);
		} else context = pg_prove_inductive_motive_context(synthesis->typing, state->instance.formation,
				state->instance.parameters, pg_binder(synthesis->typing->graph));
		if (!context) return NULL;
		if (!pg_inductive_motive_context_valid(synthesis->typing,
			state->instance.formation, state->instance.parameters, context)) return NULL;
		state->generic_context = context;
	}
	return state->generic_context;
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
	struct match_state *state = job->match;
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
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->result_checked];
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *prefix = source_context(job->inner);
	if (!branch->needs_ih || state->packet) goto skip;
	if (!branch->result) {
		branch->result = pg_alloc(typing->graph, sizeof(*branch->result));
		if (!branch->result) goto error;
		*branch->result = (struct motive_result){.scope = branch->scope, .syntax = branch->clause->right,
			.effects = pg_effect_row(typing->graph, 0, NULL), .totality = PG_TOTALITY_TOTAL};
		if (!branch->result->effects) goto error;
	}
	struct motive_result *result = branch->result;
	struct pg_synthesis_job *context_job = result->scope->context_job;
	if (context_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context_job); return; }
	if (context_job->status != PG_SYNTHESIS_DONE) goto skip;
	const struct pg_evidence *context = context_job->result;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	const struct pg_term *content;
	const struct pg_syntax *syntax = result->syntax;
	const struct pg_evidence *type = NULL;
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		result->syntax = syntax->left;
		enqueue(synthesis, job); return;
	}
	if (syntax->kind == PG_SYNTAX_LAMBDA) {
		if (!result->telescope) result->telescope = pg_synthesis_telescope(synthesis, result->scope, syntax);
		if (!result->telescope) goto error;
		if (result->telescope->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->telescope); return; }
		if (result->telescope->status != PG_SYNTHESIS_DONE) goto skip;
		struct motive_lambda *frame = pg_alloc(typing->graph, sizeof(*frame));
		if (!frame) goto error;
		*frame = (struct motive_lambda){context, result->telescope->result, result->effects, result->totality, result->lambdas};
		result->lambdas = frame;
		result->scope = result->telescope->inner;
		result->syntax = result->telescope->tail;
		result->effects = pg_effect_row(typing->graph, 0, NULL);
		result->totality = PG_TOTALITY_TOTAL;
		result->telescope = NULL;
		if (!result->effects) goto error;
		enqueue(synthesis, job); return;
	}
	if (block_syntax(syntax)) {
		size_t end = syntax->kind == PG_SYNTAX_QUALIFIED ? block_end(syntax) : syntax->item_count;
		if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
		if (!end) goto skip;
		const struct pg_syntax_item *item = &syntax->items[result->next];
		if (result->next + 1 == end) {
			result->syntax = item->expression;
			result->next = 0;
			enqueue(synthesis, job); return;
		}
		if (!result->input) {
			int recursive = branch_needs_ih(result->scope, pg_evidence_context(prefix), item->expression);
			if (recursive < 0) goto error;
			if (recursive) goto skip;
			struct pg_synthesis_job *input = pg_synthesis_request(synthesis, result->scope, item->expression);
			if (item->name.length && prepare_value_argument(synthesis, job, context_job, &input)) return;
			input = request_job(synthesis, BODY_JOB, input, NULL);
			result->input = pg_synthesis_normalize_classifier_jobs(synthesis, context_job, input);
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
			struct pg_synthesis_job *bound = pg_synthesis_result_context(synthesis, context_job, result->input, binder);
			result->scope = pg_synthesis_bind_context(synthesis, result->scope, item->name, binder, bound);
			if (!result->scope) goto error;
		}
		result->input = NULL;
		++result->next;
		enqueue(synthesis, job); return;
	}
	if (syntax->kind == PG_SYNTAX_ELIMINATION && !handler_syntax(syntax)) {
		if (!result->nested) result->nested = pg_synthesis_request(synthesis, result->scope, syntax);
		if (!result->nested) goto error;
		if (result->nested->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, result->nested); return; }
		if (result->nested->status == PG_SYNTHESIS_ERROR) goto error;
		struct match_state *nested = result->nested->match;
		if (!nested || nested->next != nested->count || result->nested->match_frame) goto skip;
		if (result->nested_checked == nested->count) goto skip;
		struct match_branch *part = &nested->branches[result->nested_checked];
		if (!part->body || part->body->status != PG_SYNTHESIS_DONE) {
			++result->nested_checked;
			enqueue(synthesis, job); return;
		}
		const struct pg_evidence *fields = source_context(part->scope);
		struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, part->body);
		struct pg_synthesis_job *formation = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
			pg_synthesis_evidence(synthesis, fields), body);
		if (!formation) goto error;
		if (formation->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, formation); return; }
		if (formation->status == PG_SYNTHESIS_ERROR) goto error;
		if (formation->status == PG_SYNTHESIS_DONE) {
			/* A nested case proposes a family at its scrutinee, not a result
			 * pinned to this constructor. The complete branches still check it. */
			const struct pg_evidence *pattern = match_constructor_pattern(synthesis,
				result->nested, result->nested_checked, fields);
			type = pg_prove_pattern_type(typing, context, pattern, formation->result);
			const struct pg_evidence *actual = pg_prove_inductive_motive_substitution(typing,
				nested->instance.formation, nested->instance.parameters,
				match_motive_context(synthesis, result->nested), context, result->nested->checking_term);
			type = pg_prove_reindex(typing, actual, type);
		}
		if (!type) {
			++result->nested_checked;
			enqueue(synthesis, job); return;
		}
		goto result_type;
	}
	if (syntax->kind != PG_SYNTAX_APPLICATION || hypothesis_syntax(syntax)) goto skip;
	if (!result->callee) {
		int recursive = branch_needs_ih(result->scope, pg_evidence_context(prefix), syntax->left);
		if (recursive < 0) goto error;
		if (recursive) goto skip;
		struct pg_synthesis_job *callee = pg_synthesis_request(synthesis, result->scope, syntax->left);
		result->callee = pg_synthesis_normalize_classifier_jobs(synthesis, context_job, callee);
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
			context = pg_evidence_premise(context, 0);
		}
		while (context && pg_evidence_context(context) != pg_evidence_context(frame->outer)) {
			type = pg_prove_pi(typing, context, type);
			context = pg_evidence_premise(context, 0);
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
		for (size_t i = state->generalized_count; candidate && i; --i, extension = pg_evidence_premise(extension, 0))
			candidate = pg_prove_pi(typing, extension, candidate);
	}
	if (candidate) {
		state->motive = candidate;
		state->motive_context = motive_context;
	}
skip:
	++state->result_checked;
	enqueue(synthesis, job); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static struct pg_synthesis_job *constructor_callable_source(struct pg_synthesis_job *producer);

static void match_demand_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->demanded];
	struct motive_demand *demand = branch->demands;
	if (!demand) { ++state->demanded; enqueue(synthesis, job); return; }
	struct pg_synthesis_job *scope = demand->scope->context_job;
	if (await_dependency(synthesis, job, scope)) return;
	const struct pg_evidence *context = scope->result;
	if (!demand->normalized) {
		int recursive = branch_needs_ih(demand->scope, pg_evidence_context(source_context(job->inner)), demand->callee->syntax);
		if (recursive < 0) goto error;
		if (recursive) goto skip;
		if (await_dependency(synthesis, job, demand->callee)) return;
		/* An unresolved implicit prefix is not the written argument's domain.
		 * Let branch equations propose the motive; the complete constructor
		 * call then recovers indices from the checked IH result type. */
		struct pg_synthesis_job *origin = constructor_callable_source(demand->callee);
		if (origin && origin->callable) goto skip;
		const struct pg_evidence *callee = demand->callee->result;
		if (pg_evidence_judgement(callee) == PG_JUDGEMENT_VALUE) callee = pg_prove_force(synthesis->typing, callee);
		if (!callee) goto skip;
		demand->normalized = pg_synthesis_normalize_classifier(synthesis, context, callee);
		if (!demand->normalized) goto error;
	}
	if (await_dependency(synthesis, job, demand->normalized)) return;
	const struct pg_term *argument_type, *result_type;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_classifier(demand->normalized->result), &argument_type, &binder, &result_type)) goto skip;
	if (!demand->domain) demand->domain = application_domain(synthesis,
		pg_synthesis_evidence(synthesis, context), demand->normalized);
	if (!demand->domain) goto error;
	if (await_dependency(synthesis, job, demand->domain)) return;
	if (!demand->solution) {
		for (size_t i = 0; i < demand->count; ++i) {
			struct pg_synthesis_job *argument = demand->arguments[i];
			int recursive = branch_needs_ih(demand->scope, pg_evidence_context(source_context(job->inner)), argument->syntax);
			if (recursive < 0) goto error;
			if (recursive) goto skip;
			if (argument->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, argument); return; }
			if (argument->status != PG_SYNTHESIS_DONE) goto skip;
			if (pg_evidence_judgement(argument->result) != PG_JUDGEMENT_VALUE) goto skip;
		}
		const struct pg_evidence *domain = demand->domain->result;
		const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
		if (!motive_context) goto skip;
		const struct pg_evidence *field = pg_prove_variable(synthesis->typing, context, demand->field);
		const struct pg_evidence *pattern = pg_prove_inductive_motive_substitution(synthesis->typing,
			state->instance.formation, state->instance.parameters,
			motive_context, context, field);
		if (!pattern) goto skip;
		const struct pg_evidence *prefix = source_context(job->inner);
		for (size_t i = 0; i < demand->count; ++i) {
			const struct pg_evidence *argument = demand->arguments[i]->result;
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
		for (size_t i = demand->count; demand->solution && i; --i, extension = pg_evidence_premise(extension, 0))
			demand->solution = pg_prove_pi(synthesis->typing, extension, demand->solution);
		if (!demand->solution) goto skip;
		if (!state->motive) {
			state->motive = demand->solution;
			state->motive_context = motive_context;
		}
	}
	struct pg_synthesis_job *comparison = request_job(synthesis, CONVERSION_JOB,
		pg_evidence_subject(state->motive)->core, pg_evidence_subject(demand->solution)->core);
	if (!comparison) goto error;
	if (await_dependency(synthesis, job, comparison)) return;
skip:
	branch->demands = demand->next;
	enqueue(synthesis, job);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* An APP domain constrains the IH's value result, not its effects. Seed the
 * candidate row from independently synthesized branches. Full branch checking
 * still has to establish that the candidate covers every recursive branch. */
static void match_effects_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->effect_checked];
	if (!state->motive_effects) state->motive_effects = pg_effect_row(synthesis->typing->graph, 0, NULL);
	if (!state->motive_effects) goto error;
	if (!branch->needs_ih) {
		const struct pg_evidence *context = source_context(branch->scope);
		struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, context, context, branch->body);
		if (!body) goto error;
		if (await_dependency(synthesis, job, body)) return;
		const struct pg_effect_row *effects;
		const struct pg_term *result;
		enum pg_totality totality;
		if (pg_computation_type_view(pg_evidence_classifier(body->result), &totality, &effects, &result)) {
			state->motive_effects = pg_effect_union(synthesis->typing->graph, state->motive_effects, effects);
			if (totality < state->motive_totality) state->motive_totality = totality;
			if (!state->motive_effects) goto error;
		}
	}
	++state->effect_checked;
	enqueue(synthesis, job);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
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
	struct match_state *state = job->match;
	struct pg_typing *typing = synthesis->typing;
	struct match_branch *branch = &state->branches[ordinal];
	const struct pg_evidence *fields = source_context(branch->scope);
	/* Copied ambient inputs remain motive arguments; hidden paths may be
	 * removed only by checked constant-codomain formation. */
	while (type && pg_evidence_context(fields) != pg_evidence_context(branch->fields)) {
		if (!state->generalization && !state->path_context) break;
		type = pg_prove_pi(typing, fields, type);
		if (state->path_context) type = pg_prove_pi_constant_codomain(typing, type);
		fields = pg_evidence_premise(fields, 0);
	}
	const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, ordinal, fields);
	return pg_prove_pattern_type(typing, source_context(job->inner), pattern, type);
}

static struct pg_synthesis_job *match_induction_scope(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t ordinal, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive)
{
	struct match_state *state = job->match;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal);
	struct pg_synthesis_job *formation = pg_synthesis_evidence(synthesis, state->instance.formation);
	struct pg_synthesis_job *parameters = pg_synthesis_evidence(synthesis, state->instance.parameters);
	struct pg_synthesis_job *context = pg_synthesis_evidence(synthesis, motive_context);
	struct pg_synthesis_job *type = pg_synthesis_evidence(synthesis, motive);
	if (!job->match_allocation)
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
	struct match_state *state = job->match;
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), ordinal);
	return pg_synthesis_induction_branch(synthesis, job->inner,
		state->instance.formation, constructor, state->instance.parameters,
		motive_context, motive, state->generalization, state->branches[ordinal].clause);
}

/* A base equation can suggest M(xs), not merely the constant M(nil).
 * Every suggested family is checked against complete induction branches;
 * unsuccessful proposals never replace the ordinary constant-motive path. */
static void match_recursive_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	if (!mc) goto done;
	if (!state->candidate) {
		if (state->candidate_next == state->count) goto done;
		struct match_branch *branch = &state->branches[state->candidate_next];
		if (branch->needs_ih || branch->contradiction) goto next;
		const struct pg_evidence *fields = source_context(branch->scope);
		if (!branch->type_job) {
			struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
			branch->type_job = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
				pg_synthesis_evidence(synthesis, fields), body);
		}
		if (!branch->type_job) goto error;
		if (branch->type_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->type_job); return; }
		if (branch->type_job->status == PG_SYNTHESIS_ERROR) goto error;
		if (branch->type_job->status != PG_SYNTHESIS_DONE) goto next;
		const struct pg_evidence *candidate = match_candidate_type(synthesis, job,
			state->candidate_next, branch->type_job->result);
		if (!candidate) goto next;
		/* Indexed motives may depend on a family index without mentioning the
		 * scrutinee itself. Test the complete motive telescope, not its tail. */
		const struct pg_context *prefix = pg_evidence_context(source_context(job->inner));
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
		enqueue(synthesis, job); return;
	}
	if (state->candidate_checked == state->count) {
		state->motive = state->candidate;
		state->motive_context = mc;
		state->checked = state->prepared = state->validated = state->count;
		for (size_t i = 0; i < state->count; ++i) {
			struct match_branch *branch = &state->branches[i];
			branch->function = branch->converted->result;
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
		branch->converted = pg_synthesis_expect(synthesis, branch->adapted, pg_synthesis_evidence(synthesis, expected));
	}
	if (!branch->converted) goto error;
	if (branch->converted->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->converted); return; }
	if (branch->converted->status == PG_SYNTHESIS_ERROR) goto error;
	if (branch->converted->status != PG_SYNTHESIS_DONE) goto next;
	++state->candidate_checked;
	enqueue(synthesis, job); return;
next:
	state->candidate = NULL;
	for (size_t i = 0; i < state->count; ++i) state->branches[i].adapted = state->branches[i].converted = NULL;
	++state->candidate_next;
	enqueue(synthesis, job); return;
done:
	state->recursive_motive_done = 1;
	state->candidate = NULL; state->candidate_next = state->candidate_checked = 0;
	enqueue(synthesis, job); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* A branch proposes a family; it does not establish the other branch
 * equations. Check every reachable body before committing to that motive. */
static void match_candidate_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = source_context(job->inner);
	if (!state->candidate) {
		struct match_branch *branch = &state->branches[state->candidate_next];
		if (!branch->contradiction) {
			const struct pg_evidence *type = match_result_type(synthesis, state, branch->type_job->result);
			if (!branch->refined) state->candidate = match_candidate_type(synthesis, job, state->candidate_next, type);
			if (!state->candidate && state->path_context) {
				if (!branch->refined) {
					const struct pg_evidence *fields = source_context(branch->scope);
					struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
					struct pg_synthesis_job *quoted = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &body);
					struct pg_synthesis_job *goal = pg_synthesis_evidence(synthesis, context);
					if (!quoted || !goal) goto error;
					const void *inputs[] = {branch->scope->context_job, quoted, goal};
					branch->refined = request_inputs(synthesis, INDEX_RESULT_JOB, 3, inputs);
				}
				if (!branch->refined) goto error;
				if (branch->refined->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->refined); return; }
				if (branch->refined->status == PG_SYNTHESIS_ERROR) goto error;
				if (branch->refined->status == PG_SYNTHESIS_DONE) {
					type = pg_prove_classifier(typing, source_context(branch->scope), branch->refined->result);
					type = match_result_type(synthesis, state, pg_prove_thunk_content(typing, type));
					state->candidate = match_candidate_type(synthesis, job, state->candidate_next, type);
				}
			}
		}
		++state->candidate_next;
		enqueue(synthesis, job); return;
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
		enqueue(synthesis, job); return;
	}
	struct match_branch *branch = &state->branches[state->candidate_checked];
	if (branch->contradiction) {
		++state->candidate_checked;
		enqueue(synthesis, job); return;
	}
	if (!branch->converted) {
		const struct pg_evidence *body_context = source_context(branch->scope);
		const struct pg_evidence *fields = state->generalization ? branch->fields : body_context;
		const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, state->candidate_checked, fields);
		const struct pg_evidence *target = pg_prove_reindex(typing, pattern, state->candidate);
		if (!target) goto next_candidate;
		struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, body_context, branch->body);
		if (state->path_context) {
			/* Transport U(C), then force, without running C during synthesis. */
			target = pg_prove_thunk_type(typing, target);
			body = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &body);
			const void *inputs[] = {branch->scope->context_job, body, pg_synthesis_evidence(synthesis, target)};
			body = request_inputs(synthesis, INDEX_TRANSPORT_JOB, 3, inputs);
			body = plain_rule(synthesis, PG_FORCE_ELIM, NULL, 1, &body);
		} else body = pg_synthesis_expect(synthesis, body, pg_synthesis_evidence(synthesis, target));
		branch->converted = pg_synthesis_abstract(synthesis, context, fields, body);
		if (!branch->converted) goto error;
	}
	if (branch->converted->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->converted); return; }
	if (branch->converted->status == PG_SYNTHESIS_ERROR) goto error;
	if (branch->converted->status != PG_SYNTHESIS_DONE) goto next_candidate;
	branch->function = branch->converted->result;
	++state->candidate_checked;
	enqueue(synthesis, job); return;
next_candidate:
	state->candidate = NULL; state->candidate_checked = 0;
	for (size_t i = 0; i < state->count; ++i) state->branches[i].converted = NULL;
	enqueue(synthesis, job); return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_dependent_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	const struct pg_evidence *context = source_context(job->inner);
	if (state->typed < state->count) {
		struct match_branch *branch = &state->branches[state->typed];
		if (branch->contradiction) { ++state->typed; enqueue(synthesis, job); return; }
		const struct pg_evidence *fields = source_context(branch->scope);
		if (!branch->type_job) {
			struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
			branch->type_job = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
				pg_synthesis_evidence(synthesis, fields), body);
			if (!branch->type_job) goto error;
		}
		if (branch->type_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->type_job); return; }
		if (branch->type_job->status != PG_SYNTHESIS_DONE) goto unsupported;
		const struct pg_effect_row *effects;
		const struct pg_term *value_type;
		enum pg_totality totality;
		if (pg_computation_type_view(pg_evidence_subject(branch->type_job->result)->core, &totality, &effects, &value_type)) {
			if (totality < state->motive_totality) state->motive_totality = totality;
			state->motive_effects = state->motive_effects
				? pg_effect_union(synthesis->typing->graph, state->motive_effects, effects) : effects;
			if (!state->motive_effects) goto error;
		}
		++state->typed;
		enqueue(synthesis, job);
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
		const struct pg_evidence *type = pg_prove_return_content(synthesis->typing, branch->type_job->result);
		for (const struct pg_evidence *scope = source_context(branch->scope);
			type && pg_evidence_context(scope) != pg_evidence_context(context); scope = pg_evidence_premise(scope, 0))
			type = pg_prove_family_abstraction(synthesis->typing, scope, type);
		families[i] = pg_prove_projection(synthesis->typing, mc, type);
	}
	const struct pg_evidence *scrutinee = pg_prove_variable(synthesis->typing, mc, pg_evidence_context(mc)->binder);
	const struct pg_evidence *type = pg_prove_type_case(synthesis->typing,
		state->instance.formation, parameters, scrutinee, state->count, families);
	free(families);
	state->motive = pg_prove_computation_type(synthesis->typing,
		state->motive_totality, state->motive_effects, type);
	if (!state->motive) goto unsupported;
	state->motive_context = mc;
	enqueue(synthesis, job);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_dependent_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->prepared];
	if (!branch->converted) {
		const struct pg_evidence *context = source_context(job->inner), *fields = source_context(branch->scope);
		struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
		const struct pg_evidence *result_type = match_result_type(synthesis, state, branch->type_job->result);
		body = pg_synthesis_expect(synthesis, body, pg_synthesis_evidence(synthesis, result_type));
		struct pg_synthesis_job *function = pg_synthesis_abstract(synthesis, context, fields, body);
		branch->converted = function;
		if (!branch->converted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (await_dependency(synthesis, job, branch->converted)) return;
	branch->function = branch->converted->result;
	branch->converted = NULL;
	++state->prepared;
	enqueue(synthesis, job);
}

static void match_validate_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->validated];
	if (!branch->converted) {
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), state->validated);
		struct pg_synthesis_job *scope = state->induction
			? match_induction_scope(synthesis, job, state->validated, state->motive_context, state->motive)
			: pg_synthesis_constructor_scope(synthesis, pg_synthesis_evidence(synthesis, state->instance.formation),
				constructor, pg_synthesis_evidence(synthesis, state->instance.parameters));
		if (await_dependency(synthesis, job, scope)) return;
		const struct pg_evidence *expected = pg_prove_match_branch_type(synthesis->typing,
			state->instance.formation, constructor, state->instance.parameters, state->motive_context, state->motive, scope->result);
		if (!expected) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		branch->converted = pg_synthesis_expect(synthesis, pg_synthesis_evidence(synthesis, branch->function),
			pg_synthesis_evidence(synthesis, expected));
		if (!branch->converted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (await_dependency(synthesis, job, branch->converted)) return;
	branch->function = branch->converted->result;
	++state->validated;
	enqueue(synthesis, job);
}

/* Abstract the ambient telescope after distinct variable indices/scrutinee. The
 * specialization is a checked section: copied inputs are supplied once after
 * elimination, not silently retyped at each constructor. */
static int match_generalize(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = source_context(job->inner);
	const struct pg_term *subject = pg_evidence_subject(job->right->result)->core;
	struct match_generalization *work = state->generalizing;
	if (work) goto advance;
	if (state->packet) return 0;
	if (subject->kind != PG_REFERENCE || subject->as.reference->kind != PG_BINDER) return 0;
	/* The index substitution also contains the declaration's Self image. */
	size_t first = pg_evidence_premise_count(state->instance.parameters) + 1;
	size_t end = state->instance.indices ? pg_evidence_premise_count(state->instance.indices) : first;
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
		const struct pg_term *index = pg_evidence_subject(pg_evidence_premise(state->instance.indices, i))->core;
		if (index->kind != PG_REFERENCE || index->as.reference->kind != PG_BINDER) return 0;
		for (size_t j = first; j < i; ++j)
			if (pg_evidence_subject(pg_evidence_premise(state->instance.indices, j))->core == index) return 0;
	}
	/* Local definition blocks and handlers need their own scoped transport;
	 * do not leave their captured environment at the old indices. */
	for (const struct pg_source_scope *scope = job->inner; scope; scope = scope->parent) {
		if (scope->effect_owner) return 0;
		if (pg_evidence_context(source_context(scope))) {
			if (scope->definitions || scope->module) return 0;
		}
	}
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	struct pg_inductive_instance generic;
	if (!mc || !pg_inductive_instance(typing, pg_evidence_premise(mc, 1), &generic)) return 0;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), NULL, &count)) return 0;
	if (count > (SIZE_MAX - sizeof(*work)) / sizeof(*work->extensions)) goto error;
	work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->extensions));
	if (!work) goto error;
	work->motive_context = mc; work->generic_indices = generic.indices;
	work->count = count; work->first = first; work->end = end;
	const struct pg_evidence *extension = context;
	for (size_t i = count; i; --i, extension = pg_evidence_premise(extension, 0)) work->extensions[i - 1] = extension;
	work->map = pg_prove_substitution_projection(typing, extension, mc);
	work->back = pg_prove_inductive_motive_substitution(typing,
		state->instance.formation, state->instance.parameters, mc, context, job->right->result);
	if (!work->map || !work->back) return 0;
	state->generalizing = work;
advance:
	if (work->next == work->count) {
		state->generalization = work->map;
		state->specialization = work->back;
		state->generalized_count = work->copied;
		return 0;
	}
	if (!work->pair) {
		extension = work->extensions[work->next];
		const struct pg_object *binder = pg_evidence_context(extension)->binder;
		const struct pg_evidence *image = NULL;
		for (size_t j = work->first; j < work->end; ++j) {
			const struct pg_term *index = pg_evidence_subject(pg_evidence_premise(state->instance.indices, j))->core;
			if (index->as.reference == binder) {
				image = pg_prove_projection(typing, work->motive_context, pg_evidence_premise(work->generic_indices, j));
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
		if (!work->pair) return 0;
	}
	if (work->pair->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, work->pair); return 1; }
	if (work->pair->status == PG_SYNTHESIS_ERROR) goto error;
	if (work->pair->status != PG_SYNTHESIS_DONE) return 0;
	if (work->replacing) work->map = work->pair->result;
	else work->back = work->pair->result;
	work->pair = NULL;
	++work->next;
	enqueue(synthesis, job);
	return 1;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
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
	for (size_t i = count; i; --i, extension = pg_evidence_premise(extension, 0)) extensions[i - 1] = extension;
	for (size_t i = 0; pattern && i < count; ++i) {
		pattern = pg_prove_substitution_lift(typing, pattern, extensions[i], pg_binder(typing->graph));
		if (!pattern) break;
		const struct pg_evidence *destination = pg_evidence_premise(pattern, 1);
		const struct pg_source_scope *old = original;
		for (; old; old = old->parent) {
			if (!old->binder) continue;
			const struct pg_evidence *image = pg_substitution_image(typing, generalization, old->binder);
			const struct pg_term *term = image ? pg_evidence_subject(image)->core : NULL;
			if (term && term->kind == PG_REFERENCE && term->as.reference == pg_evidence_context(extensions[i])->binder) break;
		}
		scope = pg_synthesis_bind(synthesis, scope, old ? old->name : (struct pg_token){0},
			pg_evidence_context(destination)->binder, destination);
		if (scope && old && old->associated_binder) {
			const struct pg_evidence *generic = pg_substitution_image(typing, generalization, old->associated_binder);
			const struct pg_term *field = generic ? pg_evidence_subject(generic)->core : NULL;
			const struct pg_evidence *associated = field && field->kind == PG_REFERENCE
				? pg_substitution_image(typing, pattern, field->as.reference) : NULL;
			field = associated ? pg_evidence_subject(associated)->core : NULL;
			if (!field || field->kind != PG_REFERENCE || field->as.reference->kind != PG_BINDER) { scope = NULL; break; }
			struct pg_source_scope linked = *scope;
			linked.associated_binder = field->as.reference;
			linked.association = old->association; linked.clause = old->clause;
			scope = intern_scope(synthesis, linked);
		}
		if (!scope) break;
	}
	free(extensions);
	if (!scope || !pattern) return NULL;
	const struct pg_evidence *map = pg_prove_substitution_compose(typing, generalization, pattern);
	if (!map) return NULL;
	/* One checked map governs aliases, copied variables and their IH/graph
	 * associations. A constructor image is a value, not a conversion axiom. */
	for (const struct pg_source_scope *old = original; old; old = old->parent) {
		if (!old->name.length) continue;
		const struct pg_evidence *image;
		struct pg_synthesis_job *producer;
		if (old->binder) {
			if (lookup_scope(scope, old->name).binder != old->binder) continue;
			image = pg_substitution_image(typing, map, old->binder);
			if (!image) continue;
			const struct pg_term *term = pg_evidence_subject(image)->core;
			if (term->kind == PG_REFERENCE && term->as.reference == old->binder) continue;
			producer = pg_synthesis_evidence(synthesis, image);
		} else {
			if (!old->producer || !pg_evidence_context(source_context(old))) continue;
			if (source_name_target(lookup_scope(scope, old->name).producer) != source_name_target(old->producer)) continue;
			/* Context action applies to the eventual subject, not the producer's
			 * current completion state. Reuse the ordinary checking dependencies. */
			producer = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_job *[]){original->context_job, old->producer});
			producer = pg_synthesis_reindex_jobs(synthesis, pg_synthesis_evidence(synthesis, map), producer);
		}
		scope = pg_synthesis_name_job(synthesis, scope, old->name, producer);
		if (!scope) return NULL;
	}
	return scope;
}

/* A rigid index is not a unification assignment. Keep the equality between
 * the actual index and the constructor index as an explicit motive argument. */
static int match_index_context(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	if (state->path_context || state->induction || state->generalization || !state->instance.indices) return 0;
	size_t first = pg_evidence_premise_count(state->instance.parameters) + 1;
	size_t end = pg_evidence_premise_count(state->instance.indices);
	int rigid = 0;
	for (size_t i = first; i < end; ++i) {
		const struct pg_term *index = pg_evidence_subject(pg_evidence_premise(state->instance.indices, i))->core;
		if (index->kind != PG_REFERENCE || index->as.reference->kind != PG_BINDER) rigid = 1;
	}
	if (!rigid) return 0;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = source_context(job->inner);
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	struct pg_inductive_instance generic;
	if (!mc || !pg_inductive_instance(typing, pg_evidence_premise(mc, 1), &generic)) return -1;
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
	for (size_t i = count; i; --i, pc = pg_evidence_premise(pc, 0)) state->path_extensions[i - 1] = pc;
	return 0;
}

static int match_index_scope(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, size_t ordinal)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[ordinal];
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *pattern = match_constructor_pattern(synthesis, job, ordinal, branch->fields);
	for (size_t i = 0; pattern && i < state->path_count; ++i) {
		pattern = pg_prove_substitution_lift(typing, pattern, state->path_extensions[i], pg_binder(typing->graph));
		if (!pattern) return -1;
		const struct pg_evidence *context = pg_evidence_premise(pattern, 1);
		branch->scope = pg_synthesis_bind(synthesis, branch->scope, (struct pg_token){0},
			pg_evidence_context(context)->binder, context);
		if (!branch->scope) return -1;
	}
	if (!pattern || state->path_count > SIZE_MAX / sizeof(*branch->paths)) return -1;
	branch->pattern = pattern;
	branch->paths = pg_alloc(typing->graph, state->path_count * sizeof(*branch->paths));
	if (!branch->paths) return -1;
	const struct pg_evidence *context = source_context(branch->scope);
	struct pg_inductive_instance generic;
	if (!pg_inductive_instance(typing, pg_evidence_premise(state->generic_context, 1), &generic)) return -1;
	size_t first = pg_evidence_premise_count(state->instance.parameters) + 1;
	const struct pg_evidence *indices = pg_prove_substitution_compose(typing, generic.indices,
		pg_prove_substitution_projection(typing, pg_evidence_premise(generic.indices, 1), state->path_context));
	indices = pg_prove_substitution_compose(typing, indices, pattern);
	if (!indices) return -1;
	for (size_t i = 0; i < state->path_count; ++i) {
		struct match_index_path *path = &branch->paths[i];
		path->left = pg_prove_projection(typing, context, pg_evidence_premise(state->instance.indices, first + i));
		path->right = pg_evidence_premise(indices, first + i);
		path->path = pg_substitution_image(typing, pattern, pg_evidence_context(state->path_extensions[i])->binder);
		if (!path->left || !path->right || !path->path) return -1;
	}
	return 0;
}

/* Head comparison selects a possible refutation only. Acceptance still needs
 * a transport derivation using the corresponding, scoped Identity witness. */
static void match_index_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->path_scoped];
	if (!branch->paths && match_index_scope(synthesis, job, state->path_scoped)) goto unsupported;
	if (!branch->contradiction && branch->path_checked < state->path_count) {
		struct match_index_path *path = &branch->paths[branch->path_checked];
		const struct pg_evidence *ends[] = {path->left, path->right};
		const struct pg_object *heads[2] = {0};
		for (size_t i = 0; i < 2; ++i) {
			if (!path->normal[i]) path->normal[i] = pg_synthesis_normalize_jobs(synthesis,
				branch->scope->context_job, pg_synthesis_evidence(synthesis, ends[i]), PG_REDUCTION_WHNF);
			if (!path->normal[i]) goto error;
			if (await_dependency(synthesis, job, path->normal[i])) return;
			const struct pg_term *head = pg_evidence_subject(path->normal[i]->result)->core;
			while (head->kind == PG_APPLICATION) head = head->as.application.function;
			if (head->kind == PG_REFERENCE) heads[i] = head->as.reference;
		}
		struct pg_inductive_instance instance;
		const struct pg_evidence *type = pg_prove_classifier(synthesis->typing,
			source_context(branch->scope), path->normal[0]->result);
		if (heads[0] && heads[1] && heads[0] != heads[1] && pg_inductive_instance(synthesis->typing, type, &instance)) {
			size_t positions[2];
			const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
			if (pg_data_constructor_position(layout, heads[0], &positions[0]) &&
				pg_data_constructor_position(layout, heads[1], &positions[1])) branch->contradiction = path;
		}
		++branch->path_checked;
		enqueue(synthesis, job); return;
	}
	++state->path_scoped;
	enqueue(synthesis, job); return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_refuted_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->prepared];
	if (!branch->contradiction) { ++state->prepared; enqueue(synthesis, job); return; }
	if (!branch->adapted) {
		struct match_index_path *path = branch->contradiction;
		/* Ex falso at U(C), then force, works for every computation carrier,
		 * including raw Pi. The unreachable source body does not choose C. */
		const struct pg_evidence *type = pg_prove_reindex(synthesis->typing, branch->pattern, state->path_result);
		type = pg_prove_thunk_type(synthesis->typing, type);
		if (!type) goto unsupported;
		branch->body = pg_synthesis_disjoint_transport(synthesis, branch->scope->context_job,
			pg_synthesis_evidence(synthesis, path->left), pg_synthesis_evidence(synthesis, path->right),
			pg_synthesis_evidence(synthesis, path->path), pg_synthesis_evidence(synthesis, path->left),
			pg_synthesis_evidence(synthesis, type));
		branch->body = plain_rule(synthesis, PG_FORCE_ELIM, NULL, 1, &branch->body);
		branch->adapted = pg_synthesis_abstract(synthesis, source_context(job->inner),
			source_context(branch->scope), branch->body);
		if (!branch->adapted) goto error;
	}
	if (await_dependency(synthesis, job, branch->adapted)) return;
	branch->function = branch->adapted->result;
	++state->prepared;
	enqueue(synthesis, job); return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->result) goto complete;
	if (!job->left) {
		job->left = pg_synthesis_request(synthesis, job->scope, job->syntax->left);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (!job->inner) {
		job->checking_term = job->left->result;
		if (job->checking_term && pg_evidence_judgement(job->checking_term) == PG_JUDGEMENT_COMPUTATION) {
			if (!job->match_frame) {
				struct block_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
				if (!frame) goto error;
				*frame = (struct block_frame){.input = job->left, .binds = 1,
					.context = pg_synthesis_result_context(synthesis, job->scope->context_job,
						job->left, source_binder(synthesis, job->scope, job->syntax, 0, NULL))};
				if (!frame->context) goto error;
				job->match_frame = frame;
			}
			struct pg_synthesis_job *extension = job->match_frame->context;
			if (await_dependency(synthesis, job, extension)) return;
			const struct pg_object *binder = pg_evidence_context(extension->result)->binder;
			job->inner = pg_synthesis_bind(synthesis, job->scope, (struct pg_token){0}, binder, extension->result);
			if (!job->inner) goto error;
			job->checking_term = pg_prove_variable(synthesis->typing, extension->result, binder);
		} else job->inner = job->scope;
	}
	const struct pg_evidence *context = source_context(job->inner), *scrutinee = job->checking_term;
	if (!scrutinee || pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE) goto unsupported;
	if (!job->right) job->right = pg_synthesis_normalize_classifier(synthesis, context, scrutinee);
	if (!job->right) goto error;
	if (await_dependency(synthesis, job, job->right)) return;
	scrutinee = job->right->result;
	if (!job->match) {
		struct pg_synthesis_job *classifier = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
			pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, scrutinee));
		if (!classifier) goto error;
		if (await_dependency(synthesis, job, classifier)) return;
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis, classifier);
		if (!instance_job) goto error;
		if (await_dependency(synthesis, job, instance_job)) return;
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) goto error;
		size_t count = pg_data_constructor_count(instance.schema);
		if (count < job->syntax->item_count) goto rejected;
		if (!count) goto unsupported;
		if (count > (SIZE_MAX - sizeof(struct match_state)) / sizeof(struct match_branch)) goto error;
		const struct source_metadata *origin = source_metadata(synthesis, pg_evidence_subject(instance.formation));
		if (!origin || !origin->exports) goto unsupported;
		job->match = pg_alloc(synthesis->typing->graph, sizeof(struct match_state) + count * sizeof(struct match_branch));
		if (!job->match) goto error;
		job->match->instance = instance;
		job->match->motive_totality = PG_TOTALITY_TOTAL;
		job->match->count = count;
		job->match->labels = origin->exports;
		job->match->packet = origin->packet;
	}
	struct match_state *state = job->match;
	const struct pg_data_layout *layout = pg_data_schema_layout(state->instance.schema);
	if (state->selected < job->syntax->item_count) {
		const struct pg_syntax *clause = job->syntax->items[state->selected].expression;
		int packet = state->packet && !clause->item_count && clause->left->kind == PG_SYNTAX_ATOM
			&& clause->left->token.kind == PG_TOKEN_IDENT;
		struct source_reference label = {0};
		if (packet) {
			if (state->count != 1) goto rejected;
		} else if (clause->left->kind == PG_SYNTAX_ATOM)
			label = lookup_scope(state->labels, clause->left->token);
		else {
			struct pg_synthesis_job *dependency = NULL;
			enum pg_synthesis_status status = resolve_reference(synthesis, job->inner, clause->left, &label, &dependency);
			if (status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, dependency); return; }
			if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return; }
		}
		const struct pg_object *constructor;
		if (packet) constructor = pg_data_constructor(layout, 0);
		else {
			if (!label.producer) goto rejected;
			if (label.producer->role != CONSTRUCTOR_VALUE_JOB) goto unsupported;
			constructor = label.producer->inputs[1];
		}
		size_t ordinal;
		if (!pg_data_constructor_position(layout, constructor, &ordinal)) goto rejected;
		if (state->branches[ordinal].clause) goto rejected;
		state->branches[ordinal].clause = clause;
		++state->selected;
		enqueue(synthesis, job);
		return;
	}
	/* Scope every constructor, including omitted clauses. Only a checked
	 * contradiction can later replace an absent body with elimination. */
	if (state->next < state->count) {
		size_t ordinal = state->next;
		struct match_branch *branch = &state->branches[ordinal];
		const struct pg_syntax *clause = branch->clause;
		int packet = state->packet && clause && !clause->item_count && clause->left->kind == PG_SYNTAX_ATOM
			&& clause->left->token.kind == PG_TOKEN_IDENT;
		const struct pg_object *constructor = pg_data_constructor(layout, ordinal);
		const struct pg_evidence *schema_fields = pg_data_schema_fields(state->instance.schema, constructor);
		size_t field_count;
		if (pg_context_extension_size(pg_evidence_context(schema_fields),
			pg_evidence_context(pg_evidence_premise(state->instance.formation, 0)), &field_count)) goto error;
		if (job->match_allocation) {
			const struct pg_context *saved;
			if (source_match_branch_context(job, ordinal, field_count, &saved) || !pg_synthesis_constructor_scope_at(synthesis,
				pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
				pg_synthesis_evidence(synthesis, state->instance.parameters), pg_evidence_context(context), saved)) goto rejected;
		}
		struct pg_synthesis_job *scope_job = pg_synthesis_constructor_scope(synthesis,
			pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
			pg_synthesis_evidence(synthesis, state->instance.parameters));
		if (!scope_job) goto error;
		if (await_dependency(synthesis, job, scope_job)) return;
		const struct pg_evidence *map = scope_job->result;
		const struct pg_evidence *fields = pg_evidence_premise(map, 1);
		size_t count;
		if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(context), &count)) goto error;
		if (packet && count != 2) goto rejected;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
		if (count && !extensions) goto error;
		for (size_t i = count; i; --i, fields = pg_evidence_premise(fields, 0)) extensions[i - 1] = fields;
		const struct pg_source_scope *scope = job->inner;
		if (!packet) {
			enum pg_synthesis_status status = case_field_scope(synthesis, scope, state->instance.formation,
				constructor, clause, count, extensions, &scope);
			if (status != PG_SYNTHESIS_DONE) { free(extensions); finish(synthesis, job, status); return; }
		}
		else for (size_t i = 0; i < count; ++i) {
			const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
			if (i == 1)
				scope = pg_synthesis_bind_graph(synthesis, scope, scope->binder, binder,
					pg_synthesis_evidence(synthesis, extensions[i]));
			else scope = pg_synthesis_bind(synthesis, scope, clause->left->token, binder, extensions[i]);
			if (!scope) break;
		}
		free(extensions);
		if (!scope) goto error;
		if (await_dependency(synthesis, job, scope->context_job)) return;
		branch->scope = scope;
		branch->fields = source_context(scope);
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
		enqueue(synthesis, job);
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
			if (state->generalization) branch->scope = generalized_scope(synthesis, job->inner, branch->scope,
				state->generalization, match_constructor_pattern(synthesis, job, i, source_context(branch->scope)));
			if (!branch->scope) goto unsupported;
			if (branch->needs_ih) continue;
			branch->body = pg_synthesis_request(synthesis, branch->scope, branch->clause->right);
			if (!branch->body) goto error;
		}
		state->scoped = 1;
		enqueue(synthesis, job); return;
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
		enqueue(synthesis, job); return;
	}
	if (state->has_demands && state->effect_checked < state->count) { match_effects_step(synthesis, job); return; }
	if (state->demanded < state->count) { match_demand_step(synthesis, job); return; }
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
			enqueue(synthesis, job);
			return;
		}
		/* Retain constructor-index information before choosing a constant
		 * result. Refuted branches above cannot propose a motive. */
		if (state->path_context) state->type_cases = 1;
		struct pg_synthesis_job *candidate = state->motive_context || state->type_cases
			? pg_synthesis_abstract(synthesis, context, source_context(branch->scope), branch->body)
			: pg_synthesis_constant_motive(synthesis, context, source_context(branch->scope), branch->body);
		if (!candidate) goto error;
		if (candidate->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, candidate); return; }
		if (candidate->status == PG_SYNTHESIS_UNSUPPORTED && !state->induction && !state->type_cases) {
			state->type_cases = 1;
			enqueue(synthesis, job);
			return;
		}
		if (candidate->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, candidate->status); return; }
		branch->function = state->motive_context || state->type_cases ? candidate->result : candidate->function;
		if (!state->motive_context && !state->type_cases) {
			const struct pg_evidence *type = candidate->result;
			if (!state->motive) state->motive = type;
			else {
				struct pg_synthesis_job *comparison = request_job(synthesis, CONVERSION_JOB,
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
		enqueue(synthesis, job);
		return;
	}
	if (state->type_cases && !state->motive_context) { match_dependent_motive_step(synthesis, job); return; }
	if (!state->motive && state->path_context) {
		for (size_t i = 0; i < state->count; ++i)
			if (!state->branches[i].contradiction) goto unsupported;
		struct pg_synthesis_job *constraint = request_job(synthesis, RESULT_TYPE_JOB, job, NULL);
		if (!constraint) goto error;
		if (await_dependency(synthesis, job, constraint)) return;
		state->motive = pg_prove_projection(synthesis->typing, context, constraint->result);
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
		if (await_dependency(synthesis, job, state->motive_job)) return;
		state->motive_context = state->generic_context;
		state->motive = state->motive_job->result;
		if (state->path_context) state->path_result = state->motive;
		const struct pg_evidence *extension = state->generalization ? pg_evidence_premise(state->generalization, 1) : NULL;
		for (size_t i = state->generalized_count; i; --i, extension = pg_evidence_premise(extension, 0))
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
		if (await_dependency(synthesis, job, scope)) return;
		if (branch->needs_ih) {
			if (!branch->body) branch->body = match_induction_source(synthesis, job,
				state->prepared, state->motive_context, state->motive);
			if (!branch->body) goto error;
			if (await_dependency(synthesis, job, branch->body)) return;
			branch->function = branch->body->result;
		} else {
			if (!branch->adapted) {
				const struct pg_evidence *map = scope->result, *destination = pg_evidence_premise(map, 1);
				struct pg_synthesis_job *body = projected_image(synthesis, destination, branch->function);
				for (size_t i = pg_evidence_premise_count(state->instance.parameters) + 1; i < pg_evidence_premise_count(map); ++i)
					body = pg_synthesis_application(synthesis, destination, body,
						projected_image(synthesis, destination, pg_evidence_premise(map, i)));
				branch->adapted = pg_synthesis_abstract(synthesis, context, destination, body);
				if (!branch->adapted) goto error;
			}
			if (await_dependency(synthesis, job, branch->adapted)) return;
			branch->function = branch->adapted->result;
		}
		if (!branch->function) goto unsupported;
		++state->prepared;
		enqueue(synthesis, job);
		return;
	}
	if (state->validated < state->count) { match_validate_branch(synthesis, job); return; }
	const struct pg_induction_allocation *allocation = source_induction_allocation(job);
	if (allocation && !state->induction) goto rejected;
	struct pg_induction_allocation generated = {0};
	const struct pg_context **clauses = NULL;
	enum pg_synthesis_status elimination_status = PG_SYNTHESIS_UNSUPPORTED;
	const struct pg_evidence **branches = malloc(state->count * sizeof(*branches));
	if (!branches) goto error;
	for (size_t i = 0; i < state->count; ++i) branches[i] = state->branches[i].function;
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
	free(branches);
	if (!job->result) {
		if (elimination_status != PG_SYNTHESIS_PENDING) finish(synthesis, job, elimination_status);
		return;
	}
complete:
	/* Instantiate the elimination, then close its computed scrutinee. Retain
	 * one producer chain so suspension cannot skip either operation. */
	if (!job->value_job) {
		struct match_state *state = job->match;
		const struct pg_evidence *context = source_context(job->inner);
		struct pg_synthesis_job *result = pg_synthesis_evidence(synthesis, job->result);
		if (state && state->path_context) {
			size_t first = pg_evidence_premise_count(state->instance.parameters) + 1;
			for (size_t i = 0; i < state->path_count; ++i) {
				const struct pg_evidence *value = pg_evidence_premise(state->instance.indices, first + i);
				const struct pg_evidence *type = pg_prove_classifier(synthesis->typing, context, value);
				const struct pg_evidence *path = pg_prove_reflexivity(synthesis->typing, type, value);
				result = pg_synthesis_application(synthesis, context, result, pg_synthesis_evidence(synthesis, path));
			}
		}
		if (state && state->specialization) {
			size_t end = pg_evidence_premise_count(state->specialization);
			for (size_t i = end - state->generalized_count; i < end; ++i)
				result = pg_synthesis_application(synthesis, context, result,
					pg_synthesis_evidence(synthesis, pg_evidence_premise(state->specialization, i)));
		}
		if (job->match_frame) {
			const struct block_frame *frame = job->match_frame;
			struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, frame->context, result);
			result = pg_synthesis_sequence(synthesis,
				rule_premise(synthesis, frame->context, 0), frame->input, continuation);
		}
		if (!result) goto error;
		job->value_job = result;
	}
	if (await_dependency(synthesis, job, job->value_job)) return;
	job->result = job->value_job->result;
	job->match_frame = NULL;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static struct substitution_state *substitution_start(struct pg_synthesis *synthesis,
	const struct pg_evidence *source, const struct pg_evidence *destination, size_t count)
{
	if (count > (SIZE_MAX - sizeof(struct substitution_state)) / sizeof(struct substitution_entry)) return NULL;
	struct substitution_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state) + count * sizeof(*state->entries));
	if (!state) return NULL;
	state->count = count;
	for (size_t i = count; i; --i, source = pg_evidence_premise(source, 0))
		state->entries[i - 1].extension = source;
	state->map = pg_prove_substitution(synthesis->typing, source, destination, 0, NULL);
	return state->map ? state : NULL;
}

static const struct pg_reduction_certificate *normalization_receipt(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_term *input, enum pg_reduction_kind kind);

static void inductive_instance_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *type = (void *)job->inputs[0];
	if (await_dependency(synthesis, job, type)) return;
	struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, type->result);
	if (!accepted) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (accepted != type) {
		struct pg_synthesis_job *canonical = pg_synthesis_inductive_instance(synthesis, accepted);
		if (!canonical) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (canonical->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, canonical); return; }
		job->inductive_instance = canonical->inductive_instance;
		job->result = canonical->result;
		finish(synthesis, job, canonical->status);
		return;
	}
	enum pg_evidence_judgement kind = pg_evidence_judgement(type->result);
	if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_TYPE_FAMILY) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	/* Retain beta evidence before recovering the nominal declaration. */
	const struct pg_reduction_certificate *receipt = normalization_receipt(synthesis, job,
		pg_evidence_subject(type->result)->core, PG_REDUCTION_WHNF);
	if (!receipt) return;
	const struct pg_evidence *normalized = pg_prove_normalization(synthesis->typing, type->result, receipt);
	if (!normalized) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	struct pg_typed_query *query = pg_inductive_request(synthesis->typing, normalized);
	int status = pg_typed_query_advance(query, 1);
	if (!status) { enqueue(synthesis, job); return; }
	if (status < 0) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	job->inductive_instance = pg_inductive_query_result(query);
	job->result = job->inductive_instance->parameters;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static struct pg_synthesis_job *family_function(struct pg_synthesis *synthesis, struct pg_synthesis_job *input)
{
	if (input && input->status == PG_SYNTHESIS_DONE) input = pg_synthesis_evidence(synthesis, input->result);
	return input ? request_job(synthesis, FAMILY_FUNCTION_JOB, input, NULL) : NULL;
}

/* Establish a stable family by checking the returned type under its generic
 * parameters. An empty effect row alone supplies no type or termination proof. */
static void family_contract_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *context = (void *)job->inputs[0], *input = (void *)job->inputs[1];
	struct pg_synthesis_job *premises[] = {context, input};
	for (size_t i = 0; i < 2; ++i) {
		if (await_dependency(synthesis, job, premises[i])) return;
	}
	const struct pg_evidence *proof = input->result;
	if (!proof) goto rejected;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_TYPE_FAMILY) {
		forward_proof(synthesis, job, input);
		return;
	}
	if (!job->left) {
		if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE) proof = pg_prove_force(synthesis->typing, proof);
		if (!proof || pg_evidence_judgement(proof) != PG_JUDGEMENT_COMPUTATION) goto rejected;
		job->left = pg_synthesis_normalize_classifier(synthesis, context->result, proof);
		if (!job->left) goto error;
	}
	if (await_dependency(synthesis, job, job->left)) return;
	proof = job->left->result;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (pg_pi_view(pg_evidence_classifier(proof), &domain, &binder, &body)) {
		if (!job->value_job) {
			struct pg_typing *typing = synthesis->typing;
			const struct pg_evidence *pi = pg_prove_classifier(typing, context->result, proof);
			const struct pg_evidence *scope = pg_prove_context_extension(typing, context->result, binder,
				pg_prove_pi_domain(typing, pi));
			if (!scope) goto unsupported;
			const struct pg_evidence *applied = pg_prove_application(typing,
				pg_prove_projection(typing, scope, proof), pg_prove_variable(typing, scope, binder));
			if (!applied) goto rejected;
			struct pg_synthesis_job *scope_job = pg_synthesis_evidence(synthesis, scope);
			struct pg_synthesis_job *family = request_job(synthesis, FAMILY_CONTRACT_JOB, scope_job,
				pg_synthesis_evidence(synthesis, applied));
			struct pg_synthesis_job *arguments[] = {scope_job, family};
			job->value_job = plain_rule(synthesis, PG_TYPE_FAMILY_ABSTRACT, NULL, 2, arguments);
		}
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	uint64_t level;
	enum pg_totality totality;
	if (!pg_pure_computation_type_view(pg_evidence_classifier(proof), &totality, &body) || !pg_universe_level(body, &level)) goto rejected;
	if (!job->value_job) job->value_job = pg_synthesis_return(synthesis, context->result, proof);
	if (!job->value_job) goto error;
	if (await_dependency(synthesis, job, job->value_job)) return;
	job->result = pg_prove_value_type(synthesis->typing, job->value_job->result);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void family_function_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *input = (void *)job->inputs[0];
	if (await_dependency(synthesis, job, input)) return;
	const struct pg_evidence *proof = input->result;
	if (!proof) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (pg_evidence_judgement(proof) != PG_JUDGEMENT_TYPE_FAMILY) { forward_proof(synthesis, job, input); return; }
	struct pg_synthesis_job *canonical = family_function(synthesis, input);
	if (canonical != job) { forward_proof(synthesis, job, canonical); return; }
	if (job->value_job) { forward_proof(synthesis, job, job->value_job); return; }
	if (!job->left) {
		struct pg_typed_query *origin = pg_construction_origin_request(synthesis->typing, proof);
		if (!pg_typed_query_advance(origin, 1)) { enqueue(synthesis, job); return; }
		const struct pg_evidence *construction = pg_typed_query_result(origin);
		const struct pg_evidence *environment = pg_construction_origin_environment(origin);
		const struct pg_occurrence *subject = construction ? pg_evidence_subject(construction) : NULL;
		if (subject && subject->core->kind == PG_LAMBDA) {
			const struct pg_evidence *context = pg_evidence_premise(construction, 0);
			struct pg_synthesis_job *body = family_function(synthesis,
				pg_synthesis_evidence(synthesis, pg_prove_structural_subject(synthesis->typing, subject->operands[0])));
			job->value_job = pg_synthesis_lambda_body(synthesis,
				pg_synthesis_evidence(synthesis, context), body);
		} else if (subject && subject->core->kind == PG_APPLICATION) {
			struct pg_typed_query *application = pg_application_body_request(synthesis->typing,
				pg_prove_structural_subject(synthesis->typing, subject->operands[0]),
				pg_prove_structural_subject(synthesis->typing, subject->operands[1]));
			if (!pg_typed_query_advance(application, 1)) { enqueue(synthesis, job); return; }
			const struct pg_evidence *body = pg_typed_query_result(application);
			if (body) job->value_job = family_function(synthesis, pg_synthesis_evidence(synthesis, body));
		}
		if (job->value_job) {
			if (environment) job->value_job = pg_synthesis_reindex_jobs(synthesis,
				pg_synthesis_evidence(synthesis, environment), job->value_job);
			forward_proof(synthesis, job, job->value_job); return;
		}
		job->left = pg_synthesis_inductive_instance(synthesis, input);
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (await_dependency(synthesis, job, job->left)) return;
	const struct pg_inductive_instance *instance = job->left->inductive_instance;
	job->result = pg_prove_inductive_family_function(synthesis->typing,
		instance->formation, instance->parameters);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void constructor_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->substitution) {
		const struct pg_evidence *proofs[2];
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *producer = (void *)job->inputs[i];
			if (await_dependency(synthesis, job, producer)) return;
			proofs[i] = producer->result;
		}
		const struct pg_evidence *formation = proofs[0], *parameters = proofs[1];
		if (!formation || !parameters) goto rejected;
		if (pg_evidence_rule(formation) != PG_INDUCTIVE_FORM || pg_evidence_rule(parameters) != PG_CONTEXT_SUBSTITUTION) goto rejected;
		if (pg_evidence_context(pg_evidence_premise(parameters, 0)) != pg_evidence_context(formation)) goto rejected;
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis, (void *)job->inputs[0]);
		if (!instance_job) goto error;
		if (await_dependency(synthesis, job, instance_job)) return;
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) goto error;
		const struct pg_evidence *fields = pg_data_schema_fields(instance.schema, job->inputs[2]);
		const struct pg_evidence *self = pg_evidence_premise(formation, 0);
		size_t count;
		if (!fields || pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(self), &count)) goto rejected;
		if (job->context_allocation) {
			const struct context_allocation *allocation = job->context_allocation;
			if (allocation->count != count || allocation->prefix != pg_evidence_context(pg_evidence_premise(parameters, 1))) goto rejected;
		}
		if (count > (SIZE_MAX - sizeof(struct substitution_state)) / sizeof(struct substitution_entry)) goto error;
		struct substitution_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state) + count * sizeof(*state->entries));
		if (!state) goto error;
		state->count = count;
		for (size_t i = count; i; --i, fields = pg_evidence_premise(fields, 0)) state->entries[i - 1].extension = fields;
		job->substitution = state;
		struct pg_synthesis_job *family = pg_synthesis_reindex_jobs(synthesis, (void *)job->inputs[1], (void *)job->inputs[0]);
		struct pg_derivation_input as_value = {.rule = PG_VALUE_FROM_TYPE, .count = 1};
		struct pg_synthesis_job *value = pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY
			? family : pg_synthesis_rule(synthesis, &as_value, &family, NULL, NULL);
		size_t prefix = pg_evidence_premise_count(parameters) - 2;
		if (prefix >= SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
		struct pg_synthesis_job **images = malloc((prefix + 1) * sizeof(*images));
		if (!images) goto error;
		for (size_t i = 0; i < prefix; ++i) images[i] = pg_synthesis_evidence(synthesis, pg_evidence_premise(parameters, i + 2));
		images[prefix] = value;
		job->right = pg_synthesis_substitution_jobs(synthesis, pg_synthesis_evidence(synthesis, self),
			pg_synthesis_evidence(synthesis, pg_evidence_premise(parameters, 1)), prefix + 1, images);
		free(images);
		if (!job->right) goto error;
	}
	struct substitution_state *state = job->substitution;
	if (await_dependency(synthesis, job, job->right)) return;
	state->map = job->right->result;
	if (state->next == state->count) {
		job->result = state->map;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	const struct pg_object *binder;
	if (job->context_allocation) binder = job->context_allocation->contexts[state->next]->binder;
	else {
		const struct pg_evidence *parameters = pg_synthesis_result(job->inputs[1]);
		const struct pg_context *scope = pg_evidence_context(pg_evidence_premise(parameters, 1));
		const struct pg_source_binding *binding = source_binding_intern(synthesis,
			&(struct pg_source_binding){.slot = state->next, .constructor = job->inputs[2]},
			(struct binding_cursor){.context = scope});
		if (!binding) goto rejected;
		binder = binding->binder;
	}
	job->right = pg_synthesis_substitution_lift(synthesis, state->map,
		state->entries[state->next++].extension, binder);
	if (!job->right) goto error;
	depend(synthesis, job, job->right);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void induction_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *proofs[4];
	for (size_t i = 0; i < 4; ++i) {
		struct pg_synthesis_job *producer = (void *)job->inputs[i];
		if (await_dependency(synthesis, job, producer)) return;
		proofs[i] = producer->result;
		if (!proofs[i]) goto rejected;
	}
	const struct pg_evidence *formation = proofs[0], *parameters = proofs[1], *motive_context = proofs[2], *motive = proofs[3];
	if (!job->left) {
		if (pg_evidence_rule(motive_context) != PG_CONTEXT_EXTEND) goto rejected;
		if (pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) goto rejected;
		if (pg_evidence_context(motive) != pg_evidence_context(motive_context)) goto rejected;
		if (!pg_inductive_motive_context_valid(synthesis->typing, formation, parameters, motive_context)) goto rejected;
		job->left = pg_synthesis_constructor_scope(synthesis, (void *)job->inputs[0], job->inputs[4], (void *)job->inputs[1]);
		if (!job->left) goto error;
	}
	if (await_dependency(synthesis, job, job->left)) return;
	const struct pg_evidence *map = job->left->result;
	if (!job->substitution) {
		if (job->context_allocation && !same_context_binders(job->context_allocation->prefix,
			pg_evidence_context(pg_evidence_premise(map, 1)))) goto rejected;
		job->substitution = pg_alloc(synthesis->typing->graph, sizeof(*job->substitution));
		if (!job->substitution) goto error;
		job->substitution->map = pg_evidence_premise(map, 1);
	}
	if (job->right) {
		if (await_dependency(synthesis, job, job->right)) return;
		job->substitution->map = job->right->result;
		job->right = NULL;
	}
	struct substitution_state *state = job->substitution;
	const struct pg_evidence *context = state->map;
	const struct substitution_state *fields = job->left->substitution;
	const struct pg_object *self = pg_evidence_context(pg_evidence_premise(formation, 0))->binder;
	while (state->next < fields->count) {
		size_t i = state->next++;
		int recursive = pg_data_recursive_field(pg_evidence_context(fields->entries[i].extension)->declared_type, self);
		if (!recursive) continue;
		if (recursive < 0) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *field = pg_prove_projection(synthesis->typing, context,
			pg_evidence_premise(map, pg_evidence_premise_count(parameters) + 1 + i));
		const struct pg_context *retained = NULL;
		if (job->context_allocation) {
			struct context_allocation *allocation = job->context_allocation;
			if (allocation->next == allocation->count) goto rejected;
			retained = allocation->contexts[allocation->next++];
		}
		const struct pg_evidence *ih = pg_prove_inductive_hypothesis_type(synthesis->typing,
			formation, parameters, motive_context, motive, context, field, retained ? retained->declared_type : NULL);
		if (!ih) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *at_field = pg_synthesis_evidence(synthesis, ih);
		struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(synthesis, context), at_field};
		struct pg_derivation_input extend = {.rule = PG_CONTEXT_EXTEND, .count = 2};
		extend.parameters.binder = retained ? retained->binder : pg_binder(synthesis->typing->graph);
		job->right = pg_synthesis_rule(synthesis, &extend, premises, NULL, NULL);
		if (!job->right) goto error;
		depend(synthesis, job, job->right);
		return;
	}
	if (job->context_allocation && job->context_allocation->next != job->context_allocation->count) goto rejected;
	struct pg_typed_query *query = pg_substitution_rebase_request(synthesis->typing, context, map);
	if (!pg_typed_query_advance(query, 1)) { enqueue(synthesis, job); return; }
	job->result = pg_typed_query_result(query);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static enum pg_synthesis_status data_result_start(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *parameters = job->inputs[1], *indices = job->inputs[2];
	const struct pg_context *prefix = pg_evidence_context(parameters);
	size_t index_count, parameter_count, field_count;
	if (pg_context_extension_size(pg_evidence_context(indices), prefix, &index_count) != 0) return PG_SYNTHESIS_REJECTED;
	if (pg_context_extension_size(pg_evidence_context(source_context(job->scope)), prefix, &field_count) != 0) return PG_SYNTHESIS_REJECTED;
	if (pg_context_extension_size(prefix, NULL, &parameter_count) != 0) return PG_SYNTHESIS_ERROR;
	const struct pg_syntax *head = job->syntax;
	size_t arguments = 0;
	while (head->kind == PG_SYNTAX_APPLICATION) { ++arguments; head = head->left; }
	if (head->kind != PG_SYNTAX_ATOM || head->token.kind != '*' || arguments != index_count) return PG_SYNTHESIS_REJECTED;
	if (index_count > SIZE_MAX - parameter_count) return PG_SYNTHESIS_ERROR;
	size_t count = parameter_count + index_count;
	struct substitution_state *state = substitution_start(synthesis, indices, source_context(job->scope), count);
	if (!state) return PG_SYNTHESIS_ERROR;
	job->substitution = state;
	head = job->syntax;
	for (size_t i = count; i; --i) {
		struct substitution_entry *entry = &state->entries[i - 1];
		if (i > parameter_count) {
			entry->image = pg_synthesis_request(synthesis, job->scope, head->right);
			head = head->left;
		} else {
			const struct pg_evidence *image = pg_prove_variable(synthesis->typing,
				source_context(job->scope), pg_evidence_context(entry->extension)->binder);
			entry->image = pg_synthesis_evidence(synthesis, image);
		}
		if (!entry->image) return PG_SYNTHESIS_ERROR;
	}
	return PG_SYNTHESIS_DONE;
}

static void substitution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->substitution) {
		if (job->role == DATA_RESULT_JOB) {
			enum pg_synthesis_status status = data_result_start(synthesis, job);
			if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return; }
		} else {
			size_t count = job->input_count - 2;
			const struct pg_evidence *contexts[2];
			for (size_t i = 0; i < 2; ++i) {
				struct pg_synthesis_job *producer = (void *)job->inputs[i];
				if (await_dependency(synthesis, job, producer)) return;
				contexts[i] = producer->result;
				if (!contexts[i] || pg_evidence_judgement(contexts[i]) != PG_JUDGEMENT_CONTEXT) {
					finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
				}
			}
			size_t arity;
			if (pg_context_extension_size(pg_evidence_context(contexts[0]), NULL, &arity) || arity != count) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			job->substitution = substitution_start(synthesis, contexts[0], contexts[1], count);
			if (!job->substitution) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			for (size_t i = 0; i < count; ++i)
				job->substitution->entries[i].image = (struct pg_synthesis_job *)job->inputs[i + 2];
		}
	}
	struct substitution_state *state = job->substitution;
	if (job->right) {
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
		state->map = job->right->result;
		job->left = job->right = NULL;
		++state->next;
	}
	if (state->next == state->count) {
		job->result = state->map;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	const struct substitution_entry *entry = &state->entries[state->next];
	if (!job->left) {
		job->left = entry->image;
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *image = job->left->result;
	if (!image || !pg_evidence_subject(image)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	image = type_input(synthesis, job, pg_evidence_premise(state->map, 1), image);
	if (!image) return;
	if (pg_evidence_rule(entry->extension) != PG_CONTEXT_FAMILY_EXTEND) image = value(synthesis, image);
	if (!image) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	job->right = pg_synthesis_substitution_pair(synthesis, state->map, entry->extension, image);
	depend(synthesis, job, job->right);
}

static void data_case_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->stage) {
		job->stage = 1;
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (job->stage == 1) {
		job->checking_term = job->left->result;
		if (!job->checking_term) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *fields = pg_data_schema_fields(job->inputs[0], job->inputs[1]);
		if (pg_evidence_context(job->checking_term) != pg_evidence_context(fields)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_evidence_judgement(job->checking_term) != PG_JUDGEMENT_COMPUTATION) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		job->right = pg_synthesis_reindex(synthesis,
			pg_data_schema_result(job->inputs[0], job->inputs[1]), job->inputs[2]);
		job->stage = 2;
		depend(synthesis, job, job->right);
		return;
	}
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	job->checking_type = job->right->result;
	const struct pg_evidence *checked = compare(synthesis, job);
	if (!checked) return;
	job->result = pg_data_branch(synthesis->typing, job->inputs[0], job->inputs[1], checked);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static int family_paths(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *left = job->inputs[0], *right = job->inputs[1];
	if (!job->family) {
		size_t arity = pg_evidence_premise_count(left) - 2, count = job->input_count - 3;
		if (count > arity || pg_evidence_premise_count(right) != arity + 2) goto rejected;
		if (pg_evidence_context(left) != pg_evidence_context(right)) goto rejected;
		if (pg_evidence_context(pg_evidence_premise(left, 0)) != pg_evidence_context(pg_evidence_premise(right, 0))) goto rejected;
		struct family_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
		if (!state) goto error;
		job->family = state;
		state->count = count;
		state->common = arity - count;
		state->declarations = pg_alloc(synthesis->typing->graph, count * sizeof(*state->declarations));
		state->paths = pg_alloc(synthesis->typing->graph, count * sizeof(*state->paths));
		if (count && (!state->declarations || !state->paths)) goto error;
		const struct pg_evidence *prefix = pg_evidence_premise(left, 0);
		for (size_t i = count; i; --i) {
			state->declarations[i - 1] = prefix;
			prefix = pg_evidence_premise(prefix, 0);
		}
		for (size_t side = 0; side < 2; ++side) {
			const struct pg_evidence *map = job->inputs[side];
			state->maps[side] = pg_prove_substitution(synthesis->typing, prefix,
				pg_evidence_premise(map, 1), state->common, pg_evidence_premises(map) + 2);
		}
		if (!state->maps[0] || !state->maps[1]) goto rejected;
	}
	struct family_state *state = job->family;
	if (state->next == state->count) return 1;
	size_t i = state->next;
	if (!job->checking_type) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[i + 3];
		if (await_dependency(synthesis, job, producer)) return 0;
		job->checking_term = producer->result;
		if (!pg_evidence_owned_by(job->checking_term, synthesis->typing)) goto rejected;
		if (pg_evidence_judgement(job->checking_term) != PG_JUDGEMENT_VALUE) goto rejected;
		if (pg_evidence_context(job->checking_term) != pg_evidence_context(left)) goto rejected;
		job->checking_type = pg_prove_family_identity_type(synthesis->typing,
			pg_evidence_premise(state->declarations[i], 1), state->maps[0], state->maps[1], i, state->paths,
			pg_evidence_premise(left, state->common + i + 2), pg_evidence_premise(right, state->common + i + 2));
		if (!job->checking_type) goto rejected;
	}
	const struct pg_evidence *checked = compare(synthesis, job);
	if (!checked) return 0;
	state->paths[i] = checked;
	for (size_t side = 0; side < 2; ++side)
		state->maps[side] = pg_prove_substitution_pair(synthesis->typing, state->maps[side], state->declarations[i],
			pg_evidence_premise(job->inputs[side], state->common + i + 2));
	if (!state->maps[0] || !state->maps[1]) goto rejected;
	job->checking_type = NULL;
	++state->next;
	enqueue(synthesis, job);
	return 0;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return 0;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 0;
}

static void formation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->formation) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[0];
		if (await_dependency(synthesis, job, producer)) return;
		const struct pg_evidence *proof = producer->result;
		if (!pg_evidence_owned_by(proof, synthesis->typing)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_formation(synthesis, pg_synthesis_evidence(synthesis, proof));
		if (forward_proof(synthesis, job, canonical)) return;
		job->formation = pg_identity_formation_init(synthesis->typing, proof);
		if (!job->formation) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	int status = pg_identity_formation_advance(job->formation, 1);
	if (!status) { enqueue(synthesis, job); return; }
	job->result = pg_identity_formation_result(job->formation);
	pg_identity_formation_destroy(job->formation);
	job->formation = NULL;
	finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static const struct pg_reduction_certificate *normalization_receipt(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_term *input, enum pg_reduction_kind kind)
{
	const struct pg_reduction_certificate *certificate = NULL;
	if (kind == PG_REDUCTION_NF || kind == PG_REDUCTION_PREFIX) {
		if (!job->normalizing.nf) job->normalizing.nf = pg_nf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!job->normalizing.nf) goto failure;
		if (kind == PG_REDUCTION_PREFIX) {
			int status = pg_nf_prefix_certificate(job->normalizing.nf, &certificate);
			if (status < 0) goto failure;
			if (status) return certificate;
		}
		if (pg_nf_advance(job->normalizing.nf, 1) == PG_NF_ERROR) goto failure;
		if (kind == PG_REDUCTION_PREFIX) {
			if (pg_nf_prefix_certificate(job->normalizing.nf, &certificate) < 0) goto failure;
		} else certificate = pg_nf_certificate(job->normalizing.nf);
	} else if (kind == PG_REDUCTION_WHNF) {
		if (!job->normalizing.whnf) job->normalizing.whnf = pg_whnf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!job->normalizing.whnf || pg_whnf_advance(job->normalizing.whnf, 1) == PG_EVAL_ERROR) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL;
		}
		certificate = pg_whnf_certificate(job->normalizing.whnf);
	} else { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL; }
	if (!certificate) enqueue(synthesis, job);
	return certificate;
failure:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return NULL;
}

static int derivation_endpoint(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_term *actual, const struct pg_term *stored)
{
	struct pg_comparison *work = &job->derivation->endpoint;
	if (!actual || !stored) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 0; }
	if (!work->state && pg_comparison_init(work, actual, stored, NULL, NULL)) {
		finish(synthesis, job, PG_SYNTHESIS_ERROR); return 0;
	}
	enum pg_comparison_status status = pg_comparison_advance(work, 1);
	if (status == PG_COMPARISON_PENDING) { enqueue(synthesis, job); return 0; }
	pg_comparison_destroy(work);
	if (status == PG_COMPARISON_EQUAL) return 1;
	finish(synthesis, job, status == PG_COMPARISON_DIFFERENT ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
	return 0;
}

static struct pg_synthesis_job *rule_premise(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index)
{
	if (!job || (job->role != DERIVATION_JOB && job->role != DERIVATION_INPUT_JOB)) return NULL;
	const struct pg_derivation_input *input = job->inputs[0];
	if (index >= input->count) return NULL;
	if (job->role == DERIVATION_INPUT_JOB)
		return pg_synthesis_derivation_inference(synthesis, input->premises[index], (void *)job->inputs[1]);
	return (void *)job->inputs[index + 3];
}

static void forward_structure(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	job->type_structure = pg_synthesis_type_structure_result(job->left);
	finish(synthesis, job, job->left->status);
}

/* Accepted typed data is the structural authority. Pending recipes are only
 * needed before that data exists, for example to close effect equations. */
static int accepted_structure(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_synthesis_job *producer)
{
	const struct pg_evidence *proof = pg_synthesis_result(producer);
	if (!proof) return 0;
	const struct pg_occurrence *subject = pg_evidence_subject(proof);
	if (job->role == TYPE_STRUCTURE_JOB) {
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) subject = NULL;
	}
	job->type_structure = !subject ? NULL : job->role == CLASSIFIER_STRUCTURE_JOB ? subject->classifier : subject->core;
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
	return 1;
}

/* A known term producer can be projected before acceptance; namespace-only
 * producers still need name resolution rather than a typing rule. */
static int named_term_ready(const struct pg_synthesis_job *producer)
{
	if (!producer) return 0;
	if (producer->status == PG_SYNTHESIS_DONE) return producer->result != NULL;
	if (producer->role == DEFINITION_JOB) return 0;
	return source_value_kind(producer) >= 0;
}

/* Project the existing rule and, when requested, its preparation state.
 * A ready namespace can have no rule. This does not inspect acceptance or
 * copy a child's dependency; subscription and publication use the same view. */
static struct pg_synthesis_job *source_rule(const struct pg_synthesis_job *producer,
	int *preparing)
{
	struct pg_synthesis_job *rule = NULL;
	int waiting = 0;
	switch (producer->role) {
	case BINDING_EXPECT_JOB:
		/* The operand supplies structure; the annotation remains a post-check. */
		rule = producer->left;
		break;
	case DERIVATION_INPUT_JOB: case PI_SCOPE_JOB: case OPERATION_JOB:
		rule = producer->left; waiting = !rule;
		break;
	case BODY_JOB: case SEQUENCE_JOB: case HANDLER_RETURN_JOB: case HANDLER_CLAUSE_JOB: case HANDLER_JOB:
		rule = producer->value_job; waiting = !rule;
		break;
	case EXPRESSION_JOB:
		if (handler_syntax(producer->syntax)) {
			rule = producer->value_job; waiting = !rule;
			break;
		}
		if (block_syntax(producer->syntax)) {
			if (producer->block && !producer->block->frames) rule = producer->block->tail;
			waiting = !rule;
			break;
		}
		switch (producer->syntax->kind) {
		case PG_SYNTAX_QUALIFIED:
			rule = producer->value_job;
			waiting = !rule && producer->syntax->left->kind != PG_SYNTAX_DEFINITIONS;
			break;
		case PG_SYNTAX_APPLICATION:
			if (producer->stage == APPLICATION_RULE_READY) rule = producer->value_job;
			waiting = !producer->stage || (producer->stage == 2 && !producer->function);
			break;
		case PG_SYNTAX_LAMBDA: case PG_SYNTAX_QUOTE:
			rule = producer->syntax->kind == PG_SYNTAX_LAMBDA ? producer->value_job : producer->right;
			waiting = !producer->stage;
			break;
		case PG_SYNTAX_ATOM:
			rule = producer->binder ? producer->left : producer->value_job;
			if (!rule && producer->syntax->token.kind == '@') rule = producer->left;
			if (producer->syntax->token.kind == PG_TOKEN_INT || producer->syntax->token.kind == PG_TOKEN_TEXT)
				waiting = !producer->value_job;
			if (preparing && producer->status == PG_SYNTHESIS_PENDING &&
				producer->syntax->token.kind == PG_TOKEN_IDENT && !producer->value_job && !producer->binder) {
				struct source_reference reference = lookup_scope(producer->scope, producer->syntax->token);
				waiting = reference.binder || (reference.producer &&
					(reference.producer->status == PG_SYNTHESIS_PENDING || named_term_ready(reference.producer)));
			}
			break;
		default: break;
		}
		break;
	default: break;
	}
	if (preparing) *preparing = producer->status == PG_SYNTHESIS_PENDING && waiting;
	return rule;
}

static int await_source_preparation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *producer, struct pg_synthesis_job **rule)
{
	int preparing;
	struct pg_synthesis_job *prepared = source_rule(producer, &preparing);
	if (rule) *rule = prepared;
	if (!preparing) return 0;
	subscribe(synthesis, job, producer, 1);
	return 1;
}

/* 2 is a type used as a value, 1 a value, 0 computation, -1 unknown.
 * Accepted judgements supersede recipes; pending adapters expose their rule. */
static int source_value_kind(const struct pg_synthesis_job *producer)
{
	const struct pg_synthesis_job *rule = producer;
	while (!pg_synthesis_result(rule)) {
		const struct pg_synthesis_job *prepared = source_rule(rule, NULL);
		if (prepared) { rule = prepared; continue; }
		if (rule->role == CLASSIFIER_JOB) { rule = rule->inputs[1]; continue; }
		if (rule->role == DERIVATION_JOB && rule->input_count > 4 &&
			((const struct pg_derivation_input *)rule->inputs[0])->rule == PG_CONTEXT_PROJECTION) {
			rule = rule->inputs[4]; continue;
		}
		break;
	}
	const struct pg_evidence *result = rule->result ? rule->result : producer->result;
	if (result) {
		enum pg_evidence_judgement judgement = pg_evidence_judgement(result);
		if (judgement == PG_JUDGEMENT_VALUE_TYPE) return 2;
		if (judgement == PG_JUDGEMENT_VALUE) return 1;
		if (judgement == PG_JUDGEMENT_COMPUTATION) return 0;
	}
	if (rule->role == SEQUENCE_JOB || rule->role == BODY_JOB || rule->role == HANDLER_JOB) return 0;
	if (rule->role == EXPRESSION_JOB) {
		if (block_syntax(rule->syntax)) return 0;
		switch (rule->syntax->kind) {
		case PG_SYNTAX_LAMBDA: case PG_SYNTAX_APPLICATION: return 0;
		case PG_SYNTAX_QUOTE: return 1;
		default: return -1;
		}
	}
	if (rule->role != DERIVATION_JOB) return -1;
	const struct pg_derivation_input *input = rule->inputs[0];
	switch (input->rule) {
	case PG_UNIVERSE_FORM: case PG_HOST_TYPE_FORM: return 2;
	case PG_VARIABLE: case PG_THUNK_INTRO: case PG_VALUE_FROM_TYPE: case PG_HOST_VALUE_INTRO: return 1;
	case PG_LAMBDA_INTRO: case PG_APP_ELIM: case PG_FORCE_ELIM: case PG_RETURN_INTRO:
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM: case PG_EFFECT_SUBSUMPTION:
	case PG_HOST_FUNCTION_INTRO: return 0;
	default: return -1;
	}
}

static void body_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *input = (void *)job->inputs[0];
	if (!job->value_job) {
		if (await_source_preparation(synthesis, job, input, NULL)) return;
		int kind = source_value_kind(input);
		if (kind < 0) {
			if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
			finish(synthesis, job, input->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : input->status);
			return;
		}
		struct pg_synthesis_job *body = input;
		if (kind == 2) body = plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &body);
		if (kind > 0) {
			struct pg_derivation_input returned = {.rule = PG_RETURN_INTRO,
				.parameters.totality = PG_TOTALITY_TOTAL, .count = 1};
			body = pg_synthesis_rule(synthesis, &returned, &body, NULL, NULL);
		}
		if (!body) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->value_job = body;
	}
	struct pg_synthesis_job *context = (void *)job->inputs[1];
	if (context) {
		struct pg_synthesis_job *premises[] = {input, context};
		for (size_t i = 0; i < 2; ++i) {
			if (await_dependency(synthesis, job, premises[i])) return;
		}
		if (!input->result || !context->result || pg_evidence_context(input->result) != pg_evidence_context(context->result)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
	}
	forward_proof(synthesis, job, job->value_job);
}

static void compound_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer, const struct pg_derivation_input *input)
{
	int handling = input->rule == PG_HANDLER_ELIM;
	size_t count = handling ? pg_handler_signature_count(input->parameters.handler) : 0;
	if (handling && (!count || count > (SIZE_MAX - 3) / 3 || input->count != 3 + 3 * count)) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (count && !job->fold_structure) {
		if (count > (SIZE_MAX - sizeof(*job->fold_structure)) / sizeof(struct pg_operation_clause)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		job->fold_structure = pg_alloc(synthesis->typing->graph,
			sizeof(*job->fold_structure) + count * sizeof(struct pg_operation_clause));
		if (!job->fold_structure) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (!job->left) {
		size_t offset = input->rule == PG_REQUEST_INTRO ? 2 : 0;
		job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, offset));
		job->right = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, offset + 1));
	}
	struct pg_synthesis_job *parts[] = {job->left, job->right};
	for (size_t i = 0; i < 2; ++i) {
		if (!parts[i]) { finish(synthesis, job, handling ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_UNSUPPORTED); return; }
		if (await_dependency(synthesis, job, parts[i])) return;
	}
	struct fold_structure_state *state = job->fold_structure;
	if (count && state->next < count) {
		if (!job->value_job) job->value_job = pg_synthesis_term_structure(synthesis,
			rule_premise(synthesis, producer, 5 + 3 * state->next));
		if (await_dependency(synthesis, job, job->value_job)) return;
		state->clauses[state->next] = (struct pg_operation_clause){
			pg_handler_signature_label(input->parameters.handler, state->next),
			pg_synthesis_type_structure_result(job->value_job)};
		++state->next;
		job->value_job = NULL;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_term *left = pg_synthesis_type_structure_result(job->left);
	const struct pg_term *right = pg_synthesis_type_structure_result(job->right);
	switch (input->rule) {
	case PG_HANDLER_ELIM: case PG_FOLD_ELIM:
		job->type_structure = pg_computation_fold(synthesis->typing->graph, left, right, count, count ? state->clauses : NULL);
		break;
	case PG_REQUEST_INTRO:
		job->type_structure = pg_computation_request(synthesis->typing->graph, input->parameters.operation_label, left, right);
		break;
	default:
		job->type_structure = pg_application(synthesis->typing->graph, left, right);
		break;
	}
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE
		: handling ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_ERROR);
}

static void type_rule_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer, const struct pg_derivation_input *input);

static const struct pg_object *context_binding(const struct pg_synthesis_job *context)
{
	const struct pg_evidence *proof = pg_synthesis_result(context);
	if (proof) {
		const struct pg_context *scope = pg_evidence_context(proof);
		return pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT && scope ? scope->binder : NULL;
	}
	if (context->role == BINDING_JOB || context->role == PI_SCOPE_JOB) return context->binder;
	if (context->role == DERIVATION_JOB) {
		const struct pg_derivation_input *input = context->inputs[0];
		if (input->rule == PG_CONTEXT_EXTEND || input->rule == PG_CONTEXT_FAMILY_EXTEND)
			return input->parameters.binder;
	}
	return NULL;
}

static void lambda_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer)
{
	if (!job->binder) {
		struct pg_synthesis_job *pi = rule_premise(synthesis, producer, 0);
		if (!pi) goto unsupported;
		while (!pg_synthesis_result(pi)) {
			struct pg_synthesis_job *prepared;
			if (await_source_preparation(synthesis, job, pi, &prepared)) return;
			if (!prepared) break;
			pi = prepared;
		}
		const struct pg_evidence *proof = pg_synthesis_result(pi);
		if (proof) {
			const struct pg_term *domain, *codomain;
			if (!pg_evidence_subject(proof) || !pg_pi_view(pg_evidence_subject(proof)->core,
				&domain, &job->binder, &codomain)) goto unsupported;
		} else if (pi->role == DERIVATION_JOB &&
			((const struct pg_derivation_input *)pi->inputs[0])->rule == PG_PI_FORM) {
			pi = rule_premise(synthesis, pi, 0);
			if (!pi) goto unsupported;
			job->binder = context_binding(pi);
		}
		if (!job->binder) {
			if (pi->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, pi); return; }
			finish(synthesis, job, pi->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : pi->status);
			return;
		}
	}
	if (!job->left) job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 1));
	if (!job->left) goto unsupported;
	if (await_dependency(synthesis, job, job->left)) return;
	job->type_structure = pg_lambda(synthesis->typing->graph, job->binder, job->left->type_structure);
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static void term_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (accepted_structure(synthesis, job, producer)) return;
	if (producer->role != DERIVATION_JOB) {
		if (!job->left) {
			struct pg_synthesis_job *prepared;
			if (await_source_preparation(synthesis, job, producer, &prepared)) return;
			/* Classifier conversion and post-checking never rewrite the subject. */
			if (producer->role == CLASSIFIER_JOB || producer->role == EXPECT_JOB)
				prepared = (void *)producer->inputs[producer->role == CLASSIFIER_JOB ? 1 : 0];
			if (!prepared) goto unsupported;
			job->left = pg_synthesis_term_structure(synthesis, prepared);
		}
		forward_structure(synthesis, job);
		return;
	}
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	const struct pg_object *operation = NULL;
	if (input) {
		if (input->rule == PG_LAMBDA_INTRO) { lambda_structure_step(synthesis, job, producer); return; }
		switch (input->rule) {
		case PG_APP_ELIM: case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM:
			compound_structure_step(synthesis, job, producer, input); return;
		default: break;
		}
		if (input->rule == PG_HOST_TYPE_FORM || input->rule == PG_HOST_VALUE_INTRO || input->rule == PG_HOST_FUNCTION_INTRO) {
			job->type_structure = pg_reference(synthesis->typing->graph, input->parameters.constant);
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
		if (input->rule == PG_UNIVERSE_FORM) {
			job->type_structure = pg_universe(synthesis->typing->graph, input->parameters.level);
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
		if (type_structure_rule(input->rule)) {
			type_rule_structure_step(synthesis, job, producer, input);
			return;
		}
		if (input->rule == PG_VARIABLE) {
			job->type_structure = pg_reference(synthesis->typing->graph, input->parameters.binder);
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
		switch (input->rule) {
		case PG_RETURN_INTRO: operation = &pg_return_operation; break;
		case PG_THUNK_INTRO: operation = &pg_thunk_operation; break;
		case PG_FORCE_ELIM: operation = &pg_force_operation; break;
		default: break;
		}
		if (!job->left && input->rule == PG_VALUE_FROM_TYPE)
			job->left = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 0));
		if (!job->left && input->rule == PG_CONTEXT_PROJECTION)
			job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 1));
		if (!job->left && input->rule == PG_EFFECT_SUBSUMPTION)
			job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 0));
		if (!job->left && operation)
			job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 0));
	}
	if (job->left) {
		if (await_dependency(synthesis, job, job->left)) return;
		const struct pg_term *term = pg_synthesis_type_structure_result(job->left);
		job->type_structure = operation ? pg_application(synthesis->typing->graph,
			pg_reference(synthesis->typing->graph, operation), term) : term;
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
unsupported:
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	finish(synthesis, job, producer->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : producer->status);
}

/* Vary the complete Identity boundary, not only a value in its left fiber.
 * The ordinary lifted telescope retains the outer context used by the result
 * family. Every endpoint image is checked by substitution pairing. */
static struct transport_scope *transport_scope_start(struct pg_typing *typing,
	const struct pg_evidence *base, const struct pg_identity_boundary *boundary)
{
	if (!base) return NULL;
	size_t count = boundary->left_substitution ? boundary->path_count : 0;
	if (count > (SIZE_MAX - sizeof(struct transport_scope)) / sizeof(const struct pg_evidence *)) return NULL;
	struct transport_scope *work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->extensions));
	if (!work) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(base, 0);
	work->left = work->right = base;
	work->count = count;
	if (boundary->left_substitution) {
		const struct pg_evidence *ls = boundary->left_substitution;
		const struct pg_evidence *source = pg_evidence_premise(ls, 0);
		size_t arity = pg_evidence_premise_count(ls) - 2;
		if (count > arity) return NULL;
		for (size_t i = count; i; --i, source = pg_evidence_premise(source, 0))
			work->extensions[i - 1] = source;
		const struct pg_evidence *map = pg_prove_substitution(typing, source, pg_evidence_premise(ls, 1),
			arity - count, pg_evidence_premises(ls) + 2);
		work->query = pg_substitution_rebase_request(typing, context, map);
	} else {
		work->map = pg_prove_substitution_projection(typing, context, context);
		if (!work->map) return NULL;
		work->query = pg_rebase_request(typing, context, boundary->family);
	}
	return work->query ? work : NULL;
}

static int transport_scope_advance(struct pg_typing *typing, struct transport_scope *work,
	const struct pg_identity_boundary *boundary,
	const struct pg_evidence *left_value, const struct pg_evidence *right_value)
{
	if (work->extended) return 1;
	const struct pg_evidence *type;
	if (work->query) {
		int status = pg_typed_query_advance(work->query, 1);
		if (status <= 0) return status;
		const struct pg_evidence *result = pg_typed_query_result(work->query);
		work->query = NULL;
		if (!result) return -1;
		if (!boundary->left_substitution) { type = result; goto extend; }
		work->map = result;
	}
	if (work->next < work->count) {
		const struct pg_evidence *field = work->extensions[work->next];
		if (!work->lift) work->lift = pg_context_lift_request(typing,
			pg_evidence_context_map(work->map), pg_evidence_context(field), pg_binder(typing->graph));
		enum pg_substitution_status status = pg_context_lift_advance(work->lift, 1);
		if (status == PG_SUBSTITUTION_PENDING) return 0;
		if (status != PG_SUBSTITUTION_DONE) return -1;
		const struct pg_context_map *lifted = pg_context_lift_result(work->lift);
		work->map = pg_prove_substitution_lift(typing, work->map, field, lifted->destination->binder);
		if (!work->map) return -1;
		const struct pg_evidence *extension = pg_evidence_premise(work->map, 1);
		size_t image = pg_evidence_premise_count(boundary->left_substitution) - work->count + work->next;
		work->left = pg_prove_substitution_pair(typing, work->left, extension,
			pg_evidence_premise(boundary->left_substitution, image));
		work->right = pg_prove_substitution_pair(typing, work->right, extension,
			pg_evidence_premise(boundary->right_substitution, image));
		if (!work->left || !work->right) return -1;
		++work->next;
		work->lift = NULL;
		return 0;
	}
	if (!work->action) work->action = pg_occurrence_action_request(typing,
		pg_evidence_context_map(work->map), pg_evidence_subject(boundary->family));
	enum pg_substitution_status status = pg_occurrence_action_advance(work->action, 1);
	if (status == PG_SUBSTITUTION_PENDING) return 0;
	if (status != PG_SUBSTITUTION_DONE) return -1;
	type = pg_prove_reindex(typing, work->map, boundary->family);
extend:;
	const struct pg_evidence *extended = pg_prove_context_extension(typing,
		pg_evidence_premise(work->map, 1), pg_binder(typing->graph), type);
	if (!extended) return -1;
	work->left = pg_prove_substitution_pair(typing, work->left, extended, left_value);
	work->right = pg_prove_substitution_pair(typing, work->right, extended, right_value);
	if (!work->left || !work->right) return -1;
	work->extended = extended;
	return 1;
}

/* Prepare each telescope through the same producer used by source constructors.
 * Branch bodies are built only after all scopes are ready. */
static int transport_constructor_scopes(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *parent, struct transport_scope *work, const struct pg_inductive_instance *instance,
	const struct pg_evidence *parameters)
{
	size_t count = pg_data_constructor_count(instance->schema);
	if (!work->branches) {
		if (count > SIZE_MAX / sizeof(*work->branches)) return -1;
		work->branches = pg_alloc(synthesis->typing->graph, count * sizeof(*work->branches));
		if (count && !work->branches) return -1;
		for (size_t i = 0; i < count; ++i) {
			work->branches[i] = pg_synthesis_constructor_scope(synthesis,
				pg_synthesis_evidence(synthesis, instance->formation),
				pg_data_constructor(pg_data_schema_layout(instance->schema), i),
				pg_synthesis_evidence(synthesis, parameters));
			if (!work->branches[i]) return -1;
		}
	}
	for (; work->scoped < count; ++work->scoped) {
		struct pg_synthesis_job *branch = work->branches[work->scoped];
		if (branch->status == PG_SYNTHESIS_PENDING) { depend(synthesis, parent, branch); return 0; }
		if (branch->status != PG_SYNTHESIS_DONE) return -1;
	}
	return 1;
}

static void constructor_transport_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->value_job) { forward_proof(synthesis, job, job->value_job); return; }
	const struct pg_evidence *inputs[6];
	for (size_t i = 0; i < 6; ++i) {
		struct pg_synthesis_job *input = (void *)job->inputs[i];
		if (await_dependency(synthesis, job, input)) return;
		inputs[i] = input->result;
		if (!inputs[i]) goto rejected;
	}
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = inputs[0], *value = inputs[4], *target = inputs[5];
	const struct pg_object *field = job->inputs[6];
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) goto rejected;
	for (size_t i = 1; i < 6; ++i)
		if (pg_evidence_context(inputs[i]) != pg_evidence_context(context)) goto rejected;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) goto rejected;
	if (pg_evidence_judgement(target) != (field ? PG_JUDGEMENT_VALUE : PG_JUDGEMENT_VALUE_TYPE)) goto rejected;
	if (!job->left) job->left = pg_synthesis_normalize_jobs(synthesis, (void *)job->inputs[0], (void *)job->inputs[1], PG_REDUCTION_WHNF);
	if (!job->right) job->right = pg_synthesis_normalize_jobs(synthesis, (void *)job->inputs[0], (void *)job->inputs[2], PG_REDUCTION_WHNF);
	struct pg_synthesis_job *endpoints[] = {job->left, job->right};
	const struct pg_object *constructors[2];
	for (size_t i = 0; i < 2; ++i) {
		struct pg_synthesis_job *endpoint = endpoints[i];
		if (!endpoint) goto error;
		if (await_dependency(synthesis, job, endpoint)) return;
		if (pg_evidence_judgement(endpoint->result) != PG_JUDGEMENT_VALUE) goto rejected;
		const struct pg_term *head = pg_evidence_subject(endpoint->result)->core;
		while (head->kind == PG_APPLICATION) head = head->as.application.function;
		if (head->kind != PG_REFERENCE) goto unsupported;
		constructors[i] = head->as.reference;
	}
	struct pg_identity_boundary boundary;
	const struct pg_evidence *identity = pg_identity_formation(typing, pg_prove_classifier(typing, context, inputs[3]));
	if (!pg_identity_boundary_view(identity, &boundary)) goto unsupported;
	if (!job->transport_scope) job->transport_scope = transport_scope_start(typing,
		pg_prove_substitution_projection(typing, context, context), &boundary);
	if (!job->transport_scope) goto unsupported;
	int status = transport_scope_advance(typing, job->transport_scope, &boundary, inputs[1], inputs[2]);
	if (!status) { enqueue(synthesis, job); return; }
	if (status < 0) goto unsupported;
	const struct pg_evidence *left = job->transport_scope->left, *right = job->transport_scope->right;
	const struct pg_evidence *extended = job->transport_scope->extended;
	const struct pg_evidence *type = pg_evidence_premise(extended, 1);
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, type, &instance)) goto unsupported;
	const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
	size_t positions[2];
	for (size_t i = 0; i < 2; ++i)
		if (!pg_data_constructor_position(layout, constructors[i], &positions[i])) goto unsupported;
	if (field ? positions[0] != positions[1] : positions[0] == positions[1]) goto unsupported;
	if (field) {
		const struct pg_context *c = pg_evidence_context(pg_data_schema_fields(instance.schema, constructors[0]));
		const struct pg_context *prefix = pg_evidence_context(pg_evidence_premise(instance.formation, 0));
		while (c != prefix && c->binder != field) c = c->parent;
		if (c == prefix) goto rejected;
	}
	const struct pg_evidence *parameters = pg_prove_substitution_compose(typing, instance.parameters,
		pg_prove_substitution_projection(typing, pg_evidence_premise(extended, 0), extended));
	status = transport_constructor_scopes(synthesis, job, job->transport_scope, &instance, parameters);
	if (!status) return;
	if (status < 0) goto unsupported;
	const struct pg_evidence *field_type = NULL;
	if (field) {
		field_type = pg_prove_classifier(typing, context, value);
		target = pg_prove_identity_type(typing, field_type, value, target);
		value = pg_prove_reflexivity(typing, field_type, value);
		if (!target || !value) goto unsupported;
	}
	const struct pg_evidence *source = pg_prove_classifier(typing, context, value);
	if (!source) goto unsupported;
	size_t count = pg_data_constructor_count(instance.schema);
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
	const struct pg_evidence **branches = malloc(count * sizeof(*branches));
	if (!branches) goto error;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *map = job->transport_scope->branches[i]->result;
		const struct pg_evidence *fields = map ? pg_evidence_premise(map, 1) : NULL;
		const struct pg_evidence *branch;
		if (field && i == positions[0]) {
			/* The caller chooses which endpoint occurrence varies. This explicit
			 * injectivity theorem is not automatic result-type inference. */
			const struct pg_evidence *image = pg_substitution_image(typing, map, field);
			branch = image ? pg_prove_identity_type(typing, pg_prove_projection(typing, fields, field_type),
				pg_prove_projection(typing, fields, inputs[4]), image) : NULL;
		} else branch = pg_prove_projection(typing, fields, i == positions[0] ? source : target);
		for (; branch && pg_evidence_context(fields) != pg_evidence_context(extended); fields = pg_evidence_premise(fields, 0))
			branch = pg_prove_family_abstraction(typing, fields, branch);
		branches[i] = branch;
	}
	const struct pg_evidence *family = pg_prove_type_case(typing, instance.formation,
		parameters, pg_prove_variable(typing, extended, pg_evidence_context(extended)->binder), count, branches);
	free(branches);
	if (!family) goto unsupported;
	if (boundary.path_count >= SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
	size_t path_count = boundary.path_count + 1;
	struct pg_synthesis_job **paths = malloc(path_count * sizeof(*paths));
	if (!paths) goto error;
	for (size_t i = 0; i < boundary.path_count; ++i)
		paths[i] = pg_synthesis_evidence(synthesis, pg_prove_projection(typing, context, boundary.paths[i]));
	paths[boundary.path_count] = (void *)job->inputs[3];
	struct pg_synthesis_job *transport = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), left, right, path_count, paths, pg_synthesis_evidence(synthesis, value), PG_IDENTITY_RIGHT);
	free(paths);
	job->value_job = pg_synthesis_expect(synthesis, transport, pg_synthesis_evidence(synthesis, target));
	if (!job->value_job) goto rejected;
	forward_proof(synthesis, job, job->value_job);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* Result synthesis asks whether the transported classifier can leave a branch
 * scope, not whether it matches a surface expectation. Both goals use the same
 * scoped path search and ordinary transport/checking rules. */
static struct pg_synthesis_job *index_transport_check(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *argument)
{
	struct pg_synthesis_job *target = (void *)job->inputs[2];
	if (job->role == INDEX_RESULT_JOB) {
		struct pg_synthesis_job *context = (void *)job->inputs[0];
		struct pg_synthesis_job *type = pg_synthesis_constant_motive(synthesis, target->result, context->result, argument);
		type = plain_rule(synthesis, PG_RETURN_CONTENT, NULL, 1, &type);
		target = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, type});
	}
	return pg_synthesis_expect(synthesis, argument, target);
}

/* Keep later independent declarations when varying an earlier index. The
 * resulting map is checked, not a partial assignment to discarded variables. */
static struct index_scope *index_transport_scope(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *prefix,
	const struct pg_index *omitted)
{
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(prefix), &count)) return NULL;
	if (count > (SIZE_MAX - sizeof(struct index_scope)) / sizeof(const struct pg_evidence *)) return NULL;
	struct index_scope *work = pg_alloc(typing->graph, sizeof(*work) + count * sizeof(*work->fields));
	if (!work) return NULL;
	work->prefix = prefix;
	work->count = count;
	work->map = pg_prove_substitution_projection(typing, prefix, context);
	const struct pg_evidence *extension = context;
	for (size_t i = count; i; --i, extension = pg_evidence_premise(extension, 0)) {
		const struct pg_object *binder = pg_evidence_context(extension)->binder;
		struct pg_index_entry *entry = pg_index_candidates(omitted, (uintptr_t)binder);
		while (entry && entry->hash != (uintptr_t)binder) entry = entry->next;
		if (!entry && pg_evidence_rule(extension) == PG_CONTEXT_EXTEND) work->fields[i - 1] = extension;
	}
	return work;
}

/* One existing rebase-query transition per turn. Failed domains are omitted
 * exactly as in the synchronous telescope check; later fields still see the
 * accepted prefix built so far. No candidate restarts its field scan. */
static int index_scope_advance(struct pg_typing *typing, struct index_scope *work)
{
	if (!work->map) return -1;
	if (work->next == work->count) return 1;
	const struct pg_evidence *field = work->fields[work->next];
	if (field) {
		const struct pg_evidence *scope = pg_evidence_premise(work->map, 0);
		if (!work->query) work->query = pg_rebase_request(typing, scope, pg_evidence_premise(field, 1));
		if (!pg_typed_query_advance(work->query, 1)) return 0;
		const struct pg_evidence *domain = pg_typed_query_result(work->query);
		work->query = NULL;
		if (domain) {
			const struct pg_object *binder = pg_evidence_context(field)->binder;
			const struct pg_evidence *extension = pg_prove_context_extension(typing, scope, binder, domain);
			work->map = pg_prove_substitution_pair(typing, work->map, extension,
				pg_prove_variable(typing, pg_evidence_premise(work->map, 1), binder));
		}
	}
	++work->next;
	return work->map ? work->next == work->count : -1;
}

/* Factor the classifier through a typed index endpoint, then transport along
 * its identity. Pattern inversion constructs
 * checked substitutions; it never rewrites a classifier's raw Core in place. */
static struct pg_synthesis_job *index_transport_candidate(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job,
	const struct pg_evidence *lv, const struct pg_evidence *rv, enum pg_identity_direction direction,
	const struct pg_evidence *normalized_from, int *pending)
{
	if (!lv || !rv) return NULL;
	struct pg_typing *typing = synthesis->typing;
	struct index_transport_state *state = job->index_transport;
	const struct pg_evidence *context = ((struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_term *from = pg_evidence_subject(direction == PG_IDENTITY_LEFT ? rv : lv)->core;
	struct index_scope **slot = &state->scopes[direction == PG_IDENTITY_RIGHT];
	if (!*slot && from->kind == PG_REFERENCE && from->as.reference->kind == PG_BINDER) {
		const struct pg_evidence *extension = context;
		while (pg_evidence_context(extension) && pg_evidence_context(extension)->binder != from->as.reference)
			extension = pg_evidence_premise(extension, 0);
		if (!pg_evidence_context(extension) || pg_evidence_rule(extension) != PG_CONTEXT_EXTEND) return NULL;
		const struct pg_evidence *prefix = pg_evidence_premise(extension, 0);
		struct pg_index omitted = {0};
		struct pg_index_entry entry;
		if (pg_index_init(&omitted)) return NULL;
		if (pg_index_insert(&omitted, &entry, (uintptr_t)from->as.reference)) {
			pg_index_destroy(&omitted); return NULL;
		}
		*slot = index_transport_scope(typing, context, prefix, &omitted);
		pg_index_destroy(&omitted);
		if (*slot) (*slot)->domain = pg_evidence_premise(extension, 1);
	} else if (!*slot) {
		*slot = index_transport_scope(typing, context, context, NULL);
		if (*slot) (*slot)->domain = pg_prove_classifier(typing, context, direction == PG_IDENTITY_LEFT ? rv : lv);
	}
	if (!*slot) return NULL;
	int status = index_scope_advance(typing, *slot);
	if (!status) { *pending = 1; return NULL; }
	if (status < 0) return NULL;
	const struct pg_evidence *map = (*slot)->map, *prefix = (*slot)->prefix;
	const struct pg_evidence *scope = pg_evidence_premise(map, 0);
	const struct pg_evidence *domain = pg_prove_projection(typing, scope, (*slot)->domain);
	const struct pg_evidence *source = pg_prove_context_extension(typing, scope, pg_binder(typing->graph), domain);
	/* Named transport along arbitrary Universe paths stays explicit. */
	struct pg_inductive_instance instance;
	const struct pg_evidence *index_type = pg_prove_classifier(typing, context, lv);
	if (!pg_inductive_instance(typing, index_type, &instance)) return NULL;
	const struct pg_evidence *ls = pg_prove_substitution_pair(typing, map, source, lv);
	const struct pg_evidence *rs = pg_prove_substitution_pair(typing, map, source, rv);
	if (!ls || !rs) return NULL;
	struct pg_synthesis_job *argument = (void *)job->inputs[1];
	const struct pg_evidence *type = pg_prove_classifier(typing, context, argument->result);
	/* Normalization may expose the index image inside the synthesized type.
	 * Only candidate factoring uses that image; transport retains the original
	 * endpoints/path and checks its source type by ordinary conversion. */
	const struct pg_evidence *pattern = normalized_from
		? pg_prove_substitution_pair(typing, map, source, normalized_from)
		: direction == PG_IDENTITY_LEFT ? rs : ls;
	const struct pg_evidence *family = pg_prove_pattern_type(typing, prefix, pattern,
		pg_prove_return_type(typing, type));
	if (!family) return NULL;
	family = pg_prove_return_content(typing, family);
	struct pg_synthesis_job *path = pg_synthesis_evidence(synthesis, state->path);
	struct pg_synthesis_job *transported = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), ls, rs, 1, &path, argument, direction);
	return transported;
}

/* Abstract the result over all fields together. A dependent tail therefore
 * travels with its size, without manufacturing separate homogeneous paths.
 * Only the selected constructor is used at either endpoint; other clauses
 * merely make the type family total and carry no inhabitance assertion. */
static struct index_scope *index_constructor_scope(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *value, const struct pg_object *constructor)
{
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_prove_classifier(typing, context, value), &instance)) return NULL;
	const struct pg_evidence *schema_fields = pg_data_schema_fields(instance.schema, constructor);
	const struct pg_evidence *schema_prefix = pg_evidence_premise(instance.formation, 0);
	size_t count;
	if (!schema_fields || pg_context_extension_size(pg_evidence_context(schema_fields),
		pg_evidence_context(schema_prefix), &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *) || count > SIZE_MAX / sizeof(struct pg_index_entry)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index omitted = {0};
	struct index_scope *result = NULL;
	const struct pg_evidence **values = pg_alloc(typing->graph, count * sizeof(*values));
	struct pg_index_entry *entries = pg_alloc(&temporary, count * sizeof(*entries));
	if (count && (!values || !entries)) goto done;
	if (pg_index_init(&omitted)) goto done;
	for (size_t i = count; i; --i, schema_fields = pg_evidence_premise(schema_fields, 0)) {
		values[i - 1] = pg_prove_constructor_field(typing, value, pg_evidence_context(schema_fields)->binder);
		if (!values[i - 1]) goto done;
		const struct pg_term *core = pg_evidence_subject(values[i - 1])->core;
		if (core->kind == PG_REFERENCE && core->as.reference->kind == PG_BINDER &&
			pg_index_insert(&omitted, &entries[i - 1], (uintptr_t)core->as.reference)) goto done;
	}
	const struct pg_evidence *prefix = context;
	for (const struct pg_evidence *scope = context; pg_evidence_context(scope); scope = pg_evidence_premise(scope, 0)) {
		uintptr_t binder = (uintptr_t)pg_evidence_context(scope)->binder;
		struct pg_index_entry *entry = pg_index_candidates(&omitted, binder);
		while (entry && entry->hash != binder) entry = entry->next;
		if (entry) prefix = pg_evidence_premise(scope, 0);
	}
	result = index_transport_scope(typing, context, prefix, &omitted);
	if (result) { result->values = values; result->value_count = count; }
done:
	pg_index_destroy(&omitted);
	pg_graph_destroy(&temporary);
	return result;
}

static struct pg_synthesis_job *index_constructor_candidate(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, enum pg_identity_direction direction, int *pending)
{
	struct pg_typing *typing = synthesis->typing;
	struct index_transport_state *state = job->index_transport;
	const struct pg_evidence *context = ((struct pg_synthesis_job *)job->inputs[0])->result;
	const struct pg_evidence *value = state->normal[direction == PG_IDENTITY_LEFT]->result;
	const struct pg_term *head = pg_evidence_subject(value)->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	if (head->kind != PG_REFERENCE) return NULL;
	const struct pg_object *constructor = head->as.reference;
	struct index_scope **slot = &state->scopes[direction == PG_IDENTITY_RIGHT];
	if (!*slot) *slot = index_constructor_scope(typing, context, value, constructor);
	if (!*slot) return NULL;
	int status = index_scope_advance(typing, *slot);
	if (!status) { enqueue(synthesis, job); *pending = 1; return NULL; }
	if (status < 0) return NULL;
	const struct pg_evidence *base = (*slot)->map, *prefix = (*slot)->prefix;
	const struct pg_evidence *const *values = (*slot)->values;
	size_t count = (*slot)->value_count;
	struct pg_identity_boundary boundary;
	const struct pg_evidence *identity = pg_identity_formation(typing, pg_prove_classifier(typing, context, state->path));
	if (!pg_identity_boundary_view(identity, &boundary)) return NULL;
	if (!(*slot)->boundary) (*slot)->boundary = transport_scope_start(typing, base, &boundary);
	if (!(*slot)->boundary) return NULL;
	status = transport_scope_advance(typing, (*slot)->boundary, &boundary, state->endpoints[0], state->endpoints[1]);
	if (!status) { enqueue(synthesis, job); *pending = 1; return NULL; }
	if (status < 0) return NULL;
	const struct pg_evidence *extended = (*slot)->boundary->extended;
	const struct pg_evidence *left = (*slot)->boundary->left, *right = (*slot)->boundary->right;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_evidence_premise(extended, 1), &instance)) return NULL;
	const struct pg_evidence *base_context = pg_evidence_premise(base, 0);
	struct pg_typed_query *parameters = pg_substitution_rebase_request(typing, base_context, instance.parameters);
	status = pg_typed_query_advance(parameters, 1);
	if (!status) { enqueue(synthesis, job); *pending = 1; return NULL; }
	const struct pg_evidence *parameter_map = pg_typed_query_result(parameters);
	if (!parameter_map) return NULL;
	status = transport_constructor_scopes(synthesis, job, (*slot)->boundary, &instance, parameter_map);
	if (!status) { *pending = 1; return NULL; }
	if (status < 0) return NULL;
	struct pg_graph temporary = {0};
	struct pg_synthesis_job *result = NULL;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	if (count && !extensions) goto done;
	const struct pg_data_layout *layout = pg_data_schema_layout(instance.schema);
	size_t branch_count = pg_data_constructor_count(instance.schema);
	if (branch_count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	const struct pg_evidence **branches = pg_alloc(&temporary, branch_count * sizeof(*branches));
	if (!branches) goto done;
	struct pg_synthesis_job *argument = (void *)job->inputs[1];
	const struct pg_evidence *type = pg_prove_classifier(typing, context, argument->result);
	for (size_t i = 0; i < branch_count; ++i) {
		const struct pg_object *label = pg_data_constructor(layout, i);
		const struct pg_evidence *map = (*slot)->boundary->branches[i]->result;
		const struct pg_evidence *fields = pg_evidence_premise(map, 1), *branch;
		if (label == constructor) {
			const struct pg_evidence *scope = fields;
			for (size_t j = count; j; --j, scope = pg_evidence_premise(scope, 0)) extensions[j - 1] = scope;
			if (pg_evidence_context(scope) != pg_evidence_context(base_context)) goto done;
			const struct pg_evidence *pattern = base;
			for (size_t j = 0; pattern && j < count; ++j)
				pattern = pg_prove_substitution_pair(typing, pattern, extensions[j], values[j]);
			branch = pg_prove_pattern_type(typing, prefix, pattern, pg_prove_return_type(typing, type));
			branch = pg_prove_return_content(typing, branch);
		} else branch = pg_prove_universe(typing, fields, 0);
		for (; branch && pg_evidence_context(fields) != pg_evidence_context(base_context); fields = pg_evidence_premise(fields, 0))
			branch = pg_prove_family_abstraction(typing, fields, branch);
		branches[i] = pg_prove_projection(typing, extended, branch);
		if (!branches[i]) goto done;
	}
	parameter_map = pg_prove_substitution_compose(typing, parameter_map,
		pg_prove_substitution_projection(typing, base_context, extended));
	const struct pg_evidence *family = pg_prove_type_case(typing, instance.formation, parameter_map,
		pg_prove_variable(typing, extended, pg_evidence_context(extended)->binder), branch_count, branches);
	if (!family || boundary.path_count >= SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto done;
	size_t path_count = boundary.path_count + 1;
	struct pg_synthesis_job **paths = pg_alloc(&temporary, path_count * sizeof(*paths));
	if (!paths) goto done;
	for (size_t i = 0; i < boundary.path_count; ++i)
		paths[i] = pg_synthesis_evidence(synthesis, pg_prove_projection(typing, context, boundary.paths[i]));
	paths[boundary.path_count] = pg_synthesis_evidence(synthesis, state->path);
	struct pg_synthesis_job *transported = pg_synthesis_family_transport_jobs(synthesis,
		pg_synthesis_evidence(synthesis, family), left, right, path_count, paths, argument, direction);
	const struct pg_evidence *target = pg_prove_reindex(typing, direction == PG_IDENTITY_LEFT ? left : right, family);
	result = pg_synthesis_expect(synthesis, transported, pg_synthesis_normalize(synthesis, context, target));
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* Continue through several checked paths only when the classifier loses a
 * dependency that the destination cannot mention. This finite measure prevents
 * left/right transport cycles; failure still explores the other candidates. */
static int index_transport_progress(struct pg_synthesis_job *job,
	const struct pg_evidence *transported, int *progress)
{
	const struct pg_synthesis_job *context = job->inputs[0], *argument = job->inputs[1], *target = job->inputs[2];
	*progress = 0;
	if (!target->result) return 1;
	const struct pg_term *before = pg_evidence_classifier(argument->result);
	const struct pg_term *after = pg_evidence_classifier(transported);
	if (!before || !after) return 1;
	struct index_progress *work = &job->index_transport->progress;
	if (!work->next) {
		work->field = pg_evidence_context(context->result);
		work->counts[0] = work->counts[1] = 0;
		work->next = 1;
	}
	if (work->field) {
		if (work->next == 1 && job->role == INDEX_RESULT_JOB) {
			work->next = pg_context_lookup(pg_evidence_context(target->result), work->field->binder) ? 4 : 2;
		} else {
			const struct pg_term *term = work->next == 1
				? pg_evidence_subject(target->result)->core : work->next == 2 ? before : after;
			if (!work->comparison.state && pg_independence_init(&work->comparison,
				term, work->field->binder)) return -1;
			enum pg_comparison_status status = pg_comparison_advance(&work->comparison, 1);
			if (status == PG_COMPARISON_PENDING) return 0;
			pg_comparison_destroy(&work->comparison);
			if (status == PG_COMPARISON_ERROR) return -1;
			if (work->next == 1) work->next = status == PG_COMPARISON_EQUAL ? 2 : 4;
			else {
				work->counts[work->next - 2] += status == PG_COMPARISON_DIFFERENT;
				++work->next;
			}
		}
		if (work->next == 4) {
			work->field = work->field->parent;
			work->next = 1;
		}
		if (work->field) return 0;
	}
	*progress = work->counts[1] < work->counts[0];
	work->next = 0;
	return 1;
}

static void index_transport_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) job->left = index_transport_check(synthesis, job, (void *)job->inputs[1]);
	if (!job->left) goto error;
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_REJECTED &&
		!(job->role == INDEX_RESULT_JOB && job->left->status == PG_SYNTHESIS_UNSUPPORTED)) {
		forward_proof(synthesis, job, job->left); return;
	}
	struct pg_synthesis_job *cj = (void *)job->inputs[0], *argument = (void *)job->inputs[1];
	if (await_dependency(synthesis, job, cj)) return;
	if (argument->status != PG_SYNTHESIS_DONE || pg_evidence_judgement(argument->result) != PG_JUDGEMENT_VALUE) goto rejected;
	struct pg_typing *typing = synthesis->typing;
	const struct pg_evidence *context = cj->result;
	if (!job->index_transport) {
		job->index_transport = pg_alloc(typing->graph, sizeof(*job->index_transport));
		if (!job->index_transport) goto error;
		job->index_transport->cursor = context;
		job->index_transport->candidate_next = 2;
	}
	struct index_transport_state *state = job->index_transport;
	if (state->candidate_next < 2) {
		size_t index = state->candidate_next;
		struct pg_synthesis_job *candidate = state->candidates[index];
		if (candidate) {
			if (candidate->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, candidate); return; }
			if (candidate->status == PG_SYNTHESIS_ERROR) goto error;
			if (candidate->status == PG_SYNTHESIS_DONE) {
				if (!state->checks[index]) {
					int progress;
					int status = index_transport_progress(job, candidate->result, &progress);
					if (!status) { enqueue(synthesis, job); return; }
					if (status < 0) goto error;
					const void *inputs[] = {job->inputs[0], candidate, job->inputs[2]};
					state->checks[index] = progress ? request_inputs(synthesis, job->role, 3, inputs)
						: index_transport_check(synthesis, job, candidate);
				}
				struct pg_synthesis_job *check = state->checks[index];
				if (!check) goto error;
				if (check->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, check); return; }
				if (check->status == PG_SYNTHESIS_DONE) { forward_proof(synthesis, job, check); return; }
				if (check->status == PG_SYNTHESIS_ERROR) goto error;
			}
		}
		state->checks[index] = NULL;
		++state->candidate_next;
		enqueue(synthesis, job); return;
	}
	if (state->constructor_checked) goto next_context;
	if (!pg_evidence_context(state->cursor)) goto rejected;
	if (!state->path) {
		if (pg_evidence_rule(state->cursor) != PG_CONTEXT_EXTEND) goto next_context;
		const struct pg_evidence *formation = pg_identity_formation(typing,
			pg_evidence_premise(state->cursor, 1));
		struct pg_identity_boundary boundary;
		if (!pg_identity_boundary_view(formation, &boundary)) goto next_context;
		state->endpoints[0] = pg_prove_projection(typing, context, boundary.left);
		state->endpoints[1] = pg_prove_projection(typing, context, boundary.right);
		state->path = pg_prove_variable(typing, context, pg_evidence_context(state->cursor)->binder);
		if (!state->endpoints[0] || !state->endpoints[1] || !state->path) goto next_context;
	}
	if (!state->direct_checked) {
		const struct pg_evidence *left = state->endpoints[0];
		const struct pg_evidence *right = state->endpoints[1];
		for (; state->building < 2; ++state->building) {
			size_t i = state->building;
			int pending = 0;
			state->candidates[i] = index_transport_candidate(synthesis, job, left, right,
				i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, NULL, &pending);
			if (pending) { enqueue(synthesis, job); return; }
		}
		state->building = 0;
		state->candidate_next = 0; state->direct_checked = 1;
		enqueue(synthesis, job); return;
	}
	for (size_t i = 0; i < 2; ++i) {
		if (!state->normal[i]) state->normal[i] = pg_synthesis_normalize_jobs(synthesis, cj,
			pg_synthesis_evidence(synthesis, state->endpoints[i]), PG_REDUCTION_WHNF);
		if (!state->normal[i]) goto error;
		if (state->normal[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, state->normal[i]); return; }
		if (state->normal[i]->status == PG_SYNTHESIS_ERROR) goto error;
		if (state->normal[i]->status != PG_SYNTHESIS_DONE) goto next_context;
	}
	if (!state->normalized_checked) {
		for (; state->building < 2; ++state->building) {
			size_t i = state->building;
			const struct pg_evidence *image = state->normal[1 - i]->result;
			state->candidates[i] = NULL;
			if (pg_alpha_equal(pg_evidence_subject(image)->core,
				pg_evidence_subject(state->endpoints[1 - i])->core) == 1) continue;
			int pending = 0;
			state->candidates[i] = index_transport_candidate(synthesis, job,
				state->endpoints[0], state->endpoints[1], i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, image, &pending);
			if (pending) { enqueue(synthesis, job); return; }
		}
		state->building = 0;
		state->scopes[0] = state->scopes[1] = NULL;
		state->candidate_next = 0; state->normalized_checked = 1;
		enqueue(synthesis, job); return;
	}
	const struct pg_object *heads[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *head = pg_evidence_subject(state->normal[i]->result)->core;
		while (head->kind == PG_APPLICATION) head = head->as.application.function;
		if (head->kind != PG_REFERENCE) goto next_context;
		heads[i] = head->as.reference;
	}
	if (heads[0] != heads[1]) goto next_context;
	struct pg_inductive_instance instance;
	const struct pg_evidence *type = pg_prove_classifier(typing, context, state->normal[0]->result);
	if (!pg_inductive_instance(typing, type, &instance)) goto next_context;
	size_t ordinal;
	if (!pg_data_constructor_position(pg_data_schema_layout(instance.schema), heads[0], &ordinal)) goto next_context;
	for (; state->building < 2; ++state->building) {
		size_t i = state->building;
		int pending = 0;
		state->candidates[i] = index_constructor_candidate(synthesis, job,
			i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT, &pending);
		if (pending) return;
	}
	state->building = 0;
	state->candidate_next = 0; state->constructor_checked = 1;
	enqueue(synthesis, job); return;
next_context:
	state->cursor = pg_evidence_premise(state->cursor, 0);
	state->path = NULL;
	state->direct_checked = state->normalized_checked = state->constructor_checked = 0;
	state->normal[0] = state->normal[1] = NULL;
	state->scopes[0] = state->scopes[1] = NULL;
	enqueue(synthesis, job); return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void declared_type_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *context = (void *)job->inputs[0];
	const struct pg_object *binder = job->inputs[1];
	const struct pg_evidence *accepted = pg_synthesis_result(context);
	if (accepted) {
		const struct pg_context *declaration = pg_evidence_judgement(accepted) == PG_JUDGEMENT_CONTEXT
			? pg_context_lookup(pg_evidence_context(accepted), binder) : NULL;
		job->type_structure = declaration ? declaration->declared_type : NULL;
		finish(synthesis, job, declaration ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (job->left) goto consume;
	struct pg_synthesis_job *prepared;
	if (await_source_preparation(synthesis, job, context, &prepared)) return;
	if (!job->left && prepared) job->left = request_job(synthesis, DECLARED_TYPE_JOB, prepared, binder);
	if (!job->left && context->role == SCOPE_CONTEXT_JOB)
		job->left = request_job(synthesis, DECLARED_TYPE_JOB, context->inputs[2], binder);
	if (!job->left && context->role == BINDING_JOB)
		job->left = context->binder == binder ? pg_synthesis_type_structure(synthesis, context->right)
			: request_job(synthesis, DECLARED_TYPE_JOB, context->scope->context_job, binder);
	if (!job->left && context->role == DERIVATION_JOB) {
		const struct pg_derivation_input *input = context->inputs[0];
		if (input->rule == PG_CONTEXT_EXTEND) {
			struct pg_synthesis_job *premise = rule_premise(synthesis, context,
				input->parameters.binder == binder ? 1 : 0);
			if (!premise) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			job->left = input->parameters.binder == binder
				? pg_synthesis_type_structure(synthesis, premise)
				: request_job(synthesis, DECLARED_TYPE_JOB, premise, binder);
		}
	}
consume:
	if (job->left) {
		if (await_dependency(synthesis, job, job->left)) return;
		job->type_structure = job->left->type_structure;
		if (context->role == BINDING_JOB && context->binder == binder && family_parameter_annotation(job->type_structure))
			job->type_structure = family_parameter_structure(synthesis->typing->graph, job->type_structure);
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
	finish(synthesis, job, context->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : context->status);
}

static const struct pg_term *continuation_effect_structure(struct pg_synthesis *synthesis,
	const struct pg_term *type, enum pg_totality totality, const struct pg_term *row)
{
	const struct pg_term *following, *result;
	enum pg_totality next_totality;
	const struct pg_term *codomain = pg_pi_constant_codomain(type);
	if (!codomain) return NULL;
	if (!pg_computation_type_spine_view(codomain, &next_totality, &following, &result)) return NULL;
	if (next_totality < totality) totality = next_totality;
	return pg_computation_type_spine(synthesis->typing->graph, totality,
		pg_effect_join_term(synthesis->typing->graph, row, following), result);
}

static void pi_application_structure_step(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_term *type, struct pg_synthesis_job *argument)
{
	if (!job->structural_substitution) {
		const struct pg_term *domain, *codomain;
		const struct pg_object *binder;
		if (!pg_pi_view(type, &domain, &binder, &codomain)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (!job->right) {
			/* A constant codomain does not wait for argument conversion.
			 * Once the argument is requested, dependence is already known. */
			if (pg_term_independent(codomain, binder) == 1) {
				job->type_structure = codomain;
				finish(synthesis, job, PG_SYNTHESIS_DONE);
				return;
			}
			job->right = pg_synthesis_term_structure(synthesis, argument);
		}
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (await_dependency(synthesis, job, job->right)) return;
		struct pg_binding_value image = {binder, pg_synthesis_type_structure_result(job->right)};
		job->structural_substitution = pg_substitution_request(&synthesis->typing->substitutions, codomain, 1, &image);
		if (!job->structural_substitution) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	enum pg_substitution_status status = pg_substitution_advance(job->structural_substitution, 1);
	if (status == PG_SUBSTITUTION_PENDING) { enqueue(synthesis, job); return; }
	job->type_structure = pg_substitution_result(job->structural_substitution);
	finish(synthesis, job, status == PG_SUBSTITUTION_DONE ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void classifier_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (accepted_structure(synthesis, job, producer)) return;
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	if (job->left) goto consume;
	struct pg_synthesis_job *prepared;
	if (await_source_preparation(synthesis, job, producer, &prepared)) return;
	if (producer->role == CLASSIFIER_JOB) {
		job->left = pg_synthesis_classifier_structure(synthesis, (void *)producer->inputs[1]);
		goto consume;
	}
	if (producer->role == EXPECT_JOB || producer->role == INDEX_TRANSPORT_JOB) {
		job->left = pg_synthesis_type_structure(synthesis,
			(void *)producer->inputs[producer->role == INDEX_TRANSPORT_JOB ? 2 : 1]);
		goto consume;
	}
	if (prepared) {
		job->left = pg_synthesis_classifier_structure(synthesis, prepared);
		goto consume;
	}
	if (input && input->rule == PG_UNIVERSE_FORM && input->parameters.level != UINT64_MAX) {
		job->type_structure = pg_universe(synthesis->typing->graph, input->parameters.level + 1);
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (input) {
		struct pg_synthesis_job *premise = rule_premise(synthesis, producer, 0);
		if (premise) switch (input->rule) {
		case PG_VARIABLE:
			job->left = request_job(synthesis, DECLARED_TYPE_JOB, premise, input->parameters.binder); break;
		case PG_HOST_VALUE_INTRO: case PG_HOST_FUNCTION_INTRO:
			job->left = pg_synthesis_type_structure(synthesis, premise); break;
		case PG_FORCE_ELIM: case PG_THUNK_INTRO: case PG_APP_ELIM: case PG_RETURN_INTRO: case PG_VALUE_FROM_TYPE: case PG_FOLD_ELIM:
			job->left = pg_synthesis_classifier_structure(synthesis, premise); break;
		case PG_CONTEXT_PROJECTION:
			job->left = pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, producer, 1)); break;
		case PG_REQUEST_INTRO:
			job->left = pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, producer, 3)); break;
		case PG_HANDLER_ELIM:
			job->left = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 2)); break;
		case PG_EFFECT_SUBSUMPTION:
			job->left = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 1)); break;
		case PG_LAMBDA_INTRO:
			job->left = pg_synthesis_type_structure(synthesis, premise); break;
		default: break;
		}
	}
	if (!job->left) goto accepted_classifier;
consume:
	{
		if (await_dependency(synthesis, job, job->left)) return;
		const struct pg_term *type = pg_synthesis_type_structure_result(job->left);
		if (producer->role == CLASSIFIER_JOB) {
			const struct pg_reduction_certificate *receipt = normalization_receipt(synthesis, job, type, PG_REDUCTION_WHNF);
			if (!receipt) return;
			type = pg_reduction_target(receipt);
		}
		if (!input) goto done;
		if (input->rule == PG_REQUEST_INTRO) {
			const struct pg_object *label = input->parameters.operation_label;
			if (!label) goto accepted_classifier;
			const struct pg_effect_row *row = pg_effect_row(synthesis->typing->graph, 1, &label);
			job->type_structure = continuation_effect_structure(synthesis, type,
				PG_TOTALITY_TOTAL, pg_effect_reference(synthesis->typing->graph, row));
			if (!job->type_structure) goto accepted_classifier;
			finish(synthesis, job, PG_SYNTHESIS_DONE);
			return;
		}
		if (input->rule == PG_FOLD_ELIM) {
			if (!job->right) job->right = pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, producer, 1));
			if (await_dependency(synthesis, job, job->right)) return;
			const struct pg_term *row, *value;
			enum pg_totality totality;
			if (!pg_computation_type_spine_view(type, &totality, &row, &value)) goto accepted_classifier;
			job->type_structure = continuation_effect_structure(synthesis, job->right->type_structure, totality, row);
			if (!job->type_structure) goto accepted_classifier;
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
		if (input->rule == PG_APP_ELIM) {
			pi_application_structure_step(synthesis, job, type, rule_premise(synthesis, producer, 1));
			return;
		}
		if (input->rule == PG_FORCE_ELIM) {
			if (!pg_thunk_type_view(type, &type)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		} else if (input->rule == PG_THUNK_INTRO) type = pg_thunk_type(synthesis->typing->graph, type);
		else if (input->rule == PG_RETURN_INTRO) type = pg_computation_type(synthesis->typing->graph,
			input->parameters.totality, pg_effect_row(synthesis->typing->graph, 0, NULL), type);
done:
		job->type_structure = type;
		finish(synthesis, job, type ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
accepted_classifier:
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	finish(synthesis, job, producer->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : producer->status);
}

static void type_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (accepted_structure(synthesis, job, producer)) return;
	if (job->left) goto forward;
	struct pg_synthesis_job *prepared;
	if (await_source_preparation(synthesis, job, producer, &prepared)) return;
	if (producer->role == CLASSIFIER_FORMATION_JOB) {
		job->left = pg_synthesis_classifier_structure(synthesis, (void *)producer->inputs[1]);
		goto forward;
	}
	if (producer->role == DOMAIN_JOB) {
		struct pg_synthesis_job *rule;
		if (await_source_preparation(synthesis, job, producer->left, &rule)) return;
		if (source_value_kind(producer->left) == 2) {
			job->left = pg_synthesis_type_structure(synthesis, producer->left);
			goto forward;
		}
		if (rule && rule->role == DERIVATION_JOB) {
			const struct pg_derivation_input *domain = rule->inputs[0];
			if (domain->rule == PG_VARIABLE) {
				job->left = pg_synthesis_term_structure(synthesis, rule);
				goto forward;
			}
		}
	}
	if (prepared) {
		job->left = pg_synthesis_type_structure(synthesis, prepared);
		goto forward;
	}
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	if (input && input->rule == PG_CONTEXT_PROJECTION) {
		job->left = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 1));
		goto forward;
	}
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	finish(synthesis, job, producer->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : producer->status);
	return;
forward:
	forward_structure(synthesis, job);
}

/* Formation and ordinary term queries share this construction. The type view
 * restricts eligible rules; neither view is acceptance evidence. */
static void type_rule_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer, const struct pg_derivation_input *input)
{
	if (input->rule == PG_PI_FORM) {
		struct pg_synthesis_job *context = rule_premise(synthesis, producer, 0);
		if (!context) goto unsupported;
		const struct pg_object *binder = context_binding(context);
		/* An unfinished context request can expose its declared annotation for
		 * effect equations. This is structure only, never accepted Pi evidence. */
		if (!binder) {
			if (await_dependency(synthesis, job, context)) return;
			goto unsupported;
		}
		if (!job->left) job->left = request_job(synthesis, DECLARED_TYPE_JOB, context, binder);
		if (!job->left) goto unsupported;
		if (await_dependency(synthesis, job, job->left)) return;
		if (!job->right) job->right = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 1));
		if (!job->right) goto unsupported;
		if (await_dependency(synthesis, job, job->right)) return;
		job->type_structure = pg_pi(synthesis->typing->graph, job->left->type_structure, binder, job->right->type_structure);
		goto done;
	}
	if (!job->left) {
		struct pg_synthesis_job *premise = rule_premise(synthesis, producer, 0);
		job->left = input->rule == PG_TYPE_FROM_VALUE ? pg_synthesis_term_structure(synthesis, premise)
			: pg_synthesis_type_structure(synthesis, premise);
		if (!job->left) goto unsupported;
	}
	if (await_dependency(synthesis, job, job->left)) return;
	const struct pg_term *left = job->left->type_structure;
	switch (input->rule) {
	case PG_RETURN_TYPE_FORM: {
		const struct pg_term *row;
		if (producer->inputs[1]) {
			const struct pg_synthesis_job *effects = producer->inputs[1];
			row = pg_reference(synthesis->typing->graph,
				pg_effect_equation_parameter(effects->inputs[0], producer->inputs[2]));
		} else row = pg_effect_reference(synthesis->typing->graph, input->parameters.effects);
		if (!row) goto unsupported;
		job->type_structure = pg_computation_type_spine(synthesis->typing->graph, input->parameters.totality, row, left);
		break;
	}
	case PG_THUNK_TYPE_FORM:
		job->type_structure = pg_thunk_type(synthesis->typing->graph, left); break;
	case PG_TYPE_FROM_VALUE:
		job->type_structure = left; break;
	case PG_RETURN_CONTENT: {
		const struct pg_term *row;
		enum pg_totality totality;
		if (!pg_computation_type_spine_view(left, &totality, &row, &job->type_structure)) goto unsupported;
		break;
	}
	case PG_PI_DOMAIN: {
		const struct pg_term *codomain;
		const struct pg_object *binder;
		if (!pg_pi_view(left, &job->type_structure, &binder, &codomain)) goto unsupported;
		break;
	}
	case PG_PI_CONSTANT_CODOMAIN: {
		const struct pg_term *domain;
		const struct pg_object *binder;
		if (!pg_pi_view(left, &domain, &binder, &job->type_structure)) goto unsupported;
		if (pg_term_independent(job->type_structure, binder) != 1) goto unsupported;
		break;
	}
	case PG_PI_CODOMAIN:
		pi_application_structure_step(synthesis, job, left, rule_premise(synthesis, producer, 1));
		return;
	default: goto unsupported;
	}
done:
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

struct rule_export {
	const struct pg_synthesis *synthesis;
	struct pg_dag proofs, workers, raw;
	const struct pg_effect_inference *source_effects;
	struct pg_effect_inference *effects;
	int status;
};

static int export_proof_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_evidence *proof = key;
	if (index == pg_evidence_premise_count(proof)) return 0;
	*child = pg_evidence_premise(proof, index);
	return 1;
}

static int export_input_child(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (input->parameters.conversion || input->parameters.reduction) return -1;
	if (index == input->count) return 0;
	*child = input->premises[index];
	return *child ? 1 : -1;
}

static int export_job_child(void *context, const void *key, size_t index, const void **child)
{
	struct rule_export *export = context;
	const struct pg_synthesis_job *job = key;
	if (job->owner != export->synthesis->owner_key) return -1;
	if (job->role == DERIVATION_INPUT_JOB) {
		if (pg_dag_add(&export->raw, job->inputs[0])) return -1;
		return job->inputs[1] ? pg_dag_add(&export->workers, job->inputs[1]) : 0;
	}
	if (job->role == DERIVATION_JOB) {
		const struct pg_derivation_input *input = job->inputs[0];
		if (index < input->count) { *child = job->inputs[index + 3]; return 1; }
		const struct pg_synthesis_job *effects = job->inputs[1];
		return effects ? pg_dag_add(&export->workers, effects->inputs[0]) : 0;
	}
	if (job->status == PG_SYNTHESIS_DONE && job->result)
		return pg_dag_add(&export->proofs, job->result);
	export->status = job->status == PG_SYNTHESIS_PENDING ? 1 : -1;
	return -1;
}

static int export_equation(void *context, const struct pg_effect_equation *equation, const struct pg_effect_row *seed)
{
	struct rule_export *export = context;
	const struct pg_object *parameter = pg_effect_equation_parameter(export->source_effects, equation);
	/* Distinct source workers may not redefine one shared site. */
	if (pg_effect_equation_find(export->effects, parameter)) return -1;
	return !pg_effect_equation_at(export->effects, parameter, seed);
}

static int export_dependency(void *context, const struct pg_effect_equation *source,
	const struct pg_effect_row *mask, const struct pg_effect_equation *target)
{
	struct rule_export *export = context;
	return pg_effect_dependency(export->effects,
		pg_effect_equation_find(export->effects, pg_effect_equation_parameter(export->source_effects, source)), mask,
		pg_effect_equation_find(export->effects, pg_effect_equation_parameter(export->source_effects, target)));
}

static struct pg_derivation_input *export_header(struct pg_graph *storage, const struct pg_derivation_input *header)
{
	if (header->count > (SIZE_MAX - sizeof(*header)) / sizeof(void *)) return NULL;
	struct pg_derivation_input *input = pg_alloc(storage, sizeof(*input) + header->count * sizeof(*input->premises));
	if (input) *input = *header;
	return input;
}

int pg_synthesis_export_rule_closure(const struct pg_synthesis *synthesis,
	struct pg_dag *roots, struct pg_graph *storage, struct pg_effect_inference *effects,
	int require_closed,
	int (*observe)(void *, const struct pg_derivation_input *, const struct pg_effect_inference *),
	void *owner, const struct pg_derivation_input *const **result)
{
	if (!synthesis || !storage || !effects || !result || !roots || roots->child || roots->failed) {
		if (effects) effects->failed = 1;
		return -1;
	}
	if (effects->rows != storage || effects->sealed || effects->failed || effects->row_sources.count) {
		effects->failed = 1;
		return -1;
	}
	struct rule_export export = {.synthesis = synthesis, .effects = effects, .status = -1};
	struct pg_dag jobs = {0};
	if (pg_dag_init(&export.proofs, export_proof_child, NULL) || pg_dag_init(&export.workers, NULL, NULL)
		|| pg_dag_init(&export.raw, export_input_child, NULL)
		|| pg_dag_init(&jobs, export_job_child, &export)) goto done;
	const struct pg_dag_node *last_raw = NULL, *last_proof = NULL, *last_job = NULL, *last_worker = NULL;
	for (const struct pg_dag_node *root = roots->first; root; root = root->next) {
		if (pg_dag_add(&jobs, root->key)) goto done;
		for (const struct pg_dag_node *node = last_worker ? last_worker->next : export.workers.first;
			node; last_worker = node, node = node->next) {
			export.source_effects = node->key;
			if (require_closed && !export.source_effects->sealed) { export.status = 1; goto done; }
			if (pg_effect_inference_visit(node->key, &export, export_equation, export_dependency)) goto done;
			if (observe && observe(owner, NULL, node->key)) goto done;
		}
		if (!observe) continue;
		for (const struct pg_dag_node *node = last_raw ? last_raw->next : export.raw.first;
			node; last_raw = node, node = node->next)
			if (observe(owner, node->key, NULL)) goto done;
		for (const struct pg_dag_node *node = last_proof ? last_proof->next : export.proofs.first;
			node; last_proof = node, node = node->next) {
			struct pg_derivation_input header;
			if (pg_derivation_input_header(node->key, &header) || observe(owner, &header, NULL)) goto done;
		}
		for (const struct pg_dag_node *node = last_job ? last_job->next : jobs.first;
			node; last_job = node, node = node->next) {
			const struct pg_synthesis_job *job = node->key;
			if (job->role != DERIVATION_JOB) continue;
			struct pg_derivation_input header = *(const struct pg_derivation_input *)job->inputs[0];
			const struct pg_synthesis_job *worker = job->inputs[1];
			if (worker) header.effect_parameter = pg_effect_equation_parameter(worker->inputs[0], job->inputs[2]);
			if (observe(owner, &header, NULL)) goto done;
		}
	}
	if (roots->failed) goto done;
	size_t count = roots->count;
	if (jobs.count > SIZE_MAX / sizeof(void *) || export.proofs.count > SIZE_MAX / sizeof(void *)
		|| export.raw.count > SIZE_MAX / sizeof(void *) || count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_derivation_input **proofs = pg_alloc(&jobs.storage, export.proofs.count * sizeof(*proofs));
	const struct pg_derivation_input **inputs = pg_alloc(&jobs.storage, jobs.count * sizeof(*inputs));
	const struct pg_derivation_input **selected = pg_alloc(storage, count * sizeof(*selected));
	const struct pg_derivation_input **raw = pg_alloc(&jobs.storage, export.raw.count * sizeof(*raw));
	if (!proofs || !inputs || !selected || !raw) goto done;
	for (const struct pg_dag_node *node = export.raw.first; node; node = node->next) {
		const struct pg_derivation_input *source = node->key;
		struct pg_derivation_input *input = export_header(storage, source);
		if (!input) goto done;
		for (size_t i = 0; i < input->count; ++i)
			input->premises[i] = raw[pg_dag_find(&export.raw, source->premises[i])->id - 1];
		raw[node->id - 1] = input;
	}
	for (const struct pg_dag_node *node = export.proofs.first; node; node = node->next) {
		struct pg_derivation_input header;
		if (pg_derivation_input_header(node->key, &header)) goto done;
		struct pg_derivation_input *input = export_header(storage, &header);
		if (!input) goto done;
		for (size_t i = 0; i < input->count; ++i) {
			const struct pg_dag_node *premise = pg_dag_find(&export.proofs, pg_evidence_premise(node->key, i));
			input->premises[i] = proofs[premise->id - 1];
		}
		proofs[node->id - 1] = input;
	}
	for (const struct pg_dag_node *node = jobs.first; node; node = node->next) {
		const struct pg_synthesis_job *job = node->key;
		if (job->role == DERIVATION_JOB) {
			struct pg_derivation_input *input = export_header(storage, job->inputs[0]);
			if (!input || input->parameters.conversion || input->parameters.reduction) goto done;
			const struct pg_synthesis_job *worker = job->inputs[1];
			if (worker) input->effect_parameter = pg_effect_equation_parameter(worker->inputs[0], job->inputs[2]);
			for (size_t i = 0; i < input->count; ++i)
				input->premises[i] = inputs[pg_dag_find(&jobs, job->inputs[i + 3])->id - 1];
			inputs[node->id - 1] = input;
		} else if (job->role == DERIVATION_INPUT_JOB)
			inputs[node->id - 1] = raw[pg_dag_find(&export.raw, job->inputs[0])->id - 1];
		else
			inputs[node->id - 1] = proofs[pg_dag_find(&export.proofs, job->result)->id - 1];
	}
	for (const struct pg_dag_node *node = roots->first; node; node = node->next)
		selected[node->id - 1] = inputs[pg_dag_find(&jobs, node->key)->id - 1];
	*result = selected;
	export.status = 0;
done:
	pg_dag_destroy(&jobs);
	pg_dag_destroy(&export.proofs);
	pg_dag_destroy(&export.workers);
	pg_dag_destroy(&export.raw);
	if (export.status) effects->failed = 1;
	return export.status;
}

int pg_synthesis_export_rules(const struct pg_synthesis *synthesis, size_t count,
	struct pg_synthesis_job *const *roots, struct pg_graph *storage,
	struct pg_effect_inference *effects, int require_closed, const struct pg_derivation_input *const **result)
{
	struct pg_dag selected = {0};
	int status = -1;
	if (!storage || !result || (count && !roots) || count > SIZE_MAX / sizeof(void *)) goto done;
	if (pg_dag_init(&selected, NULL, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&selected, roots[i])) goto done;
	const struct pg_derivation_input *const *inputs;
	status = pg_synthesis_export_rule_closure(synthesis, &selected, storage, effects, require_closed, NULL, NULL, &inputs);
	if (status) goto done;
	const struct pg_derivation_input **output = pg_alloc(storage, count * sizeof(*output));
	if (!output) { status = -1; goto done; }
	for (size_t i = 0; i < count; ++i) output[i] = inputs[pg_dag_find(&selected, roots[i])->id - 1];
	*result = output;
done:
	pg_dag_destroy(&selected);
	if (status && effects) effects->failed = 1;
	return status;
}

static void derivation_input_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_derivation_input *input = job->inputs[0];
	struct pg_effect_inference *work = (void *)job->inputs[1];
	if (!job->left) {
		if (!job->derivation_input) {
			if (input->count > (SIZE_MAX - sizeof(struct derivation_input_state)) / sizeof(void *)) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
			job->derivation_input = pg_alloc(synthesis->typing->graph,
				sizeof(*job->derivation_input) + input->count * sizeof(struct pg_synthesis_job *));
			if (!job->derivation_input) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		struct derivation_input_state *state = job->derivation_input;
		if (state->next < input->count) {
			struct pg_synthesis_job *premise = pg_synthesis_derivation_inference(synthesis, input->premises[state->next], work);
			if (!premise) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			if (await_source_preparation(synthesis, job, premise, NULL)) return;
			if (!premise->left) { finish(synthesis, job, premise->status); return; }
			state->premises[state->next++] = premise->left;
			enqueue(synthesis, job);
			return;
		}
		struct pg_derivation_input header = *input;
		struct pg_effect_equation *equation = NULL;
		if (input->effect_parameter) {
			equation = pg_effect_equation_find(work, input->effect_parameter);
			if (!equation) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			header.effect_parameter = NULL;
		}
		job->left = pg_synthesis_rule(synthesis, &header, state->premises, equation ? work : NULL, equation);
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	job->result = job->left->result;
	finish(synthesis, job, job->left->status);
}

static void derivation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_derivation_input *input = job->inputs[0];
	if (!job->derivation) {
		if (input->parameters.conversion || input->parameters.reduction) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (input->count > SIZE_MAX / sizeof(const struct pg_evidence *)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		job->derivation = pg_alloc(synthesis->typing->graph, sizeof(*job->derivation));
		if (!job->derivation) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->derivation->premises = pg_alloc(synthesis->typing->graph, input->count * sizeof(const struct pg_evidence *));
		if (!job->derivation->premises) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct derivation_state *state = job->derivation;
	if (state->next < input->count) {
		struct pg_synthesis_job *p = rule_premise(synthesis, job, state->next);
		if (await_dependency(synthesis, job, p)) return;
		state->premises[state->next++] = p->result;
		enqueue(synthesis, job);
		return;
	}
	int converting = input->rule == PG_TYPE_CONVERSION;
	int normalizing = input->rule == PG_PURE_NORMALIZATION;
	struct pg_derivation_parameters parameters = input->parameters;
	if (job->inputs[1]) {
		struct pg_synthesis_job *effects = (void *)job->inputs[1];
		if (await_dependency(synthesis, job, effects)) return;
		parameters.effects = pg_effect_inference_result(effects->inputs[0], job->inputs[2]);
		if (!parameters.effects) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	if (converting || normalizing) {
		if (input->count != (converting ? 2u : 1u) || !input->source || !input->target) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		const struct pg_evidence *source = state->premises[0];
		const struct pg_occurrence *subject = pg_evidence_subject(source);
		if (!subject) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		const struct pg_term *actual_source = converting ? pg_evidence_classifier(source) : subject->core;
		const struct pg_term *actual_target = NULL;
		if (converting) {
			const struct pg_occurrence *target = pg_evidence_subject(state->premises[1]);
			if (!target) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			actual_target = target->core;
		}
		if (!job->stage) {
			if (!derivation_endpoint(synthesis, job, actual_source, input->source)) return;
			job->stage = 1;
		}
		if (job->stage == 1) {
			if (converting) {
				struct pg_synthesis_job *comparison = request_job(synthesis, CONVERSION_JOB, actual_source, actual_target);
				if (await_dependency(synthesis, job, comparison)) return;
				job->certificate = comparison->certificate;
			} else if (input->reduction_kind == PG_REDUCTION_PREFIX && input->source == input->target) {
				/* The checked source endpoint already establishes alpha identity.
				 * A zero-step prefix makes no normality claim and must not run NF. */
				state->reduction = pg_reduction_identity(synthesis->typing->graph, &pg_pure_policy, input->target);
				if (!state->reduction) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			} else {
				state->reduction = normalization_receipt(synthesis, job, actual_source, input->reduction_kind);
				if (!state->reduction) return;
			}
			job->stage = 2;
		}
		if (normalizing) actual_target = pg_reduction_target(state->reduction);
		if (!derivation_endpoint(synthesis, job, actual_target, input->target)) return;
		parameters.conversion = job->certificate;
		parameters.reduction = state->reduction;
	} else if (input->source || input->target) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	job->result = pg_prove_derivation(synthesis->typing,
		input->rule, &parameters, input->count, state->premises);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}

static void operation_reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (await_dependency(synthesis, job, producer)) return;
	if (producer->role == OPERATION_JOB) {
		job->right = producer;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	if (!job->left) {
		struct pg_synthesis_job *source = operation_origin(producer);
		if (!source) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		job->left = pg_synthesis_operation_reference(synthesis, source);
		depend(synthesis, job, job->left);
		return;
	}
	job->right = job->left->right;
	finish(synthesis, job, job->left->status);
}

static void effect_substitution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *effects = (void *)job->inputs[1];
	if (await_dependency(synthesis, job, effects)) return;
	size_t count = job->input_count - 2;
	if (!job->effect_substitution) {
		if (count > SIZE_MAX / sizeof(struct pg_binding_value)) goto error;
		job->effect_substitution = pg_alloc(synthesis->typing->graph, sizeof(*job->effect_substitution));
		if (!job->effect_substitution) goto error;
		if (count) {
			job->effect_substitution->bindings = malloc(count * sizeof(struct pg_binding_value));
			if (!job->effect_substitution->bindings) goto error;
		}
	}
	struct effect_substitution_state *state = job->effect_substitution;
	if (state->next < count) {
		const struct pg_effect_equation *equation = job->inputs[state->next + 2];
		const struct pg_effect_row *row = pg_effect_inference_result(effects->inputs[0], equation);
		const struct pg_term *value = pg_effect_reference(synthesis->typing->graph, row);
		if (!value) goto error;
		state->bindings[state->next++] = (struct pg_binding_value){
			pg_effect_equation_parameter(effects->inputs[0], equation), value};
		enqueue(synthesis, job);
		return;
	}
	if (!state->work) {
		state->work = pg_substitution_request(&synthesis->typing->substitutions, job->inputs[0], count, state->bindings);
		free(state->bindings); state->bindings = NULL;
		if (!state->work) goto error;
	}
	enum pg_substitution_status status = pg_substitution_advance(state->work, 1);
	if (status == PG_SUBSTITUTION_PENDING) { enqueue(synthesis, job); return; }
	if (status != PG_SUBSTITUTION_DONE) goto error;
	state->result = pg_substitution_result(state->work);
	finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static int atomic_rule_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB) return 0;
	int qualified = job->syntax->kind == PG_SYNTAX_QUALIFIED;
	if (qualified && (block_syntax(job->syntax) || job->syntax->left->kind == PG_SYNTAX_DEFINITIONS)) return 0;
	if (!qualified && job->syntax->kind != PG_SYNTAX_ATOM) return 0;
	struct pg_token token = job->syntax->token;
	if (token.kind == PG_TOKEN_INT || token.kind == PG_TOKEN_TEXT) {
		if (!job->value_job) {
			if (token.kind == PG_TOKEN_INT && (token.integer < INT32_MIN || token.integer > INT32_MAX)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1;
			}
			const struct pg_object *type = pg_host_type(token.kind == PG_TOKEN_INT ? "Int32" : "Text");
			const struct pg_object *literal = token.kind == PG_TOKEN_INT
				? pg_host_integer(synthesis->typing->graph, type, token.integer)
				: pg_host_literal(synthesis->typing->graph, type, token.text_length, (const unsigned char *)token.text);
			if (!literal) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
			struct pg_derivation_input formation = {.rule = PG_HOST_TYPE_FORM, .count = 1, .parameters.constant = type};
			job->left = pg_synthesis_rule(synthesis, &formation, &job->scope->context_job, NULL, NULL);
			struct pg_derivation_input intro = {.rule = PG_HOST_VALUE_INTRO, .count = 1, .parameters.constant = literal};
			job->value_job = pg_synthesis_rule(synthesis, &intro, &job->left, NULL, NULL);
		}
		forward_proof(synthesis, job, job->value_job); return 1;
	}
	if (job->syntax->token.kind == '@') {
		if (!job->left) {
			struct pg_derivation_input input = {.rule = PG_UNIVERSE_FORM, .count = 1};
			job->left = pg_synthesis_rule(synthesis, &input, &job->scope->context_job, NULL, NULL);
		}
		forward_proof(synthesis, job, job->left);
		return 1;
	}
	if (!qualified && job->syntax->token.kind != PG_TOKEN_IDENT) return 0;
	if (job->value_job) {
		if (job->left) job->exports = job->left->exports;
		forward_proof(synthesis, job, job->value_job); return 1;
	}
	if (!job->binder) {
		if (job->left) return 0;
		struct source_reference reference;
		struct pg_synthesis_job *dependency = NULL;
		enum pg_synthesis_status status = resolve_source_reference(synthesis, job, &reference, &dependency);
		if (status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, dependency); return 1; }
		if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return 1; }
		if (named_term_ready(reference.producer)) {
			if (reference.producer->status == PG_SYNTHESIS_DONE &&
				(!reference.producer->result || !pg_evidence_subject(reference.producer->result))) {
				finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return 1;
			}
			job->left = reference.producer;
			job->exports = reference.producer->exports;
			struct pg_synthesis_job *term = source_reference_producer(synthesis, reference.producer);
			job->value_job = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_job *[]){job->scope->context_job, term});
			forward_proof(synthesis, job, job->value_job);
			return 1;
		}
		/* A pending named producer, not the enclosing context, determines when
		 * its polarity becomes available. Subscribe before waiting on the row. */
		if (reference.producer && reference.producer->status == PG_SYNTHESIS_PENDING) {
			depend(synthesis, job, reference.producer);
			return 1;
		}
		if (reference.producer) {
			finish(synthesis, job, reference.producer->status == PG_SYNTHESIS_DONE
				? PG_SYNTHESIS_UNSUPPORTED : reference.producer->status);
			return 1;
		}
		if (!reference.binder) {
			if (reference.exports || reference.module) return 0;
			finish(synthesis, job, PG_SYNTHESIS_REJECTED);
			return 1;
		}
		struct pg_derivation_input input = {.rule = PG_VARIABLE,
			.parameters.binder = reference.binder, .count = 1};
		job->binder = reference.binder;
		job->left = pg_synthesis_rule(synthesis, &input, &job->scope->context_job, NULL, NULL);
	}
	forward_proof(synthesis, job, job->left);
	return 1;
}

/* Legacy intrinsic requests are unary type/proof forms, not effect requests.
 * The operand remains a suspended value; application coercions must not force it. */
static int termination_request(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_APPLICATION) return 0;
	const struct pg_syntax *head = syntax->left;
	if (head->kind != PG_SYNTAX_QUALIFIED || head->left->kind != PG_SYNTAX_ATOM) return 0;
	if (head->left->token.kind != '#') return 0;
	struct pg_token name = head->right->token;
	if (name.length != 10) return 0;
	if (!memcmp(name.text, "Terminates", 10)) return 1;
	return !memcmp(name.text, "terminates", 10) ? 2 : 0;
}

static int prepare_constructor_spine(struct pg_synthesis *synthesis, struct pg_synthesis_job *job);

static int prepare_expression(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB || job->stage) return 1;
	const struct pg_syntax *syntax = job->syntax;
	const struct pg_source_scope *right_scope = job->scope;
	int termination = termination_request(syntax);
	if (termination) {
		job->right = pg_synthesis_normalize_classifier_jobs(synthesis, job->scope->context_job,
			pg_synthesis_request(synthesis, job->scope, syntax->right));
		job->left = request_job(synthesis, CLASSIFIER_FORMATION_JOB, job->scope->context_job, job->right);
		job->value_job = plain_rule(synthesis, PG_TERMINATION_FORM, NULL, 2,
			(struct pg_synthesis_job *[]){job->left, job->right});
		if (termination == 2) job->value_job = plain_rule(synthesis, PG_TERMINATION_INTRO, NULL, 2,
			(struct pg_synthesis_job *[]){job->value_job, job->right});
		if (!job->left || !job->right || !job->value_job) goto error;
		job->stage = APPLICATION_RULE_READY;
		return 1;
	}
	if (syntax->kind == PG_SYNTAX_APPLICATION) {
		int spine = prepare_constructor_spine(synthesis, job);
		if (spine) return spine > 0;
	}
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA: case PG_SYNTAX_PI:
		job->left = pg_synthesis_binding(synthesis, job->scope, syntax);
		job->inner = pg_synthesis_binding_scope(job->left);
		if (!job->inner) goto error;
		right_scope = job->inner;
		break;
	case PG_SYNTAX_APPLICATION: case PG_SYNTAX_EXPECT: case PG_SYNTAX_QUOTE:
		job->left = pg_synthesis_request(synthesis, job->scope, syntax->left);
		break;
	default: return 1;
	}
	if (!job->left) goto error;
	if (syntax->kind == PG_SYNTAX_QUOTE) {
		if (await_source_preparation(synthesis, job, job->left, NULL)) return 0;
		int kind = source_value_kind(job->left);
		if (kind < 0 && job->left->status == PG_SYNTHESIS_PENDING) {
			depend(synthesis, job, job->left);
			return 0;
		}
		struct pg_synthesis_job *operand = job->left;
		if (kind == 1) {
			operand = pg_synthesis_normalize_classifier_jobs(synthesis, job->scope->context_job, operand);
			struct pg_synthesis_job *shape = pg_synthesis_classifier_structure(synthesis, operand);
			if (!shape) goto error;
			if (await_dependency(synthesis, job, shape)) return 0;
			const struct pg_term *content;
			if (!pg_thunk_type_view(pg_synthesis_type_structure_result(shape), &content)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED);
				return 0;
			}
			/* Surface & preserves an already suspended value; it never runs it. */
			job->right = operand;
		} else {
			if (operand->result && pg_evidence_judgement(operand->result) == PG_JUDGEMENT_TYPE_FAMILY)
				operand = family_function(synthesis, operand);
			job->right = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &operand);
		}
		if (!job->right) goto error;
	} else {
		job->right = pg_synthesis_request(synthesis, right_scope, syntax->right);
		if (!job->right) goto error;
	}
	if (syntax->kind == PG_SYNTAX_LAMBDA) {
		job->value_job = pg_synthesis_lambda_body(synthesis, job->left, job->right);
		if (!job->value_job) goto error;
	}
	job->stage = 2;
	return 1;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 0;
}

static enum pg_synthesis_status application_bind(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *input, int callee)
{
	struct application_state *state = job->application;
	const struct pg_object *binder = source_binder(synthesis, job->scope, job->syntax, state->binding_count, NULL);
	if (!binder) return PG_SYNTHESIS_REJECTED;
	++state->binding_count;
	struct pg_synthesis_job *context = pg_synthesis_result_context(synthesis, state->context, input, binder);
	if (!context) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis_job *variable = plain_rule(synthesis, PG_VARIABLE, binder, 1, &context);
	struct pg_synthesis_job *other = callee ? state->argument : state->callee;
	other = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, (struct pg_synthesis_job *[]){context, other});
	struct block_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
	if (!variable || !other || !frame) return PG_SYNTHESIS_ERROR;
	*frame = (struct block_frame){.input = input, .context = context, .parent = state->frames, .binds = 1};
	state->frames = frame;
	state->context = context;
	state->callee = callee ? variable : other;
	state->argument = callee ? other : variable;
	return PG_SYNTHESIS_DONE;
}

/* Application equations may determine a still-open result classifier. These
 * jobs only propagate type evidence; they never certify the source term. */
static void classifier_constraint_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *source = (void *)job->inputs[0];
	struct pg_synthesis_job *type = (void *)job->inputs[1];
	if (job->left) { forward_proof(synthesis, job, job->left); return; }
	if (source->status == PG_SYNTHESIS_DONE || source->role != EXPRESSION_JOB) goto done;
	const struct pg_syntax *syntax = source->syntax;
	if (syntax->kind != PG_SYNTAX_QUOTE && syntax->kind != PG_SYNTAX_LAMBDA &&
		syntax->kind != PG_SYNTAX_ELIMINATION) goto done;
	if (handler_syntax(syntax)) goto done;
	if (await_source_preparation(synthesis, job, source, NULL)) return;
	if (syntax->kind == PG_SYNTAX_QUOTE) {
		if (source->right->role != CLASSIFIER_JOB)
			type = plain_rule(synthesis, PG_THUNK_CONTENT, NULL, 1, &type);
		job->left = type ? request_job(synthesis, CLASSIFIER_CONSTRAINT_JOB, source->left, type) : NULL;
	} else if (syntax->kind == PG_SYNTAX_LAMBDA) {
		struct pg_synthesis_job *context = source->inner->context_job;
		struct pg_synthesis_job *variable = plain_rule(synthesis, PG_VARIABLE, source->inner->binder, 1, &context);
		type = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){context, type});
		struct pg_synthesis_job *domain = plain_rule(synthesis, PG_PI_DOMAIN, NULL, 1, &type);
		variable = pg_synthesis_expect(synthesis, variable, domain);
		type = plain_rule(synthesis, PG_PI_CODOMAIN, NULL, 2,
			(struct pg_synthesis_job *[]){type, variable});
		job->left = type ? request_job(synthesis, CLASSIFIER_CONSTRAINT_JOB, source->right, type) : NULL;
	} else {
		if (await_dependency(synthesis, job, type)) return;
		if (!type->result || pg_evidence_judgement(type->result) != PG_JUDGEMENT_COMPUTATION_TYPE) goto done;
		struct pg_synthesis_job *slot = request_job(synthesis, RESULT_TYPE_JOB, source, NULL);
		if (!slot) goto error;
		if (!slot->left) {
			slot->left = type;
			if (slot->stage) enqueue(synthesis, slot);
		}
		/* Additional equations must agree; never overwrite an inferred type. */
		job->left = request_job(synthesis, CONVERSION_JOB,
			pg_evidence_subject(slot->left->result)->core, pg_evidence_subject(type->result)->core);
	}
	if (!job->left) goto error;
	forward_proof(synthesis, job, job->left);
	return;
done:
	finish(synthesis, job, PG_SYNTHESIS_DONE); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static int source_has_identity(struct pg_synthesis *synthesis, const struct pg_source_scope *scope)
{
	for (; scope; scope = scope->parent) {
		while (scope->parent && scope->context_job == scope->parent->context_job)
			scope = scope->parent;
		const struct pg_evidence *context = source_context(scope);
		if (!context || pg_evidence_rule(context) != PG_CONTEXT_EXTEND) continue;
		if (pg_identity_formation(synthesis->typing, pg_evidence_premise(context, 1))) return 1;
	}
	return 0;
}

static struct pg_synthesis_job *constructor_callable_origin(struct pg_synthesis_job *producer)
{
	if (!producer) return NULL;
	switch (producer->role) {
	case CLASSIFIER_JOB: return (void *)producer->inputs[1];
	case BODY_JOB: return (void *)producer->inputs[0];
	case BINDING_EXPECT_JOB: return producer->left;
	case DERIVATION_JOB: {
		const struct pg_derivation_input *rule = producer->inputs[0];
		switch (rule->rule) {
		case PG_THUNK_INTRO: case PG_FORCE_ELIM: case PG_RETURN_INTRO:
			return (void *)producer->inputs[3];
		case PG_CONTEXT_PROJECTION: return (void *)producer->inputs[4];
		default: return NULL;
		}
	}
	default: break;
	}
	if (producer->role == EXPRESSION_JOB && producer->binder && producer->syntax->kind == PG_SYNTAX_ATOM) {
		const struct pg_source_scope *scope = producer->scope;
		while (scope && scope->binder != producer->binder) scope = scope->parent;
		if (!scope) return NULL;
		struct pg_synthesis_job *context = scope->context_job;
		if (context->role == SCOPE_CONTEXT_JOB) context = (void *)context->inputs[2];
		/* Only an actual sequencing binder inherits its input's convention.
		 * A Lambda parameter keeps its explicit Pi contract. Follow existing
		 * input edges; no proof reconstruction or evaluation is required. */
		if (context->role != DERIVATION_JOB) return NULL;
		const struct pg_derivation_input *extension = context->inputs[0];
		if (extension->rule != PG_CONTEXT_EXTEND || extension->parameters.binder != scope->binder) return NULL;
		struct pg_synthesis_job *domain = (void *)context->inputs[4];
		if (domain->role != DERIVATION_JOB) return NULL;
		const struct pg_derivation_input *content = domain->inputs[0];
		if (content->rule != PG_RETURN_CONTENT) return NULL;
		struct pg_synthesis_job *formation = (void *)domain->inputs[3];
		return formation->role == CLASSIFIER_FORMATION_JOB ? (void *)formation->inputs[1] : NULL;
	}
	return operation_origin(producer);
}

static struct pg_synthesis_job *constructor_callable_source(struct pg_synthesis_job *producer)
{
	struct pg_synthesis_job *slow = producer, *fast = producer;
	while (slow) {
		if (slow->callable || slow->role == CONSTRUCTOR_VALUE_JOB) return slow;
		slow = constructor_callable_origin(slow);
		fast = constructor_callable_origin(constructor_callable_origin(fast));
		if (slow && slow == fast) return NULL;
	}
	return NULL;
}

/* A constructor spine owns its argument synthesis. It must not change the
 * meaning of a separately requested partial application with fewer inputs. */
static int prepare_constructor_spine(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *head = job->syntax;
	size_t count = 0;
	while (head->kind == PG_SYNTAX_APPLICATION) {
		if (hypothesis_syntax(head)) return 0;
		++count;
		head = head->left;
	}
	if (count < 2) return 0;
	struct pg_synthesis_job *callee = pg_synthesis_request(synthesis, job->scope, head);
	if (!callee) goto error;
	if (await_source_preparation(synthesis, job, callee, NULL)) return -1;
	struct pg_synthesis_job *origin = constructor_callable_source(callee);
	if (!origin) return 0;
	if (origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, origin); return -1; }
	if (!origin->callable) return 0;
	if (count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
	struct application_state *application = pg_alloc(synthesis->typing->graph, sizeof(*application));
	struct constructor_application *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
	if (!application || !state) goto error;
	state->input = origin->callable;
	state->arity = count;
	state->arguments = pg_alloc(synthesis->typing->graph, count * sizeof(*state->arguments));
	if (!state->arguments) goto error;
	const struct pg_syntax *syntax = job->syntax;
	for (size_t i = count; i; --i, syntax = syntax->left) {
		state->arguments[i - 1] = pg_synthesis_request(synthesis, job->scope, syntax->right);
		if (!state->arguments[i - 1]) goto error;
	}
	*application = (struct application_state){.context = job->scope->context_job,
		.callee = callee, .argument = state->arguments[0], .constructor = state};
	job->application = application;
	job->left = callee;
	job->right = state->arguments[0];
	job->stage = 2;
	return 1;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return -1;
}

/* Instantiate recovered indices and abstract the still-unknown ones around
 * this partial application. Neither the argument nor its effects are copied. */
static void constructor_application_step(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *callee,
	struct pg_synthesis_job *argument)
{
	struct application_state *application = job->application;
	struct constructor_application *state = application->constructor;
	const struct constructor_callable *input = state->input;
	if (!state->arity) {
		state->arity = 1;
		state->arguments = pg_alloc(synthesis->typing->graph, sizeof(*state->arguments));
		if (!state->arguments) goto error;
	}
	if (state->supplied < state->arity) {
		state->arguments[state->supplied++] = argument;
		if (state->supplied < state->arity) {
			application->argument = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_job *[]){application->context, state->arguments[state->supplied]});
			if (!application->argument) goto error;
			enqueue(synthesis, job);
			return;
		}
	}
	if (!state->function) {
		state->function = callee;
		state->context = application->context;
		state->output = pg_alloc(synthesis->typing->graph,
			sizeof(*state->output) + input->count * sizeof(*state->output->indices));
		state->scopes = pg_alloc(synthesis->typing->graph, input->count * sizeof(*state->scopes));
		if (!state->output || !state->scopes) goto error;
		if (state->arity > SIZE_MAX - input->field) goto error;
		*state->output = (struct constructor_callable){.source = input->source, .field = input->field + state->arity};
	}
	if (state->next < input->count) {
		size_t index = input->indices[state->next];
		const struct constructor_index_path *path = &input->source->indices[index];
		struct pg_synthesis_job *value;
		if (path->field >= input->field && path->field - input->field < state->arity) {
			if (!state->instance) {
				struct pg_synthesis_job *input_argument = state->arguments[path->field - input->field];
				input_argument = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
					(struct pg_synthesis_job *[]){application->context, input_argument});
				struct pg_synthesis_job *normalized = pg_synthesis_normalize_classifier_jobs(synthesis,
					application->context, input_argument);
				struct pg_synthesis_job *type = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
					application->context, normalized);
				state->instance = pg_synthesis_inductive_instance(synthesis, type);
			}
			if (!state->instance) goto error;
			if (state->instance->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, state->instance); return; }
			if (state->instance->status != PG_SYNTHESIS_DONE) goto rejected;
			struct pg_inductive_instance instance;
			size_t count;
			if (!pg_synthesis_inductive_instance_result(state->instance, &instance) || !instance.indices) goto rejected;
			if (pg_context_extension_size(pg_evidence_context(pg_data_schema_indices(instance.schema)),
				pg_evidence_context(pg_data_schema_parameters(instance.schema)), &count) || path->index >= count) goto rejected;
			size_t premises = pg_evidence_premise_count(instance.indices);
			if (premises < count) goto error;
			value = pg_synthesis_evidence(synthesis, pg_evidence_premise(instance.indices, premises - count + path->index));
			value = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_job *[]){state->context, value});
		} else {
			const struct pg_object *binder = source_binder(synthesis, job->scope, job->syntax, application->binding_count, NULL);
			if (!binder) goto error;
			++application->binding_count;
			struct pg_synthesis_job *domain = application_domain(synthesis, state->context, state->function);
			struct pg_synthesis_job *extension = plain_rule(synthesis, PG_CONTEXT_EXTEND, binder, 2,
				(struct pg_synthesis_job *[]){state->context, domain});
			if (!extension) goto error;
			state->scopes[state->count++] = extension;
			state->output->indices[state->output->count++] = index;
			state->context = extension;
			state->function = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
				(struct pg_synthesis_job *[]){extension, state->function});
			value = plain_rule(synthesis, PG_VARIABLE, binder, 1, &extension);
		}
		state->function = pg_synthesis_application_jobs(synthesis, state->context, state->function, value);
		if (!state->function) goto error;
		state->instance = NULL;
		++state->next;
		enqueue(synthesis, job);
		return;
	}
	struct pg_synthesis_job *body = state->function;
	for (size_t i = 0; body && i < state->arity; ++i) {
		struct pg_synthesis_job *value = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){state->context, state->arguments[i]});
		body = pg_synthesis_application_jobs(synthesis, state->context, body, value);
	}
	for (size_t i = state->count; body && i; --i)
		body = pg_synthesis_lambda_body(synthesis, state->scopes[i - 1], body);
	if (!body) goto error;
	application->tail = body;
	job->callable = state->output->count ? state->output : NULL;
	enqueue(synthesis, job);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

/* Expose the callee first, then sequence its argument, using ordinary rules. */
static int prepare_application(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB || job->syntax->kind != PG_SYNTAX_APPLICATION) return 0;
	if (job->stage == APPLICATION_RULE_READY) {
		if (job->application && job->application->constraint) {
			struct pg_synthesis_job *constraint = job->application->constraint;
			if (await_dependency(synthesis, job, constraint)) return 1;
		}
		forward_proof(synthesis, job, job->value_job);
		return 1;
	}
	if (job->stage != 2) return 0;
	if (!job->application) {
		job->application = pg_alloc(synthesis->typing->graph, sizeof(*job->application));
		if (!job->application) goto error;
		*job->application = (struct application_state){.context = job->scope->context_job,
			.callee = job->left, .argument = job->right};
	}
	struct application_state *state = job->application;
	if (state->tail) {
		if (state->frames) {
			const struct block_frame *frame = state->frames;
			struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, frame->context, state->tail);
			state->tail = pg_synthesis_sequence(synthesis, rule_premise(synthesis, frame->context, 0), frame->input, continuation);
			if (!state->tail) goto error;
			state->frames = frame->parent;
			enqueue(synthesis, job);
			return 1;
		}
		job->value_job = state->tail;
		job->stage = APPLICATION_RULE_READY;
		enqueue(synthesis, job);
		return 1;
	}
	/* Choose the pending rule from its signature, then check its premises.
	 * A logical signature ends in Universe, not F(Universe). Queue order must
	 * not decide whether an as-yet-unaccepted family is a CBPV computation. */
	struct pg_synthesis_job *raw_shape = pg_synthesis_classifier_structure(synthesis, state->callee);
	if (!raw_shape) goto error;
	if (await_dependency(synthesis, job, raw_shape)) return 1;
	if (logical_family_signature(pg_synthesis_type_structure_result(raw_shape))) {
		struct pg_synthesis_job *argument = state->argument;
		const struct pg_term *domain, *body;
		const struct pg_object *binder;
		if (!pg_pi_view(pg_synthesis_type_structure_result(raw_shape), &domain, &binder, &body)) goto error;
		if (logical_family_signature(domain))
			argument = request_job(synthesis, FAMILY_CONTRACT_JOB, state->context, argument);
		if (!argument) goto error;
		if (await_dependency(synthesis, job, argument)) return 1;
		if (pg_evidence_judgement(argument->result) == PG_JUDGEMENT_COMPUTATION)
			argument = pg_synthesis_return(synthesis, source_context(job->scope), argument->result);
		if (!argument) goto error;
		if (argument->result && pg_evidence_judgement(argument->result) == PG_JUDGEMENT_VALUE_TYPE)
			argument = plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &argument);
		struct pg_synthesis_job *premises[] = {state->callee, argument};
		state->tail = plain_rule(synthesis, PG_TYPE_FAMILY_APP, NULL, 2, premises);
		if (!state->tail) goto error;
		enqueue(synthesis, job);
		return 1;
	}
	struct pg_synthesis_job *callee = pg_synthesis_normalize_classifier_jobs(synthesis, state->context, state->callee);
	struct pg_synthesis_job *shape = pg_synthesis_classifier_structure(synthesis, callee);
	if (!shape) goto error;
	if (await_dependency(synthesis, job, shape)) return 1;
	const struct pg_term *type = pg_synthesis_type_structure_result(shape);
	const struct pg_term *domain, *codomain, *forced;
	const struct pg_object *binder;
	enum pg_totality totality;
	if (pg_thunk_type_view(type, &forced)) {
		state->callee = plain_rule(synthesis, PG_FORCE_ELIM, NULL, 1, &callee);
		if (!state->callee) goto error;
		enqueue(synthesis, job);
		return 1;
	}
	if (pg_computation_type_spine_view(type, &totality, &domain, &codomain)) {
		enum pg_synthesis_status status = application_bind(synthesis, job, callee, 1);
		if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return 1; }
		enqueue(synthesis, job);
		return 1;
	}
	if (!pg_pi_view(type, &domain, &binder, &codomain)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1; }
	if (!state->constructor) {
		struct pg_synthesis_job *origin = constructor_callable_source(job->left);
		if (origin && origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, origin); return 1; }
		if (origin && origin->callable) {
			state->constructor = pg_alloc(synthesis->typing->graph, sizeof(*state->constructor));
			if (!state->constructor) goto error;
			state->constructor->input = origin->callable;
		}
	}
	struct pg_synthesis_job *argument = state->argument;
	if (await_source_preparation(synthesis, job, argument, NULL)) return 1;
	if (!state->constructor && logical_family_signature(domain)) {
		argument = request_job(synthesis, FAMILY_CONTRACT_JOB, state->context, argument);
		struct pg_synthesis_job *premises[] = {callee, argument};
		state->tail = plain_rule(synthesis, PG_APP_ELIM, NULL, 2, premises);
		if (!state->tail) goto error;
		enqueue(synthesis, job);
		return 1;
	}
	if (argument->result && pg_evidence_judgement(argument->result) == PG_JUDGEMENT_TYPE_FAMILY) {
		struct pg_synthesis_job *callable = family_function(synthesis, argument);
		state->argument = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &callable);
		if (!state->argument) goto error;
		enqueue(synthesis, job);
		return 1;
	}
	int kind = source_value_kind(argument);
	if (kind < 0) {
		if (argument->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, argument); return 1; }
		finish(synthesis, job, argument->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : argument->status);
		return 1;
	}
	if (kind == 2) argument = plain_rule(synthesis, PG_VALUE_FROM_TYPE, NULL, 1, &argument);
	if (!kind) {
		argument = pg_synthesis_normalize_classifier_jobs(synthesis, state->context, argument);
		shape = pg_synthesis_classifier_structure(synthesis, argument);
		if (!shape) goto error;
		if (await_dependency(synthesis, job, shape)) return 1;
		const struct pg_term *row, *result;
		if (!pg_computation_type_spine_view(pg_synthesis_type_structure_result(shape), &totality, &row, &result)) {
			if (prepare_value_argument(synthesis, job, state->context, &argument)) return 1;
			if (source_value_kind(argument) == 1) {
				state->argument = argument;
				enqueue(synthesis, job); return 1;
			}
			finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return 1;
		}
		enum pg_synthesis_status status = application_bind(synthesis, job, argument, 0);
		if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return 1; }
		enqueue(synthesis, job);
		return 1;
	}
	if (!state->constraint && state->constructor) {
		constructor_application_step(synthesis, job, callee, argument);
		return 1;
	}
	struct pg_synthesis_job *expected = application_domain(synthesis, state->context, callee);
	if (!expected) goto error;
	if (!state->constraint) {
		state->constraint = request_job(synthesis, CLASSIFIER_CONSTRAINT_JOB, state->argument, expected);
		if (!state->constraint) goto error;
	}
	if (source_has_identity(synthesis, job->scope)) {
		const void *inputs[] = {state->context, argument, expected};
		argument = request_inputs(synthesis, INDEX_TRANSPORT_JOB, 3, inputs);
	} else argument = pg_synthesis_expect(synthesis, argument, expected);
	struct pg_synthesis_job *premises[] = {callee, argument};
	state->tail = plain_rule(synthesis, PG_APP_ELIM, NULL, 2, premises);
	if (!state->tail) goto error;
	enqueue(synthesis, job);
	return 1;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 1;
}

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role == RESULT_TYPE_JOB) {
		job->stage = 1;
		if (job->left) forward_proof(synthesis, job, job->left);
		return;
	}
	if (job->role == CLASSIFIER_CONSTRAINT_JOB) { classifier_constraint_step(synthesis, job); return; }
	/* Restored source scopes exist before name registration. Do not resolve
	 * a missing local name against an outer scope while registration is pending. */
	struct pg_synthesis_job *registration = job->scope ? job->scope->registration : NULL;
	if (registration && registration->status != PG_SYNTHESIS_DONE) {
		if (registration->status == PG_SYNTHESIS_PENDING) depend(synthesis, job, registration);
		else finish(synthesis, job, registration->status);
		return;
	}
	if (atomic_rule_step(synthesis, job)) return;
	if (job->role == PI_SCOPE_JOB) {
		if (!job->left) {
			struct pg_synthesis_job *type = (void *)job->inputs[1];
			struct pg_derivation_input domain = {.rule = PG_PI_DOMAIN, .count = 1};
			struct pg_derivation_input extend = {.rule = PG_CONTEXT_EXTEND, .parameters.binder = job->binder, .count = 2};
			struct pg_synthesis_job *premises[] = {(void *)job->inputs[0],
				pg_synthesis_rule(synthesis, &domain, &type, NULL, NULL)};
			job->left = pg_synthesis_rule(synthesis, &extend, premises, NULL, NULL);
		}
		forward_proof(synthesis, job, job->left);
		return;
	}
	if (job->role == HANDLER_RETURN_JOB) { handler_return_step(synthesis, job); return; }
	/* Self application and IH notation share syntax until scope resolution. */
	if (job->role == EXPRESSION_JOB && !hypothesis_syntax(job->syntax))
		if (!prepare_expression(synthesis, job)) return;
	if (prepare_application(synthesis, job)) return;
	if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_QUOTE) {
		forward_proof(synthesis, job, job->right);
		return;
	}
	if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_LAMBDA) {
		if (job->right->status == PG_SYNTHESIS_DONE && job->right->result &&
			pg_evidence_judgement(job->right->result) == PG_JUDGEMENT_TYPE_FAMILY) {
			struct pg_synthesis_job *premises[] = {job->left, job->right};
			job->value_job = plain_rule(synthesis, PG_TYPE_FAMILY_ABSTRACT, NULL, 2, premises);
		}
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	if (job->role == EXPRESSION_JOB && block_syntax(job->syntax)) {
		block_step(synthesis, job);
		return;
	}
	if (job->role == HANDLER_CLAUSE_JOB) { handler_clause_step(synthesis, job); return; }
	if (job->role == HANDLER_JOB) { handler_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_TRANSPORT_JOB) { constructor_transport_step(synthesis, job); return; }
	if (job->role == INDEX_TRANSPORT_JOB || job->role == INDEX_RESULT_JOB) { index_transport_step(synthesis, job); return; }
	if (job->role == EXPRESSION_JOB && handler_syntax(job->syntax)) {
		if (job->syntax->item_count == 1) { return_handler_step(synthesis, job); return; }
		if (!job->value_job) job->value_job = pg_synthesis_handler(synthesis, job->scope, NULL, job->syntax);
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	if (job->scope && job->role != TELESCOPE_JOB && job->role != TELESCOPE_STRUCTURE_JOB) {
		struct pg_synthesis_job *context = job->scope->context_job;
		if (await_dependency(synthesis, job, context)) return;
		if (!source_context(job->scope)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	const struct pg_syntax *syntax = job->syntax;
	if (job->role == SCOPE_CONTEXT_JOB) {
		struct pg_synthesis_job *parent = job->scope->context_job;
		if (await_dependency(synthesis, job, parent)) return;
		struct pg_synthesis_job *context = (void *)job->inputs[2];
		if (await_dependency(synthesis, job, context)) return;
		if (!binding_context(synthesis, job->scope, job->inputs[1], context->result)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (job->inputs[3] && !pg_context_lookup(pg_evidence_context(parent->result), job->inputs[3])) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		job->result = context->result;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	if (job->role == EFFECT_SUBSTITUTION_JOB) { effect_substitution_step(synthesis, job); return; }
	if (job->role == SEQUENCE_JOB) { sequence_step(synthesis, job); return; }
	if (job->role == OPERATION_REFERENCE_JOB) { operation_reference_step(synthesis, job); return; }
	if (job->role == EFFECT_INFERENCE_JOB) {
		struct pg_effect_inference *work = (void *)job->inputs[0];
		if (!work->sealed && !work->failed) {
			job->stage = 1;
			return;
		}
		int status = pg_effect_inference_advance(work, 1);
		if (!status) enqueue(synthesis, job);
		else finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (job->role == EFFECT_CONTRIBUTION_JOB) {
		struct pg_effect_inference *work = (void *)job->inputs[0];
		struct pg_synthesis_job *structure = (void *)job->inputs[3];
		if (work->failed) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (work->sealed) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (await_dependency(synthesis, job, structure)) return;
		const struct pg_term *row, *value;
		enum pg_totality totality;
		if (!pg_computation_type_spine_view(structure->type_structure, &totality, &row, &value)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (!job->value_job) job->value_job = pg_synthesis_row_contribution(synthesis, work,
			(void *)job->inputs[1], job->inputs[2], row);
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
		finish(synthesis, job, job->value_job->status);
		return;
	}
	if (job->role == ROW_CONTRIBUTION_JOB) {
		struct pg_effect_inference *work = (void *)job->inputs[0];
		if (work->failed) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (work->sealed) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		const struct pg_term *left, *right;
		if (!pg_effect_join_view(job->inputs[3], &left, &right)) {
			int status = pg_effect_contribution(work, job->inputs[3], job->inputs[2], (void *)job->inputs[1]);
			finish(synthesis, job, !status ? PG_SYNTHESIS_DONE
				: work->failed ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_REJECTED);
			return;
		}
		if (!job->left) {
			job->left = pg_synthesis_row_contribution(synthesis, work, (void *)job->inputs[1], job->inputs[2], left);
			job->right = pg_synthesis_row_contribution(synthesis, work, (void *)job->inputs[1], job->inputs[2], right);
		}
		struct pg_synthesis_job *children[] = {job->left, job->right};
		for (size_t i = 0; i < 2; ++i) {
			if (await_dependency(synthesis, job, children[i])) return;
		}
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	if (job->role == OPERATION_JOB) {
		operation_step(synthesis, job);
		return;
	}
	if (job->role == DERIVATION_JOB) { derivation_step(synthesis, job); return; }
	if (job->role == DERIVATION_INPUT_JOB) { derivation_input_step(synthesis, job); return; }
	if (job->role == SOURCE_EXPECT_JOB || job->role == BINDING_EXPECT_JOB) { source_expect_step(synthesis, job); return; }
	if (job->role == TYPE_STRUCTURE_JOB) { type_structure_step(synthesis, job); return; }
	if (job->role == BODY_JOB) { body_step(synthesis, job); return; }
	if (job->role == CLASSIFIER_FORMATION_JOB) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *premise = (void *)job->inputs[i];
			if (!premise) continue;
			if (await_dependency(synthesis, job, premise)) return;
		}
		const struct pg_synthesis_job *context = job->inputs[0], *body = job->inputs[1];
		struct pg_typed_query *classifier = pg_classifier_request(synthesis->typing, context->result, body->result);
		int status = pg_typed_query_advance(classifier, 1);
		if (!status) { enqueue(synthesis, job); return; }
		job->result = pg_typed_query_result(classifier);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (job->role == TERM_STRUCTURE_JOB) { term_structure_step(synthesis, job); return; }
	if (job->role == CLASSIFIER_STRUCTURE_JOB) { classifier_structure_step(synthesis, job); return; }
	if (job->role == DECLARED_TYPE_JOB) { declared_type_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_VALUE_JOB) { constructor_value_step(synthesis, job); return; }
	if (job->role == CONVERSION_JOB) { conversion_step(synthesis, job); return; }
	if (job->role == EXPECT_JOB) { expect_step(synthesis, job); return; }
	if (job->role == FORMATION_JOB) { formation_step(synthesis, job); return; }
	if (job->role == FACE_JOB) {
		if (!job->face) {
			struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[1];
			if (await_dependency(synthesis, job, producer)) return;
			if (!face_formation(synthesis, job->inputs[0], producer->result)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			struct pg_synthesis_job *canonical = pg_synthesis_identity_face(synthesis,
				job->inputs[0], producer->result, job->inputs[2]);
			if (forward_proof(synthesis, job, canonical)) return;
			if (!job->left) job->left = pg_synthesis_identity_formation(synthesis, producer);
			if (await_dependency(synthesis, job, job->left)) return;
			canonical = pg_synthesis_identity_face(synthesis, job->inputs[0], job->left->result, job->inputs[2]);
			if (forward_proof(synthesis, job, canonical)) return;
			job->face = pg_identity_face_init(synthesis->typing,
				job->inputs[0], job->left->result, job->inputs[2]);
			if (!job->face) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		int status = pg_identity_face_advance(job->face, 1);
		if (!status) { enqueue(synthesis, job); return; }
		job->result = pg_identity_face_result(job->face);
		pg_identity_face_destroy(job->face);
		job->face = NULL;
		finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (job->role == REINDEX_JOB) { reindex_step(synthesis, job); return; }
	if (job->role == PAIR_JOB) { pair_step(synthesis, job); return; }
	if (job->role == DATA_CASE_JOB) { data_case_step(synthesis, job); return; }
	if (job->role == INDUCTION_BRANCH_JOB) { induction_branch_step(synthesis, job); return; }
	if (job->role == CONSTANT_MOTIVE_JOB) { constant_motive_step(synthesis, job); return; }
	if (job->role == CLASSIFIER_JOB) { classifier_step(synthesis, job); return; }
	if (job->role == INSTANCE_JOB) { instance_step(synthesis, job); return; }
	if (job->role == REFLEXIVITY_JOB || job->role == FAMILY_ACTION_JOB) {
		if (!job->stage) {
			job->stage = 1;
			depend(synthesis, job, job->left);
			return;
		}
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *input = job->left->result;
		if (!input) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *context = job->inputs[0];
		if (job->role == FAMILY_ACTION_JOB) context = pg_evidence_premise(context, 0);
		if (!typed_input(synthesis, context, input)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_evidence_judgement(input) == PG_JUDGEMENT_VALUE_TYPE) input = value(synthesis, input);
		if (!job->right) job->right = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
			pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, input));
		if (await_dependency(synthesis, job, job->right)) return;
		const struct pg_evidence *type = job->right->result;
		if (job->role == FAMILY_ACTION_JOB) {
			if (!family_paths(synthesis, job)) return;
			job->result = pg_prove_family_action(synthesis->typing, type, input,
				job->inputs[0], job->inputs[1], job->family->count, job->family->paths);
		} else job->result = pg_prove_reflexivity(synthesis->typing, type, input);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (job->role == NORMALIZATION_JOB || job->role == NF_JOB) {
		const struct pg_synthesis_job *context = job->inputs[0], *proof = job->inputs[1];
		if (!job->stage) {
			for (size_t i = 0; i < 2; ++i) {
				struct pg_synthesis_job *premise = (void *)job->inputs[i];
				if (await_dependency(synthesis, job, premise)) return;
			}
			if (!typed_input(synthesis, context->result, proof->result)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			job->checking_term = proof->result;
			if (job->inputs[2]) {
				if (pg_evidence_context(context->result)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				const struct pg_term *content;
				if (pg_evidence_judgement(proof->result) == PG_JUDGEMENT_VALUE &&
					pg_thunk_type_view(pg_evidence_classifier(proof->result), &content))
					job->checking_term = pg_prove_force(synthesis->typing, proof->result);
				if (!job->checking_term) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			}
			job->stage = 1;
		}
		const struct pg_term *input = pg_evidence_subject(job->checking_term)->core;
		const struct pg_reduction_certificate *certificate = normalization_receipt(synthesis, job, input,
			job->role == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
		if (!certificate) return;
		job->result = pg_prove_normalization(synthesis->typing, job->checking_term, certificate);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (job->role == RETURN_JOB || job->role == THUNK_JOB) { contents_step(synthesis, job); return; }
	if (job->role == LIFT_JOB) {
		job->result = pg_prove_substitution_lift(synthesis->typing, job->inputs[0], job->inputs[1], job->inputs[2]);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
		return;
	}
	if (job->role == BINDING_JOB) { binding_step(synthesis, job); return; }
	if (job->role == DOMAIN_JOB) { domain_step(synthesis, job); return; }
	if (job->role == TELESCOPE_JOB) { telescope_step(synthesis, job); return; }
	if (job->role == TELESCOPE_STRUCTURE_JOB) { telescope_structure_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_SCOPE_JOB) { constructor_scope_step(synthesis, job); return; }
	if (job->role == FAMILY_FUNCTION_JOB) { family_function_step(synthesis, job); return; }
	if (job->role == FAMILY_CONTRACT_JOB) { family_contract_step(synthesis, job); return; }
	if (job->role == FUNCTION_GRAPH_JOB) { function_graph_step(synthesis, job); return; }
	if (job->role == FUNCTION_WITNESS_JOB) { function_witness_step(synthesis, job); return; }
	if (job->role == INDUCTIVE_INSTANCE_JOB) { inductive_instance_step(synthesis, job); return; }
	if (job->role == INDUCTION_SCOPE_JOB) { induction_scope_step(synthesis, job); return; }
	if (job->role == DATA_RESULT_JOB || job->role == SUBSTITUTION_JOB) { substitution_step(synthesis, job); return; }
	if (job->role == DATA_SCHEMA_JOB) { data_schema_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_JOB) { constructor_step(synthesis, job); return; }
	if (job->role == DEFINITION_JOB) { definition_step(synthesis, job); return; }
	if (job->role == DEFINITION_SCOPE_JOB) { definition_scope_step(synthesis, job); return; }
	if (hypothesis_reference(synthesis, job)) return;
	if (hypothesis_syntax(syntax) && !lookup_scope(job->scope, (struct pg_token){.kind = '*'}).binder) {
		graph_reference_step(synthesis, job, 1); return;
	}
	if (!prepare_expression(synthesis, job)) return;
	if (syntax->kind == PG_SYNTAX_DECLARATION) { declaration_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_GRAPH_REFERENCE) { graph_reference_step(synthesis, job, 0); return; }
	if (syntax->kind == PG_SYNTAX_ELIMINATION) { match_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_ATOM || syntax->kind == PG_SYNTAX_QUALIFIED || syntax->kind == PG_SYNTAX_IMPORT) {
		reference_step(synthesis, job); return;
	}
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA: case PG_SYNTAX_PI: case PG_SYNTAX_APPLICATION:
	case PG_SYNTAX_QUOTE:
		break;
	case PG_SYNTAX_EXPECT:
		if (!job->value_job) job->value_job = pg_synthesis_source_expect(synthesis, job->scope, job->left, job->right);
		if (job->value_job) job->exports = job->value_job->exports;
		forward_proof(synthesis, job, job->value_job); return;
	default:
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (await_dependency(synthesis, job, job->left)) return;
	if (await_dependency(synthesis, job, job->right)) return;
	const struct pg_evidence *right = job->right->result;
	switch (syntax->kind) {
	case PG_SYNTAX_PI: {
		job->domain = job->left->domain;
		const struct pg_evidence *codomain = type_input(synthesis, job, source_context(job->inner), right);
		if (!codomain) return;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_computation_type(synthesis->typing, PG_TOTALITY_TOTAL,
				pg_effect_row(synthesis->typing->graph, 0, NULL), value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, source_context(job->inner), codomain);
		break;
	}
	case PG_SYNTAX_APPLICATION:
		prepare_application(synthesis, job);
		return;
	default: break;
	}
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}

void pg_synthesis_advance(struct pg_synthesis *synthesis, uint64_t budget)
{
	while (synthesis->ready && budget) {
		struct pg_synthesis_job *job = synthesis->ready;
		synthesis->ready = job->next;
		if (!synthesis->ready) synthesis->ready_tail = NULL;
		job->next = NULL;
		--budget;
		++synthesis->steps;
		step(synthesis, job);
		if (job->waiters) {
			int preparing;
			source_rule(job, &preparing);
			if (!preparing) wake(synthesis, job, 1);
		}
	}
}
enum pg_synthesis_status pg_synthesis_status(const struct pg_synthesis_job *job) { return job->status; }
const struct pg_evidence *pg_synthesis_result(const struct pg_synthesis_job *job)
{
	return job->status == PG_SYNTHESIS_DONE ? job->result : NULL;
}
const struct pg_synthesis_job *pg_synthesis_dependency(const struct pg_synthesis_job *job)
{
	return job && job->dependency ? job->dependency->child : NULL;
}

const struct pg_synthesis_job *pg_synthesis_cycle(const struct pg_synthesis_job *job)
{
	const struct pg_synthesis_job *slow = job, *fast = job;
	do {
		slow = pg_synthesis_dependency(slow);
		fast = pg_synthesis_dependency(pg_synthesis_dependency(fast));
		if (!slow || !fast) return NULL;
	} while (slow != fast);
	return slow;
}

struct pg_synthesis_job *pg_synthesis_definition(const struct pg_synthesis_job *root,
	struct pg_token name)
{
	if (root && root->right && root->right->role == DEFINITION_SCOPE_JOB) root = root->right;
	if (!root || root->role != DEFINITION_SCOPE_JOB || !root->definitions || !name.length || !name.text) return NULL;
	struct block_name *entry = lookup_name(&root->definitions->names, name);
	return entry ? entry->producer : NULL;
}

int pg_synthesis_definition_entry(const struct pg_synthesis_job *root, size_t index,
	const struct pg_syntax_item **item, struct pg_synthesis_job **producer)
{
	if (!root || !item || !producer) return -1;
	if (root->role != EXPRESSION_JOB && root->role != DEFINITION_SCOPE_JOB) return -1;
	const struct pg_syntax *syntax = root->syntax;
	if (syntax && syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return -1;
	if (index >= syntax->item_count) return 0;
	const struct pg_synthesis_job *registration = root->role == DEFINITION_SCOPE_JOB ? root : root->right;
	const struct definition_state *state = registration && registration->role == DEFINITION_SCOPE_JOB
		? registration->definitions : NULL;
	*item = &syntax->items[index];
	*producer = state ? state->entries[index] : NULL;
	return 1;
}
