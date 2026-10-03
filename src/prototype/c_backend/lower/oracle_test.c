#include "scalar.h"
#include "classifier.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
#include "support.h"
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

static void inert_representations(struct pg_typing *t, FILE *source, size_t count, const struct pg_c_export *exports,
	size_t enum_count, const struct pg_c_export *enums, size_t data_count, const struct pg_c_export *data, int status)
{
	FILE *header = tmpfile();
	assert(header);
	size_t terms = t->graph->terms.count, objects = t->graph->objects.count;
	size_t proofs = t->proofs.count, subjects = t->occurrences.count;
	const char *error;
	emitting = 1;
	int result = (data_count || enum_count) ? pg_c_emit_native(source, header, count, exports, SIZE_MAX, enum_count, enums, data_count, data, &error) :
		pg_c_emit_scalar(source, header, count, exports, SIZE_MAX, &error);
	assert(result == status);
	emitting = 0;
	assert(terms == t->graph->terms.count && objects == t->graph->objects.count);
	assert(proofs == t->proofs.count && subjects == t->occurrences.count);
	if (status) assert(!ftell(source) && !ftell(header));
	assert(!fclose(header));
}

static void inert_emit(struct pg_typing *t, FILE *source, size_t count, const struct pg_c_export *exports,
	size_t enum_count, const struct pg_c_export *enums, int status)
{
	inert_representations(t, source, count, exports, enum_count, enums, 0, NULL, status);
}

static void inert(struct pg_typing *t, FILE *source, size_t count, const struct pg_c_export *exports, int status)
{
	inert_emit(t, source, count, exports, 0, NULL, status);
}

static void enum_match(struct pg_typing *t, const char *path)
{
	struct pg_graph *g = t->graph;
	struct pg_data_constructor_input inputs[2] = {{0}, {0}};
	const struct pg_data_declaration *d = pg_data_declaration(g, NULL, NULL, 2, inputs);
	assert(d);
	const struct pg_data_layout *layout = pg_data_declaration_layout(d);
	const struct pg_term *family = pg_reference(g, pg_data_declaration_family(d));
	struct pg_c_export enums[2] = {{"Choice", pg_occurrence(t, PG_JUDGEMENT_VALUE_TYPE, NULL, family,
		pg_universe(g, 0), NULL, 0, NULL)}};
	const struct pg_object *x = pg_binder(g);
	struct pg_match_clause clauses[2];
	for (size_t i = 0; i < 2; ++i) clauses[i] = (struct pg_match_clause){pg_data_constructor(layout, i),
		op(g, &pg_return_operation, pg_reference(g, pg_host_integer(g, pg_host_type("Int32"), (int64_t)(7 + i))))};
	const struct pg_term *body = pg_data_match(g, layout, pg_reference(g, x), 2, clauses);
	const struct pg_term *classifier = pg_pi(g, family, x, pg_computation_type(g, PG_TOTALITY_TOTAL,
		pg_effect_row(g, 0, NULL), pg_reference(g, pg_host_type("Int32"))));
	struct pg_c_export export = {"choice", occurrence(t, pg_lambda(g, x, body), classifier)};
	FILE *source = fopen(path, "w");
	assert(source);
	inert_emit(t, source, 1, &export, 1, enums, 0);
	fputs("#include <assert.h>\nint main(void) { int32_t out;\n", source);
	for (size_t i = 0; i < 2; ++i) {
		int64_t expected = evaluate(g, app(g, export.subject->core, pg_reference(g, pg_data_constructor(layout, i))));
		fprintf(source, "assert(!ap_export_choice((struct ap_enum_Choice){%zu}, &out) && out == %" PRId64 ");\n", i, expected);
	}
	fputs("}\n", source);
	fclose(source);
	/* Distinct declarations may intentionally share an erased layout. A backend
	 * cannot use that layout to choose between two nominal C type selections. */
	const struct pg_data_declaration *other = pg_data_declaration_at_layout(g, layout, NULL, NULL, 2, inputs);
	assert(other);
	enums[1] = (struct pg_c_export){"Other", pg_occurrence(t, PG_JUDGEMENT_VALUE_TYPE, NULL,
		pg_reference(g, pg_data_declaration_family(other)), pg_universe(g, 0), NULL, 0, NULL)};
	source = tmpfile();
	assert(source);
	inert_emit(t, source, 1, &export, 2, enums, -1);
	fclose(source);
}

static void shared_calls(struct pg_typing *t)
{
	struct pg_graph *g = t->graph;
	const struct pg_object *x = pg_binder(g);
	const struct pg_term *vx = pg_reference(g, x), *one = pg_reference(g, pg_host_integer(g, pg_host_type("Int32"), 1));
	const struct pg_term *previous = pg_lambda(g, x, op(g, &pg_return_operation, vx));
	assert(pg_support_contains(vx, x) == 1 && pg_support_contains(previous, x) == 0);
	for (size_t i = 0; i < 96; ++i) {
		const struct pg_term *left = op(g, &pg_total_result_operation, app(g, previous, vx));
		const struct pg_term *next = op(g, &pg_total_result_operation, app(g, op(g, pg_host_function(0), vx), one));
		const struct pg_term *right = op(g, &pg_total_result_operation, app(g, previous, next));
		previous = pg_lambda(g, x, app(g, op(g, pg_host_function(0), left), right));
	}
	const struct pg_term *type = pg_computation_type(g, PG_TOTALITY_TOTAL,
		pg_effect_row(g, 0, NULL), pg_reference(g, pg_host_type("Int32")));
	type = pg_pi(g, pg_reference(g, pg_host_type("Int32")), x, type);
	struct pg_c_export export = {"shared", occurrence(t, previous, type)};
	FILE *source = tmpfile();
	assert(source);
	inert(t, source, 1, &export, 0);
	assert(ftell(source) < 100000);
	rewind(source);
	char line[1024];
	size_t declarations = 0;
	while (fgets(line, sizeof(line), source)) if (!strncmp(line, "static uint64_t c", 17)) ++declarations;
	/* One public entry body plus 97 shared callees, declaration and definition. */
	assert(declarations == 196);
	fclose(source);
}

static void data_match(struct pg_typing *t, const char *path)
{
	struct pg_graph *g = t->graph;
	const struct pg_object *x = pg_binder(g), *y = pg_binder(g), *p = pg_binder(g);
	const struct pg_term *int32 = pg_reference(g, pg_host_type("Int32")), *int64 = pg_reference(g, pg_host_type("Int64"));
	const struct pg_context *fields = pg_context_bind(t, NULL, x, int32, PG_JUDGEMENT_VALUE);
	fields = pg_context_bind(t, fields, y, int64, PG_JUDGEMENT_VALUE);
	assert(fields);
	struct pg_data_constructor_input inputs[2] = {{0}, {.fields = fields}};
	const struct pg_data_declaration *d = pg_data_declaration(g, NULL, NULL, 2, inputs);
	assert(d);
	const struct pg_data_layout *layout = pg_data_declaration_layout(d);
	const struct pg_term *family = pg_reference(g, pg_data_declaration_family(d)), *vp = pg_reference(g, p);
	struct pg_c_export data[2] = {{"Pair", pg_occurrence(t, PG_JUDGEMENT_VALUE_TYPE, NULL, family,
		pg_universe(g, 0), NULL, 0, NULL)}};
	struct pg_c_export exports[4];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *result_type = i ? int64 : int32;
		const struct pg_term *payload = i ? pg_reference(g, y) : pg_reference(g, x);
		const struct pg_term *zero = pg_reference(g, pg_host_integer(g, pg_host_type(i ? "Int64" : "Int32"), 0));
		struct pg_match_clause clauses[2] = {
			{pg_data_constructor(layout, 0), op(g, &pg_return_operation, zero)},
			{pg_data_constructor(layout, 1), pg_lambda(g, x, pg_lambda(g, y, op(g, &pg_return_operation, payload)))}
		};
		const struct pg_term *body = pg_data_match(g, layout, vp, 2, clauses);
		const struct pg_term *type = pg_pi(g, family, p, pg_computation_type(g, PG_TOTALITY_TOTAL, pg_effect_row(g, 0, NULL), result_type));
		exports[i] = (struct pg_c_export){i ? "wide" : "small", occurrence(t, pg_lambda(g, p, body), type)};
	}
	const struct pg_term *constructed = app(g, op(g, pg_data_constructor(layout, 1), pg_reference(g, x)), pg_reference(g, y));
	const struct pg_term *type = pg_computation_type(g, PG_TOTALITY_TOTAL, pg_effect_row(g, 0, NULL), family);
	exports[2] = (struct pg_c_export){"make", occurrence(t, pg_lambda(g, x, pg_lambda(g, y, op(g, &pg_return_operation, constructed))),
		pg_pi(g, int32, x, pg_pi(g, int64, y, type)))};
	exports[3] = (struct pg_c_export){"echo", occurrence(t, pg_lambda(g, p, op(g, &pg_return_operation, vp)), pg_pi(g, family, p, type))};
	FILE *source = fopen(path, "w");
	assert(source);
	inert_representations(t, source, 4, exports, 0, NULL, 1, data, 0);
	fputs("#include <assert.h>\nint main(void) {struct ap_data_Pair pair, echo; int32_t a; int64_t b;\n", source);
	const int32_t small[] = {INT32_MIN, -1, 0, INT32_MAX};
	const int64_t wide[] = {INT64_MIN, -1, 0, INT64_MAX};
	for (size_t i = 0; i < 4; ++i) for (size_t j = 0; j < 4; ++j) {
		const struct pg_term *input = app(g, op(g, pg_data_constructor(layout, 1),
			pg_reference(g, pg_host_integer(g, pg_host_type("Int32"), small[i]))),
			pg_reference(g, pg_host_integer(g, pg_host_type("Int64"), wide[j])));
		int64_t expected32 = evaluate(g, app(g, exports[0].subject->core, input));
		int64_t expected64 = evaluate(g, app(g, exports[1].subject->core, input));
		fprintf(source, "assert(!ap_export_make(%" PRId32 ", ", small[i]);
		if (wide[j] == INT64_MIN) fputs("INT64_MIN", source);
		else fprintf(source, "INT64_C(%" PRId64 ")", wide[j]);
		fputs(", &pair)); assert(!ap_export_echo(pair, &echo));\n", source);
		fprintf(source, "assert(!ap_export_small(echo, &a) && (uint32_t)a == UINT32_C(%" PRIu32 "));\n", (uint32_t)expected32);
		fprintf(source, "assert(!ap_export_wide(echo, &b) && (uint64_t)b == UINT64_C(%" PRIu64 "));\n", (uint64_t)expected64);
	}
	fputs("}\n", source);
	assert(!fclose(source));
	const struct pg_data_declaration *other = pg_data_declaration_at_layout(g, layout, NULL, NULL, 2, inputs);
	assert(other);
	data[1] = (struct pg_c_export){"Other", pg_occurrence(t, PG_JUDGEMENT_VALUE_TYPE, NULL,
		pg_reference(g, pg_data_declaration_family(other)), pg_universe(g, 0), NULL, 0, NULL)};
	source = tmpfile(); assert(source);
	inert_representations(t, source, 4, exports, 0, NULL, 2, data, -1);
	fclose(source);
}

int main(int argc, char **argv)
{
	assert(argc == 4);
	struct pg_graph g;
	struct pg_typing t;
	assert(!pg_graph_init(&g) && !pg_typing_init(&t, &g));
	enum_match(&t, argv[2]);
	data_match(&t, argv[3]);
	shared_calls(&t);
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
