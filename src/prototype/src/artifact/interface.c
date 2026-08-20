#include "a_program/artifact/interface.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a_program/dimension/action.h"
#include "artifact_graph_internal.h"

void prototype_canonical_link_table_init(
	struct prototype_canonical_link_table* table,
	struct prototype_canonical_link_entry* entries,
	size_t entry_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->entries = entries;
	table->entry_capacity = entry_capacity;
}

void prototype_artifact_interface_init(
	struct prototype_artifact_interface* interface,
	struct prototype_artifact_term_export* term_exports,
	size_t term_export_capacity,
	struct prototype_artifact_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_artifact_type_parameter_export* type_parameters,
	size_t type_parameter_capacity,
	struct prototype_artifact_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	uint32_t* constructor_field_type_exprs,
	size_t constructor_field_type_expr_capacity,
	struct prototype_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_artifact_identity_root* identity_roots,
	size_t identity_root_capacity,
	struct prototype_artifact_dependency* dependencies,
	size_t dependency_capacity
) {
	if (!interface) {
		return;
	}
	memset(interface, 0, sizeof(*interface));
	interface->term_exports = term_exports;
	interface->term_export_capacity = term_export_capacity;
	interface->type_exports = type_exports;
	interface->type_export_capacity = type_export_capacity;
	interface->type_parameters = type_parameters;
	interface->type_parameter_capacity = type_parameter_capacity;
	interface->constructor_exports = constructor_exports;
	interface->constructor_export_capacity = constructor_export_capacity;
	interface->constructor_field_type_exprs = constructor_field_type_exprs;
	interface->constructor_field_type_expr_capacity = constructor_field_type_expr_capacity;
	interface->type_exprs = type_exprs;
	interface->type_expr_capacity = type_expr_capacity;
	interface->identity_roots = identity_roots;
	interface->identity_root_capacity = identity_root_capacity;
	interface->dependencies = dependencies;
	interface->dependency_capacity = dependency_capacity;
}

static int artifact_term_has_compiler_relation_head(
	const struct prototype_term_db* terms,
	uint32_t term_id
);

static int artifact_term_is_binding_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t binding_id
) {
	return terms && term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[term_id].as.var.binding_id == binding_id;
}

static int artifact_parse_pure_pointwise_pi_level(
	const struct prototype_term_db* terms,
	uint32_t thunk_type,
	uint32_t expected_domain,
	uint32_t* p_binding_id,
	uint32_t* p_result
) {
	if (!terms || !p_binding_id || !p_result ||
		thunk_type >= terms->term_count ||
		terms->terms[thunk_type].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t pi = terms->terms[thunk_type].as.thunk_type.computation;
	uint32_t codomain;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI ||
		terms->terms[pi].as.pi.domain != expected_domain ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			p_binding_id,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_effect_row_purity(
			terms, terms->terms[codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	*p_result = terms->terms[codomain].as.computation_type.result;
	return 1;
}

static int artifact_parse_one_dimensional_action_instance(
	const struct prototype_term_db* terms,
	uint32_t instance,
	uint32_t* p_source,
	uint32_t* p_operator_id,
	uint32_t arguments[2]
) {
	if (!terms || !p_source || !p_operator_id || !arguments ||
		instance >= terms->term_count ||
		terms->terms[instance].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	arguments[1] = terms->terms[instance].as.app.argument;
	uint32_t left = terms->terms[instance].as.app.function;
	if (left >= terms->term_count || terms->terms[left].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	arguments[0] = terms->terms[left].as.app.argument;
	uint32_t family = terms->terms[left].as.app.function;
	if (family >= terms->term_count ||
		terms->terms[family].tag != PROTOTYPE_TERM_DIMENSION_ACTION) {
		return 0;
	}
	*p_source = terms->terms[family].as.dimension_action.source;
	*p_operator_id = terms->terms[family].as.dimension_action.operator_id;
	return 1;
}

static int artifact_pi_identity_family_matches_source(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* source,
	const struct prototype_judgement_proposition* family
) {
	if (!terms || !type_declarations || !contexts || !source || !family ||
		source->subject >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t source_pi = terms->terms[source->subject].as.thunk_type.computation;
	uint32_t source_codomain_binding;
	uint32_t source_codomain;
	if (source_pi >= terms->term_count ||
		terms->terms[source_pi].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[source_pi].as.pi.codomain_family,
			&source_codomain_binding,
			&source_codomain
		) != 0 || source_codomain >= terms->term_count ||
		terms->terms[source_codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_contains_free_binding(
			terms, source_codomain, source_codomain_binding
		) || prototype_term_effect_row_purity(
			terms, terms->terms[source_codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t domain = terms->terms[source_pi].as.pi.domain;
	uint32_t x0_binding;
	uint32_t x1_binding;
	uint32_t xr_binding;
	uint32_t after_x0;
	uint32_t after_x1;
	uint32_t result_identity;
	if (!artifact_parse_pure_pointwise_pi_level(
			terms, family->subject, domain, &x0_binding, &after_x0
		) || !artifact_parse_pure_pointwise_pi_level(
			terms, after_x0, domain, &x1_binding, &after_x1
		)) {
		return 0;
	}
	if (after_x1 >= terms->term_count ||
		terms->terms[after_x1].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t third_pi = terms->terms[after_x1].as.thunk_type.computation;
	uint32_t input_identity = third_pi < terms->term_count &&
		terms->terms[third_pi].tag == PROTOTYPE_TERM_PI ?
		terms->terms[third_pi].as.pi.domain : PROTOTYPE_INVALID_ID;
	if (input_identity == PROTOTYPE_INVALID_ID ||
		!artifact_parse_pure_pointwise_pi_level(
			terms, after_x1, input_identity, &xr_binding, &result_identity
		)) {
		return 0;
	}
	uint32_t domain_action_source;
	uint32_t domain_action_operator;
	uint32_t input_identity_arguments[2];
	if (!artifact_parse_one_dimensional_action_instance(
			terms,
			input_identity,
			&domain_action_source,
			&domain_action_operator,
			input_identity_arguments
		) || domain_action_source != domain ||
		!artifact_term_is_binding_var(
			terms, input_identity_arguments[0], x0_binding
		) || !artifact_term_is_binding_var(
			terms, input_identity_arguments[1], x1_binding
		)) {
		return 0;
	}
	uint32_t result_action_source;
	uint32_t result_action_operator;
	uint32_t result_arguments[2];
	if (!artifact_parse_one_dimensional_action_instance(
			terms,
			result_identity,
			&result_action_source,
			&result_action_operator,
			result_arguments
		) || result_action_operator != domain_action_operator) {
		return 0;
	}
	uint32_t result_thunk = result_action_source;
	if (result_thunk >= terms->term_count ||
		terms->terms[result_thunk].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[result_thunk].as.thunk_type.computation != source_codomain) {
		return 0;
	}
	uint32_t endpoints[2];
	uint32_t arguments[2] = {
		input_identity_arguments[0],
		input_identity_arguments[1]
	};
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t thunked = result_arguments[i];
		uint32_t applied = thunked < terms->term_count &&
			terms->terms[thunked].tag == PROTOTYPE_TERM_THUNK ?
			terms->terms[thunked].as.thunk.computation : PROTOTYPE_INVALID_ID;
		uint32_t forced = applied < terms->term_count &&
			terms->terms[applied].tag == PROTOTYPE_TERM_APP ?
			terms->terms[applied].as.app.function : PROTOTYPE_INVALID_ID;
		if (applied >= terms->term_count ||
			terms->terms[applied].tag != PROTOTYPE_TERM_APP ||
			terms->terms[applied].as.app.argument != arguments[i] ||
			forced >= terms->term_count ||
			terms->terms[forced].tag != PROTOTYPE_TERM_FORCE) {
			return 0;
		}
		endpoints[i] = terms->terms[forced].as.force.value;
	}
	(void)type_declarations;
	const struct prototype_context* family_context = prototype_context_get(
		contexts, family->context_id
	);
	if (!family_context) {
		return 0;
	}
	if (family->context_id != source->context_id) {
		const struct prototype_context* left_context = prototype_context_get(
			contexts, family_context->parent
		);
		if (!left_context || left_context->parent != source->context_id ||
			prototype_context_classifier_term(left_context) != source->subject ||
			prototype_context_classifier_term(family_context) != source->subject ||
			!artifact_term_is_binding_var(
				terms, endpoints[0], left_context->binding_id
			) || !artifact_term_is_binding_var(
				terms, endpoints[1], family_context->binding_id
			)) {
			return 0;
		}
	}
	(void)xr_binding;
	return 1;
}

static int artifact_identity_family_matches_source(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_proposition* source,
	const struct prototype_judgement_proposition* family,
	int computation_rule
) {
	if (!terms || !type_declarations || !contexts || !dimension_operators ||
		!source || !family ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		family->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		source->classifier >= terms->term_count ||
		family->subject >= terms->term_count ||
		family->classifier != source->classifier ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		artifact_term_has_compiler_relation_head(terms, family->subject) != 0) {
		return 0;
	}
	if (computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		return artifact_pi_identity_family_matches_source(
			terms, type_declarations, contexts, source, family
		);
	}
	if (computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT ||
		computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
		uint32_t source_head = source->subject;
		while (source_head < terms->term_count &&
			terms->terms[source_head].tag == PROTOTYPE_TERM_APP) {
			source_head = terms->terms[source_head].as.app.function;
		}
		if (computation_rule ==
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
			(source_head >= terms->term_count ||
			 terms->terms[source_head].tag != PROTOTYPE_TERM_TYPE_VIEW)) {
			return 0;
		}
		if (computation_rule ==
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
			(source->subject >= terms->term_count ||
			 terms->terms[source->subject].tag != PROTOTYPE_TERM_THUNK_TYPE ||
			 terms->terms[source->subject].as.thunk_type.computation >=
				terms->term_count ||
			 terms->terms[
				terms->terms[source->subject].as.thunk_type.computation
			 ].tag != PROTOTYPE_TERM_COMPUTATION_TYPE)) {
			return 0;
		}
		const struct prototype_term* right_application =
			&terms->terms[family->subject];
		const struct prototype_term* left_application =
			right_application->tag == PROTOTYPE_TERM_APP &&
			right_application->as.app.function < terms->term_count ?
			&terms->terms[right_application->as.app.function] : NULL;
		const struct prototype_term* acted_family = left_application &&
			left_application->tag == PROTOTYPE_TERM_APP &&
			left_application->as.app.function < terms->term_count ?
			&terms->terms[left_application->as.app.function] : NULL;
		if (!left_application || left_application->tag != PROTOTYPE_TERM_APP ||
			!acted_family || acted_family->tag != PROTOTYPE_TERM_DIMENSION_ACTION ||
			acted_family->as.dimension_action.source != source->subject) {
			return 0;
		}
		return prototype_context_get(contexts, family->context_id) != NULL;
	}
	if (computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION) {
		uint32_t family_head;
		uint32_t family_source;
		uint32_t operator_id;
		uint32_t target_dimension;
		size_t argument_count;
		if (prototype_dimension_action_family_instance_info(
				terms,
				dimension_operators,
				family->subject,
				&family_head,
				&family_source,
				&operator_id,
				&target_dimension,
				NULL,
				0,
				&argument_count
			) != 0) {
			return 0;
		}
		uint32_t source_family;
		uint32_t source_of_source;
		uint32_t source_operator;
		uint32_t source_dimension;
		size_t source_argument_count;
		if (prototype_dimension_action_family_instance_info(
				terms,
				dimension_operators,
				source->subject,
				&source_family,
				&source_of_source,
				&source_operator,
				&source_dimension,
				NULL,
				0,
				&source_argument_count
			) != 0 || family_source != source_family ||
			target_dimension != source_dimension + 1) {
			return 0;
		}
		(void)family_head;
		(void)operator_id;
		(void)argument_count;
		(void)source_of_source;
		(void)source_operator;
		(void)source_argument_count;
		return prototype_context_get(contexts, family->context_id) != NULL;
	}
	/* The current Universe correspondence has only one-dimensional transport
	 * and lifting. It is compiler-local until recursive bisimulation evidence
	 * is represented and replayable, so rule 6 is not an artifact identity. */
	return 0;
}

int prototype_artifact_interface_add_identity_root(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement,
	uint32_t source_type_claim_id,
	uint32_t identity_family_has_type_claim_id,
	uint32_t witness_has_type_claim_id,
	int computation_rule,
	uint32_t* p_root_id
) {
	const struct prototype_judgement_proposition* source =
		prototype_judgement_claim_proposition(
			judgement, source_type_claim_id
		);
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_has_type_claim_id
		);
	if (!interface || !terms || !type_declarations || !contexts ||
		!dimension_operators || !judgement || !p_root_id ||
		!artifact_identity_family_matches_source(
			terms,
			type_declarations,
			contexts,
			dimension_operators,
			source,
			family,
			computation_rule
		)) {
		return -1;
	}
	if (witness_has_type_claim_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, witness_has_type_claim_id
			);
		if (!witness || witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			witness->context_id != family->context_id ||
			witness->classifier != family->subject) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		if (root->source_type_claim_id == source_type_claim_id &&
			root->identity_family_has_type_claim_id ==
				identity_family_has_type_claim_id &&
			root->witness_has_type_claim_id == witness_has_type_claim_id &&
			root->computation_rule == computation_rule) {
			*p_root_id = (uint32_t)i;
			return 0;
		}
	}
	if (!interface->identity_roots || interface->identity_root_count >=
		interface->identity_root_capacity) {
		return -1;
	}
	uint32_t root_id = (uint32_t)interface->identity_root_count++;
	interface->identity_roots[root_id] =
		(struct prototype_artifact_identity_root) {
			.source_type_claim_id = source_type_claim_id,
			.identity_family_has_type_claim_id =
				identity_family_has_type_claim_id,
			.witness_has_type_claim_id = witness_has_type_claim_id,
			.computation_rule = computation_rule
		};
	*p_root_id = root_id;
	return 0;
}

static int artifact_term_has_compiler_relation_head(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t head = term_id;
	for (size_t depth = 0; depth <= terms->term_count; ++depth) {
		const struct prototype_term* term = &terms->terms[head];
		if (term->tag != PROTOTYPE_TERM_APP) {
			return term->tag == PROTOTYPE_TERM_RELATION_TYPE_FORMER ||
				term->tag == PROTOTYPE_TERM_RELATION_WITNESS_FORMER;
		}
		if (term->as.app.function >= terms->term_count) {
			return -1;
		}
		head = term->as.app.function;
	}
	return -1;
}

int prototype_artifact_interface_validate_identity_roots(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !terms || !type_declarations || !contexts ||
		!dimension_operators || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < interface->identity_root_count; ++i) {
		const struct prototype_artifact_identity_root* root =
			&interface->identity_roots[i];
		const struct prototype_judgement_proposition* source =
			prototype_judgement_claim_proposition(
				judgement, root->source_type_claim_id
			);
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, root->identity_family_has_type_claim_id
			);
		if (!artifact_identity_family_matches_source(
				terms,
				type_declarations,
				contexts,
				dimension_operators,
				source,
				family,
				root->computation_rule
			)) {
			return -1;
		}
		if (root->witness_has_type_claim_id != PROTOTYPE_INVALID_ID) {
			const struct prototype_judgement_proposition* witness =
				prototype_judgement_claim_proposition(
					judgement, root->witness_has_type_claim_id
				);
			if (!witness || witness->kind !=
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				witness->context_id != family->context_id ||
				witness->classifier != family->subject ||
				artifact_term_has_compiler_relation_head(
					terms, witness->subject
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

void prototype_artifact_relocation_table_init(
	struct prototype_artifact_relocation_table* table,
	struct prototype_artifact_external_term_ref* external_term_refs,
	size_t external_term_ref_capacity,
	struct prototype_artifact_resolved_external_term_ref* resolved_external_term_refs,
	size_t resolved_external_term_ref_capacity,
	struct prototype_artifact_external_type_expr_ref* external_type_expr_refs,
	size_t external_type_expr_ref_capacity,
	struct prototype_artifact_resolved_external_type_expr_ref* resolved_external_type_expr_refs,
	size_t resolved_external_type_expr_ref_capacity,
	struct prototype_artifact_external_type_former_ref* external_type_former_refs,
	size_t external_type_former_ref_capacity,
	struct prototype_artifact_resolved_external_type_former_ref* resolved_external_type_former_refs,
	size_t resolved_external_type_former_ref_capacity,
	struct prototype_artifact_resolved_constructor_owner_ref* resolved_constructor_owner_refs,
	size_t resolved_constructor_owner_ref_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->external_term_refs = external_term_refs;
	table->external_term_ref_capacity = external_term_ref_capacity;
	table->resolved_external_term_refs = resolved_external_term_refs;
	table->resolved_external_term_ref_capacity = resolved_external_term_ref_capacity;
	table->external_type_expr_refs = external_type_expr_refs;
	table->external_type_expr_ref_capacity = external_type_expr_ref_capacity;
	table->resolved_external_type_expr_refs = resolved_external_type_expr_refs;
	table->resolved_external_type_expr_ref_capacity =
		resolved_external_type_expr_ref_capacity;
	table->external_type_former_refs = external_type_former_refs;
	table->external_type_former_ref_capacity = external_type_former_ref_capacity;
	table->resolved_external_type_former_refs = resolved_external_type_former_refs;
	table->resolved_external_type_former_ref_capacity =
		resolved_external_type_former_ref_capacity;
	table->resolved_constructor_owner_refs = resolved_constructor_owner_refs;
	table->resolved_constructor_owner_ref_capacity =
		resolved_constructor_owner_ref_capacity;
}

void prototype_artifact_debug_table_init(
	struct prototype_artifact_debug_table* table,
	struct prototype_artifact_debug_term_name* term_names,
	size_t term_name_capacity,
	struct prototype_artifact_debug_type_name* type_names,
	size_t type_name_capacity,
	struct prototype_artifact_debug_constructor_name* constructor_names,
	size_t constructor_name_capacity
) {
	if (!table) {
		return;
	}
	memset(table, 0, sizeof(*table));
	table->term_names = term_names;
	table->term_name_capacity = term_name_capacity;
	table->type_names = type_names;
	table->type_name_capacity = type_name_capacity;
	table->constructor_names = constructor_names;
	table->constructor_name_capacity = constructor_name_capacity;
}

static int artifact_interface_exports_type_name(
	const struct prototype_artifact_interface* interface,
	struct prototype_qualified_name name
) {
	if (!interface || name.name_symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].namespace_symbol_id ==
				name.namespace_symbol_id &&
			interface->type_exports[i].name_symbol_id == name.name_symbol_id) {
			return 1;
		}
	}
	return 0;
}

static int lookup_export_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement || !p_classifier) {
		return -1;
	}
	for (size_t i = judgement->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
}

static uint32_t find_export_source_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t operation,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement || operation == PROTOTYPE_INVALID_ID ||
		classifier == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t unique_claim = PROTOTYPE_INVALID_ID;
	int ambiguous = 0;
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, i);
		if (!claim) {
			continue;
		}
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_proposition_get(judgement, claim->proposition_id);
		if (proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->context_id != context_id || proposition->subject != subject ||
			proposition->classifier != classifier ||
			claim->closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (
			proposition->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
			proposition->authority_id == operation &&
			proposition->occurrence_id == operation) {
			return i;
		}
		if (unique_claim != PROTOTYPE_INVALID_ID) {
			ambiguous = 1;
		} else {
			unique_claim = i;
		}
	}
	return ambiguous ? PROTOTYPE_INVALID_ID : unique_claim;
}

/* A top-level name is a source projection onto an existing typed operation;
 * it does not introduce a new kernel judgement.  Preserve the exported name,
 * but publish evidence owned by the first non-NAME operation in its chain. */
static int resolve_export_evidence_occurrence(
	const struct prototype_compile_metadata* metadata,
	uint32_t operation,
	uint32_t* p_evidence_occurrence
) {
	if (!metadata || !p_evidence_occurrence) {
		return -1;
	}
	for (size_t depth = 0; depth <= metadata->typed_occurrences.occurrence_count; ++depth) {
		if (operation >= metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* node =
			&metadata->typed_occurrences.occurrences[operation];
		if (node->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			*p_evidence_occurrence = operation;
			return 0;
		}
		operation = node->wrapped_occurrence;
	}
	return -1;
}

int prototype_artifact_interface_build_from_metadata(
	struct prototype_artifact_interface* interface,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !intrinsic_environment || !metadata || !terms ||
		!type_declarations || !judgement) {
		return -1;
	}
	interface->intrinsic_environment_fingerprint =
		prototype_intrinsic_environment_fingerprint(intrinsic_environment);
	interface->default_integer_host_type =
		intrinsic_environment->default_integer_host_type;
	if (metadata->label_count > interface->term_export_capacity ||
		metadata->type_export_count > interface->type_export_capacity ||
		metadata->constructor_export_count > interface->constructor_export_capacity ||
		type_declarations->readback.parameter_count > interface->type_parameter_capacity ||
		type_declarations->readback.field_type_count > interface->constructor_field_type_expr_capacity ||
		type_declarations->readback.expr_count > interface->type_expr_capacity) {
		return -1;
	}

	interface->term_export_count = 0;
	interface->type_export_count = 0;
	interface->type_parameter_count = 0;
	interface->constructor_export_count = 0;
	interface->constructor_field_type_expr_count = 0;
	interface->type_expr_count = 0;
	interface->dependency_count = 0;

	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		interface->type_exprs[interface->type_expr_count++] =
			type_declarations->readback.exprs[i];
	}
	for (size_t i = 0; i < type_declarations->readback.parameter_count; ++i) {
		const struct prototype_type_parameter_declaration* parameter =
			&type_declarations->readback.parameter_declarations[i];
		struct prototype_artifact_type_parameter_export* export =
			&interface->type_parameters[interface->type_parameter_count++];
		export->binding_id = parameter->binding_id;
		export->name_symbol_id = parameter->name_symbol_id;
		export->type_expr = parameter->type_expr;
	}
	for (size_t i = 0; i < type_declarations->readback.field_type_count; ++i) {
		interface->constructor_field_type_exprs[
			interface->constructor_field_type_expr_count++
		] = type_declarations->readback.field_types[i];
	}

	for (size_t i = 0; i < metadata->label_count; ++i) {
		const struct prototype_compile_label* label = &metadata->labels[i];
		struct prototype_artifact_term_export* export =
			&interface->term_exports[interface->term_export_count++];
		export->namespace_symbol_id = -1;
		export->name_symbol_id = label->name_symbol_id;
		export->local_term = label->term;
		if (resolve_export_evidence_occurrence(
				metadata, label->exposed_occurrence, &export->occurrence
			) != 0) {
			return -1;
		}
		export->source_evidence.kind =
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID;
		export->source_evidence.id = PROTOTYPE_INVALID_ID;
		export->canonical_key = label->canonical_key;
		export->transparency = PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT;
		if (metadata->typed_occurrences.occurrences[export->occurrence].classifier !=
				PROTOTYPE_INVALID_ID) {
			export->classifier =
				metadata->typed_occurrences.occurrences[export->occurrence].classifier;
		} else if (label->exposed_classifier != PROTOTYPE_INVALID_ID) {
			export->classifier = label->exposed_classifier;
		} else if (lookup_export_classifier(judgement, label->term, &export->classifier) != 0) {
			export->classifier = PROTOTYPE_INVALID_ID;
		}
		memset(&export->classifier_key, 0, sizeof(export->classifier_key));
		if (export->classifier != PROTOTYPE_INVALID_ID &&
			prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				export->classifier,
				&export->classifier_key
			) != 0) {
			return -1;
		}
		if (export->classifier != PROTOTYPE_INVALID_ID) {
			uint32_t existing_classifier;
			int found_existing_classifier = prototype_internal_artifact_find_existing_term_by_canonical_key(
				terms,
				type_declarations,
				export->classifier,
				&export->classifier_key,
				export->classifier,
				&existing_classifier
			);
			if (found_existing_classifier < 0) {
				return -1;
			}
			if (found_existing_classifier > 0) {
				export->classifier = existing_classifier;
				if (prototype_term_canonical_key_with_types(
						terms,
						type_declarations,
						existing_classifier,
						&export->classifier_key
					) != 0) {
					return -1;
				}
			}
		}
		export->source_evidence.id = find_export_source_claim(
			judgement,
			export->occurrence,
			export->occurrence < metadata->typed_occurrences.occurrence_count ?
				metadata->typed_occurrences.occurrences[export->occurrence].context_id :
				PROTOTYPE_INVALID_ID,
			export->local_term,
			export->classifier
		);
		export->source_evidence.kind = export->source_evidence.id ==
			PROTOTYPE_INVALID_ID ?
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID :
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM;
		uint32_t existing_term;
		int found_existing = prototype_internal_artifact_find_existing_term_by_canonical_key(
			terms,
			type_declarations,
			export->local_term,
			&export->canonical_key,
			export->local_term,
			&existing_term
		);
		if (found_existing < 0) {
			return -1;
		}
		if (found_existing > 0) {
			export->local_term = existing_term;
			if (prototype_term_canonical_key_with_types(
					terms,
					type_declarations,
					existing_term,
					&export->canonical_key
				) != 0) {
				return -1;
			}
		}
	}

	for (size_t i = 0; i < metadata->type_export_count; ++i) {
		const struct prototype_compile_type_export* type_export =
			&metadata->type_exports[i];
		struct prototype_artifact_type_export* export =
			&interface->type_exports[interface->type_export_count++];
		export->namespace_symbol_id = -1;
		export->name_symbol_id = type_export->name_symbol_id;
		export->local_type_id = type_export->type_id;
		export->representation_fingerprint = type_export->representation_fingerprint;
		if (type_export->type_id >= type_declarations->semantic_schema.type_count) {
			return -1;
		}
			if (prototype_type_declaration_representation_anchor_type_id(
					terms,
					type_declarations,
					type_export->type_id,
					&export->core_representation_anchor_type_id
				) != 0) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[type_export->type_id];
		if (type->formation_classifier == PROTOTYPE_INVALID_ID ||
			type->formation_classifier >= terms->term_count) {
			return -1;
		}
		export->formation_classifier = type->formation_classifier;
		export->first_parameter = type->first_parameter;
		export->parameter_count = type->parameter_count;
		export->first_constructor_export = type_export->first_constructor_export;
		export->constructor_count = type_export->constructor_count;
	}

	for (size_t i = 0; i < metadata->constructor_export_count; ++i) {
		const struct prototype_compile_constructor_export* constructor_export =
			&metadata->constructor_exports[i];
		struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[interface->constructor_export_count++];
		export->type_export_index = constructor_export->type_export_index;
		export->name_symbol_id = constructor_export->name_symbol_id;
		export->ordinal = constructor_export->ordinal;
		export->readback_first_field_type = constructor_export->readback_first_field_type;
		export->readback_field_count = constructor_export->readback_field_count;
		export->curried_classifier_cache =
			constructor_export->curried_classifier_cache;
	}

	return 0;
}

int prototype_artifact_interface_add_dependency(
	struct prototype_artifact_interface* interface,
	int name_symbol_id
) {
	return prototype_artifact_interface_add_dependency_in_namespace(
		interface,
		-1,
		name_symbol_id
	);
}

int prototype_artifact_interface_add_dependency_in_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id
) {
	if (!interface || name_symbol_id < 0) {
		return -1;
	}
	for (size_t i = 0; i < interface->dependency_count; ++i) {
		if (interface->dependencies[i].namespace_symbol_id == namespace_symbol_id &&
			interface->dependencies[i].name_symbol_id == name_symbol_id) {
			return 0;
		}
	}
	if (interface->dependency_count >= interface->dependency_capacity) {
		return -1;
	}
	interface->dependencies[interface->dependency_count].namespace_symbol_id =
		namespace_symbol_id;
	interface->dependencies[interface->dependency_count].name_symbol_id = name_symbol_id;
	interface->dependency_count++;
	return 0;
}

void prototype_artifact_interface_set_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id
) {
	if (!interface) {
		return;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id < 0) {
			interface->term_exports[i].namespace_symbol_id = namespace_symbol_id;
		}
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].namespace_symbol_id < 0) {
			interface->type_exports[i].namespace_symbol_id = namespace_symbol_id;
		}
	}
}

static int collect_term_dependencies_at_depth(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth
) {
	if (!interface || !terms || term_id >= terms->term_count || depth > 256) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			if (artifact_interface_exports_term_name(interface, term->as.external_ref.name)) {
				return 0;
			}
			return prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				term->as.external_ref.name.namespace_symbol_id,
				term->as.external_ref.name.name_symbol_id
			);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.constructor.owner,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.app.function,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.app.argument,
				depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.lambda.body,
				depth + 1
			);
		case PROTOTYPE_TERM_PI:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.pi.domain,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.pi.codomain_family,
				depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.match.scrutinee,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count ||
					collect_term_dependencies_at_depth(
						interface,
						terms,
						terms->cases[case_id].body,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.type_view.core,
					depth + 1
				) != 0) {
				return -1;
			}
			return collect_term_dependencies_at_depth(
				interface,
				terms,
				term->as.type_view.source,
				depth + 1
			);
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.induction_hypothesis.argument,
					depth + 1
				);
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				if (collect_term_dependencies_at_depth(
						interface,
						terms,
						term->as.computation_type.label,
						depth + 1
					) != 0) {
					return -1;
				}
				return collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.computation_type.result,
					depth + 1
				);
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_union.left, depth + 1
				) == 0 ? collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_union.right, depth + 1
				) : -1;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.effect_row_forall.body, depth + 1
				);
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
				return collect_term_dependencies_at_depth(
					interface,
					terms,
					term->as.effect_row_operation.latent_row,
					depth + 1
				);
			case PROTOTYPE_TERM_THUNK_TYPE:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.thunk_type.computation, depth + 1
				);
			case PROTOTYPE_TERM_RETURN:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.return_term.value, depth + 1
				);
			case PROTOTYPE_TERM_THUNK:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.thunk.computation, depth + 1
				);
			case PROTOTYPE_TERM_FORCE:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.force.value, depth + 1
				);
			case PROTOTYPE_TERM_COMPUTATION_FOLD:
				if (collect_term_dependencies_at_depth(
						interface, terms, term->as.computation_fold.computation, depth + 1
					) != 0 || collect_term_dependencies_at_depth(
						interface, terms, term->as.computation_fold.return_clause, depth + 1
					) != 0) {
					return -1;
				}
				for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
					const struct prototype_computation_fold_clause* clause =
						&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
					if (collect_term_dependencies_at_depth(
							interface, terms, clause->operation, depth + 1
						) != 0 || collect_term_dependencies_at_depth(
							interface, terms, clause->body, depth + 1
						) != 0) {
						return -1;
					}
				}
				return 0;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				return collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.operation, depth + 1
				) == 0 && collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.argument, depth + 1
				) == 0 ? collect_term_dependencies_at_depth(
					interface, terms, term->as.operation_request.continuation, depth + 1
				) : -1;
			default:
				return 0;
	}
}

int prototype_artifact_interface_collect_dependencies(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!interface || !terms || !type_declarations || !judgement) {
		return -1;
	}
	interface->dependency_count = 0;
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (export->local_term < terms->term_count &&
			collect_term_dependencies_at_depth(interface, terms, export->local_term, 0) != 0) {
			return -1;
		}
		if (export->classifier < terms->term_count &&
			collect_term_dependencies_at_depth(interface, terms, export->classifier, 0) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		if (collect_term_dependencies_at_depth(interface, terms, relation->subject, 0) != 0 ||
			collect_term_dependencies_at_depth(interface, terms, relation->classifier, 0) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < type_declarations->readback.expr_count; ++i) {
		const struct prototype_type_expr* expr = &type_declarations->readback.exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(type_declarations, expr->as.name.symbol_id) &&
			prototype_artifact_interface_add_dependency(interface, expr->as.name.symbol_id) != 0) {
			return -1;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM &&
			!artifact_interface_exports_term_name(
				interface, expr->as.external_term.name
			) &&
			prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				expr->as.external_term.name.namespace_symbol_id,
				expr->as.external_term.name.name_symbol_id
			) != 0) {
			return -1;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE &&
			!artifact_interface_exports_type_name(
				interface, expr->as.imported_type.name
			) &&
			prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		const struct prototype_type_expr* expr = &interface->type_exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_NAME &&
			!prototype_type_declaration_lookup(
				type_declarations, expr->as.name.symbol_id
			) && prototype_artifact_interface_add_dependency(
				interface, expr->as.name.symbol_id
			) != 0) {
			return -1;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM &&
			!artifact_interface_exports_term_name(
				interface, expr->as.external_term.name
			) &&
			prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				expr->as.external_term.name.namespace_symbol_id,
				expr->as.external_term.name.name_symbol_id
			) != 0) {
			return -1;
		}
		if (expr->tag == PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE &&
			!artifact_interface_exports_type_name(
				interface, expr->as.imported_type.name
			) &&
			prototype_artifact_interface_add_dependency_in_namespace(
				interface,
				expr->as.imported_type.name.namespace_symbol_id,
				expr->as.imported_type.name.name_symbol_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_artifact_interface_build_definition_env(
	const struct prototype_artifact_interface* interface,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	struct prototype_term_definition_env* p_env
) {
	if (!interface || !definitions || !p_env ||
		interface->term_export_count > definition_capacity) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* export = &interface->term_exports[i];
		definitions[i].name = qualified_name_make(
			export->namespace_symbol_id,
			export->name_symbol_id
		);
		definitions[i].term = export->local_term;
		definitions[i].classifier = export->classifier;
		definitions[i].transparency =
			export->transparency == PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT ?
			PROTOTYPE_TERM_DEFINITION_TRANSPARENT :
			PROTOTYPE_TERM_DEFINITION_OPAQUE;
		definitions[i].canonical_key = export->canonical_key;
	}
	p_env->definitions = definitions;
	p_env->definition_count = interface->term_export_count;
	return 0;
}

uint32_t prototype_artifact_interface_next_universe_var(
	const struct prototype_artifact_interface* interface
) {
	uint32_t next = 0;
	if (!interface) {
		return 0;
	}
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		const struct prototype_type_expr* expr = &interface->type_exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR &&
			expr->as.universe_var.level_var >= next) {
			next = expr->as.universe_var.level_var + 1;
		}
	}
	return next;
}

int prototype_artifact_interface_renumber_universe_vars(
	struct prototype_artifact_interface* interface,
	uint32_t offset
) {
	if (!interface) {
		return -1;
	}
	if (offset == 0) {
		return 0;
	}
	for (size_t i = 0; i < interface->type_expr_count; ++i) {
		struct prototype_type_expr* expr = &interface->type_exprs[i];
		if (expr->tag == PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR) {
			expr->as.universe_var.level_var += offset;
		}
	}
	return 0;
}

int prototype_artifact_interface_find_term_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_term_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id == namespace_symbol_id &&
			interface->term_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_type_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_type_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id) {
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		if (interface->type_exports[i].namespace_symbol_id == namespace_symbol_id &&
			interface->type_exports[i].name_symbol_id == name_symbol_id) {
			*p_export_id = (uint32_t)i;
			return 0;
		}
	}
	return 1;
}

int prototype_artifact_interface_find_constructor_export(
	const struct prototype_artifact_interface* interface,
	uint32_t type_export_id,
	int name_symbol_id,
	uint32_t* p_export_id
) {
	if (!interface || !p_export_id || type_export_id >= interface->type_export_count) {
		return -1;
	}
	const struct prototype_artifact_type_export* type_export =
		&interface->type_exports[type_export_id];
	for (uint32_t i = 0; i < type_export->constructor_count; ++i) {
		uint32_t constructor_export_id = type_export->first_constructor_export + i;
		if (constructor_export_id >= interface->constructor_export_count) {
			return -1;
		}
		if (interface->constructor_exports[constructor_export_id].name_symbol_id ==
			name_symbol_id) {
			*p_export_id = constructor_export_id;
			return 0;
		}
	}
	return 1;
}
