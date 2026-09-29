#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "host.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

/* Low-level correspondence fixtures are descriptive, not acceptance evidence.
 * check.sh separately obtains all exports through source Solve/admission. */
static int emitting;
enum pg_eval_status __real_pg_eval_advance(struct pg_eval *, uint64_t);
enum pg_eval_status __wrap_pg_eval_advance(struct pg_eval *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_eval_advance(work, budget);
}
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_substitution_advance(work, budget);
}

static const struct pg_term *app(struct pg_graph *g, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *result = pg_application(g, f, x);
	assert(result);
	return result;
}

static const struct pg_term *op(struct pg_graph *g, const struct pg_object *f, const struct pg_term *x)
{
	return app(g, pg_reference(g, f), x);
}

static int64_t evaluate(struct pg_graph *g, const struct pg_term *call)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, g, op(g, &pg_total_result_operation, call));
	assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
	const struct pg_term *answer = pg_eval_readback(&machine, g);
	int64_t result;
	assert(answer && answer->kind == PG_REFERENCE && pg_host_integer_view(answer->as.reference, &result));
	pg_eval_destroy(&machine);
	return result;
}

static const struct pg_occurrence *occurrence(struct pg_typing *t, const struct pg_term *term, const struct pg_term *type)
{
	return pg_occurrence(t, PG_JUDGEMENT_COMPUTATION, NULL, term, type, NULL, 0, NULL);
}

static void inert(struct pg_typing *t, FILE *source, size_t count, const struct pg_c_export *exports, int status)
{
	FILE *header = tmpfile();
	assert(header);
	size_t terms = t->graph->terms.count, objects = t->graph->objects.count;
	size_t proofs = t->proofs.count, subjects = t->occurrences.count;
	const char *error;
	emitting = 1;
	assert(pg_c_emit_scalar(source, header, count, exports, SIZE_MAX, &error) == status);
	emitting = 0;
	assert(terms == t->graph->terms.count && objects == t->graph->objects.count);
	assert(proofs == t->proofs.count && subjects == t->occurrences.count);
	if (status) assert(!ftell(source) && !ftell(header));
	assert(!fclose(header));
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	struct pg_graph g;
	struct pg_typing t;
	assert(!pg_graph_init(&g) && !pg_typing_init(&t, &g));
	struct pg_c_export exports[8];
	char names[8][16];
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_object *function = pg_host_function(i), *domain, *result;
		size_t arity;
		assert(pg_host_function_view(function, &domain, &result, &arity));
		const struct pg_object *x = pg_binder(&g), *y = pg_binder(&g);
		const struct pg_term *body = op(&g, function, pg_reference(&g, x));
		const struct pg_term *type = pg_computation_type(&g, PG_TOTALITY_TOTAL,
			pg_effect_row(&g, 0, NULL), pg_reference(&g, result));
		if (arity == 2) {
			body = pg_lambda(&g, y, app(&g, body, pg_reference(&g, y)));
			type = pg_pi(&g, pg_reference(&g, domain), y, type);
		}
		body = pg_lambda(&g, x, body);
		type = pg_pi(&g, pg_reference(&g, domain), x, type);
		snprintf(names[i], sizeof(names[i]), "f%zu", i);
		exports[i] = (struct pg_c_export){names[i], occurrence(&t, body, type)};
	}
	FILE *source = fopen(argv[1], "w");
	assert(source);
	inert(&t, source, 8, exports, 0);
	fputs("#include <assert.h>\nint main(void)\n{\n\tint32_t a;\n\tint64_t b;\n", source);
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_object *function = pg_host_function(i), *domain, *result;
		size_t arity;
		assert(pg_host_function_view(function, &domain, &result, &arity));
		int64_t min = i < 4 ? INT32_MIN : INT64_MIN, max = i < 4 ? INT32_MAX : INT64_MAX;
		const int64_t inputs[] = {min, min + 1, -65537, -1, 0, 1, 65537, max};
		for (size_t j = 0; j < 8; ++j) for (size_t k = 0; k < (arity == 2 ? 8 : 1); ++k) {
			int64_t x = inputs[j], y = inputs[k];
			const struct pg_term *call = op(&g, function, pg_reference(&g, pg_host_integer(&g, domain, x)));
			if (arity == 2) call = app(&g, call, pg_reference(&g, pg_host_integer(&g, domain, y)));
			uint64_t expected = (uint64_t)evaluate(&g, call);
			if (i < 4) expected = (uint32_t)expected;
			fprintf(source, "\tassert(!ap_export_f%zu(%" PRId64, i, x == INT64_MIN ? x + 1 : x);
			if (x == INT64_MIN) fputs("LL - 1", source);
			if (arity == 2) {
				fprintf(source, ", %" PRId64, y == INT64_MIN ? y + 1 : y);
				if (y == INT64_MIN) fputs("LL - 1", source);
			}
			fprintf(source, ", &%c));\n\tassert((uint%u_t)%c == UINT64_C(0x%" PRIx64 "));\n",
				i < 4 ? 'a' : 'b', i < 4 ? 32 : 64, i < 4 ? 'a' : 'b', expected);
		}
	}
	fputs("}\n", source);
	assert(!fclose(source));
	/* Each shared child is lowered once; deep input uses the existing iterative
	 * graph walker, not C recursion or exponentially expanded source text. */
	const struct pg_term *value = pg_reference(&g, pg_host_integer(&g, pg_host_type("Int32"), 1));
	for (size_t i = 0; i < 4096; ++i)
		value = op(&g, &pg_total_result_operation, app(&g, op(&g, pg_host_function(0), value), value));
	const struct pg_term *type = pg_computation_type(&g, PG_TOTALITY_TOTAL,
		pg_effect_row(&g, 0, NULL), pg_reference(&g, pg_host_type("Int32")));
	exports[0] = (struct pg_c_export){"deep", occurrence(&t, op(&g, &pg_return_operation, value), type)};
	source = tmpfile();
	assert(source);
	inert(&t, source, 1, exports, 0);
	assert(ftell(source) > 100000 && ftell(source) < 1000000);
	fclose(source);
	/* A forged signature alone cannot authorize erasing an unknown payload. */
	static const struct pg_object_class unknown_class = {"native-unsupported-test"};
	static const struct pg_object unknown = {PG_SEMANTIC_OBJECT, &unknown_class};
	exports[1] = (struct pg_c_export){"bad", occurrence(&t,
		op(&g, &pg_return_operation, pg_reference(&g, &unknown)), type)};
	source = tmpfile();
	assert(source);
	inert(&t, source, 2, exports, -1);
	fclose(source);
	pg_typing_destroy(&t);
	pg_graph_destroy(&g);
	puts("Native Oracle: inert translation, shared deep DAG and integer differential fixture passed");
}
