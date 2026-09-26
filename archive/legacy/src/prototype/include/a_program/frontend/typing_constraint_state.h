#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CONSTRAINT_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CONSTRAINT_STATE_H

#include <stdint.h>

#include "a_program/frontend/typing_classifier_state.h"
#include "a_program/frontend/typing_conversion_goal.h"
#include "a_program/kernel/context.h"

#define PROTOTYPE_TYPING_CONSTRAINT_CAPACITY 16384
#define PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY 16384
#define PROTOTYPE_TYPING_CONSTRAINT_OCCURRENCE_CAPACITY 16384
#define PROTOTYPE_TYPING_DEPENDENT_CONSTRAINT_CAPACITY \
	(PROTOTYPE_TYPING_CONSTRAINT_CAPACITY * 8)
#define PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY 32768
#define PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY 65536
#define PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY 32749
#define PROTOTYPE_TYPING_EQUATION_OPERAND_CAPACITY \
	(PROTOTYPE_TYPING_CONSTRAINT_CAPACITY * 8 + \
	 PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY * 4)

enum operation_classifier_goal_kind {
	OPERATION_CONSTRAINT_HAS_TYPE = 1,
	OPERATION_CONSTRAINT_EQUAL,
	OPERATION_CONSTRAINT_IMPORTED_REFERENCE,
	OPERATION_CONSTRAINT_INTRINSIC_REFERENCE,
	OPERATION_CONSTRAINT_CONVERTIBLE,
	OPERATION_CONSTRAINT_PI_EXPECTED,
	OPERATION_CONSTRAINT_MOTIVE_EQUATION,
	OPERATION_CONSTRAINT_IH_EXPECTED,
	OPERATION_CONSTRAINT_CBPV_BOUNDARY,
	OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT,
	OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT,
	OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION,
	OPERATION_CONSTRAINT_BINDER_TYPE,
	OPERATION_CONSTRAINT_CONSTRUCTOR_HEAD,
	/* A termination witness is classified by the exact computation occurrence
	 * named by its TypedOccurrence edge. The Core witness Term alone cannot
	 * recover that occurrence when erased graphs are shared. */
	OPERATION_CONSTRAINT_TERMINATION_WITNESS,
	/* A PI elimination contributes its domain as a classifier premise for the
	 * argument projection. It does not own the APP result. */
	OPERATION_CONSTRAINT_PI_DOMAIN_RELATION,
	/* A zero-clause computation fold contributes the result type of its
	 * computation as the domain of the continuation Lambda. This relation owns
	 * that projected domain; it never rewrites the Lambda's source Context. */
	OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION,
	/* The sole classifier-answer equation for one TypedProjection. Its equation
	 * operands name every structural, seed, and relation premise. */
	OPERATION_CONSTRAINT_PROJECTED_CLASSIFIER,
	OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT
};

enum operation_classifier_goal_state {
	OPERATION_CONSTRAINT_STATE_PENDING = 1,
	OPERATION_CONSTRAINT_STATE_SOLVED = 2,
	OPERATION_CONSTRAINT_STATE_RESIDUAL = 3,
	OPERATION_CONSTRAINT_STATE_INCOMPLETE = 4,
	OPERATION_CONSTRAINT_STATE_CONTRADICTION = 5
};

/* Lifecycle of the answer owned by a base Layer T equation. Constraint state
 * records whether one premise is discharged; answer state records whether the
 * equation's current semantic result may be retained across Solver steps. */
enum prototype_typing_equation_answer_state {
	PROTOTYPE_TYPING_EQUATION_ANSWER_UNSOLVED = 0,
	PROTOTYPE_TYPING_EQUATION_ANSWER_PROVISIONAL,
	PROTOTYPE_TYPING_EQUATION_ANSWER_VALIDATED,
	PROTOTYPE_TYPING_EQUATION_ANSWER_RESIDUAL,
	PROTOTYPE_TYPING_EQUATION_ANSWER_CONTRADICTION
};

enum operation_classifier_goal_reason {
	OPERATION_CLASSIFIER_GOAL_REASON_NONE = 0,
	OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_REFERENCE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_IMPORTED_REFERENCE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_INTRINSIC_REFERENCE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_COMPUTATION_FOLD_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_OPERATION_REQUEST_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_BINDER_TYPE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_USAGE_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED,
	OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL,
	OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY,
	OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE,
	OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_RESIDUAL,
	OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT,
	OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_INVALID,
	OPERATION_CLASSIFIER_GOAL_REASON_CONTEXT_CONTRADICTION,
	OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION,
	OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET,
	OPERATION_CLASSIFIER_GOAL_REASON_EFFECT_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_TOTALITY_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_TERMINATION_WITNESS_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_SEQUENCE_BINDER_VALIDATED,
	OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_DOMAIN_CONTRADICTION
};

enum operation_motive_solution_status {
	OPERATION_MOTIVE_STATUS_UNSOLVED = 0,
	OPERATION_MOTIVE_STATUS_WAITING,
	OPERATION_MOTIVE_STATUS_SOLVED,
	OPERATION_MOTIVE_STATUS_RESIDUAL,
	OPERATION_MOTIVE_STATUS_CONTRADICTION
};

enum operation_motive_solution_phase {
	OPERATION_MOTIVE_PHASE_NONE = 0,
	OPERATION_MOTIVE_PHASE_BRANCH_CANDIDATE,
	OPERATION_MOTIVE_PHASE_EQUATION_SET,
	OPERATION_MOTIVE_PHASE_GUARDED_RECURSIVE,
	OPERATION_MOTIVE_PHASE_MATERIALIZED
};

enum operation_classifier_pi_goal_role {
	OPERATION_CLASSIFIER_PI_GOAL_INTRO = 1,
	OPERATION_CLASSIFIER_PI_GOAL_ELIM
};

enum prototype_typing_has_type_phase {
	PROTOTYPE_TYPING_HAS_TYPE_NONE = 0,
	PROTOTYPE_TYPING_HAS_TYPE_SCANNING,
	PROTOTYPE_TYPING_HAS_TYPE_WAITING_DEDUPLICATION,
	PROTOTYPE_TYPING_HAS_TYPE_WAITING_PUBLICATION
};

enum prototype_typing_pi_phase {
	PROTOTYPE_TYPING_PI_NONE = 0,
	PROTOTYPE_TYPING_PI_WAITING_FAMILY,
	PROTOTYPE_TYPING_PI_WAITING_PI,
	PROTOTYPE_TYPING_PI_WAITING_FORALL,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_TARGET_INSPECTION,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_FUNCTION_NORMALIZATION,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_FUNCTION_INSPECTION,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_EMPTY_ROW,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_FORALL_REWRITE,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_DOMAIN,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_FAMILY_INSPECTION,
	PROTOTYPE_TYPING_PI_ELIM_WAITING_RESULT_REWRITE,
	PROTOTYPE_TYPING_PI_WAITING_PUBLICATION
};

enum prototype_typing_cbpv_phase {
	PROTOTYPE_TYPING_CBPV_NONE = 0,
	PROTOTYPE_TYPING_CBPV_WAITING_TARGET_INSPECTION,
	PROTOTYPE_TYPING_CBPV_WAITING_EMPTY_ROW,
	PROTOTYPE_TYPING_CBPV_WAITING_RETURN_TYPE,
	PROTOTYPE_TYPING_CBPV_WAITING_CHILD_NORMALIZATION,
	PROTOTYPE_TYPING_CBPV_WAITING_CHILD_INSPECTION,
	PROTOTYPE_TYPING_CBPV_WAITING_THUNK_TYPE,
	PROTOTYPE_TYPING_CBPV_WAITING_PUBLICATION
};

enum prototype_typing_fold_phase {
	PROTOTYPE_TYPING_FOLD_NONE = 0,
	PROTOTYPE_TYPING_FOLD_WAITING_INPUT_NORMALIZATION,
	PROTOTYPE_TYPING_FOLD_WAITING_INPUT_INSPECTION,
	PROTOTYPE_TYPING_FOLD_WAITING_BODY_NORMALIZATION,
	PROTOTYPE_TYPING_FOLD_WAITING_BODY_INSPECTION,
	PROTOTYPE_TYPING_FOLD_WAITING_FREE_BINDING_INSPECTION,
	PROTOTYPE_TYPING_FOLD_WAITING_STRUCTURAL_RETURN,
	PROTOTYPE_TYPING_FOLD_WAITING_RESULT_SUBSTITUTION,
	PROTOTYPE_TYPING_FOLD_WAITING_RESULT_FORMATION,
	PROTOTYPE_TYPING_FOLD_WAITING_PUBLICATION,
	PROTOTYPE_TYPING_FOLD_READY_FOR_BODY,
	PROTOTYPE_TYPING_FOLD_READY_FOR_FACETS
};

enum prototype_typing_intrinsic_phase {
	PROTOTYPE_TYPING_INTRINSIC_NONE = 0,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_UNIVERSE_LEVEL,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_UNIVERSE_TERM,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_RESULT_HOST,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_TEXT_HOST,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_NAT_CORE,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_NAT_SOURCE,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_NAT_VIEW,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_EMPTY_ROW,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_OPERATION_ROW,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_RESULT_COMPUTATION,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_ARGUMENT_HOST,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_PI,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_ROW_BINDING,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_ROW_TERM,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_ARGUMENT_COMPUTATION,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_THUNK_TYPE,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_EFFECT_PI,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_EFFECT_FORALL,
	PROTOTYPE_TYPING_INTRINSIC_WAITING_PUBLICATION
};

enum prototype_typing_constructor_head_phase {
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_NONE = 0,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_TARGET_INSPECTION,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_OWNER_SPINE,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_FIELD_FAMILY,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_FIELD_PI,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_PARAMETER_LAMBDA,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_SPECIALIZATION_APP,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_SPECIALIZATION_NORMALIZATION,
	PROTOTYPE_TYPING_CONSTRUCTOR_HEAD_WAITING_PUBLICATION
};

enum prototype_typing_match_phase {
	PROTOTYPE_TYPING_MATCH_NONE = 0,
	PROTOTYPE_TYPING_MATCH_WAITING_SCRUTINEE_NORMALIZATION,
	PROTOTYPE_TYPING_MATCH_WAITING_SCRUTINEE_SPINE,
	PROTOTYPE_TYPING_MATCH_WAITING_SCRUTINEE_HEAD,
	PROTOTYPE_TYPING_MATCH_COLLECTING_CASES,
	PROTOTYPE_TYPING_MATCH_WAITING_FIELD_SPECIALIZATION,
	PROTOTYPE_TYPING_MATCH_WAITING_FIELD_VARIABLE,
	PROTOTYPE_TYPING_MATCH_WAITING_RESULT_SPECIALIZATION,
	PROTOTYPE_TYPING_MATCH_WAITING_RESULT_SPINE,
	PROTOTYPE_TYPING_MATCH_WAITING_INDEX_RELATION,
	PROTOTYPE_TYPING_MATCH_WAITING_SCRUTINEE_TERM,
	PROTOTYPE_TYPING_MATCH_WAITING_CONSTRUCTOR,
	PROTOTYPE_TYPING_MATCH_WAITING_CONSTRUCTOR_ARGUMENT,
	PROTOTYPE_TYPING_MATCH_WAITING_PULLBACK_CLASSIFIER,
	PROTOTYPE_TYPING_MATCH_WAITING_PULLBACK_VARIABLE,
	PROTOTYPE_TYPING_MATCH_WAITING_PUBLICATION
};

enum prototype_typing_imported_reference_kind {
	PROTOTYPE_TYPING_IMPORTED_REFERENCE_TERM = 1,
	PROTOTYPE_TYPING_IMPORTED_REFERENCE_TYPE = 2,
	PROTOTYPE_TYPING_IMPORTED_REFERENCE_CONSTRUCTOR = 3
};

struct prototype_typing_classifier_motive_case_goal {
	uint32_t branch_operation;
	uint32_t scrutinee_operation;
	uint32_t branch_typed_projection;
	uint32_t scrutinee_typed_projection;
	uint32_t match_case_projection;
	uint32_t case_index;
	uint32_t constructor_owner;
	uint32_t constructor_index;
	uint32_t ih_scope_id;
};

union prototype_typing_classifier_goal_payload {
	struct {
		uint32_t referenced_operation;
	} reference;
	struct {
		uint32_t classifier_core_term;
		int import_kind;
		uint32_t import_id;
	} imported_reference;
	struct {
		int target_kind;
		uint32_t target_id;
		int type_symbol_id;
	} intrinsic_reference;
	struct {
		uint32_t body_occurrence;
		uint32_t expected_classifier;
	} conversion;
	struct {
		int role;
		uint32_t body_or_function_operation;
		uint32_t domain_classifier_or_argument_occurrence;
	} pi;
	struct {
		uint32_t function_operation;
		uint32_t argument_occurrence;
	} constructor_formation;
	struct {
		/* The relation concludes the argument projection, while this projection
		 * identifies the APP whose callable domain supplies that conclusion. */
		uint32_t application_typed_projection;
		uint32_t function_typed_projection;
	} pi_domain_relation;
	struct {
		uint32_t fold_typed_projection;
		uint32_t producer_typed_projection;
	} sequence_binder_relation;
	struct {
		uint32_t constructor_id;
		uint32_t owner_typed_projection;
	} constructor_head;
	struct {
		uint32_t binding_context;
		uint32_t binding_id;
	} binder_type;
	struct prototype_typing_classifier_motive_case_goal motive_case;
	struct {
		uint32_t owner_match_occurrence;
		uint32_t recursive_argument_occurrence;
		uint32_t recursive_argument_binding_id;
		uint32_t ih_scope_id;
		uint32_t case_index;
		uint32_t field_index;
	} induction_hypothesis;
	struct {
		uint32_t child_operation;
	} cbpv_boundary;
	struct {
		uint32_t first_operation;
		uint32_t second_operation;
		uint32_t third_operation;
	} computation;
	struct {
		uint32_t computation_typed_projection;
		uint32_t return_body_typed_projection;
		uint32_t fold_topology_id;
	} computation_fold;
	struct {
		uint32_t operation_typed_projection;
		uint32_t argument_typed_projection;
		uint32_t continuation_typed_projection;
	} operation_request;
};

enum operation_constraint_domain {
	OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER = 1,
	OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT = 2,
	OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW = 3,
	OPERATION_CONSTRAINT_DOMAIN_COMPUTATION = 4,
	PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_USAGE = 5,
	PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_TOTALITY = 6,
	PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT = 7
};

enum prototype_typing_equation_operand_target_kind {
	PROTOTYPE_TYPING_EQUATION_OPERAND_TYPED_PROJECTION = 1,
	PROTOTYPE_TYPING_EQUATION_OPERAND_EQUATION,
	PROTOTYPE_TYPING_EQUATION_OPERAND_TERM,
	PROTOTYPE_TYPING_EQUATION_OPERAND_BINDING,
	PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
	PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE
};

enum prototype_typing_equation_operand_role {
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STRUCTURAL = 1000,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_REFERENCED,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_INDUCTION_OWNER,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SEQUENCE_PRODUCER,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SUBSTITUTION_TARGET,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_RELATION_SOURCE,
	/* A sequence over an induction hypothesis depends on both the projected
	 * classifier and the motive that currently justifies that projection. */
	PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SEQUENCE_PROVENANCE
};

/* Immutable edge in the common Layer T equation graph. role is semantic to the
 * owning equation; qualifier carries an ordinal or binding identity when the
 * role has several instances. */
struct prototype_typing_equation_operand {
	int role;
	int target_kind;
	uint32_t target;
	uint32_t qualifier;
};

enum prototype_typing_classifier_rhs_kind {
	PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL = 1,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_VALUE_APP,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_ACCEPTED_SUBSTITUTION,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_THUNK,
	PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES
};

/* Immutable RHS in the common Layer T graph. Children, endpoint occurrences,
 * and substitution targets use the same operand arena as equations. */
struct prototype_typing_classifier_rhs {
	int kind;
	uint32_t source_term;
	uint32_t first_operand;
	uint32_t operand_count;
};

enum prototype_typing_usage_constraint_kind {
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_EMPTY = 1,
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_VARIABLE,
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_COPY,
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_CALL,
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_ABSTRACT,
	/* Sequentially consume the first computation and the second computation
	 * body, removing the result binder from the latter. */
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_SEQUENCE_BIND,
	/* Add the scrutinee usage to the pointwise join of branch usages after
	 * removing each branch's local T bindings. */
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_MATCH,
	/* Add the input computation usage to the pointwise join of the return and
	 * operation-clause bodies after removing their local T bindings. */
	PROTOTYPE_TYPING_USAGE_CONSTRAINT_COMPUTATION_FOLD
};

enum prototype_typing_effect_constraint_kind {
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_EMPTY = 1,
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_COPY,
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_UNION,
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_COMPUTATION_CLASSIFIER,
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_NARY_UNION,
	/* Residualize the input row by the sealed operation projections, then join
	 * the return and operation-clause body rows. */
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_COMPUTATION_FOLD,
	/* Canonical answer owner for one projection-local EffectRow variable. Other
	 * effect equations propose rows to this equation; they never own a copied
	 * meta-variable solution. */
	PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION
};

enum prototype_typing_totality_constraint_kind {
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_CONSTANT = 1,
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_COPY,
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_JOIN,
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_COMPUTATION_CLASSIFIER,
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_NARY_JOIN,
	PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_COMPUTATION_FOLD
};

enum prototype_typing_projection_semantics_phase {
	PROTOTYPE_TYPING_PROJECTION_SEMANTICS_NONE = 0,
	PROTOTYPE_TYPING_PROJECTION_SEMANTICS_WAITING_FORMATION,
	PROTOTYPE_TYPING_PROJECTION_SEMANTICS_WAITING_NARY_EFFECT_FORMATION,
	PROTOTYPE_TYPING_PROJECTION_SEMANTICS_WAITING_NORMALIZATION,
	PROTOTYPE_TYPING_PROJECTION_SEMANTICS_WAITING_INSPECTION
};

enum operation_branch_refinement_constraint_kind {
	OPERATION_BRANCH_REFINEMENT_CONSTRAINT_MATCH_CASE = 1
};

/* Immutable Layer T equation edge. */
struct prototype_typing_constraint {
	uint32_t id;
	int domain;
	int kind;
	uint32_t owner_typed_projection;
	uint32_t first_operand;
	uint32_t operand_count;
	uint32_t source_occurrence;
	uint32_t source_ast;
	uint32_t origin_constraint_id;
	/* INVALID when this equation has no classifier calculation recipe. */
	uint32_t classifier_rhs_root;
	uint64_t key_hash;
	uint32_t hash_next;
	union {
		struct {
			union prototype_typing_classifier_goal_payload goal;
		} classifier;
		struct {
			uint32_t result_row;
			uint32_t left_row;
			uint32_t right_row;
		} effect;
		/* Permanent projection-owned effect equation. Its operands are Layer T
		 * TypedProjectionIds, not Core effect-row TermIds. */
		struct {
			uint32_t first_operand;
			uint32_t second_operand;
		} projection_effect;
		struct {
			uint32_t judgement_delta_constraint_id;
		} computation;
		struct {
			uint32_t first_operand;
			uint32_t second_operand;
			uint32_t binding_id;
		} usage;
		struct {
			uint32_t first_operand;
			uint32_t second_operand;
			int constant;
		} totality;
		struct {
			uint32_t first_operand;
			uint32_t operand_count;
			uint32_t first_auxiliary_operand;
			uint32_t auxiliary_operand_count;
			uint32_t topology_id;
		} projection_fold;
		struct {
			uint32_t match_occurrence;
			uint32_t scrutinee_occurrence;
			uint32_t case_index;
			uint32_t source_context;
			uint32_t match_typed_projection;
			uint32_t scrutinee_typed_projection;
			uint32_t branch_typed_projection;
			uint32_t match_case_projection;
		} branch_refinement;
	} payload;
};

/* Mutable answer for exactly one immutable constraint edge. */
struct prototype_typing_constraint_solution {
	int state;
	int reason;
	/* Advances whenever one declared direct semantic input changes. Scheduler
	 * visits and duplicate enqueue requests do not advance it. */
	uint64_t input_revision;
	/* The base classifier equation owns the sole classifier answer for its
	 * TypedProjection. Other classifier constraints are premises and never own
	 * a copied occurrence answer. */
	uint32_t result_term;
	/* Current source-occurrence classifier discovered after lowering. The
	 * PROJECTED_CLASSIFIER equation owns this input until its Context action can
	 * reindex it into result_term. It is solver progress, never immutable seed. */
	uint32_t source_classifier_candidate;
	int answer_state;
	uint64_t answer_revision;
	uint64_t answer_operand_fingerprint;
	/* Exact direct-input fingerprint for an invalidation that has not yet
	 * produced a replacement answer. This suppresses duplicate wake-ups without
	 * making a global Solver revision part of equation identity. */
	uint64_t answer_pending_operand_fingerprint;
	/* Exact answer-input snapshot. The fingerprints above are diagnostics and
	 * hash indexes only; they never authorize reuse. producer_constraint_id is
	 * an image-local graph reference and is relocated with the image. */
	uint64_t answer_producer_input_revision;
	uint64_t answer_context_revision;
	uint64_t answer_pending_producer_input_revision;
	uint64_t answer_pending_context_revision;
	/* Derived projection of the Context binder equation at this Lambda. The
	 * Context equation remains authoritative; this memo exists only when a
	 * generated binder cannot be represented in its source Context. */
	uint32_t projected_binder_classifier;
	uint32_t producer_constraint_id;
	uint32_t evidence_id;
	uint64_t activation_revision;
	uint32_t projected_motive;
	struct prototype_typing_conversion_goal conversion_goal;
	int classifier_compatibility_validated;
	uint32_t validated_motive;
	uint32_t validated_actual_classifier;
	uint32_t validated_context;
	uint32_t validated_substitution;
	uint64_t validated_recursive_revision;
	int validation_status;
	/* A contradiction belongs to the equation that compared these two
	 * classifiers. Dependency invalidation clears this pair before retrying the
	 * equation; completion reports it only if the fixed point retains the
	 * contradiction. */
	uint32_t contradiction_expected_classifier;
	uint32_t contradiction_actual_classifier;
	struct {
		uint32_t materialized_source_context;
		uint32_t refined_context;
		uint32_t substitution;
		uint32_t constructor_term;
		uint32_t residual_pattern;
		uint32_t residual_value;
		int refinement_status;
	} branch_refinement;
};

struct prototype_typing_dependent_constraint {
	int source_kind;
	uint32_t source;
	uint32_t constraint;
	uint32_t next;
};

enum prototype_typing_dependency_source_kind {
	PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION = 1,
	PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION
};

struct prototype_typing_constraint_db {
	struct prototype_typing_constraint constraints[
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
	];
	struct prototype_typing_constraint_solution solutions[
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
	];
	struct prototype_typing_equation_operand operands[
		PROTOTYPE_TYPING_EQUATION_OPERAND_CAPACITY
	];
	struct prototype_typing_classifier_rhs classifier_rhs[
		PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY
	];
	uint32_t classifier_rhs_hash_slots[
		PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY
	];
	uint32_t constraint_hash_slots[PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY];
	uint32_t operand_count;
	uint32_t classifier_rhs_count;
	uint32_t constraint_count;
	uint64_t constraint_intern_requests;
	uint64_t constraint_intern_hits;
	uint64_t constraint_intern_probes;
	/* Equations from every semantic domain share one append-only arena. These
	 * links are a rebuildable iteration index, not a second equation store. */
	uint32_t first_constraint_for_domain[
		PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT
	];
	uint32_t last_constraint_for_domain[
		PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT
	];
	uint32_t constraint_count_by_domain[
		PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT
	];
	uint32_t next_constraint_in_domain[
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
	];
	/* Rebuildable partition of non-aggregate classifier equations by their
	 * owning TypedProjection. The owner field on each equation is Authority;
	 * these links only avoid rescanning the equation arena in the Solver. */
	uint32_t first_classifier_premise_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t last_classifier_premise_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t classifier_premise_count_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t next_classifier_premise[
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
	];
	uint32_t classifier_constraint_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t conversion_constraint_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t motive_constraint_for_match_case_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t ih_constraint_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	/* Derived scheduling index. A sequence-binder relation changes the input of
	 * the Lambda introduction equation owned by the same TypedProjection. */
	uint32_t pi_intro_constraint_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	/* Derived bridge from a concrete Context classifier equation to the one
	 * sequence relation that owns that projected binder answer. */
	uint32_t sequence_relation_for_context_equation[
		PROTOTYPE_CONTEXT_CAPACITY
	];
	uint32_t branch_refinement_constraint_for_match_case_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	/* One mutable motive answer for one Context-indexed Match projection. */
	struct prototype_typing_motive_solution motive_solution_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t computation_constraint_for_occurrence[
		PROTOTYPE_TYPING_CONSTRAINT_OCCURRENCE_CAPACITY
	];
	uint32_t first_effect_constraint_for_occurrence[
		PROTOTYPE_TYPING_CONSTRAINT_OCCURRENCE_CAPACITY
	];
	/* Rebuildable lookup for the canonical EffectRow answer equation owned by a
	 * TypedProjection. The equation payload and solution are Authority. */
	uint32_t effect_solution_constraint_for_projection[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t next_effect_constraint[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t first_branch_ih_dependency[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t branch_ih_dependency_count[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t branch_ih_dependencies[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t branch_ih_dependency_total;
	uint32_t first_dependent_constraint[
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY
	];
	uint32_t first_equation_dependent_constraint[
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
	];
	struct prototype_typing_dependent_constraint dependent_constraints[
		PROTOTYPE_TYPING_DEPENDENT_CONSTRAINT_CAPACITY
	];
	uint32_t dependent_constraint_count;
	/* Strongly connected components are a derived index over the immutable
	 * equation dependency graph. They schedule recursive Match/IH/fold groups;
	 * they never own a second equation answer. */
	uint32_t scc_for_constraint[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t first_scc_member[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t scc_member_count[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t scc_members[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint8_t scc_recursive[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t scc_count;
	uint32_t scc_member_total;
	uint32_t worklist[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint8_t constraint_queued[PROTOTYPE_TYPING_CONSTRAINT_CAPACITY];
	uint32_t worklist_head;
	uint32_t worklist_count;
	/* Counts semantic answer changes, not worklist visits or cache writes. The
	 * source-to-image boundary requires every counter to remain zero. */
	uint64_t semantic_transition_count;
	uint64_t semantic_transition_count_by_domain[
		PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT
	];
	int topology_sealed;
	uint64_t topology_digest;
	uint32_t sealed_operand_count;
	uint32_t sealed_classifier_rhs_count;
	uint32_t sealed_constraint_count;
	uint32_t sealed_dependent_constraint_count;
	uint32_t sealed_branch_ih_dependency_total;
	uint32_t sealed_scc_count;
	uint32_t sealed_scc_member_total;
};

struct prototype_typing_constraint_db_mark {
	uint32_t operand_count;
	uint32_t classifier_rhs_count;
	uint32_t constraint_count;
	uint32_t dependent_constraint_count;
	uint32_t branch_ih_dependency_total;
	uint32_t worklist_head;
	uint32_t worklist_count;
	uint64_t semantic_transition_count;
	uint64_t semantic_transition_count_by_domain[
		PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT
	];
};

int prototype_typing_constraint_db_init(
	struct prototype_typing_constraint_db* db
);

uint32_t prototype_typing_constraint_first_in_domain(
	const struct prototype_typing_constraint_db* db,
	int domain
);

uint32_t prototype_typing_constraint_next_in_domain(
	const struct prototype_typing_constraint_db* db,
	uint32_t constraint_id
);

uint32_t prototype_typing_constraint_count_in_domain(
	const struct prototype_typing_constraint_db* db,
	int domain
);

int prototype_typing_constraint_has_domain(
	const struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int domain
);

void prototype_typing_constraint_solution_initialize(
	struct prototype_typing_constraint_solution* solution
);

int prototype_typing_constraint_transition(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int state,
	int reason
);

int prototype_typing_constraint_record_semantic_transition(
	struct prototype_typing_constraint_db* db,
	int domain
);

int prototype_typing_constraint_mark_input_changed(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id
);

struct prototype_typing_constraint_db_mark
prototype_typing_constraint_db_mark(
	const struct prototype_typing_constraint_db* db
);

int prototype_typing_constraint_db_rollback(
	struct prototype_typing_constraint_db* db,
	struct prototype_typing_constraint_db_mark mark
);

int prototype_typing_constraint_enqueue(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int* p_enqueued
);

int prototype_typing_constraint_pop(
	struct prototype_typing_constraint_db* db,
	uint32_t* p_constraint_id
);

int prototype_typing_constraint_intern(
	struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	uint32_t* constraint_id
);

int prototype_typing_classifier_rhs_intern(
	struct prototype_typing_constraint_db* db,
	int kind,
	uint32_t source_term,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	uint32_t* p_rhs
);

const struct prototype_typing_classifier_rhs*
prototype_typing_classifier_rhs_get(
	const struct prototype_typing_constraint_db* db,
	uint32_t rhs
);

const struct prototype_typing_equation_operand*
prototype_typing_classifier_rhs_operand_get(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_classifier_rhs* rhs,
	uint32_t index
);

const struct prototype_typing_equation_operand*
prototype_typing_constraint_operand_get(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	uint32_t index
);

uint32_t prototype_typing_constraint_typed_projection_operand(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	uint32_t index
);

int prototype_typing_constraint_add_dependency(
	struct prototype_typing_constraint_db* db,
	uint32_t projection_count,
	uint32_t source_typed_projection,
	uint32_t constraint_id
);

int prototype_typing_constraint_add_equation_dependency(
	struct prototype_typing_constraint_db* db,
	uint32_t source_equation,
	uint32_t constraint_id
);

int prototype_typing_constraint_derive_sccs(
	struct prototype_typing_constraint_db* db,
	uint32_t projection_count
);

#endif
