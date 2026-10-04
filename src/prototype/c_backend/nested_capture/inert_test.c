#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
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

static const struct pg_occurrence *select_subject(struct pg_program *program,
	struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(program, root, token);
	uint64_t steps;
	assert(selected && pg_artifact_revalidate(program, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof, &program->typing));
	return pg_evidence_subject(proof);
}

static const struct pg_term *app(struct pg_graph *graph, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(graph, f, x);
	assert(term);
	return term;
}

static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	assert(subject->core->kind == PG_REFERENCE);
	const struct pg_data_declaration *declaration = pg_data_declaration_view(subject->core->as.reference);
	assert(declaration);
	return pg_data_declaration_layout(declaration);
}

static const struct pg_term *natural(struct pg_graph *graph, const struct pg_data_layout *type, uint32_t n)
{
	const struct pg_term *term = pg_reference(graph, pg_data_constructor(type, 0));
	while (n--) term = app(graph, pg_reference(graph, pg_data_constructor(type, 1)), term);
	return term;
}

static const struct pg_term *integer(struct pg_graph *graph, const char *type, int value)
{
	const struct pg_object *literal = pg_host_integer(graph, pg_host_type(type), value);
	assert(literal);
	return pg_reference(graph, literal);
}

static int64_t evaluate(struct pg_program *program, const struct pg_term *call)
{
	struct pg_graph *graph = &program->graph;
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, app(graph, pg_reference(graph, &pg_total_result_operation), call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	int64_t value;
	assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	pg_eval_destroy(&machine);
	return value;
}

/* Existing Core runs after inert emission, independently of native helpers.
	* These finite result comparisons supply no new source checking authority. */
static void oracle(struct pg_program *program, const struct pg_c_link_plan *plan, const char *path)
{
	assert(plan->count == 10 && plan->natural_count == 1 && plan->data_count == 1 && plan->enum_count == 1);
	struct pg_graph *graph = &program->graph;
	const struct pg_data_layout *nat = layout(plan->naturals[0].subject);
	const struct pg_data_layout *boolean = layout(plan->enums[0].subject), *list = layout(plan->data[0].subject);
	FILE *file = fopen(path, "w"); assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\nint main(void) { struct ap_c_arena arena = {0}; int32_t out32; int64_t out64;\n", file);
	size_t cases = 0;
	for (uint32_t n = 0; n <= 4; ++n) {
		const struct pg_term *input = natural(graph, nat, n);
		const size_t forms[] = {0, 2, 3, 8, 9};
		for (size_t f = 0; f < sizeof(forms) / sizeof(*forms); ++f) {
			size_t i = forms[f];
			int64_t expected = evaluate(program, app(graph, plan->exports[i].subject->core, input));
			fprintf(file, "assert(!ap_export_%s(&arena, %u, &out32) && out32 == INT64_C(%" PRId64 "));\n", plan->exports[i].alias, n, expected); ++cases;
		}
		for (unsigned flag = 0; flag < 2; ++flag) {
			const struct pg_term *call = app(graph, app(graph, plan->exports[1].subject->core,
				pg_reference(graph, pg_data_constructor(boolean, flag))), input);
			fprintf(file, "assert(!ap_export_branch_sum(&arena, (struct ap_enum_Bool){%u}, %u, &out32) && out32 == INT64_C(%" PRId64 "));\n", flag, n, evaluate(program, call)); ++cases;
		}
		for (int seed = -1; seed <= 1; ++seed) for (size_t i = 4; i <= 6; ++i) {
			const struct pg_term *value = integer(graph, i == 5 ? "Int64" : "Int32", seed);
			const struct pg_term *call = app(graph, app(graph, plan->exports[i].subject->core, i == 6 ? input : value), i == 6 ? value : input);
			int64_t expected = evaluate(program, call);
			fprintf(file, "assert(!ap_export_%s(&arena, %d, %d, &out%s) && out%s == INT64_C(%" PRId64 "));\n",
				plan->exports[i].alias, i == 6 ? (int)n : seed, i == 6 ? seed : (int)n, i == 5 ? "64" : "32", i == 5 ? "64" : "32", expected); ++cases;
		}
	}
	size_t combinations = 1;
	for (size_t length = 0; length <= 3; ++length, combinations *= 3) for (size_t code = 0; code < combinations; ++code) {
		int values[3] = {0}; size_t digits = code;
		for (size_t i = 0; i < length; ++i) { values[i] = (int)(digits % 3) - 1; digits /= 3; }
		const struct pg_term *input = pg_reference(graph, pg_data_constructor(list, 0));
		for (size_t i = length; i; --i) input = app(graph, app(graph,
			pg_reference(graph, pg_data_constructor(list, 1)), integer(graph, "Int32", values[i - 1])), input);
		int64_t expected = evaluate(program, app(graph, plan->exports[7].subject->core, input));
		fprintf(file, "{ int32_t a[3] = {%d,%d,%d}; const struct ap_data_Numbers *input; assert(!ap_from_Numbers(&arena, a, %zu, &input));\n"
			"assert(!ap_export_nested_list(&arena, input, &out32) && out32 == INT64_C(%" PRId64 ")); ap_arena_Nat_destroy(&arena); }\n",
			values[0], values[1], values[2], length, expected); ++cases;
	}
	assert(cases == 120);
	fputs("ap_arena_Nat_destroy(&arena); return 0; }\n", file); assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5);
	struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error) && plan.lowering == PG_C_NATIVE_DIRECT);
	FILE *file = fopen(plan.artifact, "rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(program, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(program, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.natural_count; ++i) plan.naturals[i].subject = select_subject(program, roots[0], plan.natural_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(program, roots[0], plan.data_names[i]);
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w"); assert(source && header);
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_native_profile(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, plan.natural_count, plan.naturals, plan.data_count, plan.data, &contract, &error));
	emitting = 0;
	assert(contract.recursive && contract.natural && contract.copy_out);
	assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
	assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header));
	oracle(program, &plan, argv[4]);
	pg_program_destroy(program); pg_c_link_destroy(&plan);
	puts("Nested private IH emission inert; separate existing Core supplies 120 finite comparisons");
	return 0;
}
