#include "a_program/kernel/judgement/db.h"

#include <string.h>

static int counts_are_zero(const struct prototype_judgement_db* judgement) {
	return judgement && judgement->proposition_count == 0 &&
		judgement->derivation_candidate_count == 0 &&
		judgement->claim_count == 0 && judgement->derivation_count == 0 &&
		judgement->candidate_premise_count == 0 &&
		judgement->accepted_premise_count == 0 &&
		judgement->proposition_resource_usage_count == 0;
}

static void advance_extent(
	struct prototype_judgement_db* judgement,
	uint32_t extent
) {
	switch (extent) {
		case 0:
			judgement->proposition_count++;
			break;
		case 1:
			judgement->derivation_candidate_count++;
			break;
		case 2:
			judgement->claim_count++;
			break;
		case 3:
			judgement->derivation_count++;
			break;
		case 4:
			judgement->candidate_premise_count++;
			break;
		case 5:
			judgement->accepted_premise_count++;
			break;
		case 6:
			judgement->proposition_resource_usage_count++;
			break;
	}
}

int main(void) {
	struct prototype_judgement_proposition propositions[4];
	struct prototype_judgement_derivation_candidate candidates[4];
	struct prototype_judgement_claim claims[4];
	struct prototype_judgement_derivation derivations[4];
	struct prototype_judgement_candidate_premise candidate_premises[8];
	struct prototype_judgement_premise_edge accepted_premises[8];
	struct prototype_usage_entry usage[8];
	struct prototype_judgement_db judgement;
	memset(propositions, 0, sizeof(propositions));
	memset(candidates, 0, sizeof(candidates));
	memset(claims, 0, sizeof(claims));
	memset(derivations, 0, sizeof(derivations));
	memset(candidate_premises, 0, sizeof(candidate_premises));
	memset(accepted_premises, 0, sizeof(accepted_premises));
	prototype_judgement_db_init(
		&judgement,
		propositions,
		candidates,
		claims,
		derivations,
		4,
		candidate_premises,
		8,
		accepted_premises,
		8
	);
	prototype_judgement_db_set_resource_usage_storage(&judgement, usage, 8);

	for (uint32_t extent = 0; extent < 7; ++extent) {
		struct prototype_judgement_transaction_mark mark;
		uint64_t rebuild_count = judgement.index_rebuild_count;
		if (prototype_judgement_transaction_begin(
				&judgement, &mark
			) != 0) {
			return 1;
		}
		advance_extent(&judgement, extent);
		if (prototype_judgement_transaction_rollback(
				&judgement, &mark
			) != 0 || mark.active || !counts_are_zero(&judgement) ||
			judgement.index_rebuild_count != rebuild_count + 1) {
			return 2;
		}
		if (prototype_judgement_transaction_rollback(
				&judgement, &mark
			) == 0 || judgement.index_rebuild_count != rebuild_count + 1) {
			return 3;
		}
	}

	struct prototype_judgement_transaction_mark commit_mark;
	uint64_t rebuild_count = judgement.index_rebuild_count;
	if (prototype_judgement_transaction_begin(
			&judgement, &commit_mark
		) != 0 || prototype_judgement_transaction_commit(
			&judgement, &commit_mark
		) != 0 || commit_mark.active ||
		judgement.index_rebuild_count != rebuild_count ||
		!counts_are_zero(&judgement)) {
		return 4;
	}
	return 0;
}
