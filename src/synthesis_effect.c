#include "synthesis_effect.h"
#include "synthesis_source.h"
#include "synthesis_work.h"
#include "effect_inference.h"
#include "dag.h"

#include <stdlib.h>

struct contribution_state { struct pg_dag *traversal; int rejected; };
struct substitution_state {
	size_t next;
	struct pg_substitution *work;
	const struct pg_term *result;
};
struct substitution_inputs {
	const struct pg_term *term;
	struct pg_effect_inference *work;
	const struct pg_effect_equation *const *equations;
};

static void contribution_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void row_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void contribution_destroy(struct pg_synthesis_job *);
static void contribution_completed(struct pg_synthesis *, struct pg_synthesis_job *, int);
static void contribute_row_step(struct pg_synthesis *, struct pg_synthesis_job *, const struct pg_term *);
static void substitution_step(struct pg_synthesis *, struct pg_synthesis_job *);

static const struct pg_synthesis_work_class contribution_class = {
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct contribution_state), .advance = contribution_step,
	.destroy = contribution_destroy, .completed = contribution_completed};
static const struct pg_synthesis_work_class row_class = {
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct contribution_state), .advance = row_step,
	.destroy = contribution_destroy, .completed = contribution_completed};
static const struct pg_synthesis_work_class substitution_class = {
	.pending = {&pg_synthesis_pending_ops},
	.size = sizeof(struct substitution_state), .advance = substitution_step};

struct pg_synthesis_job *pg_synthesis_effect_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, struct pg_pending *formation)
{
	if (!work || work->rows != synthesis->typing->graph || !mask) return NULL;
	if (!pg_effect_equation_parameter(work, target)) return NULL;
	struct pg_synthesis_structure structure = pg_synthesis_type_structure_input(synthesis, (struct pg_synthesis_input){.pending = formation});
	if (structure.term) {
		const struct pg_term *row, *value;
		enum pg_totality totality;
		if (!pg_computation_type_spine_view(structure.term, &totality, &row, &value)) return NULL;
		return pg_synthesis_row_contribution(synthesis, work, target, mask, row);
	}
	if (!structure.pending) return NULL;
	const void *inputs[] = {work, target, mask, structure.pending};
	return pg_synthesis_work_request(synthesis, &contribution_class, 4, inputs);
}

struct pg_synthesis_job *pg_synthesis_row_contribution(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work, struct pg_effect_equation *target,
	const struct pg_effect_row *mask, const struct pg_term *row)
{
	if (!work || work->rows != synthesis->typing->graph || !mask || !row) return NULL;
	if (!pg_effect_equation_parameter(work, target)) return NULL;
	const void *inputs[] = {work, target, mask, row};
	return pg_synthesis_work_request(synthesis, &row_class, 4, inputs);
}

static const void *substitution_operand(const void *owner, size_t i)
{
	const struct substitution_inputs *inputs = owner;
	return !i ? (const void *)inputs->term : i == 1 ? (const void *)inputs->work : inputs->equations[i - 2];
}

struct pg_synthesis_job *pg_synthesis_effect_substitution(struct pg_synthesis *synthesis,
	const struct pg_term *term, struct pg_effect_inference *work, size_t count,
	const struct pg_effect_equation *const *equations)
{
	if (!term || (count && !equations) || count > SIZE_MAX / sizeof(void *) - 2) return NULL;
	if (!work || work->rows != synthesis->typing->graph) return NULL;
	for (size_t i = 0; i < count; ++i)
		if (!pg_effect_equation_parameter(work, equations[i])) return NULL;
	const struct substitution_inputs inputs = {term, work, equations};
	return pg_synthesis_work_request_key(synthesis, &substitution_class, count + 2, &inputs, substitution_operand);
}

const struct pg_term *pg_synthesis_effect_substitution_result(const struct pg_synthesis_job *job)
{
	const struct substitution_state *state = pg_synthesis_work_state(job, &substitution_class);
	return state && pg_synthesis_status(job) == PG_SYNTHESIS_DONE ? state->result : NULL;
}

/* Registration must finish before row solving. Completed requests remain
 * reusable after sealing, but an unfinished contribution cannot add edges. */
static int open_equation(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_effect_inference *work)
{
	if (work->failed) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return 0; }
	if (work->sealed) { pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return 0; }
	return 1;
}

static void contribution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_effect_inference *work = (void *)pg_synthesis_work_input(job, 0);
	struct pg_synthesis_job *structure = (void *)pg_synthesis_work_input(job, 3);
	if (!open_equation(synthesis, job, work)) return;
	if (pg_synthesis_await(synthesis, job, structure)) return;
	const struct pg_term *row, *value;
	enum pg_totality totality;
	if (!pg_computation_type_spine_view(pg_synthesis_type_structure_result((struct pg_synthesis_structure){.pending = structure}), &totality, &row, &value)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	contribute_row_step(synthesis, job, row);
}

static void contribution_destroy(struct pg_synthesis_job *job)
{
	struct contribution_state *state = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (state->traversal) {
		pg_dag_destroy(state->traversal);
		free(state->traversal);
		state->traversal = NULL;
	}
}

static void contribution_completed(struct pg_synthesis *synthesis, struct pg_synthesis_job *job, int first)
{
	(void)synthesis; (void)first;
	contribution_destroy(job);
}

static int contribution_child(void *owner, const void *key, size_t index, const void **child)
{
	struct pg_synthesis_job *job = owner;
	const struct pg_term *left, *right;
	if (pg_effect_join_view(key, &left, &right)) {
		if (index >= 2) return 0;
		*child = index ? right : left;
		return 1;
	}
	struct pg_effect_inference *work = (void *)job->inputs[0];
	if (pg_effect_contribution(work, key, job->inputs[2], (void *)job->inputs[1])) {
		struct contribution_state *state = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
		state->rejected = !work->failed;
		return -1;
	}
	return 0;
}

static void contribute_row_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job,
	const struct pg_term *row)
{
	struct contribution_state *state = pg_synthesis_work_state(job, pg_synthesis_work_role(job));
	if (!state->traversal) {
		const struct pg_term *left, *right;
		if (!pg_effect_join_view(row, &left, &right)) {
			struct pg_effect_inference *work = (void *)job->inputs[0];
			int status = pg_effect_contribution(work, row, job->inputs[2], (void *)job->inputs[1]);
			pg_synthesis_finish(synthesis, job, !status ? PG_SYNTHESIS_DONE
				: work->failed ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_REJECTED);
			return;
		}
		state->traversal = malloc(sizeof(*state->traversal));
		if (!state->traversal || pg_dag_init(state->traversal, contribution_child, job)) {
			pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR); return;
		}
	}
	int status = pg_dag_advance(state->traversal, row, 1);
	if (!status) { pg_synthesis_enqueue(synthesis, job); return; }
	pg_synthesis_finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE
		: state->rejected ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_ERROR);
}

static void row_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	if (!open_equation(synthesis, job, (void *)job->inputs[0])) return;
	contribute_row_step(synthesis, job, job->inputs[3]);
}

static struct pg_binding_value substitution_binding(const void *owner, size_t i)
{
	const struct pg_synthesis_job *job = owner;
	struct pg_effect_inference *inference = (void *)job->inputs[1];
	const struct pg_effect_equation *equation = job->inputs[i + 2];
	return (struct pg_binding_value){pg_effect_equation_parameter(inference, equation),
		pg_effect_reference(inference->rows, pg_effect_inference_result(inference, equation))};
}

static void substitution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_effect_inference *inference = (void *)pg_synthesis_work_input(job, 1);
	if (pg_synthesis_await_effects(synthesis, job, inference)) return;
	size_t count = pg_synthesis_work_input_count(job) - 2;
	struct substitution_state *state = pg_synthesis_work_state(job, &substitution_class);
	if (state->next < count) {
		if (!substitution_binding(job, state->next).value) goto error;
		++state->next;
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (!state->work) {
		state->work = pg_substitution_request(&synthesis->typing->substitutions, pg_synthesis_work_input(job, 0), count,
			(struct pg_binding_inputs){.owner = job, .at = substitution_binding});
		if (!state->work) goto error;
	}
	enum pg_substitution_status status = pg_substitution_advance(state->work, 1);
	if (status == PG_SUBSTITUTION_PENDING) { pg_synthesis_enqueue(synthesis, job); return; }
	if (status != PG_SUBSTITUTION_DONE) goto error;
	state->result = pg_substitution_result(state->work);
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
	return;
error:
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_ERROR);
}
