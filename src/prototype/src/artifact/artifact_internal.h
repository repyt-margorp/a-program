#ifndef A_PROGRAM_PROTOTYPE_ARTIFACT_INTERNAL_H
#define A_PROGRAM_PROTOTYPE_ARTIFACT_INTERNAL_H

int prototype_internal_canonicalize_type_view_core_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts
);

int prototype_internal_canonicalize_constructor_owner_refs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count
);

#ifdef A_PROGRAM_ARTIFACT_DEFINE_RUNTIME_CAPABILITY_HELPER
static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
) {
	if (!metadata || !terms) {
		return;
	}
	uint64_t capabilities = 0;
	struct prototype_verification_coverage coverage;
	if (prototype_verification_db_coverage(
			&metadata->verification, &coverage
		) == 0) {
		capabilities |= coverage.required_runtime_capabilities;
	}
	for (size_t i = 0; i < metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &metadata->operations[i];
		if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
			operation->core_term < terms->term_count &&
			terms->terms[operation->core_term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD &&
			terms->terms[operation->core_term].as.computation_fold.clause_count != 0) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
		}
		if (operation->tag != PROTOTYPE_OPERATION_REQUEST ||
			operation->core_term >= terms->term_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_OPERATION_REQUEST) {
			continue;
		}
		capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH;
		uint32_t head = terms->terms[operation->core_term].as.operation_request.operation;
		while (head < terms->term_count && terms->terms[head].tag == PROTOTYPE_TERM_APP) {
			head = terms->terms[head].as.app.function;
		}
		if (head < terms->term_count &&
			terms->terms[head].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(
					terms->terms[head].as.effect_operation.operation_id
				);
			if (declaration &&
				(declaration->required_host_effects &
					PROTOTYPE_HOST_EFFECT_TERMINAL) != 0) {
				capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
			}
		}
	}
	metadata->required_runtime_capabilities = capabilities;
}
#endif

#endif
