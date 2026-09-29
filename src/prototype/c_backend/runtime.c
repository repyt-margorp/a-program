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
	struct ap_value *value;
};
enum ap_value_kind { AP_DELAYED, AP_FUNCTION, AP_PRIMITIVE, AP_RETURNED,
	AP_SUSPENDED, AP_REQUESTED, AP_DATA, AP_INTEGER, AP_TEXT, AP_RESUME };
struct ap_value {
	enum ap_value_kind kind;
	const struct ap_descriptor *descriptor;
	size_t count;
	struct ap_value **arguments;
	union {
		struct {
			struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *);
			const struct ap_env *environment;
			size_t binder;
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

static struct ap_value *demand(struct ap_runtime *runtime, struct ap_value *input)
{
	/* Memoization is only of pure closure reduction, never host execution. */
	while (input->kind == AP_DELAYED) {
		if (!input->as.closure.answer) {
			if (input->as.closure.busy) fail(runtime, "cyclic demand", 4);
			input->as.closure.busy = 1;
			input->as.closure.answer = input->as.closure.body(runtime, input->as.closure.environment);
		}
		input = input->as.closure.answer;
	}
	return input;
}

struct ap_value *ap_delay(struct ap_runtime *runtime,
	struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *), const struct ap_env *environment)
{
	struct ap_value *result = value(runtime, AP_DELAYED, 0);
	result->as.closure.body = body;
	result->as.closure.environment = environment;
	return result;
}

struct ap_value *ap_function(struct ap_runtime *runtime, size_t binder,
	struct ap_value *(*body)(struct ap_runtime *, const struct ap_env *), const struct ap_env *environment)
{
	struct ap_value *result = ap_delay(runtime, body, environment);
	result->kind = AP_FUNCTION;
	result->as.closure.binder = binder;
	return result;
}

struct ap_value *ap_lookup(struct ap_runtime *runtime, const struct ap_env *environment, size_t binder)
{
	for (; environment; environment = environment->parent)
		if (environment->binder == binder) return environment->value;
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
		struct ap_value *suspended = demand(runtime, arguments[0]);
		if (suspended->kind != AP_SUSPENDED) fail(runtime, "Force requires Thunk", 4);
		return demand(runtime, suspended->arguments[0]);
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
	if (function->kind == AP_FUNCTION) {
		struct ap_env *environment = allocate(runtime, sizeof(*environment));
		*environment = (struct ap_env){function->as.closure.environment, function->as.closure.binder, argument};
		return function->as.closure.body(runtime, environment);
	}
	if (function->kind == AP_RESUME)
		return fold(runtime, ap_apply(runtime, function->arguments[0], argument), function->arguments[1]);
	if (function->kind != AP_PRIMITIVE) fail(runtime, "application requires a function", 4);
	int neutral = function->descriptor->operation == AP_ATOM;
	if (!neutral && function->count >= function->descriptor->arity) fail(runtime, "operation is not callable", 4);
	if (function->count == SIZE_MAX) fail(runtime, "arity overflow", 2);
	struct ap_value *result = value(runtime, AP_PRIMITIVE, function->count + 1);
	result->descriptor = function->descriptor;
	for (size_t i = 0; i < function->count; ++i) result->arguments[i] = function->arguments[i];
	result->arguments[function->count] = argument;
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
