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
};

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
	const struct pg_context_map *map;
	size_t operand_count;
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
	struct pg_index occurrence_actions;
	struct pg_index occurrence_inputs;
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
const struct pg_context *pg_context_lookup(const struct pg_context *context,
	const struct pg_object *binder);
/* Count declarations after an exact prefix. Returns -1 for unrelated contexts
 * or a missing output pointer. This inspects structure, not proof validity. */
int pg_context_extension_size(const struct pg_context *context,
	const struct pg_context *prefix, size_t *count);
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
/* Select a formed classifier while retaining the term's construction. */
const struct pg_occurrence *pg_occurrence_classified(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_occurrence *type);
/* A different boundary preserves construction inputs, not typing acceptance. */
const struct pg_occurrence *pg_occurrence_boundary(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *classifier);
const struct pg_occurrence *pg_occurrence_derived(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *core, const struct pg_term *classifier);
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
/* Immutable erased projection of the typed images, computed at construction. */
const struct pg_binding_value *pg_context_map_bindings(const struct pg_context_map *map);
const struct pg_occurrence *pg_occurrence_mapped(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const struct pg_term *core,
	const struct pg_term *classifier, const struct pg_term *annotation,
	const struct pg_occurrence *source, const struct pg_context_map *map);
/* A prefix projection changes scope without scheduling term substitution. */
const struct pg_occurrence *pg_occurrence_projection(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source);
/* Cancel retained exact weakenings to a prefix context. This is not general
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

/* Read a construction input, applying retained maps lazily. Lambda bodies and
 * Pi codomains use the target Core binder, freshened if it collides with the
 * destination context. The closed binder/body remains alpha-equivalent to the
 * parent. Other scoped inputs remain unavailable.
 * These requests describe structure only; they cannot certify an input. */
struct pg_occurrence_input;
enum pg_occurrence_input_status { PG_INPUT_PENDING, PG_INPUT_READY,
	PG_INPUT_UNAVAILABLE, PG_INPUT_ERROR };
struct pg_occurrence_input *pg_occurrence_input_request(struct pg_typing *typing,
	const struct pg_occurrence *source, size_t index);
struct pg_occurrence_input *pg_occurrence_type_request(struct pg_typing *typing,
	const struct pg_occurrence *source);
enum pg_occurrence_input_status pg_occurrence_input_advance(struct pg_occurrence_input *work,
	uint64_t budget);
const struct pg_occurrence *pg_occurrence_input_result(const struct pg_occurrence_input *work);
uint64_t pg_occurrence_input_steps(const struct pg_occurrence_input *work);

#endif
