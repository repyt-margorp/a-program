#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_EVIDENCE_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_EVIDENCE_STATE_H

#include <stddef.h>
#include <stdint.h>

#define PROTOTYPE_TYPING_EVIDENCE_OCCURRENCE_CAPACITY 4096

enum prototype_typing_reification_state {
	PROTOTYPE_TYPING_REIFICATION_UNSEEN = 0,
	PROTOTYPE_TYPING_REIFICATION_IN_PROGRESS,
	PROTOTYPE_TYPING_REIFICATION_SUCCEEDED
};

struct prototype_typing_reification_cache_entry {
	int state;
	uint32_t context_id;
	uint32_t classifier;
};

/* Disposable proof/evidence materialization workspace. Accepted evidence is
 * owned by JudgementDB; no field here is persistent semantic Authority. */
struct prototype_typing_evidence_workspace {
	struct prototype_typing_reification_cache_entry reification_cache[
		PROTOTYPE_TYPING_EVIDENCE_OCCURRENCE_CAPACITY
	];
	uint32_t materialization_pending_ids[
		PROTOTYPE_TYPING_EVIDENCE_OCCURRENCE_CAPACITY
	];
	uint32_t materialization_pending_count;
	size_t resource_usage_synced_proposition_count;
	int materialization_initialized;
	int materialization_substitution_pending;
	int materializing_checked_classifiers;
};

#endif
