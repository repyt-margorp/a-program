#include "a_program/frontend/typing_constraint_state.h"
#include "a_program/support/schema.h"

#include <stdint.h>
#include <stdlib.h>

int main(void) {
	struct prototype_typing_constraint_db* db = calloc(1, sizeof(*db));
	if (!db || prototype_typing_constraint_db_init(db) != 0) {
		free(db);
		return 1;
	}

	uint32_t literal;
	uint32_t duplicate_literal;
	if (prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
			11,
			NULL,
			0,
			&literal
		) != 0 || prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
			11,
			NULL,
			0,
			&duplicate_literal
		) != 0 || literal != duplicate_literal || db->classifier_rhs_count != 1) {
		free(db);
		return 2;
	}

	const struct prototype_typing_equation_operand endpoint_a = {
		.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
		.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE,
		.target = 5,
		.qualifier = 0
	};
	const struct prototype_typing_equation_operand endpoint_b = {
		.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT,
		.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE,
		.target = 6,
		.qualifier = 0
	};
	uint32_t endpoint_rhs_a;
	uint32_t endpoint_rhs_b;
	if (prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
			12,
			&endpoint_a,
			1,
			&endpoint_rhs_a
		) != 0 || prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT,
			12,
			&endpoint_b,
			1,
			&endpoint_rhs_b
		) != 0 || endpoint_rhs_a == endpoint_rhs_b) {
		free(db);
		return 3;
	}

	struct prototype_typing_constraint_db_mark mark =
		prototype_typing_constraint_db_mark(db);
	const struct prototype_typing_equation_operand app_operands[] = {
		{
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
			.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
			.target = endpoint_rhs_a,
			.qualifier = 0
		},
		{
			.role = PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD,
			.target_kind = PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS,
			.target = literal,
			.qualifier = 1
		}
	};
	uint32_t app_rhs;
	if (prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP,
			13,
			app_operands,
			2,
			&app_rhs
		) != 0 || !prototype_typing_classifier_rhs_get(db, app_rhs) ||
		!prototype_typing_classifier_rhs_operand_get(
			db, prototype_typing_classifier_rhs_get(db, app_rhs), 1
		)) {
		free(db);
		return 4;
	}
	if (prototype_typing_constraint_db_rollback(db, mark) != 0 ||
		db->classifier_rhs_count != mark.classifier_rhs_count ||
		db->operand_count != mark.operand_count ||
		prototype_typing_classifier_rhs_get(db, app_rhs)) {
		free(db);
		return 5;
	}
	uint32_t replayed_app_rhs;
	if (prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP,
			13,
			app_operands,
			2,
			&replayed_app_rhs
		) != 0 || replayed_app_rhs != app_rhs) {
		free(db);
		return 6;
	}

	uint32_t equation[5];
	for (uint32_t i = 0; i < 5; ++i) {
		struct prototype_typing_constraint constraint = {
			.domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER,
			.kind = OPERATION_CONSTRAINT_HAS_TYPE,
			.owner_typed_projection = i,
			.source_occurrence = i,
			.source_ast = PROTOTYPE_INVALID_ID,
			.origin_constraint_id = PROTOTYPE_INVALID_ID,
			.classifier_rhs_root = PROTOTYPE_INVALID_ID
		};
		if (prototype_typing_constraint_intern(
				db, &constraint, NULL, 0, &equation[i]
			) != 0) {
			free(db);
			return 7;
		}
	}
	db->classifier_constraint_for_projection[3] = equation[3];
	if (prototype_typing_constraint_add_equation_dependency(
			db, equation[0], equation[1]
		) != 0 || prototype_typing_constraint_add_equation_dependency(
			db, equation[1], equation[0]
		) != 0 || prototype_typing_constraint_add_equation_dependency(
			db, equation[2], equation[2]
		) != 0 || prototype_typing_constraint_add_dependency(
			db, 5, 3, equation[4]
		) != 0 || prototype_typing_constraint_derive_sccs(db, 5) != 0) {
		free(db);
		return 8;
	}
	uint32_t mutual_scc = db->scc_for_constraint[equation[0]];
	uint32_t self_scc = db->scc_for_constraint[equation[2]];
	uint32_t source_scc = db->scc_for_constraint[equation[3]];
	uint32_t target_scc = db->scc_for_constraint[equation[4]];
	if (mutual_scc != db->scc_for_constraint[equation[1]] ||
		mutual_scc >= db->scc_count || !db->scc_recursive[mutual_scc] ||
		db->scc_member_count[mutual_scc] != 2 ||
		self_scc >= db->scc_count || !db->scc_recursive[self_scc] ||
		db->scc_member_count[self_scc] != 1 || source_scc == target_scc ||
		db->scc_recursive[source_scc] || db->scc_recursive[target_scc] ||
		db->scc_member_total != db->constraint_count) {
		free(db);
		return 9;
	}

	db->topology_sealed = 1;
	uint32_t rejected;
	if (prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP,
			13,
			app_operands,
			2,
			&rejected
		) != 0 || rejected != replayed_app_rhs ||
		prototype_typing_classifier_rhs_intern(
			db,
			PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL,
			14,
			NULL,
			0,
			&rejected
			) == 0 || prototype_typing_constraint_db_rollback(db, mark) == 0) {
		free(db);
		return 10;
	}

	free(db);
	return 0;
}
