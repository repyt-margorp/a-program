#include "synthesis.h"
#include "computation.h"
#include "iadt.h"
#include "action.h"

#include <stdlib.h>
#include <string.h>

struct pg_source_scope {
	struct pg_index_entry index;
	const struct pg_synthesis *owner;
	const struct pg_source_scope *parent;
	struct pg_token name;
	const struct pg_object *binder;
	const struct pg_evidence *context;
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
struct family_state {
	const struct pg_evidence *maps[2];
	const struct pg_evidence **declarations;
	const struct pg_evidence **paths;
	size_t count, common, next;
};
enum job_role { EXPRESSION_JOB, DEFINITION_JOB, DEFINITION_SCOPE_JOB, EVIDENCE_JOB, RETURN_JOB, THUNK_JOB, NORMALIZATION_JOB, NF_JOB,
	REFLEXIVITY_JOB, CLASSIFIER_JOB, FAMILY_ACTION_JOB, FORMATION_JOB, FACE_JOB, EXPECT_JOB, APPLICATION_JOB, INSTANCE_JOB, CONVERSION_JOB, DATA_CASE_JOB, REINDEX_JOB, PAIR_JOB, SUBSTITUTION_JOB, BINDING_JOB, TELESCOPE_JOB, DATA_RESULT_JOB, DATA_SCHEMA_JOB, CONSTRUCTOR_JOB };
struct pg_synthesis_job {
	struct pg_index_entry index;
	const struct pg_synthesis *owner;
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
	struct family_state *family;
	const void *inputs[];
};

static void enqueue(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	job->next = NULL;
	if (synthesis->ready_tail) synthesis->ready_tail->next = job;
	else synthesis->ready = job;
	synthesis->ready_tail = job;
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
	if (pg_index_init(&synthesis->scopes) == 0) return 0;
	pg_index_destroy(&synthesis->jobs);
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
	if (!input.context) return NULL;
	/* The intrinsic root is a punctuation token with no borrowed spelling. */
	if (input.name.kind == '#') input.name = (struct pg_token){.kind = '#'};
	if (input.name.length && !input.name.text) return NULL;
	uint64_t hash = name_hash(input.name) ^ (unsigned)input.name.kind;
	const void *pointers[] = {input.parent, input.context, input.binder, input.definitions, input.producer, input.exports, input.module, input.imports};
	for (size_t i = 0; i < sizeof(pointers) / sizeof(*pointers); ++i)
		hash = (hash ^ (uintptr_t)pointers[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&synthesis->scopes, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		const struct pg_source_scope *scope = (const struct pg_source_scope *)entry;
		if (scope->parent != input.parent) continue;
		if (scope->context != input.context) continue;
		if (scope->binder != input.binder) continue;
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
	scope->owner = synthesis;
	return pg_index_insert(&synthesis->scopes, &scope->index, hash) == 0 ? scope : NULL;
}

const struct pg_source_scope *pg_synthesis_root(struct pg_synthesis *synthesis)
{
	return intern_scope(synthesis, (struct pg_source_scope){.context = pg_prove_empty_context(synthesis->typing)});
}

const struct pg_source_scope *pg_synthesis_bind(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_object *binder, const struct pg_evidence *extended_context)
{
	if (!parent || parent->owner != synthesis || !extended_context) return NULL;
	if (pg_evidence_judgement(extended_context) != PG_JUDGEMENT_CONTEXT) return NULL;
	const struct pg_context *context = pg_evidence_context(extended_context);
	if (!context || context->parent != pg_evidence_context(parent->context)) return NULL;
	if (context->binder != binder) return NULL;
	/* Check ownership through a primitive judgement, not just a context pointer. */
	if (!pg_prove_variable(synthesis->typing, extended_context, binder)) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent, .name = name,
		.binder = binder, .context = extended_context});
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
	job->owner = synthesis;
	job->role = role;
	job->input_count = count;
	for (size_t i = 0; i < count; ++i) job->inputs[i] = inputs[i];
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
	if (!scope || scope->owner != synthesis || !syntax) return NULL;
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

const struct pg_source_scope *pg_synthesis_telescope_scope(const struct pg_synthesis_job *job)
{
	if (!job || job->role != TELESCOPE_JOB || job->status != PG_SYNTHESIS_DONE) return NULL;
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

const struct pg_source_scope *pg_synthesis_name(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_evidence *proof)
{
	if (!parent || parent->owner != synthesis) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!pg_evidence_owned_by(proof, synthesis->typing) || !pg_evidence_subject(proof)) return NULL;
	if (!pg_prove_projection(synthesis->typing, parent->context, proof)) return NULL;
	return pg_synthesis_name_job(synthesis, parent, name, pg_synthesis_evidence(synthesis, proof));
}

const struct pg_source_scope *pg_synthesis_name_job(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *producer)
{
	if (!parent || parent->owner != synthesis) return NULL;
	if (name.kind != PG_TOKEN_IDENT || !name.text || !name.length) return NULL;
	if (!producer || producer->owner != synthesis) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.name = name, .context = parent->context, .producer = producer});
}

const struct pg_source_scope *pg_synthesis_import_scope(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, const struct pg_source_scope *bindings)
{
	if (!parent || parent->owner != synthesis) return NULL;
	if (!bindings || bindings->owner != synthesis) return NULL;
	if (pg_evidence_context(bindings->context)) return NULL;
	return intern_scope(synthesis, (struct pg_source_scope){.parent = parent,
		.context = parent->context, .imports = bindings});
}

static const struct pg_source_scope *publish_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports, struct pg_synthesis_job *module)
{
	if (!parent || parent->owner != synthesis) return NULL;
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
		.name = name, .context = parent->context, .exports = exports, .module = module});
}

const struct pg_source_scope *pg_synthesis_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	const struct pg_source_scope *exports)
{
	if (!exports || exports->owner != synthesis) return NULL;
	if (pg_evidence_context(exports->context)) return NULL;
	return publish_namespace(synthesis, parent, name, exports, NULL);
}

const struct pg_source_scope *pg_synthesis_module_namespace(struct pg_synthesis *synthesis,
	const struct pg_source_scope *parent, struct pg_token name,
	struct pg_synthesis_job *module)
{
	if (!module || module->owner != synthesis || module->role != EXPRESSION_JOB) return NULL;
	if (pg_evidence_context(module->scope->context)) return NULL;
	const struct pg_syntax *syntax = module->syntax;
	if (syntax->kind == PG_SYNTAX_QUALIFIED) syntax = syntax->left;
	if (syntax->kind != PG_SYNTAX_DEFINITIONS) return NULL;
	return publish_namespace(synthesis, parent, name, NULL, module);
}

struct pg_synthesis_job *pg_synthesis_reflexivity(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *input)
{
	if (!input || input->owner != synthesis) return NULL;
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
	if (!input || input->owner != synthesis) return NULL;
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
		if (!paths[i] || paths[i]->owner != synthesis) goto done;
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
	if (!substitution || substitution->owner != synthesis) return NULL;
	if (!proof || proof->owner != synthesis) return NULL;
	const void *inputs[] = {substitution, proof};
	return request_inputs(synthesis, REINDEX_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_expect(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term, struct pg_synthesis_job *type)
{
	if (!term || term->owner != synthesis) return NULL;
	if (!type || type->owner != synthesis) return NULL;
	const void *inputs[] = {term, type};
	return request_inputs(synthesis, EXPECT_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_application(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *function,
	struct pg_synthesis_job *argument)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!function || function->owner != synthesis) return NULL;
	if (!argument || argument->owner != synthesis) return NULL;
	const void *inputs[] = {context, function, argument};
	return request_inputs(synthesis, APPLICATION_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_identity_instance(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, struct pg_synthesis_job *family,
	struct pg_synthesis_job *left, struct pg_synthesis_job *right)
{
	if (!pg_evidence_owned_by(context, synthesis->typing)) return NULL;
	if (pg_evidence_judgement(context) != PG_JUDGEMENT_CONTEXT) return NULL;
	if (!family || family->owner != synthesis) return NULL;
	if (!left || left->owner != synthesis) return NULL;
	if (!right || right->owner != synthesis) return NULL;
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
	if (!fields || fields->owner != synthesis || !result) return NULL;
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
	if (!body || body->owner != synthesis) return NULL;
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
		if (!images[i] || images[i]->owner != synthesis) goto done;
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
	if (!producer || producer->owner != synthesis) return NULL;
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
	if (!formation || formation->owner != synthesis) return NULL;
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
	const struct pg_evidence *input = type_input(synthesis, job, job->scope->context, job->left->result);
	if (!input) return;
	job->domain = value_type(synthesis, input);
	if (!job->domain) { finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	struct pg_token name = syntax->token;
	if (syntax->kind == PG_SYNTAX_PI) {
		name = syntax->left->token;
		if (syntax->left->kind != PG_SYNTAX_BINDER) name = (struct pg_token){0};
	}
	const struct pg_object *binder = pg_binder(synthesis->typing->graph);
	job->result = pg_prove_context_extension(synthesis->typing, job->scope->context, binder, job->domain);
	job->inner = pg_synthesis_bind(synthesis, job->scope, name, binder, job->result);
	finish(synthesis, job, job->inner ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void telescope_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!job->inner) { job->inner = job->scope; job->tail = job->syntax; }
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		job->inner = job->left->inner;
		job->tail = job->tail->right;
		job->left = NULL;
	}
	if (job->tail->kind != job->syntax->kind ||
		(job->tail->kind != PG_SYNTAX_LAMBDA && job->tail->kind != PG_SYNTAX_PI)) {
		job->result = job->inner->context;
		finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	job->left = request_role(synthesis, job->inner, job->tail, BINDING_JOB);
	depend(synthesis, job, job->left);
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
		if (token.kind != '#') {
			if (scope->name.length != token.length) continue;
			if (memcmp(scope->name.text, token.text, token.length) != 0) continue;
		}
		return (struct source_reference){scope->binder, scope->producer, scope->exports, scope->module};
	}
	return (struct source_reference){0};
}

static enum pg_synthesis_status resolve_member(struct pg_synthesis *synthesis,
	struct source_reference *reference, struct pg_token token, struct pg_synthesis_job **dependency)
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
	if (root->kind != PG_SYNTAX_ATOM) return PG_SYNTHESIS_UNSUPPORTED;
	if (root->token.kind != PG_TOKEN_IDENT && root->token.kind != '#') return PG_SYNTHESIS_UNSUPPORTED;
	*reference = lookup_scope(scope, root->token);
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

static void reference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (job->left) {
		if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
		const struct pg_evidence *proof = job->left->result;
		if (!proof || !pg_evidence_subject(proof)) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		if (job->syntax->kind == PG_SYNTAX_IMPORT && pg_evidence_context(proof)) {
			finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		job->result = pg_prove_projection(synthesis->typing, job->scope->context, proof);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	struct pg_token token = job->syntax->token;
	if (job->syntax->kind == PG_SYNTAX_ATOM && token.kind == '@') {
		job->result = pg_prove_universe(synthesis->typing, synthesis->classifiers, job->scope->context, 0);
	} else if (job->syntax->kind == PG_SYNTAX_ATOM && token.kind == '*') {
		/* No recursive Self/IH binding has been admitted by this fragment. */
		finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
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
			job->result = pg_prove_variable(synthesis->typing, job->scope->context, reference.binder);
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
		job->checking_type = type;
	}
	job->result = compare(synthesis, job);
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
			result = pg_prove_fold(synthesis->typing, frame->input, job->continuation);
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
	const struct pg_evidence *context = job->application_frame ? job->application_frame->context : job->scope->context;
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
			for (size_t i = 0; i < block->syntax->item_count; ++i) {
				struct pg_token name = block->syntax->items[i].name;
				if (name.length && same_name(name, job->syntax->right->token)) { block->end = i + 1; break; }
			}
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
		input = classifier_input(synthesis, job, block->scope->context, input);
		if (!input) return;
		if (block->next == block->end) {
			block->tail = input;
		} else {
			struct continuation_frame *frame = open_continuation(synthesis, block->scope->context, input);
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
			.context = job->scope->context, .definitions = state});
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
	if (!job->right) {
		job->right = pg_synthesis_data_result(synthesis, pg_synthesis_telescope_scope(job->left),
			job->scope->context, job->inputs[1], pg_synthesis_telescope_body(job->left));
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
		depend(synthesis, job, job->left);
		return;
	}
	if (job->left->status != PG_SYNTHESIS_DONE) { finish(synthesis, job, job->left->status); return; }
	if (!job->declaration) {
		const struct pg_syntax *constructors = pg_synthesis_telescope_body(job->left);
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
		const void *inputs[] = {job->scope, job->left->result, item->expression};
		struct pg_synthesis_job *producer = request_inputs(synthesis, CONSTRUCTOR_JOB, 3, inputs);
		if (!producer) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		producer->scope = job->scope;
		producer->syntax = item->expression;
		state->producers[i] = producer;
		enqueue(synthesis, job);
		return;
	}
	if (state->checked == state->constructors->item_count) {
		struct pg_graph temporary = {0};
		const struct pg_evidence **results = NULL;
		if (state->checked <= SIZE_MAX / sizeof(*results))
			results = pg_alloc(&temporary, state->checked * sizeof(*results));
		if (state->checked && !results) { finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		for (size_t i = 0; i < state->checked; ++i) results[i] = state->producers[i]->result;
		job->schema = pg_data_schema(synthesis->typing, job->scope->context, job->left->result,
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
	if (pg_context_extension_size(pg_evidence_context(job->scope->context), prefix, &field_count) != 0) return PG_SYNTHESIS_REJECTED;
	if (pg_context_extension_size(prefix, NULL, &parameter_count) != 0) return PG_SYNTHESIS_ERROR;
	const struct pg_syntax *head = job->syntax;
	size_t arguments = 0;
	while (head->kind == PG_SYNTAX_APPLICATION) { ++arguments; head = head->left; }
	if (head->kind != PG_SYNTAX_ATOM || head->token.kind != '*' || arguments != index_count) return PG_SYNTHESIS_REJECTED;
	if (index_count > SIZE_MAX - parameter_count) return PG_SYNTHESIS_ERROR;
	size_t count = parameter_count + index_count;
	struct substitution_state *state = substitution_start(synthesis, indices, job->scope->context, count);
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
				job->scope->context, pg_evidence_context(entry->extension)->binder);
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

static void step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	const struct pg_syntax *syntax = job->syntax;
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
		const struct pg_reduction_certificate *certificate;
		if (job->role == NF_JOB) {
			if (!job->normalizing.nf) job->normalizing.nf = pg_nf_request(synthesis->normalization, &pg_pure_policy, input);
			if (!job->normalizing.nf || pg_nf_advance(job->normalizing.nf, 1) == PG_NF_ERROR) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
			certificate = pg_nf_certificate(job->normalizing.nf);
		} else {
			if (!job->normalizing.whnf) job->normalizing.whnf = pg_whnf_request(synthesis->normalization, &pg_pure_policy, input);
			if (!job->normalizing.whnf || pg_whnf_advance(job->normalizing.whnf, 1) == PG_EVAL_ERROR) {
				finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
			}
			certificate = pg_whnf_certificate(job->normalizing.whnf);
		}
		if (!certificate) { enqueue(synthesis, job); return; }
		job->result = pg_prove_normalization(synthesis->typing, job->inputs[1], certificate);
		finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		return;
	}
	if (job->role == RETURN_JOB || job->role == THUNK_JOB) { contents_step(synthesis, job); return; }
	if (job->role == BINDING_JOB) { binding_step(synthesis, job); return; }
	if (job->role == TELESCOPE_JOB) { telescope_step(synthesis, job); return; }
	if (job->role == DATA_RESULT_JOB || job->role == SUBSTITUTION_JOB) { substitution_step(synthesis, job); return; }
	if (job->role == DATA_SCHEMA_JOB) { data_schema_step(synthesis, job); return; }
	if (job->role == CONSTRUCTOR_JOB) { constructor_step(synthesis, job); return; }
	if (job->role == DEFINITION_JOB) { definition_step(synthesis, job); return; }
	if (job->role == DEFINITION_SCOPE_JOB) { definition_scope_step(synthesis, job); return; }
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
			job->left = request_role(synthesis, job->scope, syntax, BINDING_JOB);
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
		const struct pg_evidence *codomain = pg_prove_classifier(synthesis->typing, synthesis->classifiers, job->inner->context, body);
		if (!codomain) { finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		const struct pg_evidence *pi = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, job->inner->context, codomain);
		job->result = pg_prove_lambda(synthesis->typing, pi, body);
		break;
	}
	case PG_SYNTAX_PI: {
		const struct pg_evidence *codomain = type_input(synthesis, job, job->inner->context, right);
		if (!codomain) return;
		if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE)
			codomain = pg_prove_return_type(synthesis->typing, synthesis->classifiers, value_type(synthesis, codomain));
		job->result = pg_prove_pi(synthesis->typing, synthesis->classifiers, job->domain, job->inner->context, codomain);
		break;
	}
	case PG_SYNTAX_APPLICATION:
		application_step(synthesis, job);
		return;
	case PG_SYNTAX_EXPECT: {
		if (!job->checking_term) {
			right = type_input(synthesis, job, job->scope->context, right);
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
