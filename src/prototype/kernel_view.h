#ifndef __PROTOTYPE_KERNEL_VIEW_H__
#define __PROTOTYPE_KERNEL_VIEW_H__

struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_cwf_certificate_db;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_operation_graph;
struct prototype_judgement_db;

/* A checked capability bundle. Term reduction may extend TermDB with computed
 * nodes, but the view cannot publish Context, Substitution, or Judgement
 * records. */
struct prototype_kernel_view {
	const struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	const struct prototype_cwf_certificate_db* cwf_certificates;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_operation_graph* operations;
	const struct prototype_judgement_db* judgement;
};

struct prototype_kernel_builder {
	struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	struct prototype_cwf_certificate_db* cwf_certificates;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_operation_graph* operations;
	struct prototype_judgement_db* judgement;
};

int prototype_kernel_view_validate(
	const struct prototype_kernel_view* view
);
int prototype_kernel_builder_validate(
	const struct prototype_kernel_builder* builder
);
int prototype_kernel_builder_view(
	const struct prototype_kernel_builder* builder,
	struct prototype_kernel_view* p_view
);

#endif
