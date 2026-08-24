#include "a_program/kernel/kernel_view.h"

#include "a_program/kernel/cwf_certificate.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/type_declaration.h"

static uint64_t semantic_epoch_hash_u32(uint64_t hash, uint32_t value) {
	hash ^= value;
	return hash * UINT64_C(1099511628211);
}

static int type_schema_prefix_fingerprint(
	const struct prototype_type_semantic_schema_db* schema,
	size_t type_count,
	size_t constructor_count,
	uint64_t* p_fingerprint
) {
	if (!schema || !p_fingerprint || type_count > schema->type_count ||
		constructor_count > schema->constructor_count) {
		return -1;
	}
	uint64_t hash = UINT64_C(1469598103934665603);
	for (size_t i = 0; i < type_count; ++i) {
		const struct prototype_type_declaration* type =
			&schema->type_declarations[i];
		hash = semantic_epoch_hash_u32(hash, (uint32_t)type->name_symbol_id);
		hash = semantic_epoch_hash_u32(hash, (uint32_t)type->namespace_symbol_id);
		hash = semantic_epoch_hash_u32(hash, type->type_index);
		hash = semantic_epoch_hash_u32(hash, type->formation_classifier);
		hash = semantic_epoch_hash_u32(hash, type->parameter_context);
		hash = semantic_epoch_hash_u32(hash, type->parameter_count);
		hash = semantic_epoch_hash_u32(hash, type->index_context);
		hash = semantic_epoch_hash_u32(hash, type->index_count);
		hash = semantic_epoch_hash_u32(hash, type->first_constructor);
		hash = semantic_epoch_hash_u32(hash, type->constructor_count);
	}
	for (size_t i = 0; i < constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&schema->constructor_declarations[i];
		hash = semantic_epoch_hash_u32(hash, (uint32_t)constructor->name_symbol_id);
		hash = semantic_epoch_hash_u32(hash, constructor->owner_type);
		hash = semantic_epoch_hash_u32(hash, constructor->constructor_index);
		hash = semantic_epoch_hash_u32(hash, constructor->parameter_context);
		hash = semantic_epoch_hash_u32(hash, constructor->field_context);
		hash = semantic_epoch_hash_u32(hash, constructor->result_classifier);
		hash = semantic_epoch_hash_u32(hash, constructor->schema_revision);
	}
	*p_fingerprint = hash;
	return 0;
}

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

int prototype_kernel_semantic_epoch_capture(
	const struct prototype_kernel_view* view,
	struct prototype_kernel_semantic_epoch* p_epoch
) {
	if (!p_epoch || prototype_kernel_view_validate_stores(view) != 0) {
		return -1;
	}
	*p_epoch = (struct prototype_kernel_semantic_epoch) {
		.type_schema_type_count =
			view->type_declarations->semantic_schema.type_count,
		.type_schema_constructor_count =
			view->type_declarations->semantic_schema.constructor_count,
		.context_revision = view->contexts->semantic_revision,
		.substitution_revision = view->substitutions->semantic_revision,
		.judgement_revision = view->judgement->semantic_revision
	};
	return type_schema_prefix_fingerprint(
		&view->type_declarations->semantic_schema,
		(size_t)p_epoch->type_schema_type_count,
		(size_t)p_epoch->type_schema_constructor_count,
		&p_epoch->type_schema_revision
	);
}

int prototype_kernel_semantic_epoch_equal(
	const struct prototype_kernel_semantic_epoch* left,
	const struct prototype_kernel_semantic_epoch* right
) {
	return left && right &&
		left->type_schema_revision == right->type_schema_revision &&
		left->type_schema_type_count == right->type_schema_type_count &&
		left->type_schema_constructor_count ==
			right->type_schema_constructor_count &&
		left->context_revision == right->context_revision &&
		left->substitution_revision == right->substitution_revision &&
		left->judgement_revision == right->judgement_revision;
}

int prototype_kernel_semantic_epoch_matches(
	const struct prototype_kernel_view* view,
	const struct prototype_kernel_semantic_epoch* epoch
) {
	if (!epoch || prototype_kernel_view_validate_stores(view) != 0 ||
		view->contexts->semantic_revision != epoch->context_revision ||
		view->substitutions->semantic_revision != epoch->substitution_revision ||
		view->judgement->semantic_revision != epoch->judgement_revision) {
		return 0;
	}
	uint64_t fingerprint;
	return type_schema_prefix_fingerprint(
		&view->type_declarations->semantic_schema,
		(size_t)epoch->type_schema_type_count,
		(size_t)epoch->type_schema_constructor_count,
		&fingerprint
	) == 0 && fingerprint == epoch->type_schema_revision;
}
