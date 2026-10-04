#include "synthesis_work.h"
#include "derivation.h"
#include <stdlib.h>

struct substitution_work {
	struct pg_synthesis_input map;
	const struct pg_evidence **extensions;
	size_t next;
	struct pg_synthesis_job *returned;
};

static void substitution_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void substitution_destroy(struct pg_synthesis_job *);
static void substitution_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static struct pg_synthesis_input substitution_output(const struct pg_synthesis_job *);

static const struct pg_synthesis_work_class SUBSTITUTION_JOB[1] = {{
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct substitution_work), .advance = substitution_step,
	.destroy = substitution_destroy, .completed = substitution_completed, .output = substitution_output}};

static void substitution_destroy(struct pg_synthesis_job *job)
{
	struct substitution_work *local = pg_synthesis_work_state(job, SUBSTITUTION_JOB);
	free(local->extensions);
	local->extensions = NULL;
}

static void substitution_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int first)
{
	(void)synthesis;
	(void)first;
	substitution_destroy(job);
}

static struct pg_synthesis_input substitution_output(const struct pg_synthesis_job *job)
{
	const struct substitution_work *local = pg_synthesis_work_state(job, SUBSTITUTION_JOB);
	return local->map;
}

static int reindex_inputs(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing)) return 0;
	if (!pg_evidence_owned_by(proof, synthesis->typing)) return 0;
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return 0;
	if (!pg_evidence_subject(proof)) return 0;
	return pg_evidence_context(proof) == pg_evidence_context_map(substitution)->source;
}

struct pg_synthesis_input pg_synthesis_substitution_pair(struct pg_synthesis *synthesis,
	const struct pg_evidence *substitution, const struct pg_evidence *extension,
	const struct pg_evidence *image)
{
	if (!pg_evidence_owned_by(substitution, synthesis->typing)) return (struct pg_synthesis_input){0};
	if (!pg_evidence_owned_by(extension, synthesis->typing)) return (struct pg_synthesis_input){0};
	if (!pg_evidence_owned_by(image, synthesis->typing)) return (struct pg_synthesis_input){0};
	if (pg_evidence_rule(substitution) != PG_CONTEXT_SUBSTITUTION) return (struct pg_synthesis_input){0};
	if (pg_evidence_rule(extension) == PG_CONTEXT_FAMILY_EXTEND) {
		const struct pg_evidence *result = pg_prove_substitution_pair(synthesis->typing, substitution, extension, image);
		return (struct pg_synthesis_input){.checked = result};
	}
	if (pg_evidence_rule(extension) != PG_CONTEXT_EXTEND) return (struct pg_synthesis_input){0};
	if (pg_evidence_judgement(image) != PG_JUDGEMENT_VALUE) return (struct pg_synthesis_input){0};
	if (pg_evidence_context(substitution) != pg_evidence_context(image)) return (struct pg_synthesis_input){0};
	if (pg_evidence_context_map(substitution)->source != pg_evidence_context(extension)->parent) return (struct pg_synthesis_input){0};
	struct pg_synthesis_input type = pg_synthesis_reindex_input(synthesis,
		(struct pg_synthesis_input){.checked = substitution},
		(struct pg_synthesis_input){.checked = pg_context_declared_input(synthesis->typing, extension)});
	struct pg_synthesis_job *checked = pg_synthesis_expect_inputs(synthesis,
		(struct pg_synthesis_input){.checked = image}, type);
	struct pg_synthesis_input premises[] = {
		{.checked = extension}, {.checked = pg_evidence_premise(substitution, 1)},
		{.checked = substitution}, {.pending = pg_synthesis_pending(checked)}};
	struct pg_derivation_input rule = {.rule = PG_CONTEXT_SUBSTITUTION, .count = 4};
	return (struct pg_synthesis_input){.pending = pg_synthesis_pending(
		pg_synthesis_rule_inputs(synthesis, &rule, premises, NULL, NULL))};
}

void pg_synthesis_reindex_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!reindex_inputs(synthesis, substitution, proof)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	struct pg_occurrence_action *action = pg_occurrence_action_request(synthesis->typing,
		pg_evidence_context_map(substitution), pg_evidence_subject(proof));
	if (!action) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
	}
	switch (pg_occurrence_action_advance(action, 1)) {
	case PG_SUBSTITUTION_PENDING:
		pg_synthesis_enqueue(synthesis, job);
		return;
	case PG_SUBSTITUTION_ERROR:
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
		break;
	case PG_SUBSTITUTION_DONE:
		job->result = pg_prove_reindex(synthesis->typing, substitution, proof);
		pg_synthesis_finish(synthesis, job, job->result ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
		break;
	}
}

struct substitution_key {
	struct pg_synthesis_input contexts[2];
	const struct pg_synthesis_input *images;
};

static const void *substitution_operand(const void *input, size_t index)
{
	const struct substitution_key *key = input;
	size_t offset = index / 2;
	struct pg_synthesis_input value =
		offset < 2 ? key->contexts[offset] : key->images[offset - 2];
	return index % 2 ? (const void *)value.pending : value.checked;
}

struct pg_synthesis_job *pg_synthesis_substitution(struct pg_synthesis *synthesis,
	struct pg_synthesis_input source, struct pg_synthesis_input destination,
	size_t count, const struct pg_synthesis_input *images)
{
	if (!pg_synthesis_input_owned(synthesis, source)) return NULL;
	if (!pg_synthesis_input_owned(synthesis, destination)) return NULL;
	if (count && !images) return NULL;
	if (count > (SIZE_MAX / sizeof(void *) - 4) / 2) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_synthesis_input_owned(synthesis, images[i])) return NULL;
	struct substitution_key key = {{source, destination}, images};
	return pg_synthesis_work_request_key(synthesis, SUBSTITUTION_JOB, 4 + count * 2, &key, substitution_operand);
}

static void substitution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct substitution_work *local = pg_synthesis_work_state(job, SUBSTITUTION_JOB);
	size_t count = (job->input_count - 4) / 2;
	if (!local->map.checked && !local->map.pending) {
		const struct pg_evidence *contexts[2];
		for (size_t i = 0; i < 2; ++i) {
			struct pg_synthesis_input input = pg_synthesis_work_dependency(job, 2 * i);
			if (pg_synthesis_await_input(synthesis, job, input)) return;
			contexts[i] = pg_synthesis_input_result(input);
			if (!contexts[i] || pg_evidence_judgement(contexts[i]) != PG_JUDGEMENT_CONTEXT) goto rejected;
		}
		size_t arity;
		const struct pg_evidence *source = contexts[0];
		if (pg_context_extension_size(pg_evidence_context(source), NULL, &arity) || arity != count) goto rejected;
		if (count > SIZE_MAX / sizeof(*local->extensions)) goto error;
		/* Forward dependent checking needs this order only while unfinished. */
		local->extensions = malloc(count * sizeof(*local->extensions));
		if (count && !local->extensions) goto error;
		for (size_t i = count; i; --i, source = pg_context_parent_input(synthesis->typing, source)) local->extensions[i - 1] = source;
		local->map.checked = pg_prove_substitution(synthesis->typing, source, contexts[1], 0, NULL);
		if (!local->map.checked) goto error;
	}
	if (pg_synthesis_await_input(synthesis, job, local->map)) return;
	if (local->next == count) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
		return;
	}
	struct pg_synthesis_input producer = pg_synthesis_work_dependency(job, 4 + 2 * local->next);
	if (pg_synthesis_await_input(synthesis, job, producer)) return;
	const struct pg_evidence *image = pg_synthesis_input_result(producer);
	if (!image || !pg_evidence_subject(image)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_UNSUPPORTED); return;
	}
	if (pg_evidence_judgement(image) == PG_JUDGEMENT_COMPUTATION) {
		if (!local->returned) local->returned = pg_synthesis_return(synthesis, pg_evidence_premise(pg_synthesis_input_result(local->map), 1), image);
		if (pg_synthesis_await(synthesis, job, local->returned)) return;
		image = pg_synthesis_result(local->returned);
	}
	const struct pg_evidence *extension = local->extensions[local->next];
	if (pg_evidence_rule(extension) != PG_CONTEXT_FAMILY_EXTEND) {
		if (pg_evidence_judgement(image) == PG_JUDGEMENT_VALUE_TYPE)
			image = pg_prove_type_value(synthesis->typing, image);
		if (!image || pg_evidence_judgement(image) != PG_JUDGEMENT_VALUE) goto rejected;
	}
	local->map = pg_synthesis_substitution_pair(synthesis, pg_synthesis_input_result(local->map), extension, image);
	local->returned = NULL;
	++local->next;
	if (!pg_synthesis_await_input(synthesis, job, local->map)) pg_synthesis_enqueue(synthesis, job);
	return;
rejected:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}
