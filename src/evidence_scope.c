#include "evidence.h"
#include "evidence_structure.h"
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

struct scope_action {
	const struct pg_scope *source;
	const struct pg_context *target;
	const struct pg_evidence *base, *parent;
	struct scope_action *previous;
	unsigned stage;
};

const struct pg_evidence *pg_check_scope_action(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_scope *source,
	const struct pg_context *target)
{
	if (!pg_evidence_owned_by(prefix, typing) || pg_evidence_rule(prefix) != PG_CONTEXT_SUBSTITUTION) return NULL;
	struct pg_graph temporary = {0};
	struct scope_action root = {.source = source, .target = target, .base = prefix}, *frame = &root;
	const struct pg_evidence *result = NULL;
	while (frame) {
		const struct pg_scope *scope = frame->source;
		const struct pg_context *from = scope ? scope->context : NULL, *to = frame->target;
		if (!frame->stage) {
			const struct pg_context_map *base = pg_evidence_context_map(frame->base);
			if (from == base->source) {
				if (to != base->destination) goto fail;
				result = frame->base;
				frame = frame->previous;
				continue;
			}
			if (!from || !to || from->judgement != to->judgement) goto fail;
			struct scope_action *parent = pg_alloc(&temporary, sizeof(*parent));
			if (!parent) goto fail;
			*parent = (struct scope_action){.source = scope->parent, .target = to->parent,
				.base = frame->base, .previous = frame};
			frame->stage = 1;
			frame = parent;
			continue;
		}
		if (frame->stage == 1) {
			frame->parent = result;
			if (scope->indices) {
				struct scope_action *indices = pg_alloc(&temporary, sizeof(*indices));
				if (!indices) goto fail;
				*indices = (struct scope_action){.source = scope->indices, .target = to->indices,
					.base = result, .previous = frame};
				frame->stage = 2;
				frame = indices;
				continue;
			}
		}
		const struct pg_evidence *declaration = pg_evidence_for_scope(typing, scope);
		const struct pg_evidence *type = pg_prove_reindex(typing, result,
			pg_context_declared_input(typing, declaration));
		const struct pg_evidence *parent = pg_evidence_premise(frame->parent, 1);
		const struct pg_evidence *destination = scope->indices
			? pg_prove_family_context_extension(typing, parent, to->binder, pg_evidence_premise(result, 1), type)
			: pg_prove_context_extension(typing, parent, to->binder, type);
		if (!destination || pg_evidence_context(destination) != to) goto fail;
		const struct pg_evidence *image = pg_prove_variable(typing, destination, to->binder);
		result = pg_prove_substitution_extension(typing, declaration, destination, frame->parent, 1, &image);
		if (!result) goto fail;
		frame = frame->previous;
	}
	pg_graph_destroy(&temporary);
	return result;
fail:
	pg_graph_destroy(&temporary);
	return NULL;
}
