#include <stdint.h>

#include "a_program/graph/compile_metadata.h"

int main(void) {
	struct prototype_typed_occurrence operation = {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM,
		.classifier = 17
	};
	struct prototype_compile_label label = {
		.name_symbol_id = 5,
		.body_occurrence = 0,
		.body_classifier = 17,
		.exposed_occurrence = 0,
		.exposed_classifier = 17,
		.expectation_classifier = PROTOTYPE_INVALID_ID,
		.expectation_claim_id = PROTOTYPE_INVALID_ID
	};
	struct prototype_compile_metadata metadata = {
		.typed_occurrences = {
			.occurrences = &operation,
			.occurrence_count = 1,
			.occurrence_capacity = 1
		},
		.labels = &label,
		.label_count = 1,
		.label_capacity = 1
	};
	struct prototype_type_inspection inspection;

	if (prototype_compile_metadata_inspect_type(
			&metadata, 5, &inspection
		) != PROTOTYPE_TYPE_INSPECTION_AVAILABLE ||
		inspection.body_occurrence != 0 || inspection.body_classifier != 17 ||
		prototype_compile_metadata_inspect_type(
			&metadata, 6, &inspection
		) != PROTOTYPE_TYPE_INSPECTION_UNAVAILABLE ||
		prototype_compile_metadata_inspect_type(
			NULL, 5, &inspection
		) != PROTOTYPE_TYPE_INSPECTION_INVALID) {
		return 1;
	}

	/* A published label is occurrence-authoritative. If that selected principal
	 * is incoherent, inspection must report ambiguity instead of searching for
	 * a convenient HAS_TYPE proposition attached to the shared Core Term. */
	label.body_classifier = 18;
	if (prototype_compile_metadata_inspect_type(
			&metadata, 5, &inspection
		) != PROTOTYPE_TYPE_INSPECTION_AMBIGUOUS) {
		return 2;
	}
	return 0;
}
