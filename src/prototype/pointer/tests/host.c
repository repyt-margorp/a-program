#include "host.h"
#include "evidence.h"
#include "derivation.h"
#include "descriptor_io.h"
#include "graph_io.h"
#include "computation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const struct pg_evidence *signature(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_object *type, size_t arity)
{
	const struct pg_evidence *contexts[3] = {pg_prove_empty_context(typing)}, *value = NULL;
	for (size_t i = 0; i <= arity; ++i) {
		value = pg_prove_host_type(typing, classifiers, contexts[i], type);
		if (i < arity) contexts[i + 1] = pg_prove_context_extension(typing, contexts[i], pg_binder(typing->graph), value);
	}
	const struct pg_evidence *result = pg_prove_computation_type(typing, classifiers,
		PG_TOTALITY_TOTAL, pg_effect_row(typing->graph, 0, NULL), value);
	for (size_t i = arity; i; --i) result = pg_prove_pi(typing, classifiers, contexts[i], result);
	assert(result);
	return result;
}

static void arithmetic(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	const int64_t cases[][3] = {
		{INT32_MAX, 1, INT32_MIN}, {INT32_MIN, 1, INT32_MAX},
		{INT32_MAX, 2, -2}, {INT32_MIN, 0, INT32_MIN},
		{INT64_MAX, 1, INT64_MIN}, {INT64_MIN, 1, INT64_MAX},
		{INT64_MAX, 2, -2}, {INT64_MIN, 0, INT64_MIN}
	};
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_object *function = pg_host_function(i), *type;
		size_t arity;
		assert(pg_host_function_view(function, &type, &arity));
		assert(pg_host_function_resolve(pg_host_function_descriptor(function)) == function);
		const struct pg_evidence *pi = signature(typing, classifiers, type, arity);
		const struct pg_evidence *proof = pg_prove_host_function(typing, pi, function);
		assert(proof && pg_prove_classifier(typing, classifiers, pg_prove_empty_context(typing), proof) == pi);
		assert(!pg_prove_host_function(typing, pi, pg_binder(graph)));
		assert(!pg_prove_host_function(typing, pi, pg_host_integer(graph, type, 0)));
		struct pg_derivation_parameters parameters;
		assert(!pg_derivation_parameters(proof, &parameters) && parameters.constant == function);
		assert(pg_prove_derivation(typing, classifiers, PG_HOST_FUNCTION_INTRO, &parameters, 1, &pi) == proof);
		assert(!pg_prove_host_function(typing, signature(typing, classifiers, type, 3 - arity), function));
		const struct pg_object *other = pg_host_type(i < 4 ? "Int64" : "Int32");
		assert(!pg_prove_host_function(typing, signature(typing, classifiers, other, arity), function));
		const struct pg_term *call = pg_reference(graph, function);
		for (size_t j = 0; j < arity; ++j) {
			const struct pg_term *literal = pg_reference(graph, pg_host_integer(graph, type, cases[i][j]));
			const struct pg_object *x = pg_binder(graph);
			/* Both operands require beta, including a captured environment. */
			const struct pg_term *identity = pg_lambda(graph, x, pg_reference(graph, x));
			call = pg_application(graph, call, pg_application(graph, identity, literal));
		}
		struct pg_eval machine;
		pg_computation_eval_init(&machine, graph, call);
		while (machine.status == PG_EVAL_PENDING && machine.steps < 10000) pg_eval_advance(&machine, 1);
		assert(machine.status == PG_EVAL_WHNF);
		const struct pg_term *result = pg_eval_readback(&machine, graph);
		assert(result && result->kind == PG_APPLICATION);
		assert(result->as.application.function == pg_reference(graph, &pg_return_operation));
		assert(result->as.application.argument == pg_reference(graph, pg_host_integer(graph, type, cases[i][2])));
		pg_eval_destroy(&machine);
		pg_eval_init(&machine, call);
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, graph) == call);
		pg_eval_destroy(&machine);
	}
	assert(!pg_host_function(8));
}

int main(void)
{
	struct pg_graph graph, loaded;
	struct pg_typing typing, foreign;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_graph_init(&loaded));
	assert(!pg_typing_init(&typing, &graph) && !pg_typing_init(&foreign, &loaded));
	assert(!pg_classifiers_init(&classifiers, &graph));
	arithmetic(&typing, &classifiers);
	const struct pg_object *i32 = pg_host_type("Int32"), *i64 = pg_host_type("Int64"), *text = pg_host_type("Text");
	assert(i32 == pg_host_type("Int") && i32 != i64);
	assert(!pg_host_type("Nat") && !pg_host_type_resolve("host/int32/v2"));
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *t32 = pg_prove_host_type(&typing, &classifiers, empty, i32);
	const struct pg_evidence *t64 = pg_prove_host_type(&typing, &classifiers, empty, i64);
	assert(t32 && t64 && t32 != t64);
	assert(!pg_prove_host_type(&typing, &classifiers, pg_prove_empty_context(&foreign), i32));
	assert(!pg_prove_host_type(&typing, &classifiers, empty, pg_binder(&graph)));
	const int64_t cases[] = {INT64_MIN, INT32_MIN, -1, 0, 1, INT32_MAX, INT64_MAX};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		const struct pg_object *value = pg_host_integer(&graph, i64, cases[i]);
		int64_t result;
		assert(value && value == pg_host_integer(&graph, i64, cases[i]));
		assert(pg_host_integer_view(value, &result) && result == cases[i]);
		const struct pg_evidence *proof = pg_prove_host_value(&typing, t64, value);
		assert(proof && !pg_prove_host_value(&typing, t32, value));
		assert(pg_prove_classifier(&typing, &classifiers, empty, proof) == t64);
		struct pg_derivation_parameters parameters;
		assert(!pg_derivation_parameters(proof, &parameters) && parameters.constant == value);
		assert(pg_prove_derivation(&typing, &classifiers, PG_HOST_VALUE_INTRO, &parameters, 1, &t64) == proof);
		assert(!pg_prove_derivation(&typing, &classifiers, PG_HOST_VALUE_INTRO, &parameters, 1, &t32));
		const struct pg_object *narrow = pg_host_integer(&graph, i32, cases[i]);
		if (cases[i] < INT32_MIN || cases[i] > INT32_MAX) assert(!narrow);
		else assert(narrow != value && pg_host_integer_view(narrow, &result) && result == cases[i]);
	}
	const unsigned char payload[] = {'x', 0, 255, 'y', 'z', '0', '1', '2', '3'};
	const struct pg_object *value = pg_host_literal(&graph, text, sizeof(payload), payload);
	assert(value && value == pg_host_literal(&graph, text, sizeof(payload), payload));
	assert(value != pg_host_literal(&graph, text, sizeof(payload) - 1, payload));
	assert(!pg_host_literal(&graph, i32, sizeof(payload), payload));
	assert(!pg_host_literal(&graph, i64, 4, payload));
	assert(!pg_host_integer(&graph, text, 0));
	const struct pg_term *roots[] = {pg_reference(&graph, value),
		pg_reference(&graph, pg_host_literal(&graph, text, 0, NULL)),
		pg_reference(&graph, pg_host_integer(&graph, i64, INT64_MIN))};
	FILE *file = tmpfile();
	assert(file && !pg_graph_write_descriptors(file, 3, roots, &pg_builtin_graph_codec, &classifiers));
	rewind(file);
	size_t count;
	const struct pg_term *const *restored;
	assert(!pg_graph_read_descriptors(file, &loaded, 100, 100, &pg_builtin_graph_codec, NULL, &count, &restored));
	assert(count == 3);
	const struct pg_object *type;
	const unsigned char *bytes;
	size_t length;
	assert(pg_host_literal_view(restored[0]->as.reference, &type, &length, &bytes));
	assert(type == text && length == sizeof(payload) && !memcmp(bytes, payload, length));
	assert(restored[0]->as.reference == pg_host_literal(&loaded, text, length, bytes));
	assert(pg_host_literal_view(restored[1]->as.reference, &type, &length, &bytes) && !length);
	int64_t number;
	assert(pg_host_integer_view(restored[2]->as.reference, &number) && number == INT64_MIN);
	/* Reject malformed descriptor sizes and noncanonical trailing bits. */
	const struct pg_term *types[] = {pg_reference(&graph, text)};
	uint64_t scalars[] = {1, UINT64_C(0x7800000000000001)};
	assert(!pg_builtin_graph_codec.restore(NULL, &graph, "host/literal/v1", 1, types, 2, scalars));
	scalars[1] = UINT64_C(0x7800000000000000);
	assert(pg_builtin_graph_codec.restore(NULL, &graph, "host/literal/v1", 1, types, 2, scalars));
	types[0] = pg_reference(&graph, i32);
	assert(!pg_builtin_graph_codec.restore(NULL, &graph, "host/literal/v1", 1, types, 2, scalars));
	fclose(file);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&foreign); pg_typing_destroy(&typing);
	pg_graph_destroy(&loaded); pg_graph_destroy(&graph);
	puts("host: typed literals, modular arithmetic, ordinary evidence and descriptor relocation passed");
}
