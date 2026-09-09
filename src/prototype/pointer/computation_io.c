#include "computation_io.h"
#include "computation.h"
#include "computation_internal.h"
#include "symmetry_internal.h"
#include "identity_internal.h"
#include "eval_internal.h"
#include "dag.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGCON\2";
static const char fold_magic[8] = "APGFLD\1";
static const char symmetry_magic[8] = "APGSYM\1";
static const char prefix_magic[8] = "APGPRF\1";

static int write_axes(FILE *file, const char *magic, size_t capacity, size_t position, const size_t *axes)
{
	if (position > capacity || (position && !axes)) return -1;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, capacity)
		|| pg_wire_write_u64(file, position)) return -1;
	for (size_t i = 0; i < position; ++i)
		if (axes[i] >= capacity || pg_wire_write_u64(file, axes[i])) return -1;
	return 0;
}

static int read_axes(FILE *file, struct pg_graph *arena, const char *magic, size_t limit,
	size_t *capacity, size_t *position, size_t **axes)
{
	char header[8];
	uint64_t size, used;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)
		|| pg_wire_read_u64(file, &size) || pg_wire_read_u64(file, &used)) return -1;
	if (size > limit || size > SIZE_MAX / sizeof(size_t) || used > size) return -1;
	size_t *data = pg_alloc(arena, (size_t)size * sizeof(*data));
	if (!data) return -1;
	for (size_t i = 0; i < used; ++i) {
		uint64_t axis;
		if (pg_wire_read_u64(file, &axis) || axis >= size) return -1;
		data[i] = (size_t)axis;
	}
	*capacity = (size_t)size;
	*position = (size_t)used;
	*axes = data;
	return 0;
}

int pg_symmetry_prefix_write(FILE *file, const struct prefix_work *work,
	size_t count, const struct pg_eval_configuration *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || !work->outer || (count && !roots)) return -1;
	size_t dimension = work->outer->dimension - work->outer->fixed_prefix;
	if (count > SIZE_MAX / sizeof(struct pg_eval_configuration) - 2) return -1;
	struct pg_graph temporary;
	if (pg_graph_init(&temporary)) return -1;
	int status = -1;
	struct pg_eval_configuration *all = pg_alloc(&temporary, (count + 2) * sizeof(*all));
	if (!all) goto done;
	all[0].head.term = pg_reference(&temporary, &work->outer->base.object);
	all[1].head = work->argument;
	for (size_t i = 0; i < count; ++i) all[i + 2] = roots[i];
	if (write_axes(file, prefix_magic, dimension, work->position, work->axes)) goto done;
	status = pg_eval_configurations_write(file, count + 2, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_symmetry_prefix_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct prefix_work **work, size_t *count, const struct pg_eval_configuration **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	struct prefix_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	size_t capacity;
	if (read_axes(file, arena, prefix_magic, limit, &capacity, &candidate->position, &candidate->axes)) return -1;
	size_t total;
	const struct pg_eval_configuration *all;
	if (pg_eval_configurations_read(file, output, limit, name_limit, codec, owner, &total, &all) || total < 2) return -1;
	if (all[0].head.environment || all[0].arguments || all[1].arguments || all[0].head.term->kind != PG_REFERENCE) return -1;
	candidate->outer = pg_symmetry_owner(all[0].head.term->as.reference);
	if (!candidate->outer || capacity != candidate->outer->dimension - candidate->outer->fixed_prefix) return -1;
	candidate->argument = all[1].head;
	*work = candidate;
	*count = total - 2;
	*roots = all + 2;
	return 0;
}

int pg_symmetry_work_write(FILE *file, const struct composition_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || !work->outer || !work->inner || (count && !roots)) return -1;
	size_t dimension = work->outer->dimension > work->inner->dimension ? work->outer->dimension : work->inner->dimension;
	if (work->dimension != dimension || work->position > dimension || (work->position && !work->axes)) return -1;
	if (count > SIZE_MAX / sizeof(const struct pg_term *) - 3) return -1;
	struct pg_graph temporary;
	if (pg_graph_init(&temporary)) return -1;
	int status = -1;
	const struct pg_term **all = pg_alloc(&temporary, (count + 3) * sizeof(*all));
	if (!all) goto done;
	all[0] = pg_reference(&temporary, &work->outer->base.object);
	all[1] = pg_reference(&temporary, &work->inner->base.object);
	all[2] = work->argument;
	for (size_t i = 0; i < count; ++i) all[i + 3] = roots[i];
	if (write_axes(file, symmetry_magic, dimension, work->position, work->axes)) goto done;
	status = pg_graph_write_descriptors(file, count + 3, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_symmetry_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct composition_work **work, size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	struct composition_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	size_t capacity;
	if (read_axes(file, arena, symmetry_magic, limit, &capacity, &candidate->position, &candidate->axes)) return -1;
	size_t total;
	const struct pg_term *const *all;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &total, &all) || total < 3) return -1;
	if (all[0]->kind != PG_REFERENCE || all[1]->kind != PG_REFERENCE) return -1;
	const struct symmetry_entry *outer = pg_symmetry_owner(all[0]->as.reference), *inner = pg_symmetry_owner(all[1]->as.reference);
	if (!outer || !inner) return -1;
	size_t dimension = outer->dimension > inner->dimension ? outer->dimension : inner->dimension;
	if (dimension != capacity) return -1;
	candidate->outer = outer;
	candidate->inner = inner;
	candidate->argument = all[2];
	candidate->dimension = dimension;
	*work = candidate;
	*count = total - 3;
	*roots = all + 3;
	return 0;
}

static int fold_position(unsigned phase, size_t count, size_t position)
{
	if (count > SIZE_MAX - 2 || position > count + 2) return 0;
	switch (phase) {
	case FOLD_BINDERS: case FOLD_ABSTRACT: return 1;
	case FOLD_ARGUMENTS: return position >= 1;
	case FOLD_CLAUSE: return position == count + 2;
	default: return 0;
	}
}

int pg_fold_work_write(FILE *file, const struct fold_work *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !work || (count && !roots)) return -1;
	if (!fold_position(work->phase, work->count, work->position) || work->index > work->count) return -1;
	size_t kept = work->phase == FOLD_BINDERS ? work->position : work->count + 2;
	size_t base = work->phase == FOLD_BINDERS ? 4 : 6;
	size_t maximum = SIZE_MAX / sizeof(const struct pg_term *);
	if (kept > maximum - base || count > maximum - base - kept) return -1;
	if (!work->binders || !work->label || work->label->kind != PG_SEMANTIC_OBJECT) return -1;
	if (base == 4 && (work->x || work->next)) return -1;
	if (base == 6 && (!work->x || work->x->kind != PG_BINDER || !work->next)) return -1;
	struct pg_graph temporary;
	if (pg_graph_init(&temporary)) return -1;
	int status = -1;
	const struct pg_term **all = pg_alloc(&temporary, (base + kept + count) * sizeof(*all));
	if (!all) goto done;
	all[0] = work->head;
	all[1] = work->resume;
	all[2] = work->payload;
	all[3] = pg_reference(&temporary, work->label);
	if (base == 6) {
		all[4] = work->next;
		all[5] = pg_reference(&temporary, work->x);
	}
	for (size_t i = 0; i < kept; ++i) {
		if (!work->binders[i] || work->binders[i]->kind != PG_BINDER) goto done;
		all[base + i] = pg_reference(&temporary, work->binders[i]);
	}
	for (size_t i = 0; i < count; ++i) all[base + kept + i] = roots[i];
	if (fwrite(fold_magic, 1, 8, file) != 8 || pg_wire_write_u64(file, work->phase)
		|| pg_wire_write_u64(file, work->count) || pg_wire_write_u64(file, work->index)
		|| pg_wire_write_u64(file, work->position)) goto done;
	status = pg_graph_write_descriptors(file, base + kept + count, all, codec, owner);
done:
	pg_graph_destroy(&temporary);
	return status;
}

int pg_fold_work_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct fold_work **work, size_t *count, const struct pg_term *const **roots)
{
	if (!work || !count || !roots) return -1;
	*work = NULL;
	*count = 0;
	*roots = NULL;
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t phase, arity, index, position;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, fold_magic, 8)
		|| pg_wire_read_u64(file, &phase) || pg_wire_read_u64(file, &arity)
		|| pg_wire_read_u64(file, &index) || pg_wire_read_u64(file, &position)) return -1;
	if (phase > FOLD_ABSTRACT || arity > limit || arity > SIZE_MAX / sizeof(const struct pg_object *) - 2
		|| index > arity || position > SIZE_MAX) return -1;
	if (!fold_position((unsigned)phase, (size_t)arity, (size_t)position)) return -1;
	size_t kept = phase == FOLD_BINDERS ? (size_t)position : (size_t)arity + 2;
	size_t base = phase == FOLD_BINDERS ? 4 : 6;
	size_t total;
	const struct pg_term *const *all;
	if (pg_graph_read_descriptors(file, output, limit, name_limit, codec, owner, &total, &all)) return -1;
	if (total < base || kept > total - base) return -1;
	if (all[3]->kind != PG_REFERENCE || all[3]->as.reference->kind != PG_SEMANTIC_OBJECT) return -1;
	struct fold_work *candidate = pg_alloc(arena, sizeof(*candidate));
	if (!candidate) return -1;
	candidate->graph = output;
	candidate->head = all[0];
	candidate->resume = all[1];
	candidate->payload = all[2];
	candidate->label = all[3]->as.reference;
	candidate->phase = phase;
	candidate->count = (size_t)arity;
	candidate->index = (size_t)index;
	candidate->position = (size_t)position;
	if (base == 6) {
		if (all[5]->kind != PG_REFERENCE || all[5]->as.reference->kind != PG_BINDER) return -1;
		candidate->next = all[4];
		candidate->x = all[5]->as.reference;
	}
	candidate->binders = pg_alloc(arena, ((size_t)arity + 2) * sizeof(*candidate->binders));
	if (!candidate->binders) return -1;
	for (size_t i = 0; i < kept; ++i) {
		const struct pg_term *term = all[base + i];
		if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) return -1;
		candidate->binders[i] = term->as.reference;
	}
	*work = candidate;
	*count = total - base - kept;
	*roots = all + base + kept;
	return 0;
}

struct frame_codec {
	struct pg_graph *arena;
	const struct pg_graph_codec *codec;
	void *owner;
	size_t count;
	const struct action_scope **inputs;
	struct action_scope *const *scopes;
	const struct action_result_work *write_result;
	struct action_result_work *result;
};

static int write_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct frame_codec *context = opaque;
	return pg_action_ownership_write(file, context->count, context->inputs,
		context->write_result, count, roots, context->codec, context->owner);
}

static int read_terms(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct frame_codec *context = opaque;
	size_t n;
	if (pg_action_ownership_read(file, context->arena, graph, limit, name_limit,
		context->codec, context->owner, &n, &context->scopes, &context->result, count, roots)) return -1;
	return n == context->count ? 0 : -1;
}

int pg_computation_frames_write(FILE *file, const struct pg_eval_frame *frames, const struct action_result_work *result,
	const struct pg_eval_configuration *current, const struct pg_graph_codec *codec, void *owner)
{
	if (!file || !frames || !current) return -1;
	struct pg_dag collected = {0};
	int status = -1;
	if (pg_dag_init(&collected, NULL, NULL)) goto done;
	for (const struct pg_eval_frame *p = frames; p; p = p->parent) {
		if (pg_dag_find(&collected, p) || pg_dag_add(&collected, p)) goto done;
	}
	if (collected.count > SIZE_MAX / sizeof(const struct action_scope *)) goto done;
	struct frame_codec context = {.codec = codec, .owner = owner, .count = collected.count, .write_result = result};
	context.inputs = pg_alloc(&collected.storage, context.count * sizeof(*context.inputs));
	if (!context.inputs) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, context.count)) goto done;
	size_t i = 0;
	for (const struct pg_eval_frame *p = frames; p; p = p->parent, ++i) {
		const struct pg_eval_continuation *continuation = p->continuation;
		if (!continuation || !continuation->name) goto done;
		if (pg_computation_continuation_resolve(continuation->name) != continuation) goto done;
		int scoped = pg_identity_continuation_uses_scope(continuation);
		if (scoped != (p->state != NULL)) goto done;
		context.inputs[i] = p->state;
		size_t length = strlen(continuation->name);
		if (pg_wire_write_u64(file, length) || fwrite(continuation->name, 1, length, file) != length) goto done;
	}
	status = pg_eval_frames_payload_write_with(file, frames, current, write_terms, &context);
done:
	pg_dag_destroy(&collected);
	return status;
}

int pg_computation_frames_read(FILE *file, struct pg_graph *arena, struct pg_graph *output,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *owner,
	struct pg_eval_frame **frames, struct action_result_work **result, struct pg_eval_configuration *current)
{
	if (!frames || !result || !current) return -1;
	*frames = NULL;
	*result = NULL;
	memset(current, 0, sizeof(*current));
	if (!file || !arena || !output) return -1;
	char header[8];
	uint64_t count;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)
		|| pg_wire_read_u64(file, &count)) return -1;
	if (!count || count > limit || count > SIZE_MAX / sizeof(const struct pg_eval_continuation *)) return -1;
	const struct pg_eval_continuation **continuations = pg_alloc(arena, (size_t)count * sizeof(*continuations));
	if (!continuations) return -1;
	for (size_t i = 0; i < count; ++i) {
		uint64_t length;
		if (pg_wire_read_u64(file, &length) || !length || length > name_limit || length >= SIZE_MAX) return -1;
		char *name = pg_alloc(arena, (size_t)length + 1);
		if (!name || fread(name, 1, (size_t)length, file) != length || memchr(name, 0, (size_t)length)) return -1;
		name[length] = 0;
		continuations[i] = pg_computation_continuation_resolve(name);
		if (!continuations[i]) return -1;
	}
	struct frame_codec context = {.arena = arena, .codec = codec, .owner = owner, .count = (size_t)count};
	struct pg_eval_frame *candidate;
	struct pg_eval_configuration configuration;
	if (pg_eval_frames_payload_read_with(file, arena, output, limit, name_limit,
		read_terms, &context, &candidate, &configuration)) return -1;
	size_t i = 0;
	for (struct pg_eval_frame *p = candidate; p; p = p->parent, ++i) {
		if (i == count) goto failure;
		if (pg_identity_continuation_uses_scope(continuations[i]) != (context.scopes[i] != NULL)) goto failure;
		p->continuation = continuations[i];
		p->state = context.scopes[i];
	}
	if (i != count) goto failure;
	*frames = candidate;
	*result = context.result;
	*current = configuration;
	return 0;
failure:
	if (candidate) pg_materialize_destroy(&candidate->answer);
	return -1;
}
