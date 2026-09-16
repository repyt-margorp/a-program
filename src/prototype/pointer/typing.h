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
	const struct pg_term *annotation;
	/* A mapped construction has an origin/map, not stale direct operands. */
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
	struct pg_index proofs;
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
/* A different boundary preserves construction inputs, not typing acceptance. */
const struct pg_occurrence *pg_occurrence_boundary(struct pg_typing *typing,
	const struct pg_occurrence *source, enum pg_evidence_judgement judgement,
	const struct pg_term *classifier);
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
/* Shared, budgeted structural context action. No typing acceptance is created. */
struct pg_occurrence_action;
struct pg_occurrence_action *pg_occurrence_action_request(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_occurrence *source);
enum pg_substitution_status pg_occurrence_action_advance(struct pg_occurrence_action *work,
	uint64_t budget);
const struct pg_occurrence *pg_occurrence_action_result(const struct pg_occurrence_action *work);
uint64_t pg_occurrence_action_steps(const struct pg_occurrence_action *work);

#endif
