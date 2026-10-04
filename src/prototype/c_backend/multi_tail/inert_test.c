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
	assert(!emitting); return __real_pg_eval_advance(work, budget);
}
enum pg_substitution_status __real_pg_substitution_advance(struct pg_substitution *, uint64_t);
enum pg_substitution_status __wrap_pg_substitution_advance(struct pg_substitution *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_substitution_advance(work, budget);
}
enum pg_eval_status __real_pg_whnf_advance(struct pg_whnf_job *, uint64_t);
enum pg_eval_status __wrap_pg_whnf_advance(struct pg_whnf_job *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_whnf_advance(work, budget);
}
int __real_pg_typed_query_advance(struct pg_typed_query *, uint64_t);
int __wrap_pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	assert(!emitting); return __real_pg_typed_query_advance(work, budget);
}

static const struct pg_occurrence *select_subject(struct pg_program *program, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(program, root, token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(program, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof, &program->typing)); return pg_evidence_subject(proof);
}

static const struct pg_term *app(struct pg_graph *graph, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(graph, f, x); assert(term); return term;
}

static const struct pg_term *integer(struct pg_graph *graph, int32_t value)
{
	const struct pg_object *object = pg_host_integer(graph, pg_host_type("Int32"), value); assert(object);
	return pg_reference(graph, object);
}

static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	assert(subject->core->kind == PG_REFERENCE);
	const struct pg_data_declaration *declaration = pg_data_declaration_view(subject->core->as.reference); assert(declaration);
	return pg_data_declaration_layout(declaration);
}

static int64_t evaluate(struct pg_program *program, const struct pg_term *call)
{
	struct pg_eval machine; struct pg_graph *graph = &program->graph;
	pg_computation_eval_init(&machine, graph, app(graph, pg_reference(graph, &pg_total_result_operation), call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, graph); int64_t value;
	assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	pg_eval_destroy(&machine); return value;
}

/* Existing Core runs only after inert target emission. It supplies descriptive
	* finite comparisons, not new source admission or transformation evidence. */
static void oracle(struct pg_program *program, const struct pg_c_link_plan *plan, const char *path)
{
	assert(plan->count == 11 && plan->data_count == 2 && plan->enum_count == 1 && !plan->natural_count);
	struct pg_graph *graph = &program->graph;
	const struct pg_data_layout *types[2] = {layout(plan->data[0].subject), layout(plan->data[1].subject)};
	const struct pg_data_layout *boolean = layout(plan->enums[0].subject);
	const struct pg_term *terms[2][210]; FILE *file = fopen(path, "w"); assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\nint main(void) {\n"
		"struct ap_c_arena arena = {0}; struct ap_data_Tree t[210] = {0}; struct ap_data_Reverse r[210] = {0};\n"
		"const struct ap_data_Tree *out; const struct ap_data_Reverse *rout; int32_t value;\n", file);
	for (unsigned value = 0; value < 2; ++value) {
		for (unsigned type = 0; type < 2; ++type)
			terms[type][value] = app(graph, pg_reference(graph, pg_data_constructor(types[type], type)), integer(graph, value));
		fprintf(file, "t[%u] = (struct ap_data_Tree){.tag=0,.fields.c0={%u}}; r[%u] = (struct ap_data_Reverse){.tag=1,.fields.c1={%u}};\n", value, value, value, value);
	}
	size_t count = 2;
	for (size_t bound = 2; bound <= 10; bound += 8)
		for (unsigned flag = 0; flag < 2; ++flag) for (size_t left = 0; left < bound; ++left) for (size_t right = 0; right < bound; ++right) {
			const struct pg_term *b = pg_reference(graph, pg_data_constructor(boolean, flag));
			terms[0][count] = app(graph, app(graph, app(graph, pg_reference(graph, pg_data_constructor(types[0], 1)), b), terms[0][left]), terms[0][right]);
			terms[1][count] = app(graph, app(graph, app(graph, pg_reference(graph, pg_data_constructor(types[1], 0)), terms[1][left]), b), terms[1][right]);
			fprintf(file, "t[%zu] = (struct ap_data_Tree){.tag=1,.fields.c1={{%u},&t[%zu],&t[%zu]}};\n"
				"r[%zu] = (struct ap_data_Reverse){.tag=0,.fields.c0={&r[%zu],{%u},&r[%zu]}};\n", count, flag, left, right, count, left, flag, right);
			++count;
		}
	assert(count == 210); size_t cases = 0;
	const size_t exports[] = {0,1,2,3,4,7,8,10};
	for (size_t i = 0; i < count; ++i) {
		if (i >= 2 && i < 10) continue;
		for (size_t form = 0; form < sizeof(exports) / sizeof(*exports); ++form) {
			size_t selection = exports[form]; unsigned type = selection >= 7;
			int data_result = selection == 0 || selection == 3 || selection == 4 || selection == 7 || selection == 10;
			const struct pg_term *call = app(graph, plan->exports[selection].subject->core, terms[type][i]);
			if (selection == 4) call = app(graph, call, integer(graph, -3));
			if (data_result) call = app(graph, plan->exports[type ? 9 : 2].subject->core,
				app(graph, pg_reference(graph, &pg_total_result_operation), call));
			int64_t expected = evaluate(program, call);
			fprintf(file, "assert(!ap_export_%s(&arena, &%s[%zu]%s, &%s));\n", plan->exports[selection].alias,
				type ? "r" : "t", i, selection == 4 ? ", -3" : "", data_result ? type ? "rout" : "out" : "value");
			if (data_result) fprintf(file, "assert(!ap_export_%s(&arena, %s, &value));\n", plan->exports[type ? 9 : 2].alias, type ? "rout" : "out");
			fprintf(file, "assert(value == (%" PRId64 "));\n", expected); ++cases;
		}
		fputs("ap_arena_Tree_destroy(&arena);\n", file);
	}
	assert(cases == 1616); fputs("return 0; }\n", file); assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5); struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error) && plan.lowering == PG_C_NATIVE_DIRECT);
	FILE *file = fopen(plan.artifact, "rb"); assert(file); size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *program = pg_artifact_read_file(file, SIZE_MAX, &count, &roots);
	assert(!fclose(file) && program && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(program, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(program, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(program, roots[0], plan.data_names[i]);
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w"); assert(source && header);
	size_t terms = program->graph.terms.count, objects = program->graph.objects.count;
	size_t proofs = program->typing.proofs.count, occurrences = program->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_native_profile(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, 0, NULL, plan.data_count, plan.data, &contract, &error)); emitting = 0;
	assert(contract.recursive && contract.branching && !contract.natural && !contract.copy_out);
	assert(program->graph.terms.count == terms && program->graph.objects.count == objects);
	assert(program->typing.proofs.count == proofs && program->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header)); oracle(program, &plan, argv[4]);
	pg_program_destroy(program); pg_c_link_destroy(&plan);
	puts("Branching native emission inert; separate existing Core supplies 1616 finite comparisons"); return 0;
}
