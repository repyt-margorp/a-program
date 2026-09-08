#include "synthesis.h"
#include "computation.h"
#include "iadt.h"
#include "action.h"
#include "derivation_io.h"

#include <stdlib.h>
#include <string.h>

struct pg_source_scope {
	struct pg_index_entry index;
	const void *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder;
	const struct pg_object *hypothesis_for;
	struct pg_synthesis_job *context_job;
	struct definition_state *definitions;
	struct pg_synthesis_job *producer;
	const struct pg_source_scope *exports;
	struct pg_synthesis_job *module;
	const struct pg_source_scope *imports;
};
struct waiter {
	struct pg_synthesis_job *parent;
	struct pg_synthesis_job *child;
	struct waiter *next;
};
struct continuation_frame {
	const struct pg_evidence *input;
	const struct pg_evidence *value;
	const struct pg_evidence *domain;
	const struct pg_evidence *context;
	const struct continuation_frame *parent;
};
struct block_name {
	struct pg_index_entry index;
	struct pg_token name;
	struct pg_synthesis_job *producer;
	struct pg_synthesis_job *imported;
};
struct definition_state {
	struct pg_index names;
	const struct pg_syntax *syntax;
	const struct pg_source_scope *scope;
	struct pg_synthesis_job **entries;
	size_t count, indexed, activated, next;
};
struct block_state {
	const struct pg_syntax *syntax;
	size_t next, end;
	const struct pg_source_scope *scope;
	const struct continuation_frame *frames;
	const struct pg_evidence *tail;
	struct pg_index names;
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
struct match_branch {
	const struct pg_source_scope *scope;
	const struct pg_syntax *clause;
	struct pg_synthesis_job *body;
	const struct pg_evidence *function;
	int needs_ih;
};
struct match_state {
	struct pg_inductive_instance instance;
	const struct pg_source_scope *labels;
	const struct pg_evidence *motive;
	const struct pg_evidence *motive_context;
	size_t count, next, checked, prepared;
	int induction;
	struct match_branch branches[];
};
struct family_state {
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations;
	const struct pg_evidence **paths;
	size_t count, common, next;
};
struct derivation_state {
	size_t next;
	const struct pg_evidence **premises;
	struct pg_comparison endpoint;
	const struct pg_reduction_certificate *reduction;
};
enum job_role { EXPRESSION_JOB, DEFINITION_JOB, DEFINITION_SCOPE_JOB, EVIDENCE_JOB, RETURN_JOB, THUNK_JOB, NORMALIZATION_JOB, NF_JOB,
	REFLEXIVITY_JOB, CLASSIFIER_JOB, FAMILY_ACTION_JOB, FORMATION_JOB, FACE_JOB, EXPECT_JOB, APPLICATION_JOB, INSTANCE_JOB, CONVERSION_JOB, DATA_CASE_JOB, REINDEX_JOB, PAIR_JOB, SUBSTITUTION_JOB, BINDING_JOB, TELESCOPE_JOB, TELESCOPE_STRUCTURE_JOB, DATA_RESULT_JOB, DATA_SCHEMA_JOB, CONSTRUCTOR_JOB, CONSTRUCTOR_VALUE_JOB, INDUCTION_BRANCH_JOB, CONSTANT_MOTIVE_JOB, DERIVATION_JOB };
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
	const struct pg_evidence *function;
	const struct pg_evidence *continuation;
	struct pg_conversion comparison;
	const struct pg_conversion_certificate *certificate;
	struct pg_reindex reindex;
	struct pg_identity_face_work *face;
	struct pg_identity_formation_work *formation;
	union { struct pg_whnf_job *whnf; struct pg_nf_job *nf; } normalizing;
	struct block_state *block;
	const struct continuation_frame *application_frame;
	struct definition_state *definitions;
	struct substitution_state *substitution;
	struct declaration_state *declaration;
	const struct pg_data_schema *schema;
	const struct pg_source_scope *exports;
	struct match_state *match;
	struct family_state *family;
	struct derivation_state *derivation;
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
			pg_identity_face_destroy(job->face);
			pg_identity_formation_destroy(job->formation);
			if (job->derivation) pg_comparison_destroy(&job->derivation->endpoint);
			if (job->block) pg_index_destroy(&job->block->names);
			if (job->definitions) pg_index_destroy(&job->definitions->names);
			if (job->declaration) pg_index_destroy(&job->declaration->names);
		}
	pg_index_destroy(&synthesis->jobs);
	pg_index_destroy(&synthesis->scopes);
	memset(synthesis, 0, sizeof(*synthesis));
}

static uint64_t name_hash(struct pg_token name);
static int same_name(struct pg_token left, struct pg_token right);

static const struct pg_source_scope *intern_scope(struct pg_synthesis *synthesis, struct pg_source_scope input)
{
	if (!input.context_job || input.context_job->owner != synthesis->owner_key) return NULL;
	/* Special roots carry their spelling in kind, not borrowed text. */
	if (input.name.kind == '#' || input.name.kind == '*')
		input.name = (struct pg_token){.kind = input.name.kind};
	if (input.name.length && !input.name.text) return NULL;
	uint64_t hash = name_hash(input.name) ^ (unsigned)input.name.kind;
	const void *pointers[] = {input.parent, input.context_job, input.binder, input.hypothesis_for, input.definitions, input.producer, input.exports, input.module, input.imports};
	for (size_t i = 0; i < sizeof(pointers) / sizeof(*pointers); ++i)
		hash = (hash ^ (uintptr_t)pointers[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->scopes, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		const struct pg_source_scope *scope = (const struct pg_source_scope *)entry;
		if (scope->parent != input.parent) continue;
		if (scope->context_job != input.context_job) continue;
		if (scope->binder != input.binder) continue;
		if (scope->hypothesis_for != input.hypothesis_for) continue;
		if (scope->definitions != input.definitions) continue;
		if (scope->producer != input.producer) continue;
		if (scope->exports != input.exports) continue;
		if (scope->module != input.module) continue;
		if (scope->imports != input.imports) continue;
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

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!parent || parent->owner != synthesis->owner_key || !extended_context) return NULL;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return NULL;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!source_context(parent)) return NULL;
	if (!context || context->parent != pg_evidence_context(source_context(parent))) return NULL;
	if (context->binder != binder) return NULL;
	/* Check ownership through a primitive judgement, not just a context pointer. */
	if (!pg_prove_variable(synthesis->typing, extended_context, binder)) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context_job = pg_synthesis_evidence(synthesis, extended_context)});
}

static struct pg_synthesis_job *request_inputs(struct pg_synthesis *synthesis,
	enum job_role role, size_t count, const void *const *inputs)
{
	if (count > (SIZE_MAX - sizeof(struct pg_synthesis_job)) / sizeof(*inputs)) return NULL;
	uint64_t hash = ((unsigned)role ^ count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)inputs[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->jobs, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_synthesis_job *job = (struct pg_synthesis_job *)entry;
		if (job->role != role) continue;
		if (job->input_count != count) continue;
		size_t i = 0;
		while (i < count && job->inputs[i] == inputs[i]) ++i;
		if (i == count) return job;
	}
	struct pg_synthesis_job *job = pg_alloc(synthesis->typing->graph, sizeof(*job) + count * sizeof(*inputs));
	if (!job) return NULL;
	job->owner = synthesis->owner_key;
	job->role = role;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = inputs[i];
	if (role == BINDING_JOB) {
		job->binder = pg_binder(synthesis->typing->graph);
		if (!job->binder) return NULL;
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

struct pg_synthesis_job *pg_synthesis_request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	return request_role(synthesis, scope, syntax, EXPRESSION_JOB);
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

struct pg_synthesis_job *pg_synthesis_binding(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	if (!syntax || (syntax->kind != PG_SYNTAX_LAMBDA && syntax->kind != PG_SYNTAX_PI)) return NULL;
	struct pg_synthesis_job *job = request_role(synthesis, scope, syntax, BINDING_JOB);
	if (!job) return NULL;
	if (!job->inner) {
		struct pg_token name = syntax->token;
		if (syntax->kind == PG_SYNTAX_PI)
			name = syntax->left->kind == PG_SYNTAX_BINDER ? syntax->left->token : (struct pg_token){0};
		job->inner = intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
			.name = name, .binder = job->binder, .context_job = job});
	}
	return job->inner ? job : NULL;
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

struct pg_synthesis_job *pg_synthesis_derivation(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input)
{
	if (!input) return NULL;
	const void *inputs[] = {input};
	return request_inputs(synthesis, DERIVATION_JOB, 1, inputs);
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

struct pg_synthesis_job *pg_synthesis_application(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!function || function->owner != synthesis->owner_key) return NULL;
	if (!argument || argument->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {context, function, argument};
	return request_inputs(synthesis, APPLICATION_JOB, 3, inputs);
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
	if (!declaration || declaration->kind != PG_SYNTAX_DECLARATION) return NULL;
	return request_role(synthesis, parameters, declaration, DATA_SCHEMA_JOB);
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

static struct pg_synthesis_job *request_typed(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof, enum job_role role)
{
	if (!context || !proof) return NULL;
	if (!pg_evidence_subject(proof)) return NULL;
	if (pg_evidence_context(context) != pg_evidence_context(proof)) return NULL;
	if (pg_prove_projection(synthesis->typing, context, proof) != proof) return NULL;
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
	return request_typed(synthesis, context, proof, NORMALIZATION_JOB);
}

struct pg_synthesis_job *pg_synthesis_nf(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	return request_typed(synthesis, context, proof, NF_JOB);
}

struct pg_synthesis_job *pg_synthesis_normalize_classifier(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!proof) return NULL;
	switch (pg_evidence_judgement(proof)) {
	case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION:
		return request_typed(synthesis, context, proof, CLASSIFIER_JOB);
	default: return NULL;
	}
}

static void finish(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	enum pg_synthesis_status status)
{
	job->status = status;
	for (struct waiter *waiter = job->waiters; waiter; waiter = waiter->next) {
		waiter->parent->dependency = NULL;
		enqueue(synthesis, waiter->parent);
	}
	job->waiters = NULL;
}

/* Subscribe exactly once at a stage transition. Completed dependencies need
 * no subscription; pending dependencies wake their consumers on completion. */
static void depend(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child)
{
	if (!child) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	if (child->status != PG_SYNTHESIS_PENDING) {
		enqueue(synthesis, parent);
		return;
	}
	struct waiter *waiter = pg_alloc(synthesis->typing->graph, sizeof(*waiter));
	if (!waiter) { finish(synthesis, parent, PG_SYNTHESIS_ERROR); return; }
	*waiter = (struct waiter){parent, child, child->waiters};
	child->waiters = waiter;
	parent->dependency = waiter;
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

static void binding_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
	if (syntax->binder_marker) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	if (!job->left) {
		const struct pg_syntax *domain = syntax->left;
		if (syntax->kind == PG_SYNTAX_PI && domain->kind == PG_SYNTAX_BINDER) domain = domain->left;
		job->left = pg_synthesis_request(synthesis, job->scope, domain);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	const struct pg_evidence *input = type_input(synthesis, job, source_context(job->scope), job->left->result);
	if (!input) return;
	job->domain = value_type(synthesis, input);
	if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	job->result = pg_prove_context_extension(synthesis->typing, source_context(job->scope), job->binder, job->domain);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void telescope_structure_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->inner) { job->inner = job->scope; job->tail = job->syntax; }
	if (job->tail->kind != job->syntax->kind ||
		(job->tail->kind != PG_SYNTAX_LAMBDA && job->tail->kind != PG_SYNTAX_PI)) {
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	struct pg_synthesis_job *binding = pg_synthesis_binding(synthesis, job->inner, job->tail);
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
		job->checking_term = job->inputs[1];
		const struct pg_evidence *formation = pg_prove_classifier(synthesis->typing,
			synthesis->classifiers, job->inputs[0], job->checking_term);
		if (!formation) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		job->left = pg_synthesis_normalize(synthesis, job->inputs[0], formation);
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
				struct source_reference field = lookup_scope(scope, name);
				for (const struct pg_context *context = pg_evidence_context(source_context(scope));
					context && context != prefix; context = context->parent) {
					if (context->binder == field.binder) { result = 1; goto done; }
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
					inner = marker_bind(&arena, inner, clause->items[j].name);
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

static int hypothesis_reference(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
	if (syntax->kind != PG_SYNTAX_APPLICATION || syntax->left->kind != PG_SYNTAX_ATOM) return 0;
	if (syntax->left->token.kind != '*' || syntax->right->kind != PG_SYNTAX_ATOM) return 0;
	if (lookup_scope(job->scope, (struct pg_token){.kind = '*'}).binder) return 0;
	struct source_reference field = lookup_scope(job->scope, syntax->right->token);
	if (!field.binder) return 0;
	for (const struct pg_source_scope *scope = job->scope; scope; scope = scope->parent) {
		if (scope->hypothesis_for != field.binder) continue;
		const struct pg_evidence *value = pg_prove_variable(synthesis->typing, source_context(job->scope), scope->binder);
		job->result = pg_prove_force(synthesis->typing, value);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return 1;
	}
	return 0;
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
		struct pg_synthesis_job *registration = request_role(synthesis, module->scope, module->syntax, DEFINITION_SCOPE_JOB);
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
		struct pg_inductive_instance instance;
		if (!pg_inductive_instance(synthesis->typing, value_type(synthesis, proof), &instance))
			return PG_SYNTHESIS_UNSUPPORTED;
		struct pg_synthesis_job *origin = pg_synthesis_evidence(synthesis, instance.formation);
		if (!origin || !origin->exports) return PG_SYNTHESIS_UNSUPPORTED;
		struct source_reference member = lookup_scope(origin->exports, token);
		if (!member.producer) return PG_SYNTHESIS_REJECTED;
		if (member.producer->role != CONSTRUCTOR_VALUE_JOB) return PG_SYNTHESIS_UNSUPPORTED;
		const void *inputs[] = {instance.formation, member.producer->inputs[1], instance.parameters};
		*reference = (struct source_reference){.producer = request_inputs(synthesis, CONSTRUCTOR_VALUE_JOB, 3, inputs)};
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

static void reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *proof = job->left->result;
		if (!proof || !pg_evidence_subject(proof)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		/* Definition storage quotes a raw computation; a source reference keeps
		 * that computation's meaning. Explicit source quotation remains a value. */
		if (job->left->role == DEFINITION_JOB &&
			pg_evidence_judgement(job->left->left->result) == PG_JUDGEMENT_COMPUTATION)
			proof = pg_prove_force(synthesis->typing, proof);
		if (!proof) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (job->syntax->kind == PG_SYNTAX_IMPORT && pg_evidence_context(proof)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		job->result = pg_prove_projection(synthesis->typing, source_context(job->scope), proof);
		job->exports = job->left->exports;
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	struct pg_token token = job->syntax->token;
	if (job->syntax->kind == PG_SYNTAX_ATOM && token.kind == '@') {
		job->result = pg_prove_universe(synthesis->typing, synthesis->classifiers, source_context(job->scope), 0);
	} else if (job->syntax->kind == PG_SYNTAX_ATOM && token.kind == '*') {
		/* An explicitly supplied type assumption stays in the proof context.
		 * This is not recursive declaration admission or an IH lookup. */
		struct source_reference reference = lookup_scope(job->scope, token);
		if (!reference.binder) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		job->result = pg_prove_value_type(synthesis->typing,
			pg_prove_variable(synthesis->typing, source_context(job->scope), reference.binder));
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

static struct continuation_frame *open_continuation(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *input)
{
	const struct pg_evidence *return_type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, input);
	const struct pg_evidence *domain = pg_prove_return_content(synthesis->typing, return_type);
	if (!domain) return NULL;
	const struct pg_object *binder = pg_binder(synthesis->typing->graph);
	context = pg_prove_context_extension(synthesis->typing, context, binder, domain);
	if (!context) return NULL;
	struct continuation_frame *frame = pg_alloc(synthesis->typing->graph, sizeof(*frame));
	if (frame) *frame = (struct continuation_frame){.input = input, .domain = domain, .context = context};
	return frame;
}

static const struct pg_evidence *close_continuation(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, const struct continuation_frame *frame,
	const struct pg_evidence *body)
{
	if (!job->continuation) {
		const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, frame->context, body);
		const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, frame->domain, frame->context, codomain);
		job->continuation = pg_prove_lambda(synthesis->typing, pi, body);
		if (!job->continuation) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return NULL; }
	}
	const struct pg_evidence *result;
	if (!job->value_job) {
		if (!frame->value) {
			result = pg_prove_fold(synthesis->typing, synthesis->classifiers, frame->input, job->continuation);
			if (result) { job->continuation = NULL; return result; }
		}
		/* A dependent result needs an actual checked value producer, not an
		 * assumed execution result in the classifier. */
		const struct pg_evidence *context = pg_evidence_premise(frame->context, 0);
		struct pg_synthesis_job *argument = frame->value ? pg_synthesis_evidence(synthesis, frame->value)
			: pg_synthesis_return(synthesis, context, frame->input);
		job->value_job = pg_synthesis_application(synthesis, context,
			pg_synthesis_evidence(synthesis, job->continuation), argument);
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL; }
	}
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return NULL; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return NULL; }
	result = job->value_job->result;
	job->value_job = NULL;
	job->continuation = NULL;
	return result;
}

static int sequence_operand(struct pg_synthesis *synthesis,
	const struct continuation_frame **frames, const struct pg_evidence **context,
	const struct pg_evidence **operand, const struct pg_evidence **other)
{
	struct continuation_frame *frame = open_continuation(synthesis, *context, *operand);
	if (!frame) return -1;
	frame->parent = *frames;
	*frames = frame;
	*context = frame->context;
	*other = pg_prove_projection(synthesis->typing, *context, *other);
	*operand = pg_prove_variable(synthesis->typing, *context, pg_evidence_context(*context)->binder);
	return *other && *operand ? 0 : -1;
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

static void raw_application_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *context = job->inputs[0];
	if (!job->function) {
		struct pg_synthesis_job *producers[] = {(void *)job->inputs[1], (void *)job->inputs[2]};
		for (size_t i = 0; i < 2; ++i) {
			if (producers[i]->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, producers[i]); return; }
			if (producers[i]->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, producers[i]->status); return; }
		}
		const struct pg_evidence *function = producers[0]->result, *argument = producers[1]->result;
		if (!pg_evidence_owned_by(function, synthesis->typing)) goto rejected;
		if (!pg_evidence_owned_by(argument, synthesis->typing)) goto rejected;
		if (pg_evidence_judgement(function) != PG_JUDGEMENT_COMPUTATION) goto rejected;
		if (pg_evidence_judgement(argument) != PG_JUDGEMENT_VALUE) goto rejected;
		if (pg_evidence_context(function) != pg_evidence_context(context)) goto rejected;
		if (pg_evidence_context(argument) != pg_evidence_context(context)) goto rejected;
		struct pg_synthesis_job *canonical = pg_synthesis_application(synthesis, context,
			pg_synthesis_evidence(synthesis, function), pg_synthesis_evidence(synthesis, argument));
		if (forward_proof(synthesis, job, canonical)) return;
		job->function = function;
		job->checking_term = argument;
	}
	if (!job->stage) {
		const struct pg_evidence *function = classifier_input(synthesis, job, context, job->function);
		if (!function) return;
		job->function = function;
		const struct pg_evidence *pi = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, function);
		const struct pg_evidence *domain = pg_prove_pi_domain(synthesis->typing, pi);
		if (!domain) goto rejected;
		job->value_job = pg_synthesis_expect(synthesis, pg_synthesis_evidence(synthesis, job->checking_term),
			pg_synthesis_evidence(synthesis, domain));
		if (!job->value_job) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->stage = 1;
	}
	if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
	if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
	job->result = pg_prove_application(synthesis->typing, job->function, job->value_job->result);
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

static void application_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	/* Both source operands have synthesized independently. Expose the callee,
	 * prepare the argument, then compare/apply and close sequencing frames. */
	const struct pg_evidence *context = job->application_frame ? job->application_frame->context : source_context(job->scope);
	if (!job->function) { job->function = job->left->result; job->checking_term = job->right->result; }
	if (job->stage == 2) {
		enum pg_evidence_judgement kind = pg_evidence_judgement(job->function);
		if (kind != PG_JUDGEMENT_VALUE && kind != PG_JUDGEMENT_COMPUTATION) goto rejected;
		const struct pg_evidence *function = classifier_input(synthesis, job, context, job->function);
		if (!function) return;
		job->function = function;
		if (kind == PG_JUDGEMENT_VALUE) {
			job->function = pg_prove_force(synthesis->typing, function);
			if (!job->function) goto rejected;
		} else {
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			if (pg_pi_view(pg_evidence_classifier(function), &domain, &binder, &codomain)) job->stage = 3;
			else {
				if (!pg_return_type_view(pg_evidence_classifier(function), &domain)) goto rejected;
				if (sequence_operand(synthesis, &job->application_frame, &context, &job->function, &job->checking_term) != 0)
					goto unsupported;
			}
		}
		enqueue(synthesis, job);
		return;
	}
	if (job->stage == 3) {
		if (pg_evidence_judgement(job->checking_term) == PG_JUDGEMENT_COMPUTATION) {
			const struct pg_evidence *argument = classifier_input(synthesis, job, context, job->checking_term);
			if (!argument) return;
			job->checking_term = argument;
			if (sequence_operand(synthesis, &job->application_frame, &context, &job->checking_term, &job->function) != 0)
				goto unsupported;
		} else job->checking_term = value(synthesis, job->checking_term);
		if (!job->checking_term) goto rejected;
		job->value_job = pg_synthesis_application(synthesis, context,
			pg_synthesis_evidence(synthesis, job->function), pg_synthesis_evidence(synthesis, job->checking_term));
		if (!job->value_job) goto rejected;
		job->stage = 4;
	}
	if (job->stage == 4) {
		if (job->value_job->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, job->value_job); return; }
		if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
		job->result = job->value_job->result;
		job->value_job = NULL;
		job->stage = 5;
	}
	if (job->application_frame) {
		const struct continuation_frame *frame = job->application_frame;
		const struct pg_evidence *result = close_continuation(synthesis, job, frame, job->result);
		if (!result) return;
		job->result = result;
		job->application_frame = frame->parent;
		enqueue(synthesis, job);
		return;
	}
	finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	finish(synthesis, job, PG_SYNTHESIS_REJECTED);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
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
	}
	struct block_state *block = job->block;
	if (block->tail) {
		if (!block->frames) {
			job->result = block->tail;
			finish(synthesis, job, PG_SYNTHESIS_DONE);
			return;
		}
		const struct continuation_frame *frame = block->frames;
		const struct pg_evidence *tail = close_continuation(synthesis, job, frame, block->tail);
		if (!tail) return;
		block->tail = tail;
		block->frames = frame->parent;
		enqueue(synthesis, job);
		return;
	}
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *proof = job->left->result;
		const struct pg_evidence *input = computation(synthesis, proof);
		if (!input) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		/* Discarding a checked returned value needs neither execution nor a
		 * binding. Keep checking the statement, but do not build a dead scope. */
		if (pg_evidence_rule(input) == PG_RETURN_INTRO && block->next < block->end &&
			!block->syntax->items[block->next - 1].name.length) {
			job->left = NULL;
			enqueue(synthesis, job);
			return;
		}
		input = classifier_input(synthesis, job, source_context(block->scope), input);
		if (!input) return;
		if (block->next == block->end) {
			block->tail = input;
		} else {
			struct continuation_frame *frame = open_continuation(synthesis, source_context(block->scope), input);
			if (!frame) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			frame->value = value(synthesis, proof);
			frame->parent = block->frames;
			block->frames = frame;
			block->scope = pg_synthesis_bind(synthesis, block->scope, block->syntax->items[block->next - 1].name,
				pg_evidence_context(frame->context)->binder, frame->context);
			if (!block->scope) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		job->left = NULL;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_syntax_item *item = &block->syntax->items[block->next++];
	int name_status = register_name(synthesis, &block->names, item->name);
	if (name_status) { finish(synthesis, job, name_status > 0 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR); return; }
	const struct pg_syntax *expression = item->expression;
	if (item->annotation) {
		struct pg_syntax *check = pg_alloc(synthesis->typing->graph, sizeof(*check));
		if (!check) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		*check = (struct pg_syntax){.kind = PG_SYNTAX_EXPECT, .token = item->name, .left = expression, .right = item->annotation};
		expression = check;
	}
	job->left = pg_synthesis_request(synthesis, block->scope, expression);
	depend(synthesis, job, job->left);
}

static void definition_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
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

static void definition_scope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->definitions) {
		const struct pg_syntax *syntax = job->syntax;
		struct definition_state *state = pg_alloc(synthesis->typing->graph, sizeof(*state));
		if (!state) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->definitions = state;
		if (pg_index_init(&state->names) != 0) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		if (syntax->item_count > SIZE_MAX / sizeof(*state->entries)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		state->entries = pg_alloc(synthesis->typing->graph, syntax->item_count * sizeof(*state->entries));
		state->scope = intern_scope(synthesis, (struct pg_source_scope){.parent = job->scope,
			.context_job = job->scope->context_job, .definitions = state});
		if (!state->scope || !state->entries) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		state->count = syntax->item_count;
		state->syntax = syntax;
	}
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
				state->entries[i] = name->imported;
			} else {
				if (name->producer) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
				name->producer = request_role(synthesis, state->scope, item->expression, DEFINITION_JOB);
				state->entries[i] = name->producer;
			}
			if (!state->entries[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		}
		enqueue(synthesis, job);
		return;
	}
	if (state->activated < state->count) {
		size_t i = state->activated++;
		if (state->entries[i]) {
			struct pg_synthesis_job *producer = state->entries[i];
			if (producer->role == DEFINITION_JOB && !producer->stage) {
				producer->stage = 1;
				enqueue(synthesis, producer);
			}
		} else {
			const struct pg_syntax_item *item = &state->syntax->items[i];
			if (!lookup_definition(state, item->name)) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			struct pg_syntax *name = pg_alloc(synthesis->typing->graph, sizeof(*name));
			struct pg_syntax *check = pg_alloc(synthesis->typing->graph, sizeof(*check));
			if (!name || !check) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			*name = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = item->name};
			*check = (struct pg_syntax){.kind = PG_SYNTAX_EXPECT, .left = name, .right = item->expression};
			state->entries[i] = pg_synthesis_request(synthesis, state->scope, check);
			if (!state->entries[i]) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
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
		job->right = request_role(synthesis, job->scope, syntax, DEFINITION_SCOPE_JOB);
		job->left = job->right;
		depend(synthesis, job, job->left);
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

static void data_schema_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		job->left = pg_synthesis_telescope(synthesis, job->scope, job->syntax->left);
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
		for (size_t i = 0; i < state->checked; ++i) results[i] = state->producers[i]->result;
		job->schema = pg_data_schema(synthesis->typing, signature,
			state->checked, results);
		pg_graph_destroy(&temporary);
		finish(synthesis, job, job->schema ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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
	const struct pg_evidence *formation = job->inputs[0], *parameters = job->inputs[2];
	const struct pg_object *constructor = job->inputs[1];
	const struct pg_evidence *function = pg_prove_constructor_function(synthesis->typing,
		synthesis->classifiers, formation, constructor, parameters);
	if (!function) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	/* Nullary constructors are values; field-bearing ones are raw functions. */
	job->result = pg_evidence_rule(function) == PG_RETURN_INTRO
		? pg_prove_return_value(synthesis->typing, function) : function;
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

/* Each universe candidate owns a distinct conditional Self context. Raising
 * the bound never mutates an accepted assumption or publishes a provisional
 * family. Failure to construct a candidate is not universe inconsistency. */
static void declaration_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->syntax->left->kind != PG_SYNTAX_CONSTRUCTORS) {
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (!job->domain)
		job->domain = pg_prove_universe(synthesis->typing, synthesis->classifiers, source_context(job->scope), 0);
	if (!job->left) {
		job->binder = pg_binder(synthesis->typing->graph);
		const struct pg_evidence *self = pg_prove_context_extension(synthesis->typing,
			source_context(job->scope), job->binder, job->domain);
		job->inner = pg_synthesis_bind(synthesis, job->scope, (struct pg_token){.kind = '*'}, job->binder, self);
		job->left = pg_synthesis_data_schema(synthesis, job->inner, job->syntax);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) {
		enum pg_synthesis_status status = job->left->status;
		if (status == PG_SYNTHESIS_REJECTED) status = PG_SYNTHESIS_UNSUPPORTED;
		finish(synthesis, job, status); return;
	}
	const struct pg_data_schema *schema = pg_synthesis_schema_result(job->left);
	uint64_t candidate, bound;
	if (!job->domain || !pg_universe_level(pg_evidence_subject(job->domain)->core, &candidate) ||
		pg_data_schema_field_level(schema, &bound)) {
		finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
	}
	if (bound > candidate) {
		job->domain = pg_prove_universe(synthesis->typing, synthesis->classifiers, source_context(job->scope), bound);
		if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		job->left = NULL;
		enqueue(synthesis, job);
		return;
	}
	job->result = pg_prove_inductive_type(synthesis->typing, synthesis->classifiers, schema);
	if (!job->result) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
	job->schema = schema;
	/* Export only declared members, never fall back to enclosing lexical names. */
	job->exports = intern_scope(synthesis, (struct pg_source_scope){.context_job = job->scope->context_job});
	const struct pg_syntax *constructors = job->syntax->left;
	for (size_t i = 0; job->exports && i < constructors->item_count; ++i) {
		const struct pg_evidence *parameters = pg_prove_substitution_projection(synthesis->typing,
			source_context(job->scope), source_context(job->scope));
		const void *inputs[] = {job->result, pg_data_constructor(pg_data_schema_layout(schema), i), parameters};
		struct pg_synthesis_job *member = request_inputs(synthesis, CONSTRUCTOR_VALUE_JOB, 3, inputs);
		job->exports = pg_synthesis_name_job(synthesis, job->exports, constructors->items[i].name, member);
	}
	struct pg_synthesis_job *accepted = pg_synthesis_evidence(synthesis, job->result);
	if (!accepted) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	accepted->exports = job->exports;
	finish(synthesis, job, job->exports ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void induction_branch_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->left) {
		const struct pg_evidence *formation = job->inputs[1], *parameters = job->inputs[3];
		const struct pg_evidence *map = pg_prove_induction_scope(synthesis->typing, synthesis->classifiers,
			formation, job->inputs[2], parameters, job->inputs[4], job->inputs[5]);
		if (!map) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		size_t fields = pg_evidence_premise_count(map) - pg_evidence_premise_count(parameters) - 1, total;
		if (fields != job->syntax->item_count) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		const struct pg_evidence *context = pg_evidence_premise(map, 1);
		if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(parameters), &total)) goto error;
		if (total > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		struct pg_graph temporary = {0};
		const struct pg_evidence **extensions = pg_alloc(&temporary, total * sizeof(*extensions));
		unsigned char *recursive = pg_alloc(&temporary, fields);
		if ((total && !extensions) || (fields && !recursive)) { pg_graph_destroy(&temporary); goto error; }
		for (size_t i = total; i; --i, context = pg_evidence_premise(context, 0)) extensions[i - 1] = context;
		const struct pg_source_scope *scope = job->scope;
		for (size_t i = 0; scope && i < fields; ++i) {
			if (job->syntax->items[i].operation) { pg_graph_destroy(&temporary); finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
			scope = pg_synthesis_bind(synthesis, scope, job->syntax->items[i].name,
				pg_evidence_context(extensions[i])->binder, extensions[i]);
		}
		/* Match IHs to original field binders, not their surface spelling. */
		const struct pg_object *self = pg_evidence_context(pg_evidence_premise(formation, 0))->binder;
		const struct pg_context *source_fields = pg_evidence_context(pg_evidence_premise(map, 0));
		size_t next = fields;
		for (size_t i = fields; i; --i, source_fields = source_fields->parent)
			recursive[i - 1] = pg_data_direct_recursion(source_fields->declared_type, self) == 1;
		for (size_t i = 0; scope && i < fields; ++i) {
			if (!recursive[i]) continue;
			if (next == total) { scope = NULL; break; }
			scope = intern_scope(synthesis, (struct pg_source_scope){.parent = scope,
				.context_job = pg_synthesis_evidence(synthesis, extensions[next]),
				.binder = pg_evidence_context(extensions[next])->binder,
				.hypothesis_for = pg_evidence_context(extensions[i])->binder});
			++next;
		}
		if (next != total) scope = NULL;
		pg_graph_destroy(&temporary);
		if (!scope) goto error;
		job->inner = scope;
		job->left = pg_synthesis_request(synthesis, scope, job->syntax->right);
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	job->result = pg_prove_abstract(synthesis->typing, synthesis->classifiers,
		source_context(job->scope), source_context(job->inner), computation(synthesis, job->left->result));
	finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
	return;
error:
	finish(synthesis, job, PG_SYNTHESIS_ERROR);
}

static void constant_motive_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_evidence *destination = job->inputs[0];
	if (!job->checking_type) {
		struct pg_synthesis_job *body = (void *)job->inputs[2];
		if (body->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, body); return; }
		if (body->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, body->status); return; }
		const struct pg_evidence *fields = job->inputs[1];
		const struct pg_evidence *term = computation(synthesis, body->result);
		if (!term || pg_evidence_context(term) != pg_evidence_context(fields)) goto unsupported;
		job->function = pg_prove_abstract(synthesis->typing, synthesis->classifiers, destination, fields, term);
		if (!job->function) goto unsupported;
		job->checking_type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, destination, job->function);
		if (!job->checking_type) goto unsupported;
		job->domain = fields;
	}
	if (pg_evidence_context(job->domain) == pg_evidence_context(destination)) {
		job->result = job->checking_type;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	job->checking_type = pg_prove_pi_constant_codomain(synthesis->typing, job->checking_type);
	if (!job->checking_type) goto unsupported;
	job->domain = pg_evidence_premise(job->domain, 0);
	enqueue(synthesis, job);
	return;
unsupported:
	finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED);
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
		job->inner = job->scope;
		job->checking_term = job->left->result;
		if (job->checking_term && pg_evidence_judgement(job->checking_term) == PG_JUDGEMENT_COMPUTATION) {
			const struct continuation_frame *frame = open_continuation(synthesis, source_context(job->scope), job->checking_term);
			if (!frame) goto unsupported;
			job->application_frame = frame;
			const struct pg_object *binder = pg_evidence_context(frame->context)->binder;
			job->inner = pg_synthesis_bind(synthesis, job->scope, (struct pg_token){0}, binder, frame->context);
			if (!job->inner) goto error;
			job->checking_term = pg_prove_variable(synthesis->typing, frame->context, binder);
		}
	}
	const struct pg_evidence *context = source_context(job->inner), *scrutinee = job->checking_term;
	if (!scrutinee || pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE) goto unsupported;
	if (!job->match) {
		const struct pg_evidence *type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, scrutinee);
		struct pg_inductive_instance instance;
		if (!pg_inductive_instance(synthesis->typing, type, &instance)) goto unsupported;
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
	}
	struct match_state *state = job->match;
	const struct pg_data_layout *layout = pg_data_schema_layout(state->instance.schema);
	if (state->next < state->count) {
		const struct pg_syntax *clause = job->syntax->items[state->next].expression;
		struct source_reference label;
		if (clause->left->kind == PG_SYNTAX_ATOM)
			label = lookup_scope(state->labels, clause->left->token);
		else {
			struct pg_synthesis_job *dependency = NULL;
			enum pg_synthesis_status status = resolve_reference(synthesis, job->inner, clause->left, &label, &dependency);
			if (status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, dependency); return; }
			if (status != PG_SYNTHESIS_DONE) { finish(synthesis, job, status); return; }
		}
		if (!label.producer) goto rejected;
		if (label.producer->role != CONSTRUCTOR_VALUE_JOB) goto unsupported;
		const struct pg_object *constructor = label.producer->inputs[1];
		size_t ordinal;
		if (!pg_data_constructor_position(layout, constructor, &ordinal)) goto rejected;
		if (state->branches[ordinal].clause) goto rejected;
		const struct pg_evidence *map = pg_prove_constructor_scope(synthesis->typing,
			state->instance.formation, constructor, state->instance.parameters);
		if (!map) goto error;
		const struct pg_evidence *fields = pg_evidence_premise(map, 1);
		size_t count;
		if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(context), &count)) goto error;
		if (count != clause->item_count) goto rejected;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto error;
		const struct pg_evidence **extensions = malloc(count * sizeof(*extensions));
		if (count && !extensions) goto error;
		for (size_t i = count; i; --i, fields = pg_evidence_premise(fields, 0)) extensions[i - 1] = fields;
		const struct pg_source_scope *scope = job->inner;
		for (size_t i = 0; i < count; ++i) {
			if (clause->items[i].operation) { free(extensions); goto unsupported; }
			scope = pg_synthesis_bind(synthesis, scope, clause->items[i].name,
				pg_evidence_context(extensions[i])->binder, extensions[i]);
			if (!scope) break;
		}
		free(extensions);
		if (!scope) goto error;
		struct match_branch *branch = &state->branches[ordinal];
		branch->scope = scope;
		branch->clause = clause;
		branch->needs_ih = branch_needs_ih(scope, pg_evidence_context(context), clause->right);
		if (branch->needs_ih < 0) goto error;
		if (branch->needs_ih) state->induction = 1;
		else {
			branch->body = pg_synthesis_request(synthesis, scope, clause->right);
			if (!branch->body) goto error;
		}
		++state->next;
		enqueue(synthesis, job);
		return;
	}
	if (state->checked < state->count) {
		struct match_branch *branch = &state->branches[state->checked];
		if (branch->needs_ih) {
			++state->checked;
			enqueue(synthesis, job);
			return;
		}
		struct pg_synthesis_job *candidate = pg_synthesis_constant_motive(synthesis, context,
			source_context(branch->scope), branch->body);
		if (!candidate) goto error;
		if (candidate->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, candidate); return; }
		if (candidate->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, candidate->status); return; }
		branch->function = candidate->function;
		const struct pg_evidence *type = candidate->result;
		if (!state->motive) state->motive = type;
		else if (pg_alpha_equal(pg_evidence_subject(state->motive)->core, pg_evidence_subject(type)->core) != 1)
			goto unsupported;
		++state->checked;
		enqueue(synthesis, job);
		return;
	}
	if (!state->motive) goto unsupported;
	if (!state->motive_context) {
		const struct pg_evidence *family = pg_prove_reindex(synthesis->typing, state->instance.parameters, state->instance.formation);
		state->motive_context = pg_prove_context_extension(synthesis->typing, context,
			pg_binder(synthesis->typing->graph), family);
		state->motive = pg_prove_projection(synthesis->typing, state->motive_context, state->motive);
		if (!state->motive) goto error;
	}
	if (state->induction && state->prepared < state->count) {
		struct match_branch *branch = &state->branches[state->prepared];
		const struct pg_object *constructor = pg_data_constructor(layout, state->prepared);
		if (branch->needs_ih) {
			if (!branch->body) branch->body = pg_synthesis_induction_branch(synthesis, job->inner,
				state->instance.formation, constructor, state->instance.parameters,
				state->motive_context, state->motive, branch->clause);
			if (!branch->body) goto error;
			if (branch->body->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, branch->body); return; }
			if (branch->body->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, branch->body->status); return; }
			branch->function = branch->body->result;
		} else branch->function = pg_prove_induction_case(synthesis->typing, synthesis->classifiers,
			state->instance.formation, constructor, state->instance.parameters,
			state->motive_context, state->motive, branch->function);
		if (!branch->function) goto unsupported;
		++state->prepared;
		enqueue(synthesis, job);
		return;
	}
	const struct pg_evidence **branches = malloc(state->count * sizeof(*branches));
	if (!branches) goto error;
	for (size_t i = 0; i < state->count; ++i) branches[i] = state->branches[i].function;
	job->result = state->induction
		? pg_prove_induction(synthesis->typing, synthesis->classifiers, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches)
		: pg_prove_match(synthesis->typing, synthesis->classifiers, state->instance.formation,
			state->instance.parameters, scrutinee, state->motive_context, state->motive, state->count, branches);
	free(branches);
	if (!job->result) goto unsupported;
complete:
	if (job->application_frame) {
		const struct pg_evidence *result = close_continuation(synthesis, job, job->application_frame, job->result);
		if (!result) return;
		job->result = result;
		job->application_frame = NULL;
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
			job->substitution = substitution_start(synthesis, job->inputs[0], job->inputs[1], count);
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
	image = value(synthesis, image);
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
		struct pg_synthesis_job *p = pg_synthesis_derivation(synthesis, input->premises[state->next]);
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

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->scope && job->role != TELESCOPE_JOB && job->role != TELESCOPE_STRUCTURE_JOB) {
		struct pg_synthesis_job *context = job->scope->context_job;
		if (context->status == PG_SYNTHESIS_PENDING) { depend(synthesis, job, context); return; }
		if (context->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, context->status); return; }
		if (!source_context(job->scope)) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	const struct pg_syntax *syntax = job->syntax;
	if (job->role == DERIVATION_JOB) { derivation_step(synthesis, job); return; }
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
	if (job->role == APPLICATION_JOB) { raw_application_step(synthesis, job); return; }
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
		const struct pg_evidence *type = pg_prove_classifier(synthesis->typing, synthesis->classifiers, context, input);
		if (!type) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (job->role == FAMILY_ACTION_JOB) {
			if (!family_paths(synthesis, job)) return;
			job->result = pg_prove_family_action(synthesis->typing, type, input,
				job->inputs[0], job->inputs[1], job->family->count, job->family->paths);
		} else job->result = pg_prove_reflexivity(synthesis->typing, type, input);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_UNSUPPORTED);
		return;
	}
	if (job->role == NORMALIZATION_JOB || job->role == NF_JOB) {
		const struct pg_term *input = pg_evidence_subject(job->inputs[1])->core;
		const struct pg_reduction_certificate *certificate = normalization_receipt(synthesis, job, input,
			job->role == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
		if (!certificate) return;
		job->result = pg_prove_normalization(synthesis->typing, job->inputs[1], certificate);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (job->role == RETURN_JOB || job->role == THUNK_JOB) { contents_step(synthesis, job); return; }
	if (job->role == BINDING_JOB) { binding_step(synthesis, job); return; }
	if (job->role == TELESCOPE_JOB) { telescope_step(synthesis, job); return; }
	if (job->role == TELESCOPE_STRUCTURE_JOB) { telescope_structure_step(synthesis, job); return; }
	if (job->role == DATA_RESULT_JOB || job->role == SUBSTITUTION_JOB) { substitution_step(synthesis, job); return; }
	if (job->role == DATA_SCHEMA_JOB) { data_schema_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_JOB) { constructor_step(synthesis, job); return; }
	if (job->role == DEFINITION_JOB) { definition_step(synthesis, job); return; }
	if (job->role == DEFINITION_SCOPE_JOB) { definition_scope_step(synthesis, job); return; }
	if (hypothesis_reference(synthesis, job)) return;
	if (syntax->kind == PG_SYNTAX_DECLARATION) { declaration_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_ELIMINATION) { match_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_DEFINITIONS) { definitions_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_BLOCK) { block_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_QUALIFIED && syntax->left->kind == PG_SYNTAX_BLOCK) { block_step(synthesis, job); return; }
	if (syntax->kind == PG_SYNTAX_ATOM || syntax->kind == PG_SYNTAX_QUALIFIED || syntax->kind == PG_SYNTAX_IMPORT) {
		reference_step(synthesis, job); return;
	}
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA: case PG_SYNTAX_PI: case PG_SYNTAX_APPLICATION:
	case PG_SYNTAX_QUOTE: case PG_SYNTAX_EXPECT:
		break;
	default:
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if ((syntax->kind == PG_SYNTAX_LAMBDA || syntax->kind == PG_SYNTAX_PI) && job->stage < 2) {
		if (!job->left) {
			job->left = pg_synthesis_binding(synthesis, job->scope, syntax);
			depend(synthesis, job, job->left);
			return;
		}
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		job->domain = job->left->domain;
		job->inner = job->left->inner;
		job->right = pg_synthesis_request(synthesis, job->inner, syntax->right);
		job->stage = 2;
		depend(synthesis, job, job->right);
		return;
	}
	if (!job->stage) {
		job->left = pg_synthesis_request(synthesis, job->scope, syntax->left);
		job->stage = 1;
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (syntax->kind == PG_SYNTAX_QUOTE) {
		job->result = pg_prove_thunk(synthesis->typing, synthesis->classifiers, job->left->result);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
		return;
	}
	if (job->stage == 1) {
		job->right = pg_synthesis_request(synthesis, job->scope, syntax->right);
		job->stage = 2;
		depend(synthesis, job, job->right);
		return;
	}
	if (job->right->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->right->status); return; }
	const struct pg_evidence *left = job->left->result, *right = job->right->result;
	switch (syntax->kind) {
	case PG_SYNTAX_LAMBDA: {
		const struct pg_evidence *body = computation(synthesis, right);
		if (!body) break;
		const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, source_context(job->inner), body);
		if (!codomain) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, source_context(job->inner), codomain);
		job->result = pg_prove_lambda(synthesis->typing, pi, body);
		break;
	}
	case PG_SYNTAX_PI: {
		const struct pg_evidence *codomain = type_input(synthesis, job, source_context(job->inner), right);
		if (!codomain) return;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, source_context(job->inner), codomain);
		break;
	}
	case PG_SYNTAX_APPLICATION:
		application_step(synthesis, job);
		return;
	case PG_SYNTAX_EXPECT: {
		if (!job->checking_term) {
			right = type_input(synthesis, job, source_context(job->scope), right);
			if (!right) return;
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE_TYPE) left = value(synthesis, left);
			if (!left) break;
			if (pg_evidence_judgement(left) == PG_JUDGEMENT_VALUE) right = value_type(synthesis, right);
			else if (pg_evidence_judgement(right) != PG_JUDGEMENT_COMPUTATION_TYPE)
				right = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, right));
			if (!right) break;
			job->checking_term = left;
			job->checking_type = right;
		}
		if (!job->value_job) {
			job->value_job = pg_synthesis_expect(synthesis,
				pg_synthesis_evidence(synthesis, job->checking_term),
				pg_synthesis_evidence(synthesis, job->checking_type));
			depend(synthesis, job, job->value_job);
			return;
		}
		if (job->value_job->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->value_job->status); return; }
		job->result = job->value_job->result;
		job->exports = job->left->exports;
		break;
	}
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
