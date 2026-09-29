#include "emit.h"
#include "computation.h"
#include "classifier.h"
#include "evidence.h"
#include "host.h"
#include "identity.h"
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

static void unsupported_identity(struct pg_typing *typing, const struct pg_term *classifier)
{
	struct pg_graph *graph = typing->graph;
	const struct pg_term *type = pg_reference(graph, pg_host_type("Text"));
	const struct pg_term *literal = text(graph, "retained");
	const struct pg_object *binder = pg_binder(graph);
	const struct pg_term *variable = pg_reference(graph, binder);
	const struct pg_term *cases[] = {
		pg_identity_lift(graph, pg_identity_action(graph, type), literal, PG_IDENTITY_RIGHT),
		pg_identity_transport(graph, variable, literal, PG_IDENTITY_RIGHT),
		pg_identity_transport(graph, pg_identity_action(graph, variable), literal, PG_IDENTITY_LEFT),
		pg_identity_transport(graph, pg_identity_action(graph, pg_lambda(graph, binder, variable)), literal, PG_IDENTITY_RIGHT)
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		const struct pg_occurrence *root = pg_occurrence(typing, PG_JUDGEMENT_COMPUTATION,
			NULL, returned(graph, cases[i]), classifier, NULL, 0, NULL);
		FILE *file = tmpfile();
		const char *error;
		assert(file);
		emitting = 1;
		assert(pg_c_emit(file, root, &error) == -1 && ftell(file) == 0);
		emitting = 0;
		assert(strstr(error, "identity"));
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
	const struct pg_term *parts[13];
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
	size_t count = sizeof(parts) / sizeof(*parts);
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
	/* An unknown Oracle is not silently erased or turned into a host callback. */
	static const struct pg_object_class unknown_class = {"unsupported-test"};
	static const struct pg_object unknown = {PG_SEMANTIC_OBJECT, &unknown_class};
	root = pg_occurrence(&typing, PG_JUDGEMENT_COMPUTATION, NULL,
		returned(&graph, pg_identity_transport(&graph, family, pg_reference(&graph, &unknown), PG_IDENTITY_LEFT)), classifier, NULL, 0, NULL);
	output = tmpfile();
	assert(output && pg_c_emit(output, root, &error) == -1 && ftell(output) == 0);
	assert(strstr(error, "unsupported"));
	fclose(output);
	unsupported_identity(&typing, classifier);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("C Oracle tests: inert emission, diagonal transport, unsupported Identity rejection, two clauses and integer operations passed");
}
