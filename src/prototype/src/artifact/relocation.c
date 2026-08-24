#include "a_program/artifact/interface.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "artifact_internal.h"
#include "artifact_graph_internal.h"

static int artifact_existing_term_representative(
	const struct prototype_term_db* terms,
	uint32_t term,
	uint32_t* p_representative
) {
	if (!terms || !p_representative || term >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < term; ++i) {
		int equal = 0;
		if (!artifact_term_present(&terms->terms[i])) {
			continue;
		}
		if (prototype_term_view_shape_equal(terms, i, term, &equal) != 0) {
			return -1;
		}
		if (equal) {
			*p_representative = i;
			return 0;
		}
	}
	*p_representative = term;
	return 0;
}

static int attach_linked_declaration_support(
	struct prototype_judgement_db* judgement,
	uint32_t declaration_proposition_id,
	uint32_t source_proposition_id
) {
	if (!judgement || declaration_proposition_id >= judgement->proposition_count ||
		source_proposition_id >= judgement->proposition_count ||
		declaration_proposition_id == source_proposition_id) {
		return -1;
	}
	const struct prototype_judgement_proposition* source =
		&judgement->propositions[source_proposition_id];
	if (source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return -1;
	}
	uint32_t source_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		if (judgement->claims[i].proposition_id != source_proposition_id) {
			continue;
		}
		if (source_claim_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		source_claim_id = i;
	}
	if (source_claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	int attached = 0;
	for (size_t i = 0; i < judgement->derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* derivation =
			&judgement->derivation_candidates[i];
		if (derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_DECLARATION ||
			derivation->conclusion_proposition_id != declaration_proposition_id) {
			continue;
		}
		if (derivation->premise_count != 0) {
			return -1;
		}
		if (judgement->candidate_premise_count >=
			judgement->candidate_premise_capacity) {
			return -1;
		}
		derivation->premises = &judgement->candidate_premises[
			judgement->candidate_premise_count++
		];
		derivation->premise_count = 1;
		derivation->premises[0].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
		derivation->premises[0].proposition_id =
			judgement->claims[source_claim_id].proposition_id;
		derivation->premises[0].proposition =
			source;
		derivation->premises[0].builder_proposition = NULL;
		derivation->premises[0].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		derivation->premises[0].semantic_action_id = PROTOTYPE_INVALID_ID;
		attached = 1;
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		const struct prototype_judgement_claim* conclusion =
			prototype_judgement_claim_get(
				judgement, derivation->conclusion_claim_id
			);
		if (derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_DECLARATION ||
			!conclusion || conclusion->proposition_id !=
				declaration_proposition_id) {
			continue;
		}
		if (derivation->premise_count != 0 ||
			judgement->accepted_premise_count >=
				judgement->accepted_premise_capacity) {
			return -1;
		}
		derivation->premises = &judgement->accepted_premises[
			judgement->accepted_premise_count++
		];
		derivation->premise_count = 1;
		derivation->premises[0].claim_id = source_claim_id;
		derivation->premises[0].scoped_proposition_id = PROTOTYPE_INVALID_ID;
		derivation->premises[0].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		derivation->premises[0].semantic_action_id = PROTOTYPE_INVALID_ID;
		derivation->closure_rank = PROTOTYPE_INVALID_ID;
		derivation->key_hash = 0;
		derivation->hash_next = PROTOTYPE_INVALID_ID;
		attached = 1;
	}
	return attached ? 0 : -1;
}

static int artifact_export_source_proposition(
	const struct prototype_artifact_term_export* export,
	const struct prototype_judgement_db* judgement,
	uint32_t* p_proposition_id
) {
	if (!export || !judgement || !p_proposition_id) {
		return -1;
	}
	if ((export->source_evidence.kind !=
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM &&
		export->source_evidence.kind !=
			PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CONDITIONAL) ||
		export->source_evidence.id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, export->source_evidence.id);
	if (!claim) {
		return -1;
	}
	uint32_t proposition_id = claim->proposition_id;
	const struct prototype_judgement_proposition* proposition =
		prototype_judgement_proposition_get(judgement, proposition_id);
	if (!proposition || proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proposition->subject != export->local_term ||
		proposition->classifier != export->classifier) {
		return -1;
	}
	*p_proposition_id = proposition_id;
	return 0;
}

int prototype_artifact_apply_term_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_compile_metadata* target_metadata,
	const struct prototype_artifact_interface* provider_interface
) {
	if (!target_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !target_contexts || !target_metadata ||
		!provider_interface) {
		return -1;
	}
	for (size_t i = 0; i < provider_interface->term_export_count; ++i) {
		const struct prototype_artifact_term_export* provider =
			&provider_interface->term_exports[i];
		struct prototype_qualified_name provider_name = qualified_name_make(
			provider->namespace_symbol_id,
			provider->name_symbol_id
		);
		uint32_t provider_term;
		if (provider->local_term >= target_terms->term_count) {
			return -1;
		}
		/* A residual export has no grounded source Claim and therefore cannot
		 * discharge an external declaration during relocation. Its verification
		 * obligation is validated at the final artifact boundary. */
		if (provider->source_evidence.kind !=
				PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM) {
			continue;
		}
		uint32_t provider_proposition_id;
		if (artifact_export_source_proposition(
				provider, target_judgement, &provider_proposition_id
			) != 0) {
			return -1;
		}
		if (provider->transparency != PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT) {
			continue;
		}
		if (artifact_existing_term_representative(
				target_terms, provider->local_term, &provider_term
			) != 0) {
			return -1;
		}
		for (size_t j = 0; j < target_interface->term_export_count; ++j) {
			struct prototype_artifact_term_export* target =
				&target_interface->term_exports[j];
			uint32_t linked;
			if (target->local_term >= target_terms->term_count ||
				prototype_term_resolve_external_ref(
					target_terms,
					target->local_term,
					provider_name,
					provider_term,
					&linked
				) != 0) {
				return -1;
			}
			target->local_term = linked;
			if (prototype_term_canonical_key_with_types(
					target_terms,
					target_type_declarations,
					target->local_term,
					&target->canonical_key
				) != 0) {
				return -1;
			}
			if (target->classifier < target_terms->term_count &&
				prototype_term_resolve_external_ref(
					target_terms,
					target->classifier,
					provider_name,
					provider_term,
					&linked
				) != 0) {
				return -1;
			}
			if (target->classifier < target_terms->term_count) {
				target->classifier = linked;
				if (prototype_term_canonical_key_with_types(
						target_terms,
						target_type_declarations,
						target->classifier,
						&target->classifier_key
					) != 0) {
					return -1;
				}
			} else {
				memset(&target->classifier_key, 0, sizeof(target->classifier_key));
			}
		}
		for (size_t j = 0; j < target_judgement->proposition_count; ++j) {
			struct prototype_judgement_proposition* relation =
				&target_judgement->propositions[j];
			uint32_t original_subject;
			int subject_was_external_ref;
			uint32_t linked_subject;
			uint32_t linked_classifier;
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
				continue;
			}
			original_subject = relation->subject;
			subject_was_external_ref =
				original_subject < target_terms->term_count &&
				target_terms->terms[original_subject].tag ==
					PROTOTYPE_TERM_EXTERNAL_REF;
			if (relation->subject >= target_terms->term_count ||
				relation->classifier >= target_terms->term_count ||
				prototype_term_resolve_external_ref(
					target_terms,
					relation->subject,
					provider_name,
					provider_term,
					&linked_subject
				) != 0 ||
				prototype_term_resolve_external_ref(
					target_terms,
					relation->classifier,
					provider_name,
					provider_term,
					&linked_classifier
				) != 0) {
				return -1;
			}
			relation->subject = linked_subject;
			relation->classifier = linked_classifier;
			if (subject_was_external_ref && linked_subject != original_subject &&
				attach_linked_declaration_support(
					target_judgement,
					(uint32_t)j,
					provider_proposition_id
				) != 0) {
				int has_declaration = 0;
				for (size_t k = 0;
					k < target_judgement->derivation_candidate_count;
					++k) {
					const struct prototype_judgement_derivation_candidate* proof =
						&target_judgement->derivation_candidates[k];
					if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_DECLARATION &&
						proof->conclusion_proposition_id == j) {
						has_declaration = 1;
						break;
					}
				}
				if (has_declaration) {
					return -1;
				}
			}
		}
		for (size_t j = 0; j < target_metadata->typed_occurrences.occurrence_count; ++j) {
			struct prototype_typed_occurrence* operation =
				&target_metadata->typed_occurrences.occurrences[j];
			uint32_t* projections[] = {
				&operation->core_term,
				&operation->classifier,
				&operation->binder_classifier
			};
			for (size_t projection = 0;
				projection < sizeof(projections) / sizeof(projections[0]);
				++projection) {
				if (*projections[projection] == PROTOTYPE_INVALID_ID) {
					continue;
				}
				if (prototype_term_resolve_external_ref(
						target_terms,
						*projections[projection],
						provider_name,
						provider_term,
						projections[projection]
					) != 0) {
					return -1;
				}
			}
		}
		for (size_t j = 1; j < target_contexts->context_count; ++j) {
			struct prototype_context* context = &target_contexts->contexts[j];
			if (prototype_context_classifier_term(context) !=
					PROTOTYPE_INVALID_ID &&
				prototype_term_resolve_external_ref(
					target_terms,
					prototype_context_classifier_term(context),
					provider_name,
					provider_term,
					&context->classifier_ref.term_id
				) != 0) {
				return -1;
			}
		}
		if (prototype_context_db_rebuild_runtime_index_after_bulk_load(
				target_contexts
			) != 0) {
			return -1;
		}
		for (size_t j = 0;
			j < target_metadata->substitutions.substitution_count;
			++j) {
			struct prototype_substitution* substitution =
				&target_metadata->substitutions.substitutions[j];
			if (substitution->term != PROTOTYPE_INVALID_ID &&
				prototype_term_resolve_external_ref(
					target_terms,
					substitution->term,
					provider_name,
					provider_term,
					&substitution->term
				) != 0) {
				return -1;
			}
			if (substitution->term_classifier != PROTOTYPE_INVALID_ID &&
				prototype_term_resolve_external_ref(
					target_terms,
					substitution->term_classifier,
					provider_name,
					provider_term,
					&substitution->term_classifier
				) != 0) {
				return -1;
			}
		}
		for (size_t j = 0;
			j < target_judgement->derivation_candidate_count;
			++j) {
			struct prototype_judgement_derivation_candidate* proof =
				&target_judgement->derivation_candidates[j];
			if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
				continue;
			}
			if (proof->conclusion_proposition_id >=
				target_judgement->proposition_count) {
				return -1;
			}
			proof->conclusion = &target_judgement->propositions[
				proof->conclusion_proposition_id
			];
			if (proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
				if (prototype_term_resolve_external_ref(
						target_terms,
						proof->rule_data.induction.match,
						provider_name,
						provider_term,
						&proof->rule_data.induction.match
					) != 0 || prototype_term_resolve_external_ref(
						target_terms,
						proof->rule_data.induction.motive,
						provider_name,
						provider_term,
						&proof->rule_data.induction.motive
					) != 0) {
					return -1;
				}
			} else if ((proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
				proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) &&
				prototype_term_resolve_external_ref(
					target_terms,
					proof->rule_data.constructor.owner_view,
					provider_name,
					provider_term,
					&proof->rule_data.constructor.owner_view
				) != 0) {
				return -1;
			}
		}
	}
	prototype_term_notify_graph_mutation(target_terms);
	return prototype_artifact_interface_collect_dependencies(
		target_interface,
		target_terms,
		target_type_declarations,
		target_judgement
	);
}

int prototype_artifact_interface_recompute_keys(
	struct prototype_artifact_interface* interface,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts
) {
	if (!interface || !terms || !type_declarations || !contexts) {
		return -1;
	}
	if (prototype_constructor_curried_caches_rebuild(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache, contexts, terms
		) != 0) {
		fprintf(stderr, "artifact key recomputation failed rebuilding constructor caches\n");
		return -1;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		struct prototype_artifact_type_export* export = &interface->type_exports[i];
			if (export->local_type_id >= type_declarations->semantic_schema.type_count ||
				prototype_type_declaration_representation_fingerprint(
					terms,
					type_declarations,
					contexts,
					export->local_type_id,
					&export->representation_fingerprint
				) != 0) {
			fprintf(stderr, "artifact key recomputation failed type export=%zu\n", i);
			return -1;
			}
			uint32_t formation_classifier = type_declarations->semantic_schema.type_declarations[
				export->local_type_id
			].formation_classifier;
		if (formation_classifier == PROTOTYPE_INVALID_ID ||
			formation_classifier >= terms->term_count) {
			fprintf(
				stderr,
				"artifact key recomputation invalid formation classifier type export=%zu "
				"classifier=%u terms=%zu\n",
				i,
				formation_classifier,
				terms->term_count
			);
			return -1;
			}
			export->formation_classifier = formation_classifier;
			if (prototype_type_declaration_representation_anchor_type_id(
					terms,
					type_declarations,
					export->local_type_id,
					&export->core_representation_anchor_type_id
				) != 0) {
			fprintf(stderr, "artifact key recomputation failed representation export=%zu\n", i);
			return -1;
			}
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		struct prototype_artifact_term_export* export = &interface->term_exports[i];
		if (export->local_term >= terms->term_count ||
			prototype_term_canonical_key_with_types(
				terms,
				type_declarations,
				export->local_term,
				&export->canonical_key
			) != 0) {
			fprintf(stderr, "artifact key recomputation failed term export=%zu\n", i);
			return -1;
		}
		memset(&export->classifier_key, 0, sizeof(export->classifier_key));
		if (export->classifier != PROTOTYPE_INVALID_ID &&
			(export->classifier >= terms->term_count ||
				prototype_term_canonical_key_with_types(
					terms,
					type_declarations,
					export->classifier,
					&export->classifier_key
				) != 0)) {
			fprintf(
				stderr,
				"artifact key recomputation failed classifier export=%zu classifier=%u\n",
				i,
				export->classifier
			);
			return -1;
		}
	}
	for (size_t i = 0; i < interface->constructor_export_count; ++i) {
		struct prototype_artifact_constructor_export* export =
			&interface->constructor_exports[i];
		if (export->type_export_index >= interface->type_export_count) {
			fprintf(stderr, "artifact key recomputation invalid constructor owner export=%zu\n", i);
			return -1;
		}
		uint32_t type_id = interface->type_exports[
			export->type_export_index
		].local_type_id;
		if (type_id >= type_declarations->semantic_schema.type_count) {
			fprintf(stderr, "artifact key recomputation missing constructor type export=%zu\n", i);
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[type_id];
		if (export->ordinal >= type->constructor_count) {
			fprintf(stderr, "artifact key recomputation invalid constructor ordinal export=%zu\n", i);
			return -1;
		}
		const struct prototype_constructor_classifier_cache_entry* cache =
			prototype_type_constructor_classifier_cache_get(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache,
				type->first_constructor + export->ordinal
			);
		if (!cache) {
			return -1;
		}
		export->constructor_classifier = cache->classifier;
	}
	return 0;
}

int prototype_artifact_apply_type_expr_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_context_db* target_contexts,
	const struct prototype_artifact_interface* provider_interface
) {
	if (!target_interface || !target_terms || !target_type_declarations ||
		!target_judgement || !target_contexts || !provider_interface) {
		return -1;
	}

	for (size_t i = 0; i < target_type_declarations->readback.expr_count; ++i) {
		struct prototype_type_expr* expr = &target_type_declarations->readback.exprs[i];
		if (expr->tag != PROTOTYPE_TYPE_EXPR_NAME) {
			continue;
		}
		uint32_t export_id;
		int found = prototype_artifact_interface_find_type_export(
			provider_interface,
			expr->as.name.symbol_id,
			&export_id
		);
		if (found < 0) {
			return -1;
		}
		if (found > 0) {
			continue;
		}
		int symbol_id = expr->as.name.symbol_id;
		const struct prototype_artifact_type_export* export =
			&provider_interface->type_exports[export_id];
		memset(expr, 0, sizeof(*expr));
		expr->tag = PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE;
		expr->as.imported_type.name = qualified_name_make(
			export->namespace_symbol_id,
			symbol_id
		);
		expr->as.imported_type.representation_fingerprint = export->representation_fingerprint;
	}

	if (prototype_artifact_interface_recompute_keys(
		target_interface,
		target_terms,
		target_type_declarations,
		target_contexts
		) != 0) {
		return -1;
	}
	return prototype_artifact_interface_collect_dependencies(
		target_interface,
		target_terms,
		target_type_declarations,
		target_judgement
	);
}
