#include "a_program/frontend/lowering.h"
#include "a_program/graph/operation_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_judgement_graph_collisions(void) {
	size_t count = PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT + 1;
	struct prototype_judgement_claim* claims = calloc(count, sizeof(*claims));
	struct prototype_judgement_proposition* propositions =
		calloc(count, sizeof(*propositions));
	struct prototype_judgement_derivation* derivations =
		calloc(count, sizeof(*derivations));
	if (!claims || !propositions || !derivations) {
		free(claims);
		free(propositions);
		free(derivations);
		return -1;
	}
	struct prototype_judgement_db judgement;
	prototype_judgement_db_init(
		&judgement, propositions, NULL, claims, derivations, count,
		NULL, 0, NULL, 0
	);
	judgement.claim_count = count;
	judgement.derivation_count = count;
	judgement.proposition_count = count;
	for (uint32_t i = 0; i < count; ++i) {
		propositions[i] = (struct prototype_judgement_proposition){
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			.authority_id = i,
			.context_id = 0,
			.operation_id = PROTOTYPE_INVALID_ID,
			.subject = i,
			.classifier = i + 1
		};
		claims[i] = (struct prototype_judgement_claim){
			.proposition_id = i,
			.closure_rank = 0
		};
		derivations[i] = (struct prototype_judgement_derivation){
			.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
			.conclusion_claim_id = i,
			.closure_rank = 0,
			.semantic_action_kind =
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID,
			.semantic_action_id = PROTOTYPE_INVALID_ID
		};
	}
	if (prototype_judgement_db_rebuild_index(&judgement) != 0) {
		free(claims);
		free(propositions);
		free(derivations);
		return -1;
	}
	uint32_t claim_buckets[PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT];
	uint32_t derivation_buckets[PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT];
	for (size_t i = 0;
		i < PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		++i) {
		claim_buckets[i] = PROTOTYPE_INVALID_ID;
		derivation_buckets[i] = PROTOTYPE_INVALID_ID;
	}
	uint32_t claim_left = PROTOTYPE_INVALID_ID;
	uint32_t claim_right = PROTOTYPE_INVALID_ID;
	uint32_t derivation_left = PROTOTYPE_INVALID_ID;
	uint32_t derivation_right = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < count; ++i) {
		size_t claim_bucket = claims[i].key_hash %
			PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		if (claim_buckets[claim_bucket] != PROTOTYPE_INVALID_ID &&
			claim_left == PROTOTYPE_INVALID_ID) {
			claim_left = claim_buckets[claim_bucket];
			claim_right = i;
		}
		claim_buckets[claim_bucket] = i;
		size_t derivation_bucket = derivations[i].key_hash %
			PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		if (derivation_buckets[derivation_bucket] != PROTOTYPE_INVALID_ID &&
			derivation_left == PROTOTYPE_INVALID_ID) {
			derivation_left = derivation_buckets[derivation_bucket];
			derivation_right = i;
		}
		derivation_buckets[derivation_bucket] = i;
	}
	uint32_t found_claim_left;
	uint32_t found_claim_right;
	uint32_t found_derivation_left;
	uint32_t found_derivation_right;
	int status = claim_left == PROTOTYPE_INVALID_ID ||
		derivation_left == PROTOTYPE_INVALID_ID ||
		prototype_judgement_find_exact_claim(
			&judgement, &claims[claim_left], &found_claim_left
		) != 0 || prototype_judgement_find_exact_claim(
			&judgement, &claims[claim_right], &found_claim_right
		) != 0 || prototype_judgement_find_exact_derivation(
			&judgement, &derivations[derivation_left], &found_derivation_left
		) != 0 || prototype_judgement_find_exact_derivation(
			&judgement, &derivations[derivation_right], &found_derivation_right
		) != 0 || found_claim_left != claim_left ||
		found_claim_right != claim_right ||
		found_derivation_left != derivation_left ||
		found_derivation_right != derivation_right;
	free(claims);
	free(propositions);
	free(derivations);
	return status ? -1 : 0;
}

static int check_judgement_premise_arenas(void) {
	enum { CLAIM_COUNT = 4, DERIVATION_COUNT = 5, STORAGE_CAPACITY = 5 };
	const size_t premise_capacity =
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES + 4;
	struct prototype_judgement_proposition propositions[STORAGE_CAPACITY];
	struct prototype_judgement_claim claims[STORAGE_CAPACITY];
	struct prototype_judgement_derivation derivations[DERIVATION_COUNT];
	struct prototype_judgement_premise_edge* premises = calloc(
		premise_capacity, sizeof(*premises)
	);
	if (!premises) {
		return -1;
	}
	memset(propositions, 0, sizeof(propositions));
	memset(claims, 0, sizeof(claims));
	memset(derivations, 0, sizeof(derivations));
	for (uint32_t i = 0; i < CLAIM_COUNT; ++i) {
		propositions[i] = (struct prototype_judgement_proposition) {
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			.authority_id = i,
			.context_id = 0,
			.operation_id = PROTOTYPE_INVALID_ID,
			.subject = i,
			.classifier = i + 1
		};
		claims[i] = (struct prototype_judgement_claim) {
			.proposition_id = i,
			.closure_rank = i
		};
	}
	uint32_t counts[DERIVATION_COUNT] = {
		0, 1, 2, PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES, 1
	};
	size_t first = 0;
	for (uint32_t i = 0; i < DERIVATION_COUNT; ++i) {
		memset(&derivations[i].rule_data, 0xff, sizeof(derivations[i].rule_data));
		derivations[i].proof_kind = i == 4 ?
			PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO :
			PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION;
		derivations[i].conclusion_claim_id = i == 4 ? 2 : i;
		derivations[i].closure_rank = i;
		derivations[i].premise_count = counts[i];
		derivations[i].premises = counts[i] == 0 ? NULL : &premises[first];
		derivations[i].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		derivations[i].semantic_action_id = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < counts[i]; ++j) {
			premises[first + j].claim_id = i == 2 && j == 1 ? 1 : 0;
			premises[first + j].scoped_proposition_id = PROTOTYPE_INVALID_ID;
			premises[first + j].semantic_action_kind =
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
			premises[first + j].semantic_action_id = PROTOTYPE_INVALID_ID;
		}
		first += counts[i];
	}
	struct prototype_judgement_db judgement;
	prototype_judgement_db_init(
		&judgement, propositions, NULL, claims, derivations, STORAGE_CAPACITY,
		NULL, 0, premises, premise_capacity
	);
	judgement.proposition_count = CLAIM_COUNT;
	judgement.claim_count = CLAIM_COUNT;
	judgement.derivation_count = DERIVATION_COUNT;
	judgement.accepted_premise_count = first;
	uint32_t found;
	struct prototype_judgement_derivation reordered = derivations[2];
	struct prototype_judgement_premise_edge reordered_premises[2] = {
		derivations[2].premises[1], derivations[2].premises[0]
	};
	reordered.premises = reordered_premises;
	int status = prototype_judgement_db_rebuild_index(&judgement) != 0 ||
		prototype_judgement_claim_proposition(&judgement, 2) !=
			&judgement.propositions[judgement.claims[2].proposition_id] ||
		prototype_judgement_find_exact_derivation(
			&judgement, &judgement.derivations[3], &found
		) != 0 || found != 3 || prototype_judgement_find_exact_derivation(
			&judgement, &reordered, &found
		) != 1;
	struct prototype_judgement_proposition scoped = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID,
		.authority_id = PROTOTYPE_INVALID_ID,
		.context_id = 0,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = 99,
		.classifier = 100
	};
	uint32_t scoped_id = PROTOTYPE_INVALID_ID;
	size_t claim_count_before = judgement.claim_count;
	status = status || prototype_judgement_proposition_intern(
		&judgement, &scoped, &scoped_id
	) != 0 || judgement.claim_count != claim_count_before;
	struct prototype_judgement_premise_edge accepted = {
		.claim_id = 0,
		.scoped_proposition_id = PROTOTYPE_INVALID_ID
	};
	struct prototype_judgement_premise_edge local = {
		.claim_id = PROTOTYPE_INVALID_ID,
		.scoped_proposition_id = scoped_id
	};
	struct prototype_judgement_premise_edge both = {
		.claim_id = 0,
		.scoped_proposition_id = scoped_id
	};
	struct prototype_judgement_premise_edge neither = {
		.claim_id = PROTOTYPE_INVALID_ID,
		.scoped_proposition_id = PROTOTYPE_INVALID_ID
	};
	status = status || !prototype_judgement_premise_proposition(
		&judgement, &accepted
	) || prototype_judgement_premise_proposition(&judgement, &local) !=
		prototype_judgement_proposition_get(&judgement, scoped_id) ||
		prototype_judgement_premise_proposition(&judgement, &both) ||
		prototype_judgement_premise_proposition(&judgement, &neither);
	status = status || prototype_judgement_claim_proposition(
		&judgement, PROTOTYPE_INVALID_ID
	);
	struct prototype_judgement_premise_edge* checked_edge =
		&judgement.derivations[1].premises[0];
	struct prototype_judgement_premise_edge saved_edge = *checked_edge;
	checked_edge->claim_id = PROTOTYPE_INVALID_ID;
	checked_edge->scoped_proposition_id = PROTOTYPE_INVALID_ID;
	status = status || prototype_judgement_db_rebuild_index(&judgement) == 0;
	*checked_edge = saved_edge;
	status = status || prototype_judgement_db_rebuild_index(&judgement) != 0;
	checked_edge = &judgement.derivations[1].premises[0];
	saved_edge = *checked_edge;
	checked_edge->scoped_proposition_id = scoped_id;
	status = status || prototype_judgement_db_rebuild_index(&judgement) == 0;
	*checked_edge = saved_edge;
	status = status || prototype_judgement_db_rebuild_index(&judgement) != 0;
	free(premises);
	return status ? -1 : 0;
}

static int check_principal_operation_projection(void) {
	struct prototype_term term_storage[32];
	struct prototype_match_case term_case_storage[1];
	int term_case_label_storage[1];
	struct prototype_case_binder term_case_binder_storage[1];
	struct prototype_ih_scope term_ih_scope_storage[1];
	struct prototype_term_db term_db;
	prototype_term_db_init(
		&term_db,
		term_storage,
		32,
		term_case_storage,
		term_case_label_storage,
		1,
		term_case_binder_storage,
		1,
		term_ih_scope_storage,
		1
	);
	uint32_t text_type;
	uint32_t text_literal;
	if (prototype_term_primitive_text(&term_db, &text_type) != 0 ||
		prototype_term_text_literal(&term_db, 0, &text_literal) != 0) {
		return -1;
	}
	struct prototype_operation_node operation_storage[1];
	struct prototype_operation_match_case case_storage[1];
	struct prototype_operation_computation_fold_clause fold_storage[1];
	struct prototype_operation_graph operations;
	memset(operation_storage, 0xff, sizeof(operation_storage));
	prototype_operation_graph_init(
		&operations,
		operation_storage,
		1,
		case_storage,
		1,
		fold_storage,
		1
	);
	operations.operation_count = 1;
	operation_storage[0].tag = PROTOTYPE_OPERATION_ATOM;
	operation_storage[0].context_id = 0;
	operation_storage[0].core_term = text_literal;
	operation_storage[0].classifier = text_type;

	struct prototype_judgement_proposition propositions[1];
	struct prototype_judgement_claim claims[1];
	struct prototype_judgement_derivation derivations[1];
	struct prototype_judgement_db judgement;
	memset(propositions, 0, sizeof(propositions));
	memset(claims, 0, sizeof(claims));
	memset(derivations, 0, sizeof(derivations));
	if (prototype_judgement_project_principal_operation_proposition(
			&operations, 0, &propositions[0]
		) != 0) {
		return -1;
	}
	claims[0].proposition_id = 0;
	claims[0].closure_rank = 0;
	derivations[0].proof_kind = PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO;
	derivations[0].conclusion_claim_id = 0;
	derivations[0].closure_rank = 0;
	derivations[0].semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	derivations[0].semantic_action_id = PROTOTYPE_INVALID_ID;
	prototype_judgement_db_init(
		&judgement,
		propositions,
		NULL,
		claims,
		derivations,
		1,
		NULL,
		0,
		NULL,
		0
	);
	judgement.proposition_count = 1;
	judgement.claim_count = 1;
	judgement.derivation_count = 1;
	if (prototype_judgement_db_rebuild_index(&judgement) != 0) {
		return -1;
	}
	size_t proposition_count = judgement.proposition_count;
	size_t claim_count = judgement.claim_count;
	size_t derivation_count = judgement.derivation_count;
	uint64_t proposition_hits = judgement.proposition_intern_hits;
	uint64_t claim_hits = judgement.claim_intern_hits;
	uint64_t derivation_hits = judgement.derivation_intern_hits;
	struct prototype_judgement_principal_operation_audit audit;
	if (prototype_judgement_audit_principal_operation_claims(
			&term_db, &operations, &judgement, &audit
		) != 0 || audit.principal_operation_count != 1 ||
		audit.proposition_count != 1 || audit.accepted_claim_count != 1 ||
		audit.derivation_count != 1 ||
		audit.total_proposition_count != proposition_count ||
		audit.total_accepted_claim_count != claim_count ||
		audit.claim_wrapper_bytes != sizeof(claims[0]) ||
		audit.proof_role_counts[
			PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_PRINCIPAL
		] != 1 || judgement.proposition_count != proposition_count ||
		judgement.claim_count != claim_count ||
		judgement.derivation_count != derivation_count ||
		judgement.proposition_intern_hits != proposition_hits ||
		judgement.claim_intern_hits != claim_hits ||
		judgement.derivation_intern_hits != derivation_hits) {
		return -1;
	}
	struct prototype_judgement_proposition scoped = propositions[0];
	scoped.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
	scoped.authority_id = 3;
	scoped.operation_id = PROTOTYPE_INVALID_ID;
	struct prototype_judgement_proposition non_operation = scoped;
	non_operation.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION;
	int status = prototype_judgement_proof_reconstruction_role(
			PROTOTYPE_JUDGEMENT_PROOF_CONVERSION, &propositions[0]
		) != PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_DERIVED_OPERATION ||
		prototype_judgement_proof_reconstruction_role(
			PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION, &scoped
		) != PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_SCOPED ||
		prototype_judgement_proof_reconstruction_role(
			PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO, &non_operation
		) != PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_NON_OPERATION;
	for (int proof_kind = PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO;
		proof_kind <= PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION;
		++proof_kind) {
		status = status || prototype_judgement_proof_reconstruction_role(
			proof_kind, &non_operation
		) == PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_INVALID;
	}
	propositions[0].classifier++;
	status = status || prototype_judgement_db_rebuild_index(&judgement) != 0 ||
		prototype_judgement_audit_principal_operation_claims(
			&term_db, &operations, &judgement, &audit
		) == 0;
	return status ? -1 : 0;
}

static int check_comprehension_action_collisions(void) {
	enum { ACTION_COUNT = PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT + 9 };
	static struct prototype_term term_storage[ACTION_COUNT + 8];
	static struct prototype_match_case case_storage[1];
	static int case_labels[1];
	static struct prototype_case_binder case_binders[1];
	static struct prototype_ih_scope frames[1];
	static struct prototype_context context_storage[ACTION_COUNT * 2 + 1];
	static struct prototype_substitution substitution_storage[ACTION_COUNT * 3 + 1];
	static struct prototype_context_db contexts;
	static struct prototype_substitution_db substitutions;
	struct prototype_term_db terms;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_type_declaration type_storage[1];
	struct prototype_type_constructor_declaration constructor_storage[1];
	struct prototype_type_parameter_declaration parameter_storage[1];
	uint32_t field_type_storage[1];
	struct prototype_type_expr type_expr_storage[1];
	uint32_t sources[ACTION_COUNT];
	uint32_t target_bindings[ACTION_COUNT];
	uint32_t targets[ACTION_COUNT];
	uint32_t lifted[ACTION_COUNT];
	uint32_t int_type;
	uint32_t base_substitution;

	prototype_term_db_init(
		&terms,
		term_storage,
		ACTION_COUNT + 8,
		case_storage,
		case_labels,
		1,
		case_binders,
		1,
		frames,
		1
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_storage,
		1,
		constructor_storage,
		1,
		parameter_storage,
		1,
		field_type_storage,
		1,
		type_expr_storage,
		1
	);
	prototype_context_db_init(
		&contexts, context_storage, ACTION_COUNT * 2 + 1
	);
	prototype_substitution_db_init(
		&substitutions,
		substitution_storage,
		ACTION_COUNT * 3 + 1
	);
	if (prototype_term_primitive_int(&terms, &int_type) != 0 ||
		prototype_substitution_identity(
			&substitutions,
			&contexts,
			prototype_context_empty(&contexts),
			&base_substitution
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ACTION_COUNT; ++i) {
		if (prototype_context_extend(
				&contexts,
				prototype_context_empty(&contexts),
				ACTION_COUNT + i,
				int_type,
				PROTOTYPE_INVALID_ID,
				&sources[i]
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ACTION_COUNT; ++i) {
		if (prototype_context_comprehension_action(
				&contexts,
				&substitutions,
				&terms,
				&type_declarations,
				sources[i],
				base_substitution,
				&target_bindings[i],
				&targets[i],
				&lifted[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t binding_count = terms.next_binding_id;
	uint32_t context_count = (uint32_t)contexts.context_count;
	uint32_t substitution_count = (uint32_t)substitutions.substitution_count;
	for (uint32_t i = 0; i < ACTION_COUNT; ++i) {
		uint32_t repeated_binding;
		uint32_t repeated_target;
		uint32_t repeated_lifted;
		if (prototype_context_comprehension_action(
				&contexts,
				&substitutions,
				&terms,
				&type_declarations,
				sources[i],
				base_substitution,
				&repeated_binding,
				&repeated_target,
				&repeated_lifted
			) != 0 || repeated_binding != target_bindings[i] ||
			repeated_target != targets[i] || repeated_lifted != lifted[i]) {
			return -1;
		}
	}
	return terms.next_binding_id != binding_count ||
		contexts.context_count != context_count ||
		substitutions.substitution_count != substitution_count ||
		contexts.pullback_hits != ACTION_COUNT ||
		contexts.pullback_probes <= contexts.pullback_hits ||
		prototype_context_comprehension_actions_validate(
			&contexts, &substitutions
		) != 0 ? -1 : 0;
}

int main(void) {
	struct prototype_term terms[128];
	struct prototype_match_case cases[8];
	int case_labels[8];
	struct prototype_case_binder case_binders[8];
	struct prototype_ih_scope frames[8];
	struct prototype_term_db term_db;
	struct prototype_context_db contexts;
	struct prototype_context context_storage[16];
	struct prototype_substitution_db substitutions;
	struct prototype_substitution substitution_storage[32];
	struct prototype_type_declaration_db type_declarations;
	struct prototype_type_declaration type_storage[4];
	struct prototype_type_constructor_declaration constructor_storage[4];
	struct prototype_type_parameter_declaration parameter_storage[4];
	uint32_t field_type_storage[8];
	struct prototype_type_expr type_expr_storage[8];
	uint32_t int_type;
	uint32_t text_type;
	uint32_t int_context;
	uint32_t same_int_context;
	uint32_t repeated_int_context;
	uint32_t text_context;
	uint32_t nested_context;
	uint32_t dependent_context;
	uint32_t unresolved_left;
	uint32_t unresolved_right;
	uint32_t empty_substitution;
	uint32_t section;
	uint32_t dependent_section;
	uint32_t identity;
	uint32_t composed;
	uint32_t projection;
	uint32_t identity_projection_path;
	uint32_t one_step_projection_path;
	uint32_t nested_projection_path;
	uint32_t variable;
	uint32_t literal;
	uint32_t constant_family;
	uint32_t dependent_classifier;
	uint32_t reindexed;
	uint32_t composed_reindexed;
	uint32_t dependent_reindexed;
	uint32_t cloned_binders[2];
	uint32_t cloned_binder_count;
	uint32_t cloned_context;
	uint32_t clone_substitution;
	uint32_t cloned_classifier;
	uint32_t empty_identity;
	uint32_t right_identity_composed;
	uint32_t identity_squared;
	uint32_t associative_left;
	uint32_t associative_right;
	uint32_t law_reindexed;
	struct prototype_operation_node operation_storage[3];
	struct prototype_operation_match_case operation_case_storage[1];
	struct prototype_operation_computation_fold_clause operation_fold_clause_storage[1];
	struct prototype_operation_graph operation_graph;
	struct prototype_operation_node int_occurrence;
	struct prototype_operation_node text_occurrence;
	struct prototype_operation_node invalid_occurrence;
	struct prototype_operation_node saturated_effect_occurrence;
	uint32_t int_operation;
	uint32_t text_operation;
	uint32_t effect_operation;
	uint32_t effect_application;
	uint32_t found_context;

	prototype_term_db_init(
		&term_db,
		terms,
		128,
		cases,
		case_labels,
		8,
		case_binders,
		8,
		frames,
		8
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_storage,
		4,
		constructor_storage,
		4,
		parameter_storage,
		4,
		field_type_storage,
		8,
		type_expr_storage,
		8
	);
	prototype_context_db_init(&contexts, context_storage, 16);
	prototype_substitution_db_init(
		&substitutions, substitution_storage, 32
	);
	if (prototype_term_primitive_int(&term_db, &int_type) != 0 ||
		prototype_term_primitive_text(&term_db, &text_type) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, int_type, PROTOTYPE_INVALID_ID, &int_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 99, int_type, PROTOTYPE_INVALID_ID, &same_int_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, int_type, PROTOTYPE_INVALID_ID, &repeated_int_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, text_type, PROTOTYPE_INVALID_ID, &text_context
		) != 0 ||
		prototype_context_extend(
			&contexts, int_context, 1, text_type, PROTOTYPE_INVALID_ID,
			&nested_context
		) != 0 ||
		prototype_term_var(&term_db, 0, &variable) != 0 ||
		prototype_term_lambda(&term_db, 0, int_type, &constant_family) != 0 ||
		prototype_term_app(
			&term_db, constant_family, variable, &dependent_classifier
		) != 0 ||
		prototype_context_extend(
			&contexts,
			int_context,
			1,
			dependent_classifier,
			PROTOTYPE_INVALID_ID,
			&dependent_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, PROTOTYPE_INVALID_ID, 17, &unresolved_left
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, PROTOTYPE_INVALID_ID, 18, &unresolved_right
		) != 0) {
		fprintf(stderr, "failed to construct categorical contexts\n");
		return 1;
	}
	if (prototype_context_empty(&contexts) != 0 ||
		int_context == same_int_context ||
		int_context != repeated_int_context ||
		prototype_context_get(&contexts, same_int_context)->binding_id != 99 ||
		int_context == text_context ||
		unresolved_left == unresolved_right ||
		!prototype_context_contains_binding(&contexts, nested_context, 0) ||
		!prototype_context_contains_binding(&contexts, nested_context, 1) ||
		prototype_context_contains_binding(&contexts, text_context, 1) ||
		prototype_context_find_binding(&contexts, nested_context, 0, &found_context) != 0 ||
		found_context != int_context ||
		prototype_context_find_binding(&contexts, nested_context, 1, &found_context) != 0 ||
		found_context != nested_context ||
		prototype_context_find_binding(&contexts, nested_context, 99, &found_context) != 1 ||
		prototype_context_find_binding(&contexts, 0, 0, &found_context) != 1 ||
		prototype_context_get(&contexts, nested_context)->depth != 2 ||
		prototype_context_db_validate(&contexts, &term_db) != 0) {
		fprintf(stderr, "categorical context law failed\n");
		return 1;
	}
	if (prototype_term_int_literal(&term_db, 7, &literal) != 0 ||
		prototype_substitution_empty(
			&substitutions, &contexts, 0, &empty_substitution
		) != 0 ||
		prototype_substitution_extend(
			&substitutions,
			&contexts,
			&term_db,
			&type_declarations,
			empty_substitution,
			int_context,
			literal,
			int_type,
			&section
		) != 0 ||
		prototype_substitution_extend(
			&substitutions,
			&contexts,
			&term_db,
			&type_declarations,
			section,
			dependent_context,
			literal,
			int_type,
			&dependent_section
		) != 0 ||
		prototype_substitution_identity(
			&substitutions, &contexts, int_context, &identity
		) != 0 ||
		prototype_substitution_projection(
			&substitutions, &contexts, int_context, &projection
		) != 0 ||
		prototype_substitution_projection_path(
			&substitutions,
			&contexts,
			int_context,
			int_context,
			&identity_projection_path
		) != 0 ||
		prototype_substitution_projection_path(
			&substitutions,
			&contexts,
			int_context,
			prototype_context_empty(&contexts),
			&one_step_projection_path
		) != 0 ||
		prototype_substitution_projection_path(
			&substitutions,
			&contexts,
			nested_context,
			prototype_context_empty(&contexts),
			&nested_projection_path
		) != 0 ||
		prototype_substitution_projection_path(
			&substitutions,
			&contexts,
			int_context,
			nested_context,
			&(uint32_t){0}
		) != -1 ||
		prototype_substitution_compose(
			&substitutions, &contexts, identity, section, &composed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			section,
			&reindexed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			composed,
			&composed_reindexed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			dependent_classifier,
			section,
			&dependent_reindexed
		) != 0 ||
		prototype_context_reindex_telescope(
			&contexts,
			&substitutions,
			&term_db,
			&type_declarations,
			prototype_context_empty(&contexts),
			dependent_context,
			cloned_binders,
			2,
			&cloned_binder_count,
			&cloned_context,
			&clone_substitution
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			dependent_classifier,
			clone_substitution,
			&cloned_classifier
		) != 0 ||
		reindexed != literal ||
		composed_reindexed != literal ||
		cloned_binder_count != 2 ||
		prototype_substitution_get(
			&substitutions, clone_substitution
		)->source_context != cloned_context ||
		prototype_substitution_get(
			&substitutions, clone_substitution
		)->target_context != dependent_context ||
		!(prototype_judgement_classifier_conversion(
			&term_db,
			&type_declarations,
			dependent_reindexed,
			int_type
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		!(prototype_judgement_classifier_conversion(
			&term_db,
			&type_declarations,
			cloned_classifier,
			dependent_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		prototype_substitution_get(&substitutions, dependent_section) == NULL ||
		prototype_substitution_get(&substitutions, projection) == NULL ||
		!prototype_substitution_is_projection_path(
			&substitutions,
			&contexts,
			identity_projection_path,
			int_context,
			int_context
		) ||
		!prototype_substitution_is_projection_path(
			&substitutions,
			&contexts,
			one_step_projection_path,
			int_context,
			prototype_context_empty(&contexts)
		) ||
		!prototype_substitution_is_projection_path(
			&substitutions,
			&contexts,
			nested_projection_path,
			nested_context,
			prototype_context_empty(&contexts)
		) ||
		prototype_substitution_is_projection_path(
			&substitutions,
			&contexts,
			nested_projection_path,
			int_context,
			prototype_context_empty(&contexts)
		) ||
		prototype_substitution_db_validate_classifier_coherence(
			&substitutions, &contexts, &term_db, &type_declarations
		) != 0) {
		fprintf(stderr, "categorical substitution law failed\n");
		return 1;
	}
	{
		uint32_t cached_reindexed;
		uint32_t cached_binder_count;
		uint32_t cached_context;
		uint32_t cached_substitution;
		uint32_t binding_count_before_cache = term_db.next_binding_id;
		if (prototype_term_reindex(
				&term_db,
				&type_declarations,
				&contexts,
				&substitutions,
				dependent_classifier,
				section,
				&cached_reindexed
			) != 0 || cached_reindexed != dependent_reindexed ||
			prototype_context_reindex_telescope(
				&contexts,
				&substitutions,
				&term_db,
				&type_declarations,
				prototype_context_empty(&contexts),
				dependent_context,
				cloned_binders,
				2,
				&cached_binder_count,
				&cached_context,
				&cached_substitution
			) != 0 || cached_binder_count != cloned_binder_count ||
			cached_context != cloned_context ||
			cached_substitution != clone_substitution ||
			term_db.next_binding_id != binding_count_before_cache ||
			substitutions.reindex_hits == 0 || contexts.pullback_hits == 0) {
			fprintf(stderr, "memoized context action law failed\n");
			return 1;
		}
	}
	if (prototype_substitution_identity(
			&substitutions,
			&contexts,
			prototype_context_empty(&contexts),
			&empty_identity
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			section,
			empty_identity,
			&right_identity_composed
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity,
			identity,
			&identity_squared
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity_squared,
			section,
			&associative_left
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity,
			composed,
			&associative_right
		) != 0 || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			right_identity_composed,
			&law_reindexed
		) != 0 || law_reindexed != literal || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			associative_left,
			&law_reindexed
		) != 0 || law_reindexed != literal || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			associative_right,
			&law_reindexed
		) != 0 || law_reindexed != literal) {
		fprintf(stderr, "substitution identity/associativity law failed\n");
		return 1;
	}
	if (prototype_substitution_compose(
			&substitutions,
			&contexts,
			section,
			identity,
			&law_reindexed
		) == 0) {
		fprintf(stderr, "substitution context mismatch was accepted\n");
		return 1;
	}
	{
		enum { LARGE_CONTEXT_DEPTH = 513 };
		static struct prototype_term large_term_storage[16];
		static struct prototype_match_case large_case_storage[1];
		static int large_case_labels[1];
		static struct prototype_case_binder large_case_binders[1];
		static struct prototype_ih_scope large_frames[1];
		static struct prototype_context
			large_context_storage[LARGE_CONTEXT_DEPTH + 1];
		static struct prototype_substitution
			large_substitution_storage[LARGE_CONTEXT_DEPTH + 1];
		struct prototype_term_db large_terms;
		struct prototype_context_db large_contexts;
		struct prototype_substitution_db large_substitutions;
		uint32_t large_int_type;
		uint32_t large_literal;
		uint32_t large_context = 0;
		uint32_t large_variable;
		uint32_t large_result;

		prototype_term_db_init(
			&large_terms,
			large_term_storage,
			16,
			large_case_storage,
			large_case_labels,
			1,
			large_case_binders,
			1,
			large_frames,
			1
		);
		prototype_context_db_init(
			&large_contexts,
			large_context_storage,
			LARGE_CONTEXT_DEPTH + 1
		);
		prototype_substitution_db_init(
			&large_substitutions,
			large_substitution_storage,
			LARGE_CONTEXT_DEPTH + 1
		);
		if (prototype_term_primitive_int(&large_terms, &large_int_type) != 0 ||
			prototype_term_int_literal(&large_terms, 7, &large_literal) != 0) {
			fprintf(stderr, "failed to initialize large reindex fixture\n");
			return 1;
		}
		large_substitution_storage[0] = (struct prototype_substitution) {
			.kind = PROTOTYPE_SUBSTITUTION_EMPTY,
			.source_context = 0,
			.target_context = 0,
			.first = PROTOTYPE_INVALID_ID,
			.second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID
		};
		large_substitutions.substitution_count = 1;
		for (uint32_t i = 0; i < LARGE_CONTEXT_DEPTH; ++i) {
			uint32_t extended_context;
			if (prototype_context_extend(
					&large_contexts,
					large_context,
					1000 + i,
					large_int_type,
					PROTOTYPE_INVALID_ID,
					&extended_context
				) != 0) {
				fprintf(stderr, "failed to extend large context\n");
				return 1;
			}
			large_substitution_storage[i + 1] =
				(struct prototype_substitution) {
					.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
					.source_context = 0,
					.target_context = extended_context,
					.first = i,
					.second = PROTOTYPE_INVALID_ID,
					.term = large_literal,
					.term_classifier = large_int_type
				};
			large_substitutions.substitution_count++;
			large_context = extended_context;
		}
		uint32_t binding_count_before = large_terms.next_binding_id;
		if (prototype_term_var(
				&large_terms, 1000 + LARGE_CONTEXT_DEPTH - 1, &large_variable
			) != 0 || prototype_context_db_validate(
				&large_contexts, &large_terms
			) != 0 || prototype_substitution_db_validate_classifier_coherence(
				&large_substitutions,
				&large_contexts,
				&large_terms,
				&type_declarations
			) != 0 || prototype_term_reindex(
				&large_terms,
				&type_declarations,
				&large_contexts,
				&large_substitutions,
				large_variable,
				LARGE_CONTEXT_DEPTH,
				&large_result
			) != 0 || large_result != large_literal ||
			large_terms.next_binding_id != binding_count_before) {
			fprintf(stderr, "large direct reindex law failed\n");
			return 1;
		}
	}
	{
		struct prototype_context malformed_context_storage[1];
		struct prototype_context_db malformed_contexts;
		struct prototype_substitution malformed_substitution_storage[1];
		struct prototype_substitution_db malformed_substitutions;

		prototype_context_db_init(
			&malformed_contexts, malformed_context_storage, 1
		);
		malformed_context_storage[0].depth = 1;
		prototype_substitution_db_init(
			&malformed_substitutions, malformed_substitution_storage, 1
		);
		malformed_substitutions.substitution_count = 1;
		malformed_substitution_storage[0] = (struct prototype_substitution) {
			.kind = PROTOTYPE_SUBSTITUTION_COMPOSE,
			.source_context = prototype_context_empty(&contexts),
			.target_context = prototype_context_empty(&contexts),
			.first = 0,
			.second = 0,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID
		};
		if (prototype_context_db_validate(
				&malformed_contexts, &term_db
			) == 0 || prototype_substitution_db_validate(
				&malformed_substitutions, &contexts, &term_db
			) == 0) {
			fprintf(stderr, "malformed categorical storage was accepted\n");
			return 1;
		}
	}
	{
		struct prototype_context source_context_storage[4];
		struct prototype_context target_context_storage[4];
		struct prototype_context_db source_contexts;
		struct prototype_context_db target_contexts;
		struct prototype_substitution source_substitution_storage[4];
		struct prototype_substitution target_substitution_storage[4];
		struct prototype_substitution_db source_substitutions;
		struct prototype_substitution_db target_substitutions;
		uint32_t source_context;
		uint32_t source_empty;
		uint32_t source_section;
		uint32_t context_relocation[4];
		uint32_t substitution_relocation[4];
		uint32_t term_relocation[8];
		uint32_t binding_relocation[42];

		prototype_context_db_init(
			&source_contexts, source_context_storage, 4
		);
		prototype_context_db_init(
			&target_contexts, target_context_storage, 4
		);
		prototype_substitution_db_init(
			&source_substitutions, source_substitution_storage, 4
		);
		prototype_substitution_db_init(
			&target_substitutions, target_substitution_storage, 4
		);
		for (uint32_t i = 0; i < 8; ++i) {
			term_relocation[i] = i + 3;
		}
		for (uint32_t i = 0; i < 42; ++i) {
			binding_relocation[i] = PROTOTYPE_INVALID_ID;
		}
		binding_relocation[41] = 141;
		if (prototype_context_extend(
				&source_contexts,
				prototype_context_empty(&source_contexts),
				41,
				int_type,
				PROTOTYPE_INVALID_ID,
				&source_context
			) != 0 || prototype_substitution_empty(
				&source_substitutions,
				&source_contexts,
				source_context,
				&source_empty
			) != 0 || prototype_substitution_extend(
				&source_substitutions,
				&source_contexts,
				&term_db,
				&type_declarations,
				source_empty,
				source_context,
				literal,
				int_type,
				&source_section
			) != 0 || prototype_context_db_append_relocated(
				&target_contexts,
				&source_contexts,
				term_relocation,
				8,
				binding_relocation,
				42,
				context_relocation,
				4
			) != 0 || prototype_substitution_db_append_relocated(
				&target_substitutions,
				&source_substitutions,
				context_relocation,
				source_contexts.context_count,
				term_relocation,
				8,
				substitution_relocation,
				4
			) != 0 || source_section != 1 ||
			target_contexts.context_count != 2 ||
			target_contexts.contexts[context_relocation[source_context]].binding_id !=
				141 ||
			target_contexts.contexts[
				context_relocation[source_context]
			].classifier_ref.kind !=
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM ||
			target_contexts.contexts[
				context_relocation[source_context]
			].classifier_ref.term_id != int_type + 3 ||
			target_substitutions.substitution_count != 2 ||
			target_substitutions.substitutions[1].source_context !=
				context_relocation[source_context] ||
			target_substitutions.substitutions[1].target_context !=
				context_relocation[source_context] ||
			target_substitutions.substitutions[1].first != 0 ||
			target_substitutions.substitutions[1].term != literal + 3 ||
			target_substitutions.substitutions[1].term_classifier != int_type + 3) {
			fprintf(stderr, "context/substitution relocation law failed\n");
			return 1;
		}
	}
	{
		enum { COLLISION_ENTRY_COUNT = 1030 };
		static struct prototype_context collision_context_storage[
			COLLISION_ENTRY_COUNT + 1
		];
		static struct prototype_substitution collision_substitution_storage[
			COLLISION_ENTRY_COUNT
		];
		struct prototype_context_db collision_contexts;
		struct prototype_substitution_db collision_substitutions;
		uint32_t first_context = PROTOTYPE_INVALID_ID;
		uint32_t first_identity = PROTOTYPE_INVALID_ID;
		prototype_context_db_init(
			&collision_contexts,
			collision_context_storage,
			COLLISION_ENTRY_COUNT + 1
		);
		prototype_substitution_db_init(
			&collision_substitutions,
			collision_substitution_storage,
			COLLISION_ENTRY_COUNT
		);
		for (uint32_t i = 0; i < COLLISION_ENTRY_COUNT; ++i) {
			uint32_t context_id;
			uint32_t identity_id;
			if (prototype_context_extend(
					&collision_contexts,
					prototype_context_empty(&collision_contexts),
					1000 + i,
					int_type,
					PROTOTYPE_INVALID_ID,
					&context_id
				) != 0 || context_id != i + 1 ||
				prototype_substitution_identity(
					&collision_substitutions,
					&collision_contexts,
					context_id,
					&identity_id
				) != 0 || identity_id != i) {
				fprintf(stderr, "graph index collision insertion failed\n");
				return 1;
			}
			if (i == 0) {
				first_context = context_id;
				first_identity = identity_id;
			}
		}
		uint32_t repeated_context;
		uint32_t repeated_identity;
		if (prototype_context_extend(
				&collision_contexts,
				prototype_context_empty(&collision_contexts),
				1000,
				int_type,
				PROTOTYPE_INVALID_ID,
				&repeated_context
			) != 0 || repeated_context != first_context ||
			prototype_substitution_identity(
				&collision_substitutions,
				&collision_contexts,
				first_context,
				&repeated_identity
			) != 0 || repeated_identity != first_identity ||
			collision_contexts.intern_hits == 0 ||
			collision_substitutions.intern_hits == 0 ||
			collision_contexts.intern_probes <=
				collision_contexts.intern_hits ||
			collision_substitutions.intern_probes <=
				collision_substitutions.intern_hits) {
			fprintf(stderr, "collision-safe graph interning law failed\n");
			return 1;
		}
	}
	if (check_comprehension_action_collisions() != 0) {
		fprintf(stderr, "comprehension action collision law failed\n");
		return 1;
	}
	prototype_operation_graph_init(
		&operation_graph,
		operation_storage,
		3,
		operation_case_storage,
		1,
		operation_fold_clause_storage,
		1
	);
	memset(&int_occurrence, 0xff, sizeof(int_occurrence));
	int_occurrence.tag = PROTOTYPE_OPERATION_ATOM;
	int_occurrence.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	int_occurrence.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	int_occurrence.application_role = PROTOTYPE_TERM_APPLICATION_NONE;
	int_occurrence.context_id = int_context;
	int_occurrence.core_term = literal;
	int_occurrence.source_symbol_id = -1;
	int_occurrence.binder_symbol_id = -1;
	int_occurrence.case_count = 0;
	memset(&text_occurrence, 0xff, sizeof(text_occurrence));
	text_occurrence = int_occurrence;
	text_occurrence.context_id = text_context;
	invalid_occurrence = int_occurrence;
	invalid_occurrence.context_id = 99;
	if (prototype_operation_graph_add(
			&operation_graph, &contexts, int_occurrence, &int_operation
		) != 0 ||
		prototype_operation_graph_add(
			&operation_graph, &contexts, text_occurrence, &text_operation
		) != 0 ||
		prototype_operation_graph_add(
			&operation_graph, &contexts, invalid_occurrence, NULL
		) == 0 ||
		int_operation == text_operation ||
		operation_graph.operations[int_operation].core_term !=
			operation_graph.operations[text_operation].core_term ||
		operation_graph.operations[int_operation].context_id ==
			operation_graph.operations[text_operation].context_id ||
		prototype_operation_graph_validate(
			&operation_graph, &term_db, &contexts
		) != 0) {
		fprintf(stderr, "context-indexed operation graph law failed\n");
		return 1;
	}
	if (prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &effect_operation
		) != 0 || prototype_term_app(
			&term_db, effect_operation, literal, &effect_application
		) != 0) {
		fprintf(stderr, "failed to construct saturated effect application\n");
		return 1;
	}
	saturated_effect_occurrence = int_occurrence;
	saturated_effect_occurrence.tag = PROTOTYPE_OPERATION_APP;
	saturated_effect_occurrence.category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	saturated_effect_occurrence.computation_kind =
		PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	saturated_effect_occurrence.application_role =
		PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION;
	saturated_effect_occurrence.core_term = effect_application;
	saturated_effect_occurrence.function = int_operation;
	saturated_effect_occurrence.argument = text_operation;
	if (prototype_operation_graph_add(
			&operation_graph,
			&contexts,
			saturated_effect_occurrence,
			NULL
		) != 0 || prototype_operation_graph_validate(
			&operation_graph, &term_db, &contexts
		) == 0) {
		fprintf(stderr, "saturated effect APP escaped request validation\n");
		return 1;
	}
	if (check_judgement_graph_collisions() != 0) {
		fprintf(stderr, "judgement graph collision law failed\n");
		return 1;
	}
	if (check_judgement_premise_arenas() != 0) {
		fprintf(stderr, "judgement premise arena law failed\n");
		return 1;
	}
	if (check_principal_operation_projection() != 0) {
		fprintf(stderr, "principal operation projection law failed\n");
		return 1;
	}
	printf("context category checks passed\n");
	return 0;
}
