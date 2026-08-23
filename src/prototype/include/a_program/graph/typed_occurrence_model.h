#ifndef A_PROGRAM_PROTOTYPE_GRAPH_TYPED_OCCURRENCE_MODEL_H
#define A_PROGRAM_PROTOTYPE_GRAPH_TYPED_OCCURRENCE_MODEL_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/term.h"

struct prototype_compile_label {
	int name_symbol_id;
	uint32_t term;
	/* The assignment RHS occurrence and its independently synthesized
	 * principal. For an ASCRIPTION root this is the operation below it. */
	uint32_t body_occurrence;
	uint32_t body_classifier;
	/* Published/evaluation view. An outer ASCRIPTION may differ from body. */
	uint32_t exposed_occurrence;
	uint32_t exposed_classifier;
	uint32_t expectation_classifier;
	uint32_t expectation_claim_id;
	struct prototype_term_canonical_key canonical_key;
};

/*
 * Typed occurrences preserve the static/source occurrence graph produced by AST
 * lowering.  Their core_term fields may intentionally alias: for example,
 * \x : Bool => x and \y : Nat => y share one core lambda but have distinct
 * occurrences and classifiers.
 */
enum prototype_typed_occurrence_kind {
	PROTOTYPE_TYPED_OCCURRENCE_ATOM = 1,
	PROTOTYPE_TYPED_OCCURRENCE_VAR = 2,
	PROTOTYPE_TYPED_OCCURRENCE_REFERENCE = 3,
	PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR = 4,
	PROTOTYPE_TYPED_OCCURRENCE_APP = 5,
	PROTOTYPE_TYPED_OCCURRENCE_LAMBDA = 6,
	PROTOTYPE_TYPED_OCCURRENCE_MATCH = 7,
	PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS = 8,
	PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE = 9,
	PROTOTYPE_TYPED_OCCURRENCE_RETURN = 10,
	PROTOTYPE_TYPED_OCCURRENCE_THUNK = 11,
	PROTOTYPE_TYPED_OCCURRENCE_FORCE = 12,
	PROTOTYPE_TYPED_OCCURRENCE_REQUEST = 13,
	PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD = 14
};

enum prototype_typed_occurrence_classifier_status {
	PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING = 0,
	PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED = 1,
	PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION = 2
};

enum prototype_typed_occurrence_match_refinement_status {
	PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING = 0,
	PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED = 1,
	PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE = 2,
	PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_RESIDUAL = 3,
	/* The index equation remains unsolved, but a constant motive makes that
	 * equation irrelevant to this branch's result. No Substitution is forged. */
	PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT = 4
};

struct prototype_typed_occurrence {
	int tag;
	/* This belongs to the source operation occurrence, not to the erased core
	 * term. A shared core lambda can be raw in one occurrence and thunked in
	 * another. */
	int category;
	int computation_kind;
	/* APP is one shared core node. This occurrence field selects the typed
	 * elimination or introduction rule without changing TermDB identity. */
	int application_role;
	/* Typed occurrences are indexed by a value context. This ID is deliberately
	 * not part of the erased TermDB node or its canonical key. */
	uint32_t context_id;
	/* INVALID denotes the source occurrence. Otherwise this substitution maps
	 * the Context above back to the occurrence's source Context. The source tag
	 * remains authoritative; core_term is projected through this action when
	 * reconstructing usage and Judgement evidence. */
	uint32_t context_action_substitution;
	uint32_t source_core_term;
	uint32_t source_classifier;
	uint32_t core_term;
	/* The solver result for this source operation. */
	uint32_t classifier;
	/* Frozen result status. Residual typing has no principal classifier and must
	 * name the runtime verification obligation that closes the occurrence. */
	int classifier_status;
	uint32_t classifier_verification_obligation;
	uint32_t source_ast;
	int source_symbol_id;
	int binder_symbol_id;
	/* Source-operation binder identity for VAR occurrences. The core VAR may
	 * alias another scoped occurrence after tagless canonicalization. */
	uint32_t referenced_ast_binder_id;
	/* Exact source-occurrence Binding identity. Lambda stores the binding it
	 * introduces; VAR stores the binding it references. This cannot be recovered
	 * from core_term after alpha-interning selects another representative. */
	uint32_t binding_id;
	uint32_t first_edge;
	uint32_t edge_count;
	/* NAME and ASCRIPTION are source wrappers, not Core constructors. */
	uint32_t wrapped_occurrence;
	uint32_t binder_classifier;
	/* An IH edge belongs to one exact typed Match case field. The erased Core
	 * VAR binding may be alpha-canonical and cannot recover this occurrence
	 * identity. The owner Match is occurrence semantics rather than a Core
	 * child, while the recursive argument is an occurrence edge. */
	uint32_t ih_owner_occurrence;
	uint32_t ih_scope_id;
	uint32_t ih_case_index;
	uint32_t ih_field_index;
	/* The return branch remains singular. Operation clauses live in the
	 * computation-fold clause arena below. */
	uint32_t fold_return_ast_binder_id;
	uint32_t fold_return_binder_id;
	/* Classifier-only row binders generalized by this lambda. They are never
	 * runtime lambda arguments. */
	uint32_t implicit_effect_row_binders[16];
	uint32_t implicit_effect_row_count;
	uint32_t first_case;
	uint32_t case_count;
	uint32_t first_fold_clause;
	uint32_t fold_clause_count;
};

struct prototype_typed_occurrence_edge {
	int role;
	uint32_t ordinal;
	uint32_t child_occurrence;
};

#define PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY 64

struct prototype_typed_occurrence_match_case {
	/* Semantic case telescope. Source binder IDs below are occurrence metadata. */
	uint32_t context_id;
	/* A solved dependent branch owns the pullback Context above. This morphism
	 * maps its refined Context back to the source branch Context. Ordinary ADTs
	 * map the scrutinee to the constructor spine; indexed ADTs additionally map
	 * the solved family indices. */
	int refinement_status;
	uint32_t refinement_substitution;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	int case_label_symbol_id;
	uint32_t binder_count;
	/* Exact source-occurrence Binding identities. Core Match binders may be
	 * alpha-interned and refined Contexts need not end in the case telescope. */
	uint32_t binder_ids[PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY];
	uint32_t ast_binder_ids[PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY];
};

static inline int prototype_typed_occurrence_match_case_is_solved(
	const struct prototype_typed_occurrence_match_case* operation_case
) {
	return operation_case && operation_case->refinement_status ==
		PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED;
}

static inline int prototype_typed_occurrence_match_case_is_admitted(
	const struct prototype_typed_occurrence_match_case* operation_case
) {
	return operation_case &&
		(operation_case->refinement_status ==
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED ||
		 operation_case->refinement_status ==
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT);
}

struct prototype_typed_occurrence_fold_clause {
	uint32_t context_id;
	uint32_t argument_ast_binder_id;
	uint32_t argument_binder_id;
	uint32_t continuation_ast_binder_id;
	uint32_t continuation_binder_id;
};

struct prototype_typed_occurrence_graph {
	/* A sealed graph is immutable and may be projected into proof propositions.
	 * A frozen graph is a sealed graph published as a module snapshot. */
	int sealed;
	int frozen;
	/* A transaction appends to an immutable prefix. Copies of a previously
	 * frozen graph remain valid snapshots because prefix records are unchanged. */
	int transaction_active;
	int transaction_base_frozen;
	size_t transaction_occurrence_start;
	size_t transaction_edge_start;
	size_t transaction_case_start;
	size_t transaction_fold_clause_start;
	struct prototype_typed_occurrence* occurrences;
	size_t occurrence_count;
	size_t occurrence_capacity;
	struct prototype_typed_occurrence_edge* edges;
	size_t edge_count;
	size_t edge_capacity;
	struct prototype_typed_occurrence_match_case* cases;
	size_t case_count;
	size_t case_capacity;
	struct prototype_typed_occurrence_fold_clause* fold_clauses;
	size_t fold_clause_count;
	size_t fold_clause_capacity;
};

struct prototype_typed_occurrence_induction_edge {
	uint32_t induction_occurrence;
	uint32_t owner_match_occurrence;
	uint32_t scope_id;
	uint32_t case_index;
	uint32_t field_index;
	uint32_t binding_id;
	uint32_t argument_occurrence;
};

#endif
