#include "synthesis_source.h"
#include "synthesis_conversion.h"
#include "synthesis_effect.h"
#include "derivation.h"
#include "effect_inference.h"
#include "computation.h"
#include "dag.h"

#include <stdlib.h>
#include <string.h>

/* Only endpoint-bearing rules need comparison/reduction progress. Premises
 * remain immutable request inputs, not another retained proof array. */
struct derivation_computation {
	unsigned stage;
	struct pg_comparison endpoint;
	struct pg_synthesis_reduction normalizing;
	const struct pg_conversion_certificate *certificate;
	const struct pg_reduction_certificate *reduction;
};
struct derivation_work {
	size_t next;
	struct derivation_computation *computation;
};
struct derivation_input_work {
	size_t next;
	struct pg_synthesis_job *rule;
};

static void derivation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void derivation_input_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void derivation_destroy(struct pg_synthesis_job *);
static struct pg_synthesis_projection derivation_projection(const struct pg_synthesis_job *);
static struct pg_synthesis_projection input_projection(const struct pg_synthesis_job *);

static void evidence_ready(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	(void)synthesis;
	job->result = job->inputs[0];
	job->status = PG_SYNTHESIS_DONE;
}

static const struct pg_synthesis_work_class EVIDENCE_JOB[1] = {{
	.start = evidence_ready, .advance = evidence_ready}};
static const struct pg_synthesis_work_class DERIVATION_JOB[1] = {{
	.size = sizeof(struct derivation_work), .advance = derivation_step,
	.destroy = derivation_destroy, .project = derivation_projection}};
static const struct pg_synthesis_work_class DERIVATION_INPUT_JOB[1] = {{
	.size = sizeof(struct derivation_input_work), .advance = derivation_input_step,
	.project = input_projection}};

static void derivation_destroy(struct pg_synthesis_job *job)
{
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	if (local->computation) pg_comparison_destroy(&local->computation->endpoint);
}

static struct pg_synthesis_projection input_projection(const struct pg_synthesis_job *job)
{
	const struct derivation_input_work *local = pg_synthesis_work_state(job, DERIVATION_INPUT_JOB);
	return (struct pg_synthesis_projection){.rule = local->rule,
		.preparing = !local->rule, .value_kind = -1};
}

static struct pg_synthesis_projection derivation_projection(const struct pg_synthesis_job *job)
{
	const struct pg_derivation_input *input = job->inputs[0];
	struct pg_synthesis_projection result = {.value_kind = -1};
	switch (input->rule) {
	case PG_UNIVERSE_FORM: case PG_HOST_TYPE_FORM: result.value_kind = 2; break;
	case PG_VARIABLE: case PG_THUNK_INTRO: case PG_VALUE_FROM_TYPE: case PG_HOST_VALUE_INTRO:
		result.value_kind = 1; break;
	case PG_LAMBDA_INTRO: case PG_APP_ELIM: case PG_FORCE_ELIM: case PG_RETURN_INTRO:
	case PG_FOLD_ELIM: case PG_REQUEST_INTRO: case PG_HANDLER_ELIM: case PG_EFFECT_SUBSUMPTION:
	case PG_HOST_FUNCTION_INTRO: result.value_kind = 0; break;
	default: break;
	}
	return result;
}

const struct pg_evidence *pg_synthesis_evidence_input(const struct pg_synthesis_job *job)
{
	return job && job->role == EVIDENCE_JOB ? job->inputs[0] : NULL;
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

struct rule_input {
	struct pg_index_entry index;
	const struct pg_derivation_input *header;
};

static const struct pg_derivation_input *intern_rule_input(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input)
{
	uint64_t key[RULE_KEY_FIELDS], hash = UINT64_C(1469598103934665603);
	rule_key(input, key);
	for (size_t i = 0; i < RULE_KEY_FIELDS; ++i) hash = (hash ^ key[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&synthesis->rule_inputs, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct rule_input *candidate = (const void *)p;
		uint64_t fields[RULE_KEY_FIELDS];
		rule_key(candidate->header, fields);
		if (!memcmp(key, fields, sizeof(key))) return candidate->header;
	}
	struct rule_input *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry));
	struct pg_derivation_input *header = pg_alloc(synthesis->typing->graph, sizeof(*header));
	if (!entry || !header) return NULL;
	*header = *input;
	entry->header = header;
	return pg_index_insert(&synthesis->rule_inputs, &entry->index, hash) ? NULL : header;
}

struct pg_synthesis_job *pg_synthesis_evidence(struct pg_synthesis *synthesis,
	const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(proof, synthesis->typing)) return NULL;
	const void *inputs[] = {proof};
	return pg_synthesis_work_request(synthesis, EVIDENCE_JOB, 1, inputs);
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
	return pg_synthesis_work_request(synthesis, DERIVATION_INPUT_JOB, 2, inputs);
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
	input = intern_rule_input(synthesis, input);
	if (!input) return NULL;
	const void *inputs[] = {input, effects, equation};
	return pg_synthesis_work_request_inputs(synthesis, DERIVATION_JOB, 3, inputs, input->count, premises);
}

struct pg_synthesis_job *pg_synthesis_plain_rule(struct pg_synthesis *synthesis,
	enum pg_evidence_rule rule, const struct pg_object *binder, size_t count,
	struct pg_synthesis_job *const *premises)
{
	struct pg_derivation_input input = {.rule = rule, .parameters.binder = binder, .count = count};
	return pg_synthesis_rule(synthesis, &input, premises, NULL, NULL);
}

static int derivation_endpoint(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_term *actual, const struct pg_term *stored)
{
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	struct pg_comparison *work = &local->computation->endpoint;
	if (!actual || !stored) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 0; }
	if (!work->state && pg_comparison_init(work, actual, stored, NULL, NULL)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 0;
	}
	enum pg_comparison_status status = pg_comparison_advance(work, 1);
	if (status == PG_COMPARISON_PENDING) { pg_synthesis_enqueue(synthesis, job); return 0; }
	pg_comparison_destroy(work);
	if (status == PG_COMPARISON_EQUAL) return 1;
	pg_synthesis_finish(synthesis, job, status == PG_COMPARISON_DIFFERENT ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
	return 0;
}

const struct pg_derivation_input *pg_synthesis_plain_derivation(const struct pg_synthesis_job *job)
{
	return job && job->role == DERIVATION_JOB ? job->inputs[0] : NULL;
}

struct pg_synthesis_job *pg_synthesis_rule_premise(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index)
{
	if (!job || (job->role != DERIVATION_JOB && job->role != DERIVATION_INPUT_JOB)) return NULL;
	const struct pg_derivation_input *input = job->inputs[0];
	if (index >= input->count) return NULL;
	if (job->role == DERIVATION_INPUT_JOB)
		return pg_synthesis_derivation_inference(synthesis, input->premises[index], (void *)job->inputs[1]);
	return (void *)job->inputs[index + 3];
}

struct rule_export {
	const struct pg_synthesis *synthesis;
	struct pg_dag proofs, workers, raw;
	const struct pg_effect_inference *source_effects;
	struct pg_effect_inference *effects;
	int status;
};

static int export_proof_child(void *owner, const void *key, size_t index, const void **child)
{
	struct rule_export *export = owner;
	const struct pg_evidence *input = NULL;
	int status = pg_derivation_input_dependency(export->synthesis->typing, key, index, &input);
	*child = input;
	return status;
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
		return effects ? pg_dag_add(&export->workers, pg_synthesis_effect_worker(effects)) : 0;
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
	if (pg_dag_init(&export.proofs, export_proof_child, &export) || pg_dag_init(&export.workers, NULL, NULL)
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
			if (worker) header.effect_parameter = pg_effect_equation_parameter(pg_synthesis_effect_worker(worker), job->inputs[2]);
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
			const struct pg_evidence *dependency;
			if (pg_derivation_input_dependency(synthesis->typing, node->key, i, &dependency) != 1) goto done;
			const struct pg_dag_node *premise = pg_dag_find(&export.proofs, dependency);
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
			if (worker) input->effect_parameter = pg_effect_equation_parameter(pg_synthesis_effect_worker(worker), job->inputs[2]);
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
	struct derivation_input_work *local = pg_synthesis_work_state(job, DERIVATION_INPUT_JOB);
	const struct pg_derivation_input *input = job->inputs[0];
	struct pg_effect_inference *work = (void *)job->inputs[1];
	if (!local->rule) {
		if (input->count > SIZE_MAX / sizeof(struct pg_synthesis_job *)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
		if (local->next < input->count) {
			struct pg_synthesis_job *premise = pg_synthesis_rule_premise(synthesis, job, local->next);
			if (!premise) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			if (pg_synthesis_await_preparation(synthesis, job, premise, NULL)) return;
			if (!pg_synthesis_work_project(premise).rule) { pg_synthesis_finish(synthesis, job, premise->status); return; }
			++local->next;
			pg_synthesis_enqueue(synthesis, job);
			return;
		}
		struct pg_derivation_input header = *input;
		struct pg_effect_equation *equation = NULL;
		if (input->effect_parameter) {
			equation = pg_effect_equation_find(work, input->effect_parameter);
			if (!equation) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			header.effect_parameter = NULL;
		}
		struct pg_synthesis_job **premises = input->count ? malloc(input->count * sizeof(*premises)) : NULL;
		if (input->count && !premises) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		for (size_t i = 0; i < input->count; ++i)
			premises[i] = pg_synthesis_work_project(pg_synthesis_rule_premise(synthesis, job, i)).rule;
		local->rule = pg_synthesis_rule(synthesis, &header, premises, equation ? work : NULL, equation);
		free(premises);
		if (!local->rule) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	pg_synthesis_forward(synthesis, job, local->rule);
}

static void derivation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	const struct pg_derivation_input *input = job->inputs[0];
	if (input->parameters.conversion || input->parameters.reduction) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	if (input->count > SIZE_MAX / sizeof(const struct pg_evidence *)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
	}
	if (local->next < input->count) {
		struct pg_synthesis_job *p = pg_synthesis_rule_premise(synthesis, job, local->next);
		if (pg_synthesis_await(synthesis, job, p)) return;
		++local->next;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	int converting = input->rule == PG_TYPE_CONVERSION;
	int normalizing = input->rule == PG_PURE_NORMALIZATION;
	struct pg_derivation_parameters parameters = input->parameters;
	if (job->inputs[1]) {
		struct pg_synthesis_job *effects = (void *)job->inputs[1];
		if (pg_synthesis_await(synthesis, job, effects)) return;
		parameters.effects = pg_effect_inference_result(pg_synthesis_effect_worker(effects), job->inputs[2]);
		if (!parameters.effects) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	if (converting || normalizing) {
		if (input->count != (converting ? 2u : 1u) || !input->source || !input->target) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		const struct pg_evidence *source = pg_synthesis_rule_premise(synthesis, job, 0)->result;
		const struct pg_occurrence *subject = pg_evidence_subject(source);
		if (!subject) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (!local->computation) local->computation = pg_alloc(synthesis->typing->graph, sizeof(*local->computation));
		struct derivation_computation *state = local->computation;
		if (!state) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		const struct pg_term *actual_source = converting ? pg_evidence_classifier(source) : subject->core;
		const struct pg_term *actual_target = NULL;
		if (converting) {
			const struct pg_occurrence *target = pg_evidence_subject(pg_synthesis_rule_premise(synthesis, job, 1)->result);
			if (!target) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			actual_target = target->core;
		}
		if (!state->stage) {
			if (!derivation_endpoint(synthesis, job, actual_source, input->source)) return;
			state->stage = 1;
		}
		if (state->stage == 1) {
			if (converting) {
				struct pg_synthesis_job *comparison = pg_synthesis_compare_terms(synthesis, actual_source, actual_target);
				if (pg_synthesis_await(synthesis, job, comparison)) return;
				state->certificate = pg_synthesis_comparison_certificate(comparison);
			} else if (input->reduction_kind == PG_REDUCTION_PREFIX && input->source == input->target) {
				/* The checked source endpoint already establishes alpha identity.
				 * A zero-step prefix makes no normality claim and must not run NF. */
				state->reduction = pg_reduction_identity(synthesis->typing->graph, &pg_pure_policy, input->target);
				if (!state->reduction) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
			} else {
				state->reduction = pg_synthesis_reduction_advance(synthesis, job, &state->normalizing, actual_source, input->reduction_kind);
				if (!state->reduction) return;
			}
			state->stage = 2;
		}
		if (normalizing) actual_target = pg_reduction_target(state->reduction);
		if (!derivation_endpoint(synthesis, job, actual_target, input->target)) return;
		parameters.conversion = state->certificate;
		parameters.reduction = state->reduction;
	} else if (input->source || input->target) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	const struct pg_evidence **premises = input->count ? malloc(input->count * sizeof(*premises)) : NULL;
	if (input->count && !premises) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	for (size_t i = 0; i < input->count; ++i)
		premises[i] = pg_synthesis_rule_premise(synthesis, job, i)->result;
	job->result = pg_prove_derivation(synthesis->typing, input->rule, &parameters, input->count, premises);
	free(premises);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}
