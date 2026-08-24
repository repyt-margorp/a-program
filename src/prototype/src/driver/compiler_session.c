#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/function_graph.h"
#include "a_program/frontend/reader.h"
#include "a_program/frontend/ast_inspect.h"
#include "a_program/frontend/universe_collection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t compiler_session_clock_ns(void) {
	clock_t now = clock();
	if (now == (clock_t)-1) {
		return 0;
	}
	return (uint64_t)now / (uint64_t)CLOCKS_PER_SEC * UINT64_C(1000000000) +
		(uint64_t)now % (uint64_t)CLOCKS_PER_SEC * UINT64_C(1000000000) /
		(uint64_t)CLOCKS_PER_SEC;
}

static uint64_t compiler_session_elapsed_ns(uint64_t started) {
	uint64_t finished = compiler_session_clock_ns();
	return finished >= started ? finished - started : 0;
}

static int program_has_function_graph_requests(
	const struct prototype_program* program
) {
	if (!program || !program->asts || !program->metadata) {
		return 0;
	}
	const struct prototype_ast_db* asts = program->asts;
	if (program->metadata->function_graph_request_count != 0) {
		return 1;
	}
	for (size_t i = 0; i < asts->type_expr_count; ++i) {
		if (asts->type_exprs[i].tag ==
				PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE) {
			return 1;
		}
	}
	for (size_t i = 0; i < asts->node_count; ++i) {
		if (asts->nodes[i].tag ==
				PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE) {
			return 1;
		}
	}
	return 0;
}

static int prototype_install_system_nat(struct prototype_program* program) {
	int nat_symbol;
	int zero_symbol;
	int succ_symbol;
	uint32_t type_id;
	uint32_t self_expr;
	uint32_t succ_field;
	uint32_t nat_term;
	uint32_t universe;
	uint32_t zero_term;
	uint32_t succ_term;
	uint32_t zero_constructor_id;
	uint32_t succ_constructor_id;
	uint32_t succ_classifier;
	uint32_t empty_context;
	uint32_t succ_field_context;
	uint32_t succ_binder;

	if (!program || !program->symbols || !program->type_declarations ||
		!program->terms || !program->judgement || !program->metadata) {
		return -1;
	}

	nat_symbol = symbol_intern(program->symbols, "#.Nat", 5);
	zero_symbol = symbol_intern(program->symbols, "zero", 4);
	succ_symbol = symbol_intern(program->symbols, "succ", 4);
	if (nat_symbol < 0 || zero_symbol < 0 || succ_symbol < 0) {
		return -1;
	}
	const struct prototype_type_declaration* existing =
		prototype_type_declaration_lookup(
			&program->type_declarations->semantic_schema, nat_symbol);
	if (existing) {
		return 0;
	}
	if (prototype_type_declaration_add(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			&program->type_declarations->representation_db, nat_symbol, &type_id) != 0 ||
		prototype_type_expr_self(&program->type_declarations->readback, &self_expr) != 0 ||
		prototype_term_type_instance_make(
			program->terms,
			program->type_declarations,
			type_id,
			NULL,
			0,
			&nat_term
		) != 0 ||
		prototype_term_pi(program->terms, nat_term, nat_term, &succ_classifier) != 0 ||
		(empty_context = prototype_context_empty(
			&program->metadata->contexts
		)) == PROTOTYPE_INVALID_ID ||
		prototype_type_declaration_add_constructor_schema(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			&program->type_declarations->representation_db,
			&program->type_declarations->constructor_classifier_cache,
			type_id,
			zero_symbol,
			empty_context,
			empty_context,
			nat_term,
			&zero_constructor_id
		) != 0 || prototype_type_readback_attach_constructor(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			zero_constructor_id,
			NULL,
			0,
			self_expr
		) != 0 || prototype_type_constructor_classifier_cache_set(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->constructor_classifier_cache,
			zero_constructor_id,
			nat_term
		) != 0) {
		return -1;
	}
	succ_field = self_expr;
	succ_binder = prototype_term_binding_for_scope_slot(program->terms, 0);
	if (succ_binder == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&program->metadata->contexts,
			empty_context,
			succ_binder,
			nat_term,
			PROTOTYPE_INVALID_ID,
			&succ_field_context
		) != 0) {
		return -1;
	}
	if (prototype_type_declaration_add_constructor_schema(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			&program->type_declarations->representation_db,
			&program->type_declarations->constructor_classifier_cache,
			type_id,
			succ_symbol,
			empty_context,
			succ_field_context,
			nat_term,
			&succ_constructor_id
		) != 0 || prototype_type_readback_attach_constructor(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			succ_constructor_id,
			&succ_field,
			1,
			self_expr
		) != 0 || prototype_type_constructor_classifier_cache_set(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->constructor_classifier_cache,
			succ_constructor_id,
			succ_classifier
		) != 0 ||
		prototype_term_universe_var(
			program->terms, program->judgement->next_universe_var++, &universe
		) != 0) {
		return -1;
	}
	if (type_id >= program->type_declarations->semantic_schema.type_count) {
		return -1;
	}
	program->type_declarations->semantic_schema.type_declarations[type_id].formation_classifier = universe;
	program->type_declarations->semantic_schema.type_declarations[type_id].parameter_context =
		empty_context;
	program->type_declarations->semantic_schema.type_declarations[type_id].index_context =
		empty_context;
	prototype_type_declaration_db_mark_semantic_change(
		&program->type_declarations->semantic_schema
	);
	if (prototype_judgement_expand_type_def(
			program->judgement,
			program->terms,
			program->type_declarations,
			nat_term,
			universe
		) != 0 ||
		prototype_term_constructor(program->terms, nat_term, 0, &zero_term) != 0 ||
		prototype_judgement_expand_constructor_def(
			program->judgement,
			program->terms,
			program->type_declarations,
			zero_term,
			nat_term
			) != 0 ||
			prototype_term_constructor(program->terms, nat_term, 1, &succ_term) != 0 ||
			prototype_judgement_expand_constructor_def(
				program->judgement,
				program->terms,
			program->type_declarations,
			succ_term,
			succ_classifier
		) != 0) {
		return -1;
	}
	return 0;
}

int prototype_compile_graph_with_imports(
	struct prototype_program* program,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_read_error* error
) {
	if (error) {
		memset(error, 0, sizeof(*error));
	}
	if (!program || !program->intrinsic_environment || !program->asts ||
		!program->type_declarations || !program->terms || !program->judgement ||
		!program->metadata) {
		if (error) {
			snprintf(error->message, sizeof(error->message), "%s", "invalid graph compile arguments");
		}
		return -1;
	}
	if (prototype_install_system_nat(program) != 0) {
		if (error) {
			snprintf(error->message, sizeof(error->message), "%s", "failed to install system Nat");
		}
		return -1;
	}
	if (program->compile_options.normalization_step_limit_is_set) {
		program->metadata->normalization_step_limit =
			program->compile_options.normalization_step_limit;
	}
	if (program->compile_options.solver_step_limit_is_set) {
		program->metadata->solver_step_limit =
			program->compile_options.solver_step_limit;
	}
	if (program->compile_options.compile_policy != 0) {
		program->metadata->compile_policy = program->compile_options.compile_policy;
	}
	if (program->compile_options.definition_thunk_policy != 0) {
		program->metadata->definition_thunk_policy =
			program->compile_options.definition_thunk_policy;
	}
	int function_graph_requested = program_has_function_graph_requests(program);
	program->metadata->function_graph_preflight = function_graph_requested ?
		PROTOTYPE_FUNCTION_GRAPH_COMPILE_OWNER_PREFLIGHT :
		PROTOTYPE_FUNCTION_GRAPH_COMPILE_NORMAL;
	uint64_t stage_started = compiler_session_clock_ns();
	if (prototype_ast_compile_pending_with_imports(
		program->asts,
		program->terms,
		program->type_declarations,
		program->judgement,
		program->metadata,
		program->symbols,
		program->intrinsic_environment,
		program->namespace_symbol_id,
		imported_interfaces,
		imported_interface_count
	) != 0) {
		if (error) {
			if (program->metadata &&
				program->metadata->compile_diagnostic_count > 0) {
				const struct prototype_compile_diagnostic* diagnostic =
					&program->metadata->compile_diagnostics[0];
				error->line = diagnostic->span.line;
				error->column = diagnostic->span.column;
			} else if (program->metadata &&
				program->metadata->resolve_error_count > 0) {
				const struct prototype_resolve_error* resolve_error =
					&program->metadata->resolve_errors[0];
				error->line = resolve_error->span.line;
				error->column = resolve_error->span.column;
			}
			snprintf(
				error->message,
				sizeof(error->message),
				"%s",
				program->metadata->solver_exhausted ?
					"classifier solver step limit exhausted" :
					"failed to compile AST graph"
			);
		}
		return -1;
	}
	program->metadata->source_compile_time_ns +=
		compiler_session_elapsed_ns(stage_started);
	if (function_graph_requested) {
		size_t source_ast_nodes = program->asts->node_count;
		size_t source_assignments = program->asts->assignment_count;
		size_t source_types = program->asts->type_def_count;
		size_t source_constructors = program->asts->type_constructor_count;
		stage_started = compiler_session_clock_ns();
		if (prototype_function_graph_generate_requested(
				program->asts,
				program->terms,
				program->type_declarations,
				program->judgement,
				program->metadata,
				program->symbols
				) != 0) {
			if (error) {
				snprintf(
					error->message,
					sizeof(error->message),
					"%s",
						"failed to generate accepted function graph"
					);
			}
			return -1;
		}
		program->metadata->function_graph_generation_time_ns +=
			compiler_session_elapsed_ns(stage_started);
		program->metadata->function_graph_source_ast_node_count += source_ast_nodes;
		program->metadata->function_graph_generated_ast_node_count +=
			program->asts->node_count - source_ast_nodes;
		program->metadata->function_graph_generated_assignment_count +=
			program->asts->assignment_count - source_assignments;
		program->metadata->function_graph_generated_type_count +=
			program->asts->type_def_count - source_types;
		program->metadata->function_graph_generated_constructor_count +=
			program->asts->type_constructor_count - source_constructors;
		/* The source pass is an accepted immutable prefix, not disposable scratch.
		 * Extend it with generated declarations, promote the certified projection,
		 * then compile only the graph-reference consumers skipped by the first pass. */
		program->metadata->function_graph_preflight =
			PROTOTYPE_FUNCTION_GRAPH_COMPILE_GENERATED_CLOSURE;

		stage_started = compiler_session_clock_ns();
		if (prototype_ast_compile_pending_with_imports(
				program->asts,
				program->terms,
				program->type_declarations,
				program->judgement,
				program->metadata,
				program->symbols,
				program->intrinsic_environment,
				program->namespace_symbol_id,
				imported_interfaces,
				imported_interface_count
				) != 0 || prototype_function_graph_finalize_associations(
					program->asts,
					program->terms,
					program->type_declarations,
					program->metadata
				) != 0) {
			if (error) {
				snprintf(
					error->message,
					sizeof(error->message),
					"%s",
					"failed to generate accepted function graph"
				);
			}
			return -1;
		}
		program->metadata->function_graph_generated_compile_time_ns +=
			compiler_session_elapsed_ns(stage_started);
		program->metadata->function_graph_preflight =
			PROTOTYPE_FUNCTION_GRAPH_COMPILE_NORMAL;
	}
	if (prototype_link_external_refs(program) != 0) {
		if (error) {
			snprintf(error->message, sizeof(error->message), "%s", "failed to link external refs");
		}
		return -1;
	}
	if (program->universe &&
		prototype_universe_build_closed(
			program->universe,
			program->type_declarations,
			program->terms,
			&(struct prototype_typed_occurrence_graph) {
				.occurrences = program->metadata->typed_occurrences.occurrences,
				.occurrence_count = program->metadata->typed_occurrences.occurrence_count,
				.edges = program->metadata->typed_occurrences.edges,
				.edge_count = program->metadata->typed_occurrences.edge_count,
				.cases = program->metadata->typed_occurrences.cases,
				.case_count = program->metadata->typed_occurrences.case_count,
				.fold_clauses = program->metadata->typed_occurrences.fold_clauses,
				.fold_clause_count =
					program->metadata->typed_occurrences.fold_clause_count
			},
			program->judgement
		) != 0) {
		if (error) {
			snprintf(error->message, sizeof(error->message), "%s", "failed to collect universe constraints");
		}
		return -1;
	}
	if (prototype_type_declaration_project_reduction_environment(
			program->terms,
			program->type_declarations,
			program->symbols,
			&program->metadata->reduction_environment
		) != 0) {
		if (error) {
			snprintf(
				error->message,
				sizeof(error->message),
				"%s",
				"failed to project the Core reduction environment"
			);
		}
		return -1;
	}
	return 0;
}

int prototype_compile_graph(
	struct prototype_program* program,
	struct prototype_read_error* error
) {
	return prototype_compile_graph_with_imports(program, NULL, 0, error);
}

static int link_term_against_labels(
	struct prototype_program* program,
	uint32_t term,
	uint32_t* p_ret
) {
	if (!program || !program->terms || !program->metadata || !p_ret ||
		term >= program->terms->term_count) {
		return -1;
	}
	uint32_t current = term;
	for (size_t i = 0; i < program->metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &program->metadata->labels[i];
		uint32_t linked;
		if (label->term >= program->terms->term_count ||
			prototype_term_resolve_external_ref(
				program->terms,
				current,
					(struct prototype_qualified_name){
						program->namespace_symbol_id,
						label->name_symbol_id
					},
				label->term,
				&linked
			) != 0) {
			return -1;
		}
		current = linked;
	}
	*p_ret = current;
	return 0;
}

static int refresh_compile_label_key(
	struct prototype_program* program,
	struct prototype_compile_label* label
) {
	if (!program || !program->terms || !label || label->term >= program->terms->term_count) {
		return -1;
	}
	return prototype_term_canonical_key_with_types(
		program->terms,
		program->type_declarations,
		label->term,
		&label->canonical_key
	);
}

static int term_is_closed_for_link_validation(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term,
	int* p_is_closed
) {
	if (!terms || !p_is_closed || term >= terms->term_count) {
		return -1;
	}
	struct prototype_term_canonical_key key;
	if (prototype_term_canonical_key_with_types(terms, type_declarations, term, &key) != 0) {
		return -1;
	}
	*p_is_closed = key.free_binder_count == 0;
	return 0;
}

static int validate_linked_terms_closed(struct prototype_program* program) {
	if (!program || !program->terms || !program->judgement) {
		return -1;
	}
	for (size_t i = 0; i < program->metadata->label_count; ++i) {
		int closed = 0;
		if (term_is_closed_for_link_validation(
				program->terms,
				program->type_declarations,
				program->metadata->labels[i].term,
				&closed
			) != 0 ||
			!closed) {
			return -1;
		}
	}
	return 0;
}

int prototype_link_external_refs(struct prototype_program* program) {
	if (!program || !program->terms || !program->metadata || !program->judgement) {
		return -1;
	}
	/* Each successful pass must eliminate or relocate at least one reference.
	 * Bound the fixed point by the finite linked graph rather than a magic
	 * pass count unrelated to the artifact size. */
	size_t pass_limit = program->terms->term_count +
		program->judgement->proposition_count + program->judgement->derivation_candidate_count + 1;
	for (size_t pass = 0; pass < pass_limit; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < program->metadata->label_count; ++i) {
			uint32_t linked;
			if (link_term_against_labels(program, program->metadata->labels[i].term, &linked) != 0) {
				return -1;
			}
				if (linked != program->metadata->labels[i].term) {
					program->metadata->labels[i].term = linked;
					if (refresh_compile_label_key(program, &program->metadata->labels[i]) != 0) {
						return -1;
					}
					changed = 1;
				}
			}
		for (size_t i = 0; i < program->judgement->proposition_count; ++i) {
			uint32_t linked_subject;
			uint32_t linked_classifier;
			struct prototype_judgement_proposition* relation =
				&program->judgement->propositions[i];
			if (link_term_against_labels(program, relation->subject, &linked_subject) != 0 ||
				link_term_against_labels(program, relation->classifier, &linked_classifier) != 0) {
				return -1;
			}
			if (linked_subject != relation->subject) {
				relation->subject = linked_subject;
				changed = 1;
			}
			if (linked_classifier != relation->classifier) {
				relation->classifier = linked_classifier;
				changed = 1;
			}
		}
		for (size_t i = 0; i < program->judgement->derivation_candidate_count; ++i) {
			struct prototype_judgement_derivation_candidate* proof = &program->judgement->derivation_candidates[i];
			if (proof->conclusion_proposition_id >=
				program->judgement->proposition_count) {
				return -1;
			}
			proof->conclusion = &program->judgement->propositions[
				proof->conclusion_proposition_id
			];
				if (proof->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
					proof->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
					uint32_t linked_owner;
					if (link_term_against_labels(
							program,
							proof->rule_data.constructor.owner_view,
							&linked_owner
						) != 0) {
						return -1;
					}
					if (linked_owner != proof->rule_data.constructor.owner_view) {
						proof->rule_data.constructor.owner_view = linked_owner;
						changed = 1;
					}
				}
				if (proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
					uint32_t linked_match;
					if (link_term_against_labels(
							program,
							proof->rule_data.induction.match,
							&linked_match
						) != 0) {
						return -1;
					}
					if (linked_match != proof->rule_data.induction.match) {
						proof->rule_data.induction.match = linked_match;
						changed = 1;
					}
				}
				if (proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
					uint32_t linked_motive;
					if (link_term_against_labels(
							program,
							proof->rule_data.induction.motive,
							&linked_motive
						) != 0) {
						return -1;
					}
					if (linked_motive != proof->rule_data.induction.motive) {
						proof->rule_data.induction.motive = linked_motive;
						changed = 1;
					}
				}
		}
		if (!changed) {
			for (size_t i = 0; i < program->metadata->label_count; ++i) {
				if (refresh_compile_label_key(program, &program->metadata->labels[i]) != 0) {
					return -1;
				}
			}
			return validate_linked_terms_closed(program);
		}
	}
	return -1;
}
