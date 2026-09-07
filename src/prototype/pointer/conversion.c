#include "conversion.h"

#include <string.h>
#include <stdlib.h>

struct pg_conversion_certificate {
	const struct pg_term *left;
	const struct pg_term *right;
};

struct pg_conversion_state {
	struct pg_beta_work *work;
	struct pg_graph storage;
	struct pg_index visited;
	struct pg_conversion_task *pending;
	enum pg_conversion_status status;
	uint64_t steps;
	const struct pg_term *left;
	const struct pg_term *right;
	const struct pg_conversion_certificate *certificate;
};

struct conversion_scope {
	const struct pg_object *left;
	const struct pg_object *right;
	const struct conversion_scope *parent;
};

struct pg_conversion_task {
	struct pg_index_entry index;
	const struct pg_term *left;
	const struct pg_term *right;
	const struct conversion_scope *scope;
	struct pg_conversion_task *next;
};

static int push(struct pg_conversion_state *conversion, const struct pg_term *left,
	const struct pg_term *right, const struct conversion_scope *scope)
{
	if (!left || !right) return -1;
	/* Pointer identity is not sufficient under a nontrivial correspondence:
	 * the same binder pointer may be bound on one side and free on the other. */
	if (left == right && !scope) return 0;
	uint64_t hash = ((uintptr_t)left ^ (uintptr_t)right) * UINT64_C(1099511628211);
	hash ^= (uintptr_t)scope;
	for (struct pg_index_entry *candidate = pg_index_candidates(&conversion->visited, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_conversion_task *task = (const struct pg_conversion_task *)candidate;
		if (task->left != left) continue;
		if (task->right != right) continue;
		if (task->scope == scope) return 0;
	}
	struct pg_conversion_task *task = pg_alloc(&conversion->storage, sizeof(*task));
	if (!task) return -1;
	task->left = left;
	task->right = right;
	task->scope = scope;
	task->next = conversion->pending;
	if (pg_index_insert(&conversion->visited, &task->index, hash) != 0) return -1;
	conversion->pending = task;
	return 0;
}

static int initialize(struct pg_conversion_state *conversion, struct pg_beta_work *work,
	const struct pg_term *left, const struct pg_term *right)
{
	memset(conversion, 0, sizeof(*conversion));
	conversion->work = work;
	conversion->left = left;
	conversion->right = right;
	conversion->status = PG_CONVERSION_ERROR;
	if (pg_index_init(&conversion->visited) != 0) return -1;
	if (push(conversion, left, right, NULL) != 0) {
		pg_index_destroy(&conversion->visited);
		pg_graph_destroy(&conversion->storage);
		return -1;
	}
	conversion->status = conversion->pending ? PG_CONVERSION_PENDING : PG_CONVERSION_EQUAL;
	return 0;
}

static int references_equal(const struct pg_object *left, const struct pg_object *right,
	const struct conversion_scope *scope)
{
	for (; scope; scope = scope->parent) {
		if (scope->left == left) return scope->right == right;
		if (scope->right == right) return 0;
	}
	return left == right;
}

/* One transition either advances one normalization or decomposes one pair. */
static enum pg_conversion_status step(struct pg_conversion_state *conversion)
{
	struct pg_conversion_task *task = conversion->pending;
	struct pg_beta_job *left_job = pg_beta_request(conversion->work, task->left);
	struct pg_beta_job *right_job = pg_beta_request(conversion->work, task->right);
	if (!left_job || !right_job) return PG_CONVERSION_ERROR;
	if (pg_beta_status(left_job) == PG_EVAL_PENDING) {
		if (pg_beta_advance(left_job, 1) == PG_EVAL_ERROR) return PG_CONVERSION_ERROR;
		return PG_CONVERSION_PENDING;
	}
	if (pg_beta_status(right_job) == PG_EVAL_PENDING) {
		if (pg_beta_advance(right_job, 1) == PG_EVAL_ERROR) return PG_CONVERSION_ERROR;
		return PG_CONVERSION_PENDING;
	}
	const struct pg_term *left = pg_beta_result(left_job);
	const struct pg_term *right = pg_beta_result(right_job);
	if (!left || !right) return PG_CONVERSION_ERROR;
	if (left->kind != right->kind) return PG_CONVERSION_DIFFERENT;
	conversion->pending = task->next;
	switch (left->kind) {
	case PG_REFERENCE:
		if (!references_equal(left->as.reference, right->as.reference, task->scope)) return PG_CONVERSION_DIFFERENT;
		break;
	case PG_APPLICATION:
		if (push(conversion, left->as.application.argument, right->as.application.argument, task->scope) != 0) return PG_CONVERSION_ERROR;
		if (push(conversion, left->as.application.function, right->as.application.function, task->scope) != 0) return PG_CONVERSION_ERROR;
		break;
	case PG_LAMBDA: {
		struct conversion_scope *scope = pg_alloc(&conversion->storage, sizeof(*scope));
		if (!scope) return PG_CONVERSION_ERROR;
		*scope = (struct conversion_scope){left->as.lambda.binder, right->as.lambda.binder, task->scope};
		if (push(conversion, left->as.lambda.body, right->as.lambda.body, scope) != 0) return PG_CONVERSION_ERROR;
		break;
	}
	}
	return conversion->pending ? PG_CONVERSION_PENDING : PG_CONVERSION_EQUAL;
}

static enum pg_conversion_status advance(struct pg_conversion_state *conversion, uint64_t budget)
{
	while (conversion->status == PG_CONVERSION_PENDING && budget) {
		--budget;
		++conversion->steps;
		conversion->status = step(conversion);
	}
	return conversion->status;
}

static void destroy(struct pg_conversion_state *conversion)
{
	pg_index_destroy(&conversion->visited);
	pg_graph_destroy(&conversion->storage);
	memset(conversion, 0, sizeof(*conversion));
}

static void certify(struct pg_conversion_state *state)
{
	if (state->status != PG_CONVERSION_EQUAL || state->certificate) return;
	struct pg_conversion_certificate *certificate = pg_alloc(state->work->graph, sizeof(*certificate));
	if (!certificate) { state->status = PG_CONVERSION_ERROR; return; }
	*certificate = (struct pg_conversion_certificate){state->left, state->right};
	state->certificate = certificate;
}

int pg_conversion_init(struct pg_conversion *conversion, struct pg_beta_work *work,
	const struct pg_term *left, const struct pg_term *right)
{
	conversion->state = calloc(1, sizeof(*conversion->state));
	if (!conversion->state) return -1;
	if (initialize(conversion->state, work, left, right) != 0) {
		free(conversion->state);
		conversion->state = NULL;
		return -1;
	}
	certify(conversion->state);
	if (conversion->state->status == PG_CONVERSION_ERROR) {
		pg_conversion_destroy(conversion);
		return -1;
	}
	return 0;
}

enum pg_conversion_status pg_conversion_advance(struct pg_conversion *conversion, uint64_t budget)
{
	if (!conversion->state) return PG_CONVERSION_ERROR;
	advance(conversion->state, budget);
	certify(conversion->state);
	return conversion->state->status;
}

void pg_conversion_destroy(struct pg_conversion *conversion)
{
	if (!conversion->state) return;
	destroy(conversion->state);
	free(conversion->state);
	conversion->state = NULL;
}

enum pg_conversion_status pg_conversion_status(const struct pg_conversion *conversion)
{
	return conversion->state ? conversion->state->status : PG_CONVERSION_ERROR;
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
	return conversion->state ? conversion->state->visited.count : 0;
}
const struct pg_term *pg_conversion_left(const struct pg_conversion_certificate *certificate) { return certificate->left; }
const struct pg_term *pg_conversion_right(const struct pg_conversion_certificate *certificate) { return certificate->right; }
