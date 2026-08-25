#include "a_program/checker/module.h"
#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"
#include "a_program/graph/compile_metadata.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int semantic_projection_equal(
	const struct prototype_elaborated_module* left,
	const struct prototype_elaborated_module* right
) {
	const struct prototype_elaborated_module_view* a = &left->view;
	const struct prototype_elaborated_module_view* b = &right->view;
	if (a->calculus_fingerprint != b->calculus_fingerprint ||
		a->intrinsic_fingerprint != b->intrinsic_fingerprint ||
		a->symbols.count != b->symbols.count ||
		a->intrinsic_environment.pure_primitive_count !=
			b->intrinsic_environment.pure_primitive_count ||
		a->intrinsic_environment.effect_operation_count !=
			b->intrinsic_environment.effect_operation_count ||
		a->intrinsic_environment.default_integer_host_type !=
			b->intrinsic_environment.default_integer_host_type ||
		a->terms.term_count != b->terms.term_count ||
		a->terms.case_count != b->terms.case_count ||
		a->terms.case_binder_count != b->terms.case_binder_count ||
		a->terms.ih_scope_count != b->terms.ih_scope_count ||
		a->terms.computation_fold_clause_count !=
			b->terms.computation_fold_clause_count ||
		a->contexts.context_count != b->contexts.context_count ||
		a->substitutions.substitution_count != b->substitutions.substitution_count ||
		a->universes.level_count != b->universes.level_count ||
		a->type_schema.type_count != b->type_schema.type_count ||
		a->type_schema.constructor_count != b->type_schema.constructor_count ||
		a->dimensions.operator_count != b->dimensions.operator_count ||
		a->dimensions.image_count != b->dimensions.image_count ||
		a->occurrences.occurrence_count != b->occurrences.occurrence_count ||
		a->occurrences.edge_count != b->occurrences.edge_count ||
		a->occurrences.case_count != b->occurrences.case_count ||
		a->occurrences.fold_clause_count != b->occurrences.fold_clause_count ||
		a->contracts.contract_count != b->contracts.contract_count ||
		a->contracts.dependency_count != b->contracts.dependency_count ||
		a->interface.term_export_count != b->interface.term_export_count ||
		a->interface.type_export_count != b->interface.type_export_count ||
		a->interface.constructor_export_count !=
			b->interface.constructor_export_count ||
		a->interface.dependency_count != b->interface.dependency_count ||
		a->interface.function_graph_association_count !=
			b->interface.function_graph_association_count ||
		a->interface.function_graph_selector_group_count !=
			b->interface.function_graph_selector_group_count ||
		a->selected_entry_term != b->selected_entry_term ||
		a->selected_entry_classifier != b->selected_entry_classifier ||
		a->selected_entry_occurrence != b->selected_entry_occurrence ||
		a->required_runtime_capabilities != b->required_runtime_capabilities) {
		return 0;
	}
	for (size_t i = 0; i < a->symbols.count; ++i) {
		if (strcmp(a->symbols.strings[i], b->symbols.strings[i]) != 0) {
			return 0;
		}
	}
	return memcmp(
		left->pure_primitives,
		right->pure_primitives,
		a->intrinsic_environment.pure_primitive_count *
			sizeof(*left->pure_primitives)
	) == 0 && memcmp(
		left->effect_operations,
		right->effect_operations,
		a->intrinsic_environment.effect_operation_count *
			sizeof(*left->effect_operations)
	) == 0 && memcmp(
		left->terms,
		right->terms,
		a->terms.term_count * sizeof(*left->terms)
	) == 0 && memcmp(
		left->term_cases,
		right->term_cases,
		a->terms.case_count * sizeof(*left->term_cases)
	) == 0 && memcmp(
		left->case_binders,
		right->case_binders,
		a->terms.case_binder_count * sizeof(*left->case_binders)
	) == 0 && memcmp(
		left->ih_scopes,
		right->ih_scopes,
		a->terms.ih_scope_count * sizeof(*left->ih_scopes)
	) == 0 && memcmp(
		left->computation_fold_clauses,
		right->computation_fold_clauses,
		a->terms.computation_fold_clause_count *
			sizeof(*left->computation_fold_clauses)
	) == 0 && memcmp(
		left->contexts,
		right->contexts,
		a->contexts.context_count * sizeof(*left->contexts)
	) == 0 && memcmp(
		left->substitutions,
		right->substitutions,
		a->substitutions.substitution_count * sizeof(*left->substitutions)
	) == 0 && memcmp(
		left->universe_levels,
		right->universe_levels,
		a->universes.level_count * sizeof(*left->universe_levels)
	) == 0 && memcmp(
		left->type_declarations,
		right->type_declarations,
		a->type_schema.type_count * sizeof(*left->type_declarations)
	) == 0 && memcmp(
		left->constructor_declarations,
		right->constructor_declarations,
		a->type_schema.constructor_count *
			sizeof(*left->constructor_declarations)
	) == 0 && memcmp(
		left->dimension_operators,
		right->dimension_operators,
		a->dimensions.operator_count * sizeof(*left->dimension_operators)
	) == 0 && memcmp(
		left->dimension_images,
		right->dimension_images,
		a->dimensions.image_count * sizeof(*left->dimension_images)
	) == 0 && memcmp(
		left->occurrences,
		right->occurrences,
		a->occurrences.occurrence_count * sizeof(*left->occurrences)
	) == 0 && memcmp(
		left->occurrence_edges,
		right->occurrence_edges,
		a->occurrences.edge_count * sizeof(*left->occurrence_edges)
	) == 0 && memcmp(
		left->match_cases,
		right->match_cases,
		a->occurrences.case_count * sizeof(*left->match_cases)
	) == 0 && memcmp(
		left->fold_clauses,
		right->fold_clauses,
		a->occurrences.fold_clause_count * sizeof(*left->fold_clauses)
	) == 0 && memcmp(
		left->contracts,
		right->contracts,
		a->contracts.contract_count * sizeof(*left->contracts)
	) == 0 && memcmp(
		left->contract_dependencies,
		right->contract_dependencies,
		a->contracts.dependency_count * sizeof(*left->contract_dependencies)
	) == 0 && memcmp(
		left->term_exports,
		right->term_exports,
		a->interface.term_export_count * sizeof(*left->term_exports)
	) == 0 && memcmp(
		left->type_exports,
		right->type_exports,
		a->interface.type_export_count * sizeof(*left->type_exports)
	) == 0 && memcmp(
		left->constructor_exports,
		right->constructor_exports,
		a->interface.constructor_export_count *
			sizeof(*left->constructor_exports)
	) == 0 && memcmp(
		left->dependencies,
		right->dependencies,
		a->interface.dependency_count * sizeof(*left->dependencies)
	) == 0 && memcmp(
		left->function_graph_associations,
		right->function_graph_associations,
		a->interface.function_graph_association_count *
			sizeof(*left->function_graph_associations)
	) == 0 && memcmp(
		left->function_graph_selector_groups,
		right->function_graph_selector_groups,
		a->interface.function_graph_selector_group_count *
			sizeof(*left->function_graph_selector_groups)
	) == 0;
}

static int copy_occurrence_graph(
	const struct prototype_typed_occurrence_graph* source,
	struct prototype_typed_occurrence_graph* target
) {
	*target = *source;
	target->occurrences = NULL;
	target->cases = NULL;
	target->fold_clauses = NULL;
	if (source->occurrence_count != 0) {
		target->occurrences = malloc(
			source->occurrence_count * sizeof(*target->occurrences)
		);
	}
	if (source->case_count != 0) {
		target->cases = malloc(source->case_count * sizeof(*target->cases));
	}
	if (source->fold_clause_count != 0) {
		target->fold_clauses = malloc(
			source->fold_clause_count * sizeof(*target->fold_clauses)
		);
	}
	if ((source->occurrence_count != 0 && !target->occurrences) ||
		(source->case_count != 0 && !target->cases) ||
		(source->fold_clause_count != 0 && !target->fold_clauses)) {
		free(target->occurrences);
		free(target->cases);
		free(target->fold_clauses);
		return -1;
	}
	memcpy(
		target->occurrences,
		source->occurrences,
		source->occurrence_count * sizeof(*target->occurrences)
	);
	memcpy(
		target->cases,
		source->cases,
		source->case_count * sizeof(*target->cases)
	);
	memcpy(
		target->fold_clauses,
		source->fold_clauses,
		source->fold_clause_count * sizeof(*target->fold_clauses)
	);
	return 0;
}

static void destroy_occurrence_graph_copy(
	struct prototype_typed_occurrence_graph* graph
) {
	free(graph->occurrences);
	free(graph->cases);
	free(graph->fold_clauses);
	memset(graph, 0, sizeof(*graph));
}

int main(void) {
	static const char source[] =
		"Bool := @{ true : *; false : *; };\n"
		"identity := \\value : Bool => value;\n"
		"identity :: Bool -> Bool;\n"
		"negate := \\value : Bool =>\n"
		"  value @true => Bool.false @false => Bool.true;\n"
		"main := negate (identity Bool.false);\n";
	struct prototype_program_storage storage;
	struct prototype_read_error error;
	struct prototype_frozen_module_snapshot snapshot;
	struct prototype_frozen_module_snapshot changed_snapshot;
	struct prototype_typed_occurrence_graph changed_graph;
	struct prototype_context* changed_contexts = NULL;
	struct prototype_substitution* changed_substitutions = NULL;
	struct prototype_dimension_operator* changed_dimension_operators = NULL;
	struct prototype_type_constructor_declaration* changed_constructors = NULL;
	struct prototype_type_semantic_schema_db changed_type_schema;
	struct prototype_elaborated_module baseline;
	struct prototype_elaborated_module changed;
	struct prototype_elaborated_module invalid;
	int result = 1;

	memset(&error, 0, sizeof(error));
	memset(&changed_graph, 0, sizeof(changed_graph));
	prototype_elaborated_module_init(&baseline);
	prototype_elaborated_module_init(&changed);
	prototype_elaborated_module_init(&invalid);
	if (prototype_program_storage_init(&storage) != 0) {
		return 1;
	}
	if (prototype_read_string(
			"<checked-core-projection>", source, &storage.program, &error
		) != 0) {
		fprintf(stderr, "compile failed: %s\n", error.message);
		goto cleanup;
	}
	if (prototype_compile_metadata_frozen_snapshot(
			&storage.metadata, &snapshot
		) != 0) {
		fprintf(stderr, "completed compile did not freeze\n");
		goto cleanup;
	}
	if (prototype_elaborated_module_project(
			&storage.symbols,
			&storage.terms,
			&storage.type_declarations.semantic_schema,
			storage.program.intrinsic_environment,
			&storage.universe,
			&snapshot,
			&baseline
		) != 0) {
		fprintf(stderr, "completed compile did not project\n");
		goto cleanup;
	}
	if (baseline.view.occurrences.occurrence_count == 0 ||
		baseline.view.occurrences.case_count == 0 ||
		baseline.view.terms.term_count >= storage.terms.term_count ||
		baseline.view.symbols.count >= storage.symbols.storage.count) {
		fprintf(stderr, "completed compile did not produce a dense semantic projection\n");
		goto cleanup;
	}
	for (size_t i = 0; i < baseline.view.symbols.count; ++i) {
		if (strcmp(baseline.view.symbols.strings[i], "value") == 0) {
			fprintf(stderr, "source-only binder name entered semantic symbols\n");
			goto cleanup;
		}
	}
	int saved_term_tag = baseline.terms[0].tag;
	baseline.terms[0].tag = 0;
	if (prototype_elaborated_module_validate_structure(&baseline.view) == 0) {
		fprintf(stderr, "missing Term slot passed structural validation\n");
		goto cleanup;
	}
	baseline.terms[0].tag = saved_term_tag;
	uint32_t saved_parent = baseline.contexts[0].parent;
	baseline.contexts[0].parent = 0;
	if (prototype_elaborated_module_validate_structure(&baseline.view) == 0) {
		fprintf(stderr, "cyclic empty Context passed structural validation\n");
		goto cleanup;
	}
	baseline.contexts[0].parent = saved_parent;
	if (baseline.view.substitutions.substitution_count != 0) {
		int saved_kind = baseline.substitutions[0].kind;
		baseline.substitutions[0].kind = PROTOTYPE_SUBSTITUTION_COMPOSE;
		if (prototype_elaborated_module_validate_structure(&baseline.view) == 0) {
			fprintf(stderr, "forged Substitution payload passed validation\n");
			goto cleanup;
		}
		baseline.substitutions[0].kind = saved_kind;
	}
	uint32_t saved_result_classifier =
		baseline.constructor_declarations[0].result_classifier;
	baseline.constructor_declarations[0].result_classifier = PROTOTYPE_INVALID_ID;
	if (prototype_elaborated_module_validate_structure(&baseline.view) == 0) {
		fprintf(stderr, "missing constructor classifier passed validation\n");
		goto cleanup;
	}
	baseline.constructor_declarations[0].result_classifier =
		saved_result_classifier;

	if (copy_occurrence_graph(&snapshot.typed_occurrences, &changed_graph) != 0) {
		goto cleanup;
	}
	changed_contexts = malloc(
		snapshot.contexts.context_count * sizeof(*changed_contexts)
	);
	if (snapshot.substitutions.substitution_count != 0) {
		changed_substitutions = malloc(
			snapshot.substitutions.substitution_count *
				sizeof(*changed_substitutions)
		);
	}
	if (snapshot.dimension_operators.operator_count != 0) {
		changed_dimension_operators = malloc(
			snapshot.dimension_operators.operator_count *
				sizeof(*changed_dimension_operators)
		);
	}
	if (storage.type_declarations.semantic_schema.constructor_count != 0) {
		changed_constructors = malloc(
			storage.type_declarations.semantic_schema.constructor_count *
				sizeof(*changed_constructors)
		);
	}
	if (!changed_contexts ||
		(snapshot.substitutions.substitution_count != 0 &&
		 !changed_substitutions) ||
		(snapshot.dimension_operators.operator_count != 0 &&
		 !changed_dimension_operators) ||
		(storage.type_declarations.semantic_schema.constructor_count != 0 &&
		 !changed_constructors)) {
		goto cleanup;
	}
	memcpy(
		changed_contexts,
		snapshot.contexts.contexts,
		snapshot.contexts.context_count * sizeof(*changed_contexts)
	);
	if (snapshot.substitutions.substitution_count != 0) {
		memcpy(
			changed_substitutions,
			snapshot.substitutions.substitutions,
			snapshot.substitutions.substitution_count *
				sizeof(*changed_substitutions)
		);
	}
	if (snapshot.dimension_operators.operator_count != 0) {
		memcpy(
			changed_dimension_operators,
			snapshot.dimension_operators.operators,
			snapshot.dimension_operators.operator_count *
				sizeof(*changed_dimension_operators)
		);
	}
	if (storage.type_declarations.semantic_schema.constructor_count != 0) {
		memcpy(
			changed_constructors,
			storage.type_declarations.semantic_schema.constructor_declarations,
			storage.type_declarations.semantic_schema.constructor_count *
				sizeof(*changed_constructors)
		);
	}
	for (size_t i = 0; i < snapshot.contexts.context_count; ++i) {
		changed_contexts[i].depth ^= UINT32_C(0x55555555);
		changed_contexts[i].key_hash ^= UINT64_C(0x13579bdf2468ace0);
		changed_contexts[i].hash_next ^= UINT32_C(0x77777777);
	}
	for (size_t i = 0; i < snapshot.substitutions.substitution_count; ++i) {
		changed_substitutions[i].key_hash ^= UINT64_C(0x2468ace013579bdf);
		changed_substitutions[i].hash_next ^= UINT32_C(0x44444444);
	}
	for (size_t i = 0; i < snapshot.dimension_operators.operator_count; ++i) {
		changed_dimension_operators[i].key_hash ^=
			UINT64_C(0x1020304050607080);
		changed_dimension_operators[i].hash_next ^= UINT32_C(0x12121212);
	}
	for (size_t i = 0;
		i < storage.type_declarations.semantic_schema.constructor_count;
		++i) {
		changed_constructors[i].schema_revision ^= UINT32_C(0x34343434);
	}
	for (size_t i = 0; i < changed_graph.occurrence_count; ++i) {
		changed_graph.occurrences[i].source_ast ^= UINT32_C(0x5a5a5a5a);
		changed_graph.occurrences[i].source_symbol_id ^= 0x13579;
		changed_graph.occurrences[i].binder_symbol_id ^= 0x2468a;
		changed_graph.occurrences[i].referenced_ast_binder_id ^=
			UINT32_C(0xa5a5a5a5);
		changed_graph.occurrences[i].fold_return_ast_binder_id ^=
			UINT32_C(0x0f0f0f0f);
	}
	for (size_t i = 0; i < changed_graph.case_count; ++i) {
		changed_graph.cases[i].case_label_symbol_id ^= 0x55aa;
		for (size_t j = 0; j < changed_graph.cases[i].binder_count; ++j) {
			changed_graph.cases[i].ast_binder_ids[j] ^= UINT32_C(0x33333333);
		}
	}
	for (size_t i = 0; i < changed_graph.fold_clause_count; ++i) {
		changed_graph.fold_clauses[i].argument_ast_binder_id ^=
			UINT32_C(0x11111111);
		changed_graph.fold_clauses[i].continuation_ast_binder_id ^=
			UINT32_C(0x22222222);
	}
	changed_snapshot = snapshot;
	changed_snapshot.typed_occurrences = changed_graph;
	changed_snapshot.contexts.contexts = changed_contexts;
	changed_snapshot.substitutions.substitutions = changed_substitutions;
	changed_snapshot.dimension_operators.operators = changed_dimension_operators;
	changed_type_schema = storage.type_declarations.semantic_schema;
	changed_type_schema.semantic_revision ^= UINT64_C(0x1122334455667788);
	changed_type_schema.constructor_declarations = changed_constructors;
	if (prototype_elaborated_module_project(
			&storage.symbols,
			&storage.terms,
			&changed_type_schema,
			storage.program.intrinsic_environment,
			&storage.universe,
			&changed_snapshot,
			&changed
		) != 0 || !semantic_projection_equal(&baseline, &changed)) {
		fprintf(stderr, "source provenance changed semantic projection\n");
		goto cleanup;
	}

	changed_graph.occurrences[0].classifier_status =
		PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING;
	if (prototype_elaborated_module_project(
			&storage.symbols,
			&storage.terms,
			&changed_type_schema,
			storage.program.intrinsic_environment,
			&storage.universe,
			&changed_snapshot,
			&invalid
		) == 0) {
		fprintf(stderr, "pending classifier projected as completed semantics\n");
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_elaborated_module_destroy(&invalid);
	prototype_elaborated_module_destroy(&changed);
	prototype_elaborated_module_destroy(&baseline);
	destroy_occurrence_graph_copy(&changed_graph);
	free(changed_contexts);
	free(changed_substitutions);
	free(changed_dimension_operators);
	free(changed_constructors);
	prototype_program_storage_destroy(&storage);
	return result;
}
