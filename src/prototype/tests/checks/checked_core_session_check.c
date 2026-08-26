#include "a_program/checker/module.h"
#include "a_program/checker/module_set.h"
#include "a_program/checker/parallel.h"
#include "a_program/checker/session.h"
#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/producer/checked_incremental.h"
#include "a_program/producer/merge.h"

#include "src/checker/module_set_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int project_source(
	const char* name,
	const char* source,
	struct prototype_program_storage* storage,
	struct prototype_elaborated_module* module
) {
	struct prototype_read_error error;
	struct prototype_frozen_module_snapshot snapshot;
	memset(&error, 0, sizeof(error));
	if (prototype_program_storage_init(storage) != 0) {
		return -1;
	}
	if (prototype_read_string(name, source, &storage->program, &error) != 0) {
		fprintf(stderr, "%s: compile failed: %s\n", name, error.message);
		return -1;
	}
	if (prototype_compile_metadata_frozen_snapshot(
			&storage->metadata, &snapshot
		) != 0 || prototype_elaborated_module_project(
			&storage->symbols,
			&storage->terms,
			&storage->type_declarations.semantic_schema,
			storage->program.intrinsic_environment,
			&storage->universe,
			&snapshot,
			module
		) != 0) {
		fprintf(stderr, "%s: semantic projection failed\n", name);
		return -1;
	}
	return 0;
}

static int project_file(
	const char* path,
	struct prototype_program_storage* storage,
	struct prototype_elaborated_module* module
) {
	struct prototype_read_error error;
	struct prototype_frozen_module_snapshot snapshot;
	memset(&error, 0, sizeof(error));
	if (prototype_program_storage_init(storage) != 0) {
		return -1;
	}
	if (prototype_read_file(path, &storage->program, &error) != 0) {
		fprintf(stderr, "%s: compile failed: %s\n", path, error.message);
		return -1;
	}
	if (prototype_compile_metadata_frozen_snapshot(
			&storage->metadata, &snapshot
		) != 0 || prototype_elaborated_module_project(
			&storage->symbols,
			&storage->terms,
			&storage->type_declarations.semantic_schema,
			storage->program.intrinsic_environment,
			&storage->universe,
			&snapshot,
			module
		) != 0) {
		fprintf(stderr, "%s: semantic projection failed\n", path);
		return -1;
	}
	return 0;
}

static int check_complete_fragment(void) {
	static const char source[] =
		"Bool := @{ true : *; false : *; };\n"
		"Counter := @{ zero : *; succ : * -> *; };\n"
		"identity := \\value : Bool => value;\n"
		"boolResult := identity Bool.true;\n"
		"identityCounter := \\value : Counter => value;\n"
		"main := identityCounter (Counter.succ Counter.zero);\n";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;
	int storage_initialized = 0;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	prototype_elaborated_module_init(&module);
	if (project_source(
			"<checked-core-session>", source, &storage, &module
		) != 0) {
		goto cleanup;
	}
	storage_initialized = 1;
	/* The checked-Core projection owns its full semantic closure. Destroying the
	 * producer also erases JudgementDB, accepted Derivations, solver state, and
	 * normalization caches before independent checking begins. */
	prototype_program_storage_destroy(&storage);
	storage_initialized = 0;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked ||
		prototype_checked_module_elaborated_view(checked) != &module.view) {
		fprintf(
			stderr,
			"supported fragment did not complete: status=%d reason=%d subject=%u\n",
			report.status,
			report.stop_reason,
			report.subject
		);
		if (report.subject < module.view.occurrences.occurrence_count) {
			const struct prototype_semantic_occurrence* occurrence =
				&module.view.occurrences.occurrences[report.subject];
			fprintf(stderr, "occurrence kind=%d core=%u classifier=%u role=%d binding=%u binder=%u edges=%u+%u\n",
				occurrence->kind, occurrence->core_term,
				occurrence->asserted_classifier, occurrence->application_role,
				occurrence->binding_id, occurrence->binder_classifier,
				occurrence->first_edge, occurrence->edge_count);
			for (uint32_t i = 0; i < occurrence->edge_count; ++i) {
				const struct prototype_semantic_occurrence_edge* edge =
					&module.view.occurrences.edges[occurrence->first_edge + i];
				const struct prototype_semantic_occurrence* child =
					&module.view.occurrences.occurrences[edge->child_occurrence];
				fprintf(stderr, "child=%u role=%d core=%u classifier=%u\n",
					edge->child_occurrence, edge->role, child->core_term,
					child->asserted_classifier);
			}
		}
		goto cleanup;
	}
	if (prototype_checked_module_export_count(
			checked, PROTOTYPE_CHECKED_EXPORT_TERM
		) != module.view.interface.term_export_count ||
		prototype_checked_module_export_count(
			checked, PROTOTYPE_CHECKED_EXPORT_TYPE
		) != module.view.interface.type_export_count ||
		prototype_checked_module_export_count(
			checked, PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR
		) != module.view.interface.constructor_export_count) {
		fprintf(stderr, "checked export capability count mismatch\n");
		goto cleanup;
	}
	const struct prototype_checked_export_ref* checked_export =
		prototype_checked_module_export_at(
			checked, PROTOTYPE_CHECKED_EXPORT_TERM, 0
		);
	const struct prototype_semantic_term_export* term_export =
		prototype_checked_term_export(checked_export);
	if (!term_export || term_export->term !=
		module.view.interface.term_exports[0].term ||
		prototype_checked_type_export(checked_export) != NULL) {
		fprintf(stderr, "checked export capability did not preserve its kind\n");
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;
	if (module.view.universes.level_count == 0) {
		fprintf(stderr, "fixture has no Universe solution\n");
		goto cleanup;
	}
	int saved_universe_value = module.universe_levels[0].value;
	module.universe_levels[0].value = saved_universe_value + 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked ||
		report.stop_reason != PROTOTYPE_CHECKER_STOP_UNIVERSE) {
		fprintf(stderr, "forged Universe solution minted checked authority\n");
		goto cleanup;
	}
	module.universe_levels[0].value = saved_universe_value;

	uint32_t universe_term = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.terms.term_count; ++i) {
		if (module.terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
			universe_term = i;
			break;
		}
	}
	if (universe_term == PROTOTYPE_INVALID_ID ||
		module.view.occurrences.occurrence_count == 0) {
		fprintf(stderr, "fixture cannot form a positive Universe cycle\n");
		goto cleanup;
	}
	uint32_t saved_cycle_core = module.occurrences[0].core_term;
	uint32_t saved_cycle_classifier =
		module.occurrences[0].asserted_classifier;
	module.occurrences[0].core_term = universe_term;
	module.occurrences[0].asserted_classifier = universe_term;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked ||
		report.stop_reason != PROTOTYPE_CHECKER_STOP_UNIVERSE) {
		fprintf(stderr, "positive Universe cycle minted checked authority\n");
		goto cleanup;
	}
	module.occurrences[0].core_term = saved_cycle_core;
	module.occurrences[0].asserted_classifier = saved_cycle_classifier;

	prototype_effort_account_init(&effort, 0);
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_PAUSED ||
		report.stop_reason != PROTOTYPE_CHECKER_STOP_EFFORT || checked) {
		fprintf(stderr, "effort exhaustion minted checked authority\n");
		goto cleanup;
	}
	if (!effort.exhausted || effort.exhausted_phase !=
		PROTOTYPE_EFFORT_PHASE_CHECKER || effort.used != 0) {
		fprintf(stderr, "checker pause was not attributed to typed effort\n");
		goto cleanup;
	}

	uint32_t variable_occurrence = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.occurrences.occurrence_count; ++i) {
		if (module.occurrences[i].kind == PROTOTYPE_SEMANTIC_OCCURRENCE_VAR) {
			variable_occurrence = i;
			break;
		}
	}
	if (variable_occurrence == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "fixture has no variable occurrence\n");
		goto cleanup;
	}
	uint32_t saved_classifier =
		module.occurrences[variable_occurrence].asserted_classifier;
	module.occurrences[variable_occurrence].asserted_classifier =
		module.type_declarations[0].formation_classifier;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged classifier minted checked authority\n");
		goto cleanup;
	}
	module.occurrences[variable_occurrence].asserted_classifier = saved_classifier;
	if (module.view.intrinsic_environment.pure_primitive_count == 0) {
		fprintf(stderr, "fixture has no intrinsic declaration\n");
		goto cleanup;
	}
	int saved_primitive_id = module.pure_primitives[0].primitive_id;
	module.pure_primitives[0].primitive_id ^= 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "intrinsic fingerprint mismatch minted checked authority\n");
		goto cleanup;
	}
	module.pure_primitives[0].primitive_id = saved_primitive_id;
	if (module.view.interface.term_export_count < 2) {
		fprintf(stderr, "fixture has too few semantic exports\n");
		goto cleanup;
	}
	uint32_t saved_export_classifier = module.term_exports[0].classifier;
	module.term_exports[0].classifier =
		module.type_declarations[0].formation_classifier;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged export classifier minted checked authority\n");
		goto cleanup;
	}
	module.term_exports[0].classifier = saved_export_classifier;

	int saved_export_name = module.term_exports[1].name_symbol_id;
	module.term_exports[1].name_symbol_id = module.term_exports[0].name_symbol_id;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "duplicate export name minted checked authority\n");
		goto cleanup;
	}
	module.term_exports[1].name_symbol_id = saved_export_name;

	struct prototype_semantic_dependency extra_dependency = {
		.namespace_symbol_id = PROTOTYPE_BASE_NAMESPACE_ID,
		.name_symbol_id = module.term_exports[0].name_symbol_id
	};
	const struct prototype_semantic_dependency* saved_dependencies =
		module.view.interface.dependencies;
	size_t saved_dependency_count = module.view.interface.dependency_count;
	module.view.interface.dependencies = &extra_dependency;
	module.view.interface.dependency_count = 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "extra dependency minted checked authority\n");
		goto cleanup;
	}
	module.view.interface.dependencies = saved_dependencies;
	module.view.interface.dependency_count = saved_dependency_count;
	size_t saved_term_count = module.view.terms.term_count;
	struct prototype_term* extended_terms = realloc(
		module.terms, (saved_term_count + 1) * sizeof(*extended_terms)
	);
	if (!extended_terms) goto cleanup;
	module.terms = extended_terms;
	module.view.terms.terms = extended_terms;
	memset(&module.terms[saved_term_count], 0, sizeof(module.terms[saved_term_count]));
	module.terms[saved_term_count].tag = PROTOTYPE_TERM_EXTERNAL_REF;
	module.terms[saved_term_count].as.external_ref.name.namespace_symbol_id =
		PROTOTYPE_BASE_NAMESPACE_ID;
	module.terms[saved_term_count].as.external_ref.name.name_symbol_id =
		module.term_exports[0].name_symbol_id;
	module.view.terms.term_count = saved_term_count + 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked ||
		report.stop_reason != PROTOTYPE_CHECKER_STOP_INTERFACE) {
		fprintf(stderr, "missing semantic dependency minted checked authority\n");
		goto cleanup;
	}
	module.view.terms.term_count = saved_term_count;
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	if (storage_initialized) {
		prototype_program_storage_destroy(&storage);
	}
	return result;
}

static int check_match_fragment(void) {
	static const char source[] =
		"Bool := @{ true : *; false : *; };\n"
		"negate := \\value : Bool =>\n"
		"  value @true => Bool.false @false => Bool.true;\n"
		"main := negate Bool.false;\n";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	prototype_elaborated_module_init(&module);
	if (project_source(
			"<checked-core-match>", source, &storage, &module
		) != 0) {
		goto cleanup;
	}
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(
			stderr,
			"match fragment did not complete: status=%d reason=%d\n",
			report.status,
			report.stop_reason
		);
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;

	uint32_t match_occurrence = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.occurrences.occurrence_count; ++i) {
		if (module.occurrences[i].kind == PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH) {
			match_occurrence = i;
			break;
		}
	}
	if (match_occurrence == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "match fixture has no Match occurrence\n");
		goto cleanup;
	}
	uint32_t saved_motive = module.occurrences[match_occurrence].match_motive;
	module.occurrences[match_occurrence].match_motive =
		module.occurrences[match_occurrence].asserted_classifier;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged Match motive minted checked authority\n");
		goto cleanup;
	}
	module.occurrences[match_occurrence].match_motive = saved_motive;

	uint32_t first_case = module.occurrences[match_occurrence].first_case;
	uint32_t saved_refinement = module.match_cases[first_case].refinement_substitution;
	module.match_cases[first_case].refinement_substitution = 0;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged Match refinement minted checked authority\n");
		goto cleanup;
	}
	module.match_cases[first_case].refinement_substitution = saved_refinement;

	uint32_t extension = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.substitutions.substitution_count; ++i) {
		if (module.substitutions[i].kind ==
			PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND) {
			extension = i;
			break;
		}
	}
	if (extension == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "match fixture has no extension substitution\n");
		goto cleanup;
	}
	uint32_t saved_classifier = module.substitutions[extension].term_classifier;
	module.substitutions[extension].term_classifier =
		module.type_declarations[0].formation_classifier;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged substitution classifier minted checked authority\n");
		goto cleanup;
	}
	module.substitutions[extension].term_classifier = saved_classifier;

	uint32_t saved_source = module.substitutions[extension].source_context;
	module.substitutions[extension].source_context = saved_source == 0 ? 1 : 0;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged substitution endpoint minted checked authority\n");
		goto cleanup;
	}
	module.substitutions[extension].source_context = saved_source;
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	prototype_program_storage_destroy(&storage);
	return result;
}

static int check_effect_fragment(void) {
	static const char source[] =
		"main := ((#.print #\"x\"))\n"
		"  @#.return y => y\n"
		"  @#.print x k => k x;\n";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	prototype_elaborated_module_init(&module);
	if (project_source(
			"<checked-core-effect>", source, &storage, &module
		) != 0) {
		goto cleanup;
	}
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(
			stderr,
			"effect fragment did not complete: status=%d reason=%d subject=%u\n",
			report.status,
			report.stop_reason,
			report.subject
		);
		if (report.subject < module.view.occurrences.occurrence_count) {
			fprintf(
				stderr,
				"effect occurrence kind=%d core-tag=%d classifier=%u\n",
				module.occurrences[report.subject].kind,
				module.terms[module.occurrences[report.subject].core_term].tag,
				module.occurrences[report.subject].asserted_classifier
			);
		}
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;

	uint32_t request_occurrence = PROTOTYPE_INVALID_ID;
	uint32_t empty_row = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.occurrences.occurrence_count; ++i) {
		if (module.occurrences[i].kind == PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST) {
			request_occurrence = i;
		}
	}
	for (uint32_t i = 0; i < module.view.terms.term_count; ++i) {
		if (module.terms[i].tag == PROTOTYPE_TERM_EFFECT_ROW_EMPTY) {
			empty_row = i;
			break;
		}
	}
	if (request_occurrence == PROTOTYPE_INVALID_ID ||
		empty_row == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "effect fixture lacks request or empty effect row\n");
		goto cleanup;
	}
	uint32_t result_classifier =
		module.occurrences[request_occurrence].asserted_classifier;
	if (module.terms[result_classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		fprintf(stderr, "effect request classifier is not a computation type\n");
		goto cleanup;
	}
	uint32_t saved_effect =
		module.terms[result_classifier].as.computation_type.label;
	module.terms[result_classifier].as.computation_type.label = empty_row;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "underreported effect row minted checked authority\n");
		goto cleanup;
	}
	module.terms[result_classifier].as.computation_type.label = saved_effect;

	int saved_totality =
		module.terms[result_classifier].as.computation_type.totality;
	module.terms[result_classifier].as.computation_type.totality =
		PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged totality minted checked authority\n");
		goto cleanup;
	}
	module.terms[result_classifier].as.computation_type.totality = saved_totality;

	uint64_t saved_capabilities = module.view.required_runtime_capabilities;
	module.view.required_runtime_capabilities = 0;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "underreported runtime capability minted checked authority\n");
		goto cleanup;
	}
	module.view.required_runtime_capabilities = saved_capabilities;

	uint32_t handler_occurrence = PROTOTYPE_INVALID_ID;
	uint32_t clause_operation_edge = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < module.view.occurrences.occurrence_count; ++i) {
		if (module.occurrences[i].kind ==
				PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD &&
			module.occurrences[i].fold_clause_count != 0) {
			handler_occurrence = i;
			for (uint32_t j = 0; j < module.occurrences[i].edge_count; ++j) {
				uint32_t edge_id = module.occurrences[i].first_edge + j;
				if (module.occurrence_edges[edge_id].role ==
					PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION) {
					clause_operation_edge = edge_id;
					break;
				}
			}
			break;
		}
	}
	if (handler_occurrence == PROTOTYPE_INVALID_ID ||
		clause_operation_edge == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "effect fixture lacks a handler clause\n");
		goto cleanup;
	}
	uint32_t saved_clause_operation =
		module.occurrence_edges[clause_operation_edge].child_occurrence;
	module.occurrence_edges[clause_operation_edge].child_occurrence =
		request_occurrence;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged handler operation minted checked authority\n");
		goto cleanup;
	}
	module.occurrence_edges[clause_operation_edge].child_occurrence =
		saved_clause_operation;

	struct prototype_semantic_occurrence* handler =
		&module.occurrences[handler_occurrence];
	struct prototype_semantic_fold_clause* clause =
		&module.fold_clauses[handler->first_fold_clause];
	uint32_t saved_continuation_binding = clause->continuation_binding_id;
	clause->continuation_binding_id = clause->argument_binding_id;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged handler continuation minted checked authority\n");
		goto cleanup;
	}
	clause->continuation_binding_id = saved_continuation_binding;

	if (module.view.contracts.contract_count != 0 ||
		module.view.contracts.dependency_count != 0) {
		fprintf(stderr, "effect fixture unexpectedly has producer contracts\n");
		goto cleanup;
	}
	module.contracts = calloc(1, sizeof(*module.contracts));
	module.contract_dependencies = calloc(
		1, sizeof(*module.contract_dependencies)
	);
	if (!module.contracts || !module.contract_dependencies) {
		goto cleanup;
	}
	module.contracts[0] = (struct prototype_semantic_contract) {
		.kind = PROTOTYPE_SEMANTIC_CONTRACT_EFFECT_ROW_EQUATION,
		.occurrence = request_occurrence,
		.core_term = module.occurrences[request_occurrence].core_term,
		.computation_occurrence = PROTOTYPE_INVALID_ID,
		.continuation_occurrence = PROTOTYPE_INVALID_ID,
		.continuation_binding_id = PROTOTYPE_INVALID_ID,
		.input_classifier = saved_effect,
		.classifier_family = PROTOTYPE_INVALID_ID,
		.effect_row = saved_effect,
		.effect_constraint_kind = PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_EXACT,
		.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
		.schema_version = 2
	};
	module.contract_dependencies[0] =
		(struct prototype_semantic_contract_dependency) {
			.occurrence = request_occurrence,
			.contract_id = 0
		};
	module.view.contracts.contracts = module.contracts;
	module.view.contracts.contract_count = 1;
	module.view.contracts.dependencies = module.contract_dependencies;
	module.view.contracts.dependency_count = 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(stderr, "well-formed conditional effect contract was rejected\n");
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;
	module.contract_dependencies[0].occurrence = handler_occurrence;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged conditional contract dependency was accepted\n");
		goto cleanup;
	}
	module.contract_dependencies[0].occurrence = request_occurrence;
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	prototype_program_storage_destroy(&storage);
	return result;
}

static int check_indexed_constructor_fragment(void) {
	static const char source[] =
		"Nat := @{ zero : *; succ : * -> *; };\n"
		"Vec := \\A : @ => @\\n : Nat => {\n"
		"  nil : * Nat.zero;\n"
		"  cons : (k : Nat) -> A -> * k -> * (Nat.succ k);\n"
		"};\n"
		"empty := (Vec Nat).nil;\n"
		"single := (Vec Nat).cons Nat.zero Nat.zero empty;\n";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	prototype_elaborated_module_init(&module);
	if (project_source(
			"<checked-core-indexed-constructor>", source, &storage, &module
		) != 0) {
		goto cleanup;
	}
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(stderr, "indexed constructor fragment did not complete\n");
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;

	const struct prototype_semantic_type_declaration* indexed = NULL;
	for (size_t i = 0; i < module.view.type_schema.type_count; ++i) {
		if (module.type_declarations[i].index_count != 0 &&
			module.type_declarations[i].constructor_count >= 2) {
			indexed = &module.type_declarations[i];
			break;
		}
	}
	if (!indexed) {
		fprintf(stderr, "indexed fixture has no indexed declaration\n");
		goto cleanup;
	}
	struct prototype_semantic_type_constructor* nil_constructor =
		&module.constructor_declarations[indexed->first_constructor];
	struct prototype_semantic_type_constructor* cons_constructor =
		&module.constructor_declarations[indexed->first_constructor + 1];
	uint32_t saved_result = cons_constructor->result_classifier;
	cons_constructor->result_classifier = nil_constructor->result_classifier;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged constructor result index minted checked authority\n");
		goto cleanup;
	}
	cons_constructor->result_classifier = saved_result;
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	prototype_program_storage_destroy(&storage);
	return result;
}

static int check_function_graph_interface_fragment(void) {
	static const char path[] =
		"src/prototype/tests/fixtures/typing/"
		"function_graph_generated_length_check.p";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;

	prototype_effort_account_init(&effort, UINT64_C(1000000));
	prototype_elaborated_module_init(&module);
	if (project_file(path, &storage, &module) != 0) {
		goto cleanup;
	}
	if (module.view.interface.function_graph_association_count == 0 ||
		module.view.interface.function_graph_selector_group_count == 0) {
		fprintf(stderr, "Function Graph metadata did not enter checked Core\n");
		goto cleanup;
	}
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(
			stderr,
			"Function Graph interface did not complete: status=%d reason=%d "
			"subject=%u\n",
			report.status,
			report.stop_reason,
			report.subject
		);
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;

	struct prototype_semantic_function_graph_association* association =
		&module.function_graph_associations[0];
	uint32_t saved_graph_type = association->graph_type_export;
	association->graph_type_export = association->result_type_export;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged Function Graph association minted authority\n");
		goto cleanup;
	}
	association->graph_type_export = saved_graph_type;

	struct prototype_semantic_function_graph_selector_group* group =
		&module.function_graph_selector_groups[association->first_selector_group];
	uint32_t saved_value_field = group->value_field_ordinal;
	group->value_field_ordinal = UINT32_MAX;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged Function Graph selector minted authority\n");
		goto cleanup;
	}
	group->value_field_ordinal = saved_value_field;
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	prototype_program_storage_destroy(&storage);
	return result;
}

static int check_dimension_boundary(void) {
	static const char source[] =
		"Bool := @{ true : *; false : *; };\n"
		"main := Bool.true;\n";
	struct prototype_program_storage storage;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	struct prototype_semantic_dimension_operator operator = {
		.source_dimension = 0,
		.target_dimension = 0,
		.image_offset = 0,
		.image_count = 0
	};
	int result = -1;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	prototype_elaborated_module_init(&module);
	if (project_source(
			"<checked-core-dimension-boundary>", source, &storage, &module
		) != 0) {
		goto cleanup;
	}
	module.view.dimensions.operators = &operator;
	module.view.dimensions.operator_count = 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(stderr, "valid dimension operator was not independently checked\n");
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;
	operator.source_dimension = 1;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED || checked) {
		fprintf(stderr, "forged dimension operator minted checked authority\n");
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	prototype_program_storage_destroy(&storage);
	return result;
}

static int check_checked_import_boundary(void) {
	static const char provider_identity_source[] =
		"main := \\value : #.Int => value;\n";
	static const char provider_constant_source[] =
		"main := \\value : #.Int => #1;\n";
	static const char consumer_source[] =
		"Marker := @{ main : *; };\n"
		"consumer := \\value : #.Int => value;\n"
		"nameCarrier := #1;\n";
	struct prototype_program_storage provider_identity_storage;
	struct prototype_program_storage provider_constant_storage;
	struct prototype_program_storage consumer_storage;
	struct prototype_elaborated_module provider_identity;
	struct prototype_elaborated_module provider_constant;
	struct prototype_elaborated_module consumer;
	struct prototype_checked_module* checked_identity = NULL;
	struct prototype_checked_module* checked_constant = NULL;
	struct prototype_checked_module* checked_consumer = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	struct prototype_checked_module_set* previous_set = NULL;
	struct prototype_checked_module_set* current_set = NULL;
	struct prototype_checked_incremental_snapshot* previous_snapshot = NULL;
	struct prototype_checked_incremental_snapshot* current_snapshot = NULL;
	int result = -1;

	memset(&provider_identity_storage, 0, sizeof(provider_identity_storage));
	memset(&provider_constant_storage, 0, sizeof(provider_constant_storage));
	memset(&consumer_storage, 0, sizeof(consumer_storage));
	prototype_elaborated_module_init(&provider_identity);
	prototype_elaborated_module_init(&provider_constant);
	prototype_elaborated_module_init(&consumer);
	if (project_source(
			"<checked-import-identity>", provider_identity_source,
			&provider_identity_storage, &provider_identity
		) != 0 || project_source(
			"<checked-import-constant>", provider_constant_source,
			&provider_constant_storage, &provider_constant
		) != 0 || project_source(
			"<checked-import-consumer>", consumer_source,
			&consumer_storage, &consumer
		) != 0) {
		goto cleanup;
	}
	if (provider_identity.view.interface.term_export_count != 1 ||
		provider_constant.view.interface.term_export_count != 1 ||
		consumer.view.interface.term_export_count < 1) {
		fprintf(stderr, "checked import fixture export count mismatch\n");
		goto cleanup;
	}
	provider_identity.term_exports[0].transparency =
		PROTOTYPE_SEMANTIC_EXPORT_OPAQUE;
	provider_constant.term_exports[0].transparency =
		PROTOTYPE_SEMANTIC_EXPORT_OPAQUE;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&provider_identity.view, &options, &checked_identity, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!checked_identity) {
		fprintf(stderr, "identity import provider did not check\n");
		goto cleanup;
	}
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&provider_constant.view, &options, &checked_constant, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!checked_constant) {
		fprintf(stderr, "constant import provider did not check\n");
		goto cleanup;
	}

	size_t consumer_export_index = SIZE_MAX;
	for (size_t i = 0; i < consumer.view.interface.term_export_count; ++i) {
		int symbol_id = consumer.view.interface.term_exports[i].name_symbol_id;
		if (symbol_id >= 0 && (size_t)symbol_id < consumer.view.symbols.count &&
			strcmp(consumer.view.symbols.strings[symbol_id], "consumer") == 0) {
			consumer_export_index = i;
			break;
		}
	}
	if (consumer_export_index == SIZE_MAX) {
		fprintf(stderr, "checked import fixture omitted consumer export\n");
		goto cleanup;
	}
	const struct prototype_semantic_term_export* consumer_export =
		&consumer.view.interface.term_exports[consumer_export_index];
	int provider_name_symbol_id = -1;
	for (size_t i = 0; i < consumer.view.symbols.count; ++i) {
		if (strcmp(consumer.view.symbols.strings[i], "main") == 0) {
			provider_name_symbol_id = (int)i;
			break;
		}
	}
	if (provider_name_symbol_id < 0) {
		fprintf(stderr, "checked import fixture omitted provider symbol\n");
		goto cleanup;
	}
	uint32_t external_term = consumer_export->term;
	uint32_t external_occurrence = consumer_export->occurrence;
	consumer.terms[external_term] = (struct prototype_term) {
		.tag = PROTOTYPE_TERM_EXTERNAL_REF,
		.as.external_ref.name = {
			.namespace_symbol_id = consumer_export->namespace_symbol_id,
			.name_symbol_id = provider_name_symbol_id
		}
	};
	consumer.occurrences[external_occurrence].kind =
		PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM;
	consumer.occurrences[external_occurrence].first_edge = 0;
	consumer.occurrences[external_occurrence].edge_count = 0;
	free(consumer.dependencies);
	consumer.dependencies = calloc(1, sizeof(*consumer.dependencies));
	if (!consumer.dependencies) {
		goto cleanup;
	}
	consumer.dependencies[0] = (struct prototype_semantic_dependency) {
		.namespace_symbol_id = consumer_export->namespace_symbol_id,
		.name_symbol_id = provider_name_symbol_id
	};
	consumer.view.interface.dependencies = consumer.dependencies;
	consumer.view.interface.dependency_count = 1;

	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&consumer.view, &options, &checked_consumer, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED ||
		checked_consumer || report.stop_reason !=
			PROTOTYPE_CHECKER_STOP_INTERFACE) {
		fprintf(stderr,
			"missing checked import base was accepted status=%d reason=%d term=%u tag=%d\n",
			report.status, report.stop_reason, external_term,
			consumer.terms[external_term].tag);
		goto cleanup;
	}

	const struct prototype_checked_module* identity_bases[] = {
		checked_identity
	};
	options.imported_bases = identity_bases;
	options.imported_base_count = 1;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&consumer.view, &options, &checked_consumer, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!checked_consumer) {
		fprintf(stderr, "exact checked import was rejected\n");
		goto cleanup;
	}
	prototype_checked_module_destroy(checked_consumer);
	checked_consumer = NULL;

	const struct prototype_checked_module* constant_bases[] = {
		checked_constant
	};
	options.imported_bases = constant_bases;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&consumer.view, &options, &checked_consumer, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!checked_consumer) {
		fprintf(stderr, "opaque provider body affected interface checking\n");
		goto cleanup;
	}
	const struct prototype_checked_module* previous_modules[] = {
		checked_identity, checked_consumer
	};
	const struct prototype_checked_module* current_modules[] = {
		checked_constant, checked_consumer
	};
	struct prototype_checked_module_set_report set_report;
	if (prototype_checked_module_set_create(
			previous_modules, 2, &previous_set, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_module_set_create(
			current_modules, 2, &current_set, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_incremental_snapshot_create(
			previous_set, &previous_snapshot
		) != 0 || prototype_checked_incremental_snapshot_create(
			current_set, &current_snapshot
		) != 0) {
		fprintf(stderr, "checked import incremental snapshot failed\n");
		goto cleanup;
	}
	size_t previous_goal_count =
		prototype_checked_incremental_snapshot_goal_count(previous_snapshot);
	unsigned char* dependency_invalidated = previous_goal_count == 0 ? NULL :
		calloc(previous_goal_count, 1);
	if ((previous_goal_count != 0 && !dependency_invalidated) ||
		prototype_checked_incremental_compare(
			previous_snapshot,
			current_snapshot,
			dependency_invalidated,
			previous_goal_count
		) != 0) {
		free(dependency_invalidated);
		fprintf(stderr, "checked import incremental comparison failed\n");
		goto cleanup;
	}
	for (size_t i = 0; i < previous_goal_count; ++i) {
		if (!dependency_invalidated[i]) {
			free(dependency_invalidated);
			fprintf(stderr, "checked import reverse dependency stayed valid\n");
			goto cleanup;
		}
	}
	free(dependency_invalidated);
	prototype_checked_incremental_snapshot_destroy(current_snapshot);
	prototype_checked_incremental_snapshot_destroy(previous_snapshot);
	prototype_checked_module_set_destroy(current_set);
	prototype_checked_module_set_destroy(previous_set);
	current_snapshot = NULL;
	previous_snapshot = NULL;
	current_set = NULL;
	previous_set = NULL;
	prototype_checked_module_destroy(checked_consumer);
	checked_consumer = NULL;

	const struct prototype_checked_module* duplicate_bases[] = {
		checked_identity,
		checked_constant
	};
	options.imported_bases = duplicate_bases;
	options.imported_base_count = 2;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&consumer.view, &options, &checked_consumer, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED ||
		checked_consumer || report.stop_reason !=
			PROTOTYPE_CHECKER_STOP_INTERFACE) {
		fprintf(stderr, "ambiguous checked import providers were accepted\n");
		goto cleanup;
	}

	options.imported_bases = identity_bases;
	options.imported_base_count = 1;
	uint32_t saved_classifier =
		consumer.occurrences[external_occurrence].asserted_classifier;
	consumer.occurrences[external_occurrence].asserted_classifier =
		consumer_export->term;
	consumer.term_exports[consumer_export_index].classifier = consumer_export->term;
	prototype_effort_account_init(&effort, UINT64_C(100000));
	if (prototype_checker_check_module(
			&consumer.view, &options, &checked_consumer, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_REJECTED ||
		checked_consumer || report.stop_reason !=
			PROTOTYPE_CHECKER_STOP_INTERFACE) {
		fprintf(stderr, "forged imported classifier was accepted\n");
		goto cleanup;
	}
	consumer.occurrences[external_occurrence].asserted_classifier =
		saved_classifier;
	consumer.term_exports[consumer_export_index].classifier = saved_classifier;
	result = 0;

cleanup:
	prototype_checked_incremental_snapshot_destroy(current_snapshot);
	prototype_checked_incremental_snapshot_destroy(previous_snapshot);
	prototype_checked_module_set_destroy(current_set);
	prototype_checked_module_set_destroy(previous_set);
	prototype_checked_module_destroy(checked_consumer);
	prototype_checked_module_destroy(checked_constant);
	prototype_checked_module_destroy(checked_identity);
	prototype_elaborated_module_destroy(&consumer);
	prototype_elaborated_module_destroy(&provider_constant);
	prototype_elaborated_module_destroy(&provider_identity);
	prototype_program_storage_destroy(&consumer_storage);
	prototype_program_storage_destroy(&provider_constant_storage);
	prototype_program_storage_destroy(&provider_identity_storage);
	return result;
}

static int check_checked_module_set_boundary(void) {
	static const char* sources[] = {
		"alpha := #1;\n",
		"alpha := #1;\n",
		"beta := #2;\n",
		"alpha := #3;\n"
	};
	struct prototype_program_storage storages[4];
	struct prototype_elaborated_module modules[4];
	struct prototype_checked_module* checked[4] = {NULL};
	struct prototype_effort_account efforts[4];
	struct prototype_parallel_check_task tasks[4];
	struct prototype_checker_report checker_reports[4];
	struct prototype_checked_module_set* first = NULL;
	struct prototype_checked_module_set* second = NULL;
	struct prototype_checked_module_set* changed_set = NULL;
	struct prototype_checked_incremental_snapshot* previous_snapshot = NULL;
	struct prototype_checked_incremental_snapshot* current_snapshot = NULL;
	struct prototype_checked_module_set_report set_report;
	struct prototype_merge_producer_session* merge = NULL;
	struct prototype_work_capsule merge_capsule;
	struct prototype_work_capsule decoded_capsule;
	int result = -1;

	prototype_work_capsule_init(&merge_capsule);
	prototype_work_capsule_init(&decoded_capsule);
	memset(storages, 0, sizeof(storages));
	for (size_t i = 0; i < 4; ++i) {
		prototype_elaborated_module_init(&modules[i]);
	}
	for (size_t i = 0; i < 4; ++i) {
		if (project_source(
				"<checked-module-set>", sources[i], &storages[i], &modules[i]
			) != 0) goto cleanup;
		prototype_effort_account_init(&efforts[i], UINT64_C(100000));
			tasks[i] = (struct prototype_parallel_check_task) {
				.module = &modules[i].view,
				.options = {.effort = &efforts[i]}
			};
		}
	struct prototype_effort_account* distinct_effort = tasks[1].options.effort;
	tasks[1].options.effort = tasks[0].options.effort;
	if (prototype_checker_check_modules_parallel(
			tasks, 4, 3, checked, checker_reports
		) == 0) {
		fprintf(stderr, "parallel checker accepted a shared effort account\n");
		goto cleanup;
	}
	tasks[1].options.effort = distinct_effort;
	if (prototype_checker_check_modules_parallel(
			tasks, 4, 3, checked, checker_reports
		) != 0) {
		fprintf(stderr, "parallel checked module-set execution failed\n");
		goto cleanup;
	}
	for (size_t i = 0; i < 4; ++i) {
		if (checker_reports[i].status != PROTOTYPE_CHECKER_COMPLETE ||
			!checked[i]) {
			fprintf(stderr, "checked module-set fixture did not check\n");
			goto cleanup;
		}
	}
	const struct prototype_checked_module* forward[] = {
		checked[0], checked[2]
	};
	const struct prototype_checked_module* reverse[] = {
		checked[2], checked[0]
	};
	if (prototype_checked_module_set_create(
			forward, 2, &first, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_module_set_create(
			reverse, 2, &second, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_module_set_count(first) != 2 ||
		prototype_checked_module_set_count(second) != 2 ||
		prototype_checked_module_set_at(first, 0) !=
			prototype_checked_module_set_at(second, 0) ||
		prototype_checked_module_set_at(first, 1) !=
			prototype_checked_module_set_at(second, 1)) {
		fprintf(stderr, "checked module-set order was not canonical\n");
		goto cleanup;
	}
	const struct prototype_checked_module* changed_modules[] = {
		checked[3], checked[2]
	};
	if (prototype_checked_module_set_create(
			changed_modules, 2, &changed_set, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_incremental_snapshot_create(
			first, &previous_snapshot
		) != 0 || prototype_checked_incremental_snapshot_create(
			changed_set, &current_snapshot
		) != 0) {
		fprintf(stderr, "checked incremental snapshot construction failed\n");
		goto cleanup;
	}
	size_t goal_count = prototype_checked_incremental_snapshot_goal_count(
		previous_snapshot
	);
	unsigned char* invalidated = goal_count == 0 ? NULL : calloc(
		goal_count, 1
	);
	if ((goal_count != 0 && !invalidated) ||
		prototype_checked_incremental_compare(
			previous_snapshot, current_snapshot, invalidated, goal_count
		) != 0) {
		free(invalidated);
		fprintf(stderr, "checked incremental comparison failed\n");
		goto cleanup;
	}
	size_t changed_count = 0;
	size_t stable_count = 0;
	for (size_t i = 0; i < goal_count; ++i) {
		const struct prototype_incremental_goal* previous_goal =
			prototype_checked_incremental_snapshot_goal_at(previous_snapshot, i);
		const struct prototype_incremental_goal* current_goal = NULL;
		for (size_t j = 0;
			j < prototype_checked_incremental_snapshot_goal_count(current_snapshot);
			++j) {
			const struct prototype_incremental_goal* candidate =
				prototype_checked_incremental_snapshot_goal_at(current_snapshot, j);
			if (prototype_goal_key_equal(&previous_goal->key, &candidate->key)) {
				current_goal = candidate;
				break;
			}
		}
		if (!current_goal) {
			free(invalidated);
			fprintf(stderr, "checked incremental goal identity changed\n");
			goto cleanup;
		}
		int content_changed = memcmp(
			previous_goal->content_fingerprint,
			current_goal->content_fingerprint,
			sizeof(previous_goal->content_fingerprint)
		) != 0;
		if (content_changed && invalidated[i]) changed_count += 1;
		else if (!content_changed && !invalidated[i]) stable_count += 1;
		else {
			free(invalidated);
			fprintf(stderr, "checked incremental invalidation was unsound\n");
			goto cleanup;
		}
	}
	free(invalidated);
	if (changed_count != 1 || stable_count != 1) {
		fprintf(stderr, "checked incremental edit boundary was incomplete\n");
		goto cleanup;
	}
	prototype_checked_incremental_snapshot_destroy(current_snapshot);
	prototype_checked_incremental_snapshot_destroy(previous_snapshot);
	prototype_checked_module_set_destroy(changed_set);
	current_snapshot = NULL;
	previous_snapshot = NULL;
	changed_set = NULL;
	prototype_checked_module_set_destroy(first);
	prototype_checked_module_set_destroy(second);
	first = NULL;
	second = NULL;

	struct prototype_work_capsule_compatibility merge_identity = {
		.producer_kind = PROTOTYPE_WORK_CAPSULE_FRAGMENT_MERGE,
		.producer_version = PROTOTYPE_MERGE_PRODUCER_VERSION,
		.cost_model_version = PROTOTYPE_EFFORT_COST_MODEL_VERSION,
		.calculus_fingerprint = modules[0].view.calculus_fingerprint,
		.intrinsic_fingerprint = modules[0].view.intrinsic_fingerprint,
		.base_revision = {131, 0, 0, 0},
		.goal_key = {137, 0, 0, 0}
	};
	struct prototype_merge_producer_report merge_report;
	prototype_checked_module_image_serialization_count_reset();
	if (prototype_merge_producer_create(
			forward, 2, &merge_identity, &merge
		) != 0 || prototype_checked_module_image_serialization_count() != 2 ||
		prototype_merge_producer_advance(
			merge, 0, &merge_report
		) != 0 || merge_report.status != PROTOTYPE_MERGE_PRODUCER_PAUSED ||
		prototype_checked_module_image_serialization_count() != 2 ||
		prototype_merge_producer_make_capsule(merge, &merge_capsule) != 0) {
		fprintf(stderr, "merge producer did not pause with a typed capsule\n");
		goto cleanup;
	}
	FILE* capsule_stream = tmpfile();
	if (!capsule_stream || prototype_work_capsule_write(
			capsule_stream, &merge_capsule
		) != 0 || fseek(capsule_stream, 0, SEEK_SET) != 0 ||
		prototype_work_capsule_read(capsule_stream, &decoded_capsule) != 0) {
		if (capsule_stream) fclose(capsule_stream);
		fprintf(stderr, "merge producer capsule did not round trip\n");
		goto cleanup;
	}
	fclose(capsule_stream);
	prototype_merge_producer_destroy(merge);
	merge = NULL;
	prototype_effort_account_init(&efforts[0], UINT64_C(100000));
	struct prototype_checker_options restore_options = {
		.effort = &efforts[0]
	};
	unsigned char saved_magic = decoded_capsule.payload[0];
	decoded_capsule.payload[0] ^= 1;
	if (prototype_merge_producer_restore(
			&decoded_capsule, &merge_identity, &restore_options, &merge
		) == 0 || merge) {
		fprintf(stderr, "malformed merge payload was accepted\n");
		goto cleanup;
	}
	decoded_capsule.payload[0] = saved_magic;
	if (prototype_merge_producer_restore(
			&decoded_capsule, &merge_identity, &restore_options, &merge
		) != 0 || prototype_merge_producer_advance(
			merge, 1, &merge_report
		) != 0 || merge_report.status != PROTOTYPE_MERGE_PRODUCER_COMPLETE ||
		prototype_checked_module_set_count(
			prototype_merge_producer_result(merge)
		) != 2) {
		fprintf(stderr, "fresh merge producer resume did not complete\n");
		goto cleanup;
	}
	prototype_merge_producer_destroy(merge);
	merge = NULL;

	const struct prototype_checked_module* equal[] = {checked[0], checked[1]};
	if (prototype_checked_module_set_create(
			equal, 2, &first, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE ||
		prototype_checked_module_set_count(first) != 1) {
		fprintf(stderr, "equal checked modules were not deduplicated\n");
		goto cleanup;
	}
	prototype_checked_module_set_destroy(first);
	first = NULL;
	const struct prototype_checked_module* malformed[] = { NULL };
	if (prototype_checked_module_set_create(
			malformed, 1, &first, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_MALFORMED ||
		first) {
		fprintf(stderr, "malformed checked module set did not reject cleanly\n");
		goto cleanup;
	}

	const struct prototype_checked_module* conflicting[] = {
		checked[0], checked[3]
	};
	if (prototype_checked_module_set_create(
			conflicting, 2, &first, &set_report
		) != 0 || set_report.status != PROTOTYPE_CHECKED_MODULE_SET_CONFLICT ||
		first || set_report.export_kind != PROTOTYPE_CHECKED_EXPORT_TERM) {
		fprintf(stderr, "unequal same-name checked modules did not conflict\n");
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_checked_incremental_snapshot_destroy(current_snapshot);
	prototype_checked_incremental_snapshot_destroy(previous_snapshot);
	prototype_checked_module_set_destroy(changed_set);
	prototype_merge_producer_destroy(merge);
	prototype_work_capsule_destroy(&decoded_capsule);
	prototype_work_capsule_destroy(&merge_capsule);
	prototype_checked_module_set_destroy(second);
	prototype_checked_module_set_destroy(first);
	for (size_t i = 0; i < 4; ++i) {
		prototype_checked_module_destroy(checked[i]);
		prototype_elaborated_module_destroy(&modules[i]);
		prototype_program_storage_destroy(&storages[i]);
	}
	return result;
}

int main(void) {
	return check_complete_fragment() != 0 || check_match_fragment() != 0 ||
		check_effect_fragment() != 0 || check_indexed_constructor_fragment() != 0 ||
		check_function_graph_interface_fragment() != 0 ||
		check_dimension_boundary() != 0 || check_checked_import_boundary() != 0 ||
		check_checked_module_set_boundary() != 0;
}
