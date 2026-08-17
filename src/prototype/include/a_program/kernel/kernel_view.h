#ifndef __PROTOTYPE_KERNEL_VIEW_H__
#define __PROTOTYPE_KERNEL_VIEW_H__

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

#endif
