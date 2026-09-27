#include "typed_query.h"

static size_t state_extent(const struct pg_typed_query_class *role)
{
	size_t alignment = _Alignof(struct pg_typed_query);
	return (role->size + alignment - 1) / alignment * alignment;
}

struct pg_typed_query *pg_typed_query_request(struct pg_typing *typing,
	const struct pg_typed_query_class *role, size_t ordinal,
	const void *const *inputs, int *created)
{
	if (created) *created = 0;
	if (!typing || !role || !role->advance) return NULL;
	if (role->input_count && !inputs) return NULL;
	if (role->input_count > (SIZE_MAX - sizeof(struct pg_typed_query)) / sizeof(*inputs)) return NULL;
	if (role->size > SIZE_MAX - (_Alignof(struct pg_typed_query) - 1)) return NULL;
	uint64_t hash = ((uintptr_t)role ^ ordinal) * UINT64_C(1099511628211);
	for (size_t i = 0; i < role->input_count; ++i)
		hash = (hash ^ (uintptr_t)inputs[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *entry = pg_index_candidates(&typing->typed_queries, hash); entry; entry = entry->next) {
		if (entry->hash != hash) continue;
		struct pg_typed_query *query = (void *)entry;
		if (query->role != role || query->ordinal != ordinal) continue;
		size_t i = 0;
		while (i < role->input_count && query->inputs[i] == inputs[i]) ++i;
		if (i == role->input_count) return query;
	}
	size_t bytes = sizeof(struct pg_typed_query) + role->input_count * sizeof(*inputs);
	size_t offset = state_extent(role);
	if (bytes > SIZE_MAX - offset) return NULL;
	char *storage = pg_alloc(typing->graph, offset + bytes);
	if (!storage) return NULL;
	struct pg_typed_query *query = (void *)(storage + offset);
	query->typing = typing;
	query->role = role;
	query->ordinal = ordinal;
	for (size_t i = 0; i < role->input_count; ++i) query->inputs[i] = inputs[i];
	if (pg_index_insert(&typing->typed_queries, &query->index, hash)) return NULL;
	if (created) *created = 1;
	return query;
}

void *pg_typed_query_state(const struct pg_typed_query *query)
{
	return (char *)query - state_extent(query->role);
}

int pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	if (!work) return -1;
	while (!work->status && budget--) {
		struct pg_typed_query *current = work->waiting ? work->waiting->work : work;
		++work->steps;
		if (!current->status && (!current->dependency || current->dependency->status)) {
			if (current != work) ++current->steps;
			current->status = current->role->advance(current);
		}
		if (current->status) {
			pg_typing_wait_clear(work->typing, &current->waiting);
			pg_typing_wait_pop(work->typing, &work->waiting);
		} else if (current->dependency && !current->dependency->status) {
			if (pg_typing_wait_push(work->typing, &work->waiting, current->dependency))
				work->status = -1;
		}
	}
	if (work->status) pg_typing_wait_clear(work->typing, &work->waiting);
	return work->status;
}

const struct pg_evidence *pg_typed_query_result(const struct pg_typed_query *work)
{
	return work && work->status == 1 ? work->result : NULL;
}

uint64_t pg_typed_query_steps(const struct pg_typed_query *work)
{
	return work ? work->steps : 0;
}
