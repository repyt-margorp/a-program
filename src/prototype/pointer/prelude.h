#ifndef A_PROGRAM_POINTER_PRELUDE_H
#define A_PROGRAM_POINTER_PRELUDE_H

#include "evidence.h"

/* Ordinary closed computation functions, specialized to a value-universe
 * level. These are derived terms, not new kernel rules or implicit syntax.
 * Callers choose names and quotation using the ordinary source-scope API.
 * Storage and evidence belong to typing->graph; retain this library to share
 * its binders and proofs. Separate builds have fresh lexical binders.
 * Assembly checks fixed conversion obligations synchronously using the
 * caller's shared pure normalization work; it never runs a source program.
 * Symmetry/composition are transport derivations, not extra equality axioms. */
struct pg_identity_library {
	const struct pg_evidence *equality;
	const struct pg_evidence *reflexivity;
	/* (A B:U_i) -> (R:Id U_i A B) -> (x:A) -> (y:B) -> F U_i,
	 * returning the selected family instance R x y, not Id A x y. */
	const struct pg_evidence *instance;
	const struct pg_evidence *symmetry;
	const struct pg_evidence *composition;
	/* Applies the reflexive action of f:U(Pi A (F B)) to an input path.
	 * Its result has computation-side Identity, not an assumed returned value. */
	const struct pg_evidence *congruence;
	const struct pg_evidence *transport[2];
	const struct pg_evidence *lifting[2];
};
const struct pg_identity_library *pg_identity_library(struct pg_typing *typing,
	struct pg_classifiers *classifiers, struct pg_whnf_work *normalization, uint64_t level);

#endif
