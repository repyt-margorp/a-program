#ifndef A_PROGRAM_PROTOTYPE_KERNEL_JUDGEMENT_OCCURRENCE_CONTEXT_VIEW_H
#define A_PROGRAM_PROTOTYPE_KERNEL_JUDGEMENT_OCCURRENCE_CONTEXT_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/graph/typed_publication_view.h"

/*
 * Immutable Layer T read view used by accepted replay. Source occurrences keep
 * their candidate Context; this view supplies the concrete Context selected by
 * Context projection solving without changing Core or source topology.
 */
struct prototype_judgement_occurrence_context_view {
	const struct prototype_typed_publication_view* publication;
	void* projection_authority;
	int (*lookup_projection_classifier)(
		void* authority,
		uint32_t occurrence_id,
		uint32_t context_id,
		uint32_t subject,
		uint32_t* p_classifier
	);
};

#endif
