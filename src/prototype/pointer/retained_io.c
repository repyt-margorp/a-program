#include "retained_io.h"

static const char magic[8] = "APGRET\1";

struct write_state {
	size_t count;
	const struct pg_derivation_input *const *roots;
	const struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	void *owner;
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *context)
{
	const struct write_state *state = context;
	if (pg_derivation_inputs_write_inference(file, state->count, state->roots, state->effects, codec, state->owner)) return -1;
	return pg_reduction_archive_write(file, state->reductions, codec, state->owner);
}

int pg_retained_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects, const struct pg_reduction_archive *reductions,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!reductions || (count && !roots)) return -1;
	struct write_state state = {count, roots, effects, reductions, owner};
	return pg_graph_image_write(file, magic, codec, owner, write_payload, &state);
}

struct read_state {
	struct pg_typing *typing;
	size_t count;
	const struct pg_derivation_input *const *roots;
	struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	void *owner;
};

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context)
{
	struct read_state *state = context;
	if (graph != state->typing->graph) return -1;
	if (pg_derivations_read_inference(file, state->typing, limit, name_limit, state->effects,
		codec, state->owner, &state->count, &state->roots)) return -1;
	return pg_reduction_records_read(file, graph, limit, name_limit, codec, state->owner, &state->reductions);
}

int pg_retained_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	const struct pg_reduction_archive **reductions)
{
	if (!typing || !count || !roots || !reductions) return -1;
	struct read_state state = {.typing = typing, .effects = effects, .owner = owner};
	if (pg_graph_image_read(file, magic, typing->graph, limit, name_limit, codec, owner, read_payload, &state)) return -1;
	*count = state.count;
	*roots = state.roots;
	*reductions = state.reductions;
	return 0;
}
