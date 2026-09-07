#include "iadt.h"

static const struct pg_object_class constructor_class = {"constructor"};
static const struct pg_object_class match_class = {"match"};

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

static int match_answer(struct pg_eval *machine, const struct pg_term *answer)
{
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
	const struct pg_object *k = pg_binder(machine->output);
	const struct pg_term *body = pg_reference(machine->output, k);
	for (size_t i = 0; i < count; ++i) body = pg_application(machine->output, body, fields[i]);
	size_t position = (size_t)(c - layout->constructors);
	struct pg_closure branch = *pg_eval_argument(machine, position + 1);
	/* The administrative lambda preserves the chosen branch's environment;
	 * fields have already been materialized by the shared demand machinery. */
	return pg_eval_apply(machine, (struct pg_closure){pg_lambda(machine->output, k, body), NULL},
		branch, layout->count + 1);
}

int pg_data_dispatch(struct pg_eval *machine)
{
	const struct pg_object *object = machine->current.term->as.reference;
	if (object->kind != PG_SEMANTIC_OBJECT || object->owner != &match_class) return 1;
	const struct pg_data_layout *layout = (const struct pg_data_layout *)object;
	if (!pg_eval_argument(machine, layout->count)) return 1;
	return pg_eval_demand(machine, 0, match_answer);
}
