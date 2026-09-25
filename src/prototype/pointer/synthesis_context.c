#include "synthesis_work.h"

struct reindex_work { struct pg_occurrence_action *action; };
struct pair_work { struct pg_synthesis_job *checked; };

static void reindex_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void pair_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void lift_step(struct pg_synthesis *, struct pg_synthesis_job *);

static const struct pg_synthesis_work_class REINDEX_JOB[1] = {{
	.size = sizeof(struct reindex_work), .advance = reindex_step}};
static const struct pg_synthesis_work_class PAIR_JOB[1] = {{
	.size = sizeof(struct pair_work), .advance = pair_step}};
static const struct pg_synthesis_work_class LIFT_JOB[1] = {{.advance = lift_step}};

static int reindex_inputs(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing)) return 0;
	if (!pg_evidence_owned_by(proof, synthesis->typing)) return 0;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	return pg_evidence_context(proof) == pg_evidence_context_map(substitution)->source;
}

struct pg_synthesis_job *pg_synthesis_reindex_jobs(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *substitution, struct pg_synthesis_job *proof)
{
	if (!substitution || substitution->owner != synthesis->owner_key) return NULL;
	if (!proof || proof->owner != synthesis->owner_key) return NULL;
	const void *inputs[] = {substitution, proof};
	return pg_synthesis_work_request(synthesis, REINDEX_JOB, 2, inputs);
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
	if (pg_evidence_context_map(substitution)->source != pg_evidence_context(extension)->parent) return NULL;
	const void *inputs[] = {substitution, extension, image};
	return pg_synthesis_work_request(synthesis, PAIR_JOB, 3, inputs);
}

struct pg_synthesis_job *pg_synthesis_substitution_lift(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing) || pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(extension, synthesis->typing)) return NULL;
	enum pg_evidence_rule rule = pg_evidence_rule(extension);
	if (rule != PG_CONTEXT_EXTEND && rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (!binder || pg_evidence_context(extension)->parent != pg_evidence_context_map(substitution)->source) return NULL;
	const void *inputs[] = {substitution, extension, binder};
	return pg_synthesis_work_request(synthesis, LIFT_JOB, 3, inputs);
}

static void reindex_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct reindex_work *local = pg_synthesis_work_state(job, REINDEX_JOB);
	if (!local->action) {
		for (size_t i = 0; i < 2; ++i)
			if (pg_synthesis_await(synthesis, job, (void *)job->inputs[i])) return;
		const struct pg_evidence *substitution = ((const struct pg_synthesis_job *)job->inputs[0])->result;
		const struct pg_evidence *proof = ((const struct pg_synthesis_job *)job->inputs[1])->result;
		if (!reindex_inputs(synthesis, substitution, proof)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
		}
		/* Producers converge before borrowing the one typed action worker. */
		if (pg_synthesis_forward(synthesis, job, pg_synthesis_reindex(synthesis, substitution, proof))) return;
		local->action = pg_occurrence_action_request(synthesis->typing,
			pg_evidence_context_map(substitution), pg_evidence_subject(proof));
		if (!local->action) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	switch (pg_occurrence_action_advance(local->action, 1)) {
	case PG_SUBSTITUTION_PENDING:
		pg_synthesis_enqueue(synthesis, job);
		return;
	case PG_SUBSTITUTION_ERROR:
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
		break;
	case PG_SUBSTITUTION_DONE:
		job->result = pg_prove_reindex(synthesis->typing,
			((const struct pg_synthesis_job *)job->inputs[0])->result,
			((const struct pg_synthesis_job *)job->inputs[1])->result);
		pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		break;
	}
}

static void pair_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pair_work *local = pg_synthesis_work_state(job, PAIR_JOB);
	if (!local->checked) {
		struct pg_synthesis_job *type = pg_synthesis_reindex(synthesis,
			job->inputs[0], pg_evidence_premise(job->inputs[1], 1));
		local->checked = pg_synthesis_expect(synthesis,
			pg_synthesis_evidence(synthesis, job->inputs[2]), type);
	}
	if (pg_synthesis_await(synthesis, job, local->checked)) return;
	job->result = pg_prove_substitution_pair(synthesis->typing,
		job->inputs[0], job->inputs[1], local->checked->result);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
}

static void lift_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	job->result = pg_prove_substitution_lift(synthesis->typing, job->inputs[0], job->inputs[1], job->inputs[2]);
	pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED);
}
