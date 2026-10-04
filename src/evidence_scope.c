#include "evidence.h"
#include "evidence_structure.h"
#include "scope.h"
#include "dag.h"
#include "typed_query.h"
#include <stdlib.h>

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
	const struct pg_evidence *parent, *indices;
	struct pg_occurrence_action *action;
	unsigned stage;
};
struct checked_lift { struct pg_context_lift *allocation; };

static int scope_action_step(struct pg_typed_query *work);
static int checked_lift_step(struct pg_typed_query *work);
static const struct pg_typed_query_class SCOPE_ACTION[1] = {{
	.pending = {&pg_typed_query_pending_ops},
	.size = sizeof(struct scope_action), .input_count = 3, .advance = scope_action_step}};
static const struct pg_typed_query_class CONTEXT_LIFT[1] = {{
	.pending = {&pg_typed_query_pending_ops},
	.size = sizeof(struct checked_lift), .input_count = 3, .advance = checked_lift_step}};

static int checked_map(const struct pg_typing *typing, const struct pg_evidence *map)
{
	return pg_evidence_owned_by(map, typing) && pg_evidence_rule(map) == PG_CONTEXT_SUBSTITUTION;
}

static struct pg_typed_query *scope_action_request(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_scope *source,
	const struct pg_context *target)
{
	/* Only accepted immutable inputs may seed a permanent query. Missing
	 * scope admission is not cached as rejection of a later checked scope. */
	if (!checked_map(typing, prefix) || !pg_evidence_for_scope(typing, source)) return NULL;
	const void *inputs[] = {prefix, source, target};
	return pg_typed_query_request(typing, SCOPE_ACTION, 0, inputs, NULL);
}

static int scope_action_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = pg_typed_query_typing(work);
	struct scope_action *local = pg_typed_query_state(work);
	const struct pg_evidence *prefix = work->inputs[0];
	const struct pg_scope *scope = work->inputs[1];
	const struct pg_context *to = work->inputs[2];
	const struct pg_context *from = scope ? scope->context : NULL;
	if (!local->stage) {
		const struct pg_context_map *base = pg_evidence_context_map(prefix);
		if (from == base->source) {
			if (to != base->destination) return -1;
			work->result = prefix;
			return 1;
		}
		if (!from || !to || from->judgement != to->judgement) return -1;
		work->dependency = scope_action_request(typing, prefix, scope->parent, to->parent);
		local->stage = 1;
		return work->dependency ? 0 : -1;
	}
	if (local->stage == 1) {
		local->parent = pg_typed_query_result(work->dependency);
		work->dependency = NULL;
		if (!local->parent) return -1;
		if (scope->indices) {
			work->dependency = scope_action_request(typing, local->parent, scope->indices, to->indices);
			local->stage = 2;
			return work->dependency ? 0 : -1;
		}
		local->indices = local->parent;
		local->stage = 3;
	}
	if (local->stage == 2) {
		local->indices = pg_typed_query_result(work->dependency);
		work->dependency = NULL;
		if (!local->indices) return -1;
		local->stage = 3;
	}
	if (!local->action) local->action = pg_occurrence_action_request(typing,
		pg_evidence_context_map(local->indices), scope->type);
	switch (pg_occurrence_action_advance(local->action, 1)) {
	case PG_SUBSTITUTION_PENDING: return 0;
	case PG_SUBSTITUTION_ERROR: return -1;
	case PG_SUBSTITUTION_DONE: break;
	}
	const struct pg_evidence *declaration = pg_evidence_for_scope(typing, scope);
	const struct pg_evidence *type = pg_prove_reindex(typing, local->indices,
		pg_context_declared_input(typing, declaration));
	const struct pg_evidence *parent = pg_evidence_premise(local->parent, 1);
	const struct pg_evidence *destination = scope->indices
		? pg_prove_family_context_extension(typing, parent, to->binder,
			pg_evidence_premise(local->indices, 1), type)
		: pg_prove_context_extension(typing, parent, to->binder, type);
	if (!destination || pg_evidence_context(destination) != to) return -1;
	const struct pg_evidence *image = pg_prove_variable(typing, destination, to->binder);
	work->result = pg_prove_substitution_extension(typing, declaration, destination, local->parent, 1, (struct pg_evidence_inputs){.owner = &image});
	return work->result ? 1 : -1;
}

const struct pg_evidence *pg_check_scope_action(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_scope *source,
	const struct pg_context *target)
{
	struct pg_typed_query *work = scope_action_request(typing, prefix, source, target);
	while (!pg_typed_query_advance(work, UINT64_MAX)) {}
	return pg_typed_query_result(work);
}

struct pg_typed_query *pg_substitution_lift_request(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_evidence *extension,
	const struct pg_object *binder)
{
	if (!checked_map(typing, prefix) || !pg_evidence_owned_by(extension, typing)) return NULL;
	enum pg_evidence_rule rule = pg_evidence_rule(extension);
	if (rule != PG_CONTEXT_EXTEND && rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	struct pg_context_lift *allocation = pg_context_lift_request(typing,
		pg_evidence_context_map(prefix), pg_evidence_context(extension), binder);
	if (!allocation) return NULL;
	const void *inputs[] = {prefix, pg_evidence_scope(extension), binder};
	int created;
	struct pg_typed_query *work = pg_typed_query_request(typing, CONTEXT_LIFT, 0, inputs, &created);
	if (created) {
		struct checked_lift *local = pg_typed_query_state(work);
		local->allocation = allocation;
	}
	return work;
}

static int checked_lift_step(struct pg_typed_query *work)
{
	if (work->dependency) {
		work->result = pg_typed_query_result(work->dependency);
		return work->result ? 1 : -1;
	}
	struct checked_lift *local = pg_typed_query_state(work);
	switch (pg_context_lift_advance(local->allocation, 1)) {
	case PG_SUBSTITUTION_PENDING: return 0;
	case PG_SUBSTITUTION_ERROR: return -1;
	case PG_SUBSTITUTION_DONE: break;
	}
	const struct pg_context_map *map = pg_context_lift_result(local->allocation);
	work->dependency = scope_action_request(pg_typed_query_typing(work), work->inputs[0], work->inputs[1], map->destination);
	return work->dependency ? 0 : -1;
}

const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_evidence *extension,
	const struct pg_object *binder)
{
	struct pg_typed_query *work = pg_substitution_lift_request(typing, prefix, extension, binder);
	while (!pg_typed_query_advance(work, UINT64_MAX)) {}
	return pg_typed_query_result(work);
}

struct checked_composition {
	struct pg_occurrence_action *action;
	const struct pg_evidence *value;
	size_t composed;
};
static int composition_step(struct pg_typed_query *work);
static const struct pg_typed_query_class CONTEXT_COMPOSE[1] = {{
	.pending = {&pg_typed_query_pending_ops},
	.size = sizeof(struct checked_composition), .input_count = 2, .advance = composition_step}};

struct pg_typed_query *pg_substitution_compose_request(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second)
{
	if (!checked_map(typing, first)) return NULL;
	if (!checked_map(typing, second)) return NULL;
	if (pg_evidence_context(first) != pg_evidence_context(pg_evidence_premise(second, 0))) return NULL;
	const void *inputs[] = {first, second};
	return pg_typed_query_request(typing, CONTEXT_COMPOSE, 0, inputs, NULL);
}

static int composition_step(struct pg_typed_query *work)
{
	struct checked_composition *local = pg_typed_query_state(work);
	struct pg_typing *typing = pg_typed_query_typing(work);
	const struct pg_evidence *first = work->inputs[0], *second = work->inputs[1];
	/* Prefix projections compose without substituting their variable images. */
	if (pg_evidence_premise_count(first) == 2 && pg_evidence_premise_count(second) == 2) {
		work->result = pg_prove_substitution_projection(typing, pg_evidence_premise(first, 0), pg_evidence_premise(second, 1));
		return work->result ? 1 : -1;
	}
	const struct pg_context_map *from = pg_evidence_context_map(first), *to = pg_evidence_context_map(second);
	size_t count = from->count;
	if (local->composed < count) {
		if (pg_evidence_premise_count(first) == 2) {
			if (!pg_substitution_image_at(typing, second, local->composed)) return -1;
		} else {
			if (!local->action) {
				local->value = pg_substitution_image_at(typing, first, local->composed);
				if (!local->value) return -1;
				local->action = pg_occurrence_action_request(typing, to, from->images[local->composed]);
			}
			switch (pg_occurrence_action_advance(local->action, 1)) {
			case PG_SUBSTITUTION_PENDING: return 0;
			case PG_SUBSTITUTION_ERROR: return -1;
			case PG_SUBSTITUTION_DONE: break;
			}
			const struct pg_occurrence *image = pg_occurrence_action_result(local->action);
			/* A checked map acts on a checked image. Do not rediscover its
			 * construction from the transported output's shape. */
			const struct pg_evidence *proof = pg_evidence_for_subject(typing, image, NULL);
			if (!proof) proof = pg_prove_reindex(typing, second, local->value);
			if (!proof || pg_evidence_subject(proof) != image) return -1;
			local->action = NULL;
			local->value = NULL;
		}
		++local->composed;
		return 0;
	}
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return -1;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return -1;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *image = pg_evidence_premise_count(first) == 2 ? to->images[i]
			: pg_occurrence_action_result(pg_occurrence_action_request(typing, to, from->images[i]));
		images[i] = pg_evidence_for_subject(typing, image, NULL);
		if (!images[i]) goto done;
	}
	work->result = pg_prove_substitution(typing, pg_evidence_premise(first, 0), pg_evidence_premise(second, 1), count, images);
done:
	free(images);
	return work->result ? 1 : -1;
}

const struct pg_evidence *pg_prove_substitution_compose(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second)
{
	struct pg_typed_query *work = pg_substitution_compose_request(typing, first, second);
	while (!pg_typed_query_advance(work, UINT64_MAX)) {}
	return pg_typed_query_result(work);
}
