#include "evidence.h"
#include "scope.h"
#include "dag.h"

const struct pg_evidence *pg_context_parent_input(const struct pg_typing *typing,
	const struct pg_evidence *context)
{
	if (!pg_evidence_owned_by(context, typing)) return NULL;
	const struct pg_scope *scope = pg_evidence_scope(context);
	return scope ? pg_evidence_for_scope(typing, scope->parent) : NULL;
}

const struct pg_evidence *pg_context_indices_input(const struct pg_typing *typing,
	const struct pg_evidence *context)
{
	if (!pg_evidence_owned_by(context, typing)) return NULL;
	const struct pg_scope *scope = pg_evidence_scope(context);
	return scope && scope->indices ? pg_evidence_for_scope(typing, scope->indices) : NULL;
}

const struct pg_evidence *pg_context_declared_input(const struct pg_typing *typing,
	const struct pg_evidence *context)
{
	if (!pg_evidence_owned_by(context, typing)) return NULL;
	const struct pg_scope *scope = pg_evidence_scope(context);
	return scope ? pg_evidence_for_subject(typing, scope->type, NULL) : NULL;
}

static int scope_dependency(void *owner, const void *key, size_t index, const void **child)
{
	(void)index;
	struct pg_typing *typing = owner;
	const struct pg_scope *scope = key;
	if (pg_evidence_for_scope(typing, scope)) return 0;
	const struct pg_evidence *parent = scope->parent
		? pg_evidence_for_scope(typing, scope->parent) : pg_prove_empty_context(typing);
	if (!parent) { *child = scope->parent; return 1; }
	const struct pg_evidence *indices = NULL;
	if (scope->indices) {
		indices = pg_evidence_for_scope(typing, scope->indices);
		if (!indices) { *child = scope->indices; return 1; }
	}
	const struct pg_evidence *type = pg_prove_structural_subject(typing, scope->type);
	if (!type) return -1;
	const struct pg_evidence *result = indices
		? pg_prove_family_context_extension(typing, parent, scope->context->binder, indices, type)
		: pg_prove_context_extension(typing, parent, scope->context->binder, type);
	return result && pg_evidence_scope(result) == scope ? 0 : -1;
}

const struct pg_evidence *pg_prove_scope(struct pg_typing *typing,
	const struct pg_scope *scope)
{
	if (!typing) return NULL;
	if (!scope) return pg_prove_empty_context(typing);
	const struct pg_evidence *result = pg_evidence_for_scope(typing, scope);
	if (result) return result;
	struct pg_dag dag = {0};
	if (!pg_dag_init(&dag, scope_dependency, typing) && !pg_dag_add(&dag, scope))
		result = pg_evidence_for_scope(typing, scope);
	pg_dag_destroy(&dag);
	return result;
}

const struct pg_evidence *pg_prove_scoped_subject(struct pg_typing *typing,
	const struct pg_scope *scope, const struct pg_occurrence *subject)
{
	if (!subject || subject->context != (scope ? scope->context : NULL)) return NULL;
	return pg_prove_scope(typing, scope) ? pg_prove_structural_subject(typing, subject) : NULL;
}
