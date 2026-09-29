#include "emit.h"
#include "computation.h"
#include "classifier.h"
#include "evidence.h"
#include "host.h"
#include "identity.h"
#include "iadt.h"
#include <assert.h>
#include <string.h>

/* Raw Oracle correspondence tests do not claim source/kernel acceptance.
 * The command-level tests separately require whole-module checked exports. */
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

static const struct pg_term *apply(struct pg_graph *graph, const struct pg_term *f, const struct pg_term *x)
{
	const struct pg_term *term = pg_application(graph, f, x);
	assert(term);
	return term;
}

static const struct pg_term *returned(struct pg_graph *graph, const struct pg_term *term)
{
	return apply(graph, pg_reference(graph, &pg_return_operation), term);
}

static const struct pg_term *text(struct pg_graph *graph, const char *bytes)
{
	return pg_reference(graph, pg_host_literal(graph, pg_host_type("Text"),
		strlen(bytes), (const unsigned char *)bytes));
}

static const struct pg_term *pure_result(struct pg_graph *graph, const struct pg_term *term)
{
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, term);
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	const struct pg_term *answer = pg_eval_readback(&machine, graph);
	pg_eval_destroy(&machine);
	assert(answer && answer->kind == PG_APPLICATION);
	assert(answer->as.application.function == pg_reference(graph, &pg_return_operation));
	return answer->as.application.argument;
}

static void record_result(struct pg_graph *graph, const struct pg_term *term, FILE *expected)
{
	const struct pg_term *answer = pure_result(graph, term);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, graph, answer);
	assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
	answer = pg_eval_readback(&machine, graph);
	pg_eval_destroy(&machine);
	assert(answer->kind == PG_REFERENCE);
	const struct pg_object *type;
	size_t length;
	const unsigned char *bytes;
	assert(pg_host_literal_view(answer->as.reference, &type, &length, &bytes));
	assert(type == pg_host_type("Text"));
	assert(fwrite(bytes, 1, length, expected) == length);
}

static const struct pg_term *print_result(struct pg_graph *graph, const struct pg_term *term,
	const struct pg_term *tail)
{
	const struct pg_object *x = pg_binder(graph), *ignored = pg_binder(graph);
	const struct pg_term *request = pg_computation_request(graph, pg_host_print(graph),
		pg_reference(graph, x), pg_lambda(graph, ignored, tail));
	return pg_computation_fold(graph, term, pg_lambda(graph, x, request), 0, NULL);
}

static const struct pg_term *handlers(struct pg_graph *graph)
{
	const struct pg_term *type = pg_reference(graph, pg_host_type("Text"));
	const struct pg_object *a = pg_operation_label_create(graph, type, type);
	const struct pg_object *b = pg_operation_label_create(graph, type, type);
	assert(a != b);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_term *body = pg_computation_request(graph, b, pg_reference(graph, x),
		pg_lambda(graph, y, returned(graph, pg_reference(graph, y))));
	body = pg_computation_request(graph, a, text(graph, "first"), pg_lambda(graph, x, body));
	struct pg_operation_clause clauses[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_object *payload = pg_binder(graph), *resume = pg_binder(graph);
		const struct pg_term *function = apply(graph, pg_reference(graph, &pg_force_operation), pg_reference(graph, resume));
		const struct pg_term *result = apply(graph, function, i ? pg_reference(graph, payload) : text(graph, "handled"));
		clauses[i] = (struct pg_operation_clause){i ? b : a,
			pg_lambda(graph, payload, pg_lambda(graph, resume, result))};
	}
	return pg_computation_fold(graph, body, pg_lambda(graph, z, returned(graph, pg_reference(graph, z))), 2, clauses);
}

static const struct pg_term *checked_transport(struct pg_typing *typing,
	enum pg_identity_direction direction)
{
	const struct pg_evidence *context = pg_prove_empty_context(typing);
	const struct pg_evidence *type = pg_prove_host_type(typing, context, pg_host_type("Text"));
	const struct pg_evidence *universe = pg_prove_universe(typing, context, 0);
	const struct pg_evidence *family = pg_prove_reflexivity(typing, universe, pg_prove_type_value(typing, type));
	const struct pg_term *literal = text(typing->graph, direction == PG_IDENTITY_RIGHT ? "right" : "left");
	const struct pg_evidence *value = pg_prove_host_value(typing, type, literal->as.reference);
	const struct pg_evidence *transport = pg_prove_identity_transport(typing, family, value, direction);
	assert(transport);
	const struct pg_evidence *computation = pg_prove_return(typing, transport);
	assert(computation);
	return pg_evidence_subject(computation)->core;
}

static const struct pg_term *unary(struct pg_graph *graph, const struct pg_object *operation, const struct pg_term *input)
{
	return apply(graph, pg_reference(graph, operation), input);
}

static size_t identity_cases(struct pg_graph *g, const struct pg_term **parts)
{
	size_t n = 0;
	const struct pg_object *x = pg_binder(g), *y = pg_binder(g);
	const struct pg_term *vx = pg_reference(g, x), *vy = pg_reference(g, y);
	const struct pg_term *a = text(g, "left"), *b = text(g, "right"), *p = text(g, "chosen");
	const struct pg_term *id = pg_lambda(g, x, vx);
	parts[n++] = returned(g, pg_identity_apply(g, id, a, b, p));
	parts[n++] = returned(g, pg_identity_apply(g, pg_lambda(g, x, apply(g, pg_lambda(g, y, vy), vx)), a, b, p));
	const struct pg_term *partial = pg_identity_apply(g, pg_lambda(g, x, pg_lambda(g, y, vx)), a, b, p);
	parts[n++] = returned(g, apply(g, pg_identity_instance(g, partial, b, a), text(g, "not-chosen")));
	partial = pg_identity_apply(g, pg_lambda(g, x, pg_lambda(g, x, vx)), a, b, p);
	parts[n++] = returned(g, apply(g, pg_identity_instance(g, partial, a, b), p));
	const struct pg_term *type = pg_reference(g, pg_host_type("Text"));
	const struct pg_term *captured = pg_identity_apply(g, pg_lambda(g, y, vx), a, b, p);
	parts[n++] = returned(g, pg_identity_transport(g, apply(g, pg_lambda(g, x, captured), type), a, PG_IDENTITY_RIGHT));
	const struct pg_term *self = pg_lambda(g, x, apply(g, vx, vx));
	const struct pg_term *omega = apply(g, self, self);
	const struct pg_term *constant = pg_identity_apply(g, pg_lambda(g, y, type), omega, omega, omega);
	parts[n++] = returned(g, pg_identity_transport(g, constant, a, PG_IDENTITY_LEFT));
	/* Chosen constructor-field paths, not equality of the supplied endpoints,
	 * determine the Match action's center. These are raw reduction fixtures. */
	const size_t arities[] = {0, 1};
	const struct pg_data_layout *layout = pg_data_layout(g, 2, arities);
	const struct pg_object *zero = pg_data_constructor(layout, 0), *succ = pg_data_constructor(layout, 1);
	const struct pg_match_clause clauses[] = {{zero, text(g, "zero")}, {succ, id}};
	const struct pg_term *match = pg_data_match(g, layout, vy, 2, clauses);
	const struct pg_term *path = apply(g, pg_identity_instance(g,
		pg_identity_action(g, pg_reference(g, succ)), a, b), p);
	parts[n++] = returned(g, pg_identity_apply(g, pg_lambda(g, y, match),
		apply(g, pg_reference(g, succ), a), apply(g, pg_reference(g, succ), b), path));
	for (unsigned direction = 0; direction < 2; ++direction) {
		/* Distinct endpoints keep the scoped U/F/Pi map visible. The center
		 * is an explicit action of Text; this does not assert these raw
		 * endpoint triples are well typed. Checked source coverage is separate. */
		const struct pg_term *center = pg_identity_action(g, type);
		const struct pg_term *uf = pg_thunk_type(g, pg_return_type(g, vx));
		const struct pg_term *family = pg_identity_apply(g, pg_lambda(g, x, uf), a, b, center);
		const struct pg_term *quoted = unary(g, &pg_thunk_operation, apply(g, pg_lambda(g, y, returned(g, vy)), p));
		parts[n++] = unary(g, &pg_force_operation, pg_identity_transport(g, family, quoted, direction));
		const struct pg_term *pi = pg_pi(g, vx, y, pg_return_type(g, vx));
		family = pg_identity_apply(g, pg_lambda(g, x, pg_thunk_type(g, pi)), a, b, center);
		quoted = unary(g, &pg_thunk_operation, pg_lambda(g, y, returned(g, vy)));
		parts[n++] = apply(g, unary(g, &pg_force_operation, pg_identity_transport(g, family, quoted, direction)), p);
		const struct pg_term *lift = pg_identity_lift(g, pg_identity_action(g, pg_universe(g, 0)), type, direction);
		parts[n++] = returned(g, pg_identity_transport(g, lift, p, direction));
	}
	/* A Return payload must remain lazy through transport: the subsequent
	 * Fold discards it without trying to execute an opaque center's field. */
	const struct pg_term *uf = pg_thunk_type(g, pg_return_type(g, vx));
	const struct pg_term *family = pg_identity_apply(g, pg_lambda(g, x, uf), a, b, text(g, "opaque"));
	const struct pg_term *mapped = pg_identity_transport(g, family,
		unary(g, &pg_thunk_operation, returned(g, p)), PG_IDENTITY_RIGHT);
	parts[n++] = pg_computation_fold(g, unary(g, &pg_force_operation, mapped), pg_lambda(g, y, returned(g, a)), 0, NULL);
	return n;
}

static void neutral_boundaries(struct pg_typing *typing, const struct pg_term *classifier, const char *prefix)
{
	struct pg_graph *g = typing->graph;
	const struct pg_object *x = pg_binder(g);
	const struct pg_term *a = text(g, "a"), *b = text(g, "b");
	const struct pg_term *family = pg_lambda(g, x, apply(g, pg_universe(g, 0), pg_reference(g, x)));
	const struct pg_term *paths[] = {text(g, "chosen-loop"), pg_identity_action(g, b)};
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_term *relation = pg_identity_apply(g, family, a, a, paths[i]);
		const struct pg_term *field = pg_identity_transport(g, relation, text(g, "must-not-erase"), PG_IDENTITY_RIGHT);
		struct pg_eval machine;
		pg_computation_eval_init(&machine, g, field);
		assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
		assert(pg_identity_field_view(pg_eval_readback(&machine, g), NULL, NULL, NULL, NULL));
		pg_eval_destroy(&machine);
		const struct pg_term *body = print_result(g, returned(g, field), returned(g, a));
		const struct pg_occurrence *root = pg_occurrence(typing, PG_JUDGEMENT_COMPUTATION, NULL, body, classifier, NULL, 0, NULL);
		char path[4096];
		int length = snprintf(path, sizeof(path), "%s.fail-%zu.c", prefix, i);
		assert(length > 0 && (size_t)length < sizeof(path));
		FILE *file = fopen(path, "w");
		const char *error;
		assert(file);
		emitting = 1;
		assert(!pg_c_emit(file, root, &error));
		emitting = 0;
		assert(!fclose(file));
	}
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	struct pg_graph graph;
	struct pg_typing typing;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	FILE *output = fopen(argv[1], "w"), *expected = fopen(argv[2], "wb");
	assert(output && expected);
	const struct pg_term *parts[32];
	parts[0] = handlers(&graph);
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_object *function = pg_host_function(i), *domain, *codomain;
		size_t arity;
		assert(pg_host_function_view(function, &domain, &codomain, &arity));
		int64_t left = (i % 4 == 1 || i % 4 == 3)
			? (i < 4 ? INT32_MIN : INT64_MIN) : (i < 4 ? INT32_MAX : INT64_MAX);
		const struct pg_term *call = apply(&graph, pg_reference(&graph, function),
			pg_reference(&graph, pg_host_integer(&graph, domain, left)));
		if (arity == 2) call = apply(&graph, call, pg_reference(&graph, pg_host_integer(&graph, domain, i % 4 == 2 ? 2 : 1)));
		const struct pg_object *x = pg_binder(&graph);
		const struct pg_term *format = apply(&graph, pg_reference(&graph, pg_host_function(i < 4 ? 8 : 9)), pg_reference(&graph, x));
		parts[i + 1] = pg_computation_fold(&graph, call, pg_lambda(&graph, x, format), 0, NULL);
	}
	parts[9] = checked_transport(&typing, PG_IDENTITY_RIGHT);
	parts[10] = checked_transport(&typing, PG_IDENTITY_LEFT);
	const struct pg_term *family = pg_identity_action(&graph, pg_reference(&graph, pg_host_type("Text")));
	const struct pg_object *binder = pg_binder(&graph);
	const struct pg_term *open = pg_identity_transport(&graph, family, pg_reference(&graph, binder), PG_IDENTITY_RIGHT);
	parts[11] = apply(&graph, pg_lambda(&graph, binder, returned(&graph, open)), text(&graph, "bound"));
	const struct pg_term *universe = pg_evidence_subject(pg_prove_universe(&typing, pg_prove_empty_context(&typing), 0))->core;
	/* Raw correspondence at an inert classifier head, independent of typing. */
	parts[12] = returned(&graph, pg_identity_transport(&graph,
		pg_identity_action(&graph, universe), text(&graph, "universe"), PG_IDENTITY_LEFT));
	size_t count = 13 + identity_cases(&graph, parts + 13);
	assert(count <= sizeof(parts) / sizeof(*parts));
	/* Prepending computations reverses their execution order. */
	const struct pg_term *body = returned(&graph, text(&graph, ""));
	for (size_t i = 0; i < count; ++i) body = print_result(&graph, parts[i], body);
	for (size_t i = count; i; --i) record_result(&graph, parts[i - 1], expected);
	assert(!fclose(expected));
	const struct pg_object *print = pg_host_print(&graph);
	const struct pg_term *classifier = pg_computation_type(&graph, PG_TOTALITY_TOTAL,
		pg_effect_row(&graph, 1, &print), pg_reference(&graph, pg_host_type("Text")));
	const struct pg_occurrence *root = pg_occurrence(&typing, PG_JUDGEMENT_COMPUTATION,
		NULL, body, classifier, NULL, 0, NULL);
	size_t terms = graph.terms.count, objects = graph.objects.count, proofs = typing.proofs.count;
	const char *error;
	emitting = 1;
	assert(!pg_c_emit(output, root, &error));
	emitting = 0;
	assert(!fclose(output));
	assert(terms == graph.terms.count && objects == graph.objects.count && proofs == typing.proofs.count);
	struct pg_c_export exports[] = {{"first", root}, {"second", root}};
	output = tmpfile();
	assert(output);
	emitting = 1;
	assert(!pg_c_emit_exports(output, 2, exports, SIZE_MAX, &error));
	assert(terms == graph.terms.count && objects == graph.objects.count && proofs == typing.proofs.count);
	fclose(output);
	output = tmpfile();
	assert(output);
	exports[1].alias = "first";
	assert(pg_c_emit_exports(output, 2, exports, SIZE_MAX, &error) == -1 && ftell(output) == 0);
	exports[1].alias = "bad-alias";
	assert(pg_c_emit_exports(output, 2, exports, SIZE_MAX, &error) == -1 && ftell(output) == 0);
	exports[1].alias = "second";
	assert(pg_c_emit_exports(output, 2, exports, 2, &error) == -1 && ftell(output) == 0);
	fclose(output);
	emitting = 0;
	/* An unknown Oracle is not silently erased or turned into a host callback. */
	static const struct pg_object_class unknown_class = {"unsupported-test"};
	static const struct pg_object unknown = {PG_SEMANTIC_OBJECT, &unknown_class};
	root = pg_occurrence(&typing, PG_JUDGEMENT_COMPUTATION, NULL,
		returned(&graph, pg_identity_transport(&graph, family, pg_reference(&graph, &unknown), PG_IDENTITY_LEFT)), classifier, NULL, 0, NULL);
	output = tmpfile();
	assert(output && pg_c_emit(output, root, &error) == -1 && ftell(output) == 0);
	assert(strstr(error, "unsupported"));
	fclose(output);
	/* Validate every selected root before writing any of the common component. */
	exports[1].subject = root;
	output = tmpfile();
	assert(output && pg_c_emit_exports(output, 2, exports, SIZE_MAX, &error) == -1 && ftell(output) == 0);
	fclose(output);
	neutral_boundaries(&typing, classifier, argv[1]);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("C Oracle tests: inert emission, chosen Identity actions, scoped U/F/Pi maps, two clauses and integer operations passed");
}
