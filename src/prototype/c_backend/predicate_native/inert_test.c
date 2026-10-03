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

static const struct pg_term *evaluate(struct pg_graph *graph, const struct pg_term *call)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, op(graph, &pg_total_result_operation, call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	assert(result);
	pg_eval_destroy(&machine);
	return result;
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
	const struct pg_term *value = pg_reference(graph, pg_data_constructor(type, 0));
	while (n--) value = op(graph, pg_data_constructor(type, 1), value);
	return value;
}

static int64_t integer(struct pg_graph *graph, const struct pg_term *call)
{
	const struct pg_term *result = evaluate(graph, call);
	int64_t value;
	assert(result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	return value;
}

/* Inputs use existing Core constructors after inert emission; callbacks are
	* admitted source definitions. No new formation/equality receipt is created. */
static void oracle(struct pg_program *program, const struct pg_c_link_plan *plan,
	const struct pg_occurrence *keep, const struct pg_occurrence *compare, const char *path)
{
	assert(plan->count == 10 && plan->enum_count == 2 && plan->natural_count == 1 && plan->data_count == 1);
	struct pg_graph *graph = &program->graph;
	const struct pg_data_layout *nat = layout(plan->naturals[0].subject);
	const struct pg_data_layout *boolean = layout(plan->enums[0].subject);
	const struct pg_data_layout *list = layout(plan->data[0].subject);
	const struct pg_term *unary = op(graph, &pg_thunk_operation, keep->core);
	const struct pg_term *binary = op(graph, &pg_thunk_operation, compare->core);
	FILE *file = fopen(path, "w");
	assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\n"
		"static struct ap_enum_Bool keep(void *p, uint32_t n) { (void)p; return (struct ap_enum_Bool){n != 0}; }\n"
		"static struct ap_enum_Bool compare(void *p, uint32_t x, uint32_t y) { (void)p; return (struct ap_enum_Bool){x <= y}; }\n"
		"static uint32_t fingerprint(const struct ap_data_Numbers *out) { uint32_t values[4], n = 0; size_t written;\n"
		"assert(!ap_copy_Numbers(out, values, 4, &written)); while (written) n = n * 5 + values[--written] + 1; return n; }\n"
		"int main(void) { struct ap_c_arena arena = {0}; struct ap_enum_Bool flag;\n"
		"struct ap_c_predicate1_d3_Nat_r4_Bool unary = {0, keep}; struct ap_c_predicate2_d3_Nat_r4_Bool binary = {0, compare};\n", file);
	for (uint32_t x = 0; x <= 8; ++x) {
		const struct pg_term *call = app(graph, app(graph, plan->exports[0].subject->core, unary), natural(graph, nat, x));
		const struct pg_term *result = evaluate(graph, call);
		assert(result->kind == PG_REFERENCE);
		size_t tag = result->as.reference == pg_data_constructor(boolean, 0) ? 0 : 1;
		assert(result->as.reference == pg_data_constructor(boolean, tag));
		fprintf(file, "assert(!ap_export_apply_predicate(&arena, unary, %u, &flag) && flag.tag == %zu);\n", x, tag);
		for (uint32_t y = 0; y <= 8; ++y) {
			call = app(graph, app(graph, app(graph, plan->exports[1].subject->core, binary), natural(graph, nat, x)), natural(graph, nat, y));
			result = evaluate(graph, call);
			assert(result->kind == PG_REFERENCE);
			tag = result->as.reference == pg_data_constructor(boolean, 0) ? 0 : 1;
			assert(result->as.reference == pg_data_constructor(boolean, tag));
			fprintf(file, "assert(!ap_export_apply_comparator(&arena, binary, %u, %u, &flag) && flag.tag == %zu);\n", x, y, tag);
		}
	}
	size_t combinations = 1;
	for (size_t length = 0; length <= 4; ++length, combinations *= 3) for (size_t code = 0; code < combinations; ++code) {
		uint32_t values[4]; size_t digits = code;
		for (size_t i = 0; i < length; ++i) { values[i] = digits % 3; digits /= 3; }
		const struct pg_term *input = pg_reference(graph, pg_data_constructor(list, 0));
		for (size_t i = length; i; --i)
			input = app(graph, op(graph, pg_data_constructor(list, 1), natural(graph, nat, values[i - 1])), input);
		fputs("{ const struct ap_data_Numbers *input, *out; uint32_t values[4] = {", file);
		for (size_t i = 0; i < 4; ++i) fprintf(file, "%s%u", i ? "," : "", i < length ? values[i] : 0);
		fprintf(file, "}; assert(!ap_from_Numbers(&arena, values, %zu, &input));\n", length);
		const struct pg_term *call = app(graph, app(graph, plan->exports[4].subject->core, unary), input);
		int64_t expected = integer(graph, app(graph, plan->exports[8].subject->core, op(graph, &pg_total_result_operation, call)));
		fprintf(file, "assert(!ap_export_filter(&arena, unary, input, &out) && fingerprint(out) == UINT32_C(%" PRId64 "));\n", expected);
		for (uint32_t pivot = 0; pivot <= 2; ++pivot) {
			call = app(graph, app(graph, app(graph, plan->exports[5].subject->core, binary), input), natural(graph, nat, pivot));
			expected = integer(graph, app(graph, plan->exports[8].subject->core, op(graph, &pg_total_result_operation, call)));
			fprintf(file, "assert(!ap_export_select(&arena, binary, input, %u, &out) && fingerprint(out) == UINT32_C(%" PRId64 "));\n", pivot, expected);
		}
		fputs("ap_arena_Nat_destroy(&arena); }\n", file);
	}
	fputs("ap_arena_Nat_destroy(&arena); return 0; }\n", file);
	assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5);
	struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error));
	assert(plan.lowering == PG_C_PREDICATE_NATIVE_DIRECT);
	FILE *file = fopen(plan.artifact, "rb"); assert(file);
	size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(program, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(program, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.natural_count; ++i) plan.naturals[i].subject = select_subject(program, roots[0], plan.natural_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(program, roots[0], plan.data_names[i]);
	const struct pg_occurrence *keep = select_subject(program, roots[0], "keep");
	const struct pg_occurrence *compare = select_subject(program, roots[0], "less_equal");
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w"); assert(source && header);
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	struct pg_c_native_contract contract = {0};
	emitting = 1;
	assert(!pg_c_emit_predicate_native(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, plan.natural_count, plan.naturals, plan.data_count, plan.data, &contract, &error));
	emitting = 0;
	assert(contract.recursive && contract.natural && contract.copy_out);
	assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
	assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header));
	oracle(program, &plan, keep, compare, argv[4]);
	pg_program_destroy(program); pg_c_link_destroy(&plan);
	puts("Native predicate emission inert; separate existing Core evaluator supplies 574 comparisons");
	return 0;
}
