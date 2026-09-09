#ifndef A_PROGRAM_POINTER_IADT_H
#define A_PROGRAM_POINTER_IADT_H

#include "eval.h"

struct pg_data_layout;
struct pg_match_clause {
	const struct pg_object *constructor;
	const struct pg_term *branch;
};

/* Erased, immutable constructor layout, not an accepted type declaration.
 * Arity is runtime layout information, not a positivity/index/type schema.
 * Each creation has fresh identity; descriptors live in graph. */
const struct pg_data_layout *pg_data_layout(struct pg_graph *graph,
	size_t count, const size_t *arities);
const struct pg_object *pg_data_constructor(const struct pg_data_layout *layout, size_t index);
/* Constant-time inverse in this exact layout; not nominal type membership. */
int pg_data_constructor_position(const struct pg_data_layout *layout,
	const struct pg_object *constructor, size_t *position);
const struct pg_object *pg_data_matcher(const struct pg_data_layout *layout);
/* Inert representation inspection for shared graph transport. No schema or
 * typing-store lookup occurs and no nominal formation is established. */
const struct pg_data_layout *pg_data_layout_view(const struct pg_object *matcher);
size_t pg_data_layout_count(const struct pg_data_layout *layout);
int pg_data_constructor_view(const struct pg_object *constructor,
	const struct pg_data_layout **layout, size_t *position, size_t *arity);
/* Complete pointer-labelled clauses, in any order. Branches are ordinary
 * lambda terms over erased fields. Core operands retain every branch, so
 * substitution/readback needs no traversal into opaque descriptor payloads. */
const struct pg_term *pg_data_match(struct pg_graph *graph, const struct pg_data_layout *layout,
	const struct pg_term *scrutinee, size_t count, const struct pg_match_clause *clauses);
/* Erased recursive case template. Free occurrences of recursion in branches
 * denote the whole case function. This is Lambda/Application fixed-point
 * encoding, NOT a typing or termination rule. A typed caller must separately
 * establish that its recursive uses are permitted by the declaration.
 * Branches and their captured variables remain ordinary visible Core edges. */
const struct pg_term *pg_data_recursive_match(struct pg_graph *graph,
	const struct pg_data_layout *layout, const struct pg_object *recursion,
	const struct pg_term *scrutinee, size_t count, const struct pg_match_clause *clauses);
int pg_data_dispatch(struct pg_eval *machine);
/* Fixed one-direction action on an erased matcher and any supplied prefix.
 * Called with the materialized source of Act; adds no typing evidence. */
int pg_data_action(struct pg_eval *machine, const struct pg_term *source);

struct pg_typing;
struct pg_classifiers;
struct pg_evidence;
struct pg_data_schema;
struct pg_data_signature;
struct pg_context;
struct pg_data_declaration;
struct pg_data_constructor_input {
	const struct pg_context *fields;
	const struct pg_term *const *images;
};
/* Inert nominal declaration. Images are ordered over the entire index
 * context, including parameters. Copies the arrays, borrows immutable terms
 * and contexts; creates no typing evidence. Each call is generative.
 * The family fixes its complete declaration, not just erased arities. */
const struct pg_data_declaration *pg_data_declaration(struct pg_graph *graph,
	const struct pg_context *parameters, const struct pg_context *indices,
	size_t count, const struct pg_data_constructor_input *constructors);
/* Reuse an already relocated erased layout, checking every field arity.
 * The nominal family is still fresh; this does not attach typing evidence. */
const struct pg_data_declaration *pg_data_declaration_at_layout(struct pg_graph *graph,
	const struct pg_data_layout *layout, const struct pg_context *parameters,
	const struct pg_context *indices, size_t count,
	const struct pg_data_constructor_input *constructors);
const struct pg_data_layout *pg_data_declaration_layout(const struct pg_data_declaration *declaration);
const struct pg_object *pg_data_declaration_family(const struct pg_data_declaration *declaration);
const struct pg_context *pg_data_declaration_parameters(const struct pg_data_declaration *declaration);
const struct pg_context *pg_data_declaration_indices(const struct pg_data_declaration *declaration);
const struct pg_context *pg_data_declaration_fields(const struct pg_data_declaration *declaration, size_t index);
const struct pg_data_declaration *pg_data_declaration_view(const struct pg_object *object);
const struct pg_data_declaration *pg_data_schema_declaration(const struct pg_data_schema *schema);
/* Declaration payload root slices: contexts = parameters, indices, fields...;
 * terms = matcher, then each constructor's ordered result images. Pack borrows
 * immutable nodes and allocates arrays in storage. Include these slices in the
 * same relocation table as other module roots. Unpack creates no evidence.
 * This payload alone does not relocate references to the nominal family. */
int pg_data_declaration_pack(const struct pg_data_declaration *declaration,
	struct pg_graph *storage, size_t *context_count,
	const struct pg_context *const **contexts, size_t *term_count,
	const struct pg_term *const **terms);
const struct pg_data_declaration *pg_data_declaration_unpack(struct pg_graph *graph,
	size_t context_count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms);
/* Checked parameter/index telescopes, available before checking fields.
 * Immutable and owned by typing->graph; not nominal formation or membership.
 * No layout, constructor, universe bound or accepted-declaration flag lives
 * here. The same scoped signature may be used by distinct generative schemas. */
const struct pg_data_signature *pg_data_signature(struct pg_typing *typing,
	const struct pg_evidence *parameters, const struct pg_evidence *indices);
/* Instantiate its indices after a checked parameter substitution. The result
 * is a substitution into the signature, not yet a nominal type formation. */
const struct pg_evidence *pg_data_signature_instance(struct pg_typing *typing,
	const struct pg_data_signature *signature, const struct pg_evidence *parameters,
	size_t count, const struct pg_evidence *const *indices);
/* Conservative syntactic strict positivity of a field classifier relative
 * to a dedicated Self binder. Recognizes saturated Self applications with
 * independent indices, Pi with independent domains, and F/U wrappers.
 * Other forms must be independent of Self. No reduction is performed.
 * 1 establishes this syntactic condition; 0 means not established, -1 error.
 * This is not formation, universe checking, membership or fibrancy evidence.
 * The caller must supply the resolved, scoped classifier and index arity.
 * Independence is syntactic: opaque aliases, unsolved producers and hidden
 * descriptor definitions must not be treated as established independent
 * inputs to declaration admission merely because this helper returns 1. */
int pg_data_field_positive(const struct pg_term *type,
	const struct pg_object *self, size_t index_count);
/* Current direct-IH fragment: 1 exact Self, 0 Self-independent, -1 other or
 * invalid. Shared by IH typing, source binding and erasure; not positivity. */
int pg_data_direct_recursion(const struct pg_term *type, const struct pg_object *self);
/* indices extends parameters. Each result is a checked substitution from
 * its field context into indices, leaving the parameter prefix unchanged.
 * Fields and arities are derived from those substitutions, not copied into
 * a second semantic schema. Positivity and fibrancy are not certified here. */
const struct pg_data_schema *pg_data_schema(struct pg_typing *typing,
	const struct pg_data_signature *signature,
	size_t count, const struct pg_evidence *const *results);
/* Attach checked premises to the exact inert declaration. Never replace its
 * contexts, images or nominal identity with facts supplied by a caller. */
const struct pg_data_schema *pg_data_schema_check(struct pg_typing *typing,
	const struct pg_data_declaration *declaration,
	const struct pg_data_signature *signature,
	size_t count, const struct pg_evidence *const *results);
/* Apply the syntactic positivity condition to every field in the checked
 * schema. Index arity comes from the signature, not the erased layout.
 * Same return convention and limitations as pg_data_field_positive; this
 * does not discharge Self, check universe bounds or admit the declaration. */
int pg_data_schema_positive(const struct pg_data_schema *schema,
	const struct pg_object *self);
/* Maximum universe bound of retained field formations (zero for no fields).
 * Parameters and indices are not stored fields. This is a necessary lower
 * bound for predicative admission, not formation/cumulativity evidence.
 * Returns 0 on success; failure leaves the output unchanged. */
int pg_data_schema_field_level(const struct pg_data_schema *schema, uint64_t *level);
const struct pg_data_layout *pg_data_schema_layout(const struct pg_data_schema *schema);
/* Raw nominal reference and retained signature, not formation evidence. */
const struct pg_object *pg_data_family_object(const struct pg_data_schema *schema);
size_t pg_data_constructor_count(const struct pg_data_schema *schema);
const struct pg_evidence *pg_data_schema_parameters(const struct pg_data_schema *schema);
const struct pg_evidence *pg_data_schema_indices(const struct pg_data_schema *schema);
const struct pg_evidence *pg_data_schema_fields(const struct pg_data_schema *schema,
	const struct pg_object *constructor);
const struct pg_evidence *pg_data_schema_result(const struct pg_data_schema *schema,
	const struct pg_object *constructor);
/* Compose the constructor's result substitution with its checked field
 * instance. This computes checked index images, not constructor membership. */
const struct pg_evidence *pg_data_result(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *constructor,
	const struct pg_evidence *instance);
/* Extend a checked parameter substitution with field values, using the
 * ordinary dependent substitution rule. Returns that substitution evidence,
 * not a proof of constructor membership. No computation is executed. */
const struct pg_evidence *pg_data_instance(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *constructor,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *values);
/* Abstract a synthesized computation in the constructor's field context over
 * exactly those fields. Ordinary Pi/Lambda derivations retain the parameter
 * prefix. No expected motive guides synthesis, and no RETURN is inserted.
 * This proves the branch function, not an entire Match elimination. */
const struct pg_evidence *pg_data_branch(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema,
	const struct pg_object *constructor, const struct pg_evidence *body);
/* Pull an index-dependent computation motive back along the constructor's
 * result map. This constructs a checking obligation, never synthesizes a body. */
const struct pg_evidence *pg_data_branch_motive(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *constructor,
	const struct pg_evidence *motive);
struct pg_conversion_certificate;
/* Post-check an independently synthesized body against that motive, then
 * abstract its fields. Retains both derivations through ordinary conversion;
 * callers obtain the comparison certificate from the shared conversion work.
 * This certifies a case function, not scrutinee membership or a whole Match. */
const struct pg_evidence *pg_data_case(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema,
	const struct pg_object *constructor, const struct pg_evidence *motive,
	const struct pg_evidence *body, const struct pg_conversion_certificate *conversion);

/* Resolve only this owner's known, versioned evaluator continuations. */
struct pg_eval_continuation;
const struct pg_eval_continuation *pg_data_continuation_resolve(const char *name);

#endif
