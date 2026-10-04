#include "retained_io.h"
#include "wire.h"
#include "occurrence_io.h"
#include <stdlib.h>

static const char magic[8] = "APGRET\2";
static const char semantic_magic[8] = "APGRET\4";
static const char continued_magic[8] = "APGRET\5";

struct write_state {
	struct pg_derivation_view derivations;
	const struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	size_t term_count;
	const struct pg_term *const *terms;
	void *owner;
	int semantic_format;
	size_t semantic_count;
	const struct pg_occurrence *const *semantic;
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *);
	void *continuation_owner;
};

static int write_semantic(FILE *file, const struct write_state *state,
	const struct pg_graph_codec *codec)
{
	size_t count = state->semantic_count, present = 0;
	if (count > SIZE_MAX / sizeof(void *)) return -1;
	const struct pg_occurrence **subjects = malloc((count ? count : 1) * sizeof(*subjects));
	int status = -1;
	if (!subjects || pg_wire_write_u64(file, count)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *root = state->semantic[i];
		if (root) subjects[present++] = root;
		if (pg_wire_write_u64(file, root ? present : 0)) goto done;
	}
	status = pg_occurrences_write_descriptors(file, present, subjects, codec, state->owner);
done:
	free(subjects);
	return status;
}

static int write_payload(FILE *file, const struct pg_graph_codec *codec, void *context)
{
	const struct write_state *state = context;
	if (!state->semantic_format && pg_wire_write_u64(file, state->reductions != NULL)) return -1;
	if (pg_derivation_view_write(file, &state->derivations, state->effects, codec, state->owner)) return -1;
	if (state->reductions && pg_reduction_archive_write(file, state->reductions, codec, state->owner)) return -1;
	if (pg_graph_write_descriptors(file, state->term_count, state->terms, codec, state->owner)) return -1;
	if (state->semantic_format && write_semantic(file, state, codec)) return -1;
	return state->continuation ? state->continuation(file, codec, state->owner, state->continuation_owner) : 0;
}

int pg_retained_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects, const struct pg_reduction_archive *reductions,
	size_t term_count, const struct pg_term *const *terms,
	const struct pg_graph_codec *codec, void *owner)
{
	if ((count && !roots) || (term_count && !terms)) return -1;
	struct write_state state = {.derivations = pg_derivation_input_view(count, roots), .effects = effects,
		.reductions = reductions, .term_count = term_count, .terms = terms, .owner = owner};
	return pg_graph_image_write(file, magic, codec, owner, write_payload, &state);
}

int pg_retained_write_semantic(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms,
	size_t semantic_count, const struct pg_occurrence *const *semantic,
	const struct pg_graph_codec *codec, void *owner)
{
	return pg_retained_write_semantic_with(file, count, roots, effects, term_count, terms,
		semantic_count, semantic, codec, owner, NULL, NULL);
}

int pg_retained_write_semantic_with(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms,
	size_t semantic_count, const struct pg_occurrence *const *semantic,
	const struct pg_graph_codec *codec, void *owner,
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *), void *continuation_owner)
{
	if (count && !roots) return -1;
	struct pg_derivation_view view = pg_derivation_input_view(count, roots);
	return pg_retained_write_semantic_view(file, &view, effects, term_count, terms,
		semantic_count, semantic, codec, owner, continuation, continuation_owner);
}

int pg_retained_write_semantic_view(FILE *file, const struct pg_derivation_view *view,
	const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms,
	size_t semantic_count, const struct pg_occurrence *const *semantic,
	const struct pg_graph_codec *codec, void *owner,
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *), void *continuation_owner)
{
	if (!view || (term_count && !terms) || (semantic_count && !semantic)) return -1;
	struct write_state state = {.derivations = *view, .effects = effects,
		.term_count = term_count, .terms = terms, .owner = owner,
		.semantic_format = 1, .semantic_count = semantic_count, .semantic = semantic,
		.continuation = continuation, .continuation_owner = continuation_owner};
	return pg_graph_image_write(file, continuation ? continued_magic : semantic_magic, codec, owner, write_payload, &state);
}

struct read_state {
	struct pg_typing *typing;
	struct pg_graph *inputs;
	size_t count;
	const struct pg_derivation_input *const *roots;
	struct pg_effect_inference *effects;
	const struct pg_reduction_archive *reductions;
	size_t term_count;
	const struct pg_term *const *terms;
	void *owner;
	int semantic_format;
	size_t semantic_count;
	const struct pg_occurrence **semantic;
	int (*continuation)(FILE *, struct pg_typing *, size_t, size_t, const struct pg_graph_codec *, void *, void *);
	void *continuation_owner;
};

static int read_semantic(FILE *file, struct read_state *state, size_t limit,
	size_t name_limit, const struct pg_graph_codec *codec)
{
	uint64_t count;
	if (pg_wire_read_u64(file, &count) || count > limit || count > SIZE_MAX / sizeof(*state->semantic)
		|| count > SIZE_MAX / sizeof(uint64_t)) return -1;
	const struct pg_occurrence **roots = pg_alloc(state->typing->graph, (size_t)count * sizeof(*roots));
	if (!roots) return -1;
	uint64_t *ids = malloc((count ? (size_t)count : 1) * sizeof(*ids));
	if (!ids) return -1;
	size_t expected = 0, present;
	const struct pg_occurrence *const *subjects;
	int status = -1;
	for (size_t i = 0; i < count; ++i) {
		if (pg_wire_read_u64(file, &ids[i])) goto done;
		if (ids[i] && ids[i] != ++expected) goto done;
	}
	if (pg_occurrences_read_descriptors(file, state->typing, limit, name_limit,
		codec, state->owner, &present, &subjects) || present != expected) goto done;
	for (size_t i = 0; i < count; ++i)
		if (ids[i]) roots[i] = subjects[ids[i] - 1];
	state->semantic_count = (size_t)count;
	state->semantic = roots;
	status = 0;
done:
	free(ids);
	return status;
}

static int read_payload(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context)
{
	struct read_state *state = context;
	if (graph != state->typing->graph) return -1;
	uint64_t has_reductions = 0;
	if (!state->semantic_format && (pg_wire_read_u64(file, &has_reductions) || has_reductions > 1)) return -1;
	if (pg_derivations_read_inference(file, state->typing, state->inputs, limit, name_limit, state->effects,
		codec, state->owner, &state->count, &state->roots)) return -1;
	if (has_reductions && pg_reduction_records_read(file, graph, limit, name_limit,
		codec, state->owner, &state->reductions)) return -1;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, state->owner,
		&state->term_count, &state->terms)) return -1;
	if (state->semantic_format && read_semantic(file, state, limit, name_limit, codec)) return -1;
	return state->continuation ? state->continuation(file, state->typing, limit, name_limit,
		codec, state->owner, state->continuation_owner) : 0;
}

int pg_retained_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	const struct pg_reduction_archive **reductions,
	size_t *term_count, const struct pg_term *const **terms)
{
	if (!typing || !count || !roots || !reductions || !term_count || !terms) return -1;
	struct read_state state = {.typing = typing, .inputs = typing->graph, .effects = effects, .owner = owner};
	if (pg_graph_image_read(file, magic, typing->graph, limit, name_limit, codec, owner, read_payload, &state)) return -1;
	*count = state.count;
	*roots = state.roots;
	*reductions = state.reductions;
	*term_count = state.term_count;
	*terms = state.terms;
	return 0;
}

int pg_retained_read_semantic(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	size_t *term_count, const struct pg_term *const **terms,
	size_t *semantic_count, const struct pg_occurrence *const **semantic)
{
	return pg_retained_read_semantic_with(file, typing, typing ? typing->graph : NULL, limit, name_limit, effects, codec, owner,
		count, roots, term_count, terms, semantic_count, semantic, NULL, NULL);
}

int pg_retained_read_semantic_with(FILE *file, struct pg_typing *typing, struct pg_graph *inputs, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	size_t *term_count, const struct pg_term *const **terms,
	size_t *semantic_count, const struct pg_occurrence *const **semantic,
	int (*continuation)(FILE *, struct pg_typing *, size_t, size_t, const struct pg_graph_codec *, void *, void *),
	void *continuation_owner)
{
	if (!typing || !inputs || !count || !roots || !term_count || !terms || !semantic_count || !semantic) return -1;
	struct read_state state = {.typing = typing, .inputs = inputs, .effects = effects, .owner = owner, .semantic_format = 1,
		.continuation = continuation, .continuation_owner = continuation_owner};
	if (pg_graph_image_read(file, continuation ? continued_magic : semantic_magic,
		typing->graph, limit, name_limit, codec, owner, read_payload, &state)) return -1;
	*count = state.count; *roots = state.roots;
	*term_count = state.term_count; *terms = state.terms;
	*semantic_count = state.semantic_count; *semantic = state.semantic;
	return 0;
}
