#include "host.h"
#include "computation.h"

#include <string.h>

static const struct pg_object_class type_class = {"host-type"};
static const struct pg_object_class literal_class = {"host-literal"};
static const struct pg_object int32_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct pg_object int64_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct pg_object text_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct {
	const struct pg_object *type;
	const char *source, *descriptor;
} types[] = {
	{&int32_type, "Int32", "host/int32/v1"},
	{&int64_type, "Int64", "host/int64/v1"},
	{&text_type, "Text", "host/text-bytes/v1"}
};

struct host_literal {
	struct pg_object_entry base;
	const struct pg_object *type;
	size_t count;
	unsigned char bytes[];
};

static const struct pg_object_class print_class = {"host-print"};
struct host_print {
	struct pg_object_entry base;
	const struct pg_term *text;
};

const struct pg_object *pg_host_print(struct pg_graph *graph)
{
	if (!graph) return NULL;
	const struct pg_term *text = pg_reference(graph, &text_type);
	if (!text) return NULL;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = (uintptr_t)&print_class;
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		const struct pg_object_entry *base = (const void *)p;
		if (base->object.owner == &print_class) return &base->object;
	}
	struct host_print *label = pg_alloc(graph, sizeof(*label));
	if (!label) return NULL;
	label->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &print_class};
	label->text = text;
	return pg_index_insert(&graph->objects, &label->base.index, hash) ? NULL : &label->base.object;
}

const char *pg_host_operation_descriptor(const struct pg_object *label)
{
	return label && label->owner == &print_class ? "host/print-text/v1" : NULL;
}

int pg_host_operation_types(const struct pg_object *object,
	const struct pg_term **payload, const struct pg_term **response)
{
	if (!pg_host_operation_descriptor(object) || !payload || !response) return 0;
	const struct host_print *label = (const void *)((const char *)object - offsetof(struct pg_object_entry, object));
	*payload = *response = label->text;
	return 1;
}

const struct pg_object *pg_host_type(const char *name)
{
	if (!name) return NULL;
	if (!strcmp(name, "Int")) return &int32_type;
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (!strcmp(name, types[i].source)) return types[i].type;
	return NULL;
}

const char *pg_host_type_name(const struct pg_object *type)
{
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (type == types[i].type) return types[i].descriptor;
	return NULL;
}

const struct pg_object *pg_host_type_resolve(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (!strcmp(name, types[i].descriptor)) return types[i].type;
	return NULL;
}

const struct pg_object *pg_host_literal(struct pg_graph *graph,
	const struct pg_object *type, size_t count, const unsigned char *bytes)
{
	if (!graph || !pg_host_type_name(type) || (count && !bytes)) return NULL;
	if (type == &int32_type && count != 4) return NULL;
	if (type == &int64_type && count != 8) return NULL;
	if (count > SIZE_MAX - sizeof(struct host_literal)) return NULL;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = (uintptr_t)type ^ count;
	for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct pg_object_entry *base = (const void *)p;
		if (base->object.owner != &literal_class) continue;
		const struct host_literal *value = (const void *)p;
		if (value->type == type && value->count == count && (!count || !memcmp(value->bytes, bytes, count)))
			return &value->base.object;
	}
	struct host_literal *value = pg_alloc(graph, sizeof(*value) + count);
	if (!value) return NULL;
	value->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &literal_class};
	value->type = type;
	value->count = count;
	if (count) memcpy(value->bytes, bytes, count);
	return pg_index_insert(&graph->objects, &value->base.index, hash) ? NULL : &value->base.object;
}

int pg_host_literal_view(const struct pg_object *object,
	const struct pg_object **type, size_t *count, const unsigned char **bytes)
{
	if (!object || object->owner != &literal_class || !type || !count || !bytes) return 0;
	const struct host_literal *value = (const void *)((const char *)object - offsetof(struct pg_object_entry, object));
	*type = value->type; *count = value->count; *bytes = value->bytes;
	return 1;
}

const struct pg_object *pg_host_integer(struct pg_graph *graph,
	const struct pg_object *type, int64_t value)
{
	size_t count;
	if (type == &int32_type) {
		if (value < INT32_MIN || value > INT32_MAX) return NULL;
		count = 4;
	} else if (type == &int64_type) count = 8;
	else return NULL;
	unsigned char bytes[8];
	uint64_t bits = (uint64_t)value;
	for (size_t i = count; i; --i, bits >>= 8) bytes[i - 1] = (unsigned char)(bits & 255);
	return pg_host_literal(graph, type, count, bytes);
}

int pg_host_integer_view(const struct pg_object *object, int64_t *value)
{
	const struct pg_object *type;
	size_t count;
	const unsigned char *bytes;
	if (!value || !pg_host_literal_view(object, &type, &count, &bytes)) return 0;
	if (type != &int32_type && type != &int64_type) return 0;
	uint64_t bits = 0;
	for (size_t i = 0; i < count; ++i) bits = (bits << 8) | bytes[i];
	uint64_t maximum = count == 4 ? UINT32_MAX : UINT64_MAX;
	*value = bytes[0] & 128 ? -1 - (int64_t)(maximum - bits) : (int64_t)bits;
	return 1;
}

enum host_operation { ADD, SUBTRACT, MULTIPLY, NEGATE, DECIMAL };
static const struct pg_object_class function_class = {"host-function"};
static const struct host_function {
	struct pg_object object;
	const char *name, *descriptor;
	const struct pg_object *domain, *result;
	size_t arity;
	enum host_operation operation;
} functions[] = {
	{{PG_SEMANTIC_OBJECT, &function_class}, "int_add", "host/int32/add/v1", &int32_type, &int32_type, 2, ADD},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int_sub", "host/int32/sub/v1", &int32_type, &int32_type, 2, SUBTRACT},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int_mul", "host/int32/mul/v1", &int32_type, &int32_type, 2, MULTIPLY},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int_neg", "host/int32/neg/v1", &int32_type, &int32_type, 1, NEGATE},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int64_add", "host/int64/add/v1", &int64_type, &int64_type, 2, ADD},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int64_sub", "host/int64/sub/v1", &int64_type, &int64_type, 2, SUBTRACT},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int64_mul", "host/int64/mul/v1", &int64_type, &int64_type, 2, MULTIPLY},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int64_neg", "host/int64/neg/v1", &int64_type, &int64_type, 1, NEGATE},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int_to_text", "host/int32/decimal-ascii/v1", &int32_type, &text_type, 1, DECIMAL},
	{{PG_SEMANTIC_OBJECT, &function_class}, "int64_to_text", "host/int64/decimal-ascii/v1", &int64_type, &text_type, 1, DECIMAL}
};

const struct pg_object *pg_host_function(size_t index)
{
	return index < sizeof(functions) / sizeof(*functions) ? &functions[index].object : NULL;
}

static const struct host_function *function_view(const struct pg_object *object)
{
	for (size_t i = 0; i < sizeof(functions) / sizeof(*functions); ++i)
		if (object == &functions[i].object) return &functions[i];
	return NULL;
}

const char *pg_host_function_name(const struct pg_object *object)
{
	const struct host_function *function = function_view(object);
	return function ? function->name : NULL;
}

const char *pg_host_function_descriptor(const struct pg_object *object)
{
	const struct host_function *function = function_view(object);
	return function ? function->descriptor : NULL;
}

const struct pg_object *pg_host_function_resolve(const char *descriptor)
{
	if (!descriptor) return NULL;
	for (size_t i = 0; i < sizeof(functions) / sizeof(*functions); ++i)
		if (!strcmp(descriptor, functions[i].descriptor)) return &functions[i].object;
	return NULL;
}

int pg_host_function_view(const struct pg_object *object,
	const struct pg_object **domain, const struct pg_object **result, size_t *arity)
{
	const struct host_function *function = function_view(object);
	if (!function || !domain || !result || !arity) return 0;
	*domain = function->domain;
	*result = function->result;
	*arity = function->arity;
	return 1;
}

/* A demanded answer replaces its operand in the caller. No private invocation
 * state is needed, so suspended evaluation uses the ordinary frame codec. */
static int operand_answer(struct pg_eval *machine, const struct pg_term *answer, size_t index)
{
	const struct host_function *function = function_view(machine->current.term->as.reference);
	const struct pg_object *type;
	size_t length;
	const unsigned char *bytes;
	if (!function || answer->kind != PG_REFERENCE
		|| !pg_host_literal_view(answer->as.reference, &type, &length, &bytes)
		|| type != function->domain) return 1;
	const struct pg_term *head = machine->current.term;
	if (index) head = pg_application(machine->output, head, pg_eval_argument(machine, 0)->term);
	return head ? pg_eval_apply(machine, (struct pg_closure){head, NULL},
		(struct pg_closure){answer, NULL}, index + 1) : -1;
}

static int first_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	return operand_answer(machine, answer, 0);
}

static int second_answer(struct pg_eval *machine, const struct pg_term *answer, const void *state)
{
	(void)state;
	return operand_answer(machine, answer, 1);
}

static const struct pg_eval_continuation first_operand = {"host/first-operand/v1", first_answer};
static const struct pg_eval_continuation second_operand = {"host/second-operand/v1", second_answer};

const struct pg_eval_continuation *pg_host_continuation_resolve(const char *name)
{
	const struct pg_eval_continuation *entries[] = {&first_operand, &second_operand};
	return pg_eval_continuation_find(name, 2, entries);
}

static const struct pg_object *decimal(struct pg_graph *graph, uint64_t bits, size_t width)
{
	int negative = (bits >> (width * 8 - 1)) != 0;
	uint64_t magnitude = negative ? (UINT64_C(0) - bits) & (width == 4 ? UINT32_MAX : UINT64_MAX) : bits;
	/* At most 19 digits and a sign, including Int64's minimum. Unsigned
	 * magnitude avoids negating that minimum in a signed machine type. */
	unsigned char bytes[20];
	size_t start = sizeof(bytes);
	do {
		bytes[--start] = (unsigned char)(0x30 + magnitude % 10);
		magnitude /= 10;
	} while (magnitude);
	if (negative) bytes[--start] = 0x2d;
	return pg_host_literal(graph, &text_type, sizeof(bytes) - start, bytes + start);
}

static const struct pg_object *calculate(struct pg_graph *graph, const struct host_function *function,
	size_t width, const uint64_t *arguments)
{
	uint64_t result;
	switch (function->operation) {
	case ADD: result = arguments[0] + arguments[1]; break;
	case SUBTRACT: result = arguments[0] - arguments[1]; break;
	case MULTIPLY: result = arguments[0] * arguments[1]; break;
	case NEGATE: result = UINT64_C(0) - arguments[0]; break;
	case DECIMAL: return decimal(graph, arguments[0], width);
	default: return NULL;
	}
	unsigned char bytes[8];
	for (size_t i = width; i; --i, result >>= 8) bytes[i - 1] = (unsigned char)(result & 255);
	return pg_host_literal(graph, function->result, width, bytes);
}

int pg_host_dispatch(struct pg_eval *machine)
{
	const struct host_function *function = function_view(machine->current.term->as.reference);
	if (!function) return 1;
	size_t arity = function->arity;
	if (!pg_eval_argument(machine, arity - 1)) return 1;
	uint64_t arguments[2] = {0};
	size_t width = 0;
	for (size_t i = 0; i < arity; ++i) {
		const struct pg_term *term = pg_eval_argument(machine, i)->term;
		const struct pg_object *type;
		const unsigned char *bytes;
		if (term->kind != PG_REFERENCE || !pg_host_literal_view(term->as.reference, &type, &width, &bytes))
			return pg_eval_demand(machine, i, i ? &second_operand : &first_operand, NULL);
		if (type != function->domain) return 1;
		for (size_t j = 0; j < width; ++j) arguments[i] = (arguments[i] << 8) | bytes[j];
	}
	const struct pg_object *value = calculate(machine->output, function, width, arguments);
	const struct pg_term *returned = value ? pg_application(machine->output,
		pg_reference(machine->output, &pg_return_operation), pg_reference(machine->output, value)) : NULL;
	return returned ? pg_eval_enter(machine, (struct pg_closure){returned, NULL}, arity) : -1;
}
