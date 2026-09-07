#include "typing.h"

#include <string.h>

struct context_entry {
	struct pg_index_entry index;
	struct pg_context context;
};

struct occurrence_entry {
	struct pg_index_entry index;
	const struct pg_occurrence *occurrence;
};

int pg_typing_init(struct pg_typing *typing, struct pg_graph *graph)
{
	memset(typing, 0, sizeof(*typing));
	typing->graph = graph;
	if (pg_index_init(&typing->contexts) != 0) return -1;
	if (pg_index_init(&typing->occurrences) == 0) return 0;
	pg_index_destroy(&typing->contexts);
	return -1;
}

void pg_typing_destroy(struct pg_typing *typing)
{
	pg_index_destroy(&typing->contexts);
	pg_index_destroy(&typing->occurrences);
	memset(typing, 0, sizeof(*typing));
}

static uint64_t context_hash(const struct pg_context *parent,
	const struct pg_object *binder, const struct pg_term *declared_type)
{
	uint64_t hash = (uintptr_t)parent;
	hash = (hash ^ (uintptr_t)binder) * UINT64_C(1099511628211);
	return (hash ^ (uintptr_t)declared_type) * UINT64_C(1099511628211);
}

const struct pg_context *pg_context_bind(struct pg_typing *typing,
	const struct pg_context *parent, const struct pg_object *binder,
	const struct pg_term *declared_type)
{
	if (!binder || !declared_type) return NULL;
	if (binder->kind != PG_BINDER) return NULL;
	uint64_t hash = context_hash(parent, binder, declared_type);
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->contexts, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct context_entry *entry = (const struct context_entry *)candidate;
		if (entry->context.parent != parent) continue;
		if (entry->context.binder != binder) continue;
		if (entry->context.declared_type == declared_type) return &entry->context;
	}
	struct context_entry *entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->context = (struct pg_context){parent, binder, declared_type};
	if (pg_index_insert(&typing->contexts, &entry->index, hash) != 0) return NULL;
	return &entry->context;
}

const struct pg_context *pg_context_lookup(const struct pg_context *context,
	const struct pg_object *binder)
{
	for (; context; context = context->parent) {
		if (context->binder == binder) return context;
	}
	return NULL;
}

const struct pg_occurrence *pg_occurrence(struct pg_typing *typing,
	const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands)
{
	if (!core) return NULL;
	if (operand_count && !operands) return NULL;
	if (operand_count > (SIZE_MAX - sizeof(struct pg_occurrence)) / sizeof(*operands)) return NULL;
	uint64_t hash = context_hash(context, NULL, core);
	hash = (hash ^ (uintptr_t)annotation) * UINT64_C(1099511628211);
	hash = (hash ^ operand_count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < operand_count; ++i) {
		if (!operands[i]) return NULL;
		hash = (hash ^ (uintptr_t)operands[i]) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->occurrences, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_occurrence *found = ((const struct occurrence_entry *)candidate)->occurrence;
		if (found->context != context) continue;
		if (found->core != core) continue;
		if (found->annotation != annotation) continue;
		if (found->operand_count != operand_count) continue;
		size_t i = 0;
		while (i < operand_count && found->operands[i] == operands[i]) ++i;
		if (i == operand_count) return found;
	}
	struct pg_occurrence *result = pg_alloc(typing->graph,
		sizeof(*result) + operand_count * sizeof(*operands));
	if (!result) return NULL;
	result->context = context;
	result->core = core;
	result->annotation = annotation;
	result->operand_count = operand_count;
	for (size_t i = 0; i < operand_count; ++i) result->operands[i] = operands[i];
	struct occurrence_entry *entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->occurrence = result;
	if (pg_index_insert(&typing->occurrences, &entry->index, hash) != 0) return NULL;
	return result;
}
