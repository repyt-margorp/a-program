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
	if (pg_index_init(&typing->context_projections) != 0) goto fail;
	if (pg_index_init(&typing->context_lifts) != 0) goto fail;
	if (pg_index_init(&typing->occurrence_actions) != 0) goto fail;
	if (pg_index_init(&typing->occurrence_inputs) != 0) goto fail;
	if (pg_index_init(&typing->typed_bodies) != 0) goto fail;
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
	pg_index_destroy(&typing->context_projections);
	pg_index_destroy(&typing->context_lifts);
	pg_index_destroy(&typing->occurrence_actions);
	pg_index_destroy(&typing->occurrence_inputs);
	pg_index_destroy(&typing->typed_bodies);
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
	return pg_context_intern(typing, &(struct pg_context){
		.parent = parent, .binder = binder, .declared_type = declared_type, .judgement = judgement});
}

const struct pg_context *pg_context_intern(struct pg_typing *typing,
	const struct pg_context *declaration)
{
	if (!declaration) return NULL;
	const struct pg_context *parent = declaration->parent;
	const struct pg_object *binder = declaration->binder;
	const struct pg_term *declared_type = declaration->declared_type;
	enum pg_evidence_judgement judgement = declaration->judgement;
	if (!binder || !declared_type) return NULL;
	if (binder->kind != PG_BINDER) return NULL;
	if (judgement != PG_JUDGEMENT_VALUE && judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (declaration->indices) {
		size_t count;
		if (judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
		if (pg_context_extension_size(declaration->indices, parent, &count) || !count) return NULL;
	}
	uint64_t hash = context_hash(parent, binder, declared_type);
	hash = (hash ^ judgement) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)declaration->indices) * UINT64_C(1099511628211);
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->contexts, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct context_entry *entry = (const struct context_entry *)candidate;
		if (entry->context.parent != parent) continue;
		if (entry->context.binder != binder) continue;
		if (entry->context.judgement != judgement) continue;
		if (entry->context.indices != declaration->indices) continue;
		if (entry->context.declared_type == declared_type) return &entry->context;
	}
	struct context_entry *entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->context = *declaration;
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

const struct pg_term *pg_context_signature(struct pg_graph *graph,
	const struct pg_context *parent, const struct pg_context *indices,
	const struct pg_term *universe)
{
	const struct pg_term *signature = universe;
	for (const struct pg_context *slot = indices; slot != parent; slot = slot->parent) {
		if (!slot) return NULL;
		signature = pg_pi(graph, slot->declared_type, slot->binder, signature);
		if (!signature) return NULL;
	}
	return signature;
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

const struct pg_occurrence *pg_occurrence_intern(struct pg_typing *typing,
	const struct pg_occurrence *header, const struct pg_occurrence *const *operands,
	const struct pg_context_map *const *maps)
{
	if (!header) return NULL;
	enum pg_evidence_judgement judgement = header->judgement;
	const struct pg_context *context = header->context;
	const struct pg_term *core = header->core, *classifier = header->classifier, *annotation = header->annotation;
	const struct pg_occurrence *origin = header->origin, *type = header->type;
	const struct pg_context_map *map = header->map;
	const struct pg_induction_allocation *induction = header->induction;
	size_t selection = header->selection, operand_count = header->operand_count, map_count = header->map_count;
	if (!core) return NULL;
	if (judgement < PG_JUDGEMENT_VALUE_TYPE || judgement > PG_JUDGEMENT_INPUT) return NULL;
	if (judgement == PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if ((judgement == PG_JUDGEMENT_INPUT) != (classifier == NULL)) return NULL;
	if (type && (type->context != context || type->core != classifier)) return NULL;
	if (operand_count && !operands) return NULL;
	if (selection && (!origin || map || operand_count > 1)) return NULL;
	if (map && (!origin || origin->context != map->source || context != map->destination)) return NULL;
	if (origin && !map && context != origin->context) return NULL;
	if (origin && !selection && operand_count) return NULL;
	if (selection && operand_count && operands[0] && operands[0]->context != context) return NULL;
	if (map_count && !maps) return NULL;
	if (map_count > (SIZE_MAX - sizeof(struct pg_occurrence)) / sizeof(*maps)) return NULL;
	if (operand_count > (SIZE_MAX - sizeof(struct pg_occurrence) - map_count * sizeof(*maps)) / sizeof(*operands)) return NULL;
	size_t size = sizeof(struct pg_occurrence) + operand_count * sizeof(*operands) + map_count * sizeof(*maps);
	if (induction) {
		if (origin || map || (induction->count && !induction->clauses)) return NULL;
		if (!induction->recursion || induction->recursion->kind != PG_BINDER) return NULL;
		if (!induction->argument || induction->argument->kind != PG_BINDER) return NULL;
		if (!induction->self || induction->self->kind != PG_BINDER) return NULL;
		if (size > SIZE_MAX - sizeof(*induction)) return NULL;
		size += sizeof(*induction);
		if (induction->count > (SIZE_MAX - size) / sizeof(*induction->clauses)) return NULL;
		size += induction->count * sizeof(*induction->clauses);
	}
	uint64_t hash = context_hash(context, NULL, core);
	hash = (hash ^ judgement) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)classifier) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)type) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)annotation) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)origin) * UINT64_C(1099511628211);
	hash = (hash ^ selection) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)map) * UINT64_C(1099511628211);
	hash = (hash ^ operand_count) * UINT64_C(1099511628211);
	hash = (hash ^ map_count) * UINT64_C(1099511628211);
	if (induction) {
		hash = (hash ^ (uintptr_t)induction->recursion) * UINT64_C(1099511628211);
		hash = (hash ^ (uintptr_t)induction->argument) * UINT64_C(1099511628211);
		hash = (hash ^ (uintptr_t)induction->self) * UINT64_C(1099511628211);
		hash = (hash ^ induction->count) * UINT64_C(1099511628211);
		for (size_t i = 0; i < induction->count; ++i)
			hash = (hash ^ (uintptr_t)induction->clauses[i]) * UINT64_C(1099511628211);
	}
	for (size_t i = 0; i < map_count; ++i) {
		if (!maps[i] || maps[i]->destination != context) return NULL;
		hash = (hash ^ (uintptr_t)maps[i]) * UINT64_C(1099511628211);
	}
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
		if (found->type != type) continue;
		if (found->annotation != annotation) continue;
		if (found->origin != origin || found->map != map) continue;
		if (found->selection != selection) continue;
		if (found->operand_count != operand_count) continue;
		if (found->map_count != map_count) continue;
		if (!!found->induction != !!induction) continue;
		if (induction) {
			const struct pg_induction_allocation *a = found->induction;
			if (a->recursion != induction->recursion || a->argument != induction->argument ||
				a->self != induction->self || a->count != induction->count) continue;
			size_t j = 0;
			while (j < a->count && a->clauses[j] == induction->clauses[j]) ++j;
			if (j != a->count) continue;
		}
		size_t i = 0;
		while (i < operand_count && found->operands[i] == operands[i]) ++i;
		if (i != operand_count) continue;
		for (i = 0; i < map_count && pg_occurrence_maps(found)[i] == maps[i]; ++i) {}
		if (i == map_count) return found;
	}
	struct pg_occurrence *result = pg_alloc(typing->graph, size);
	if (!result) return NULL;
	*result = *header;
	result->induction = NULL;
	for (size_t i = 0; i < operand_count; ++i) result->operands[i] = operands[i];
	if (map_count) memcpy(result->operands + operand_count, maps, map_count * sizeof(*maps));
	if (induction) {
		struct pg_induction_allocation *a = (void *)((char *)(result->operands + operand_count) + map_count * sizeof(*maps));
		const struct pg_context **clauses = (void *)(a + 1);
		*a = *induction;
		for (size_t i = 0; i < a->count; ++i) clauses[i] = induction->clauses[i];
		a->clauses = clauses;
		result->induction = a;
	}
	if (pg_index_insert(&typing->occurrences, &result->index, hash) != 0) return NULL;
	return result;
}

const struct pg_occurrence *pg_occurrence(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands)
{
	return pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = judgement,
		.context = context, .core = core, .classifier = classifier, .annotation = annotation,
		.operand_count = operand_count}, operands, NULL);
}

const struct pg_occurrence *pg_occurrence_typed(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_occurrence *type, const struct pg_term *annotation,
	size_t operand_count, const struct pg_occurrence *const *operands)
{
	return type ? pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = judgement,
		.context = type->context, .core = core, .classifier = type->core, .type = type,
		.annotation = annotation, .operand_count = operand_count}, operands, NULL) : NULL;
}

const struct pg_occurrence *pg_occurrence_reclassified(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_occurrence *type)
{
	if (!source || !type) return NULL;
	if (source->type == type) return source;
	return pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = source->judgement,
		.context = source->context, .core = source->core, .classifier = type->core,
		.origin = source, .type = type}, NULL, NULL);
}

const struct pg_context_map *const *pg_occurrence_maps(const struct pg_occurrence *subject)
{
	return (const struct pg_context_map *const *)(subject->operands + subject->operand_count);
}

const struct pg_occurrence *pg_occurrence_boundary(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *classifier)
{
	if (!source) return NULL;
	if (source->judgement == judgement && source->classifier == classifier) return source;
	/* Type/value round trips retain the original construction. Evidence still
	 * records both rules; a changed formation is not such a round trip. */
	const struct pg_occurrence *origin = source->origin;
	if (origin && !source->map && !source->selection && source->core == origin->core && source->type == origin->type)
		if (origin->judgement == judgement && origin->classifier == classifier) return origin;
	return pg_occurrence_derived(typing, source, judgement, source->core, classifier);
}

const struct pg_occurrence *pg_occurrence_mapped(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation,
	const struct pg_occurrence *source, const struct pg_context_map *map)
{
	if (!source || !map || source->context != map->source) return NULL;
	return pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = judgement,
		.context = map->destination, .core = core, .classifier = classifier,
		.annotation = annotation, .origin = source, .map = map}, NULL, NULL);
}

const struct pg_occurrence *pg_occurrence_derived(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *core, const struct pg_term *classifier)
{
	return source ? pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = judgement,
		.context = source->context, .core = core, .classifier = classifier, .origin = source,
		.type = source->classifier == classifier ? source->type : NULL}, NULL, NULL) : NULL;
}

const struct pg_occurrence *pg_occurrence_selected(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index, const struct pg_occurrence *argument,
	enum pg_evidence_judgement judgement, const struct pg_term *core, const struct pg_term *classifier)
{
	if (!source || index == SIZE_MAX) return NULL;
	if (argument && argument->context != source->context) return NULL;
	return pg_occurrence_intern(typing, &(struct pg_occurrence){.judgement = judgement,
		.context = source->context, .core = core, .classifier = classifier,
		.origin = source, .selection = index + 1, .operand_count = argument != NULL,
		.type = source->classifier == classifier ? source->type : NULL}, argument ? &argument : NULL, NULL);
}

const struct pg_binding_value *pg_context_map_bindings(const struct pg_context_map *map)
{
	return (const struct pg_binding_value *)(map->images + map->count);
}

const struct pg_occurrence *pg_context_map_image(const struct pg_context_map *map,
	const struct pg_object *binder)
{
	if (!map) return NULL;
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	for (size_t i = 0; i < map->count; ++i)
		if (bindings[i].binder == binder) return map->images[i];
	return NULL;
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

struct context_projection {
	struct pg_index_entry index;
	const struct pg_context_map *map;
};

const struct pg_context_map *pg_context_map_projection(struct pg_typing *typing,
	const struct pg_context *source, const struct pg_context *destination)
{
	uint64_t hash = (uintptr_t)source * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)destination) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->context_projections, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct pg_context_map *map = ((const struct context_projection *)p)->map;
		if (map->source == source && map->destination == destination) return map;
	}
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
	if (!map) return NULL;
	struct context_projection *entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->map = map;
	return pg_index_insert(&typing->context_projections, &entry->index, hash) ? NULL : map;
}

static const struct pg_context_map *context_map_lift_at(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder, const struct pg_term *type, const struct pg_context *indices)
{
	if (!map || !extension || extension->parent != map->source) return NULL;
	if (!binder || binder->kind != PG_BINDER || pg_context_lookup(map->destination, binder)) return NULL;
	if (map->count >= SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_context *destination = pg_context_intern(typing, &(struct pg_context){
		.parent = map->destination, .binder = binder, .declared_type = type,
		.judgement = extension->judgement, .indices = indices});
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

struct lift_wait { struct pg_context_lift *work; struct lift_wait *parent; };

struct pg_context_lift {
	struct pg_index_entry index;
	struct pg_typing *typing;
	const struct pg_context_map *map, *result, *indices_map;
	const struct pg_context *extension;
	const struct pg_object *binder;
	const struct pg_context **indices;
	const struct pg_term *universe;
	size_t count, next;
	struct pg_context_lift *child;
	struct pg_substitution *substitution;
	struct lift_wait *waiting;
	enum pg_substitution_status status;
	uint64_t steps;
};

struct pg_context_lift *pg_context_lift_request(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder)
{
	if (!map || !extension || extension->parent != map->source) return NULL;
	if (!binder || binder->kind != PG_BINDER || pg_context_lookup(map->destination, binder)) return NULL;
	uint64_t hash = (uintptr_t)map * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)extension) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)binder) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->context_lifts, hash); p; p = p->next) {
		struct pg_context_lift *work = (void *)p;
		if (p->hash == hash && work->map == map && work->extension == extension && work->binder == binder) return work;
	}
	struct pg_context_lift *work = pg_alloc(typing->graph, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->map = map;
	work->extension = extension;
	work->binder = binder;
	work->indices_map = map;
	work->universe = extension->declared_type;
	if (extension->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
		if (!extension->indices || pg_context_extension_size(extension->indices, extension->parent, &work->count)) return NULL;
		if (work->count > SIZE_MAX / sizeof(*work->indices)) return NULL;
		work->indices = pg_alloc(typing->graph, work->count * sizeof(*work->indices));
		if (!work->indices) return NULL;
		const struct pg_context *index = extension->indices;
		for (size_t i = work->count; i; --i, index = index->parent) work->indices[i - 1] = index;
		for (size_t i = 0; i < work->count; ++i) {
			const struct pg_term *domain, *body;
			const struct pg_object *bound;
			if (!pg_pi_view(work->universe, &domain, &bound, &body)) return NULL;
			if (domain != work->indices[i]->declared_type || bound != work->indices[i]->binder) return NULL;
			work->universe = body;
		}
	}
	return pg_index_insert(&typing->context_lifts, &work->index, hash) ? NULL : work;
}

static enum pg_substitution_status context_lift_step(struct pg_context_lift *work)
{
	struct pg_typing *typing = work->typing;
	if (work->child) {
		if (work->child->status != PG_SUBSTITUTION_DONE) return work->child->status;
		work->indices_map = work->child->result;
		work->child = NULL;
		++work->next;
	}
	if (work->next < work->count) {
		const struct pg_context *index = work->indices[work->next];
		const struct pg_object *name = index->binder;
		if (pg_context_lookup(work->indices_map->destination, name)) name = pg_binder(typing->graph);
		work->child = pg_context_lift_request(typing, work->indices_map, index, name);
		return work->child ? PG_SUBSTITUTION_PENDING : PG_SUBSTITUTION_ERROR;
	}
	if (!work->substitution) work->substitution = pg_substitution_request(&typing->substitutions,
		work->universe, work->indices_map->count, pg_context_map_bindings(work->indices_map));
	enum pg_substitution_status status = pg_substitution_advance(work->substitution, 1);
	if (status != PG_SUBSTITUTION_DONE) return status;
	const struct pg_term *type = pg_substitution_result(work->substitution);
	const struct pg_context *indices = work->count ? work->indices_map->destination : NULL;
	if (indices) type = pg_context_signature(typing->graph, work->map->destination, indices, type);
	work->result = context_map_lift_at(typing, work->map, work->extension, work->binder, type, indices);
	return work->result ? PG_SUBSTITUTION_DONE : PG_SUBSTITUTION_ERROR;
}

enum pg_substitution_status pg_context_lift_advance(struct pg_context_lift *work, uint64_t budget)
{
	if (!work) return PG_SUBSTITUTION_ERROR;
	while (work->status == PG_SUBSTITUTION_PENDING && budget--) {
		struct pg_context_lift *current = work->waiting ? work->waiting->work : work;
		++work->steps;
		if (current->status == PG_SUBSTITUTION_PENDING) {
			if (current != work) ++current->steps;
			current->status = context_lift_step(current);
		}
		if (current->status != PG_SUBSTITUTION_PENDING) {
			if (work->waiting) work->waiting = work->waiting->parent;
		} else if (current->child && current->child->status == PG_SUBSTITUTION_PENDING) {
			struct lift_wait *wait = pg_alloc(work->typing->graph, sizeof(*wait));
			if (!wait) { work->status = PG_SUBSTITUTION_ERROR; break; }
			*wait = (struct lift_wait){current->child, work->waiting};
			work->waiting = wait;
		}
	}
	return work->status;
}

const struct pg_context_map *pg_context_lift_result(const struct pg_context_lift *work)
{
	return work && work->status == PG_SUBSTITUTION_DONE ? work->result : NULL;
}

const struct pg_context_map *pg_context_lift_indices(const struct pg_context_lift *work)
{
	return pg_context_lift_result(work) && work->count ? work->indices_map : NULL;
}

uint64_t pg_context_lift_steps(const struct pg_context_lift *work)
{
	return work ? work->steps : 0;
}

const struct pg_context_map *pg_context_map_lift(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder)
{
	struct pg_context_lift *work = pg_context_lift_request(typing, map, extension, binder);
	while (pg_context_lift_advance(work, 1024) == PG_SUBSTITUTION_PENDING) {}
	return pg_context_lift_result(work);
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
	if (source->context == destination) return source;
	if (source->core->kind == PG_REFERENCE && source->core->as.reference->kind == PG_BINDER) {
		const struct pg_context *declaration = pg_context_lookup(destination, source->core->as.reference);
		if (declaration && declaration->judgement == source->judgement && declaration->declared_type == source->classifier)
			return pg_occurrence(typing, source->judgement, destination, source->core, source->classifier, NULL, 0, NULL);
	}
	while (source->context != destination) {
		if (!source->map) return NULL;
		/* Only cancel an exact weakening. A converted boundary or a map that
		 * changes images must retain its justification and typed dependencies. */
		if (pg_occurrence_projection(typing, source->map, source->origin) != source) return NULL;
		source = source->origin;
		if (source->context == destination) return source;
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
	const struct pg_context_map *map;
	const struct pg_term *core;
	struct input_map *next;
};

struct input_wait {
	struct pg_occurrence_input *work;
	struct input_wait *parent;
};

struct input_scope {
	const struct pg_context *context;
	struct input_scope *next;
};

struct pg_occurrence_input {
	struct pg_index_entry entry;
	struct pg_typing *typing;
	const struct pg_occurrence *source, *current, *result;
	const struct pg_context_map *outer;
	size_t index;
	struct input_map *maps;
	struct pg_occurrence_action *action;
	struct pg_context_lift *lift;
	struct pg_occurrence_input *selected;
	struct input_wait *waiting;
	struct input_scope *scopes;
	const struct pg_context_map *effective;
	uint64_t steps;
	enum pg_occurrence_input_status status;
};

static struct pg_occurrence_input *input_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index, const struct pg_context_map *map)
{
	if (!source || !source->classifier) return NULL;
	if (map && map->source != source->context) return NULL;
	uint64_t hash = ((uintptr_t)source ^ index) * UINT64_C(1099511628211);
	hash = (hash ^ (uintptr_t)map) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->occurrence_inputs, hash); p; p = p->next) {
		struct pg_occurrence_input *work = (void *)p;
		if (p->hash == hash && work->source == source && work->index == index && work->outer == map) return work;
	}
	struct pg_occurrence_input *work = pg_alloc(typing->graph, sizeof(*work));
	if (!work) return NULL;
	work->typing = typing;
	work->source = work->current = source;
	work->index = index;
	work->outer = map;
	if (map) {
		work->maps = pg_alloc(typing->graph, sizeof(*work->maps));
		if (!work->maps) return NULL;
		*work->maps = (struct input_map){map, source->core, NULL};
	}
	return pg_index_insert(&typing->occurrence_inputs, &work->entry, hash) ? NULL : work;
}

struct pg_occurrence_input *pg_occurrence_input_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index)
{
	return index == SIZE_MAX ? NULL : input_request(typing, source, index, NULL);
}

struct pg_occurrence_input *pg_occurrence_input_mapped_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index, const struct pg_context_map *map)
{
	return map && index != SIZE_MAX ? input_request(typing, source, index, map) : NULL;
}

struct pg_occurrence_input *pg_occurrence_type_request(struct pg_typing *typing,
	const struct pg_occurrence *source)
{
	/* No operand array can have this index; the same scoped edge machinery
	 * handles the classifier without another work graph or source of truth. */
	return input_request(typing, source, SIZE_MAX, NULL);
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

const struct pg_occurrence *pg_occurrence_scoped_input(const struct pg_occurrence *source,
	size_t index)
{
	while (source && source->origin && !source->map && !source->selection && source->core == source->origin->core)
		source = source->origin;
	if (!source || source->origin || index >= source->operand_count) return NULL;
	const struct pg_occurrence *input = source->operands[index];
	const struct pg_context *scope = input->context;
	if (!scope || scope->parent != source->context) return NULL;
	const struct pg_term *body;
	const struct pg_object *binder = input_binder(source->core, index, &body);
	return binder && scope->binder == binder && input->core == body ? input : NULL;
}

/* These declarations are inputs in their own context, not terms in the
 * elimination's destination. Its explicit parameter maps carry the action. */
static int retained_input(const struct pg_occurrence *source, size_t index)
{
	if (source->map_count == 2) return index == 0;
	return source->map_count == 1 && source->operand_count >= 3 && index == source->operand_count - 1;
}

static int motive_input(const struct pg_occurrence *source, size_t index)
{
	if (source->judgement != PG_JUDGEMENT_COMPUTATION || source->map_count != 1) return 0;
	if (source->operand_count < 3 || index != source->operand_count - 2) return 0;
	size_t count;
	return !pg_context_extension_size(source->operands[index]->context, source->context, &count);
}

static enum pg_occurrence_input_status occurrence_input_step(struct pg_occurrence_input *work)
{
	struct pg_typing *typing = work->typing;
	if (work->current) {
		const struct pg_occurrence *current = work->current;
		if (work->index == SIZE_MAX && current->type) {
			work->result = current->type;
			work->current = NULL;
			return PG_INPUT_PENDING;
		}
		if (current->selection && work->index != SIZE_MAX) {
			if (!work->selected) work->selected = input_request(typing, current->origin, current->selection - 1, NULL);
			if (!work->selected) return PG_INPUT_ERROR;
			enum pg_occurrence_input_status status = work->selected->status;
			if (status != PG_INPUT_READY) return status;
			const struct pg_occurrence *child = pg_occurrence_input_result(work->selected);
			if (current->operand_count) {
				if (!work->action) work->action = pg_occurrence_instantiate_request(typing, child, current->operands[0]);
				enum pg_substitution_status action = pg_occurrence_action_advance(work->action, 1);
				if (action == PG_SUBSTITUTION_ERROR) return PG_INPUT_ERROR;
				if (action == PG_SUBSTITUTION_PENDING) return PG_INPUT_PENDING;
				child = pg_occurrence_action_result(work->action);
			} else child = pg_occurrence_unproject(typing, child, current->context);
			if (!child) return PG_INPUT_UNAVAILABLE;
			work->current = child;
			work->selected = NULL;
			work->action = NULL;
			return PG_INPUT_PENDING;
		}
		if (current->map) {
			const struct pg_term *origin = current->origin->core;
			if (work->index != SIZE_MAX && origin->kind == PG_REFERENCE && origin->as.reference->kind == PG_BINDER) {
				const struct pg_occurrence *image = pg_context_map_image(current->map, origin->as.reference);
				if (image) {
					work->current = image;
					return PG_INPUT_PENDING;
				}
			}
			struct input_map *frame = pg_alloc(typing->graph, sizeof(*frame));
			if (!frame) return PG_INPUT_ERROR;
			*frame = (struct input_map){current->map, current->core, work->maps};
			work->maps = frame;
			work->current = current->origin;
		} else {
			if (work->index == SIZE_MAX) {
				if (!current->origin || current->classifier != current->origin->classifier) return PG_INPUT_UNAVAILABLE;
				work->current = current->origin;
				return PG_INPUT_PENDING;
			}
			if (current->origin) {
				if (current->core != current->origin->core) return PG_INPUT_UNAVAILABLE;
				work->current = current->origin;
				return PG_INPUT_PENDING;
			}
			if (work->index >= current->operand_count) return PG_INPUT_UNAVAILABLE;
			work->result = current->operands[work->index];
			if (retained_input(current, work->index)) work->maps = NULL;
			else if (work->result->context != current->context && !motive_input(current, work->index) &&
				!pg_occurrence_scoped_input(current, work->index)) return PG_INPUT_UNAVAILABLE;
			work->current = NULL;
		}
		return PG_INPUT_PENDING;
	}
	if (!work->maps) return PG_INPUT_READY;
	const struct pg_context_map *map = work->maps->map;
	if (!work->effective) {
		for (const struct pg_context *scope = work->result->context; scope != map->source; scope = scope->parent) {
			if (!scope) return PG_INPUT_UNAVAILABLE;
			struct input_scope *frame = pg_alloc(typing->graph, sizeof(*frame));
			if (!frame) return PG_INPUT_ERROR;
			*frame = (struct input_scope){scope, work->scopes};
			work->scopes = frame;
		}
		work->effective = map;
	}
	if (work->scopes) {
		const struct pg_context *scope = work->scopes->context;
		if (!work->lift) {
			const struct pg_term *body;
			const struct pg_object *binder = input_binder(work->maps->core, work->index, &body);
			if (!binder) binder = scope->binder;
			map = work->effective;
			if (pg_context_lookup(map->destination, binder)) binder = pg_binder(typing->graph);
			work->lift = pg_context_lift_request(typing, map, scope, binder);
		}
		enum pg_substitution_status status = pg_context_lift_advance(work->lift, 1);
		if (status == PG_SUBSTITUTION_ERROR) return PG_INPUT_ERROR;
		if (status == PG_SUBSTITUTION_PENDING) return PG_INPUT_PENDING;
		work->effective = pg_context_lift_result(work->lift);
		if (!work->effective) return PG_INPUT_ERROR;
		work->scopes = work->scopes->next;
		work->lift = NULL;
		return PG_INPUT_PENDING;
	}
	if (!work->action) {
		const struct pg_occurrence *projection = pg_occurrence_projection(typing, work->effective, work->result);
		if (projection) {
			work->result = projection;
			work->maps = work->maps->next;
			work->effective = NULL;
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
	}
	return PG_INPUT_PENDING;
}

enum pg_occurrence_input_status pg_occurrence_input_advance(struct pg_occurrence_input *work,
	uint64_t budget)
{
	if (!work) return PG_INPUT_ERROR;
	while (work->status == PG_INPUT_PENDING && budget--) {
		struct pg_occurrence_input *current = work->waiting ? work->waiting->work : work;
		++work->steps;
		if (current->status == PG_INPUT_PENDING) {
			if (current != work) ++current->steps;
			current->status = occurrence_input_step(current);
		}
		/* Shared dependencies can also finish through another caller. Resume
		 * their parent without recursive advancement or restarting the query. */
		if (current->status != PG_INPUT_PENDING) {
			if (work->waiting) work->waiting = work->waiting->parent;
		} else if (current->selected && current->selected->status == PG_INPUT_PENDING) {
			struct input_wait *frame = pg_alloc(work->typing->graph, sizeof(*frame));
			if (!frame) { work->status = PG_INPUT_ERROR; break; }
			*frame = (struct input_wait){current->selected, work->waiting};
			work->waiting = frame;
		}
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
