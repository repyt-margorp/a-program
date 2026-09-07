#ifndef A_PROGRAM_PROTOTYPE_GRAPH_COMPILE_LABEL_H
#define A_PROGRAM_PROTOTYPE_GRAPH_COMPILE_LABEL_H

#include <stdint.h>

#include "a_program/core/term.h"

/* Published source-name metadata. This is not part of typed occurrence
 * topology because the canonical key belongs to publication. */
struct prototype_compile_label {
	int name_symbol_id;
	uint32_t term;
	uint32_t body_occurrence;
	uint32_t body_classifier;
	uint32_t exposed_occurrence;
	uint32_t exposed_classifier;
	uint32_t expectation_classifier;
	struct prototype_term_canonical_key canonical_key;
};

#endif
