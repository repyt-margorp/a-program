#include "kernel_view.h"

#include "cwf_certificate.h"

int prototype_kernel_view_validate(
	const struct prototype_kernel_view* view
) {
	if (!view || !view->contexts || !view->substitutions ||
		!view->cwf_certificates || !view->terms || !view->type_declarations ||
		!view->operations || !view->judgement) {
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

int prototype_kernel_builder_validate(
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
		.operations = builder->operations,
		.judgement = builder->judgement
	};
	return prototype_kernel_view_validate(&view);
}

int prototype_kernel_builder_view(
	const struct prototype_kernel_builder* builder,
	struct prototype_kernel_view* p_view
) {
	if (!p_view || prototype_kernel_builder_validate(builder) != 0) {
		return -1;
	}
	*p_view = (struct prototype_kernel_view) {
		.contexts = builder->contexts,
		.substitutions = builder->substitutions,
		.cwf_certificates = builder->cwf_certificates,
		.terms = builder->terms,
		.type_declarations = builder->type_declarations,
		.operations = builder->operations,
		.judgement = builder->judgement
	};
	return 0;
}
