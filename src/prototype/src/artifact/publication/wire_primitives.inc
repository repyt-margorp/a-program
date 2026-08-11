#include "a_program/artifact/interface.h"
#include "a_program/artifact/wire_v72.h"

#include "a_program/frontend/universe_collection.h"
#include "a_program/graph/operation_graph.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../artifact_graph_internal.h"

static void print_artifact_key(
	FILE* stream,
	const struct prototype_term_canonical_key* key
) {
	fprintf(
		stream,
		"%llu %u %u %u %d %d %d %d",
		(unsigned long long)key->hash,
		key->node_count,
		key->bound_binder_count,
		key->free_binder_count,
		key->has_frame_local_reference,
		key->has_type_local_reference,
		key->has_type_name_reference,
		key->has_type_universe_reference
	);
}

static void print_artifact_type_representation_fingerprint(
	FILE* stream,
	const struct prototype_type_representation_fingerprint* key
) {
	fprintf(
		stream,
		"%llu %u %u %u %u %u %d %d",
		(unsigned long long)key->hash,
		key->node_count,
		key->parameter_count,
		key->constructor_count,
		key->bound_binder_count,
		key->free_binder_count,
		key->has_local_universe_reference,
		key->has_name_reference
	);
}

static int write_artifact_type_expr(
	FILE* stream,
	const struct symbol_table* symbols,
	uint32_t expr_id,
	const struct prototype_type_expr* expr
) {
	if (!stream || !symbols || !expr) {
		return -1;
	}
	fprintf(stream, "type_expr %u %d", expr_id, expr->tag);
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			fprintf(stream, " %u", expr->as.universe.level);
			break;
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			fprintf(stream, " %u", expr->as.universe_var.level_var);
			break;
		case PROTOTYPE_TYPE_EXPR_SELF:
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			fprintf(
				stream,
				" %u %s",
				expr->as.var.binding_id,
				symbol_to_string(symbols, expr->as.var.symbol_id)
			);
			break;
			case PROTOTYPE_TYPE_EXPR_NAME:
				fprintf(stream, " %s", symbol_to_string(symbols, expr->as.name.symbol_id));
				break;
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT:
			case PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64:
				break;
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE:
				fprintf(
					stream,
					" %s %s ",
					expr->as.imported_type.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id)
			);
				print_artifact_type_representation_fingerprint(stream, &expr->as.imported_type.representation_fingerprint);
				break;
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				fprintf(
					stream,
					" %s %s",
					expr->as.external_term.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, expr->as.external_term.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, expr->as.external_term.name.name_symbol_id)
				);
				break;
		case PROTOTYPE_TYPE_EXPR_APP:
			fprintf(stream, " %u %u", expr->as.app.function, expr->as.app.argument);
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			fprintf(stream, " %u %u", expr->as.arrow.domain, expr->as.arrow.codomain);
			break;
		case PROTOTYPE_TYPE_EXPR_PI:
			fprintf(
				stream,
				" %u %s %u %u",
				expr->as.pi.binding_id,
				symbol_to_string(symbols, expr->as.pi.symbol_id),
				expr->as.pi.domain,
				expr->as.pi.codomain
			);
			break;
		default:
			return -1;
	}
	fprintf(stream, "\n");
	return 0;
}

static int write_artifact_term(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	const struct prototype_term* term
) {
	if (!stream || !symbols || !intrinsic_environment || !type_declarations ||
		!terms || !term) {
		return -1;
	}
	fprintf(stream, "term_node %u %d", term_id, term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(stream, " %u", term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			fprintf(
				stream,
				" %u %u",
				term->as.constructor.owner,
				term->as.constructor.constructor_id
			);
			break;
		case PROTOTYPE_TERM_APP:
			fprintf(stream, " %u %u", term->as.app.function, term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(stream, " %u %u", term->as.lambda.binding_id, term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(stream, " %u %u", term->as.pi.domain, term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(
				stream,
				" %u %u %u %u",
				term->as.match.scrutinee,
				term->as.match.first_case,
				term->as.match.case_count,
				term->as.match.ih_scope_id
			);
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			{
				uint32_t representative_type_id;
				if (prototype_type_declaration_representation_type_id(
						type_declarations,
						term->as.type_former.representation_id,
						&representative_type_id
					) != 0) {
					return -1;
				}
				/* Artifacts carry a declaration anchor, not an artifact-local handle. */
				fprintf(stream, " %u", representative_type_id);
			}
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(
				stream,
				" %u %s %s",
				term->as.type_declaration.type_id,
				term->as.type_declaration.identity.namespace_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_declaration.identity.namespace_symbol_id
					) : "-",
				term->as.type_declaration.identity.name_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_declaration.identity.name_symbol_id
					) : "-"
			);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(
				stream,
				" %u %s %s %u %u",
				term->as.type_view.view_type_id,
				term->as.type_view.identity.namespace_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_view.identity.namespace_symbol_id
					) : "-",
				term->as.type_view.identity.name_symbol_id >= 0 ?
					symbol_to_string(
						symbols,
						term->as.type_view.identity.name_symbol_id
					) : "-",
				term->as.type_view.core,
				term->as.type_view.source
			);
			break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			fprintf(
				stream,
				" %u %u",
				term->as.induction_hypothesis.ih_scope_id,
				term->as.induction_hypothesis.argument
			);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			fprintf(stream, " %u", term->as.universe_var.level_var);
			break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(stream, " %s", symbol_to_string(symbols, term->as.text_literal.text_symbol_id));
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(stream, " %" PRId64, term->as.int_literal.value);
				break;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				fprintf(
					stream,
					" %s %s",
					term->as.external_ref.name.namespace_symbol_id >= 0 ?
						symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
					symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id)
				);
				break;
			case PROTOTYPE_TERM_PURE_PRIMITIVE:
				fprintf(
					stream,
					" %s %s",
					prototype_intrinsic_namespace_source_name(
						intrinsic_environment,
						PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
						term->as.pure_primitive.primitive_id
					),
				term->as.pure_primitive.type_symbol_id >= 0 ?
					symbol_to_string(symbols, term->as.pure_primitive.type_symbol_id) :
					"-"
				);
				break;
			case PROTOTYPE_TERM_EFFECT_OPERATION:
				fprintf(
					stream,
					" %s %u",
					prototype_intrinsic_namespace_source_name(
						intrinsic_environment,
						PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
						term->as.effect_operation.operation_id
					),
					term->as.effect_operation.classifier
				);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				fprintf(stream, " %u", term->as.effect_row_var.binding_id);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				fprintf(stream, " %u %u", term->as.effect_row_union.left,
					term->as.effect_row_union.right);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				fprintf(stream, " %u %u", term->as.effect_row_forall.binding_id,
					term->as.effect_row_forall.body);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
				fprintf(
					stream,
					" %s %u",
					prototype_intrinsic_namespace_source_name(
						intrinsic_environment,
						PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
						term->as.effect_row_operation.operation_id
					),
					term->as.effect_row_operation.latent_row
				);
				break;
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				fprintf(
					stream,
					" %u %u",
					term->as.computation_type.label,
					term->as.computation_type.result
				);
				break;
			case PROTOTYPE_TERM_THUNK_TYPE:
				fprintf(stream, " %u", term->as.thunk_type.computation);
				break;
			case PROTOTYPE_TERM_RETURN:
				fprintf(stream, " %u", term->as.return_term.value);
				break;
			case PROTOTYPE_TERM_THUNK:
				fprintf(stream, " %u", term->as.thunk.computation);
				break;
			case PROTOTYPE_TERM_FORCE:
				fprintf(stream, " %u", term->as.force.value);
				break;
			case PROTOTYPE_TERM_COMPUTATION_FOLD:
				fprintf(stream, " %u %u %u", term->as.computation_fold.computation,
					term->as.computation_fold.return_clause, term->as.computation_fold.clause_count);
				for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
					const struct prototype_computation_fold_clause* clause =
						&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
					fprintf(stream, " %u %u", clause->operation, clause->body);
				}
				break;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				fprintf(stream, " %u %u %u", term->as.operation_request.operation,
					term->as.operation_request.argument,
					term->as.operation_request.continuation);
				break;
			default:
				return -1;
	}
	fprintf(stream, "\n");
	return 0;
}
