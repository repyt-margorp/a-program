#include "synthesis.h"
#include "computation.h"
#include "iadt.h"
#include "action.h"
#include "derivation.h"
#include "dag.h"
#include "effect_inference.h"
#include "function_graph.h"

#include <stdlib.h>
#include <string.h>

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
	const struct block_frame *frames;
	struct pg_synthesis_job *tail;
	struct pg_index names;
};
struct application_state {
	struct pg_synthesis_job *context, *callee, *argument, *tail;
	const struct block_frame *frames;
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
struct declaration_state {
	struct pg_index names;
	const struct pg_syntax *constructors;
	struct pg_synthesis_job **producers;
	size_t indexed, checked;
};
struct motive_demand {
	struct motive_demand *next;
	const struct pg_object *field;
	struct pg_synthesis_job *callee, *normalized, *domain;
	const struct pg_evidence *solution;
};
struct motive_result {
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	size_t next;
	struct pg_synthesis_job *input, *callee;
	const struct pg_effect_row *effects;
};
struct match_branch {
	const struct pg_source_scope *scope;
	const struct pg_syntax *clause;
	struct pg_synthesis_job *body;
	struct pg_synthesis_job *adapted;
	struct pg_synthesis_job *type_job, *converted;
	const struct pg_evidence *function;
	const struct pg_evidence *type_family;
	struct motive_demand *demands;
	struct motive_result *result;
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
struct match_state {
	struct pg_inductive_instance instance;
	const struct pg_source_scope *labels;
	const struct pg_evidence *motive;
	const struct pg_evidence *motive_context;
	struct pg_synthesis_job *motive_context_job, *motive_job;
	const struct pg_effect_row *motive_effects;
	size_t count, next, effect_checked, demanded, checked, prepared, typed, result_checked;
	int induction, has_demands, type_cases, packet;
	struct match_branch branches[];
};
struct family_state {
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations;
	const struct pg_evidence **paths;
	size_t count, common, next;
};
struct source_handler_clause {
	struct pg_synthesis_job *reference;
	struct pg_synthesis_job *body;
};
struct handler_state {
	struct pg_synthesis_job *source;
	size_t scanned, next, count;
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
	REFLEXIVITY_JOB, CLASSIFIER_JOB, FAMILY_ACTION_JOB, FORMATION_JOB, FACE_JOB, EXPECT_JOB, SOURCE_EXPECT_JOB, INSTANCE_JOB, CONVERSION_JOB, DATA_CASE_JOB, REINDEX_JOB, PAIR_JOB, SUBSTITUTION_JOB, BINDING_JOB, TELESCOPE_JOB, TELESCOPE_STRUCTURE_JOB, DATA_RESULT_JOB, DATA_SCHEMA_JOB, CONSTRUCTOR_JOB, CONSTRUCTOR_VALUE_JOB, INDUCTION_BRANCH_JOB, CONSTANT_MOTIVE_JOB, DERIVATION_JOB, OPERATION_JOB, OPERATION_REFERENCE_JOB, EFFECT_INFERENCE_JOB, HANDLER_RETURN_JOB, HANDLER_CLAUSE_JOB, HANDLER_JOB, SCOPE_CONTEXT_JOB, EFFECT_SUBSTITUTION_JOB, FAMILY_FUNCTION_JOB, FAMILY_CONTRACT_JOB, FUNCTION_GRAPH_JOB };
enum { APPLICATION_RULE_READY = 6 };
struct context_allocation {
	const struct pg_context *prefix, *end;
	size_t count, next;
	const struct pg_object *binders[];
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
	struct pg_conversion comparison;
	const struct pg_conversion_certificate *certificate;
	struct pg_reindex reindex;
	struct pg_classifier_recovery *classifier_recovery;
	struct pg_inductive_recovery *inductive_recovery;
	struct pg_function_graph_work function_graph;
	const struct pg_inductive_instance *inductive_instance;
	struct pg_identity_face_work *face;
	struct pg_identity_formation_work *formation;
	union { struct pg_whnf_job *whnf; struct pg_nf_job *nf; } normalizing;
	struct block_state *block;
	struct application_state *application;
	const struct block_frame *match_frame;
	struct definition_state *definitions;
	struct substitution_state *substitution;
	struct declaration_state *declaration;
	const struct pg_data_schema *schema;
	const struct pg_operation_declaration *operation;
	const struct pg_data_declaration *nominal_input;
	struct pg_synthesis_job *allocation_origin;
	struct pg_synthesis_job *source_origin;
	struct pg_synthesis_job *packet_origin;
	struct pg_synthesis_job *graph_origin;
	struct source_case_layout *case_layouts;
	struct context_allocation *context_allocation;
	struct pg_constructor_allocation *member_allocations;
	const struct pg_source_scope *exports;
	struct match_state *match;
	struct handler_state *handler;
	struct pg_handler_clause_input *handler_clause;
	struct pg_synthesis_job *handler_owner;
	struct family_state *family;
	struct derivation_state *derivation;
	struct derivation_input_state *derivation_input;
	struct fold_structure_state *fold_structure;
	struct effect_substitution_state *effect_substitution;
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
	struct pg_classifiers *classifiers, struct pg_whnf_work *normalization,
	enum pg_definition_policy definition_policy)
{
	memset(synthesis, 0, sizeof(*synthesis));
	if (classifiers->graph != typing->graph || normalization->graph != typing->graph) return -1;
	if ((unsigned)definition_policy > PG_DEFINITION_EXPLICIT_THUNK) return -1;
	synthesis->typing = typing;
	synthesis->classifiers = classifiers;
	synthesis->normalization = normalization;
	synthesis->definition_policy = definition_policy;
	if (pg_index_init(&synthesis->jobs) != 0) return -1;
	if (pg_index_init(&synthesis->scopes) == 0) {
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
			pg_conversion_destroy(&job->comparison);
			pg_reindex_destroy(&job->reindex);
			if (job->classifier_recovery) pg_classifier_recovery_destroy(job->classifier_recovery);
			if (job->inductive_recovery) pg_inductive_recovery_destroy(job->inductive_recovery);
			pg_function_graph_destroy(&job->function_graph);
			pg_identity_face_destroy(job->face);
			pg_identity_formation_destroy(job->formation);
			if (job->derivation) pg_comparison_destroy(&job->derivation->endpoint);
			if (job->effect_substitution) {
				free(job->effect_substitution->bindings);
			}
			if (job->block) pg_index_destroy(&job->block->names);
			if (job->handler && job->handler->effect_owner == job->handler) pg_effect_inference_destroy(&job->handler->effects);
			if (job->definitions) pg_index_destroy(&job->definitions->names);
			if (job->declaration) pg_index_destroy(&job->declaration->names);
		}
	pg_index_destroy(&synthesis->jobs);
	pg_index_destroy(&synthesis->scopes);
	memset(synthesis, 0, sizeof(*synthesis));
}

static uint64_t name_hash(struct pg_token name);
static int same_name(struct pg_token left, struct pg_token right);
static int handler_syntax(const struct pg_syntax *syntax);
static int handler_clause_origin(struct pg_synthesis *synthesis, struct pg_synthesis_job *handler,
	struct pg_synthesis_job *clause, size_t index);
static struct pg_synthesis_job *plain_rule(struct pg_synthesis *synthesis,
	enum pg_evidence_rule rule, const struct pg_object *binder, size_t count,
	struct pg_synthesis_job *const *premises);

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
	const void *pointers[] = {input.parent, input.context_job, input.binder, input.associated_binder, input.definitions, input.producer, input.exports, input.module, input.imports, input.effect_owner, input.clause};
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
		if (scope->producer != input.producer) continue;
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
	if (job->allocation_origin) {
		const struct pg_derivation_input *input = job->allocation_origin->inputs[0];
		return input->rule == PG_INDUCTION_ELIM ? input->parameters.induction : NULL;
	}
	return pg_evidence_induction_allocation(pg_synthesis_result(job));
}

int pg_synthesis_visit_source_allocations(const struct pg_synthesis *synthesis,
	int (*visit)(void *, struct pg_synthesis_job *), void *owner)
{
	if (!synthesis || !visit) return -1;
	for (size_t i = 0; i < synthesis->jobs.capacity; ++i)
		for (struct pg_index_entry *entry = synthesis->jobs.buckets[i]; entry; entry = entry->next) {
			struct pg_synthesis_job *job = (void *)entry;
			if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_ELIMINATION) {
				int returned = handler_syntax(job->syntax) &&
					(job->allocation_origin || job->status == PG_SYNTHESIS_DONE);
				if ((returned || source_induction_allocation(job)) && visit(owner, job)) return -1;
				continue;
			}
			if (job->role == BINDING_JOB && job->allocation_origin) {
				if (visit(owner, job)) return -1;
				continue;
			}
			if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_APPLICATION) {
				if (job->allocation_origin || (job->status == PG_SYNTHESIS_DONE && job->application
					&& job->application->context != job->scope->context_job
					&& job->application->context->status == PG_SYNTHESIS_DONE))
					if (visit(owner, job)) return -1;
				continue;
			}
			if (job->role == EXPRESSION_JOB &&
				(job->syntax->kind == PG_SYNTAX_LAMBDA || job->syntax->kind == PG_SYNTAX_PI)) {
				if (job->left && job->left->role == BINDING_JOB && job->left->status == PG_SYNTHESIS_DONE)
					if (visit(owner, job->left)) return -1;
				continue;
			}
			if (job->role != EXPRESSION_JOB || job->syntax->kind != PG_SYNTAX_DECLARATION) continue;
			if (!job->allocation_origin && (job->status != PG_SYNTHESIS_DONE || !job->schema)) continue;
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

static int binding_context(struct pg_synthesis *synthesis, const struct pg_source_scope *parent,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!parent || parent->owner != synthesis->owner_key || !extended_context) return 0;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return 0;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!source_context(parent)) return 0;
	if (!context || context->parent != pg_evidence_context(source_context(parent))) return 0;
	if (context->binder != binder) return 0;
	/* Check ownership through a primitive judgement, not just a context pointer. */
	return pg_prove_variable(synthesis->typing, extended_context, binder) != NULL;
}

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!binding_context(synthesis, parent, binder, extended_context)) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context_job = pg_synthesis_evidence(synthesis, extended_context)});
}

enum { RULE_KEY_FIELDS = 16 };

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
		(uintptr_t)input->source, (uintptr_t)input->target, input->reduction_kind, input->count};
	memcpy(key, fields, sizeof(fields));
}

static struct pg_synthesis_job *request_inputs(struct pg_synthesis *synthesis,
	enum job_role role, size_t count, const void *const *inputs)
{
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_job)) / sizeof(*inputs)) return NULL;
	uint64_t hash = ((unsigned)role ^ count) * UINT64_C(1099511628211);
	int producer_rule = role == DERIVATION_JOB;
	uint64_t key[RULE_KEY_FIELDS];
	if (producer_rule) {
		rule_key(inputs[0], key);
		for (size_t i = 0; i < RULE_KEY_FIELDS; ++i) hash = (hash ^ key[i]) * UINT64_C(1099511628211);
	}
	for (size_t i = producer_rule; i < count; ++i) hash = (hash ^ (uintptr_t)inputs[i]) * UINT64_C(1099511628211);
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
		while (i < count && job->inputs[i] == inputs[i]) ++i;
		if (i == count) return job;
	}
	struct pg_synthesis_job *job = pg_alloc(synthesis->typing->graph, sizeof(*job) + count * sizeof(*inputs));
	if (!job) return NULL;
	job->owner = synthesis->owner_key;
	job->role = role;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = inputs[i];
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

static struct pg_synthesis_job *attach_nominal(struct pg_synthesis_job *job,
	const struct pg_data_declaration *allocation)
{
	if (!job || !allocation) return job;
	if (job->nominal_input) return job->nominal_input == allocation ? job : NULL;
	if (job->left || job->domain)
		return job->schema && pg_data_schema_declaration(job->schema) == allocation ? job : NULL;
	job->nominal_input = allocation;
	return job;
}

struct pg_synthesis_job *pg_synthesis_declaration_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_data_declaration *allocation)
{
	if (!syntax || syntax->kind != PG_SYNTAX_DECLARATION) return NULL;
	return attach_nominal(pg_synthesis_request(synthesis, scope, syntax), allocation);
}

struct pg_synthesis_job *pg_synthesis_allocation_origin(const struct pg_synthesis_job *job)
{
	if (job->allocation_origin) return job->allocation_origin;
	if (job->application) return job->application->context;
	if (job->role == EXPRESSION_JOB && handler_syntax(job->syntax) && job->syntax->item_count == 1
		&& job->right && job->right->inner) {
		struct pg_synthesis_job *context = job->right->inner->context_job;
		return context->role == SCOPE_CONTEXT_JOB ? (void *)context->inputs[2] : context;
	}
	return (struct pg_synthesis_job *)job;
}

const struct pg_object *pg_synthesis_allocation_object(const struct pg_synthesis_job *job)
{
	if (!job) return NULL;
	if (job->role == BINDING_JOB) return job->binder;
	if (job->role == EXPRESSION_JOB && job->syntax->kind == PG_SYNTAX_ELIMINATION) {
		const struct pg_induction_allocation *allocation = source_induction_allocation(job);
		if (allocation) return allocation->self;
		if (handler_syntax(job->syntax) && job->syntax->item_count > 1) {
			if (job->allocation_origin) {
				const struct pg_derivation_input *input = job->allocation_origin->inputs[0];
				if (input->rule != PG_HANDLER_ELIM || input->count < 3) return NULL;
				input = input->premises[1];
				if (input->rule != PG_LAMBDA_INTRO || input->count != 2) return NULL;
				input = input->premises[0];
				if (input->rule != PG_PI_FORM || input->count != 2) return NULL;
				input = input->premises[0];
				return input->rule == PG_CONTEXT_EXTEND ? input->parameters.binder : NULL;
			}
			const struct pg_evidence *proof = pg_synthesis_result(job);
			if (pg_evidence_rule(proof) != PG_HANDLER_ELIM) return NULL;
			return pg_evidence_subject(pg_evidence_premise(proof, 1))->core->as.lambda.binder;
		}
	}
	if (job->role == EXPRESSION_JOB && (job->syntax->kind == PG_SYNTAX_APPLICATION || handler_syntax(job->syntax))) {
		const struct pg_synthesis_job *origin = pg_synthesis_allocation_origin(job);
		if (origin->role == DERIVATION_INPUT_JOB) {
			const struct pg_derivation_input *input = origin->inputs[0];
			return input->parameters.binder;
		}
		const struct pg_context *context = pg_evidence_context(pg_synthesis_result(origin));
		return context ? context->binder : NULL;
	}
	const struct pg_data_declaration *declaration = job->nominal_input;
	if (!declaration && job->schema) declaration = pg_data_schema_declaration(job->schema);
	return declaration ? pg_data_declaration_family(declaration) : NULL;
}

struct pg_synthesis_job *pg_synthesis_restore_declaration(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin)
{
	if (!origin || origin->owner != synthesis->owner_key || origin->role != DERIVATION_INPUT_JOB) return NULL;
	const struct pg_derivation_input *input = origin->inputs[0];
	if (input->rule != PG_INDUCTIVE_FORM || !input->parameters.declaration) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_declaration_at(synthesis, scope, syntax, input->parameters.declaration);
	if (!job || (job->allocation_origin && job->allocation_origin != origin)) return NULL;
	job->allocation_origin = origin;
	return job;
}

struct pg_synthesis_job *pg_synthesis_restore_elimination(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin)
{
	if (!syntax || syntax->kind != PG_SYNTAX_ELIMINATION) return NULL;
	if (!origin || origin->owner != synthesis->owner_key || origin->role != DERIVATION_INPUT_JOB) return NULL;
	const struct pg_derivation_input *input = origin->inputs[0];
	if (handler_syntax(syntax)) {
		if (syntax->item_count == 1) {
			if (input->rule != PG_CONTEXT_EXTEND || !input->parameters.binder) return NULL;
		} else if (syntax->item_count > SIZE_MAX / 3 || input->rule != PG_HANDLER_ELIM
			|| input->count != 3 * syntax->item_count) return NULL;
	} else if (input->rule != PG_INDUCTION_ELIM || !input->parameters.induction
		|| input->parameters.induction->count != syntax->item_count) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job || (job->allocation_origin && job->allocation_origin != origin)) return NULL;
	if (job->result || job->value_job) return NULL;
	if (handler_syntax(syntax) && syntax->item_count > 1) {
		struct pg_synthesis_job *handler = pg_synthesis_handler(synthesis, scope, NULL, syntax);
		if (!handler || handler->status != PG_SYNTHESIS_PENDING ||
			(handler->handler && handler->handler->scanned) ||
			(handler->right && handler->right->inner) ||
			(handler->allocation_origin && handler->allocation_origin != origin)) return NULL;
		handler->allocation_origin = origin;
		if (handler->right) {
			for (size_t i = 0; i < syntax->item_count; ++i)
				if (syntax->items[i].expression == handler->right->syntax &&
					handler_clause_origin(synthesis, handler, handler->right, i)) return NULL;
		}
	}
	job->allocation_origin = origin;
	return job;
}

struct pg_synthesis_job *pg_synthesis_restore_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin)
{
	if (!origin || origin->owner != synthesis->owner_key || origin->role != DERIVATION_INPUT_JOB) return NULL;
	const struct pg_derivation_input *input = origin->inputs[0];
	if (input->rule != PG_CONTEXT_EXTEND || !input->parameters.binder) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_binding_at(synthesis, scope, syntax, input->parameters.binder);
	if (!job || (job->allocation_origin && job->allocation_origin != origin)) return NULL;
	job->allocation_origin = origin;
	return job;
}

struct pg_synthesis_job *pg_synthesis_restore_application(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	struct pg_synthesis_job *origin)
{
	if (!syntax || syntax->kind != PG_SYNTAX_APPLICATION) return NULL;
	if (!origin || origin->owner != synthesis->owner_key || origin->role != DERIVATION_INPUT_JOB) return NULL;
	const struct pg_derivation_input *input = origin->inputs[0];
	if (input->rule != PG_CONTEXT_EXTEND || !input->parameters.binder) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job || job->application) return NULL;
	if (job->allocation_origin && job->allocation_origin != origin) return NULL;
	job->allocation_origin = origin;
	return job;
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
		allocation = pg_alloc(synthesis->typing->graph, sizeof(*allocation) + count * sizeof(*allocation->binders));
		if (!allocation) return -1;
		allocation->prefix = prefix; allocation->end = end; allocation->count = count;
		const struct pg_context *context = end;
		for (size_t i = count; i; --i, context = context->parent) allocation->binders[i - 1] = context->binder;
		job->context_allocation = allocation;
	}
	return 0;
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

struct pg_synthesis_job *pg_synthesis_application_at(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax,
	const struct pg_context *prefix, const struct pg_context *end)
{
	if (!syntax || syntax->kind != PG_SYNTAX_APPLICATION) return NULL;
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	if (!job || context_allocation_at(synthesis, job, prefix, end, job->application != NULL)) return NULL;
	return job;
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
	if (!job->binder) job->binder = binder ? binder : pg_binder(synthesis->typing->graph);
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

struct pg_synthesis_job *pg_synthesis_type_structure(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *formation)
{
	if (!formation || formation->owner != synthesis->owner_key) return NULL;
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
	struct pg_graph temporary = {0};
	const void **inputs = pg_alloc(&temporary, (input->count + 3) * sizeof(*inputs));
	if (!inputs) return NULL;
	inputs[0] = input; inputs[1] = effects; inputs[2] = equation;
	for (size_t i = 0; i < input->count; ++i) inputs[i + 3] = premises[i];
	struct pg_synthesis_job *job = request_inputs(synthesis, DERIVATION_JOB, input->count + 3, inputs);
	pg_graph_destroy(&temporary);
	return job;
}

const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_evidence *proof)
{
	if (!parent || parent->owner != synthesis->owner_key) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!pg_evidence_owned_by(proof, synthesis->typing) || !pg_evidence_subject(proof)) return NULL;
	if (!pg_prove_projection(synthesis->typing, source_context(parent), proof)) return NULL;
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
	struct pg_graph temporary = {0};
	const void **inputs = pg_alloc(&temporary, (count + 3) * sizeof(*inputs));
	struct pg_synthesis_job *job = NULL;
	if (!inputs) goto done;
	inputs[0] = left_substitution;
	inputs[1] = right_substitution;
	inputs[2] = input;
	for (size_t i = 0; i < count; ++i) {
		if (!paths[i] || paths[i]->owner != synthesis->owner_key) goto done;
		inputs[i + 3] = paths[i];
	}
	job = request_inputs(synthesis, FAMILY_ACTION_JOB, count + 3, inputs);
	if (job) job->left = input;
done:
	pg_graph_destroy(&temporary);
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

struct pg_synthesis_job *pg_synthesis_source_expect(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, struct pg_synthesis_job *term,
	struct pg_synthesis_job *type)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!term || term->owner != synthesis->owner_key) return NULL;
	if (!type || type->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {scope, term, type};
	struct pg_synthesis_job *job = request_inputs(synthesis, SOURCE_EXPECT_JOB, 3, inputs);
	if (job) { job->scope = scope; job->left = term; job->right = type; }
	return job;
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
	return attach_nominal(request_role(synthesis, parameters, declaration, DATA_SCHEMA_JOB), allocation);
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
	const struct pg_syntax *clause)
{
	if (!scope || scope->owner != synthesis->owner_key) return NULL;
	if (!pg_evidence_owned_by(formation, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(parameters, synthesis->typing)) return NULL;
	if (pg_evidence_context(parameters) != pg_evidence_context(source_context(scope))) return NULL;
	if (!pg_evidence_owned_by(motive_context, synthesis->typing)) return NULL;
	if (!pg_evidence_owned_by(motive, synthesis->typing)) return NULL;
	if (!clause || clause->kind != PG_SYNTAX_CLAUSE) return NULL;
	const void *inputs[] = {scope, formation, constructor, parameters, motive_context, motive, clause};
	struct pg_synthesis_job *job = request_inputs(synthesis, INDUCTION_BRANCH_JOB, 7, inputs);
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
	struct pg_graph temporary = {0};
	const void **inputs = pg_alloc(&temporary, (count + 2) * sizeof(*inputs));
	struct pg_synthesis_job *result = NULL;
	if (!inputs) goto done;
	inputs[0] = source;
	inputs[1] = destination;
	for (size_t i = 0; i < count; ++i) {
		if (!images[i] || images[i]->owner != synthesis->owner_key) goto done;
		inputs[i + 2] = images[i];
	}
	result = request_inputs(synthesis, SUBSTITUTION_JOB, count + 2, inputs);
done:
	pg_graph_destroy(&temporary);
	return result;
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
		input->prefix = pg_evidence_context(pg_evidence_premise(input->parameters->result, 1));
		input->fields = pg_evidence_context(pg_evidence_premise(scope->result, 1));
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
	if (!context || !proof) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	if (pg_evidence_context(context) != pg_evidence_context(proof)) return 0;
	return pg_prove_projection(synthesis->typing, context, proof) == proof;
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
	if (!proof) return NULL;
	switch (pg_evidence_judgement(proof)) {
	case PG_JUDGEMENT_TYPE_FAMILY:
		/* A pending callee may resolve to a family, not a CBPV computation. */
		if (!context || pg_evidence_context(context) != pg_evidence_context(proof)) return NULL;
		if (pg_prove_projection(synthesis->typing, context, proof) != proof) return NULL;
		return pg_synthesis_evidence(synthesis, proof);
	case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION:
		if (!pg_evidence_subject(proof)) return NULL;
		if (!context || pg_evidence_context(context) != pg_evidence_context(proof)) return NULL;
		if (pg_prove_projection(synthesis->typing, context, proof) != proof) return NULL;
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
		struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, job->result);
		if (!accepted) status = PG_SYNTHESIS_ERROR;
		else if (!accepted->source_origin) accepted->source_origin = job;
	}
	if (status > PG_SYNTHESIS_DONE && job->handler && job->handler->effect_owner) {
		struct handler_state *owner = job->handler->effect_owner;
		if (!owner->effects.sealed && !owner->failure) {
			owner->failure = status;
			owner->effects.failed = 1;
			pg_synthesis_effect_inference(synthesis, &owner->effects);
		}
	}
	job->status = status;
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
		return pg_prove_thunk_type(synthesis->typing, synthesis->classifiers, proof);
	return pg_prove_value_type(synthesis->typing, proof);
}

/* Type positions may advance pure computations, but only checked RETURN
 * evidence exposes a value. Expected types never supply missing synthesis. */
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
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *input = type_input(synthesis, job, source_context(job->scope), job->left->result);
	if (!input) return;
	job->result = value_type(synthesis, input);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
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
	return count && pg_return_type_view(type, &body) && pg_universe_level(body, &level);
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
	if (!pg_return_type_view(type, &result)) goto done;
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
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
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
		binder = allocation->binders[allocation->next++];
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
	if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
	if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
	job->result = source_context(job->inner);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void classifier_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (void *)job->inputs[i];
			if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
			if (input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, input->status); return; }
		}
		const struct pg_synthesis_job *context = job->inputs[0], *term = job->inputs[1];
		struct pg_synthesis_job *canonical = pg_synthesis_normalize_classifier(synthesis, context->result, term->result);
		if (!canonical) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (forward_proof(synthesis, job, canonical)) return;
		job->checking_term = term->result;
		if (!job->right) job->right = request_job(synthesis, CLASSIFIER_FORMATION_JOB, context, term);
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
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
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}


static const struct pg_evidence *value(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE_TYPE)
		return pg_prove_type_value(synthesis->typing, proof);
	return pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE ? proof : NULL;
}

static const struct pg_evidence *computation(struct pg_synthesis *synthesis, const struct pg_evidence *proof)
{
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION) return proof;
	proof = value(synthesis, proof);
	return proof ? pg_prove_return(synthesis->typing, synthesis->classifiers, proof) : NULL;
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
			if (producer) return (struct source_reference){.producer = producer};
		}
		if (scope->name.kind != token.kind) continue;
		/* Punctuation tokens carry their spelling in kind, not text. */
		if (token.kind != '#' && token.kind != '*') {
			if (scope->name.length != token.length) continue;
			if (memcmp(scope->name.text, token.text, token.length) != 0) continue;
		}
		return (struct source_reference){scope->binder, scope->producer, scope->exports, scope->module};
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
static int branch_needs_ih(const struct pg_source_scope *scope,
	const struct pg_context *prefix, const struct pg_syntax *body)
{
	if (lookup_scope(scope, (struct pg_token){.kind = '*'}).binder) return 0;
	struct pg_graph arena = {0};
	struct marker_task *tasks = NULL;
	int result = -1;
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
		if (syntax->kind == PG_SYNTAX_APPLICATION && syntax->left->kind == PG_SYNTAX_ATOM &&
			syntax->left->token.kind == '*' && syntax->right->kind == PG_SYNTAX_ATOM) {
			struct pg_token name = syntax->right->token;
			const struct marker_shadow *bound = shadow;
			while (bound && !same_name(bound->name, name)) bound = bound->parent;
			if (!bound) {
				const struct pg_object *field = hypothesis_field(scope, name);
				for (const struct pg_context *context = pg_evidence_context(source_context(scope));
					context && context != prefix; context = context->parent) {
					if (context->binder == field) { result = 1; goto done; }
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
			for (size_t i = 0; i < syntax->item_count; ++i) {
				const struct pg_syntax *clause = syntax->items[i].expression;
				const struct marker_shadow *inner = shadow;
				for (size_t j = 0; j < clause->item_count; ++j) {
					struct pg_token name = clause->items[j].operation == PG_TOKEN_ASSIGN
						? clause->items[j].expression->token : clause->items[j].name;
					inner = marker_bind(&arena, inner, name);
					if (!inner) goto done;
				}
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
	result = 0;
done:
	pg_graph_destroy(&arena);
	return result;
}

static int hypothesis_syntax(const struct pg_syntax *syntax)
{
	if (syntax->kind != PG_SYNTAX_APPLICATION || syntax->left->kind != PG_SYNTAX_ATOM) return 0;
	return syntax->left->token.kind == '*' && syntax->right->kind == PG_SYNTAX_ATOM;
}

/* Collect ordinary APP domain constraints before assigning an IH classifier.
 * This fragment stays in the current lexical scope; nested binders require
 * their own scoped constraint generation. An annotation is never a demand. */
static int motive_demands(struct pg_synthesis *synthesis, struct match_branch *branch,
	const struct pg_context *prefix)
{
	struct pg_graph temporary = {0};
	struct marker_task *tasks = NULL;
	int result = -1;
	if (marker_push(&temporary, &tasks, branch->clause->right, NULL)) goto done;
	while (tasks) {
		const struct pg_syntax *syntax = tasks->syntax;
		tasks = tasks->next;
		if (syntax->kind == PG_SYNTAX_EXPECT) {
			if (marker_push(&temporary, &tasks, syntax->left, NULL)) goto done;
			continue;
		}
		if (syntax->kind != PG_SYNTAX_APPLICATION) continue;
		if (hypothesis_syntax(syntax->right)) {
			int depends = branch_needs_ih(branch->scope, prefix, syntax->left);
			int recursive = branch_needs_ih(branch->scope, prefix, syntax->right);
			if (depends < 0 || recursive < 0) goto done;
			if (!depends && recursive) {
				struct motive_demand *demand = pg_alloc(synthesis->typing->graph, sizeof(*demand));
				if (!demand) goto done;
				demand->field = hypothesis_field(branch->scope, syntax->right->right->token);
				demand->callee = pg_synthesis_request(synthesis, branch->scope, syntax->left);
				if (!demand->callee) goto done;
				demand->next = branch->demands;
				branch->demands = demand;
			}
		}
		if (marker_push(&temporary, &tasks, syntax->left, NULL)) goto done;
		if (marker_push(&temporary, &tasks, syntax->right, NULL)) goto done;
	}
	result = 0;
done:
	pg_graph_destroy(&temporary);
	return result;
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

static enum pg_synthesis_status resolve_member(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct source_reference *reference,
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
		*reference = (struct source_reference){.producer = member->producer};
		return PG_SYNTHESIS_DONE;
	}
	if (reference->producer) {
		struct pg_synthesis_job *producer = reference->producer;
		*dependency = producer;
		if (producer->status != PG_SYNTHESIS_DONE) return producer->status;
		if (producer->exports) {
			*reference = lookup_scope(producer->exports, token);
			return PG_SYNTHESIS_DONE;
		}
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
		struct pg_synthesis_job *origin = pg_synthesis_evidence(synthesis, instance.formation);
		if (!origin || !origin->exports) return PG_SYNTHESIS_UNSUPPORTED;
		struct source_reference member = lookup_scope(origin->exports, token);
		if (!member.producer) return PG_SYNTHESIS_REJECTED;
		if (member.producer->role != CONSTRUCTOR_VALUE_JOB) return PG_SYNTHESIS_UNSUPPORTED;
		*reference = (struct source_reference){.producer = pg_synthesis_constructor_value(synthesis,
			instance.formation, member.producer->inputs[1], instance.parameters)};
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
		*reference = (struct source_reference){.producer = pg_synthesis_request(synthesis, scope, root)};
		if (!reference->producer) return PG_SYNTHESIS_ERROR;
	}
	if (!count) return PG_SYNTHESIS_DONE;
	if (count > SIZE_MAX / sizeof(const struct pg_syntax *)) return PG_SYNTHESIS_ERROR;
	const struct pg_syntax **path = malloc(count * sizeof(*path));
	if (!path) return PG_SYNTHESIS_ERROR;
	for (size_t i = count; i; --i, syntax = syntax->left) path[i - 1] = syntax->right;
	enum pg_synthesis_status status = PG_SYNTHESIS_DONE;
	for (size_t i = 0; i < count; ++i) {
		status = resolve_member(synthesis, source_context(scope), reference, path[i]->token, dependency);
		if (status != PG_SYNTHESIS_DONE) break;
	}
	free(path);
	return status;
}

static int function_graph_exports(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *input = pg_function_graph_case_input(&job->function_graph);
	if (!input) return 0;
	const struct pg_evidence *declaration = pg_function_graph_declaration(&job->function_graph);
	struct pg_synthesis_job *origin = pg_synthesis_evidence(synthesis, input);
	if (!origin || !origin->exports) return -1;
	const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(declaration, 0), 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing, context, context);
	const struct pg_data_layout *source = pg_data_declaration_layout(pg_evidence_inductive_declaration(input));
	const struct pg_data_layout *target = pg_data_declaration_layout(pg_evidence_inductive_declaration(declaration));
	const struct pg_source_scope *exports = intern_scope(synthesis,
		(struct pg_source_scope){.context_job = pg_synthesis_evidence(synthesis, context)});
	for (const struct pg_source_scope *name = origin->exports; name; name = name->parent) {
		if (!name->producer || name->producer->role != CONSTRUCTOR_VALUE_JOB) continue;
		size_t index;
		if (!pg_data_constructor_position(source, name->producer->inputs[1], &index)) return -1;
		const struct pg_object *constructor = pg_data_constructor(target, index);
		exports = pg_synthesis_name_job(synthesis, exports, name->name,
			pg_synthesis_constructor_value(synthesis, declaration, constructor, parameters));
		if (!exports) return -1;
	}
	struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, declaration);
	if (!accepted) return -1;
	accepted->exports = exports;
	accepted->graph_origin = job;
	return 0;
}

/* Source call slots are an interface layout, not an execution schedule.
 * Lexical shadowing and block cutoffs use the same marker walker as IH scope
 * analysis. The backend validates every slot against its typed call plan. */
static int function_graph_order(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *origin)
{
	if (!origin || !origin->match) return 0;
	struct pg_graph temporary = {0};
	size_t count = origin->match->count;
	if (count > SIZE_MAX / sizeof(struct pg_function_graph_order)) return -1;
	struct pg_function_graph_order *orders = pg_alloc(&temporary, count * sizeof(*orders));
	int result = -1;
	if (count && !orders) goto done;
	job->case_layouts = pg_alloc(synthesis->typing->graph, count * sizeof(*job->case_layouts));
	if (count && !job->case_layouts) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_syntax *clause = origin->match->branches[i].clause;
		/* Named source patterns require their resolved field layout here too. */
		if (clause->item_count && clause->items[0].operation) goto done;
		const struct pg_syntax *top = block_syntax(clause->right);
		size_t groups = top ? block_end(clause->right) : 1;
		if (clause->item_count > SIZE_MAX / sizeof(size_t)) goto done;
		size_t *last_group = pg_alloc(&temporary, clause->item_count * sizeof(*last_group));
		if (clause->item_count && !last_group) goto done;
		for (size_t field = 0; field < clause->item_count; ++field) last_group[field] = SIZE_MAX;
		struct marker_task *tasks = NULL;
		struct slot { size_t field; struct pg_token name; struct slot *next; };
		struct slot *slots = NULL, **tail = &slots;
		const struct marker_shadow *top_shadow = NULL;
		for (size_t group = 0; group < groups; ++group) {
			const struct pg_syntax *expression = top ? top->items[group].expression : clause->right;
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
					slot->field = field;
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
		if (orders[i].count > (SIZE_MAX - clause->item_count) / 2) goto done;
		layout->count = clause->item_count + 2 * orders[i].count;
		if (layout->count > SIZE_MAX / sizeof(*layout->fields)) goto done;
		layout->fields = pg_alloc(synthesis->typing->graph, layout->count * sizeof(*layout->fields));
		if (layout->count && !layout->fields) goto done;
		for (size_t field = 0; field < clause->item_count; ++field) layout->fields[field].name = clause->items[field].name;
		for (size_t slot = 0; slots; ++slot, slots = slots->next) {
			fields[slot] = slots->field;
			size_t value = clause->item_count + 2 * slot;
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

static void function_graph_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *function = job->inputs[0];
	if (!job->function_graph.state) {
		if (pg_function_graph_init(&job->function_graph,
			synthesis->typing, synthesis->classifiers, synthesis->normalization, function)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		if (function_graph_order(synthesis, job, (void *)job->inputs[1])) {
			finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
		}
	}
	enum pg_function_graph_status status = pg_function_graph_advance(&job->function_graph, 1);
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
	if (graph->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, graph); return; }
	if (graph->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, graph->status); return; }
	enum pg_function_graph_status status = pg_function_graph_witness_advance(&graph->function_graph, 1);
	if (status == PG_FUNCTION_GRAPH_PENDING) { enqueue(synthesis, job); return; }
	if (status != PG_FUNCTION_GRAPH_DONE) {
		finish(synthesis, job, status == PG_FUNCTION_GRAPH_UNSUPPORTED ? PG_SYNTHESIS_UNSUPPORTED : PG_SYNTHESIS_ERROR);
		return;
	}
	const struct pg_evidence *packet = pg_function_graph_packet(&graph->function_graph);
	const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(packet, 0), 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing, context, context);
	const struct pg_object *constructor = pg_data_constructor(pg_data_declaration_layout(pg_evidence_inductive_declaration(packet)), 0);
	const struct pg_source_scope *exports = intern_scope(synthesis,
		(struct pg_source_scope){.context_job = pg_synthesis_evidence(synthesis, context)});
	exports = pg_synthesis_name_job(synthesis, exports,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "returned", .length = 8},
		pg_synthesis_constructor_value(synthesis, packet, constructor, parameters));
	struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, packet);
	if (!accepted || !exports) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	accepted->exports = exports;
	job->result = pg_function_graph_witness(&graph->function_graph);
	accepted->packet_origin = graph;
	finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void graph_reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int witness)
{
	if (!job->value_job) {
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
		if (reference.producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, reference.producer); return; }
		if (reference.producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, reference.producer->status); return; }
		const struct pg_evidence *function = pg_function_graph_source(reference.producer->result);
		if (!function) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *body = function;
		while (pg_evidence_rule(body) == PG_LAMBDA_INTRO) body = pg_evidence_premise(body, 1);
		struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, body);
		if (!accepted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_synthesis_job *graph = request_job(synthesis, FUNCTION_GRAPH_JOB, function, accepted->source_origin);
		if (witness) graph = request_job(synthesis, FUNCTION_WITNESS_JOB, graph, NULL);
		struct pg_synthesis_job *premises[] = {job->scope->context_job, graph};
		job->value_job = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2, premises);
	}
	forward_proof(synthesis, job, job->value_job);
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
		enum pg_synthesis_status status = resolve_reference(synthesis, job->scope, job->syntax, &reference, &dependency);
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
	if (!comparison) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL; }
	if (comparison->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, comparison); return NULL; }
	if (comparison->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, comparison->status); return NULL; }
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
	if (!pg_effect_type_view(pg_evidence_classifier(term), &source_row, &source_value)) return target;
	if (!pg_effect_type_view(pg_evidence_subject(target)->core, &target_row, &target_value)) return target;
	if (source_row == target_row) return target;
	if (pg_effect_subset(source_row, target_row) != 1) return target;
	/* Compare result types at the source row before widening. Conversion
	 * remains symmetric and does not silently become effect subtyping. */
	return pg_prove_effect_type(synthesis->typing, synthesis->classifiers,
		source_row, pg_prove_return_content(synthesis->typing, target));
}

static void expect_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->checking_term) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (struct pg_synthesis_job *)job->inputs[i];
			if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
			if (input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, input->status); return; }
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
	struct pg_synthesis_job *job, struct pg_synthesis_job *producer);

/* The compatibility policy quotes functions at value boundaries. Returning
 * computations keep their ordinary sequencing; no result value is assumed. */
static int prepare_value_argument(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *context, struct pg_synthesis_job **input)
{
	if (synthesis->definition_policy != PG_DEFINITION_IMPLICIT_THUNK) return 0;
	if (await_source_preparation(synthesis, job, *input)) return 1;
	struct pg_synthesis_job *normalized = pg_synthesis_normalize_classifier_jobs(synthesis, context, *input);
	struct pg_synthesis_job *shape = pg_synthesis_classifier_structure(synthesis, normalized);
	if (!shape) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return 1; }
	if (shape->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, shape); return 1; }
	if (shape->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, shape->status); return 1; }
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
		if (await_source_preparation(synthesis, job, argument)) return;
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
	if (job->value_job) { forward_proof(synthesis, job, job->value_job); return; }
	for (size_t i = 0; i < 3; ++i) {
		struct pg_synthesis_job *input = (void *)job->inputs[i];
		if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
		if (input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, input->status); return; }
	}
	if (!job->value_job) {
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
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		input = job->left->result;
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_REJECTED) { forward_proof(synthesis, job, job->right); return; }
		struct pg_synthesis_job *argument = pg_synthesis_return(synthesis, context, input);
		job->value_job = pg_synthesis_application(synthesis, context,
			(void *)job->inputs[2], argument);
	}
	forward_proof(synthesis, job, job->value_job);
}

static struct pg_synthesis_job *rule_premise(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index);

static const struct pg_evidence *close_match_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct block_frame *frame,
	const struct pg_evidence *body)
{
	if (!job->value_job) {
		struct pg_synthesis_job *continuation = pg_synthesis_lambda_body(synthesis, frame->context,
			pg_synthesis_evidence(synthesis, body));
		job->value_job = pg_synthesis_sequence(synthesis,
			rule_premise(synthesis, frame->context, 0), frame->input, continuation);
	}
	if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL; }
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return NULL; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return NULL; }
	const struct pg_evidence *result = job->value_job->result;
	job->value_job = NULL;
	return result;
}

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
			if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
			if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
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
			synthesis->classifiers, job->function, side);
		if (!type) goto rejected;
		job->value_job = pg_synthesis_expect(synthesis, (void *)job->inputs[job->stage + 1],
			pg_synthesis_evidence(synthesis, type));
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
	if (job->stage == 1) {
		job->checking_term = job->value_job->result;
		job->value_job = NULL;
		job->stage = 2;
		enqueue(synthesis, job);
		return;
	}
	job->result = pg_prove_identity_instance(synthesis->typing, synthesis->classifiers,
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
			if (frame->input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, frame->input); return; }
			if (frame->input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, frame->input->status); return; }
			if (value(synthesis, frame->input->result)) {
				block->frames = frame->parent;
				enqueue(synthesis, job);
				return;
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
	struct pg_synthesis_job *input = pg_synthesis_request(synthesis, block->scope, item->expression);
	if (!input) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (item->annotation) {
		input = pg_synthesis_source_expect(synthesis, block->scope, input,
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
		result = pg_prove_thunk(synthesis->typing, synthesis->classifiers, result);
	}
	job->result = result;
	job->exports = job->left->exports;
	finish(synthesis, job, result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void source_expect_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *parts[] = {job->left, job->right};
	for (size_t i = 0; i < 2; ++i) {
		if (!parts[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (parts[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, parts[i]); return; }
		if (parts[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, parts[i]->status); return; }
	}
	if (!job->checking_term) {
		struct pg_synthesis_job *term = source_reference_producer(synthesis, job->left);
		if (!term) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (term->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, term); return; }
		if (term->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, term->status); return; }
		const struct pg_evidence *left = pg_prove_projection(synthesis->typing,
			source_context(job->scope), term->result);
		const struct pg_evidence *right = type_input(synthesis, job, source_context(job->scope), job->right->result);
		if (!right) return;
		if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE_TYPE) left = value(synthesis, left);
		if (!left) goto rejected;
		if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) right = value_type(synthesis, right);
		else if (pg_evidence_judgement(right) != PG_JUDGEMENT_COMPUTATION_TYPE)
			right = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, right));
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
	if (!job->reindex.state) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (struct pg_synthesis_job *)job->inputs[i];
			if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
			if (input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, input->status); return; }
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
		if (pg_reindex_init(&job->reindex, synthesis->typing, substitution, proof) != 0) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	switch (pg_reindex_advance(&job->reindex, 1)) {
	case PG_REINDEX_PENDING:
		enqueue(synthesis, job);
		return;
	case PG_REINDEX_ERROR:
		finish(synthesis, job, PG_SYNTHESIS_ERROR);
		break;
	case PG_REINDEX_DONE:
		job->result = pg_reindex_result(&job->reindex);
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		break;
	}
	pg_reindex_destroy(&job->reindex);
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
	if (indices->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, indices); return; }
	if (indices->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, indices->status); return; }
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
	const struct pg_evidence *result, const struct pg_evidence *target)
{
	const struct pg_evidence *source = pg_evidence_premise(result, 1);
	const struct pg_context *left = pg_evidence_context(source), *right = pg_evidence_context(target);
	if (left == right) return result;
	size_t count;
	if (pg_context_extension_size(left, NULL, &count) || count > SIZE_MAX / sizeof(void *)) return NULL;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	const struct pg_evidence *adapted = NULL;
	for (size_t i = count; i; --i, left = left->parent, right = right->parent) {
		if (!right || left->binder != right->binder) goto done;
		images[i - 1] = pg_prove_variable(typing, target, right->binder);
	}
	if (right) goto done;
	const struct pg_evidence *map = pg_prove_substitution(typing, source, target, count, images);
	adapted = pg_prove_substitution_compose(typing, result, map);
done:
	free(images);
	return adapted;
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
		if (count > SIZE_MAX / sizeof(*state->producers)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		state->producers = pg_alloc(synthesis->typing->graph, count * sizeof(*state->producers));
		if (count && !state->producers) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	struct declaration_state *state = job->declaration;
	if (state->indexed < state->constructors->item_count) {
		size_t i = state->indexed++;
		const struct pg_syntax_item *item = &state->constructors->items[i];
		if (job->nominal_input) {
			const struct pg_context *prefix = pg_data_declaration_parameters(job->nominal_input);
			if (i >= pg_data_layout_count(pg_data_declaration_layout(job->nominal_input))) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			if (!pg_synthesis_telescope_at(synthesis, job->scope, item->expression, prefix,
				pg_data_declaration_fields(job->nominal_input, i))) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
		}
		int status = register_name(synthesis, &state->names, item->name);
		if (status) { finish(synthesis, job, status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
		const void *inputs[] = {job->scope, job->left, item->expression};
		struct pg_synthesis_job *producer = request_inputs(synthesis, CONSTRUCTOR_JOB, 3, inputs);
		if (!producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		producer->scope = job->scope;
		producer->syntax = item->expression;
		state->producers[i] = producer;
		enqueue(synthesis, job);
		return;
	}
	if (state->checked == state->constructors->item_count) {
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_data_signature *signature = pg_data_signature(synthesis->typing, source_context(job->scope), job->left->result);
		if (!signature) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		struct pg_graph temporary = {0};
		const struct pg_evidence **results = NULL;
		if (state->checked <= SIZE_MAX / sizeof(*results))
			results = pg_alloc(&temporary, state->checked * sizeof(*results));
		if (state->checked && !results) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		const struct pg_evidence *origin = NULL;
		if (job->allocation_origin) {
			if (job->allocation_origin->status == PG_SYNTHESIS_PENDING) {
				pg_graph_destroy(&temporary);
				depend(synthesis, job, job->allocation_origin); return;
			}
			origin = job->allocation_origin->result;
			if (!origin || pg_evidence_inductive_declaration(origin) != job->nominal_input) {
				pg_graph_destroy(&temporary);
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
		}
		for (size_t i = 0; i < state->checked; ++i) {
			results[i] = state->producers[i]->result;
			if (origin) {
				size_t prefix = pg_evidence_judgement(origin) == PG_JUDGEMENT_TYPE_FAMILY ? 2 : 1;
				const struct pg_evidence *saved = pg_evidence_premise(origin, i + prefix);
				results[i] = schema_result_context(synthesis->typing, results[i], pg_evidence_premise(saved, 1));
			}
		}
		job->schema = job->nominal_input
			? pg_data_schema_check(synthesis->typing, job->nominal_input, signature, state->checked, results)
			: pg_data_schema(synthesis->typing, signature, state->checked, results);
		pg_graph_destroy(&temporary);
		finish(synthesis, job, job->schema ? PG_SYNTHESIS_DONE
			: job->nominal_input ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
		return;
	}
	struct pg_synthesis_job *producer = state->producers[state->checked];
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
	++state->checked;
	enqueue(synthesis, job);
}

static void constructor_value_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *formation_job = (void *)job->inputs[0], *parameter_job = (void *)job->inputs[2];
	const struct pg_object *constructor = job->inputs[1];
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *formation = formation_job->result, *parameters = parameter_job->result;
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
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	if (!job->value_job) {
		size_t count = pg_evidence_premise_count(map) - prefix - 3;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		const struct pg_evidence **fields = malloc(count * sizeof(*fields));
		if (count && !fields) goto error;
		for (size_t i = 0; i < count; ++i) fields[i] = pg_evidence_premise(map, prefix + 3 + i);
		const struct pg_evidence *body = pg_prove_constructor(synthesis->typing, formation, constructor, job->right->result, count, fields);
		free(fields);
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
			const struct pg_context *prefix = pg_evidence_context(source_context(job->scope)), *end = prefix;
			if (!self || self->parent != prefix) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			const struct pg_term *tail = self->declared_type, *domain, *body;
			const struct pg_object *binder;
			while (pg_pi_view(tail, &domain, &binder, &body)) {
				end = pg_context_bind(synthesis->typing, end, binder, domain);
				if (!end) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				tail = body;
			}
			job->right = pg_synthesis_telescope_at(synthesis, job->scope, job->syntax->left, prefix, end);
		} else if (!job->right) job->right = pg_synthesis_telescope(synthesis, job->scope, job->syntax->left);
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	}
	if (!job->domain)
		job->domain = pg_prove_universe(synthesis->typing, synthesis->classifiers, source_context(job->scope), 0);
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
		if (job->left && allocation) {
			if (job->left->allocation_origin && job->left->allocation_origin != job->allocation_origin) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			job->left->allocation_origin = job->allocation_origin;
		}
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) {
		enum pg_synthesis_status status = job->left->status;
		if (status == PG_SYNTHESIS_REJECTED && !job->nominal_input) status = PG_SYNTHESIS_UNSUPPORTED;
		finish(synthesis, job, status); return;
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
		job->domain = pg_prove_universe(synthesis->typing, synthesis->classifiers, source_context(job->scope), bound);
		if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->left = NULL;
		enqueue(synthesis, job);
		return;
	}
	if (job->nominal_input && pg_data_schema_declaration(schema) != job->nominal_input) {
		finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	job->result = pg_prove_inductive_type(synthesis->typing, synthesis->classifiers, schema);
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
	struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, job->result);
	if (!accepted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	accepted->exports = job->exports;
	finish(synthesis, job, job->exports ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

/* Resolve source selectors against the generated telescope, then use the
 * same lexical field bindings in ordinary and inductive branch synthesis. */
static enum pg_synthesis_status case_field_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_syntax *clause, size_t count,
	const struct pg_evidence *const *extensions, struct pg_synthesis_job *const *contexts,
	const struct pg_source_scope **result)
{
	int named = clause->item_count && clause->items[0].operation;
	if (!named && count != clause->item_count) return PG_SYNTHESIS_REJECTED;
	if (count > SIZE_MAX / sizeof(struct source_case_field)) return PG_SYNTHESIS_ERROR;
	struct source_case_field *bindings = calloc(count, sizeof(*bindings));
	if (count && !bindings) return PG_SYNTHESIS_ERROR;
	enum pg_synthesis_status status = PG_SYNTHESIS_REJECTED;
	if (named) {
		struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, formation);
		struct pg_synthesis_job *graph = accepted ? accepted->graph_origin : NULL;
		if (!graph || !graph->case_layouts) { status = PG_SYNTHESIS_UNSUPPORTED; goto done; }
		size_t ordinal;
		const struct pg_data_layout *layout = pg_data_declaration_layout(pg_evidence_inductive_declaration(formation));
		if (!pg_data_constructor_position(layout, constructor, &ordinal)) goto done;
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
	} else for (size_t i = 0; i < count; ++i) {
		if (clause->items[i].operation) goto done;
		bindings[i].name = clause->items[i].name;
	}
	const struct pg_source_scope *scope = parent;
	for (size_t i = 0; scope && i < count; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		const struct pg_object *value = NULL;
		if (bindings[i].graph_value) {
			if (bindings[i].graph_value > i) goto done;
			value = pg_evidence_context(extensions[bindings[i].graph_value - 1])->binder;
		}
		if (contexts) scope = bind_context(synthesis, scope, bindings[i].name, binder, contexts[i], value, NULL,
			value ? PG_SOURCE_GRAPH : PG_SOURCE_UNASSOCIATED);
		else {
			scope = pg_synthesis_bind(synthesis, scope, bindings[i].name, binder, extensions[i]);
			if (scope && value) {
				struct pg_source_scope associated = *scope;
				associated.association = PG_SOURCE_GRAPH;
				associated.associated_binder = value;
				scope = intern_scope(synthesis, associated);
			}
		}
	}
	*result = scope;
	status = scope ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR;
done:
	free(bindings);
	return status;
}

static void induction_branch_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		const struct pg_evidence *formation = job->inputs[1], *parameters = job->inputs[3];
		if (!job->right) job->right = pg_synthesis_induction_scope(synthesis,
			pg_synthesis_evidence(synthesis, formation), job->inputs[2], pg_synthesis_evidence(synthesis, parameters),
			pg_synthesis_evidence(synthesis, job->inputs[4]), pg_synthesis_evidence(synthesis, job->inputs[5]));
		if (!job->right) goto error;
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
		const struct pg_evidence *map = job->right->result;
		size_t fields = pg_evidence_premise_count(map) - pg_evidence_premise_count(parameters) - 1, total;
		const struct pg_evidence *context = pg_evidence_premise(map, 1);
		if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(parameters), &total)) goto error;
		if (total > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		struct pg_graph temporary = {0};
		const struct pg_evidence **extensions = pg_alloc(&temporary, total * sizeof(*extensions));
		struct pg_synthesis_job **contexts = pg_alloc(&temporary, total * sizeof(*contexts));
		unsigned char *recursive = pg_alloc(&temporary, fields);
		if ((total && (!extensions || !contexts)) || (fields && !recursive)) { pg_graph_destroy(&temporary); goto error; }
		for (size_t i = total; i; --i, context = pg_evidence_premise(context, 0)) extensions[i - 1] = context;
		const struct pg_derivation_input *input = job->allocation_origin ? job->allocation_origin->inputs[0] : NULL;
		for (size_t i = 0; i < total; ++i) {
			if (!job->allocation_origin) {
				contexts[i] = pg_synthesis_evidence(synthesis, extensions[i]);
				continue;
			}
			if (input->rule != PG_LAMBDA_INTRO || input->count != 2 ||
				input->premises[0]->rule != PG_PI_FORM || input->premises[0]->count != 2) {
				pg_graph_destroy(&temporary); goto error;
			}
			contexts[i] = pg_synthesis_derivation_inference(synthesis, input->premises[0]->premises[0],
				(void *)job->allocation_origin->inputs[1]);
			if (!contexts[i] || pg_evidence_context(pg_synthesis_result(contexts[i])) != pg_evidence_context(extensions[i])) {
				pg_graph_destroy(&temporary); goto error;
			}
			input = input->premises[1];
		}
		const struct pg_source_scope *scope = job->scope;
		enum pg_synthesis_status status = case_field_scope(synthesis, scope, formation, job->inputs[2],
			job->syntax, fields, extensions, contexts, &scope);
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
				pg_evidence_context(extensions[next])->binder, contexts[next]);
			++next;
		}
		if (next != total) scope = NULL;
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
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	job->function = job->left->result;
	if (job->right->status == PG_SYNTHESIS_REJECTED) goto unsupported;
	forward_proof(synthesis, job, job->right);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
}

static int return_clause(const struct pg_syntax *clause)
{
	const struct pg_syntax *head = clause->left;
	if (head->kind != PG_SYNTAX_QUALIFIED) return 0;
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
	if (job->allocation_origin) {
		if (job->allocation_origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->allocation_origin); return; }
		if (job->allocation_origin->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->allocation_origin->status); return; }
	}
	if (!job->inner) {
		if (!job->binder) job->binder = job->allocation_origin
			? pg_evidence_context(job->allocation_origin->result)->binder : pg_binder(synthesis->typing->graph);
		struct pg_synthesis_job *context = pg_synthesis_result_context(synthesis,
			job->scope->context_job, job->left, job->binder);
		if (!context) goto error;
		if (job->allocation_origin) {
			if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
			if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
			if (pg_evidence_context(context->result) != pg_evidence_context(job->allocation_origin->result)) goto rejected;
			context = job->allocation_origin;
		}
		job->inner = pg_synthesis_bind_context(synthesis, job->scope, clause->items[0].name,
			job->binder, context);
		if (!job->inner) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->right = pg_synthesis_request(synthesis, job->inner, clause->right);
		job->value_job = pg_synthesis_lambda_body(synthesis,
			context, job->right);
	}
	if (!job->value_job) goto error;
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
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
		if (job->allocation_origin && job->right->allocation_origin != job->allocation_origin) {
			if (job->right->allocation_origin || job->right->inner) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			job->right->allocation_origin = job->allocation_origin;
		}
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
		if (pg_synthesis_operation_reference_input(synthesis, clauses[i].reference, &signature)) goto done;
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
			if (signature->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, signature); return; }
			if (signature->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, signature->status); return; }
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
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_payload_type(operation))->core);
			if (allocation) allocation = pg_context_bind(synthesis->typing, allocation,
				pg_binder(synthesis->typing->graph), pg_evidence_subject(pg_operation_response_type(operation))->core);
			if (!allocation || context_allocation_at(synthesis, job, NULL, allocation, 0)) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
		}
		const struct pg_object *a = job->context_allocation->binders[0];
		const struct pg_object *b = job->context_allocation->binders[1];
		struct pg_synthesis_job *scope = plain_rule(synthesis, PG_CONTEXT_EXTEND, a, 2,
			(struct pg_synthesis_job *[]){empty, payload});
		struct pg_synthesis_job *domain = plain_rule(synthesis, PG_CONTEXT_PROJECTION, NULL, 2,
			(struct pg_synthesis_job *[]){scope, response});
		struct pg_synthesis_job *response_scope = plain_rule(synthesis, PG_CONTEXT_EXTEND, b, 2,
			(struct pg_synthesis_job *[]){scope, domain});
		struct pg_synthesis_job *value = plain_rule(synthesis, PG_VARIABLE, b, 1, &response_scope);
		struct pg_synthesis_job *returned = plain_rule(synthesis, PG_RETURN_INTRO, NULL, 1, &value);
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
		if (job->allocation_origin) {
			if (job->allocation_origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->allocation_origin); return; }
			if (job->allocation_origin->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->allocation_origin->status); return; }
			const struct pg_evidence *lambda = job->allocation_origin->result;
			if (pg_evidence_rule(lambda) != PG_LAMBDA_INTRO) goto rejected;
			input.payload = pg_evidence_subject(lambda)->core->as.lambda.binder;
			lambda = pg_evidence_premise(lambda, 1);
			if (pg_evidence_rule(lambda) != PG_LAMBDA_INTRO) goto rejected;
			input.resume = pg_evidence_subject(lambda)->core->as.lambda.binder;
			const struct pg_evidence *pi = pg_evidence_premise(lambda, 0);
			const struct pg_context *saved = pg_evidence_context(pg_evidence_premise(pi, 0));
			const struct pg_term *function, *domain, *codomain;
			if (!pg_thunk_type_view(saved->declared_type, &function) ||
				!pg_pi_view(function, &domain, &input.response, &codomain)) goto rejected;
		} else if (job->handler_clause) {
			input = *job->handler_clause;
			input.operation = operation;
		} else {
			input.payload = pg_binder(synthesis->typing->graph);
			input.resume = pg_binder(synthesis->typing->graph);
			input.response = pg_binder(synthesis->typing->graph);
		}
		if (!input.payload || !input.resume || !input.response) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		int status = prepare_handler_clause(synthesis, job, &input);
		if (status) { finish(synthesis, job, status); return; }
	}
	if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
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

static int handler_clause_origin(struct pg_synthesis *synthesis, struct pg_synthesis_job *handler,
	struct pg_synthesis_job *clause, size_t index)
{
	if (!handler->allocation_origin) return 0;
	size_t premise = 1;
	if (!return_clause(handler->syntax->items[index].expression)) {
		premise = 5;
		for (size_t i = 0; i < index; ++i)
			if (!return_clause(handler->syntax->items[i].expression)) premise += 3;
	}
	struct pg_synthesis_job *origin = rule_premise(synthesis, handler->allocation_origin, premise);
	if (premise == 1) origin = rule_premise(synthesis, rule_premise(synthesis, origin, 0), 1);
	if (!origin || (clause->allocation_origin && clause->allocation_origin != origin)) return -1;
	/* A prepared operation clause still checks all three binders against this
	 * origin in its ordinary worker; a return clause has no such input tuple. */
	if (!clause->allocation_origin && clause->inner && !clause->handler_clause) return -1;
	clause->allocation_origin = origin;
	return 0;
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
		if (handler_clause_origin(synthesis, job, job->right, selected)) return PG_SYNTHESIS_REJECTED;
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
		.clause = clause->syntax, .allocation = *clause->handler_clause, .slot = slot,
		.origin = handler->allocation_origin ? handler->allocation_origin : handler->result ? (void *)handler : NULL};
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
	if (input->origin && !pg_synthesis_restore_elimination(synthesis, input->parent, input->handler, input->origin)) return NULL;
	struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(synthesis, input->parent, input->handler);
	if (!carrier) return NULL;
	struct pg_synthesis_job *handler = pg_synthesis_handler(synthesis, input->parent, NULL, input->handler);
	struct pg_synthesis_job *clause = source_handler_clause_job(synthesis, handler, carrier, input->clause);
	if (!clause || handler_clause_origin(synthesis, handler, clause, index)) return NULL;
	if (prepare_handler_clause(synthesis, clause, &input->allocation)) return NULL;
	return input->slot ? clause->inner : clause->inner->parent;
}

static void handler_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *carrier = (void *)job->inputs[1];
	size_t count = job->syntax->item_count;
	if (job->allocation_origin) {
		if (job->allocation_origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->allocation_origin); return; }
		if (job->allocation_origin->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->allocation_origin->status); return; }
	}
	if (prepare_handler(synthesis, job, 0)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	int preparation = prepare_handler_carrier(synthesis, job);
	if (preparation) { finish(synthesis, job, preparation); return; }
	struct handler_state *state = job->handler;
	const struct pg_source_scope *scope = state->scope;
	struct handler_state *owner = state->effect_owner;
	if (owner && owner->failure) { finish(synthesis, job, owner->failure); return; }
	if (state->scanned < count) {
		const struct pg_syntax *clause = job->syntax->items[state->scanned].expression;
		if (!return_clause(clause) && !carrier) {
			struct pg_synthesis_job *operation = pg_synthesis_operation_reference(synthesis,
				pg_synthesis_request(synthesis, scope, clause->left));
			if (!operation) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			struct pg_operation_input signature;
			if (pg_synthesis_operation_reference_input(synthesis, operation, &signature)) {
				if (operation->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, operation); return; }
				finish(synthesis, job, operation->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : operation->status);
				return;
			}
			state->labels[state->count++] = signature.label;
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
			state->count = 0;
		}
		carrier = state->carrier;
		if (state->collected < count + 2) {
			if (!state->collection) {
				struct pg_synthesis_job *body;
				size_t parameters;
				if (!state->collected) {
					body = request_job(synthesis, BODY_JOB, job->left, NULL);
					parameters = 0;
				} else if (state->collected == 1) { body = job->right; parameters = 1; }
				else {
					const struct pg_syntax *clause = job->syntax->items[state->collected - 2].expression;
					if (return_clause(clause)) { ++state->collected; enqueue(synthesis, job); return; }
					body = source_handler_clause_job(synthesis, job, carrier, clause);
					if (!body || handler_clause_origin(synthesis, job, body, state->collected - 2)) goto rejected;
					parameters = 2;
				}
				struct pg_synthesis_job *formation = pg_synthesis_constant_result(synthesis, scope->context_job, body, parameters);
				state->collection = pg_synthesis_effect_contribution(synthesis, &owner->effects, state->equation,
					state->collected ? empty : state->handled, formation);
			}
			if (!state->collection) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (state->collection->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, state->collection); return; }
			if (state->collection->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, state->collection->status); return; }
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
	if (state->next < count) {
		const struct pg_syntax *clause = job->syntax->items[state->next].expression;
		if (!return_clause(clause)) {
			struct pg_synthesis_job *operation = pg_synthesis_operation_reference(synthesis,
				pg_synthesis_request(synthesis, scope, clause->left));
			struct pg_synthesis_job *body = source_handler_clause_job(synthesis, job, carrier, clause);
			if (!operation || !body) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (handler_clause_origin(synthesis, job, body, state->next)) goto rejected;
			struct pg_operation_input signature;
			if (pg_synthesis_operation_reference_input(synthesis, operation, &signature)) {
				if (operation->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, operation); return; }
				finish(synthesis, job, operation->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_REJECTED : operation->status);
				return;
			}
			state->clauses[state->count++] = (struct source_handler_clause){operation, body};
		}
		++state->next;
		enqueue(synthesis, job);
		return;
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

/* The stored derivation is checked by ordinary Solve. Its Lambda contexts
 * supply allocation identity only; source bodies are still synthesized. */
static const struct pg_evidence *source_match_branch_context(const struct pg_synthesis_job *job,
	size_t ordinal, size_t count)
{
	const struct pg_evidence *origin = pg_synthesis_result(job->allocation_origin);
	if (!origin || pg_evidence_rule(origin) != PG_INDUCTION_ELIM) return NULL;
	if (pg_evidence_premise_count(origin) < 6 || ordinal >= pg_evidence_premise_count(origin) - 6) return NULL;
	const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(origin, 2), 1);
	const struct pg_evidence *branch = pg_evidence_premise(origin, ordinal + 5);
	for (size_t i = 0; i < count; ++i) {
		if (pg_evidence_rule(branch) != PG_LAMBDA_INTRO) return NULL;
		const struct pg_evidence *pi = pg_evidence_premise(branch, 0);
		const struct pg_evidence *extension = pg_evidence_premise(pi, 0);
		if (pg_evidence_rule(extension) != PG_CONTEXT_EXTEND ||
			pg_evidence_context(extension)->parent != pg_evidence_context(context)) return NULL;
		context = extension;
		branch = pg_evidence_premise(branch, 1);
	}
	return context;
}

static const struct pg_evidence *match_motive_context(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	if (!state->motive_context_job) {
		const struct pg_evidence *context = job->allocation_origin
			? pg_evidence_premise(job->allocation_origin->result, 4)
			: pg_prove_inductive_motive_context(synthesis->typing, state->instance.formation,
				state->instance.parameters, pg_binder(synthesis->typing->graph));
		if (!context) return NULL;
		if (!pg_inductive_motive_context_valid(synthesis->typing,
			state->instance.formation, state->instance.parameters, context)) return NULL;
		state->motive_context_job = pg_synthesis_evidence(synthesis, context);
	}
	return state->motive_context_job ? state->motive_context_job->result : NULL;
}

/* A known call can synthesize its result without knowing an argument when
 * its codomain does not depend on that argument. This proposes a motive;
 * ordinary induction checking must subsequently check the complete branch. */
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
			.effects = pg_effect_row(typing->graph, 0, NULL)};
		if (!branch->result->effects) goto error;
	}
	struct motive_result *result = branch->result;
	struct pg_synthesis_job *context_job = result->scope->context_job;
	if (context_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context_job); return; }
	if (context_job->status != PG_SYNTHESIS_DONE) goto skip;
	const struct pg_evidence *context = context_job->result;
	const struct pg_effect_row *effects;
	const struct pg_term *content;
	const struct pg_syntax *syntax = result->syntax;
	if (syntax->kind == PG_SYNTAX_EXPECT) {
		result->syntax = syntax->left;
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
		if (pg_effect_type_view(pg_evidence_classifier(result->input->result), &effects, &content))
			result->effects = pg_effect_union(typing->graph, result->effects, effects);
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
	const struct pg_evidence *type = pg_prove_classifier(typing, synthesis->classifiers, context, result->callee->result);
	while (type) {
		const struct pg_term *term = pg_evidence_subject(type)->core;
		if (pg_thunk_type_view(term, &content)) type = pg_prove_thunk_content(typing, type);
		else if (pg_effect_type_view(term, &effects, &content)) {
			result->effects = pg_effect_union(typing->graph, result->effects, effects);
			if (!result->effects) goto error;
			type = pg_prove_return_content(typing, type);
		} else break;
	}
	type = pg_prove_pi_constant_codomain(typing, type);
	if (!type) goto skip;
	if (pg_effect_type_view(pg_evidence_subject(type)->core, &effects, &content)) {
		result->effects = pg_effect_union(typing->graph, result->effects, effects);
		if (!result->effects) goto error;
		type = pg_prove_effect_type(typing, synthesis->classifiers, result->effects, pg_prove_return_content(typing, type));
	} else if (pg_effect_count(result->effects)) goto skip;
	if (!type) goto skip;
	const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
	if (!motive_context) goto skip;
	const struct pg_evidence *parameters = pg_prove_substitution_compose(typing, state->instance.parameters,
		pg_prove_substitution_projection(typing, prefix, context));
	size_t count = branch->field_count;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
	const struct pg_evidence **fields = malloc(count * sizeof(*fields));
	if (count && !fields) goto error;
	const struct pg_context *field = pg_evidence_context(source_context(branch->scope));
	for (size_t i = count; i; --i, field = field->parent) fields[i - 1] = pg_prove_variable(typing, context, field->binder);
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), state->result_checked);
	const struct pg_evidence *value = pg_prove_constructor(typing, state->instance.formation, constructor, parameters, count, fields);
	free(fields);
	const struct pg_evidence *pattern = pg_prove_inductive_motive_substitution(typing, synthesis->classifiers,
		state->instance.formation, state->instance.parameters, motive_context, context, value);
	const struct pg_evidence *candidate = pg_prove_pattern_type(typing, synthesis->classifiers, prefix, pattern, type);
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

static void match_demand_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->demanded];
	struct motive_demand *demand = branch->demands;
	if (!demand) { ++state->demanded; enqueue(synthesis, job); return; }
	const struct pg_evidence *context = source_context(branch->scope);
	if (!demand->normalized) {
		if (demand->callee->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, demand->callee); return; }
		if (demand->callee->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, demand->callee->status); return; }
		const struct pg_evidence *callee = demand->callee->result;
		if (pg_evidence_judgement(callee) == PG_JUDGEMENT_VALUE) callee = pg_prove_force(synthesis->typing, callee);
		if (!callee) goto skip;
		demand->normalized = pg_synthesis_normalize_classifier(synthesis, context, callee);
		if (!demand->normalized) goto error;
	}
	if (demand->normalized->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, demand->normalized); return; }
	if (demand->normalized->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, demand->normalized->status); return; }
	const struct pg_term *argument_type, *result_type;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_classifier(demand->normalized->result), &argument_type, &binder, &result_type)) goto skip;
	if (!demand->domain) demand->domain = application_domain(synthesis,
		pg_synthesis_evidence(synthesis, context), demand->normalized);
	if (!demand->domain) goto error;
	if (demand->domain->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, demand->domain); return; }
	if (demand->domain->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, demand->domain->status); return; }
	if (!demand->solution) {
		const struct pg_evidence *domain = demand->domain->result;
		const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
		if (!motive_context) goto skip;
		const struct pg_evidence *field = pg_prove_variable(synthesis->typing, context, demand->field);
		const struct pg_evidence *pattern = pg_prove_inductive_motive_substitution(synthesis->typing,
			synthesis->classifiers, state->instance.formation, state->instance.parameters,
			motive_context, context, field);
		const struct pg_evidence *type = pg_prove_effect_type(synthesis->typing,
			synthesis->classifiers, state->motive_effects, domain);
		demand->solution = pg_prove_pattern_type(synthesis->typing, synthesis->classifiers,
			source_context(job->inner), pattern, type);
		if (!demand->solution) goto skip;
		if (!state->motive) {
			state->motive = demand->solution;
			state->motive_context = motive_context;
		}
	}
	struct pg_synthesis_job *comparison = request_job(synthesis, CONVERSION_JOB,
		pg_evidence_subject(state->motive)->core, pg_evidence_subject(demand->solution)->core);
	if (!comparison) goto error;
	if (comparison->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, comparison); return; }
	if (comparison->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, comparison->status); return; }
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
		if (body->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, body); return; }
		if (body->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, body->status); return; }
		const struct pg_effect_row *effects;
		const struct pg_term *result;
		if (pg_effect_type_view(pg_evidence_classifier(body->result), &effects, &result)) {
			state->motive_effects = pg_effect_union(synthesis->typing->graph, state->motive_effects, effects);
			if (!state->motive_effects) goto error;
		}
	}
	++state->effect_checked;
	enqueue(synthesis, job);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_type_case_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	const struct pg_evidence *context = source_context(job->inner);
	if (state->typed < state->count) {
		struct match_branch *branch = &state->branches[state->typed];
		const struct pg_evidence *fields = source_context(branch->scope);
		if (!branch->type_job) {
			struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
			struct pg_synthesis_job *type = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
				pg_synthesis_evidence(synthesis, fields), body);
			branch->type_job = plain_rule(synthesis, PG_RETURN_CONTENT, NULL, 1, &type);
			if (!branch->type_job) goto error;
		}
		if (branch->type_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->type_job); return; }
		if (branch->type_job->status != PG_SYNTHESIS_DONE) goto unsupported;
		const struct pg_evidence *type = branch->type_job->result;
		for (const struct pg_evidence *scope = fields;
			type && pg_evidence_context(scope) != pg_evidence_context(context); scope = pg_evidence_premise(scope, 0))
			type = pg_prove_family_abstraction(synthesis->typing, scope, type);
		if (!type) goto unsupported;
		branch->type_family = type;
		const struct pg_effect_row *effects;
		const struct pg_term *value_type;
		const struct pg_evidence *formation = pg_evidence_premise(branch->type_job->result, 0);
		if (!pg_effect_type_view(pg_evidence_subject(formation)->core, &effects, &value_type)) goto unsupported;
		state->motive_effects = state->motive_effects
			? pg_effect_union(synthesis->typing->graph, state->motive_effects, effects) : effects;
		if (!state->motive_effects) goto error;
		++state->typed;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_evidence *mc = match_motive_context(synthesis, job);
	if (!mc) goto unsupported;
	const struct pg_evidence *projection = pg_prove_substitution_projection(synthesis->typing, context, mc);
	const struct pg_evidence *parameters = pg_prove_substitution_compose(synthesis->typing, state->instance.parameters, projection);
	const struct pg_evidence **families = malloc(state->count * sizeof(*families));
	if (!families) goto error;
	for (size_t i = 0; i < state->count; ++i)
		families[i] = pg_prove_projection(synthesis->typing, mc, state->branches[i].type_family);
	const struct pg_evidence *scrutinee = pg_prove_variable(synthesis->typing, mc, pg_evidence_context(mc)->binder);
	const struct pg_evidence *type = pg_prove_type_case(synthesis->typing, synthesis->classifiers,
		state->instance.formation, parameters, scrutinee, state->count, families);
	free(families);
	state->motive = pg_prove_effect_type(synthesis->typing, synthesis->classifiers, state->motive_effects, type);
	if (!state->motive) goto unsupported;
	state->motive_context = mc;
	enqueue(synthesis, job);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void match_type_case_branch(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct match_state *state = job->match;
	struct match_branch *branch = &state->branches[state->prepared];
	if (!branch->converted) {
		const struct pg_evidence *context = source_context(job->inner), *fields = source_context(branch->scope);
		struct pg_synthesis_job *body = pg_synthesis_abstract(synthesis, fields, fields, branch->body);
		const struct pg_evidence *result_type = pg_prove_effect_type(synthesis->typing,
			synthesis->classifiers, state->motive_effects, branch->type_job->result);
		body = pg_synthesis_expect(synthesis, body, pg_synthesis_evidence(synthesis, result_type));
		struct pg_synthesis_job *function = pg_synthesis_abstract(synthesis, context, fields, body);
		const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(state->instance.schema), state->prepared);
		const struct pg_evidence *map = pg_prove_constructor_scope(synthesis->typing,
			state->instance.formation, constructor, state->instance.parameters);
		const struct pg_evidence *expected = pg_prove_match_branch_type(synthesis->typing,
			synthesis->classifiers, state->instance.formation, constructor, state->instance.parameters,
			state->motive_context, state->motive, map);
		branch->converted = pg_synthesis_expect(synthesis, function, pg_synthesis_evidence(synthesis, expected));
		if (!branch->converted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (branch->converted->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->converted); return; }
	if (branch->converted->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, branch->converted->status); return; }
	branch->function = branch->converted->result;
	++state->prepared;
	enqueue(synthesis, job);
}

static void match_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->result) goto complete;
	if (job->allocation_origin) {
		struct pg_synthesis_job *origin = job->allocation_origin;
		if (origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, origin); return; }
		if (origin->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, origin->status); return; }
	}
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
						job->left, pg_binder(synthesis->typing->graph))};
				if (!frame->context) goto error;
				job->match_frame = frame;
			}
			struct pg_synthesis_job *extension = job->match_frame->context;
			if (extension->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, extension); return; }
			if (extension->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, extension->status); return; }
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
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	scrutinee = job->right->result;
	if (!job->match) {
		struct pg_synthesis_job *classifier = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
			pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, scrutinee));
		if (!classifier) goto error;
		if (classifier->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, classifier); return; }
		if (classifier->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, classifier->status); return; }
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis, classifier);
		if (!instance_job) goto error;
		if (instance_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, instance_job); return; }
		if (instance_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, instance_job->status); return; }
		struct pg_inductive_instance instance;
		if (!pg_synthesis_inductive_instance_result(instance_job, &instance)) goto error;
		size_t count = pg_data_constructor_count(instance.schema);
		if (count != job->syntax->item_count) goto rejected;
		if (!count) goto unsupported;
		if (count > (SIZE_MAX - sizeof(struct match_state)) / sizeof(struct match_branch)) goto error;
		struct pg_synthesis_job *origin = pg_synthesis_evidence(synthesis, instance.formation);
		if (!origin || !origin->exports) goto unsupported;
		job->match = pg_alloc(synthesis->typing->graph, sizeof(struct match_state) + count * sizeof(struct match_branch));
		if (!job->match) goto error;
		job->match->instance = instance;
		job->match->count = count;
		job->match->labels = origin->exports;
		job->match->packet = origin->packet_origin != NULL;
	}
	struct match_state *state = job->match;
	const struct pg_data_layout *layout = pg_data_schema_layout(state->instance.schema);
	if (state->next < state->count) {
		const struct pg_syntax *clause = job->syntax->items[state->next].expression;
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
		const struct pg_evidence *schema_fields = pg_data_schema_fields(state->instance.schema, constructor);
		size_t field_count;
		if (pg_context_extension_size(pg_evidence_context(schema_fields),
			pg_evidence_context(pg_evidence_premise(state->instance.formation, 0)), &field_count)) goto error;
		if (job->allocation_origin) {
			const struct pg_evidence *saved = source_match_branch_context(job, ordinal, field_count);
			if (!saved || !pg_synthesis_constructor_scope_at(synthesis,
				pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
				pg_synthesis_evidence(synthesis, state->instance.parameters), pg_evidence_context(context), pg_evidence_context(saved))) goto rejected;
		}
		struct pg_synthesis_job *scope_job = pg_synthesis_constructor_scope(synthesis,
			pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
			pg_synthesis_evidence(synthesis, state->instance.parameters));
		if (!scope_job) goto error;
		if (scope_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, scope_job); return; }
		if (scope_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, scope_job->status); return; }
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
				constructor, clause, count, extensions, NULL, &scope);
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
		if (scope->context_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, scope->context_job); return; }
		if (scope->context_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, scope->context_job->status); return; }
		struct match_branch *branch = &state->branches[ordinal];
		branch->scope = scope;
		branch->clause = clause;
		branch->field_count = count;
		branch->needs_ih = branch_needs_ih(scope, pg_evidence_context(context), clause->right);
		if (branch->needs_ih < 0) goto error;
		if (branch->needs_ih) {
			state->induction = 1;
			if (motive_demands(synthesis, branch, pg_evidence_context(context))) goto error;
			if (branch->demands) state->has_demands = 1;
		}
		else {
			branch->body = pg_synthesis_request(synthesis, scope, clause->right);
			if (!branch->body) goto error;
		}
		++state->next;
		enqueue(synthesis, job);
		return;
	}
	if (state->has_demands && state->effect_checked < state->count) { match_effects_step(synthesis, job); return; }
	if (state->demanded < state->count) { match_demand_step(synthesis, job); return; }
	if (state->checked < state->count) {
		struct match_branch *branch = &state->branches[state->checked];
		if (branch->needs_ih) {
			++state->checked;
			enqueue(synthesis, job);
			return;
		}
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
			else if (pg_alpha_equal(pg_evidence_subject(state->motive)->core, pg_evidence_subject(type)->core) != 1) {
				if (state->induction) goto unsupported;
				state->type_cases = 1;
			}
		}
		++state->checked;
		enqueue(synthesis, job);
		return;
	}
	if (state->type_cases && !state->motive_context) { match_type_case_step(synthesis, job); return; }
	if (!state->motive && state->result_checked < state->count) { match_result_step(synthesis, job); return; }
	if (!state->motive) goto unsupported;
	if (!state->motive_context) {
		if (!state->motive_job) {
			const struct pg_evidence *motive_context = match_motive_context(synthesis, job);
			if (!motive_context) goto unsupported;
			struct pg_synthesis_job *premises[] = {state->motive_context_job,
				pg_synthesis_evidence(synthesis, state->motive)};
			struct pg_derivation_input project = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
			state->motive_job = pg_synthesis_rule(synthesis, &project, premises, NULL, NULL);
			if (!state->motive_job) goto error;
		}
		if (state->motive_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, state->motive_job); return; }
		if (state->motive_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, state->motive_job->status); return; }
		state->motive_context = state->motive_context_job->result;
		state->motive = state->motive_job->result;
	}
	if (state->type_cases && state->prepared < state->count) { match_type_case_branch(synthesis, job); return; }
	if (state->induction && state->prepared < state->count) {
		struct match_branch *branch = &state->branches[state->prepared];
		const struct pg_object *constructor = pg_data_constructor(layout, state->prepared);
		if (job->allocation_origin) {
			const struct pg_evidence *fields = pg_data_schema_fields(state->instance.schema, constructor);
			const struct pg_object *self = pg_evidence_context(pg_evidence_premise(state->instance.formation, 0))->binder;
			size_t count = branch->field_count, total = count;
			const struct pg_context *field = pg_evidence_context(fields);
			for (size_t i = 0; i < count; ++i, field = field->parent) {
				int recursive = pg_data_recursive_field(field->declared_type, self);
				if (recursive < 0) goto unsupported;
				if (recursive && total == SIZE_MAX) goto error;
				total += recursive != 0;
			}
			const struct pg_evidence *prefix = source_match_branch_context(job, state->prepared, count);
			const struct pg_evidence *end = source_match_branch_context(job, state->prepared, total);
			if (!prefix || !end || !pg_synthesis_induction_scope_at(synthesis,
				pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
				pg_synthesis_evidence(synthesis, state->instance.parameters), pg_synthesis_evidence(synthesis, state->motive_context),
				pg_synthesis_evidence(synthesis, state->motive), pg_evidence_context(prefix), pg_evidence_context(end))) goto rejected;
		}
		if (branch->needs_ih) {
			if (!branch->body) branch->body = pg_synthesis_induction_branch(synthesis, job->inner,
				state->instance.formation, constructor, state->instance.parameters,
				state->motive_context, state->motive, branch->clause);
			if (!branch->body) goto error;
			if (job->allocation_origin) {
				const struct pg_derivation_input *input = job->allocation_origin->inputs[0];
				if (input->count < 6 || state->prepared >= input->count - 6) goto rejected;
				struct pg_synthesis_job *origin = pg_synthesis_derivation_inference(synthesis,
					input->premises[state->prepared + 5], (void *)job->allocation_origin->inputs[1]);
				if (!origin) goto error;
				if (branch->body->allocation_origin != origin) {
					if (branch->body->allocation_origin || branch->body->left) goto rejected;
					branch->body->allocation_origin = origin;
				}
			}
			if (branch->body->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->body); return; }
			if (branch->body->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, branch->body->status); return; }
			branch->function = branch->body->result;
		} else {
			if (!branch->adapted) {
				struct pg_synthesis_job *scope = pg_synthesis_induction_scope(synthesis,
					pg_synthesis_evidence(synthesis, state->instance.formation), constructor,
					pg_synthesis_evidence(synthesis, state->instance.parameters),
					pg_synthesis_evidence(synthesis, state->motive_context), pg_synthesis_evidence(synthesis, state->motive));
				if (!scope) goto error;
				if (scope->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, scope); return; }
				if (scope->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, scope->status); return; }
				const struct pg_evidence *map = scope->result, *destination = pg_evidence_premise(map, 1);
				struct pg_synthesis_job *body = projected_image(synthesis, destination, branch->function);
				for (size_t i = pg_evidence_premise_count(state->instance.parameters) + 1; i < pg_evidence_premise_count(map); ++i)
					body = pg_synthesis_application(synthesis, destination, body,
						projected_image(synthesis, destination, pg_evidence_premise(map, i)));
				branch->adapted = pg_synthesis_abstract(synthesis, context, destination, body);
				if (!branch->adapted) goto error;
			}
			if (branch->adapted->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->adapted); return; }
			if (branch->adapted->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, branch->adapted->status); return; }
			branch->function = branch->adapted->result;
		}
		if (!branch->function) goto unsupported;
		++state->prepared;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_evidence **branches = malloc(state->count * sizeof(*branches));
	if (!branches) goto error;
	for (size_t i = 0; i < state->count; ++i) branches[i] = state->branches[i].function;
	const struct pg_induction_allocation *allocation = source_induction_allocation(job);
	if (allocation && !state->induction) { free(branches); goto rejected; }
	job->result = allocation
		? pg_prove_induction_at(synthesis->typing, synthesis->classifiers, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches, allocation)
		: state->induction
		? pg_prove_induction(synthesis->typing, synthesis->classifiers, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches)
		: pg_prove_match(synthesis->typing, synthesis->classifiers, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches);
	free(branches);
	if (!job->result) goto unsupported;
complete:
	if (job->match_frame) {
		const struct pg_evidence *result = close_match_input(synthesis, job, job->match_frame, job->result);
		if (!result) return;
		job->result = result;
		job->match_frame = NULL;
	}
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
	if (type->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, type); return; }
	if (type->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, type->status); return; }
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
	if (!job->inductive_recovery) {
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
		job->inductive_recovery = pg_alloc(synthesis->typing->graph, sizeof(*job->inductive_recovery));
		if (!job->inductive_recovery) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		pg_inductive_recovery_init(job->inductive_recovery, synthesis->typing, normalized);
	}
	int status = pg_inductive_recovery_advance(job->inductive_recovery, 1);
	if (!status) { enqueue(synthesis, job); return; }
	struct pg_inductive_instance instance = job->inductive_recovery->result;
	pg_inductive_recovery_destroy(job->inductive_recovery);
	if (status < 0) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	struct pg_inductive_instance *result = pg_alloc(synthesis->typing->graph, sizeof(*result));
	if (!result) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	*result = instance;
	job->inductive_instance = result;
	job->result = instance.parameters;
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
		if (premises[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, premises[i]); return; }
		if (premises[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, premises[i]->status); return; }
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
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	proof = job->left->result;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (pg_pi_view(pg_evidence_classifier(proof), &domain, &binder, &body)) {
		if (!job->value_job) {
			struct pg_typing *typing = synthesis->typing;
			const struct pg_evidence *pi = pg_prove_classifier(typing, synthesis->classifiers, context->result, proof);
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
	if (!pg_return_type_view(pg_evidence_classifier(proof), &body) || !pg_universe_level(body, &level)) goto rejected;
	if (!job->value_job) job->value_job = pg_synthesis_return(synthesis, context->result, proof);
	if (!job->value_job) goto error;
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
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
	if (input->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, input); return; }
	if (input->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, input->status); return; }
	const struct pg_evidence *proof = input->result;
	if (!proof) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (pg_evidence_judgement(proof) != PG_JUDGEMENT_TYPE_FAMILY) { forward_proof(synthesis, job, input); return; }
	struct pg_synthesis_job *canonical = family_function(synthesis, input);
	if (canonical != job) { forward_proof(synthesis, job, canonical); return; }
	if (!job->left) {
		enum pg_evidence_rule rule = pg_evidence_rule(proof);
		if (rule == PG_TYPE_FAMILY_ABSTRACT) {
			const struct pg_evidence *context = pg_evidence_premise(proof, 0);
			struct pg_synthesis_job *body = family_function(synthesis,
				pg_synthesis_evidence(synthesis, pg_evidence_premise(proof, 1)));
			job->value_job = pg_synthesis_lambda_body(synthesis,
				pg_synthesis_evidence(synthesis, context), body);
		} else if (rule == PG_CONTEXT_PROJECTION || rule == PG_REINDEX) {
			struct pg_synthesis_job *premises[] = {
				pg_synthesis_evidence(synthesis, pg_evidence_premise(proof, 0)),
				family_function(synthesis, pg_synthesis_evidence(synthesis, pg_evidence_premise(proof, 1)))};
			job->value_job = plain_rule(synthesis, rule, NULL, 2, premises);
		} else if (rule == PG_TYPE_FAMILY_APP) {
			const struct pg_evidence *body = pg_prove_application_body(synthesis->typing,
				pg_evidence_premise(proof, 0), pg_evidence_premise(proof, 1));
			if (body) job->value_job = family_function(synthesis, pg_synthesis_evidence(synthesis, body));
		}
		if (job->value_job) { forward_proof(synthesis, job, job->value_job); return; }
		job->left = pg_synthesis_inductive_instance(synthesis, input);
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_inductive_instance *instance = job->left->inductive_instance;
	job->result = pg_prove_inductive_family_function(synthesis->typing, synthesis->classifiers,
		instance->formation, instance->parameters);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void constructor_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->substitution) {
		const struct pg_evidence *proofs[2];
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *producer = (void *)job->inputs[i];
			if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
			if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
			proofs[i] = producer->result;
		}
		const struct pg_evidence *formation = proofs[0], *parameters = proofs[1];
		if (!formation || !parameters) goto rejected;
		if (pg_evidence_rule(formation) != PG_INDUCTIVE_FORM || pg_evidence_rule(parameters) != PG_CONTEXT_SUBSTITUTION) goto rejected;
		if (pg_evidence_context(pg_evidence_premise(parameters, 0)) != pg_evidence_context(formation)) goto rejected;
		struct pg_synthesis_job *instance_job = pg_synthesis_inductive_instance(synthesis, (void *)job->inputs[0]);
		if (!instance_job) goto error;
		if (instance_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, instance_job); return; }
		if (instance_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, instance_job->status); return; }
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
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	state->map = job->right->result;
	if (state->next == state->count) {
		job->result = state->map;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	const struct pg_object *binder = job->context_allocation
		? job->context_allocation->binders[state->next] : pg_binder(synthesis->typing->graph);
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
		if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
		if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
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
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *map = job->left->result;
	if (job->value_job) { forward_proof(synthesis, job, job->value_job); return; }
	if (!job->substitution) {
		if (job->context_allocation && job->context_allocation->prefix != pg_evidence_context(pg_evidence_premise(map, 1))) goto rejected;
		job->substitution = pg_alloc(synthesis->typing->graph, sizeof(*job->substitution));
		if (!job->substitution) goto error;
		job->substitution->map = pg_evidence_premise(map, 1);
	}
	if (job->right) {
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
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
		const struct pg_evidence *ih = pg_prove_inductive_hypothesis_type(synthesis->typing, synthesis->classifiers,
			formation, parameters, motive_context, motive, context, field);
		if (!ih) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		struct pg_synthesis_job *at_field = pg_synthesis_evidence(synthesis, ih);
		struct pg_synthesis_job *premises[] = {pg_synthesis_evidence(synthesis, context), at_field};
		struct pg_derivation_input extend = {.rule = PG_CONTEXT_EXTEND, .count = 2};
		if (job->context_allocation) {
			struct context_allocation *allocation = job->context_allocation;
			if (allocation->next == allocation->count) goto rejected;
			extend.parameters.binder = allocation->binders[allocation->next++];
		} else extend.parameters.binder = pg_binder(synthesis->typing->graph);
		job->right = pg_synthesis_rule(synthesis, &extend, premises, NULL, NULL);
		if (!job->right) goto error;
		depend(synthesis, job, job->right);
		return;
	}
	if (job->context_allocation && job->context_allocation->next != job->context_allocation->count) goto rejected;
	size_t count = pg_evidence_premise_count(map) - 2;
	if (count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) goto error;
	struct pg_synthesis_job **images = malloc(count * sizeof(*images));
	if (count && !images) goto error;
	for (size_t i = 0; i < count; ++i) images[i] = projected_image(synthesis, context, pg_evidence_premise(map, i + 2));
	job->value_job = pg_synthesis_substitution_jobs(synthesis, pg_synthesis_evidence(synthesis, pg_evidence_premise(map, 0)),
		pg_synthesis_evidence(synthesis, context), count, images);
	free(images);
	if (!job->value_job) goto error;
	forward_proof(synthesis, job, job->value_job);
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
				if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
				if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
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
	job->result = pg_data_branch(synthesis->typing, synthesis->classifiers, job->inputs[0], job->inputs[1], checked);
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
		struct pg_graph temporary = {0};
		const struct pg_evidence **images = pg_alloc(&temporary, state->common * sizeof(*images));
		if (state->common && !images) { pg_graph_destroy(&temporary); goto error; }
		for (size_t side = 0; side < 2; ++side) {
			const struct pg_evidence *map = job->inputs[side];
			for (size_t i = 0; i < state->common; ++i) images[i] = pg_evidence_premise(map, i + 2);
			state->maps[side] = pg_prove_substitution(synthesis->typing, prefix,
				pg_evidence_premise(map, 1), state->common, images);
		}
		pg_graph_destroy(&temporary);
		if (!state->maps[0] || !state->maps[1]) goto rejected;
	}
	struct family_state *state = job->family;
	if (state->next == state->count) return 1;
	size_t i = state->next;
	if (!job->checking_type) {
		struct pg_synthesis_job *producer = (struct pg_synthesis_job *)job->inputs[i + 3];
		if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return 0; }
		if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return 0; }
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
		if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
		if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
		const struct pg_evidence *proof = producer->result;
		if (!pg_evidence_owned_by(proof, synthesis->typing)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_identity_formation(synthesis, pg_synthesis_evidence(synthesis, proof));
		if (forward_proof(synthesis, job, canonical)) return;
		job->formation = pg_identity_formation_init(synthesis->typing, synthesis->classifiers, proof);
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
	const struct pg_reduction_certificate *certificate;
	if (kind == PG_REDUCTION_NF) {
		if (!job->normalizing.nf) job->normalizing.nf = pg_nf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!job->normalizing.nf || pg_nf_advance(job->normalizing.nf, 1) == PG_NF_ERROR) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL;
		}
		certificate = pg_nf_certificate(job->normalizing.nf);
	} else if (kind == PG_REDUCTION_WHNF) {
		if (!job->normalizing.whnf) job->normalizing.whnf = pg_whnf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!job->normalizing.whnf || pg_whnf_advance(job->normalizing.whnf, 1) == PG_EVAL_ERROR) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL;
		}
		certificate = pg_whnf_certificate(job->normalizing.whnf);
	} else { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL; }
	if (!certificate) enqueue(synthesis, job);
	return certificate;
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

static struct pg_synthesis_job *prepared_source_rule(const struct pg_synthesis_job *job)
{
	if (job->role == DERIVATION_INPUT_JOB) return job->left;
	if (job->role == OPERATION_JOB) return job->left;
	if (job->role == HANDLER_JOB) return job->value_job;
	if (job->role == HANDLER_RETURN_JOB || job->role == HANDLER_CLAUSE_JOB) return job->value_job;
	if (job->role == SEQUENCE_JOB) return job->value_job ? job->value_job : job->right;
	if (job->role != EXPRESSION_JOB) return NULL;
	if (handler_syntax(job->syntax))
		return job->value_job;
	if (job->block && job->block->tail && !job->block->frames) return job->block->tail;
	if (job->syntax->kind == PG_SYNTAX_APPLICATION && job->stage == APPLICATION_RULE_READY) return job->value_job;
	if (job->syntax->kind == PG_SYNTAX_QUOTE) return job->right;
	if (job->syntax->kind == PG_SYNTAX_LAMBDA) return job->value_job;
	if (job->syntax->kind == PG_SYNTAX_ATOM && job->binder) return job->left;
	if (job->syntax->kind == PG_SYNTAX_ATOM && job->value_job) return job->value_job;
	if (job->syntax->kind == PG_SYNTAX_ATOM && job->syntax->token.kind == '@') return job->left;
	return NULL;
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

/* Wait for preparation's current prerequisite, not whole-source acceptance. */
static int await_source_preparation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_job *producer)
{
	if (producer->status != PG_SYNTHESIS_PENDING) return 0;
	if (producer->role == DERIVATION_INPUT_JOB && !producer->left) {
		subscribe(synthesis, job, producer, 1);
		return 1;
	}
	int preparing = 0;
	switch (producer->role) {
	case OPERATION_JOB:
		preparing = !producer->left;
		break;
	case SEQUENCE_JOB:
		preparing = !producer->value_job && !producer->right && source_value_kind(producer->inputs[1]) >= 0;
		break;
	case HANDLER_RETURN_JOB: case HANDLER_CLAUSE_JOB: case HANDLER_JOB:
		preparing = !producer->value_job;
		break;
	case EXPRESSION_JOB:
		if (!producer->value_job && handler_syntax(producer->syntax)) { preparing = 1; break; }
		if (block_syntax(producer->syntax)) {
			preparing = !producer->block || !producer->block->tail || producer->block->frames;
			break;
		}
		switch (producer->syntax->kind) {
		case PG_SYNTAX_APPLICATION:
			preparing = !producer->stage || (producer->stage == 2 && !producer->function);
			break;
		case PG_SYNTAX_LAMBDA: case PG_SYNTAX_QUOTE: preparing = !producer->stage; break;
		case PG_SYNTAX_ATOM:
			if (producer->syntax->token.kind == PG_TOKEN_IDENT && !producer->value_job && !producer->binder) {
				struct source_reference reference = lookup_scope(producer->scope, producer->syntax->token);
				preparing = reference.producer && (reference.producer->status == PG_SYNTHESIS_PENDING ||
					named_term_ready(reference.producer));
			}
			break;
		default: break;
		}
		break;
	default: break;
	}
	if (!preparing) return 0;
	if (producer->dependency)
		depend(synthesis, job, producer->dependency->child);
	else enqueue(synthesis, job);
	return 1;
}

/* Structural polarity from known rules; unknown producers await acceptance. */
static const struct pg_synthesis_job *polarity_origin(const struct pg_synthesis_job *rule)
{
	for (;;) {
		const struct pg_synthesis_job *prepared = prepared_source_rule(rule);
		if (prepared) { rule = prepared; continue; }
		if (rule->role == CLASSIFIER_JOB) { rule = rule->inputs[1]; continue; }
		if (rule->role == DERIVATION_JOB && rule->input_count > 4 &&
			((const struct pg_derivation_input *)rule->inputs[0])->rule == PG_CONTEXT_PROJECTION) {
			rule = rule->inputs[4]; continue;
		}
		return rule;
	}
}

static int body_rule_polarity(const struct pg_synthesis_job *rule)
{
	rule = polarity_origin(rule);
	if (rule->result) {
		enum pg_evidence_judgement judgement = pg_evidence_judgement(rule->result);
		if (judgement == PG_JUDGEMENT_VALUE || judgement == PG_JUDGEMENT_VALUE_TYPE) return 1;
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
	case PG_VARIABLE: case PG_THUNK_INTRO: case PG_VALUE_FROM_TYPE: case PG_UNIVERSE_FORM: return 1;
	case PG_LAMBDA_INTRO: case PG_APP_ELIM: case PG_FORCE_ELIM: case PG_RETURN_INTRO:
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM: case PG_EFFECT_SUBSUMPTION: return 0;
	default: return -1;
	}
}

/* 2 is a type used as a value, 1 a value, 0 computation, -1 unknown. */
static int source_value_kind(const struct pg_synthesis_job *producer)
{
	const struct pg_synthesis_job *rule = polarity_origin(producer);
	int kind = body_rule_polarity(rule);
	if (kind == 1 && rule->role == DERIVATION_JOB &&
		((const struct pg_derivation_input *)rule->inputs[0])->rule == PG_UNIVERSE_FORM) return 2;
	const struct pg_evidence *result = rule->result ? rule->result : producer->result;
	if (result) {
		enum pg_evidence_judgement judgement = pg_evidence_judgement(result);
		if (judgement == PG_JUDGEMENT_VALUE_TYPE) return 2;
		if (judgement == PG_JUDGEMENT_VALUE) return 1;
		if (judgement == PG_JUDGEMENT_COMPUTATION) return 0;
	}
	return kind;
}

static struct pg_synthesis_job *body_rule(const struct pg_synthesis_job *adapter)
{
	struct pg_synthesis_job *body = (void *)adapter->inputs[0];
	if (body->role == SEQUENCE_JOB) return body;
	struct pg_synthesis_job *rule = prepared_source_rule(body);
	return rule ? rule : body;
}

static void handler_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_synthesis_job *producer, const struct pg_derivation_input *input)
{
	size_t count = pg_handler_signature_count(input->parameters.handler);
	if (!count || count > (SIZE_MAX - 3) / 3 || input->count != 3 + 3 * count) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (!job->fold_structure) {
		if (count > (SIZE_MAX - sizeof(*job->fold_structure)) / sizeof(struct pg_operation_clause)) {
			finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		job->fold_structure = pg_alloc(synthesis->typing->graph,
			sizeof(*job->fold_structure) + count * sizeof(struct pg_operation_clause));
		if (!job->fold_structure) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->left = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 0));
		job->right = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, 1));
	}
	struct pg_synthesis_job *parts[] = {job->left, job->right};
	for (size_t i = 0; i < 2; ++i) {
		if (!parts[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (parts[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, parts[i]); return; }
		if (parts[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, parts[i]->status); return; }
	}
	struct fold_structure_state *state = job->fold_structure;
	if (state->next < count) {
		if (!job->value_job) job->value_job = pg_synthesis_term_structure(synthesis,
			rule_premise(synthesis, producer, 5 + 3 * state->next));
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
		if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
		state->clauses[state->next] = (struct pg_operation_clause){
			pg_handler_signature_label(input->parameters.handler, state->next),
			pg_synthesis_type_structure_result(job->value_job)};
		++state->next;
		job->value_job = NULL;
		enqueue(synthesis, job);
		return;
	}
	job->type_structure = pg_computation_fold(synthesis->typing->graph,
		pg_synthesis_type_structure_result(job->left), pg_synthesis_type_structure_result(job->right), count, state->clauses);
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void term_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (await_source_preparation(synthesis, job, producer)) return;
	/* Classifier conversion and post-checking never rewrite the subject. */
	if (producer->role == CLASSIFIER_JOB || producer->role == EXPECT_JOB) {
		if (!job->left) job->left = pg_synthesis_term_structure(synthesis,
			(void *)producer->inputs[producer->role == CLASSIFIER_JOB ? 1 : 0]);
		forward_structure(synthesis, job);
		return;
	}
	if (producer->role == BODY_JOB) {
		struct pg_synthesis_job *rule = body_rule(producer);
		int polarity = body_rule_polarity(rule);
		if (polarity >= 0) {
			if (!job->left) job->left = pg_synthesis_term_structure(synthesis, rule);
			if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
			if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
			const struct pg_term *term = pg_synthesis_type_structure_result(job->left);
			job->type_structure = polarity ? pg_application(synthesis->typing->graph,
				pg_reference(synthesis->typing->graph, &pg_return_operation), term) : term;
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
	}
	struct pg_synthesis_job *source_rule = prepared_source_rule(producer);
	if (source_rule) {
		/* A source sequence may replace a rejected fold with checked pure application. */
		if (producer->role == SEQUENCE_JOB && source_rule == producer->right) {
			struct pg_synthesis_job *shapes[] = {
				pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, source_rule, 0)),
				pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, source_rule, 1))};
			for (size_t i = 0; i < 2; ++i) {
				if (!shapes[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (shapes[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, shapes[i]); return; }
				if (shapes[i]->status == PG_SYNTHESIS_ERROR) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (shapes[i]->status != PG_SYNTHESIS_DONE) goto accepted_subject;
			}
			const struct pg_term *row, *value, *domain, *codomain, *following, *result;
			const struct pg_object *binder;
			if (!pg_effect_type_spine_view(shapes[0]->type_structure, &row, &value)) goto accepted_subject;
			if (!pg_pi_view(shapes[1]->type_structure, &domain, &binder, &codomain)) goto accepted_subject;
			if (pg_alpha_equal(domain, value) != 1) goto accepted_subject;
			if (pg_term_independent(codomain, binder) != 1) goto accepted_subject;
			if (!pg_effect_type_spine_view(codomain, &following, &result)) {
				const struct pg_effect_row *closed = pg_effect_row_view(row);
				if (!closed || pg_effect_count(closed)) goto accepted_subject;
			}
		}
		if (!job->left) job->left = pg_synthesis_term_structure(synthesis, source_rule);
		forward_structure(synthesis, job);
		return;
	}
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	const struct pg_object *operation = NULL;
	if (input) {
		if (input->rule == PG_HANDLER_ELIM) { handler_structure_step(synthesis, job, producer, input); return; }
		if (input->rule == PG_UNIVERSE_FORM) {
			job->type_structure = pg_universe(synthesis->classifiers, input->parameters.level);
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
		if (input->rule == PG_LAMBDA_INTRO || input->rule == PG_APP_ELIM
			|| input->rule == PG_FOLD_ELIM || input->rule == PG_REQUEST_INTRO) {
			if (!job->left) {
				size_t offset = input->rule == PG_REQUEST_INTRO ? 2 : 0;
				struct pg_synthesis_job *first = rule_premise(synthesis, producer, offset);
				job->left = input->rule == PG_LAMBDA_INTRO ? pg_synthesis_type_structure(synthesis, first)
					: pg_synthesis_term_structure(synthesis, first);
				job->right = pg_synthesis_term_structure(synthesis, rule_premise(synthesis, producer, offset + 1));
			}
			struct pg_synthesis_job *parts[] = {job->left, job->right};
			for (size_t i = 0; i < 2; ++i) {
				if (!parts[i]) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
				if (parts[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, parts[i]); return; }
				if (parts[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, parts[i]->status); return; }
			}
			const struct pg_term *left = pg_synthesis_type_structure_result(job->left);
			const struct pg_term *right = pg_synthesis_type_structure_result(job->right);
			if (input->rule == PG_LAMBDA_INTRO) {
				const struct pg_term *domain, *codomain;
				const struct pg_object *binder;
				if (!pg_pi_view(left, &domain, &binder, &codomain)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
				job->type_structure = pg_lambda(synthesis->typing->graph, binder, right);
			} else if (input->rule == PG_FOLD_ELIM)
				job->type_structure = pg_computation_fold(synthesis->typing->graph, left, right, 0, NULL);
			else if (input->rule == PG_REQUEST_INTRO)
				job->type_structure = pg_computation_request(synthesis->typing->graph,
					input->parameters.operation_label, left, right);
			else job->type_structure = pg_application(synthesis->typing->graph, left, right);
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_term *term = pg_synthesis_type_structure_result(job->left);
		job->type_structure = operation ? pg_application(synthesis->typing->graph,
			pg_reference(synthesis->typing->graph, operation), term) : term;
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
accepted_subject:
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
	const struct pg_occurrence *subject = producer->result ? pg_evidence_subject(producer->result) : NULL;
	job->type_structure = subject ? subject->core : NULL;
	finish(synthesis, job, subject ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void declared_type_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *context = (void *)job->inputs[0];
	const struct pg_object *binder = job->inputs[1];
	if (await_source_preparation(synthesis, job, context)) return;
	struct pg_synthesis_job *prepared = prepared_source_rule(context);
	if (!job->left && prepared) job->left = request_job(synthesis, DECLARED_TYPE_JOB, prepared, binder);
	if (!job->left && context->role == SCOPE_CONTEXT_JOB)
		job->left = request_job(synthesis, DECLARED_TYPE_JOB, context->inputs[2], binder);
	if (!job->left && context->role == BINDING_JOB)
		job->left = context->binder == binder ? pg_synthesis_type_structure(synthesis, context->right)
			: request_job(synthesis, DECLARED_TYPE_JOB, context->scope->context_job, binder);
	if (!job->left && context->role == PI_SCOPE_JOB && context->binder == binder) {
		struct pg_synthesis_job *type = (void *)context->inputs[1];
		struct pg_synthesis_job *domain = plain_rule(synthesis, PG_PI_DOMAIN, NULL, 1, &type);
		job->left = pg_synthesis_type_structure(synthesis, domain);
	}
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
	if (job->left) {
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		job->type_structure = job->left->type_structure;
		if (context->role == BINDING_JOB && context->binder == binder && family_parameter_annotation(job->type_structure))
			job->type_structure = family_parameter_structure(synthesis->typing->graph, job->type_structure);
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
	if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
	if (!context->result || pg_evidence_judgement(context->result) != PG_JUDGEMENT_CONTEXT) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	const struct pg_context *declaration = pg_context_lookup(pg_evidence_context(context->result), binder);
	job->type_structure = declaration ? declaration->declared_type : NULL;
	finish(synthesis, job, declaration ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static const struct pg_term *continuation_effect_structure(struct pg_synthesis *synthesis,
	const struct pg_term *type, const struct pg_term *row)
{
	const struct pg_term *following, *result;
	const struct pg_term *codomain = pg_pi_constant_codomain(type);
	if (!codomain) return NULL;
	if (!pg_effect_type_spine_view(codomain, &following, &result)) return NULL;
	return pg_effect_type_spine(synthesis->classifiers,
		pg_effect_join_term(synthesis->typing->graph, row, following), result);
}

static void pi_application_structure_step(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct pg_term *type, struct pg_synthesis_job *argument)
{
	if (!job->right) job->right = pg_synthesis_term_structure(synthesis, argument);
	if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	if (!job->structural_substitution) {
		const struct pg_term *domain, *codomain;
		const struct pg_object *binder;
		if (!pg_pi_view(type, &domain, &binder, &codomain)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
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
	if (await_source_preparation(synthesis, job, producer)) return;
	if (producer->role == HANDLER_JOB) {
		struct pg_synthesis_job *carrier = (void *)producer->inputs[1];
		if (!carrier && producer->handler) carrier = producer->handler->carrier;
		if (!carrier) {
			if (producer->status != PG_SYNTHESIS_PENDING) {
				finish(synthesis, job, producer->status == PG_SYNTHESIS_DONE ? PG_SYNTHESIS_UNSUPPORTED : producer->status); return;
			}
			if (producer->dependency) depend(synthesis, job, producer->dependency->child);
			else enqueue(synthesis, job);
			return;
		}
		if (!job->left) job->left = pg_synthesis_type_structure(synthesis, carrier);
		forward_structure(synthesis, job);
		return;
	}
	if (producer->role == CLASSIFIER_JOB) {
		if (!job->left) job->left = pg_synthesis_classifier_structure(synthesis, (void *)producer->inputs[1]);
		if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_reduction_certificate *receipt = normalization_receipt(synthesis, job,
			pg_synthesis_type_structure_result(job->left), PG_REDUCTION_WHNF);
		if (!receipt) return;
		job->type_structure = pg_reduction_target(receipt);
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	if (producer->role == EXPECT_JOB) {
		if (!job->left) job->left = pg_synthesis_type_structure(synthesis, (void *)producer->inputs[1]);
		forward_structure(synthesis, job);
		return;
	}
	if (producer->role == BODY_JOB) {
		struct pg_synthesis_job *rule = body_rule(producer);
		int returns_value = body_rule_polarity(rule);
		if (returns_value >= 0) {
			if (!job->left) job->left = pg_synthesis_classifier_structure(synthesis, rule);
			if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
			if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
			const struct pg_term *type = pg_synthesis_type_structure_result(job->left);
			job->type_structure = returns_value ? pg_return_type(synthesis->classifiers, type) : type;
			finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
			return;
		}
	}
	struct pg_synthesis_job *source_rule = prepared_source_rule(producer);
	if (source_rule) {
		if (!job->left) job->left = pg_synthesis_classifier_structure(synthesis, source_rule);
		/* A failed provisional FOLD may still close by checked pure APP. */
		if (producer->role == SEQUENCE_JOB && job->left &&
			(job->left->status == PG_SYNTHESIS_REJECTED || job->left->status == PG_SYNTHESIS_UNSUPPORTED))
			goto accepted_classifier;
		forward_structure(synthesis, job);
		return;
	}
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	if (input && input->rule == PG_UNIVERSE_FORM && input->parameters.level != UINT64_MAX) {
		job->type_structure = pg_universe(synthesis->classifiers, input->parameters.level + 1);
		finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (!job->left && input) {
		struct pg_synthesis_job *premise = rule_premise(synthesis, producer, 0);
		if (premise) switch (input->rule) {
		case PG_VARIABLE:
			job->left = request_job(synthesis, DECLARED_TYPE_JOB, premise, input->parameters.binder); break;
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
	if (job->left) {
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_term *type = pg_synthesis_type_structure_result(job->left);
		if (input->rule == PG_REQUEST_INTRO) {
			const struct pg_object *label = input->parameters.operation_label;
			if (!label) goto accepted_classifier;
			const struct pg_effect_row *row = pg_effect_row(synthesis->typing->graph, 1, &label);
			job->type_structure = continuation_effect_structure(synthesis, type,
				pg_effect_reference(synthesis->typing->graph, row));
			if (!job->type_structure) goto accepted_classifier;
			finish(synthesis, job, PG_SYNTHESIS_DONE);
			return;
		}
		if (input->rule == PG_FOLD_ELIM) {
			if (!job->right) job->right = pg_synthesis_classifier_structure(synthesis, rule_premise(synthesis, producer, 1));
			if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
			if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
			const struct pg_term *row, *value;
			if (!pg_effect_type_spine_view(type, &row, &value)) goto accepted_classifier;
			job->type_structure = continuation_effect_structure(synthesis, job->right->type_structure, row);
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
		} else if (input->rule == PG_THUNK_INTRO) type = pg_thunk_type(synthesis->classifiers, type);
		else if (input->rule == PG_RETURN_INTRO) type = pg_return_type(synthesis->classifiers, type);
		job->type_structure = type;
		finish(synthesis, job, type ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
accepted_classifier:
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
	job->type_structure = producer->result ? pg_evidence_classifier(producer->result) : NULL;
	finish(synthesis, job, job->type_structure ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
}

static void type_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (await_source_preparation(synthesis, job, producer)) return;
	if (producer->role == CLASSIFIER_FORMATION_JOB) {
		if (!job->left) job->left = pg_synthesis_classifier_structure(synthesis, (void *)producer->inputs[1]);
		forward_structure(synthesis, job);
		return;
	}
	if (producer->role == DOMAIN_JOB) {
		if (await_source_preparation(synthesis, job, producer->left)) return;
		if (source_value_kind(producer->left) == 2) {
			if (!job->left) job->left = pg_synthesis_type_structure(synthesis, producer->left);
			forward_structure(synthesis, job);
			return;
		}
		struct pg_synthesis_job *rule = prepared_source_rule(producer->left);
		if (rule && rule->role == DERIVATION_JOB) {
			const struct pg_derivation_input *domain = rule->inputs[0];
			if (domain->rule == PG_VARIABLE) {
				if (!job->left) job->left = pg_synthesis_term_structure(synthesis, rule);
				forward_structure(synthesis, job);
				return;
			}
		}
	}
	struct pg_synthesis_job *source_rule = prepared_source_rule(producer);
	if (source_rule) {
		if (!job->left) job->left = pg_synthesis_type_structure(synthesis, source_rule);
		forward_structure(synthesis, job);
		return;
	}
	const struct pg_derivation_input *input = producer->role == DERIVATION_JOB ? producer->inputs[0] : NULL;
	if (input) {
		switch (input->rule) {
		case PG_UNIVERSE_FORM:
			job->type_structure = pg_universe(synthesis->classifiers, input->parameters.level);
			goto done;
		case PG_RETURN_TYPE_FORM: case PG_THUNK_TYPE_FORM: case PG_PI_FORM: case PG_PI_DOMAIN: case PG_PI_CODOMAIN: case PG_RETURN_CONTENT:
		case PG_CONTEXT_PROJECTION: case PG_TYPE_FROM_VALUE: case PG_PI_CONSTANT_CODOMAIN:
			break;
		default: input = NULL; break;
		}
	}
	if (!input) {
		if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
		if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
		const struct pg_evidence *proof = producer->result;
		if (!proof) goto unsupported;
		enum pg_evidence_judgement kind = pg_evidence_judgement(proof);
		if (kind != PG_JUDGEMENT_VALUE_TYPE && kind != PG_JUDGEMENT_COMPUTATION_TYPE) goto unsupported;
		job->type_structure = pg_evidence_subject(proof)->core;
		goto done;
	}
	if (input->rule == PG_PI_FORM) {
		struct pg_synthesis_job *context = rule_premise(synthesis, producer, 0);
		if (!context) goto unsupported;
		const struct pg_object *binder = NULL;
		/* An unfinished context request can expose its declared annotation for
		 * effect equations. This is structure only, never accepted Pi evidence. */
		if (context->role == DERIVATION_JOB) {
			const struct pg_derivation_input *extension = context->inputs[0];
			if (extension->rule == PG_CONTEXT_EXTEND || extension->rule == PG_CONTEXT_FAMILY_EXTEND)
				binder = extension->parameters.binder;
		} else if (context->role == PI_SCOPE_JOB || context->role == BINDING_JOB)
			binder = context->binder;
		if (!binder) {
			if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
			if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
			if (!context->result || pg_evidence_judgement(context->result) != PG_JUDGEMENT_CONTEXT) goto unsupported;
			const struct pg_context *scope = pg_evidence_context(context->result);
			if (!scope) goto unsupported;
			binder = scope->binder;
		}
		if (!job->left) job->left = request_job(synthesis, DECLARED_TYPE_JOB, context, binder);
		if (!job->left) goto unsupported;
		if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		if (!job->right) job->right = pg_synthesis_type_structure(synthesis, rule_premise(synthesis, producer, 1));
		if (!job->right) goto unsupported;
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
		job->type_structure = pg_pi(synthesis->typing->graph, job->left->type_structure, binder, job->right->type_structure);
		goto done;
	}
	if (!job->left) {
		size_t index = input->rule == PG_CONTEXT_PROJECTION ? 1 : 0;
		struct pg_synthesis_job *premise = rule_premise(synthesis, producer, index);
		job->left = input->rule == PG_TYPE_FROM_VALUE ? pg_synthesis_term_structure(synthesis, premise)
			: pg_synthesis_type_structure(synthesis, premise);
		if (!job->left) goto unsupported;
	}
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
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
		job->type_structure = pg_effect_type_spine(synthesis->classifiers, row, left);
		break;
	}
	case PG_THUNK_TYPE_FORM:
		job->type_structure = pg_thunk_type(synthesis->classifiers, left); break;
	case PG_CONTEXT_PROJECTION: case PG_TYPE_FROM_VALUE:
		job->type_structure = left; break;
	case PG_RETURN_CONTENT: {
		const struct pg_term *row;
		if (!pg_effect_type_spine_view(left, &row, &job->type_structure)) goto unsupported;
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
			if (await_source_preparation(synthesis, job, premise)) return;
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
		wake(synthesis, job, 1);
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
		if (!p) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (p->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, p); return; }
		if (p->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, p->status); return; }
		state->premises[state->next++] = p->result;
		enqueue(synthesis, job);
		return;
	}
	int converting = input->rule == PG_TYPE_CONVERSION;
	int normalizing = input->rule == PG_PURE_NORMALIZATION;
	struct pg_derivation_parameters parameters = input->parameters;
	if (job->inputs[1]) {
		struct pg_synthesis_job *effects = (void *)job->inputs[1];
		if (effects->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, effects); return; }
		if (effects->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, effects->status); return; }
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
				if (!comparison) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				if (comparison->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, comparison); return; }
				if (comparison->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, comparison->status); return; }
				job->certificate = comparison->certificate;
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
	job->result = pg_prove_derivation(synthesis->typing, synthesis->classifiers,
		input->rule, &parameters, input->count, state->premises);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}

static void operation_reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *producer = (void *)job->inputs[0];
	if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
	if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
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
	if (effects->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, effects); return; }
	if (effects->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, effects->status); return; }
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
	if (job->role != EXPRESSION_JOB || job->syntax->kind != PG_SYNTAX_ATOM) return 0;
	if (job->syntax->token.kind == '@') {
		if (!job->left) {
			struct pg_derivation_input input = {.rule = PG_UNIVERSE_FORM, .count = 1};
			job->left = pg_synthesis_rule(synthesis, &input, &job->scope->context_job, NULL, NULL);
		}
		forward_proof(synthesis, job, job->left);
		return 1;
	}
	if (job->syntax->token.kind != PG_TOKEN_IDENT) return 0;
	if (job->value_job) {
		if (job->left) job->exports = job->left->exports;
		forward_proof(synthesis, job, job->value_job); return 1;
	}
	if (!job->binder) {
		if (job->left) return 0;
		struct source_reference reference = lookup_scope(job->scope, job->syntax->token);
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
		if (!reference.binder) return 0;
		struct pg_derivation_input input = {.rule = PG_VARIABLE,
			.parameters.binder = reference.binder, .count = 1};
		job->binder = reference.binder;
		job->left = pg_synthesis_rule(synthesis, &input, &job->scope->context_job, NULL, NULL);
	}
	forward_proof(synthesis, job, job->left);
	return 1;
}

static int prepare_expression(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB || job->stage) return 1;
	const struct pg_syntax *syntax = job->syntax;
	const struct pg_source_scope *right_scope = job->scope;
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
		struct pg_derivation_input input = {.rule = PG_THUNK_INTRO, .count = 1};
		job->right = pg_synthesis_rule(synthesis, &input, &job->left, NULL, NULL);
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
	struct context_allocation *allocation = job->context_allocation;
	if (allocation && allocation->next == allocation->count) return PG_SYNTHESIS_REJECTED;
	const struct pg_object *binder = allocation
		? allocation->binders[allocation->next++] : pg_binder(synthesis->typing->graph);
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

/* Expose the callee first, then sequence its argument, using ordinary rules. */
static int prepare_application(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->role != EXPRESSION_JOB || job->syntax->kind != PG_SYNTAX_APPLICATION) return 0;
	if (job->stage == APPLICATION_RULE_READY) {
		if (job->context_allocation) {
			struct pg_synthesis_job *prefix = job->scope->context_job;
			if (prefix->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, prefix); return 1; }
			if (prefix->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, prefix->status); return 1; }
			if (pg_evidence_context(prefix->result) != job->context_allocation->prefix) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1;
			}
		}
		forward_proof(synthesis, job, job->value_job);
		return 1;
	}
	if (job->stage != 2) return 0;
	if (!job->application) {
		if (job->allocation_origin) {
			struct pg_synthesis_job *origin = job->allocation_origin;
			if (origin->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, origin); return 1; }
			if (origin->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, origin->status); return 1; }
			/* Binder identity selects the saved prefix before its effects are
			 * solved. Exact typed-prefix agreement still gates final acceptance. */
			const struct pg_source_scope *binding = job->scope;
			while (binding && !binding->binder) binding = binding->parent;
			const struct pg_context *end = pg_evidence_context(origin->result), *prefix = end;
			while (prefix && (!binding || prefix->binder != binding->binder)) prefix = prefix->parent;
			if (binding && !prefix) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1; }
			if (context_allocation_at(synthesis, job, prefix, end, 0)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1;
			}
		}
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
		if (job->context_allocation && job->context_allocation->next != job->context_allocation->count) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1;
		}
		job->stage = APPLICATION_RULE_READY;
		enqueue(synthesis, job);
		return 1;
	}
	/* Choose the pending rule from its signature, then check its premises.
	 * A logical signature ends in Universe, not F(Universe). Queue order must
	 * not decide whether an as-yet-unaccepted family is a CBPV computation. */
	struct pg_synthesis_job *raw_shape = pg_synthesis_classifier_structure(synthesis, state->callee);
	if (!raw_shape) goto error;
	if (raw_shape->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, raw_shape); return 1; }
	if (raw_shape->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, raw_shape->status); return 1; }
	if (logical_family_signature(pg_synthesis_type_structure_result(raw_shape))) {
		struct pg_synthesis_job *argument = state->argument;
		if (argument->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, argument); return 1; }
		if (argument->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, argument->status); return 1; }
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
	if (shape->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, shape); return 1; }
	if (shape->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, shape->status); return 1; }
	const struct pg_term *type = pg_synthesis_type_structure_result(shape);
	const struct pg_term *domain, *codomain, *forced;
	const struct pg_object *binder;
	if (pg_thunk_type_view(type, &forced)) {
		state->callee = plain_rule(synthesis, PG_FORCE_ELIM, NULL, 1, &callee);
		if (!state->callee) goto error;
		enqueue(synthesis, job);
		return 1;
	}
	if (pg_effect_type_spine_view(type, &domain, &codomain)) {
		enum pg_synthesis_status status = application_bind(synthesis, job, callee, 1);
		if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return 1; }
		enqueue(synthesis, job);
		return 1;
	}
	if (!pg_pi_view(type, &domain, &binder, &codomain)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 1; }
	struct pg_synthesis_job *argument = state->argument;
	if (await_source_preparation(synthesis, job, argument)) return 1;
	if (logical_family_signature(domain)) {
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
		if (shape->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, shape); return 1; }
		if (shape->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, shape->status); return 1; }
		const struct pg_term *row, *result;
		if (!pg_effect_type_spine_view(pg_synthesis_type_structure_result(shape), &row, &result)) {
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
	state->tail = pg_synthesis_application_jobs(synthesis, state->context, callee, argument);
	if (!state->tail) goto error;
	enqueue(synthesis, job);
	return 1;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return 1;
}

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
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
		if (job->left->status == PG_SYNTHESIS_DONE && job->left->result &&
			pg_evidence_judgement(job->left->result) == PG_JUDGEMENT_TYPE_FAMILY) {
			struct pg_synthesis_job *callable = family_function(synthesis, job->left);
			job->right = plain_rule(synthesis, PG_THUNK_INTRO, NULL, 1, &callable);
		}
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
	if (job->role == EXPRESSION_JOB && handler_syntax(job->syntax)) {
		if (job->syntax->item_count == 1) { return_handler_step(synthesis, job); return; }
		if (!job->value_job) job->value_job = pg_synthesis_handler(synthesis, job->scope, NULL, job->syntax);
		forward_proof(synthesis, job, job->value_job);
		return;
	}
	if (job->scope && job->role != TELESCOPE_JOB && job->role != TELESCOPE_STRUCTURE_JOB) {
		struct pg_synthesis_job *context = job->scope->context_job;
		if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
		if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
		if (!source_context(job->scope)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	const struct pg_syntax *syntax = job->syntax;
	if (job->role == SCOPE_CONTEXT_JOB) {
		struct pg_synthesis_job *parent = job->scope->context_job;
		if (parent->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, parent); return; }
		if (parent->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, parent->status); return; }
		struct pg_synthesis_job *context = (void *)job->inputs[2];
		if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
		if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
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
		if (structure->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, structure); return; }
		if (structure->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, structure->status); return; }
		const struct pg_term *row, *value;
		if (!pg_effect_type_spine_view(structure->type_structure, &row, &value)) {
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
			if (!children[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (children[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, children[i]); return; }
			if (children[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, children[i]->status); return; }
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
	if (job->role == SOURCE_EXPECT_JOB) { source_expect_step(synthesis, job); return; }
	if (job->role == TYPE_STRUCTURE_JOB) { type_structure_step(synthesis, job); return; }
	if (job->role == BODY_JOB || job->role == CLASSIFIER_FORMATION_JOB) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *premise = (void *)job->inputs[i];
			if (!premise) continue;
			if (premise->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, premise); return; }
			if (premise->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, premise->status); return; }
		}
		const struct pg_synthesis_job *first = job->inputs[0];
		if (job->role == BODY_JOB) {
			const struct pg_synthesis_job *context = job->inputs[1];
			if (context && pg_evidence_context(first->result) != pg_evidence_context(context->result)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			job->result = computation(synthesis, first->result);
		}
		else {
			const struct pg_synthesis_job *body = job->inputs[1];
			if (!job->classifier_recovery) {
				job->classifier_recovery = pg_alloc(synthesis->typing->graph, sizeof(*job->classifier_recovery));
				if (!job->classifier_recovery) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
				pg_classifier_recovery_init(job->classifier_recovery, synthesis->typing, synthesis->classifiers, first->result, body->result);
			}
			int status = pg_classifier_recovery_advance(job->classifier_recovery, 1);
			if (!status) { enqueue(synthesis, job); return; }
			job->result = status > 0 ? job->classifier_recovery->result : NULL;
			pg_classifier_recovery_destroy(job->classifier_recovery);
		}
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE
			: job->role == BODY_JOB ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_UNSUPPORTED);
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
			if (producer->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producer); return; }
			if (producer->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producer->status); return; }
			if (!face_formation(synthesis, job->inputs[0], producer->result)) {
				finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
			}
			struct pg_synthesis_job *canonical = pg_synthesis_identity_face(synthesis,
				job->inputs[0], producer->result, job->inputs[2]);
			if (forward_proof(synthesis, job, canonical)) return;
			if (!job->left) job->left = pg_synthesis_identity_formation(synthesis, producer);
			if (!job->left) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
			if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
			canonical = pg_synthesis_identity_face(synthesis, job->inputs[0], job->left->result, job->inputs[2]);
			if (forward_proof(synthesis, job, canonical)) return;
			job->face = pg_identity_face_init(synthesis->typing, synthesis->classifiers,
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
		if (pg_evidence_context(context) != pg_evidence_context(input)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_prove_projection(synthesis->typing, context, input) != input) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_evidence_judgement(input) == PG_JUDGEMENT_VALUE_TYPE) input = value(synthesis, input);
		if (!job->right) job->right = request_job(synthesis, CLASSIFIER_FORMATION_JOB,
			pg_synthesis_evidence(synthesis, context), pg_synthesis_evidence(synthesis, input));
		if (!job->right) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
		if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
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
				if (premise->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, premise); return; }
				if (premise->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, premise->status); return; }
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
	if (job->left->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->left); return; }
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (job->right->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->right); return; }
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	const struct pg_evidence *right = job->right->result;
	switch (syntax->kind) {
	case PG_SYNTAX_PI: {
		job->domain = job->left->domain;
		const struct pg_evidence *codomain = type_input(synthesis, job, source_context(job->inner), right);
		if (!codomain) return;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, synthesis->classifiers, source_context(job->inner), codomain);
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
	if (!root || !root->definitions || !name.length || !name.text) return NULL;
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
