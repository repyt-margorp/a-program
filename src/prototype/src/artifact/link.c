#include "a_program/artifact/interface.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artifact_internal.h"
#include "artifact_graph_internal.h"
#include "../internal/ast_common.h"

static uint32_t offset_artifact_id(uint32_t id, uint32_t offset) {
	return id == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : id + offset;
}

static uint32_t relocate_artifact_authority_id(
	int authority_kind,
	uint32_t authority_id,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	uint32_t operation_offset
) {
	if (authority_id == PROTOTYPE_INVALID_ID) {
		return authority_id;
	}
	if (authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
		return operation_offset == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID :
			offset_artifact_id(authority_id, operation_offset);
	}
	if (authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_EXPORT ||
		authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return authority_id;
	}
	return authority_id < term_relocation_count ?
		term_relocation[authority_id] : PROTOTYPE_INVALID_ID;
}

int prototype_internal_canonicalize_type_view_core_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts
) {
	if (!terms || !type_declarations || !contexts) {
		return -1;
	}
	if (type_declarations->representations_dirty) {
		if (prototype_type_declaration_rebuild_representations(
				terms, type_declarations, contexts
			) != 0) {
			return -1;
		}
		if (prototype_term_rebind_type_former_anchors(terms, type_declarations) != 0) {
			return -1;
		}
	}
	size_t original_term_count = terms->term_count;
	for (size_t i = 0; i < original_term_count; ++i) {
		if (terms->terms[i].tag != PROTOTYPE_TERM_TYPE_VIEW) {
			continue;
		}
		uint32_t type_id;
		uint32_t args[16];
		uint32_t arg_count;
		uint32_t canonical_view;
		if (prototype_term_type_instance_info(
				terms,
				(uint32_t)i,
				&type_id,
				args,
				&arg_count
			) != 0 ||
			prototype_term_type_instance_make(
				terms,
				type_declarations,
				type_id,
				args,
				arg_count,
				&canonical_view
			) != 0 ||
			canonical_view >= terms->term_count ||
			terms->terms[canonical_view].tag != PROTOTYPE_TERM_TYPE_VIEW) {
			return -1;
		}
		/* type_instance_make may grow TermDB and relocate terms. Reacquire the
		 * slot by index rather than writing through a pointer captured above. */
		terms->terms[i].as.type_view.core =
			terms->terms[canonical_view].as.type_view.core;
	}
	return 0;
}

int prototype_internal_artifact_find_existing_term_by_canonical_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	const struct prototype_term_canonical_key* key,
	uint32_t appended_term,
	uint32_t* p_term_id
) {
	if (!terms || !type_declarations || !key || !p_term_id ||
		appended_term >= terms->term_count) {
		return -1;
	}
	if (!canonical_key_is_cross_artifact_linkable(key)) {
		return 0;
	}
	for (uint32_t i = 0; i < old_term_count; ++i) {
		struct prototype_term_canonical_key candidate;
		int same_term = 0;
		if (terms->terms[i].tag == 0) {
			continue;
		}
		if (prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				i,
				&candidate
			) != 0) {
			return -1;
		}
		if (!canonical_keys_equal(&candidate, key)) {
			continue;
		}
			if (prototype_term_view_shape_equal_for_link(
					terms,
					type_declarations,
					i,
					terms,
				type_declarations,
				appended_term,
				&same_term
			) != 0) {
			return -1;
		}
		if (same_term) {
			*p_term_id = i;
			return 1;
		}
	}
	return 0;
}

static int canonicalize_constructor_owner_ref(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	uint32_t* p_owner
) {
	if (!terms || !type_declarations || !p_owner) {
		return -1;
	}
	if (*p_owner == PROTOTYPE_INVALID_ID || *p_owner < old_term_count) {
		return 0;
	}
	if (*p_owner >= terms->term_count) {
		return -1;
	}
	uint32_t search_term_count = old_term_count;
	if (search_term_count == 0 || search_term_count > *p_owner) {
		search_term_count = *p_owner;
	}

	struct prototype_term_canonical_key key;
	if (prototype_term_canonical_key_with_types(
			terms,
			type_declarations,
			*p_owner,
			&key
		) != 0) {
		return -1;
	}
	uint32_t existing_owner;
	int found_existing = prototype_internal_artifact_find_existing_term_by_canonical_key(
		terms,
		type_declarations,
		search_term_count,
		&key,
		*p_owner,
		&existing_owner
	);
	if (found_existing < 0) {
		return -1;
	}
	if (found_existing > 0) {
		*p_owner = existing_owner;
	}
	return 0;
}

int prototype_internal_canonicalize_constructor_owner_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count
) {
	if (!terms || !type_declarations) {
		return -1;
	}
	for (size_t i = old_term_count; i < terms->term_count; ++i) {
		struct prototype_term* term = &terms->terms[i];
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
			continue;
		}
		if (canonicalize_constructor_owner_ref(
				terms,
				type_declarations,
				old_term_count,
				&term->as.constructor.owner
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < terms->case_count; ++i) {
		struct prototype_match_case* match_case = &terms->cases[i];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			match_case->constructor_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (canonicalize_constructor_owner_ref(
				terms,
				type_declarations,
				old_term_count,
				&match_case->constructor_owner
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int relocate_artifact_type_expr(
	struct prototype_type_expr* expr,
	const uint32_t* expr_relocation,
	size_t expr_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
	uint32_t universe_offset
) {
	if (!expr || !expr_relocation || !binding_relocation) {
		return -1;
	}
#define RELOCATE_TYPE_EXPR_REF(field) \
	do { \
		if ((field) >= expr_relocation_count || \
			expr_relocation[(field)] == PROTOTYPE_INVALID_ID) { \
			return -1; \
		} \
		(field) = expr_relocation[(field)]; \
	} while (0)
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			expr->as.universe_var.level_var += universe_offset;
			break;
		case PROTOTYPE_TYPE_EXPR_VAR:
			if (expr->as.var.binding_id >= binding_relocation_count ||
				binding_relocation[expr->as.var.binding_id] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			expr->as.var.binding_id = binding_relocation[expr->as.var.binding_id];
			break;
		case PROTOTYPE_TYPE_EXPR_APP:
			RELOCATE_TYPE_EXPR_REF(expr->as.app.function);
			RELOCATE_TYPE_EXPR_REF(expr->as.app.argument);
			break;
		case PROTOTYPE_TYPE_EXPR_ARROW:
			RELOCATE_TYPE_EXPR_REF(expr->as.arrow.domain);
			RELOCATE_TYPE_EXPR_REF(expr->as.arrow.codomain);
			break;
		case PROTOTYPE_TYPE_EXPR_PI:
			if (expr->as.pi.binding_id >= binding_relocation_count ||
				binding_relocation[expr->as.pi.binding_id] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			expr->as.pi.binding_id = binding_relocation[expr->as.pi.binding_id];
			RELOCATE_TYPE_EXPR_REF(expr->as.pi.domain);
			RELOCATE_TYPE_EXPR_REF(expr->as.pi.codomain);
			break;
		default:
			break;
	}
	return 0;
#undef RELOCATE_TYPE_EXPR_REF
}

static int artifact_compare_u32(uint32_t left, uint32_t right) {
	return left < right ? -1 : (left > right ? 1 : 0);
}

static uint32_t artifact_relocated_semantic_action(
	int kind,
	uint32_t id,
	const uint32_t* substitution_relocation,
	size_t substitution_count
) {
	if (kind != PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
		return id;
	}
	return id < substitution_count ?
		substitution_relocation[id] : PROTOTYPE_INVALID_ID;
}

static uint32_t artifact_relocated_rule_word(
	const struct prototype_judgement_derivation* derivation,
	uint32_t word,
	const uint32_t* term_relocation,
	size_t term_count
) {
	uint32_t value = derivation->rule_data.words[word];
	if ((derivation->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM && word < 2) ||
		((derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
		  derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) && word == 0)) {
		return value < term_count ?
			term_relocation[value] : PROTOTYPE_INVALID_ID;
	}
	return value;
}

static int artifact_compare_relocated_derivations(
	const struct prototype_judgement_derivation* left,
	const struct prototype_judgement_derivation* right,
	const uint32_t* proposition_relocation,
	const uint32_t* claim_relocation,
	const uint32_t* substitution_relocation,
	size_t substitution_count,
	const uint32_t* term_relocation,
	size_t term_count
) {
#define COMPARE_FIELD(lhs, rhs) do { \
	int field_comparison = artifact_compare_u32((uint32_t)(lhs), (uint32_t)(rhs)); \
	if (field_comparison != 0) { \
		return field_comparison; \
	} \
} while (0)
	COMPARE_FIELD(left->proof_kind, right->proof_kind);
	COMPARE_FIELD(
		claim_relocation[left->conclusion_claim_id],
		claim_relocation[right->conclusion_claim_id]
	);
	for (uint32_t i = 0; i < 4; ++i) {
		COMPARE_FIELD(
			artifact_relocated_rule_word(left, i, term_relocation, term_count),
			artifact_relocated_rule_word(right, i, term_relocation, term_count)
		);
	}
	COMPARE_FIELD(left->semantic_action_kind, right->semantic_action_kind);
	COMPARE_FIELD(
		artifact_relocated_semantic_action(
			left->semantic_action_kind,
			left->semantic_action_id,
			substitution_relocation,
			substitution_count
		),
		artifact_relocated_semantic_action(
			right->semantic_action_kind,
			right->semantic_action_id,
			substitution_relocation,
			substitution_count
		)
	);
	COMPARE_FIELD(left->premise_count, right->premise_count);
	for (uint32_t i = 0; i < left->premise_count; ++i) {
		const struct prototype_judgement_premise_edge* left_premise =
			&left->premises[i];
		const struct prototype_judgement_premise_edge* right_premise =
			&right->premises[i];
		COMPARE_FIELD(
			left_premise->claim_id != PROTOTYPE_INVALID_ID,
			right_premise->claim_id != PROTOTYPE_INVALID_ID
		);
		COMPARE_FIELD(
			left_premise->claim_id != PROTOTYPE_INVALID_ID ?
				claim_relocation[left_premise->claim_id] :
				proposition_relocation[left_premise->scoped_proposition_id],
			right_premise->claim_id != PROTOTYPE_INVALID_ID ?
				claim_relocation[right_premise->claim_id] :
				proposition_relocation[right_premise->scoped_proposition_id]
		);
		COMPARE_FIELD(
			left_premise->semantic_action_kind,
			right_premise->semantic_action_kind
		);
		COMPARE_FIELD(
			artifact_relocated_semantic_action(
				left_premise->semantic_action_kind,
				left_premise->semantic_action_id,
				substitution_relocation,
				substitution_count
			),
			artifact_relocated_semantic_action(
				right_premise->semantic_action_kind,
				right_premise->semantic_action_id,
				substitution_relocation,
				substitution_count
			)
		);
	}
#undef COMPARE_FIELD
	return 0;
}

static int artifact_append_accepted_judgement(
	struct prototype_judgement_db* target,
	const struct prototype_judgement_db* source,
	const struct prototype_context_db* source_contexts,
	const uint32_t* context_relocation,
	const struct prototype_substitution_db* source_substitutions,
	const uint32_t* substitution_relocation,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	uint32_t operation_offset,
	uint32_t* proposition_relocation,
	uint32_t* claim_relocation,
	const struct artifact_append_order* order
) {
	if (!target || !source || !source_contexts || !context_relocation ||
		!source_substitutions || !substitution_relocation || !term_relocation ||
		!proposition_relocation || !claim_relocation ||
		prototype_judgement_db_rebuild_index(target) != 0) {
		return -1;
	}
	for (size_t i = 0; i < source->proposition_count; ++i) {
		proposition_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	size_t proposition_count = order ?
		order->proposition_count : source->proposition_count;
	for (size_t position = 0; position < proposition_count; ++position) {
		uint32_t i = order ? order->propositions[position] : (uint32_t)position;
		if (i >= source->proposition_count) {
			return -1;
		}
		const struct prototype_judgement_proposition* source_proposition =
			prototype_judgement_proposition_get(source, i);
		if (!source_proposition) {
			continue;
		}
		struct prototype_judgement_proposition proposition =
			*source_proposition;
		if (proposition.context_id >= source_contexts->context_count ||
			(proposition.operation_id != PROTOTYPE_INVALID_ID &&
			 operation_offset == PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		if (proposition.subject >= term_relocation_count ||
			proposition.classifier >= term_relocation_count) {
			return -1;
		}
		proposition.subject = term_relocation[proposition.subject];
		proposition.classifier = term_relocation[proposition.classifier];
		proposition.context_id = context_relocation[proposition.context_id];
		proposition.operation_id = proposition.operation_id ==
			PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID :
			offset_artifact_id(proposition.operation_id, operation_offset);
		proposition.authority_id = relocate_artifact_authority_id(
			proposition.authority_kind,
			proposition.authority_id,
			term_relocation,
			term_relocation_count,
			operation_offset
		);
		if (proposition.authority_id == PROTOTYPE_INVALID_ID &&
			proposition.authority_kind !=
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		proposition.key_hash = 0;
		proposition.hash_next = PROTOTYPE_INVALID_ID;
		if (prototype_judgement_proposition_intern(
				target,
				&proposition,
				&proposition_relocation[i]
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < source->claim_count; ++i) {
		claim_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	size_t claim_count = order ? order->claim_count : source->claim_count;
	for (size_t position = 0; position < claim_count; ++position) {
		uint32_t i = order ? order->claims[position] : (uint32_t)position;
		if (i >= source->claim_count) {
			return -1;
		}
		const struct prototype_judgement_claim* source_claim =
			prototype_judgement_claim_get(source, i);
		if (!source_claim || source_claim->proposition_id >=
				source->proposition_count || proposition_relocation[
				source_claim->proposition_id] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_judgement_claim_intern_exact(
				target,
				proposition_relocation[source_claim->proposition_id],
				&claim_relocation[i]
			) != 0) {
			return -1;
		}
		if (target->claims[claim_relocation[i]].closure_rank ==
				PROTOTYPE_INVALID_ID || source_claim->closure_rank <
				target->claims[claim_relocation[i]].closure_rank) {
			target->claims[claim_relocation[i]].closure_rank =
				source_claim->closure_rank;
		}
	}
	size_t derivation_count = order ?
		order->derivation_count : source->derivation_count;
	size_t derivation_order_capacity = derivation_count == 0 ? 1 : derivation_count;
	uint32_t canonical_derivation_order[derivation_order_capacity];
	for (size_t position = 0; position < derivation_count; ++position) {
		uint32_t derivation_id = order ?
			order->derivations[position] : (uint32_t)position;
		if (derivation_id >= source->derivation_count ||
			!prototype_judgement_derivation_get(source, derivation_id)) {
			return -1;
		}
		canonical_derivation_order[position] = derivation_id;
	}
	if (order) {
		for (size_t i = 1; i < derivation_count; ++i) {
			uint32_t current = canonical_derivation_order[i];
			size_t j = i;
			while (j > 0 && artifact_compare_relocated_derivations(
					prototype_judgement_derivation_get(source, current),
					prototype_judgement_derivation_get(
						source, canonical_derivation_order[j - 1]
					),
					proposition_relocation,
					claim_relocation,
					substitution_relocation,
					source_substitutions->substitution_count,
					term_relocation,
					term_relocation_count
				) < 0) {
				canonical_derivation_order[j] =
					canonical_derivation_order[j - 1];
				--j;
			}
			canonical_derivation_order[j] = current;
		}
	}
	for (size_t position = 0; position < derivation_count; ++position) {
		uint32_t i = canonical_derivation_order[position];
		const struct prototype_judgement_derivation* source_derivation =
			prototype_judgement_derivation_get(source, i);
		if (!source_derivation || source_derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		if (source_derivation->conclusion_claim_id >= source->claim_count ||
			claim_relocation[source_derivation->conclusion_claim_id] ==
				PROTOTYPE_INVALID_ID || source_derivation->premise_count >
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		struct prototype_judgement_premise_edge premises[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		struct prototype_judgement_derivation derivation =
			*source_derivation;
		derivation.conclusion_claim_id =
			claim_relocation[source_derivation->conclusion_claim_id];
		/* v69 still carries this derived cache. The accepted append preserves the
		 * source DAG exactly, so its topological rank remains valid after ID
		 * relocation. v70 removes rank from the wire and recomputes it on read. */
		derivation.closure_rank = source_derivation->closure_rank;
		derivation.premises = premises;
		derivation.key_hash = 0;
		derivation.hash_next = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < derivation.premise_count; ++j) {
			premises[j].claim_id = PROTOTYPE_INVALID_ID;
			premises[j].scoped_proposition_id = PROTOTYPE_INVALID_ID;
			premises[j].semantic_action_kind =
				source_derivation->premises[j].semantic_action_kind;
			premises[j].semantic_action_id =
				source_derivation->premises[j].semantic_action_id;
			if (premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				if (premises[j].semantic_action_id >=
					source_substitutions->substitution_count) {
					return -1;
				}
				premises[j].semantic_action_id = substitution_relocation[
					premises[j].semantic_action_id
				];
			} else if (premises[j].semantic_action_kind !=
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
				premises[j].semantic_action_id != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			if (source_derivation->premises[j].claim_id !=
					PROTOTYPE_INVALID_ID) {
				uint32_t source_claim_id =
					source_derivation->premises[j].claim_id;
				if (source_claim_id >= source->claim_count ||
					claim_relocation[source_claim_id] == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				premises[j].claim_id = claim_relocation[source_claim_id];
			} else {
				uint32_t source_proposition_id =
					source_derivation->premises[j].scoped_proposition_id;
				if (source_proposition_id >= source->proposition_count ||
					proposition_relocation[source_proposition_id] ==
						PROTOTYPE_INVALID_ID) {
					return -1;
				}
				premises[j].scoped_proposition_id =
					proposition_relocation[source_proposition_id];
			}
		}
		if (derivation.proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
			if (derivation.rule_data.induction.match >= term_relocation_count ||
				derivation.rule_data.induction.motive >= term_relocation_count) {
				return -1;
			}
			derivation.rule_data.induction.match = term_relocation[
				derivation.rule_data.induction.match
			];
			derivation.rule_data.induction.motive = term_relocation[
				derivation.rule_data.induction.motive
			];
		} else if (derivation.proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
			derivation.proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
			if (derivation.rule_data.constructor.owner_view >=
				term_relocation_count) {
				return -1;
			}
			derivation.rule_data.constructor.owner_view = term_relocation[
				derivation.rule_data.constructor.owner_view
			];
		}
		if (derivation.semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			if (derivation.semantic_action_id >=
				source_substitutions->substitution_count) {
				return -1;
			}
			derivation.semantic_action_id = substitution_relocation[
				derivation.semantic_action_id
			];
		} else if (derivation.semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
			derivation.semantic_action_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t derivation_id;
		if (prototype_judgement_derivation_intern_exact(
				target, &derivation, &derivation_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int artifact_mark_binding(
	unsigned char* used,
	size_t count,
	uint32_t binding_id
) {
	if (!used || binding_id == PROTOTYPE_INVALID_ID || binding_id >= count) {
		return -1;
	}
	used[binding_id] = 1;
	return 0;
}

static int artifact_build_binding_relocation(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct artifact_append_order* order,
	uint32_t target_first_binding,
	uint32_t* relocation,
	size_t relocation_count,
	uint32_t* p_next_binding
) {
	if (!terms || !type_declarations || !contexts || !relocation ||
		!p_next_binding || relocation_count < terms->next_binding_id) {
		return -1;
	}
	size_t used_count = terms->next_binding_id == 0 ? 1 : terms->next_binding_id;
	unsigned char used[used_count];
	memset(used, 0, sizeof(used));
	for (size_t i = 0; i < relocation_count; ++i) {
		relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < terms->term_count; ++i) {
		const struct prototype_term* term = &terms->terms[i];
		if (term->tag == PROTOTYPE_TERM_VAR) {
			if (artifact_mark_binding(used, terms->next_binding_id,
					term->as.var.binding_id) != 0) {
				return -1;
			}
		} else if (term->tag == PROTOTYPE_TERM_LAMBDA) {
			if (artifact_mark_binding(used, terms->next_binding_id,
					term->as.lambda.binding_id) != 0) {
				return -1;
			}
		} else if (term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			if (artifact_mark_binding(used, terms->next_binding_id,
					term->as.effect_row_var.binding_id) != 0) {
				return -1;
			}
		} else if (term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			if (artifact_mark_binding(used, terms->next_binding_id,
					term->as.effect_row_forall.binding_id) != 0) {
				return -1;
			}
		} else if (term->tag == PROTOTYPE_TERM_MATCH) {
			uint32_t first = term->as.match.first_case;
			uint32_t count = term->as.match.case_count;
			if (first > terms->case_count || count > terms->case_count - first) {
				return -1;
			}
			for (uint32_t j = 0; j < count; ++j) {
				const struct prototype_match_case* match_case =
					&terms->cases[first + j];
				if (match_case->first_binder > terms->case_binder_count ||
					match_case->binder_count > terms->case_binder_count -
						match_case->first_binder) {
					return -1;
				}
				for (uint32_t k = 0; k < match_case->binder_count; ++k) {
					if (artifact_mark_binding(
							used,
							terms->next_binding_id,
							terms->case_binders[
								match_case->first_binder + k
							].binding_id
						) != 0) {
						return -1;
					}
				}
			}
		}
	}
	for (size_t i = 1; i < contexts->context_count; ++i) {
		const struct prototype_context* context = &contexts->contexts[i];
		if (artifact_mark_binding(
				used, terms->next_binding_id, context->binding_id
			) != 0) {
			return -1;
		}
		if (context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE &&
			artifact_mark_binding(
				used,
				terms->next_binding_id,
				context->classifier_ref.variable_id
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->exprs[i];
		uint32_t binding_id = PROTOTYPE_INVALID_ID;
		if (!artifact_type_expr_present(expr)) {
			continue;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_VAR) {
			binding_id = expr->as.var.binding_id;
		} else if (expr->tag == PROTOTYPE_TYPE_EXPR_PI) {
			binding_id = expr->as.pi.binding_id;
		}
		if (binding_id != PROTOTYPE_INVALID_ID && artifact_mark_binding(
				used, terms->next_binding_id, binding_id
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->parameter_declarations[i];
		if (artifact_parameter_present(parameter) && artifact_mark_binding(
				used, terms->next_binding_id, parameter->binding_id
			) != 0) {
			return -1;
		}
	}
	uint32_t next = target_first_binding;
	if (order) {
#define ASSIGN_BINDING(binding) do { \
	uint32_t assign_binding_id = (binding); \
	if (assign_binding_id < terms->next_binding_id && used[assign_binding_id] && \
		relocation[assign_binding_id] == PROTOTYPE_INVALID_ID) { \
		relocation[assign_binding_id] = next++; \
	} \
} while (0)
		for (size_t position = 0; position < order->term_count; ++position) {
			uint32_t term_id = order->terms[position];
			if (term_id >= terms->term_count) {
				return -1;
			}
			const struct prototype_term* term = &terms->terms[term_id];
			if (term->tag == PROTOTYPE_TERM_VAR) {
				ASSIGN_BINDING(term->as.var.binding_id);
			} else if (term->tag == PROTOTYPE_TERM_LAMBDA) {
				ASSIGN_BINDING(term->as.lambda.binding_id);
			} else if (term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
				ASSIGN_BINDING(term->as.effect_row_var.binding_id);
			} else if (term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
				ASSIGN_BINDING(term->as.effect_row_forall.binding_id);
			} else if (term->tag == PROTOTYPE_TERM_MATCH) {
				for (uint32_t j = 0; j < term->as.match.case_count; ++j) {
					const struct prototype_match_case* match_case =
						&terms->cases[term->as.match.first_case + j];
					for (uint32_t k = 0; k < match_case->binder_count; ++k) {
						ASSIGN_BINDING(terms->case_binders[
							match_case->first_binder + k
						].binding_id);
					}
				}
			}
		}
		for (size_t position = 0; position < order->type_expr_count; ++position) {
			uint32_t expr_id = order->type_exprs[position];
			if (expr_id >= type_declarations->expr_count) {
				return -1;
			}
			const struct prototype_type_expr* expr =
				&type_declarations->exprs[expr_id];
			if (expr->tag == PROTOTYPE_TYPE_EXPR_VAR) {
				ASSIGN_BINDING(expr->as.var.binding_id);
			} else if (expr->tag == PROTOTYPE_TYPE_EXPR_PI) {
				ASSIGN_BINDING(expr->as.pi.binding_id);
			}
		}
		for (size_t position = 0; position < order->parameter_count; ++position) {
			uint32_t parameter_id = order->parameters[position];
			if (parameter_id >= type_declarations->parameter_count) {
				return -1;
			}
			ASSIGN_BINDING(
				type_declarations->parameter_declarations[parameter_id].binding_id
			);
		}
#undef ASSIGN_BINDING
	}
	for (uint32_t i = 0; i < terms->next_binding_id; ++i) {
		if (used[i] && relocation[i] == PROTOTYPE_INVALID_ID) {
			relocation[i] = next++;
		}
	}
	*p_next_binding = next;
	return 0;
}

int prototype_internal_artifact_append_graph_ordered(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_substitution_db* target_substitutions,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement,
	const struct prototype_context_db* source_contexts,
	const struct prototype_substitution_db* source_substitutions,
	uint32_t operation_offset,
	uint32_t* term_relocation,
	size_t term_relocation_capacity,
	uint32_t* context_relocation,
	size_t context_relocation_capacity,
	struct prototype_artifact_graph_relocation* additional_relocation,
	int canonicalize_link_references,
	const struct artifact_append_order* order
) {
	if (!appended_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !target_contexts || !target_substitutions ||
		!source_interface ||
		!source_terms || !source_type_declarations || !source_judgement ||
		!source_contexts || !source_substitutions || !term_relocation ||
		!context_relocation || source_terms->term_count >
			term_relocation_capacity || source_contexts->context_count >
			context_relocation_capacity) {
		return -1;
	}
	if (additional_relocation &&
		((additional_relocation->binding_ids &&
		  additional_relocation->binding_id_capacity <
			source_terms->next_binding_id) ||
		 (additional_relocation->type_ids &&
		  additional_relocation->type_id_capacity <
			source_type_declarations->type_count) ||
		 (additional_relocation->type_expr_ids &&
		  additional_relocation->type_expr_id_capacity <
			source_type_declarations->expr_count) ||
		 (additional_relocation->parameter_ids &&
		  additional_relocation->parameter_id_capacity <
			source_type_declarations->parameter_count) ||
		 (additional_relocation->constructor_ids &&
		  additional_relocation->constructor_id_capacity <
			source_type_declarations->constructor_count) ||
		 (additional_relocation->field_type_ids &&
		  additional_relocation->field_type_id_capacity <
			source_type_declarations->readback_field_type_count) ||
		 (additional_relocation->proposition_ids &&
		  additional_relocation->proposition_id_capacity <
			source_judgement->proposition_count) ||
		 (additional_relocation->claim_ids &&
		  additional_relocation->claim_id_capacity < source_judgement->claim_count) ||
		 (additional_relocation->substitution_ids &&
		  additional_relocation->substitution_id_capacity <
			source_substitutions->substitution_count))) {
		return -1;
	}

	uint32_t type_offset = (uint32_t)target_type_declarations->type_count;
	size_t type_relocation_count = source_type_declarations->type_count == 0 ?
		1 : source_type_declarations->type_count;
	uint32_t type_relocation[type_relocation_count];
	size_t expr_relocation_count = source_type_declarations->expr_count == 0 ?
		1 : source_type_declarations->expr_count;
	size_t field_relocation_count =
		source_type_declarations->readback_field_type_count == 0 ?
		1 : source_type_declarations->readback_field_type_count;
	size_t parameter_relocation_count =
		source_type_declarations->parameter_count == 0 ?
		1 : source_type_declarations->parameter_count;
	size_t constructor_relocation_count =
		source_type_declarations->constructor_count == 0 ?
		1 : source_type_declarations->constructor_count;
	uint32_t expr_relocation[expr_relocation_count];
	uint32_t field_relocation[field_relocation_count];
	uint32_t parameter_relocation[parameter_relocation_count];
	uint32_t constructor_relocation[constructor_relocation_count];
	uint32_t field_boundary_relocation[
		source_type_declarations->readback_field_type_count + 1
	];
	uint32_t parameter_boundary_relocation[
		source_type_declarations->parameter_count + 1
	];
	uint32_t constructor_boundary_relocation[
		source_type_declarations->constructor_count + 1
	];
	size_t binding_relocation_count = source_terms->next_binding_id == 0 ?
		1 : source_terms->next_binding_id;
	uint32_t binding_relocation[binding_relocation_count];
	uint32_t next_binding_id;
	uint32_t universe_offset = target_type_declarations->next_level_var;
	size_t proposition_relocation_count = source_judgement->proposition_count == 0 ?
		1 : source_judgement->proposition_count;
	size_t claim_relocation_count = source_judgement->claim_count == 0 ?
		1 : source_judgement->claim_count;
	uint32_t proposition_relocation[proposition_relocation_count];
	uint32_t claim_relocation[claim_relocation_count];
	for (size_t i = 0; i < proposition_relocation_count; ++i) {
		proposition_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < claim_relocation_count; ++i) {
		claim_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < expr_relocation_count; ++i) {
		expr_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < field_relocation_count; ++i) {
		field_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < parameter_relocation_count; ++i) {
		parameter_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < constructor_relocation_count; ++i) {
		constructor_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	if (artifact_build_binding_relocation(
			source_terms,
			source_type_declarations,
			source_contexts,
			order,
			target_terms->next_binding_id,
			binding_relocation,
			binding_relocation_count,
			&next_binding_id
		) != 0) {
		return -1;
	}
	if (additional_relocation && additional_relocation->binding_ids) {
		memcpy(
			additional_relocation->binding_ids,
			binding_relocation,
			source_terms->next_binding_id * sizeof(*binding_relocation)
		);
	}
	uint32_t substitution_relocation[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t source_representation_anchors[512];
	uint32_t representation_relocation[512];
	size_t old_target_representation_count = target_type_declarations->representation_count;
	size_t source_representation_count = source_type_declarations->representation_count;
	if (old_target_representation_count > 512 || source_representation_count > 512) {
		return -1;
	}
	uint32_t next_representation_id = (uint32_t)old_target_representation_count;
	for (uint32_t i = 0; i < source_representation_count; ++i) {
		source_representation_anchors[i] =
			source_type_declarations->representations[i].representative_type_id;
		representation_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	if (order) {
		if (old_target_representation_count != 0) {
			return -1;
		}
		for (size_t position = 0; position < order->type_count; ++position) {
			uint32_t type_id = order->types[position];
			if (type_id >= source_type_declarations->type_count) {
				return -1;
			}
			const struct prototype_type_declaration* type =
				&source_type_declarations->type_declarations[type_id];
			if (!artifact_type_present(type)) {
				continue;
			}
			if (type->representation_id >= source_representation_count) {
				return -1;
			}
			if (representation_relocation[type->representation_id] ==
					PROTOTYPE_INVALID_ID) {
				representation_relocation[type->representation_id] =
					next_representation_id++;
				source_representation_anchors[type->representation_id] = type_id;
			}
		}
	} else {
	for (uint32_t i = 0; i < source_representation_count; ++i) {
		if (source_representation_anchors[i] >=
				source_type_declarations->type_count ||
			!artifact_type_present(&source_type_declarations->type_declarations[
				source_representation_anchors[i]
			])) {
			continue;
		}
		for (uint32_t j = 0; j < old_target_representation_count; ++j) {
			int representations_equal = 0;
			if (prototype_type_code_shape_keys_equal(
					&source_type_declarations->representations[i].fingerprint,
					&target_type_declarations->representations[j].fingerprint
				) && prototype_type_declaration_representations_equal(
					source_terms,
					source_type_declarations,
					source_contexts,
					source_representation_anchors[i],
					target_terms,
					target_type_declarations,
					target_contexts,
					target_type_declarations->representations[j].representative_type_id,
					&representations_equal
				) == 0 && representations_equal) {
				representation_relocation[i] = j;
				break;
			}
		}
		if (representation_relocation[i] == PROTOTYPE_INVALID_ID) {
			for (uint32_t j = 0; j < i; ++j) {
				int representations_equal = 0;
				if (representation_relocation[j] != PROTOTYPE_INVALID_ID &&
					prototype_type_code_shape_keys_equal(
						&source_type_declarations->representations[i].fingerprint,
						&source_type_declarations->representations[j].fingerprint
					) && prototype_type_declaration_representations_equal(
						source_terms,
						source_type_declarations,
						source_contexts,
						source_representation_anchors[i],
						source_terms,
						source_type_declarations,
						source_contexts,
						source_representation_anchors[j],
						&representations_equal
					) == 0 && representations_equal) {
					representation_relocation[i] = representation_relocation[j];
					break;
				}
			}
		}
		if (representation_relocation[i] == PROTOTYPE_INVALID_ID) {
			representation_relocation[i] = next_representation_id++;
		}
	}
	}
	uint32_t next_type_id = type_offset;
	for (uint32_t i = 0; i < source_type_declarations->type_count; ++i) {
		type_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	size_t ordered_type_count = order ?
		order->type_count : source_type_declarations->type_count;
	if (order && type_offset != 0) {
		return -1;
	}
	for (size_t position = 0; position < ordered_type_count; ++position) {
		uint32_t i = order ? order->types[position] : (uint32_t)position;
		if (i >= source_type_declarations->type_count) {
			return -1;
		}
		const struct prototype_type_declaration* source_type =
			&source_type_declarations->type_declarations[i];
		if (!artifact_type_present(source_type)) {
			continue;
		}
		if (source_type->origin_kind ==
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY) {
			type_relocation[i] = next_type_id++;
			continue;
		}
		for (uint32_t j = 0; j < type_offset; ++j) {
			const struct prototype_type_declaration* target_type =
				&target_type_declarations->type_declarations[j];
			if (!artifact_type_present(target_type) ||
				target_type->namespace_symbol_id !=
					source_type->namespace_symbol_id ||
				target_type->name_symbol_id != source_type->name_symbol_id) {
				continue;
			}
			struct prototype_type_code_shape_key target_key;
			struct prototype_type_code_shape_key source_key;
			int declarations_equal = 0;
			if (prototype_type_declaration_code_shape_key(
					target_terms,
					target_type_declarations,
					target_contexts,
					j,
					&target_key
				) != 0 || prototype_type_declaration_code_shape_key(
					source_terms,
					source_type_declarations,
					source_contexts,
					i,
					&source_key
				) != 0 || !prototype_type_code_shape_keys_equal(
					&target_key, &source_key
				) || prototype_type_declaration_representations_equal(
					target_terms,
					target_type_declarations,
					target_contexts,
					j,
					source_terms,
					source_type_declarations,
					source_contexts,
					i,
					&declarations_equal
				) != 0 || !declarations_equal) {
				return -1;
			}
			type_relocation[i] = j;
			break;
		}
		if (type_relocation[i] == PROTOTYPE_INVALID_ID) {
			type_relocation[i] = next_type_id++;
		}
	}
	uint32_t next_expr_id = (uint32_t)target_type_declarations->expr_count;
	size_t ordered_expr_count = order ?
		order->type_expr_count : source_type_declarations->expr_count;
	for (size_t position = 0; position < ordered_expr_count; ++position) {
		uint32_t i = order ? order->type_exprs[position] : (uint32_t)position;
		if (i >= source_type_declarations->expr_count) {
			return -1;
		}
		if (artifact_type_expr_present(&source_type_declarations->exprs[i])) {
			expr_relocation[i] = next_expr_id++;
		}
	}
	uint32_t next_field_id =
		(uint32_t)target_type_declarations->readback_field_type_count;
	uint32_t next_parameter_id =
		(uint32_t)target_type_declarations->parameter_count;
	uint32_t next_constructor_id =
		(uint32_t)target_type_declarations->constructor_count;
	if (order) {
		for (size_t position = 0; position < ordered_type_count; ++position) {
			uint32_t type_id = order->types[position];
			const struct prototype_type_declaration* type =
				&source_type_declarations->type_declarations[type_id];
			if (!artifact_type_present(type)) {
				continue;
			}
			if (type->first_parameter > source_type_declarations->parameter_count ||
				type->parameter_count > source_type_declarations->parameter_count -
					type->first_parameter ||
				type->first_constructor > source_type_declarations->constructor_count ||
				type->constructor_count > source_type_declarations->constructor_count -
					type->first_constructor) {
				return -1;
			}
			parameter_boundary_relocation[type->first_parameter] = next_parameter_id;
			for (uint32_t j = 0; j < type->parameter_count; ++j) {
				uint32_t parameter_id = type->first_parameter + j;
				if (!artifact_parameter_present(
						&source_type_declarations->parameter_declarations[parameter_id]
					)) {
					return -1;
				}
				parameter_relocation[parameter_id] = next_parameter_id++;
			}
			constructor_boundary_relocation[type->first_constructor] =
				next_constructor_id;
			for (uint32_t j = 0; j < type->constructor_count; ++j) {
				uint32_t constructor_id = type->first_constructor + j;
				const struct prototype_type_constructor_declaration* constructor =
					&source_type_declarations->constructor_declarations[constructor_id];
				if (!artifact_constructor_present(constructor) ||
					constructor->owner_type != type_id ||
					constructor->constructor_index != j) {
					return -1;
				}
				constructor_relocation[constructor_id] = next_constructor_id++;
				if (constructor->readback.first_field_type != PROTOTYPE_INVALID_ID) {
					if (constructor->readback.first_field_type >
							source_type_declarations->readback_field_type_count ||
						constructor->readback.field_count >
							source_type_declarations->readback_field_type_count -
							constructor->readback.first_field_type) {
						return -1;
					}
					field_boundary_relocation[
						constructor->readback.first_field_type
					] = next_field_id;
					for (uint32_t k = 0; k < constructor->readback.field_count; ++k) {
						uint32_t field_id = constructor->readback.first_field_type + k;
						if (!artifact_field_type_present(
								&source_type_declarations->readback_field_types[field_id]
							)) {
							return -1;
						}
						field_relocation[field_id] = next_field_id++;
					}
				}
			}
		}
	} else {
		for (uint32_t i = 0;
			i < source_type_declarations->readback_field_type_count;
			++i) {
			field_boundary_relocation[i] = next_field_id;
			if (source_type_declarations->readback_field_types[i] !=
				PROTOTYPE_INVALID_ID) {
				field_relocation[i] = next_field_id++;
			}
		}
		field_boundary_relocation[
			source_type_declarations->readback_field_type_count
		] = next_field_id;
		for (uint32_t i = 0; i < source_type_declarations->parameter_count; ++i) {
			parameter_boundary_relocation[i] = next_parameter_id;
			if (artifact_parameter_present(
					&source_type_declarations->parameter_declarations[i]
				)) {
				parameter_relocation[i] = next_parameter_id++;
			}
		}
		parameter_boundary_relocation[source_type_declarations->parameter_count] =
			next_parameter_id;
		for (uint32_t i = 0; i < source_type_declarations->constructor_count; ++i) {
			constructor_boundary_relocation[i] = next_constructor_id;
			const struct prototype_type_constructor_declaration* constructor =
				&source_type_declarations->constructor_declarations[i];
			if (!artifact_constructor_present(constructor)) {
				continue;
			}
			if (constructor->owner_type >= source_type_declarations->type_count ||
				type_relocation[constructor->owner_type] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			if (type_relocation[constructor->owner_type] < type_offset) {
				const struct prototype_type_declaration* target_owner =
					&target_type_declarations->type_declarations[
						type_relocation[constructor->owner_type]
					];
				if (constructor->constructor_index >= target_owner->constructor_count) {
					return -1;
				}
				constructor_relocation[i] = target_owner->first_constructor +
					constructor->constructor_index;
			} else {
				constructor_relocation[i] = next_constructor_id++;
			}
		}
		constructor_boundary_relocation[
			source_type_declarations->constructor_count
		] = next_constructor_id;
	}

	if (target_terms->term_count + source_terms->term_count > target_terms->term_capacity ||
		target_terms->case_count + source_terms->case_count > target_terms->case_capacity ||
		target_terms->case_binder_count + source_terms->case_binder_count > target_terms->case_binder_capacity ||
		target_terms->ih_scope_count + source_terms->ih_scope_count > target_terms->ih_scope_capacity ||
		target_terms->computation_fold_clause_count + source_terms->computation_fold_clause_count >
			PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY ||
		target_type_declarations->type_count + source_type_declarations->type_count > target_type_declarations->type_capacity ||
		target_type_declarations->parameter_count + source_type_declarations->parameter_count > target_type_declarations->parameter_capacity ||
		target_type_declarations->constructor_count + source_type_declarations->constructor_count > target_type_declarations->constructor_capacity ||
		target_type_declarations->readback_field_type_count + source_type_declarations->readback_field_type_count > target_type_declarations->readback_field_type_capacity ||
		target_type_declarations->expr_count + source_type_declarations->expr_count > target_type_declarations->expr_capacity ||
		target_judgement->proposition_count + source_judgement->proposition_count > target_judgement->proposition_capacity ||
		target_judgement->claim_count + source_judgement->claim_count >
			target_judgement->claim_capacity ||
		target_judgement->derivation_count + source_judgement->derivation_count >
			target_judgement->derivation_capacity ||
		target_judgement->accepted_premise_count +
			source_judgement->accepted_premise_count >
			target_judgement->accepted_premise_capacity ||
		source_interface->term_export_count > appended_interface->term_export_capacity ||
		source_interface->type_export_count > appended_interface->type_export_capacity ||
		source_interface->type_parameter_count > appended_interface->type_parameter_capacity ||
		source_interface->constructor_export_count > appended_interface->constructor_export_capacity ||
		source_interface->constructor_field_type_expr_count >
			appended_interface->constructor_field_type_expr_capacity ||
		source_interface->type_expr_count > appended_interface->type_expr_capacity ||
		 source_interface->identity_root_count >
				appended_interface->identity_root_capacity ||
		 source_interface->dependency_count > appended_interface->dependency_capacity) {
		fprintf(
			stderr,
			"artifact append: capacity check failed terms=%zu/%zu types=%zu/%zu "
			"constructors=%zu/%zu propositions=%zu/%zu claims=%zu/%zu "
			"derivations=%zu/%zu roots=%zu/%zu\n",
			target_terms->term_count + source_terms->term_count,
			target_terms->term_capacity,
			target_type_declarations->type_count + source_type_declarations->type_count,
			target_type_declarations->type_capacity,
			target_type_declarations->constructor_count +
				source_type_declarations->constructor_count,
			target_type_declarations->constructor_capacity,
			target_judgement->proposition_count + source_judgement->proposition_count,
			target_judgement->proposition_capacity,
			target_judgement->claim_count + source_judgement->claim_count,
			target_judgement->claim_capacity,
			target_judgement->derivation_count + source_judgement->derivation_count,
			target_judgement->derivation_capacity,
			source_interface->identity_root_count,
			appended_interface->identity_root_capacity
		);
		return -1;
	}

	if (prototype_term_db_append_relocated(
			target_terms,
			source_terms,
			type_relocation,
			type_relocation_count,
			binding_relocation,
			binding_relocation_count,
			universe_offset,
			representation_relocation,
			source_representation_count,
			order ? order->terms : NULL,
			order ? order->term_count : 0,
			term_relocation,
			term_relocation_capacity
		) != 0) {
		fprintf(stderr, "artifact append: term relocation failed\n");
		return -1;
	}
	for (uint32_t i = 0; i < source_type_declarations->type_count; ++i) {
		if (type_relocation[i] >= type_offset) {
			continue;
		}
		uint32_t source_classifier = source_type_declarations->type_declarations[
			i
		].formation_classifier;
		uint32_t target_classifier = target_type_declarations->type_declarations[
			type_relocation[i]
		].formation_classifier;
		if (source_classifier >= term_relocation_capacity ||
			target_classifier == PROTOTYPE_INVALID_ID ||
			target_classifier >= target_terms->term_count) {
			return -1;
		}
		term_relocation[source_classifier] = target_classifier;
	}
	if (prototype_context_db_append_relocated(
			target_contexts,
			source_contexts,
			term_relocation,
			term_relocation_capacity,
			binding_relocation,
			binding_relocation_count,
			context_relocation,
			context_relocation_capacity
		) != 0) {
		fprintf(stderr, "artifact append: context relocation failed\n");
		return -1;
	}
	if (prototype_substitution_db_append_relocated(
			target_substitutions,
			source_substitutions,
			context_relocation,
			source_contexts->context_count,
			term_relocation,
			term_relocation_capacity,
			substitution_relocation,
			PROTOTYPE_SUBSTITUTION_CAPACITY
		) != 0) {
		fprintf(stderr, "artifact append: substitution relocation failed\n");
		return -1;
	}

	for (size_t position = 0; position < ordered_expr_count; ++position) {
		uint32_t i = order ? order->type_exprs[position] : (uint32_t)position;
		struct prototype_type_expr expr = source_type_declarations->exprs[i];
		if (!artifact_type_expr_present(&expr)) {
			continue;
		}
		if (relocate_artifact_type_expr(
				&expr,
				expr_relocation,
				source_type_declarations->expr_count,
				binding_relocation,
				binding_relocation_count,
				universe_offset
			) != 0 || target_type_declarations->expr_count != expr_relocation[i]) {
			return -1;
		}
		target_type_declarations->exprs[target_type_declarations->expr_count++] = expr;
	}
	size_t ordered_field_count = order ?
		order->field_count : source_type_declarations->readback_field_type_count;
	for (size_t position = 0; position < ordered_field_count; ++position) {
		uint32_t i = (uint32_t)position;
		if (order) {
			i = PROTOTYPE_INVALID_ID;
			for (uint32_t candidate = 0;
				candidate < source_type_declarations->readback_field_type_count;
				++candidate) {
				if (field_relocation[candidate] ==
					target_type_declarations->readback_field_type_count) {
					i = candidate;
					break;
				}
			}
		}
		if (i >= source_type_declarations->readback_field_type_count) {
			return -1;
		}
		uint32_t field_type = source_type_declarations->readback_field_types[i];
		if (field_type == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (field_type >= source_type_declarations->expr_count ||
			expr_relocation[field_type] == PROTOTYPE_INVALID_ID ||
			target_type_declarations->readback_field_type_count !=
				field_relocation[i]) {
			return -1;
		}
		field_type = expr_relocation[field_type];
		target_type_declarations->readback_field_types[target_type_declarations->readback_field_type_count++] =
			field_type;
	}
	size_t ordered_parameter_count = order ?
		order->parameter_count : source_type_declarations->parameter_count;
	for (size_t position = 0; position < ordered_parameter_count; ++position) {
		uint32_t i = (uint32_t)position;
		if (order) {
			i = PROTOTYPE_INVALID_ID;
			for (uint32_t candidate = 0;
				candidate < source_type_declarations->parameter_count;
				++candidate) {
				if (parameter_relocation[candidate] ==
					target_type_declarations->parameter_count) {
					i = candidate;
					break;
				}
			}
		}
		if (i >= source_type_declarations->parameter_count) {
			return -1;
		}
		struct prototype_type_parameter_declaration parameter =
			source_type_declarations->parameter_declarations[i];
		if (!artifact_parameter_present(&parameter)) {
			continue;
		}
		if (parameter.type_expr >= source_type_declarations->expr_count ||
			expr_relocation[parameter.type_expr] == PROTOTYPE_INVALID_ID ||
			target_type_declarations->parameter_count != parameter_relocation[i]) {
			return -1;
		}
		if (parameter.binding_id >= binding_relocation_count ||
			binding_relocation[parameter.binding_id] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		parameter.binding_id = binding_relocation[parameter.binding_id];
		parameter.type_expr = expr_relocation[parameter.type_expr];
		target_type_declarations->parameter_declarations[target_type_declarations->parameter_count++] =
			parameter;
	}
	size_t ordered_constructor_count = order ?
		order->constructor_count : source_type_declarations->constructor_count;
	for (size_t position = 0; position < ordered_constructor_count; ++position) {
		uint32_t i = (uint32_t)position;
		if (order) {
			i = PROTOTYPE_INVALID_ID;
			for (uint32_t candidate = 0;
				candidate < source_type_declarations->constructor_count;
				++candidate) {
				if (constructor_relocation[candidate] ==
					target_type_declarations->constructor_count) {
					i = candidate;
					break;
				}
			}
		}
		if (i >= source_type_declarations->constructor_count) {
			return -1;
		}
		struct prototype_type_constructor_declaration constructor =
			source_type_declarations->constructor_declarations[i];
		if (!artifact_constructor_present(&constructor)) {
			continue;
		}
		if (constructor_relocation[i] <
				target_type_declarations->constructor_count) {
			continue;
		}
		if (constructor.owner_type >= source_type_declarations->type_count ||
			type_relocation[constructor.owner_type] == PROTOTYPE_INVALID_ID ||
			(constructor.readback.result_type != PROTOTYPE_INVALID_ID &&
			 (constructor.readback.result_type >=
				source_type_declarations->expr_count ||
			  expr_relocation[constructor.readback.result_type] ==
				PROTOTYPE_INVALID_ID)) ||
			 target_type_declarations->constructor_count != constructor_relocation[i]) {
			fprintf(
				stderr,
				"artifact append: constructor relocation invariant failed source=%u "
				"owner=%u relocated=%u result_expr=%u target_index=%zu expected=%u\n",
				i,
				constructor.owner_type,
				constructor.owner_type < source_type_declarations->type_count ?
					type_relocation[constructor.owner_type] : PROTOTYPE_INVALID_ID,
				constructor.readback.result_type,
				target_type_declarations->constructor_count,
				constructor_relocation[i]
			);
			return -1;
		}
		constructor.owner_type = type_relocation[constructor.owner_type];
		if (constructor.readback.field_count == 0) {
			if (constructor.readback.first_field_type == PROTOTYPE_INVALID_ID) {
				/* A fieldless constructor has no readback slice to relocate. */
			} else if (constructor.readback.first_field_type >
					source_type_declarations->readback_field_type_count) {
				fprintf(
					stderr,
					"artifact append: empty constructor readback boundary failed "
					"source=%u first=%u fields=%zu\n",
					i,
					constructor.readback.first_field_type,
					source_type_declarations->readback_field_type_count
				);
				return -1;
			} else {
				constructor.readback.first_field_type = field_boundary_relocation[
					constructor.readback.first_field_type
				];
			}
		} else {
			if (constructor.readback.first_field_type >=
					source_type_declarations->readback_field_type_count ||
				field_relocation[constructor.readback.first_field_type] ==
					PROTOTYPE_INVALID_ID) {
				fprintf(stderr, "artifact append: constructor field relocation failed source=%u\n", i);
				return -1;
			}
			constructor.readback.first_field_type =
				field_relocation[constructor.readback.first_field_type];
		}
		constructor.readback.result_type = artifact_relocate_optional_id(
			constructor.readback.result_type,
			expr_relocation,
			source_type_declarations->expr_count
		);
		{
			if (constructor.parameter_context >= source_contexts->context_count ||
				constructor.field_context >= source_contexts->context_count) {
				fprintf(stderr, "artifact append: constructor context relocation failed source=%u\n", i);
				return -1;
			}
			constructor.parameter_context =
				context_relocation[constructor.parameter_context];
			constructor.field_context =
				context_relocation[constructor.field_context];
			if (constructor.result_classifier >= term_relocation_capacity ||
				constructor.curried_classifier_cache >= term_relocation_capacity) {
				fprintf(
					stderr,
					"artifact append: constructor term reference failed source=%u "
					"owner=%u result=%u curried=%u term_slots=%zu\n",
					i,
					constructor.owner_type,
					constructor.result_classifier,
					constructor.curried_classifier_cache,
					term_relocation_capacity
				);
				return -1;
			}
			constructor.result_classifier =
				term_relocation[constructor.result_classifier];
			constructor.curried_classifier_cache =
				term_relocation[constructor.curried_classifier_cache];
		}
		target_type_declarations->constructor_declarations[target_type_declarations->constructor_count++] =
			constructor;
	}
	for (size_t position = 0; position < ordered_type_count; ++position) {
		uint32_t i = order ? order->types[position] : (uint32_t)position;
		struct prototype_type_declaration type =
			source_type_declarations->type_declarations[i];
		if (!artifact_type_present(&type) || type_relocation[i] < type_offset) {
			continue;
		}
		if (target_type_declarations->type_count != type_relocation[i]) {
			return -1;
		}
		{
			if (type.parameter_context >= source_contexts->context_count ||
				type.index_context >= source_contexts->context_count) {
				return -1;
			}
			if (type.type_index >= source_type_declarations->type_count ||
				type_relocation[type.type_index] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			type.type_index = type_relocation[type.type_index];
			type.representation_id = PROTOTYPE_INVALID_ID;
			if (type.formation_classifier >= term_relocation_capacity) {
				return -1;
			}
			type.formation_classifier = term_relocation[type.formation_classifier];
			if (type.origin_source_carrier_term_id != PROTOTYPE_INVALID_ID) {
				if (type.origin_source_carrier_term_id >= term_relocation_capacity ||
					term_relocation[type.origin_source_carrier_term_id] ==
						PROTOTYPE_INVALID_ID) {
					return -1;
				}
				type.origin_source_carrier_term_id = term_relocation[
					type.origin_source_carrier_term_id
				];
			}
			type.parameter_context =
				context_relocation[type.parameter_context];
			type.index_context = context_relocation[type.index_context];
			if (type.parameter_count == 0) {
				if (type.first_parameter >
						source_type_declarations->parameter_count) {
					return -1;
				}
				type.first_parameter =
					parameter_boundary_relocation[type.first_parameter];
			} else if (type.first_parameter >=
					source_type_declarations->parameter_count ||
				parameter_relocation[type.first_parameter] == PROTOTYPE_INVALID_ID) {
				return -1;
			} else {
				type.first_parameter = parameter_relocation[type.first_parameter];
			}
			if (type.constructor_count == 0) {
				if (type.first_constructor >
						source_type_declarations->constructor_count) {
					return -1;
				}
				type.first_constructor =
					constructor_boundary_relocation[type.first_constructor];
			} else if (type.first_constructor >=
					source_type_declarations->constructor_count ||
				constructor_relocation[type.first_constructor] == PROTOTYPE_INVALID_ID) {
				return -1;
			} else {
				type.first_constructor = constructor_relocation[type.first_constructor];
			}
		}
		target_type_declarations->type_declarations[target_type_declarations->type_count++] = type;
	}
	target_type_declarations->representations_dirty = 1;

	if (prototype_type_declaration_rebuild_representations(
			target_terms,
			target_type_declarations,
			target_contexts
		) != 0) {
		fprintf(stderr, "artifact append: representation rebuild failed\n");
		return -1;
	}
	for (uint32_t i = 0; i < source_representation_count; ++i) {
		if (representation_relocation[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t source_anchor_type_id = source_representation_anchors[i];
		if (source_anchor_type_id >= type_relocation_count ||
			type_relocation[source_anchor_type_id] >=
				target_type_declarations->type_count ||
			target_type_declarations->type_declarations[
				type_relocation[source_anchor_type_id]
			].representation_id != representation_relocation[i]) {
			fprintf(
				stderr,
				"artifact append: representation relocation mismatch source=%u "
				"anchor=%u relocated_type=%u expected_rep=%u actual_rep=%u\n",
				i,
				source_anchor_type_id,
				source_anchor_type_id < type_relocation_count ?
					type_relocation[source_anchor_type_id] : PROTOTYPE_INVALID_ID,
				representation_relocation[i],
				source_anchor_type_id < type_relocation_count &&
					type_relocation[source_anchor_type_id] <
						target_type_declarations->type_count ?
					target_type_declarations->type_declarations[
						type_relocation[source_anchor_type_id]
					].representation_id : PROTOTYPE_INVALID_ID
			);
			return -1;
		}
	}
	if (prototype_term_canonicalize_type_former_references(target_terms) != 0) {
		return -1;
	}

	if (operation_offset != PROTOTYPE_INVALID_ID &&
		artifact_append_accepted_judgement(
			target_judgement,
			source_judgement,
			source_contexts,
			context_relocation,
			source_substitutions,
			substitution_relocation,
			term_relocation,
			source_terms->term_count,
			operation_offset,
			proposition_relocation,
			claim_relocation,
			order
		) != 0) {
		fprintf(stderr, "artifact append: accepted judgement relocation failed\n");
		return -1;
	}

	target_terms->next_binding_id = next_binding_id;
	target_type_declarations->next_level_var =
		universe_offset + source_type_declarations->next_level_var;
	if (target_judgement->next_universe_var < target_type_declarations->next_level_var) {
		target_judgement->next_universe_var = target_type_declarations->next_level_var;
	}
	prototype_internal_sync_artifact_universe_level_counters(
		target_terms,
		target_type_declarations,
		target_judgement
	);

	if (canonicalize_link_references &&
		(prototype_internal_canonicalize_type_view_core_refs(
			target_terms, target_type_declarations, target_contexts
		) != 0 || prototype_constructor_curried_caches_rebuild(
			target_type_declarations, target_contexts, target_terms
		) != 0)) {
		return -1;
	}

	appended_interface->term_export_count = source_interface->term_export_count;
	appended_interface->type_export_count = source_interface->type_export_count;
	appended_interface->type_parameter_count = source_interface->type_parameter_count;
	appended_interface->constructor_export_count = source_interface->constructor_export_count;
	appended_interface->constructor_field_type_expr_count =
		source_interface->constructor_field_type_expr_count;
	appended_interface->type_expr_count = source_interface->type_expr_count;
	appended_interface->identity_root_count = source_interface->identity_root_count;
	appended_interface->dependency_count = source_interface->dependency_count;
	for (size_t i = 0; i < source_interface->type_expr_count; ++i) {
		appended_interface->type_exprs[i] = source_interface->type_exprs[i];
	}
	for (size_t i = 0; i < source_interface->type_parameter_count; ++i) {
		appended_interface->type_parameters[i] = source_interface->type_parameters[i];
	}
	for (size_t i = 0; i < source_interface->constructor_field_type_expr_count; ++i) {
		appended_interface->constructor_field_type_exprs[i] =
			source_interface->constructor_field_type_exprs[i];
	}
	for (size_t i = 0; i < source_interface->term_export_count; ++i) {
		appended_interface->term_exports[i] = source_interface->term_exports[i];
		if (appended_interface->term_exports[i].local_term >=
				term_relocation_capacity) {
			return -1;
		}
		appended_interface->term_exports[i].local_term = term_relocation[
			appended_interface->term_exports[i].local_term
		];
		appended_interface->term_exports[i].operation =
			operation_offset == PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID :
			offset_artifact_id(
				source_interface->term_exports[i].operation, operation_offset
			);
		if (source_interface->term_exports[i].source_evidence.kind !=
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID) {
			if (operation_offset == PROTOTYPE_INVALID_ID) {
				appended_interface->term_exports[i].source_evidence.kind =
					PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID;
				appended_interface->term_exports[i].source_evidence.id =
					PROTOTYPE_INVALID_ID;
			} else if (source_interface->term_exports[i].source_evidence.kind !=
					PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM ||
				source_interface->term_exports[i].source_evidence.id >=
					source_judgement->claim_count || claim_relocation[
					source_interface->term_exports[i].source_evidence.id
				] == PROTOTYPE_INVALID_ID) {
				return -1;
			} else {
				appended_interface->term_exports[i].source_evidence.kind =
					PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM;
				appended_interface->term_exports[i].source_evidence.id =
					claim_relocation[
						source_interface->term_exports[i].source_evidence.id
					];
			}
		} else {
			appended_interface->term_exports[i].source_evidence.kind =
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID;
			appended_interface->term_exports[i].source_evidence.id =
				PROTOTYPE_INVALID_ID;
		}
		if (appended_interface->term_exports[i].classifier != PROTOTYPE_INVALID_ID) {
			if (appended_interface->term_exports[i].classifier >=
					term_relocation_capacity) {
				return -1;
			}
			appended_interface->term_exports[i].classifier = term_relocation[
				appended_interface->term_exports[i].classifier
			];
		}
		if (prototype_term_canonical_key_with_types(
				target_terms,
				target_type_declarations,
				appended_interface->term_exports[i].local_term,
				&appended_interface->term_exports[i].canonical_key
			) != 0) {
			return -1;
		}
		memset(
			&appended_interface->term_exports[i].classifier_key,
			0,
			sizeof(appended_interface->term_exports[i].classifier_key)
		);
		if (appended_interface->term_exports[i].classifier != PROTOTYPE_INVALID_ID &&
			(appended_interface->term_exports[i].classifier >= target_terms->term_count ||
				prototype_term_canonical_key_with_types(
					target_terms,
					target_type_declarations,
					appended_interface->term_exports[i].classifier,
					&appended_interface->term_exports[i].classifier_key
				) != 0)) {
			return -1;
		}
	}
	for (size_t i = 0; i < source_interface->type_export_count; ++i) {
		appended_interface->type_exports[i] = source_interface->type_exports[i];
		if (appended_interface->type_exports[i].local_type_id >=
				type_relocation_count) {
			return -1;
		}
		appended_interface->type_exports[i].local_type_id = type_relocation[
			appended_interface->type_exports[i].local_type_id
		];
		if (appended_interface->type_exports[i].core_representation_anchor_type_id != PROTOTYPE_INVALID_ID) {
			if (appended_interface->type_exports[i].core_representation_anchor_type_id >=
					type_relocation_count) {
				return -1;
			}
			appended_interface->type_exports[i].core_representation_anchor_type_id =
				type_relocation[appended_interface->type_exports[i].core_representation_anchor_type_id];
		}
		if (appended_interface->type_exports[i].formation_classifier >=
				term_relocation_capacity) {
			return -1;
		}
		appended_interface->type_exports[i].formation_classifier = term_relocation[
			appended_interface->type_exports[i].formation_classifier
		];
			if (prototype_type_declaration_code_shape_key(
					target_terms,
					target_type_declarations,
					target_contexts,
					appended_interface->type_exports[i].local_type_id,
					&appended_interface->type_exports[i].code_shape_key
				) != 0) {
				return -1;
			}
			if (prototype_type_declaration_representation_anchor_type_id(
					target_terms,
					target_type_declarations,
					appended_interface->type_exports[i].local_type_id,
					&appended_interface->type_exports[i].core_representation_anchor_type_id
				) != 0) {
				return -1;
			}
	}
	for (size_t i = 0; i < source_interface->constructor_export_count; ++i) {
		appended_interface->constructor_exports[i] = source_interface->constructor_exports[i];
		const struct prototype_artifact_constructor_export* export =
			&appended_interface->constructor_exports[i];
		if (export->type_export_index >= appended_interface->type_export_count) {
			return -1;
		}
		uint32_t type_id = appended_interface->type_exports[
			export->type_export_index
		].local_type_id;
		if (type_id >= target_type_declarations->type_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&target_type_declarations->type_declarations[type_id];
		if (export->ordinal >= type->constructor_count) {
			return -1;
		}
		appended_interface->constructor_exports[i].curried_classifier_cache =
			target_type_declarations->constructor_declarations[
				type->first_constructor + export->ordinal
			].curried_classifier_cache;
	}
	for (size_t i = 0; i < source_interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* source_root =
			&source_interface->identity_roots[i];
		struct prototype_artifact_identity_root* target_root =
			&appended_interface->identity_roots[i];
		if (source_root->source_type_claim_id >=
				source_judgement->claim_count || claim_relocation[
				source_root->source_type_claim_id
			] == PROTOTYPE_INVALID_ID ||
			source_root->identity_family_has_type_claim_id >=
				source_judgement->claim_count || claim_relocation[
				source_root->identity_family_has_type_claim_id
			] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		target_root->source_type_claim_id = claim_relocation[
			source_root->source_type_claim_id
		];
		target_root->identity_family_has_type_claim_id = claim_relocation[
			source_root->identity_family_has_type_claim_id
		];
		target_root->witness_has_type_claim_id = PROTOTYPE_INVALID_ID;
		target_root->computation_rule = source_root->computation_rule;
		if (source_root->witness_has_type_claim_id != PROTOTYPE_INVALID_ID) {
			if (source_root->witness_has_type_claim_id >=
					source_judgement->claim_count || claim_relocation[
					source_root->witness_has_type_claim_id
				] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			target_root->witness_has_type_claim_id = claim_relocation[
				source_root->witness_has_type_claim_id
			];
		}
	}
	for (size_t i = 0; i < source_interface->dependency_count; ++i) {
		appended_interface->dependencies[i] = source_interface->dependencies[i];
	}
	if (additional_relocation) {
#define COPY_ADDITIONAL_RELOCATION(field, source, count) \
		do { \
			if (additional_relocation->field && (count) != 0) { \
				memcpy( \
					additional_relocation->field, \
					(source), \
					(count) * sizeof(*(source)) \
				); \
			} \
		} while (0)
		COPY_ADDITIONAL_RELOCATION(
			type_ids, type_relocation, source_type_declarations->type_count
		);
		COPY_ADDITIONAL_RELOCATION(
			type_expr_ids, expr_relocation, source_type_declarations->expr_count
		);
		COPY_ADDITIONAL_RELOCATION(
			parameter_ids,
			parameter_relocation,
			source_type_declarations->parameter_count
		);
		COPY_ADDITIONAL_RELOCATION(
			constructor_ids,
			constructor_relocation,
			source_type_declarations->constructor_count
		);
		COPY_ADDITIONAL_RELOCATION(
			field_type_ids,
			field_relocation,
			source_type_declarations->readback_field_type_count
		);
		COPY_ADDITIONAL_RELOCATION(
			proposition_ids,
			proposition_relocation,
			source_judgement->proposition_count
		);
		COPY_ADDITIONAL_RELOCATION(
			claim_ids, claim_relocation, source_judgement->claim_count
		);
		COPY_ADDITIONAL_RELOCATION(
			substitution_ids,
			substitution_relocation,
			source_substitutions->substitution_count
		);
#undef COPY_ADDITIONAL_RELOCATION
	}
	prototype_term_notify_graph_mutation(target_terms);
	return 0;
}

int prototype_artifact_append_graph(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_substitution_db* target_substitutions,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement,
	const struct prototype_context_db* source_contexts,
	const struct prototype_substitution_db* source_substitutions,
	uint32_t operation_offset,
	uint32_t* term_relocation,
	size_t term_relocation_capacity,
	uint32_t* context_relocation,
	size_t context_relocation_capacity,
	struct prototype_artifact_graph_relocation* additional_relocation,
	int canonicalize_link_references
) {
	return prototype_internal_artifact_append_graph_ordered(
		appended_interface,
		target_terms,
		target_type_declarations,
		target_judgement,
		target_contexts,
		target_substitutions,
		source_interface,
		source_terms,
		source_type_declarations,
		source_judgement,
		source_contexts,
		source_substitutions,
		operation_offset,
		term_relocation,
		term_relocation_capacity,
		context_relocation,
		context_relocation_capacity,
		additional_relocation,
		canonicalize_link_references,
		NULL
	);
}

static int align_operation_projection(
	const struct prototype_term_db* terms,
	struct prototype_judgement_db* judgement,
	struct prototype_operation_graph* graph,
	uint32_t operation_id,
	uint32_t core_term,
	unsigned char* visited
) {
	if (!terms || !judgement || !graph || !visited ||
		operation_id >= graph->operation_count || core_term >= terms->term_count) {
		return -1;
	}
	struct prototype_operation_node* operation = &graph->operations[operation_id];
	if (visited[operation_id]) {
		return operation->core_term == core_term ? 0 : -1;
	}
	visited[operation_id] = 1;
	uint32_t old_core = operation->core_term;
	if (old_core != core_term) {
		return -1;
	}
	operation->core_term = core_term;
	const struct prototype_term* term = &terms->terms[core_term];
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			return align_operation_projection(
				terms, judgement, graph, operation->function, core_term, visited
			);
		case PROTOTYPE_OPERATION_ASCRIPTION:
			return align_operation_projection(
				terms, judgement, graph, operation->body, core_term, visited
			);
		case PROTOTYPE_OPERATION_LAMBDA:
			if (term->tag != PROTOTYPE_TERM_LAMBDA) {
				return -1;
			}
			/* The shared Lambda is alpha-interned, while its typed body
			 * occurrence retains the provider binder. The Lambda validator
			 * compares those bodies under the two binder identities. */
			return 0;
		case PROTOTYPE_OPERATION_APP:
			if (term->tag != PROTOTYPE_TERM_APP ||
				align_operation_projection(
					terms, judgement, graph, operation->function,
					term->as.app.function, visited
				) != 0) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->argument,
				term->as.app.argument, visited
			);
		case PROTOTYPE_OPERATION_RETURN:
			if (term->tag != PROTOTYPE_TERM_RETURN) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->argument,
				term->as.return_term.value, visited
			);
		case PROTOTYPE_OPERATION_THUNK:
			if (term->tag != PROTOTYPE_TERM_THUNK) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->argument,
				term->as.thunk.computation, visited
			);
		case PROTOTYPE_OPERATION_FORCE:
			if (term->tag != PROTOTYPE_TERM_FORCE) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->argument,
				term->as.force.value, visited
			);
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			if (term->tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->argument,
				term->as.induction_hypothesis.argument, visited
			);
		case PROTOTYPE_OPERATION_REQUEST:
			if (term->tag != PROTOTYPE_TERM_OPERATION_REQUEST ||
				align_operation_projection(
					terms, judgement, graph, operation->function,
					term->as.operation_request.operation, visited
				) != 0 || align_operation_projection(
					terms, judgement, graph, operation->argument,
					term->as.operation_request.argument, visited
				) != 0) {
				return -1;
			}
			return align_operation_projection(
				terms, judgement, graph, operation->body,
				term->as.operation_request.continuation, visited
			);
		case PROTOTYPE_OPERATION_MATCH:
			if (term->tag != PROTOTYPE_TERM_MATCH ||
				operation->case_count != term->as.match.case_count ||
				operation->first_case > graph->case_count ||
				operation->case_count > graph->case_count - operation->first_case ||
				term->as.match.first_case > terms->case_count ||
				term->as.match.case_count > terms->case_count - term->as.match.first_case ||
				align_operation_projection(
					terms, judgement, graph, operation->scrutinee,
					term->as.match.scrutinee, visited
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				if (align_operation_projection(
						terms,
						judgement,
						graph,
						graph->cases[operation->first_case + i].body_operation,
						terms->cases[term->as.match.first_case + i].body,
						visited
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			/* Fold projection alignment needs clause-by-clause continuation
			 * transport and is not guessed from a root canonical key. */
			return old_core == core_term ? 0 : -1;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_VAR:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
			return 0;
		default:
			return -1;
	}
}

int prototype_artifact_align_export_operations(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
) {
	if (!interface || !terms || !judgement || !metadata) {
		return -1;
	}
	struct prototype_operation_graph graph;
	prototype_compile_metadata_operation_graph(metadata, &graph);
	unsigned char visited[graph.operation_count];
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export =
			&interface->term_exports[i];
		if (export->operation >= graph.operation_count ||
			export->local_term >= terms->term_count) {
			return -1;
		}
		memset(visited, 0, sizeof(visited));
		if (align_operation_projection(
				terms,
				judgement,
				&graph,
				export->operation,
				export->local_term,
				visited
			) != 0) {
			return -1;
		}
		graph.operations[export->operation].classifier = export->classifier;
	}
	return 0;
}

int prototype_canonical_link_table_add_metadata(
	struct prototype_canonical_link_table* table,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	uint32_t unit_id,
	int reject_frame_local_references
) {
	if (!table || !terms || !metadata) {
		return -1;
	}
	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		if (label->term >= terms->term_count) {
			return -1;
		}
		if (label->canonical_key.free_binder_count != 0) {
			return -1;
		}
		if (reject_frame_local_references &&
			label->canonical_key.has_frame_local_reference) {
			return -1;
		}

		uint32_t representative = (uint32_t)table->entry_count;
		for (size_t j = 0; j < table->entry_count; ++j) {
			if (!canonical_keys_equal(&table->entries[j].canonical_key, &label->canonical_key)) {
				continue;
			}
			const struct prototype_canonical_link_entry* candidate = &table->entries[j];
			if (!candidate->terms || candidate->local_term >= candidate->terms->term_count) {
				return -1;
			}
			int same_term = 0;
				if (prototype_term_view_shape_equal_for_link(
						candidate->terms,
						candidate->type_declarations,
						candidate->local_term,
					terms,
					type_declarations,
					label->term,
					&same_term
				) != 0) {
				return -1;
			}
			if (!same_term) {
				/* Canonical keys are hash prefilters. Distinct Terms may collide and
				 * must retain independent representatives. */
				continue;
			}
			representative = candidate->representative;
			break;
		}

		if (reserve_slot(table->entry_count, table->entry_capacity) != 0) {
			return -1;
		}
		uint32_t id = (uint32_t)table->entry_count;
		table->entries[id].unit_id = unit_id;
		table->entries[id].label_index = (uint32_t)i;
		table->entries[id].name_symbol_id = label->name_symbol_id;
		table->entries[id].terms = terms;
		table->entries[id].type_declarations = type_declarations;
		table->entries[id].local_term = label->term;
		table->entries[id].representative = representative;
		table->entries[id].canonical_key = label->canonical_key;
		table->entry_count++;
	}
	return 0;
}
