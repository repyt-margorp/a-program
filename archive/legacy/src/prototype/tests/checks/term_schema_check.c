#include "a_program/core/term_schema.h"

#include <stdio.h>
#include <string.h>

static int tag_is_expected(int tag) {
	return tag >= PROTOTYPE_TERM_VAR && tag <= PROTOTYPE_TERM_TAG_MAX &&
		tag != 35 && tag != 36;
}

static int check_tag_coverage(void) {
	for (int tag = 0; tag <= PROTOTYPE_TERM_TAG_MAX + 1; ++tag) {
		int expected = tag_is_expected(tag);
		if (prototype_term_schema_tag_known(tag) != expected) {
			fprintf(stderr, "Term schema tag coverage mismatch: %d\n", tag);
			return -1;
		}
		size_t field_count = 999;
		if ((prototype_term_schema_field_count(tag, &field_count) == 0) !=
			expected) {
			fprintf(stderr, "Term schema field coverage mismatch: %d\n", tag);
			return -1;
		}
	}
	return 0;
}

static int check_field_round_trip(void) {
	for (int tag = PROTOTYPE_TERM_VAR; tag <= PROTOTYPE_TERM_TAG_MAX; ++tag) {
		if (!tag_is_expected(tag)) continue;
		struct prototype_term term;
		memset(&term, 0, sizeof(term));
		term.tag = tag;
		size_t field_count;
		if (prototype_term_schema_field_count(tag, &field_count) != 0) return -1;
		for (size_t i = 0; i < field_count; ++i) {
			struct prototype_term_field_value value;
			if (prototype_term_schema_field_at(&term, i, &value) != 0 ||
				value.role == PROTOTYPE_TERM_FIELD_ROLE_INVALID) return -1;
			if (value.kind == PROTOTYPE_TERM_FIELD_SCALAR_I64) {
				value.as.i64 = INT64_C(-8000000000) - (int64_t)i;
			} else if (value.kind == PROTOTYPE_TERM_FIELD_SYMBOL ||
				value.kind == PROTOTYPE_TERM_FIELD_OPERATION ||
				value.kind == PROTOTYPE_TERM_FIELD_SCALAR_I32) {
				value.as.i32 = -1000 - (int)i;
			} else {
				value.as.u32 = UINT32_C(100000) + (uint32_t)i;
			}
			if (prototype_term_schema_field_write(&term, i, &value) != 0) {
				return -1;
			}
			struct prototype_term_field_value actual;
			if (prototype_term_schema_field_at(&term, i, &actual) != 0 ||
				actual.kind != value.kind || actual.role != value.role ||
				actual.child_role != value.child_role ||
				memcmp(&actual.as, &value.as, sizeof(value.as)) != 0) {
				fprintf(stderr, "Term schema field round trip failed: %d:%zu\n", tag, i);
				return -1;
			}
		}
	}
	return 0;
}

static int check_reference_roles(void) {
	struct prototype_term constructor = {
		.tag = PROTOTYPE_TERM_CONSTRUCTOR,
		.as.constructor = { .owner = 17, .constructor_id = 3 }
	};
	struct prototype_term_field_value owner;
	uint32_t child_count;
	if (prototype_term_schema_field_at(&constructor, 0, &owner) != 0 ||
		owner.kind != PROTOTYPE_TERM_FIELD_TERM_REQUIRED ||
		owner.role != PROTOTYPE_TERM_FIELD_ROLE_CONSTRUCTOR_OWNER ||
		owner.child_role != PROTOTYPE_TERM_CHILD_INVALID ||
		prototype_term_schema_fixed_child_count(&constructor, &child_count) != 0 ||
		child_count != 0) return -1;

	struct prototype_term app = {
		.tag = PROTOTYPE_TERM_APP,
		.as.app = { .function = 41, .argument = 43 }
	};
	struct prototype_term_child child;
	if (prototype_term_schema_fixed_child_count(&app, &child_count) != 0 ||
		child_count != 2 ||
		prototype_term_schema_fixed_child_at(&app, 0, &child) != 0 ||
		child.role != PROTOTYPE_TERM_CHILD_FUNCTION || child.term != 41 ||
		prototype_term_schema_fixed_child_at(&app, 1, &child) != 0 ||
		child.role != PROTOTYPE_TERM_CHILD_ARGUMENT || child.term != 43 ||
		prototype_term_schema_fixed_child_at(&app, 2, &child) == 0) return -1;
	return 0;
}

int main(void) {
	if (check_tag_coverage() != 0 || check_field_round_trip() != 0 ||
		check_reference_roles() != 0) return 1;
	puts("Term semantic field schema checks passed");
	return 0;
}
