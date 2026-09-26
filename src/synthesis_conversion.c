#include "synthesis_conversion.h"
#include "synthesis_source.h"
#include "computation.h"

struct conversion_work {
	struct pg_conversion comparison;
	const struct pg_conversion_certificate *certificate;
};
struct expectation_work { const struct pg_evidence *checking_type; };
struct normalization_work {
	struct pg_synthesis_reduction reduction;
	const struct pg_evidence *checking_term;
};
struct classifier_work { struct pg_synthesis_job *left, *right; };

static void conversion_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void conversion_destroy(struct pg_synthesis_job *);
static void expect_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void normalization_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void classifier_step(struct pg_synthesis *, struct pg_synthesis_job *);

static const struct pg_synthesis_work_class CONVERSION_JOB[1] = {{
	.size = sizeof(struct conversion_work), .advance = conversion_step, .destroy = conversion_destroy}};
static const struct pg_synthesis_work_class EXPECT_JOB[1] = {{
	.size = sizeof(struct expectation_work), .advance = expect_step}};
static const struct pg_synthesis_work_class NORMALIZATION_JOB[1] = {{
	.size = sizeof(struct normalization_work), .advance = normalization_step}};
static const struct pg_synthesis_work_class NF_JOB[1] = {{
	.size = sizeof(struct normalization_work), .advance = normalization_step}};
static const struct pg_synthesis_work_class CLASSIFIER_JOB[1] = {{
	.size = sizeof(struct classifier_work), .advance = classifier_step}};

struct pg_synthesis_job *pg_synthesis_classifier_input(const struct pg_synthesis_job *job)
{
	return job && job->role == CLASSIFIER_JOB ? (void *)job->inputs[1] : NULL;
}

struct pg_synthesis_job *pg_synthesis_expect_input(const struct pg_synthesis_job *job)
{
	return job && job->role == EXPECT_JOB ? (void *)job->inputs[0] : NULL;
}

struct pg_synthesis_job *pg_synthesis_expect_target(const struct pg_synthesis_job *job)
{
	return job && job->role == EXPECT_JOB ? (void *)job->inputs[1] : NULL;
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

struct pg_synthesis_job *pg_synthesis_expect(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *term, struct pg_synthesis_job *type)
{
	if (!term || term->owner != synthesis->owner_key) return NULL;
	if (!type || type->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {term, type};
	return pg_synthesis_work_request(synthesis, EXPECT_JOB, 2, inputs);
}

struct pg_synthesis_job *pg_synthesis_normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
	return pg_synthesis_normalize_jobs(synthesis, pg_synthesis_evidence(synthesis, context),
		pg_synthesis_evidence(synthesis, proof), PG_REDUCTION_WHNF);
}

struct pg_synthesis_job *pg_synthesis_nf(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
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
	return pg_synthesis_work_request(synthesis, kind == PG_REDUCTION_NF ? NF_JOB : NORMALIZATION_JOB, 3, inputs);
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
	if (!pg_synthesis_typed_input(synthesis, context, proof)) return NULL;
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
	const void *inputs[] = {context, proof};
	return pg_synthesis_work_request(synthesis, CLASSIFIER_JOB, 2, inputs);
}

static void classifier_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct classifier_work *local = pg_synthesis_work_state(job, CLASSIFIER_JOB);
	const struct pg_synthesis_job *context = job->inputs[0], *term = job->inputs[1];
	if (!local->left) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (void *)job->inputs[i];
			if (pg_synthesis_await(synthesis, job, input)) return;
		}
		struct pg_synthesis_job *canonical = pg_synthesis_normalize_classifier(synthesis, context->result, term->result);
		if (!canonical) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		if (!local->right) local->right = pg_synthesis_classifier_formation(synthesis, context, term);
		if (pg_synthesis_await(synthesis, job, local->right)) return;
		local->left = pg_synthesis_normalize(synthesis, context->result, local->right->result);
		pg_synthesis_subscribe(synthesis, job, local->left, 0);
		return;
	}
	if (local->left->status != PG_SYNTHESIS_DONE) { pg_synthesis_finish(synthesis, job, local->left->status); return; }
	if (pg_evidence_classifier(term->result) == pg_evidence_subject(local->left->result)->core)
		job->result = term->result;
	else job->result = pg_synthesis_convert(synthesis, job, term->result, local->left->result);
	if (job->result) pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
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
	const struct pg_synthesis_job *producer = job->inputs[0];
	if (!local->checking_type) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *input = (struct pg_synthesis_job *)job->inputs[i];
			if (pg_synthesis_await(synthesis, job, input)) return;
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
		if (pg_synthesis_forward(synthesis, job, canonical)) return;
		local->checking_type = post_check_type(synthesis, term, type);
		if (!local->checking_type) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return; }
	}
	job->result = pg_synthesis_convert(synthesis, job, producer->result, local->checking_type);
	const struct pg_evidence *target = ((const struct pg_synthesis_job *)job->inputs[1])->result;
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

static void normalization_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct normalization_work *local = pg_synthesis_work_state(job, job->role);
	const struct pg_synthesis_job *context = job->inputs[0], *proof = job->inputs[1];
	if (!local->checking_term) {
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_job *premise = (void *)job->inputs[i];
			if (pg_synthesis_await(synthesis, job, premise)) return;
		}
		if (!pg_synthesis_typed_input(synthesis, context->result, proof->result)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		local->checking_term = proof->result;
		if (job->inputs[2]) {
			if (pg_evidence_context(context->result)) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
			const struct pg_term *content;
			if (pg_evidence_judgement(proof->result) == PG_JUDGEMENT_VALUE &&
				pg_thunk_type_view(pg_evidence_classifier(proof->result), &content))
				local->checking_term = pg_prove_force(synthesis->typing, proof->result);
			if (!local->checking_term) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return; }
		}
	}
	const struct pg_term *input = pg_evidence_subject(local->checking_term)->core;
	const struct pg_reduction_certificate *certificate = pg_synthesis_reduction_advance(synthesis, job, &local->reduction, input,
		job->role == NF_JOB ? PG_REDUCTION_NF : PG_REDUCTION_WHNF);
	if (!certificate) return;
	job->result = pg_prove_normalization(synthesis->typing, local->checking_term, certificate);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
	return;
}
