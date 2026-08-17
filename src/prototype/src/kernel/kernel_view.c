#include "a_program/kernel/kernel_view.h"

#include "a_program/kernel/cwf_certificate.h"

int prototype_kernel_view_validate_stores(
	const struct prototype_kernel_view* view
) {
	if (!view || !view->contexts || !view->substitutions ||
		!view->cwf_certificates || !view->terms || !view->type_declarations ||
		!view->occurrences || !view->judgement || !view->dimension_operators) {
		return -1;
	}
	return prototype_cwf_certificate_db_validate(
		view->cwf_certificates,
		view->contexts,
		view->substitutions,
		view->terms,
		view->type_declarations,
		view->judgement
	);
}

int prototype_kernel_builder_validate_stores(
	const struct prototype_kernel_builder* builder
) {
	if (!builder) {
		return -1;
	}
	struct prototype_kernel_view view = {
		.contexts = builder->contexts,
		.substitutions = builder->substitutions,
		.cwf_certificates = builder->cwf_certificates,
		.terms = builder->terms,
		.type_declarations = builder->type_declarations,
		.occurrences = builder->occurrences,
		.judgement = builder->judgement,
		.dimension_operators = builder->dimension_operators
	};
	return prototype_kernel_view_validate_stores(&view);
}

int prototype_kernel_builder_view(
	const struct prototype_kernel_builder* builder,
	struct prototype_kernel_view* p_view
) {
	if (!p_view || prototype_kernel_builder_validate_stores(builder) != 0) {
		return -1;
	}
	*p_view = (struct prototype_kernel_view) {
		.contexts = builder->contexts,
		.substitutions = builder->substitutions,
		.cwf_certificates = builder->cwf_certificates,
		.terms = builder->terms,
		.type_declarations = builder->type_declarations,
		.occurrences = builder->occurrences,
		.judgement = builder->judgement,
		.dimension_operators = builder->dimension_operators
	};
	return 0;
}
