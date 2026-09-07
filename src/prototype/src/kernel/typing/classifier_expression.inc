int prototype_classifier_totality_join(int left, int right) {
	if (left == PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE ||
		right == PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE) {
		return PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE;
	}
	if (left == PROTOTYPE_COMPUTATION_TOTALITY_TOTAL &&
		right == PROTOTYPE_COMPUTATION_TOTALITY_TOTAL) {
		return PROTOTYPE_COMPUTATION_TOTALITY_TOTAL;
	}
	return PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN;
}

int prototype_classifier_effect_row_purity(
	const struct prototype_term_db* terms,
	uint32_t row
) {
	struct prototype_effect_row_normal_form normal;
	if (prototype_term_effect_row_normal_form(terms, row, &normal) != 0) {
		return PROTOTYPE_EFFECT_ROW_PURITY_INVALID;
	}
	for (uint32_t i = 0; i < normal.atom_count; ++i) {
		const struct prototype_term* atom = &terms->terms[normal.atoms[i]];
		if (atom->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR ||
			prototype_term_effect_row_is_closed(terms, normal.atoms[i]) == 0) {
			return PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED;
		}
	}
	return normal.atom_count == 0 ?
		PROTOTYPE_EFFECT_ROW_PURITY_PURE :
		PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL;
}

int prototype_classifier_computation_type_is_pure_total(
	const struct prototype_term_db* terms,
	uint32_t computation_type
) {
	if (!terms || computation_type >= terms->term_count ||
		terms->terms[computation_type].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	const struct prototype_term* type = &terms->terms[computation_type];
	int purity = prototype_classifier_effect_row_purity(
		terms, type->as.computation_type.label
	);
	if (purity == PROTOTYPE_EFFECT_ROW_PURITY_INVALID) {
		return -1;
	}
	return purity == PROTOTYPE_EFFECT_ROW_PURITY_PURE &&
		type->as.computation_type.totality ==
			PROTOTYPE_COMPUTATION_TOTALITY_TOTAL;
}
