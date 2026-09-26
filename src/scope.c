#include "scope.h"

const struct pg_scope *pg_scope_intern(struct pg_typing *typing,
	const struct pg_context *context, const struct pg_scope *parent,
	const struct pg_scope *indices, const struct pg_occurrence *type)
{
	if (!typing || !context || !type) return NULL;
	if (context->parent != (parent ? parent->context : NULL)) return NULL;
	if (context->indices != (indices ? indices->context : NULL)) return NULL;
	if (type->context != (indices ? indices->context : context->parent)) return NULL;
	if (type->judgement != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	if (!indices && type->core != context->declared_type) return NULL;
	uint64_t hash = (uintptr_t)context;
	hash = (hash ^ (uintptr_t)parent) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)indices) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)type) * UINT64_C(1099511628211);
	if (!typing->scopes.capacity && pg_index_init(&typing->scopes)) return NULL;
	for (struct pg_index_entry *p = pg_index_candidates(&typing->scopes, hash); p; p = p->next) {
		const struct pg_scope *scope = (const void *)p;
		if (p->hash != hash || scope->context != context || scope->parent != parent) continue;
		if (scope->indices == indices && scope->type == type) return scope;
	}
	struct pg_scope *scope = pg_alloc(typing->graph, sizeof(*scope));
	if (!scope) return NULL;
	*scope = (struct pg_scope){.context = context, .parent = parent, .indices = indices, .type = type};
	return pg_index_insert(&typing->scopes, &scope->index, hash) ? NULL : scope;
}
