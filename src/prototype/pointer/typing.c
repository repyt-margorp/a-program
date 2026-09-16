#include "typing.h"
#include "classifier.h"

#include <string.h>
#include <stdlib.h>

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
	if (pg_index_init(&typing->occurrence_inputs) != 0) goto fail;
	if (pg_index_init(&typing->proofs) != 0) goto fail;
	if (pg_index_init(&typing->evidence_conclusions) != 0) goto fail;
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
	pg_index_destroy(&typing->occurrence_inputs);
	pg_index_destroy(&typing->proofs);
	pg_index_destroy(&typing->evidence_conclusions);
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
	const struct pg_term *declared_type, enum pg_evidence_judgement judgement)
{
	if (!binder || !declared_type) return NULL;
	if (binder->kind != PG_BINDER) return NULL;
	if (judgement != PG_JUDGEMENT_VALUE && judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	uint64_t hash = context_hash(parent, binder, declared_type);
	hash = (hash ^ judgement) * UINT64_C(1099511628211);
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->contexts, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct context_entry *entry = (const struct context_entry *)candidate;
		if (entry->context.parent != parent) continue;
		if (entry->context.binder != binder) continue;
		if (entry->context.judgement != judgement) continue;
		if (entry->context.declared_type == declared_type) return &entry->context;
	}
	struct context_entry *entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->context = (struct pg_context){parent, binder, declared_type, judgement};
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

const struct pg_occurrence *pg_occurrence_derived(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *core, const struct pg_term *classifier)
{
	return source ? occurrence(typing, judgement, source->context, core, classifier,
		NULL, 0, NULL, source, NULL) : NULL;
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

const struct pg_context_map *pg_context_map_projection(struct pg_typing *typing,
	const struct pg_context *source, const struct pg_context *destination)
{
	size_t count;
	if (pg_context_extension_size(destination, source, &count)) return NULL;
	if (pg_context_extension_size(source, NULL, &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_occurrence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	const struct pg_context *scope = source;
	for (size_t i = count; i; --i, scope = scope->parent)
		images[i - 1] = pg_occurrence(typing, scope->judgement, destination,
			pg_reference(typing->graph, scope->binder), scope->declared_type, NULL, 0, NULL);
	const struct pg_context_map *map = pg_context_map(typing, source, destination, count, images);
	free(images);
	return map;
}

static const struct pg_context_map *context_map_lift_at(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder, const struct pg_term *type)
{
	if (!map || !extension || extension->parent != map->source) return NULL;
	if (!binder || binder->kind != PG_BINDER || pg_context_lookup(map->destination, binder)) return NULL;
	if (map->count >= SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_context *destination = pg_context_bind(typing, map->destination,
		binder, type, extension->judgement);
	if (!destination) return NULL;
	const struct pg_context_map *projection = pg_context_map_projection(typing, map->destination, destination);
	if (!projection) return NULL;
	const struct pg_occurrence **images = malloc((map->count + 1) * sizeof(*images));
	if (!images) return NULL;
	for (size_t i = 0; i < map->count; ++i)
		images[i] = pg_occurrence_projection(typing, projection, map->images[i]);
	images[map->count] = pg_occurrence(typing, extension->judgement, destination,
		pg_reference(typing->graph, binder), type, NULL, 0, NULL);
	const struct pg_context_map *result = pg_context_map(typing, extension, destination, map->count + 1, images);
	free(images);
	return result;
}

const struct pg_context_map *pg_context_map_lift(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder)
{
	if (!map || !extension || extension->parent != map->source) return NULL;
	const struct pg_term *type = pg_substitution_compute(&typing->substitutions,
		extension->declared_type, map->count, pg_context_map_bindings(map));
	return context_map_lift_at(typing, map, extension, binder, type);
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
			if (map->images[i]->judgement == source->judgement && map->images[i]->classifier == classifier)
				return map->images[i];
			/* Keep the checked substitution's recipe when its classifier has
			 * different binder pointers. A bare boundary loses that premise. */
			return pg_occurrence_mapped(typing, source->judgement, core, classifier,
				annotation, source, map);
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

const struct pg_occurrence *pg_occurrence_unproject(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_context *destination)
{
	size_t count;
	if (!source || pg_context_extension_size(source->context, destination, &count)) return NULL;
	while (source->context != destination) {
		if (!source->map) return NULL;
		/* Only cancel an exact weakening. A converted boundary or a map that
		 * changes images must retain its justification and typed dependencies. */
		if (pg_occurrence_projection(typing, source->map, source->origin) != source) return NULL;
		source = source->origin;
		if (!pg_context_extension_size(destination, source->context, &count))
			return pg_occurrence_projection(typing,
				pg_context_map_projection(typing, source->context, destination), source);
	}
	return source;
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

struct pg_occurrence_action *pg_occurrence_instantiate_request(struct pg_typing *typing,
	const struct pg_occurrence *body, const struct pg_occurrence *argument)
{
	if (!body || !argument || !body->context) return NULL;
	if (body->context->parent != argument->context) return NULL;
	const struct pg_context_map *prefix = pg_context_map_projection(typing,
		argument->context, argument->context);
	if (!prefix || prefix->count >= SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_occurrence **images = malloc((prefix->count + 1) * sizeof(*images));
	if (!images) return NULL;
	memcpy(images, prefix->images, prefix->count * sizeof(*images));
	images[prefix->count] = argument;
	const struct pg_context_map *map = pg_context_map(typing, body->context,
		argument->context, prefix->count + 1, images);
	free(images);
	return pg_occurrence_action_request(typing, map, body);
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

struct input_map {
	const struct pg_occurrence *parent;
	struct input_map *next;
};

struct pg_occurrence_input {
	struct pg_index_entry entry;
	struct pg_typing *typing;
	const struct pg_occurrence *source, *current, *result;
	size_t index;
	struct input_map *maps;
	struct pg_occurrence_action *action;
	struct pg_substitution *domain;
	const struct pg_context_map *effective;
	uint64_t steps;
	enum pg_occurrence_input_status status;
};

struct pg_occurrence_input *pg_occurrence_input_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index)
{
	if (!source || !source->classifier) return NULL;
	uint64_t hash = ((uintptr_t)source ^ index) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->occurrence_inputs, hash); p; p = p->next) {
		struct pg_occurrence_input *work = (void *)p;
		if (p->hash == hash && work->source == source && work->index == index) return work;
	}
	struct pg_occurrence_input *work = pg_alloc(typing->graph, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->source = work->current = source;
	work->index = index;
	return pg_index_insert(&typing->occurrence_inputs, &work->entry, hash) ? NULL : work;
}

/* The semantic owner exposes the same lexical binding carried by Core.
 * This is a scope view, not another executable binder representation. */
static const struct pg_object *input_binder(const struct pg_term *core, size_t index,
	const struct pg_term **body)
{
	if (core->kind == PG_LAMBDA && index == 0) {
		*body = core->as.lambda.body;
		return core->as.lambda.binder;
	}
	const struct pg_term *domain;
	const struct pg_object *binder;
	return index == 1 && pg_pi_view(core, &domain, &binder, body) ? binder : NULL;
}

static enum pg_occurrence_input_status occurrence_input_step(struct pg_occurrence_input *work)
{
	struct pg_typing *typing = work->typing;
	if (work->current) {
		const struct pg_occurrence *current = work->current;
		if (current->map) {
			const struct pg_term *origin = current->origin->core;
			if (origin->kind == PG_REFERENCE && origin->as.reference->kind == PG_BINDER) {
				const struct pg_binding_value *bindings = pg_context_map_bindings(current->map);
				for (size_t i = 0; i < current->map->count; ++i) {
					if (bindings[i].binder != origin->as.reference) continue;
					work->current = current->map->images[i];
					return PG_INPUT_PENDING;
				}
			}
			struct input_map *frame = pg_alloc(typing->graph, sizeof(*frame));
			if (!frame) return PG_INPUT_ERROR;
			*frame = (struct input_map){current, work->maps};
			work->maps = frame;
			work->current = current->origin;
		} else {
			if (current->origin) return PG_INPUT_UNAVAILABLE;
			if (work->index >= current->operand_count) return PG_INPUT_UNAVAILABLE;
			work->result = current->operands[work->index];
			if (work->result->context != current->context) {
				const struct pg_context *scope = work->result->context;
				if (!scope || scope->parent != current->context) return PG_INPUT_UNAVAILABLE;
				const struct pg_term *body;
				if (scope->binder != input_binder(current->core, work->index, &body)) return PG_INPUT_UNAVAILABLE;
				if (work->result->core != body) return PG_INPUT_UNAVAILABLE;
			}
			work->current = NULL;
		}
		return PG_INPUT_PENDING;
	}
	if (!work->maps) return PG_INPUT_READY;
	const struct pg_occurrence *parent = work->maps->parent;
	const struct pg_context_map *map = parent->map;
	if (!work->effective) {
		const struct pg_context *scope = work->result->context;
		if (scope == map->source) work->effective = map;
		else {
			if (!scope || scope->parent != map->source) return PG_INPUT_UNAVAILABLE;
			const struct pg_term *body;
			const struct pg_object *binder = input_binder(parent->core, work->index, &body);
			if (!binder) return PG_INPUT_UNAVAILABLE;
			if (!work->domain) work->domain = pg_substitution_request(&typing->substitutions,
				scope->declared_type, map->count, pg_context_map_bindings(map));
			enum pg_substitution_status status = pg_substitution_advance(work->domain, 1);
			if (status == PG_SUBSTITUTION_ERROR) return PG_INPUT_ERROR;
			if (status == PG_SUBSTITUTION_PENDING) return PG_INPUT_PENDING;
			if (pg_context_lookup(map->destination, binder)) binder = pg_binder(typing->graph);
			work->effective = context_map_lift_at(typing, map, scope,
				binder, pg_substitution_result(work->domain));
			if (!work->effective) return PG_INPUT_ERROR;
		}
	}
	if (!work->action) {
		const struct pg_occurrence *projection = pg_occurrence_projection(typing, work->effective, work->result);
		if (projection) {
			work->result = projection;
			work->maps = work->maps->next;
			work->effective = NULL;
			work->domain = NULL;
			return PG_INPUT_PENDING;
		}
		work->action = pg_occurrence_action_request(typing, work->effective, work->result);
	}
	enum pg_substitution_status status = pg_occurrence_action_advance(work->action, 1);
	if (status == PG_SUBSTITUTION_ERROR) return PG_INPUT_ERROR;
	if (status == PG_SUBSTITUTION_DONE) {
		work->result = pg_occurrence_action_result(work->action);
		work->maps = work->maps->next;
		work->action = NULL;
		work->effective = NULL;
		work->domain = NULL;
	}
	return PG_INPUT_PENDING;
}

enum pg_occurrence_input_status pg_occurrence_input_advance(struct pg_occurrence_input *work,
	uint64_t budget)
{
	if (!work) return PG_INPUT_ERROR;
	while (work->status == PG_INPUT_PENDING && budget--) {
		++work->steps;
		work->status = occurrence_input_step(work);
	}
	return work->status;
}

const struct pg_occurrence *pg_occurrence_input_result(const struct pg_occurrence_input *work)
{
	return work && work->status == PG_INPUT_READY ? work->result : NULL;
}

uint64_t pg_occurrence_input_steps(const struct pg_occurrence_input *work)
{
	return work ? work->steps : 0;
}
