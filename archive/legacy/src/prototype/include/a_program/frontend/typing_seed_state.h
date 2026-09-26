#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_SEED_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_SEED_STATE_H

#include <stdint.h>

#define PROTOTYPE_TYPING_CLASSIFIER_OCCURRENCE_CAPACITY 4096
#define PROTOTYPE_TYPING_MATCH_RESOLUTION_GOAL_CAPACITY 2048
#define PROTOTYPE_TYPING_MATCH_GOAL_CAPACITY 512
#define PROTOTYPE_TYPING_IMPORTED_CONSTRUCTOR_GOAL_CAPACITY 1024
#define PROTOTYPE_TYPING_BINDER_INPUT_CAPACITY 1024
#define PROTOTYPE_TYPING_DECLARATION_INPUT_CAPACITY 1024
#define PROTOTYPE_TYPING_PUBLICATION_INPUT_CAPACITY 4096
#define PROTOTYPE_TYPING_TERMINATION_WITNESS_INPUT_CAPACITY 4096

enum prototype_typing_classifier_seed_reason {
	PROTOTYPE_TYPING_CLASSIFIER_SEED_NONE = 0,
	PROTOTYPE_TYPING_CLASSIFIER_SEED_DECLARATION,
	PROTOTYPE_TYPING_CLASSIFIER_SEED_ANNOTATION,
	PROTOTYPE_TYPING_CLASSIFIER_SEED_BINDER,
	PROTOTYPE_TYPING_CLASSIFIER_SEED_STRUCTURAL
};

struct prototype_typing_classifier_seed {
	uint32_t owner_occurrence;
	uint32_t classifier;
	uint32_t binder_classifier;
	int reason;
};

/* Immutable Layer T inputs derived while lowering source into an open image. */
struct prototype_typing_match_resolution_goal {
	/* Immutable source topology. Resolution state belongs to the current typed
	 * Match case and diagnostic transitions belong to compile metadata. */
	uint32_t match_term;
	uint32_t match_operation;
	uint32_t scrutinee_operation;
	uint32_t case_index;
	uint32_t scrutinee_term;
	int constructor_symbol_id;
};

struct prototype_typing_match_goal {
	uint32_t match_term;
	uint32_t operation;
	uint32_t universe_level_var;
};

struct prototype_typing_imported_constructor_goal {
	uint32_t constructor_term;
	uint32_t owner;
	uint32_t imported_interface_index;
	uint32_t type_export_id;
	uint32_t constructor_export_id;
};

struct prototype_typing_binder_input {
	uint32_t context_id;
	uint32_t binder_var;
	uint32_t classifier;
	uint32_t source_operation;
	/* Root of the immutable classifier replay recipe materialized at the
	 * source-lowering boundary. INVALID means the classifier is a literal and
	 * needs no endpoint replay. */
	uint32_t classifier_rhs_root;
};

struct prototype_typing_declaration_input {
	uint32_t context_id;
	uint32_t subject;
	uint32_t classifier;
};

struct prototype_typing_publication_input {
	int name_symbol_id;
	uint32_t exposed_occurrence;
	uint32_t expectation_occurrence;
	uint32_t expectation_classifier;
};

struct prototype_typing_seed_mark {
	uint32_t match_resolution_goal_count;
	uint32_t match_goal_count;
	uint32_t imported_constructor_goal_count;
	uint32_t binder_input_count;
	uint32_t declaration_input_count;
	uint32_t publication_input_count;
	uint32_t termination_witness_input_count;
};

struct prototype_typing_seed_state {
	struct prototype_typing_classifier_seed classifier_seeds[
		PROTOTYPE_TYPING_CLASSIFIER_OCCURRENCE_CAPACITY
	];
	struct prototype_typing_match_resolution_goal match_resolution_goals[
		PROTOTYPE_TYPING_MATCH_RESOLUTION_GOAL_CAPACITY
	];
	struct prototype_typing_match_goal match_goals[
		PROTOTYPE_TYPING_MATCH_GOAL_CAPACITY
	];
	struct prototype_typing_imported_constructor_goal imported_constructor_goals[
		PROTOTYPE_TYPING_IMPORTED_CONSTRUCTOR_GOAL_CAPACITY
	];
	struct prototype_typing_binder_input binder_inputs[
		PROTOTYPE_TYPING_BINDER_INPUT_CAPACITY
	];
	struct prototype_typing_declaration_input declaration_inputs[
		PROTOTYPE_TYPING_DECLARATION_INPUT_CAPACITY
	];
	struct prototype_typing_publication_input publication_inputs[
		PROTOTYPE_TYPING_PUBLICATION_INPUT_CAPACITY
	];
	uint32_t termination_witness_inputs[
		PROTOTYPE_TYPING_TERMINATION_WITNESS_INPUT_CAPACITY
	];
	uint32_t match_resolution_goal_count;
	uint32_t match_goal_count;
	uint32_t imported_constructor_goal_count;
	uint32_t binder_input_count;
	uint32_t declaration_input_count;
	uint32_t publication_input_count;
	uint32_t termination_witness_input_count;
};

void prototype_typing_seed_state_init(
	struct prototype_typing_seed_state* state
);

int prototype_typing_seed_state_mark(
	const struct prototype_typing_seed_state* state,
	struct prototype_typing_seed_mark* mark
);

int prototype_typing_seed_state_truncate(
	struct prototype_typing_seed_state* state,
	const struct prototype_typing_seed_mark* mark
);

/* Field-wise digest of the immutable replay input. Padding bytes and addresses
 * are deliberately excluded so this can guard the open-image boundary. */
uint64_t prototype_typing_seed_state_digest(
	const struct prototype_typing_seed_state* state
);

#endif
