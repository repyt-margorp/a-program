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
	const struct pg_operation_declaration *operation;
	const struct pg_handler_signature *handler;
	uint64_t level;
	enum pg_identity_direction direction;
	const struct pg_conversion_certificate *conversion;
	const struct pg_reduction_certificate *reduction;
};
int pg_derivation_parameters(const struct pg_evidence *evidence,
	struct pg_derivation_parameters *parameters);
/* Reconstruct from accepted premises using only pg_prove_* rules. The returned
 * conclusion is computed, not supplied by the caller. If an image separately
 * declares a conclusion/export type, the loader must match it before publishing
 * that export. A valid request may canonicalize to an existing proof with a
 * different rule. Serialization retains the resulting evidence, not the request.
 * NULL includes invalid arity/parameters or inconsistent formation choices. */
const struct pg_evidence *pg_prove_derivation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, enum pg_evidence_rule rule,
	const struct pg_derivation_parameters *parameters, size_t count,
	const struct pg_evidence *const *premises);

#endif
