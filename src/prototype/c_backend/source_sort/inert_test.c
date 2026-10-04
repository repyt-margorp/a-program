#include "artifact/file.h"
#include "../link/plan.h"
#include "../lower/scalar.h"
#include "../selection.h"
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
	const struct pg_term *head = subject->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	assert(head->kind == PG_REFERENCE);
	const struct pg_data_declaration *declaration = pg_data_declaration_view(head->as.reference);
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

static const struct pg_term *evaluate(struct pg_program *program, const struct pg_term *call)
{
	struct pg_graph *graph = &program->graph;
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, app(graph, pg_reference(graph, &pg_total_result_operation), call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph);
	assert(result);
	pg_eval_destroy(&machine);
	return result;
}


static int64_t integer_result(struct pg_program *program, const struct pg_term *call)
{
	const struct pg_term *result = evaluate(program, call); int64_t value;
	assert(result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	return value;
}

static size_t boolean_result(struct pg_program *program, const struct pg_data_layout *boolean, const struct pg_term *call)
{
	const struct pg_term *result = evaluate(program, call);
	const struct pg_data_layout *owner; size_t position, arity;
	assert(result->kind == PG_REFERENCE && pg_data_constructor_view(result->as.reference, &owner, &position, &arity));
	assert(owner == boolean && !arity);
	return position;
}

/* Existing Core evaluates the admitted source after inert emission. These
	* finite comparisons are descriptive, not a new transformation checker. */
static void oracle(struct pg_program *program, const struct pg_c_link_plan *plan,
	const struct pg_occurrence *fingerprint, const struct pg_occurrence *count, const char *path)
{
	assert(plan->count == 9 && plan->natural_count == 1 && plan->data_count == 1 && plan->enum_count == 1);
	struct pg_graph *graph = &program->graph;
	const struct pg_data_layout *nat = layout(plan->naturals[0].subject);
	const struct pg_data_layout *list = layout(plan->data[0].subject), *boolean = layout(plan->enums[0].subject);
	FILE *file = fopen(path, "w"); assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\n"
		"static uint32_t fingerprint(const struct ap_data_List *list) { uint32_t a[4], code = 0; size_t written;\n"
		"assert(!ap_copy_List(list, a, 4, &written)); while (written) code = code * 5 + a[--written] + 1; return code; }\n"
		"int main(void) { struct ap_c_arena arena = {0};\n", file);
	size_t cases = 0, combinations = 1;
	for (size_t length = 0; length <= 3; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		uint32_t values[3] = {0}; size_t digits = code;
		for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; digits /= 4; }
		const struct pg_term *input = pg_reference(graph, pg_data_constructor(list, 0));
		for (size_t i = length; i; --i) input = app(graph, app(graph,
			pg_reference(graph, pg_data_constructor(list, 1)), natural(graph, nat, values[i - 1])), input);
		fprintf(file, "{ uint32_t a[3] = {%u,%u,%u}; const struct ap_data_List *input, *out;\n"
			"assert(!ap_from_List(&arena, a, %zu, &input));\n", values[0], values[1], values[2], length);
		for (unsigned form = 0; form < 6; ++form) {
			const struct pg_term *call = plan->exports[form < 2 ? form : 2].subject->core;
			if (form >= 2) call = app(graph, call, natural(graph, nat, form - 2));
			call = app(graph, call, input);
			call = app(graph, fingerprint->core, app(graph, pg_reference(graph, &pg_total_result_operation), call));
			int64_t expected = integer_result(program, call);
			if (form < 2) fprintf(file, "assert(!ap_export_%s(&arena, input, &out));\n", plan->exports[form].alias);
			else fprintf(file, "assert(!ap_export_insert(&arena, %u, input, &out));\n", form - 2);
			fprintf(file, "assert(fingerprint(out) == UINT32_C(%" PRId64 "));\n", expected); ++cases;
		}
		fputs("ap_arena_Nat_destroy(&arena); }\n", file);
	}
	for (uint32_t n = 0; n <= 8; ++n) {
		const struct pg_term *call = app(graph, plan->exports[3].subject->core, natural(graph, nat, n));
		call = app(graph, count->core, app(graph, pg_reference(graph, &pg_total_result_operation), call));
		fprintf(file, "{ uint32_t out; assert(!ap_export_fixed_id(&arena, %u, &out) && out == UINT32_C(%" PRId64 ")); }\n",
			n, integer_result(program, call)); ++cases;
	}
	for (size_t form = 4; form <= 5; ++form) for (int value = -1; value <= 1; ++value) {
		const struct pg_term *call = app(graph, plan->exports[form].subject->core, integer(graph, form == 4 ? "Int32" : "Int64", value));
		fprintf(file, "{ int%s_t out; assert(!ap_export_%s(&arena, %d, &out) && out == INT64_C(%" PRId64 ")); }\n",
			form == 4 ? "32" : "64", plan->exports[form].alias, value, integer_result(program, call)); ++cases;
	}
	for (unsigned flag = 0; flag < 2; ++flag) {
		const struct pg_term *call = app(graph, plan->exports[6].subject->core, pg_reference(graph, pg_data_constructor(boolean, flag)));
		fprintf(file, "{ struct ap_enum_Bool out; assert(!ap_export_fixed_bool(&arena, (struct ap_enum_Bool){%u}, &out) && out.tag == %zu); }\n",
			flag, boolean_result(program, boolean, call)); ++cases;
	}
	for (uint32_t left = 0; left < 5; ++left) for (uint32_t right = 0; right < 5; ++right) {
		const struct pg_term *call = app(graph, app(graph, plan->exports[7].subject->core, natural(graph, nat, left)), natural(graph, nat, right));
		fprintf(file, "{ struct ap_enum_Bool out; assert(!ap_export_compare(&arena, %u, %u, &out) && out.tag == %zu); }\n",
			left, right, boolean_result(program, boolean, call)); ++cases;
	}
	assert(cases == 552);
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
	for (size_t i = 0; i < plan.data_count; ++i) {
		plan.data[i].subject = select_subject(program, roots[0], plan.data_names[i]);
		if (plan.data_from_value[i]) plan.data[i].subject = pg_c_value_classifier(plan.data[i].subject);
		assert(plan.data[i].subject);
	}
	const struct pg_occurrence *fingerprint = select_subject(program, roots[0], "fingerprint");
	const struct pg_occurrence *number = select_subject(program, roots[0], "count");
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
	oracle(program, &plan, fingerprint, number, argv[4]);
	pg_program_destroy(program); pg_c_link_destroy(&plan);
	puts("Known selected type binding emission inert; separate existing Core supplies 552 source-sort/identity comparisons");
	return 0;
}
