#ifndef A_PROGRAM_POINTER_SYNTHESIS_EFFECT_H
#define A_PROGRAM_POINTER_SYNTHESIS_EFFECT_H

#include "synthesis.h"

/* Borrow the row worker of an inference request. No progress or acceptance.
 * NULL for other requests; consumers do not depend on its input layout. */
struct pg_effect_inference *pg_synthesis_effect_worker(const struct pg_synthesis_job *job);

#endif
