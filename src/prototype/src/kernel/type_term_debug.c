#include "a_program/kernel/type_term_debug.h"

#include "a_program/kernel/intrinsic.h"

#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/support/symbol.h"

#include <inttypes.h>
#include <stdio.h>

static int type_instance_type_id(
	const struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t term_id,
	uint32_t* p_type_id
) {
	uint32_t argument_count;
	uint32_t arguments[16];
	return prototype_type_projection_instance_info(
		terms, semantic_schema, term_id, p_type_id, arguments, &argument_count
	);
}

static int constructor_owner_type_former(
	const struct prototype_term_db* terms,
	uint32_t owner,
	const struct prototype_term** p_type_former
) {
	if (!terms || !p_type_former || owner >= terms->term_count) {
		return -1;
	}
	uint32_t current = owner;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return -1;
	}
	*p_type_former = &terms->terms[current];
	return 0;
}

static int constructor_owner_representation_id(
	const struct prototype_term_db* terms,
	uint32_t owner,
	uint32_t* p_representation_id
) {
	const struct prototype_term* type_former;
	if (!p_representation_id || constructor_owner_type_former(
			terms, owner, &type_former
		) != 0) {
		return -1;
	}
	*p_representation_id = type_former->as.type_former.representation_id;
	return 0;
}

static const char* safe_symbol_name(const struct symbol_table* symbols, int symbol_id) {
	if (symbol_id == PROTOTYPE_BASE_NAMESPACE_ID) {
		return "_";
	}
	const char* name = symbol_to_string(symbols, symbol_id);
	return name ? name : "<unknown>";
}

static void print_escaped_text(FILE* output, const char* text) {
	if (!output || !text) {
		return;
	}
	for (const char* p = text; *p != '\0'; ++p) {
		switch (*p) {
			case '\n':
				fprintf(output, "\\n");
				break;
			case '\r':
				fprintf(output, "\\r");
				break;
			case '\t':
				fprintf(output, "\\t");
				break;
			case '"':
				fprintf(output, "\\\"");
				break;
			case '\\':
				fprintf(output, "\\\\");
				break;
			default:
				fputc(*p, output);
				break;
		}
	}
}

static void print_term_debug_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_typing_environment* environment,
	uint32_t term_id,
	unsigned depth
);

#define print_term_debug_depth(output, symbols, type_declarations, terms, term_id, depth) \
	print_term_debug_depth_impl( \
		output, symbols, type_declarations, terms, environment, term_id, depth \
	)

static void print_constructor_name(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_typing_environment* environment,
	const struct prototype_term* term
) {
	uint32_t type_id;
	if (type_instance_type_id(
			terms, &type_declarations->semantic_schema,
			term->as.constructor.owner, &type_id
		) != 0 ||
		type_id >= type_declarations->semantic_schema.type_count) {
		uint32_t representation_id;
		if (constructor_owner_representation_id(
				terms,
				term->as.constructor.owner,
				&representation_id
			) != 0) {
			fprintf(output, "<bad-constructor>");
		} else {
			fprintf(output, "rep#%u.ordinal#%u", representation_id,
				term->as.constructor.constructor_id);
		}
		return;
	}

	const struct prototype_type_declaration* type = &type_declarations->semantic_schema.type_declarations[type_id];
	if (term->as.constructor.constructor_id >= type->constructor_count) {
		fprintf(output, "<bad-constructor>");
		return;
	}

	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->semantic_schema.constructor_declarations[type->first_constructor + term->as.constructor.constructor_id];
	print_term_debug_depth(output, symbols, type_declarations, terms, term->as.constructor.owner, 32);
	fprintf(output, ".%s", safe_symbol_name(symbols, constructor->name_symbol_id));
}

static void print_term_debug_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_typing_environment* environment,
	uint32_t term_id,
	unsigned depth
) {
	if (!output || !type_declarations || !terms) {
		return;
	}
	if (term_id >= terms->term_count) {
		fprintf(output, "BAD_TERM(%u)", term_id);
		return;
	}
	if (depth == 0) {
		fprintf(output, "...");
		return;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(output, "VAR(_#%u)", term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			fprintf(output, "EXTERNAL_REF(");
			if (term->as.external_ref.name.namespace_symbol_id >= 0) {
				fprintf(output, "%s.", safe_symbol_name(symbols, term->as.external_ref.name.namespace_symbol_id));
			}
			fprintf(output, "%s)", safe_symbol_name(symbols, term->as.external_ref.name.name_symbol_id));
			break;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			fprintf(output, "PURE_PRIMITIVE(");
			if (environment) {
				fprintf(output, "%s", prototype_intrinsic_namespace_source_name(
					environment,
					PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
					term->as.pure_primitive.primitive_id
				));
			} else {
				fprintf(output, "primitive#%u", term->as.pure_primitive.primitive_id);
			}
			if (term->as.pure_primitive.type_symbol_id >= 0) {
				fprintf(output, ":%s", safe_symbol_name(symbols, term->as.pure_primitive.type_symbol_id));
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			fprintf(output, "EFFECT_OPERATION(");
			if (environment) {
				fprintf(output, "%s", prototype_intrinsic_namespace_source_name(
					environment,
					PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
					term->as.effect_operation.operation_id
				));
			} else {
				fprintf(output, "operation#%u", term->as.effect_operation.operation_id);
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
			fprintf(output, "RELATION_TYPE_FORMER");
			break;
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
			fprintf(output, "RELATION_WITNESS_FORMER");
			break;
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
			fprintf(output, "TERMINATES_TYPE_FORMER");
			break;
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			fprintf(output, "TERMINATES_WITNESS_FORMER");
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			fprintf(output, "DIMENSION_ACTION(op#%u, ",
				term->as.dimension_action.operator_id);
			print_term_debug_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.dimension_action.source,
				depth - 1
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			fprintf(output, "CONSTRUCTOR(");
			print_constructor_name(
				output, symbols, type_declarations, terms, environment, term
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_APP:
			fprintf(output, "APP(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.function, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.argument, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(output,
				"LAMBDA(_#%u, ",
				term->as.lambda.binding_id);
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.lambda.body, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(output, "PI(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.domain, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.codomain_family, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(output, "MATCH(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.match.scrutinee, depth - 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				fprintf(
					output,
					", CASE(%s",
					safe_symbol_name(symbols, terms->case_label_symbols[term->as.match.first_case + i])
				);
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					struct prototype_case_binder binder = terms->case_binders[match_case->first_binder + j];
					fprintf(output, " _#%u", binder.binding_id);
				}
				fprintf(output, " -> ");
				print_term_debug_depth(output, symbols, type_declarations, terms, match_case->body, depth - 1);
				fprintf(output, ")");
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			fprintf(output, "TYPE_FORMER(rep#%u)", term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(output, "TYPE_DECLARATION(");
			fprintf(output, "%s", safe_symbol_name(
				symbols, term->as.type_declaration.identity.name_symbol_id
			));
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(output, "TYPE_VIEW(");
			fprintf(output, "%s", safe_symbol_name(
				symbols, term->as.type_view.identity.name_symbol_id
			));
			fprintf(output, ", core=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.core, depth - 1);
			fprintf(output, ", source=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.source, depth - 1);
			fprintf(output, ")");
			break;
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				fprintf(output, "UNIVERSE(?u%u)", term->as.universe_var.level_var);
				break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				fprintf(output, "PRIMITIVE(Text)");
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(output, "TEXT_LITERAL(\"");
				print_escaped_text(output, safe_symbol_name(symbols, term->as.text_literal.text_symbol_id));
				fprintf(output, "\")");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
				fprintf(output, "PRIMITIVE(Int)");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				fprintf(output, "PRIMITIVE(Int64)");
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(output, "INT_LITERAL(%" PRId64 ")", term->as.int_literal.value);
				break;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
					fprintf(output, "INDUCTION_HYPOTHESIS(ih_scope#%u, ",
						term->as.induction_hypothesis.ih_scope_id);
			print_term_debug_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.induction_hypothesis.argument,
				depth - 1
			);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				fprintf(output, "EFFECT_ROW_EMPTY");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				fprintf(output, "EFFECT_ROW_VAR(%u)", term->as.effect_row_var.binding_id);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				fprintf(output, "EFFECT_ROW_UNION(");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_union.left, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_union.right, depth - 1);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				fprintf(output, "EFFECT_ROW_FORALL(%u, ",
					term->as.effect_row_forall.binding_id);
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_forall.body, depth - 1);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
				fprintf(output, "EFFECT_ROW_OPERATION(%d, ",
					term->as.effect_row_operation.operation_id);
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_operation.latent_row, depth - 1);
				fprintf(output, ")");
				break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			fprintf(output, "COMPUTATION_TYPE(");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.computation_type.label, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.computation_type.result, depth - 1);
			fprintf(
				output,
				", %s)",
				term->as.computation_type.totality ==
					PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ? "TOTAL" : "MAY_DIVERGE"
			);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			fprintf(output, "Thunk(");
			print_term_debug_depth(
				output, symbols, type_declarations, terms, term->as.thunk_type.computation, depth - 1
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_RETURN:
			fprintf(output, "RETURN(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.return_term.value, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_THUNK:
			fprintf(output, "THUNK(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.thunk.computation, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_FORCE:
			fprintf(output, "FORCE(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.force.value, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			fprintf(output, "COMPUTATION_FOLD(");
			print_term_debug_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.computation, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.return_clause, depth - 1);
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				fprintf(output, ", OP_CLAUSE(");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					clause->operation, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					clause->body, depth - 1);
				fprintf(output, ")");
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			fprintf(output, "OPERATION_REQUEST(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.operation, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.argument, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.continuation, depth - 1);
			fprintf(output, ")");
			break;
			default:
				fprintf(output, "UNKNOWN_TERM");
				break;
	}
}

void prototype_type_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	print_term_debug_depth(output, symbols, type_declarations, terms, term_id, 64);
}

#undef print_term_debug_depth
