#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"
#include "../support/compiler_session_storage.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int root_view_is_strictly_ordered(
	const struct prototype_compilation_image_root_view* view
) {
	if (!view || (view->count != 0 && !view->roots)) return 0;
	for (size_t i = 1; i < view->count; ++i) {
		const struct prototype_compilation_image_root* left = &view->roots[i - 1];
		const struct prototype_compilation_image_root* right = &view->roots[i];
		if (left->kind > right->kind ||
			(left->kind == right->kind && left->id >= right->id)) {
			return 0;
		}
	}
	return 1;
}

int main(void) {
	static const char source[] =
		"Bool := @{ false : *; true : *; };\n"
		"id := \\b : Bool => b;\n"
		"main := id Bool.true;\n";
	struct prototype_program_storage storage;
	struct prototype_program_storage uninterrupted;
	struct prototype_read_error error;
	struct prototype_compilation_image* image = NULL;
	struct prototype_compilation_image_report report;
	int result = 1;
	int uninterrupted_initialized = 0;
	struct prototype_effort_account split_account;
	static struct prototype_typing_seed_state seed_state;
	struct prototype_typing_seed_mark seed_mark;
	uint64_t split_credits[PROTOTYPE_EFFORT_PHASE_COUNT] = { 0 };
	split_credits[PROTOTYPE_EFFORT_PHASE_GRAPH] = 1;
	split_credits[PROTOTYPE_EFFORT_PHASE_PROOF] = 1;
	prototype_effort_account_init(&split_account, 1);
	if (prototype_effort_consume_split(&split_account, split_credits) != 1 ||
		split_account.used != 0 || prototype_effort_add_credits(
			&split_account, 1
		) != 0 || prototype_effort_consume_split(
			&split_account, split_credits
		) != 0 || split_account.used != 2) {
		fprintf(stderr, "split effort charge was not atomic\n");
		return 1;
	}
	prototype_typing_seed_state_init(&seed_state);
	seed_state.binder_input_count = 1;
	if (prototype_typing_seed_state_mark(&seed_state, &seed_mark) != 0) {
		fprintf(stderr, "typing seed mark failed\n");
		return 1;
	}
	seed_state.binder_input_count = 2;
	seed_state.match_goal_count = 1;
	if (prototype_typing_seed_state_truncate(&seed_state, &seed_mark) != 0 ||
		seed_state.binder_input_count != 1 ||
		seed_state.match_goal_count != 0) {
		fprintf(stderr, "typing seed truncation failed\n");
		return 1;
	}
	memset(&error, 0, sizeof(error));
	if (prototype_program_storage_init(&storage) != 0 ||
		prototype_read_ast_string(
			"<producer-image>", source, &storage.private->program, &error
		) != 0) {
		fprintf(stderr, "producer image setup failed: %s\n", error.message);
		return 1;
	}
	struct prototype_typing_constraint_db* constraint_db =
		&storage.private->typing->constraints;
	struct prototype_typing_constraint duplicate = {
		.domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER,
		.kind = OPERATION_CONSTRAINT_HAS_TYPE,
		.owner_typed_projection = 1,
		.source_occurrence = 2,
		.source_ast = 3,
		.origin_constraint_id = PROTOTYPE_INVALID_ID,
		.classifier_rhs_root = PROTOTYPE_INVALID_ID
	};
	struct prototype_typing_equation_operand duplicate_operand = {
		.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STRUCTURAL,
		.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_TYPED_PROJECTION,
		.target = 2,
		.qualifier = 0
	};
	uint32_t first_constraint;
	uint32_t duplicate_constraint;
	int constraint_init_status =
		prototype_typing_constraint_db_init(constraint_db);
	int constraint_intern_status = constraint_init_status == 0 ?
		prototype_typing_constraint_intern(
			constraint_db, &duplicate, &duplicate_operand, 1,
			&first_constraint
		) : -1;
	if (constraint_init_status != 0 || constraint_intern_status != 0) {
		fprintf(stderr, "constraint interning setup failed init=%d intern=%d\n",
			constraint_init_status, constraint_intern_status);
		return 1;
	}
	struct prototype_typing_constraint_db_mark constraint_mark =
		prototype_typing_constraint_db_mark(constraint_db);
	duplicate.source_ast = 4;
	if (prototype_typing_constraint_intern(
			constraint_db, &duplicate, &duplicate_operand, 1,
			&duplicate_constraint
		) != 0 || duplicate_constraint != first_constraint ||
		constraint_db->constraint_count != 1 ||
		constraint_db->constraint_intern_hits != 1) {
		fprintf(stderr, "constraint key retained provenance or missed a duplicate\n");
		return 1;
	}
	if (prototype_typing_constraint_mark_input_changed(
			constraint_db, first_constraint
		) != 0 || constraint_db->solutions[first_constraint].input_revision != 1) {
		fprintf(stderr, "constraint input revision did not advance\n");
		return 1;
	}
	constraint_db->solutions[first_constraint].input_revision = UINT64_MAX;
	if (prototype_typing_constraint_mark_input_changed(
			constraint_db, first_constraint
		) == 0) {
		fprintf(stderr, "constraint input revision overflow was accepted\n");
		return 1;
	}
	constraint_db->solutions[first_constraint].input_revision = 1;
	constraint_db->topology_sealed = 1;
	if (prototype_typing_constraint_intern(
			constraint_db, &duplicate, &duplicate_operand, 1,
			&duplicate_constraint
		) != 0 || duplicate_constraint != first_constraint) {
		fprintf(stderr, "sealed constraint lookup did not reuse immutable topology\n");
		return 1;
	}
	constraint_db->topology_sealed = 0;
	duplicate.source_occurrence = 3;
	if (prototype_typing_constraint_intern(
			constraint_db, &duplicate, &duplicate_operand, 1,
			&duplicate_constraint
		) != 0 || duplicate_constraint == first_constraint ||
		prototype_typing_constraint_db_rollback(
			constraint_db, constraint_mark
		) != 0 || prototype_typing_constraint_intern(
			constraint_db, &duplicate, &duplicate_operand, 1,
			&duplicate_constraint
		) != 0 || duplicate_constraint != 1 ||
		prototype_typing_constraint_db_init(constraint_db) != 0) {
		fprintf(stderr, "constraint hash rollback is inconsistent\n");
		return 1;
	}
	if (prototype_program_storage_init(&uninterrupted) != 0 ||
		prototype_read_ast_string(
			"<producer-uninterrupted>", source, &uninterrupted.private->program, &error
		) != 0) {
		fprintf(stderr, "uninterrupted producer setup failed: %s\n", error.message);
		prototype_program_storage_destroy(&storage);
		return 1;
	}
	uninterrupted_initialized = 1;
	prototype_effort_account_init(&storage.private->metadata.effort, 0);
	if (prototype_compilation_image_create(
			&storage.private->asts,
			&storage.private->core,
			storage.private->typing,
			&storage.private->type_declarations,
			&storage.private->judgement,
			&storage.private->universe,
			&storage.private->metadata,
			&storage.private->symbols,
			storage.private->program.intrinsic_environment,
			storage.private->program.namespace_symbol_id,
			NULL,
			0,
		&image
		) != 0 || prototype_compilation_image_lower(image, &report) != 0) {
		fprintf(stderr, "atomic image lowering failed\n");
		goto cleanup;
	}
	if (report.status != PROTOTYPE_COMPILATION_IMAGE_OPEN || report.phase != 1 ||
		report.effort_used != 0 ||
		storage.private->typing->constraints.semantic_transition_count != 0 ||
		storage.private->typing->constraints.constraint_count == 0 ||
		storage.private->typing->seed.binder_input_count == 0 ||
		storage.private->typing->seed.declaration_input_count == 0 ||
		storage.private->metadata.typed_occurrences.occurrence_count == 0 ||
		storage.private->typing->context_projections.typed_projection_count == 0 ||
		!storage.private->metadata.typed_occurrences.transaction_active) {
		fprintf(stderr,
			"lowering did not return a non-empty zero-transition open image\n"
		);
		goto cleanup;
	}
	struct prototype_compilation_image_root_view seed_roots;
	struct prototype_compilation_image_root_view provenance_roots;
	struct prototype_compilation_image_root_view checkpoint_roots;
	struct prototype_compilation_image_root_view accepted_roots;
	if (prototype_compilation_image_roots(
			image,
			PROTOTYPE_COMPILATION_IMAGE_ROOT_REPLAY_SEED,
			&seed_roots
		) != 0 || prototype_compilation_image_roots(
			image,
			PROTOTYPE_COMPILATION_IMAGE_ROOT_SOURCE_PROVENANCE,
			&provenance_roots
		) != 0 || prototype_compilation_image_roots(
			image,
			PROTOTYPE_COMPILATION_IMAGE_ROOT_CHECKPOINT_PROGRESS,
			&checkpoint_roots
		) != 0 || prototype_compilation_image_roots(
			image,
			PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION,
			&accepted_roots
		) != 0 || seed_roots.count == 0 || seed_roots.revision != 1 ||
		provenance_roots.count == 0 || provenance_roots.revision != 1 ||
		checkpoint_roots.count != 0 || checkpoint_roots.revision != 0 ||
		accepted_roots.count != 0 || accepted_roots.revision != 0 ||
		!root_view_is_strictly_ordered(&seed_roots) ||
		!root_view_is_strictly_ordered(&provenance_roots)) {
		fprintf(stderr, "open image reachability roles are not separated\n");
		goto cleanup;
	}
	for (size_t root = 0; root < seed_roots.count; ++root) {
		if (seed_roots.roots[root].kind ==
				PROTOTYPE_COMPILATION_IMAGE_ROOT_EQUATION) {
			fprintf(stderr, "recompute seed retained reconstructible equation\n");
			goto cleanup;
		}
	}
	for (uint32_t equation = 0;
		equation < storage.private->typing->constraints.constraint_count;
		++equation) {
		const struct prototype_typing_constraint_solution* solution =
			&storage.private->typing->constraints.solutions[equation];
		if (solution->state != OPERATION_CONSTRAINT_STATE_PENDING ||
			solution->reason != OPERATION_CLASSIFIER_GOAL_REASON_NONE ||
			solution->answer_state != PROTOTYPE_TYPING_EQUATION_ANSWER_UNSOLVED ||
			solution->result_term != PROTOTYPE_INVALID_ID ||
			solution->evidence_id != PROTOTYPE_INVALID_ID) {
			fprintf(stderr, "open image equation %u has semantic progress\n", equation);
			goto cleanup;
		}
	}

	int saw_classifier_pause = 0;
	for (uint32_t round = 0; round < 10000; ++round) {
		uint64_t credit = report.phase >= 3 ? UINT64_C(1000000) : 1;
		if (prototype_compilation_image_solve(
				image, credit, &report
			) != 0) {
			fprintf(stderr, "image solve API failed at round %u\n", round);
			goto cleanup;
		}
		if (report.status == PROTOTYPE_COMPILATION_IMAGE_REJECTED) {
			fprintf(stderr, "producer rejected a valid program at phase %d\n", report.phase);
			goto cleanup;
		}
		if (report.status == PROTOTYPE_COMPILATION_IMAGE_PAUSED && report.phase == 2) {
			saw_classifier_pause = 1;
			if (prototype_compilation_image_roots(
					image,
					PROTOTYPE_COMPILATION_IMAGE_ROOT_CHECKPOINT_PROGRESS,
					&checkpoint_roots
				) != 0 || checkpoint_roots.count < seed_roots.count ||
				checkpoint_roots.revision == 0 ||
				!root_view_is_strictly_ordered(&checkpoint_roots)) {
				fprintf(stderr, "paused image has no canonical progress roots\n");
				goto cleanup;
			}
		}
		if (report.status == PROTOTYPE_COMPILATION_IMAGE_SOLVED) break;
	}
	if (report.status != PROTOTYPE_COMPILATION_IMAGE_SOLVED ||
		!saw_classifier_pause || report.phase != 4 ||
		storage.private->typing->constraints.semantic_transition_count == 0 ||
		storage.private->typing->constraints.semantic_transition_count_by_domain[
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		] == 0 ||
		storage.private->metadata.typed_occurrences.transaction_active ||
		!storage.private->metadata.typed_occurrences.frozen) {
		fprintf(stderr, "producer did not preserve and complete its frontier\n");
		goto cleanup;
	}
	if (prototype_compilation_image_roots(
			image,
			PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION,
			&accepted_roots
		) != 0 || accepted_roots.count == 0 || accepted_roots.revision != 1 ||
		!root_view_is_strictly_ordered(&accepted_roots)) {
		fprintf(stderr, "solved image has no accepted publication roots\n");
		goto cleanup;
	}
	struct prototype_compilation_image_seal first_seal;
	struct prototype_compilation_image_seal second_seal;
	uint64_t transitions_before_seal =
		storage.private->typing->constraints.semantic_transition_count;
	uint64_t core_revision_before_seal =
		storage.private->core.transaction_revision;
	uint64_t typing_revision_before_seal =
		storage.private->typing->transaction_revision;
	if (prototype_compilation_image_seal(image, &first_seal) != 0 ||
		prototype_compilation_image_seal(image, &second_seal) != 0 ||
		memcmp(&first_seal, &second_seal, sizeof(first_seal)) != 0 ||
		storage.private->typing->constraints.semantic_transition_count !=
			transitions_before_seal ||
		storage.private->core.transaction_revision != core_revision_before_seal ||
		storage.private->typing->transaction_revision != typing_revision_before_seal ||
		first_seal.constraint_topology_digest !=
			storage.private->typing->constraints.topology_digest ||
		first_seal.term_count != storage.private->core.terms.term_count ||
		first_seal.typed_occurrence_count !=
			storage.private->metadata.typed_occurrences.occurrence_count ||
		first_seal.claim_count != storage.private->judgement.claim_count ||
		first_seal.derivation_count !=
			storage.private->judgement.derivation_count ||
		first_seal.accepted_root_revision != accepted_roots.revision ||
		first_seal.accepted_root_count != accepted_roots.count ||
		first_seal.accepted_root_digest == 0 ||
		first_seal.accepted_evidence_digest == 0) {
		fprintf(stderr, "read-only image seal is unstable or mutated Authority\n");
		goto cleanup;
	}
	int domain_present[PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT] = { 0 };
	for (uint32_t i = 0;
		i < storage.private->typing->constraints.constraint_count;
		++i) {
		int domain = storage.private->typing->constraints.constraints[i].domain;
		if (domain <= 0 ||
			domain >= PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT) {
			fprintf(stderr, "producer emitted an invalid constraint domain\n");
			goto cleanup;
		}
		domain_present[domain] = 1;
	}
	for (int domain = 1;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		if (domain_present[domain] &&
			storage.private->typing->constraints.
				semantic_transition_count_by_domain[domain] == 0) {
			fprintf(stderr,
				"producer did not account for constraint domain %d\n", domain);
			goto cleanup;
		}
	}
	uint64_t phase_sum = 0;
	for (int phase = 0; phase < PROTOTYPE_EFFORT_PHASE_COUNT; ++phase) {
		phase_sum += storage.private->metadata.effort.phase_used[phase];
	}
	if (phase_sum != storage.private->metadata.effort.used ||
		storage.private->metadata.effort.phase_used[PROTOTYPE_EFFORT_PHASE_CLASSIFIER] == 0 ||
		storage.private->metadata.effort.phase_used[PROTOTYPE_EFFORT_PHASE_PROOF] == 0) {
		fprintf(stderr, "producer effort accounting is incomplete\n");
		goto cleanup;
	}
	prototype_effort_account_init(
		&uninterrupted.private->metadata.effort, UINT64_C(1000000)
	);
	if (prototype_ast_compile_pending_with_imports(
			&uninterrupted.private->asts,
			&uninterrupted.private->core,
			uninterrupted.private->typing,
			&uninterrupted.private->type_declarations,
			&uninterrupted.private->judgement,
			&uninterrupted.private->universe,
			&uninterrupted.private->metadata,
			&uninterrupted.private->symbols,
			uninterrupted.private->program.intrinsic_environment,
			uninterrupted.private->program.namespace_symbol_id,
			NULL,
			0
		) != 0 || storage.private->core.terms.term_count != uninterrupted.private->core.terms.term_count ||
		storage.private->core.terms.case_count != uninterrupted.private->core.terms.case_count ||
		storage.private->metadata.contexts.context_count !=
			uninterrupted.private->metadata.contexts.context_count ||
		storage.private->metadata.substitutions.substitution_count !=
			uninterrupted.private->metadata.substitutions.substitution_count ||
		storage.private->metadata.typed_occurrences.occurrence_count !=
			uninterrupted.private->metadata.typed_occurrences.occurrence_count ||
		storage.private->metadata.typed_occurrences.edge_count !=
			uninterrupted.private->metadata.typed_occurrences.edge_count ||
		storage.private->metadata.typed_occurrences.case_count !=
			uninterrupted.private->metadata.typed_occurrences.case_count ||
		storage.private->typing->constraints.constraint_count !=
			uninterrupted.private->typing->constraints.constraint_count ||
		storage.private->typing->constraints.operand_count !=
			uninterrupted.private->typing->constraints.operand_count ||
		memcmp(
			storage.private->typing->constraints.constraints,
			uninterrupted.private->typing->constraints.constraints,
			storage.private->typing->constraints.constraint_count *
				sizeof(*storage.private->typing->constraints.constraints)
		) != 0 || memcmp(
			storage.private->typing->constraints.operands,
			uninterrupted.private->typing->constraints.operands,
			storage.private->typing->constraints.operand_count *
				sizeof(*storage.private->typing->constraints.operands)
		) != 0 || memcmp(
			storage.private->typing->constraints.solutions,
			uninterrupted.private->typing->constraints.solutions,
			storage.private->typing->constraints.constraint_count *
				sizeof(*storage.private->typing->constraints.solutions)
		) != 0 ||
		memcmp(
			storage.private->core.terms.terms,
			uninterrupted.private->core.terms.terms,
			storage.private->core.terms.term_count * sizeof(*storage.private->core.terms.terms)
		) != 0 || memcmp(
			storage.private->metadata.typed_occurrences.occurrences,
			uninterrupted.private->metadata.typed_occurrences.occurrences,
			storage.private->metadata.typed_occurrences.occurrence_count *
				sizeof(*storage.private->metadata.typed_occurrences.occurrences)
		) != 0 || memcmp(
			storage.private->metadata.typed_occurrences.edges,
			uninterrupted.private->metadata.typed_occurrences.edges,
			storage.private->metadata.typed_occurrences.edge_count *
				sizeof(*storage.private->metadata.typed_occurrences.edges)
		) != 0) {
		fprintf(stderr,
			"split and uninterrupted producer content differs "
			"transitions=%llu/%llu\n",
			(unsigned long long)storage.private->typing->constraints.
				semantic_transition_count,
			(unsigned long long)uninterrupted.private->typing->constraints.
				semantic_transition_count);
		for (int domain = 1;
			domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
			++domain) {
			fprintf(stderr, "domain[%d]=%llu/%llu\n", domain,
				(unsigned long long)storage.private->typing->constraints.
					semantic_transition_count_by_domain[domain],
				(unsigned long long)uninterrupted.private->typing->constraints.
					semantic_transition_count_by_domain[domain]);
		}
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_compilation_image_destroy(image);
	if (uninterrupted_initialized) {
		prototype_program_storage_destroy(&uninterrupted);
	}
	prototype_program_storage_destroy(&storage);
	if (result == 0) puts("compilation image check passed");
	return result;
}
