#include "a_program/kernel/context.h"
#include "a_program/support/schema.h"

#include <stdio.h>
#include <string.h>

int main(void) {
	struct prototype_context context_storage[8];
	struct prototype_substitution substitution_storage[8];
	struct prototype_context_db contexts;
	struct prototype_substitution_db substitutions;
	prototype_context_db_init(&contexts, context_storage, 8);
	prototype_substitution_db_init(&substitutions, substitution_storage, 8);

	uint32_t first_context;
	uint32_t second_context;
	if (prototype_context_extend(
			&contexts, prototype_context_empty(&contexts), 1, 10,
			PROTOTYPE_INVALID_ID, &first_context
		) != 0) {
		return 1;
	}
	struct prototype_context first_context_snapshot = context_storage[first_context];
	if (prototype_context_extend(
			&contexts, prototype_context_empty(&contexts), 2, 11,
			PROTOTYPE_INVALID_ID, &second_context
		) != 0 || memcmp(
			&first_context_snapshot, &context_storage[first_context],
			sizeof(first_context_snapshot)
		) != 0) {
		return 1;
	}

	uint32_t projection;
	if (prototype_substitution_projection(
			&substitutions, &contexts, first_context, &projection
		) != 0) {
		return 1;
	}
	struct prototype_substitution projection_snapshot =
		substitution_storage[projection];
	uint32_t rebased;
	if (prototype_substitution_rebase(
			&substitutions, projection, second_context,
			prototype_context_empty(&contexts), PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, &rebased
		) != 0 || rebased == projection || memcmp(
			&projection_snapshot, &substitution_storage[projection],
			sizeof(projection_snapshot)
		) != 0) {
		return 1;
	}
	uint32_t repeated;
	if (prototype_substitution_rebase(
			&substitutions, projection, second_context,
			prototype_context_empty(&contexts), PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, &repeated
		) != 0 || repeated != rebased || memcmp(
			&projection_snapshot, &substitution_storage[projection],
			sizeof(projection_snapshot)
		) != 0) {
		return 1;
	}

	printf("context and substitution immutability checks passed\n");
	return 0;
}
