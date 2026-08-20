#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_TYPES_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_TYPES_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "calculus.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/resource_usage.h"
#include "a_program/support/symbol.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

struct prototype_typed_occurrence_graph;
struct prototype_typed_occurrence;
struct prototype_typed_occurrence_match_case;

enum prototype_judgement_kind {
	PROTOTYPE_JUDGEMENT_KIND_UNKNOWN = 0,
	PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE = 1,
	PROTOTYPE_JUDGEMENT_KIND_IS_TYPE = 2
};

enum prototype_judgement_proof_kind {
	PROTOTYPE_JUDGEMENT_PROOF_INVALID = 0,
	PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO = 1,
	PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO = 2,
	PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION = 3,
	PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION = 4,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION = 5,
	PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO = 6,
	PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM = 7,
	PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO = 8,
	PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO = 9,
	PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM = 10,
	PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO = 11,
	PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM = 12,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO = 13,
	PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM = 14,
	PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE = 15,
	PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM = 16,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO = 17,
	PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO = 18,
	PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO = 19,
	PROTOTYPE_JUDGEMENT_PROOF_CONVERSION = 20,
	/* Explicit expected-type boundary. Unlike CONVERSION, this may discharge
	 * universe/effect metavariable constraints before exposing the expected
	 * classifier. It is not object equality. */
	PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE = 21,
	PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO = 22,
	PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO = 23,
	PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY = 24,
	PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO = 25,
	PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO = 26,
	PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE = 27,
	PROTOTYPE_JUDGEMENT_PROOF_DECLARATION = 28,
	PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY = 29,
	PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO = 30,
	PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN = 31,
	PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN = 32,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION = 33,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO = 34,
	PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX = 35,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS = 36,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS = 37,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS = 38,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS = 39,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS = 40,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS = 41,
	PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS = 42,
	PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION = 43,
	PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION = 44,
	PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TYPE_FORMATION = 45,
	PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM = 46,
	PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_CONSTRUCTOR = 47,
	PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_THUNK_RETURN_WITNESS = 48,
	PROTOTYPE_JUDGEMENT_PROOF_RETURNS_TYPE_FORMATION = 49,
	PROTOTYPE_JUDGEMENT_PROOF_RETURNS_EVALUATION = 50,
	PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION = 51,
	PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_FROM_RETURNS = 52
};

/* Reconstruction role is a property of an accepted rule application, not of
 * a proof tag alone. The same syntax-directed tag may conclude an Operation
 * proposition or a declaration/helper proposition. */
enum prototype_judgement_proof_reconstruction_role {
	PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_INVALID = 0,
	PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_PRINCIPAL = 1,
	PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_DERIVED_OPERATION = 2,
	PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_SCOPED = 3,
	PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_NON_OPERATION = 4
};

enum prototype_judgement_authority_kind {
	PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID = 0,
	PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE = 1,
	PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING = 2,
	PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION = 3,
	PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC = 4,
	PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION = 5,
	PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE = 6,
	PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER = 7,
	PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT = 8
};

enum prototype_judgement_semantic_action_kind {
	PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID = 0,
	PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION = 1
};

#define PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES 31
#define PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT 2053
#define PROTOTYPE_COMPUTATION_FOLD_STRUCTURAL_PREMISE_COUNT(clause_count) \
	(2u + 2u * (clause_count))
#define PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES \
	PROTOTYPE_COMPUTATION_FOLD_STRUCTURAL_PREMISE_COUNT( \
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES \
	)

/* Immutable proposition identity shared by solver candidates and accepted
 * Claims. Lifecycle state is deliberately not part of this record. */
struct prototype_judgement_proposition {
	int kind;
	int authority_kind;
	uint32_t authority_id;
	uint32_t context_id;
	uint32_t occurrence_id;
	uint32_t subject;
	uint32_t classifier;
	/* Quantitative Context usage is part of the judgement identity. */
	uint32_t resource_usage_count;
	const struct prototype_usage_entry* resource_usage;
	uint64_t key_hash;
	uint32_t hash_next;
};

enum prototype_judgement_proposition_store_kind {
	PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_INVALID = 0,
	PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB = 1,
	PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DELTA = 2
};

/* One ordered candidate premise. The store-qualified Proposition ID is the
 * authority. proposition is a checked read-only resolution used by rule views;
 * it is neither identity nor serialized authority. */
struct prototype_judgement_candidate_premise {
	int proposition_store_kind;
	uint32_t proposition_id;
	/* Validators consume only this immutable resolution. */
	const struct prototype_judgement_proposition* proposition;
	/* Transient candidate construction owns separate caller-provided storage.
	 * It is not identity, is never serialized, and is cleared when a premise is
	 * resolved from an accepted store. */
	struct prototype_judgement_proposition* builder_proposition;
	int semantic_action_kind;
	uint32_t semantic_action_id;
};

/* One ordered accepted premise. Exactly one id is valid. A Claim id is a DAG
 * edge; a scoped Proposition id is replayed without publishing a Claim. */
struct prototype_judgement_premise_edge {
	uint32_t claim_id;
	uint32_t scoped_proposition_id;
	int semantic_action_kind;
	uint32_t semantic_action_id;
};

/* Storage-neutral premise resolved for a rule validator. Candidate and
 * accepted storage IDs are adapter concerns and cannot enter kernel rules. */
struct prototype_judgement_premise_view {
	const struct prototype_judgement_proposition* proposition;
	int semantic_action_kind;
	uint32_t semantic_action_id;
};

union prototype_judgement_rule_data {
	uint32_t words[4];
	struct {
		uint32_t owner_view;
		uint32_t constructor_index;
		uint32_t field_index;
	} constructor;
	struct {
		uint32_t match;
		uint32_t motive;
		uint32_t case_index;
		uint32_t field_index;
	} induction;
};

/* Solver-local candidate. */
struct prototype_judgement_derivation_candidate {
	int proof_kind;
	/* Solver-local adjacency. Claim candidates are propositions; derivation
	 * candidates independently point at the proposition they establish. */
	uint32_t conclusion_proposition_id;
	const struct prototype_judgement_proposition* conclusion;
	/* Rule parameters for Match-pattern assumptions. The Match core erases
	 * owner views, so the derivation retains the selected declaration. */
	union prototype_judgement_rule_data rule_data;
	/* Exact graph operation certified by this rule. This is distinct from a
	 * premise Claim and from rule-local diagnostic parameters. */
	int semantic_action_kind;
	uint32_t semantic_action_id;
	uint32_t premise_count;
	struct prototype_judgement_candidate_premise* premises;
	uint64_t key_hash;
	uint32_t hash_next;
};

/* Immutable validator input shared by candidate and accepted replay. It owns
 * no Proposition and carries no solver lifecycle state. */
struct prototype_judgement_rule_application_view {
	int proof_kind;
	const struct prototype_judgement_proposition* conclusion;
	union prototype_judgement_rule_data rule_data;
	int semantic_action_kind;
	uint32_t semantic_action_id;
	uint32_t premise_count;
	struct prototype_judgement_premise_view
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
};

/* Complete evidence selected by a proof-producing lookup. Classifier-only
 * selection is insufficient because one erased Core term may occur under
 * several typed Operation, ContextBinding, or declaration authorities. */
struct prototype_judgement_selected_evidence {
	int kind;
	int authority_kind;
	uint32_t authority_id;
	uint32_t context_id;
	/* Canonical INVALID unless authority_kind is OPERATION. Structural callers
	 * retain the direct child Operation separately when its evidence owner is a
	 * ContextBinding or TypeDeclaration. */
	uint32_t occurrence_id;
	uint32_t subject;
	uint32_t classifier;
	uint32_t resource_usage_count;
	const struct prototype_usage_entry* resource_usage;
};

/* Accepted proposition identity. Rule identity is deliberately absent. */
struct prototype_judgement_claim {
	uint32_t proposition_id;
	uint32_t closure_rank;
	uint64_t key_hash;
	uint32_t hash_next;
};

/* Accepted rule application. Valid premise Claim ids are graph edges;
 * structural dependencies also remain available from TypedOccurrenceGraph. */
struct prototype_judgement_derivation {
	int proof_kind;
	uint32_t conclusion_claim_id;
	uint32_t closure_rank;
	union prototype_judgement_rule_data rule_data;
	int semantic_action_kind;
	uint32_t semantic_action_id;
	/* Rule-premise order is part of the derivation. A valid Claim id denotes an
	 * accepted proposition dependency. INVALID denotes a scoped rule parameter
	 * whose local tuple is retained below and replayed by the rule validator. */
	uint32_t premise_count;
	struct prototype_judgement_premise_edge* premises;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_judgement_match_motive_result {
	uint32_t match_term;
	uint32_t classifier;
};

enum prototype_judgement_computation_constraint_kind {
	PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD = 1,
	PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST = 2
};

enum prototype_judgement_constraint_operand_state {
	PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED = 0,
	PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED = 1,
	PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL = 2
};

/* A computation constraint is the immutable, typed input payload required to
 * validate one CBPV computation rule. It is neither solver lifecycle state nor
 * a runtime environment. The caller owns lifecycle and result publication. */
struct prototype_judgement_computation_constraint {
	int kind;
	uint32_t context_id;
	/* Operation occurrence which generated this constraint.  INVALID is
	 * reserved for authority-neutral constraints generated by a raw TermDB
	 * scan. */
	uint32_t occurrence_id;
	/* Structural operands remain Operation-owned even though the kernel solver
	 * also receives their erased TermDB projections below. */
	uint32_t premise_occurrence_count;
	uint32_t premise_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_contexts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	/* A Lambda operand may be locally classified under assumptions owned by the
	 * enclosing fold. Such an operand is replayed by the fold rule and is not
	 * promoted to an independently publishable Claim. */
	unsigned char premise_states[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	/* Exact body occurrence used when the fold solver weakens the computation
	 * returned by its return-clause lambda. Context and Core subject are read
	 * from this Operation authority when the constraint is solved. */
	uint32_t return_body_occurrence_id;
	/* Current fixed-point operands selected by the exact premise Operations.
	 * These are refreshed by the Operation solver before each kernel pass. */
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	/* Authority-complete evidence selected by the TypedOccurrenceGraph-aware solver.
	 * premise_occurrences remains the structural child edge; it is not an
	 * evidence-owner identifier. LOCAL operands deliberately keep zero evidence
	 * because the enclosing rule replays their scoped assumptions. */
	struct prototype_judgement_selected_evidence
		premise_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t subject;
	uint32_t computation;
	uint32_t continuation;
	uint32_t argument;
	uint32_t application;
};

/* Invocation-local output for one computation constraint. The kernel resets
 * and fills this record but does not retain it in JudgementDelta. A frontend
 * may provide a projected classifier as an expected representative; the
 * kernel publishes it only after independently deriving a convertible result. */
struct prototype_judgement_computation_constraint_result {
	uint32_t projected_classifier;
	uint32_t solved_classifier;
	int effect_residual_pending;
	uint32_t effect_residual_row;
	uint32_t effect_output_row;
};

enum prototype_judgement_effect_row_constraint_kind {
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN = 1,
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL = 2,
	PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION = 3
};

/* Immutable kernel output consumed by the compile-time ConstraintDB. JOIN is
 * n-ary; RESIDUAL uses input and removed rows; INCLUSION uses source and target
 * rows. These are neither TermDB nodes nor solver lifecycle state. */
#define PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS 34

struct prototype_judgement_effect_row_constraint {
	int kind;
	/* Exact computation obligation which emitted this equation. INVALID denotes
	 * an authority-neutral helper equation. */
	uint32_t computation_constraint_id;
	uint32_t subject;
	uint32_t result_row;
	uint32_t operand_count;
	uint32_t operands[PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS];
};

struct prototype_judgement_db {
	/* Mutable solver frontier. These records are reconstructed after artifact
	 * readback when linking or further solving needs a local candidate view;
	 * they are not the accepted certificate image. */
	struct prototype_judgement_proposition* propositions;
	struct prototype_judgement_derivation_candidate* derivation_candidates;
	size_t proposition_count;
	size_t proposition_capacity;
	size_t derivation_candidate_count;
	size_t derivation_candidate_capacity;
	struct prototype_judgement_claim* claims;
	struct prototype_judgement_derivation* derivations;
	struct prototype_judgement_candidate_premise* candidate_premises;
	struct prototype_judgement_premise_edge* accepted_premises;
	struct prototype_usage_entry* proposition_resource_usage;
	size_t claim_count;
	size_t claim_capacity;
	size_t derivation_count;
	size_t derivation_capacity;
	size_t candidate_premise_count;
	size_t candidate_premise_capacity;
	size_t accepted_premise_count;
	size_t accepted_premise_capacity;
	size_t proposition_resource_usage_count;
	size_t proposition_resource_usage_capacity;
	uint32_t claim_index_heads[PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT];
	uint32_t proposition_index_heads[
		PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT
	];
	uint32_t derivation_index_heads[PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT];
	uint32_t candidate_index_heads[PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT];
	uint64_t claim_intern_requests;
	uint64_t claim_intern_hits;
	uint64_t claim_intern_probes;
	uint64_t proposition_intern_requests;
	uint64_t proposition_intern_hits;
	uint64_t proposition_intern_probes;
	uint64_t derivation_intern_requests;
	uint64_t derivation_intern_hits;
	uint64_t derivation_intern_probes;
	uint64_t candidate_premise_allocations;
	uint64_t accepted_premise_allocations;
	uint64_t accepted_premise_reuses;

	uint32_t next_universe_var;
};

#endif
