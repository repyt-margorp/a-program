#include "a_program/checker/session.h"
#include "internal.h"

#include <stdlib.h>
#include <string.h>

#define PROTOTYPE_CHECKED_MODULE_SEAL UINT64_C(0x4150524f4743484b)
#define PROTOTYPE_CHECKED_EXPORT_SEAL UINT64_C(0x4150524f47455850)

struct prototype_checked_export_ref {
	uint64_t seal;
	const struct prototype_checked_module* owner;
	int kind;
	size_t index;
};

struct prototype_checked_module {
	uint64_t seal;
	const struct prototype_elaborated_module_view* elaborated;
	struct prototype_checker_universe_solution* universe_solutions;
	size_t universe_solution_count;
	struct prototype_checker_usage_solution* usage_solutions;
	struct prototype_usage_entry* usage_entries;
	size_t usage_entry_count;
	struct prototype_checked_export_ref* exports;
	size_t export_count;
};

struct checker_state {
	const struct prototype_elaborated_module_view* module;
	const struct prototype_checked_module* const* imported_bases;
	size_t imported_base_count;
	const struct prototype_checker_universe_solution* universe_solutions;
	size_t universe_solution_count;
	struct prototype_effort_account* effort;
	uint64_t effort_start;
	int stop_reason;
	uint32_t subject;
};

static const struct prototype_term* checker_term(
	const struct checker_state* state,
	uint32_t term_id
);

static int checker_term_has_type_after_bindings(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t classifier_id,
	const uint32_t* inherited_binding_ids,
	const uint32_t* inherited_argument_ids,
	size_t inherited_binding_count,
	uint32_t depth
);

static int checker_pi_parts(
	const struct checker_state* state,
	uint32_t classifier_id,
	uint32_t* p_domain,
	uint32_t* p_binding,
	uint32_t* p_body
);

static int checker_term_equal_after_bindings(
	const struct checker_state* state,
	uint32_t source_id,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count,
	uint32_t target_id,
	uint32_t depth
);

static int checker_is_type_with_locals(
	struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	const uint32_t* local_binding_ids,
	const uint32_t* local_classifier_ids,
	size_t local_count,
	uint32_t depth
);

static int checker_type_instance_arguments(
	const struct checker_state* state,
	uint32_t classifier_id,
	uint32_t* p_type_id,
	uint32_t* arguments,
	size_t capacity,
	size_t* p_argument_count
);

static int checker_indexed_motive_application_equal(
	const struct checker_state* state,
	uint32_t motive_id,
	const uint32_t* index_arguments,
	size_t index_count,
	uint32_t value,
	const uint32_t* inherited_bindings,
	const uint32_t* inherited_arguments,
	size_t inherited_count,
	uint32_t target,
	int compare_computation_result
);

static int checker_stop(
	struct checker_state* state,
	int status,
	int reason,
	uint32_t subject
);

static int checker_charge(struct checker_state* state, uint32_t subject) {
	int effort_status = prototype_effort_consume(
		state->effort, PROTOTYPE_EFFORT_PHASE_CHECKER, 1
	);
	if (effort_status == 1) {
		state->stop_reason = PROTOTYPE_CHECKER_STOP_EFFORT;
		state->subject = subject;
		return PROTOTYPE_CHECKER_PAUSED;
	}
	return effort_status == 0 ? PROTOTYPE_CHECKER_COMPLETE :
		checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_MALFORMED, subject
		);
}

static int checker_stop(
	struct checker_state* state,
	int status,
	int reason,
	uint32_t subject
) {
	state->stop_reason = reason;
	state->subject = subject;
	return status;
}

static const struct prototype_term* checker_term(
	const struct checker_state* state,
	uint32_t term_id
) {
	if (term_id >= state->module->terms.term_count) {
		return NULL;
	}
	return &state->module->terms.terms[term_id];
}

static int checker_has_type_family_root(
	const struct checker_state* state,
	uint32_t term_id
) {
	const struct prototype_term* term = checker_term(state, term_id);
	while (term && term->tag == PROTOTYPE_TERM_APP) {
		term = checker_term(state, term->as.app.function);
	}
	return term && (term->tag == PROTOTYPE_TERM_TYPE_VIEW ||
		term->tag == PROTOTYPE_TERM_TYPE_DECLARATION ||
		term->tag == PROTOTYPE_TERM_TYPE_FORMER);
}

/* Returns -2 when the source is not a fully applied pure type-family spine. */
static int checker_pure_family_spine_equal(
	const struct checker_state* state,
	uint32_t source_id,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count,
	uint32_t target_id,
	uint32_t depth
) {
	const struct prototype_term* source = checker_term(state, source_id);
	const struct prototype_term* target = checker_term(state, target_id);
	if (!source || !target || source->tag != PROTOTYPE_TERM_APP ||
		!checker_has_type_family_root(state, target_id)) {
		return -2;
	}
	size_t capacity = state->module->terms.term_count;
	uint32_t* spine_arguments = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*spine_arguments)
	);
	if (!spine_arguments) {
		return -1;
	}
	size_t spine_count = 0;
	const struct prototype_term* spine = source;
	while (spine && spine->tag == PROTOTYPE_TERM_APP &&
		spine_count < capacity) {
		spine_arguments[spine_count++] = spine->as.app.argument;
		spine = checker_term(state, spine->as.app.function);
	}
	if (!spine || spine->tag != PROTOTYPE_TERM_LAMBDA ||
		binding_count > SIZE_MAX - spine_count) {
		free(spine_arguments);
		return -2;
	}
	size_t beta_count = binding_count + spine_count;
	uint32_t* beta_bindings = malloc(beta_count * sizeof(*beta_bindings));
	uint32_t* beta_arguments = malloc(beta_count * sizeof(*beta_arguments));
	if (!beta_bindings || !beta_arguments) {
		free(beta_bindings);
		free(beta_arguments);
		free(spine_arguments);
		return -1;
	}
	if (binding_count != 0) {
		memcpy(beta_bindings, binding_ids,
			binding_count * sizeof(*beta_bindings));
		memcpy(beta_arguments, argument_ids,
			binding_count * sizeof(*beta_arguments));
	}
	for (size_t i = 0; i < spine_count; ++i) {
		if (!spine || spine->tag != PROTOTYPE_TERM_LAMBDA) {
			free(beta_bindings);
			free(beta_arguments);
			free(spine_arguments);
			return -2;
		}
		beta_bindings[binding_count + i] = spine->as.lambda.binding_id;
		beta_arguments[binding_count + i] =
			spine_arguments[spine_count - 1 - i];
		spine = checker_term(state, spine->as.lambda.body);
	}
	int result = spine && spine->tag == PROTOTYPE_TERM_RETURN ?
		checker_term_equal_after_bindings(
			state, spine->as.return_term.value,
			beta_bindings, beta_arguments, beta_count, target_id, depth + 1
		) : -2;
	free(beta_bindings);
	free(beta_arguments);
	free(spine_arguments);
	return result;
}

static int checker_is_universe(
	const struct checker_state* state,
	uint32_t term_id
) {
	const struct prototype_term* term = checker_term(state, term_id);
	return term && term->tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

static int checker_universe_value(
	const struct checker_state* state,
	uint32_t term_id,
	int* p_value
) {
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term || term->tag != PROTOTYPE_TERM_UNIVERSE_VAR || !p_value) {
		return -1;
	}
	for (size_t i = 0; i < state->universe_solution_count; ++i) {
		if (state->universe_solutions[i].level_var ==
			term->as.universe_var.level_var) {
			*p_value = state->universe_solutions[i].value;
			return 0;
		}
	}
	return -1;
}

static int checker_universes_equal(
	const struct checker_state* state,
	uint32_t left,
	uint32_t right
) {
	int left_value;
	int right_value;
	return checker_universe_value(state, left, &left_value) == 0 &&
		checker_universe_value(state, right, &right_value) == 0 &&
		left_value == right_value;
}

static int checker_type_view_is_well_formed(
	const struct checker_state* state,
	const struct prototype_term* view
) {
	if (!view || view->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		view->as.type_view.view_type_id >= state->module->type_schema.type_count) {
		return 0;
	}
	const struct prototype_semantic_type_declaration* declaration =
		&state->module->type_schema.type_declarations[
			view->as.type_view.view_type_id
		];
	uint32_t core_arguments[64];
	uint32_t source_arguments[64];
	uint32_t core_count = 0;
	uint32_t source_count = 0;
	const struct prototype_term* core = checker_term(state, view->as.type_view.core);
	const struct prototype_term* source = checker_term(
		state, view->as.type_view.source
	);
	while (core && core->tag == PROTOTYPE_TERM_APP && core_count < 64) {
		core_arguments[core_count++] = core->as.app.argument;
		core = checker_term(state, core->as.app.function);
	}
	while (source && source->tag == PROTOTYPE_TERM_APP && source_count < 64) {
		source_arguments[source_count++] = source->as.app.argument;
		source = checker_term(state, source->as.app.function);
	}
	if (core_count != source_count || core_count >
		declaration->parameter_count + declaration->index_count) {
		return 0;
	}
	for (uint32_t i = 0; i < core_count; ++i) {
		const struct prototype_term* source_argument = checker_term(
			state, source_arguments[i]
		);
		uint32_t source_core = source_argument &&
			source_argument->tag == PROTOTYPE_TERM_TYPE_VIEW ?
				source_argument->as.type_view.core : source_arguments[i];
		if (core_arguments[i] != source_core) {
			return 0;
		}
	}
	return core && core->tag == PROTOTYPE_TERM_TYPE_FORMER &&
		core->as.type_former.representation_id == declaration->representation_id &&
		source && source->tag == PROTOTYPE_TERM_TYPE_DECLARATION &&
		source->as.type_declaration.type_id == declaration->type_index &&
		source->as.type_declaration.identity.namespace_symbol_id ==
			declaration->namespace_symbol_id &&
		source->as.type_declaration.identity.name_symbol_id ==
			declaration->name_symbol_id &&
		view->as.type_view.identity.namespace_symbol_id ==
			declaration->namespace_symbol_id &&
		view->as.type_view.identity.name_symbol_id == declaration->name_symbol_id;
}

static const struct prototype_effect_operation_declaration*
checker_effect_operation_declaration(
	const struct checker_state* state,
	int operation_id
) {
	for (size_t i = 0;
		i < state->module->intrinsic_environment.effect_operation_count;
		++i) {
		const struct prototype_effect_operation_declaration* declaration =
			&state->module->intrinsic_environment.effect_operations[i];
		if (declaration->operation_id == operation_id) {
			return declaration;
		}
	}
	return NULL;
}

static const struct prototype_pure_primitive_declaration*
checker_pure_primitive_declaration(
	const struct checker_state* state,
	int primitive_id
) {
	for (size_t i = 0;
		i < state->module->intrinsic_environment.pure_primitive_count;
		++i) {
		const struct prototype_pure_primitive_declaration* declaration =
			&state->module->intrinsic_environment.pure_primitives[i];
		if (declaration->primitive_id == primitive_id) {
			return declaration;
		}
	}
	return NULL;
}

static int checker_is_effect_row_at_depth(
	struct checker_state* state,
	uint32_t term_id,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER,
			term_id
		);
	}
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term) {
		return PROTOTYPE_CHECKER_REJECTED;
	}
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return PROTOTYPE_CHECKER_COMPLETE;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int status = checker_is_effect_row_at_depth(
				state, term->as.effect_row_union.left, depth + 1
			);
			return status == PROTOTYPE_CHECKER_COMPLETE ?
				checker_is_effect_row_at_depth(
					state, term->as.effect_row_union.right, depth + 1
				) : status;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return term->as.effect_row_forall.binding_id == PROTOTYPE_INVALID_ID ?
				checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					term_id
				) : checker_is_effect_row_at_depth(
					state, term->as.effect_row_forall.body, depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return !checker_effect_operation_declaration(
				state, term->as.effect_row_operation.operation_id
			) ? checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				term_id
			) : checker_is_effect_row_at_depth(
				state, term->as.effect_row_operation.latent_row, depth + 1
			);
		default:
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				term_id
			);
	}
}

static int checker_is_effect_row(
	struct checker_state* state,
	uint32_t term_id
) {
	return checker_is_effect_row_at_depth(state, term_id, 0);
}

static int checker_context_binding(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_classifier
);

static int checker_term_contains_binding(
	const struct checker_state* state,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t depth
);

static uint32_t checker_find_variable_term(
	const struct checker_state* state,
	uint32_t binding_id
);

static const struct prototype_semantic_type_declaration*
checker_type_declaration_for_term(
	const struct checker_state* state,
	const struct prototype_term* term
) {
	if (!term || term->tag != PROTOTYPE_TERM_TYPE_DECLARATION) {
		return NULL;
	}
	for (uint32_t i = 0; i < state->module->type_schema.type_count; ++i) {
		const struct prototype_semantic_type_declaration* declaration =
			&state->module->type_schema.type_declarations[i];
		if (term->as.type_declaration.type_id == declaration->type_index &&
			term->as.type_declaration.identity.namespace_symbol_id ==
				declaration->namespace_symbol_id &&
			term->as.type_declaration.identity.name_symbol_id ==
				declaration->name_symbol_id) {
			return declaration;
		}
	}
	return NULL;
}

static int checker_application_is_type(
	struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	const uint32_t* local_binding_ids,
	const uint32_t* local_classifier_ids,
	size_t local_count,
	uint32_t depth
) {
	int result = 0;
	size_t capacity = state->module->terms.term_count;
	uint32_t* arguments = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*arguments)
	);
	uint32_t* binding_ids = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*binding_ids)
	);
	uint32_t* argument_ids = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*argument_ids)
	);
	if (capacity != 0 && (!arguments || !binding_ids || !argument_ids)) {
		free(arguments);
		free(binding_ids);
		free(argument_ids);
		return -1;
	}
	size_t argument_count = 0;
	size_t binding_count = 0;
	uint32_t root_id = term_id;
	const struct prototype_term* root = checker_term(state, term_id);
	while (root && root->tag == PROTOTYPE_TERM_APP) {
		if (argument_count == capacity) {
			free(arguments);
			free(binding_ids);
			free(argument_ids);
			return 0;
		}
		arguments[argument_count++] = root->as.app.argument;
		root_id = root->as.app.function;
		root = checker_term(state, root->as.app.function);
	}
	if (root && root->tag == PROTOTYPE_TERM_LAMBDA) {
		size_t nested_count = local_count + argument_count;
		uint32_t* nested_bindings = nested_count == 0 ? NULL : malloc(
			nested_count * sizeof(*nested_bindings)
		);
		uint32_t* nested_classifiers = nested_count == 0 ? NULL : malloc(
			nested_count * sizeof(*nested_classifiers)
		);
		if (nested_count != 0 && (!nested_bindings || !nested_classifiers)) {
			free(nested_bindings);
			free(nested_classifiers);
			goto cleanup;
		}
		if (local_count != 0) {
			memcpy(
				nested_bindings,
				local_binding_ids,
				local_count * sizeof(*nested_bindings)
			);
			memcpy(
				nested_classifiers,
				local_classifier_ids,
				local_count * sizeof(*nested_classifiers)
			);
		}
		uint32_t body_id = root_id;
		int beta_valid = 1;
		for (size_t i = argument_count; i != 0; --i) {
			const struct prototype_term* lambda = checker_term(state, body_id);
			const struct prototype_term* argument = checker_term(
				state, arguments[i - 1]
			);
			uint32_t argument_classifier = PROTOTYPE_INVALID_ID;
			if (!lambda || lambda->tag != PROTOTYPE_TERM_LAMBDA || !argument ||
				argument->tag != PROTOTYPE_TERM_VAR) {
				beta_valid = 0;
				break;
			}
			for (size_t j = local_count; j != 0; --j) {
				if (local_binding_ids[j - 1] == argument->as.var.binding_id) {
					argument_classifier = local_classifier_ids[j - 1];
					break;
				}
			}
			if (argument_classifier == PROTOTYPE_INVALID_ID &&
				checker_context_binding(
					state,
					context_id,
					argument->as.var.binding_id,
					&argument_classifier
				) != 0) {
				beta_valid = 0;
				break;
			}
			nested_bindings[local_count + argument_count - i] =
				lambda->as.lambda.binding_id;
			nested_classifiers[local_count + argument_count - i] =
				argument_classifier;
			body_id = lambda->as.lambda.body;
		}
		if (beta_valid) {
			int status = checker_is_type_with_locals(
				state,
				context_id,
				body_id,
				nested_bindings,
				nested_classifiers,
				nested_count,
				depth + 1
			);
			free(nested_bindings);
			free(nested_classifiers);
			free(arguments);
			free(binding_ids);
			free(argument_ids);
			return status == PROTOTYPE_CHECKER_COMPLETE;
		}
		free(nested_bindings);
		free(nested_classifiers);
		goto cleanup;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	if (root && root->tag == PROTOTYPE_TERM_VAR) {
		for (size_t i = local_count; i != 0; --i) {
			if (local_binding_ids[i - 1] == root->as.var.binding_id) {
				classifier = local_classifier_ids[i - 1];
				break;
			}
		}
		if (classifier == PROTOTYPE_INVALID_ID && checker_context_binding(
				state, context_id, root->as.var.binding_id, &classifier
			) != 0) {
			classifier = PROTOTYPE_INVALID_ID;
		}
	} else if (root && root->tag == PROTOTYPE_TERM_TYPE_DECLARATION) {
		const struct prototype_semantic_type_declaration* declaration =
			checker_type_declaration_for_term(state, root);
		if (declaration) {
			classifier = declaration->formation_classifier;
		}
	} else if (root && root->tag == PROTOTYPE_TERM_TYPE_FORMER) {
		for (size_t i = 0; i < state->module->type_schema.type_count; ++i) {
			const struct prototype_semantic_type_declaration* declaration =
				&state->module->type_schema.type_declarations[i];
			if (declaration->representation_id ==
					root->as.type_former.representation_id) {
				classifier = declaration->formation_classifier;
				break;
			}
		}
	} else if (root && root->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		root->as.type_view.view_type_id < state->module->type_schema.type_count) {
		const struct prototype_semantic_type_declaration* declaration =
			&state->module->type_schema.type_declarations[
				root->as.type_view.view_type_id
			];
		uint32_t type_id;
		uint32_t view_arguments[64];
		size_t view_argument_count;
		if (checker_type_instance_arguments(
				state, root_id, &type_id, view_arguments, 64,
				&view_argument_count
			) != 0 || type_id != declaration->type_index ||
			view_argument_count >
				declaration->parameter_count + declaration->index_count ||
			view_argument_count > capacity) {
			goto cleanup;
		}
		classifier = declaration->formation_classifier;
		for (size_t i = 0; i < view_argument_count; ++i) {
			uint32_t domain;
			uint32_t binding;
			uint32_t body;
			if (checker_pi_parts(
					state, classifier, &domain, &binding, &body
				) != 0) {
				goto cleanup;
			}
			binding_ids[binding_count] = binding;
			argument_ids[binding_count] = view_arguments[i];
			++binding_count;
			classifier = body;
		}
	}
	if (classifier == PROTOTYPE_INVALID_ID) {
		goto cleanup;
	}
	for (size_t i = argument_count; i != 0; --i) {
		const struct prototype_term* classifier_term = checker_term(
			state, classifier
		);
		if (classifier_term && classifier_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
			classifier = classifier_term->as.thunk_type.computation;
		}
		uint32_t domain;
		uint32_t binding;
		uint32_t body;
		if (checker_pi_parts(
				state, classifier, &domain, &binding, &body
			) != 0) {
			goto cleanup;
		}
		const struct prototype_term* argument = checker_term(
			state, arguments[i - 1]
		);
		int argument_has_type = 0;
		if (argument && argument->tag == PROTOTYPE_TERM_VAR) {
			uint32_t actual = PROTOTYPE_INVALID_ID;
			for (size_t j = local_count; j != 0; --j) {
				if (local_binding_ids[j - 1] == argument->as.var.binding_id) {
					actual = local_classifier_ids[j - 1];
					break;
				}
			}
			if (actual == PROTOTYPE_INVALID_ID) {
				(void)checker_context_binding(
					state, context_id, argument->as.var.binding_id, &actual
				);
			}
			argument_has_type = actual != PROTOTYPE_INVALID_ID &&
				checker_term_equal_after_bindings(
					state, domain, binding_ids, argument_ids, binding_count,
					actual, 0
				) == 1;
		} else {
			argument_has_type = checker_term_has_type_after_bindings(
				state,
				context_id,
				arguments[i - 1],
				domain,
				binding_ids,
				argument_ids,
				binding_count,
				depth + 1
			) == 1;
		}
		if (!argument_has_type) {
			goto cleanup;
		}
		binding_ids[binding_count] = binding;
		argument_ids[binding_count] = arguments[i - 1];
		binding_count += 1;
		classifier = body;
	}
	const struct prototype_term* result_term = checker_term(state, classifier);
	if (result_term && result_term->tag == PROTOTYPE_TERM_VAR) {
		for (size_t i = 0; i < binding_count; ++i) {
			if (binding_ids[i] == result_term->as.var.binding_id) {
				result_term = checker_term(state, argument_ids[i]);
				break;
			}
		}
	}
	result = result_term && result_term->tag == PROTOTYPE_TERM_UNIVERSE_VAR;
	if (!result && result_term &&
		result_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* effect_row = checker_term(
			state, result_term->as.computation_type.label
		);
		const struct prototype_term* computation_result = checker_term(
			state, result_term->as.computation_type.result
		);
		result = effect_row &&
			effect_row->tag == PROTOTYPE_TERM_EFFECT_ROW_EMPTY &&
			computation_result &&
			computation_result->tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			result_term->as.computation_type.totality !=
				PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE;
	}

cleanup:
	free(arguments);
	free(binding_ids);
	free(argument_ids);
	return result;
}

static int checker_is_type_with_locals(
	struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	const uint32_t* local_binding_ids,
	const uint32_t* local_classifier_ids,
	size_t local_count,
	uint32_t depth
) {
	int status = checker_charge(state, term_id);
	if (status != PROTOTYPE_CHECKER_COMPLETE) {
		return status;
	}
	if (depth > state->module->terms.term_count) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER,
			term_id
		);
	}
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER,
			term_id
		);
	}
	switch (term->tag) {
		case PROTOTYPE_TERM_APP: {
			int application_status = checker_application_is_type(
				state, context_id, term_id, local_binding_ids,
				local_classifier_ids, local_count, depth
			);
			if (application_status < 0) {
				return -1;
			}
			return application_status ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				term_id
			);
		}
		case PROTOTYPE_TERM_VAR: {
			uint32_t classifier = PROTOTYPE_INVALID_ID;
			for (size_t i = local_count; i != 0; --i) {
				if (local_binding_ids[i - 1] == term->as.var.binding_id) {
					classifier = local_classifier_ids[i - 1];
					break;
				}
			}
			if ((classifier == PROTOTYPE_INVALID_ID && checker_context_binding(
					state, context_id, term->as.var.binding_id, &classifier
				) != 0) || !checker_is_universe(state, classifier)) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					term_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		}
		case PROTOTYPE_TERM_UNIVERSE_VAR:
		case PROTOTYPE_TERM_TYPE_VIEW:
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
			if (term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
				!checker_type_view_is_well_formed(state, term)) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA,
					term_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		case PROTOTYPE_TERM_TYPE_DECLARATION: {
			const struct prototype_semantic_type_declaration* declaration =
				checker_type_declaration_for_term(state, term);
			if (!declaration || declaration->parameter_count != 0 ||
				declaration->index_count != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					term_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		}
		case PROTOTYPE_TERM_PI: {
			status = checker_is_type_with_locals(
				state, context_id, term->as.pi.domain, local_binding_ids,
				local_classifier_ids, local_count, depth + 1
			);
			if (status != PROTOTYPE_CHECKER_COMPLETE) {
				return status;
			}
			const struct prototype_term* thunk = checker_term(
				state, term->as.pi.codomain_family
			);
			const struct prototype_term* lambda = thunk &&
				thunk->tag == PROTOTYPE_TERM_THUNK ? checker_term(
					state, thunk->as.thunk.computation
				) : NULL;
			const struct prototype_term* returned = lambda &&
				lambda->tag == PROTOTYPE_TERM_LAMBDA ? checker_term(
					state, lambda->as.lambda.body
				) : NULL;
			if (!returned || returned->tag != PROTOTYPE_TERM_RETURN) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					term_id
				);
			}
			if (local_count == SIZE_MAX) {
				return -1;
			}
			uint32_t* nested_bindings = malloc(
				(local_count + 1) * sizeof(*nested_bindings)
			);
			uint32_t* nested_classifiers = malloc(
				(local_count + 1) * sizeof(*nested_classifiers)
			);
			if (!nested_bindings || !nested_classifiers) {
				free(nested_bindings);
				free(nested_classifiers);
				return -1;
			}
			if (local_count != 0) {
				memcpy(nested_bindings, local_binding_ids,
					local_count * sizeof(*nested_bindings));
				memcpy(nested_classifiers, local_classifier_ids,
					local_count * sizeof(*nested_classifiers));
			}
			nested_bindings[local_count] = lambda->as.lambda.binding_id;
			nested_classifiers[local_count] = term->as.pi.domain;
			status = checker_is_type_with_locals(
				state, context_id, returned->as.return_term.value,
				nested_bindings, nested_classifiers, local_count + 1,
				depth + 1
			);
			free(nested_bindings);
			free(nested_classifiers);
			return status;
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			status = checker_is_effect_row(
				state, term->as.computation_type.label
			);
			if (status != PROTOTYPE_CHECKER_COMPLETE) {
				return status;
			}
			return checker_is_type_with_locals(
				state,
				context_id,
				term->as.computation_type.result,
				local_binding_ids,
				local_classifier_ids,
				local_count,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return checker_is_type_with_locals(
				state,
				context_id,
				term->as.thunk_type.computation,
				local_binding_ids,
				local_classifier_ids,
				local_count,
				depth + 1
			);
		default:
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_PAUSED,
				PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
				term_id
			);
	}
}

static int checker_is_type(
	struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t depth
) {
	return checker_is_type_with_locals(
		state, context_id, term_id, NULL, NULL, 0, depth
	);
}

static int checker_context_binding(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_classifier
) {
	while (context_id != 0) {
		const struct prototype_semantic_context* context =
			&state->module->contexts.contexts[context_id];
		if (context->binding_id == binding_id) {
			*p_classifier = context->classifier;
			return 0;
		}
		context_id = context->parent;
	}
	return -1;
}

/* Compare one classifier body after substituting a single Pi binder. This is
 * deliberately a checker-local structural operation: it neither interns a
 * Term nor consults producer normalization state. */
static int checker_term_equal_after_binding(
	const struct checker_state* state,
	uint32_t source_id,
	uint32_t binding_id,
	uint32_t argument_id,
	uint32_t target_id,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return -1;
	}
	const struct prototype_term* source = checker_term(state, source_id);
	const struct prototype_term* target = checker_term(state, target_id);
	if (!source || !target) {
		return 0;
	}
	const uint32_t* binding_ids = binding_id == PROTOTYPE_INVALID_ID ?
		NULL : &binding_id;
	const uint32_t* argument_ids = binding_id == PROTOTYPE_INVALID_ID ?
		NULL : &argument_id;
	size_t binding_count = binding_id == PROTOTYPE_INVALID_ID ? 0 : 1;
	int pure_family_equal = checker_pure_family_spine_equal(
		state, source_id, binding_ids, argument_ids, binding_count,
		target_id, depth
	);
	if (pure_family_equal != -2) {
		return pure_family_equal;
	}
	if (source->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* function = checker_term(
			state, source->as.app.function
		);
		if (function && function->tag == PROTOTYPE_TERM_LAMBDA &&
			!checker_term_contains_binding(
				state, function->as.lambda.body,
				function->as.lambda.binding_id, 0
			)) {
			return checker_term_equal_after_binding(
				state, function->as.lambda.body, binding_id, argument_id,
				target_id, depth + 1
			);
		}
	}
	if (target->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* function = checker_term(
			state, target->as.app.function
		);
		if (function && function->tag == PROTOTYPE_TERM_LAMBDA &&
			!checker_term_contains_binding(
				state, function->as.lambda.body,
				function->as.lambda.binding_id, 0
			)) {
			return checker_term_equal_after_binding(
				state, source_id, binding_id, argument_id,
				function->as.lambda.body, depth + 1
			);
		}
	}
	if (source->tag == PROTOTYPE_TERM_VAR &&
		source->as.var.binding_id == binding_id) {
		if (argument_id == target_id) {
			return 1;
		}
		const struct prototype_term* argument = checker_term(state, argument_id);
		return (target->tag == PROTOTYPE_TERM_TYPE_VIEW &&
			target->as.type_view.core == argument_id) ||
			(argument && argument->tag == PROTOTYPE_TERM_TYPE_VIEW &&
			 argument->as.type_view.core == target_id);
	}
	if (source->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
		source->as.effect_row_var.binding_id == binding_id) {
		return argument_id == target_id;
	}
	if (target->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		source->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		const struct prototype_term* source_root = source;
		while (source_root && source_root->tag == PROTOTYPE_TERM_APP) {
			source_root = checker_term(state, source_root->as.app.function);
		}
		return checker_term_equal_after_binding(
			state,
			source_id,
			binding_id,
			argument_id,
			source_root && source_root->tag == PROTOTYPE_TERM_TYPE_DECLARATION ?
				target->as.type_view.source : target->as.type_view.core,
			depth + 1
		);
	}
	if (source->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		target->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		return checker_term_equal_after_binding(
			state,
			source->as.type_view.core,
			binding_id,
			argument_id,
			target_id,
			depth + 1
		);
	}
	if (source->tag != target->tag) {
		return 0;
	}
	switch (source->tag) {
		case PROTOTYPE_TERM_VAR:
			return source->as.var.binding_id == target->as.var.binding_id;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 1;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return checker_universes_equal(state, source_id, target_id);
		case PROTOTYPE_TERM_TYPE_FORMER:
			return source->as.type_former.representation_id ==
				target->as.type_former.representation_id &&
				source->as.type_former.constructor_count ==
					target->as.type_former.constructor_count;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return source->as.type_declaration.type_id ==
				target->as.type_declaration.type_id &&
				source->as.type_declaration.identity.namespace_symbol_id ==
					target->as.type_declaration.identity.namespace_symbol_id &&
				source->as.type_declaration.identity.name_symbol_id ==
					target->as.type_declaration.identity.name_symbol_id;
		case PROTOTYPE_TERM_TYPE_VIEW:
			return source->as.type_view.view_type_id ==
				target->as.type_view.view_type_id &&
				checker_term_equal_after_binding(
					state,
					source->as.type_view.core,
					binding_id,
					argument_id,
					target->as.type_view.core,
					depth + 1
				) == 1 && checker_term_equal_after_binding(
					state,
					source->as.type_view.source,
					binding_id,
					argument_id,
					target->as.type_view.source,
					depth + 1
				) == 1;
		case PROTOTYPE_TERM_APP:
			return checker_term_equal_after_binding(
				state,
				source->as.app.function,
				binding_id,
				argument_id,
				target->as.app.function,
				depth + 1
			) == 1 && checker_term_equal_after_binding(
				state,
				source->as.app.argument,
				binding_id,
				argument_id,
				target->as.app.argument,
				depth + 1
			) == 1;
		case PROTOTYPE_TERM_LAMBDA:
			if (source->as.lambda.binding_id == target->as.lambda.binding_id) {
				return checker_term_equal_after_binding(
					state,
					source->as.lambda.body,
					binding_id,
					argument_id,
					target->as.lambda.body,
					depth + 1
				);
			}
			return checker_term_equal_after_bindings(
				state,
				source_id,
				binding_id == PROTOTYPE_INVALID_ID ? NULL : &binding_id,
				binding_id == PROTOTYPE_INVALID_ID ? NULL : &argument_id,
				binding_id == PROTOTYPE_INVALID_ID ? 0 : 1,
				target_id,
				depth + 1
			);
		case PROTOTYPE_TERM_PI:
			return checker_term_equal_after_binding(
				state,
				source->as.pi.domain,
				binding_id,
				argument_id,
				target->as.pi.domain,
				depth + 1
			) == 1 && checker_term_equal_after_binding(
				state,
				source->as.pi.codomain_family,
				binding_id,
				argument_id,
				target->as.pi.codomain_family,
				depth + 1
			) == 1;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return source->as.constructor.constructor_id ==
				target->as.constructor.constructor_id &&
				checker_term_equal_after_binding(
					state,
					source->as.constructor.owner,
					binding_id,
					argument_id,
					target->as.constructor.owner,
					depth + 1
				) == 1;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			return source->as.text_literal.text_symbol_id ==
				target->as.text_literal.text_symbol_id;
		case PROTOTYPE_TERM_INT_LITERAL:
			return source->as.int_literal.value == target->as.int_literal.value;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return source->as.effect_row_var.binding_id ==
				target->as.effect_row_var.binding_id;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return checker_term_equal_after_binding(
				state,
				source->as.effect_row_union.left,
				binding_id,
				argument_id,
				target->as.effect_row_union.left,
				depth + 1
			) == 1 && checker_term_equal_after_binding(
				state,
				source->as.effect_row_union.right,
				binding_id,
				argument_id,
				target->as.effect_row_union.right,
				depth + 1
			) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return source->as.effect_row_forall.binding_id ==
				target->as.effect_row_forall.binding_id &&
				checker_term_equal_after_binding(
					state,
					source->as.effect_row_forall.body,
					binding_id,
					argument_id,
					target->as.effect_row_forall.body,
					depth + 1
				) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return source->as.effect_row_operation.operation_id ==
				target->as.effect_row_operation.operation_id &&
				checker_term_equal_after_binding(
					state,
					source->as.effect_row_operation.latent_row,
					binding_id,
					argument_id,
					target->as.effect_row_operation.latent_row,
					depth + 1
				) == 1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return source->as.computation_type.totality ==
				target->as.computation_type.totality &&
				checker_term_equal_after_binding(
					state,
					source->as.computation_type.label,
					binding_id,
					argument_id,
					target->as.computation_type.label,
					depth + 1
				) == 1 && checker_term_equal_after_binding(
					state,
					source->as.computation_type.result,
					binding_id,
					argument_id,
					target->as.computation_type.result,
					depth + 1
				) == 1;
		case PROTOTYPE_TERM_THUNK_TYPE:
			return checker_term_equal_after_binding(
				state,
				source->as.thunk_type.computation,
				binding_id,
				argument_id,
				target->as.thunk_type.computation,
				depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return checker_term_equal_after_binding(
				state,
				source->as.return_term.value,
				binding_id,
				argument_id,
				target->as.return_term.value,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return checker_term_equal_after_binding(
				state,
				source->as.thunk.computation,
				binding_id,
				argument_id,
				target->as.thunk.computation,
				depth + 1
			);
		default:
			return source_id == target_id ? 1 : -1;
	}
}

struct checker_substitution_path {
	uint32_t substitution_id;
	const struct checker_substitution_path* next;
};

static int checker_term_equal_reindexed_path(
	const struct checker_state* state,
	uint32_t source_id,
	const struct checker_substitution_path* path,
	uint32_t target_id,
	uint32_t depth
);

/* Compare A[sigma] with a stored classifier without constructing A[sigma].
 * Binding IDs are object identity, so identity, empty, and projection preserve
 * variables. EXTEND replaces only the newest target binding and delegates all
 * older bindings to its prefix. COMPOSE is interpreted in application order. */
static int checker_variable_equal_reindexed_path(
	const struct checker_state* state,
	uint32_t binding_id,
	const struct checker_substitution_path* path,
	uint32_t target_id,
	uint32_t depth
) {
	if (!path) {
		uint32_t variable = checker_find_variable_term(state, binding_id);
		return variable == PROTOTYPE_INVALID_ID ? -1 :
			checker_term_equal_after_binding(
				state,
				variable,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				target_id,
				depth + 1
			);
	}
	if (path->substitution_id >=
		state->module->substitutions.substitution_count) {
		return 0;
	}
	const struct prototype_semantic_substitution* substitution =
		&state->module->substitutions.substitutions[path->substitution_id];
	switch (substitution->kind) {
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_IDENTITY:
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_EMPTY:
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_PROJECTION:
			return checker_variable_equal_reindexed_path(
				state, binding_id, path->next, target_id, depth + 1
			);
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND: {
			const struct prototype_semantic_context* target =
				&state->module->contexts.contexts[
					substitution->target_context
				];
			if (target->binding_id == binding_id) {
				return checker_term_equal_reindexed_path(
					state,
					substitution->term,
					path->next,
					target_id,
					depth + 1
				);
			}
			struct checker_substitution_path prefix = {
				.substitution_id = substitution->first,
				.next = path->next
			};
			return checker_variable_equal_reindexed_path(
				state, binding_id, &prefix, target_id, depth + 1
			);
		}
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_COMPOSE: {
			struct checker_substitution_path inner = {
				.substitution_id = substitution->second,
				.next = path->next
			};
			struct checker_substitution_path outer = {
				.substitution_id = substitution->first,
				.next = &inner
			};
			return checker_variable_equal_reindexed_path(
				state, binding_id, &outer, target_id, depth + 1
			);
		}
		default:
			return 0;
	}
}

static int checker_term_equal_reindexed_path(
	const struct checker_state* state,
	uint32_t source_id,
	const struct checker_substitution_path* path,
	uint32_t target_id,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count +
		state->module->substitutions.substitution_count) {
		return -1;
	}
	if (!path) {
		return checker_term_equal_after_binding(
			state,
			source_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			target_id,
			depth + 1
		);
	}
	if (path->substitution_id >=
		state->module->substitutions.substitution_count) {
		return 0;
	}
	const struct prototype_semantic_substitution* substitution =
		&state->module->substitutions.substitutions[path->substitution_id];
	if (substitution->kind == PROTOTYPE_SEMANTIC_SUBSTITUTION_IDENTITY ||
		substitution->kind == PROTOTYPE_SEMANTIC_SUBSTITUTION_EMPTY ||
		substitution->kind == PROTOTYPE_SEMANTIC_SUBSTITUTION_PROJECTION) {
		return checker_term_equal_reindexed_path(
			state, source_id, path->next, target_id, depth + 1
		);
	}
	if (substitution->kind == PROTOTYPE_SEMANTIC_SUBSTITUTION_COMPOSE) {
		struct checker_substitution_path inner = {
			.substitution_id = substitution->second,
			.next = path->next
		};
		struct checker_substitution_path outer = {
			.substitution_id = substitution->first,
			.next = &inner
		};
		return checker_term_equal_reindexed_path(
			state, source_id, &outer, target_id, depth + 1
		);
	}
	const struct prototype_term* source = checker_term(state, source_id);
	const struct prototype_term* target = checker_term(state, target_id);
	if (!source || !target ||
		substitution->kind != PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND) {
		return 0;
	}
	if (source->tag == PROTOTYPE_TERM_VAR) {
		return checker_variable_equal_reindexed_path(
			state, source->as.var.binding_id, path, target_id, depth + 1
		);
	}
	if (target->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		source->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		const struct prototype_term* root = source;
		while (root && root->tag == PROTOTYPE_TERM_APP) {
			root = checker_term(state, root->as.app.function);
		}
		return checker_term_equal_reindexed_path(
			state,
			source_id,
			path,
			root && root->tag == PROTOTYPE_TERM_TYPE_DECLARATION ?
				target->as.type_view.source : target->as.type_view.core,
			depth + 1
		);
	}
	if (source->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		target->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		return checker_term_equal_reindexed_path(
			state,
			source->as.type_view.core,
			path,
			target_id,
			depth + 1
		);
	}
	if (source->tag != target->tag) {
		return 0;
	}
#define REINDEX_CHILD(source_child, target_child) \
	checker_term_equal_reindexed_path( \
		state, (source_child), path, (target_child), depth + 1 \
	)
	switch (source->tag) {
		case PROTOTYPE_TERM_UNIVERSE_VAR:
		case PROTOTYPE_TERM_TYPE_FORMER:
		case PROTOTYPE_TERM_TYPE_DECLARATION:
		case PROTOTYPE_TERM_TEXT_LITERAL:
		case PROTOTYPE_TERM_INT_LITERAL:
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return checker_term_equal_after_binding(
				state,
				source_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				target_id,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return source->as.type_view.view_type_id ==
				target->as.type_view.view_type_id &&
				REINDEX_CHILD(
					source->as.type_view.core, target->as.type_view.core
				) == 1 && REINDEX_CHILD(
					source->as.type_view.source, target->as.type_view.source
				) == 1;
		case PROTOTYPE_TERM_APP:
			return REINDEX_CHILD(
				source->as.app.function, target->as.app.function
			) == 1 && REINDEX_CHILD(
				source->as.app.argument, target->as.app.argument
			) == 1;
		case PROTOTYPE_TERM_LAMBDA:
			return source->as.lambda.binding_id == target->as.lambda.binding_id &&
				REINDEX_CHILD(
					source->as.lambda.body, target->as.lambda.body
				) == 1;
		case PROTOTYPE_TERM_PI:
			return REINDEX_CHILD(
				source->as.pi.domain, target->as.pi.domain
			) == 1 && REINDEX_CHILD(
				source->as.pi.codomain_family,
				target->as.pi.codomain_family
			) == 1;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return source->as.constructor.constructor_id ==
				target->as.constructor.constructor_id && REINDEX_CHILD(
					source->as.constructor.owner,
					target->as.constructor.owner
				) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return source->as.effect_row_var.binding_id ==
				target->as.effect_row_var.binding_id;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return REINDEX_CHILD(
				source->as.effect_row_union.left,
				target->as.effect_row_union.left
			) == 1 && REINDEX_CHILD(
				source->as.effect_row_union.right,
				target->as.effect_row_union.right
			) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return source->as.effect_row_forall.binding_id ==
				target->as.effect_row_forall.binding_id && REINDEX_CHILD(
					source->as.effect_row_forall.body,
					target->as.effect_row_forall.body
				) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return source->as.effect_row_operation.operation_id ==
				target->as.effect_row_operation.operation_id && REINDEX_CHILD(
					source->as.effect_row_operation.latent_row,
					target->as.effect_row_operation.latent_row
				) == 1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return source->as.computation_type.totality ==
				target->as.computation_type.totality && REINDEX_CHILD(
					source->as.computation_type.label,
					target->as.computation_type.label
				) == 1 && REINDEX_CHILD(
					source->as.computation_type.result,
					target->as.computation_type.result
				) == 1;
		case PROTOTYPE_TERM_THUNK_TYPE:
			return REINDEX_CHILD(
				source->as.thunk_type.computation,
				target->as.thunk_type.computation
			);
		case PROTOTYPE_TERM_RETURN:
			return REINDEX_CHILD(
				source->as.return_term.value,
				target->as.return_term.value
			);
		case PROTOTYPE_TERM_THUNK:
			return REINDEX_CHILD(
				source->as.thunk.computation,
				target->as.thunk.computation
			);
		case PROTOTYPE_TERM_FORCE:
			return REINDEX_CHILD(
				source->as.force.value, target->as.force.value
			);
		default:
			return -1;
	}
#undef REINDEX_CHILD
}

static int checker_term_equal_reindexed(
	const struct checker_state* state,
	uint32_t source_id,
	uint32_t substitution_id,
	uint32_t target_id
) {
	struct checker_substitution_path path = {
		.substitution_id = substitution_id,
		.next = NULL
	};
	return checker_term_equal_reindexed_path(
		state, source_id, &path, target_id, 0
	);
}

struct checker_computation_type_view {
	uint32_t effect_row;
	uint32_t result;
	int totality;
	uint32_t binding_ids[64];
	uint32_t argument_ids[64];
	size_t binding_count;
};

static int checker_computation_type_view(
	const struct checker_state* state,
	uint32_t classifier_id,
	struct checker_computation_type_view* p_view
) {
	if (!p_view) {
		return -1;
	}
	memset(p_view, 0, sizeof(*p_view));
	for (size_t depth = 0; depth <= state->module->terms.term_count; ++depth) {
		const struct prototype_term* classifier = checker_term(
			state, classifier_id
		);
		if (!classifier) {
			return -1;
		}
		if (classifier->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
			p_view->effect_row = classifier->as.computation_type.label;
			p_view->result = classifier->as.computation_type.result;
			p_view->totality = classifier->as.computation_type.totality;
			return 0;
		}
		if (classifier->tag != PROTOTYPE_TERM_APP) {
			return 1;
		}
		uint32_t arguments[64];
		size_t argument_count = 0;
		uint32_t root_id = classifier_id;
		const struct prototype_term* root = classifier;
		while (root && root->tag == PROTOTYPE_TERM_APP) {
			if (argument_count == 64) {
				return 1;
			}
			arguments[argument_count++] = root->as.app.argument;
			root_id = root->as.app.function;
			root = checker_term(state, root_id);
		}
		for (size_t i = argument_count; i != 0; --i) {
			if (!root || root->tag != PROTOTYPE_TERM_LAMBDA ||
				p_view->binding_count == 64) {
				return 1;
			}
			p_view->binding_ids[p_view->binding_count] =
				root->as.lambda.binding_id;
			p_view->argument_ids[p_view->binding_count] = arguments[i - 1];
			p_view->binding_count += 1;
			root_id = root->as.lambda.body;
			root = checker_term(state, root_id);
		}
		classifier_id = root_id;
	}
	return -1;
}

static int checker_computation_view_term_equal(
	const struct checker_state* state,
	const struct checker_computation_type_view* view,
	uint32_t source,
	uint32_t target
) {
	return checker_term_equal_after_bindings(
		state,
		source,
		view->binding_ids,
		view->argument_ids,
		view->binding_count,
		target,
		0
	);
}

static uint32_t checker_find_variable_term(
	const struct checker_state* state,
	uint32_t binding_id
) {
	for (uint32_t i = 0; i < state->module->terms.term_count; ++i) {
		const struct prototype_term* term = &state->module->terms.terms[i];
		if (term->tag == PROTOTYPE_TERM_VAR &&
			term->as.var.binding_id == binding_id) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int checker_substitution_binding_image(
	const struct checker_state* state,
	uint32_t substitution_id,
	uint32_t binding_id,
	uint32_t* p_term
) {
	if (substitution_id >= state->module->substitutions.substitution_count) {
		return -1;
	}
	const struct prototype_semantic_substitution* substitution =
		&state->module->substitutions.substitutions[substitution_id];
	switch (substitution->kind) {
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_IDENTITY:
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_PROJECTION:
			*p_term = checker_find_variable_term(state, binding_id);
			return *p_term == PROTOTYPE_INVALID_ID ? -1 : 0;
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND: {
			const struct prototype_semantic_context* target =
				&state->module->contexts.contexts[substitution->target_context];
			if (target->binding_id == binding_id) {
				*p_term = substitution->term;
				return 0;
			}
			return checker_substitution_binding_image(
				state, substitution->first, binding_id, p_term
			);
		}
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_COMPOSE:
		case PROTOTYPE_SEMANTIC_SUBSTITUTION_EMPTY:
		default:
			return 1;
	}
}

static int checker_term_contains_binding(
	const struct checker_state* state,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return 1;
	}
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term) {
		return 1;
	}
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			return term->as.var.binding_id == binding_id;
		case PROTOTYPE_TERM_APP:
			return checker_term_contains_binding(
				state, term->as.app.function, binding_id, depth + 1
			) || checker_term_contains_binding(
				state, term->as.app.argument, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
			return term->as.lambda.binding_id == binding_id ? 0 :
				checker_term_contains_binding(
					state, term->as.lambda.body, binding_id, depth + 1
				);
		case PROTOTYPE_TERM_PI:
			return checker_term_contains_binding(
				state, term->as.pi.domain, binding_id, depth + 1
			) || checker_term_contains_binding(
				state, term->as.pi.codomain_family, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return checker_term_contains_binding(
				state, term->as.type_view.core, binding_id, depth + 1
			) || checker_term_contains_binding(
				state, term->as.type_view.source, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return checker_term_contains_binding(
				state, term->as.computation_type.label, binding_id, depth + 1
			) || checker_term_contains_binding(
				state, term->as.computation_type.result, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return checker_term_contains_binding(
				state, term->as.thunk_type.computation, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return checker_term_contains_binding(
				state, term->as.return_term.value, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return checker_term_contains_binding(
				state, term->as.thunk.computation, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return checker_term_contains_binding(
				state, term->as.effect_row_union.left, binding_id, depth + 1
			) || checker_term_contains_binding(
				state, term->as.effect_row_union.right, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return term->as.effect_row_forall.binding_id == binding_id ? 0 :
				checker_term_contains_binding(
					state, term->as.effect_row_forall.body, binding_id, depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return checker_term_contains_binding(
				state, term->as.effect_row_operation.latent_row,
				binding_id, depth + 1
			);
		default:
			return 0;
	}
}

static int checker_pi_parts(
	const struct checker_state* state,
	uint32_t pi_id,
	uint32_t* p_domain,
	uint32_t* p_binding,
	uint32_t* p_body
) {
	const struct prototype_term* pi = checker_term(state, pi_id);
	if (pi && pi->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* family = checker_term(
			state, pi->as.app.function
		);
		const struct prototype_term* body = family &&
			family->tag == PROTOTYPE_TERM_LAMBDA ? checker_term(
				state, family->as.lambda.body
			) : NULL;
		if (!body || body->tag != PROTOTYPE_TERM_PI ||
			checker_term_contains_binding(
				state,
				family->as.lambda.body,
				family->as.lambda.binding_id,
				0
			)) {
			return -1;
		}
		pi = body;
	}
	const struct prototype_term* thunk = pi && pi->tag == PROTOTYPE_TERM_PI ?
		checker_term(state, pi->as.pi.codomain_family) : NULL;
	const struct prototype_term* family = thunk &&
		thunk->tag == PROTOTYPE_TERM_THUNK ?
		checker_term(state, thunk->as.thunk.computation) : NULL;
	const struct prototype_term* returned = family &&
		family->tag == PROTOTYPE_TERM_LAMBDA ?
		checker_term(state, family->as.lambda.body) : NULL;
	if (!pi || !family || !returned || returned->tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	*p_domain = pi->as.pi.domain;
	*p_binding = family->as.lambda.binding_id;
	*p_body = returned->as.return_term.value;
	return 0;
}

static int checker_term_equal_after_bindings(
	const struct checker_state* state,
	uint32_t source_id,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count,
	uint32_t target_id,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return -1;
	}
	const struct prototype_term* source = checker_term(state, source_id);
	const struct prototype_term* target = checker_term(state, target_id);
	if (!source || !target) {
		return 0;
	}
	int pure_family_equal = checker_pure_family_spine_equal(
		state, source_id, binding_ids, argument_ids, binding_count,
		target_id, depth
	);
	if (pure_family_equal != -2) {
		return pure_family_equal;
	}
	if (source->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* function = checker_term(
			state, source->as.app.function
		);
		if (function && function->tag == PROTOTYPE_TERM_LAMBDA) {
			if (binding_count == SIZE_MAX) {
				return -1;
			}
			uint32_t* beta_bindings = malloc(
				(binding_count + 1) * sizeof(*beta_bindings)
			);
			uint32_t* beta_arguments = malloc(
				(binding_count + 1) * sizeof(*beta_arguments)
			);
			if (!beta_bindings || !beta_arguments) {
				free(beta_bindings);
				free(beta_arguments);
				return -1;
			}
			if (binding_count != 0) {
				memcpy(beta_bindings, binding_ids,
					binding_count * sizeof(*beta_bindings));
				memcpy(beta_arguments, argument_ids,
					binding_count * sizeof(*beta_arguments));
			}
			beta_bindings[binding_count] = function->as.lambda.binding_id;
			beta_arguments[binding_count] = source->as.app.argument;
			int beta_equal = checker_term_equal_after_bindings(
				state, function->as.lambda.body, beta_bindings, beta_arguments,
				binding_count + 1, target_id, depth + 1
			);
			free(beta_bindings);
			free(beta_arguments);
			return beta_equal;
		}
	}
	if (target->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* function = checker_term(
			state, target->as.app.function
		);
		if (function && function->tag == PROTOTYPE_TERM_LAMBDA &&
			!checker_term_contains_binding(
				state, function->as.lambda.body,
				function->as.lambda.binding_id, 0
			)) {
			return checker_term_equal_after_bindings(
				state, source_id, binding_ids, argument_ids, binding_count,
				function->as.lambda.body, depth + 1
			);
		}
	}
	if (source->tag == PROTOTYPE_TERM_VAR ||
		source->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		uint32_t source_binding = source->tag == PROTOTYPE_TERM_VAR ?
			source->as.var.binding_id : source->as.effect_row_var.binding_id;
		for (size_t i = 0; i < binding_count; ++i) {
			if (binding_ids[i] == source_binding) {
				return checker_term_equal_after_bindings(
					state,
					argument_ids[i],
					binding_ids,
					argument_ids,
					binding_count,
					target_id,
					depth + 1
				);
			}
		}
	}
	if (target->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		source->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		const struct prototype_term* root = source;
		while (root && root->tag == PROTOTYPE_TERM_APP) {
			root = checker_term(state, root->as.app.function);
		}
		return checker_term_equal_after_bindings(
			state,
			source_id,
			binding_ids,
			argument_ids,
			binding_count,
			root && root->tag == PROTOTYPE_TERM_TYPE_DECLARATION ?
				target->as.type_view.source : target->as.type_view.core,
			depth + 1
		);
	}
	if (source->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		target->tag != PROTOTYPE_TERM_TYPE_VIEW) {
		return checker_term_equal_after_bindings(
			state,
			source->as.type_view.core,
			binding_ids,
			argument_ids,
			binding_count,
			target_id,
			depth + 1
		);
	}
	if (source->tag != target->tag) {
		return 0;
	}
#define COMPARE_CHILD(source_child, target_child) \
	checker_term_equal_after_bindings( \
		state, (source_child), binding_ids, argument_ids, binding_count, \
		(target_child), depth + 1 \
	)
	switch (source->tag) {
		case PROTOTYPE_TERM_VAR:
			return source->as.var.binding_id == target->as.var.binding_id;
		case PROTOTYPE_TERM_TYPE_VIEW: {
			int core_equal = COMPARE_CHILD(
				source->as.type_view.core, target->as.type_view.core
			);
			int source_equal = COMPARE_CHILD(
				source->as.type_view.source, target->as.type_view.source
			);
			return source->as.type_view.view_type_id ==
				target->as.type_view.view_type_id && core_equal == 1 &&
				source_equal == 1;
		}
		case PROTOTYPE_TERM_APP:
			return COMPARE_CHILD(
				source->as.app.function, target->as.app.function
			) == 1 && COMPARE_CHILD(
				source->as.app.argument, target->as.app.argument
			) == 1;
		case PROTOTYPE_TERM_LAMBDA:
			if (source->as.lambda.binding_id == target->as.lambda.binding_id) {
				return COMPARE_CHILD(
					source->as.lambda.body, target->as.lambda.body
				);
			}
			int source_uses_binding = checker_term_contains_binding(
				state,
				source->as.lambda.body,
				source->as.lambda.binding_id,
				0
			);
			int target_uses_binding = checker_term_contains_binding(
				state,
				target->as.lambda.body,
				target->as.lambda.binding_id,
				0
			);
			if (!source_uses_binding && !target_uses_binding) {
				return COMPARE_CHILD(
					source->as.lambda.body, target->as.lambda.body
				);
			}
			if (!source_uses_binding) {
				return COMPARE_CHILD(
					source->as.lambda.body, target->as.lambda.body
				);
			}
			if (binding_count == SIZE_MAX) {
				return -1;
			}
			uint32_t target_variable = checker_find_variable_term(
				state, target->as.lambda.binding_id
			);
			if (target_variable == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			uint32_t* nested_bindings = malloc(
				(binding_count + 1) * sizeof(*nested_bindings)
			);
			uint32_t* nested_arguments = malloc(
				(binding_count + 1) * sizeof(*nested_arguments)
			);
			if (!nested_bindings || !nested_arguments) {
				free(nested_bindings);
				free(nested_arguments);
				return -1;
			}
			if (binding_count != 0) {
				memcpy(nested_bindings, binding_ids,
					binding_count * sizeof(*nested_bindings));
				memcpy(nested_arguments, argument_ids,
					binding_count * sizeof(*nested_arguments));
			}
			nested_bindings[binding_count] = source->as.lambda.binding_id;
			nested_arguments[binding_count] = target_variable;
			int lambda_equal = checker_term_equal_after_bindings(
				state,
				source->as.lambda.body,
				nested_bindings,
				nested_arguments,
				binding_count + 1,
				target->as.lambda.body,
				depth + 1
			);
			free(nested_bindings);
			free(nested_arguments);
			return lambda_equal;
		case PROTOTYPE_TERM_PI:
			return COMPARE_CHILD(
				source->as.pi.domain, target->as.pi.domain
			) == 1 && COMPARE_CHILD(
				source->as.pi.codomain_family,
				target->as.pi.codomain_family
			) == 1;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return source->as.constructor.constructor_id ==
				target->as.constructor.constructor_id && COMPARE_CHILD(
					source->as.constructor.owner,
					target->as.constructor.owner
				) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return source->as.effect_row_var.binding_id ==
				target->as.effect_row_var.binding_id;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return COMPARE_CHILD(
				source->as.effect_row_union.left,
				target->as.effect_row_union.left
			) == 1 && COMPARE_CHILD(
				source->as.effect_row_union.right,
				target->as.effect_row_union.right
			) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return source->as.effect_row_forall.binding_id ==
				target->as.effect_row_forall.binding_id && COMPARE_CHILD(
					source->as.effect_row_forall.body,
					target->as.effect_row_forall.body
				) == 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return source->as.effect_row_operation.operation_id ==
				target->as.effect_row_operation.operation_id && COMPARE_CHILD(
					source->as.effect_row_operation.latent_row,
					target->as.effect_row_operation.latent_row
				) == 1;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return source->as.computation_type.totality ==
				target->as.computation_type.totality && COMPARE_CHILD(
					source->as.computation_type.label,
					target->as.computation_type.label
				) == 1 && COMPARE_CHILD(
					source->as.computation_type.result,
					target->as.computation_type.result
				) == 1;
		case PROTOTYPE_TERM_THUNK_TYPE:
			return COMPARE_CHILD(
				source->as.thunk_type.computation,
				target->as.thunk_type.computation
			);
		case PROTOTYPE_TERM_RETURN:
			return COMPARE_CHILD(
				source->as.return_term.value, target->as.return_term.value
			);
		case PROTOTYPE_TERM_THUNK:
			return COMPARE_CHILD(
				source->as.thunk.computation, target->as.thunk.computation
			);
		default:
			return checker_term_equal_after_binding(
				state,
				source_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				target_id,
				depth + 1
			);
	}
#undef COMPARE_CHILD
}

static int checker_parameter_instantiation(
	const struct checker_state* state,
	const struct prototype_semantic_type_declaration* type,
	uint32_t owner_term,
	uint32_t* binding_ids,
	uint32_t* argument_ids,
	size_t capacity
) {
	if (type->parameter_count > capacity) {
		return -1;
	}
	uint32_t context = type->parameter_context;
	for (uint32_t i = type->parameter_count; i != 0; --i) {
		if (context == 0) {
			return -1;
		}
		binding_ids[i - 1] = state->module->contexts.contexts[context].binding_id;
		context = state->module->contexts.contexts[context].parent;
	}
	if (context != 0) {
		return -1;
	}
	const struct prototype_term* owner = checker_term(state, owner_term);
	if (owner && owner->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		owner = checker_term(state, owner->as.type_view.core);
	}
	for (uint32_t i = type->parameter_count; i != 0; --i) {
		if (!owner || owner->tag != PROTOTYPE_TERM_APP) {
			return -1;
		}
		argument_ids[i - 1] = owner->as.app.argument;
		owner = checker_term(state, owner->as.app.function);
	}
	while (owner && owner->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		owner = checker_term(state, owner->as.type_view.core);
	}
	return owner && owner->tag == PROTOTYPE_TERM_TYPE_FORMER ? 0 : -1;
}

static int checker_constructor_classifier_matches(
	const struct checker_state* state,
	const struct prototype_semantic_type_declaration* type,
	const struct prototype_semantic_type_constructor* constructor,
	uint32_t owner_term,
	uint32_t classifier_id
) {
	size_t capacity = state->module->contexts.context_count;
	uint32_t* path = capacity == 0 ? NULL : malloc(capacity * sizeof(*path));
	if (capacity != 0 && !path) {
		return -1;
	}
	size_t count = 0;
	uint32_t cursor = constructor->field_context;
	while (cursor != constructor->parameter_context) {
		if (cursor == 0 || count == capacity) {
			free(path);
			return 0;
		}
		path[count++] = cursor;
		cursor = state->module->contexts.contexts[cursor].parent;
	}
	uint32_t* parameter_bindings = type->parameter_count == 0 ? NULL : malloc(
		type->parameter_count * sizeof(*parameter_bindings)
	);
	uint32_t* parameter_arguments = type->parameter_count == 0 ? NULL : malloc(
		type->parameter_count * sizeof(*parameter_arguments)
	);
	if (type->parameter_count != 0 &&
		(!parameter_bindings || !parameter_arguments)) {
		free(parameter_bindings);
		free(parameter_arguments);
		free(path);
		return -1;
	}
	if (checker_parameter_instantiation(
			state,
			type,
			owner_term,
			parameter_bindings,
			parameter_arguments,
			type->parameter_count
		) != 0) {
		free(parameter_bindings);
		free(parameter_arguments);
		free(path);
		return 0;
	}
	for (size_t i = count; i != 0; --i) {
		const struct prototype_semantic_context* field =
			&state->module->contexts.contexts[path[i - 1]];
		uint32_t domain;
		uint32_t binding;
		uint32_t body;
		int domain_equal;
		if (checker_pi_parts(
				state, classifier_id, &domain, &binding, &body
			) != 0) {
			free(path);
			return 0;
		}
		domain_equal = checker_term_equal_after_bindings(
				state,
				field->classifier,
				parameter_bindings,
				parameter_arguments,
				type->parameter_count,
				domain,
				0
			);
		if (domain_equal != 1 || binding != field->binding_id) {
			free(parameter_bindings);
			free(parameter_arguments);
			free(path);
			return 0;
		}
		classifier_id = body;
	}
	free(path);
	int result = checker_term_equal_after_bindings(
			state,
			constructor->result_classifier,
			parameter_bindings,
			parameter_arguments,
			type->parameter_count,
			classifier_id,
			0
		);
	free(parameter_bindings);
	free(parameter_arguments);
	return result;
}

static const struct prototype_semantic_type_constructor*
checker_find_constructor(
	const struct checker_state* state,
	uint32_t owner_term,
	uint32_t constructor_ordinal,
	uint32_t classifier_id
) {
	const struct prototype_term* owner = checker_term(state, owner_term);
	for (;;) {
		while (owner && owner->tag == PROTOTYPE_TERM_APP) {
			owner = checker_term(state, owner->as.app.function);
		}
		if (!owner || owner->tag != PROTOTYPE_TERM_TYPE_VIEW) {
			break;
		}
		owner = checker_term(state, owner->as.type_view.core);
	}
	if (!owner || owner->tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return NULL;
	}
	for (uint32_t i = 0; i < state->module->type_schema.type_count; ++i) {
		const struct prototype_semantic_type_declaration* type =
			&state->module->type_schema.type_declarations[i];
		if (type->representation_id != owner->as.type_former.representation_id ||
			constructor_ordinal >= type->constructor_count) {
			continue;
		}
		const struct prototype_semantic_type_constructor* constructor =
			&state->module->type_schema.constructor_declarations[
			type->first_constructor + constructor_ordinal
		];
		if (checker_constructor_classifier_matches(
				state, type, constructor, owner_term, classifier_id
			) == 1) {
			return constructor;
		}
	}
	return NULL;
}

static const struct prototype_term* checker_constructor_owner_core(
	const struct checker_state* state,
	uint32_t owner_id
) {
	const struct prototype_term* owner = checker_term(state, owner_id);
	for (;;) {
		while (owner && owner->tag == PROTOTYPE_TERM_APP) {
			owner = checker_term(state, owner->as.app.function);
		}
		if (!owner || owner->tag != PROTOTYPE_TERM_TYPE_VIEW) {
			break;
		}
		owner = checker_term(state, owner->as.type_view.core);
	}
	return owner && owner->tag == PROTOTYPE_TERM_TYPE_FORMER ? owner : NULL;
}

static int checker_constructor_owners_match(
	const struct checker_state* state,
	uint32_t left_id,
	uint32_t right_id
) {
	const struct prototype_term* left = checker_constructor_owner_core(
		state, left_id
	);
	const struct prototype_term* right = checker_constructor_owner_core(
		state, right_id
	);
	return left && right &&
		left->as.type_former.representation_id ==
			right->as.type_former.representation_id &&
		left->as.type_former.constructor_count ==
			right->as.type_former.constructor_count;
}

static int checker_term_has_type(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t classifier_id,
	uint32_t depth
);

static int checker_occurrence_has_type_after_bindings(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t classifier_id,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count
) {
	for (uint32_t i = 0; i < state->module->occurrences.occurrence_count; ++i) {
		const struct prototype_semantic_occurrence* occurrence =
			&state->module->occurrences.occurrences[i];
		if (occurrence->context_id != context_id ||
			occurrence->core_term != term_id) {
			continue;
		}
		int equal = checker_term_equal_after_bindings(
			state,
			classifier_id,
			binding_ids,
			argument_ids,
			binding_count,
			occurrence->asserted_classifier,
			0
		);
		if (equal != 0) {
			return equal;
		}
	}
	return 0;
}

static int checker_term_has_type_after_bindings(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t classifier_id,
	const uint32_t* inherited_binding_ids,
	const uint32_t* inherited_argument_ids,
	size_t inherited_binding_count,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return 0;
	}
	int occurrence_status = checker_occurrence_has_type_after_bindings(
		state,
		context_id,
		term_id,
		classifier_id,
		inherited_binding_ids,
		inherited_argument_ids,
		inherited_binding_count
	);
	if (occurrence_status != 0) {
		return occurrence_status;
	}
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term) {
		return 0;
	}
	if (term->tag == PROTOTYPE_TERM_VAR) {
		uint32_t actual_classifier;
		return checker_context_binding(
			state,
			context_id,
			term->as.var.binding_id,
			&actual_classifier
		) == 0 && checker_term_equal_after_bindings(
			state,
			classifier_id,
			inherited_binding_ids,
			inherited_argument_ids,
			inherited_binding_count,
			actual_classifier,
			0
		) == 1;
	}
	size_t argument_capacity = state->module->terms.term_count;
	uint32_t* arguments = argument_capacity == 0 ? NULL : malloc(
		argument_capacity * sizeof(*arguments)
	);
	if (argument_capacity != 0 && !arguments) {
		return -1;
	}
	size_t argument_count = 0;
	while (term && term->tag == PROTOTYPE_TERM_APP) {
		if (argument_count == argument_capacity) {
			free(arguments);
			return 0;
		}
		arguments[argument_count++] = term->as.app.argument;
		term = checker_term(state, term->as.app.function);
	}
	if (!term || term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		free(arguments);
		return -1;
	}
	const struct prototype_term* classifier = checker_term(state, classifier_id);
	if (!classifier || classifier->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		classifier->as.type_view.view_type_id >=
			state->module->type_schema.type_count ||
		!checker_constructor_owners_match(
			state, term->as.constructor.owner, classifier_id
		)) {
		free(arguments);
		return 0;
	}
	const struct prototype_semantic_type_declaration* type =
		&state->module->type_schema.type_declarations[
			classifier->as.type_view.view_type_id
		];
	if (term->as.constructor.constructor_id >= type->constructor_count) {
		free(arguments);
		return 0;
	}
	const struct prototype_semantic_type_constructor* constructor =
		&state->module->type_schema.constructor_declarations[
			type->first_constructor + term->as.constructor.constructor_id
		];
	size_t context_capacity = state->module->contexts.context_count;
	uint32_t* fields = context_capacity == 0 ? NULL : malloc(
		context_capacity * sizeof(*fields)
	);
	if (context_capacity != 0 && !fields) {
		free(arguments);
		return -1;
	}
	size_t field_count = 0;
	uint32_t cursor = constructor->field_context;
	while (cursor != constructor->parameter_context) {
		if (cursor == 0 || field_count == context_capacity) {
			free(fields);
			free(arguments);
			return 0;
		}
		fields[field_count++] = cursor;
		cursor = state->module->contexts.contexts[cursor].parent;
	}
	if (cursor != constructor->parameter_context || field_count != argument_count) {
		free(fields);
		free(arguments);
		return 0;
	}
	if (inherited_binding_count > SIZE_MAX - type->parameter_count ||
		inherited_binding_count + type->parameter_count >
			SIZE_MAX - field_count) {
		free(fields);
		free(arguments);
		return -1;
	}
	size_t mapping_capacity = inherited_binding_count +
		type->parameter_count + field_count;
	uint32_t* binding_ids = mapping_capacity == 0 ? NULL : malloc(
		mapping_capacity * sizeof(*binding_ids)
	);
	uint32_t* argument_ids = mapping_capacity == 0 ? NULL : malloc(
		mapping_capacity * sizeof(*argument_ids)
	);
	if (mapping_capacity != 0 && (!binding_ids || !argument_ids)) {
		free(binding_ids);
		free(argument_ids);
		free(fields);
		free(arguments);
		return -1;
	}
	if (inherited_binding_count != 0) {
		memcpy(
			binding_ids,
			inherited_binding_ids,
			inherited_binding_count * sizeof(*binding_ids)
		);
		memcpy(
			argument_ids,
			inherited_argument_ids,
			inherited_binding_count * sizeof(*argument_ids)
		);
	}
	if (checker_parameter_instantiation(
			state,
			type,
			term->as.constructor.owner,
			binding_ids + inherited_binding_count,
			argument_ids + inherited_binding_count,
			type->parameter_count
		) != 0) {
		free(binding_ids);
		free(argument_ids);
		free(fields);
		free(arguments);
		return 0;
	}
	size_t mapping_count = inherited_binding_count + type->parameter_count;
	for (size_t i = 0; i < field_count; ++i) {
		uint32_t expected = state->module->contexts.contexts[
			fields[field_count - 1 - i]
		].classifier;
		uint32_t argument = arguments[argument_count - 1 - i];
		if (checker_term_has_type_after_bindings(
				state,
				context_id,
				argument,
				expected,
				binding_ids,
				argument_ids,
				mapping_count,
				depth + 1
			) != 1) {
			free(binding_ids);
			free(argument_ids);
			free(fields);
			free(arguments);
			return 0;
		}
		binding_ids[mapping_count] = state->module->contexts.contexts[
			fields[field_count - 1 - i]
		].binding_id;
		argument_ids[mapping_count] = argument;
		mapping_count += 1;
	}
	int result_equal = checker_term_equal_after_bindings(
		state,
		constructor->result_classifier,
		binding_ids,
		argument_ids,
		mapping_count,
		classifier_id,
		0
	);
	free(binding_ids);
	free(argument_ids);
	free(fields);
	free(arguments);
	return result_equal;
}

static int checker_term_has_type(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t term_id,
	uint32_t classifier_id,
	uint32_t depth
) {
	return checker_term_has_type_after_bindings(
		state,
		context_id,
		term_id,
		classifier_id,
		NULL,
		NULL,
		0,
		depth
	);
}

static int checker_context_weakens_to(
	const struct checker_state* state,
	uint32_t source_context,
	uint32_t target_context
) {
	uint32_t cursor = source_context;
	while (cursor != 0) {
		const struct prototype_semantic_context* source =
			&state->module->contexts.contexts[cursor];
		uint32_t target_classifier;
		if (checker_context_binding(
				state, target_context, source->binding_id, &target_classifier
			) != 0 || checker_term_equal_after_binding(
				state,
				source->classifier,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				target_classifier,
				0
			) != 1) {
			return 0;
		}
		cursor = source->parent;
	}
	return 1;
}

static int checker_check_contexts(struct checker_state* state) {
	for (uint32_t i = 1; i < state->module->contexts.context_count; ++i) {
		int status = checker_charge(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_context* context =
			&state->module->contexts.contexts[i];
		uint32_t ignored_classifier;
		if (checker_context_binding(
				state,
				context->parent,
				context->binding_id,
				&ignored_classifier
			) == 0) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CONTEXT,
				i
			);
		}
		if (context->extension_kind ==
				PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_SEQUENCE_RESULT) {
			int producer_found = 0;
			for (uint32_t j = 0;
				j < state->module->occurrences.occurrence_count;
				++j) {
				const struct prototype_semantic_occurrence* producer =
					&state->module->occurrences.occurrences[j];
				struct checker_computation_type_view computation;
				if (producer->classifier_evidence_kind !=
						PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT ||
					!checker_context_weakens_to(
						state, producer->context_id, context->parent
					) ||
					producer->core_term != context->producer_computation ||
					checker_computation_type_view(
						state, producer->asserted_classifier, &computation
					) != 0 || checker_computation_view_term_equal(
						state, &computation, computation.result,
						context->classifier
					) != 1) {
					continue;
				}
				producer_found = 1;
				break;
			}
			if (!producer_found) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CONTEXT,
					i
				);
			}
		}
		status = checker_is_type(
			state, context->parent, context->classifier, 0
		);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_substitutions(struct checker_state* state) {
	for (uint32_t i = 0;
		i < state->module->substitutions.substitution_count;
		++i) {
		int status = checker_charge(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_substitution_assignments(
	struct checker_state* state
) {
	for (uint32_t i = 0;
		i < state->module->substitutions.substitution_count;
		++i) {
		const struct prototype_semantic_substitution* substitution =
			&state->module->substitutions.substitutions[i];
		if (substitution->kind != PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND) {
			continue;
		}
		int status = checker_charge(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_context* target =
			&state->module->contexts.contexts[substitution->target_context];
		int classifier_equal = checker_term_equal_reindexed(
			state,
			target->classifier,
			substitution->first,
			substitution->term_classifier
		);
		if (classifier_equal < 0) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_PAUSED,
				PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
				i
			);
		}
		int assignment_status = checker_term_has_type(
			state,
			substitution->source_context,
			substitution->term,
			substitution->term_classifier,
			0
		);
		if (assignment_status < 0) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_PAUSED,
				PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
				i
			);
		}
		if (!classifier_equal || !assignment_status) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_SUBSTITUTION,
				i
			);
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_context_extends(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t ancestor
) {
	while (context_id != ancestor && context_id != 0) {
		context_id = state->module->contexts.contexts[context_id].parent;
	}
	return context_id == ancestor;
}

static int checker_context_path_length(
	const struct checker_state* state,
	uint32_t ancestor,
	uint32_t context_id,
	uint32_t* p_length
) {
	uint32_t length = 0;
	while (context_id != ancestor) {
		if (context_id == 0 || length >= state->module->contexts.context_count) {
			return -1;
		}
		context_id = state->module->contexts.contexts[context_id].parent;
		++length;
	}
	*p_length = length;
	return 0;
}

static int checker_formation_classifier_matches(
	const struct checker_state* state,
	uint32_t context_id,
	uint32_t classifier_id
) {
	size_t capacity = state->module->contexts.context_count;
	uint32_t* path = capacity == 0 ? NULL : malloc(capacity * sizeof(*path));
	if (capacity != 0 && !path) {
		return -1;
	}
	size_t count = 0;
	uint32_t cursor = context_id;
	while (cursor != 0) {
		if (count == capacity) {
			free(path);
			return 0;
		}
		path[count++] = cursor;
		cursor = state->module->contexts.contexts[cursor].parent;
	}
	for (size_t i = count; i != 0; --i) {
		const struct prototype_semantic_context* binding =
			&state->module->contexts.contexts[path[i - 1]];
		uint32_t domain;
		uint32_t family_binding;
		uint32_t body;
		if (checker_pi_parts(
				state, classifier_id, &domain, &family_binding, &body
			) != 0 || family_binding != binding->binding_id ||
			(domain != binding->classifier && !checker_universes_equal(
				state, domain, binding->classifier
			))) {
			free(path);
			return 0;
		}
		classifier_id = body;
	}
	free(path);
	return checker_is_universe(state, classifier_id);
}

static int checker_check_type_schema(struct checker_state* state) {
	for (uint32_t i = 0; i < state->module->type_schema.type_count; ++i) {
		int status = checker_charge(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_type_declaration* type =
			&state->module->type_schema.type_declarations[i];
		uint32_t parameter_count;
		uint32_t index_count;
		if (checker_context_path_length(
				state, 0, type->parameter_context, &parameter_count
			) != 0 || checker_context_path_length(
				state,
				type->parameter_context,
				type->index_context,
				&index_count
			) != 0 || parameter_count != type->parameter_count ||
			index_count != type->index_count ||
			checker_formation_classifier_matches(
				state, type->index_context, type->formation_classifier
			) != 1) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA,
				i
			);
		}
	}
	for (uint32_t i = 0;
		i < state->module->type_schema.constructor_count;
		++i) {
		int status = checker_charge(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_type_constructor* constructor =
			&state->module->type_schema.constructor_declarations[i];
		if (!checker_context_extends(
				state,
				constructor->field_context,
				constructor->parameter_context
			)) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA,
				i
			);
		}
		status = checker_is_type(
			state,
			constructor->field_context,
			constructor->result_classifier,
			0
		);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_dimensions(struct checker_state* state) {
	for (uint32_t operator_id = 0;
		operator_id < state->module->dimensions.operator_count;
		++operator_id) {
		int status = checker_charge(state, operator_id);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_dimension_operator* operator =
			&state->module->dimensions.operators[operator_id];
		if (operator->image_count != operator->source_dimension ||
			operator->image_offset > state->module->dimensions.image_count ||
			operator->image_count > state->module->dimensions.image_count -
				operator->image_offset) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_MALFORMED, operator_id
			);
		}
		for (size_t i = 0; i < operator->image_count; ++i) {
			const struct prototype_dimension_axis_image* image =
				&state->module->dimensions.images[
					operator->image_offset + i
				];
			if (image->kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_0 ||
				image->kind == PROTOTYPE_DIMENSION_AXIS_ENDPOINT_1) {
				if (image->target_axis != 0) {
					return checker_stop(
						state, PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_MALFORMED, operator_id
					);
				}
				continue;
			}
			if (image->kind != PROTOTYPE_DIMENSION_AXIS_TARGET ||
				image->target_axis >= operator->target_dimension) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_MALFORMED, operator_id
				);
			}
			for (size_t j = 0; j < i; ++j) {
				const struct prototype_dimension_axis_image* prior =
					&state->module->dimensions.images[
						operator->image_offset + j
					];
				if (prior->kind == PROTOTYPE_DIMENSION_AXIS_TARGET &&
					prior->target_axis == image->target_axis) {
					return checker_stop(
						state, PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_MALFORMED, operator_id
					);
				}
			}
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_occurrence_child(
	const struct checker_state* state,
	const struct prototype_semantic_occurrence* occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_child
) {
	for (uint32_t i = 0; i < occurrence->edge_count; ++i) {
		const struct prototype_semantic_occurrence_edge* edge =
			&state->module->occurrences.edges[occurrence->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal) {
			*p_child = edge->child_occurrence;
			return 0;
		}
	}
	return -1;
}

static int checker_type_view_application_matches(
	const struct checker_state* state,
	const struct prototype_term* result,
	const struct prototype_term* function,
	const struct prototype_semantic_occurrence* argument
) {
	if (!result || !function || result->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		function->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		result->as.type_view.view_type_id != function->as.type_view.view_type_id ||
		!checker_type_view_is_well_formed(state, result)) {
		return 0;
	}
	const struct prototype_term* core_application = checker_term(
		state, result->as.type_view.core
	);
	const struct prototype_term* source_application = checker_term(
		state, result->as.type_view.source
	);
	const struct prototype_term* argument_term = checker_term(
		state, argument->core_term
	);
	uint32_t expected_core_argument = argument_term &&
		argument_term->tag == PROTOTYPE_TERM_TYPE_VIEW ?
			argument_term->as.type_view.core : argument->core_term;
	return core_application && core_application->tag == PROTOTYPE_TERM_APP &&
		core_application->as.app.function == function->as.type_view.core &&
		core_application->as.app.argument == expected_core_argument &&
		source_application && source_application->tag == PROTOTYPE_TERM_APP &&
		source_application->as.app.function == function->as.type_view.source &&
		source_application->as.app.argument == argument->core_term;
}

static int checker_type_view_classifier_matches(
	const struct checker_state* state,
	uint32_t type_view_id,
	uint32_t asserted_classifier
) {
	const struct prototype_term* view = checker_term(state, type_view_id);
	if (!view || view->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		view->as.type_view.view_type_id >= state->module->type_schema.type_count) {
		return 0;
	}
	const struct prototype_semantic_type_declaration* declaration =
		&state->module->type_schema.type_declarations[
			view->as.type_view.view_type_id
		];
	uint32_t type_id;
	uint32_t arguments[64];
	size_t argument_count;
	if (checker_type_instance_arguments(
			state, type_view_id, &type_id, arguments, 64, &argument_count
		) != 0 || type_id != declaration->type_index || argument_count >
			declaration->parameter_count + declaration->index_count) {
		return 0;
	}
	uint32_t bindings[64];
	uint32_t classifier = declaration->formation_classifier;
	for (size_t i = 0; i < argument_count; ++i) {
		uint32_t domain;
		uint32_t binding;
		uint32_t body;
		if (checker_pi_parts(
				state, classifier, &domain, &binding, &body
			) != 0) {
			return 0;
		}
		bindings[i] = binding;
		classifier = body;
		(void)domain;
	}
	return checker_term_equal_after_bindings(
		state, classifier, bindings, arguments, argument_count,
		asserted_classifier, 0
	);
}

static int checker_host_type_matches(
	const struct checker_state* state,
	uint32_t term_id,
	int host_type
) {
	const struct prototype_term* term = checker_term(state, term_id);
	if (!term) {
		return 0;
	}
	switch (host_type) {
		case PROTOTYPE_HOST_TYPE_TEXT:
			return term->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT;
		case PROTOTYPE_HOST_TYPE_INT32:
			return term->tag == PROTOTYPE_TERM_PRIMITIVE_INT;
		case PROTOTYPE_HOST_TYPE_INT64:
			return term->tag == PROTOTYPE_TERM_PRIMITIVE_INT64;
		default:
			return 0;
	}
}

static int checker_pure_primitive_classifier_matches(
	const struct checker_state* state,
	const struct prototype_term* primitive,
	uint32_t classifier_id
) {
	const struct prototype_pure_primitive_declaration* declaration =
		checker_pure_primitive_declaration(
			state, primitive->as.pure_primitive.primitive_id
		);
	if (!declaration || declaration->arity >
		PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY) {
		return 0;
	}
	for (uint32_t i = 0; i < declaration->arity; ++i) {
		uint32_t domain;
		uint32_t binding;
		uint32_t body;
		if (checker_pi_parts(
				state, classifier_id, &domain, &binding, &body
			) != 0 || !checker_host_type_matches(
				state, domain, declaration->argument_types[i]
			)) {
			return 0;
		}
		classifier_id = body;
		(void)binding;
	}
	struct checker_computation_type_view result;
	const struct prototype_term* effect;
	return checker_computation_type_view(state, classifier_id, &result) == 0 &&
		(effect = checker_term(state, result.effect_row)) != NULL &&
		effect->tag == PROTOTYPE_TERM_EFFECT_ROW_EMPTY &&
		result.totality == PROTOTYPE_COMPUTATION_TOTALITY_TOTAL &&
		checker_host_type_matches(
			state, result.result, declaration->result_type
		);
}

static int checker_effect_operation_classifier_matches(
	const struct checker_state* state,
	const struct prototype_term* operation,
	uint32_t classifier_id
) {
	const struct prototype_effect_operation_declaration* declaration =
		checker_effect_operation_declaration(
			state, operation->as.effect_operation.operation_id
		);
	uint32_t declared_classifier_id = classifier_id;
	const struct prototype_term* classifier = checker_term(state, classifier_id);
	uint32_t effect_binding = PROTOTYPE_INVALID_ID;
	if (classifier && classifier->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		effect_binding = classifier->as.effect_row_forall.binding_id;
		classifier_id = classifier->as.effect_row_forall.body;
	}
	uint32_t domain;
	uint32_t binding;
	uint32_t body;
	struct checker_computation_type_view result;
	if (!declaration || declaration->arity != 1 ||
		operation->as.effect_operation.classifier != declared_classifier_id ||
		checker_pi_parts(
			state, classifier_id, &domain, &binding, &body
		) != 0 || checker_computation_type_view(state, body, &result) != 0 ||
		result.totality != PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ||
		!checker_host_type_matches(
			state, result.result, PROTOTYPE_HOST_TYPE_TEXT
		)) {
		return 0;
	}
	const struct prototype_term* row = checker_term(state, result.effect_row);
	if (!row || row->tag != PROTOTYPE_TERM_EFFECT_ROW_OPERATION ||
		row->as.effect_row_operation.operation_id != declaration->operation_id) {
		return 0;
	}
	const struct prototype_term* latent = checker_term(
		state, row->as.effect_row_operation.latent_row
	);
	if (declaration->classifier_schema ==
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT) {
		return effect_binding == PROTOTYPE_INVALID_ID &&
			checker_host_type_matches(
			state, domain, PROTOTYPE_HOST_TYPE_TEXT
		) && latent && latent->tag == PROTOTYPE_TERM_EFFECT_ROW_EMPTY;
	}
	if (declaration->classifier_schema !=
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT) {
		return 0;
	}
	const struct prototype_term* thunk = checker_term(state, domain);
	struct checker_computation_type_view suspended;
	const struct prototype_term* suspended_row;
	return effect_binding != PROTOTYPE_INVALID_ID && thunk &&
		thunk->tag == PROTOTYPE_TERM_THUNK_TYPE &&
		checker_computation_type_view(
			state, thunk->as.thunk_type.computation, &suspended
		) == 0 && (suspended_row = checker_term(
			state, suspended.effect_row
		)) != NULL && suspended_row->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
		suspended_row->as.effect_row_var.binding_id == effect_binding &&
		checker_host_type_matches(
			state, suspended.result, PROTOTYPE_HOST_TYPE_TEXT
		) && latent && latent->tag == PROTOTYPE_TERM_EFFECT_ROW_EMPTY;
}

static int checker_check_atom(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* core = checker_term(state, occurrence->core_term);
	const struct prototype_term* classifier = checker_term(
		state, occurrence->asserted_classifier
	);
	if (!core || !classifier) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			occurrence_id
		);
	}
	if (core->tag == PROTOTYPE_TERM_APP) {
		const struct prototype_term* former = checker_term(
			state, core->as.app.function
		);
		if (former && (former->tag == PROTOTYPE_TERM_TERMINATES_TYPE_FORMER ||
			former->tag == PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER)) {
			uint32_t computation_occurrence_id;
			if (checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION,
					0,
					&computation_occurrence_id
				) != 0) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* computation_occurrence =
				&state->module->occurrences.occurrences[
					computation_occurrence_id
				];
			const struct prototype_term* thunk_type = checker_term(
				state, computation_occurrence->asserted_classifier
			);
			if (computation_occurrence->core_term != core->as.app.argument ||
				!thunk_type || thunk_type->tag != PROTOTYPE_TERM_THUNK_TYPE) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
				);
			}
			if (former->tag == PROTOTYPE_TERM_TERMINATES_TYPE_FORMER) {
				return checker_is_universe(
					state, occurrence->asserted_classifier
				) ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
				);
			}
			const struct prototype_term* relation = classifier->tag ==
				PROTOTYPE_TERM_APP ? checker_term(
					state, classifier->as.app.function
				) : NULL;
			const struct prototype_term* computation_type = checker_term(
				state, thunk_type->as.thunk_type.computation
			);
			return relation &&
				relation->tag == PROTOTYPE_TERM_TERMINATES_TYPE_FORMER &&
				classifier->as.app.argument == core->as.app.argument &&
				computation_type &&
				computation_type->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
				computation_type->as.computation_type.totality ==
					PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ?
				PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
				);
		}
	}
	if (core->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		if (!checker_type_view_is_well_formed(state, core)) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA,
				occurrence_id
			);
		}
		int classifier_matches = checker_type_view_classifier_matches(
			state, occurrence->core_term, occurrence->asserted_classifier
		);
		return classifier_matches == 1 ? PROTOTYPE_CHECKER_COMPLETE :
			checker_stop(
				state,
				classifier_matches < 0 ? PROTOTYPE_CHECKER_PAUSED :
					PROTOTYPE_CHECKER_REJECTED,
				classifier_matches < 0 ? PROTOTYPE_CHECKER_STOP_UNSUPPORTED :
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				occurrence_id
			);
	}
	if (core->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT ||
		core->tag == PROTOTYPE_TERM_PRIMITIVE_INT ||
		core->tag == PROTOTYPE_TERM_PRIMITIVE_INT64) {
		return checker_is_universe(state, occurrence->asserted_classifier) ?
			PROTOTYPE_CHECKER_COMPLETE : checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				occurrence_id
			);
	}
	if (core->tag == PROTOTYPE_TERM_TEXT_LITERAL &&
		classifier->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT) {
		return PROTOTYPE_CHECKER_COMPLETE;
	}
	if (core->tag == PROTOTYPE_TERM_INT_LITERAL &&
		checker_host_type_matches(
			state,
			occurrence->asserted_classifier,
			state->module->intrinsic_environment.default_integer_host_type
		)) {
		if (state->module->intrinsic_environment.default_integer_host_type ==
				PROTOTYPE_HOST_TYPE_INT32 &&
			(core->as.int_literal.value < INT32_MIN ||
			 core->as.int_literal.value > INT32_MAX)) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				occurrence_id
			);
		}
		return PROTOTYPE_CHECKER_COMPLETE;
	}
	if (core->tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		return checker_pure_primitive_classifier_matches(
			state, core, occurrence->asserted_classifier
		) ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER,
			occurrence_id
		);
	}
	if (core->tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		return checker_effect_operation_classifier_matches(
			state, core, occurrence->asserted_classifier
		) ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER,
			occurrence_id
		);
	}
	/* The interface pass resolves the qualified capability and checks this
	 * asserted classifier against the checked provider. No external body is
	 * available to the occurrence checker. */
	if (core->tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		return PROTOTYPE_CHECKER_COMPLETE;
	}
	return checker_stop(
		state,
		PROTOTYPE_CHECKER_PAUSED,
		PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
		occurrence_id
	);
}

static int checker_motive_application_has_shape(
	const struct checker_state* state,
	uint32_t motive,
	const uint32_t* index_arguments,
	size_t index_count,
	uint32_t value,
	uint32_t application
) {
	if (index_count > 64) {
		return 0;
	}
	uint32_t lambda_cursor = motive;
	uint32_t lambda_bindings[65];
	size_t lambda_count = 0;
	while (lambda_count <= index_count) {
		const struct prototype_term* lambda = checker_term(
			state, lambda_cursor
		);
		if (!lambda || lambda->tag != PROTOTYPE_TERM_LAMBDA) {
			break;
		}
		lambda_bindings[lambda_count] = lambda->as.lambda.binding_id;
		++lambda_count;
		lambda_cursor = lambda->as.lambda.body;
	}
	size_t motive_index_count;
	if (lambda_count == 1) {
		motive_index_count = 0;
	} else if (lambda_count == index_count + 1) {
		motive_index_count = index_count;
	} else {
		return 0;
	}
	const struct prototype_term* application_cursor = checker_term(
		state, application
	);
	if (application_cursor && application_cursor->tag == PROTOTYPE_TERM_APP &&
		checker_term_equal_after_binding(
			state, application_cursor->as.app.argument,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, value, 0
		) == 1) {
		uint32_t function = application_cursor->as.app.function;
		int exact_spine = 1;
		for (size_t i = motive_index_count; i != 0; --i) {
			application_cursor = checker_term(state, function);
			if (!application_cursor ||
				application_cursor->tag != PROTOTYPE_TERM_APP ||
				checker_term_equal_after_binding(
					state, application_cursor->as.app.argument,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					index_arguments[i - 1], 0
				) != 1) {
				exact_spine = 0;
				break;
			}
			function = application_cursor->as.app.function;
		}
		if (exact_spine && function == motive) {
			return 1;
		}
	}
	uint32_t arguments[65];
	for (size_t i = 0; i < motive_index_count; ++i) {
		arguments[i] = index_arguments[i];
	}
	arguments[motive_index_count] = value;
	return checker_term_equal_after_bindings(
		state,
		lambda_cursor,
		lambda_bindings,
		arguments,
		motive_index_count + 1,
		application,
		0
	) == 1;
}

static int checker_match_case_result_arguments(
	const struct checker_state* state,
	const struct prototype_semantic_type_declaration* declaration,
	const struct prototype_semantic_match_case* semantic_case,
	uint32_t* result_arguments,
	size_t result_capacity,
	size_t* p_result_count,
	uint32_t** p_binding_ids,
	uint32_t** p_argument_ids,
	size_t* p_binding_count
) {
	if (!declaration || !semantic_case || !p_result_count || !p_binding_ids ||
		!p_argument_ids || !p_binding_count || semantic_case->constructor_id >=
		declaration->constructor_count) {
		return -1;
	}
	const struct prototype_semantic_type_constructor* constructor =
		&state->module->type_schema.constructor_declarations[
			declaration->first_constructor + semantic_case->constructor_id
		];
	size_t parameter_count = declaration->parameter_count;
	size_t binder_count = semantic_case->binder_count;
	size_t capacity = parameter_count + binder_count;
	if (capacity < parameter_count) {
		return -1;
	}
	uint32_t* binding_ids = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*binding_ids)
	);
	uint32_t* argument_ids = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*argument_ids)
	);
	uint32_t* field_contexts = semantic_case->binder_count == 0 ? NULL : malloc(
		semantic_case->binder_count * sizeof(*field_contexts)
	);
	if ((capacity != 0 && (!binding_ids || !argument_ids)) ||
		(semantic_case->binder_count != 0 && !field_contexts)) {
		free(binding_ids);
		free(argument_ids);
		free(field_contexts);
		return -1;
	}
	if (checker_parameter_instantiation(
			state,
			declaration,
			semantic_case->constructor_owner,
			binding_ids,
			argument_ids,
			declaration->parameter_count
		) != 0) {
		free(binding_ids);
		free(argument_ids);
		free(field_contexts);
		return -1;
	}
	uint32_t cursor = constructor->field_context;
	for (uint32_t i = semantic_case->binder_count; i != 0; --i) {
		if (cursor == constructor->parameter_context || cursor == 0) {
			free(binding_ids);
			free(argument_ids);
			free(field_contexts);
			return -1;
		}
		field_contexts[i - 1] = cursor;
		cursor = state->module->contexts.contexts[cursor].parent;
	}
	if (cursor != constructor->parameter_context) {
		free(binding_ids);
		free(argument_ids);
		free(field_contexts);
		return -1;
	}
	for (uint32_t i = 0; i < semantic_case->binder_count; ++i) {
		uint32_t variable = checker_find_variable_term(
			state, semantic_case->binder_ids[i]
		);
		if (variable == PROTOTYPE_INVALID_ID) {
			free(binding_ids);
			free(argument_ids);
			free(field_contexts);
			return -1;
		}
		binding_ids[declaration->parameter_count + i] =
			state->module->contexts.contexts[field_contexts[i]].binding_id;
		argument_ids[declaration->parameter_count + i] = variable;
	}
	free(field_contexts);
	uint32_t result_type;
	size_t result_count;
	if (checker_type_instance_arguments(
			state,
			constructor->result_classifier,
			&result_type,
			result_arguments,
			result_capacity,
			&result_count
		) != 0 || result_type != declaration->type_index || result_count !=
			declaration->parameter_count + declaration->index_count) {
		free(binding_ids);
		free(argument_ids);
		return -1;
	}
	*p_result_count = result_count;
	*p_binding_ids = binding_ids;
	*p_argument_ids = argument_ids;
	*p_binding_count = capacity;
	return 0;
}

static int checker_check_match(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* match = checker_term(
		state, occurrence->core_term
	);
	const struct prototype_term* motive = checker_term(
		state, occurrence->match_motive
	);
	const struct prototype_term* result = checker_term(
		state, occurrence->asserted_classifier
	);
	uint32_t scrutinee_occurrence_id;
	if (!match || match->tag != PROTOTYPE_TERM_MATCH || !motive ||
		motive->tag != PROTOTYPE_TERM_LAMBDA || !result ||
		match->as.match.case_count != occurrence->case_count ||
		checker_occurrence_child(
			state,
			occurrence,
			PROTOTYPE_TERM_CHILD_SCRUTINEE,
			0,
			&scrutinee_occurrence_id
		) != 0) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* scrutinee =
		&state->module->occurrences.occurrences[scrutinee_occurrence_id];
	if (scrutinee->core_term != match->as.match.scrutinee) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			occurrence_id
		);
	}
	const struct prototype_term* scrutinee_type = checker_term(
		state, scrutinee->asserted_classifier
	);
	if (!scrutinee_type || scrutinee_type->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		scrutinee_type->as.type_view.view_type_id >=
			state->module->type_schema.type_count) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
			occurrence_id
		);
	}
	const struct prototype_semantic_type_declaration* declaration =
		&state->module->type_schema.type_declarations[
			scrutinee_type->as.type_view.view_type_id
		];
	uint32_t scrutinee_type_id;
	uint32_t scrutinee_arguments[64];
	size_t scrutinee_argument_count;
	int instance_status = checker_type_instance_arguments(
			state,
			scrutinee->asserted_classifier,
			&scrutinee_type_id,
			scrutinee_arguments,
			64,
			&scrutinee_argument_count
		);
	int motive_shape = instance_status == 0 ?
		checker_motive_application_has_shape(
			state,
			occurrence->match_motive,
			&scrutinee_arguments[declaration->parameter_count],
			declaration->index_count,
			match->as.match.scrutinee,
			occurrence->asserted_classifier
		) : 0;
	if (instance_status != 0 || scrutinee_type_id != declaration->type_index ||
		scrutinee_argument_count !=
			declaration->parameter_count + declaration->index_count ||
		!motive_shape) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	if (declaration->index_count == 0) {
		uint32_t motive_binding = motive->as.lambda.binding_id;
		uint32_t motive_domain = scrutinee->asserted_classifier;
		int status = checker_is_type_with_locals(
			state,
			occurrence->context_id,
			motive->as.lambda.body,
			&motive_binding,
			&motive_domain,
			1,
			0
		);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
	}
	if (occurrence->case_count != declaration->constructor_count ||
		occurrence->first_case > state->module->occurrences.case_count ||
		occurrence->case_count > state->module->occurrences.case_count -
			occurrence->first_case ||
		match->as.match.first_case > state->module->terms.case_count ||
		match->as.match.case_count > state->module->terms.case_count -
			match->as.match.first_case) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			occurrence_id
		);
	}
	uint64_t covered = 0;
	if (declaration->constructor_count > 64) {
		return checker_stop(
			state,
			PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
			occurrence_id
		);
	}
	for (uint32_t i = 0; i < occurrence->case_count; ++i) {
		const struct prototype_semantic_match_case* semantic_case =
			&state->module->occurrences.cases[occurrence->first_case + i];
		const struct prototype_match_case* term_case =
			&state->module->terms.cases[match->as.match.first_case + i];
		uint32_t branch_occurrence_id;
		int semantic_owner_matches = checker_constructor_owners_match(
				state,
				semantic_case->constructor_owner,
				scrutinee->asserted_classifier
			);
		int term_owner_matches = checker_constructor_owners_match(
				state,
				semantic_case->constructor_owner,
				term_case->constructor_owner
			);
		int branch_child_status = checker_occurrence_child(
				state,
				occurrence,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				i,
				&branch_occurrence_id
			);
		if (semantic_case->constructor_id >= declaration->constructor_count ||
			(covered & (UINT64_C(1) << semantic_case->constructor_id)) != 0 ||
			!semantic_owner_matches ||
			semantic_case->constructor_id != term_case->constructor_id ||
			!term_owner_matches ||
			semantic_case->binder_count != term_case->binder_count ||
			branch_child_status != 0) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE,
				occurrence_id
			);
		}
		covered |= UINT64_C(1) << semantic_case->constructor_id;
		for (uint32_t j = 0; j < semantic_case->binder_count; ++j) {
			if (semantic_case->binder_ids[j] != state->module->terms.case_binders[
					term_case->first_binder + j
				].binding_id) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
		}
		const struct prototype_semantic_occurrence* branch =
			&state->module->occurrences.occurrences[branch_occurrence_id];
		int branch_body_equal;
		if (branch->core_term == term_case->body) {
			branch_body_equal = 1;
		} else if (branch->context_action_substitution != PROTOTYPE_INVALID_ID &&
			branch->origin_core_term == term_case->body &&
			branch->context_action_substitution <
				state->module->substitutions.substitution_count) {
			const struct prototype_semantic_substitution* action =
				&state->module->substitutions.substitutions[
					branch->context_action_substitution
				];
			branch_body_equal = action->source_context == branch->context_id &&
				checker_term_equal_reindexed(
					state,
					branch->origin_core_term,
					branch->context_action_substitution,
					branch->core_term
				) == 1;
		} else {
			branch_body_equal = 0;
		}
		if (branch->context_id != semantic_case->context_id ||
			!branch_body_equal) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE,
				occurrence_id
			);
		}
		if (semantic_case->refinement_kind ==
				PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_IMPOSSIBLE) {
			continue;
		}
		uint32_t pattern_term = match->as.match.scrutinee;
		if (semantic_case->refinement_kind ==
				PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_SOLVED) {
			if (semantic_case->refinement_substitution >=
					state->module->substitutions.substitution_count) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_SUBSTITUTION,
					semantic_case->refinement_substitution
				);
			}
			const struct prototype_semantic_substitution* refinement =
				&state->module->substitutions.substitutions[
					semantic_case->refinement_substitution
				];
			if (refinement->source_context != semantic_case->context_id ||
				!checker_context_extends(
					state,
					refinement->target_context,
					occurrence->context_id
				) ||
				scrutinee->binding_id == PROTOTYPE_INVALID_ID) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_SUBSTITUTION,
					semantic_case->refinement_substitution
				);
			}
			int image_status = checker_substitution_binding_image(
				state,
				semantic_case->refinement_substitution,
				scrutinee->binding_id,
				&pattern_term
			);
			if (image_status > 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_PAUSED,
					PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
					semantic_case->refinement_substitution
				);
			}
			if (image_status < 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_SUBSTITUTION,
					semantic_case->refinement_substitution
				);
			}
		}
		uint32_t case_arguments[64];
		size_t case_argument_count;
		uint32_t* case_bindings = NULL;
		uint32_t* case_values = NULL;
		size_t case_binding_count = 0;
		int case_result_status = checker_match_case_result_arguments(
			state,
			declaration,
			semantic_case,
			case_arguments,
			64,
			&case_argument_count,
			&case_bindings,
			&case_values,
			&case_binding_count
		);
		int branch_equal = case_result_status == 0 ?
			checker_indexed_motive_application_equal(
				state,
				occurrence->match_motive,
				&case_arguments[declaration->parameter_count],
				declaration->index_count,
				pattern_term,
				case_bindings,
				case_values,
				case_binding_count,
				branch->asserted_classifier,
				0
			) : 0;
		free(case_bindings);
		free(case_values);
		if (branch_equal < 0) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_PAUSED,
				PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
				occurrence_id
			);
		}
		if (!branch_equal) {
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				branch_occurrence_id
			);
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_induction_hypothesis(
	struct checker_state* state,
	uint32_t occurrence_id
);

static int checker_type_instance_arguments(
	const struct checker_state* state,
	uint32_t classifier_id,
	uint32_t* p_type_id,
	uint32_t* arguments,
	size_t capacity,
	size_t* p_argument_count
) {
	const struct prototype_term* view = checker_term(state, classifier_id);
	if (!view || view->tag != PROTOTYPE_TERM_TYPE_VIEW ||
		view->as.type_view.view_type_id >= state->module->type_schema.type_count ||
		!p_type_id || !p_argument_count) {
		return -1;
	}
	const struct prototype_term* cursor = checker_term(
		state, view->as.type_view.source
	);
	size_t count = 0;
	while (cursor && cursor->tag == PROTOTYPE_TERM_APP) {
		if (count == capacity) {
			return -1;
		}
		arguments[count++] = cursor->as.app.argument;
		cursor = checker_term(state, cursor->as.app.function);
	}
	if (!cursor || cursor->tag != PROTOTYPE_TERM_TYPE_DECLARATION ||
		cursor->as.type_declaration.type_id != view->as.type_view.view_type_id) {
		return -1;
	}
	for (size_t i = 0; i < count / 2; ++i) {
		uint32_t temporary = arguments[i];
		arguments[i] = arguments[count - 1 - i];
		arguments[count - 1 - i] = temporary;
	}
	*p_type_id = view->as.type_view.view_type_id;
	*p_argument_count = count;
	return 0;
}

static uint32_t checker_find_app_term(
	const struct checker_state* state,
	uint32_t function,
	uint32_t argument
) {
	for (uint32_t i = 0; i < state->module->terms.term_count; ++i) {
		const struct prototype_term* term = checker_term(state, i);
		if (term->tag == PROTOTYPE_TERM_APP &&
			term->as.app.function == function &&
			term->as.app.argument == argument) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int checker_recursive_instance_matches(
	const struct checker_state* state,
	uint32_t candidate,
	uint32_t scrutinee_classifier,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count,
	uint32_t* index_arguments,
	size_t index_capacity,
	size_t* p_index_count
) {
	uint32_t candidate_type;
	uint32_t scrutinee_type;
	uint32_t candidate_arguments[64];
	uint32_t scrutinee_arguments[64];
	size_t candidate_count;
	size_t scrutinee_count;
	if (checker_type_instance_arguments(
			state, candidate, &candidate_type, candidate_arguments, 64,
			&candidate_count
		) != 0 || checker_type_instance_arguments(
			state, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, 64, &scrutinee_count
		) != 0 || candidate_type != scrutinee_type) {
		return 0;
	}
	const struct prototype_semantic_type_declaration* type =
		&state->module->type_schema.type_declarations[candidate_type];
	if (candidate_count != type->parameter_count + type->index_count ||
		scrutinee_count != candidate_count || type->index_count > index_capacity) {
		return 0;
	}
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		if (checker_term_equal_after_bindings(
				state,
				candidate_arguments[i],
				binding_ids,
				argument_ids,
				binding_count,
				scrutinee_arguments[i],
				0
			) != 1) {
			return 0;
		}
	}
	for (uint32_t i = 0; i < type->index_count; ++i) {
		index_arguments[i] = candidate_arguments[type->parameter_count + i];
	}
	*p_index_count = type->index_count;
	return 1;
}

static int checker_indexed_motive_application_equal(
	const struct checker_state* state,
	uint32_t motive_id,
	const uint32_t* index_arguments,
	size_t index_count,
	uint32_t value,
	const uint32_t* inherited_bindings,
	const uint32_t* inherited_arguments,
	size_t inherited_count,
	uint32_t target,
	int compare_computation_result
) {
	uint32_t lambda_bindings[65];
	uint32_t cursor = motive_id;
	size_t lambda_count = 0;
	while (lambda_count < 65) {
		const struct prototype_term* lambda = checker_term(state, cursor);
		if (!lambda || lambda->tag != PROTOTYPE_TERM_LAMBDA) {
			break;
		}
		lambda_bindings[lambda_count++] = lambda->as.lambda.binding_id;
		cursor = lambda->as.lambda.body;
	}
	size_t motive_index_count;
	if (lambda_count == 1) {
		motive_index_count = 0;
	} else if (lambda_count == index_count + 1) {
		motive_index_count = index_count;
	} else {
		return 0;
	}
	if (inherited_count > SIZE_MAX - motive_index_count - 1) {
		return -1;
	}
	size_t count = inherited_count + motive_index_count + 1;
	uint32_t* bindings = malloc(count * sizeof(*bindings));
	uint32_t* arguments = malloc(count * sizeof(*arguments));
	if (!bindings || !arguments) {
		free(bindings);
		free(arguments);
		return -1;
	}
	if (inherited_count != 0) {
		memcpy(bindings, inherited_bindings,
			inherited_count * sizeof(*bindings));
		memcpy(arguments, inherited_arguments,
			inherited_count * sizeof(*arguments));
	}
	for (size_t i = 0; i < motive_index_count; ++i) {
		bindings[inherited_count + i] = lambda_bindings[i];
		arguments[inherited_count + i] = index_arguments[i];
	}
	bindings[count - 1] = lambda_bindings[motive_index_count];
	arguments[count - 1] = value;
	int equal = checker_term_equal_after_bindings(
		state, cursor, bindings, arguments, count, target, 0
	);
	if (equal == 0 && compare_computation_result) {
		const struct prototype_term* body = checker_term(state, cursor);
		if (body && body->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
			equal = checker_term_equal_after_bindings(
				state, body->as.computation_type.result,
				bindings, arguments, count, target, 0
			);
		}
	}
	free(bindings);
	free(arguments);
	return equal;
}

static int checker_lifted_recursive_classifier_equal(
	const struct checker_state* state,
	uint32_t candidate,
	uint32_t field_value,
	uint32_t scrutinee_classifier,
	uint32_t motive,
	const uint32_t* binding_ids,
	const uint32_t* argument_ids,
	size_t binding_count,
	uint32_t target,
	int under_computation,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count) {
		return -1;
	}
	uint32_t indices[64];
	size_t index_count = 0;
	int direct = checker_recursive_instance_matches(
		state, candidate, scrutinee_classifier, binding_ids, argument_ids,
		binding_count, indices, 64, &index_count
	);
	if (direct < 0) {
		return -1;
	}
	if (direct) {
		return checker_indexed_motive_application_equal(
			state, motive, indices, index_count, field_value,
			binding_ids, argument_ids, binding_count, target,
			under_computation
		);
	}
	const struct prototype_term* candidate_term = checker_term(state, candidate);
	const struct prototype_term* target_term = checker_term(state, target);
	if (!candidate_term || !target_term) {
		return 0;
	}
	if (target_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		candidate_term->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* row = checker_term(
			state, target_term->as.computation_type.label
		);
		if (!row || row->tag != PROTOTYPE_TERM_EFFECT_ROW_EMPTY ||
			target_term->as.computation_type.totality !=
				PROTOTYPE_COMPUTATION_TOTALITY_TOTAL) {
			return -1;
		}
		return checker_lifted_recursive_classifier_equal(
			state, candidate, field_value, scrutinee_classifier, motive,
			binding_ids, argument_ids, binding_count,
			target_term->as.computation_type.result, under_computation,
			depth + 1
		);
	}
	if (candidate_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return target_term->tag == PROTOTYPE_TERM_THUNK_TYPE ?
			checker_lifted_recursive_classifier_equal(
				state,
				candidate_term->as.thunk_type.computation,
				field_value,
				scrutinee_classifier,
				motive,
				binding_ids,
				argument_ids,
				binding_count,
				target_term->as.thunk_type.computation,
				under_computation,
				depth + 1
			) : 0;
	}
	if (candidate_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* row = checker_term(
			state, candidate_term->as.computation_type.label
		);
		if (!row || row->tag != PROTOTYPE_TERM_EFFECT_ROW_EMPTY ||
			candidate_term->as.computation_type.totality !=
				PROTOTYPE_COMPUTATION_TOTALITY_TOTAL) {
			return -1;
		}
		int flattened = checker_lifted_recursive_classifier_equal(
			state,
			candidate_term->as.computation_type.result,
			field_value,
			scrutinee_classifier,
			motive,
			binding_ids,
			argument_ids,
			binding_count,
			target,
			1,
			depth + 1
		);
		if (flattened != 0) {
			return flattened;
		}
		if (target_term->tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			target_term->as.computation_type.totality !=
				candidate_term->as.computation_type.totality ||
			checker_term_equal_after_bindings(
				state,
				candidate_term->as.computation_type.label,
				binding_ids,
				argument_ids,
				binding_count,
				target_term->as.computation_type.label,
				0
			) != 1) {
			return 0;
		}
		return checker_lifted_recursive_classifier_equal(
			state,
			candidate_term->as.computation_type.result,
			field_value,
			scrutinee_classifier,
			motive,
			binding_ids,
			argument_ids,
			binding_count,
			target_term->as.computation_type.result,
			1,
			depth + 1
		);
	}
	uint32_t candidate_domain;
	uint32_t candidate_binding;
	uint32_t candidate_body;
	uint32_t target_domain;
	uint32_t target_binding;
	uint32_t target_body;
	if (checker_pi_parts(
			state, candidate, &candidate_domain, &candidate_binding,
			&candidate_body
		) != 0 || checker_pi_parts(
			state, target, &target_domain, &target_binding, &target_body
		) != 0 || checker_term_equal_after_bindings(
			state, candidate_domain, binding_ids, argument_ids, binding_count,
			target_domain, 0
		) != 1) {
		return 0;
	}
	uint32_t target_variable = checker_find_variable_term(state, target_binding);
	if (candidate_binding == target_binding) {
		uint32_t applied_value = target_variable == PROTOTYPE_INVALID_ID ?
			field_value : checker_find_app_term(
				state, field_value, target_variable
			);
		if (applied_value == PROTOTYPE_INVALID_ID) {
			applied_value = field_value;
		}
		return checker_lifted_recursive_classifier_equal(
			state, candidate_body, applied_value, scrutinee_classifier, motive,
			binding_ids, argument_ids, binding_count, target_body,
			under_computation, depth + 1
		);
	}
	uint32_t applied_value = target_variable == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_INVALID_ID : checker_find_app_term(
			state, field_value, target_variable
		);
	if (binding_count == SIZE_MAX) {
		return -1;
	}
	/* A constant motive erases the applied recursive value, so the producer may
	 * not retain that APP in the reachable checked graph. In that case the
	 * original value is a sound placeholder: comparison can succeed only when
	 * the motive's endpoint binder is actually unused. */
	if (target_variable == PROTOTYPE_INVALID_ID) {
		target_variable = field_value;
	}
	if (applied_value == PROTOTYPE_INVALID_ID) {
		applied_value = field_value;
	}
	uint32_t* nested_bindings = malloc(
		(binding_count + 1) * sizeof(*nested_bindings)
	);
	uint32_t* nested_arguments = malloc(
		(binding_count + 1) * sizeof(*nested_arguments)
	);
	if (!nested_bindings || !nested_arguments) {
		free(nested_bindings);
		free(nested_arguments);
		return -1;
	}
	if (binding_count != 0) {
		memcpy(nested_bindings, binding_ids,
			binding_count * sizeof(*nested_bindings));
		memcpy(nested_arguments, argument_ids,
			binding_count * sizeof(*nested_arguments));
	}
	nested_bindings[binding_count] = candidate_binding;
	nested_arguments[binding_count] = target_variable;
	int equal = checker_lifted_recursive_classifier_equal(
		state, candidate_body, applied_value, scrutinee_classifier, motive,
		nested_bindings, nested_arguments, binding_count + 1, target_body,
		under_computation, depth + 1
	);
	free(nested_bindings);
	free(nested_arguments);
	return equal;
}

static int checker_check_induction_hypothesis(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* induction = checker_term(
		state, occurrence->core_term
	);
	uint32_t argument_occurrence_id;
	if (!induction || induction->tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		occurrence->ih_owner_occurrence >=
			state->module->occurrences.occurrence_count ||
		occurrence->ih_scope_id >= state->module->terms.ih_scope_count ||
		induction->as.induction_hypothesis.ih_scope_id !=
			occurrence->ih_scope_id || checker_occurrence_child(
			state,
			occurrence,
			PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT,
			0,
			&argument_occurrence_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* owner =
		&state->module->occurrences.occurrences[
			occurrence->ih_owner_occurrence
		];
	const struct prototype_semantic_occurrence* argument =
		&state->module->occurrences.occurrences[argument_occurrence_id];
	const struct prototype_term* owner_match = checker_term(
		state, owner->core_term
	);
	const struct prototype_term* motive = checker_term(
		state, owner->match_motive
	);
	const struct prototype_term* argument_term = checker_term(
		state, argument->core_term
	);
	const struct prototype_semantic_ih_scope* scope =
		&state->module->terms.ih_scopes[occurrence->ih_scope_id];
	if (owner->kind != PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH || !owner_match ||
		owner_match->tag != PROTOTYPE_TERM_MATCH || !motive ||
		motive->tag != PROTOTYPE_TERM_LAMBDA || !argument_term ||
		argument_term->tag != PROTOTYPE_TERM_VAR ||
		argument->core_term != induction->as.induction_hypothesis.argument ||
		scope->match_term != owner->core_term || occurrence->ih_case_index >=
			owner->case_count || occurrence->ih_case_index >=
			owner_match->as.match.case_count) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_match_case* semantic_case =
		&state->module->occurrences.cases[
			owner->first_case + occurrence->ih_case_index
		];
	const struct prototype_match_case* core_case =
		&state->module->terms.cases[
			owner_match->as.match.first_case + occurrence->ih_case_index
		];
	if (occurrence->ih_field_index >= semantic_case->binder_count ||
		occurrence->ih_field_index >= core_case->binder_count) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_case_binder* core_binder =
		&state->module->terms.case_binders[
			core_case->first_binder + occurrence->ih_field_index
		];
	if (!core_binder->is_recursive ||
		semantic_case->binder_ids[occurrence->ih_field_index] !=
			argument_term->as.var.binding_id ||
		core_binder->binding_id != argument_term->as.var.binding_id) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	uint32_t scrutinee_occurrence_id;
	if (checker_occurrence_child(
			state, owner, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_occurrence_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* scrutinee =
		&state->module->occurrences.occurrences[scrutinee_occurrence_id];
	int classifier_equal = checker_lifted_recursive_classifier_equal(
		state,
		argument->asserted_classifier,
		argument->core_term,
		scrutinee->asserted_classifier,
		owner->match_motive,
		NULL,
		NULL,
		0,
		occurrence->asserted_classifier,
		0,
		0
	);
	if (classifier_equal < 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	return classifier_equal ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
		state, PROTOTYPE_CHECKER_REJECTED,
		PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
	);
}

static int checker_totality_join(int left, int right) {
	if (left == PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE ||
		right == PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE) {
		return PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE;
	}
	return left == PROTOTYPE_COMPUTATION_TOTALITY_TOTAL &&
		right == PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ?
			PROTOTYPE_COMPUTATION_TOTALITY_TOTAL :
			PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN;
}

static int checker_effect_union_atoms_match(
	const struct checker_state* state,
	uint32_t left,
	uint32_t right,
	uint32_t result
);

static int checker_effect_union_matches(
	const struct checker_state* state,
	uint32_t left,
	uint32_t right,
	uint32_t result
) {
	return checker_effect_union_atoms_match(state, left, right, result);
}

static int checker_operation_is_handled(
	int operation_id,
	const int* handled,
	uint32_t handled_count
) {
	for (uint32_t i = 0; i < handled_count; ++i) {
		if (handled[i] == operation_id) {
			return 1;
		}
	}
	return 0;
}

static int checker_collect_effect_atoms(
	const struct checker_state* state,
	uint32_t row_id,
	const int* handled,
	uint32_t handled_count,
	uint32_t* atoms,
	uint32_t capacity,
	uint32_t* p_count,
	uint32_t depth
) {
	if (depth > state->module->terms.term_count || *p_count > capacity) {
		return -1;
	}
	const struct prototype_term* row = checker_term(state, row_id);
	if (!row) {
		return -1;
	}
	switch (row->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return checker_collect_effect_atoms(
				state,
				row->as.effect_row_union.left,
				handled,
				handled_count,
				atoms,
				capacity,
				p_count,
				depth + 1
			) != 0 ? -1 : checker_collect_effect_atoms(
				state,
				row->as.effect_row_union.right,
				handled,
				handled_count,
				atoms,
				capacity,
				p_count,
				depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			if (checker_operation_is_handled(
					row->as.effect_row_operation.operation_id,
					handled,
					handled_count
				)) {
				return checker_collect_effect_atoms(
					state,
					row->as.effect_row_operation.latent_row,
					handled,
					handled_count,
					atoms,
					capacity,
					p_count,
					depth + 1
				);
			}
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			break;
		default:
			return -1;
	}
	if (*p_count == capacity) {
		return -1;
	}
	atoms[(*p_count)++] = row_id;
	return 0;
}

static int checker_effect_union_atoms_match(
	const struct checker_state* state,
	uint32_t left,
	uint32_t right,
	uint32_t result
) {
	if (state->module->terms.term_count > UINT32_MAX / 2) {
		return 0;
	}
	uint32_t capacity = (uint32_t)state->module->terms.term_count * 2;
	uint32_t* expected = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*expected)
	);
	uint32_t* actual = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*actual)
	);
	if (capacity != 0 && (!expected || !actual)) {
		free(expected);
		free(actual);
		return 0;
	}
	uint32_t expected_count = 0;
	uint32_t actual_count = 0;
	int valid = checker_collect_effect_atoms(
		state, left, NULL, 0, expected, capacity, &expected_count, 0
	) == 0 && checker_collect_effect_atoms(
		state, right, NULL, 0, expected, capacity, &expected_count, 0
	) == 0 && checker_collect_effect_atoms(
		state, result, NULL, 0, actual, capacity, &actual_count, 0
	) == 0 && expected_count == actual_count;
	unsigned char* matched = actual_count == 0 ? NULL : calloc(actual_count, 1);
	if (valid && actual_count != 0 && !matched) {
		valid = 0;
	}
	for (uint32_t i = 0; valid && i < expected_count; ++i) {
		int found = 0;
		for (uint32_t j = 0; j < actual_count; ++j) {
			if (!matched[j] && checker_term_equal_after_binding(
					state,
					expected[i],
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					actual[j],
					0
				) == 1) {
				matched[j] = 1;
				found = 1;
				break;
			}
		}
		if (!found) {
			valid = 0;
		}
	}
	free(matched);
	free(expected);
	free(actual);
	return valid;
}

static int checker_effect_row_is_well_formed(
	const struct checker_state* state,
	uint32_t row_id
) {
	uint32_t capacity = state->module->terms.term_count > UINT32_MAX ?
		UINT32_MAX : (uint32_t)state->module->terms.term_count;
	uint32_t* atoms = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*atoms)
	);
	if (capacity != 0 && !atoms) {
		return 0;
	}
	uint32_t count = 0;
	int valid = checker_collect_effect_atoms(
		state, row_id, NULL, 0, atoms, capacity, &count, 0
	) == 0;
	free(atoms);
	return valid;
}

static int checker_effect_rows_handle_match(
	const struct checker_state* state,
	uint32_t input_row,
	uint32_t carrier_row,
	const int* handled,
	uint32_t handled_count,
	uint32_t result_row
) {
	if (state->module->terms.term_count > UINT32_MAX / 2) {
		return -1;
	}
	uint32_t capacity = (uint32_t)state->module->terms.term_count * 2;
	uint32_t* expected = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*expected)
	);
	uint32_t* actual = capacity == 0 ? NULL : malloc(
		capacity * sizeof(*actual)
	);
	if (capacity != 0 && (!expected || !actual)) {
		free(expected);
		free(actual);
		return -1;
	}
	uint32_t expected_count = 0;
	uint32_t actual_count = 0;
	int status = checker_collect_effect_atoms(
		state,
		input_row,
		handled,
		handled_count,
		expected,
		capacity,
		&expected_count,
		0
	) != 0 || checker_collect_effect_atoms(
		state,
		carrier_row,
		NULL,
		0,
		expected,
		capacity,
		&expected_count,
		0
	) != 0 || checker_collect_effect_atoms(
		state,
		result_row,
		NULL,
		0,
		actual,
		capacity,
		&actual_count,
		0
	) != 0 ? -1 : 0;
	if (status == 0 && expected_count != actual_count) {
		status = 1;
	}
	unsigned char* matched = actual_count == 0 ? NULL : calloc(actual_count, 1);
	if (status == 0 && actual_count != 0 && !matched) {
		status = -1;
	}
	for (uint32_t i = 0; status == 0 && i < expected_count; ++i) {
		int found = 0;
		for (uint32_t j = 0; j < actual_count; ++j) {
			if (!matched[j] && checker_term_equal_after_binding(
					state,
					expected[i],
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					actual[j],
					0
				) == 1) {
				matched[j] = 1;
				found = 1;
				break;
			}
		}
		if (!found) {
			status = 1;
		}
	}
	free(matched);
	free(expected);
	free(actual);
	return status;
}

struct checker_effect_operation_instantiation {
	const struct prototype_effect_operation_declaration* declaration;
	uint32_t domain;
	uint32_t binding;
	uint32_t body;
	struct checker_computation_type_view result;
	uint32_t request_effect_row;
};

static int checker_instantiate_effect_operation(
	const struct checker_state* state,
	const struct prototype_semantic_occurrence* operation,
	uint32_t argument_classifier_id,
	struct checker_effect_operation_instantiation* p_instantiation
) {
	const struct prototype_term* operation_core = checker_term(
		state, operation->core_term
	);
	while (operation_core && operation_core->tag == PROTOTYPE_TERM_APP) {
		operation_core = checker_term(
			state, operation_core->as.app.function
		);
	}
	const struct prototype_effect_operation_declaration* declaration =
		operation_core && operation_core->tag == PROTOTYPE_TERM_EFFECT_OPERATION ?
			checker_effect_operation_declaration(
				state, operation_core->as.effect_operation.operation_id
			) : NULL;
	const struct prototype_term* classifier = checker_term(
		state, operation->asserted_classifier
	);
	uint32_t effect_binding = PROTOTYPE_INVALID_ID;
	uint32_t classifier_id = operation->asserted_classifier;
	if (classifier && classifier->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		effect_binding = classifier->as.effect_row_forall.binding_id;
		classifier_id = classifier->as.effect_row_forall.body;
	}
	if (!declaration || checker_pi_parts(
			state,
			classifier_id,
			&p_instantiation->domain,
			&p_instantiation->binding,
			&p_instantiation->body
		) != 0 || checker_computation_type_view(
			state, p_instantiation->body, &p_instantiation->result
		) != 0) {
		return -1;
	}
	uint32_t suspended_effect_row = PROTOTYPE_INVALID_ID;
	if (declaration->classifier_schema ==
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT) {
		const struct prototype_term* argument_classifier = checker_term(
			state, argument_classifier_id
		);
		struct checker_computation_type_view suspended;
		if (effect_binding == PROTOTYPE_INVALID_ID || !argument_classifier ||
			argument_classifier->tag != PROTOTYPE_TERM_THUNK_TYPE ||
			checker_computation_type_view(
				state,
				argument_classifier->as.thunk_type.computation,
				&suspended
			) != 0) {
			return -1;
		}
		suspended_effect_row = suspended.effect_row;
	} else if (effect_binding != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (checker_term_equal_after_binding(
			state,
			p_instantiation->domain,
			effect_binding,
			suspended_effect_row,
			argument_classifier_id,
			0
		) != 1) {
		return -1;
	}
	p_instantiation->declaration = declaration;
	p_instantiation->request_effect_row = p_instantiation->result.effect_row;
	if (suspended_effect_row == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	p_instantiation->request_effect_row = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < state->module->terms.term_count; ++i) {
		const struct prototype_term* candidate = checker_term(state, i);
		if (candidate->tag == PROTOTYPE_TERM_EFFECT_ROW_OPERATION &&
			candidate->as.effect_row_operation.operation_id ==
				declaration->operation_id && checker_term_equal_after_binding(
				state,
				candidate->as.effect_row_operation.latent_row,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				suspended_effect_row,
				0
			) == 1) {
			p_instantiation->request_effect_row = i;
			break;
		}
	}
	return p_instantiation->request_effect_row == PROTOTYPE_INVALID_ID ? -1 : 0;
}

static int checker_check_zero_clause_fold(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* fold = checker_term(
		state, occurrence->core_term
	);
	uint32_t computation_id;
	uint32_t continuation_id;
	if (!fold || fold->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		fold->as.computation_fold.clause_count != 0 ||
		occurrence->fold_clause_count != 0 || checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
			&computation_id
		) != 0 || checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&continuation_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* computation =
		&state->module->occurrences.occurrences[computation_id];
	const struct prototype_semantic_occurrence* continuation =
		&state->module->occurrences.occurrences[continuation_id];
	const struct prototype_term* continuation_core = checker_term(
		state, continuation->core_term
	);
	if (fold->as.computation_fold.computation != computation->core_term ||
		fold->as.computation_fold.return_clause != continuation->core_term ||
		!continuation_core || continuation_core->tag != PROTOTYPE_TERM_LAMBDA ||
		continuation->binding_id != continuation_core->as.lambda.binding_id ||
		(occurrence->fold_return_binding_id != PROTOTYPE_INVALID_ID &&
		 occurrence->fold_return_binding_id != continuation->binding_id)) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	struct checker_computation_type_view input;
	struct checker_computation_type_view output;
	struct checker_computation_type_view result;
	uint32_t domain;
	uint32_t binding;
	uint32_t body;
	int input_status = checker_computation_type_view(
		state, computation->asserted_classifier, &input
	);
	int pi_status = checker_pi_parts(
			state, continuation->asserted_classifier, &domain, &binding, &body
		);
	int output_status = pi_status == 0 ?
		checker_computation_type_view(state, body, &output) : -1;
	int result_status = checker_computation_type_view(
		state, occurrence->asserted_classifier, &result
	);
	if (input_status != 0 || pi_status != 0 || output_status != 0 ||
		result_status != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	if (checker_computation_view_term_equal(
			state, &input, input.result, domain
		) != 1 || checker_computation_view_term_equal(
			state, &output, output.result, result.result
		) != 1 || !checker_effect_union_matches(
			state, input.effect_row, output.effect_row, result.effect_row
		) || checker_totality_join(input.totality, output.totality) !=
			result.totality) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	(void)binding;
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_computation_fold(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* fold = checker_term(
		state, occurrence->core_term
	);
	if (!fold || fold->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		fold->as.computation_fold.clause_count != occurrence->fold_clause_count) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	if (occurrence->fold_clause_count == 0) {
		return checker_check_zero_clause_fold(state, occurrence_id);
	}
	uint32_t computation_id;
	uint32_t return_id;
	if (checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
			&computation_id
		) != 0 || checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&return_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* computation =
		&state->module->occurrences.occurrences[computation_id];
	const struct prototype_semantic_occurrence* return_clause =
		&state->module->occurrences.occurrences[return_id];
	if (computation->core_term != fold->as.computation_fold.computation ||
		return_clause->core_term != fold->as.computation_fold.return_clause) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	struct checker_computation_type_view input;
	struct checker_computation_type_view result;
	uint32_t return_domain;
	uint32_t return_binding;
	uint32_t return_body;
	struct checker_computation_type_view carrier;
	if (checker_computation_type_view(
			state, computation->asserted_classifier, &input
		) != 0 || checker_pi_parts(
			state,
			return_clause->asserted_classifier,
			&return_domain,
			&return_binding,
			&return_body
		) != 0 || checker_computation_view_term_equal(
			state, &input, input.result, return_domain
		) != 1 || checker_computation_type_view(
			state, return_body, &carrier
		) != 0 || checker_computation_type_view(
			state, occurrence->asserted_classifier, &result
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	int* handled = calloc(occurrence->fold_clause_count, sizeof(*handled));
	if (!handled) {
		return -1;
	}
	int expected_totality = checker_totality_join(
		input.totality, carrier.totality
	);
	for (uint32_t i = 0; i < occurrence->fold_clause_count; ++i) {
		uint32_t operation_id;
		uint32_t body_id;
		if (checker_occurrence_child(
				state,
				occurrence,
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION,
				i,
				&operation_id
			) != 0 || checker_occurrence_child(
				state,
				occurrence,
				PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
				i,
				&body_id
			) != 0) {
			free(handled);
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
			);
		}
		const struct prototype_semantic_occurrence* operation =
			&state->module->occurrences.occurrences[operation_id];
		const struct prototype_semantic_occurrence* body =
			&state->module->occurrences.occurrences[body_id];
		const struct prototype_term* operation_core = checker_term(
			state, operation->core_term
		);
		const struct prototype_term* body_core = checker_term(
			state, body->core_term
		);
		const struct prototype_term* continuation_core = body_core &&
			body_core->tag == PROTOTYPE_TERM_LAMBDA ? checker_term(
				state, body_core->as.lambda.body
			) : NULL;
		const struct prototype_computation_fold_clause* core_clause =
			&state->module->terms.computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		const struct prototype_semantic_fold_clause* semantic_clause =
			&state->module->occurrences.fold_clauses[
				occurrence->first_fold_clause + i
			];
		if (!operation_core ||
			operation_core->tag != PROTOTYPE_TERM_EFFECT_OPERATION || !body_core ||
			body_core->tag != PROTOTYPE_TERM_LAMBDA || !continuation_core ||
			continuation_core->tag != PROTOTYPE_TERM_LAMBDA ||
			operation->core_term != core_clause->operation ||
			body->core_term != core_clause->body ||
			body_core->as.lambda.binding_id !=
				semantic_clause->argument_binding_id ||
			continuation_core->as.lambda.binding_id !=
				semantic_clause->continuation_binding_id) {
			free(handled);
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
			);
		}
		handled[i] = operation_core->as.effect_operation.operation_id;
		for (uint32_t j = 0; j < i; ++j) {
			if (handled[j] == handled[i]) {
				free(handled);
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
				);
			}
		}
		uint32_t clause_domain;
		uint32_t clause_binding;
		uint32_t clause_body;
		uint32_t continuation_domain;
		uint32_t continuation_binding;
		uint32_t continuation_body;
		if (checker_pi_parts(
				state,
				body->asserted_classifier,
				&clause_domain,
				&clause_binding,
				&clause_body
			) != 0 || checker_pi_parts(
				state,
				clause_body,
				&continuation_domain,
				&continuation_binding,
				&continuation_body
			) != 0) {
			free(handled);
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
			);
		}
		struct checker_effect_operation_instantiation instantiation;
		if (checker_instantiate_effect_operation(
				state, operation, clause_domain, &instantiation
			) != 0) {
			free(handled);
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
			);
		}
		const struct prototype_term* continuation_thunk = checker_term(
			state, continuation_domain
		);
		uint32_t resumed_domain;
		uint32_t resumed_binding;
		uint32_t resumed_body;
		struct checker_computation_type_view resumed_result;
		struct checker_computation_type_view clause_result;
		if (!continuation_thunk ||
			continuation_thunk->tag != PROTOTYPE_TERM_THUNK_TYPE ||
			checker_pi_parts(
				state,
				continuation_thunk->as.thunk_type.computation,
				&resumed_domain,
				&resumed_binding,
				&resumed_body
			) != 0 || checker_computation_view_term_equal(
				state,
				&instantiation.result,
				instantiation.result.result,
				resumed_domain
			) != 1 || checker_computation_type_view(
				state, resumed_body, &resumed_result
			) != 0 || checker_computation_type_view(
				state, continuation_body, &clause_result
			) != 0 || checker_term_equal_after_binding(
				state,
				resumed_result.result,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				carrier.result,
				0
			) != 1 || checker_term_equal_after_binding(
				state,
				clause_result.result,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				carrier.result,
				0
			) != 1 || checker_term_equal_after_binding(
				state,
				resumed_result.effect_row,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				carrier.effect_row,
				0
			) != 1 || checker_term_equal_after_binding(
				state,
				clause_result.effect_row,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				carrier.effect_row,
				0
			) != 1) {
			free(handled);
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
			);
		}
		expected_totality = checker_totality_join(
			expected_totality, checker_totality_join(
				resumed_result.totality, clause_result.totality
			)
		);
		(void)clause_binding;
		(void)continuation_binding;
		(void)resumed_binding;
	}
	int effect_status = checker_effect_rows_handle_match(
		state,
		input.effect_row,
		carrier.effect_row,
		handled,
		occurrence->fold_clause_count,
		result.effect_row
	);
	free(handled);
	int result_equal = checker_term_equal_after_binding(
		state,
		carrier.result,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		result.result,
		0
	);
	if (effect_status < 0 || result_equal < 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	return effect_status == 0 && result_equal == 1 && expected_totality ==
		result.totality ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
}

static int checker_pure_family_parts(
	const struct checker_state* state,
	uint32_t family_id,
	uint32_t* p_binding_id,
	uint32_t* p_body_id
) {
	const struct prototype_term* thunk = checker_term(state, family_id);
	const struct prototype_term* lambda = thunk &&
		thunk->tag == PROTOTYPE_TERM_THUNK ? checker_term(
			state, thunk->as.thunk.computation
		) : NULL;
	const struct prototype_term* returned = lambda &&
		lambda->tag == PROTOTYPE_TERM_LAMBDA ? checker_term(
			state, lambda->as.lambda.body
		) : NULL;
	if (!p_binding_id || !p_body_id || !returned ||
		returned->tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	*p_binding_id = lambda->as.lambda.binding_id;
	*p_body_id = returned->as.return_term.value;
	return 0;
}

static int checker_contract_has_exact_source_dependency(
	const struct checker_state* state,
	uint32_t contract_id,
	uint32_t occurrence_id
) {
	size_t count = 0;
	for (size_t i = 0;
		i < state->module->contracts.dependency_count;
		++i) {
		const struct prototype_semantic_contract_dependency* dependency =
			&state->module->contracts.dependencies[i];
		if (dependency->contract_id != contract_id) {
			continue;
		}
		if (dependency->occurrence != occurrence_id) {
			return 0;
		}
		count++;
	}
	return count == 1;
}

static int checker_check_conditional_fold_occurrence(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	if (occurrence->conditional_contract >=
		state->module->contracts.contract_count) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	uint32_t contract_id = occurrence->conditional_contract;
	const struct prototype_semantic_contract* contract =
		&state->module->contracts.contracts[contract_id];
	const struct prototype_term* fold = checker_term(
		state, occurrence->core_term
	);
	if (occurrence->kind !=
			PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD ||
		occurrence->asserted_classifier != PROTOTYPE_INVALID_ID ||
		contract->kind !=
			PROTOTYPE_SEMANTIC_CONTRACT_COMPUTATION_FOLD_RESULT ||
		contract->occurrence != occurrence_id ||
		contract->core_term != occurrence->core_term || !fold ||
		fold->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		contract->schema_version != 1 ||
		contract->normalization_profile !=
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
		!checker_contract_has_exact_source_dependency(
			state, contract_id, occurrence_id
		) || contract->computation_occurrence >=
			state->module->occurrences.occurrence_count ||
		contract->continuation_occurrence >=
			state->module->occurrences.occurrence_count) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* computation =
		&state->module->occurrences.occurrences[
			contract->computation_occurrence
		];
	const struct prototype_semantic_occurrence* continuation =
		&state->module->occurrences.occurrences[
			contract->continuation_occurrence
		];
	struct checker_computation_type_view input;
	uint32_t family_binding;
	uint32_t family_body;
	if (computation->classifier_evidence_kind !=
			PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT ||
		continuation->classifier_evidence_kind !=
			PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT ||
		computation->core_term != fold->as.computation_fold.computation ||
		checker_computation_type_view(
			state, computation->asserted_classifier, &input
		) != 0 || checker_computation_view_term_equal(
			state, &input, input.result, contract->input_classifier
		) != 1 || checker_computation_view_term_equal(
			state, &input, input.effect_row, contract->effect_row
		) != 1 || checker_pure_family_parts(
			state, contract->classifier_family, &family_binding, &family_body
		) != 0 || family_binding != contract->continuation_binding_id) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	uint32_t return_occurrence_id;
	if (checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&return_occurrence_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* return_occurrence =
		&state->module->occurrences.occurrences[return_occurrence_id];
	if (fold->as.computation_fold.return_clause !=
		return_occurrence->core_term) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	if (fold->as.computation_fold.clause_count == 0) {
		uint32_t domain;
		uint32_t binding;
		uint32_t family;
		if (return_occurrence_id != contract->continuation_occurrence ||
			checker_pi_parts(
				state, continuation->asserted_classifier,
				&domain, &binding, &family
			) != 0 || checker_computation_view_term_equal(
				state, &input, input.result, domain
			) != 1 || binding != contract->continuation_binding_id ||
			checker_term_equal_after_binding(
				state, family, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, contract->classifier_family, 0
			) != 1) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
			);
		}
	} else {
		uint32_t body_occurrence_id;
		if (checker_occurrence_child(
				state, return_occurrence,
				PROTOTYPE_TERM_CHILD_BODY, 0, &body_occurrence_id
			) != 0 || body_occurrence_id !=
				contract->continuation_occurrence ||
			checker_term_equal_after_binding(
				state, family_body, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, continuation->asserted_classifier, 0
			) != 1) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
			);
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_operation_request(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* request = checker_term(
		state, occurrence->core_term
	);
	uint32_t operation_id;
	uint32_t argument_id;
	uint32_t continuation_id;
	if (!request || request->tag != PROTOTYPE_TERM_OPERATION_REQUEST ||
		checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
			&operation_id
		) != 0 || checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
			&argument_id
		) != 0 || checker_occurrence_child(
			state, occurrence, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
			&continuation_id
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	const struct prototype_semantic_occurrence* operation =
		&state->module->occurrences.occurrences[operation_id];
	const struct prototype_semantic_occurrence* argument =
		&state->module->occurrences.occurrences[argument_id];
	const struct prototype_semantic_occurrence* continuation =
		&state->module->occurrences.occurrences[continuation_id];
	if (operation->core_term != request->as.operation_request.operation ||
		argument->core_term != request->as.operation_request.argument ||
		continuation->core_term != request->as.operation_request.continuation) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
		);
	}
	struct checker_effect_operation_instantiation instantiation;
	if (checker_instantiate_effect_operation(
			state,
			operation,
			argument->asserted_classifier,
			&instantiation
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	const struct prototype_term* continuation_type = checker_term(
		state, continuation->asserted_classifier
	);
	uint32_t continuation_domain;
	uint32_t continuation_binding;
	uint32_t continuation_body;
	if (!continuation_type ||
		continuation_type->tag != PROTOTYPE_TERM_THUNK_TYPE ||
		checker_pi_parts(
			state,
			continuation_type->as.thunk_type.computation,
			&continuation_domain,
			&continuation_binding,
			&continuation_body
		) != 0 || checker_computation_view_term_equal(
			state,
			&instantiation.result,
			instantiation.result.result,
			continuation_domain
		) != 1) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	struct checker_computation_type_view continuation_result;
	struct checker_computation_type_view result;
	if (checker_computation_type_view(
			state, continuation_body, &continuation_result
		) != 0 || checker_computation_type_view(
			state, occurrence->asserted_classifier, &result
		) != 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	int result_equal = checker_computation_view_term_equal(
		state,
		&continuation_result,
		continuation_result.result,
		result.result
	);
	if (result_equal < 0) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_PAUSED,
			PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
		);
	}
	if (!result_equal || !checker_effect_union_matches(
			state,
			instantiation.request_effect_row,
			continuation_result.effect_row,
			result.effect_row
		) || checker_totality_join(
		instantiation.result.totality, continuation_result.totality
		) != result.totality) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	(void)continuation_binding;
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_occurrence(
	struct checker_state* state,
	uint32_t occurrence_id
) {
	int status = checker_charge(state, occurrence_id);
	if (status != PROTOTYPE_CHECKER_COMPLETE) {
		return status;
	}
	const struct prototype_semantic_occurrence* occurrence =
		&state->module->occurrences.occurrences[occurrence_id];
	const struct prototype_term* core = checker_term(state, occurrence->core_term);
	if (occurrence->classifier_evidence_kind ==
		PROTOTYPE_SEMANTIC_CLASSIFIER_CONDITIONAL) {
		return checker_check_conditional_fold_occurrence(
			state, occurrence_id
		);
	}
	if (occurrence->classifier_evidence_kind !=
		PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT) {
		return checker_stop(
			state, PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
		);
	}
	switch (occurrence->kind) {
		case PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM:
			return checker_check_atom(state, occurrence_id);
		case PROTOTYPE_SEMANTIC_OCCURRENCE_VAR: {
			uint32_t binding_classifier = PROTOTYPE_INVALID_ID;
			if (occurrence->binding_id == PROTOTYPE_INVALID_ID) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
			}
			int core_equal;
			int classifier_equal;
			if (occurrence->context_action_substitution == PROTOTYPE_INVALID_ID) {
				if (!core || core->tag != PROTOTYPE_TERM_VAR ||
					core->as.var.binding_id != occurrence->binding_id ||
					checker_context_binding(
						state,
						occurrence->context_id,
						occurrence->binding_id,
						&binding_classifier
					) != 0) {
					return checker_stop(
						state,
						PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_CLASSIFIER,
						occurrence_id
					);
				}
				core_equal = 1;
				classifier_equal = checker_term_equal_after_binding(
					state,
					binding_classifier,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					occurrence->asserted_classifier,
					0
				);
			} else {
				const struct prototype_term* origin = checker_term(
					state, occurrence->origin_core_term
				);
				if (occurrence->context_action_substitution >=
						state->module->substitutions.substitution_count ||
					!origin || origin->tag != PROTOTYPE_TERM_VAR ||
					origin->as.var.binding_id != occurrence->binding_id) {
					return checker_stop(
						state,
						PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_OCCURRENCE,
						occurrence_id
					);
				}
				const struct prototype_semantic_substitution* action =
					&state->module->substitutions.substitutions[
						occurrence->context_action_substitution
					];
				if (action->source_context != occurrence->context_id ||
					checker_context_binding(
						state,
						action->target_context,
						occurrence->binding_id,
						&binding_classifier
					) != 0) {
					return checker_stop(
						state,
						PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_CONTEXT,
						occurrence_id
					);
				}
				core_equal = checker_term_equal_reindexed(
					state,
					occurrence->origin_core_term,
					occurrence->context_action_substitution,
					occurrence->core_term
				);
				classifier_equal = checker_term_equal_reindexed(
					state,
					binding_classifier,
					occurrence->context_action_substitution,
					occurrence->asserted_classifier
				);
			}
			int comparison = core_equal < 0 || classifier_equal < 0 ? -1 :
				core_equal == 1 && classifier_equal == 1 ? 1 : 0;
			return comparison == 1 ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
				state,
				comparison < 0 ? PROTOTYPE_CHECKER_PAUSED :
					PROTOTYPE_CHECKER_REJECTED,
				comparison < 0 ? PROTOTYPE_CHECKER_STOP_UNSUPPORTED :
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				occurrence_id
			);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_REFERENCE: {
			if (occurrence->wrapped_occurrence >=
				state->module->occurrences.occurrence_count) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* wrapped =
				&state->module->occurrences.occurrences[
					occurrence->wrapped_occurrence
				];
			return wrapped->core_term == occurrence->core_term &&
				wrapped->asserted_classifier == occurrence->asserted_classifier ?
				PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE: {
			if (occurrence->wrapped_occurrence >=
				state->module->occurrences.occurrence_count) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* wrapped =
				&state->module->occurrences.occurrences[
					occurrence->wrapped_occurrence
				];
			int equal = checker_term_equal_after_binding(
				state,
				wrapped->asserted_classifier,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				occurrence->asserted_classifier,
				0
			);
			if (equal < 0) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_PAUSED,
					PROTOTYPE_CHECKER_STOP_UNSUPPORTED, occurrence_id
				);
			}
			return wrapped->core_term == occurrence->core_term && equal ?
				PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
				);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_CONSTRUCTOR: {
			if (!core || core->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			const struct prototype_semantic_type_constructor* constructor =
				checker_find_constructor(
					state,
					core->as.constructor.owner,
					core->as.constructor.constructor_id,
					occurrence->asserted_classifier
				);
			if (!constructor) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_TYPE_SCHEMA,
					occurrence_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_APP: {
			uint32_t function_id;
			uint32_t argument_id;
			if (!core || checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_FUNCTION,
					0,
					&function_id
				) != 0 || checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_ARGUMENT,
					0,
					&argument_id
				) != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* function =
				&state->module->occurrences.occurrences[function_id];
			const struct prototype_semantic_occurrence* argument =
				&state->module->occurrences.occurrences[argument_id];
			const struct prototype_term* function_core = checker_term(
				state, function->core_term
			);
			if (occurrence->application_role ==
					PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) {
				int application_matches = checker_type_view_application_matches(
					state, core, function_core, argument
				) || (core->tag == PROTOTYPE_TERM_APP &&
					function->core_term == core->as.app.function &&
					argument->core_term == core->as.app.argument);
				if (!application_matches) {
					return checker_stop(
						state,
						PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_OCCURRENCE,
						occurrence_id
					);
				}
			} else if (core->tag != PROTOTYPE_TERM_APP ||
				function->core_term != core->as.app.function ||
				argument->core_term != core->as.app.argument) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			uint32_t domain;
			uint32_t binding;
			uint32_t body;
			if (checker_pi_parts(
					state,
					function->asserted_classifier,
					&domain,
					&binding,
					&body
				) != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_PAUSED,
					PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
					occurrence_id
				);
			}
			int domain_equal = checker_term_equal_after_bindings(
				state,
				domain,
				NULL,
				NULL,
				0,
				argument->asserted_classifier,
				0
			);
			if (domain_equal != 1 && !checker_universes_equal(
					state, domain, argument->asserted_classifier
				)) {
				return checker_stop(
					state,
					domain_equal < 0 ? PROTOTYPE_CHECKER_PAUSED :
						PROTOTYPE_CHECKER_REJECTED,
					domain_equal < 0 ? PROTOTYPE_CHECKER_STOP_UNSUPPORTED :
						PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
			}
			int equal = checker_term_equal_after_binding(
				state,
				body,
				binding,
				argument->core_term,
				occurrence->asserted_classifier,
				0
			);
			if (equal < 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_PAUSED,
					PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
					occurrence_id
				);
			}
			return equal ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
				state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER,
				occurrence_id
			);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_RETURN: {
			uint32_t child_id;
			if (!core || core->tag != PROTOTYPE_TERM_RETURN ||
				checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_RETURN_VALUE,
					0,
					&child_id
				) != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* child =
				&state->module->occurrences.occurrences[child_id];
			const struct prototype_term* classifier = checker_term(
				state, occurrence->asserted_classifier
			);
			const struct prototype_term* effect = classifier &&
				classifier->tag == PROTOTYPE_TERM_COMPUTATION_TYPE ?
					checker_term(state, classifier->as.computation_type.label) : NULL;
			int result_equal = classifier &&
				classifier->tag == PROTOTYPE_TERM_COMPUTATION_TYPE ?
				checker_term_equal_after_binding(
					state,
					classifier->as.computation_type.result,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					child->asserted_classifier,
					0
				) : 0;
			if (child->core_term != core->as.return_term.value || !classifier ||
				classifier->tag != PROTOTYPE_TERM_COMPUTATION_TYPE || !effect ||
				effect->tag != PROTOTYPE_TERM_EFFECT_ROW_EMPTY ||
				result_equal != 1 ||
				classifier->as.computation_type.totality !=
					PROTOTYPE_COMPUTATION_TOTALITY_TOTAL) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK: {
			uint32_t child_id;
			const struct prototype_term* classifier = checker_term(
				state, occurrence->asserted_classifier
			);
			if (!core || core->tag != PROTOTYPE_TERM_THUNK || !classifier ||
				classifier->tag != PROTOTYPE_TERM_THUNK_TYPE ||
				checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION,
					0,
					&child_id
				) != 0) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* child =
				&state->module->occurrences.occurrences[child_id];
			int equal = checker_term_equal_after_binding(
				state,
				child->asserted_classifier,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				classifier->as.thunk_type.computation,
				0
			);
			return child->core_term == core->as.thunk.computation && equal == 1 ?
				PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state,
					equal < 0 ? PROTOTYPE_CHECKER_PAUSED :
						PROTOTYPE_CHECKER_REJECTED,
					equal < 0 ? PROTOTYPE_CHECKER_STOP_UNSUPPORTED :
						PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_FORCE: {
			uint32_t child_id;
			if (!core || core->tag != PROTOTYPE_TERM_FORCE ||
				checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_FORCE_VALUE,
					0,
					&child_id
				) != 0) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE, occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* child =
				&state->module->occurrences.occurrences[child_id];
			const struct prototype_term* child_classifier = checker_term(
				state, child->asserted_classifier
			);
			int equal = child_classifier &&
				child_classifier->tag == PROTOTYPE_TERM_THUNK_TYPE ?
				checker_term_equal_after_binding(
					state,
					child_classifier->as.thunk_type.computation,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					occurrence->asserted_classifier,
					0
				) : 0;
			return child->core_term == core->as.force.value && equal == 1 ?
				PROTOTYPE_CHECKER_COMPLETE : checker_stop(
					state,
					equal < 0 ? PROTOTYPE_CHECKER_PAUSED :
						PROTOTYPE_CHECKER_REJECTED,
					equal < 0 ? PROTOTYPE_CHECKER_STOP_UNSUPPORTED :
						PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_LAMBDA: {
			uint32_t child_id;
			if (!core || core->tag != PROTOTYPE_TERM_LAMBDA ||
				occurrence->binding_id == PROTOTYPE_INVALID_ID ||
				occurrence->binder_classifier == PROTOTYPE_INVALID_ID ||
				checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_BODY,
					0,
					&child_id
				) != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_OCCURRENCE,
					occurrence_id
				);
			}
			const struct prototype_semantic_occurrence* child =
				&state->module->occurrences.occurrences[child_id];
			uint32_t pi_id = occurrence->asserted_classifier;
			for (uint32_t i = 0;
				i < occurrence->implicit_effect_row_count;
				++i) {
				const struct prototype_term* quantified = checker_term(
					state, pi_id
				);
				if (!quantified || quantified->tag !=
						PROTOTYPE_TERM_EFFECT_ROW_FORALL ||
					quantified->as.effect_row_forall.binding_id !=
						occurrence->implicit_effect_row_binders[i]) {
					return checker_stop(
						state, PROTOTYPE_CHECKER_REJECTED,
						PROTOTYPE_CHECKER_STOP_CLASSIFIER, occurrence_id
					);
				}
				pi_id = quantified->as.effect_row_forall.body;
			}
			uint32_t pi_domain;
			uint32_t pi_binding;
			uint32_t pi_body;
			if (checker_pi_parts(
					state, pi_id, &pi_domain, &pi_binding, &pi_body
				) != 0) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
			}
			uint32_t occurrence_variable = checker_find_variable_term(
				state, occurrence->binding_id
			);
			int body_equal = core->as.lambda.binding_id ==
				occurrence->binding_id ? checker_term_equal_after_binding(
					state,
					core->as.lambda.body,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					child->core_term,
					0
				) : occurrence_variable == PROTOTYPE_INVALID_ID ? 0 :
				checker_term_equal_after_binding(
					state,
					core->as.lambda.body,
					core->as.lambda.binding_id,
					occurrence_variable,
					child->core_term,
					0
				);
			int classifier_body_equal = pi_binding == occurrence->binding_id ||
				!checker_term_contains_binding(
					state, pi_body, pi_binding, 0
				) ?
				checker_term_equal_after_binding(
					state,
					pi_body,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					child->asserted_classifier,
					0
				) : occurrence_variable == PROTOTYPE_INVALID_ID ? 0 :
				checker_term_equal_after_binding(
					state,
					pi_body,
					pi_binding,
					occurrence_variable,
					child->asserted_classifier,
					0
				);
			if (body_equal != 1 || classifier_body_equal != 1 ||
				checker_term_equal_after_binding(
					state,
					pi_domain,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					occurrence->binder_classifier,
					0
				) != 1) {
				return checker_stop(
					state,
					PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER,
					occurrence_id
				);
			}
			return PROTOTYPE_CHECKER_COMPLETE;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH:
			return checker_check_match(state, occurrence_id);
		case PROTOTYPE_SEMANTIC_OCCURRENCE_INDUCTION_HYPOTHESIS:
			return checker_check_induction_hypothesis(state, occurrence_id);
		case PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST:
			return checker_check_operation_request(state, occurrence_id);
		case PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD:
			return checker_check_computation_fold(state, occurrence_id);
		default:
			return checker_stop(
				state,
				PROTOTYPE_CHECKER_PAUSED,
				PROTOTYPE_CHECKER_STOP_UNSUPPORTED,
				occurrence_id
			);
	}
}

static int checker_check_occurrences(struct checker_state* state) {
	for (uint32_t i = 0; i < state->module->occurrences.occurrence_count; ++i) {
		int status = checker_check_occurrence(state, i);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_check_contracts(struct checker_state* state) {
	for (uint32_t contract_id = 0;
		contract_id < state->module->contracts.contract_count;
		++contract_id) {
		int status = checker_charge(state, contract_id);
		if (status != PROTOTYPE_CHECKER_COMPLETE) {
			return status;
		}
		const struct prototype_semantic_contract* contract =
			&state->module->contracts.contracts[contract_id];
		if (contract->occurrence >=
			state->module->occurrences.occurrence_count) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE, contract_id
			);
		}
		const struct prototype_semantic_occurrence* occurrence =
			&state->module->occurrences.occurrences[contract->occurrence];
		if (occurrence->core_term != contract->core_term ||
			!checker_contract_has_exact_source_dependency(
				state, contract_id, contract->occurrence
			)) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE, contract_id
			);
		}
		if (contract->kind ==
			PROTOTYPE_SEMANTIC_CONTRACT_COMPUTATION_FOLD_RESULT) {
			if (occurrence->classifier_evidence_kind !=
					PROTOTYPE_SEMANTIC_CLASSIFIER_CONDITIONAL ||
				occurrence->conditional_contract != contract_id) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_CLASSIFIER, contract_id
				);
			}
			continue;
		}
		if (contract->kind !=
				PROTOTYPE_SEMANTIC_CONTRACT_EFFECT_ROW_EQUATION ||
			contract->schema_version != 2 ||
			contract->normalization_profile !=
				PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
			contract->computation_occurrence != PROTOTYPE_INVALID_ID ||
			contract->continuation_occurrence != PROTOTYPE_INVALID_ID ||
			contract->continuation_binding_id != PROTOTYPE_INVALID_ID ||
			contract->effect_constraint_kind <
				PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_EXACT ||
			contract->effect_constraint_kind >
				PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_RESIDUAL) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, contract_id
			);
		}
		int needs_right = contract->effect_constraint_kind ==
				PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_UNION ||
			contract->effect_constraint_kind ==
				PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_RESIDUAL;
		if (needs_right !=
				(contract->classifier_family != PROTOTYPE_INVALID_ID) ||
			!checker_effect_row_is_well_formed(
				state, contract->input_classifier
			) || !checker_effect_row_is_well_formed(
				state, contract->effect_row
			) || (needs_right && !checker_effect_row_is_well_formed(
				state, contract->classifier_family
			))) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_CLASSIFIER, contract_id
			);
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

static int checker_runtime_capabilities(
	const struct checker_state* state,
	uint64_t* p_capabilities
) {
	uint64_t capabilities = 0;
	for (uint32_t i = 0;
		i < state->module->occurrences.occurrence_count;
		++i) {
		const struct prototype_semantic_occurrence* occurrence =
			&state->module->occurrences.occurrences[i];
		if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST) {
			uint32_t operation_occurrence;
			if (checker_occurrence_child(
					state,
					occurrence,
					PROTOTYPE_TERM_CHILD_REQUEST_OPERATION,
					0,
					&operation_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_term* operation = checker_term(
				state,
				state->module->occurrences.occurrences[
					operation_occurrence
				].core_term
			);
			while (operation && operation->tag == PROTOTYPE_TERM_APP) {
				operation = checker_term(state, operation->as.app.function);
			}
			if (!operation || operation->tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				checker_effect_operation_declaration(
					state, operation->as.effect_operation.operation_id
				);
			if (!declaration) {
				return -1;
			}
			capabilities |=
				PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_OPERATION_DISPATCH;
			if ((declaration->required_host_effects &
				PROTOTYPE_HOST_EFFECT_TERMINAL) != 0) {
				capabilities |=
					PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_TERMINAL;
			}
		}
		if (occurrence->kind ==
				PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD &&
			occurrence->fold_clause_count != 0) {
			capabilities |= PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_HANDLER;
		}
	}
	for (size_t i = 0; i < state->module->contracts.contract_count; ++i) {
		if (state->module->contracts.contracts[i].kind ==
			PROTOTYPE_SEMANTIC_CONTRACT_COMPUTATION_FOLD_RESULT) {
			capabilities |=
				PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER;
		}
	}
	*p_capabilities = capabilities;
	return 0;
}

struct checker_cross_binding {
	uint32_t left;
	uint32_t right;
};

static const char* checker_symbol_string(
	const struct prototype_elaborated_module_view* module,
	int symbol_id
) {
	if (!module || symbol_id < 0 ||
		(size_t)symbol_id >= module->symbols.count) {
		return NULL;
	}
	return module->symbols.strings[symbol_id];
}

static int checker_cross_symbol_equal(
	const struct prototype_elaborated_module_view* left,
	int left_symbol,
	const struct prototype_elaborated_module_view* right,
	int right_symbol
) {
	if (left_symbol < 0 || right_symbol < 0) {
		return left_symbol == right_symbol;
	}
	const char* left_string = checker_symbol_string(left, left_symbol);
	const char* right_string = checker_symbol_string(right, right_symbol);
	return left_string && right_string && strcmp(left_string, right_string) == 0;
}

static int checker_cross_name_equal(
	const struct prototype_elaborated_module_view* left,
	struct prototype_qualified_name left_name,
	const struct prototype_elaborated_module_view* right,
	struct prototype_qualified_name right_name
) {
	return checker_cross_symbol_equal(
		left, left_name.namespace_symbol_id,
		right, right_name.namespace_symbol_id
	) && checker_cross_symbol_equal(
		left, left_name.name_symbol_id, right, right_name.name_symbol_id
	);
}

static int checker_cross_bound_equal(
	const struct checker_cross_binding* bindings,
	size_t binding_count,
	uint32_t left,
	uint32_t right
) {
	for (size_t i = binding_count; i > 0; --i) {
		if (bindings[i - 1].left == left || bindings[i - 1].right == right) {
			return bindings[i - 1].left == left &&
				bindings[i - 1].right == right;
		}
	}
	return 0;
}

static int checker_cross_universe_value(
	const struct prototype_elaborated_module_view* module,
	const struct prototype_checked_module* checked,
	uint32_t level_var,
	int* p_value
) {
	if (!module || !p_value ||
		(checked && checked->seal != PROTOTYPE_CHECKED_MODULE_SEAL)) {
		return -1;
	}
	if (checked) {
		for (size_t i = 0; i < checked->universe_solution_count; ++i) {
			if (checked->universe_solutions[i].level_var == level_var) {
				*p_value = checked->universe_solutions[i].value;
				return 0;
			}
		}
		return -1;
	}
	for (size_t i = 0; i < module->universes.level_count; ++i) {
		if (module->universes.levels[i].level_var == level_var) {
			*p_value = module->universes.levels[i].value;
			return 0;
		}
	}
	return -1;
}

static int checker_cross_term_equal_at_depth(
	const struct prototype_elaborated_module_view* left_module,
	const struct prototype_checked_module* left_checked,
	uint32_t left_id,
	const struct prototype_elaborated_module_view* right_module,
	const struct prototype_checked_module* right_checked,
	uint32_t right_id,
	struct checker_cross_binding* bindings,
	size_t binding_count,
	size_t binding_capacity,
	uint32_t depth
) {
	if (!left_module || !right_module ||
		left_id >= left_module->terms.term_count ||
		right_id >= right_module->terms.term_count ||
		depth > left_module->terms.term_count + right_module->terms.term_count) {
		return 0;
	}
	const struct prototype_term* left = &left_module->terms.terms[left_id];
	const struct prototype_term* right = &right_module->terms.terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
#define CROSS_TERM(left_child, right_child) \
	checker_cross_term_equal_at_depth( \
		left_module, left_checked, (left_child), \
		right_module, right_checked, (right_child), bindings, \
		binding_count, binding_capacity, depth + 1 \
	)
#define PUSH_CROSS_BINDING(left_binding, right_binding) \
	do { \
		if (binding_count >= binding_capacity) { \
			return 0; \
		} \
		bindings[binding_count++] = (struct checker_cross_binding) { \
			.left = (left_binding), \
			.right = (right_binding) \
		}; \
	} while (0)
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return checker_cross_bound_equal(
				bindings, binding_count,
				left->as.var.binding_id, right->as.var.binding_id
			);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id ==
				right->as.constructor.constructor_id && CROSS_TERM(
					left->as.constructor.owner, right->as.constructor.owner
				);
		case PROTOTYPE_TERM_APP:
			return CROSS_TERM(
				left->as.app.function, right->as.app.function
			) && CROSS_TERM(left->as.app.argument, right->as.app.argument);
		case PROTOTYPE_TERM_LAMBDA:
			PUSH_CROSS_BINDING(
				left->as.lambda.binding_id, right->as.lambda.binding_id
			);
			return CROSS_TERM(left->as.lambda.body, right->as.lambda.body);
		case PROTOTYPE_TERM_PI:
			return CROSS_TERM(left->as.pi.domain, right->as.pi.domain) &&
				CROSS_TERM(
					left->as.pi.codomain_family,
					right->as.pi.codomain_family
				);
		case PROTOTYPE_TERM_MATCH:
			if (left->as.match.case_count != right->as.match.case_count ||
				!CROSS_TERM(
					left->as.match.scrutinee, right->as.match.scrutinee
				)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&left_module->terms.cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&right_module->terms.cases[right->as.match.first_case + i];
				if (left_case->constructor_id != right_case->constructor_id ||
					left_case->binder_count != right_case->binder_count) {
					return 0;
				}
				size_t case_binding_count = binding_count;
				for (uint32_t j = 0; j < left_case->binder_count; ++j) {
					const struct prototype_case_binder* left_binder =
						&left_module->terms.case_binders[
							left_case->first_binder + j
						];
					const struct prototype_case_binder* right_binder =
						&right_module->terms.case_binders[
							right_case->first_binder + j
						];
					if (left_binder->is_recursive != right_binder->is_recursive ||
						case_binding_count >= binding_capacity) {
						return 0;
					}
					bindings[case_binding_count++] =
						(struct checker_cross_binding) {
							.left = left_binder->binding_id,
							.right = right_binder->binding_id
						};
				}
				if (!checker_cross_term_equal_at_depth(
						left_module, left_checked, left_case->body,
						right_module, right_checked, right_case->body,
						bindings, case_binding_count, binding_capacity,
						depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_FORMER: {
			if (left->as.type_former.constructor_count !=
				right->as.type_former.constructor_count ||
				left->as.type_former.declaration_type_id >=
					left_module->type_schema.type_count ||
				right->as.type_former.declaration_type_id >=
					right_module->type_schema.type_count) {
				return 0;
			}
			const struct prototype_semantic_type_declaration* left_type =
				&left_module->type_schema.type_declarations[
					left->as.type_former.declaration_type_id
				];
			const struct prototype_semantic_type_declaration* right_type =
				&right_module->type_schema.type_declarations[
					right->as.type_former.declaration_type_id
				];
			return checker_cross_symbol_equal(
				left_module, left_type->namespace_symbol_id,
				right_module, right_type->namespace_symbol_id
			) && checker_cross_symbol_equal(
				left_module, left_type->name_symbol_id,
				right_module, right_type->name_symbol_id
			);
		}
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return checker_cross_name_equal(
				left_module, left->as.type_declaration.identity,
				right_module, right->as.type_declaration.identity
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return checker_cross_name_equal(
				left_module, left->as.type_view.identity,
				right_module, right->as.type_view.identity
			) && CROSS_TERM(
				left->as.type_view.source, right->as.type_view.source
			);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return 0;
		case PROTOTYPE_TERM_UNIVERSE_VAR: {
			int left_value;
			int right_value;
			return checker_cross_universe_value(
				left_module, left_checked,
				left->as.universe_var.level_var, &left_value
			) == 0 && checker_cross_universe_value(
				right_module, right_checked,
				right->as.universe_var.level_var, &right_value
			) == 0 && left_value == right_value;
		}
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 1;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			return checker_cross_symbol_equal(
				left_module, left->as.text_literal.text_symbol_id,
				right_module, right->as.text_literal.text_symbol_id
			);
		case PROTOTYPE_TERM_INT_LITERAL:
			return left->as.int_literal.value == right->as.int_literal.value;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return checker_cross_name_equal(
				left_module, left->as.external_ref.name,
				right_module, right->as.external_ref.name
			);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			return left->as.pure_primitive.primitive_id ==
				right->as.pure_primitive.primitive_id &&
				checker_cross_symbol_equal(
					left_module, left->as.pure_primitive.type_symbol_id,
					right_module, right->as.pure_primitive.type_symbol_id
				);
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return left->as.effect_operation.operation_id ==
				right->as.effect_operation.operation_id && CROSS_TERM(
					left->as.effect_operation.classifier,
					right->as.effect_operation.classifier
				);
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return checker_cross_bound_equal(
				bindings, binding_count,
				left->as.effect_row_var.binding_id,
				right->as.effect_row_var.binding_id
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return CROSS_TERM(
				left->as.effect_row_union.left,
				right->as.effect_row_union.left
			) && CROSS_TERM(
				left->as.effect_row_union.right,
				right->as.effect_row_union.right
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			PUSH_CROSS_BINDING(
				left->as.effect_row_forall.binding_id,
				right->as.effect_row_forall.binding_id
			);
			return CROSS_TERM(
				left->as.effect_row_forall.body,
				right->as.effect_row_forall.body
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return left->as.effect_row_operation.operation_id ==
				right->as.effect_row_operation.operation_id && CROSS_TERM(
					left->as.effect_row_operation.latent_row,
					right->as.effect_row_operation.latent_row
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return left->as.computation_type.totality ==
				right->as.computation_type.totality && CROSS_TERM(
					left->as.computation_type.label,
					right->as.computation_type.label
				) && CROSS_TERM(
					left->as.computation_type.result,
					right->as.computation_type.result
				);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return CROSS_TERM(
				left->as.thunk_type.computation,
				right->as.thunk_type.computation
			);
		case PROTOTYPE_TERM_RETURN:
			return CROSS_TERM(
				left->as.return_term.value, right->as.return_term.value
			);
		case PROTOTYPE_TERM_THUNK:
			return CROSS_TERM(
				left->as.thunk.computation, right->as.thunk.computation
			);
		case PROTOTYPE_TERM_FORCE:
			return CROSS_TERM(left->as.force.value, right->as.force.value);
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return CROSS_TERM(
				left->as.operation_request.operation,
				right->as.operation_request.operation
			) && CROSS_TERM(
				left->as.operation_request.argument,
				right->as.operation_request.argument
			) && CROSS_TERM(
				left->as.operation_request.continuation,
				right->as.operation_request.continuation
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (left->as.computation_fold.clause_count !=
				right->as.computation_fold.clause_count || !CROSS_TERM(
					left->as.computation_fold.computation,
					right->as.computation_fold.computation
				) || !CROSS_TERM(
					left->as.computation_fold.return_clause,
					right->as.computation_fold.return_clause
				)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&left_module->terms.computation_fold_clauses[
						left->as.computation_fold.first_clause + i
					];
				const struct prototype_computation_fold_clause* right_clause =
					&right_module->terms.computation_fold_clauses[
						right->as.computation_fold.first_clause + i
					];
				if (!CROSS_TERM(left_clause->operation, right_clause->operation) ||
					!CROSS_TERM(left_clause->body, right_clause->body)) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return left->as.dimension_action.operator_id ==
				right->as.dimension_action.operator_id && CROSS_TERM(
					left->as.dimension_action.source,
					right->as.dimension_action.source
				);
		default:
			return 0;
	}
#undef PUSH_CROSS_BINDING
#undef CROSS_TERM
}

static int checker_cross_classifier_equal(
	const struct checker_state* state,
	uint32_t local_classifier,
	const struct prototype_checked_module* imported,
	uint32_t imported_classifier
) {
	size_t capacity = state->module->terms.term_count +
		imported->elaborated->terms.term_count;
	struct checker_cross_binding* bindings = capacity == 0 ? NULL : calloc(
		capacity, sizeof(*bindings)
	);
	if (capacity != 0 && !bindings) {
		return -1;
	}
	int equal = checker_cross_term_equal_at_depth(
		state->module, NULL, local_classifier,
		imported->elaborated, imported, imported_classifier,
		bindings, 0, capacity, 0
	);
	free(bindings);
	return equal;
}

static int checker_external_ref_classifier(
	const struct checker_state* state,
	uint32_t term_id,
	uint32_t* p_classifier
) {
	int found = 0;
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (size_t i = 0; i < state->module->occurrences.occurrence_count; ++i) {
		const struct prototype_semantic_occurrence* occurrence =
			&state->module->occurrences.occurrences[i];
		if (occurrence->core_term != term_id ||
			occurrence->classifier_evidence_kind !=
				PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT) {
			continue;
		}
		if (found && classifier != occurrence->asserted_classifier) {
			return -1;
		}
		classifier = occurrence->asserted_classifier;
		found = 1;
	}
	if (!found || !p_classifier) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

static int checker_resolve_imported_term(
	const struct checker_state* state,
	struct prototype_qualified_name local_name,
	const struct prototype_checked_module** p_owner,
	const struct prototype_semantic_term_export** p_export
) {
	const struct prototype_checked_module* owner = NULL;
	const struct prototype_semantic_term_export* resolved = NULL;
	for (size_t i = 0; i < state->imported_base_count; ++i) {
		const struct prototype_checked_module* base = state->imported_bases[i];
		if (!base || base->seal != PROTOTYPE_CHECKED_MODULE_SEAL) {
			return -1;
		}
		for (size_t j = 0;
			j < base->elaborated->interface.term_export_count;
			++j) {
			const struct prototype_semantic_term_export* candidate =
				&base->elaborated->interface.term_exports[j];
			if (!checker_cross_symbol_equal(
					state->module, local_name.namespace_symbol_id,
					base->elaborated, candidate->namespace_symbol_id
				) || !checker_cross_symbol_equal(
					state->module, local_name.name_symbol_id,
					base->elaborated, candidate->name_symbol_id
				)) {
				continue;
			}
			if (resolved) {
				return -1;
			}
			owner = base;
			resolved = candidate;
		}
	}
	if (!resolved || !p_owner || !p_export) {
		return -1;
	}
	*p_owner = owner;
	*p_export = resolved;
	return 0;
}

static int checker_check_interface(struct checker_state* state) {
	const struct prototype_semantic_interface_view* interface =
		&state->module->interface;
	for (uint32_t i = 0; i < interface->term_export_count; ++i) {
		const struct prototype_semantic_term_export* export =
			&interface->term_exports[i];
		const struct prototype_semantic_occurrence* occurrence =
			&state->module->occurrences.occurrences[export->occurrence];
		if (occurrence->classifier_evidence_kind !=
				PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT ||
			occurrence->core_term != export->term ||
			occurrence->asserted_classifier != export->classifier) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (interface->term_exports[j].namespace_symbol_id ==
					export->namespace_symbol_id &&
				interface->term_exports[j].name_symbol_id ==
					export->name_symbol_id) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_INTERFACE, i
				);
			}
		}
	}
	for (uint32_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_semantic_type_export* export =
			&interface->type_exports[i];
		const struct prototype_semantic_type_declaration* declaration =
			&state->module->type_schema.type_declarations[
				export->type_declaration
			];
		if (export->namespace_symbol_id != declaration->namespace_symbol_id ||
			export->name_symbol_id != declaration->name_symbol_id) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
	}
	for (uint32_t i = 0; i < interface->constructor_export_count; ++i) {
		const struct prototype_semantic_constructor_export* export =
			&interface->constructor_exports[i];
		const struct prototype_semantic_type_export* owner =
			&interface->type_exports[export->type_export];
		const struct prototype_semantic_type_declaration* declaration =
			&state->module->type_schema.type_declarations[
				owner->type_declaration
			];
		const struct prototype_semantic_type_constructor* constructor =
			&state->module->type_schema.constructor_declarations[
				export->constructor_declaration
			];
		if (export->ordinal >= declaration->constructor_count ||
			export->constructor_declaration !=
				declaration->first_constructor + export->ordinal ||
			constructor->owner_type != declaration->type_index ||
			constructor->constructor_index != export->ordinal ||
			constructor->name_symbol_id != export->name_symbol_id) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
	}
	for (uint32_t i = 0;
		i < interface->function_graph_association_count;
		++i) {
		const struct prototype_semantic_function_graph_association* association =
			&interface->function_graph_associations[i];
		if (association->owner_term_export >= interface->term_export_count ||
			association->graph_type_export >= interface->type_export_count ||
			association->result_type_export >= interface->type_export_count ||
			association->graph_interface_term_export >=
				interface->term_export_count ||
			association->certified_runner_term_export >=
				interface->term_export_count ||
			(association->certified_adapter_term_export != PROTOTYPE_INVALID_ID &&
			 association->certified_adapter_term_export >=
				interface->term_export_count)) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (interface->function_graph_associations[j].owner_term_export ==
				association->owner_term_export) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_INTERFACE, i
				);
			}
		}
		const struct prototype_semantic_type_export* graph_export =
			&interface->type_exports[association->graph_type_export];
		const struct prototype_semantic_type_export* result_export =
			&interface->type_exports[association->result_type_export];
		const struct prototype_semantic_type_declaration* graph_type =
			&state->module->type_schema.type_declarations[
				graph_export->type_declaration
			];
		const struct prototype_semantic_type_declaration* result_type =
			&state->module->type_schema.type_declarations[
				result_export->type_declaration
			];
		if (result_type->constructor_count != 1) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		const struct prototype_semantic_type_constructor* result_constructor =
			&state->module->type_schema.constructor_declarations[
				result_type->first_constructor
			];
		uint32_t field_context = result_constructor->field_context;
		uint32_t field_count = 0;
		while (field_context != result_constructor->parameter_context) {
			if (field_context == 0 || field_context >=
					state->module->contexts.context_count ||
				field_count == state->module->contexts.context_count) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_INTERFACE, i
				);
			}
			field_context =
				state->module->contexts.contexts[field_context].parent;
			field_count += 1;
		}
		if (field_count != 2) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		const struct prototype_semantic_term_export* runner =
			&interface->term_exports[
				association->certified_runner_term_export
			];
		uint32_t runner_classifier = runner->classifier;
		uint32_t runner_domain_count = 0;
		uint32_t domain;
		uint32_t binding;
		uint32_t body;
		while (checker_pi_parts(
				state, runner_classifier, &domain, &binding, &body
			) == 0) {
			runner_classifier = body;
			runner_domain_count += 1;
			if (runner_domain_count > state->module->terms.term_count) {
				break;
			}
		}
		struct checker_computation_type_view runner_result;
		if (runner_domain_count == 0 || checker_computation_type_view(
				state, runner_classifier, &runner_result
			) != 0) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		const struct prototype_term* runner_result_term = checker_term(
			state, runner_result.result
		);
		if (!runner_result_term || runner_result_term->tag !=
				PROTOTYPE_TERM_TYPE_VIEW ||
			runner_result_term->as.type_view.view_type_id !=
				result_export->type_declaration ||
			graph_type->constructor_count == 0) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		(void)binding;
		(void)domain;
	}
	for (uint32_t i = 0;
		i < interface->function_graph_selector_group_count;
		++i) {
		const struct prototype_semantic_function_graph_selector_group* group =
			&interface->function_graph_selector_groups[i];
		const struct prototype_semantic_function_graph_association* association =
			&interface->function_graph_associations[group->association];
		const struct prototype_semantic_type_export* graph_export =
			&interface->type_exports[association->graph_type_export];
		const struct prototype_semantic_type_declaration* graph_type =
			&state->module->type_schema.type_declarations[
				graph_export->type_declaration
			];
		if (group->constructor_ordinal >= graph_type->constructor_count) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		const struct prototype_semantic_type_constructor* constructor =
			&state->module->type_schema.constructor_declarations[
				graph_type->first_constructor + group->constructor_ordinal
			];
		uint32_t context = constructor->field_context;
		uint32_t field_count = 0;
		while (context != constructor->parameter_context) {
			if (context == 0 || context >= state->module->contexts.context_count ||
				field_count == state->module->contexts.context_count) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_INTERFACE, i
				);
			}
			context = state->module->contexts.contexts[context].parent;
			field_count += 1;
		}
		if (group->value_field_ordinal >= field_count ||
			(group->graph_field_ordinal != PROTOTYPE_INVALID_ID &&
			 group->graph_field_ordinal >= field_count) ||
			(((group->role_mask &
				PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_GRAPH) == 0) !=
			 (group->graph_field_ordinal == PROTOTYPE_INVALID_ID)) ||
			(((group->role_mask &
				PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_IH) != 0) !=
			 (group->recursive != 0)) ||
			(group->graph_field_ordinal != PROTOTYPE_INVALID_ID &&
			 group->graph_field_ordinal != group->value_field_ordinal + 1)) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_semantic_function_graph_selector_group* previous =
				&interface->function_graph_selector_groups[j];
			if (previous->association == group->association &&
				previous->constructor_ordinal == group->constructor_ordinal &&
				previous->display_symbol_id == group->display_symbol_id) {
				return checker_stop(
					state, PROTOTYPE_CHECKER_REJECTED,
					PROTOTYPE_CHECKER_STOP_INTERFACE, i
				);
			}
		}
	}
	for (uint32_t i = 0; i < state->module->terms.term_count; ++i) {
		const struct prototype_term* term = &state->module->terms.terms[i];
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		int found = 0;
		for (uint32_t j = 0; j < interface->dependency_count; ++j) {
			if (interface->dependencies[j].namespace_symbol_id ==
					term->as.external_ref.name.namespace_symbol_id &&
				interface->dependencies[j].name_symbol_id ==
					term->as.external_ref.name.name_symbol_id) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
		const struct prototype_checked_module* imported_owner;
		const struct prototype_semantic_term_export* imported_export;
		uint32_t local_classifier;
		if (checker_resolve_imported_term(
				state, term->as.external_ref.name,
				&imported_owner, &imported_export
			) != 0 || checker_external_ref_classifier(
				state, i, &local_classifier
			) != 0 || checker_cross_classifier_equal(
				state, local_classifier,
				imported_owner, imported_export->classifier
			) != 1) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
	}
	for (uint32_t i = 0; i < interface->dependency_count; ++i) {
		int found = 0;
		for (uint32_t j = 0; j < state->module->terms.term_count; ++j) {
			const struct prototype_term* term = &state->module->terms.terms[j];
			if (term->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
				term->as.external_ref.name.namespace_symbol_id ==
					interface->dependencies[i].namespace_symbol_id &&
				term->as.external_ref.name.name_symbol_id ==
					interface->dependencies[i].name_symbol_id) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return checker_stop(
				state, PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_INTERFACE, i
			);
		}
	}
	return PROTOTYPE_CHECKER_COMPLETE;
}

int prototype_checker_check_module(
	const struct prototype_elaborated_module_view* module,
	const struct prototype_checker_options* options,
	struct prototype_checked_module** p_checked,
	struct prototype_checker_report* p_report
) {
	if (!p_checked || !p_report) {
		return -1;
	}
	*p_checked = NULL;
	*p_report = (struct prototype_checker_report) {
		.status = PROTOTYPE_CHECKER_REJECTED,
		.stop_reason = PROTOTYPE_CHECKER_STOP_MALFORMED,
		.effort_used = 0,
		.subject = PROTOTYPE_INVALID_ID
	};
	if (!module || !options || !options->effort ||
		(options->imported_base_count != 0 && !options->imported_bases) ||
		prototype_elaborated_module_validate_structure(module) != 0) {
		return 0;
	}
	struct checker_state state = {
		.module = module,
		.imported_bases = options->imported_bases,
		.imported_base_count = options->imported_base_count,
		.universe_solutions = NULL,
		.universe_solution_count = 0,
		.effort = options->effort,
		.effort_start = options->effort->used,
		.stop_reason = PROTOTYPE_CHECKER_STOP_NONE,
		.subject = PROTOTYPE_INVALID_ID
	};
	struct prototype_checker_universe_solution* universe_solutions = NULL;
	size_t universe_solution_count = 0;
	int status = prototype_checker_reconstruct_universes(
		module, &universe_solutions, &universe_solution_count
	) == 0 ? PROTOTYPE_CHECKER_COMPLETE : checker_stop(
		&state,
		PROTOTYPE_CHECKER_REJECTED,
		PROTOTYPE_CHECKER_STOP_UNIVERSE,
		PROTOTYPE_INVALID_ID
	);
	state.universe_solutions = universe_solutions;
	state.universe_solution_count = universe_solution_count;
	if (status == PROTOTYPE_CHECKER_COMPLETE &&
		universe_solution_count != module->universes.level_count) {
		status = checker_stop(
			&state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_UNIVERSE,
			PROTOTYPE_INVALID_ID
		);
	}
	for (size_t i = 0;
		status == PROTOTYPE_CHECKER_COMPLETE && i < universe_solution_count;
		++i) {
		int found = 0;
		for (size_t j = 0; j < module->universes.level_count; ++j) {
			if (module->universes.levels[j].level_var ==
					universe_solutions[i].level_var &&
				module->universes.levels[j].value == universe_solutions[i].value) {
				found = 1;
				break;
			}
		}
		if (!found) {
			status = checker_stop(
				&state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_UNIVERSE,
				universe_solutions[i].level_var
			);
		}
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_contexts(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_substitutions(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_type_schema(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_dimensions(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_occurrences(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_contracts(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_substitution_assignments(&state);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		status = checker_check_interface(&state);
	}
	uint64_t reconstructed_capabilities = 0;
	if (status == PROTOTYPE_CHECKER_COMPLETE &&
		checker_runtime_capabilities(
			&state, &reconstructed_capabilities
		) != 0) {
		status = checker_stop(
			&state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			PROTOTYPE_INVALID_ID
		);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE &&
		state.module->required_runtime_capabilities !=
			reconstructed_capabilities) {
		status = checker_stop(
			&state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_OCCURRENCE,
			PROTOTYPE_INVALID_ID
		);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE &&
		state.module->selected_entry_occurrence != PROTOTYPE_INVALID_ID) {
		const struct prototype_semantic_occurrence* entry =
			&state.module->occurrences.occurrences[
				state.module->selected_entry_occurrence
			];
		if (entry->core_term != state.module->selected_entry_term ||
			entry->asserted_classifier != state.module->selected_entry_classifier) {
			status = checker_stop(
				&state,
				PROTOTYPE_CHECKER_REJECTED,
				PROTOTYPE_CHECKER_STOP_OCCURRENCE,
				state.module->selected_entry_occurrence
			);
		}
	}
	struct prototype_checker_usage_solution* usage_solutions = NULL;
	struct prototype_usage_entry* usage_entries = NULL;
	size_t usage_entry_count = 0;
	if (status == PROTOTYPE_CHECKER_COMPLETE &&
		prototype_checker_reconstruct_usage(
			state.module,
			&usage_solutions,
			&usage_entries,
			&usage_entry_count
		) != 0) {
		status = checker_stop(
			&state,
			PROTOTYPE_CHECKER_REJECTED,
			PROTOTYPE_CHECKER_STOP_RESOURCE_USAGE,
			PROTOTYPE_INVALID_ID
		);
	}
	if (status == PROTOTYPE_CHECKER_COMPLETE) {
		struct prototype_checked_module* checked = malloc(sizeof(*checked));
		if (!checked) {
			free(usage_solutions);
			free(usage_entries);
			free(universe_solutions);
			return -1;
		}
		if (module->interface.term_export_count >
				SIZE_MAX - module->interface.type_export_count ||
			module->interface.term_export_count +
				module->interface.type_export_count >
				SIZE_MAX - module->interface.constructor_export_count) {
			free(checked);
			free(usage_solutions);
			free(usage_entries);
			free(universe_solutions);
			return -1;
		}
		size_t export_count = module->interface.term_export_count +
			module->interface.type_export_count +
			module->interface.constructor_export_count;
		struct prototype_checked_export_ref* exports = export_count == 0 ?
			NULL : malloc(export_count * sizeof(*exports));
		if (export_count != 0 && !exports) {
			free(checked);
			free(usage_solutions);
			free(usage_entries);
			free(universe_solutions);
			return -1;
		}
		checked->seal = PROTOTYPE_CHECKED_MODULE_SEAL;
		checked->elaborated = module;
		checked->universe_solutions = universe_solutions;
		checked->universe_solution_count = universe_solution_count;
		checked->usage_solutions = usage_solutions;
		checked->usage_entries = usage_entries;
		checked->usage_entry_count = usage_entry_count;
		checked->exports = exports;
		checked->export_count = export_count;
		size_t export_index = 0;
#define INITIALIZE_EXPORTS(count, export_kind) \
		for (size_t i = 0; i < (count); ++i) { \
		exports[export_index++] = (struct prototype_checked_export_ref) { \
			.seal = PROTOTYPE_CHECKED_EXPORT_SEAL, \
			.owner = checked, \
			.kind = (export_kind), \
			.index = i \
		}; \
	}
		INITIALIZE_EXPORTS(
			module->interface.term_export_count, PROTOTYPE_CHECKED_EXPORT_TERM
		);
		INITIALIZE_EXPORTS(
			module->interface.type_export_count, PROTOTYPE_CHECKED_EXPORT_TYPE
		);
		INITIALIZE_EXPORTS(
			module->interface.constructor_export_count,
			PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR
		);
#undef INITIALIZE_EXPORTS
		*p_checked = checked;
		universe_solutions = NULL;
	} else {
		free(usage_solutions);
		free(usage_entries);
	}
	p_report->status = status;
	p_report->stop_reason = state.stop_reason;
	p_report->effort_used = state.effort->used - state.effort_start;
	p_report->subject = state.subject;
	free(universe_solutions);
	return 0;
}

void prototype_checked_module_destroy(
	struct prototype_checked_module* checked
) {
	if (!checked) {
		return;
	}
	checked->seal = 0;
	checked->elaborated = NULL;
	free(checked->universe_solutions);
	checked->universe_solutions = NULL;
	checked->universe_solution_count = 0;
	free(checked->usage_solutions);
	free(checked->usage_entries);
	checked->usage_solutions = NULL;
	checked->usage_entries = NULL;
	checked->usage_entry_count = 0;
	for (size_t i = 0; i < checked->export_count; ++i) {
		checked->exports[i].seal = 0;
		checked->exports[i].owner = NULL;
	}
	free(checked->exports);
	checked->exports = NULL;
	checked->export_count = 0;
	free(checked);
}

const struct prototype_elaborated_module_view*
prototype_checked_module_elaborated_view(
	const struct prototype_checked_module* checked
) {
	if (!checked || checked->seal != PROTOTYPE_CHECKED_MODULE_SEAL) {
		return NULL;
	}
	return checked->elaborated;
}

int prototype_checked_module_occurrence_usage(
	const struct prototype_checked_module* checked,
	uint32_t occurrence_id,
	const struct prototype_usage_entry** p_entries,
	size_t* p_entry_count,
	int* p_binder_usage
) {
	if (!checked || checked->seal != PROTOTYPE_CHECKED_MODULE_SEAL ||
		!checked->elaborated || !p_entries || !p_entry_count || !p_binder_usage ||
		occurrence_id >= checked->elaborated->occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_checker_usage_solution* solution =
		&checked->usage_solutions[occurrence_id];
	if (solution->first_entry > checked->usage_entry_count ||
		solution->entry_count > checked->usage_entry_count -
			solution->first_entry) {
		return -1;
	}
	*p_entries = solution->entry_count == 0 ? NULL :
		&checked->usage_entries[solution->first_entry];
	*p_entry_count = solution->entry_count;
	*p_binder_usage = solution->binder_usage;
	return 0;
}

static size_t checked_export_kind_count(
	const struct prototype_checked_module* checked,
	int kind
) {
	if (!checked || checked->seal != PROTOTYPE_CHECKED_MODULE_SEAL ||
		!checked->elaborated) {
		return 0;
	}
	switch (kind) {
		case PROTOTYPE_CHECKED_EXPORT_TERM:
			return checked->elaborated->interface.term_export_count;
		case PROTOTYPE_CHECKED_EXPORT_TYPE:
			return checked->elaborated->interface.type_export_count;
		case PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR:
			return checked->elaborated->interface.constructor_export_count;
		default:
			return 0;
	}
}

size_t prototype_checked_module_export_count(
	const struct prototype_checked_module* checked,
	int kind
) {
	return checked_export_kind_count(checked, kind);
}

const struct prototype_checked_export_ref* prototype_checked_module_export_at(
	const struct prototype_checked_module* checked,
	int kind,
	size_t index
) {
	size_t count = checked_export_kind_count(checked, kind);
	if (index >= count) {
		return NULL;
	}
	size_t offset = 0;
	if (kind == PROTOTYPE_CHECKED_EXPORT_TYPE) {
		offset = checked->elaborated->interface.term_export_count;
	} else if (kind == PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR) {
		offset = checked->elaborated->interface.term_export_count +
			checked->elaborated->interface.type_export_count;
	}
	return &checked->exports[offset + index];
}

static int checked_export_ref_valid(
	const struct prototype_checked_export_ref* export_ref,
	int kind
) {
	return export_ref && export_ref->seal == PROTOTYPE_CHECKED_EXPORT_SEAL &&
		export_ref->owner && export_ref->owner->seal ==
			PROTOTYPE_CHECKED_MODULE_SEAL && export_ref->kind == kind &&
		export_ref->index < checked_export_kind_count(export_ref->owner, kind);
}

const struct prototype_semantic_term_export* prototype_checked_term_export(
	const struct prototype_checked_export_ref* export_ref
) {
	return checked_export_ref_valid(export_ref, PROTOTYPE_CHECKED_EXPORT_TERM) ?
		&export_ref->owner->elaborated->interface.term_exports[export_ref->index] :
		NULL;
}

const struct prototype_semantic_type_export* prototype_checked_type_export(
	const struct prototype_checked_export_ref* export_ref
) {
	return checked_export_ref_valid(export_ref, PROTOTYPE_CHECKED_EXPORT_TYPE) ?
		&export_ref->owner->elaborated->interface.type_exports[export_ref->index] :
		NULL;
}

const struct prototype_semantic_constructor_export*
prototype_checked_constructor_export(
	const struct prototype_checked_export_ref* export_ref
) {
	return checked_export_ref_valid(
		export_ref, PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR
	) ? &export_ref->owner->elaborated->interface.constructor_exports[
		export_ref->index
	] : NULL;
}
