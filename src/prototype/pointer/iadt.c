#include "iadt.h"
#include "evidence.h"

static const struct pg_object_class constructor_class = {"constructor"};
static const struct pg_object_class match_class = {"match"};
static const struct pg_object_class match_action_class = {"match-action"};
static const struct pg_object match_action = {PG_SEMANTIC_OBJECT, &match_action_class};

struct pg_constructor {
	struct pg_object object;
	const struct pg_data_layout *layout;
	size_t arity;
};
struct pg_data_layout {
	struct pg_object matcher;
	size_t count;
	struct pg_constructor constructors[];
};

const struct pg_data_layout *pg_data_layout(struct pg_graph *graph,
	size_t count, const size_t *arities)
{
	if (count && !arities) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_data_layout)) / sizeof(struct pg_constructor)) return NULL;
	struct pg_data_layout *layout = pg_alloc(graph, sizeof(*layout) + count * sizeof(*layout->constructors));
	if (!layout) return NULL;
	layout->matcher = (struct pg_object){PG_SEMANTIC_OBJECT, &match_class};
	layout->count = count;
	for (size_t i = 0; i < count; ++i)
		layout->constructors[i] = (struct pg_constructor){{PG_SEMANTIC_OBJECT, &constructor_class}, layout, arities[i]};
	return layout;
}

const struct pg_object *pg_data_constructor(const struct pg_data_layout *layout, size_t index)
{
	return layout && index < layout->count ? &layout->constructors[index].object : NULL;
}

const struct pg_object *pg_data_matcher(const struct pg_data_layout *layout)
{
	return layout ? &layout->matcher : NULL;
}

static const struct pg_constructor *constructor(const struct pg_object *object,
	const struct pg_data_layout *layout)
{
	if (!object || object->kind != PG_SEMANTIC_OBJECT || object->owner != &constructor_class) return NULL;
	const struct pg_constructor *result = (const struct pg_constructor *)object;
	return result->layout == layout ? result : NULL;
}

const struct pg_term *pg_data_match(struct pg_graph *graph, const struct pg_data_layout *layout,
	const struct pg_term *scrutinee, size_t count, const struct pg_match_clause *clauses)
{
	if (!layout || !scrutinee || count != layout->count) return NULL;
	if (count && !clauses) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_term *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_term **branches = pg_alloc(&temporary, count * sizeof(*branches));
	const struct pg_term *result = NULL;
	if (count && !branches) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_constructor *c = constructor(clauses[i].constructor, layout);
		if (!c || !clauses[i].branch) goto done;
		size_t position = (size_t)(c - layout->constructors);
		if (branches[position]) goto done;
		branches[position] = clauses[i].branch;
	}
	result = pg_application(graph, pg_reference(graph, &layout->matcher), scrutinee);
	for (size_t i = 0; i < count; ++i) result = pg_application(graph, result, branches[i]);
done:
	pg_graph_destroy(&temporary);
	return result;
}

static int apply_fields(struct pg_eval *machine, struct pg_closure branch,
	size_t count, const struct pg_term *const *fields, size_t consume)
{
	const struct pg_object *k = pg_binder(machine->output);
	const struct pg_term *body = pg_reference(machine->output, k);
	for (size_t i = 0; i < count; ++i) body = pg_application(machine->output, body, fields[i]);
	/* Reuse lexical substitution for the selected branch's captured scope. */
	return pg_eval_apply(machine, (struct pg_closure){pg_lambda(machine->output, k, body), NULL}, branch, consume);
}

static int match_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_data_layout *layout = (const struct pg_data_layout *)machine->current.term->as.reference;
	const struct pg_term *head = answer;
	size_t count = 0;
	while (head->kind == PG_APPLICATION) { ++count; head = head->as.application.function; }
	if (head->kind != PG_REFERENCE) return 1;
	const struct pg_constructor *c = constructor(head->as.reference, layout);
	if (!c || count != c->arity) return 1;
	if (count > SIZE_MAX / sizeof(const struct pg_term *)) return -1;
	const struct pg_term **fields = pg_alloc(&machine->temporary, count * sizeof(*fields));
	if (count && !fields) return -1;
	for (size_t i = count; i; --i, answer = answer->as.application.function)
		fields[i - 1] = answer->as.application.argument;
	size_t position = (size_t)(c - layout->constructors);
	struct pg_closure branch = *pg_eval_argument(machine, position + 1);
	return apply_fields(machine, branch, count, fields, layout->count + 1);
}

static const struct pg_data_layout *matcher(const struct pg_term *term)
{
	if (term->kind != PG_REFERENCE) return NULL;
	const struct pg_object *object = term->as.reference;
	if (object->kind != PG_SEMANTIC_OBJECT || object->owner != &match_class) return NULL;
	return (const struct pg_data_layout *)object;
}

static int action_answer(struct pg_eval *machine, const struct pg_term *answer, const void *unused)
{
	(void)unused;
	const struct pg_data_layout *layout = matcher(pg_eval_argument(machine, 0)->term);
	const struct pg_term *prefix = answer, *source;
	size_t supplied = 0;
	while (!pg_identity_action_view(prefix, &source)) {
		if (prefix->kind != PG_APPLICATION) return 1;
		++supplied;
		prefix = prefix->as.application.function;
	}
	const struct pg_term *head = source;
	size_t diagonal = 0;
	while (head->kind == PG_APPLICATION) { ++diagonal; head = head->as.application.function; }
	if (head->kind != PG_REFERENCE || supplied % 3) return 1;
	const struct pg_constructor *c = constructor(head->as.reference, layout);
	if (!c || diagonal > c->arity || supplied / 3 != c->arity - diagonal) return 1;
	if (c->arity > SIZE_MAX / (3 * sizeof(const struct pg_term *))) return -1;
	size_t count = 3 * c->arity;
	const struct pg_term **fields = pg_alloc(&machine->temporary, count * sizeof(*fields));
	if (count && !fields) return -1;
	/* Diagonal_argument can compress an initial part of the boundary spine.
	 * Decode that prefix and retain every explicit remaining chosen path. */
	for (size_t i = diagonal; i; --i, source = source->as.application.function) {
		const struct pg_term *value = source->as.application.argument;
		fields[3 * (i - 1)] = fields[3 * (i - 1) + 1] = value;
		fields[3 * (i - 1) + 2] = pg_identity_action(machine->output, value);
	}
	for (size_t i = supplied; i; --i, answer = answer->as.application.function)
		fields[3 * diagonal + i - 1] = answer->as.application.argument;
	size_t position = (size_t)(c - layout->constructors);
	return apply_fields(machine, *pg_eval_argument(machine, 6 + 3 * position), count, fields,
		1 + 3 * (layout->count + 1));
}

int pg_data_action(struct pg_eval *machine, const struct pg_term *source)
{
	const struct pg_term *head = source;
	size_t count = 0;
	while (head->kind == PG_APPLICATION) { ++count; head = head->as.application.function; }
	if (!matcher(head)) return 1;
	if (count > SIZE_MAX / sizeof(const struct pg_term *)) return -1;
	const struct pg_term **arguments = pg_alloc(&machine->temporary, count * sizeof(*arguments));
	if (count && !arguments) return -1;
	for (size_t i = count; i; --i, source = source->as.application.function)
		arguments[i - 1] = source->as.application.argument;
	const struct pg_term *result = pg_application(machine->output, pg_reference(machine->output, &match_action), head);
	for (size_t i = 0; i < count; ++i) {
		result = pg_identity_instance(machine->output, result, arguments[i], arguments[i]);
		result = pg_application(machine->output, result, pg_identity_action(machine->output, arguments[i]));
	}
	return pg_eval_enter(machine, (struct pg_closure){result, NULL}, 1);
}

int pg_data_dispatch(struct pg_eval *machine)
{
	const struct pg_object *object = machine->current.term->as.reference;
	if (object == &match_action) {
		const struct pg_closure *owner = pg_eval_argument(machine, 0);
		const struct pg_data_layout *layout = owner ? matcher(owner->term) : NULL;
		if (!layout) return 1;
		if (layout->count > (SIZE_MAX - 4) / 3) return -1;
		if (!pg_eval_argument(machine, 3 * (layout->count + 1))) return 1;
		return pg_eval_demand(machine, 3, action_answer, NULL);
	}
	if (object->kind != PG_SEMANTIC_OBJECT || object->owner != &match_class) return 1;
	const struct pg_data_layout *layout = (const struct pg_data_layout *)object;
	if (!pg_eval_argument(machine, layout->count)) return 1;
	return pg_eval_demand(machine, 0, match_answer, NULL);
}

struct pg_data_signature {
	const struct pg_typing *owner;
	const struct pg_evidence *parameters;
	const struct pg_evidence *indices;
};

struct pg_data_schema {
	const struct pg_data_layout *layout;
	const struct pg_data_signature *signature;
	const struct pg_evidence *results[];
};

const struct pg_data_signature *pg_data_signature(struct pg_typing *typing,
	const struct pg_evidence *parameters, const struct pg_evidence *indices)
{
	if (!pg_evidence_owned_by(parameters, typing) || !pg_evidence_owned_by(indices, typing)) return NULL;
	if (pg_evidence_judgement(parameters) != PG_JUDGEMENT_CONTEXT
		|| pg_evidence_judgement(indices) != PG_JUDGEMENT_CONTEXT) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(indices), pg_evidence_context(parameters), &count)) return NULL;
	struct pg_data_signature *signature = pg_alloc(typing->graph, sizeof(*signature));
	if (!signature) return NULL;
	*signature = (struct pg_data_signature){typing, parameters, indices};
	return signature;
}

int pg_data_field_positive(const struct pg_term *type,
	const struct pg_object *self, size_t index_count)
{
	if (!type || !self || self->kind != PG_BINDER) return -1;
	for (;;) {
		const struct pg_term *domain, *codomain;
		const struct pg_object *binder;
		if (pg_pi_view(type, &domain, &binder, &codomain)) {
			int independent = pg_term_independent(domain, self);
			if (independent != 1) return independent;
			if (binder == self) return 1;
			type = codomain;
			continue;
		}
		if (pg_return_type_view(type, &codomain) || pg_thunk_type_view(type, &codomain)) {
			type = codomain;
			continue;
		}
		const struct pg_term *head = type;
		size_t count = 0;
		while (head->kind == PG_APPLICATION) {
			++count;
			head = head->as.application.function;
		}
		if (head->kind != PG_REFERENCE || head->as.reference != self)
			return pg_term_independent(type, self);
		if (count != index_count) return 0;
		while (type->kind == PG_APPLICATION) {
			int independent = pg_term_independent(type->as.application.argument, self);
			if (independent != 1) return independent;
			type = type->as.application.function;
		}
		return 1;
	}
}

const struct pg_data_schema *pg_data_schema(struct pg_typing *typing,
	const struct pg_data_signature *signature,
	size_t count, const struct pg_evidence *const *results)
{
	if (!signature || signature->owner != typing || (count && !results)) return NULL;
	const struct pg_evidence *parameters = signature->parameters, *indices = signature->indices;
	const struct pg_context *prefix = pg_evidence_context(parameters);
	size_t parameter_count;
	if (pg_context_extension_size(prefix, NULL, &parameter_count) != 0) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_data_schema)) / sizeof(*results)) return NULL;
	if (count > SIZE_MAX / sizeof(size_t)) return NULL;
	struct pg_graph temporary = {0};
	size_t *arities = pg_alloc(&temporary, count * sizeof(*arities));
	struct pg_data_schema *schema = NULL;
	if (count && !arities) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *result = results[i];
		if (!pg_evidence_owned_by(result, typing)) goto done;
		if (pg_evidence_rule(result) != PG_CONTEXT_SUBSTITUTION) goto done;
		if (pg_evidence_context(pg_evidence_premise(result, 0)) != pg_evidence_context(indices)) goto done;
		if (pg_context_extension_size(pg_evidence_context(result), prefix, &arities[i]) != 0) goto done;
		const struct pg_context *parameter = prefix;
		for (size_t j = parameter_count; j; --j, parameter = parameter->parent) {
			const struct pg_term *image = pg_evidence_subject(pg_evidence_premise(result, j + 1))->core;
			if (image->kind != PG_REFERENCE || image->as.reference != parameter->binder) goto done;
		}
	}
	const struct pg_data_layout *layout = pg_data_layout(typing->graph, count, arities);
	if (!layout) goto done;
	schema = pg_alloc(typing->graph, sizeof(*schema) + count * sizeof(*results));
	if (!schema) goto done;
	schema->signature = signature;
	schema->layout = layout;
	for (size_t i = 0; i < count; ++i) schema->results[i] = results[i];
done:
	pg_graph_destroy(&temporary);
	return schema;
}

const struct pg_data_layout *pg_data_schema_layout(const struct pg_data_schema *schema)
{
	return schema ? schema->layout : NULL;
}

const struct pg_evidence *pg_data_schema_indices(const struct pg_data_schema *schema)
{
	return schema ? schema->signature->indices : NULL;
}

const struct pg_evidence *pg_data_schema_result(const struct pg_data_schema *schema,
	const struct pg_object *object)
{
	if (!schema) return NULL;
	const struct pg_constructor *c = constructor(object, schema->layout);
	return c ? schema->results[c - schema->layout->constructors] : NULL;
}

const struct pg_evidence *pg_data_schema_fields(const struct pg_data_schema *schema,
	const struct pg_object *object)
{
	const struct pg_evidence *result = pg_data_schema_result(schema, object);
	return result ? pg_evidence_premise(result, 1) : NULL;
}

const struct pg_evidence *pg_data_result(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *object,
	const struct pg_evidence *instance)
{
	return pg_prove_substitution_compose(typing, pg_data_schema_result(schema, object), instance);
}

static const struct pg_evidence *signature_suffix(struct pg_typing *typing,
	const struct pg_data_signature *signature, const struct pg_evidence *fields,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *values)
{
	if (!signature || signature->owner != typing) return NULL;
	if (!fields || !pg_evidence_owned_by(parameters, typing) || (count && !values)) return NULL;
	if (pg_evidence_rule(parameters) != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(pg_evidence_premise(parameters, 0)) != pg_evidence_context(signature->parameters)) return NULL;
	return pg_prove_substitution_extend(typing, parameters, fields, count, values);
}

const struct pg_evidence *pg_data_signature_instance(struct pg_typing *typing,
	const struct pg_data_signature *signature, const struct pg_evidence *parameters,
	size_t count, const struct pg_evidence *const *indices)
{
	return signature ? signature_suffix(typing, signature, signature->indices, parameters, count, indices) : NULL;
}

const struct pg_evidence *pg_data_instance(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *object,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *values)
{
	return schema ? signature_suffix(typing, schema->signature, pg_data_schema_fields(schema, object), parameters, count, values) : NULL;
}

const struct pg_evidence *pg_data_branch(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema,
	const struct pg_object *object, const struct pg_evidence *body)
{
	const struct pg_evidence *context = pg_data_schema_fields(schema, object);
	return context ? pg_prove_abstract(typing, classifiers, schema->signature->parameters, context, body) : NULL;
}

const struct pg_evidence *pg_data_branch_motive(struct pg_typing *typing,
	const struct pg_data_schema *schema, const struct pg_object *object,
	const struct pg_evidence *motive)
{
	if (!pg_evidence_owned_by(motive, typing)) return NULL;
	if (pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	return pg_prove_reindex(typing, pg_data_schema_result(schema, object), motive);
}

const struct pg_evidence *pg_data_case(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema,
	const struct pg_object *object, const struct pg_evidence *motive,
	const struct pg_evidence *body, const struct pg_conversion_certificate *conversion)
{
	const struct pg_evidence *target = pg_data_branch_motive(typing, schema, object, motive);
	const struct pg_evidence *checked = pg_prove_conversion(typing, body, target, conversion);
	return pg_data_branch(typing, classifiers, schema, object, checked);
}
