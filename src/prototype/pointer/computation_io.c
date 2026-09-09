#include "computation_io.h"
#include "computation.h"
#include "identity_internal.h"
#include "eval_internal.h"
#include "dag.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = "APGCON\1";

struct frame_codec {
	struct pg_graph *arena;
	const struct pg_graph_codec *codec;
	void *owner;
	size_t count;
	const struct action_scope **inputs;
	struct action_scope *const *scopes;
};

static int write_terms(FILE *file, size_t count, const struct pg_term *const *roots, void *opaque)
{
	struct frame_codec *context = opaque;
	return pg_action_scopes_write(file, context->count, context->inputs,
		count, roots, context->codec, context->owner);
}

static int read_terms(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	size_t *count, const struct pg_term *const **roots, void *opaque)
{
	struct frame_codec *context = opaque;
	size_t n;
	if (pg_action_scopes_read(file, context->arena, graph, limit, name_limit,
		context->codec, context->owner, &n, &context->scopes, count, roots)) return -1;
	return n == context->count ? 0 : -1;
}

int pg_computation_frames_write(FILE *file, const struct pg_eval_frame *frames,
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
	struct frame_codec context = {.codec = codec, .owner = owner, .count = collected.count};
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
	struct pg_eval_frame **frames, struct pg_eval_configuration *current)
{
	if (!frames || !current) return -1;
	*frames = NULL;
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
	*current = configuration;
	return 0;
failure:
	if (candidate) pg_materialize_destroy(&candidate->answer);
	return -1;
}
