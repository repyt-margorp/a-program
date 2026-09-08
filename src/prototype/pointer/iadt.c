#include "iadt.h"
#include "evidence.h"
#include "dag.h"

static const struct pg_object_class constructor_class = {"constructor"};
static const struct pg_object_class match_class = {"match"};
static const struct pg_object_class family_class = {"data-family"};
static const struct pg_object_class match_action_class = {"match-action"};
static const struct pg_object match_action = {PG_SEMANTIC_OBJECT, &match_action_class};

int pg_data_direct_recursion(const struct pg_term *type, const struct pg_object *self)
{
	if (!type || !self || self->kind != PG_BINDER) return -1;
	if (type->kind == PG_REFERENCE && type->as.reference == self) return 1;
	return pg_term_independent(type, self) == 1 ? 0 : -1;
}

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

const struct pg_data_layout *pg_data_layout_view(const struct pg_object *object)
{
	if (!object || object->kind != PG_SEMANTIC_OBJECT || object->owner != &match_class) return NULL;
	return (const struct pg_data_layout *)object;
}

size_t pg_data_layout_count(const struct pg_data_layout *layout)
{
	return layout ? layout->count : 0;
}

int pg_data_constructor_view(const struct pg_object *object,
	const struct pg_data_layout **layout, size_t *position, size_t *arity)
{
	if (!object || object->kind != PG_SEMANTIC_OBJECT || object->owner != &constructor_class) return 0;
	if (!layout || !position || !arity) return 0;
	const struct pg_constructor *constructor = (const struct pg_constructor *)object;
	*layout = constructor->layout;
	*position = (size_t)(constructor - constructor->layout->constructors);
	*arity = constructor->arity;
	return 1;
}

static const struct pg_constructor *constructor(const struct pg_object *object,
	const struct pg_data_layout *layout)
{
	if (!object || object->kind != PG_SEMANTIC_OBJECT || object->owner != &constructor_class) return NULL;
	const struct pg_constructor *result = (const struct pg_constructor *)object;
	return result->layout == layout ? result : NULL;
}

int pg_data_constructor_position(const struct pg_data_layout *layout,
	const struct pg_object *object, size_t *position)
{
	if (!layout || !position) return 0;
	const struct pg_constructor *found = constructor(object, layout);
	if (!found) return 0;
	*position = (size_t)(found - layout->constructors);
	return 1;
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
		size_t position;
		if (!pg_data_constructor_position(layout, clauses[i].constructor, &position) || !clauses[i].branch) goto done;
		if (branches[position]) goto done;
		branches[position] = clauses[i].branch;
	}
	result = pg_application(graph, pg_reference(graph, &layout->matcher), scrutinee);
	for (size_t i = 0; i < count; ++i) result = pg_application(graph, result, branches[i]);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_term *pg_data_recursive_match(struct pg_graph *graph,
	const struct pg_data_layout *layout, const struct pg_object *recursion,
	const struct pg_term *scrutinee, size_t count, const struct pg_match_clause *clauses)
{
	if (!graph || !recursion || recursion->kind != PG_BINDER || !scrutinee) return NULL;
	const struct pg_object *argument = pg_binder(graph), *self = pg_binder(graph);
	const struct pg_term *body = pg_data_match(graph, layout,
		pg_reference(graph, argument), count, clauses);
	if (!body) return NULL;
	const struct pg_term *function = pg_lambda(graph, recursion, pg_lambda(graph, argument, body));
	const struct pg_term *reference = pg_reference(graph, self);
	const struct pg_term *unfold = pg_lambda(graph, self,
		pg_application(graph, function, pg_application(graph, reference, reference)));
	return pg_application(graph, pg_application(graph, unfold, unfold), scrutinee);
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
	return pg_data_layout_view(term->as.reference);
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
	const struct pg_evidence *parameters;
	const struct pg_evidence *indices;
};

struct pg_data_declaration {
	struct pg_object family;
	const struct pg_data_layout *layout;
	const struct pg_context *parameters, *indices;
	size_t image_count;
	struct pg_data_constructor_input constructors[];
};

struct pg_data_schema {
	const struct pg_data_declaration *declaration;
	const struct pg_data_signature *signature;
	const struct pg_evidence *results[];
};

static const struct pg_data_declaration *declaration_build(struct pg_graph *graph,
	const struct pg_data_layout *layout,
	const struct pg_context *parameters, const struct pg_context *indices,
	size_t count, const struct pg_data_constructor_input *constructors)
{
	if (!graph || (count && !constructors)) return NULL;
	if (layout && layout->count != count) return NULL;
	size_t suffix, images;
	if (pg_context_extension_size(indices, parameters, &suffix)
		|| pg_context_extension_size(indices, NULL, &images)) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_data_declaration)) / sizeof(*constructors)) return NULL;
	if (images > SIZE_MAX / sizeof(const struct pg_term *)) return NULL;
	struct pg_graph temporary = {0};
	size_t *arities = pg_alloc(&temporary, count * sizeof(*arities));
	struct pg_data_declaration *declaration = NULL;
	if (!arities) goto done;
	for (size_t i = 0; i < count; ++i) {
		if (pg_context_extension_size(constructors[i].fields, parameters, &arities[i])) goto done;
		if (layout && layout->constructors[i].arity != arities[i]) goto done;
		if (images && !constructors[i].images) goto done;
		for (size_t j = 0; j < images; ++j) if (!constructors[i].images[j]) goto done;
	}
	if (!layout) layout = pg_data_layout(graph, count, arities);
	if (!layout) goto done;
	declaration = pg_alloc(graph, sizeof(*declaration) + count * sizeof(*constructors));
	if (!declaration) goto done;
	declaration->family = (struct pg_object){PG_SEMANTIC_OBJECT, &family_class};
	declaration->layout = layout;
	declaration->parameters = parameters;
	declaration->indices = indices;
	declaration->image_count = images;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_term **copy = pg_alloc(graph, images * sizeof(*copy));
		if (!copy) { declaration = NULL; goto done; }
		for (size_t j = 0; j < images; ++j) copy[j] = constructors[i].images[j];
		declaration->constructors[i] = (struct pg_data_constructor_input){constructors[i].fields, copy};
	}
done:
	pg_graph_destroy(&temporary);
	return declaration;
}

const struct pg_data_declaration *pg_data_declaration(struct pg_graph *graph,
	const struct pg_context *parameters, const struct pg_context *indices,
	size_t count, const struct pg_data_constructor_input *constructors)
{
	return declaration_build(graph, NULL, parameters, indices, count, constructors);
}

const struct pg_data_declaration *pg_data_declaration_at_layout(struct pg_graph *graph,
	const struct pg_data_layout *layout, const struct pg_context *parameters,
	const struct pg_context *indices, size_t count,
	const struct pg_data_constructor_input *constructors)
{
	return layout ? declaration_build(graph, layout, parameters, indices, count, constructors) : NULL;
}

const struct pg_data_layout *pg_data_declaration_layout(const struct pg_data_declaration *declaration)
{
	return declaration ? declaration->layout : NULL;
}

const struct pg_object *pg_data_declaration_family(const struct pg_data_declaration *declaration)
{
	return declaration ? &declaration->family : NULL;
}

const struct pg_data_declaration *pg_data_schema_declaration(const struct pg_data_schema *schema)
{
	return schema ? schema->declaration : NULL;
}

int pg_data_declaration_pack(const struct pg_data_declaration *declaration,
	struct pg_graph *storage, size_t *context_count,
	const struct pg_context *const **contexts, size_t *term_count,
	const struct pg_term *const **terms)
{
	if (!declaration || !storage || !context_count || !contexts || !term_count || !terms) return -1;
	size_t count = declaration->layout->count, images = declaration->image_count;
	if (count > SIZE_MAX / sizeof(void *) - 2) return -1;
	if (images && count > (SIZE_MAX / sizeof(void *) - 1) / images) return -1;
	size_t total = 1 + count * images;
	const struct pg_context **scopes = pg_alloc(storage, (count + 2) * sizeof(*scopes));
	const struct pg_term **roots = pg_alloc(storage, total * sizeof(*roots));
	if (!scopes || !roots) return -1;
	scopes[0] = declaration->parameters;
	scopes[1] = declaration->indices;
	roots[0] = pg_reference(storage, pg_data_matcher(declaration->layout));
	if (!roots[0]) return -1;
	for (size_t i = 0; i < count; ++i) {
		scopes[i + 2] = declaration->constructors[i].fields;
		for (size_t j = 0; j < images; ++j) roots[1 + i * images + j] = declaration->constructors[i].images[j];
	}
	*context_count = count + 2;
	*contexts = scopes;
	*term_count = total;
	*terms = roots;
	return 0;
}

const struct pg_data_declaration *pg_data_declaration_unpack(struct pg_graph *graph,
	size_t context_count, const struct pg_context *const *contexts,
	size_t term_count, const struct pg_term *const *terms)
{
	if (!graph || context_count < 2 || !contexts || !term_count || !terms || !terms[0]) return NULL;
	if (terms[0]->kind != PG_REFERENCE) return NULL;
	const struct pg_data_layout *layout = pg_data_layout_view(terms[0]->as.reference);
	size_t count = context_count - 2, images;
	if (!layout || layout->count != count || pg_context_extension_size(contexts[1], NULL, &images)) return NULL;
	if (images && count > (SIZE_MAX - 1) / images) return NULL;
	if (term_count != 1 + count * images || count > SIZE_MAX / sizeof(struct pg_data_constructor_input)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_data_constructor_input *inputs = pg_alloc(&temporary, count * sizeof(*inputs));
	if (!inputs) return NULL;
	for (size_t i = 0; i < count; ++i)
		inputs[i] = (struct pg_data_constructor_input){contexts[i + 2], terms + 1 + i * images};
	const struct pg_data_declaration *result = pg_data_declaration_at_layout(graph, layout,
		contexts[0], contexts[1], count, inputs);
	pg_graph_destroy(&temporary);
	return result;
}

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
	*signature = (struct pg_data_signature){parameters, indices};
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

static const struct pg_data_schema *schema_build(struct pg_typing *typing,
	const struct pg_data_declaration *declaration,
	const struct pg_data_signature *signature,
	size_t count, const struct pg_evidence *const *results)
{
	if (!signature || !pg_evidence_owned_by(signature->parameters, typing) || (count && !results)) return NULL;
	const struct pg_evidence *parameters = signature->parameters, *indices = signature->indices;
	const struct pg_context *prefix = pg_evidence_context(parameters);
	if (declaration && (declaration->parameters != prefix
		|| declaration->indices != pg_evidence_context(indices) || declaration->layout->count != count)) return NULL;
	size_t parameter_count;
	if (pg_context_extension_size(prefix, NULL, &parameter_count) != 0) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_data_schema)) / sizeof(*results)) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_data_constructor_input)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_data_schema *schema = NULL;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *result = results[i];
		if (!pg_evidence_owned_by(result, typing)) goto done;
		if (pg_evidence_rule(result) != PG_CONTEXT_SUBSTITUTION) goto done;
		if (pg_evidence_context(pg_evidence_premise(result, 0)) != pg_evidence_context(indices)) goto done;
		size_t fields;
		if (pg_context_extension_size(pg_evidence_context(result), prefix, &fields) != 0) goto done;
		if (declaration) {
			const struct pg_data_constructor_input *input = &declaration->constructors[i];
			if (input->fields != pg_evidence_context(result)) goto done;
			if (pg_evidence_premise_count(result) != declaration->image_count + 2) goto done;
			for (size_t j = 0; j < declaration->image_count; ++j)
				if (input->images[j] != pg_evidence_subject(pg_evidence_premise(result, j + 2))->core) goto done;
		}
		const struct pg_context *parameter = prefix;
		for (size_t j = parameter_count; j; --j, parameter = parameter->parent) {
			const struct pg_term *image = pg_evidence_subject(pg_evidence_premise(result, j + 1))->core;
			if (image->kind != PG_REFERENCE || image->as.reference != parameter->binder) goto done;
		}
	}
	if (!declaration) {
		struct pg_data_constructor_input *inputs = pg_alloc(&temporary, count * sizeof(*inputs));
		if (!inputs) goto done;
		for (size_t i = 0; i < count; ++i) {
			size_t images = pg_evidence_premise_count(results[i]) - 2;
			const struct pg_term **terms = pg_alloc(&temporary, images * sizeof(*terms));
			if (!terms) goto done;
			for (size_t j = 0; j < images; ++j) terms[j] = pg_evidence_subject(pg_evidence_premise(results[i], j + 2))->core;
			inputs[i] = (struct pg_data_constructor_input){pg_evidence_context(results[i]), terms};
		}
		declaration = pg_data_declaration(typing->graph, prefix, pg_evidence_context(indices), count, inputs);
		if (!declaration) goto done;
	}
	schema = pg_alloc(typing->graph, sizeof(*schema) + count * sizeof(*results));
	if (!schema) goto done;
	schema->signature = signature;
	schema->declaration = declaration;
	for (size_t i = 0; i < count; ++i) schema->results[i] = results[i];
done:
	pg_graph_destroy(&temporary);
	return schema;
}

const struct pg_data_schema *pg_data_schema(struct pg_typing *typing,
	const struct pg_data_signature *signature, size_t count, const struct pg_evidence *const *results)
{
	return schema_build(typing, NULL, signature, count, results);
}

const struct pg_data_schema *pg_data_schema_check(struct pg_typing *typing,
	const struct pg_data_declaration *declaration, const struct pg_data_signature *signature,
	size_t count, const struct pg_evidence *const *results)
{
	return declaration ? schema_build(typing, declaration, signature, count, results) : NULL;
}

const struct pg_data_layout *pg_data_schema_layout(const struct pg_data_schema *schema)
{
	return pg_data_declaration_layout(pg_data_schema_declaration(schema));
}

const struct pg_object *pg_data_family_object(const struct pg_data_schema *schema)
{
	return pg_data_declaration_family(pg_data_schema_declaration(schema));
}

size_t pg_data_constructor_count(const struct pg_data_schema *schema)
{
	return schema ? schema->declaration->layout->count : 0;
}

const struct pg_evidence *pg_data_schema_parameters(const struct pg_data_schema *schema)
{
	return schema ? schema->signature->parameters : NULL;
}

static int schema_fields_check(const struct pg_data_schema *schema,
	int (*check)(const struct pg_evidence *, void *), void *state)
{
	const struct pg_context *prefix = pg_evidence_context(schema->signature->parameters);
	struct pg_dag checked;
	if (pg_dag_init(&checked, NULL, NULL)) return -1;
	int result = 1;
	for (size_t i = 0; i < pg_data_constructor_count(schema); ++i) {
		const struct pg_evidence *fields = pg_evidence_premise(schema->results[i], 1);
		for (; pg_evidence_context(fields) != prefix; fields = pg_evidence_premise(fields, 0)) {
			if (pg_dag_find(&checked, fields)) break;
			result = check(pg_evidence_premise(fields, 1), state);
			if (result != 1) goto done;
			if (pg_dag_add(&checked, fields)) { result = -1; goto done; }
		}
	}
done:
	pg_dag_destroy(&checked);
	return result;
}

struct positivity_check { const struct pg_object *self; size_t indices; };

static int field_positive(const struct pg_evidence *formation, void *state)
{
	struct positivity_check *check = state;
	return pg_data_field_positive(pg_evidence_subject(formation)->core, check->self, check->indices);
}

int pg_data_schema_positive(const struct pg_data_schema *schema,
	const struct pg_object *self)
{
	if (!schema || !self || self->kind != PG_BINDER) return -1;
	struct positivity_check check = {self, 0};
	if (pg_context_extension_size(pg_evidence_context(schema->signature->indices),
		pg_evidence_context(schema->signature->parameters), &check.indices)) return -1;
	return schema_fields_check(schema, field_positive, &check);
}

static int field_level(const struct pg_evidence *formation, void *state)
{
	uint64_t *bound = state, current;
	if (!pg_universe_level(pg_evidence_classifier(formation), &current)) return -1;
	if (current > *bound) *bound = current;
	return 1;
}

int pg_data_schema_field_level(const struct pg_data_schema *schema, uint64_t *level)
{
	if (!schema || !level) return -1;
	uint64_t bound = 0;
	if (schema_fields_check(schema, field_level, &bound) != 1) return -1;
	*level = bound;
	return 0;
}

const struct pg_evidence *pg_data_schema_indices(const struct pg_data_schema *schema)
{
	return schema ? schema->signature->indices : NULL;
}

const struct pg_evidence *pg_data_schema_result(const struct pg_data_schema *schema,
	const struct pg_object *object)
{
	if (!schema) return NULL;
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	const struct pg_constructor *c = constructor(object, layout);
	return c ? schema->results[c - layout->constructors] : NULL;
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
	if (!signature || !pg_evidence_owned_by(signature->parameters, typing)) return NULL;
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
