#include "retained_io.h"
#include "wire.h"

static const char magic[8] = "APGRET\2";

struct write_state {
	size_t count;
	const struct pg_derivation_input *const *roots;
	const struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	size_t term_count;
	const struct pg_term *const *terms;
	void *owner;
};

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *context)
{
	const struct write_state *state = context;
	if (pg_wire_write_u64(file, state->reductions != NULL)) return -1;
	if (pg_derivation_inputs_write_inference(file, state->count, state->roots, state->effects, codec, state->owner)) return -1;
	if (state->reductions && pg_reduction_archive_write(file, state->reductions, codec, state->owner)) return -1;
	return pg_graph_write_descriptors(file, state->term_count, state->terms, codec, state->owner);
}

int pg_retained_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects, const struct pg_reduction_archive *reductions,
	size_t term_count, const struct pg_term *const *terms,
	const struct pg_graph_codec *codec, void *owner)
{
	if ((count && !roots) || (term_count && !terms)) return -1;
	struct write_state state = {count, roots, effects, reductions, term_count, terms, owner};
	return pg_graph_image_write(file, magic, codec, owner, write_payload, &state);
}

struct read_state {
	struct pg_typing *typing;
	size_t count;
	const struct pg_derivation_input *const *roots;
	struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	size_t term_count;
	const struct pg_term *const *terms;
	void *owner;
};

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context)
{
	struct read_state *state = context;
	if (graph != state->typing->graph) return -1;
	uint64_t has_reductions;
	if (pg_wire_read_u64(file, &has_reductions) || has_reductions > 1) return -1;
	if (pg_derivations_read_inference(file, state->typing, limit, name_limit, state->effects,
		codec, state->owner, &state->count, &state->roots)) return -1;
	if (has_reductions && pg_reduction_records_read(file, graph, limit, name_limit,
		codec, state->owner, &state->reductions)) return -1;
	return pg_graph_read_descriptors(file, graph, limit, name_limit, codec, state->owner,
		&state->term_count, &state->terms);
}

int pg_retained_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	const struct pg_reduction_archive **reductions,
	size_t *term_count, const struct pg_term *const **terms)
{
	if (!typing || !count || !roots || !reductions || !term_count || !terms) return -1;
	struct read_state state = {.typing = typing, .effects = effects, .owner = owner};
	if (pg_graph_image_read(file, magic, typing->graph, limit, name_limit, codec, owner, read_payload, &state)) return -1;
	*count = state.count;
	*roots = state.roots;
	*reductions = state.reductions;
	*term_count = state.term_count;
	*terms = state.terms;
	return 0;
}
