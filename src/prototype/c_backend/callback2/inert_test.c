#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
#include "computation.h"
#include "host.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

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

static const struct pg_term *app(struct pg_graph *graph, const struct pg_term *function, const struct pg_term *value)
{
	const struct pg_term *term = pg_application(graph, function, value);
	assert(term);
	return term;
}

static const struct pg_term *op(struct pg_graph *graph, const struct pg_object *operation, const struct pg_term *value)
{
	return app(graph, pg_reference(graph, operation), value);
}

static const struct pg_term *literal(struct pg_graph *graph, int width, int64_t value)
{
	return pg_reference(graph, pg_host_integer(graph, pg_host_type(width == 32 ? "Int32" : "Int64"), value));
}

/* These well-scoped test interpretations use existing Core/host operations;
	* they are not Surface admission or new formation/equality receipts. */
static const struct pg_term *binary_callback(struct pg_graph *graph, int width, int64_t offset)
{
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const char *subtract = width == 32 ? "host/int32/sub/v1" : "host/int64/sub/v1";
	const char *add = width == 32 ? "host/int32/add/v1" : "host/int64/add/v1";
	const struct pg_term *body = app(graph, op(graph, pg_host_function_resolve(subtract), pg_reference(graph, x)), pg_reference(graph, y));
	body = app(graph, op(graph, pg_host_function_resolve(add), op(graph, &pg_total_result_operation, body)), literal(graph, width, offset));
	return op(graph, &pg_thunk_operation, pg_lambda(graph, x, pg_lambda(graph, y, body)));
}

static const struct pg_term *negative_callback(struct pg_graph *graph)
{
	const struct pg_object *x = pg_binder(graph);
	const struct pg_term *body = op(graph, pg_host_function_resolve("host/int32/neg/v1"), pg_reference(graph, x));
	return op(graph, &pg_thunk_operation, pg_lambda(graph, x, body));
}

static int64_t evaluate(struct pg_graph *graph, const struct pg_term *call)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, op(graph, &pg_total_result_operation, call));
	assert(pg_eval_advance(&machine, 10000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	int64_t value;
	assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	pg_eval_destroy(&machine);
	return value;
}

static void oracle(struct pg_program *program, const struct pg_c_link_plan *plan, const char *path)
{
	assert(plan->count == 8);
	FILE *file = fopen(path, "w");
	assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\n#include <limits.h>\n"
		"static int32_t signed32(uint32_t n) { return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n); }\n"
		"static int64_t signed64(uint64_t n) { return n <= INT64_MAX ? (int64_t)n : -1 - (int64_t)(UINT64_MAX - n); }\n"
		"static int32_t sub32(void *p, int32_t x, int32_t y) { return signed32((uint32_t)x - (uint32_t)y + (uint32_t)*(int32_t *)p); }\n"
		"static int64_t sub64(void *p, int64_t x, int64_t y) { return signed64((uint64_t)x - (uint64_t)y + (uint64_t)*(int64_t *)p); }\n"
		"static int32_t neg32(void *p, int32_t n) { (void)p; return signed32(UINT32_C(0) - (uint32_t)n); }\n"
		"int main(void)\n{\n", file);
	int64_t inputs32[] = {INT32_MIN, INT32_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT32_MAX - 1, INT32_MAX};
	int64_t offsets32[] = {INT32_MIN, -7, 0, 7, INT32_MAX};
	int64_t inputs64[] = {INT64_MIN, INT64_MIN + 1, -1024, -1, 0, 1, 7, 1024, INT64_MAX - 1, INT64_MAX};
	int64_t offsets64[] = {INT64_MIN, -7, 0, 7, INT64_MAX};
	struct pg_graph *graph = &program->graph;
	for (size_t i = 0; i < 5; ++i) for (size_t j = 0; j < 10; ++j) {
		size_t second = (i + j + 3) % 10;
		fprintf(file, "\t{ int32_t offset32 = signed32(UINT32_C(0x%" PRIx32 ")), out32;\n"
			"\tint64_t offset64 = signed64(UINT64_C(0x%" PRIx64 ")), out64;\n"
			"\tstruct ap_c_callback2_i32 cb32 = {&offset32, sub32};\n"
			"\tstruct ap_c_callback2_i64 cb64 = {&offset64, sub64};\n"
			"\tstruct ap_c_callback_i32 negative = {0, neg32};\n", (uint32_t)offsets32[i], (uint64_t)offsets64[i]);
		for (size_t k = 0; k < plan->count; ++k) {
			int width = k < 5 ? 32 : 64;
			int64_t offset = k < 5 ? offsets32[i] : offsets64[i];
			int64_t x = k < 5 ? inputs32[j] : inputs64[j], y = k < 5 ? inputs32[second] : inputs64[second];
			const struct pg_term *call = app(graph, plan->exports[k].subject->core, binary_callback(graph, width, offset));
			if (k == 3) call = app(graph, call, negative_callback(graph));
			call = app(graph, app(graph, call, literal(graph, width, x)), literal(graph, width, y));
			int64_t expected = evaluate(graph, call);
			fprintf(file, "\tassert(!ap_export_%s(cb%d, %ssigned%d(UINT%d_C(0x%" PRIx64 ")), "
				"signed%d(UINT%d_C(0x%" PRIx64 ")), &out%d) && (uint%d_t)out%d == UINT%d_C(0x%" PRIx64 "));\n",
				plan->exports[k].alias, width, k == 3 ? "negative, " : "", width, width,
				width == 32 ? (uint32_t)x : (uint64_t)x, width, width,
				width == 32 ? (uint32_t)y : (uint64_t)y, width, width, width, width,
				width == 32 ? (uint32_t)expected : (uint64_t)expected);
		}
		fputs("\t}\n", file);
	}
	fputs("\treturn 0;\n}\n", file);
	assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5);
	struct pg_c_link_plan plan = {0};
	size_t line;
	const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error));
	assert(plan.lowering == PG_C_CALLBACK2_DIRECT);
	FILE *file = fopen(plan.artifact, "rb");
	assert(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) {
		const char *name = plan.names[i];
		struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
		struct pg_synthesis_job *selected = pg_program_select_name(program, roots[0], token);
		uint64_t steps;
		assert(selected && pg_artifact_revalidate(program, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *proof = pg_synthesis_result(selected);
		assert(pg_evidence_owned_by(proof, &program->typing));
		plan.exports[i].subject = pg_evidence_subject(proof);
	}
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w");
	assert(source && header);
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	emitting = 1;
	assert(!pg_c_emit_callbacks2(source, header, plan.count, plan.exports, SIZE_MAX, &error));
	emitting = 0;
	assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
	assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header));
	oracle(program, &plan, argv[4]);
	pg_program_destroy(program);
	pg_c_link_destroy(&plan);
	puts("Unary/binary callback emission inert; separate Core evaluator supplies 400 Int32/Int64 comparisons");
	return 0;
}
