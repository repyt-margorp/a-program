#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_PUBLICATION_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_PUBLICATION_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/context_projection_builder.h"
#include "a_program/frontend/typing_pipeline.h"
#include "a_program/graph/typed_publication_view.h"

struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_typed_occurrence_graph;
struct prototype_typing_source_results;

/* Read-only publication and graph-construction views over Layer T. */
struct prototype_accepted_typing_input_view {
	const struct prototype_typed_occurrence_graph* occurrences;
	const struct prototype_typed_publication_view* publication;
	size_t occurrence_count;
	size_t case_count;
	size_t fold_clause_count;
	uint64_t topology_revision;
	uint64_t solution_revision;
	int valid;
};

int prototype_typing_pipeline_graph_build_context(
	struct prototype_typing_pipeline* pipeline,
	const struct prototype_typing_source_results* source_results,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t transaction_occurrence_start,
	uint32_t selected_entry_occurrence,
	struct prototype_typing_graph_build_context* build
);

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
);

const struct prototype_typed_publication_projection*
prototype_accepted_typing_input_projection(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id,
	uint32_t* p_concrete_context
);

/* Returns 0 for an accepted projection, 1 for a structurally unreachable
 * occurrence, and -1 for an invalid or incomplete accepted input. */
int prototype_accepted_typing_input_lookup(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id,
	const struct prototype_typed_publication_projection** p_projection,
	uint32_t* p_concrete_context
);

uint32_t prototype_accepted_typing_input_binder_classifier(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t occurrence_id
);

const struct prototype_typed_publication_match_case_projection*
prototype_accepted_typing_input_match_case(
	const struct prototype_accepted_typing_input_view* view,
	uint32_t case_id
);

int prototype_typing_pipeline_build_publication_view(
	struct prototype_typing_pipeline* pipeline,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_typed_publication_view* publication
);

#endif
