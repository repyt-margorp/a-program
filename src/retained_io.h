#ifndef A_PROGRAM_POINTER_RETAINED_IO_H
#define A_PROGRAM_POINTER_RETAINED_IO_H

#include "derivation_io.h"
#include "computation_io.h"

/* APGRET4 adds descriptive typed roots to the same Core/object table.
 * Entries are ordered producer outputs; NULL is unavailable. Raw Contexts
 * are retained; declaration formation is not guessed from Context identity.
 * No reduction archive is represented. Earlier semantic formats reject. */
int pg_retained_write_semantic(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms,
	size_t semantic_count, const struct pg_occurrence *const *semantic,
	const struct pg_graph_codec *codec, void *owner);
int pg_retained_read_semantic(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	size_t *term_count, const struct pg_term *const **terms,
	size_t *semantic_count, const struct pg_occurrence *const **semantic);

/* APGRET5 adds owner continuation metadata before the same terminal Core table.
 * Callbacks use the supplied codec/object owner; they must not create another
 * relocation table. Referenced objects and non-Ref terms outlive the entire
 * write, including table serialization. Read callbacks run before the enclosing
 * boundary check: outputs are provisional until the outer read succeeds.
 * Callbacks perform no Solve or evidence admission. NULL selects APGRET4. */
int pg_retained_write_semantic_with(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects,
	size_t term_count, const struct pg_term *const *terms,
	size_t semantic_count, const struct pg_occurrence *const *semantic,
	const struct pg_graph_codec *codec, void *owner,
	int (*continuation)(FILE *, const struct pg_graph_codec *, void *, void *), void *continuation_owner);
/* Same wire grammar, borrowing existing rule inputs for this synchronous write.
 * The view is not retained, solved or accepted. */
int pg_retained_write_semantic_view(FILE *, const struct pg_derivation_view *,
	const struct pg_effect_inference *, size_t, const struct pg_term *const *,
	size_t, const struct pg_occurrence *const *, const struct pg_graph_codec *, void *,
	int (*)(FILE *, const struct pg_graph_codec *, void *, void *), void *);
/* inputs owns only the returned raw rule DAG, not referenced semantic objects. */
int pg_retained_read_semantic_with(FILE *file, struct pg_typing *typing, struct pg_graph *inputs, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	size_t *term_count, const struct pg_term *const **terms,
	size_t *semantic_count, const struct pg_occurrence *const **semantic,
	int (*continuation)(FILE *, struct pg_typing *, size_t, size_t, const struct pg_graph_codec *, void *, void *),
	void *continuation_owner);

/* Unaccepted rule inputs, optional reduction records and raw input terms share
 * one Core/object table. Raw terms carry no typing or reduction evidence.
 * Reading performs no Solve or evidence admission. The caller owns effects
 * and descriptor state, and destroys them after any failure (including an
 * enclosing boundary failure). Output pointers publish only on success. */
int pg_retained_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_effect_inference *effects, const struct pg_reduction_archive *reductions,
	size_t term_count, const struct pg_term *const *terms,
	const struct pg_graph_codec *codec, void *owner);
int pg_retained_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *effects, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots,
	const struct pg_reduction_archive **reductions,
	size_t *term_count, const struct pg_term *const **terms);

#endif
