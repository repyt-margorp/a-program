#ifndef __PROTOTYPE_KERNEL_VIEW_H__
#define __PROTOTYPE_KERNEL_VIEW_H__

#include <stdint.h>

struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_cwf_certificate_db;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_typed_occurrence_graph;
struct prototype_judgement_db;
struct prototype_dimension_operator_db;

/* A checked capability bundle. Term reduction may extend TermDB with computed
 * nodes, but the view cannot publish Context, Substitution, or Judgement
 * records. */
struct prototype_kernel_view {
	const struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	const struct prototype_cwf_certificate_db* cwf_certificates;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_typed_occurrence_graph* occurrences;
	const struct prototype_judgement_db* judgement;
	const struct prototype_dimension_operator_db* dimension_operators;
};

struct prototype_kernel_builder {
	struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	struct prototype_cwf_certificate_db* cwf_certificates;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_typed_occurrence_graph* occurrences;
	struct prototype_judgement_db* judgement;
	struct prototype_dimension_operator_db* dimension_operators;
};

struct prototype_kernel_semantic_epoch {
	uint64_t type_schema_revision;
	uint64_t type_schema_type_count;
	uint64_t type_schema_constructor_count;
	uint64_t context_revision;
	uint64_t substitution_revision;
	uint64_t judgement_revision;
};

/* Validates the component stores and every certificate already present. It
 * deliberately does not assert global certificate coverage for all candidate
 * substitutions; consumers validate explicit certified roots. */
int prototype_kernel_view_validate_stores(
	const struct prototype_kernel_view* view
);
int prototype_kernel_builder_validate_stores(
	const struct prototype_kernel_builder* builder
);
int prototype_kernel_builder_view(
	const struct prototype_kernel_builder* builder,
	struct prototype_kernel_view* p_view
);
int prototype_kernel_semantic_epoch_capture(
	const struct prototype_kernel_view* view,
	struct prototype_kernel_semantic_epoch* p_epoch
);
int prototype_kernel_semantic_epoch_equal(
	const struct prototype_kernel_semantic_epoch* left,
	const struct prototype_kernel_semantic_epoch* right
);
int prototype_kernel_semantic_epoch_matches(
	const struct prototype_kernel_view* view,
	const struct prototype_kernel_semantic_epoch* epoch
);

#endif
