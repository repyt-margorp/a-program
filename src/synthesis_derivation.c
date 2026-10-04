#include "synthesis_source.h"
#include "synthesis_conversion.h"
#include "synthesis_effect.h"
#include "derivation.h"
#include "effect_inference.h"
#include "computation.h"
#include "dag.h"
#include "derivation_io.h"

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
static void derivation_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void derivation_destroy(struct pg_synthesis_job *);
static struct pg_synthesis_projection derivation_projection(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class DERIVATION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct derivation_work), .advance = derivation_step,
	.destroy = derivation_destroy, .project = derivation_projection}};
static void derivation_destroy(struct pg_synthesis_job *job)
{
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	if (local->computation) pg_comparison_destroy(&local->computation->endpoint);
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
	_Alignas(struct pg_derivation_input) unsigned char header[];
};

static const struct pg_derivation_input *rule_header(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, int create)
{
	uint64_t key[RULE_KEY_FIELDS], hash = UINT64_C(1469598103934665603);
	rule_key(input, key);
	for (size_t i = 0; i < RULE_KEY_FIELDS; ++i) hash = (hash ^ key[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&synthesis->rule_inputs, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct rule_input *candidate = (const void *)p;
		const struct pg_derivation_input *header = (const void *)candidate->header;
		uint64_t fields[RULE_KEY_FIELDS];
		rule_key(header, fields);
		if (!memcmp(key, fields, sizeof(key))) return header;
	}
	if (!create) return NULL;
	struct rule_input *entry = pg_alloc(synthesis->typing->graph, sizeof(*entry) + sizeof(*input));
	if (!entry) return NULL;
	struct pg_derivation_input *header = (void *)entry->header;
	*header = *input;
	return pg_index_insert(&synthesis->rule_inputs, &entry->index, hash) ? NULL : header;
}

struct rule_request_key {
	const void *header[3];
	const void *owner;
	struct pg_synthesis_input (*premise)(const void *, size_t);
};

static struct pg_synthesis_input array_premise(const void *owner, size_t i)
{
	return ((const struct pg_synthesis_input *)owner)[i];
}

static const void *rule_request_operand(const void *owner, size_t i)
{
	const struct rule_request_key *key = owner;
	if (i < 3) return key->header[i];
	struct pg_synthesis_input input = key->premise(key->owner, (i - 3) / 2);
	return (i - 3) % 2 ? (const void *)input.pending : input.checked;
}

static struct pg_synthesis_job *rule_request(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct rule_request_key key,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation)
{
	if (!synthesis || !input || (input->count && !key.premise)) return NULL;
	if (input->effect_parameter) return NULL;
	if (input->count > (SIZE_MAX / sizeof(void *) - 3) / 2) return NULL;
	if (work || equation) {
		if (!work || !equation || input->parameters.effects) return NULL;
		if (input->rule != PG_RETURN_TYPE_FORM) return NULL;
		if (!pg_effect_equation_parameter(work, equation)) return NULL;
		if (work->rows != synthesis->typing->graph) return NULL;
	}
	for (size_t i = 0; i < input->count; ++i)
		if (!pg_synthesis_input_owned(synthesis, key.premise(key.owner, i))) return NULL;
	input = rule_header(synthesis, input, 1);
	if (!input) return NULL;
	key.header[0] = input; key.header[1] = work; key.header[2] = equation;
	return pg_synthesis_work_request_key(synthesis, DERIVATION_JOB, 3 + 2 * input->count, &key, rule_request_operand);
}

struct pg_synthesis_job *pg_synthesis_rule_inputs(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, const struct pg_synthesis_input *premises,
	struct pg_effect_inference *work, const struct pg_effect_equation *equation)
{
	if (input && input->count && !premises) return NULL;
	return rule_request(synthesis, input,
		(struct rule_request_key){.owner = premises, .premise = array_premise}, work, equation);
}

struct pg_synthesis_job *pg_synthesis_plain_rule_inputs(struct pg_synthesis *synthesis,
	enum pg_evidence_rule rule, const struct pg_object *binder, size_t count,
	const struct pg_synthesis_input *premises)
{
	struct pg_derivation_input input = {.rule = rule, .parameters.binder = binder, .count = count};
	return pg_synthesis_rule_inputs(synthesis, &input, premises, NULL, NULL);
}

struct pg_synthesis_input pg_synthesis_reindex_input(struct pg_synthesis *synthesis,
	struct pg_synthesis_input substitution, struct pg_synthesis_input proof)
{
	if (!pg_synthesis_input_owned(synthesis, substitution)) return (struct pg_synthesis_input){0};
	if (!pg_synthesis_input_owned(synthesis, proof)) return (struct pg_synthesis_input){0};
	struct pg_synthesis_input premises[] = {substitution, proof};
	struct pg_derivation_input input = {.rule = PG_REINDEX, .count = 2};
	if (substitution.checked && proof.checked) {
		const struct pg_evidence *result = pg_reindex_receipt(synthesis->typing, substitution.checked, proof.checked);
		if (result) {
			const struct pg_derivation_input *header = rule_header(synthesis, &input, 0);
			const void *key[] = {header, NULL, NULL, substitution.checked, NULL, proof.checked, NULL};
			struct pg_synthesis_job *existing = header ? pg_synthesis_work_find(synthesis, DERIVATION_JOB, 7, key) : NULL;
			return existing ? (struct pg_synthesis_input){.pending = pg_synthesis_pending(existing)} : (struct pg_synthesis_input){.checked = result};
		}
	}
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(
		pg_synthesis_rule_inputs(synthesis, &input, premises, NULL, NULL))};
}

static int import_premise(void *unused, const void *key, size_t i, const void **child)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (i == input->count) return 0;
	*child = input->premises[i];
	return *child ? 1 : -1;
}

struct imported_inputs {
	const struct pg_dag *dag;
	struct pg_synthesis_job *const *jobs;
	const struct pg_derivation_input *input;
};

static struct pg_synthesis_input imported_input(const void *owner, size_t i)
{
	const struct imported_inputs *inputs = owner;
	const struct pg_dag_node *node = pg_dag_find(inputs->dag, inputs->input->premises[i]);
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(inputs->jobs[node->id - 1])};
}

int pg_synthesis_import_rules(struct pg_synthesis *s, size_t count,
	const struct pg_derivation_input *const *inputs, struct pg_effect_inference *effects,
	struct pg_synthesis_job **selected)
{
	if (!s || (count && (!selected || !inputs)) || count > SIZE_MAX / sizeof(void *)) return -1;
	if (!count) return 0;
	struct pg_dag dag;
	if (pg_dag_init(&dag, import_premise, NULL)) return -1;
	int status = -1;
	for (size_t i = 0; i < count; ++i) if (!inputs[i] || pg_dag_add(&dag, inputs[i])) goto done;
	if (dag.count > SIZE_MAX / sizeof(void *)) goto done;
	struct pg_synthesis_job **jobs = pg_alloc(&dag.storage, dag.count * sizeof(*jobs));
	if (!jobs) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct pg_derivation_input *input = node->key;
		struct pg_derivation_input header = *input;
		struct pg_effect_equation *equation = NULL;
		if (header.effect_parameter) {
			equation = pg_effect_equation_find(effects, header.effect_parameter);
			header.effect_parameter = NULL;
		}
		const struct imported_inputs operands = {&dag, jobs, input};
		jobs[node->id - 1] = input->effect_parameter && !equation ? NULL
			: rule_request(s, &header, (struct rule_request_key){.owner = &operands,
				.premise = imported_input}, equation ? effects : NULL, equation);
		if (!jobs[node->id - 1]) goto done;
	}
	for (size_t i = 0; i < count; ++i) selected[i] = jobs[pg_dag_find(&dag, inputs[i])->id - 1];
	status = 0;
done:
	pg_dag_destroy(&dag);
	return status;
}

struct pg_synthesis_job *pg_synthesis_derivation(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input)
{
	return pg_synthesis_derivation_inference(synthesis, input, NULL);
}

struct pg_synthesis_job *pg_synthesis_derivation_inference(struct pg_synthesis *synthesis,
	const struct pg_derivation_input *input, struct pg_effect_inference *work)
{
	struct pg_synthesis_job *result;
	return pg_synthesis_import_rules(synthesis, 1, &input, work, &result) ? NULL : result;
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
	return job && pg_synthesis_work_role(job) == DERIVATION_JOB ? job->inputs[0] : NULL;
}

int pg_synthesis_rule_header_matches(const struct pg_synthesis_job *job,
	const struct pg_derivation_input *input)
{
	const struct pg_derivation_input *actual = pg_synthesis_plain_derivation(job);
	if (!actual || !input) return 0;
	uint64_t expected[RULE_KEY_FIELDS], found[RULE_KEY_FIELDS];
	rule_key(input, expected);
	rule_key(actual, found);
	return !memcmp(expected, found, sizeof(expected));
}

int pg_synthesis_rule_frontier(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t *next)
{
	if (!synthesis || !job || !next || job->pending.owner != synthesis->owner_key || pg_synthesis_work_role(job) != DERIVATION_JOB) return -1;
	const struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	if (job->status != PG_SYNTHESIS_PENDING || local->computation) return 0;
	const struct pg_derivation_input *input = job->inputs[0];
	/* The premise cursor cannot transport the action's substitution cursor. */
	if (input->rule == PG_REINDEX && local->next == input->count) return 0;
	*next = local->next;
	return 1;
}

int pg_synthesis_rule_resume(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, size_t next)
{
	size_t previous;
	if (pg_synthesis_rule_frontier(synthesis, job, &previous) != 1 || previous) return -1;
	const struct pg_derivation_input *input = job->inputs[0];
	if (next > input->count) return -1;
	if (input->rule == PG_REINDEX && next == input->count) return -1;
	for (size_t i = 0; i < next; ++i) {
		struct pg_synthesis_input premise = pg_synthesis_work_dependency(job, 3 + 2 * i);
		if (!pg_evidence_owned_by(pg_synthesis_input_result(premise), synthesis->typing)) return -1;
	}
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	local->next = next;
	return 0;
}

struct pg_synthesis_input pg_synthesis_rule_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, size_t index)
{
	(void)synthesis;
	if (!job || pg_synthesis_work_role(job) != DERIVATION_JOB) return (struct pg_synthesis_input){0};
	const struct pg_derivation_input *input = job->inputs[0];
	if (index >= input->count) return (struct pg_synthesis_input){0};
	return pg_synthesis_work_dependency(job, 3 + 2 * index);
}

struct rule_export {
	const struct pg_synthesis *synthesis;
	struct pg_dag proofs, workers;
	const struct pg_dag *jobs;
	const struct pg_effect_inference *source_effects;
	struct pg_effect_inference *effects;
	const void **roots;
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

static int export_job_child(void *context, const void *key, size_t index, const void **child)
{
	struct rule_export *export = context;
	struct pg_pending *pending = (void *)key;
	if (!pg_synthesis_input_owned(export->synthesis, (struct pg_synthesis_input){.pending = pending})) return -1;
	const struct pg_evidence *proof = pg_pending_result(pending);
	if (proof) return pg_dag_add(&export->proofs, proof);
	const struct pg_synthesis_job *job = pg_pending_job(pending);
	if (job && pg_synthesis_work_role(job) == DERIVATION_JOB) {
		const struct pg_derivation_input *input = job->inputs[0];
		if (index < input->count) {
			struct pg_synthesis_input premise = pg_synthesis_work_dependency(job, 3 + 2 * index);
			if (premise.checked) return pg_dag_add(&export->proofs, premise.checked) ? -1 : 2;
			*child = premise.pending;
			return *child ? 1 : -1;
		}
		return job->inputs[1] ? pg_dag_add(&export->workers, job->inputs[1]) : 0;
	}
	export->status = pg_synthesis_pending_status(pending) == PG_SYNTHESIS_PENDING ? 1 : -1;
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

static const void *export_key(const struct rule_export *export, struct pg_synthesis_input input)
{
	if (input.checked) return input.checked;
	const struct pg_evidence *proof = pg_pending_result(input.pending);
	if (proof) return pg_evidence_owned_by(proof, export->synthesis->typing) ? proof : NULL;
	const struct pg_synthesis_job *job = pg_pending_job(input.pending);
	return job && pg_synthesis_work_role(job) == DERIVATION_JOB ? input.pending : NULL;
}

static const void *export_root(const void *owner, size_t index)
{
	const struct rule_export *export = owner;
	return export->roots[index];
}

static int export_view_header(const void *owner, const void *key, struct pg_derivation_input *header)
{
	const struct rule_export *export = owner;
	if (pg_dag_find(&export->proofs, key)) return pg_derivation_input_header(key, header);
	if (!pg_dag_find(export->jobs, key)) return -1;
	const struct pg_synthesis_job *job = pg_pending_job((void *)key);
	if (!job || pg_synthesis_work_role(job) != DERIVATION_JOB) return -1;
	*header = *(const struct pg_derivation_input *)job->inputs[0];
	const struct pg_effect_inference *worker = job->inputs[1];
	if (worker) header->effect_parameter = pg_effect_equation_parameter(worker, job->inputs[2]);
	return 0;
}

static int export_view_child(const void *owner, const void *key, size_t index, const void **child)
{
	const struct rule_export *export = owner;
	if (pg_dag_find(&export->proofs, key)) {
		const struct pg_evidence *proof = NULL;
		int status = pg_derivation_input_dependency(export->synthesis->typing, key, index, &proof);
		*child = proof;
		return status;
	}
	if (!pg_dag_find(export->jobs, key)) return -1;
	const struct pg_synthesis_job *job = pg_pending_job((void *)key);
	if (!job || pg_synthesis_work_role(job) != DERIVATION_JOB) return -1;
	const struct pg_derivation_input *input = job->inputs[0];
	if (index > input->count) return -1;
	if (index == input->count) return 0;
	*child = export_key(export, pg_synthesis_work_dependency(job, 3 + 2 * index));
	return *child ? 1 : -1;
}

int pg_synthesis_export_rule_closure(const struct pg_synthesis *synthesis,
	struct pg_dag *roots, const struct pg_dag *checked, struct pg_graph *storage, struct pg_effect_inference *effects,
	int require_closed,
	int (*observe)(void *, const struct pg_derivation_input *, const struct pg_effect_inference *),
	void *owner, int (*emit)(void *, const struct pg_derivation_view *))
{
	if (!synthesis || !storage || !effects || !emit || !roots || roots->child || roots->failed) {
		if (effects) pg_effect_inference_fail(effects);
		return -1;
	}
	if (effects->rows != storage || effects->sealed || effects->failed || effects->row_sources.count) {
		pg_effect_inference_fail(effects);
		return -1;
	}
	struct pg_dag jobs = {0};
	struct rule_export export = {.synthesis = synthesis, .jobs = &jobs, .effects = effects, .status = -1};
	if (pg_dag_init(&export.proofs, export_proof_child, &export) || pg_dag_init(&export.workers, NULL, NULL)
		|| pg_dag_init(&jobs, export_job_child, &export)) goto done;
	const struct pg_dag_node *last_proof = NULL, *last_job = NULL, *last_worker = NULL;
	for (const struct pg_dag_node *root = roots->first; root; root = root->next) {
		if (checked && pg_dag_find(checked, root->key)) {
			if (!pg_evidence_owned_by(root->key, synthesis->typing)) goto done;
			if (pg_dag_add(&export.proofs, root->key)) goto done;
		} else if (pg_dag_add(&jobs, root->key)) goto done;
		for (const struct pg_dag_node *node = last_worker ? last_worker->next : export.workers.first;
			node; last_worker = node, node = node->next) {
			export.source_effects = node->key;
			if (require_closed && !export.source_effects->sealed) { export.status = 1; goto done; }
			if (pg_effect_inference_visit(node->key, &export, export_equation, export_dependency)) goto done;
			if (observe && observe(owner, NULL, node->key)) goto done;
		}
		if (!observe) continue;
		for (const struct pg_dag_node *node = last_proof ? last_proof->next : export.proofs.first;
			node; last_proof = node, node = node->next) {
			struct pg_derivation_input header;
			if (pg_derivation_input_header(node->key, &header) || observe(owner, &header, NULL)) goto done;
		}
		for (const struct pg_dag_node *node = last_job ? last_job->next : jobs.first;
			node; last_job = node, node = node->next) {
			if (pg_pending_result((void *)node->key)) continue;
			const struct pg_synthesis_job *job = pg_pending_job((void *)node->key);
			if (!job || pg_synthesis_work_role(job) != DERIVATION_JOB) continue;
			struct pg_derivation_input header = *(const struct pg_derivation_input *)job->inputs[0];
			const struct pg_effect_inference *worker = job->inputs[1];
			if (worker) header.effect_parameter = pg_effect_equation_parameter(worker, job->inputs[2]);
			if (observe(owner, &header, NULL)) goto done;
		}
	}
	if (roots->failed) goto done;
	size_t count = roots->count;
	if (count > SIZE_MAX / sizeof(*export.roots)) goto done;
	export.roots = pg_alloc(&jobs.storage, count * sizeof(*export.roots));
	if (!export.roots) goto done;
	for (const struct pg_dag_node *node = roots->first; node; node = node->next) {
		struct pg_synthesis_input input = checked && pg_dag_find(checked, node->key)
			? (struct pg_synthesis_input){.checked = node->key} : (struct pg_synthesis_input){.pending = (void *)node->key};
		if (!(export.roots[node->id - 1] = export_key(&export, input))) goto done;
	}
	struct pg_derivation_view view = {count, &export, export_root, export_view_header, export_view_child};
	if (emit(owner, &view)) goto done;
	export.status = 0;
done:
	pg_dag_destroy(&jobs);
	pg_dag_destroy(&export.proofs);
	pg_dag_destroy(&export.workers);
	if (export.status) pg_effect_inference_fail(effects);
	return export.status;
}

struct export_selection {
	const struct pg_dag *selected;
	const struct pg_synthesis_input *inputs;
	size_t count;
	const struct pg_derivation_view *view;
	void *owner;
	int (*emit)(void *, const struct pg_derivation_view *);
};

static const void *selection_root(const void *owner, size_t index)
{
	const struct export_selection *selection = owner;
	struct pg_synthesis_input input = selection->inputs[index];
	const struct pg_dag_node *node = pg_dag_find(selection->selected,
		input.checked ? (const void *)input.checked : input.pending);
	return selection->view->root(selection->view->owner, node->id - 1);
}

static int selection_header(const void *owner, const void *key, struct pg_derivation_input *header)
{
	const struct export_selection *selection = owner;
	return selection->view->header(selection->view->owner, key, header);
}

static int selection_child(const void *owner, const void *key, size_t index, const void **child)
{
	const struct export_selection *selection = owner;
	return selection->view->child(selection->view->owner, key, index, child);
}

static int emit_selection(void *owner, const struct pg_derivation_view *view)
{
	struct export_selection *selection = owner;
	selection->view = view;
	struct pg_derivation_view ordered = {selection->count, selection,
		selection_root, selection_header, selection_child};
	return selection->emit(selection->owner, &ordered);
}

int pg_synthesis_export_rules(const struct pg_synthesis *synthesis, size_t count,
	const struct pg_synthesis_input *roots, struct pg_graph *storage,
	struct pg_effect_inference *effects, int require_closed,
	void *owner, int (*emit)(void *, const struct pg_derivation_view *))
{
	struct pg_dag selected = {0}, checked = {0};
	int status = -1;
	if (!synthesis || !storage || !emit || (count && !roots) || count > SIZE_MAX / sizeof(*roots)) goto done;
	if (pg_dag_init(&selected, NULL, NULL) || pg_dag_init(&checked, NULL, NULL)) goto done;
	for (size_t i = 0; i < count; ++i) {
		if (!pg_synthesis_input_owned(synthesis, roots[i])) goto done;
		struct pg_synthesis_input input = roots[i];
		if (pg_dag_add(&selected, input.checked ? (const void *)input.checked : input.pending)) goto done;
		if (input.checked && pg_dag_add(&checked, input.checked)) goto done;
	}
	struct export_selection selection = {&selected, roots, count, NULL, owner, emit};
	status = pg_synthesis_export_rule_closure(synthesis, &selected, &checked, storage,
		effects, require_closed, NULL, &selection, emit_selection);
done:
	pg_dag_destroy(&selected);
	pg_dag_destroy(&checked);
	if (status && effects) pg_effect_inference_fail(effects);
	return status;
}

static const struct pg_evidence *checked_input(const void *owner, size_t i)
{
	const struct pg_synthesis_job *job = owner;
	return pg_synthesis_input_result(pg_synthesis_work_dependency(job, 3 + 2 * i));
}

static void derivation_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct derivation_work *local = pg_synthesis_work_state(job, DERIVATION_JOB);
	const struct pg_derivation_input *input = job->inputs[0];
	if (input->parameters.conversion || input->parameters.reduction) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	while (local->next < input->count) {
		struct pg_synthesis_input p = pg_synthesis_rule_input(synthesis, job, local->next);
		if (pg_synthesis_await_input(synthesis, job, p)) return;
		++local->next;
	}
	int converting = input->rule == PG_TYPE_CONVERSION;
	int normalizing = input->rule == PG_PURE_NORMALIZATION;
	struct pg_derivation_parameters parameters = input->parameters;
	if (job->inputs[1]) {
		struct pg_effect_inference *effects = (void *)job->inputs[1];
		if (pg_synthesis_await_effects(synthesis, job, effects)) return;
		parameters.effects = pg_effect_inference_result(effects, job->inputs[2]);
		if (!parameters.effects) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	}
	if (converting || normalizing) {
		if (input->count != (converting ? 2u : 1u) || !input->source || !input->target) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		const struct pg_evidence *source = pg_synthesis_input_result(pg_synthesis_rule_input(synthesis, job, 0));
		const struct pg_occurrence *subject = pg_evidence_subject(source);
		if (!subject) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (!local->computation) local->computation = pg_alloc(synthesis->typing->graph, sizeof(*local->computation));
		struct derivation_computation *state = local->computation;
		if (!state) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
		const struct pg_term *actual_source = converting ? pg_evidence_classifier(source) : subject->core;
		const struct pg_term *actual_target = NULL;
		if (converting) {
			const struct pg_occurrence *target = pg_evidence_subject(pg_synthesis_input_result(pg_synthesis_rule_input(synthesis, job, 1)));
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
	if (input->rule == PG_REINDEX) {
		if (input->count != 2) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		pg_synthesis_reindex_step(synthesis, job, checked_input(job, 0), checked_input(job, 1));
		return;
	}
	job->result = pg_prove_derivation_inputs(synthesis->typing, input->rule,
		&parameters, input->count, job, checked_input);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}
