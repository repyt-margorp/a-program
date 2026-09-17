#include "conversion.h"
#include "computation.h"
#include "classifier.h"
#include "iadt.h"

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
	/* Borrow the shared job while a comparison endpoint is suspended. */
	const struct pg_term *normalizing;
	struct {
		struct pg_whnf_job *whnf;
		struct pg_nf_job *nf;
	} normalization;
	uint64_t steps;
	size_t initial_tasks;
	int strong;
	int failed;
};

/* These eliminators cannot reduce while their demanded operand is open.
 * Follow only the demanded operand, never recursive branch bodies. */
static int neutral(const struct pg_term *term)
{
	for (;;) {
		const struct pg_term *first = NULL;
		while (term->kind == PG_APPLICATION) {
			first = term->as.application.argument;
			term = term->as.application.function;
		}
		if (term->kind != PG_REFERENCE) return 0;
		const struct pg_object *head = term->as.reference;
		if (head->kind == PG_BINDER) return 1;
		if (!first) return 0;
		if (!pg_data_layout_view(head) && head != &pg_total_result_operation &&
			head != &pg_force_operation) return 0;
		term = first;
	}
}

/* Congruence compares children of rigid heads without normalizing unrelated
 * siblings. Reducible oracle applications still need parent rechecking. */
static int rigid_head(const struct pg_term *term)
{
	if (term->kind == PG_LAMBDA || neutral(term)) return 1;
	while (term->kind == PG_APPLICATION) term = term->as.application.function;
	if (term->kind != PG_REFERENCE) return 0;
	const struct pg_object *head = term->as.reference;
	if (head->kind == PG_BINDER) return 1;
	if (pg_classifier_rigid(head)) return 1;
	if (pg_data_declaration_view(head)) return 1;
	const struct pg_data_layout *layout;
	size_t position, arity;
	if (pg_data_constructor_view(head, &layout, &position, &arity)) return 1;
	return head == &pg_return_operation || head == &pg_request_operation;
}

static int normalize(void *policy, const struct pg_term *input, const struct pg_term **output)
{
	struct pg_conversion_state *state = policy;
	if (state->normalizing != input) {
		state->normalization.whnf = pg_whnf_request(state->work, &pg_pure_policy, input);
		state->normalization.nf = NULL;
		state->normalizing = input;
	}
	struct pg_whnf_job *job = state->normalization.whnf;
	if (!job) return -1;
	if (pg_whnf_status(job) == PG_EVAL_PENDING)
		return pg_whnf_advance(job, 1) == PG_EVAL_ERROR ? -1 : 0;
	*output = pg_whnf_result(job);
	if (!*output) return -1;
	if (!state->strong || rigid_head(*output)) return 1;
	/* A suspended lambda cannot contract by thunk/force eta. Expose its
	 * head without normalizing recursive code under the lambda binder. */
	if ((*output)->kind == PG_APPLICATION &&
		(*output)->as.application.function == pg_reference(state->work->graph, &pg_thunk_operation)) {
		struct pg_whnf_job *body = pg_whnf_request(state->work, &pg_pure_policy,
			(*output)->as.application.argument);
		if (!body) return -1;
		if (pg_whnf_status(body) == PG_EVAL_PENDING)
			return pg_whnf_advance(body, 1) == PG_EVAL_ERROR ? -1 : 0;
		const struct pg_term *head = pg_whnf_result(body);
		if (!head) return -1;
		if (head->kind == PG_LAMBDA) {
			*output = pg_application(state->work->graph,
				(*output)->as.application.function, head);
			return *output ? 1 : -1;
		}
	}
	if (!state->normalization.nf)
		state->normalization.nf = pg_nf_request(state->work, &pg_pure_policy, *output);
	struct pg_nf_job *nf = state->normalization.nf;
	if (!nf) return -1;
	if (pg_nf_status(nf) == PG_NF_PENDING)
		return pg_nf_advance(nf, 1) == PG_NF_ERROR ? -1 : 0;
	*output = pg_nf_result(nf);
	return *output ? 1 : -1;
}

enum pg_conversion_status pg_conversion_status(const struct pg_conversion *conversion)
{
	if (!conversion->state || conversion->state->failed) return PG_CONVERSION_ERROR;
	switch (pg_comparison_status(&conversion->state->comparison)) {
	case PG_COMPARISON_PENDING: return PG_CONVERSION_PENDING;
	case PG_COMPARISON_EQUAL: return PG_CONVERSION_EQUAL;
	case PG_COMPARISON_DIFFERENT:
		return conversion->state->strong ? PG_CONVERSION_DIFFERENT : PG_CONVERSION_PENDING;
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
	if (pg_comparison_init(&state->comparison, left, right, state, normalize) != 0) goto fail;
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
	struct pg_conversion_state *state = conversion->state;
	while (pg_conversion_status(conversion) == PG_CONVERSION_PENDING && budget) {
		--budget;
		++state->steps;
		if (pg_comparison_status(&state->comparison) == PG_COMPARISON_DIFFERENT) {
			state->initial_tasks = pg_comparison_task_count(&state->comparison);
			pg_comparison_destroy(&state->comparison);
			state->strong = 1;
			state->normalizing = NULL;
			if (pg_comparison_init(&state->comparison, state->left, state->right, state, normalize) != 0)
				state->failed = 1;
		} else pg_comparison_advance(&state->comparison, 1);
	}
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
	return conversion->state ? conversion->state->steps : 0;
}

const struct pg_conversion_certificate *pg_conversion_certificate(const struct pg_conversion *conversion)
{
	return conversion->state ? conversion->state->certificate : NULL;
}

size_t pg_conversion_task_count(const struct pg_conversion *conversion)
{
	return conversion->state ? conversion->state->initial_tasks + pg_comparison_task_count(&conversion->state->comparison) : 0;
}

const struct pg_term *pg_conversion_left(const struct pg_conversion_certificate *certificate) { return certificate->left; }
const struct pg_term *pg_conversion_right(const struct pg_conversion_certificate *certificate) { return certificate->right; }

const struct pg_conversion_certificate *pg_conversion_substitution(
	struct pg_substitution_work *work, const struct pg_term *left,
	const struct pg_term *right, const struct pg_term *body, size_t count,
	const struct pg_binding_value *bindings,
	const struct pg_reduction_certificate *const *reductions)
{
	if (!work || !left || !right || !body || (count && (!bindings || !reductions))) return NULL;
	if (count > SIZE_MAX / sizeof(*bindings)) return NULL;
	struct pg_binding_value *images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	struct pg_conversion_certificate *result = NULL;
	for (size_t i = 0; i < count; ++i) {
		images[i] = bindings[i];
		if (!reductions[i]) continue;
		if (pg_reduction_policy(reductions[i]) != &pg_pure_policy ||
			pg_alpha_equal(bindings[i].value, pg_reduction_source(reductions[i])) != 1) goto done;
		images[i].value = pg_reduction_target(reductions[i]);
	}
	if (pg_alpha_equal(left, pg_substitution_compute(work, body, count, bindings)) != 1 ||
		pg_alpha_equal(right, pg_substitution_compute(work, body, count, images)) != 1) goto done;
	result = pg_alloc(work->graph, sizeof(*result));
	if (result) *result = (struct pg_conversion_certificate){left, right};
done:
	free(images);
	return result;
}
