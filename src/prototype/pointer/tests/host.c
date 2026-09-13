#include "host.h"
#include "evidence.h"
#include "derivation.h"
#include "descriptor_io.h"
#include "graph_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct pg_graph graph, loaded;
	struct pg_typing typing, foreign;
	struct pg_classifiers classifiers;
	assert(!pg_graph_init(&graph) && !pg_graph_init(&loaded));
	assert(!pg_typing_init(&typing, &graph) && !pg_typing_init(&foreign, &loaded));
	assert(!pg_classifiers_init(&classifiers, &graph));
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
	puts("host: exact typed literals, fixed widths, ordinary evidence and descriptor relocation passed");
}
