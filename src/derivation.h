#ifndef A_PROGRAM_POINTER_DERIVATION_H
#define A_PROGRAM_POINTER_DERIVATION_H

#include "evidence.h"

/* Non-premise arguments to ordinary rule constructors. This borrowed call
 * description is not a certificate or another authoritative evidence store.
 * Conversion/reduction receipts must already be genuine local certificates;
 * they cannot be manufactured from serialized addresses or accepted flags. */
struct pg_derivation_parameters {
	const struct pg_object *binder;
	const struct pg_effect_row *effects;
	const struct pg_object *operation_label;
	const struct pg_handler_signature *handler;
	const struct pg_data_declaration *declaration;
	const struct pg_object *constructor;
	const struct pg_object *constant;
	const struct pg_induction_allocation *induction;
	uint64_t level;
	enum pg_totality totality;
	enum pg_identity_direction direction;
	const struct pg_conversion_certificate *conversion;
	const struct pg_reduction_certificate *reduction;
};
/* Immutable unaccepted rule application, shared by synthesis and images.
 * Endpoints are obligations, not receipts. Constructors compute conclusions
 * after checking premises. The certificate parameter pointers remain NULL. */
struct pg_derivation_input {
	enum pg_evidence_rule rule;
	struct pg_derivation_parameters parameters;
	/* An unresolved F row is identified by its graph object, not its worker.
	 * Mutually exclusive with parameters.effects. */
	const struct pg_object *effect_parameter;
	const struct pg_term *source, *target;
	enum pg_reduction_kind reduction_kind;
	size_t count;
	const struct pg_derivation_input *premises[];
};
/* Extract checking inputs, not their physical receipt layout. Context inputs
 * come from selected declarations; other rules still retain premise history.
 * Receipt endpoints become obligations and no acceptance flag is copied. */
int pg_derivation_input_header(const struct pg_evidence *proof, struct pg_derivation_input *input);
/* Read-only dependency enumeration: 1 supplies an already accepted input,
 * 0 is past the end, -1 is unavailable/foreign. Never reconstruct a proof. */
int pg_derivation_input_dependency(const struct pg_typing *typing,
	const struct pg_evidence *proof, size_t index, const struct pg_evidence **child);
int pg_derivation_parameters(const struct pg_evidence *evidence,
	struct pg_derivation_parameters *parameters);
/* Reconstruct from accepted premises using only pg_prove_* rules. The returned
 * conclusion is computed, not supplied by the caller. If an image separately
 * declares a conclusion/export type, the loader must match it before publishing
 * that export. A valid request may canonicalize to an existing proof with a
 * different rule. Serialization retains the resulting evidence, not the request.
 * Constructor/Match/IH result formations must have the same exact typed
 * subject; a different receipt for it does not clone the resulting evidence.
 * NULL includes invalid arity/parameters or inconsistent formation choices. */
const struct pg_evidence *pg_prove_derivation(struct pg_typing *typing,
	enum pg_evidence_rule rule,
	const struct pg_derivation_parameters *parameters, size_t count,
	const struct pg_evidence *const *premises);

#endif
