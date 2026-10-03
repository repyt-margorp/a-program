#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "host.h"
#include <assert.h>
#include <inttypes.h>

/* Descriptive correspondence probes; source admission is verified separately. */
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

int main(int argc, char **argv)
{
	assert(argc == 2);
	struct pg_graph g;
	struct pg_typing t;
	assert(!pg_graph_init(&g) && !pg_typing_init(&t, &g));
	const struct pg_object *x = pg_binder(&g), *y = pg_binder(&g), *f = pg_binder(&g), *shadow = pg_binder(&g);
	const struct pg_term *vx = pg_reference(&g, x), *vy = pg_reference(&g, y), *vs = pg_reference(&g, shadow);
	const struct pg_term *closure = op(&g, &pg_thunk_operation,
		pg_lambda(&g, y, app(&g, op(&g, pg_host_function(0), vy), vx)));
	const struct pg_term *force = op(&g, &pg_force_operation, pg_reference(&g, f));
	const struct pg_term *terms[] = {
		pg_lambda(&g, x, app(&g, pg_lambda(&g, f, app(&g, force, vx)), closure)),
		pg_lambda(&g, x, app(&g, pg_lambda(&g, f, app(&g,
			pg_lambda(&g, shadow, app(&g, force, vs)),
			pg_reference(&g, pg_host_integer(&g, pg_host_type("Int32"), 7)))), closure))
	};
	const struct pg_term *type = pg_pi(&g, pg_reference(&g, pg_host_type("Int32")), x,
		pg_computation_type(&g, PG_TOTALITY_TOTAL, pg_effect_row(&g, 0, NULL),
			pg_reference(&g, pg_host_type("Int32"))));
	struct pg_c_export exports[] = {
		{"captured", pg_occurrence(&t, PG_JUDGEMENT_COMPUTATION, NULL, terms[0], type, NULL, 0, NULL)},
		{"shadowed", pg_occurrence(&t, PG_JUDGEMENT_COMPUTATION, NULL, terms[1], type, NULL, 0, NULL)}
	};
	FILE *source = fopen(argv[1], "w"), *header = tmpfile();
	assert(source && header);
	size_t terms_before = g.terms.count, objects_before = g.objects.count;
	size_t proofs_before = t.proofs.count, subjects_before = t.occurrences.count;
	const char *error;
	emitting = 1;
	assert(!pg_c_emit_scalar(source, header, 2, exports, SIZE_MAX, &error));
	emitting = 0;
	assert(g.terms.count == terms_before && g.objects.count == objects_before);
	assert(t.proofs.count == proofs_before && t.occurrences.count == subjects_before);
	assert(!fclose(header));
	fputs("#include <assert.h>\nint main(void)\n{\n\tint32_t out;\n", source);
	const int32_t values[] = {0, 1, -1, INT32_MIN, INT32_MAX, 7, -9, 42, -99, 305419896};
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) for (size_t j = 0; j < 2; ++j) {
		struct pg_eval machine;
		const struct pg_term *argument = pg_reference(&g, pg_host_integer(&g, pg_host_type("Int32"), values[i]));
		pg_computation_eval_init(&machine, &g, op(&g, &pg_total_result_operation, app(&g, terms[j], argument)));
		assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
		const struct pg_term *answer = pg_eval_readback(&machine, &g);
		int64_t expected;
		assert(answer && answer->kind == PG_REFERENCE && pg_host_integer_view(answer->as.reference, &expected));
		fprintf(source, "\tassert(!ap_export_%s(INT32_C(%" PRId32 "), &out) && out == INT32_C(%" PRId64 "));\n",
			j ? "shadowed" : "captured", values[i], expected);
		pg_eval_destroy(&machine);
	}
	fputs("\treturn 0;\n}\n", source);
	assert(!fclose(source));
	pg_typing_destroy(&t);
	pg_graph_destroy(&g);
	puts("Static function Oracle: inert emission, lexical captures and 20 evaluator comparisons passed");
	return 0;
}
