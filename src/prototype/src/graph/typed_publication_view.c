#include "a_program/graph/typed_publication_view.h"

#include <string.h>

#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/support/schema.h"

void prototype_typed_publication_view_init(
	struct prototype_typed_publication_view* view,
	struct prototype_typed_publication_projection* projections,
	uint32_t* concrete_contexts,
	size_t capacity,
	struct prototype_typed_publication_match_case_projection*
		match_case_projections,
	size_t match_case_capacity
) {
	if (!view) {
		return;
	}
	memset(view, 0, sizeof(*view));
	view->projections = projections;
	view->concrete_contexts = concrete_contexts;
	view->capacity = projections && concrete_contexts ? capacity : 0;
	view->match_case_projections = match_case_projections;
	view->match_case_capacity = match_case_projections ?
		match_case_capacity : 0;
	for (size_t i = 0; i < view->capacity; ++i) {
		view->projections[i] =
			(struct prototype_typed_publication_projection) {
				.source_occurrence = PROTOTYPE_INVALID_ID,
				.typed_projection = PROTOTYPE_INVALID_ID,
				.substitution = PROTOTYPE_INVALID_ID,
				.subject = PROTOTYPE_INVALID_ID,
				.classifier = PROTOTYPE_INVALID_ID
			};
		view->concrete_contexts[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < view->match_case_capacity; ++i) {
		view->match_case_projections[i] =
			(struct prototype_typed_publication_match_case_projection) {
				.source_case = PROTOTYPE_INVALID_ID,
				.match_case_projection = PROTOTYPE_INVALID_ID,
				.concrete_context = PROTOTYPE_INVALID_ID,
				.refinement_substitution = PROTOTYPE_INVALID_ID
			};
	}
}

int prototype_typed_publication_view_begin_extension(
	struct prototype_typed_publication_view* view
) {
	if (!view ||
		(view->capacity != 0 && (!view->projections || !view->concrete_contexts)) ||
		(view->match_case_capacity != 0 && !view->match_case_projections)) {
		return -1;
	}
	view->sealed = 0;
	view->topology_revision = 0;
	view->solution_revision = 0;
	return 0;
}

int prototype_typed_publication_view_put(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t concrete_context,
	uint32_t substitution,
	uint32_t subject,
	uint32_t classifier
) {
	if (!view || view->sealed || source_occurrence >= view->capacity ||
		concrete_context == PROTOTYPE_INVALID_ID ||
		substitution == PROTOTYPE_INVALID_ID || subject == PROTOTYPE_INVALID_ID ||
		classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_typed_publication_projection value = {
		.source_occurrence = source_occurrence,
		.typed_projection = typed_projection,
		.substitution = substitution,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_typed_publication_projection* current =
		&view->projections[source_occurrence];
	if (current->source_occurrence != PROTOTYPE_INVALID_ID) {
		return current->source_occurrence == value.source_occurrence &&
			current->typed_projection == value.typed_projection &&
			current->substitution == value.substitution &&
			current->subject == value.subject &&
			current->classifier == value.classifier &&
			view->concrete_contexts[source_occurrence] == concrete_context ?
			0 : -1;
	}
	*current = value;
	view->concrete_contexts[source_occurrence] = concrete_context;
	if (view->count <= source_occurrence) {
		view->count = (size_t)source_occurrence + 1;
	}
	return 0;
}

int prototype_typed_publication_view_project_classifier(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t source_classifier,
	uint32_t projected_classifier
) {
	if (!view || view->sealed || source_occurrence >= view->count ||
		projected_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_typed_publication_projection* projection =
		&view->projections[source_occurrence];
	if (projection->source_occurrence != source_occurrence ||
		projection->typed_projection != typed_projection ||
		projection->classifier != source_classifier) {
		return -1;
	}
	projection->classifier = projected_classifier;
	return 0;
}

int prototype_typed_publication_view_project_subject(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t typed_projection,
	uint32_t source_subject,
	uint32_t projected_subject
) {
	if (!view || view->sealed || source_occurrence >= view->count ||
		projected_subject == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_typed_publication_projection* projection =
		&view->projections[source_occurrence];
	if (projection->source_occurrence != source_occurrence ||
		projection->typed_projection != typed_projection ||
		projection->subject != source_subject) {
		return -1;
	}
	projection->subject = projected_subject;
	return 0;
}

int prototype_typed_publication_view_mark_unreachable(
	struct prototype_typed_publication_view* view,
	uint32_t source_occurrence
) {
	if (!view || view->sealed || source_occurrence >= view->capacity) {
		return -1;
	}
	struct prototype_typed_publication_projection value = {
		.source_occurrence = source_occurrence,
		.typed_projection = PROTOTYPE_INVALID_ID,
		.substitution = PROTOTYPE_INVALID_ID,
		.subject = PROTOTYPE_INVALID_ID,
		.classifier = PROTOTYPE_INVALID_ID
	};
	struct prototype_typed_publication_projection* current =
		&view->projections[source_occurrence];
	if (current->source_occurrence != PROTOTYPE_INVALID_ID &&
		memcmp(current, &value, sizeof(value)) != 0) {
		return -1;
	}
	*current = value;
	view->concrete_contexts[source_occurrence] = PROTOTYPE_INVALID_ID;
	if (view->count <= source_occurrence) {
		view->count = (size_t)source_occurrence + 1;
	}
	return 0;
}

int prototype_typed_publication_view_occurrence_is_unreachable(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence
) {
	if (!view || source_occurrence >= view->count) {
		return 0;
	}
	const struct prototype_typed_publication_projection* projection =
		&view->projections[source_occurrence];
	return projection->source_occurrence == source_occurrence &&
		projection->typed_projection == PROTOTYPE_INVALID_ID &&
		projection->substitution == PROTOTYPE_INVALID_ID &&
		projection->subject == PROTOTYPE_INVALID_ID &&
		projection->classifier == PROTOTYPE_INVALID_ID &&
		view->concrete_contexts[source_occurrence] == PROTOTYPE_INVALID_ID;
}

int prototype_typed_publication_view_put_match_case(
	struct prototype_typed_publication_view* view,
	uint32_t source_case,
	uint32_t match_case_projection,
	uint32_t concrete_context,
	uint32_t refinement_substitution
) {
	if (!view || view->sealed || source_case >= view->match_case_capacity ||
		concrete_context == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_typed_publication_match_case_projection value = {
		.source_case = source_case,
		.match_case_projection = match_case_projection,
		.concrete_context = concrete_context,
		.refinement_substitution = refinement_substitution
	};
	struct prototype_typed_publication_match_case_projection* current =
		&view->match_case_projections[source_case];
	if (current->source_case != PROTOTYPE_INVALID_ID) {
		return current->source_case == value.source_case &&
			current->match_case_projection == value.match_case_projection &&
			current->concrete_context == value.concrete_context &&
			current->refinement_substitution == value.refinement_substitution ?
			0 : -1;
	}
	*current = value;
	if (view->match_case_count <= source_case) {
		view->match_case_count = (size_t)source_case + 1;
	}
	return 0;
}

int prototype_typed_publication_view_seal(
	struct prototype_typed_publication_view* view,
	const struct prototype_typed_occurrence_graph* occurrences,
	uint64_t topology_revision,
	uint64_t solution_revision
) {
	if (!view || !occurrences || occurrences->occurrence_count > view->capacity ||
		occurrences->case_count > view->match_case_capacity) {
		return -1;
	}
	if (view->sealed) {
		return view->count == occurrences->occurrence_count &&
			view->match_case_count == occurrences->case_count &&
			view->topology_revision == topology_revision &&
			view->solution_revision == solution_revision ? 0 : -1;
	}
	for (size_t i = 0; i < occurrences->occurrence_count; ++i) {
		int unreachable =
			prototype_typed_occurrence_graph_occurrence_is_unreachable(
				occurrences, (uint32_t)i
			);
		if (unreachable < 0 ||
			(unreachable &&
			 !prototype_typed_publication_view_occurrence_is_unreachable(
				view, (uint32_t)i
			 ))) {
			return -1;
		}
		if (unreachable) {
			continue;
		}
		if (view->projections[i].source_occurrence != i ||
			view->concrete_contexts[i] == PROTOTYPE_INVALID_ID ||
			view->projections[i].substitution == PROTOTYPE_INVALID_ID ||
			view->projections[i].subject == PROTOTYPE_INVALID_ID ||
			view->projections[i].classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (size_t i = 0; i < occurrences->case_count; ++i) {
		if (view->match_case_projections[i].source_case != i ||
			view->match_case_projections[i].concrete_context ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	view->count = occurrences->occurrence_count;
	view->match_case_count = occurrences->case_count;
	view->topology_revision = topology_revision;
	view->solution_revision = solution_revision;
	view->sealed = 1;
	return 0;
}

const struct prototype_typed_publication_projection*
prototype_typed_publication_view_get(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	uint32_t* p_concrete_context
) {
	if (!view || !view->sealed || source_occurrence >= view->count ||
		view->projections[source_occurrence].source_occurrence !=
			source_occurrence ||
		view->concrete_contexts[source_occurrence] == PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	if (p_concrete_context) {
		*p_concrete_context = view->concrete_contexts[source_occurrence];
	}
	return &view->projections[source_occurrence];
}

int prototype_typed_publication_view_lookup(
	const struct prototype_typed_publication_view* view,
	uint32_t source_occurrence,
	const struct prototype_typed_publication_projection** p_projection,
	uint32_t* p_concrete_context
) {
	if (!view || !p_projection || !view->sealed ||
		source_occurrence >= view->count) {
		return -1;
	}
	if (prototype_typed_publication_view_occurrence_is_unreachable(
			view, source_occurrence
		)) {
		*p_projection = &view->projections[source_occurrence];
		if (p_concrete_context) {
			*p_concrete_context = PROTOTYPE_INVALID_ID;
		}
		return 1;
	}
	const struct prototype_typed_publication_projection* projection =
		prototype_typed_publication_view_get(
			view, source_occurrence, p_concrete_context
		);
	if (!projection) {
		return -1;
	}
	*p_projection = projection;
	return 0;
}

const struct prototype_typed_publication_match_case_projection*
prototype_typed_publication_view_get_match_case(
	const struct prototype_typed_publication_view* view,
	uint32_t source_case
) {
	if (!view || !view->sealed || source_case >= view->match_case_count ||
		view->match_case_projections[source_case].source_case != source_case ||
		view->match_case_projections[source_case].concrete_context ==
			PROTOTYPE_INVALID_ID) {
		return NULL;
	}
	return &view->match_case_projections[source_case];
}
