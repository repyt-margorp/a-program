#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_REINDEX_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_REINDEX_STATE_H

#include <stdint.h>

#define PROTOTYPE_TYPING_REINDEX_CACHE_CAPACITY 16384

/* Derived Layer T cache. Term IDs are immutable Core references, while the
 * occurrence/Context key and dependency signature belong entirely to typing. */
struct prototype_typing_reindex_cache_entry {
	uint32_t occurrence_id;
	uint32_t context_id;
	uint32_t term;
	uint32_t reindexed;
	uint64_t dependency_signature;
	int occupied;
};

struct prototype_typing_reindex_state {
	uint64_t revision;
	struct prototype_typing_reindex_cache_entry entries[
		PROTOTYPE_TYPING_REINDEX_CACHE_CAPACITY
	];
};

#endif
