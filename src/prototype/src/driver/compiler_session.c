#include "compiler_session_internal.h"
#include "a_program/frontend/function_graph.h"
#include "a_program/frontend/source_epoch.h"
#include "a_program/frontend/source_lowering_plan.h"
#include "a_program/frontend/reader.h"
#include "a_program/frontend/ast_inspect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

static struct prototype_term_db* program_core_terms(
	struct prototype_program* program
) {
	return program && program->core ?
		prototype_core_pipeline_terms(program->core) : NULL;
}

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
		!program_core_terms(program) || !program->judgement || !program->metadata) {
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
	int install_stage = 1;
	if (prototype_type_declaration_add(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			&program->type_declarations->representation_db, nat_symbol, &type_id) != 0 ||
		(++install_stage, prototype_type_expr_self(
			&program->type_declarations->readback, &self_expr
		)) != 0 ||
		(++install_stage,
		prototype_type_projection_source_instance_make(
			program_core_terms(program),
			&program->type_declarations->semantic_schema,
			type_id,
			NULL,
			0,
			&nat_term
		)) != 0 ||
		(++install_stage, prototype_term_pi(
			program_core_terms(program), nat_term, nat_term, &succ_classifier
		)) != 0 ||
		(++install_stage,
		(empty_context = prototype_context_empty(
			&program->metadata->contexts
		))) == PROTOTYPE_INVALID_ID ||
		(++install_stage,
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
		)) != 0 || (++install_stage, prototype_type_readback_attach_constructor(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->readback,
			zero_constructor_id,
			NULL,
			0,
			self_expr
		)) != 0 || (++install_stage, prototype_type_constructor_classifier_cache_set(
			&program->type_declarations->semantic_schema,
			&program->type_declarations->constructor_classifier_cache,
			zero_constructor_id,
			nat_term
		)) != 0) {
		fprintf(stderr, "system Nat installation failed at stage %d\n", install_stage);
		return -1;
	}
	succ_field = self_expr;
	succ_binder = prototype_term_binding_for_scope_slot(program_core_terms(program), 0);
	if (succ_binder == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			&program->metadata->contexts,
			empty_context,
			succ_binder,
			nat_term,
			&succ_field_context
		) != 0) {
		fprintf(stderr, "system Nat field context construction failed\n");
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
			program_core_terms(program), program->judgement->next_universe_var++, &universe
		) != 0) {
		fprintf(stderr, "system Nat successor schema construction failed\n");
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
	int representation_status = prototype_type_declaration_rebuild_representations(
			program_core_terms(program),
			program->type_declarations,
			&program->metadata->contexts
		);
	int projection_status = representation_status == 0 ?
		prototype_type_projection_instance_make(
			program_core_terms(program),
			program->type_declarations,
			type_id,
			NULL,
			0,
			&nat_term
		) : -1;
	if (representation_status != 0 || projection_status != 0) {
		fprintf(
			stderr,
			"system Nat projection failed: representation=%d projection=%d\n",
			representation_status,
			projection_status
		);
		return -1;
	}
	if (prototype_judgement_expand_type_def(
			program->judgement,
			program_core_terms(program),
			program->type_declarations,
			nat_term,
			universe
		) != 0 ||
		prototype_term_constructor(program_core_terms(program), nat_term, 0, &zero_term) != 0 ||
		prototype_judgement_expand_constructor_def(
			program->judgement,
			program_core_terms(program),
			program->type_declarations,
			zero_term,
			nat_term
			) != 0 ||
			prototype_term_constructor(program_core_terms(program), nat_term, 1, &succ_term) != 0 ||
			prototype_judgement_expand_constructor_def(
				program->judgement,
				program_core_terms(program),
			program->type_declarations,
			succ_term,
			succ_classifier
		) != 0) {
		fprintf(stderr, "system Nat judgement installation failed\n");
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
	if (!program || !program->core || !program->typing ||
		!program->intrinsic_environment || !program->asts ||
		!program->type_declarations || !program_core_terms(program) || !program->judgement ||
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
	if (program->compile_options.solve_effort_is_set) {
		prototype_effort_account_init(
			&program->metadata->effort,
			program->compile_options.solve_effort_steps
		);
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
		program->core,
		program->typing,
		program->type_declarations,
		program->judgement,
		program->universe,
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
				program->metadata->effort.exhausted ?
					"classifier solver step limit exhausted" :
					"failed to compile AST graph"
			);
		}
		return -1;
	}
	program->metadata->source_compile_time_ns +=
		compiler_session_elapsed_ns(stage_started);
	if (function_graph_requested) {
		struct prototype_source_epoch_storage generated_epoch;
		if (prototype_source_epoch_clone(
				program->asts, &generated_epoch
			) != 0) {
			if (error) {
				snprintf(
					error->message, sizeof(error->message), "%s",
					"failed to clone sealed source epoch"
				);
			}
			return -1;
		}
		struct prototype_ast_db* generated_asts = &generated_epoch.asts;
		uint64_t generated_source_fingerprint = 0;
		size_t source_ast_nodes = generated_asts->node_count;
		size_t source_assignments = generated_asts->assignment_count;
		size_t source_types = generated_asts->type_def_count;
		size_t source_constructors = generated_asts->type_constructor_count;
		stage_started = compiler_session_clock_ns();
		int generation_status = prototype_function_graph_generate_requested(
				generated_asts,
				program_core_terms(program),
				program->type_declarations,
				program->judgement,
				program->metadata,
				program->symbols
			);
		if (generation_status == 0) {
			generation_status = prototype_function_graph_prepare_generated_source(
				generated_asts,
				program->type_declarations,
				program->metadata
			);
		}
		if (generation_status == 0) {
			generation_status = prototype_source_epoch_parent_matches(
				&generated_epoch,
				program->asts
			);
		}
		if (generation_status == 0) {
			generation_status = prototype_source_epoch_fingerprint(
				generated_asts,
				&generated_source_fingerprint
			);
		}
		if (generation_status != 0) {
			if (error) {
				snprintf(
					error->message,
					sizeof(error->message),
					"%s",
						"failed to generate accepted function graph"
					);
			}
			prototype_source_epoch_destroy(&generated_epoch);
			return -1;
		}
		program->metadata->function_graph_generation_time_ns +=
			compiler_session_elapsed_ns(stage_started);
		program->metadata->function_graph_source_ast_node_count += source_ast_nodes;
		program->metadata->function_graph_generated_ast_node_count +=
			generated_asts->node_count - source_ast_nodes;
		program->metadata->function_graph_generated_assignment_count +=
			generated_asts->assignment_count - source_assignments;
		program->metadata->function_graph_generated_type_count +=
			generated_asts->type_def_count - source_types;
		program->metadata->function_graph_generated_constructor_count +=
			generated_asts->type_constructor_count - source_constructors;
		/* Generated declarations form a distinct source epoch. The original
		 * accepted epoch remains immutable and is used only as the parent. */
		program->metadata->function_graph_preflight =
			PROTOTYPE_FUNCTION_GRAPH_COMPILE_GENERATED_CLOSURE;

		stage_started = compiler_session_clock_ns();
		int generated_compile_status = prototype_ast_compile_pending_with_imports(
				generated_asts,
				program->core,
				program->typing,
				program->type_declarations,
				program->judgement,
				program->universe,
				program->metadata,
				program->symbols,
				program->intrinsic_environment,
				program->namespace_symbol_id,
				imported_interfaces,
				imported_interface_count
			);
		if (generated_compile_status != 0) {
			fprintf(stderr,
				"generated function graph compilation failed status=%d "
				"effort=%" PRIu64 "/%" PRIu64 " phase=%d\n",
				generated_compile_status,
				program->metadata->effort.used,
				program->metadata->effort.limit,
				program->metadata->effort.exhausted_phase);
			if (getenv("A_PROGRAM_SOLVER_COUNTERS")) {
				fprintf(stderr,
					"generated solver counters enqueue=%" PRIu64
					" duplicate=%" PRIu64 " pop=%" PRIu64 "\n",
					program->metadata->constraint_enqueue_count,
					program->metadata->constraint_enqueue_duplicate_count,
					program->metadata->constraint_pop_count);
				for (int cause = 0;
					cause < PROTOTYPE_COMPILE_ENQUEUE_CAUSE_COUNT; ++cause) {
					fprintf(stderr,
						"generated solver enqueue cause=%d accepted=%" PRIu64
						" duplicate=%" PRIu64 "\n",
						cause,
						program->metadata->constraint_enqueue_by_cause[cause],
						program->metadata->constraint_enqueue_duplicate_by_cause[cause]);
				}
				for (int kind = 0;
					kind < OPERATION_CLASSIFIER_CONSTRAINT_KIND_COUNT;
					++kind) {
					if (program->metadata->constraint_pop_by_kind[kind] != 0) {
						fprintf(stderr,
							"generated solver constraint kind=%d pop=%" PRIu64
							" changed=%" PRIu64 " noop=%" PRIu64 "\n",
							kind,
							program->metadata->constraint_pop_by_kind[kind],
							program->metadata->constraint_changed_by_kind[kind],
							program->metadata->constraint_noop_by_kind[kind]);
					}
				}
			}
		}
		int association_status = generated_compile_status == 0 ?
			prototype_function_graph_finalize_associations(
					generated_asts,
					program_core_terms(program),
					program->type_declarations,
					program->metadata
				) : 0;
		if (prototype_source_epoch_parent_matches(
				&generated_epoch, program->asts
			) != 0) {
			fprintf(stderr, "generated function graph mutated parent source epoch\n");
			association_status = -1;
		}
		uint64_t compiled_source_fingerprint;
		int fingerprint_status = prototype_source_epoch_fingerprint(
				generated_asts,
				&compiled_source_fingerprint
			);
		if (fingerprint_status != 0) {
			fprintf(stderr, "generated function graph source epoch is invalid\n");
			association_status = -1;
		} else if (compiled_source_fingerprint != generated_source_fingerprint) {
			fprintf(stderr, "generated function graph compilation mutated source epoch\n");
			association_status = -1;
		}
		if (association_status != 0) {
			fprintf(stderr,
				"generated function graph association finalization failed status=%d\n",
				association_status);
		}
		if (generated_compile_status != 0 || association_status != 0) {
			if (error) {
				snprintf(
					error->message,
					sizeof(error->message),
					"%s",
					"failed to generate accepted function graph"
				);
			}
			prototype_source_epoch_destroy(&generated_epoch);
			return -1;
		}
		program->metadata->function_graph_generated_compile_time_ns +=
			compiler_session_elapsed_ns(stage_started);
		program->metadata->function_graph_preflight =
			PROTOTYPE_FUNCTION_GRAPH_COMPILE_NORMAL;
		prototype_source_epoch_destroy(&generated_epoch);
	}
	if (prototype_type_declaration_project_reduction_environment(
			program_core_terms(program),
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
