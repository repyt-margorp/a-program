#include "conversion.h"
#include "computation.h"

#include <stdlib.h>

struct pg_conversion_certificate {
	const struct pg_term *left;
	const struct pg_term *right;
};

struct pg_conversion_state {
	struct pg_whnf_work *work;
	struct pg_comparison comparison;
	const struct pg_term *left;
	const struct pg_term *right;
	const struct pg_conversion_certificate *certificate;
	int failed;
};

static int normalize(void *policy, const struct pg_term *input, const struct pg_term **output)
{
	struct pg_whnf_job *job = pg_whnf_request(policy, &pg_pure_policy, input);
	if (!job) return -1;
	if (pg_whnf_status(job) == PG_EVAL_PENDING)
		return pg_whnf_advance(job, 1) == PG_EVAL_ERROR ? -1 : 0;
	*output = pg_whnf_result(job);
	return *output ? 1 : -1;
}

enum pg_conversion_status pg_conversion_status(const struct pg_conversion *conversion)
{
	if (!conversion->state || conversion->state->failed) return PG_CONVERSION_ERROR;
	switch (pg_comparison_status(&conversion->state->comparison)) {
	case PG_COMPARISON_PENDING: return PG_CONVERSION_PENDING;
	case PG_COMPARISON_EQUAL: return PG_CONVERSION_EQUAL;
	case PG_COMPARISON_DIFFERENT: return PG_CONVERSION_DIFFERENT;
	case PG_COMPARISON_ERROR: return PG_CONVERSION_ERROR;
	}
	return PG_CONVERSION_ERROR;
}

static void certify(struct pg_conversion *conversion)
{
	struct pg_conversion_state *state = conversion->state;
	if (pg_conversion_status(conversion) != PG_CONVERSION_EQUAL || state->certificate) return;
	struct pg_conversion_certificate *certificate = pg_alloc(state->work->graph, sizeof(*certificate));
	if (!certificate) { state->failed = 1; return; }
	*certificate = (struct pg_conversion_certificate){state->left, state->right};
	state->certificate = certificate;
}

int pg_conversion_init(struct pg_conversion *conversion, struct pg_whnf_work *work,
	const struct pg_term *left, const struct pg_term *right)
{
	conversion->state = NULL;
	if (!work) return -1;
	struct pg_conversion_state *state = calloc(1, sizeof(*state));
	if (!state) return -1;
	conversion->state = state;
	state->work = work;
	state->left = left;
	state->right = right;
	if (pg_comparison_init(&state->comparison, left, right, work, normalize) != 0) goto fail;
	certify(conversion);
	if (pg_conversion_status(conversion) == PG_CONVERSION_ERROR) goto fail;
	return 0;
fail:
	pg_conversion_destroy(conversion);
	return -1;
}

enum pg_conversion_status pg_conversion_advance(struct pg_conversion *conversion, uint64_t budget)
{
	if (!conversion->state) return PG_CONVERSION_ERROR;
	pg_comparison_advance(&conversion->state->comparison, budget);
	certify(conversion);
	return pg_conversion_status(conversion);
}

void pg_conversion_destroy(struct pg_conversion *conversion)
{
	if (!conversion->state) return;
	pg_comparison_destroy(&conversion->state->comparison);
	free(conversion->state);
	conversion->state = NULL;
}

uint64_t pg_conversion_steps(const struct pg_conversion *conversion)
{
	return conversion->state ? pg_comparison_steps(&conversion->state->comparison) : 0;
}

const struct pg_conversion_certificate *pg_conversion_certificate(const struct pg_conversion *conversion)
{
	return conversion->state ? conversion->state->certificate : NULL;
}

size_t pg_conversion_task_count(const struct pg_conversion *conversion)
{
	return conversion->state ? pg_comparison_task_count(&conversion->state->comparison) : 0;
}

const struct pg_term *pg_conversion_left(const struct pg_conversion_certificate *certificate) { return certificate->left; }
const struct pg_term *pg_conversion_right(const struct pg_conversion_certificate *certificate) { return certificate->right; }
