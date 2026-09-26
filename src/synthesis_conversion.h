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
struct pg_synthesis_job *pg_synthesis_classifier_input(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_expect_input(const struct pg_synthesis_job *);
struct pg_synthesis_job *pg_synthesis_expect_target(const struct pg_synthesis_job *);

#endif
