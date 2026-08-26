#include "a_program/core/term_schema.h"

#include <stddef.h>
#include <string.h>

struct term_field_descriptor {
	int kind;
	int role;
	int child_role;
	size_t offset;
};

struct term_tag_descriptor {
	const struct term_field_descriptor* fields;
	size_t field_count;
	int known;
};

_Static_assert(sizeof(int) == sizeof(uint32_t), "Term scalar width");

#define TERM_FIELD(member, field_kind, field_role, term_child_role) \
	{ (field_kind), (field_role), (term_child_role), \
		offsetof(struct prototype_term, member) }
#define TERM_U32(member, field_role) \
	TERM_FIELD(member, PROTOTYPE_TERM_FIELD_SCALAR_U32, field_role, \
		PROTOTYPE_TERM_CHILD_INVALID)
#define TERM_I32(member, field_role) \
	TERM_FIELD(member, PROTOTYPE_TERM_FIELD_SCALAR_I32, field_role, \
		PROTOTYPE_TERM_CHILD_INVALID)
#define TERM_I64(member, field_role) \
	TERM_FIELD(member, PROTOTYPE_TERM_FIELD_SCALAR_I64, field_role, \
		PROTOTYPE_TERM_CHILD_INVALID)
#define TERM_REF(member, field_kind, field_role, term_child_role) \
	TERM_FIELD(member, field_kind, field_role, term_child_role)

static const struct term_field_descriptor var_fields[] = {
	TERM_REF(as.var.binding_id, PROTOTYPE_TERM_FIELD_BINDING,
		PROTOTYPE_TERM_FIELD_ROLE_VAR_BINDING, PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor constructor_fields[] = {
	TERM_REF(as.constructor.owner, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_CONSTRUCTOR_OWNER, PROTOTYPE_TERM_CHILD_INVALID),
	TERM_U32(as.constructor.constructor_id,
		PROTOTYPE_TERM_FIELD_ROLE_CONSTRUCTOR_ORDINAL)
};
static const struct term_field_descriptor app_fields[] = {
	TERM_REF(as.app.function, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_APP_FUNCTION, PROTOTYPE_TERM_CHILD_FUNCTION),
	TERM_REF(as.app.argument, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_APP_ARGUMENT, PROTOTYPE_TERM_CHILD_ARGUMENT)
};
static const struct term_field_descriptor lambda_fields[] = {
	TERM_REF(as.lambda.binding_id, PROTOTYPE_TERM_FIELD_BINDING,
		PROTOTYPE_TERM_FIELD_ROLE_LAMBDA_BINDING, PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.lambda.body, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_LAMBDA_BODY, PROTOTYPE_TERM_CHILD_BODY)
};
static const struct term_field_descriptor pi_fields[] = {
	TERM_REF(as.pi.domain, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_PI_DOMAIN, PROTOTYPE_TERM_CHILD_DOMAIN),
	TERM_REF(as.pi.codomain_family, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_PI_CODOMAIN_FAMILY,
		PROTOTYPE_TERM_CHILD_CODOMAIN_FAMILY)
};
static const struct term_field_descriptor match_fields[] = {
	TERM_REF(as.match.scrutinee, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_MATCH_SCRUTINEE,
		PROTOTYPE_TERM_CHILD_SCRUTINEE),
	TERM_REF(as.match.first_case, PROTOTYPE_TERM_FIELD_CASE_SLICE,
		PROTOTYPE_TERM_FIELD_ROLE_MATCH_FIRST_CASE, PROTOTYPE_TERM_CHILD_INVALID),
	TERM_U32(as.match.case_count, PROTOTYPE_TERM_FIELD_ROLE_MATCH_CASE_COUNT),
	TERM_REF(as.match.ih_scope_id, PROTOTYPE_TERM_FIELD_IH_SCOPE,
		PROTOTYPE_TERM_FIELD_ROLE_MATCH_IH_SCOPE, PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor type_former_fields[] = {
	TERM_REF(as.type_former.declaration_type_id,
		PROTOTYPE_TERM_FIELD_TYPE_DECLARATION,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_FORMER_DECLARATION,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_former.representation_id,
		PROTOTYPE_TERM_FIELD_REPRESENTATION,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_FORMER_REPRESENTATION,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_U32(as.type_former.constructor_count,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_FORMER_CONSTRUCTOR_COUNT)
};
static const struct term_field_descriptor type_declaration_fields[] = {
	TERM_REF(as.type_declaration.type_id, PROTOTYPE_TERM_FIELD_TYPE_DECLARATION,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_DECLARATION_ID,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_declaration.identity.namespace_symbol_id,
		PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_DECLARATION_NAMESPACE,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_declaration.identity.name_symbol_id,
		PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_DECLARATION_NAME,
		PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor type_view_fields[] = {
	TERM_REF(as.type_view.view_type_id, PROTOTYPE_TERM_FIELD_TYPE_DECLARATION,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_ID, PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_view.identity.namespace_symbol_id,
		PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_NAMESPACE,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_view.identity.name_symbol_id,
		PROTOTYPE_TERM_FIELD_SYMBOL, PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_NAME,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.type_view.core, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_CORE,
		PROTOTYPE_TERM_CHILD_TYPE_VIEW_CORE),
	TERM_REF(as.type_view.source, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_SOURCE,
		PROTOTYPE_TERM_CHILD_TYPE_VIEW_SOURCE)
};
static const struct term_field_descriptor induction_hypothesis_fields[] = {
	TERM_REF(as.induction_hypothesis.ih_scope_id, PROTOTYPE_TERM_FIELD_IH_SCOPE,
		PROTOTYPE_TERM_FIELD_ROLE_IH_SCOPE, PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.induction_hypothesis.argument, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_IH_ARGUMENT,
		PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT)
};
static const struct term_field_descriptor universe_var_fields[] = {
	TERM_REF(as.universe_var.level_var, PROTOTYPE_TERM_FIELD_UNIVERSE_LEVEL,
		PROTOTYPE_TERM_FIELD_ROLE_UNIVERSE_LEVEL, PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor text_literal_fields[] = {
	TERM_REF(as.text_literal.text_symbol_id, PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_TEXT_SYMBOL, PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor int_literal_fields[] = {
	TERM_I64(as.int_literal.value, PROTOTYPE_TERM_FIELD_ROLE_INT_LITERAL)
};
static const struct term_field_descriptor external_ref_fields[] = {
	TERM_REF(as.external_ref.name.namespace_symbol_id, PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_EXTERNAL_NAMESPACE,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.external_ref.name.name_symbol_id, PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_EXTERNAL_NAME, PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor pure_primitive_fields[] = {
	TERM_I32(as.pure_primitive.primitive_id,
		PROTOTYPE_TERM_FIELD_ROLE_PRIMITIVE_ID),
	TERM_REF(as.pure_primitive.type_symbol_id, PROTOTYPE_TERM_FIELD_SYMBOL,
		PROTOTYPE_TERM_FIELD_ROLE_PRIMITIVE_TYPE,
		PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor effect_operation_fields[] = {
	TERM_REF(as.effect_operation.operation_id, PROTOTYPE_TERM_FIELD_OPERATION,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_OPERATION_ID,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.effect_operation.classifier, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_OPERATION_CLASSIFIER,
		PROTOTYPE_TERM_CHILD_EFFECT_OPERATION_CLASSIFIER)
};
static const struct term_field_descriptor effect_row_var_fields[] = {
	TERM_REF(as.effect_row_var.binding_id, PROTOTYPE_TERM_FIELD_BINDING,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_BINDING,
		PROTOTYPE_TERM_CHILD_INVALID)
};
static const struct term_field_descriptor effect_row_union_fields[] = {
	TERM_REF(as.effect_row_union.left, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_LEFT,
		PROTOTYPE_TERM_CHILD_EFFECT_ROW_LEFT),
	TERM_REF(as.effect_row_union.right, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_RIGHT,
		PROTOTYPE_TERM_CHILD_EFFECT_ROW_RIGHT)
};
static const struct term_field_descriptor effect_row_forall_fields[] = {
	TERM_REF(as.effect_row_forall.binding_id, PROTOTYPE_TERM_FIELD_BINDING,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_FORALL_BINDING,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.effect_row_forall.body, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_FORALL_BODY,
		PROTOTYPE_TERM_CHILD_EFFECT_ROW_BODY)
};
static const struct term_field_descriptor effect_row_operation_fields[] = {
	TERM_REF(as.effect_row_operation.operation_id,
		PROTOTYPE_TERM_FIELD_OPERATION,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_OPERATION_ID,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_REF(as.effect_row_operation.latent_row,
		PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_EFFECT_ROW_LATENT,
		PROTOTYPE_TERM_CHILD_EFFECT_ROW_LATENT)
};
static const struct term_field_descriptor computation_type_fields[] = {
	TERM_REF(as.computation_type.label, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_COMPUTATION_EFFECT_ROW,
		PROTOTYPE_TERM_CHILD_COMPUTATION_EFFECT_ROW),
	TERM_REF(as.computation_type.result, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_COMPUTATION_RESULT,
		PROTOTYPE_TERM_CHILD_SEQUENCE_RESULT),
	TERM_I32(as.computation_type.totality,
		PROTOTYPE_TERM_FIELD_ROLE_COMPUTATION_TOTALITY)
};
static const struct term_field_descriptor thunk_type_fields[] = {
	TERM_REF(as.thunk_type.computation, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_THUNK_TYPE_COMPUTATION,
		PROTOTYPE_TERM_CHILD_THUNK_TYPE_COMPUTATION)
};
static const struct term_field_descriptor return_fields[] = {
	TERM_REF(as.return_term.value, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_RETURN_VALUE,
		PROTOTYPE_TERM_CHILD_RETURN_VALUE)
};
static const struct term_field_descriptor thunk_fields[] = {
	TERM_REF(as.thunk.computation, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_THUNK_COMPUTATION,
		PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION)
};
static const struct term_field_descriptor force_fields[] = {
	TERM_REF(as.force.value, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_FORCE_VALUE, PROTOTYPE_TERM_CHILD_FORCE_VALUE)
};
static const struct term_field_descriptor operation_request_fields[] = {
	TERM_REF(as.operation_request.operation, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_REQUEST_OPERATION,
		PROTOTYPE_TERM_CHILD_REQUEST_OPERATION),
	TERM_REF(as.operation_request.argument, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_REQUEST_ARGUMENT,
		PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT),
	TERM_REF(as.operation_request.continuation,
		PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_REQUEST_CONTINUATION,
		PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION)
};
static const struct term_field_descriptor computation_fold_fields[] = {
	TERM_REF(as.computation_fold.computation,
		PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_FOLD_COMPUTATION,
		PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION),
	TERM_REF(as.computation_fold.return_clause,
		PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_FOLD_RETURN_CLAUSE,
		PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE),
	TERM_REF(as.computation_fold.first_clause,
		PROTOTYPE_TERM_FIELD_FOLD_CLAUSE_SLICE,
		PROTOTYPE_TERM_FIELD_ROLE_FOLD_FIRST_CLAUSE,
		PROTOTYPE_TERM_CHILD_INVALID),
	TERM_U32(as.computation_fold.clause_count,
		PROTOTYPE_TERM_FIELD_ROLE_FOLD_CLAUSE_COUNT)
};
static const struct term_field_descriptor dimension_action_fields[] = {
	TERM_REF(as.dimension_action.source, PROTOTYPE_TERM_FIELD_TERM_REQUIRED,
		PROTOTYPE_TERM_FIELD_ROLE_DIMENSION_ACTION_SOURCE,
		PROTOTYPE_TERM_CHILD_DIMENSION_ACTION_SOURCE),
	TERM_REF(as.dimension_action.operator_id,
		PROTOTYPE_TERM_FIELD_DIMENSION_OPERATOR,
		PROTOTYPE_TERM_FIELD_ROLE_DIMENSION_ACTION_OPERATOR,
		PROTOTYPE_TERM_CHILD_INVALID)
};

#define TAG(fields_name) \
	{ fields_name, sizeof(fields_name) / sizeof(fields_name[0]), 1 }
#define FIELDLESS { NULL, 0, 1 }

static const struct term_tag_descriptor term_tags[PROTOTYPE_TERM_TAG_MAX + 1] = {
	[PROTOTYPE_TERM_VAR] = TAG(var_fields),
	[PROTOTYPE_TERM_CONSTRUCTOR] = TAG(constructor_fields),
	[PROTOTYPE_TERM_APP] = TAG(app_fields),
	[PROTOTYPE_TERM_LAMBDA] = TAG(lambda_fields),
	[PROTOTYPE_TERM_PI] = TAG(pi_fields),
	[PROTOTYPE_TERM_MATCH] = TAG(match_fields),
	[PROTOTYPE_TERM_TYPE_FORMER] = TAG(type_former_fields),
	[PROTOTYPE_TERM_TYPE_DECLARATION] = TAG(type_declaration_fields),
	[PROTOTYPE_TERM_INDUCTION_HYPOTHESIS] = TAG(induction_hypothesis_fields),
	[PROTOTYPE_TERM_UNIVERSE_VAR] = TAG(universe_var_fields),
	[PROTOTYPE_TERM_PRIMITIVE_TEXT] = FIELDLESS,
	[PROTOTYPE_TERM_TEXT_LITERAL] = TAG(text_literal_fields),
	[PROTOTYPE_TERM_PRIMITIVE_INT] = FIELDLESS,
	[PROTOTYPE_TERM_PRIMITIVE_INT64] = FIELDLESS,
	[PROTOTYPE_TERM_INT_LITERAL] = TAG(int_literal_fields),
	[PROTOTYPE_TERM_EXTERNAL_REF] = TAG(external_ref_fields),
	[PROTOTYPE_TERM_PURE_PRIMITIVE] = TAG(pure_primitive_fields),
	[PROTOTYPE_TERM_EFFECT_OPERATION] = TAG(effect_operation_fields),
	[PROTOTYPE_TERM_TYPE_VIEW] = TAG(type_view_fields),
	[PROTOTYPE_TERM_EFFECT_ROW_EMPTY] = FIELDLESS,
	[PROTOTYPE_TERM_EFFECT_ROW_VAR] = TAG(effect_row_var_fields),
	[PROTOTYPE_TERM_EFFECT_ROW_UNION] = TAG(effect_row_union_fields),
	[PROTOTYPE_TERM_EFFECT_ROW_FORALL] = TAG(effect_row_forall_fields),
	[PROTOTYPE_TERM_COMPUTATION_TYPE] = TAG(computation_type_fields),
	[PROTOTYPE_TERM_THUNK_TYPE] = TAG(thunk_type_fields),
	[PROTOTYPE_TERM_RETURN] = TAG(return_fields),
	[PROTOTYPE_TERM_THUNK] = TAG(thunk_fields),
	[PROTOTYPE_TERM_FORCE] = TAG(force_fields),
	[PROTOTYPE_TERM_OPERATION_REQUEST] = TAG(operation_request_fields),
	[PROTOTYPE_TERM_COMPUTATION_FOLD] = TAG(computation_fold_fields),
	[PROTOTYPE_TERM_EFFECT_ROW_OPERATION] = TAG(effect_row_operation_fields),
	[PROTOTYPE_TERM_RELATION_TYPE_FORMER] = FIELDLESS,
	[PROTOTYPE_TERM_RELATION_WITNESS_FORMER] = FIELDLESS,
	[PROTOTYPE_TERM_DIMENSION_ACTION] = TAG(dimension_action_fields),
	[PROTOTYPE_TERM_TERMINATES_TYPE_FORMER] = FIELDLESS,
	[PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER] = FIELDLESS
};

static const struct term_tag_descriptor* term_tag_descriptor_at(int tag) {
	if (tag < 0 || tag > PROTOTYPE_TERM_TAG_MAX || !term_tags[tag].known) {
		return NULL;
	}
	return &term_tags[tag];
}

int prototype_term_schema_tag_known(int tag) {
	return term_tag_descriptor_at(tag) != NULL;
}

int prototype_term_schema_field_count(int tag, size_t* p_count) {
	const struct term_tag_descriptor* descriptor = term_tag_descriptor_at(tag);
	if (!descriptor || !p_count) return -1;
	*p_count = descriptor->field_count;
	return 0;
}

static int term_field_read(
	const struct prototype_term* term,
	const struct term_field_descriptor* descriptor,
	struct prototype_term_field_value* p_value
) {
	if (!term || !descriptor || !p_value) return -1;
	memset(p_value, 0, sizeof(*p_value));
	p_value->kind = descriptor->kind;
	p_value->role = descriptor->role;
	p_value->child_role = descriptor->child_role;
	const unsigned char* source = (const unsigned char*)term + descriptor->offset;
	if (descriptor->kind == PROTOTYPE_TERM_FIELD_SCALAR_I64) {
		memcpy(&p_value->as.i64, source, sizeof(p_value->as.i64));
	} else if (descriptor->kind == PROTOTYPE_TERM_FIELD_SYMBOL ||
		descriptor->kind == PROTOTYPE_TERM_FIELD_OPERATION ||
		descriptor->kind == PROTOTYPE_TERM_FIELD_SCALAR_I32) {
		memcpy(&p_value->as.i32, source, sizeof(p_value->as.i32));
	} else {
		memcpy(&p_value->as.u32, source, sizeof(p_value->as.u32));
	}
	return 0;
}

int prototype_term_schema_field_at(
	const struct prototype_term* term,
	size_t field_index,
	struct prototype_term_field_value* p_value
) {
	const struct term_tag_descriptor* descriptor = term ?
		term_tag_descriptor_at(term->tag) : NULL;
	if (!descriptor || field_index >= descriptor->field_count) return -1;
	return term_field_read(term, &descriptor->fields[field_index], p_value);
}

int prototype_term_schema_field_write(
	struct prototype_term* term,
	size_t field_index,
	const struct prototype_term_field_value* value
) {
	const struct term_tag_descriptor* tag = term ?
		term_tag_descriptor_at(term->tag) : NULL;
	if (!tag || !value || field_index >= tag->field_count) return -1;
	const struct term_field_descriptor* descriptor = &tag->fields[field_index];
	if (value->kind != descriptor->kind || value->role != descriptor->role ||
		value->child_role != descriptor->child_role) return -1;
	unsigned char* target = (unsigned char*)term + descriptor->offset;
	if (descriptor->kind == PROTOTYPE_TERM_FIELD_SCALAR_I64) {
		memcpy(target, &value->as.i64, sizeof(value->as.i64));
	} else if (descriptor->kind == PROTOTYPE_TERM_FIELD_SYMBOL ||
		descriptor->kind == PROTOTYPE_TERM_FIELD_OPERATION ||
		descriptor->kind == PROTOTYPE_TERM_FIELD_SCALAR_I32) {
		memcpy(target, &value->as.i32, sizeof(value->as.i32));
	} else {
		memcpy(target, &value->as.u32, sizeof(value->as.u32));
	}
	return 0;
}

int prototype_term_schema_fixed_child_count(
	const struct prototype_term* term,
	uint32_t* p_count
) {
	const struct term_tag_descriptor* descriptor = term ?
		term_tag_descriptor_at(term->tag) : NULL;
	if (!descriptor || !p_count) return -1;
	uint32_t count = 0;
	for (size_t i = 0; i < descriptor->field_count; ++i) {
		if (descriptor->fields[i].child_role != PROTOTYPE_TERM_CHILD_INVALID) {
			++count;
		}
	}
	*p_count = count;
	return 0;
}

int prototype_term_schema_fixed_child_at(
	const struct prototype_term* term,
	uint32_t child_index,
	struct prototype_term_child* p_child
) {
	const struct term_tag_descriptor* descriptor = term ?
		term_tag_descriptor_at(term->tag) : NULL;
	if (!descriptor || !p_child) return -1;
	uint32_t current = 0;
	for (size_t i = 0; i < descriptor->field_count; ++i) {
		if (descriptor->fields[i].child_role == PROTOTYPE_TERM_CHILD_INVALID) {
			continue;
		}
		if (current++ == child_index) {
			struct prototype_term_field_value value;
			if (term_field_read(term, &descriptor->fields[i], &value) != 0) {
				return -1;
			}
			p_child->role = descriptor->fields[i].child_role;
			p_child->ordinal = 0;
			p_child->term = value.as.u32;
			return 0;
		}
	}
	return -1;
}
