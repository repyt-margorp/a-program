#include "support.h"

struct pg_support {
	struct pg_index_entry index;
	uintptr_t prefix, bit;
	const struct pg_support *left, *right;
};

static const struct pg_support empty;

static const struct pg_support *intern_support(struct pg_graph *graph, uintptr_t prefix,
	uintptr_t bit, const struct pg_support *left, const struct pg_support *right)
{
	uint64_t hash = (prefix ^ bit) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)left) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)right) * UINT64_C(1099511628211);
	if (!graph->supports.capacity && pg_index_init(&graph->supports)) return NULL;
	for (struct pg_index_entry *candidate = pg_index_candidates(&graph->supports, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_support *set = (const struct pg_support *)candidate;
		if (set->prefix != prefix || set->bit != bit) continue;
		if (set->left == left && set->right == right) return set;
	}
	struct pg_support *set = pg_alloc(graph, sizeof(*set));
	if (!set) return NULL;
	*set = (struct pg_support){.prefix = prefix, .bit = bit, .left = left, .right = right};
	if (pg_index_insert(&graph->supports, &set->index, hash)) return NULL;
	return set;
}

static uintptr_t prefix_at(uintptr_t key, uintptr_t bit)
{
	return key & ~(bit | (bit - 1));
}

static const struct pg_support *branch(struct pg_graph *graph, uintptr_t prefix,
	uintptr_t bit, const struct pg_support *left, const struct pg_support *right)
{
	if (!left || !right) return NULL;
	if (left == &empty) return right;
	if (right == &empty) return left;
	return intern_support(graph, prefix, bit, left, right);
}

/* Compressed pointer-key trie: descendants split at strictly lower bits.
 * Adding/removing one binder shares all off-path nodes; no prefix array copy. */
static const struct pg_support *join(struct pg_graph *graph,
	const struct pg_support *left, const struct pg_support *right)
{
	uintptr_t different = left->prefix ^ right->prefix;
	if (!different) return NULL;
	uintptr_t bit = 1;
	while (different >>= 1) bit <<= 1;
	uintptr_t prefix = prefix_at(left->prefix, bit);
	return left->prefix & bit ? branch(graph, prefix, bit, right, left)
		: branch(graph, prefix, bit, left, right);
}

static const struct pg_support *unite(struct pg_graph *graph,
	const struct pg_support *left, const struct pg_support *right)
{
	if (left == right || right == &empty) return left;
	if (left == &empty) return right;
	if (left->bit < right->bit) return unite(graph, right, left);
	if (left->bit > right->bit) {
		if (prefix_at(right->prefix, left->bit) != left->prefix) return join(graph, left, right);
		if (right->prefix & left->bit)
			return branch(graph, left->prefix, left->bit, left->left, unite(graph, left->right, right));
		return branch(graph, left->prefix, left->bit, unite(graph, left->left, right), left->right);
	}
	if (left->prefix != right->prefix) return join(graph, left, right);
	if (!left->bit) return left;
	return branch(graph, left->prefix, left->bit,
		unite(graph, left->left, right->left), unite(graph, left->right, right->right));
}

static const struct pg_support *without(struct pg_graph *graph,
	const struct pg_support *set, uintptr_t key)
{
	if (set == &empty) return set;
	if (!set->bit) return set->prefix == key ? &empty : set;
	if (prefix_at(key, set->bit) != set->prefix) return set;
	if (key & set->bit)
		return branch(graph, set->prefix, set->bit, set->left, without(graph, set->right, key));
	return branch(graph, set->prefix, set->bit, without(graph, set->left, key), set->right);
}

int pg_support_construct(struct pg_graph *graph, struct pg_term *term)
{
	const struct pg_support *left, *right;
	switch (term->kind) {
	case PG_REFERENCE:
		term->support = term->as.reference->kind == PG_BINDER
			? intern_support(graph, (uintptr_t)term->as.reference, 0, NULL, NULL) : &empty;
		break;
	case PG_LAMBDA:
		left = term->as.lambda.body->support;
		if (!left) return 0;
		term->support = without(graph, left, (uintptr_t)term->as.lambda.binder);
		break;
	case PG_APPLICATION:
		left = term->as.application.function->support;
		right = term->as.application.argument->support;
		if (!left || !right) return 0;
		term->support = unite(graph, left, right);
		break;
	}
	return term->support ? 0 : -1;
}

int pg_term_closed(const struct pg_term *term)
{
	if (term->kind == PG_REFERENCE) return term->as.reference->kind == PG_SEMANTIC_OBJECT;
	return term->support == &empty;
}

int pg_support_contains(const struct pg_term *term, const struct pg_object *binder)
{
	if (!term || !binder) return -1;
	if (term->kind == PG_REFERENCE) return term->as.reference == binder && binder->kind == PG_BINDER;
	const struct pg_support *set = term->support;
	if (!set) return -1;
	uintptr_t key = (uintptr_t)binder;
	while (set != &empty && set->bit) {
		if (prefix_at(key, set->bit) != set->prefix) return 0;
		set = key & set->bit ? set->right : set->left;
	}
	return set != &empty && set->prefix == key;
}
