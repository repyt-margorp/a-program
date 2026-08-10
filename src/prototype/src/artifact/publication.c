#include "a_program/artifact/interface.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artifact_graph_internal.h"

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

static void print_artifact_type_code_shape_key(
	FILE* stream,
	const struct prototype_type_code_shape_key* key
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
				print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
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
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	const struct prototype_term* term
) {
	if (!stream || !symbols || !type_declarations || !terms || !term) {
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
						PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
						term->as.effect_operation.operation_id
					),
					term->as.effect_operation.classifier
				);
				break;
			case PROTOTYPE_TERM_EFFECT_LABEL:
				fprintf(stream, " %u", term->as.effect_label.effects);
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

struct artifact_graph_marks {
	const struct prototype_type_declaration_db* type_declarations;
	const struct prototype_judgement_db* judgement;
	unsigned char* terms;
	unsigned char* cases;
	unsigned char* case_binders;
	unsigned char* frames;
	unsigned char* types;
	unsigned char* parameters;
	unsigned char* constructors;
	unsigned char* readback_field_types;
	unsigned char* type_exprs;
	unsigned char* propositions;
	unsigned char* claims;
	unsigned char* derivations;
	unsigned char* substitutions;
	uint32_t* term_order;
	uint32_t* case_order;
	uint32_t* case_binder_order;
	uint32_t* frame_order;
	uint32_t* type_order;
	uint32_t* parameter_order;
	uint32_t* constructor_order;
	uint32_t* readback_field_type_order;
	uint32_t* type_expr_order;
	uint32_t* proposition_order;
	uint32_t* claim_order;
	uint32_t* derivation_order;
	uint32_t* substitution_order;
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t frame_count;
	size_t type_count;
	size_t parameter_count;
	size_t constructor_count;
	size_t readback_field_type_count;
	size_t type_expr_count;
	size_t proposition_count;
	size_t claim_count;
	size_t derivation_count;
	size_t substitution_count;
	size_t ordered_term_count;
	size_t ordered_case_count;
	size_t ordered_case_binder_count;
	size_t ordered_frame_count;
	size_t ordered_type_count;
	size_t ordered_parameter_count;
	size_t ordered_constructor_count;
	size_t ordered_readback_field_type_count;
	size_t ordered_type_expr_count;
	size_t ordered_proposition_count;
	size_t ordered_claim_count;
	size_t ordered_derivation_count;
	size_t ordered_substitution_count;
};

struct artifact_context_slice {
	unsigned char* reachable;
	uint32_t* relocation;
	size_t source_count;
	size_t context_count;
};

struct artifact_substitution_slice {
	unsigned char* reachable;
	uint32_t* relocation;
	size_t source_count;
	size_t substitution_count;
};

static int artifact_derivation_present(
	const struct prototype_judgement_derivation* derivation
);

static void artifact_context_slice_free(struct artifact_context_slice* slice) {
	if (!slice) {
		return;
	}
	free(slice->reachable);
	free(slice->relocation);
	memset(slice, 0, sizeof(*slice));
}

static void artifact_substitution_slice_free(
	struct artifact_substitution_slice* slice
) {
	if (!slice) {
		return;
	}
	free(slice->reachable);
	free(slice->relocation);
	memset(slice, 0, sizeof(*slice));
}

static int artifact_substitution_slice_mark(
	struct artifact_substitution_slice* slice,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t depth
) {
	if (!slice || !substitutions || depth > 512 ||
		substitution_id >= slice->source_count) {
		return -1;
	}
	if (slice->reachable[substitution_id] == 1) {
		return 0;
	}
	if (slice->reachable[substitution_id] == 2) {
		return -1;
	}
	slice->reachable[substitution_id] = 2;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		if (artifact_substitution_slice_mark(
				slice, substitutions, substitution->first, depth + 1
			) != 0) {
			return -1;
		}
	} else if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		if (artifact_substitution_slice_mark(
				slice, substitutions, substitution->first, depth + 1
			) != 0 || artifact_substitution_slice_mark(
				slice, substitutions, substitution->second, depth + 1
			) != 0) {
			return -1;
		}
	}
	slice->reachable[substitution_id] = 1;
	return 0;
}

static int artifact_substitution_slice_init(
	struct artifact_substitution_slice* slice,
	const struct prototype_judgement_db* judgement,
	const struct prototype_substitution_db* substitutions
) {
	if (!slice || !judgement || !substitutions) {
		return -1;
	}
	memset(slice, 0, sizeof(*slice));
	slice->source_count = substitutions->substitution_count;
	slice->reachable = calloc(
		slice->source_count == 0 ? 1 : slice->source_count,
		sizeof(*slice->reachable)
	);
	slice->relocation = malloc(
		(slice->source_count == 0 ? 1 : slice->source_count) *
			sizeof(*slice->relocation)
	);
	if (!slice->reachable || !slice->relocation) {
		artifact_substitution_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, (uint32_t)i);
		if (!derivation) {
			continue;
		}
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			artifact_substitution_slice_mark(
				slice, substitutions, derivation->semantic_action_id, 0
			) != 0) {
			artifact_substitution_slice_free(slice);
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				artifact_substitution_slice_mark(
					slice,
					substitutions,
					derivation->premises[j].semantic_action_id,
					0
				) != 0) {
				artifact_substitution_slice_free(slice);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		if (slice->reachable[i]) {
			slice->relocation[i] = (uint32_t)slice->substitution_count++;
		}
	}
	return 0;
}

static int artifact_substitution_slice_canonicalize(
	struct artifact_substitution_slice* slice,
	const uint32_t* order,
	size_t order_count
) {
	if (!slice || (order_count > 0 && !order)) {
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	slice->substitution_count = 0;
	for (size_t position = 0; position < order_count; ++position) {
		uint32_t substitution_id = order[position];
		if (substitution_id >= slice->source_count ||
			!slice->reachable[substitution_id] ||
			slice->relocation[substitution_id] != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		slice->relocation[substitution_id] =
			(uint32_t)slice->substitution_count++;
	}
	for (uint32_t substitution_id = 0;
		substitution_id < slice->source_count;
		++substitution_id) {
		if (slice->reachable[substitution_id] &&
			slice->relocation[substitution_id] == PROTOTYPE_INVALID_ID) {
			slice->relocation[substitution_id] =
				(uint32_t)slice->substitution_count++;
		}
	}
	return 0;
}

static int artifact_context_slice_mark(
	struct artifact_context_slice* slice,
	const struct prototype_context_db* contexts,
	uint32_t context_id
) {
	if (!slice || !contexts || context_id >= contexts->context_count) {
		return -1;
	}
	while (context_id != prototype_context_empty(contexts)) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context) {
			return -1;
		}
		slice->reachable[context_id] = 1;
		context_id = context->parent;
	}
	slice->reachable[prototype_context_empty(contexts)] = 1;
	return 0;
}

static int artifact_context_slice_init(
	struct artifact_context_slice* slice,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const struct artifact_substitution_slice* substitution_slice
) {
	if (!slice || !type_declarations || !judgement || !metadata ||
		metadata->contexts.context_count == 0) {
		return -1;
	}
	memset(slice, 0, sizeof(*slice));
	slice->source_count = metadata->contexts.context_count;
	slice->reachable = calloc(slice->source_count, sizeof(*slice->reachable));
	slice->relocation = malloc(
		slice->source_count * sizeof(*slice->relocation)
	);
	if (!slice->reachable || !slice->relocation) {
		artifact_context_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	if (artifact_context_slice_mark(
		slice, &metadata->contexts, prototype_context_empty(&metadata->contexts)
	) != 0) {
		artifact_context_slice_free(slice);
		return -1;
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (type->type_index != PROTOTYPE_INVALID_ID &&
			artifact_context_slice_mark(
				slice, &metadata->contexts, type->parameter_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_context_slice_mark(
				slice, &metadata->contexts, constructor->parameter_context
			) != 0 || artifact_context_slice_mark(
				slice, &metadata->contexts, constructor->field_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, (uint32_t)i);
		if (proposition &&
			artifact_context_slice_mark(
				slice,
				&metadata->contexts,
				proposition->context_id
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[j]
				);
			if (!proposition || artifact_context_slice_mark(
					slice, &metadata->contexts, proposition->context_id
				) != 0) {
				artifact_context_slice_free(slice);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < metadata->operation_count; ++i) {
		if (artifact_context_slice_mark(
			slice, &metadata->contexts, metadata->operations[i].context_id
		) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (size_t i = 0;
		i < metadata->substitutions.substitution_count;
		++i) {
		if (substitution_slice &&
			(i >= substitution_slice->source_count ||
			 !substitution_slice->reachable[i])) {
			continue;
		}
		const struct prototype_substitution* substitution =
			&metadata->substitutions.substitutions[i];
		if (artifact_context_slice_mark(
				slice, &metadata->contexts, substitution->source_context
			) != 0 || artifact_context_slice_mark(
				slice, &metadata->contexts, substitution->target_context
			) != 0) {
			artifact_context_slice_free(slice);
			return -1;
		}
	}
	for (uint32_t i = 0; i < slice->source_count; ++i) {
		if (slice->reachable[i]) {
			slice->relocation[i] = (uint32_t)slice->context_count++;
		}
	}
	return 0;
}

static int artifact_context_slice_assign_path(
	struct artifact_context_slice* slice,
	const struct prototype_context_db* contexts,
	uint32_t context_id
) {
	if (!slice || !contexts || context_id >= slice->source_count ||
		!slice->reachable[context_id]) {
		return -1;
	}
	if (slice->relocation[context_id] != PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t empty = prototype_context_empty(contexts);
	if (context_id != empty) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context || artifact_context_slice_assign_path(
				slice, contexts, context->parent
			) != 0) {
			return -1;
		}
	}
	slice->relocation[context_id] = (uint32_t)slice->context_count++;
	return 0;
}

static int artifact_context_slice_canonicalize(
	struct artifact_context_slice* slice,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const uint32_t* type_order,
	size_t type_order_count,
	const uint32_t* constructor_order,
	size_t constructor_order_count,
	const uint32_t* claim_order,
	size_t claim_order_count,
	const uint32_t* derivation_order,
	size_t derivation_order_count,
	const uint32_t* substitution_order,
	size_t substitution_order_count
) {
	if (!slice || !type_declarations || !judgement || !metadata) {
		return -1;
	}
	for (size_t i = 0; i < slice->source_count; ++i) {
		slice->relocation[i] = PROTOTYPE_INVALID_ID;
	}
	slice->context_count = 0;
	if (artifact_context_slice_assign_path(
			slice,
			&metadata->contexts,
			prototype_context_empty(&metadata->contexts)
		) != 0) {
		return -1;
	}
	for (size_t position = 0; position < type_order_count; ++position) {
		uint32_t type_id = type_order[position];
		if (type_id >= type_declarations->type_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_id];
		if (type->type_index == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_context_slice_assign_path(
				slice, &metadata->contexts, type->parameter_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, type->index_context
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < constructor_order_count; ++position) {
		uint32_t constructor_id = constructor_order[position];
		if (constructor_id >= type_declarations->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		if (artifact_context_slice_assign_path(
				slice, &metadata->contexts, constructor->parameter_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, constructor->field_context
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < claim_order_count; ++position) {
		uint32_t claim_id = claim_order[position];
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (!proposition || artifact_context_slice_assign_path(
				slice, &metadata->contexts, proposition->context_id
			) != 0) {
			return -1;
		}
	}
	for (size_t position = 0; position < derivation_order_count; ++position) {
		uint32_t derivation_id = derivation_order[position];
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, derivation_id);
		if (!derivation) {
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[j]
				);
			if (!proposition || artifact_context_slice_assign_path(
					slice, &metadata->contexts, proposition->context_id
				) != 0) {
				return -1;
			}
		}
	}
	for (size_t position = 0; position < substitution_order_count; ++position) {
		uint32_t substitution_id = substitution_order[position];
		const struct prototype_substitution* substitution =
			prototype_substitution_get(&metadata->substitutions, substitution_id);
		if (!substitution || artifact_context_slice_assign_path(
				slice, &metadata->contexts, substitution->source_context
			) != 0 || artifact_context_slice_assign_path(
				slice, &metadata->contexts, substitution->target_context
			) != 0) {
			return -1;
		}
	}
	for (uint32_t context_id = 0; context_id < slice->source_count; ++context_id) {
		if (slice->reachable[context_id] &&
			artifact_context_slice_assign_path(
				slice, &metadata->contexts, context_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static uint32_t artifact_context_slice_relocate(
	const struct artifact_context_slice* slice,
	uint32_t context_id
) {
	if (!slice || context_id >= slice->source_count ||
		!slice->reachable[context_id]) {
		return PROTOTYPE_INVALID_ID;
	}
	return slice->relocation[context_id];
}

static int artifact_record_publication_order(
	uint32_t* order,
	size_t capacity,
	size_t* count,
	uint32_t id
) {
	if (!order || !count || *count >= capacity || id >= capacity) {
		return -1;
	}
	order[(*count)++] = id;
	return 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
);

static int artifact_mark_substitution(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t depth
) {
	if (!marks || !terms || !substitutions || depth > 512 ||
		substitution_id >= substitutions->substitution_count ||
		substitution_id >= marks->substitution_count) {
		return -1;
	}
	if (marks->substitutions[substitution_id] == 1) {
		return 0;
	}
	if (marks->substitutions[substitution_id] == 2) {
		return -1;
	}
	marks->substitutions[substitution_id] = 2;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution ||
		(substitution->term != PROTOTYPE_INVALID_ID && artifact_mark_term(
			marks, terms, substitution->term, depth + 1
		) != 0) ||
		(substitution->term_classifier != PROTOTYPE_INVALID_ID &&
		 artifact_mark_term(
			marks, terms, substitution->term_classifier, depth + 1
		 ) != 0)) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		if (artifact_mark_substitution(
				marks, terms, substitutions, substitution->first, depth + 1
			) != 0) {
			return -1;
		}
	} else if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		if (artifact_mark_substitution(
				marks, terms, substitutions, substitution->first, depth + 1
			) != 0 || artifact_mark_substitution(
				marks, terms, substitutions, substitution->second, depth + 1
			) != 0) {
			return -1;
		}
	}
	marks->substitutions[substitution_id] = 1;
	return artifact_record_publication_order(
		marks->substitution_order,
		marks->substitution_count,
		&marks->ordered_substitution_count,
		substitution_id
	);
}

static uint32_t artifact_term_publication_rank(
	const struct artifact_graph_marks* marks,
	uint32_t term_id
) {
	if (term_id == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < marks->ordered_term_count; ++i) {
		if (marks->term_order[i] == term_id) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int artifact_substitution_candidate_is_ready(
	const struct prototype_substitution* substitution,
	const uint32_t* ranks,
	size_t rank_count
) {
	if (!substitution || !ranks) {
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
		return substitution->first < rank_count &&
			ranks[substitution->first] != PROTOTYPE_INVALID_ID;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return substitution->first < rank_count &&
			substitution->second < rank_count &&
			ranks[substitution->first] != PROTOTYPE_INVALID_ID &&
			ranks[substitution->second] != PROTOTYPE_INVALID_ID;
	}
	return 1;
}

static int artifact_substitution_candidate_compare(
	const struct artifact_graph_marks* marks,
	const struct prototype_substitution* left,
	uint32_t left_id,
	const struct prototype_substitution* right,
	uint32_t right_id,
	const uint32_t* ranks
) {
#define COMPARE_FIELD(a, b) \
	do { \
		if ((a) != (b)) { \
			return (a) < (b) ? -1 : 1; \
		} \
	} while (0)
	COMPARE_FIELD(left->kind, right->kind);
	COMPARE_FIELD(left->source_context, right->source_context);
	COMPARE_FIELD(left->target_context, right->target_context);
	uint32_t left_first = left->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
		left->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[left->first] : PROTOTYPE_INVALID_ID;
	uint32_t right_first = right->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
		right->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[right->first] : PROTOTYPE_INVALID_ID;
	COMPARE_FIELD(left_first, right_first);
	uint32_t left_second = left->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[left->second] : PROTOTYPE_INVALID_ID;
	uint32_t right_second = right->kind == PROTOTYPE_SUBSTITUTION_COMPOSE ?
		ranks[right->second] : PROTOTYPE_INVALID_ID;
	COMPARE_FIELD(left_second, right_second);
	COMPARE_FIELD(
		artifact_term_publication_rank(marks, left->term),
		artifact_term_publication_rank(marks, right->term)
	);
	COMPARE_FIELD(
		artifact_term_publication_rank(marks, left->term_classifier),
		artifact_term_publication_rank(marks, right->term_classifier)
	);
	COMPARE_FIELD(left_id, right_id);
#undef COMPARE_FIELD
	return 0;
}

static int artifact_canonicalize_substitution_order(
	struct artifact_graph_marks* marks,
	const struct prototype_substitution_db* substitutions
) {
	if (!marks || !substitutions ||
		marks->substitution_count != substitutions->substitution_count) {
		return -1;
	}
	uint32_t* ranks = malloc(
		(marks->substitution_count == 0 ? 1 : marks->substitution_count) *
			sizeof(*ranks)
	);
	uint32_t* order = malloc(
		(marks->ordered_substitution_count == 0 ? 1 :
		 marks->ordered_substitution_count) * sizeof(*order)
	);
	if (!ranks || !order) {
		free(ranks);
		free(order);
		return -1;
	}
	for (size_t i = 0; i < marks->substitution_count; ++i) {
		ranks[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t position = 0;
		position < marks->ordered_substitution_count;
		++position) {
		uint32_t best = PROTOTYPE_INVALID_ID;
		for (uint32_t candidate = 0;
			candidate < marks->substitution_count;
			++candidate) {
			if (!marks->substitutions[candidate] ||
				ranks[candidate] != PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_substitution* substitution =
				prototype_substitution_get(substitutions, candidate);
			if (!artifact_substitution_candidate_is_ready(
					substitution, ranks, marks->substitution_count
				)) {
				continue;
			}
			if (best == PROTOTYPE_INVALID_ID ||
				artifact_substitution_candidate_compare(
					marks,
					substitution,
					candidate,
					prototype_substitution_get(substitutions, best),
					best,
					ranks
				) < 0) {
				best = candidate;
			}
		}
		if (best == PROTOTYPE_INVALID_ID) {
			free(ranks);
			free(order);
			return -1;
		}
		order[position] = best;
		ranks[best] = (uint32_t)position;
	}
	memcpy(
		marks->substitution_order,
		order,
		marks->ordered_substitution_count * sizeof(*order)
	);
	free(ranks);
	free(order);
	return 0;
}

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
);

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
);

static int artifact_mark_type_expr(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t expr_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (expr_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (expr_id >= marks->type_expr_count || expr_id >= type_declarations->expr_count) {
		return -1;
	}
	const struct prototype_type_expr* expr = &type_declarations->exprs[expr_id];
	if (!artifact_type_expr_present(expr)) {
		return -1;
	}
	if (marks->type_exprs[expr_id]) {
		return 0;
	}
	marks->type_exprs[expr_id] = 1;
	if (artifact_record_publication_order(
			marks->type_expr_order,
			marks->type_expr_count,
			&marks->ordered_type_expr_count,
			expr_id
		) != 0) {
		return -1;
	}
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_NAME: {
			const struct prototype_type_declaration* local_type =
				prototype_type_declaration_lookup(
					type_declarations,
					expr->as.name.symbol_id
				);
			if (local_type) {
				return artifact_mark_type(
					marks,
					terms,
					local_type->type_index,
					depth + 1
				);
			}
			return 0;
		}
		case PROTOTYPE_TYPE_EXPR_APP:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.app.function,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.app.argument,
					depth + 1
				) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return artifact_mark_type_expr(
				marks,
				terms,
				expr->as.arrow.domain,
				depth + 1
			) == 0 &&
				artifact_mark_type_expr(
					marks,
					terms,
					expr->as.arrow.codomain,
					depth + 1
				) == 0 ? 0 : -1;
		case PROTOTYPE_TYPE_EXPR_PI:
			return artifact_mark_type_expr(
				marks, terms, expr->as.pi.domain, depth + 1
			) == 0 && artifact_mark_type_expr(
				marks, terms, expr->as.pi.codomain, depth + 1
			) == 0 ? 0 : -1;
		default:
			return 0;
	}
}

static int artifact_mark_parameter(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t parameter_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		parameter_id >= marks->parameter_count ||
		parameter_id >= type_declarations->parameter_count) {
		return -1;
	}
	const struct prototype_type_parameter_declaration* parameter =
		&type_declarations->parameter_declarations[parameter_id];
	if (!artifact_parameter_present(parameter)) {
		return -1;
	}
	if (marks->parameters[parameter_id]) {
		return 0;
	}
	marks->parameters[parameter_id] = 1;
	if (artifact_record_publication_order(
			marks->parameter_order,
			marks->parameter_count,
			&marks->ordered_parameter_count,
			parameter_id
		) != 0) {
		return -1;
	}
	return artifact_mark_type_expr(marks, terms, parameter->type_expr, depth + 1);
}

static int artifact_mark_field_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t field_type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		field_type_id >= marks->readback_field_type_count ||
		field_type_id >= type_declarations->readback_field_type_count) {
		return -1;
	}
	const uint32_t* field_type = &type_declarations->readback_field_types[field_type_id];
	if (!artifact_field_type_present(field_type)) {
		return -1;
	}
	if (marks->readback_field_types[field_type_id]) {
		return 0;
	}
	marks->readback_field_types[field_type_id] = 1;
	if (artifact_record_publication_order(
			marks->readback_field_type_order,
			marks->readback_field_type_count,
			&marks->ordered_readback_field_type_count,
			field_type_id
		) != 0) {
		return -1;
	}
	return artifact_mark_type_expr(marks, terms, *field_type, depth + 1);
}

static int artifact_mark_constructor(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t constructor_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512 ||
		constructor_id >= marks->constructor_count ||
		constructor_id >= type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[constructor_id];
	if (!artifact_constructor_present(constructor)) {
		return -1;
	}
	if (marks->constructors[constructor_id]) {
		return 0;
	}
	marks->constructors[constructor_id] = 1;
	if (artifact_record_publication_order(
			marks->constructor_order,
			marks->constructor_count,
			&marks->ordered_constructor_count,
			constructor_id
		) != 0) {
		return -1;
	}
	if (artifact_mark_type(marks, terms, constructor->owner_type, depth + 1) != 0) {
		return -1;
	}
	if (constructor->readback.result_type != PROTOTYPE_INVALID_ID &&
		artifact_mark_type_expr(
			marks, terms, constructor->readback.result_type, depth + 1
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < constructor->readback.field_count; ++i) {
		uint32_t field_id = constructor->readback.first_field_type + i;
		if (constructor->readback.first_field_type == PROTOTYPE_INVALID_ID ||
			field_id >= type_declarations->readback_field_type_count ||
			!artifact_field_type_present(
				&type_declarations->readback_field_types[field_id]
			)) {
			continue;
		}
		if (artifact_mark_field_type(
				marks,
				terms,
				field_id,
				depth + 1
			) != 0) {
			return -1;
		}
	}
	if ((constructor->result_classifier != PROTOTYPE_INVALID_ID &&
			artifact_mark_term(
				marks, terms, constructor->result_classifier, depth + 1
			) != 0) ||
		(constructor->curried_classifier_cache != PROTOTYPE_INVALID_ID &&
		 artifact_mark_term(
			marks, terms, constructor->curried_classifier_cache, depth + 1
		 ) != 0)) {
		return -1;
	}
	return 0;
}

static int artifact_mark_type(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t type_id,
	uint32_t depth
) {
	const struct prototype_type_declaration_db* type_declarations =
		marks ? marks->type_declarations : NULL;
	if (!marks || !terms || !type_declarations || depth > 512) {
		return -1;
	}
	if (type_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (type_id >= marks->type_count || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (!artifact_type_present(type)) {
		return -1;
	}
	if ((type->origin_kind != PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE &&
		 type->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY) ||
		type->parameter_context == PROTOTYPE_INVALID_ID ||
		type->index_context == PROTOTYPE_INVALID_ID ||
		(type->origin_kind == PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE &&
		 (type->name_symbol_id < 0 || type->origin_source_carrier_term_id !=
			PROTOTYPE_INVALID_ID)) ||
		(type->origin_kind ==
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			 (type->name_symbol_id != -1 || type->namespace_symbol_id != -1 ||
			  type->origin_source_carrier_term_id == PROTOTYPE_INVALID_ID))) {
		fprintf(
			stderr,
			"artifact closure: invalid type metadata type=%u origin=%d name=%d namespace=%d carrier=%u parameter_context=%u index_context=%u\n",
			type_id,
			type->origin_kind,
			type->name_symbol_id,
			type->namespace_symbol_id,
			type->origin_source_carrier_term_id,
			type->parameter_context,
			type->index_context
		);
		return -1;
	}
	if (marks->types[type_id]) {
		return 0;
	}
	/* Mark before following the generated declaration's source carrier. A
	 * carrier TYPE_FORMER can select this declaration as its representation
	 * anchor, so the origin edge is allowed to close a graph cycle. */
	marks->types[type_id] = 1;
	if (artifact_record_publication_order(
			marks->type_order,
			marks->type_count,
			&marks->ordered_type_count,
			type_id
		) != 0) {
		return -1;
	}
	if (type->origin_kind ==
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
		artifact_mark_term(
			marks,
			terms,
			type->origin_source_carrier_term_id,
			depth + 1
		) != 0) {
		fprintf(stderr, "artifact closure: generated type origin failed type=%u carrier=%u\n",
			type_id, type->origin_source_carrier_term_id);
		return -1;
	}
	if (type->formation_classifier == PROTOTYPE_INVALID_ID ||
		artifact_mark_term(
			marks, terms, type->formation_classifier, depth + 1
		) != 0) {
		fprintf(stderr, "artifact closure: type formation failed type=%u classifier=%u\n",
			type_id, type->formation_classifier);
		return -1;
	}
	if (type->first_parameter + type->parameter_count > marks->parameter_count ||
		type->first_constructor + type->constructor_count > marks->constructor_count) {
		fprintf(stderr, "artifact closure: type slice failed type=%u\n", type_id);
		return -1;
	}
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		if (artifact_mark_parameter(marks, terms, type->first_parameter + i, depth + 1) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		if (artifact_mark_constructor(
				marks,
				terms,
				type->first_constructor + i,
				depth + 1
			) != 0) {
			fprintf(stderr, "artifact closure: constructor traversal failed type=%u constructor=%u\n",
				type_id, type->first_constructor + i);
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_case(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t case_id,
	uint32_t depth
) {
	if (!marks || !terms || case_id >= marks->case_count ||
		case_id >= terms->case_count || depth > 512) {
		return -1;
	}
	if (marks->cases[case_id]) {
		return 0;
	}
	marks->cases[case_id] = 1;
	if (artifact_record_publication_order(
			marks->case_order,
			marks->case_count,
			&marks->ordered_case_count,
			case_id
		) != 0) {
		return -1;
	}
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->constructor_owner, depth + 1) != 0) {
		return -1;
	}
	if (match_case->body != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(marks, terms, match_case->body, depth + 1) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		uint32_t binding_id = match_case->first_binder + i;
		if (binding_id >= marks->case_binder_count) {
			return -1;
		}
		if (!marks->case_binders[binding_id]) {
			marks->case_binders[binding_id] = 1;
			if (artifact_record_publication_order(
					marks->case_binder_order,
					marks->case_binder_count,
					&marks->ordered_case_binder_count,
					binding_id
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_mark_frame(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t ih_scope_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		return -1;
	}
	if (ih_scope_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (ih_scope_id >= marks->frame_count || ih_scope_id >= terms->ih_scope_count) {
		return -1;
	}
	if (marks->frames[ih_scope_id]) {
		return 0;
	}
	marks->frames[ih_scope_id] = 1;
	if (artifact_record_publication_order(
			marks->frame_order,
			marks->frame_count,
			&marks->ordered_frame_count,
			ih_scope_id
		) != 0) {
		return -1;
	}
	if (terms->ih_scopes[ih_scope_id].match_term != PROTOTYPE_INVALID_ID &&
		artifact_mark_term(
			marks,
			terms,
			terms->ih_scopes[ih_scope_id].match_term,
			depth + 1
		) != 0) {
		return -1;
	}
	return 0;
}

static int artifact_mark_term(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
) {
	if (!marks || !terms || depth > 512) {
		return -1;
	}
	if (term_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (term_id >= marks->term_count || term_id >= terms->term_count) {
		return -1;
	}
	if (marks->terms[term_id]) {
		return 0;
	}
	marks->terms[term_id] = 1;
	if (artifact_record_publication_order(
			marks->term_order,
			marks->term_count,
			&marks->ordered_term_count,
			term_id
		) != 0) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_mark_term(marks, terms, term->as.constructor.owner, depth + 1);
		case PROTOTYPE_TERM_APP:
			if (artifact_mark_term(
					marks, terms, term->as.app.function, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.app.argument, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: APP traversal failed term=%u function=%u argument=%u\n",
					term_id, term->as.app.function, term->as.app.argument);
				return -1;
			}
			return 0;
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_mark_term(marks, terms, term->as.lambda.body, depth + 1);
		case PROTOTYPE_TERM_PI:
			return artifact_mark_term(marks, terms, term->as.pi.domain, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.pi.codomain_family, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_MATCH:
			if (artifact_mark_term(marks, terms, term->as.match.scrutinee, depth + 1) != 0 ||
				artifact_mark_frame(marks, terms, term->as.match.ih_scope_id, depth + 1) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				if (artifact_mark_case(
						marks,
						terms,
						term->as.match.first_case + i,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			{
				for (uint32_t i = 0; i < marks->type_count; ++i) {
					const struct prototype_type_declaration* marked_type =
						&marks->type_declarations->type_declarations[i];
					if (marks->types[i] && artifact_type_present(marked_type) &&
						marked_type->representation_id ==
							term->as.type_former.representation_id) {
						return 0;
					}
				}
				uint32_t representative_type_id;
				if (prototype_type_declaration_representation_type_id(
						marks->type_declarations,
						term->as.type_former.representation_id,
						&representative_type_id
					) != 0) {
					fprintf(stderr, "artifact closure: TYPE_FORMER representation failed term=%u representation=%u\n",
						term_id, term->as.type_former.representation_id);
					return -1;
				}
				return artifact_mark_type(marks, terms, representative_type_id, depth + 1);
			}
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return artifact_mark_type(marks, terms, term->as.type_declaration.type_id, depth + 1);
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (artifact_mark_type(
					marks, terms, term->as.type_view.view_type_id, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: type view declaration failed term=%u type=%u\n",
					term_id, term->as.type_view.view_type_id);
				return -1;
			}
			if (artifact_mark_term(
					marks, terms, term->as.type_view.core, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.type_view.source, depth + 1
				) != 0) {
				fprintf(stderr, "artifact closure: type view payload failed term=%u core=%u(tag=%d) source=%u(tag=%d)\n",
					term_id,
					term->as.type_view.core,
					term->as.type_view.core < terms->term_count ?
						terms->terms[term->as.type_view.core].tag : -1,
					term->as.type_view.source,
					term->as.type_view.source < terms->term_count ?
						terms->terms[term->as.type_view.source].tag : -1);
				return -1;
			}
			return 0;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_mark_frame(marks, terms, term->as.induction_hypothesis.ih_scope_id, depth + 1) == 0 &&
					artifact_mark_term(marks, terms, term->as.induction_hypothesis.argument, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return artifact_mark_term(
				marks, terms, term->as.effect_operation.classifier, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return artifact_mark_term(marks, terms, term->as.computation_type.label, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.computation_type.result, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return artifact_mark_term(marks, terms, term->as.effect_row_union.left, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.effect_row_union.right, depth + 1) == 0 ? 0 : -1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_mark_term(marks, terms, term->as.effect_row_forall.body, depth + 1);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return artifact_mark_term(
				marks, terms, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_mark_term(marks, terms, term->as.thunk_type.computation, depth + 1);
		case PROTOTYPE_TERM_RETURN:
			return artifact_mark_term(marks, terms, term->as.return_term.value, depth + 1);
		case PROTOTYPE_TERM_THUNK:
			return artifact_mark_term(marks, terms, term->as.thunk.computation, depth + 1);
		case PROTOTYPE_TERM_FORCE:
			return artifact_mark_term(marks, terms, term->as.force.value, depth + 1);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (artifact_mark_term(
					marks, terms, term->as.computation_fold.computation, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, term->as.computation_fold.return_clause, depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (artifact_mark_term(marks, terms, clause->operation, depth + 1) != 0 ||
					artifact_mark_term(marks, terms, clause->body, depth + 1) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return artifact_mark_term(marks, terms, term->as.operation_request.operation, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.argument, depth + 1) == 0 &&
				artifact_mark_term(marks, terms, term->as.operation_request.continuation, depth + 1) == 0 ? 0 : -1;
			default:
				return 0;
	}
}

static int artifact_mark_accepted_claim(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t depth
) {
	if (!marks || !terms || !judgement || claim_id >= judgement->claim_count ||
		depth > 512) {
		return -1;
	}
	if (claim_id >= marks->claim_count) {
		return -1;
	}
	const struct prototype_judgement_proposition* claim_proposition =
		prototype_judgement_claim_proposition(judgement, claim_id);
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim_proposition || !claim || claim->proposition_id >=
			marks->proposition_count) {
		return -1;
	}
	if (marks->claims[claim_id] == 1) {
		return 0;
	}
	if (marks->claims[claim_id] == 2) {
		fprintf(stderr, "artifact closure: cyclic claim dependency claim=%u\n", claim_id);
		return -1;
	}
	marks->claims[claim_id] = 2;
	if (!marks->propositions[claim->proposition_id]) {
		marks->propositions[claim->proposition_id] = 1;
		if (artifact_record_publication_order(
				marks->proposition_order,
				marks->proposition_count,
				&marks->ordered_proposition_count,
				claim->proposition_id
			) != 0) {
			return -1;
		}
	}
	if (artifact_record_publication_order(
			marks->claim_order,
			marks->claim_count,
			&marks->ordered_claim_count,
			claim_id
		) != 0) {
		return -1;
	}
	if (artifact_mark_term(marks, terms, claim_proposition->subject, depth + 1) != 0 ||
		artifact_mark_term(marks, terms, claim_proposition->classifier, depth + 1) != 0 ||
		(claim_proposition->authority_id != PROTOTYPE_INVALID_ID &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT &&
		 claim_proposition->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
		 artifact_mark_term(
			marks, terms, claim_proposition->authority_id, depth + 1
			 ) != 0)) {
		fprintf(
			stderr,
			"artifact closure: claim term traversal failed claim=%u subject=%u(tag=%d) classifier=%u(tag=%d) authority=%d:%u\n",
			claim_id,
			claim_proposition->subject,
			claim_proposition->subject < terms->term_count ?
				terms->terms[claim_proposition->subject].tag : -1,
			claim_proposition->classifier,
			claim_proposition->classifier < terms->term_count ?
				terms->terms[claim_proposition->classifier].tag : -1,
			claim_proposition->authority_kind,
			claim_proposition->authority_id
		);
		return -1;
	}
	int found = 0;
	for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id != claim_id) {
			continue;
		}
		found = 1;
		if (i >= marks->derivation_count) {
			return -1;
		}
		if (!marks->derivations[i]) {
			marks->derivations[i] = 1;
			if (artifact_record_publication_order(
					marks->derivation_order,
					marks->derivation_count,
					&marks->ordered_derivation_count,
					i
				) != 0) {
				return -1;
			}
		}
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			if (artifact_mark_term(
					marks, terms, derivation->rule_data.induction.match, depth + 1
				) != 0 || artifact_mark_term(
					marks, terms, derivation->rule_data.induction.motive, depth + 1
				) != 0) {
				return -1;
			}
		} else if ((derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
			derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) &&
			artifact_mark_term(
				marks, terms, derivation->rule_data.constructor.owner_view, depth + 1
			) != 0) {
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].claim_id != PROTOTYPE_INVALID_ID) {
				uint32_t source_claim_id = derivation->premises[j].claim_id;
				if (source_claim_id >= judgement->claim_count) {
					return -1;
				}
				if (marks->claims[source_claim_id] == 2) {
					fprintf(
						stderr,
						"artifact closure: claim cycle edge claim=%u derivation=%u premise=%u\n",
						claim_id,
						i,
						source_claim_id
					);
					return -1;
				}
				if (artifact_mark_accepted_claim(
						marks, terms, judgement,
						source_claim_id, depth + 1
					) != 0) {
					return -1;
				}
			} else {
				const struct prototype_judgement_proposition* proposition =
					prototype_judgement_premise_proposition(
						judgement, &derivation->premises[j]
					);
				uint32_t proposition_id =
					derivation->premises[j].scoped_proposition_id;
				if (!proposition || proposition_id >= marks->proposition_count) {
					return -1;
				}
				if (!marks->propositions[proposition_id]) {
					marks->propositions[proposition_id] = 1;
					if (artifact_record_publication_order(
							marks->proposition_order,
							marks->proposition_count,
							&marks->ordered_proposition_count,
							proposition_id
						) != 0) {
						return -1;
					}
				}
				if (artifact_mark_term(
						marks, terms, proposition->subject, depth + 1
					) != 0 || artifact_mark_term(
						marks, terms, proposition->classifier, depth + 1
					) != 0 ||
					(proposition->authority_id != PROTOTYPE_INVALID_ID &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT &&
					 proposition->authority_kind !=
						PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
					 artifact_mark_term(
						marks, terms, proposition->authority_id, depth + 1
					 ) != 0)) {
					return -1;
				}
			}
		}
	}
	if (!found) {
		fprintf(stderr, "artifact closure: accepted claim %u has no derivation\n", claim_id);
		return -1;
	}
	marks->claims[claim_id] = 1;
	return 0;
}

static int artifact_mark_exact_relation(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t classifier
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (!claim) {
			continue;
		}
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_proposition_get(judgement, claim->proposition_id);
		if (proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->subject == subject && proposition->classifier == classifier) {
			return artifact_mark_accepted_claim(marks, terms, judgement, i, 0);
		}
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (!claim) {
			continue;
		}
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_proposition_get(judgement, claim->proposition_id);
		int same_projection = 0;
		if (proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->classifier != classifier) {
			continue;
		}
		if (prototype_term_core_shape_equal(
				(struct prototype_term_db*)terms,
				proposition->subject,
				subject,
				&same_projection
			) != 0) {
			continue;
		}
		if (same_projection) {
			return artifact_mark_accepted_claim(marks, terms, judgement, i, 0);
		}
	}
	return 1;
}

static int artifact_export_has_residual_constraint(
	const struct prototype_compile_metadata* metadata,
	uint32_t operation
) {
	if (!metadata || operation >= metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_graph graph = {
		.operations = metadata->operations,
		.operation_count = metadata->operation_count,
		.operation_capacity = metadata->operation_capacity,
		.cases = metadata->operation_cases,
		.case_count = metadata->operation_case_count,
		.case_capacity = metadata->operation_case_capacity,
		.fold_clauses = metadata->operation_fold_clauses,
		.fold_clause_count = metadata->operation_fold_clause_count,
		.fold_clause_capacity = metadata->operation_fold_clause_capacity
	};
	for (size_t i = 0; i < metadata->effect_constraint_count; ++i) {
		const struct prototype_operation_effect_constraint* constraint =
			&metadata->effect_constraints[i];
		if (constraint->state == PROTOTYPE_OPERATION_EFFECT_CONSTRAINT_SOLVED) {
			continue;
		}
		int reaches = prototype_operation_graph_reaches(
			&graph, operation, constraint->operation
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	for (size_t i = 0;
		i < prototype_verification_db_count(&metadata->verification);
		++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(&metadata->verification, (uint32_t)i);
		if (!obligation || obligation->state !=
				PROTOTYPE_VERIFICATION_OBLIGATION_PENDING) {
			continue;
		}
		int reaches = prototype_operation_graph_reaches(
			&graph, operation, obligation->operation
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

static int artifact_mark_subject_relations(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject
) {
	if (!marks || !terms || !judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (claim && prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			)->subject == subject &&
			artifact_mark_accepted_claim(marks, terms, judgement, i, 0) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_claim_present(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* claim
) {
	return prototype_judgement_proposition_get(judgement, claim->proposition_id) != NULL;
}

static int artifact_derivation_present(
	const struct prototype_judgement_derivation* derivation
) {
	return derivation && derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID;
}

static const char* artifact_optional_symbol_name(
	const struct symbol_table* symbols,
	int symbol_id
);

static int write_artifact_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct artifact_context_slice* context_slice
) {
	if (!stream || !symbols || !terms || !type_declarations || !judgement ||
		!context_slice) {
		return -1;
	}
	fprintf(stream, "SECTION graph\n");
	size_t present_term_count = 0;
	size_t present_case_count = 0;
	size_t present_case_binder_count = 0;
	size_t present_frame_count = 0;
	size_t present_type_count = 0;
	size_t present_parameter_count = 0;
	size_t present_constructor_count = 0;
	size_t present_field_type_count = 0;
	size_t present_type_expr_count = 0;
	size_t present_proposition_count = 0;
	size_t present_claim_count = 0;
	size_t present_derivation_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (artifact_term_present(&terms->terms[i])) {
			present_term_count++;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (artifact_case_present(&terms->cases[i])) {
			present_case_count++;
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (artifact_case_binder_present(&terms->case_binders[i])) {
			present_case_binder_count++;
		}
	}
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		if (artifact_frame_present(&terms->ih_scopes[i])) {
			present_frame_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		if (artifact_type_present(&type_declarations->type_declarations[i])) {
			present_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		if (artifact_parameter_present(&type_declarations->parameter_declarations[i])) {
			present_parameter_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		if (artifact_constructor_present(&type_declarations->constructor_declarations[i])) {
			present_constructor_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			present_field_type_count++;
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (artifact_type_expr_present(&type_declarations->exprs[i])) {
			present_type_expr_count++;
		}
	}
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		if (artifact_claim_present(judgement, &judgement->claims[i])) {
			present_claim_count++;
		}
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		if (artifact_candidate_claim_present(&judgement->propositions[i])) {
			present_proposition_count++;
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		if (artifact_derivation_present(&judgement->derivations[i])) {
			present_derivation_count++;
		}
	}
	if (present_term_count != terms->term_count ||
		present_case_count != terms->case_count ||
		present_case_binder_count != terms->case_binder_count ||
		present_frame_count != terms->ih_scope_count ||
		present_type_count != type_declarations->type_count ||
		present_parameter_count != type_declarations->parameter_count ||
		present_constructor_count != type_declarations->constructor_count ||
		present_field_type_count !=
			type_declarations->readback_field_type_count ||
		present_type_expr_count != type_declarations->expr_count ||
		present_proposition_count != judgement->proposition_count ||
		present_claim_count != judgement->claim_count ||
		present_derivation_count != judgement->derivation_count) {
		return -1;
	}
	fprintf(
		stream,
		"counts terms %zu cases %zu case_binders %zu frames %zu types %zu parameters %zu constructors %zu field_types %zu type_exprs %zu propositions %zu claims %zu derivations %zu\n",
		terms->term_count,
		terms->case_count,
		terms->case_binder_count,
		terms->ih_scope_count,
		type_declarations->type_count,
		type_declarations->parameter_count,
		type_declarations->constructor_count,
		type_declarations->readback_field_type_count,
		type_declarations->expr_count,
		judgement->proposition_count,
		judgement->claim_count,
		judgement->derivation_count
	);

	fprintf(stream, "type_declarations %zu\n", present_type_count);
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (!artifact_type_present(type)) {
			continue;
		}
		const char* type_name = artifact_optional_symbol_name(
			symbols, type->name_symbol_id
		);
		const char* namespace_name = artifact_optional_symbol_name(
			symbols, type->namespace_symbol_id
		);
		if (!type_name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"type_decl %zu %s %s %u %u %u %u %u %u %u %u %u %d %u\n",
			i,
			type_name,
			namespace_name,
			type->type_index,
			type->first_parameter,
			type->parameter_count,
			type->first_constructor,
			type->constructor_count,
			type->formation_classifier,
			artifact_context_slice_relocate(
				context_slice, type->parameter_context
			),
			artifact_context_slice_relocate(
				context_slice, type->index_context
			),
			type->index_count,
			type->origin_kind,
			type->origin_source_carrier_term_id
		);
	}

	fprintf(stream, "type_parameters %zu\n", present_parameter_count);
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[i];
		if (!artifact_parameter_present(parameter)) {
			continue;
		}
		fprintf(
			stream,
			"type_param %zu %u %s %u\n",
			i,
			parameter->binding_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}

	fprintf(stream, "type_constructors %zu\n", present_constructor_count);
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[i];
		if (!artifact_constructor_present(constructor)) {
			continue;
		}
		const char* constructor_name = artifact_optional_symbol_name(
			symbols, constructor->name_symbol_id
		);
		if (!constructor_name) {
			return -1;
		}
		fprintf(
			stream,
			"type_constructor %zu %s %u %u %u %u %u %u %u %u %u\n",
			i,
			constructor_name,
				constructor->owner_type,
				constructor->constructor_index,
				constructor->readback.first_field_type,
				constructor->readback.field_count,
				constructor->readback.result_type,
				artifact_context_slice_relocate(
					context_slice, constructor->parameter_context
				),
				artifact_context_slice_relocate(
					context_slice, constructor->field_context
				),
				constructor->result_classifier,
				constructor->curried_classifier_cache
			);
	}

	fprintf(stream, "type_field_refs %zu\n", present_field_type_count);
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (!artifact_field_type_present(&type_declarations->readback_field_types[i])) {
			continue;
		}
		fprintf(stream, "type_field_ref %zu %u\n", i, type_declarations->readback_field_types[i]);
	}

	fprintf(stream, "type_exprs %zu\n", present_type_expr_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (!artifact_type_expr_present(&type_declarations->exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&type_declarations->exprs[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "terms %zu\n", present_term_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (write_artifact_term(
				stream,
				symbols,
				type_declarations,
				terms,
				(uint32_t)i,
				&terms->terms[i]
			) != 0) {
			return -1;
		}
	}

	fprintf(stream, "match_cases %zu\n", present_case_count);
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		if (!artifact_case_present(match_case)) {
			continue;
		}
		fprintf(
			stream,
			"match_case %zu %s %u %u %u %u %u\n",
			i,
			symbol_to_string(symbols, terms->case_label_symbols[i]),
			match_case->constructor_owner,
			match_case->constructor_id,
			match_case->first_binder,
			match_case->binder_count,
			match_case->body
		);
	}

	fprintf(stream, "case_binders %zu\n", present_case_binder_count);
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (!artifact_case_binder_present(&terms->case_binders[i])) {
			continue;
		}
		fprintf(
			stream,
			"case_binder %zu %u %d\n",
			i,
			terms->case_binders[i].binding_id,
			terms->case_binders[i].is_recursive
		);
	}

	fprintf(stream, "match_frames %zu\n", present_frame_count);
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		const struct prototype_ih_scope* frame = &terms->ih_scopes[i];
		if (!artifact_frame_present(frame)) {
			continue;
		}
		fprintf(stream, "match_frame %zu %u %u %d ", i, frame->match_term, frame->key.case_count, frame->key.is_linkable);
		print_artifact_key(stream, &frame->key.match_key);
		fprintf(stream, "\n");
	}

	fprintf(stream, "propositions %zu\n", present_proposition_count);
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			&judgement->propositions[i];
		if (!artifact_candidate_claim_present(proposition)) {
			return -1;
		}
		fprintf(
			stream,
			"proposition %zu %d %d %u %u %u %u %u\n",
			i,
			proposition->kind,
			proposition->authority_kind,
			proposition->authority_id,
			artifact_context_slice_relocate(
				context_slice, proposition->context_id
			),
			proposition->operation_id,
			proposition->subject,
			proposition->classifier
		);
	}

	fprintf(stream, "claims %zu\n", present_claim_count);
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (!artifact_claim_present(judgement, claim)) {
			return -1;
		}
		fprintf(
			stream,
			"claim %zu proposition %u\n",
			i,
			claim->proposition_id
		);
	}
	fprintf(stream, "derivations %zu\n", present_derivation_count);
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		if ((derivation->proof_kind >=
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION &&
			 derivation->proof_kind <=
				PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS &&
			 derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX) ||
			derivation->proof_kind >
				PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION) {
			return -1;
		}
		fprintf(
			stream,
			"derivation %zu %d claim %u premises %u\n",
			i,
			derivation->proof_kind,
			derivation->conclusion_claim_id,
			derivation->premise_count
		);
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
			fprintf(
				stream,
				"payload constructor %u %u %u\n",
				derivation->rule_data.constructor.owner_view,
				derivation->rule_data.constructor.constructor_index,
				derivation->rule_data.constructor.field_index
			);
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
			fprintf(
				stream,
				"payload constructor_spine %u\n",
				derivation->rule_data.constructor.owner_view
			);
		} else if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			fprintf(
				stream,
				"payload induction %u %u %u %u\n",
				derivation->rule_data.induction.match,
				derivation->rule_data.induction.motive,
				derivation->rule_data.induction.case_index,
				derivation->rule_data.induction.field_index
			);
		} else {
			fprintf(stream, "payload none\n");
		}
		fprintf(
			stream,
			"action %d %u\n",
			derivation->semantic_action_kind,
			derivation->semantic_action_id
		);
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			const struct prototype_judgement_premise_edge* premise =
				&derivation->premises[j];
			if ((premise->claim_id == PROTOTYPE_INVALID_ID) ==
				(premise->scoped_proposition_id == PROTOTYPE_INVALID_ID)) {
				return -1;
			}
			fprintf(
				stream,
				"premise %s %u action %d %u\n",
				premise->claim_id != PROTOTYPE_INVALID_ID ? "claim" : "scoped",
				premise->claim_id != PROTOTYPE_INVALID_ID ?
					premise->claim_id : premise->scoped_proposition_id,
				premise->semantic_action_kind,
				premise->semantic_action_id
			);
		}
	}
	fprintf(stream, "END graph\n");
	return 0;
}

static int artifact_term_reaches_term_at_depth(
	const struct prototype_term_db* terms,
	uint32_t root,
	uint32_t needle,
	uint32_t depth
) {
	if (!terms || root >= terms->term_count || needle >= terms->term_count ||
		depth > 256) {
		return -1;
	}
	if (root == needle) {
		return 1;
	}
	const struct prototype_term* term = &terms->terms[root];
	switch (term->tag) {
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.constructor.owner,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_APP: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.function,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.app.argument,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA:
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.lambda.body,
				needle,
				depth + 1
			);
		case PROTOTYPE_TERM_PI: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.domain,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.pi.codomain_family,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			int found = artifact_term_reaches_term_at_depth(
				terms,
				term->as.match.scrutinee,
				needle,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count) {
					return -1;
				}
				found = artifact_term_reaches_term_at_depth(
					terms,
					terms->cases[case_id].body,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW:
			{
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.core,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.type_view.source,
					needle,
					depth + 1
				);
			}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return artifact_term_reaches_term_at_depth(
					terms,
					term->as.induction_hypothesis.argument,
					needle,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
				int found = artifact_term_reaches_term_at_depth(
					terms,
					term->as.computation_type.label,
					needle,
					depth + 1
				);
				if (found != 0) {
					return found;
				}
			return artifact_term_reaches_term_at_depth(
				terms,
				term->as.computation_type.result,
				needle,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.left, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_union.right, needle, depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_forall.body, needle, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.effect_row_operation.latent_row, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk_type.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.return_term.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.thunk.computation, needle, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return artifact_term_reaches_term_at_depth(
				terms, term->as.force.value, needle, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.computation_fold.computation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.computation_fold.return_clause, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				found = artifact_term_reaches_term_at_depth(
					terms, clause->operation, needle, depth + 1
				);
				if (found != 0) {
					return found;
				}
				found = artifact_term_reaches_term_at_depth(
					terms, clause->body, needle, depth + 1
				);
				if (found != 0) {
					return found;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			int found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.operation, needle, depth + 1
			);
			if (found != 0) {
				return found;
			}
			found = artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.argument, needle, depth + 1
			);
			return found != 0 ? found : artifact_term_reaches_term_at_depth(
				terms, term->as.operation_request.continuation, needle, depth + 1
			);
		}
			default:
				return 0;
	}
}

static int artifact_external_term_ref_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t term_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		term_id >= terms->term_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		int found = 0;
		if (export->local_term < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->local_term, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
		if (export->classifier < terms->term_count) {
			found = artifact_term_reaches_term_at_depth(terms, export->classifier, term_id, 0);
			if (found < 0) {
				return -1;
			}
			if (found) {
				*p_reachable = 1;
				return 0;
			}
		}
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		int found = artifact_term_reaches_term_at_depth(terms, relation->subject, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
		found = artifact_term_reaches_term_at_depth(terms, relation->classifier, term_id, 0);
		if (found < 0) {
			return -1;
		}
		if (found) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_term_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (!artifact_interface_exports_term_name(interface, terms->terms[i].as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (type_declarations->exprs[i].tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(
				type_declarations,
				type_declarations->exprs[i].as.name.symbol_id
			)) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_count_resolved_external_type_expr_refs(
	const struct prototype_type_declaration_db* type_declarations,
	size_t* p_count
) {
	if (!type_declarations || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (type_declarations->exprs[i].tag == PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			(*p_count)++;
		}
	}
	return 0;
}

static int artifact_match_case_is_reachable(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t case_id,
	int* p_reachable
) {
	if (!interface || !terms || !judgement || !p_reachable ||
		case_id >= terms->case_count) {
		return -1;
	}
	*p_reachable = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_MATCH ||
			case_id < term->as.match.first_case ||
			case_id >= term->as.match.first_case + term->as.match.case_count) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (reachable) {
			*p_reachable = 1;
			return 0;
		}
	}
	return 0;
}

static int artifact_count_resolved_constructor_owner_refs(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	size_t* p_count
) {
	if (!interface || !terms || !judgement || !p_count) {
		return -1;
	}
	*p_count = 0;
	for (size_t i = 0; i < terms->term_count; ++i) {
		int reachable = 0;
		if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			terms->terms[i].as.constructor.owner != PROTOTYPE_INVALID_ID) {
			if (artifact_external_term_ref_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		int reachable = 0;
		if (terms->cases[i].constructor_owner != PROTOTYPE_INVALID_ID) {
			if (artifact_match_case_is_reachable(
					interface,
					terms,
					judgement,
					(uint32_t)i,
					&reachable
				) != 0) {
				return -1;
			}
			if (reachable) {
				(*p_count)++;
			}
		}
	}
	return 0;
}

static int write_artifact_relocation_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t external_term_ref_count;
	size_t resolved_external_term_ref_count;
	size_t external_type_expr_ref_count;
	size_t resolved_external_type_expr_ref_count;
	size_t resolved_constructor_owner_ref_count;
	if (artifact_count_external_term_refs(
			interface,
			terms,
			judgement,
			&external_term_ref_count
		) != 0 ||
		artifact_count_resolved_external_term_refs(
			interface,
			terms,
			judgement,
			&resolved_external_term_ref_count
		) != 0 ||
		artifact_count_external_type_expr_refs(type_declarations, &external_type_expr_ref_count) != 0 ||
		artifact_count_resolved_external_type_expr_refs(
			type_declarations,
			&resolved_external_type_expr_ref_count
		) != 0 ||
		artifact_count_resolved_constructor_owner_refs(
			interface,
			terms,
			judgement,
			&resolved_constructor_owner_ref_count
		) != 0) {
		return -1;
	}

	fprintf(stream, "SECTION relocation\n");
	fprintf(stream, "external_term_refs %zu\n", external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (artifact_interface_exports_term_name(interface, term->as.external_ref.name)) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"external_term_ref %zu %s %s\n",
			i,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "resolved_external_term_refs %zu\n", resolved_external_term_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		int reachable = 0;
		uint32_t export_id;
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		if (prototype_artifact_interface_find_term_export_in_namespace(
					interface,
					term->as.external_ref.name.namespace_symbol_id,
					term->as.external_ref.name.name_symbol_id,
				&export_id
			) != 0) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		const char* name = symbol_to_string(symbols, term->as.external_ref.name.name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_external_term_ref %zu %u %s %s\n",
			i,
			export_id,
			term->as.external_ref.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, term->as.external_ref.name.namespace_symbol_id) : "-",
			name
		);
	}

	fprintf(stream, "external_type_expr_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_expr_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}

	fprintf(stream, "resolved_external_type_expr_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_expr_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
		fprintf(stream, "\n");
	}

	fprintf(stream, "external_type_former_refs %zu\n", external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME ||
			prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id)) {
			continue;
		}
		const char* name = symbol_to_string(symbols, expr->as.name.symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(stream, "external_type_former_ref %zu %d %s\n", i, expr->as.name.symbol_id, name);
	}
	fprintf(stream, "resolved_external_type_former_refs %zu\n", resolved_external_type_expr_ref_count);
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		uint32_t export_id = PROTOTYPE_INVALID_ID;
		if (expr->tag != PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE) {
			continue;
		}
			const char* name = symbol_to_string(symbols, expr->as.imported_type.name.name_symbol_id);
		if (!name) {
			return -1;
		}
			(void)prototype_artifact_interface_find_type_export_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id,
			&export_id
		);
		fprintf(
			stream,
			"resolved_external_type_former_ref %zu %u %s %s ",
			i,
			export_id,
			expr->as.imported_type.name.namespace_symbol_id >= 0 ?
				symbol_to_string(symbols, expr->as.imported_type.name.namespace_symbol_id) : "-",
			name
		);
		print_artifact_type_code_shape_key(stream, &expr->as.imported_type.code_shape_key);
		fprintf(stream, "\n");
	}
	fprintf(stream, "resolved_constructor_owner_refs %zu\n", resolved_constructor_owner_ref_count);
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR ||
			term->as.constructor.owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_external_term_ref_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				term->as.constructor.owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 1 %zu %u %u ",
			i,
			term->as.constructor.owner,
			term->as.constructor.constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[i];
		struct prototype_term_canonical_key owner_key;
		int reachable = 0;
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (artifact_match_case_is_reachable(
				interface,
				terms,
				judgement,
				(uint32_t)i,
				&reachable
			) != 0) {
			return -1;
		}
		if (!reachable) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				match_case->constructor_owner,
				&owner_key
			) != 0) {
			return -1;
		}
		fprintf(
			stream,
			"resolved_constructor_owner_ref 2 %zu %u %u ",
			i,
			match_case->constructor_owner,
			match_case->constructor_id
		);
		print_artifact_key(stream, &owner_key);
		fprintf(stream, "\n");
	}
	fprintf(stream, "external_constructor_owner_refs 0\n");
	fprintf(stream, "END relocation\n");
	return 0;
}

static int write_artifact_universe_section(
	FILE* stream,
	const struct prototype_universe_db* universe
) {
	if (!stream || !universe) {
		return -1;
	}
	fprintf(stream, "SECTION universe\n");
	size_t present_node_count = 0;
	size_t present_edge_count = 0;
	for (size_t i = 0; i < universe->node_count; ++i) {
		if (artifact_universe_node_present(&universe->nodes[i])) {
			present_node_count++;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		if (artifact_universe_edge_present(&universe->edges[i])) {
			present_edge_count++;
		}
	}
	fprintf(
		stream,
		"counts node_slots %zu nodes %zu edge_slots %zu edges %zu levels %zu constraints %zu solved %d\n",
		universe->node_count,
		present_node_count,
		universe->edge_count,
		present_edge_count,
		universe->level_count,
		universe->constraint_count,
		universe->solved
	);
	fprintf(stream, "universe_nodes %zu\n", present_node_count);
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (!artifact_universe_node_present(node)) {
			continue;
		}
		fprintf(
			stream,
			"universe_node %zu %d %u %u %d %u\n",
			i,
			node->tag,
			node->type_id,
			node->parameter_id,
			node->symbol_id,
			node->type_expr
		);
	}
	fprintf(stream, "universe_edges %zu\n", present_edge_count);
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (!artifact_universe_edge_present(edge)) {
			continue;
		}
		fprintf(
			stream,
			"universe_edge %zu %d %u %u\n",
			i,
			edge->tag,
			edge->from_node,
			edge->to_node
		);
	}
	fprintf(stream, "universe_levels %zu\n", universe->level_count);
	for (size_t i = 0; i < universe->level_count; ++i) {
		const struct prototype_universe_level* level = &universe->levels[i];
		fprintf(
			stream,
			"universe_level %zu %u %d\n",
			i,
			level->level_var,
			level->value
		);
	}
	fprintf(stream, "universe_constraints %zu\n", universe->constraint_count);
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		fprintf(
			stream,
			"universe_constraint %zu %u %u %d %u %u %d %u %d %u %u %u\n",
			i,
			constraint->lower_level_var,
			constraint->upper_level_var,
			constraint->offset,
			constraint->subject,
			constraint->classifier,
			constraint->reason,
			constraint->source_claim_id,
			constraint->source_authority_kind,
			constraint->source_authority_id,
			constraint->source_subject,
			constraint->source_classifier
		);
	}
	fprintf(stream, "END universe\n");
	return 0;
}

static int write_artifact_debug_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_ast_db* asts
) {
	if (!stream || !symbols || !interface) {
		return -1;
	}
	fprintf(stream, "SECTION debug\n");
	fprintf(stream, "term_names %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		uint32_t source_entry_id = PROTOTYPE_INVALID_ID;
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->assignment_count; ++j) {
				const struct prototype_ast_term_assignment_def* assignment =
					&asts->assignments[j];
				if (assignment->name_symbol_id != export->name_symbol_id ||
					assignment->compiled_term != export->local_term) {
					continue;
				}
				source_entry_id = assignment->source_entry_id;
				name_span = assignment->name_span;
				body_span = assignment->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"term_name %s %u %u %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_term,
			export->classifier,
			source_entry_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "type_names %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		struct prototype_source_span name_span;
		struct prototype_source_span body_span;
		memset(&name_span, 0, sizeof(name_span));
		memset(&body_span, 0, sizeof(body_span));
		if (asts) {
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != export->local_type_id) {
					continue;
				}
				name_span = type->name_span;
				body_span = type->body_span;
				break;
			}
		}
		fprintf(
			stream,
			"type_name %s %u %u %u %u %u\n",
			symbol_to_string(symbols, export->name_symbol_id),
			export->local_type_id,
			name_span.line,
			name_span.column,
			body_span.line,
			body_span.column
		);
	}
	fprintf(stream, "constructor_names %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		struct prototype_source_span name_span;
		memset(&name_span, 0, sizeof(name_span));
		if (asts && export->type_export_index < interface->type_export_count) {
			const struct prototype_artifact_type_export* type_export =
				&interface->type_exports[export->type_export_index];
			for (uint32_t j = 0; j < (uint32_t)asts->type_def_count; ++j) {
				const struct prototype_ast_type_def* type = &asts->type_defs[j];
				if (!type->compiled || type->compiled_type != type_export->local_type_id ||
					export->ordinal >= type->constructor_count) {
					continue;
				}
				uint32_t constructor_id = type->first_constructor + export->ordinal;
				if (constructor_id >= asts->type_constructor_count) {
					continue;
				}
				name_span = asts->type_constructors[constructor_id].name_span;
				break;
			}
		}
		fprintf(
			stream,
			"constructor_name %u %s %u %u %u\n",
			export->type_export_index,
			symbol_to_string(symbols, export->name_symbol_id),
			export->ordinal,
			name_span.line,
			name_span.column
		);
	}
	fprintf(stream, "END debug\n");
	return 0;
}

/* This graph retains source IDs only while computing reachability. It is not
 * a serializable artifact graph. Publication must relocate it into dense,
 * independently numbered arenas before writing. */
struct artifact_closure_graph {
	struct prototype_term_db terms;
	struct prototype_type_declaration_db type_declarations;
	struct prototype_judgement_db judgement;
	struct prototype_universe_db universe;
	struct prototype_term* term_nodes;
	struct prototype_match_case* cases;
	int* case_label_symbols;
	struct prototype_case_binder* case_binders;
	struct prototype_ih_scope* frames;
	struct prototype_type_declaration* type_nodes;
	struct prototype_type_parameter_declaration* parameter_declarations;
	struct prototype_type_constructor_declaration* constructor_declarations;
	uint32_t* readback_field_types;
	struct prototype_type_expr* type_exprs;
	struct prototype_judgement_proposition* propositions;
	struct prototype_judgement_derivation_candidate* derivation_candidates;
	struct prototype_judgement_claim* claims;
	struct prototype_judgement_derivation* derivations;
	struct prototype_judgement_candidate_premise* candidate_premises;
	struct prototype_judgement_premise_edge* accepted_premises;
	struct prototype_universe_node* universe_nodes;
	struct prototype_universe_edge* universe_edges;
	struct prototype_universe_level* universe_levels;
	struct prototype_universe_constraint* universe_constraints;
	uint32_t* term_order;
	uint32_t* case_order;
	uint32_t* case_binder_order;
	uint32_t* frame_order;
	uint32_t* type_order;
	uint32_t* parameter_order;
	uint32_t* constructor_order;
	uint32_t* readback_field_type_order;
	uint32_t* type_expr_order;
	uint32_t* proposition_order;
	uint32_t* claim_order;
	uint32_t* derivation_order;
	uint32_t* substitution_order;
	size_t ordered_term_count;
	size_t ordered_case_count;
	size_t ordered_case_binder_count;
	size_t ordered_frame_count;
	size_t ordered_type_count;
	size_t ordered_parameter_count;
	size_t ordered_constructor_count;
	size_t ordered_readback_field_type_count;
	size_t ordered_type_expr_count;
	size_t ordered_proposition_count;
	size_t ordered_claim_count;
	size_t ordered_derivation_count;
	size_t ordered_substitution_count;
};

struct artifact_publication_graph {
	struct artifact_closure_graph object;
	struct prototype_artifact_interface interface;
	struct prototype_compile_metadata metadata;
	struct prototype_artifact_term_export* term_exports;
	struct prototype_artifact_type_export* type_exports;
	struct prototype_artifact_type_parameter_export* type_parameters;
	struct prototype_artifact_constructor_export* constructor_exports;
	uint32_t* constructor_field_type_exprs;
	struct prototype_type_expr* interface_type_exprs;
	struct prototype_artifact_identity_root* identity_roots;
	struct prototype_artifact_dependency* dependencies;
	struct prototype_context* contexts;
	struct prototype_substitution* substitutions;
	struct prototype_operation_node* operations;
	struct prototype_operation_match_case* operation_cases;
	struct prototype_operation_computation_fold_clause* operation_fold_clauses;
	struct prototype_operation_effect_constraint* effect_constraints;
	struct prototype_verification_obligation* verification_obligations;
	uint32_t* term_relocation;
	uint32_t* binding_relocation;
	uint32_t* context_relocation;
	uint32_t* type_relocation;
	uint32_t* type_expr_relocation;
	uint32_t* parameter_relocation;
	uint32_t* constructor_relocation;
	uint32_t* field_relocation;
	uint32_t* proposition_relocation;
	uint32_t* claim_relocation;
	uint32_t* substitution_relocation;
};

static void artifact_closure_graph_free(struct artifact_closure_graph* graph) {
	if (!graph) {
		return;
	}
	free(graph->term_nodes);
	free(graph->cases);
	free(graph->case_label_symbols);
	free(graph->case_binders);
	free(graph->frames);
	free(graph->type_nodes);
	free(graph->parameter_declarations);
	free(graph->constructor_declarations);
	free(graph->readback_field_types);
	free(graph->type_exprs);
	free(graph->type_declarations.representations);
	free(graph->claims);
	free(graph->derivations);
	free(graph->propositions);
	free(graph->derivation_candidates);
	free(graph->candidate_premises);
	free(graph->accepted_premises);
	free(graph->universe_nodes);
	free(graph->universe_edges);
	free(graph->universe_levels);
	free(graph->universe_constraints);
	free(graph->term_order);
	free(graph->case_order);
	free(graph->case_binder_order);
	free(graph->frame_order);
	free(graph->type_order);
	free(graph->parameter_order);
	free(graph->constructor_order);
	free(graph->readback_field_type_order);
	free(graph->type_expr_order);
	free(graph->proposition_order);
	free(graph->claim_order);
	free(graph->derivation_order);
	free(graph->substitution_order);
	memset(graph, 0, sizeof(*graph));
}

static void artifact_closure_graph_take_publication_order(
	struct artifact_closure_graph* graph,
	struct artifact_graph_marks* marks
) {
	if (!graph || !marks) {
		return;
	}
#define TAKE_ORDER(field, count_field) \
	do { \
		graph->field = marks->field; \
		graph->count_field = marks->count_field; \
		marks->field = NULL; \
		marks->count_field = 0; \
	} while (0)
	TAKE_ORDER(term_order, ordered_term_count);
	TAKE_ORDER(case_order, ordered_case_count);
	TAKE_ORDER(case_binder_order, ordered_case_binder_count);
	TAKE_ORDER(frame_order, ordered_frame_count);
	TAKE_ORDER(type_order, ordered_type_count);
	TAKE_ORDER(parameter_order, ordered_parameter_count);
	TAKE_ORDER(constructor_order, ordered_constructor_count);
	TAKE_ORDER(readback_field_type_order, ordered_readback_field_type_count);
	TAKE_ORDER(type_expr_order, ordered_type_expr_count);
	TAKE_ORDER(proposition_order, ordered_proposition_count);
	TAKE_ORDER(claim_order, ordered_claim_count);
	TAKE_ORDER(derivation_order, ordered_derivation_count);
	TAKE_ORDER(substitution_order, ordered_substitution_count);
#undef TAKE_ORDER
}

static void artifact_publication_graph_free(
	struct artifact_publication_graph* graph
) {
	if (!graph) {
		return;
	}
	artifact_closure_graph_free(&graph->object);
	free(graph->term_exports);
	free(graph->type_exports);
	free(graph->type_parameters);
	free(graph->constructor_exports);
	free(graph->constructor_field_type_exprs);
	free(graph->interface_type_exprs);
	free(graph->identity_roots);
	free(graph->dependencies);
	free(graph->contexts);
	free(graph->substitutions);
	free(graph->operations);
	free(graph->operation_cases);
	free(graph->operation_fold_clauses);
	free(graph->effect_constraints);
	free(graph->verification_obligations);
	free(graph->term_relocation);
	free(graph->binding_relocation);
	free(graph->context_relocation);
	free(graph->type_relocation);
	free(graph->type_expr_relocation);
	free(graph->parameter_relocation);
	free(graph->constructor_relocation);
	free(graph->field_relocation);
	free(graph->proposition_relocation);
	free(graph->claim_relocation);
	free(graph->substitution_relocation);
	memset(graph, 0, sizeof(*graph));
}

static int artifact_alloc_bytes(void** p_ret, size_t count, size_t size) {
	if (!p_ret || size == 0) {
		return -1;
	}
	*p_ret = NULL;
	if (count == 0) {
		return 0;
	}
	*p_ret = calloc(count, size);
	return *p_ret ? 0 : -1;
}

static void artifact_init_closure_defaults(struct artifact_closure_graph* graph) {
	for (size_t i = 0; i < graph->judgement.claim_count; ++i) {
		graph->judgement.claims[i].proposition_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.case_count; ++i) {
		graph->terms.cases[i].constructor_owner = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].first_binder = PROTOTYPE_INVALID_ID;
		graph->terms.cases[i].body = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.case_binder_count; ++i) {
		graph->terms.case_binders[i].binding_id = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->terms.ih_scope_count; ++i) {
		graph->terms.ih_scopes[i].match_term = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.type_count; ++i) {
		graph->type_declarations.type_declarations[i].name_symbol_id = -1;
		graph->type_declarations.type_declarations[i].type_index = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].representation_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].parameter_context = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].index_context = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].first_parameter = PROTOTYPE_INVALID_ID;
		graph->type_declarations.type_declarations[i].first_constructor = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.parameter_count; ++i) {
		graph->type_declarations.parameter_declarations[i].binding_id = PROTOTYPE_INVALID_ID;
		graph->type_declarations.parameter_declarations[i].name_symbol_id = -1;
		graph->type_declarations.parameter_declarations[i].type_expr = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.constructor_count; ++i) {
		graph->type_declarations.constructor_declarations[i].name_symbol_id = -1;
		graph->type_declarations.constructor_declarations[i].owner_type = PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].readback.first_field_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].readback.result_type =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].parameter_context =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].field_context =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].result_classifier =
			PROTOTYPE_INVALID_ID;
		graph->type_declarations.constructor_declarations[i].curried_classifier_cache =
			PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < graph->type_declarations.readback_field_type_count; ++i) {
		graph->type_declarations.readback_field_types[i] = PROTOTYPE_INVALID_ID;
	}
}

static void artifact_marks_free(struct artifact_graph_marks* marks) {
	if (!marks) {
		return;
	}
	free(marks->terms);
	free(marks->cases);
	free(marks->case_binders);
	free(marks->frames);
	free(marks->types);
	free(marks->parameters);
	free(marks->constructors);
	free(marks->readback_field_types);
	free(marks->type_exprs);
	free(marks->propositions);
	free(marks->claims);
	free(marks->derivations);
	free(marks->substitutions);
	free(marks->term_order);
	free(marks->case_order);
	free(marks->case_binder_order);
	free(marks->frame_order);
	free(marks->type_order);
	free(marks->parameter_order);
	free(marks->constructor_order);
	free(marks->readback_field_type_order);
	free(marks->type_expr_order);
	free(marks->proposition_order);
	free(marks->claim_order);
	free(marks->derivation_order);
	free(marks->substitution_order);
	memset(marks, 0, sizeof(*marks));
}

static int artifact_marks_init(
	struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata
) {
	if (!marks || !terms || !type_declarations || !judgement) {
		return -1;
	}
	memset(marks, 0, sizeof(*marks));
	marks->type_declarations = type_declarations;
	marks->judgement = judgement;
	marks->term_count = terms->term_count;
	marks->case_count = terms->case_count;
	marks->case_binder_count = terms->case_binder_count;
	marks->frame_count = terms->ih_scope_count;
	marks->type_count = type_declarations->type_count;
	marks->parameter_count = type_declarations->parameter_count;
	marks->constructor_count = type_declarations->constructor_count;
	marks->readback_field_type_count = type_declarations->readback_field_type_count;
	marks->type_expr_count = type_declarations->expr_count;
	marks->proposition_count = judgement->proposition_count;
	marks->claim_count = judgement->claim_count;
	marks->derivation_count = judgement->derivation_count;
	marks->substitution_count = metadata ?
		metadata->substitutions.substitution_count : 0;
	if (artifact_alloc_bytes((void**)&marks->terms, marks->term_count, sizeof(*marks->terms)) != 0 ||
		artifact_alloc_bytes((void**)&marks->cases, marks->case_count, sizeof(*marks->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->case_binders,
			marks->case_binder_count,
			sizeof(*marks->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&marks->frames, marks->frame_count, sizeof(*marks->frames)) != 0 ||
		artifact_alloc_bytes((void**)&marks->types, marks->type_count, sizeof(*marks->types)) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->parameters,
			marks->parameter_count,
			sizeof(*marks->parameters)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->constructors,
			marks->constructor_count,
			sizeof(*marks->constructors)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->readback_field_types,
			marks->readback_field_type_count,
			sizeof(*marks->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->type_exprs,
			marks->type_expr_count,
			sizeof(*marks->type_exprs)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->propositions,
			marks->proposition_count,
			sizeof(*marks->propositions)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->claims,
			marks->claim_count,
			sizeof(*marks->claims)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&marks->derivations,
			marks->derivation_count,
			sizeof(*marks->derivations)
		) != 0 || artifact_alloc_bytes(
		(void**)&marks->substitutions,
			marks->substitution_count,
			sizeof(*marks->substitutions)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->term_order,
			marks->term_count,
			sizeof(*marks->term_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->case_order,
			marks->case_count,
			sizeof(*marks->case_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->case_binder_order,
			marks->case_binder_count,
			sizeof(*marks->case_binder_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->frame_order,
			marks->frame_count,
			sizeof(*marks->frame_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->type_order,
			marks->type_count,
			sizeof(*marks->type_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->parameter_order,
			marks->parameter_count,
			sizeof(*marks->parameter_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->constructor_order,
			marks->constructor_count,
			sizeof(*marks->constructor_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->readback_field_type_order,
			marks->readback_field_type_count,
			sizeof(*marks->readback_field_type_order)
		) != 0 || artifact_alloc_bytes(
		(void**)&marks->type_expr_order,
			marks->type_expr_count,
			sizeof(*marks->type_expr_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->proposition_order,
			marks->proposition_count,
			sizeof(*marks->proposition_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->claim_order,
			marks->claim_count,
			sizeof(*marks->claim_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->derivation_order,
			marks->derivation_count,
			sizeof(*marks->derivation_order)
		) != 0 || artifact_alloc_bytes(
			(void**)&marks->substitution_order,
			marks->substitution_count,
			sizeof(*marks->substitution_order)
		) != 0) {
		artifact_marks_free(marks);
		return -1;
	}
	return 0;
}

static int artifact_mark_roots(
	struct artifact_graph_marks* marks,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata
) {
	if (!marks || !interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	/* A structural TYPE_FORMER does not select a nominal declaration. Root the
	 * exported TypeViews first so representation traversal cannot promote an
	 * allocation-dependent representative declaration into the artifact. */
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (artifact_mark_type(
				marks,
				terms,
				interface->type_exports[i].local_type_id,
				0
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (!metadata || export->operation >= metadata->operation_count ||
			metadata->operations[export->operation].classifier != export->classifier) {
			return -1;
		}
		if (artifact_mark_term(marks, terms, export->local_term, 0) != 0 ||
			artifact_mark_subject_relations(marks, terms, judgement, export->local_term) != 0) {
			return -1;
		}
		if (export->classifier != PROTOTYPE_INVALID_ID) {
			int claim_status = artifact_mark_exact_relation(
					marks,
					terms,
					judgement,
					export->local_term,
					export->classifier
				);
			if (claim_status < 0 ||
				(claim_status > 0 && artifact_export_has_residual_constraint(
					metadata, export->operation
				) <= 0) ||
				artifact_mark_term(marks, terms, export->classifier, 0) != 0 ||
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->classifier
				) != 0) {
				fprintf(
					stderr,
					"artifact export has neither accepted claim nor residual obligation "
					"operation=%u subject=%u classifier=%u\n",
					export->operation,
					export->local_term,
					export->classifier
				);
				return -1;
			}
		} else if (artifact_export_has_residual_constraint(
			metadata, export->operation
		) <= 0) {
			fprintf(
				stderr,
				"artifact export has neither classifier Claim nor residual obligation "
				"operation=%u subject=%u\n",
				export->operation,
				export->local_term
			);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		if (artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->source_type_claim_id,
				0
			) != 0 || artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->identity_family_has_type_claim_id,
				0
			) != 0) {
			fprintf(
				stderr,
				"artifact closure: identity claim traversal failed root=%zu source=%u family=%u\n",
				i,
				root->source_type_claim_id,
				root->identity_family_has_type_claim_id
			);
			return -1;
		}
		if (root->witness_has_type_claim_id != PROTOTYPE_INVALID_ID &&
			 artifact_mark_accepted_claim(
				marks,
				terms,
				judgement,
				root->witness_has_type_claim_id,
				0
			 ) != 0) {
			fprintf(
				stderr,
				"artifact closure: identity witness claim traversal failed root=%zu claim=%u\n",
				i,
				root->witness_has_type_claim_id
			);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		if (export->formation_classifier == PROTOTYPE_INVALID_ID ||
			artifact_mark_term(marks, terms, export->formation_classifier, 0) != 0 ||
			artifact_mark_subject_relations(
				marks, terms, judgement, export->formation_classifier
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		if (export->curried_classifier_cache != PROTOTYPE_INVALID_ID &&
			(artifact_mark_term(
					marks, terms, export->curried_classifier_cache, 0
				) != 0 ||
				artifact_mark_subject_relations(
					marks,
					terms,
					judgement,
					export->curried_classifier_cache
				) != 0)) {
			return -1;
		}
	}
	if (metadata) {
		unsigned char reachable_contexts[PROTOTYPE_CONTEXT_CAPACITY];
		memset(reachable_contexts, 0, sizeof(reachable_contexts));
		reachable_contexts[prototype_context_empty(&metadata->contexts)] = 1;
		for (size_t i = 0; i < interface->type_export_count; ++i) {
			uint32_t type_id = interface->type_exports[i].local_type_id;
			if (type_id >= type_declarations->type_count) {
				return -1;
			}
			const struct prototype_type_declaration* type =
				&type_declarations->type_declarations[type_id];
			for (uint32_t j = 0; j < type->constructor_count; ++j) {
				const struct prototype_type_constructor_declaration* constructor =
					&type_declarations->constructor_declarations[
						type->first_constructor + j
					];
				uint32_t context_ids[] = {
					constructor->parameter_context,
					constructor->field_context
				};
				for (size_t k = 0; k < 2; ++k) {
					uint32_t context_id = context_ids[k];
					while (context_id != prototype_context_empty(
						&metadata->contexts
					)) {
						const struct prototype_context* context =
							prototype_context_get(
								&metadata->contexts, context_id
							);
						if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
							return -1;
						}
						reachable_contexts[context_id] = 1;
						context_id = context->parent;
					}
				}
			}
		}
		for (size_t position = 0;
			position < marks->ordered_derivation_count;
			++position) {
			uint32_t derivation_id = marks->derivation_order[position];
			if (derivation_id >= marks->derivation_count ||
				!marks->derivations[derivation_id]) {
				return -1;
			}
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[derivation_id];
			if (derivation->semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				artifact_mark_substitution(
					marks,
					terms,
					&metadata->substitutions,
					derivation->semantic_action_id,
					0
				) != 0) {
				return -1;
			}
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				if (derivation->premises[j].semantic_action_kind ==
						PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
					artifact_mark_substitution(
						marks,
						terms,
						&metadata->substitutions,
						derivation->premises[j].semantic_action_id,
						0
					) != 0) {
					return -1;
				}
			}
		}
		if (artifact_canonicalize_substitution_order(
				marks, &metadata->substitutions
			) != 0) {
			return -1;
		}
		for (size_t i = 0; i < marks->substitution_count; ++i) {
			if (!marks->substitutions[i]) {
				continue;
			}
			const struct prototype_substitution* substitution =
				prototype_substitution_get(
					&metadata->substitutions, (uint32_t)i
				);
			if (!substitution) {
				return -1;
			}
			uint32_t context_ids[] = {
				substitution->source_context,
				substitution->target_context
			};
			for (size_t j = 0; j < 2; ++j) {
				uint32_t context_id = context_ids[j];
				while (context_id != prototype_context_empty(
					&metadata->contexts
				)) {
					const struct prototype_context* context =
						prototype_context_get(&metadata->contexts, context_id);
					if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
						return -1;
					}
					reachable_contexts[context_id] = 1;
					context_id = context->parent;
				}
			}
		}
		for (size_t i = 0; i < metadata->operation_count; ++i) {
			const struct prototype_operation_node* operation = &metadata->operations[i];
			uint32_t context_id = operation->context_id;
			while (context_id != prototype_context_empty(&metadata->contexts)) {
				const struct prototype_context* context =
					prototype_context_get(&metadata->contexts, context_id);
				if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
					return -1;
				}
				reachable_contexts[context_id] = 1;
				context_id = context->parent;
			}
			uint32_t references[] = {
				operation->core_term,
				operation->known_classifier,
				operation->classifier,
				operation->binder_classifier
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
				}
			}
			for (size_t i = 0; i < metadata->effect_constraint_count; ++i) {
				const struct prototype_operation_effect_constraint* constraint =
					&metadata->effect_constraints[i];
				uint32_t references[] = {
					constraint->result_row,
					constraint->left_row,
					constraint->right_row
				};
				for (size_t j = 0;
					j < sizeof(references) / sizeof(references[0]);
					++j) {
					if (references[j] != PROTOTYPE_INVALID_ID &&
						artifact_mark_term(
							marks, terms, references[j], 0
						) != 0) {
						return -1;
					}
				}
			}
			for (size_t i = 0;
			i < prototype_verification_db_count(&metadata->verification);
			++i) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(&metadata->verification, (uint32_t)i);
			if (!obligation) {
				return -1;
			}
			uint32_t references[] = {
				obligation->core_term,
				obligation->input_classifier,
				obligation->classifier_family,
				obligation->effect_row
			};
			for (size_t j = 0; j < sizeof(references) / sizeof(references[0]); ++j) {
				if (references[j] != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(marks, terms, references[j], 0) != 0) {
					return -1;
				}
			}
		}
		for (size_t i = 0; i < marks->claim_count; ++i) {
			if (!marks->claims[i]) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i);
			if (!proposition) {
				return -1;
			}
			uint32_t context_id = proposition->context_id;
			while (context_id != prototype_context_empty(&metadata->contexts)) {
				const struct prototype_context* context =
					prototype_context_get(&metadata->contexts, context_id);
				if (!context || context_id >= PROTOTYPE_CONTEXT_CAPACITY) {
					return -1;
				}
				reachable_contexts[context_id] = 1;
				context_id = context->parent;
			}
		}
		for (uint32_t i = 1; i < metadata->contexts.context_count; ++i) {
			if (!reachable_contexts[i]) {
				continue;
			}
			const struct prototype_context* context =
				prototype_context_get(&metadata->contexts, i);
			uint32_t classifier = prototype_context_classifier_term(context);
			if (!context ||
				(classifier != PROTOTYPE_INVALID_ID &&
					artifact_mark_term(
						marks, terms, classifier, 0
					) != 0)) {
				return -1;
			}
		}
	}
	return 0;
}

static int artifact_closure_graph_alloc(
	struct artifact_closure_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	size_t judgement_capacity = judgement->claim_count;
	if (judgement_capacity < judgement->derivation_count) {
		judgement_capacity = judgement->derivation_count;
	}
	if (judgement_capacity < judgement->proposition_count) {
		judgement_capacity = judgement->proposition_count;
	}
	if (artifact_alloc_bytes((void**)&graph->term_nodes, terms->term_count, sizeof(*graph->term_nodes)) != 0 ||
		artifact_alloc_bytes((void**)&graph->cases, terms->case_count, sizeof(*graph->cases)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_label_symbols,
			terms->case_count,
			sizeof(*graph->case_label_symbols)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->case_binders,
			terms->case_binder_count,
			sizeof(*graph->case_binders)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->frames, terms->ih_scope_count, sizeof(*graph->frames)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_nodes,
			type_declarations->type_count,
			sizeof(*graph->type_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->parameter_declarations,
			type_declarations->parameter_count,
			sizeof(*graph->parameter_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->constructor_declarations,
			type_declarations->constructor_count,
			sizeof(*graph->constructor_declarations)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->readback_field_types,
			type_declarations->readback_field_type_count,
			sizeof(*graph->readback_field_types)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->type_exprs,
			type_declarations->expr_count,
			sizeof(*graph->type_exprs)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->claims, judgement_capacity, sizeof(*graph->claims)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->derivations,
			judgement_capacity,
			sizeof(*graph->derivations)
		) != 0 ||
		artifact_alloc_bytes((void**)&graph->propositions, judgement_capacity, sizeof(*graph->propositions)) != 0 ||
		artifact_alloc_bytes((void**)&graph->derivation_candidates, judgement_capacity, sizeof(*graph->derivation_candidates)) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->candidate_premises,
			judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			sizeof(*graph->candidate_premises)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->accepted_premises,
			judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			sizeof(*graph->accepted_premises)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_nodes,
			universe->node_count,
			sizeof(*graph->universe_nodes)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_edges,
			universe->edge_count,
			sizeof(*graph->universe_edges)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_levels,
			universe->level_count,
			sizeof(*graph->universe_levels)
		) != 0 ||
		artifact_alloc_bytes(
			(void**)&graph->universe_constraints,
			universe->constraint_count,
			sizeof(*graph->universe_constraints)
		) != 0) {
		artifact_closure_graph_free(graph);
		return -1;
	}
	prototype_term_db_init(
		&graph->terms,
		graph->term_nodes,
		terms->term_count,
		graph->cases,
		graph->case_label_symbols,
		terms->case_count,
		graph->case_binders,
		terms->case_binder_count,
		graph->frames,
		terms->ih_scope_count
	);
	prototype_type_declaration_db_init(
		&graph->type_declarations,
		graph->type_nodes,
		type_declarations->type_count,
		graph->constructor_declarations,
		type_declarations->constructor_count,
		graph->parameter_declarations,
		type_declarations->parameter_count,
		graph->readback_field_types,
		type_declarations->readback_field_type_count,
		graph->type_exprs,
		type_declarations->expr_count
	);
	prototype_judgement_db_init(
		&graph->judgement,
		graph->propositions,
		graph->derivation_candidates,
		graph->claims,
		graph->derivations,
		judgement_capacity,
		graph->candidate_premises,
		judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		graph->accepted_premises,
		judgement_capacity * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	);
	prototype_universe_db_init(
		&graph->universe,
		graph->universe_nodes,
		universe->node_count,
		graph->universe_edges,
		universe->edge_count,
		graph->universe_levels,
		universe->level_count,
		graph->universe_constraints,
		universe->constraint_count
	);
	graph->terms.term_count = terms->term_count;
	graph->terms.case_count = terms->case_count;
	graph->terms.case_binder_count = terms->case_binder_count;
	graph->terms.ih_scope_count = terms->ih_scope_count;
	graph->terms.computation_fold_clause_count = terms->computation_fold_clause_count;
	graph->terms.next_binding_id = terms->next_binding_id;
	graph->type_declarations.type_count = type_declarations->type_count;
	graph->type_declarations.parameter_count = type_declarations->parameter_count;
	graph->type_declarations.constructor_count = type_declarations->constructor_count;
	graph->type_declarations.readback_field_type_count = type_declarations->readback_field_type_count;
	graph->type_declarations.expr_count = type_declarations->expr_count;
	graph->type_declarations.next_level_var = type_declarations->next_level_var;
	graph->judgement.claim_count = judgement->claim_count;
	graph->judgement.derivation_count = judgement->derivation_count;
	graph->judgement.next_universe_var = judgement->next_universe_var;
	graph->universe.node_count = universe->node_count;
	graph->universe.edge_count = universe->edge_count;
	graph->universe.level_count = universe->level_count;
	graph->universe.constraint_count = 0;
	graph->universe.solved = universe->solved;
	artifact_init_closure_defaults(graph);
	return 0;
}

static int artifact_closure_graph_copy_marked(
	struct artifact_closure_graph* graph,
	const struct artifact_graph_marks* marks,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe
) {
	if (!graph || !marks || !terms || !judgement || !universe) {
		return -1;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		if (marks->terms[i]) {
			graph->terms.terms[i] = terms->terms[i];
		}
	}
	memcpy(
		graph->terms.computation_fold_clauses,
		terms->computation_fold_clauses,
		terms->computation_fold_clause_count * sizeof(*terms->computation_fold_clauses)
	);
	for (size_t i = 0; i < terms->case_count; ++i) {
		if (marks->cases[i]) {
			graph->terms.cases[i] = terms->cases[i];
			graph->terms.case_label_symbols[i] = terms->case_label_symbols[i];
		}
	}
	for (size_t i = 0; i < terms->case_binder_count; ++i) {
		if (marks->case_binders[i]) {
			graph->terms.case_binders[i] = terms->case_binders[i];
		}
	}
	for (size_t i = 0; i < terms->ih_scope_count; ++i) {
		if (marks->frames[i]) {
			graph->terms.ih_scopes[i] = terms->ih_scopes[i];
		}
	}
	const struct prototype_type_declaration_db* type_declarations =
		marks->type_declarations;
	for (size_t i = 0; i < type_declarations->type_count; ++i) {
		if (marks->types[i]) {
			graph->type_declarations.type_declarations[i] =
				type_declarations->type_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		if (marks->parameters[i]) {
			graph->type_declarations.parameter_declarations[i] =
				type_declarations->parameter_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->constructor_count; ++i) {
		if (marks->constructors[i]) {
			graph->type_declarations.constructor_declarations[i] =
				type_declarations->constructor_declarations[i];
		}
	}
	for (size_t i = 0; i < type_declarations->readback_field_type_count; ++i) {
		if (marks->readback_field_types[i]) {
			graph->type_declarations.readback_field_types[i] =
				type_declarations->readback_field_types[i];
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		if (marks->type_exprs[i]) {
			graph->type_declarations.exprs[i] = type_declarations->exprs[i];
		}
	}
	if (graph->type_declarations.representation_capacity <
		type_declarations->representation_count) {
		return -1;
	}
	memcpy(
		graph->type_declarations.representations,
		type_declarations->representations,
		type_declarations->representation_count * sizeof(*type_declarations->representations)
	);
	graph->type_declarations.representation_count = type_declarations->representation_count;
	graph->type_declarations.representations_dirty = type_declarations->representations_dirty;
	graph->judgement.proposition_count = judgement->proposition_count;
	for (size_t i = 0; i < judgement->claim_count; ++i) {
		if (marks->claims[i]) {
			const struct prototype_judgement_claim* source_claim =
				&judgement->claims[i];
			const struct prototype_judgement_proposition* source_proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i);
			if (!source_proposition || source_claim->proposition_id >=
				graph->judgement.proposition_capacity) {
				return -1;
			}
			graph->judgement.propositions[source_claim->proposition_id] =
				*source_proposition;
			graph->judgement.claims[i] = judgement->claims[i];
		}
	}
	graph->judgement.accepted_premise_count = 0;
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		if (marks->derivations[i]) {
			const struct prototype_judgement_derivation* source_derivation =
				&judgement->derivations[i];
			struct prototype_judgement_derivation* target_derivation =
				&graph->judgement.derivations[i];
			if (source_derivation->premise_count >
				graph->judgement.accepted_premise_capacity -
				graph->judgement.accepted_premise_count) {
				return -1;
			}
			*target_derivation = *source_derivation;
			target_derivation->premises = source_derivation->premise_count == 0 ?
				NULL : &graph->judgement.accepted_premises[
					graph->judgement.accepted_premise_count
				];
			for (uint32_t j = 0; j < source_derivation->premise_count; ++j) {
				target_derivation->premises[j] = source_derivation->premises[j];
				if (source_derivation->premises[j].claim_id == PROTOTYPE_INVALID_ID) {
					const struct prototype_judgement_proposition* scoped =
						prototype_judgement_premise_proposition(
							judgement, &source_derivation->premises[j]
						);
					uint32_t proposition_id =
						source_derivation->premises[j].scoped_proposition_id;
					if (!scoped || proposition_id >=
						graph->judgement.proposition_capacity) {
						return -1;
					}
					graph->judgement.propositions[proposition_id] = *scoped;
				}
			}
			graph->judgement.accepted_premise_count +=
				source_derivation->premise_count;
		}
	}
	if (prototype_judgement_db_rebuild_index(&graph->judgement) != 0) {
		return -1;
	}
	for (size_t i = 0; i < universe->node_count; ++i) {
		const struct prototype_universe_node* node = &universe->nodes[i];
		if (node->tag == PROTOTYPE_UNIVERSE_NODE_TYPE &&
			node->type_id < marks->type_count &&
			marks->types[node->type_id]) {
			graph->universe.nodes[i] = *node;
		} else if (node->tag == PROTOTYPE_UNIVERSE_NODE_PARAMETER &&
			node->parameter_id < marks->parameter_count &&
			marks->parameters[node->parameter_id]) {
			graph->universe.nodes[i] = *node;
		}
	}
	for (size_t i = 0; i < universe->edge_count; ++i) {
		const struct prototype_universe_edge* edge = &universe->edges[i];
		if (edge->from_node < graph->universe.node_count &&
			edge->to_node < graph->universe.node_count &&
			artifact_universe_node_present(&graph->universe.nodes[edge->from_node]) &&
			artifact_universe_node_present(&graph->universe.nodes[edge->to_node])) {
			graph->universe.edges[i] = *edge;
		}
	}
	for (size_t i = 0; i < universe->level_count; ++i) {
		graph->universe.levels[i] = universe->levels[i];
	}
	for (size_t i = 0; i < universe->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &universe->constraints[i];
		if (constraint->subject < marks->term_count &&
			constraint->classifier < marks->term_count &&
			marks->terms[constraint->subject] &&
			marks->terms[constraint->classifier] &&
			(constraint->source_claim_id == PROTOTYPE_INVALID_ID ||
			 (constraint->source_claim_id < marks->claim_count &&
			  marks->claims[constraint->source_claim_id]))) {
			graph->universe.constraints[graph->universe.constraint_count++] = *constraint;
		}
	}
	graph->universe.solved = universe->solved;
	return 0;
}

static int artifact_build_closure_graph(
	struct artifact_closure_graph* graph,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_compile_metadata* metadata
) {
	if (!graph || !interface || !terms || !type_declarations || !judgement || !universe) {
		return -1;
	}
	memset(graph, 0, sizeof(*graph));
	struct artifact_graph_marks marks;
	if (artifact_marks_init(
			&marks, terms, type_declarations, judgement, metadata
		) != 0) {
		fprintf(stderr, "artifact closure: mark allocation failed\n");
		return -1;
	}
	int status = 0;
	if (artifact_mark_roots(
			&marks, interface, terms, type_declarations, judgement, metadata
		) != 0) {
		fprintf(stderr, "artifact closure: root traversal failed\n");
		status = -1;
	} else if (artifact_closure_graph_alloc(
			graph, terms, type_declarations, judgement, universe
		) != 0) {
		fprintf(stderr, "artifact closure: graph allocation failed\n");
		status = -1;
	} else if (artifact_closure_graph_copy_marked(
			graph, &marks, terms, judgement, universe
		) != 0) {
		fprintf(stderr, "artifact closure: marked graph copy failed\n");
		status = -1;
	} else {
		artifact_closure_graph_take_publication_order(graph, &marks);
	}
	artifact_marks_free(&marks);
	if (status != 0) {
		artifact_closure_graph_free(graph);
	}
	return status;
}

static void artifact_reset_object_graph_for_publication(
	struct artifact_closure_graph* graph
) {
	graph->terms.term_count = 0;
	graph->terms.case_count = 0;
	graph->terms.case_binder_count = 0;
	graph->terms.ih_scope_count = 0;
	graph->terms.computation_fold_clause_count = 0;
	graph->terms.next_binding_id = 0;
	graph->type_declarations.type_count = 0;
	graph->type_declarations.parameter_count = 0;
	graph->type_declarations.constructor_count = 0;
	graph->type_declarations.readback_field_type_count = 0;
	graph->type_declarations.expr_count = 0;
	graph->type_declarations.representation_count = 0;
	graph->type_declarations.representations_dirty = 1;
	graph->type_declarations.next_level_var = 0;
	graph->judgement.proposition_count = 0;
	graph->judgement.derivation_candidate_count = 0;
	graph->judgement.claim_count = 0;
	graph->judgement.derivation_count = 0;
	graph->judgement.candidate_premise_count = 0;
	graph->judgement.accepted_premise_count = 0;
	graph->judgement.next_universe_var = 0;
	graph->universe.node_count = 0;
	graph->universe.edge_count = 0;
	graph->universe.level_count = 0;
	graph->universe.constraint_count = 0;
	graph->universe.solved = 0;
}

static int artifact_publication_metadata_relocate(
	struct artifact_publication_graph* publication,
	const struct prototype_compile_metadata* source,
	const struct artifact_closure_graph* closure
) {
	if (!publication || !source || !closure) {
		return -1;
	}
	struct prototype_compile_metadata* target = &publication->metadata;
	target->compile_policy = source->compile_policy;
	target->definition_thunk_policy = source->definition_thunk_policy;
	target->selected_entry_symbol_id = source->selected_entry_symbol_id;
	target->selected_entry_term = artifact_relocate_optional_id(
		source->selected_entry_term,
		publication->term_relocation,
		closure->terms.term_count
	);
	target->selected_entry_classifier = artifact_relocate_optional_id(
		source->selected_entry_classifier,
		publication->term_relocation,
		closure->terms.term_count
	);
	target->selected_entry_operation = source->selected_entry_operation;
	target->required_runtime_capabilities = source->required_runtime_capabilities;
	target->normalization_step_limit = source->normalization_step_limit;
	target->normalization_steps_used = source->normalization_steps_used;
	target->solver_step_limit = source->solver_step_limit;
	target->solver_steps_used = source->solver_steps_used;
	target->solver_exhausted = source->solver_exhausted;
	target->solver_constraint_count = source->solver_constraint_count;
	target->solver_solved_count = source->solver_solved_count;
	target->solver_residual_count = source->solver_residual_count;
	target->solver_incomplete_count = source->solver_incomplete_count;
	target->operation_count = source->operation_count;
	for (size_t i = 0; i < source->operation_count; ++i) {
		target->operations[i] = source->operations[i];
		struct prototype_operation_node* operation = &target->operations[i];
		uint32_t* term_fields[] = {
			&operation->core_term,
			&operation->known_classifier,
			&operation->classifier,
			&operation->binder_classifier
		};
		for (size_t j = 0; j < sizeof(term_fields) / sizeof(term_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*term_fields[j],
				publication->term_relocation,
				closure->terms.term_count
			);
			if (*term_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*term_fields[j] = relocated;
		}
		if (operation->context_id >= source->contexts.context_count ||
			publication->context_relocation[operation->context_id] ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		operation->context_id = publication->context_relocation[
			operation->context_id
		];
		uint32_t* binding_fields[] = {
			&operation->binding_id,
			&operation->fold_return_binder_id
		};
		for (size_t j = 0; j < sizeof(binding_fields) /
				sizeof(binding_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*binding_fields[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (*binding_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*binding_fields[j] = relocated;
		}
		for (uint32_t j = 0; j < operation->implicit_effect_row_count; ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				operation->implicit_effect_row_binders[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			operation->implicit_effect_row_binders[j] = relocated;
		}
	}
	target->operation_case_count = source->operation_case_count;
	for (size_t i = 0; i < source->operation_case_count; ++i) {
		target->operation_cases[i] = source->operation_cases[i];
		struct prototype_operation_match_case* operation_case =
			&target->operation_cases[i];
		if (operation_case->context_id >= source->contexts.context_count ||
			operation_case->constructor_owner >= closure->terms.term_count ||
			publication->context_relocation[operation_case->context_id] ==
				PROTOTYPE_INVALID_ID ||
			publication->term_relocation[operation_case->constructor_owner] ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		operation_case->context_id = publication->context_relocation[
			operation_case->context_id
		];
		operation_case->constructor_owner = publication->term_relocation[
			operation_case->constructor_owner
		];
	}
	target->operation_fold_clause_count = source->operation_fold_clause_count;
	for (size_t i = 0; i < source->operation_fold_clause_count; ++i) {
		target->operation_fold_clauses[i] = source->operation_fold_clauses[i];
		if (target->operation_fold_clauses[i].context_id >=
			source->contexts.context_count) {
			return -1;
		}
		target->operation_fold_clauses[i].context_id =
			publication->context_relocation[
				target->operation_fold_clauses[i].context_id
			];
		uint32_t* binding_fields[] = {
			&target->operation_fold_clauses[i].argument_binder_id,
			&target->operation_fold_clauses[i].continuation_binder_id
		};
		for (size_t j = 0; j < sizeof(binding_fields) /
				sizeof(binding_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*binding_fields[j],
				publication->binding_relocation,
				closure->terms.next_binding_id
			);
			if (*binding_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*binding_fields[j] = relocated;
		}
	}
	target->effect_constraint_count = source->effect_constraint_count;
	for (size_t i = 0; i < source->effect_constraint_count; ++i) {
		target->effect_constraints[i] = source->effect_constraints[i];
		uint32_t* rows[] = {
			&target->effect_constraints[i].result_row,
			&target->effect_constraints[i].left_row,
			&target->effect_constraints[i].right_row
		};
		for (size_t j = 0; j < sizeof(rows) / sizeof(rows[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*rows[j], publication->term_relocation, closure->terms.term_count
			);
			if (*rows[j] != PROTOTYPE_INVALID_ID && relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*rows[j] = relocated;
		}
	}
	target->verification.obligation_count = source->verification.obligation_count;
	for (size_t i = 0; i < source->verification.obligation_count; ++i) {
		target->verification.obligations[i] = source->verification.obligations[i];
		struct prototype_verification_obligation* obligation =
			&target->verification.obligations[i];
		uint32_t* term_fields[] = {
			&obligation->core_term,
			&obligation->input_classifier,
			&obligation->classifier_family,
			&obligation->effect_row
		};
		for (size_t j = 0; j < sizeof(term_fields) / sizeof(term_fields[0]); ++j) {
			uint32_t relocated = artifact_relocate_optional_id(
				*term_fields[j],
				publication->term_relocation,
				closure->terms.term_count
			);
			if (*term_fields[j] != PROTOTYPE_INVALID_ID &&
				relocated == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*term_fields[j] = relocated;
		}
		uint32_t relocated_binding = artifact_relocate_optional_id(
			obligation->continuation_binder_id,
			publication->binding_relocation,
			closure->terms.next_binding_id
		);
		if (obligation->continuation_binder_id != PROTOTYPE_INVALID_ID &&
			relocated_binding == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		obligation->continuation_binder_id = relocated_binding;
	}
	return 0;
}

static int artifact_publication_graph_alloc(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure,
	const struct prototype_artifact_interface* interface,
	const struct prototype_compile_metadata* metadata
) {
	if (!publication || !closure || !interface || !metadata) {
		return -1;
	}
	if (closure->terms.term_count > SIZE_MAX -
			closure->type_declarations.expr_count ||
		closure->terms.term_count + closure->type_declarations.expr_count >
			SIZE_MAX - interface->type_expr_count ||
		closure->terms.term_count + closure->type_declarations.expr_count +
			interface->type_expr_count >
			SIZE_MAX - closure->type_declarations.type_count ||
		closure->terms.term_count + closure->type_declarations.expr_count +
			interface->type_expr_count + closure->type_declarations.type_count >
			SIZE_MAX - interface->dependency_count) {
		return -1;
	}
	size_t dependency_capacity = interface->dependency_count +
		closure->terms.term_count + closure->type_declarations.expr_count +
		interface->type_expr_count + closure->type_declarations.type_count;
	memset(publication, 0, sizeof(*publication));
#define ALLOC_PUBLICATION(field, count) \
	do { \
		if (artifact_alloc_bytes( \
				(void**)&publication->field, \
				(count), \
				sizeof(*publication->field) \
			) != 0) { \
			artifact_publication_graph_free(publication); \
			return -1; \
		} \
	} while (0)
	ALLOC_PUBLICATION(term_exports, interface->term_export_count);
	ALLOC_PUBLICATION(type_exports, interface->type_export_count);
	ALLOC_PUBLICATION(type_parameters, interface->type_parameter_count);
	ALLOC_PUBLICATION(constructor_exports, interface->constructor_export_count);
	ALLOC_PUBLICATION(
		constructor_field_type_exprs,
		interface->constructor_field_type_expr_count
	);
	ALLOC_PUBLICATION(interface_type_exprs, interface->type_expr_count);
	ALLOC_PUBLICATION(identity_roots, interface->identity_root_count);
	ALLOC_PUBLICATION(dependencies, dependency_capacity);
	ALLOC_PUBLICATION(contexts, metadata->contexts.context_count);
	ALLOC_PUBLICATION(substitutions, metadata->substitutions.substitution_count);
	ALLOC_PUBLICATION(operations, metadata->operation_count);
	ALLOC_PUBLICATION(operation_cases, metadata->operation_case_count);
	ALLOC_PUBLICATION(
		operation_fold_clauses, metadata->operation_fold_clause_count
	);
	ALLOC_PUBLICATION(effect_constraints, metadata->effect_constraint_count);
	ALLOC_PUBLICATION(
		verification_obligations, metadata->verification.obligation_count
	);
	ALLOC_PUBLICATION(term_relocation, closure->terms.term_count);
	ALLOC_PUBLICATION(binding_relocation, closure->terms.next_binding_id);
	ALLOC_PUBLICATION(context_relocation, metadata->contexts.context_count);
	ALLOC_PUBLICATION(type_relocation, closure->type_declarations.type_count);
	ALLOC_PUBLICATION(type_expr_relocation, closure->type_declarations.expr_count);
	ALLOC_PUBLICATION(
		parameter_relocation, closure->type_declarations.parameter_count
	);
	ALLOC_PUBLICATION(
		constructor_relocation, closure->type_declarations.constructor_count
	);
	ALLOC_PUBLICATION(
		field_relocation, closure->type_declarations.readback_field_type_count
	);
	ALLOC_PUBLICATION(
		proposition_relocation, closure->judgement.proposition_count
	);
	ALLOC_PUBLICATION(claim_relocation, closure->judgement.claim_count);
	ALLOC_PUBLICATION(
		substitution_relocation, metadata->substitutions.substitution_count
	);
#undef ALLOC_PUBLICATION
	size_t derived_classifier_term_capacity = 0;
	for (size_t i = 0;
		i < closure->type_declarations.constructor_count;
		++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&closure->type_declarations.constructor_declarations[i];
		if (constructor->owner_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_context* parameter_context =
			prototype_context_get(
				&metadata->contexts, constructor->parameter_context
			);
		const struct prototype_context* field_context = prototype_context_get(
			&metadata->contexts, constructor->field_context
		);
		if (!parameter_context || !field_context ||
			field_context->depth < parameter_context->depth) {
			artifact_publication_graph_free(publication);
			return -1;
		}
		size_t parameter_terms = parameter_context->depth;
		size_t field_terms =
			(size_t)(field_context->depth - parameter_context->depth) * 4;
		if (derived_classifier_term_capacity >
				SIZE_MAX - parameter_terms ||
			derived_classifier_term_capacity + parameter_terms >
				SIZE_MAX - field_terms) {
			artifact_publication_graph_free(publication);
			return -1;
		}
		derived_classifier_term_capacity += parameter_terms + field_terms;
	}
	if (closure->terms.term_count >
		SIZE_MAX - derived_classifier_term_capacity) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_term_db publication_term_capacity = closure->terms;
	publication_term_capacity.term_count = closure->terms.term_count +
		derived_classifier_term_capacity;
	if (artifact_closure_graph_alloc(
			&publication->object,
			&publication_term_capacity,
			&closure->type_declarations,
			&closure->judgement,
			&(struct prototype_universe_db){
				.node_count = PROTOTYPE_UNIVERSE_NODE_CAPACITY,
				.edge_count = PROTOTYPE_UNIVERSE_EDGE_CAPACITY,
				.level_count = PROTOTYPE_UNIVERSE_LEVEL_CAPACITY,
				.constraint_count = PROTOTYPE_UNIVERSE_CONSTRAINT_CAPACITY
			}
		) != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	artifact_reset_object_graph_for_publication(&publication->object);
	prototype_artifact_interface_init(
		&publication->interface,
		publication->term_exports,
		interface->term_export_count,
		publication->type_exports,
		interface->type_export_count,
		publication->type_parameters,
		interface->type_parameter_count,
		publication->constructor_exports,
		interface->constructor_export_count,
		publication->constructor_field_type_exprs,
		interface->constructor_field_type_expr_count,
		publication->interface_type_exprs,
		interface->type_expr_count,
		publication->identity_roots,
		interface->identity_root_count,
		publication->dependencies,
		dependency_capacity
	);
	prototype_compile_metadata_init(
		&publication->metadata,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		NULL,
		0,
		publication->contexts,
		metadata->contexts.context_count,
		publication->substitutions,
		metadata->substitutions.substitution_count,
		publication->operations,
		metadata->operation_count,
		publication->operation_cases,
		metadata->operation_case_count,
		publication->operation_fold_clauses,
		metadata->operation_fold_clause_count,
		publication->effect_constraints,
		metadata->effect_constraint_count,
		publication->verification_obligations,
		metadata->verification.obligation_count
	);
	return 0;
}

static int artifact_publication_universe_relocate(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure
) {
	if (!publication || !closure) {
		return -1;
	}
	struct prototype_operation_graph operations;
	prototype_compile_metadata_operation_graph(
		&publication->metadata, &operations
	);
	if (prototype_universe_collect(
		&publication->object.universe,
		&publication->object.type_declarations,
		&publication->object.terms,
		&operations,
		&publication->object.judgement
	) != 0) {
		return -1;
	}
	for (size_t i = 0; i < publication->object.universe.node_count; ++i) {
		/* The owning Type/Parameter carries the semantic name. This field is a
		 * process-local diagnostic cache and must not affect artifact bytes. */
		publication->object.universe.nodes[i].symbol_id = -1;
	}
	return 0;
}

static int artifact_build_publication_graph(
	struct artifact_publication_graph* publication,
	const struct artifact_closure_graph* closure,
	const struct prototype_artifact_interface* interface,
	const struct prototype_compile_metadata* metadata
) {
	if (artifact_publication_graph_alloc(
			publication, closure, interface, metadata
		) != 0) {
		return -1;
	}
	struct prototype_artifact_graph_relocation additional = {
		.binding_ids = publication->binding_relocation,
		.binding_id_capacity = closure->terms.next_binding_id,
		.type_ids = publication->type_relocation,
		.type_id_capacity = closure->type_declarations.type_count,
		.type_expr_ids = publication->type_expr_relocation,
		.type_expr_id_capacity = closure->type_declarations.expr_count,
		.parameter_ids = publication->parameter_relocation,
		.parameter_id_capacity = closure->type_declarations.parameter_count,
		.constructor_ids = publication->constructor_relocation,
		.constructor_id_capacity = closure->type_declarations.constructor_count,
		.field_type_ids = publication->field_relocation,
		.field_type_id_capacity = closure->type_declarations.readback_field_type_count,
		.proposition_ids = publication->proposition_relocation,
		.proposition_id_capacity = closure->judgement.proposition_count,
		.claim_ids = publication->claim_relocation,
		.claim_id_capacity = closure->judgement.claim_count,
		.substitution_ids = publication->substitution_relocation,
		.substitution_id_capacity = metadata->substitutions.substitution_count
	};
	struct artifact_substitution_slice substitution_slice;
	if (artifact_substitution_slice_init(
			&substitution_slice,
			&closure->judgement,
			&metadata->substitutions
		) != 0 || artifact_substitution_slice_canonicalize(
			&substitution_slice,
			closure->substitution_order,
			closure->ordered_substitution_count
		) != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct artifact_context_slice context_slice;
	if (artifact_context_slice_init(
			&context_slice,
			&closure->type_declarations,
			&closure->judgement,
			metadata,
			&substitution_slice
		) != 0) {
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	if (artifact_context_slice_canonicalize(
			&context_slice,
			&closure->type_declarations,
			&closure->judgement,
			metadata,
			closure->type_order,
			closure->ordered_type_count,
			closure->constructor_order,
			closure->ordered_constructor_count,
			closure->claim_order,
			closure->ordered_claim_count,
			closure->derivation_order,
			closure->ordered_derivation_count,
			closure->substitution_order,
			closure->ordered_substitution_count
		) != 0) {
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_context* compact_contexts = calloc(
		context_slice.context_count,
		sizeof(*compact_contexts)
	);
	struct prototype_substitution* compact_substitutions = calloc(
		substitution_slice.substitution_count == 0 ? 1 :
			substitution_slice.substitution_count,
		sizeof(*compact_substitutions)
	);
	struct prototype_type_declaration* compact_types = malloc(
		(closure->type_declarations.type_count == 0 ? 1 :
		 closure->type_declarations.type_count) * sizeof(*compact_types)
	);
	struct prototype_type_constructor_declaration* compact_constructors = malloc(
		(closure->type_declarations.constructor_count == 0 ? 1 :
		 closure->type_declarations.constructor_count) * sizeof(*compact_constructors)
	);
	struct prototype_judgement_proposition* compact_propositions = malloc(
		(closure->judgement.proposition_count == 0 ? 1 :
		 closure->judgement.proposition_count) * sizeof(*compact_propositions)
	);
	uint32_t* compact_context_relocation = calloc(
		context_slice.context_count,
		sizeof(*compact_context_relocation)
	);
	if (!compact_contexts || !compact_substitutions || !compact_types ||
		!compact_constructors || !compact_propositions ||
		!compact_context_relocation) {
		free(compact_contexts);
		free(compact_substitutions);
		free(compact_types);
		free(compact_constructors);
		free(compact_propositions);
		free(compact_context_relocation);
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	struct prototype_context_db compact_context_db;
	prototype_context_db_init(
		&compact_context_db,
		compact_contexts,
		context_slice.context_count
	);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		if (!context_slice.reachable[i]) {
			continue;
		}
		uint32_t compact_id = context_slice.relocation[i];
		compact_contexts[compact_id] = metadata->contexts.contexts[i];
		if (i == prototype_context_empty(&metadata->contexts)) {
			continue;
		}
		compact_contexts[compact_id].parent = context_slice.relocation[
			metadata->contexts.contexts[i].parent
		];
	}
	compact_context_db.context_count = context_slice.context_count;
	struct prototype_substitution_db compact_substitution_db;
	prototype_substitution_db_init(
		&compact_substitution_db,
		compact_substitutions,
		substitution_slice.substitution_count
	);
	compact_substitution_db.substitution_count =
		substitution_slice.substitution_count;
	for (size_t i = 0; i < metadata->substitutions.substitution_count; ++i) {
		if (!substitution_slice.reachable[i]) {
			continue;
		}
		uint32_t compact_id = substitution_slice.relocation[i];
		compact_substitutions[compact_id] =
			metadata->substitutions.substitutions[i];
		compact_substitutions[compact_id].source_context = context_slice.relocation[
			compact_substitutions[compact_id].source_context
		];
		compact_substitutions[compact_id].target_context = context_slice.relocation[
			compact_substitutions[compact_id].target_context
		];
		if (compact_substitutions[compact_id].kind ==
				PROTOTYPE_SUBSTITUTION_EXTEND) {
			compact_substitutions[compact_id].first = substitution_slice.relocation[
				compact_substitutions[compact_id].first
			];
		} else if (compact_substitutions[compact_id].kind ==
				PROTOTYPE_SUBSTITUTION_COMPOSE) {
			compact_substitutions[compact_id].first = substitution_slice.relocation[
				compact_substitutions[compact_id].first
			];
			compact_substitutions[compact_id].second = substitution_slice.relocation[
				compact_substitutions[compact_id].second
			];
		}
	}
	memcpy(
		compact_types,
		closure->type_declarations.type_declarations,
		closure->type_declarations.type_count * sizeof(*compact_types)
	);
	memcpy(
		compact_constructors,
		closure->type_declarations.constructor_declarations,
		closure->type_declarations.constructor_count * sizeof(*compact_constructors)
	);
	struct prototype_type_declaration_db compact_type_declarations =
		closure->type_declarations;
	compact_type_declarations.type_declarations = compact_types;
	compact_type_declarations.constructor_declarations = compact_constructors;
	for (size_t i = 0; i < compact_type_declarations.type_count; ++i) {
		if (!artifact_type_present(&compact_types[i])) {
			continue;
		}
		compact_types[i].parameter_context = context_slice.relocation[
			compact_types[i].parameter_context
		];
		compact_types[i].index_context = context_slice.relocation[
			compact_types[i].index_context
		];
	}
	for (size_t i = 0; i < compact_type_declarations.constructor_count; ++i) {
		if (!artifact_constructor_present(&compact_constructors[i])) {
			continue;
		}
		if (compact_constructors[i].owner_type >=
				compact_type_declarations.type_count) {
			free(compact_contexts);
			free(compact_substitutions);
			free(compact_types);
			free(compact_constructors);
			free(compact_propositions);
			free(compact_context_relocation);
			artifact_context_slice_free(&context_slice);
			artifact_substitution_slice_free(&substitution_slice);
			artifact_publication_graph_free(publication);
			return -1;
		}
		int incomplete_readback = 0;
		if (compact_constructors[i].readback.field_count > 0) {
			uint32_t first = compact_constructors[i].readback.first_field_type;
			uint32_t count = compact_constructors[i].readback.field_count;
			if (first == PROTOTYPE_INVALID_ID ||
				first > closure->type_declarations.readback_field_type_count ||
				count > closure->type_declarations.readback_field_type_count - first) {
				incomplete_readback = 1;
			} else {
				for (uint32_t j = 0; j < count; ++j) {
					if (closure->type_declarations.readback_field_types[first + j] ==
						PROTOTYPE_INVALID_ID) {
						incomplete_readback = 1;
						break;
					}
				}
			}
		}
		if (compact_types[compact_constructors[i].owner_type].origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
			incomplete_readback) {
			/* Generated declarations have no source readback schema. Their field
			 * telescope and result classifier are the sole wire authority. The same
			 * rule applies when optional source readback metadata is incomplete. */
			compact_constructors[i].readback.first_field_type = PROTOTYPE_INVALID_ID;
			compact_constructors[i].readback.field_count = 0;
			if (compact_types[compact_constructors[i].owner_type].origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY) {
				compact_constructors[i].readback.result_type = PROTOTYPE_INVALID_ID;
			}
		}
		compact_constructors[i].parameter_context = context_slice.relocation[
			compact_constructors[i].parameter_context
		];
		compact_constructors[i].field_context = context_slice.relocation[
			compact_constructors[i].field_context
		];
	}
	memcpy(
		compact_propositions,
		closure->judgement.propositions,
		closure->judgement.proposition_count * sizeof(*compact_propositions)
	);
	struct prototype_judgement_db compact_judgement = closure->judgement;
	compact_judgement.propositions = compact_propositions;
	for (size_t i = 0; i < compact_judgement.proposition_count; ++i) {
		if (!artifact_candidate_claim_present(&compact_propositions[i])) {
			continue;
		}
		compact_propositions[i].context_id = context_slice.relocation[
			compact_propositions[i].context_id
		];
	}
	for (size_t i = 0; i < compact_judgement.derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&compact_judgement.derivations[i];
		if (!artifact_derivation_present(derivation)) {
			continue;
		}
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			if (derivation->semantic_action_id >=
					substitution_slice.source_count ||
				substitution_slice.relocation[derivation->semantic_action_id] ==
					PROTOTYPE_INVALID_ID) {
				free(compact_contexts);
				free(compact_substitutions);
				free(compact_types);
				free(compact_constructors);
				free(compact_propositions);
				free(compact_context_relocation);
				artifact_context_slice_free(&context_slice);
				artifact_substitution_slice_free(&substitution_slice);
				artifact_publication_graph_free(publication);
				return -1;
			}
			derivation->semantic_action_id = substitution_slice.relocation[
				derivation->semantic_action_id
			];
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind !=
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				continue;
			}
			uint32_t substitution_id =
				derivation->premises[j].semantic_action_id;
			if (substitution_id >= substitution_slice.source_count ||
				substitution_slice.relocation[substitution_id] ==
					PROTOTYPE_INVALID_ID) {
				free(compact_contexts);
				free(compact_substitutions);
				free(compact_types);
				free(compact_constructors);
				free(compact_propositions);
				free(compact_context_relocation);
				artifact_context_slice_free(&context_slice);
				artifact_substitution_slice_free(&substitution_slice);
				artifact_publication_graph_free(publication);
				return -1;
			}
			derivation->premises[j].semantic_action_id =
				substitution_slice.relocation[substitution_id];
		}
	}
	const struct artifact_append_order publication_order = {
		.terms = closure->term_order,
		.term_count = closure->ordered_term_count,
		.types = closure->type_order,
		.type_count = closure->ordered_type_count,
		.type_exprs = closure->type_expr_order,
		.type_expr_count = closure->ordered_type_expr_count,
		.fields = closure->readback_field_type_order,
		.field_count = closure->ordered_readback_field_type_count,
		.parameters = closure->parameter_order,
		.parameter_count = closure->ordered_parameter_count,
		.constructors = closure->constructor_order,
		.constructor_count = closure->ordered_constructor_count,
		.propositions = closure->proposition_order,
		.proposition_count = closure->ordered_proposition_count,
		.claims = closure->claim_order,
		.claim_count = closure->ordered_claim_count,
		.derivations = closure->derivation_order,
		.derivation_count = closure->ordered_derivation_count,
		.substitutions = closure->substitution_order,
		.substitution_count = closure->ordered_substitution_count
	};
	if (prototype_internal_artifact_append_graph_ordered(
			&publication->interface,
			&publication->object.terms,
			&publication->object.type_declarations,
			&publication->object.judgement,
			&publication->metadata.contexts,
			&publication->metadata.substitutions,
			interface,
			&closure->terms,
			&compact_type_declarations,
			&compact_judgement,
			&compact_context_db,
			&compact_substitution_db,
			0,
			publication->term_relocation,
			closure->terms.term_count,
				compact_context_relocation,
				context_slice.context_count,
				&additional,
				0,
				&publication_order
			) != 0) {
		fprintf(stderr, "artifact publication append failed\n");
		free(compact_contexts);
		free(compact_substitutions);
		free(compact_types);
		free(compact_constructors);
		free(compact_propositions);
		free(compact_context_relocation);
		artifact_context_slice_free(&context_slice);
		artifact_substitution_slice_free(&substitution_slice);
		artifact_publication_graph_free(publication);
		return -1;
	}
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		publication->context_relocation[i] = context_slice.reachable[i] ?
			compact_context_relocation[context_slice.relocation[i]] :
			PROTOTYPE_INVALID_ID;
	}
	int metadata_status = artifact_publication_metadata_relocate(
		publication, metadata, closure
	);
	struct prototype_operation_graph publication_operations;
	prototype_compile_metadata_operation_graph(
		&publication->metadata, &publication_operations
	);
	int accepted_status = metadata_status == 0 ?
		prototype_judgement_recompute_closure_ranks(
			&publication_operations, &publication->object.judgement
		) : -1;
	int universe_status = accepted_status == 0 ?
		artifact_publication_universe_relocate(publication, closure) : -1;
	int key_status = metadata_status == 0 && universe_status == 0 ?
		prototype_artifact_interface_recompute_keys(
			&publication->interface,
			&publication->object.terms,
			&publication->object.type_declarations,
			&publication->metadata.contexts
		) : -1;
	int status = metadata_status == 0 && accepted_status == 0 &&
		universe_status == 0 && key_status == 0 ?
		0 : -1;
	if (status != 0) {
		fprintf(
			stderr,
			"artifact publication relocation failed metadata=%d accepted=%d "
			"universe=%d keys=%d\n",
			metadata_status,
			accepted_status,
			universe_status,
			key_status
		);
	}
	free(compact_contexts);
	free(compact_substitutions);
	free(compact_types);
	free(compact_constructors);
	free(compact_propositions);
	free(compact_context_relocation);
	artifact_context_slice_free(&context_slice);
	artifact_substitution_slice_free(&substitution_slice);
	if (status != 0) {
		artifact_publication_graph_free(publication);
		return -1;
	}
	return 0;
}

static int artifact_republish_publication_graph(
	struct artifact_publication_graph* target,
	const struct artifact_publication_graph* source
) {
	if (!target || !source) {
		return -1;
	}
	struct artifact_closure_graph closure;
	if (artifact_build_closure_graph(
			&closure,
			&source->interface,
			&source->object.terms,
			&source->object.type_declarations,
			&source->object.judgement,
			&source->object.universe,
			&source->metadata
		) != 0) {
		return -1;
	}
	if (artifact_build_publication_graph(
			target,
			&closure,
			&source->interface,
			&source->metadata
		) != 0) {
		artifact_closure_graph_free(&closure);
		return -1;
	}
	artifact_closure_graph_free(&closure);
	if (prototype_artifact_interface_collect_dependencies(
			&target->interface,
			&target->object.terms,
			&target->object.type_declarations,
			&target->object.judgement
		) != 0) {
		artifact_publication_graph_free(target);
		return -1;
	}
	for (size_t i = 0; i < source->interface.dependency_count; ++i) {
		const struct prototype_artifact_dependency* dependency =
			&source->interface.dependencies[i];
		if (prototype_artifact_interface_add_dependency_in_namespace(
				&target->interface,
				dependency->namespace_symbol_id,
				dependency->name_symbol_id
			) != 0) {
			artifact_publication_graph_free(target);
			return -1;
		}
	}
	return 0;
}

static const char* artifact_optional_symbol_name(
	const struct symbol_table* symbols,
	int symbol_id
) {
	if (symbol_id < 0) {
		return "-";
	}
	return symbol_to_string(symbols, symbol_id);
}

static int write_artifact_operation_graph_section(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	const struct artifact_context_slice* context_slice
) {
	if (!stream || !symbols || !type_declarations || !judgement ||
		!metadata || !context_slice) {
		return -1;
	}
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph_const(metadata, &graph);
	size_t operation_count = prototype_operation_graph_count(&graph);
	size_t case_count = prototype_operation_graph_case_count(&graph);
	size_t fold_clause_count = graph.fold_clause_count;
	size_t obligation_count = metadata ?
		prototype_verification_db_count(&metadata->verification) : 0;
	fprintf(stream, "SECTION operation_graph\n");
	fprintf(
		stream,
		"compile_policy %d %d %s %u %u %u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64
		" %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
		metadata ? metadata->compile_policy : PROTOTYPE_COMPILE_POLICY_HYBRID,
		metadata ? metadata->definition_thunk_policy :
			PROTOTYPE_DEFINITION_THUNK_IMPLICIT,
		metadata ? artifact_optional_symbol_name(
			symbols, metadata->selected_entry_symbol_id
		) : "-",
		metadata ? metadata->selected_entry_term : PROTOTYPE_INVALID_ID,
		metadata ? metadata->selected_entry_classifier : PROTOTYPE_INVALID_ID,
		metadata ? metadata->selected_entry_operation : PROTOTYPE_INVALID_ID,
		metadata ? metadata->required_runtime_capabilities : 0,
		metadata ? metadata->normalization_step_limit : 0,
		metadata ? metadata->normalization_steps_used : 0,
		metadata ? metadata->solver_step_limit : 0,
		metadata ? metadata->solver_steps_used : 0,
		metadata ? metadata->solver_exhausted : 0,
		metadata ? metadata->solver_constraint_count : 0,
		metadata ? metadata->solver_solved_count : 0,
		metadata ? metadata->solver_residual_count : 0,
		metadata ? metadata->solver_incomplete_count : 0
	);
	fprintf(stream, "contexts %zu\n", context_slice->context_count);
	for (size_t i = 0; i < metadata->contexts.context_count; ++i) {
		if (!context_slice->reachable[i]) {
			continue;
		}
		const struct prototype_context* context =
			&metadata->contexts.contexts[i];
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		if (classifier == PROTOTYPE_INVALID_ID &&
			classifier_variable != PROTOTYPE_INVALID_ID) {
			const struct prototype_operation_node* classifier_operation =
				prototype_operation_graph_get(
					&graph, classifier_variable
				);
			if (!classifier_operation ||
				classifier_operation->classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			classifier = classifier_operation->classifier;
		}
		if (i != 0 && classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		fprintf(
			stream,
			"context %zu %u %u %u %u\n",
			(size_t)context_slice->relocation[i],
			context->parent == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID :
				artifact_context_slice_relocate(
					context_slice, context->parent
				),
			context->binding_id,
			classifier,
			context->depth
		);
	}
	size_t substitution_count = metadata ?
		metadata->substitutions.substitution_count : 0;
	fprintf(stream, "substitutions %zu\n", substitution_count);
	for (size_t i = 0; i < substitution_count; ++i) {
		const struct prototype_substitution* substitution =
			&metadata->substitutions.substitutions[i];
		fprintf(
			stream,
			"substitution %zu %d %u %u %u %u %u %u\n",
			i,
			substitution->kind,
			artifact_context_slice_relocate(
				context_slice, substitution->source_context
			),
			artifact_context_slice_relocate(
				context_slice, substitution->target_context
			),
			substitution->first,
			substitution->second,
			substitution->term,
			substitution->term_classifier
		);
	}
	fprintf(stream, "operations %zu\n", operation_count);
	for (size_t i = 0; i < operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(&graph, (uint32_t)i);
		if (!operation) {
			return -1;
		}
		const char* source_name = artifact_optional_symbol_name(
			symbols, operation->source_symbol_id
		);
		const char* binder_name = artifact_optional_symbol_name(
			symbols, operation->binder_symbol_id
		);
		if (!source_name || !binder_name) {
			return -1;
		}
		fprintf(
			stream,
			"operation %zu %d %d %d %u %u %u %s %s %u %u %u %u %u %u %u"
			" %u %u %u %u %u %u %u %u %u\n",
			i,
			operation->tag,
			operation->polarity,
			operation->application_role,
			operation->core_term,
			operation->known_classifier,
			operation->classifier,
			source_name,
			binder_name,
			operation->referenced_ast_binder_id,
			operation->binding_id,
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee,
			operation->binder_classifier,
			operation->fold_return_ast_binder_id,
			operation->fold_return_binder_id,
			operation->fold_return_operation,
			operation->implicit_effect_row_count,
			operation->first_case,
			operation->case_count,
			operation->first_fold_clause,
			operation->fold_clause_count,
			artifact_context_slice_relocate(
				context_slice, operation->context_id
			)
		);
		fprintf(stream, "operation_rows %zu", i);
		for (uint32_t j = 0; j < operation->implicit_effect_row_count; ++j) {
			fprintf(stream, " %u", operation->implicit_effect_row_binders[j]);
		}
		fprintf(stream, "\n");
	}
	fprintf(stream, "operation_fold_clauses %zu\n", fold_clause_count);
	for (size_t i = 0; i < fold_clause_count; ++i) {
		const struct prototype_operation_computation_fold_clause* clause =
			prototype_operation_graph_get_fold_clause(&graph, (uint32_t)i);
		if (!clause) {
			return -1;
		}
		fprintf(
			stream,
			"operation_fold_clause %zu %u %u %u %u %u %u %u %u\n",
			i,
			clause->operation_operation,
			clause->body_operation,
			clause->clause_operation,
			artifact_context_slice_relocate(context_slice, clause->context_id),
			clause->argument_ast_binder_id,
			clause->argument_binder_id,
			clause->continuation_ast_binder_id,
			clause->continuation_binder_id
		);
	}
	fprintf(stream, "operation_cases %zu\n", case_count);
	for (size_t i = 0; i < case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(&graph, (uint32_t)i);
		if (!operation_case) {
			return -1;
		}
		const char* label = artifact_optional_symbol_name(
			symbols, operation_case->case_label_symbol_id
		);
		if (!label) {
			return -1;
		}
		fprintf(
			stream,
			"operation_case %zu %u %u %u %u %s\n",
			i,
			operation_case->body_operation,
			artifact_context_slice_relocate(
				context_slice, operation_case->context_id
			),
			operation_case->constructor_owner,
			operation_case->constructor_id,
			label
		);
		fprintf(stream, "operation_case_binders %zu %u", i, operation_case->binder_count);
		for (uint32_t j = 0; j < operation_case->binder_count; ++j) {
			fprintf(stream, " %u", operation_case->ast_binder_ids[j]);
		}
		fprintf(stream, "\n");
	}
	size_t effect_constraint_count = metadata ?
		metadata->effect_constraint_count : 0;
	fprintf(stream, "effect_constraints %zu\n", effect_constraint_count);
	for (size_t i = 0; i < effect_constraint_count; ++i) {
		const struct prototype_operation_effect_constraint* constraint =
			&metadata->effect_constraints[i];
		fprintf(
			stream,
			"effect_constraint %zu %d %d %u %u %u %u\n",
			i,
			constraint->kind,
			constraint->state,
			constraint->operation,
			constraint->result_row,
			constraint->left_row,
			constraint->right_row
		);
	}
	fprintf(stream, "verification_obligations %zu\n", obligation_count);
	for (size_t i = 0; i < obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			prototype_verification_db_get(&metadata->verification, (uint32_t)i);
		if (!obligation) {
			return -1;
		}
		fprintf(
			stream,
			"verification %zu %d %d %u %u %u %u %u %u %u %u %d %u\n",
			i,
			obligation->kind,
			obligation->state,
			obligation->operation,
			obligation->core_term,
			obligation->computation_operation,
			obligation->continuation_operation,
			obligation->continuation_binder_id,
			obligation->input_classifier,
			obligation->classifier_family,
			obligation->effect_row,
			obligation->normalization_profile,
			obligation->schema_version
		);
	}
	return fprintf(stream, "END operation_graph\n") < 0 ? -1 : 0;
}

static int prototype_artifact_write_text_body(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}
	(void)asts;

	fprintf(
		stream,
		"A_PROGRAM_ARTIFACT %d %s\n",
		PROTOTYPE_ARTIFACT_FORMAT_VERSION,
		PROTOTYPE_ARTIFACT_CALCULUS_FINGERPRINT
	);
	fprintf(stream, "SECTION interface\n");
	size_t present_interface_type_expr_count = 0;
	size_t present_interface_parameter_count = 0;
	size_t present_interface_field_ref_count = 0;
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (artifact_type_expr_present(&interface->type_exprs[i])) {
			present_interface_type_expr_count++;
		}
	}
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		if (artifact_interface_parameter_present(&interface->type_parameters[i])) {
			present_interface_parameter_count++;
		}
	}
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (interface->constructor_field_type_exprs[i] != PROTOTYPE_INVALID_ID) {
			present_interface_field_ref_count++;
		}
	}
	fprintf(
		stream,
		"interface_type_exprs slots %zu type_exprs %zu\n",
		interface->type_expr_count,
		present_interface_type_expr_count
	);
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		if (!artifact_type_expr_present(&interface->type_exprs[i])) {
			continue;
		}
		if (write_artifact_type_expr(
				stream,
				symbols,
				(uint32_t)i,
				&interface->type_exprs[i]
			) != 0) {
			return -1;
		}
	}
	fprintf(
		stream,
		"interface_type_parameters slots %zu parameters %zu\n",
		interface->type_parameter_count,
		present_interface_parameter_count
	);
	for (size_t i = 0; i < interface->type_parameter_count; ++i) {
		const struct prototype_artifact_type_parameter_export* parameter =
			&interface->type_parameters[i];
		if (!artifact_interface_parameter_present(parameter)) {
			continue;
		}
		fprintf(
			stream,
			"interface_type_parameter %zu %u %s %u\n",
			i,
			parameter->binding_id,
			symbol_to_string(symbols, parameter->name_symbol_id),
			parameter->type_expr
		);
	}
	fprintf(
		stream,
		"interface_constructor_field_refs slots %zu field_refs %zu\n",
		interface->constructor_field_type_expr_count,
		present_interface_field_ref_count
	);
	for (size_t i = 0; i < interface->constructor_field_type_expr_count; ++i) {
		if (interface->constructor_field_type_exprs[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		fprintf(
			stream,
			"interface_constructor_field_ref %zu %u\n",
			i,
			interface->constructor_field_type_exprs[i]
		);
	}
	fprintf(stream, "term_exports %zu\n", interface->term_export_count);
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		uint32_t source_claim_id = PROTOTYPE_INVALID_ID;
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		if (export->source_evidence.kind ==
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM) {
			if (!prototype_judgement_claim_get(
					judgement, export->source_evidence.id
				)) {
				return -1;
			}
			source_claim_id = export->source_evidence.id;
		} else if (export->source_evidence.kind !=
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID ||
			export->source_evidence.id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		fprintf(
			stream,
			"term %s %u %u %d ",
			name,
			export->local_term,
			export->classifier,
			export->transparency
		);
		print_artifact_key(stream, &export->canonical_key);
		fprintf(stream, " ");
		print_artifact_key(stream, &export->classifier_key);
		fprintf(
			stream,
			" namespace %s operation %u claim %u\n",
			namespace_name,
			export->operation,
			source_claim_id
		);
	}

	fprintf(stream, "type_exports %zu\n", interface->type_export_count);
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export = &interface->type_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		const char* namespace_name = symbol_to_string(symbols, export->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(
			stream,
			"type %s %u %u %u %u %u %u %u ",
			name,
			export->local_type_id,
			export->core_representation_anchor_type_id,
			export->formation_classifier,
			export->first_parameter,
			export->parameter_count,
			export->first_constructor_export,
			export->constructor_count
		);
		print_artifact_type_code_shape_key(stream, &export->code_shape_key);
		fprintf(stream, " namespace %s\n", namespace_name);
	}

	fprintf(stream, "constructor_exports %zu\n", interface->constructor_export_count);
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		const char* name = symbol_to_string(symbols, export->name_symbol_id);
		if (!name) {
			return -1;
		}
		fprintf(
			stream,
			"constructor %u %s %u %u %u %u\n",
			export->type_export_index,
			name,
			export->ordinal,
			export->readback_first_field_type,
			export->readback_field_count,
			export->curried_classifier_cache
		);
	}

	fprintf(stream, "identity_roots %zu\n", interface->identity_root_count);
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		fprintf(
			stream,
			"identity_root %zu %u %u %u %d\n",
			i,
			root->source_type_claim_id,
			root->identity_family_has_type_claim_id,
			root->witness_has_type_claim_id,
			root->computation_rule
		);
	}

	fprintf(stream, "dependencies %zu\n", interface->dependency_count);
	for (size_t i = 0; i < interface->dependency_count; ++i) {
		const struct prototype_artifact_dependency* dependency = &interface->dependencies[i];
		const char* name = symbol_to_string(symbols, dependency->name_symbol_id);
		const char* namespace_name = dependency->namespace_symbol_id < 0 ?
			"-" :
			symbol_to_string(symbols, dependency->namespace_symbol_id);
		if (!name || !namespace_name) {
			return -1;
		}
		fprintf(stream, "dependency %s namespace %s\n", name, namespace_name);
	}
	fprintf(stream, "END interface\n");

	struct artifact_context_slice context_slice;
	if (artifact_context_slice_init(
		&context_slice,
		type_declarations,
		judgement,
		metadata,
		NULL
	) != 0) {
		return -1;
	}
	if (write_artifact_graph_section(
			stream,
			symbols,
			terms,
			type_declarations,
			judgement,
			&context_slice
		) != 0) {
		artifact_context_slice_free(&context_slice);
		return -1;
	}
	if (write_artifact_operation_graph_section(
		stream,
		symbols,
		type_declarations,
		judgement,
		metadata,
		&context_slice
	) != 0) {
		artifact_context_slice_free(&context_slice);
		return -1;
	}
	artifact_context_slice_free(&context_slice);
	if (write_artifact_universe_section(stream, universe) != 0) {
		return -1;
	}
	if (write_artifact_debug_section(stream, symbols, interface, NULL) != 0) {
		return -1;
	}
	return write_artifact_relocation_section(
		stream,
		symbols,
		interface,
		terms,
		type_declarations,
		judgement
	);
}

int prototype_artifact_write_text(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts,
	const struct prototype_compile_metadata* metadata
) {
	if (!stream || !symbols || !interface || !terms || !type_declarations || !judgement ||
		!universe) {
		return -1;
	}
	if (metadata &&
		(prototype_context_db_validate(&metadata->contexts, terms) != 0 ||
		 prototype_substitution_db_validate(
			&metadata->substitutions, &metadata->contexts, terms
		 ) != 0 ||
		 prototype_constructor_telescopes_validate(
			type_declarations, &metadata->contexts, terms
		 ) != 0)) {
		fprintf(stderr, "artifact source graph validation failed\n");
		return -1;
	}
	struct artifact_closure_graph closure_graph;
	if (artifact_build_closure_graph(
			&closure_graph,
			interface,
			terms,
			type_declarations,
			judgement,
			universe,
		metadata
	) != 0) {
		fprintf(stderr, "artifact closure construction failed\n");
		return -1;
	}
	struct artifact_publication_graph publication;
	if (artifact_build_publication_graph(
			&publication,
			&closure_graph,
			interface,
		metadata
	) != 0) {
		fprintf(stderr, "artifact dense publication construction failed\n");
		artifact_closure_graph_free(&closure_graph);
		return -1;
	}
	/* Dependencies are part of the public rooted graph. Recompute them after
	 * dense publication so unused accepted work cannot leak imports, while
	 * external references reachable only through proof evidence are retained. */
	if (prototype_artifact_interface_collect_dependencies(
			&publication.interface,
			&publication.object.terms,
			&publication.object.type_declarations,
			&publication.object.judgement
		) != 0) {
		fprintf(stderr, "artifact publication dependency collection failed\n");
		artifact_publication_graph_free(&publication);
		artifact_closure_graph_free(&closure_graph);
		return -1;
	}
	if (asts) {
		for (size_t i = 0; i < publication.object.terms.term_count; ++i) {
			const struct prototype_term* term = &publication.object.terms.terms[i];
			if (term->tag != PROTOTYPE_TERM_TYPE_DECLARATION) {
				continue;
			}
			for (size_t j = 0; j < interface->dependency_count; ++j) {
				const struct prototype_artifact_dependency* dependency =
					&interface->dependencies[j];
				if (dependency->namespace_symbol_id !=
						term->as.type_declaration.identity.namespace_symbol_id ||
					dependency->name_symbol_id !=
						term->as.type_declaration.identity.name_symbol_id) {
					continue;
				}
				if (prototype_artifact_interface_add_dependency_in_namespace(
						&publication.interface,
						dependency->namespace_symbol_id,
						dependency->name_symbol_id
					) != 0) {
					fprintf(stderr, "artifact rooted type dependency collection failed\n");
					artifact_publication_graph_free(&publication);
					artifact_closure_graph_free(&closure_graph);
					return -1;
				}
				break;
			}
		}
		for (size_t i = 0; i < asts->import_count; ++i) {
			if (prototype_artifact_interface_add_dependency(
					&publication.interface, asts->imports[i].name_symbol_id
				) != 0) {
				fprintf(stderr, "artifact source import collection failed\n");
				artifact_publication_graph_free(&publication);
				artifact_closure_graph_free(&closure_graph);
				return -1;
			}
		}
	}
	if (publication.object.terms.term_count != 0 ||
		publication.object.type_declarations.type_count != 0 ||
		publication.object.judgement.claim_count != 0) {
		struct artifact_publication_graph normalized_publication;
		if (artifact_republish_publication_graph(
				&normalized_publication, &publication
			) != 0) {
			fprintf(stderr, "artifact publication normalization failed\n");
			artifact_publication_graph_free(&publication);
			artifact_closure_graph_free(&closure_graph);
			return -1;
		}
		artifact_publication_graph_free(&publication);
		publication = normalized_publication;
	}
	int status = prototype_artifact_write_text_body(
		stream,
		symbols,
		&publication.interface,
		&publication.object.terms,
		&publication.object.type_declarations,
		&publication.object.judgement,
		&publication.object.universe,
		asts,
		&publication.metadata
	);
	if (status != 0) {
		fprintf(stderr, "artifact dense publication write failed\n");
	}
	artifact_publication_graph_free(&publication);
	artifact_closure_graph_free(&closure_graph);
	return status;
}
