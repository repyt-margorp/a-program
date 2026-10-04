#ifndef A_PROGRAM_POINTER_SYNTHESIS_CONVERSION_H
#define A_PROGRAM_POINTER_SYNTHESIS_CONVERSION_H

#include "synthesis_work.h"

/* Borrowed progress in the existing pure reduction engine. The owning request
 * fixes the mode and input; this does not store another normalization result. */
struct pg_synthesis_reduction {
	union { struct pg_whnf_job *whnf; struct pg_nf_job *nf; };
};
const struct pg_reduction_certificate *pg_synthesis_reduction_advance(
	struct pg_synthesis *, struct pg_synthesis_job *, struct pg_synthesis_reduction *,
	const struct pg_term *, enum pg_reduction_kind);
const struct pg_conversion_certificate *pg_synthesis_comparison_certificate(const struct pg_synthesis_job *);
const struct pg_evidence *pg_synthesis_convert(struct pg_synthesis *, struct pg_synthesis_job *,
	const struct pg_evidence *, const struct pg_evidence *);
/* Read-only adapter inputs for provisional inspection, not acceptance. */
struct pg_synthesis_input pg_synthesis_classifier_input(const struct pg_synthesis_job *);
struct pg_synthesis_input pg_synthesis_expect_input(const struct pg_synthesis_job *);
struct pg_synthesis_input pg_synthesis_expect_target(const struct pg_synthesis_job *);

/* Owner-local WHNF continuation, not a second reduction result/cache.
 * pending borrows a live continuation; it does not allocate or advance work.
 * attach is for an unstarted WHNF request whose premises are already checked.
 * The caller MUST establish the continuation's provenance before attachment;
 * matching its input is not proof that saved intermediate states are correct.
 * This is not an untrusted-image admission API. It imports no evidence/DONE bit.
 * Storage must outlive the normalization store, which destroys attached work.
 * Several request modes may share the exact same pending WHNF in that store.
 * On success the existing queued request resumes via ordinary Solve dispatch. */
const struct pg_whnf_job *pg_synthesis_normalization_pending(const struct pg_synthesis *,
	const struct pg_synthesis_job *);
int pg_synthesis_normalization_attach(struct pg_synthesis *, struct pg_synthesis_job *,
	struct pg_whnf_job *);

#endif
