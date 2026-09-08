#ifndef A_PROGRAM_POINTER_DIMENSION_H
#define A_PROGRAM_POINTER_DIMENSION_H

#include "graph.h"

enum pg_coordinate_kind { PG_ENDPOINT_ZERO, PG_ENDPOINT_ONE, PG_AXIS };
struct pg_coordinate {
	enum pg_coordinate_kind kind;
	size_t axis;
};

/* m -> n has n coordinates in the m-dimensional source. Axes cannot repeat. */
struct pg_dimension_map {
	size_t source;
	size_t target;
	const struct pg_coordinate *coordinates;
};

struct pg_dimensions {
	struct pg_graph *graph;
	struct pg_index maps;
	struct pg_index binding_faces;
};

struct pg_binding_cube {
	size_t dimension;
};

/* One variable of a cube's dependent boundary telescope. */
struct pg_binding_face {
	struct pg_object variable;
	const struct pg_binding_cube *cube;
	const struct pg_dimension_map *face;
};

int pg_dimensions_init(struct pg_dimensions *dimensions, struct pg_graph *graph);
/* Decode a cube's boundary argument slot in lexicographic 0,1,axis order
 * (last coordinate varies fastest). Includes the center as the last slot.
 * Writes dimension coordinates and their source-axis count without allocation.
 * Rejects overflow/out-of-range input without changing either output. */
int pg_dimension_cube_coordinates(size_t dimension, size_t slot,
	struct pg_coordinate *coordinates, size_t *source);
/* Factor permutation composed with the ordered face at slot. Return the slot
 * of the ordered source face and its intrinsic orientation, including centers.
 * This only plans argument movement; it does not apply/force a term or infer
 * that a raw APP spine represents a cube. Outputs change only on success. */
int pg_dimension_cube_permute_slot(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *permutation, size_t slot,
	size_t *source_slot, const struct pg_dimension_map **intrinsic);
/* Frees the index; maps remain owned by graph. */
void pg_dimensions_destroy(struct pg_dimensions *dimensions);
const struct pg_dimension_map *pg_dimension_map(struct pg_dimensions *dimensions,
	size_t source, size_t target, const struct pg_coordinate *coordinates);
const struct pg_dimension_map *pg_dimension_identity(struct pg_dimensions *dimensions, size_t dimension);
/* Validate/intern a map and require every source axis: no degeneracy. */
const struct pg_dimension_map *pg_dimension_face(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *map);
/* Invert a permutation, rejecting endpoint coordinates and dropped axes.
 * The result uses the same map interner; this is geometry, not typed symmetry. */
const struct pg_dimension_map *pg_dimension_inverse(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *permutation);
/* Disjoint sum identity(fixed) + map. Prefix axes are unchanged; this is
 * dimension algebra, not renumbering lexical binders or typed admission. */
const struct pg_dimension_map *pg_dimension_prefix(struct pg_dimensions *dimensions,
	size_t fixed, const struct pg_dimension_map *map);
/* Factor a strict face f:k->n as ordered o intrinsic. ordered retains the
 * endpoint coordinates and uses axes in increasing occurrence order;
 * intrinsic:k->k retains f's local orientation. Outputs change only on success.
 * This is map algebra, not evidence that Identity proofs can be permuted. */
int pg_dimension_face_factor(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *face, const struct pg_dimension_map **ordered,
	const struct pg_dimension_map **intrinsic);
/* outer : m -> n, inner : l -> m; result : l -> n. */
const struct pg_dimension_map *pg_dimension_compose(struct pg_dimensions *dimensions,
	const struct pg_dimension_map *outer, const struct pg_dimension_map *inner);

const struct pg_binding_cube *pg_binding_cube(struct pg_dimensions *dimensions, size_t dimension);
/* Faces use every source axis. Degeneracies are actions, not new variables. */
const struct pg_binding_face *pg_binding_face(struct pg_dimensions *dimensions,
	const struct pg_binding_cube *cube, const struct pg_dimension_map *face);
/* Recover embedded geometry from a boundary binder, without an index lookup.
 * Ordinary binders and semantic references return NULL. This supplies no
 * classifier or Identity evidence. The view has the owning graph's lifetime. */
const struct pg_binding_face *pg_binding_face_view(const struct pg_object *binder);
const struct pg_binding_face *pg_binding_restrict(struct pg_dimensions *dimensions,
	const struct pg_binding_face *binding, const struct pg_dimension_map *face);
/* Postcompose the face with a permutation of its owning cube's axes. Unlike
 * restriction, this also acts on corners and lower-dimensional faces. This
 * only renames geometric bindings; it does not transpose an Identity proof. */
const struct pg_binding_face *pg_binding_permute(struct pg_dimensions *dimensions,
	const struct pg_binding_face *binding, const struct pg_dimension_map *permutation);

/* Substitute the listed free boundary bindings along a strict face. Other
 * references are unchanged. This is syntactic restriction, not typed Act,
 * transport, or restriction of an opaque semantic object's internal data. */
const struct pg_term *pg_term_restrict_bindings(struct pg_dimensions *dimensions,
	const struct pg_term *term, const struct pg_dimension_map *face,
	size_t count, const struct pg_binding_face *const *bindings);

#endif
