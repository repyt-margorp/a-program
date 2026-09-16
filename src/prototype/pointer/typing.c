#include "typing.h"

#include <string.h>

struct context_entry {
	struct pg_index_entry index;
	struct pg_context context;
};

int pg_typing_init(struct pg_typing *typing, struct pg_graph *graph)
{
	memset(typing, 0, sizeof(*typing));
	typing->graph = graph;
	typing->owner_key = pg_alloc(graph, 1);
	if (!typing->owner_key) return -1;
	if (pg_index_init(&typing->contexts) != 0) goto fail;
	if (pg_index_init(&typing->occurrences) != 0) goto fail;
	if (pg_index_init(&typing->context_maps) != 0) goto fail;
	if (pg_index_init(&typing->occurrence_actions) != 0) goto fail;
	if (pg_index_init(&typing->proofs) != 0) goto fail;
	if (pg_substitution_work_init(&typing->substitutions, graph) == 0) return 0;
fail:
	pg_typing_destroy(typing);
	return -1;
}

void pg_typing_destroy(struct pg_typing *typing)
{
	pg_index_destroy(&typing->contexts);
	pg_index_destroy(&typing->occurrences);
	pg_index_destroy(&typing->context_maps);
	pg_index_destroy(&typing->occurrence_actions);
	pg_index_destroy(&typing->proofs);
	pg_substitution_work_destroy(&typing->substitutions);
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

int pg_context_extension_size(const struct pg_context *context,
	const struct pg_context *prefix, size_t *count)
{
	if (!count) return -1;
	size_t length = 0;
	for (; context != prefix; context = context->parent) {
		if (!context) return -1;
		++length;
	}
	*count = length;
	return 0;
}

static const struct pg_occurrence *occurrence(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands, const struct pg_occurrence *origin,
	const struct pg_context_map *map)
{
	if (!core) return NULL;
	if (judgement < PG_JUDGEMENT_VALUE_TYPE || judgement > PG_JUDGEMENT_INPUT) return NULL;
	if (judgement == PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if ((judgement == PG_JUDGEMENT_INPUT) != (classifier == NULL)) return NULL;
	if (operand_count && !operands) return NULL;
	if (operand_count > (SIZE_MAX - sizeof(struct pg_occurrence)) / sizeof(*operands)) return NULL;
	uint64_t hash = context_hash(context, NULL, core);
	hash = (hash ^ judgement) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)classifier) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)annotation) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)origin) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)map) * UINT64_C(1099511628211);
	hash = (hash ^ operand_count) * UINT64_C(1099511628211);
	for (size_t i = 0; i < operand_count; ++i) {
		if (!operands[i]) return NULL;
		hash = (hash ^ (uintptr_t)operands[i]) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->occurrences, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_occurrence *found = (const struct pg_occurrence *)candidate;
		if (found->judgement != judgement) continue;
		if (found->context != context) continue;
		if (found->core != core) continue;
		if (found->classifier != classifier) continue;
		if (found->annotation != annotation) continue;
		if (found->origin != origin || found->map != map) continue;
		if (found->operand_count != operand_count) continue;
		size_t i = 0;
		while (i < operand_count && found->operands[i] == operands[i]) ++i;
		if (i == operand_count) return found;
	}
	struct pg_occurrence *result = pg_alloc(typing->graph,
		sizeof(*result) + operand_count * sizeof(*operands));
	if (!result) return NULL;
	result->judgement = judgement;
	result->context = context;
	result->core = core;
	result->classifier = classifier;
	result->annotation = annotation;
	result->origin = origin;
	result->map = map;
	result->operand_count = operand_count;
	for (size_t i = 0; i < operand_count; ++i) result->operands[i] = operands[i];
	if (pg_index_insert(&typing->occurrences, &result->index, hash) != 0) return NULL;
	return result;
}

const struct pg_occurrence *pg_occurrence(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands)
{
	return occurrence(typing, judgement, context, core, classifier, annotation,
		operand_count, operands, NULL, NULL);
}

const struct pg_occurrence *pg_occurrence_boundary(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *classifier)
{
	return source ? occurrence(typing, judgement, source->context, source->core,
		classifier, source->annotation, source->operand_count, source->operands,
		source->origin, source->map) : NULL;
}

const struct pg_occurrence *pg_occurrence_mapped(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation,
	const struct pg_occurrence *source, const struct pg_context_map *map)
{
	if (!source || !map || source->context != map->source) return NULL;
	return occurrence(typing, judgement, map->destination, core, classifier,
		annotation, 0, NULL, source, map);
}

const struct pg_binding_value *pg_context_map_bindings(const struct pg_context_map *map)
{
	return (const struct pg_binding_value *)(map->images + map->count);
}

const struct pg_context_map *pg_context_map(struct pg_typing *typing,
	const struct pg_context *source, const struct pg_context *destination,
	size_t count, const struct pg_occurrence *const *images)
{
	size_t length;
	if (pg_context_extension_size(source, NULL, &length) || length != count) return NULL;
	if (count && !images) return NULL;
	size_t stride = sizeof(*images) + sizeof(struct pg_binding_value);
	if (count > (SIZE_MAX - sizeof(struct pg_context_map)) / stride) return NULL;
	uint64_t hash = (uintptr_t)source;
	hash = (hash ^ (uintptr_t)destination) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) {
		if (!images[i] || images[i]->context != destination || !images[i]->classifier) return NULL;
		hash = (hash ^ (uintptr_t)images[i]) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *p = pg_index_candidates(&typing->context_maps, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct pg_context_map *map = (const struct pg_context_map *)p;
		if (map->source != source || map->destination != destination || map->count != count) continue;
		if (!count || !memcmp(map->images, images, count * sizeof(*images))) return map;
	}
	struct pg_context_map *map = pg_alloc(typing->graph, sizeof(*map) + count * stride);
	if (!map) return NULL;
	map->source = source;
	map->destination = destination;
	map->count = count;
	struct pg_binding_value *bindings = (struct pg_binding_value *)(map->images + count);
	const struct pg_context *scope = source;
	for (size_t i = count; i; --i, scope = scope->parent) {
		map->images[i - 1] = images[i - 1];
		bindings[i - 1] = (struct pg_binding_value){scope->binder, images[i - 1]->core};
	}
	return pg_index_insert(&typing->context_maps, &map->index, hash) ? NULL : map;
}

struct pg_occurrence_action {
	struct pg_index_entry index;
	struct pg_typing *typing;
	const struct pg_context_map *map;
	const struct pg_occurrence *source, *result;
	struct pg_substitution *substitution;
	const struct pg_term *outputs[3];
	size_t next;
	uint64_t steps;
	enum pg_substitution_status status;
};

static const struct pg_occurrence *action_result(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source,
	const struct pg_term *core, const struct pg_term *classifier, const struct pg_term *annotation)
{
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	if (source->core->kind == PG_REFERENCE) {
		for (size_t i = 0; i < map->count; ++i) {
			if (bindings[i].binder != source->core->as.reference) continue;
			if (map->images[i]->core != core) return NULL;
			return pg_occurrence_boundary(typing, map->images[i], source->judgement, classifier);
		}
	}
	int identity = map->source == map->destination;
	for (size_t i = 0; identity && i < map->count; ++i)
		identity = bindings[i].value->kind == PG_REFERENCE && bindings[i].value->as.reference == bindings[i].binder;
	return identity ? source : pg_occurrence_mapped(typing,
		source->judgement, core, classifier, annotation, source, map);
}

const struct pg_occurrence *pg_occurrence_projection(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source)
{
	size_t count;
	if (!map || !source || source->context != map->source) return NULL;
	if (pg_context_extension_size(map->destination, map->source, &count)) return NULL;
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	for (size_t i = 0; i < map->count; ++i) {
		if (bindings[i].value->kind != PG_REFERENCE) return NULL;
		if (bindings[i].value->as.reference != bindings[i].binder) return NULL;
	}
	return action_result(typing, map, source, source->core, source->classifier, source->annotation);
}

struct pg_occurrence_action *pg_occurrence_action_request(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source)
{
	if (!map || !source || source->context != map->source || !source->classifier) return NULL;
	uint64_t hash = (UINT64_C(1469598103934665603) ^ (uintptr_t)map) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)source) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->occurrence_actions, hash); p; p = p->next) {
		struct pg_occurrence_action *work = (struct pg_occurrence_action *)p;
		if (p->hash == hash && work->map == map && work->source == source) return work;
	}
	struct pg_occurrence_action *work = pg_alloc(typing->graph, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->map = map;
	work->source = source;
	return pg_index_insert(&typing->occurrence_actions, &work->index, hash) ? NULL : work;
}

static enum pg_substitution_status occurrence_action_step(struct pg_occurrence_action *work)
{
	const struct pg_occurrence *source = work->source;
	const struct pg_context_map *map = work->map;
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	if (work->next < 3) {
		const struct pg_term *inputs[] = {source->core, source->classifier, source->annotation};
		if (!inputs[work->next]) { ++work->next; return PG_SUBSTITUTION_PENDING; }
		if (!work->substitution) {
			work->substitution = pg_substitution_request(&work->typing->substitutions,
				inputs[work->next], map->count, bindings);
			return work->substitution ? PG_SUBSTITUTION_PENDING : PG_SUBSTITUTION_ERROR;
		}
		enum pg_substitution_status status = pg_substitution_advance(work->substitution, 1);
		if (status != PG_SUBSTITUTION_DONE) return status;
		work->outputs[work->next++] = pg_substitution_result(work->substitution);
		work->substitution = NULL;
		return PG_SUBSTITUTION_PENDING;
	}
	work->result = action_result(work->typing, map, source,
		work->outputs[0], work->outputs[1], work->outputs[2]);
	return work->result ? PG_SUBSTITUTION_DONE : PG_SUBSTITUTION_ERROR;
}

enum pg_substitution_status pg_occurrence_action_advance(struct pg_occurrence_action *work,
	uint64_t budget)
{
	if (!work) return PG_SUBSTITUTION_ERROR;
	while (work->status == PG_SUBSTITUTION_PENDING && budget) {
		--budget;
		++work->steps;
		work->status = occurrence_action_step(work);
	}
	return work->status;
}

const struct pg_occurrence *pg_occurrence_action_result(const struct pg_occurrence_action *work)
{
	return work && work->status == PG_SUBSTITUTION_DONE ? work->result : NULL;
}

uint64_t pg_occurrence_action_steps(const struct pg_occurrence_action *work)
{
	return work ? work->steps : 0;
}
