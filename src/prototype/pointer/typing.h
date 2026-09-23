#ifndef A_PROGRAM_POINTER_TYPING_H
#define A_PROGRAM_POINTER_TYPING_H

#include "eval.h"

enum pg_evidence_judgement { PG_JUDGEMENT_CONTEXT, PG_JUDGEMENT_VALUE_TYPE,
	PG_JUDGEMENT_COMPUTATION_TYPE, PG_JUDGEMENT_VALUE, PG_JUDGEMENT_COMPUTATION,
	PG_JUDGEMENT_SUBSTITUTION, PG_JUDGEMENT_TYPE_FAMILY, PG_JUDGEMENT_INPUT };

/* Immutable declared telescope. Its existence is not a typing certificate. */
struct pg_context {
	const struct pg_context *parent;
	const struct pg_object *binder;
	const struct pg_term *declared_type;
	enum pg_evidence_judgement judgement;
	/* Family signature telescope, extending parent; not formation evidence. */
	const struct pg_context *indices;
};

/* Lexical allocation of recursive elimination, not acceptance evidence.
 * Scope checking validates the declarations separately. */
struct pg_induction_allocation {
	const struct pg_object *recursion, *argument, *self;
	size_t count;
	const struct pg_context *const *clauses;
};

uint64_t pg_induction_allocation_hash(const struct pg_induction_allocation *allocation);
int pg_induction_allocation_equal(const struct pg_induction_allocation *left,
	const struct pg_induction_allocation *right);

/* Descriptive typed structure, not acceptance evidence. A NULL classifier
 * denotes an unclassified input. Source locations belong to diagnostics. */
struct pg_occurrence {
	struct pg_index_entry index;
	enum pg_evidence_judgement judgement;
	const struct pg_context *context;
	const struct pg_term *core;
	const struct pg_term *classifier;
	/* Retained formation of the classifier, not its acceptance receipt.
	 * Mapped uses obtain it by the same context action as other typed edges. */
	const struct pg_occurrence *type;
	const struct pg_term *annotation;
	/* A mapped construction has an origin/map, not stale direct operands.
	 * An origin without a map records a derived result, not current children. */
	const struct pg_occurrence *origin;
	/* One-based input selection from origin, or zero for a result recipe.
	 * operands then retain the optional argument instantiating a scoped input,
	 * not the current children. Acceptance belongs to the inversion rule. */
	size_t selection;
	const struct pg_context_map *map;
	const struct pg_induction_allocation *induction;
	size_t operand_count;
	/* Selected construction maps follow operands in the same allocation.
	 * Unlike map above, they are inputs, not an action on the whole subject. */
	size_t map_count;
	const struct pg_occurrence *operands[];
};

/* Structural substitution, not its well-formedness proof. Images are ordered
 * from the oldest source declaration to the newest, in the destination scope. */
struct pg_context_map {
	struct pg_index_entry index;
	const struct pg_context *source, *destination;
	size_t count;
	const struct pg_occurrence *images[];
};

struct pg_typing {
	struct pg_graph *graph;
	/* Fresh arena-owned identity per initialization, never a reusable address
	 * of this mutable index container. Not a serialized identifier. */
	const void *owner_key;
	struct pg_index contexts;
	struct pg_index occurrences;
	struct pg_index context_maps;
	/* Projection lookup references the same interned maps, not new evidence. */
	struct pg_index context_projections;
	struct pg_index context_lifts;
	struct pg_index occurrence_actions;
	struct pg_index occurrence_inputs;
	struct pg_index typed_queries;
	/* Memoized default lexical allocation requests, not acceptance evidence. */
	struct pg_index induction_requests;
	struct pg_index proofs;
	/* Read-only lookup paths to the same accepted derivations, not claims or
	 * another acceptance store. Alternatives are never replaced. */
	struct pg_index evidence_conclusions;
	/* Computation work, not an additional source of typing evidence. */
	struct pg_substitution_work substitutions;
};

int pg_typing_init(struct pg_typing *typing, struct pg_graph *graph);
void pg_typing_destroy(struct pg_typing *typing);
/* NULL is the empty context. Binder freshness is a scope-construction duty;
 * this operation records a declaration, not its well-formedness proof. */
const struct pg_context *pg_context_bind(struct pg_typing *typing,
	const struct pg_context *parent, const struct pg_object *binder,
	const struct pg_term *declared_type, enum pg_evidence_judgement judgement);
const struct pg_context *pg_context_intern(struct pg_typing *typing,
	const struct pg_context *declaration);
/* Close a declared telescope into its logical Pi signature. No typing claim. */
const struct pg_term *pg_context_signature(struct pg_graph *graph,
	const struct pg_context *parent, const struct pg_context *indices,
	const struct pg_term *universe);
const struct pg_context *pg_context_lookup(const struct pg_context *context,
	const struct pg_object *binder);
/* Count declarations after an exact prefix. Returns -1 for unrelated contexts
 * or a missing output pointer. This inspects structure, not proof validity. */
int pg_context_extension_size(const struct pg_context *context,
	const struct pg_context *prefix, size_t *count);
/* Compare binder allocation without trusting saved declared types. */
int pg_context_same_allocation_shape(const struct pg_context *left,
	const struct pg_context *right);
/* annotation is a declaration supplied by syntax, never a mutable solver
 * answer or an expected-type check. NULL means no explicit annotation. */
const struct pg_occurrence *pg_occurrence(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_context *context, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation, size_t operand_count,
	const struct pg_occurrence *const *operands);
const struct pg_occurrence *pg_occurrence_typed(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_occurrence *type, const struct pg_term *annotation,
	size_t operand_count, const struct pg_occurrence *const *operands);
/* Intern one complete descriptive tuple. The header's index is ignored;
 * operand/map arrays and induction allocation are copied, their targets are
 * borrowed. A stack header is sufficient; no incomplete node is published.
 * This does not check typing or authorize replacing an accepted conclusion. */
const struct pg_occurrence *pg_occurrence_intern(struct pg_typing *typing,
	const struct pg_occurrence *header, const struct pg_occurrence *const *operands,
	const struct pg_context_map *const *maps);
/* Describe a changed judgement boundary by referencing the original typed use,
 * without copying its operands. Neither API accepts a derivation. */
const struct pg_occurrence *pg_occurrence_reclassified(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_occurrence *type);
/* Scoped construction inputs (for example selected Identity boundaries), not
 * an action on the entire subject. Their roles belong to the semantic owner. */
const struct pg_context_map *const *pg_occurrence_maps(const struct pg_occurrence *subject);
/* A different sort/classifier boundary references the original construction;
 * it does not copy inputs, scoped maps or induction allocation. */
const struct pg_occurrence *pg_occurrence_boundary(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *classifier);
const struct pg_occurrence *pg_occurrence_derived(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *core, const struct pg_term *classifier);
const struct pg_occurrence *pg_occurrence_selected(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index, const struct pg_occurrence *argument,
	enum pg_evidence_judgement judgement, const struct pg_term *core, const struct pg_term *classifier);
const struct pg_context_map *pg_context_map(struct pg_typing *typing,
	const struct pg_context *source, const struct pg_context *destination,
	size_t count, const struct pg_occurrence *const *images);
const struct pg_context_map *pg_context_map_projection(struct pg_typing *typing,
	const struct pg_context *source, const struct pg_context *destination);
/* Lift under the named target binder, including one allocated by Core
 * substitution. This describes scope transport, not context acceptance. */
const struct pg_context_map *pg_context_map_lift(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder);
struct pg_context_lift;
struct pg_context_lift *pg_context_lift_request(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context *extension,
	const struct pg_object *binder);
enum pg_substitution_status pg_context_lift_advance(struct pg_context_lift *work, uint64_t budget);
const struct pg_context_map *pg_context_lift_result(const struct pg_context_lift *work);
/* Completed action on a family's index telescope; NULL for a value binder. */
const struct pg_context_map *pg_context_lift_indices(const struct pg_context_lift *work);
uint64_t pg_context_lift_steps(const struct pg_context_lift *work);
/* Immutable erased projection of the typed images, computed at construction. */
const struct pg_binding_value *pg_context_map_bindings(const struct pg_context_map *map);
/* Return the oldest source image for binder. Optional index identifies its
 * source-telescope position even when multiple binders have the same image. */
const struct pg_occurrence *pg_context_map_lookup(const struct pg_context_map *map,
	const struct pg_object *binder, size_t *index);
const struct pg_occurrence *pg_occurrence_mapped(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation,
	const struct pg_occurrence *source, const struct pg_context_map *map);
/* A prefix projection changes scope without scheduling term substitution. */
const struct pg_occurrence *pg_occurrence_projection(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source);
/* Build the same prefix projection, materializing a map only when its recipe
 * is retained by the resulting occurrence. This does not accept a judgement. */
const struct pg_occurrence *pg_occurrence_weaken(struct pg_typing *typing,
	const struct pg_context *destination, const struct pg_occurrence *source);
/* Cancel retained exact weakenings, including declaration-bound variables
 * whose weakening selected the target variable, to a prefix context. This is not general
 * strengthening or a total substitution assigning a removed variable a value.
 * NULL means the construction is not exposed in the requested context. */
const struct pg_occurrence *pg_occurrence_unproject(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_context *destination);
/* Shared, budgeted structural context action. No typing acceptance is created. */
struct pg_occurrence_action;
struct pg_occurrence_action *pg_occurrence_action_request(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source);
/* Substitute the newest scoped binder, preserving the argument's typed
 * structure. Classifier agreement is checked by the calling kernel rule. */
struct pg_occurrence_action *pg_occurrence_instantiate_request(struct pg_typing *typing,
	const struct pg_occurrence *body, const struct pg_occurrence *argument);
enum pg_substitution_status pg_occurrence_action_advance(struct pg_occurrence_action *work,
	uint64_t budget);
const struct pg_occurrence *pg_occurrence_action_result(const struct pg_occurrence_action *work);
uint64_t pg_occurrence_action_steps(const struct pg_occurrence_action *work);

/* Inspect a direct Lambda/Pi scoped input with exact Core/binder agreement.
 * No context action or acceptance is performed; mapped/derived inputs need
 * the budgeted query below. NULL means this direct edge is not exposed. */
const struct pg_occurrence *pg_occurrence_scoped_input(const struct pg_occurrence *source,
	size_t index);
/* Binder/body encoded by a scoped Lambda or Pi input. This is descriptive:
 * it establishes neither typing nor equality of another input's scope. */
const struct pg_object *pg_occurrence_input_binder(const struct pg_term *core,
	size_t index, const struct pg_term **body);
/* Read a construction input, applying retained maps lazily. Lambda bodies and
 * Pi codomains use the target Core binder, freshened if it collides with the
 * destination context. The closed binder/body remains alpha-equivalent to the
 * parent. Match motives lift their dependent telescope; nominal formations
 * and selected Identity families retain the source context of their maps.
 * Other scoped inputs remain unavailable.
 * These requests describe structure only; they cannot certify an input. */
struct pg_occurrence_input;
enum pg_occurrence_input_status { PG_INPUT_PENDING, PG_INPUT_READY,
	PG_INPUT_UNAVAILABLE, PG_INPUT_ERROR };
struct pg_occurrence_input *pg_occurrence_input_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index);
/* Request an input under an outer action without constructing the mapped
 * parent. Scoped binders may differ from a separately mapped closed Core. */
struct pg_occurrence_input *pg_occurrence_input_mapped_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index, const struct pg_context_map *map);
/* An unavailable query can stop at a computed source whose input needs
 * checked reduction. Resume with that exposed input and the existing map
 * spine, without repeating the traversal or mutating the original query.
 * The caller checks the input's relation to the blocked source. */
const struct pg_occurrence *pg_occurrence_input_blocked_source(const struct pg_occurrence_input *work);
struct pg_occurrence_input *pg_occurrence_input_resume_request(struct pg_typing *typing,
	const struct pg_occurrence_input *work, const struct pg_occurrence *child);
struct pg_occurrence_input *pg_occurrence_type_request(struct pg_typing *typing,
	const struct pg_occurrence *source);
/* The budget includes transitions of selected-input dependencies; nested
 * requests use explicit waiting frames, not recursive C calls. */
enum pg_occurrence_input_status pg_occurrence_input_advance(struct pg_occurrence_input *work,
	uint64_t budget);
const struct pg_occurrence *pg_occurrence_input_result(const struct pg_occurrence_input *work);
uint64_t pg_occurrence_input_steps(const struct pg_occurrence_input *work);

#endif
