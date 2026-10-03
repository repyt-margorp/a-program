#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "host.h"
#include "evidence.h"
#include <assert.h>
#include <inttypes.h>

/* Descriptive evaluator correspondence; source admission is tested separately. */
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
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_whnf_advance(work, budget);
}
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	assert(!emitting);
	return __real_pg_typed_query_advance(work, budget);
}

static const struct pg_term *app(struct pg_graph *graph, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(graph, f, x);
	assert(term);
	return term;
}

static const struct pg_term *op(struct pg_graph *graph, const struct pg_object *f, const struct pg_term *x)
{
	return app(graph, pg_reference(graph, f), x);
}

static const struct pg_term *chain(struct pg_graph *graph, const struct pg_object *offset,
	const struct pg_object *input, size_t count)
{
	assert(count && count <= 8);
	const struct pg_object *binders[8];
	const struct pg_term *closures[8];
	for (size_t i = 0; i < count; ++i) {
		binders[i] = pg_binder(graph);
		const struct pg_object *argument = pg_binder(graph);
		const struct pg_term *body = i ?
			app(graph, op(graph, &pg_force_operation, pg_reference(graph, binders[i - 1])), pg_reference(graph, argument)) :
			app(graph, op(graph, pg_host_function(0), pg_reference(graph, argument)), pg_reference(graph, offset));
		closures[i] = op(graph, &pg_thunk_operation, pg_lambda(graph, argument, body));
	}
	const struct pg_term *body = app(graph,
		op(graph, &pg_force_operation, pg_reference(graph, binders[count - 1])), pg_reference(graph, input));
	for (size_t i = count; i; --i) body = app(graph, pg_lambda(graph, binders[i - 1], body), closures[i - 1]);
	return pg_lambda(graph, offset, pg_lambda(graph, input, body));
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	struct pg_graph graph;
	struct pg_typing typing;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	const struct pg_object *offset = pg_binder(&graph), *input = pg_binder(&graph);
	const struct pg_term *type = pg_pi(&graph, pg_reference(&graph, pg_host_type("Int32")), offset,
		pg_pi(&graph, pg_reference(&graph, pg_host_type("Int32")), input,
			pg_computation_type(&graph, PG_TOTALITY_TOTAL, pg_effect_row(&graph, 0, NULL),
				pg_reference(&graph, pg_host_type("Int32")))));
	const size_t lengths[] = {3, 4, 8};
	const char *names[] = {"three", "four", "eight"};
	const struct pg_term *terms[3];
	struct pg_c_export exports[3];
	for (size_t i = 0; i < 3; ++i) {
		terms[i] = chain(&graph, offset, input, lengths[i]);
		exports[i] = (struct pg_c_export){names[i], pg_occurrence(&typing,
			PG_JUDGEMENT_COMPUTATION, NULL, terms[i], type, NULL, 0, NULL)};
	}
	FILE *source = fopen(argv[1], "w"), *header = tmpfile();
	assert(source && header);
	size_t term_count = graph.terms.count, objects = graph.objects.count;
	size_t proofs = typing.proofs.count, occurrences = typing.occurrences.count;
	const char *error;
	emitting = 1;
	assert(!pg_c_emit_scalar(source, header, 3, exports, SIZE_MAX, &error));
	emitting = 0;
	assert(graph.terms.count == term_count && graph.objects.count == objects);
	assert(typing.proofs.count == proofs && typing.occurrences.count == occurrences);
	assert(!fclose(header));
	fputs("#include <assert.h>\nint main(void)\n{\n\tint32_t out;\n", source);
	const int32_t values[] = {0, 1, -1, INT32_MIN, INT32_MAX, 7, -9, 42, -99, 305419896};
	for (size_t i = 0; i < 10; ++i) for (size_t j = 0; j < 10; ++j) for (size_t k = 0; k < 3; ++k) {
		const struct pg_term *a = pg_reference(&graph, pg_host_integer(&graph, pg_host_type("Int32"), values[i]));
		const struct pg_term *b = pg_reference(&graph, pg_host_integer(&graph, pg_host_type("Int32"), values[j]));
		struct pg_eval machine;
		pg_computation_eval_init(&machine, &graph,
			op(&graph, &pg_total_result_operation, app(&graph, app(&graph, terms[k], a), b)));
		assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
		const struct pg_term *answer = pg_eval_readback(&machine, &graph);
		int64_t expected;
		assert(answer && answer->kind == PG_REFERENCE && pg_host_integer_view(answer->as.reference, &expected));
		fprintf(source, "\tassert(!ap_export_%s(INT32_C(%" PRId32 "), INT32_C(%" PRId32 "), &out) && out == INT32_C(%" PRId64 "));\n",
			names[k], values[i], values[j], expected);
		pg_eval_destroy(&machine);
	}
	fputs("\treturn 0;\n}\n", source);
	assert(!fclose(source));
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("Transitive capture Oracle: inert emission and 300 evaluator comparisons for three/four/eight closures passed");
	return 0;
}
