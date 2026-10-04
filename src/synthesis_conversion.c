#include "synthesis_conversion.h"
#include "synthesis_source.h"
#include "computation.h"
#include "eval_internal.h"

struct conversion_work {
	struct pg_conversion comparison;
	const struct pg_conversion_certificate *certificate;
};
struct expectation_work { const struct pg_evidence *checking_type; };
struct normalization_work {
	struct pg_synthesis_reduction reduction;
	const struct pg_evidence *checking_term;
};
struct classifier_work {
	struct pg_typed_query *formation;
	struct pg_synthesis_job *normalizing;
};

static void conversion_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void conversion_destroy(struct pg_synthesis_job *);
static void expect_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void normalization_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void classifier_step(struct pg_synthesis *, struct pg_synthesis_job *);

static const void *resolved_input(const struct pg_synthesis_job *job, size_t ordinal)
{
	return ordinal % 2 ? NULL : pg_synthesis_input_result(pg_synthesis_work_dependency(job, ordinal));
}

static const void *normalization_resolved_input(const struct pg_synthesis_job *job, size_t ordinal)
{
	return ordinal == 4 ? job->inputs[4] : resolved_input(job, ordinal);
}

static const struct pg_synthesis_work_class CONVERSION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct conversion_work), .advance = conversion_step, .destroy = conversion_destroy}};
static const struct pg_synthesis_work_class EXPECT_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct expectation_work), .advance = expect_step, .resolved_input = resolved_input}};
static const struct pg_synthesis_work_class NORMALIZATION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct normalization_work), .advance = normalization_step,
	.resolved_input = normalization_resolved_input}};
static const struct pg_synthesis_work_class NF_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct normalization_work), .advance = normalization_step,
	.resolved_input = normalization_resolved_input}};
static const struct pg_synthesis_work_class CLASSIFIER_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct classifier_work), .advance = classifier_step, .resolved_input = resolved_input}};

struct pg_synthesis_input pg_synthesis_classifier_input(const struct pg_synthesis_job *job)
{
	return job && pg_synthesis_work_role(job) == CLASSIFIER_JOB ? pg_synthesis_work_dependency(job, 2) : (struct pg_synthesis_input){0};
}

struct pg_synthesis_input pg_synthesis_expect_input(const struct pg_synthesis_job *job)
{
	return job && pg_synthesis_work_role(job) == EXPECT_JOB ? pg_synthesis_work_dependency(job, 0) : (struct pg_synthesis_input){0};
}

struct pg_synthesis_input pg_synthesis_expect_target(const struct pg_synthesis_job *job)
{
	return job && pg_synthesis_work_role(job) == EXPECT_JOB ? pg_synthesis_work_dependency(job, 2) : (struct pg_synthesis_input){0};
}

static void conversion_destroy(struct pg_synthesis_job *job)
{
	struct conversion_work *local = pg_synthesis_work_state(job, CONVERSION_JOB);
	pg_conversion_destroy(&local->comparison);
}

const struct pg_conversion_certificate *pg_synthesis_comparison_certificate(const struct pg_synthesis_job *job)
{
	const struct conversion_work *local = pg_synthesis_work_state(job, CONVERSION_JOB);
	return local && job->status == PG_SYNTHESIS_DONE ? local->certificate : NULL;
}

struct pg_synthesis_job *pg_synthesis_expect_inputs(struct pg_synthesis *synthesis,
	struct pg_synthesis_input term, struct pg_synthesis_input type)
{
	if (!pg_synthesis_input_owned(synthesis, term)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, type)) return NULL;
	const void *inputs[] = {term.checked, term.pending, type.checked, type.pending};
	return pg_synthesis_work_request(synthesis, EXPECT_JOB, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
	return pg_synthesis_reduction_request(synthesis, (struct pg_synthesis_input){.checked = context},
		(struct pg_synthesis_input){.checked = proof}, PG_REDUCTION_WHNF, 0);
}

struct pg_synthesis_job *pg_synthesis_nf(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
	return pg_synthesis_reduction_request(synthesis, (struct pg_synthesis_input){.checked = context},
		(struct pg_synthesis_input){.checked = proof}, PG_REDUCTION_NF, 0);
}

struct pg_synthesis_job *pg_synthesis_reduction_request(struct pg_synthesis *synthesis,
	struct pg_synthesis_input context, struct pg_synthesis_input proof,
	enum pg_reduction_kind kind, int force)
{
	if (!pg_synthesis_input_owned(synthesis, context)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, proof)) return NULL;
	if (kind != PG_REDUCTION_WHNF && kind != PG_REDUCTION_NF) return NULL;
	if (force != 0 && force != 1) return NULL;
	/* Checked and pending operands use the same request identity contract. */
	const void *inputs[] = {context.checked, context.pending, proof.checked, proof.pending,
		force ? &pg_force_operation : NULL};
	return pg_synthesis_work_request(synthesis, kind == PG_REDUCTION_NF ? NF_JOB : NORMALIZATION_JOB, 5, inputs);
}

int pg_synthesis_normalization_input(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job, struct pg_synthesis_input *context,
	struct pg_synthesis_input *proof, enum pg_reduction_kind *kind, int *force)
{
	if (!synthesis || !job || !context || !proof || !kind) return -1;
	if (job->pending.owner != synthesis->owner_key) return -1;
	if (pg_synthesis_work_role(job) != NORMALIZATION_JOB && pg_synthesis_work_role(job) != NF_JOB) return -1;
	*context = pg_synthesis_work_dependency(job, 0);
	*proof = pg_synthesis_work_dependency(job, 2);
	*kind = pg_synthesis_work_role(job) == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF;
	if (force) *force = job->inputs[4] != NULL;
	return 0;
}

struct pg_synthesis_input pg_synthesis_normalize_classifier(struct pg_synthesis *synthesis,
	struct pg_synthesis_input context, struct pg_synthesis_input proof)
{
	if (!pg_synthesis_input_owned(synthesis, context)) return (struct pg_synthesis_input){0};
	if (!pg_synthesis_input_owned(synthesis, proof)) return (struct pg_synthesis_input){0};
	if (proof.checked) {
		switch (pg_evidence_judgement(proof.checked)) {
		case PG_JUDGEMENT_TYPE_FAMILY:
			if (!context.checked) break;
			if (!pg_synthesis_typed_input(synthesis, context.checked, proof.checked)) return (struct pg_synthesis_input){0};
			/* Already a checked family: no classifier work or scheduler frame. */
			return proof;
		case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION: break;
		default: return (struct pg_synthesis_input){0};
		}
	}
	const void *inputs[] = {context.checked, context.pending, proof.checked, proof.pending};
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(pg_synthesis_work_request(synthesis, CLASSIFIER_JOB, 4, inputs))};
}

static void classifier_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct classifier_work *local = pg_synthesis_work_state(job, CLASSIFIER_JOB);
	struct pg_synthesis_input context = pg_synthesis_work_dependency(job, 0);
	struct pg_synthesis_input input = pg_synthesis_classifier_input(job);
	const struct pg_evidence *term = pg_synthesis_input_result(input);
	if (!local->normalizing) {
		if (pg_synthesis_await_input(synthesis, job, context)) return;
		if (pg_synthesis_await_input(synthesis, job, input)) return;
		if (!pg_synthesis_typed_input(synthesis, pg_synthesis_input_result(context), term)) goto rejected;
		if (pg_synthesis_forward(synthesis, job, pg_synthesis_work_resolve(synthesis, job))) return;
		switch (pg_evidence_judgement(term)) {
		case PG_JUDGEMENT_TYPE_FAMILY:
			job->result = term;
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
			return;
		case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION: break;
		default: goto rejected;
		}
		if (!local->formation) local->formation = pg_classifier_request(synthesis->typing,
			pg_synthesis_input_result(context), term);
		if (pg_synthesis_yield_query(synthesis, job, local->formation)) return;
		const struct pg_evidence *type = pg_typed_query_result(local->formation);
		if (!type) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return; }
		local->normalizing = pg_synthesis_normalize(synthesis, pg_synthesis_input_result(context), type);
		pg_synthesis_subscribe(synthesis, job, local->normalizing, 0);
		return;
	}
	if (pg_synthesis_await(synthesis, job, local->normalizing)) return;
	if (pg_evidence_classifier(term) == pg_evidence_subject(local->normalizing->result)->core)
		job->result = term;
	else job->result = pg_synthesis_convert(synthesis, job, term, local->normalizing->result);
	if (job->result) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

struct pg_synthesis_job *pg_synthesis_compare_terms(struct pg_synthesis *synthesis,
	const struct pg_term *left, const struct pg_term *right)
{
	const void *inputs[] = {left, right};
	return pg_synthesis_work_request(synthesis, CONVERSION_JOB, 2, inputs);
}

static void conversion_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct conversion_work *local = pg_synthesis_work_state(job, CONVERSION_JOB);
	if (!local->comparison.state) {
		if (pg_conversion_init(&local->comparison, synthesis->normalization,
			job->inputs[0], job->inputs[1]) != 0) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	enum pg_conversion_status status = pg_conversion_advance(&local->comparison, 1);
	if (status == PG_CONVERSION_PENDING) {
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	local->certificate = pg_conversion_certificate(&local->comparison);
	pg_conversion_destroy(&local->comparison);
	if (status == PG_CONVERSION_DIFFERENT) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
	pg_synthesis_finish(synthesis, job, status == PG_CONVERSION_EQUAL ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

const struct pg_evidence *pg_synthesis_convert(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_evidence *term, const struct pg_evidence *type)
{
	struct pg_synthesis_job *comparison = pg_synthesis_compare_terms(synthesis,
		pg_evidence_classifier(term), pg_evidence_subject(type)->core);
	if (pg_synthesis_await(synthesis, job, comparison)) return NULL;
	const struct pg_evidence *result = pg_prove_conversion(synthesis->typing,
		term, type, pg_synthesis_comparison_certificate(comparison));
	if (!result) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
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
	struct expectation_work *local = pg_synthesis_work_state(job, EXPECT_JOB);
	struct pg_synthesis_input value = pg_synthesis_expect_input(job);
	struct pg_synthesis_input expected = pg_synthesis_expect_target(job);
	if (!local->checking_type) {
		if (pg_synthesis_await_input(synthesis, job, value)) return;
		if (pg_synthesis_await_input(synthesis, job, expected)) return;
		const struct pg_evidence *term = pg_synthesis_input_result(value);
		const struct pg_evidence *type = pg_synthesis_input_result(expected);
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
		if (pg_synthesis_forward(synthesis, job, pg_synthesis_work_resolve(synthesis, job))) return;
		local->checking_type = post_check_type(synthesis, term, type);
		if (!local->checking_type) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	job->result = pg_synthesis_convert(synthesis, job, pg_synthesis_input_result(value), local->checking_type);
	const struct pg_evidence *target = pg_synthesis_input_result(expected);
	if (job->result && local->checking_type != target) {
		job->result = pg_prove_effect_subsumption(synthesis->typing, job->result, target);
		if (!job->result) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	if (job->result) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED);
}

const struct pg_reduction_certificate *pg_synthesis_reduction_advance(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, struct pg_synthesis_reduction *work,
	const struct pg_term *input, enum pg_reduction_kind kind)
{
	const struct pg_reduction_certificate *certificate = NULL;
	if (kind == PG_REDUCTION_NF || kind == PG_REDUCTION_PREFIX) {
		if (!work->nf) work->nf = pg_nf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!work->nf) goto failure;
		if (kind == PG_REDUCTION_PREFIX) {
			int status = pg_nf_prefix_certificate(work->nf, &certificate);
			if (status < 0) goto failure;
			if (status) return certificate;
		}
		if (pg_nf_advance(work->nf, 1) == PG_NF_ERROR) goto failure;
		if (kind == PG_REDUCTION_PREFIX) {
			if (pg_nf_prefix_certificate(work->nf, &certificate) < 0) goto failure;
		} else certificate = pg_nf_certificate(work->nf);
	} else if (kind == PG_REDUCTION_WHNF) {
		if (!work->whnf) work->whnf = pg_whnf_request(synthesis->normalization, &pg_pure_policy, input);
		if (!work->whnf || pg_whnf_advance(work->whnf, 1) == PG_EVAL_ERROR) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return NULL;
		}
		certificate = pg_whnf_certificate(work->whnf);
	} else { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return NULL; }
	if (!certificate) pg_synthesis_enqueue(synthesis, job);
	return certificate;
failure:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
	return NULL;
}

static const struct pg_evidence *normalization_input(struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	const struct pg_evidence *context = pg_synthesis_input_result(pg_synthesis_work_dependency(job, 0));
	const struct pg_evidence *proof = pg_synthesis_input_result(pg_synthesis_work_dependency(job, 2));
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
	if (!job->inputs[4]) return proof;
	if (pg_evidence_context(context)) return NULL;
	const struct pg_term *content;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE &&
		pg_thunk_type_view(pg_evidence_classifier(proof), &content))
		return pg_prove_force(synthesis->typing, proof);
	return proof;
}

const struct pg_whnf_job *pg_synthesis_normalization_pending(const struct pg_synthesis *synthesis,
	const struct pg_synthesis_job *job)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key) return NULL;
	if (job->status != PG_SYNTHESIS_PENDING) return NULL;
	const struct normalization_work *local = pg_synthesis_work_state(job, NORMALIZATION_JOB);
	if (!local || !local->checking_term || !local->reduction.whnf) return NULL;
	return pg_whnf_status(local->reduction.whnf) == PG_EVAL_PENDING ? local->reduction.whnf : NULL;
}

int pg_synthesis_normalization_attach(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	struct pg_whnf_job *whnf)
{
	if (!synthesis || !job || job->pending.owner != synthesis->owner_key || !whnf) return -1;
	if (job->status != PG_SYNTHESIS_PENDING || job->dependency) return -1;
	struct normalization_work *local = pg_synthesis_work_state(job, NORMALIZATION_JOB);
	if (!local || local->checking_term || local->reduction.whnf) return -1;
	if (whnf->request.policy != &pg_pure_policy) return -1;
	if (whnf->status != PG_EVAL_PENDING || whnf->certificate) return -1;
	if (whnf->request.work && whnf->request.work != synthesis->normalization) return -1;
	const struct pg_evidence *input = normalization_input(synthesis, job);
	if (!input || pg_evidence_subject(input)->core != whnf->request.input) return -1;
	if (!whnf->request.work && pg_whnf_job_attach(synthesis->normalization, whnf)) return -1;
	local->checking_term = input;
	local->reduction.whnf = whnf;
	return 0;
}

static void normalization_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct normalization_work *local = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (!local->checking_term) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_input premise = pg_synthesis_work_dependency(job, 2 * i);
			if (pg_synthesis_await_input(synthesis, job, premise)) return;
		}
		const struct pg_evidence *input = normalization_input(synthesis, job);
		if (!input) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		if (pg_synthesis_forward(synthesis, job, pg_synthesis_work_resolve(synthesis, job))) return;
		local->checking_term = input;
	}
	const struct pg_term *input = pg_evidence_subject(local->checking_term)->core;
	const struct pg_reduction_certificate *certificate = pg_synthesis_reduction_advance(synthesis, job, &local->reduction, input,
		pg_synthesis_work_role(job) == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
	if (!certificate) return;
	job->result = pg_prove_normalization(synthesis->typing, local->checking_term, certificate);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return;
}
