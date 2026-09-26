#ifndef A_PROGRAM_PROTOTYPE_CORE_PIPELINE_H
#define A_PROGRAM_PROTOTYPE_CORE_PIPELINE_H

#include "a_program/core/inspect.h"
#include "a_program/core/read.h"
#include "a_program/core/request.h"
#include "a_program/core/term.h"

/* Driver-provided backing memory. Ownership of the mutable database assembled
 * over this memory belongs to CorePipeline, not to the driver or Layer T. */
struct prototype_core_pipeline_storage {
	struct prototype_term* terms;
	size_t term_capacity;
	struct prototype_match_case* cases;
	int* case_label_symbols;
	size_t case_capacity;
	struct prototype_case_binder* case_binders;
	size_t case_binder_capacity;
	struct prototype_ih_scope* ih_scopes;
	size_t ih_scope_capacity;
};

/* Mutable owner of Layer C calculation state. It deliberately contains no
 * typed occurrence, Context, constraint, or proof capability. */
struct prototype_core_pipeline {
	struct prototype_term_db terms;
	const struct prototype_term_definition_env* definitions;
	uint64_t transaction_revision;
	struct {
		size_t term_count;
		size_t case_count;
		size_t case_binder_count;
		size_t ih_scope_count;
		size_t computation_fold_clause_count;
		uint32_t next_binding_id;
		uint32_t next_universe_level_id;
		uint32_t scope_bindings[PROTOTYPE_SCOPE_BINDING_CAPACITY];
	} transaction_mark;
	int transaction_active;
	int initialized;
};

int prototype_core_pipeline_init(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_pipeline_storage* storage,
	const struct prototype_term_definition_env* definitions
);

void prototype_core_pipeline_dispose(
	struct prototype_core_pipeline* pipeline
);

struct prototype_term_db* prototype_core_pipeline_terms(
	struct prototype_core_pipeline* pipeline
);

const struct prototype_term_db* prototype_core_pipeline_terms_const(
	const struct prototype_core_pipeline* pipeline
);

int prototype_core_pipeline_begin_transaction(
	struct prototype_core_pipeline* pipeline
);

int prototype_core_pipeline_commit_transaction(
	struct prototype_core_pipeline* pipeline
);

int prototype_core_pipeline_rollback_transaction(
	struct prototype_core_pipeline* pipeline
);

int prototype_core_pipeline_read_snapshot(
	const struct prototype_core_pipeline* pipeline,
	struct prototype_core_read_snapshot* snapshot
);

int prototype_core_pipeline_normalize(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_normalization_request* request,
	struct prototype_core_normalization_response* response
);

int prototype_core_pipeline_project_structural_return(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_structural_return_projection_request* request,
	struct prototype_core_structural_return_projection_response* response
);

int prototype_core_pipeline_inspect(
	const struct prototype_core_pipeline* pipeline,
	const struct prototype_core_inspect_request* request,
	struct prototype_core_inspect_response* response
);

int prototype_core_pipeline_form(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* response
);

int prototype_core_pipeline_rewrite(
	struct prototype_core_pipeline* pipeline,
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_rewrite_response* response
);

#endif
