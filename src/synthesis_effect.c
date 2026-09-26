#include "synthesis_effect.h"
#include "synthesis_work.h"
#include "effect_inference.h"

#include <stdlib.h>

struct inference_state { int waiting; };
struct contribution_state { struct pg_synthesis_job *row; };
struct row_state { struct pg_synthesis_job *children[2]; };
struct substitution_state {
	size_t next;
	struct pg_binding_value *bindings;
	struct pg_substitution *work;
	const struct pg_term *result;
};

static void inference_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void contribution_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void row_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void substitution_step(struct pg_synthesis *, struct pg_synthesis_job *);
static void substitution_destroy(struct pg_synthesis_job *);

static const struct pg_synthesis_work_class inference_class = {
	.size = sizeof(struct inference_state), .advance = inference_step};
static const struct pg_synthesis_work_class contribution_class = {
	.size = sizeof(struct contribution_state), .advance = contribution_step};
static const struct pg_synthesis_work_class row_class = {
	.size = sizeof(struct row_state), .advance = row_step};
static const struct pg_synthesis_work_class substitution_class = {
	.size = sizeof(struct substitution_state), .advance = substitution_step, .destroy = substitution_destroy};

struct pg_effect_inference *pg_synthesis_effect_worker(const struct pg_synthesis_job *job)
{
	return pg_synthesis_work_state(job, &inference_class) ? (void *)pg_synthesis_work_input(job, 0) : NULL;
}

struct pg_synthesis_job *pg_synthesis_effect_inference(struct pg_synthesis *synthesis,
	struct pg_effect_inference *work)
{
	if (!work || work->rows != synthesis->typing->graph) return NULL;
	const void *inputs[] = {work};
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, &inference_class, 1, inputs);
	struct inference_state *state = pg_synthesis_work_state(job, &inference_class);
	if (state && state->waiting && (work->sealed || work->failed)) {
		state->waiting = 0;
		pg_synthesis_enqueue(synthesis, job);
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
	if (!inputs) { pg_graph_destroy(&temporary); return NULL; }
	inputs[0] = term; inputs[1] = effects;
	for (size_t i = 0; i < count; ++i) inputs[i + 2] = equations[i];
	struct pg_synthesis_job *job = pg_synthesis_work_request(synthesis, &substitution_class, count + 2, inputs);
	pg_graph_destroy(&temporary);
	return job;
}

const struct pg_term *pg_synthesis_effect_substitution_result(const struct pg_synthesis_job *job)
{
	const struct substitution_state *state = pg_synthesis_work_state(job, &substitution_class);
	return state && pg_synthesis_status(job) == PG_SYNTHESIS_DONE ? state->result : NULL;
}

static void inference_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_effect_inference *work = pg_synthesis_effect_worker(job);
	struct inference_state *state = pg_synthesis_work_state(job, &inference_class);
	if (!work->sealed && !work->failed) { state->waiting = 1; return; }
	int status = pg_effect_inference_advance(work, 1);
	if (!status) pg_synthesis_enqueue(synthesis, job);
	else pg_synthesis_finish(synthesis, job, status > 0 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_ERROR);
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
	if (!pg_computation_type_spine_view(pg_synthesis_type_structure_result(structure), &totality, &row, &value)) {
		pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_REJECTED); return;
	}
	struct contribution_state *state = pg_synthesis_work_state(job, &contribution_class);
	if (!state->row) state->row = pg_synthesis_row_contribution(synthesis, work,
		(void *)pg_synthesis_work_input(job, 1), pg_synthesis_work_input(job, 2), row);
	if (pg_synthesis_await(synthesis, job, state->row)) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void row_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_effect_inference *work = (void *)pg_synthesis_work_input(job, 0);
	struct pg_effect_equation *target = (void *)pg_synthesis_work_input(job, 1);
	const struct pg_effect_row *mask = pg_synthesis_work_input(job, 2);
	const struct pg_term *row = pg_synthesis_work_input(job, 3), *left, *right;
	if (!open_equation(synthesis, job, work)) return;
	if (!pg_effect_join_view(row, &left, &right)) {
		int status = pg_effect_contribution(work, row, mask, target);
		pg_synthesis_finish(synthesis, job, !status ? PG_SYNTHESIS_DONE
			: work->failed ? PG_SYNTHESIS_ERROR : PG_SYNTHESIS_REJECTED);
		return;
	}
	struct row_state *state = pg_synthesis_work_state(job, &row_class);
	if (!state->children[0]) {
		state->children[0] = pg_synthesis_row_contribution(synthesis, work, target, mask, left);
		state->children[1] = pg_synthesis_row_contribution(synthesis, work, target, mask, right);
	}
	for (size_t i = 0; i < 2; ++i)
		if (pg_synthesis_await(synthesis, job, state->children[i])) return;
	pg_synthesis_finish(synthesis, job, PG_SYNTHESIS_DONE);
}

static void substitution_destroy(struct pg_synthesis_job *job)
{
	struct substitution_state *state = pg_synthesis_work_state(job, &substitution_class);
	free(state->bindings);
}

static void substitution_step(struct pg_synthesis *synthesis, struct pg_synthesis_job *job)
{
	struct pg_synthesis_job *effects = (void *)pg_synthesis_work_input(job, 1);
	if (pg_synthesis_await(synthesis, job, effects)) return;
	struct pg_effect_inference *inference = pg_synthesis_effect_worker(effects);
	size_t count = pg_synthesis_work_input_count(job) - 2;
	struct substitution_state *state = pg_synthesis_work_state(job, &substitution_class);
	if (!state->bindings && !state->work && count) {
		if (count > SIZE_MAX / sizeof(*state->bindings)) goto error;
		state->bindings = malloc(count * sizeof(*state->bindings));
		if (!state->bindings) goto error;
	}
	if (state->next < count) {
		const struct pg_effect_equation *equation = pg_synthesis_work_input(job, state->next + 2);
		const struct pg_effect_row *row = pg_effect_inference_result(inference, equation);
		const struct pg_term *value = pg_effect_reference(synthesis->typing->graph, row);
		if (!value) goto error;
		state->bindings[state->next++] = (struct pg_binding_value){pg_effect_equation_parameter(inference, equation), value};
		pg_synthesis_enqueue(synthesis, job);
		return;
	}
	if (!state->work) {
		state->work = pg_substitution_request(&synthesis->typing->substitutions, pg_synthesis_work_input(job, 0), count, state->bindings);
		free(state->bindings); state->bindings = NULL;
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
