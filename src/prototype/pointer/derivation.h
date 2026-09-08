#ifndef A_PROGRAM_POINTER_DERIVATION_H
#define A_PROGRAM_POINTER_DERIVATION_H

#include "evidence.h"

/* Non-premise arguments to ordinary rule constructors. This borrowed call
 * description is not a certificate or another authoritative evidence store.
 * Conversion/reduction receipts must already be genuine local certificates;
 * they cannot be manufactured from serialized addresses or accepted flags. */
struct pg_derivation_parameters {
	const struct pg_object *binder;
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
 * that export. NULL includes invalid arity/parameters and canonicalized-away rules. */
const struct pg_evidence *pg_prove_derivation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, enum pg_evidence_rule rule,
	const struct pg_derivation_parameters *parameters, size_t count,
	const struct pg_evidence *const *premises);

#endif
