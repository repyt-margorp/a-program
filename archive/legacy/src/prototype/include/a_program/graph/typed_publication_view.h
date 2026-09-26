#ifndef A_PROGRAM_PROTOTYPE_GRAPH_TYPED_PUBLICATION_VIEW_H
#define A_PROGRAM_PROTOTYPE_GRAPH_TYPED_PUBLICATION_VIEW_H

#include <stddef.h>
#include <stdint.h>

struct prototype_typed_occurrence_graph;

/* Immutable T2 output for one accepted Layer T occurrence projection. */
struct prototype_typed_publication_projection {
	uint32_t source_occurrence;
	uint32_t typed_projection;
	uint32_t substitution;
	uint32_t subject;
	uint32_t classifier;
};

/* Immutable T2 output for one accepted Layer T Match-case projection. */
struct prototype_typed_publication_match_case_projection {
	uint32_t source_case;
	uint32_t match_case_projection;
	uint32_t concrete_context;
	uint32_t refinement_substitution;
};

struct prototype_typed_publication_view {
	struct prototype_typed_publication_projection* projections;
	uint32_t* concrete_contexts;
	size_t count;
	size_t capacity;
	struct prototype_typed_publication_match_case_projection*
		match_case_projections;
	size_t match_case_count;
	size_t match_case_capacity;
	uint64_t topology_revision;
	uint64_t solution_revision;
	int sealed;
};

void prototype_typed_publication_view_init(
	struct prototype_typed_publication_view* view,
	struct prototype_typed_publication_projection* projections,
	uint32_t* concrete_contexts,
	size_t capacity,
	struct prototype_typed_publication_match_case_projection*
		match_case_projections,
	size_t match_case_capacity
);

int prototype_typed_publication_view_begin_extension(
	struct prototype_typed_publication_view* view
);

int prototype_typed_publication_view_put(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t concrete_context,
	uint32_t substitution,
	uint32_t subject,
	uint32_t classifier
);

/* Complete an unsealed accepted projection by replacing its source-equation
 * classifier with the classifier materialized from the same projection's
 * static endpoint edges. The source answer must still match exactly. */
int prototype_typed_publication_view_project_classifier(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t source_classifier,
	uint32_t projected_classifier
);
int prototype_typed_publication_view_project_subject(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t source_subject,
	uint32_t projected_subject
);
int prototype_typed_publication_view_mark_unreachable(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence
);
int prototype_typed_publication_view_occurrence_is_unreachable(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence
);

int prototype_typed_publication_view_put_match_case(
	struct prototype_typed_publication_view* view,
	uint32_t source_case,
	uint32_t match_case_projection,
	uint32_t concrete_context,
	uint32_t refinement_substitution
);

int prototype_typed_publication_view_seal(
	struct prototype_typed_publication_view* view,
	const struct prototype_typed_occurrence_graph* occurrences,
	uint64_t topology_revision,
	uint64_t solution_revision
);

const struct prototype_typed_publication_projection*
prototype_typed_publication_view_get(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t* p_concrete_context
);

/* Returns 0 for an accepted projection, 1 for a structurally unreachable
 * occurrence, and -1 for an invalid or incomplete publication entry. */
int prototype_typed_publication_view_lookup(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	const struct prototype_typed_publication_projection** p_projection,
	uint32_t* p_concrete_context
);

const struct prototype_typed_publication_match_case_projection*
prototype_typed_publication_view_get_match_case(
	const struct prototype_typed_publication_view* view,
	uint32_t source_case
);

#endif
