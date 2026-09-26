#include "a_program/kernel/type_projection.h"

#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

#include <stdint.h>
#include <stdlib.h>

static int projection_arguments_valid(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration* declaration,
	const uint32_t* arguments,
	uint32_t argument_count
) {
	if (!terms || !declaration || argument_count > 16 ||
		declaration->parameter_count > UINT32_MAX - declaration->index_count ||
		argument_count > declaration->parameter_count + declaration->index_count ||
		(argument_count > 0 && !arguments)) {
		return 0;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (arguments[i] >= terms->term_count) {
			return 0;
		}
	}
	return 1;
}

int prototype_type_projection_instance_info(
	const struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t instance,
	uint32_t* p_type_id,
	uint32_t* arguments,
	uint32_t* p_argument_count
) {
	if (!terms || !semantic_schema || !p_type_id || !p_argument_count) {
		return -1;
	}
	struct prototype_qualified_name identity;
	if (prototype_term_nominal_type_instance_info(
			terms, instance, &identity, arguments, p_argument_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < semantic_schema->type_count; ++i) {
		const struct prototype_type_declaration* declaration =
			&semantic_schema->type_declarations[i];
		if (declaration->namespace_symbol_id == identity.namespace_symbol_id &&
			declaration->name_symbol_id == identity.name_symbol_id) {
			*p_type_id = i;
			return 0;
		}
	}
	return 1;
}

int prototype_type_projection_source_instance_make(
	struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t type_id,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_instance
) {
	if (!terms || !semantic_schema || !p_instance ||
		type_id >= semantic_schema->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* declaration =
		&semantic_schema->type_declarations[type_id];
	if (!projection_arguments_valid(
			terms, declaration, arguments, argument_count
		)) {
		return -1;
	}
	struct prototype_qualified_name identity = {
		.namespace_symbol_id = declaration->namespace_symbol_id,
		.name_symbol_id = declaration->name_symbol_id
	};
	uint32_t current;
	if (prototype_term_type_declaration(
			terms, identity, &current
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_term_app(terms, current, arguments[i], &current) != 0) {
			return -1;
		}
	}
	*p_instance = current;
	return 0;
}

int prototype_type_projection_source_instance_extend(
	struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_result
) {
	if (!terms || !semantic_schema || !p_result ||
		instance >= terms->term_count || argument >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			terms, semantic_schema, instance, &type_id, arguments, &argument_count
		) != 0 || type_id >= semantic_schema->type_count ||
		argument_count >= 16) {
		return -1;
	}
	const struct prototype_type_declaration* declaration =
		&semantic_schema->type_declarations[type_id];
	if (declaration->representation_id != PROTOTYPE_INVALID_ID ||
		declaration->parameter_count > UINT32_MAX - declaration->index_count ||
		argument_count >= declaration->parameter_count + declaration->index_count) {
		return -1;
	}
	arguments[argument_count] = argument;
	return prototype_type_projection_source_instance_make(
		terms,
		semantic_schema,
		type_id,
		arguments,
		argument_count + 1,
		p_result
	);
}

int prototype_type_projection_instance_make(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_instance
) {
	if (!terms || !type_declarations || !p_instance ||
		type_id >= type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* declaration =
		&type_declarations->semantic_schema.type_declarations[type_id];
	if (!projection_arguments_valid(
			terms, declaration, arguments, argument_count
		) || declaration->representation_id == PROTOTYPE_INVALID_ID ||
		declaration->representation_id >=
			type_declarations->representation_db.representation_count) {
		return -1;
	}
	uint32_t core;
	if (prototype_term_type_former(
			terms,
			declaration->representation_id,
			declaration->constructor_count,
			&core
		) != 0) {
		return -1;
	}
	uint32_t source;
	if (prototype_type_projection_source_instance_make(
			terms,
			&type_declarations->semantic_schema,
			type_id,
			arguments,
			argument_count,
			&source
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t argument = arguments[i];
		if (terms->terms[argument].tag == PROTOTYPE_TERM_TYPE_VIEW) {
			argument = terms->terms[argument].as.type_view.core;
		}
		if (prototype_term_app(terms, core, argument, &core) != 0) {
			return -1;
		}
	}
	struct prototype_qualified_name identity = {
		.namespace_symbol_id = declaration->namespace_symbol_id,
		.name_symbol_id = declaration->name_symbol_id
	};
	return prototype_term_type_view(
		terms, identity, core, source, p_instance
	);
}

int prototype_type_projection_instance_extend(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_result
) {
	if (!terms || !type_declarations || !p_result ||
		argument >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			terms, &type_declarations->semantic_schema,
			instance, &type_id, arguments, &argument_count
		) != 0 || argument_count >= 16) {
		return -1;
	}
	arguments[argument_count] = argument;
	return prototype_type_projection_instance_make(
		terms,
		type_declarations,
		type_id,
		arguments,
		argument_count + 1,
		p_result
	);
}

int prototype_type_projection_instance_is_saturated(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance
) {
	if (!terms || !type_declarations) {
		return 0;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			terms, &type_declarations->semantic_schema,
			instance, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->semantic_schema.type_count) {
		return 0;
	}
	const struct prototype_type_declaration* declaration =
		&type_declarations->semantic_schema.type_declarations[type_id];
	return declaration->parameter_count <= UINT32_MAX - declaration->index_count &&
		argument_count == declaration->parameter_count + declaration->index_count;
}

int prototype_type_projection_parameter_instance(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t* p_parameter_instance
) {
	if (!terms || !type_declarations || !p_parameter_instance ||
		instance >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	int instance_status = prototype_type_projection_instance_info(
		terms, &type_declarations->semantic_schema, instance, &type_id,
		arguments, &argument_count
	);
	if (instance_status != 0) {
		/* This query is also used with residual Pi/Comp classifiers. They are
		 * valid terms but do not denote a nominal family instance. */
		return 1;
	}
	if (type_id >= type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* declaration =
		&type_declarations->semantic_schema.type_declarations[type_id];
	if (declaration->parameter_count > 16 ||
		declaration->parameter_count > UINT32_MAX - declaration->index_count ||
		argument_count < declaration->parameter_count ||
		argument_count > declaration->parameter_count + declaration->index_count) {
		return 1;
	}
	return prototype_type_projection_instance_make(
		terms, type_declarations, type_id, arguments,
		declaration->parameter_count, p_parameter_instance
	);
}

static int projection_owner_representation(
	const struct prototype_term_db* terms,
	uint32_t owner,
	uint32_t* p_representation
) {
	if (!terms || !p_representation || owner >= terms->term_count) {
		return -1;
	}
	uint32_t current = owner;
	if (terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return 1;
	}
	*p_representation =
		terms->terms[current].as.type_former.representation_id;
	return 0;
}

static int projection_parameter_instance_in_classifier(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t representation,
	unsigned depth,
	uint32_t* p_parameter_instance
) {
	if (!terms || !type_declarations || !p_parameter_instance ||
		classifier >= terms->term_count || depth == 0) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			terms, &type_declarations->semantic_schema, classifier, &type_id,
			arguments, &argument_count
		) == 0 && type_id < type_declarations->semantic_schema.type_count) {
		const struct prototype_type_declaration* declaration =
			&type_declarations->semantic_schema.type_declarations[type_id];
		if (declaration->representation_id == representation &&
			argument_count >= declaration->parameter_count) {
			return prototype_type_projection_instance_make(
				terms, type_declarations, type_id, arguments,
				declaration->parameter_count, p_parameter_instance
			);
		}
	}
	const struct prototype_term* term = &terms->terms[classifier];
	uint32_t first = PROTOTYPE_INVALID_ID;
	uint32_t second = PROTOTYPE_INVALID_ID;
	switch (term->tag) {
		case PROTOTYPE_TERM_PI:
			first = term->as.pi.codomain_family;
			second = term->as.pi.domain;
			break;
		case PROTOTYPE_TERM_LAMBDA:
			first = term->as.lambda.body;
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			first = term->as.computation_type.result;
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			first = term->as.thunk_type.computation;
			break;
		case PROTOTYPE_TERM_RETURN:
			first = term->as.return_term.value;
			break;
		case PROTOTYPE_TERM_THUNK:
			first = term->as.thunk.computation;
			break;
		case PROTOTYPE_TERM_FORCE:
			first = term->as.force.value;
			break;
		case PROTOTYPE_TERM_APP:
			first = term->as.app.function;
			second = term->as.app.argument;
			break;
		default:
			return 1;
	}
	if (first != PROTOTYPE_INVALID_ID) {
		int status = projection_parameter_instance_in_classifier(
			terms, type_declarations, first, representation, depth - 1,
			p_parameter_instance
		);
		if (status <= 0) {
			return status;
		}
	}
	return second == PROTOTYPE_INVALID_ID ? 1 :
		projection_parameter_instance_in_classifier(
			terms, type_declarations, second, representation, depth - 1,
			p_parameter_instance
		);
}

int prototype_type_projection_parameter_instance_for_owner_shape(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t structural_owner,
	uint32_t classifier,
	uint32_t* p_parameter_instance
) {
	if (!terms || !type_declarations || !p_parameter_instance ||
		classifier >= terms->term_count) {
		return -1;
	}
	uint32_t representation;
	int representation_status = projection_owner_representation(
		terms, structural_owner, &representation
	);
	if (representation_status != 0) {
		return representation_status;
	}
	return projection_parameter_instance_in_classifier(
		terms, type_declarations, classifier, representation, 128,
		p_parameter_instance
	);
}

struct classifier_projection_rewrite {
	uint32_t source;
	uint32_t target;
};

struct classifier_projection_context {
	struct prototype_term_db* terms;
	const struct prototype_type_declaration_db* type_declarations;
	uint32_t owner_type_id;
	uint32_t owner_representation_id;
	unsigned char* visited;
	size_t original_term_count;
	struct classifier_projection_rewrite* rewrites;
	size_t rewrite_count;
};

static int classifier_projection_core_instance_info(
	const struct classifier_projection_context* context,
	uint32_t term,
	uint32_t* arguments,
	uint32_t* p_argument_count
) {
	if (!context || !arguments || !p_argument_count ||
		context->owner_type_id == PROTOTYPE_INVALID_ID ||
		context->owner_representation_id == PROTOTYPE_INVALID_ID ||
		term >= context->terms->term_count) {
		return 1;
	}
	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = term;
	while (current < context->terms->term_count &&
		context->terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16) {
			return -1;
		}
		reversed[count++] = context->terms->terms[current].as.app.argument;
		current = context->terms->terms[current].as.app.function;
	}
	if (current >= context->terms->term_count ||
		context->terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER ||
		context->terms->terms[current].as.type_former.representation_id !=
			context->owner_representation_id) {
		return 1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_argument_count = count;
	return 0;
}

static int classifier_projection_apply_rewrites(
	struct classifier_projection_context* context,
	uint32_t term,
	uint32_t* p_projected
) {
	if (!context || !p_projected || term >= context->terms->term_count) {
		return -1;
	}
	uint32_t current = term;
	for (size_t i = context->rewrite_count; i > 0; --i) {
		const struct classifier_projection_rewrite* rewrite =
			&context->rewrites[i - 1];
		if (prototype_term_graph_replace_exact_outside_type_views(
				context->terms,
				current,
				rewrite->source,
				rewrite->target,
				&current
			) != 0) {
			return -1;
		}
	}
	*p_projected = current;
	return 0;
}

static int classifier_projection_collect(
	struct classifier_projection_context* context,
	uint32_t term_id
) {
	if (!context || term_id >= context->original_term_count) {
		return -1;
	}
	if (context->visited[term_id]) {
		return 0;
	}
	context->visited[term_id] = 1;
	const struct prototype_term term = context->terms->terms[term_id];
	if (term.tag == PROTOTYPE_TERM_TYPE_VIEW) {
		return 0;
	}

	uint32_t owner_arguments[16];
	uint32_t owner_argument_count;
	int owner_status = classifier_projection_core_instance_info(
		context, term_id, owner_arguments, &owner_argument_count
	);
	if (owner_status < 0) {
		return -1;
	}
	if (owner_status == 0) {
		for (uint32_t i = 0; i < owner_argument_count; ++i) {
			if (owner_arguments[i] < context->original_term_count &&
				classifier_projection_collect(context, owner_arguments[i]) != 0) {
				return -1;
			}
		}
		uint32_t projected_arguments[16];
		for (uint32_t i = 0; i < owner_argument_count; ++i) {
			if (classifier_projection_apply_rewrites(
					context, owner_arguments[i], &projected_arguments[i]
				) != 0) {
				return -1;
			}
		}
		uint32_t projected;
		int projection_status = prototype_type_projection_instance_make(
				context->terms,
				context->type_declarations,
				context->owner_type_id,
				projected_arguments,
				owner_argument_count,
				&projected
			);
		if (projection_status != 0) {
			if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
				fprintf(stderr,
					"classifier-projection failed stage=owner-instance term=%u "
					"owner-type=%u owner-representation=%u arguments=%u status=%d\n",
					term_id, context->owner_type_id,
					context->owner_representation_id, owner_argument_count,
					projection_status);
			}
			return -1;
		}
		if (projected != term_id) {
			if (context->rewrite_count >= context->original_term_count) {
				return -1;
			}
			context->rewrites[context->rewrite_count++] =
				(struct classifier_projection_rewrite) {
					.source = term_id,
					.target = projected
				};
		}
		return 0;
	}

	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_type_projection_instance_info(
			context->terms,
			&context->type_declarations->semantic_schema,
			term_id,
			&type_id,
			arguments,
			&argument_count
		) == 0) {
		if (type_id >= context->type_declarations->semantic_schema.type_count) {
			return -1;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (arguments[i] < context->original_term_count &&
				classifier_projection_collect(context, arguments[i]) != 0) {
				return -1;
			}
		}
		const struct prototype_type_declaration* declaration =
			&context->type_declarations->semantic_schema.type_declarations[type_id];
		if (declaration->representation_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		uint32_t projected_arguments[16];
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (classifier_projection_apply_rewrites(
					context, arguments[i], &projected_arguments[i]
				) != 0) {
				return -1;
			}
		}
		uint32_t projected;
		if (prototype_type_projection_instance_make(
				context->terms,
				context->type_declarations,
				type_id,
				projected_arguments,
				argument_count,
				&projected
			) != 0) {
			return -1;
		}
		if (projected != term_id) {
			if (context->rewrite_count >= context->original_term_count) {
				return -1;
			}
			context->rewrites[context->rewrite_count++] =
				(struct classifier_projection_rewrite) {
					.source = term_id,
					.target = projected
				};
		}
		return 0;
	}

	uint32_t child_count;
	if (prototype_term_child_count(
			context->terms, term_id, &child_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < child_count; ++i) {
		struct prototype_term_child child;
		if (prototype_term_child_at(
				context->terms, term_id, i, &child
			) != 0 || (child.term < context->original_term_count &&
			classifier_projection_collect(context, child.term) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int classifier_projection_graph(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner,
	uint32_t* p_projected
) {
	if (!terms || !type_declarations || !p_projected ||
		classifier >= terms->term_count) {
		return -1;
	}
	size_t original_term_count = terms->term_count;
	unsigned char* visited = calloc(original_term_count, sizeof(*visited));
	struct classifier_projection_rewrite* rewrites = calloc(
		original_term_count, sizeof(*rewrites)
	);
	if ((original_term_count > 0 && !visited) ||
		(original_term_count > 0 && !rewrites)) {
		free(visited);
		free(rewrites);
		return -1;
	}
	struct classifier_projection_context context = {
		.terms = terms,
		.type_declarations = type_declarations,
		.owner_type_id = PROTOTYPE_INVALID_ID,
		.owner_representation_id = PROTOTYPE_INVALID_ID,
		.visited = visited,
		.original_term_count = original_term_count,
		.rewrites = rewrites,
		.rewrite_count = 0
	};
	if (owner != PROTOTYPE_INVALID_ID) {
		uint32_t owner_arguments[16];
		uint32_t owner_argument_count;
		int owner_status = prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				owner,
				&context.owner_type_id,
				owner_arguments,
				16,
				&owner_argument_count
			);
		if (owner_status != 0 || context.owner_type_id >=
				type_declarations->semantic_schema.type_count) {
			if (getenv("A_PROGRAM_MOTIVE_TERM_TRACE") != NULL) {
				fprintf(stderr,
					"classifier-projection failed stage=owner owner=%u classifier=%u "
					"status=%d type=%u type-count=%zu arguments=%u\n",
					owner, classifier, owner_status, context.owner_type_id,
					type_declarations->semantic_schema.type_count,
					owner_argument_count);
			}
			free(rewrites);
			free(visited);
			return -1;
		}
		context.owner_representation_id = type_declarations->semantic_schema
			.type_declarations[context.owner_type_id].representation_id;
	}
	int status = classifier_projection_collect(&context, classifier);
	if (status == 0) {
		status = classifier_projection_apply_rewrites(
			&context, classifier, p_projected
		);
	}
	free(rewrites);
	free(visited);
	return status;
}

int prototype_type_projection_classifier_graph(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_projected
) {
	return classifier_projection_graph(
		terms, type_declarations, classifier, PROTOTYPE_INVALID_ID, p_projected
	);
}

int prototype_type_projection_classifier_graph_for_owner(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner,
	uint32_t* p_projected
) {
	return classifier_projection_graph(
		terms, type_declarations, classifier, owner, p_projected
	);
}
