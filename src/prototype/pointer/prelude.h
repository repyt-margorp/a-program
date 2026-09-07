#ifndef A_PROGRAM_POINTER_PRELUDE_H
#define A_PROGRAM_POINTER_PRELUDE_H

#include "evidence.h"

/* Ordinary closed computation functions, specialized to a value-universe
 * level. These are derived terms, not new kernel rules or implicit syntax.
 * Callers choose names and quotation using the ordinary source-scope API.
 * Storage and evidence belong to typing->graph; retain this library to share
 * its binders and proofs. Separate builds have fresh lexical binders. */
struct pg_identity_library {
	const struct pg_evidence *equality;
	const struct pg_evidence *reflexivity;
	const struct pg_evidence *transport[2];
	const struct pg_evidence *lifting[2];
};
const struct pg_identity_library *pg_identity_library(struct pg_typing *typing,
	struct pg_classifiers *classifiers, uint64_t level);

#endif
