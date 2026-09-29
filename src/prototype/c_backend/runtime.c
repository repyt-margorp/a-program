#include "runtime.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "A Program host byte contract requires 8-bit bytes");

struct ap_allocation {
	struct ap_allocation *next;
	max_align_t alignment;
};
struct ap_env {
	const struct ap_env *parent;
	size_t binder;
	struct ap_value *values[3];
	int related, boundary;
	enum ap_projection projection;
};
enum ap_value_kind { AP_DELAYED, AP_FUNCTION, AP_PRIMITIVE, AP_RETURNED,
	AP_SUSPENDED, AP_REQUESTED, AP_DATA, AP_INTEGER, AP_TEXT, AP_RESUME, AP_ACTED, AP_FAMILY };
struct ap_value {
	enum ap_value_kind kind;
	const struct ap_descriptor *descriptor;
	size_t count;
	struct ap_value **arguments;
	union {
		struct {
			struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *, enum ap_projection);
			const struct ap_env *environment;
			size_t binder;
			enum ap_projection projection;
			struct ap_value *answer;
			int busy;
		} closure;
		struct { uint64_t bits; size_t width; } integer;
		struct { const unsigned char *bytes; size_t count; } text;
	} as;
};

static _Noreturn void fail(struct ap_runtime *runtime, const char *message, int status)
{
	fprintf(stderr, "A Program C runtime: %s\n", message);
	runtime->status = status;
	longjmp(runtime->failure, 1);
}

static void *allocate(struct ap_runtime *runtime, size_t size)
{
	if (size > SIZE_MAX - sizeof(struct ap_allocation)) fail(runtime, "allocation overflow", 2);
	struct ap_allocation *block = calloc(1, sizeof(*block) + size);
	if (!block) fail(runtime, "allocation failed", 2);
	block->next = runtime->allocations;
	runtime->allocations = block;
	return block + 1;
}

static struct ap_value *value(struct ap_runtime *runtime, enum ap_value_kind kind, size_t count)
{
	struct ap_value *result = allocate(runtime, sizeof(*result));
	result->kind = kind;
	result->count = count;
	if (count > SIZE_MAX / sizeof(*result->arguments)) fail(runtime, "arity overflow", 2);
	if (count) result->arguments = allocate(runtime, count * sizeof(*result->arguments));
	return result;
}

static struct ap_value *unary(struct ap_runtime *, enum ap_value_kind, struct ap_value *);

static struct ap_value *demand(struct ap_runtime *runtime, struct ap_value *input)
{
	/* Memoization is only of pure closure reduction, never host execution. */
	for (;;) {
		if (input->kind == AP_DELAYED) {
			if (!input->as.closure.answer) {
				if (input->as.closure.busy) fail(runtime, "cyclic demand", 4);
				input->as.closure.busy = 1;
				input->as.closure.answer = input->as.closure.body(runtime,
					input->as.closure.environment, input->as.closure.projection);
			}
			input = input->as.closure.answer;
			continue;
		}
		if (input->kind == AP_ACTED && input->count == 1) {
			struct ap_value *source = demand(runtime, input->arguments[0]);
			if (source->kind == AP_RETURNED || source->kind == AP_SUSPENDED)
				return unary(runtime, source->kind, ap_action(runtime, source->arguments[0]));
		}
		return input;
	}
}

struct ap_value *ap_delay(struct ap_runtime *runtime,
	struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *, enum ap_projection),
	const struct ap_env *environment, enum ap_projection projection)
{
	struct ap_value *result = value(runtime, AP_DELAYED, 0);
	result->as.closure.body = body;
	result->as.closure.environment = environment;
	result->as.closure.projection = projection;
	return result;
}

struct ap_value *ap_function(struct ap_runtime *runtime, size_t binder,
	struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *, enum ap_projection),
	const struct ap_env *environment, enum ap_projection projection)
{
	struct ap_value *result = ap_delay(runtime, body, environment, projection);
	result->kind = AP_FUNCTION;
	result->as.closure.binder = binder;
	return result;
}

struct ap_value *ap_lookup(struct ap_runtime *runtime, const struct ap_env *environment,
	size_t binder, enum ap_projection projection)
{
	int acted = 0;
	for (; environment; environment = environment->parent) {
		if (environment->boundary) {
			acted |= projection == AP_RELATION;
			projection = environment->projection;
			continue;
		}
		if (environment->binder != binder) continue;
		size_t side = environment->related && projection ? (size_t)projection - 1 : 0;
		struct ap_value *result = environment->values[side];
		if (acted || (projection == AP_RELATION && !environment->related)) return ap_action(runtime, result);
		return result;
	}
	fail(runtime, "free runtime binder", 4);
}

struct ap_value *ap_integer(struct ap_runtime *runtime, uint64_t bits, size_t width)
{
	struct ap_value *result = value(runtime, AP_INTEGER, 0);
	result->as.integer.bits = width == 4 ? bits & UINT32_MAX : bits;
	result->as.integer.width = width;
	return result;
}

struct ap_value *ap_text(struct ap_runtime *runtime, const unsigned char *bytes, size_t count)
{
	struct ap_value *result = value(runtime, AP_TEXT, 0);
	result->as.text.bytes = bytes;
	result->as.text.count = count;
	return result;
}

static struct ap_value *unary(struct ap_runtime *runtime, enum ap_value_kind kind, struct ap_value *input)
{
	struct ap_value *result = value(runtime, kind, 1);
	result->arguments[0] = input;
	return result;
}

struct ap_value *ap_action(struct ap_runtime *runtime, struct ap_value *input)
{
	/* Do not demand an unused path or execute a suspended computation. */
	return unary(runtime, AP_ACTED, input);
}

struct ap_value *ap_family(struct ap_runtime *runtime, enum ap_operation kind,
	struct ap_value *first, struct ap_value *second)
{
	static const struct ap_descriptor descriptors[] = {
		{.operation = AP_F_FAMILY}, {.operation = AP_U_FAMILY}, {.operation = AP_PI_FAMILY}
	};
	if (kind < AP_F_FAMILY || kind > AP_PI_FAMILY) fail(runtime, "invalid relation family", 4);
	struct ap_value *result = value(runtime, AP_FAMILY, 2);
	result->descriptor = &descriptors[kind - AP_F_FAMILY];
	result->arguments[0] = first; result->arguments[1] = second;
	return result;
}

static struct ap_value *append_argument(struct ap_runtime *runtime,
	struct ap_value *function, struct ap_value *argument)
{
	if (function->count == SIZE_MAX) fail(runtime, "arity overflow", 2);
	struct ap_value *result = value(runtime, function->kind, function->count + 1);
	result->descriptor = function->descriptor;
	result->as = function->as;
	for (size_t i = 0; i < function->count; ++i) result->arguments[i] = function->arguments[i];
	result->arguments[function->count] = argument;
	return result;
}

static struct ap_value *apply_action(struct ap_runtime *runtime,
	struct ap_value *action, struct ap_value *argument);
static struct ap_value *transport_thunk(struct ap_runtime *, struct ap_value *, struct ap_value *, size_t);

static struct ap_value *field(struct ap_runtime *runtime,
	struct ap_value *family, struct ap_value *input, size_t operation)
{
	family = demand(runtime, family);
	if (family->kind == AP_FAMILY && family->count == 2 && family->descriptor->operation == AP_U_FAMILY)
		return transport_thunk(runtime, family->arguments[0], input, operation);
	/* A bare reflexive action is the diagonal Universe identification. */
	if (family->kind == AP_ACTED && family->count == 1) {
		struct ap_value *source = demand(runtime, family->arguments[0]);
		if (source->kind == AP_PRIMITIVE && source->descriptor->operation == AP_MATCH)
			fail(runtime, "matcher action is not a diagonal family", 4);
		return operation < 2 ? input : ap_action(runtime, input);
	}
	fail(runtime, "unsupported Identity family field", 4);
}

static struct ap_value *force(struct ap_runtime *runtime, struct ap_value *input)
{
	input = demand(runtime, input);
	if (input->kind != AP_SUSPENDED) fail(runtime, "Force requires Thunk", 4);
	return demand(runtime, input->arguments[0]);
}

static struct ap_env *capture(struct ap_runtime *runtime, struct ap_value *a,
	struct ap_value *b, struct ap_value *c, size_t operation)
{
	struct ap_env *environment = allocate(runtime, sizeof(*environment));
	*environment = (struct ap_env){.values = {a, b, c}, .binder = operation};
	return environment;
}

static struct ap_value *mapped_field(struct ap_runtime *runtime, const struct ap_env *e, enum ap_projection p)
{
	(void)p;
	return field(runtime, e->values[0], e->values[1], e->binder);
}

static struct ap_value *delayed_field(struct ap_runtime *runtime, struct ap_value *family, struct ap_value *input, size_t operation)
{
	return ap_delay(runtime, mapped_field, capture(runtime, family, input, NULL, operation), AP_VALUE);
}

static struct ap_value *map_returned(struct ap_runtime *runtime, const struct ap_env *e, enum ap_projection p)
{
	(void)p;
	const struct ap_env *captured = e->parent;
	return unary(runtime, AP_RETURNED, delayed_field(runtime, captured->values[0], e->values[0], captured->binder));
}

static struct ap_value *fold(struct ap_runtime *, struct ap_value *, struct ap_value *);

static struct ap_value *map_computation(struct ap_runtime *runtime, const struct ap_env *e, enum ap_projection p)
{
	(void)p;
	static const struct ap_descriptor descriptor = {.operation = AP_FOLD, .arity = 2};
	struct ap_value *handler = value(runtime, AP_PRIMITIVE, 2);
	handler->descriptor = &descriptor;
	handler->arguments[1] = ap_function(runtime, 0, map_returned, e, AP_VALUE);
	return fold(runtime, force(runtime, e->values[1]), handler);
}

static struct ap_value *call_suspended(struct ap_runtime *runtime, const struct ap_env *e, enum ap_projection p)
{
	(void)p;
	return ap_apply(runtime, force(runtime, e->values[0]), e->values[1]);
}

static struct ap_value *map_function(struct ap_runtime *runtime, const struct ap_env *e, enum ap_projection p)
{
	(void)p;
	const struct ap_env *captured = e->parent;
	struct ap_value *family = captured->values[0], *target = e->values[0];
	size_t direction = captured->binder, reverse = 1 - direction;
	struct ap_value *input = delayed_field(runtime, family->arguments[0], target, reverse);
	struct ap_value *path = delayed_field(runtime, family->arguments[0], target, reverse + 2);
	struct ap_value *result_family = family->arguments[1];
	result_family = ap_apply(runtime, result_family, direction ? target : input);
	result_family = ap_apply(runtime, result_family, direction ? input : target);
	result_family = ap_apply(runtime, result_family, path);
	result_family = ap_family(runtime, AP_U_FAMILY, result_family, NULL);
	struct ap_value *call = ap_delay(runtime, call_suspended, capture(runtime, captured->values[1], input, NULL, 0), AP_VALUE);
	return force(runtime, field(runtime, result_family, unary(runtime, AP_SUSPENDED, call), direction));
}

static struct ap_value *transport_thunk(struct ap_runtime *runtime,
	struct ap_value *computation, struct ap_value *input, size_t operation)
{
	computation = demand(runtime, computation);
	if (computation->kind != AP_FAMILY || computation->count != 2)
		fail(runtime, "unsupported computation family", 4);
	if (operation >= 2) fail(runtime, "unsupported dependent thunk lift", 4);
	struct ap_env *environment;
	if (computation->descriptor->operation == AP_F_FAMILY) {
		environment = capture(runtime, computation->arguments[0], input, NULL, operation);
		return unary(runtime, AP_SUSPENDED, ap_delay(runtime, map_computation, environment, AP_VALUE));
	}
	if (computation->descriptor->operation == AP_PI_FAMILY) {
		environment = capture(runtime, computation, input, NULL, operation);
		return unary(runtime, AP_SUSPENDED, ap_function(runtime, 0, map_function, environment, AP_VALUE));
	}
	fail(runtime, "unsupported thunk family", 4);
}

static struct ap_value *action_argument(struct ap_runtime *runtime,
	struct ap_value *action, struct ap_value *source, size_t field, size_t side)
{
	if (field < source->count) {
		struct ap_value *item = source->arguments[field];
		return side == 2 ? ap_action(runtime, item) : item;
	}
	return action->arguments[1 + 3 * (field - source->count) + side];
}

/* A conservative target equality used only for the diagonal action equation.
 * Distinct closures are not equated by testing their output on sample inputs. */
static int same_value(struct ap_runtime *runtime, struct ap_value *left, struct ap_value *right)
{
	if (left == right) return 1;
	left = demand(runtime, left); right = demand(runtime, right);
	if (left == right) return 1;
	if (left->kind != right->kind || left->descriptor != right->descriptor || left->count != right->count) return 0;
	switch (left->kind) {
	case AP_INTEGER:
		return left->as.integer.width == right->as.integer.width && left->as.integer.bits == right->as.integer.bits;
	case AP_TEXT:
		return left->as.text.count == right->as.text.count && !memcmp(left->as.text.bytes, right->as.text.bytes, left->as.text.count);
	case AP_FUNCTION:
		if (left->as.closure.body != right->as.closure.body || left->as.closure.environment != right->as.closure.environment
			|| left->as.closure.binder != right->as.closure.binder || left->as.closure.projection != right->as.closure.projection) return 0;
		break;
	case AP_DATA: case AP_PRIMITIVE: case AP_ACTED: break;
	default: return 0;
	}
	for (size_t i = 0; i < left->count; ++i)
		if (!same_value(runtime, left->arguments[i], right->arguments[i])) return 0;
	return 1;
}

static struct ap_value *apply_action(struct ap_runtime *runtime,
	struct ap_value *action, struct ap_value *argument)
{
	struct ap_value *source = demand(runtime, action->arguments[0]);
	struct ap_value *result = append_argument(runtime, action, argument);
	if (source->kind == AP_FUNCTION && source->as.closure.projection != AP_RELATION) {
		if (result->count < 4) return result;
		struct ap_env *boundary = allocate(runtime, sizeof(*boundary));
		*boundary = (struct ap_env){.parent = source->as.closure.environment,
			.boundary = 1, .projection = source->as.closure.projection};
		struct ap_env *environment = allocate(runtime, sizeof(*environment));
		*environment = (struct ap_env){.parent = boundary, .binder = source->as.closure.binder,
			.values = {result->arguments[1], result->arguments[2], result->arguments[3]}, .related = 1};
		return source->as.closure.body(runtime, environment, AP_RELATION);
	}
	if (source->kind != AP_PRIMITIVE || source->descriptor->operation != AP_MATCH) {
		if (result->count == 4) {
			struct ap_value *path = demand(runtime, result->arguments[3]);
			if (path->kind == AP_ACTED && path->count == 1
				&& same_value(runtime, path->arguments[0], result->arguments[1])
				&& same_value(runtime, path->arguments[0], result->arguments[2]))
				return ap_action(runtime, ap_apply(runtime, source, result->arguments[1]));
		}
		return result;
	}
	size_t arity = source->descriptor->arity;
	if (arity - source->count > (SIZE_MAX - 1) / 3) fail(runtime, "action arity overflow", 2);
	if (result->count != 1 + 3 * (arity - source->count)) return result;
	struct ap_value *path = demand(runtime, action_argument(runtime, result, source, 0, 2));
	if (path->kind != AP_ACTED) fail(runtime, "Match action requires a constructor path", 4);
	struct ap_value *constructor = demand(runtime, path->arguments[0]);
	if (constructor->kind != AP_DATA && constructor->kind != AP_PRIMITIVE)
		fail(runtime, "invalid constructor path", 4);
	const struct ap_descriptor *d = constructor->descriptor;
	if (d->operation != AP_CONSTRUCTOR || d->family != source->descriptor->family)
		fail(runtime, "nominal path layout mismatch", 4);
	if ((path->count - 1) / 3 != d->arity - constructor->count || (path->count - 1) % 3)
		fail(runtime, "incomplete constructor path", 4);
	struct ap_value *branch = action_argument(runtime, result, source, 1 + d->position, 2);
	for (size_t i = 0; i < d->arity; ++i)
		for (size_t side = 0; side < 3; ++side)
			branch = ap_apply(runtime, branch, action_argument(runtime, path, constructor, i, side));
	return branch;
}

static struct ap_value *fold(struct ap_runtime *runtime, struct ap_value *input, struct ap_value *handler)
{
	input = demand(runtime, input);
	if (input->kind == AP_RETURNED)
		return ap_apply(runtime, handler->arguments[1], input->arguments[0]);
	if (input->kind != AP_REQUESTED) fail(runtime, "Fold requires Return or Request", 4);
	struct ap_value *label = demand(runtime, input->arguments[0]);
	if (label->kind != AP_PRIMITIVE || label->descriptor->operation != AP_LABEL)
		fail(runtime, "invalid operation label", 4);
	struct ap_value *resume = value(runtime, AP_RESUME, 2);
	resume->arguments[0] = input->arguments[2];
	resume->arguments[1] = handler;
	for (size_t i = 0; i < handler->descriptor->clause_count; ++i) {
		if (handler->descriptor->labels[i] != label->descriptor->family) continue;
		struct ap_value *clause = ap_apply(runtime, handler->arguments[2 + i], input->arguments[1]);
		return ap_apply(runtime, clause, unary(runtime, AP_SUSPENDED, resume));
	}
	struct ap_value *forwarded = value(runtime, AP_REQUESTED, 3);
	forwarded->arguments[0] = label;
	forwarded->arguments[1] = input->arguments[1];
	forwarded->arguments[2] = resume;
	return forwarded;
}

static struct ap_value *arithmetic(struct ap_runtime *runtime, struct ap_value *function)
{
	const struct ap_descriptor *descriptor = function->descriptor;
	uint64_t arguments[2] = {0};
	for (size_t i = 0; i < function->count; ++i) {
		struct ap_value *argument = demand(runtime, function->arguments[i]);
		if (argument->kind != AP_INTEGER || argument->as.integer.width != descriptor->width)
			fail(runtime, "integer domain mismatch", 4);
		arguments[i] = argument->as.integer.bits;
	}
	uint64_t result = 0;
	switch (descriptor->operation) {
	case AP_ADD: result = arguments[0] + arguments[1]; break;
	case AP_SUBTRACT: result = arguments[0] - arguments[1]; break;
	case AP_MULTIPLY: result = arguments[0] * arguments[1]; break;
	case AP_NEGATE: result = UINT64_C(0) - arguments[0]; break;
	case AP_DECIMAL: {
		int negative = (arguments[0] >> (descriptor->width * 8 - 1)) != 0;
		uint64_t mask = descriptor->width == 4 ? UINT32_MAX : UINT64_MAX;
		uint64_t magnitude = negative ? (UINT64_C(0) - arguments[0]) & mask : arguments[0];
		unsigned char *bytes = allocate(runtime, 20);
		size_t start = 20;
		do { bytes[--start] = (unsigned char)(0x30 + magnitude % 10); magnitude /= 10; } while (magnitude);
		if (negative) bytes[--start] = 0x2d;
		return unary(runtime, AP_RETURNED, ap_text(runtime, bytes + start, 20 - start));
	}
	default: fail(runtime, "invalid arithmetic operation", 4);
	}
	return unary(runtime, AP_RETURNED, ap_integer(runtime, result, descriptor->width));
}

static struct ap_value *invoke(struct ap_runtime *runtime, struct ap_value *function)
{
	const struct ap_descriptor *descriptor = function->descriptor;
	struct ap_value **arguments = function->arguments;
	switch (descriptor->operation) {
	case AP_RETURN: return unary(runtime, AP_RETURNED, arguments[0]);
	case AP_THUNK: return unary(runtime, AP_SUSPENDED, arguments[0]);
	case AP_FORCE: {
		return force(runtime, arguments[0]);
	}
	case AP_TOTAL_RESULT: {
		struct ap_value *computation = demand(runtime, arguments[0]);
		if (computation->kind != AP_RETURNED) fail(runtime, "total result did not return", 4);
		return computation->arguments[0];
	}
	case AP_FOLD: return fold(runtime, arguments[0], function);
	case AP_REQUEST: function->kind = AP_REQUESTED; return function;
	case AP_CONSTRUCTOR: function->kind = AP_DATA; return function;
	case AP_MATCH: {
		struct ap_value *data = demand(runtime, arguments[0]);
		if (data->kind != AP_DATA) fail(runtime, "Match requires a constructor", 4);
		if (data->descriptor->family != descriptor->family) fail(runtime, "nominal layout mismatch", 4);
		struct ap_value *branch = arguments[1 + data->descriptor->position];
		for (size_t i = 0; i < data->count; ++i) branch = ap_apply(runtime, branch, data->arguments[i]);
		return branch;
	}
	case AP_LABEL: case AP_ATOM: return function;
	case AP_ID_ACTION: return ap_action(runtime, arguments[0]);
	case AP_ID_FIELD: return field(runtime, arguments[0], arguments[1], descriptor->position);
	default: return arithmetic(runtime, function);
	}
}

struct ap_value *ap_primitive(struct ap_runtime *runtime, const struct ap_descriptor *descriptor)
{
	struct ap_value *result = value(runtime, AP_PRIMITIVE, 0);
	result->descriptor = descriptor;
	return descriptor->arity ? result : invoke(runtime, result);
}

struct ap_value *ap_apply(struct ap_runtime *runtime, struct ap_value *function, struct ap_value *argument)
{
	function = demand(runtime, function);
	if (function->kind == AP_FAMILY) {
		struct ap_value *result = append_argument(runtime, function, argument);
		if (result->count == 4 && result->descriptor->operation == AP_F_FAMILY) {
			struct ap_value *left = demand(runtime, result->arguments[2]);
			struct ap_value *right = demand(runtime, result->arguments[3]);
			if (left->kind != AP_RETURNED || right->kind != AP_RETURNED)
				fail(runtime, "F relation requires returned endpoints", 4);
			return ap_apply(runtime, ap_apply(runtime, result->arguments[0], left->arguments[0]), right->arguments[0]);
		}
		return result;
	}
	if (function->kind == AP_ACTED) return apply_action(runtime, function, argument);
	if (function->kind == AP_FUNCTION) {
		if (function->as.closure.projection == AP_RELATION) {
			function = append_argument(runtime, function, argument);
			if (function->count < 3) return function;
			struct ap_env *environment = allocate(runtime, sizeof(*environment));
			*environment = (struct ap_env){.parent = function->as.closure.environment,
				.binder = function->as.closure.binder,
				.values = {function->arguments[0], function->arguments[1], function->arguments[2]}, .related = 1};
			return function->as.closure.body(runtime, environment, AP_RELATION);
		}
		struct ap_env *environment = allocate(runtime, sizeof(*environment));
		*environment = (struct ap_env){.parent = function->as.closure.environment,
			.binder = function->as.closure.binder, .values = {argument}};
		return function->as.closure.body(runtime, environment, function->as.closure.projection);
	}
	if (function->kind == AP_RESUME)
		return fold(runtime, ap_apply(runtime, function->arguments[0], argument), function->arguments[1]);
	if (function->kind != AP_PRIMITIVE) fail(runtime, "application requires a function", 4);
	int neutral = function->descriptor->operation == AP_ATOM;
	if (!neutral && function->count >= function->descriptor->arity) fail(runtime, "operation is not callable", 4);
	struct ap_value *result = append_argument(runtime, function, argument);
	return !neutral && result->count == result->descriptor->arity ? invoke(runtime, result) : result;
}

void ap_run(struct ap_runtime *runtime, struct ap_value *root, int entry_mode)
{
	root = demand(runtime, root);
	if (entry_mode == 1) {
		if (root->kind != AP_SUSPENDED) fail(runtime, "entry is not a thunk", 4);
		root = root->arguments[0];
	} else if (entry_mode == 2) return;
	for (;;) {
		root = demand(runtime, root);
		if (root->kind == AP_RETURNED) { (void)demand(runtime, root->arguments[0]); return; }
		if (root->kind != AP_REQUESTED) fail(runtime, "entry did not return a computation", 4);
		struct ap_value *label = demand(runtime, root->arguments[0]);
		if (label->kind != AP_PRIMITIVE || !label->descriptor->host_print)
			fail(runtime, "unhandled operation", 4);
		struct ap_value *payload = demand(runtime, root->arguments[1]);
		if (payload->kind != AP_TEXT) fail(runtime, "print requires Text", 4);
		if (fwrite(payload->as.text.bytes, 1, payload->as.text.count, stdout) != payload->as.text.count)
			fail(runtime, "output error (not retried)", 2);
		if (fflush(stdout)) fail(runtime, "output error (not retried)", 2);
		root = ap_apply(runtime, root->arguments[2], payload);
	}
}

void ap_destroy(struct ap_runtime *runtime)
{
	while (runtime->allocations) {
		struct ap_allocation *block = runtime->allocations;
		runtime->allocations = block->next;
		free(block);
	}
}
