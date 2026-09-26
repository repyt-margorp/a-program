#include "a_program/frontend/typing_publication.h"

#include "a_program/core/term.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/support/schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int prototype_accepted_typing_input_view_init(
	struct prototype_accepted_typing_input_view* view,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms,
	const struct prototype_typed_publication_view* publication,
	size_t accepted_occurrence_count,
	size_t accepted_case_count,
	size_t accepted_fold_clause_count
) {
	if (!view || !occurrences || !contexts || !substitutions || !terms ||
		accepted_occurrence_count > occurrences->occurrence_count ||
		accepted_case_count > occurrences->case_count ||
		accepted_fold_clause_count > occurrences->fold_clause_count) {
		return -1;
	}
	memset(view, 0, sizeof(*view));
	view->occurrences = occurrences;
	view->publication = publication;
	view->occurrence_count = accepted_occurrence_count;
	view->case_count = accepted_case_count;
	view->fold_clause_count = accepted_fold_clause_count;
	if (accepted_occurrence_count == 0 && accepted_case_count == 0) {
		view->valid = 1;
		return 0;
	}
	if (!publication || !publication->sealed ||
		publication->count != accepted_occurrence_count ||
		publication->match_case_count != accepted_case_count) {
		fprintf(
			stderr,
			"accepted typing publication mismatch sealed=%d occurrences=%zu/%zu "
			"cases=%zu/%zu\n",
			publication ? publication->sealed : 0,
			publication ? publication->count : 0,
			accepted_occurrence_count,
			publication ? publication->match_case_count : 0,
			accepted_case_count
		);
		return -1;
	}
	for (uint32_t i = 0; i < accepted_occurrence_count; ++i) {
		uint32_t concrete_context;
		const struct prototype_typed_publication_projection* projection = NULL;
		int lookup_status = prototype_typed_publication_view_lookup(
			publication, i, &projection, &concrete_context
		);
		if (lookup_status == 1) {
			continue;
		}
		const struct prototype_substitution* substitution =
			lookup_status == 0 && projection ?
			prototype_substitution_get(substitutions, projection->substitution) : NULL;
		if (lookup_status != 0 || !projection || !substitution ||
			!prototype_context_get(contexts, concrete_context) ||
			substitution->source_context != concrete_context ||
			projection->subject >= terms->term_count) {
			fprintf(
				stderr,
				"accepted typing projection invalid occurrence=%u projection=%p "
				"context=%u substitution=%u source=%u subject=%u terms=%zu\n",
				i,
				(void*)projection,
				concrete_context,
				projection ? projection->substitution : PROTOTYPE_INVALID_ID,
				substitution ? substitution->source_context : PROTOTYPE_INVALID_ID,
				projection ? projection->subject : PROTOTYPE_INVALID_ID,
				terms->term_count
			);
			return -1;
		}
	}
	for (uint32_t i = 0; i < accepted_case_count; ++i) {
		const struct prototype_typed_publication_match_case_projection* projection =
			prototype_typed_publication_view_get_match_case(publication, i);
		if (!projection || !prototype_context_get(
			contexts, projection->concrete_context
		)) {
			fprintf(
				stderr,
				"accepted typing Match projection invalid case=%u projection=%p "
				"context=%u\n",
				i,
				(void*)projection,
				projection ? projection->concrete_context : PROTOTYPE_INVALID_ID
			);
			return -1;
		}
	}
	view->topology_revision = publication->topology_revision;
	view->solution_revision = publication->solution_revision;
	view->valid = 1;
	return 0;
}

const struct prototype_typed_publication_projection*
prototype_accepted_typing_input_projection(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id,
	uint32_t* p_concrete_context
) {
	if (!view || !view->valid || occurrence_id >= view->occurrence_count) {
		return NULL;
	}
	return prototype_typed_publication_view_get(
		view->publication, occurrence_id, p_concrete_context
	);
}

int prototype_accepted_typing_input_lookup(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id,
	const struct prototype_typed_publication_projection** p_projection,
	uint32_t* p_concrete_context
) {
	if (!view || !view->valid || !view->occurrences || !p_projection ||
		occurrence_id >= view->occurrence_count) {
		return -1;
	}
	return prototype_typed_publication_view_lookup(
		view->publication, occurrence_id, p_projection, p_concrete_context
	);
}

uint32_t prototype_accepted_typing_input_binder_classifier(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id
) {
	if (!view || !view->valid || !view->occurrences ||
		occurrence_id >= view->occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	return view->occurrences->occurrences[occurrence_id].binder_classifier;
}

const struct prototype_typed_publication_match_case_projection*
prototype_accepted_typing_input_match_case(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t case_id
) {
	if (!view || !view->valid || case_id >= view->case_count) {
		return NULL;
	}
	return prototype_typed_publication_view_get_match_case(
		view->publication, case_id
	);
}

int prototype_typing_pipeline_graph_build_context(
	struct prototype_typing_pipeline* pipeline,
	const struct prototype_typing_source_results* source_results,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t transaction_occurrence_start,
	uint32_t selected_entry_occurrence,
	struct prototype_typing_graph_build_context* build
) {
	if (!pipeline || !pipeline->initialized || !source_results || !occurrences ||
		!contexts || !substitutions || !build) {
		return -1;
	}
	memset(build, 0, sizeof(*build));
	build->source_results = source_results;
	build->occurrences = occurrences;
	build->contexts = contexts;
	build->substitutions = substitutions;
	build->projections = &pipeline->context_projections;
	/* The occurrence graph may be copied into a fresh compilation image while
	 * retaining the source transaction boundary that introduced only its newest
	 * nodes. Projection incrementality is relative to this pipeline, not to that
	 * foreign boundary. An empty projection DB therefore rebuilds the complete
	 * immutable occurrence topology; only a non-empty DB may append from the
	 * occurrence transaction boundary. */
	build->transaction_occurrence_start =
		pipeline->context_projections.typed_projection_count == 0 ? 0 :
		transaction_occurrence_start;
	build->selected_entry_occurrence = selected_entry_occurrence;
	build->base_projection_for_occurrence =
		pipeline->base_typed_projection_for_occurrence;
	build->base_projection_capacity =
		PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY;
	build->publication_projection_for_occurrence =
		pipeline->publication_typed_projection_for_occurrence;
	build->publication_projection_capacity =
		PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY;
	return 0;
}

static int typing_pipeline_publication_projection_context(
	const struct prototype_typing_pipeline* pipeline,
	uint32_t typed_projection,
	uint32_t* p_concrete_context,
	uint32_t* p_substitution
) {
	if (!pipeline || !p_concrete_context || !p_substitution) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&pipeline->context_projections, typed_projection
		);
	const struct prototype_context_projection* context_projection = projection ?
		prototype_context_projection_get(
			&pipeline->context_projections, projection->context_projection
		) : NULL;
	const struct prototype_context_projection_solution* solution = projection ?
		prototype_context_projection_solution_get(
			&pipeline->context_projections, projection->context_projection
		) : NULL;
	if (!context_projection || !solution) {
		return -1;
	}
	if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
		*p_concrete_context = solution->concrete_context;
		*p_substitution = solution->substitution;
		return 0;
	}
	if (solution->state != PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
		return 1;
	}
	const struct prototype_context_action_expression* action =
		prototype_context_action_expression_get(
			&pipeline->context_projections,
			context_projection->action_expression
		);
	uint32_t parent_projection;
	if (!action || action->kind != PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT ||
		prototype_context_projection_find(
			&pipeline->context_projections,
			context_projection->candidate_context,
			action->parent_action,
			&parent_projection
		) != 0) {
		return -1;
	}
	const struct prototype_context_projection_solution* parent =
		prototype_context_projection_solution_get(
			&pipeline->context_projections, parent_projection
		);
	if (!parent || parent->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
		return -1;
	}
	*p_concrete_context = parent->concrete_context;
	*p_substitution = parent->substitution;
	return 0;
}

static uint32_t typing_pipeline_select_publication_projection(
	const struct prototype_typing_pipeline* pipeline,
	uint32_t occurrence_id
) {
	uint32_t selected =
		pipeline->publication_typed_projection_for_occurrence[occurrence_id];
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&pipeline->context_projections, selected
		);
	if (!projection || projection->source_occurrence != occurrence_id) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t concrete_context;
	uint32_t substitution;
	return typing_pipeline_publication_projection_context(
		pipeline, selected, &concrete_context, &substitution
	) == 0 ? selected : PROTOTYPE_INVALID_ID;
}

static uint32_t typing_pipeline_projection_classifier(
	const struct prototype_typing_pipeline* pipeline,
	uint32_t typed_projection
) {
	if (!pipeline || typed_projection >=
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t constraint = pipeline->constraints.
		classifier_constraint_for_projection[typed_projection];
	if (!prototype_typing_constraint_has_domain(
			&pipeline->constraints, constraint,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return PROTOTYPE_INVALID_ID;
	}
	return pipeline->constraints.solutions[constraint].result_term;
}

static uint32_t typing_pipeline_select_publication_match_case_projection(
	const struct prototype_typing_pipeline* pipeline,
	uint32_t source_case,
	uint32_t owner_typed_projection
) {
	uint32_t selected;
	if (!pipeline || prototype_typed_match_case_projection_find(
			&pipeline->context_projections, source_case, owner_typed_projection,
			&selected
		) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	const struct prototype_typed_match_case_projection* projection =
		prototype_typed_match_case_projection_get(
			&pipeline->context_projections, selected
		);
	const struct prototype_context_projection_solution* context_solution =
		projection ? prototype_context_projection_solution_get(
			&pipeline->context_projections,
			projection->context_projection
		) : NULL;
	uint32_t refinement_constraint = pipeline->constraints.
		branch_refinement_constraint_for_match_case_projection[selected];
	const struct prototype_typing_constraint_solution* refinement_solution =
		refinement_constraint < pipeline->constraints.constraint_count ?
			&pipeline->constraints.solutions[refinement_constraint] : NULL;
	return projection && context_solution &&
		context_solution->state == PROTOTYPE_CONTEXT_PROJECTION_SOLVED &&
		refinement_solution &&
		refinement_solution->branch_refinement.materialized_source_context ==
			context_solution->concrete_context ? selected : PROTOTYPE_INVALID_ID;
}

int prototype_typing_pipeline_build_publication_view(
	struct prototype_typing_pipeline* pipeline,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_typed_publication_view* publication
) {
	if (!pipeline || !pipeline->initialized ||
		!occurrences || !contexts || !substitutions || !terms ||
		!type_declarations || !publication ||
		occurrences->occurrence_count >
			PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY ||
		occurrences->occurrence_count > publication->capacity ||
		occurrences->case_count > publication->match_case_capacity) {
		return -1;
	}
	if (prototype_typed_publication_view_begin_extension(publication) != 0) {
		return -1;
	}
	for (uint32_t occurrence_id = 0;
		occurrence_id < occurrences->occurrence_count;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&occurrences->occurrences[occurrence_id];
		int unreachable =
			prototype_typed_occurrence_graph_occurrence_is_unreachable(
				occurrences, occurrence_id
			);
		if (unreachable < 0) {
			return -1;
		}
		if (unreachable) {
			if (prototype_typed_publication_view_mark_unreachable(
					publication, occurrence_id
				) != 0) {
				return -1;
			}
			continue;
		}
		if (publication->projections[occurrence_id].source_occurrence ==
				occurrence_id) {
			const struct prototype_typed_publication_projection* accepted =
				&publication->projections[occurrence_id];
			const struct prototype_substitution* accepted_substitution =
				prototype_substitution_get(substitutions, accepted->substitution);
			if (!prototype_context_get(
						contexts,
						publication->concrete_contexts[occurrence_id]
					) || !accepted_substitution ||
				accepted_substitution->source_context !=
					publication->concrete_contexts[occurrence_id] ||
				accepted->subject >= terms->term_count) {
				return -1;
			}
			continue;
		}
		uint32_t typed_projection =
			typing_pipeline_select_publication_projection(
				pipeline,
				occurrence_id
			);
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&pipeline->context_projections, typed_projection
			);
		uint32_t concrete_context = PROTOTYPE_INVALID_ID;
		uint32_t substitution = PROTOTYPE_INVALID_ID;
		int projection_context_status = projection ?
			typing_pipeline_publication_projection_context(
				pipeline, typed_projection, &concrete_context, &substitution
			) : -1;
		const struct prototype_substitution* action =
			prototype_substitution_get(substitutions, substitution);
		if (!action || action->source_context != concrete_context) {
			projection_context_status = -1;
		}
		uint32_t subject = PROTOTYPE_INVALID_ID;
		uint32_t classifier = PROTOTYPE_INVALID_ID;
		int subject_status = substitution == PROTOTYPE_INVALID_ID ? -1 :
			prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				occurrence->core_term,
				substitution,
				&subject
			);
		classifier = typing_pipeline_projection_classifier(
			pipeline, typed_projection
		);
		int classifier_status = classifier == PROTOTYPE_INVALID_ID ? -1 : 0;
		if (projection_context_status != 0 ||
			!prototype_context_get(contexts, concrete_context) ||
			typed_projection == PROTOTYPE_INVALID_ID || subject_status != 0 ||
			classifier_status != 0 ||
			prototype_typed_publication_view_put(
				publication,
				occurrence_id,
				typed_projection,
				concrete_context,
				substitution,
				subject,
				classifier
			) != 0) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				uint32_t base = pipeline->base_typed_projection_for_occurrence[
					occurrence_id
				];
				const struct prototype_typed_projection* base_projection =
					prototype_typed_projection_get(
						&pipeline->context_projections, base
					);
				const struct prototype_context_projection_solution* base_solution =
					base_projection ? prototype_context_projection_solution_get(
						&pipeline->context_projections,
						base_projection->context_projection
					) : NULL;
				fprintf(
					stderr,
					"publication occurrence failed source=%u tag=%d context=%u "
					"projection=%u base=%u base-context-projection=%u base-state=%d\n",
					occurrence_id,
					occurrence->tag,
					concrete_context,
					typed_projection,
					base,
					base_projection ? base_projection->context_projection :
						PROTOTYPE_INVALID_ID,
					base_solution ? base_solution->state : -1
				);
			}
			return -1;
		}
	}
	for (uint32_t case_id = 0; case_id < occurrences->case_count; ++case_id) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&occurrences->cases[case_id];
		if (publication->match_case_projections[case_id].source_case ==
				case_id) {
			const struct prototype_typed_publication_match_case_projection*
				accepted = &publication->match_case_projections[case_id];
			uint32_t expected_refinement =
				prototype_typed_occurrence_match_case_is_solved(operation_case) ?
					operation_case->refinement_substitution :
					PROTOTYPE_INVALID_ID;
			if (accepted->refinement_substitution != expected_refinement ||
					!prototype_context_get(
						contexts, accepted->concrete_context
					)) {
				if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
					fprintf(stderr,
						"accepted Match case changed case=%u accepted-refinement=%u "
						"expected-refinement=%u context=%u\n",
						case_id, accepted->refinement_substitution,
						expected_refinement, accepted->concrete_context);
				}
				return -1;
			}
			continue;
		}
		uint32_t owner_occurrence = PROTOTYPE_INVALID_ID;
		for (uint32_t occurrence_id = 0;
			occurrence_id < occurrences->occurrence_count;
			++occurrence_id) {
			const struct prototype_typed_occurrence* occurrence =
				&occurrences->occurrences[occurrence_id];
			if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
				occurrence->first_case == PROTOTYPE_INVALID_ID ||
				case_id < occurrence->first_case ||
				case_id - occurrence->first_case >= occurrence->case_count) {
				continue;
			}
			if (owner_occurrence != PROTOTYPE_INVALID_ID) {
				if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
					fprintf(stderr,
						"publication Match case has multiple owners case=%u "
						"first=%u second=%u\n",
						case_id, owner_occurrence, occurrence_id);
				}
				return -1;
			}
			owner_occurrence = occurrence_id;
		}
		if (owner_occurrence == PROTOTYPE_INVALID_ID ||
			publication->projections[owner_occurrence].source_occurrence !=
				owner_occurrence) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case owner unavailable case=%u owner=%u\n",
					case_id, owner_occurrence);
			}
			return -1;
		}
		uint32_t match_case_projection =
			typing_pipeline_select_publication_match_case_projection(
				pipeline, case_id,
				publication->projections[owner_occurrence].typed_projection
			);
		if (match_case_projection == PROTOTYPE_INVALID_ID) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case has no solved projection case=%u "
					"owner=%u source-context=%u\n",
					case_id, owner_occurrence, operation_case->context_id);
			}
			return -1;
		}
		const struct prototype_typed_match_case_projection* selected =
			prototype_typed_match_case_projection_get(
				&pipeline->context_projections, match_case_projection
			);
		const struct prototype_context_projection* context_projection =
			selected ? prototype_context_projection_get(
				&pipeline->context_projections, selected->context_projection
			) : NULL;
		const struct prototype_context_projection_solution* context_solution =
			selected ? prototype_context_projection_solution_get(
				&pipeline->context_projections, selected->context_projection
			) : NULL;
		if (!context_projection ||
			!context_solution ||
			context_projection->candidate_context != operation_case->context_id ||
			context_solution->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case Context mismatch case=%u projection=%u "
					"expected=%u candidate=%u state=%d\n",
					case_id, match_case_projection, operation_case->context_id,
					context_projection ? context_projection->candidate_context :
						PROTOTYPE_INVALID_ID,
					context_solution ? context_solution->state : -1);
			}
			return -1;
		}
		uint32_t refinement_constraint = pipeline->constraints.
			branch_refinement_constraint_for_match_case_projection[
				match_case_projection
			];
		const struct prototype_typing_constraint* constraint =
			refinement_constraint < pipeline->constraints.constraint_count ?
			&pipeline->constraints.constraints[refinement_constraint] : NULL;
		const struct prototype_typing_constraint_solution* solution =
			refinement_constraint < pipeline->constraints.constraint_count ?
			&pipeline->constraints.solutions[refinement_constraint] : NULL;
		if (!constraint || !solution || constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			solution->branch_refinement.materialized_source_context !=
				context_solution->concrete_context) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case refinement mismatch case=%u projection=%u "
					"constraint=%u domain=%d materialized=%u concrete=%u\n",
					case_id, match_case_projection, refinement_constraint,
					constraint ? constraint->domain : -1,
					solution ? solution->branch_refinement.materialized_source_context :
						PROTOTYPE_INVALID_ID,
					context_solution->concrete_context);
			}
			return -1;
		}
		uint32_t concrete_context =
			solution->branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
				solution->branch_refinement.refined_context :
				solution->branch_refinement.materialized_source_context;
		uint32_t refinement_substitution =
			solution->branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
				solution->branch_refinement.substitution : PROTOTYPE_INVALID_ID;
		if ((refinement_substitution != PROTOTYPE_INVALID_ID) !=
				prototype_typed_occurrence_match_case_is_solved(operation_case) ||
			(refinement_substitution != PROTOTYPE_INVALID_ID &&
			 refinement_substitution != operation_case->refinement_substitution)) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case substitution mismatch case=%u "
					"projection=%u selected=%u source=%u\n",
					case_id, match_case_projection, refinement_substitution,
					operation_case->refinement_substitution);
			}
			return -1;
		}
		if (!prototype_context_get(contexts, concrete_context) ||
			prototype_typed_publication_view_put_match_case(
				publication,
				case_id,
				match_case_projection,
				concrete_context,
				refinement_substitution
			) != 0) {
			if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
				fprintf(stderr,
					"publication Match case insertion failed case=%u projection=%u "
					"context=%u refinement=%u\n",
					case_id, match_case_projection, concrete_context,
					refinement_substitution);
			}
			return -1;
		}
	}
	return 0;
}
