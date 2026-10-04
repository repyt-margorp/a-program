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

static const struct pg_occurrence *select_subject(struct pg_program *p, struct pg_synthesis_job *root, const char *name)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)};
	struct pg_synthesis_job *selected = pg_program_select_name(p, root, token); uint64_t steps;
	assert(selected && pg_artifact_revalidate(p, selected, 1000000, 1000000, &steps) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *proof = pg_synthesis_result(selected);
	assert(pg_evidence_owned_by(proof, &p->typing)); return pg_evidence_subject(proof);
}

static const struct pg_term *app(struct pg_graph *g, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(g, f, x); assert(term); return term;
}

static const struct pg_term *integer(struct pg_graph *g, const char *type, int64_t value)
{
	const struct pg_object *object = pg_host_integer(g, pg_host_type(type), value); assert(object);
	return pg_reference(g, object);
}

static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	assert(subject->core->kind == PG_REFERENCE);
	const struct pg_data_declaration *d = pg_data_declaration_view(subject->core->as.reference); assert(d);
	return pg_data_declaration_layout(d);
}

static int64_t evaluate(struct pg_program *p, const struct pg_term *call)
{
	struct pg_eval machine; struct pg_graph *g = &p->graph;
	pg_computation_eval_init(&machine, g, app(g, pg_reference(g, &pg_total_result_operation), call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, g); int64_t value;
	assert(result && result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &value));
	pg_eval_destroy(&machine); return value;
}

/* Existing Core supplies descriptive comparisons only after inert emission.
	* Int64 extrema are typed Core inputs, never frontend Int32 literals. */
static void oracle(struct pg_program *p, const struct pg_c_link_plan *plan, const char *path)
{
	assert(plan->count == 10 && plan->data_count == 4 && plan->enum_count == 1);
	struct pg_graph *g = &p->graph;
	const struct pg_data_layout *packet = layout(plan->data[0].subject), *envelope = layout(plan->data[1].subject);
	const struct pg_data_layout *types[2] = {layout(plan->data[2].subject), layout(plan->data[3].subject)};
	const struct pg_data_layout *boolean = layout(plan->enums[0].subject);
	const struct pg_term *values[6], *terms[2][222]; FILE *out = fopen(path, "w"); assert(out);
	fputs("#include \"component.h\"\n#include <assert.h>\nint main(void) {\n"
		"struct ap_c_arena arena={0}; struct ap_data_Envelope e[6]={0};\n"
		"struct ap_data_Tree t[222]={0}; struct ap_data_Reverse r[222]={0};\n"
		"const struct ap_data_Tree *result; const struct ap_data_Reverse *reverse; int32_t value;\n", out);
	values[0] = pg_reference(g, pg_data_constructor(envelope, 0));
	for (size_t i = 1; i < 6; ++i) {
		const struct pg_term *payload = pg_reference(g, pg_data_constructor(packet, i == 1 ? 0 : i < 4 ? 1 : 2));
		int64_t n = i == 1 ? -1 : i == 2 ? 0 : i == 3 ? 3 : i == 4 ? -6 : 1;
		if (i == 2 || i == 3) payload = app(g, app(g, payload, integer(g, "Int32", i == 2 ? INT32_MAX : -7)),
			pg_reference(g, pg_data_constructor(boolean, i == 2)));
		if (i >= 4) payload = app(g, app(g, payload, integer(g, "Int64", i == 4 ? INT64_MIN : INT64_MAX)),
			integer(g, "Int32", i == 4 ? 23 : INT32_MAX));
		values[i] = app(g, app(g, pg_reference(g, pg_data_constructor(envelope, 1)), payload), integer(g, "Int32", n));
		if (i == 1) fputs("e[1]=(struct ap_data_Envelope){.tag=1,.fields.c1={{.tag=0},-1}};\n", out);
		else if (i < 4) fprintf(out, "e[%zu]=(struct ap_data_Envelope){.tag=1,.fields.c1={{.tag=1,.fields.c1={%d,{%u}}},%d}};\n",
			i, i == 2 ? INT32_MAX : -7, i == 2, (int)n);
		else fprintf(out, "e[%zu]=(struct ap_data_Envelope){.tag=1,.fields.c1={{.tag=2,.fields.c2={%s,%d}},%d}};\n",
			i, i == 4 ? "INT64_MIN" : "INT64_MAX", i == 4 ? 23 : INT32_MAX, (int)n);
	}
	for (size_t i = 0; i < 6; ++i) {
		for (size_t type = 0; type < 2; ++type)
			terms[type][i] = app(g, pg_reference(g, pg_data_constructor(types[type], type)), values[i]);
		fprintf(out, "t[%zu]=(struct ap_data_Tree){.tag=0,.fields.c0={e[%zu]}}; r[%zu]=(struct ap_data_Reverse){.tag=1,.fields.c1={e[%zu]}};\n", i,i,i,i);
	}
	size_t count = 6;
	for (size_t left = 0; left < 6; ++left) for (size_t payload = 0; payload < 6; ++payload)
		for (size_t right = 0; right < 6; ++right, ++count) {
			terms[0][count] = app(g, app(g, app(g, pg_reference(g, pg_data_constructor(types[0], 1)), terms[0][left]), values[payload]), terms[0][right]);
			terms[1][count] = app(g, app(g, app(g, pg_reference(g, pg_data_constructor(types[1], 0)), terms[1][left]), terms[1][right]), values[payload]);
			fprintf(out, "t[%zu]=(struct ap_data_Tree){.tag=1,.fields.c1={&t[%zu],e[%zu],&t[%zu]}};\n"
				"r[%zu]=(struct ap_data_Reverse){.tag=0,.fields.c0={&r[%zu],&r[%zu],e[%zu]}};\n", count,left,payload,right,count,left,right,payload);
		}
	assert(count == 222); size_t comparisons = 0;
	for (size_t i = 0; i < count; ++i) for (size_t type = 0; type < 2; ++type)
		for (size_t mirrored = 0; mirrored < 2; ++mirrored) {
			size_t sum = type ? 8 : 1, mirror = type ? 9 : 2;
			const struct pg_term *term = terms[type][i];
			if (mirrored) term = app(g, pg_reference(g, &pg_total_result_operation), app(g, plan->exports[mirror].subject->core, term));
			int64_t expected = evaluate(p, app(g, plan->exports[sum].subject->core, term));
			if (mirrored) fprintf(out, "assert(!ap_export_%s(&arena,&%s[%zu],&%s));\n", plan->exports[mirror].alias,type?"r":"t",i,type?"reverse":"result");
			if (mirrored) fprintf(out, "assert(!ap_export_%s(&arena,%s,&value));\n", plan->exports[sum].alias,type?"reverse":"result");
			else fprintf(out, "assert(!ap_export_%s(&arena,&%s[%zu],&value));\n", plan->exports[sum].alias,type?"r":"t",i);
			fprintf(out, "assert(value==(INT64_C(%" PRId64 "))); ap_arena_Tree_destroy(&arena);\n", expected); ++comparisons;
		}
	assert(comparisons == 888); fputs("return 0; }\n", out); assert(!fclose(out));
}

int main(int argc, char **argv)
{
	assert(argc == 5); struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error) && plan.lowering == PG_C_NATIVE_DIRECT);
	FILE *file = fopen(plan.artifact, "rb"); assert(file); size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(file, SIZE_MAX, &count, &roots); assert(!fclose(file) && p && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(p, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(p, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(p, roots[0], plan.data_names[i]);
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w"); assert(source && header);
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count;
	size_t proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_native_profile(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, 0, NULL, plan.data_count, plan.data, &contract, &error)); emitting = 0;
	assert(contract.recursive && contract.branching && !contract.natural && !contract.copy_out);
	assert(p->graph.terms.count == terms && p->graph.objects.count == objects);
	assert(p->typing.proofs.count == proofs && p->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header)); oracle(p, &plan, argv[4]);
	pg_program_destroy(p); pg_c_link_destroy(&plan);
	puts("Record-tree emission inert; separate existing Core supplies 888 finite comparisons"); return 0;
}
