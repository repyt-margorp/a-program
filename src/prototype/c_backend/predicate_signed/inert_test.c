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
static const struct pg_term *op(struct pg_graph *g, const struct pg_object *operation, const struct pg_term *x)
{
	return app(g, pg_reference(g, operation), x);
}
static const struct pg_term *number(struct pg_graph *g, unsigned wide, int64_t value)
{
	const struct pg_object *object = pg_host_integer(g, pg_host_type(wide ? "Int64" : "Int32"), value); assert(object);
	return pg_reference(g, object);
}
static const struct pg_term *evaluate(struct pg_program *p, const struct pg_term *call)
{
	struct pg_eval machine; pg_computation_eval_init(&machine, &p->graph, op(&p->graph, &pg_total_result_operation, call));
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_eval_readback(&machine, &p->graph); assert(result); pg_eval_destroy(&machine); return result;
}
static const struct pg_data_layout *layout(const struct pg_occurrence *subject)
{
	assert(subject->core->kind == PG_REFERENCE);
	const struct pg_data_declaration *d = pg_data_declaration_view(subject->core->as.reference); assert(d);
	return pg_data_declaration_layout(d);
}

static void literal(char *buffer, unsigned wide, int64_t value)
{
	if (value == INT64_MIN) strcpy(buffer, "INT64_MIN");
	else if (!wide && value == INT32_MIN) strcpy(buffer, "INT32_MIN");
	else snprintf(buffer, 48, "INT%u_C(%" PRId64 ")", wide ? 64 : 32, value);
}

/* Admitted constant source callbacks are evaluated after inert emission.
	* Core tag/length comparisons do not prove a source signed comparator. */
static void oracle(struct pg_program *p, const struct pg_c_link_plan *plan,
	const struct pg_occurrence *callbacks[2][2], const struct pg_occurrence *binary[2], const char *path)
{
	assert(plan->count == 13 && plan->enum_count == 2 && plan->data_count == 2 && !plan->natural_count);
	struct pg_graph *g = &p->graph; const struct pg_data_layout *boolean = layout(plan->enums[0].subject);
	const struct pg_data_layout *lists[] = {layout(plan->data[0].subject),layout(plan->data[1].subject)};
	FILE *file = fopen(path, "w"); assert(file);
	fputs("#include \"component.h\"\n#include <assert.h>\n"
		"static struct ap_enum_Bool unary32(void *p,int32_t n) {(void)n;return (struct ap_enum_Bool){*(const unsigned *)p};}\n"
		"static struct ap_enum_Bool unary64(void *p,int64_t n) {(void)n;return (struct ap_enum_Bool){*(const unsigned *)p};}\n"
		"static struct ap_enum_Bool binary32(void *p,int32_t x,int32_t y) {(void)x;return unary32(p,y);}\n"
		"static struct ap_enum_Bool binary64(void *p,int64_t x,int64_t y) {(void)x;return unary64(p,y);}\n"
		"int main(void) {struct ap_c_arena arena={0};unsigned truth;struct ap_enum_Bool flag;int32_t length;\n"
		"struct ap_c_predicate_signed1_i32_r4_Bool u32={&truth,unary32};struct ap_c_predicate_signed1_i64_r4_Bool u64={&truth,unary64};\n"
		"struct ap_c_predicate_signed2_i32_r4_Bool b32={&truth,binary32};struct ap_c_predicate_signed2_i64_r4_Bool b64={&truth,binary64};\n", file);
	size_t comparisons = 0;
	for (unsigned wide = 0; wide < 2; ++wide) for (unsigned truth = 0; truth < 2; ++truth) {
		const int64_t values[] = {wide ? INT64_MIN : INT32_MIN,-1,0,1,wide ? INT64_MAX : INT32_MAX};
		const struct pg_term *unary = op(g, &pg_thunk_operation, callbacks[wide][truth]->core);
		const struct pg_term *pair = op(g, &pg_thunk_operation, app(g, binary[wide]->core, pg_reference(g, pg_data_constructor(boolean, truth))));
		fprintf(file, "truth=%u;\n", truth);
		for (size_t left = 0; left < 5; ++left) {
			char left_literal[48]; literal(left_literal, wide, values[left]);
			const struct pg_term *call = app(g, app(g, plan->exports[wide ? 2 : 0].subject->core, unary), number(g, wide, values[left]));
			const struct pg_term *result = evaluate(p, call); assert(result->kind == PG_REFERENCE);
			assert(result->as.reference == pg_data_constructor(boolean, truth));
			fprintf(file, "assert(!ap_export_apply%u(&arena,u%u,%s,&flag)&&flag.tag==%u);\n", wide?64:32,wide?64:32,left_literal,truth); ++comparisons;
			for (size_t right = 0; right < 5; ++right) {
				char right_literal[48]; literal(right_literal, wide, values[right]);
				call = app(g, app(g, app(g, plan->exports[wide ? 3 : 1].subject->core, pair), number(g, wide, values[left])), number(g, wide, values[right]));
				result = evaluate(p, call); assert(result->kind == PG_REFERENCE && result->as.reference == pg_data_constructor(boolean, truth));
				fprintf(file, "assert(!ap_export_choose%u(&arena,b%u,%s,%s,&flag)&&flag.tag==%u);\n",
					wide?64:32,wide?64:32,left_literal,right_literal,truth); ++comparisons;
			}
		}
		for (size_t count = 0, combinations = 1; count <= 3; ++count, combinations *= 5)
			for (size_t code = 0; code < combinations; ++code) {
				int64_t values[3] = {0}; size_t digits = code;
				for (size_t i = 0; i < count; ++i, digits /= 5) {
					const int64_t choices[] = {wide ? INT64_MIN : INT32_MIN,-1,0,1,wide ? INT64_MAX : INT32_MAX}; values[i] = choices[digits % 5];
				}
				const struct pg_term *input = pg_reference(g, pg_data_constructor(lists[wide], wide ? 1 : 0));
				for (size_t i = count; i; --i) {
					const struct pg_term *cell = pg_reference(g, pg_data_constructor(lists[wide], wide ? 0 : 1));
					input = wide ? app(g, app(g, cell, input), number(g, wide, values[i-1])) : app(g, app(g, cell, number(g, wide, values[i-1])), input);
				}
				fprintf(file, "{int%u_t a[3]={", wide?64:32);
				for (size_t i = 0; i < 3; ++i) { char text[48]; literal(text, wide, values[i]); fprintf(file, "%s%s", i?",":"",text); }
				fprintf(file, "};const struct ap_data_Numbers%u *input,*out;assert(!ap_from_Numbers%u(&arena,a,%zu,&input));\n",wide?64:32,wide?64:32,count);
				for (unsigned selected = 0; selected < 2; ++selected) {
					const struct pg_term *call = app(g, app(g, plan->exports[(wide ? 8 : 6) + selected].subject->core, selected ? pair : unary), input);
					if (selected) call = app(g, call, number(g, wide, 0));
					call = app(g, plan->exports[wide ? 11 : 10].subject->core, op(g, &pg_total_result_operation, call));
					const struct pg_term *result = evaluate(p, call); int64_t length;
					assert(result->kind == PG_REFERENCE && pg_host_integer_view(result->as.reference, &length));
					fprintf(file, "assert(!ap_export_%s%u(&arena,%s%u,input%s,&out));assert(!ap_export_length%u(&arena,out,&length)&&length==%" PRId64 ");\n",
						selected?"select":"filter",wide?64:32,selected?"b":"u",wide?64:32,selected?",0":"",wide?64:32,length); ++comparisons;
				}
				fputs("ap_arena_Numbers32_destroy(&arena);}\n", file);
			}
	}
	assert(comparisons == 1368); fputs("return 0;}\n", file); assert(!fclose(file));
}

int main(int argc, char **argv)
{
	assert(argc == 5); struct pg_c_link_plan plan = {0}; size_t line; const char *error;
	assert(!pg_c_link_read(&plan, argv[1], &line, &error) && plan.lowering == PG_C_PREDICATE_SIGNED_DIRECT);
	FILE *file = fopen(plan.artifact, "rb"); assert(file); size_t count; struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_artifact_read_file(file, SIZE_MAX, &count, &roots); assert(!fclose(file) && p && count);
	for (size_t i = 0; i < plan.count; ++i) plan.exports[i].subject = select_subject(p, roots[0], plan.names[i]);
	for (size_t i = 0; i < plan.enum_count; ++i) plan.enums[i].subject = select_subject(p, roots[0], plan.enum_names[i]);
	for (size_t i = 0; i < plan.data_count; ++i) plan.data[i].subject = select_subject(p, roots[0], plan.data_names[i]);
	const struct pg_occurrence *callbacks[2][2] = {{select_subject(p,roots[0],"drop32"),select_subject(p,roots[0],"keep32")},
		{select_subject(p,roots[0],"drop64"),select_subject(p,roots[0],"keep64")}};
	const struct pg_occurrence *binary[2] = {select_subject(p,roots[0],"left32"),select_subject(p,roots[0],"left64")};
	FILE *source = fopen(argv[2], "w"), *header = fopen(argv[3], "w"); assert(source && header);
	size_t terms = p->graph.terms.count, objects = p->graph.objects.count, proofs = p->typing.proofs.count, occurrences = p->typing.occurrences.count;
	struct pg_c_native_contract contract = {0}; emitting = 1;
	assert(!pg_c_emit_predicate_signed(source, header, plan.count, plan.exports, SIZE_MAX,
		plan.enum_count, plan.enums, 0, NULL, plan.data_count, plan.data, &contract, &error)); emitting = 0;
	assert(contract.recursive && contract.copy_out && !contract.natural && !contract.branching);
	assert(p->graph.terms.count == terms && p->graph.objects.count == objects);
	assert(p->typing.proofs.count == proofs && p->typing.occurrences.count == occurrences);
	assert(!fclose(source) && !fclose(header)); oracle(p, &plan, callbacks, binary, argv[4]);
	pg_program_destroy(p); pg_c_link_destroy(&plan);
	puts("Signed predicate emission inert; separate Core supplies 120 tags/1248 lengths from admitted constant predicates"); return 0;
}
