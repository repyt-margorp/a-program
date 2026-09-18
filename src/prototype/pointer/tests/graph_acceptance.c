#include "graph_io.h"
#include "context_io.h"
#include "context_payload.h"
#include "dag.h"
#include "evidence.h"
#include "derivation.h"
#include "computation.h"
#include "descriptor_io.h"
#include "effect_inference.h"
#include "wire.h"
#include "iadt.h"
#include "declaration_io.h"

#include <assert.h>
#include <string.h>

static const char *name(void *owner, const struct pg_object *object)
{
	(void)owner;
	static char buffer[64];
	const char *label = pg_classifier_name(object, buffer, sizeof(buffer));
	return label ? label : pg_computation_name(object);
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	const struct pg_object *object = pg_classifier_resolve(owner, label);
	return object ? object : pg_computation_resolve(label);
}

static void rejected_prefixes(FILE *file, const struct pg_graph_codec *codec)
{
	unsigned char bytes[4096];
	rewind(file);
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file) && length > 32);
	for (size_t cut = 0; cut <= length + 3; ++cut) {
		struct pg_graph graph;
		struct pg_typing typing;
		assert(pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
		struct pg_declaration_io io;
		int declarations = codec == &pg_declaration_graph_codec;
		if (declarations) assert(!pg_declaration_io_init(&io, &typing));
		FILE *fragment = tmpfile();
		/* The complete-sized final case has an invalid parent reference. */
		if (cut == length) bytes[32] = 255;
		if (cut == length + 1) { bytes[32] = 0; bytes[40] = PG_JUDGEMENT_COMPUTATION; }
		if (cut == length + 2) { bytes[40] = PG_JUDGEMENT_VALUE; bytes[6] = 1; }
		if (cut == length + 3) bytes[6] = 2;
		size_t size = cut < length ? cut : length;
		assert(fragment && fwrite(bytes, 1, size, fragment) == size);
		rewind(fragment);
		size_t nc = 71, nt = 72;
		const struct pg_context *const *contexts = NULL;
		const struct pg_term *const *terms = NULL;
		assert(pg_contexts_read_descriptors(fragment, &typing, 100, 100, codec, declarations ? (void *)&io : &graph,
			&nc, &contexts, &nt, &terms) == -1);
		assert(nc == 71 && nt == 72 && !contexts && !terms && typing.proofs.count == 0);
		assert(fclose(fragment) == 0);
		if (declarations) pg_declaration_io_destroy(&io);
		pg_typing_destroy(&typing);
		pg_graph_destroy(&graph);
	}
}

static void context_boundaries(struct pg_typing *typing, const struct pg_term *type)
{
	struct pg_context cycle = {.binder = pg_binder(typing->graph), .declared_type = type, .judgement = PG_JUDGEMENT_VALUE};
	cycle.parent = &cycle;
	const struct pg_context *root = &cycle;
	FILE *file = tmpfile();
	assert(file && pg_contexts_write(file, 1, &root, 0, NULL, NULL, NULL) == -1);
	assert(fclose(file) == 0);
	cycle.parent = NULL;
	cycle.indices = &cycle;
	file = tmpfile();
	assert(file && pg_contexts_write(file, 1, &root, 0, NULL, NULL, NULL) == -1);
	assert(fclose(file) == 0);
	cycle.indices = NULL;
	cycle.judgement = PG_JUDGEMENT_COMPUTATION;
	file = tmpfile();
	assert(file && pg_contexts_write(file, 1, &root, 0, NULL, NULL, NULL) == -1);
	assert(fclose(file) == 0);
	root = NULL;
	for (size_t i = 0; i < 10000; ++i)
		root = pg_context_bind(typing, root, pg_binder(typing->graph), type, PG_JUDGEMENT_VALUE);
	assert(root);
	file = tmpfile();
	assert(file && pg_contexts_write(file, 1, &root, 1, &type, NULL, NULL) == 0);
	rewind(file);
	struct pg_graph graph;
	struct pg_typing destination;
	assert(pg_graph_init(&graph) == 0 && pg_typing_init(&destination, &graph) == 0);
	size_t nc, nt, depth;
	const struct pg_context *const *contexts;
	const struct pg_term *const *terms;
	assert(pg_contexts_read(file, &destination, 100000, 0, NULL, NULL,
		&nc, &contexts, &nt, &terms) == 0);
	assert(nc == 1 && nt == 1 && contexts[0]->declared_type == terms[0]);
	assert(pg_context_extension_size(contexts[0], NULL, &depth) == 0 && depth == 10000);
	assert(destination.proofs.count == 0 && fclose(file) == 0);
	pg_typing_destroy(&destination);
	pg_graph_destroy(&graph);
}

static void write_graph(FILE *file, struct pg_graph *graph)
{
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_term *value = pg_reference(graph, x);
	const struct pg_term *body = pg_application(graph, pg_reference(graph, &pg_return_operation), value);
	const struct pg_term *identity = pg_lambda(graph, x, body);
	const struct pg_term *roots[] = {pg_reference(graph, a), pg_reference(graph, b), value, identity, identity};
	struct pg_typing typing;
	assert(pg_typing_init(&typing, graph) == 0);
	const struct pg_term *u = pg_universe(graph, 0);
	const struct pg_context *ca = pg_context_bind(&typing, NULL, a, u, PG_JUDGEMENT_VALUE);
	const struct pg_context *cb = pg_context_bind(&typing, ca, b, u, PG_JUDGEMENT_VALUE);
	const struct pg_context *xa = pg_context_bind(&typing, cb, x, roots[0], PG_JUDGEMENT_VALUE);
	const struct pg_context *xb = pg_context_bind(&typing, cb, x, roots[1], PG_JUDGEMENT_VALUE);
	const struct pg_context *xf = pg_context_bind(&typing, cb, x, roots[0], PG_JUDGEMENT_TYPE_FAMILY);
	const struct pg_object *k = pg_binder(graph);
	const struct pg_context *index = pg_context_bind(&typing, cb, k, u, PG_JUDGEMENT_VALUE);
	const struct pg_context *family = pg_context_intern(&typing, &(struct pg_context){
		.parent = cb, .binder = x, .declared_type = pg_pi(graph, u, k, u),
		.judgement = PG_JUDGEMENT_TYPE_FAMILY, .indices = index});
	const struct pg_context *contexts[] = {ca, cb, xa, xb, NULL, xa, xf, index, family, family};
	struct pg_dag dependencies;
	assert(!pg_dag_init(&dependencies, pg_context_dependency, NULL));
	size_t terms_before = graph->terms.count;
	for (unsigned i = 0; i < 128; ++i)
		assert(!pg_dag_add(&dependencies, family) && !pg_dag_add(&dependencies, xa));
	assert(dependencies.count == 5 && dependencies.first->key == ca && dependencies.last->key == xa);
	assert(pg_dag_find(&dependencies, index)->id < pg_dag_find(&dependencies, family)->id);
	assert(graph->terms.count == terms_before && !typing.proofs.count);
	pg_dag_destroy(&dependencies);
	assert(pg_contexts_write(file, 10, contexts, 5, roots, name, graph) == 0);
	context_boundaries(&typing, roots[0]);
	pg_typing_destroy(&typing);
}

static void read_graph(FILE *file, struct pg_graph *graph)
{
	size_t count = 0;
	const struct pg_term *const *roots = NULL;
	struct pg_typing typing;
	assert(pg_typing_init(&typing, graph) == 0);
	size_t context_count = 0;
	const struct pg_context *const *contexts = NULL;
	assert(pg_contexts_read(file, &typing, 100, 100, resolve, graph,
		&context_count, &contexts, &count, &roots) == 0);
	assert(context_count == 10 && contexts[4] == NULL && contexts[2] == contexts[5]);
	assert(contexts[8] == contexts[9] && contexts[8]->indices == contexts[7]);
	assert(contexts[7]->parent == contexts[1] && contexts[8]->parent == contexts[1]);
	assert(contexts[2] != contexts[3] && contexts[2]->parent == contexts[1]);
	assert(contexts[2] != contexts[6] && contexts[6]->parent == contexts[1]);
	assert(contexts[2]->binder == contexts[6]->binder && contexts[2]->declared_type == contexts[6]->declared_type);
	assert(contexts[2]->judgement == PG_JUDGEMENT_VALUE && contexts[6]->judgement == PG_JUDGEMENT_TYPE_FAMILY);
	assert(contexts[3]->parent == contexts[1] && contexts[1]->parent == contexts[0]);
	assert(typing.proofs.count == 0);
	assert(count == 5 && roots[3] == roots[4]);
	assert(roots[0]->kind == PG_REFERENCE && roots[1]->kind == PG_REFERENCE && roots[2]->kind == PG_REFERENCE);
	const struct pg_object *a = roots[0]->as.reference, *b = roots[1]->as.reference, *x = roots[2]->as.reference;
	assert(a != b && a != x && b != x);
	/* The fixture supplies declarations, not a serialized accepted flag.
	 * Every judgement below is constructed by the ordinary kernel rules. */
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, empty, 0);
	const struct pg_evidence *ca = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *ua = pg_prove_universe(&typing, ca, 0);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, ca, b, ua);
	assert(context && !pg_prove_variable(&typing, context, x));
	assert(pg_evidence_context(context) == contexts[1]);
	const struct pg_evidence *indices = pg_prove_context_extension(&typing, context, contexts[7]->binder,
		pg_prove_universe(&typing, context, 0));
	const struct pg_evidence *family = pg_prove_family_context_extension(&typing, context, x, indices,
		pg_prove_universe(&typing, indices, 0));
	assert(family && pg_evidence_context(family) == contexts[8]);
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *pi[2], *body[2], *identity[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(&typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(&typing, context, x, domain);
		assert(pg_evidence_context(extended) == contexts[2 + i]);
		const struct pg_evidence *codomain = pg_prove_return_type(&typing,
			pg_prove_variable(&typing, extended, types[i]));
		pi[i] = pg_prove_pi(&typing, extended, codomain);
		body[i] = pg_prove_return(&typing, pg_prove_variable(&typing, extended, x));
		const struct pg_evidence *premises[] = {pi[i], body[i]};
		struct pg_derivation_parameters parameters = {0};
		identity[i] = pg_prove_derivation(&typing, PG_LAMBDA_INTRO, &parameters, 2, premises);
		assert(identity[i] && pg_evidence_subject(identity[i])->core == roots[3]);
		assert(pg_prove_lambda(&typing, pi[i], body[i]) == identity[i]);
	}
	assert(identity[0] != identity[1]);
	assert(pg_evidence_subject(identity[0]) != pg_evidence_subject(identity[1]));
	assert(pg_evidence_classifier(identity[0]) != pg_evidence_classifier(identity[1]));
	assert(!pg_prove_lambda(&typing, pi[0], body[1]));
	assert(!pg_prove_lambda(&typing, pi[1], body[0]));
	rejected_prefixes(file, &(const struct pg_graph_codec){.resolve = resolve});
	pg_typing_destroy(&typing);
}

static void descriptor_boundaries(FILE *file)
{
	unsigned char bytes[4096];
	rewind(file);
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file) && length);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		assert(!pg_graph_init(&graph));
		FILE *fragment = tmpfile();
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		size_t count = 71;
		const struct pg_term *const *roots = NULL;
		struct pg_graph_codec codec = pg_builtin_graph_codec;
		/* Complete stream, but no implementation of its descriptor contract. */
		if (cut == length) codec.restore = NULL;
		assert(pg_graph_read_descriptors(fragment, &graph, 1000, 100, &codec, &graph, &count, &roots));
		assert(count == 71 && !roots && !fclose(fragment));
		pg_graph_destroy(&graph);
	}
}

static void operation_graph(FILE *file, struct pg_graph *graph, int writing)
{
	struct pg_typing typing;
	assert(!pg_typing_init(&typing, graph));
	if (writing) {
		const struct pg_term *u = pg_universe(graph, 0);
		const struct pg_object *labels[] = {pg_operation_label_create(graph, u, u), pg_operation_label_create(graph, u, u)};
		const struct pg_effect_row *row = pg_effect_row(graph, 2, labels);
		const struct pg_term *delayed = pg_thunk_type(graph, pg_effect_type(graph, row, u));
		const struct pg_object *higher = pg_operation_label_create(graph, delayed, u), *x = pg_binder(graph);
		const struct pg_term *request = pg_computation_request(graph, labels[0], u,
			pg_lambda(graph, x, pg_application(graph, pg_reference(graph, &pg_return_operation), pg_reference(graph, x))));
		const struct pg_term *roots[] = {pg_reference(graph, labels[0]), pg_reference(graph, labels[1]),
			pg_reference(graph, higher), pg_effect_reference(graph, row), request, request, u};
		size_t terms = graph->terms.count, objects = graph->objects.count;
		assert(!pg_graph_write_descriptors(file, 7, roots, &pg_builtin_graph_codec, graph));
		assert(terms == graph->terms.count && objects == graph->objects.count && !typing.proofs.count);
	} else {
		size_t count = 0;
		const struct pg_term *const *roots = NULL;
		assert(!pg_graph_read_descriptors(file, graph, 1000, 100, &pg_builtin_graph_codec, graph, &count, &roots));
		assert(count == 7 && roots[4] == roots[5] && roots[0] != roots[1] && !typing.proofs.count);
		const struct pg_object *a = roots[0]->as.reference, *b = roots[1]->as.reference;
		const struct pg_term *payload, *response;
		assert(pg_operation_label_types(a, &payload, &response) && payload == roots[6] && response == roots[6]);
		assert(pg_operation_label_types(b, &payload, &response) && payload == roots[6] && response == roots[6]);
		const struct pg_effect_row *row = pg_effect_row_view(roots[3]);
		assert(pg_effect_count(row) == 2 && pg_effect_contains(row, a) == 1 && pg_effect_contains(row, b) == 1);
		assert(pg_operation_label_types(roots[2]->as.reference, &payload, &response) && response == roots[6]);
		assert(payload == pg_thunk_type(graph, pg_effect_type(graph, row, roots[6])));
		const struct pg_object *label;
		const struct pg_term *argument, *continuation;
		assert(pg_computation_request_view(roots[4], &label, &argument, &continuation));
		assert(label == a && argument == roots[6]);
		const struct pg_evidence *empty = pg_prove_empty_context(&typing);
		const struct pg_evidence *u = pg_prove_universe(&typing, empty, 0);
		assert(pg_operation_declaration_at(&typing, a, u, u));
		assert(!pg_operation_declaration_at(&typing, a, pg_prove_universe(&typing, empty, 1), u));
		descriptor_boundaries(file);
	}
	pg_typing_destroy(&typing);
}

static void effect_graph(FILE *file, struct pg_graph *graph, int writing, uint64_t budget)
{
	struct pg_effect_inference work;
	assert(!pg_effect_inference_init(&work, graph));
	const struct pg_effect_row *empty = pg_effect_row(graph, 0, NULL);
	if (writing) {
		const struct pg_term *u = pg_universe(graph, 0);
		const struct pg_object *op = pg_operation_label_create(graph, u, u);
		const struct pg_effect_row *seed = pg_effect_row(graph, 1, &op);
		struct pg_effect_equation *a = pg_effect_equation(&work, seed), *b = pg_effect_equation(&work, empty);
		assert(!pg_effect_dependency(&work, a, empty, b) && !pg_effect_dependency(&work, b, empty, a));
		assert(!pg_effect_contribution(&work, pg_effect_reference(graph, seed), seed, b));
		pg_effect_inference_seal(&work);
		assert(!pg_effect_inference_advance(&work, 1));
		size_t equations, count;
		const struct pg_term *const *definitions;
		assert(!pg_effect_inference_pack(&work, graph, &equations, &count, &definitions));
		assert(equations == 3 && count == 15);
		const struct pg_term **roots = pg_alloc(graph, (count + 1) * sizeof(*roots));
		assert(roots);
		memcpy(roots, definitions, count * sizeof(*roots));
		roots[count] = pg_effect_type_spine(graph,
			pg_reference(graph, pg_effect_equation_parameter(&work, b)), u);
		assert(!pg_wire_write_u64(file, equations));
		assert(!pg_graph_write_descriptors(file, count + 1, roots, &pg_builtin_graph_codec, graph));
	} else {
		uint64_t equations;
		size_t count;
		const struct pg_term *const *roots;
		assert(!pg_wire_read_u64(file, &equations) && equations == 3);
		assert(!pg_graph_read_descriptors(file, graph, 1000, 100, &pg_builtin_graph_codec, graph, &count, &roots));
		assert(count == 16);
		const struct pg_term *parameter, *value;
		assert(pg_effect_type_spine_view(roots[count - 1], &parameter, &value));
		assert(value == pg_universe(graph, 0));
		assert(!pg_effect_inference_unpack(&work, equations, count - 1, roots));
		struct pg_effect_equation *b = NULL;
		const struct pg_effect_row *seed = NULL;
		for (size_t i = 0; i < 2 * equations; i += 2) {
			const struct pg_effect_row *row = pg_effect_row_view(roots[i + 1]);
			if (pg_effect_count(row)) seed = row;
			if (roots[i] == parameter) b = pg_effect_equation_at(&work, parameter->as.reference, empty);
		}
		assert(b && seed && !pg_effect_inference_result(&work, b));
		assert(!pg_effect_inference_advance(&work, 100) && !work.sealed);
		pg_effect_inference_seal(&work);
		int status = 0;
		unsigned steps = 0;
		while (!status) { status = pg_effect_inference_advance(&work, budget); assert(++steps < 20); }
		assert(status == 1 && pg_effect_inference_result(&work, b) == seed);
		assert(pg_effect_equation_seed(&work, b) == empty);
		/* Reject incomplete definitions, duplicate sites and undeclared endpoints.
		 * Failed imports cannot later publish a least solution of a partial graph. */
		for (unsigned variant = 0; variant < 3; ++variant) {
			struct pg_effect_inference bad;
			assert(!pg_effect_inference_init(&bad, graph));
			const struct pg_term *changed[15];
			memcpy(changed, roots, sizeof(changed));
			size_t n = 15;
			if (!variant) --n;
			if (variant == 1) changed[2] = changed[0];
			if (variant == 2) changed[12] = pg_reference(graph, pg_binder(graph));
			assert(pg_effect_inference_unpack(&bad, equations, n, changed) == -1);
			pg_effect_inference_seal(&bad);
			assert(pg_effect_inference_advance(&bad, 100) == -1);
			pg_effect_inference_destroy(&bad);
		}
		puts("effect image: shared classifier parameter, immutable seeds, cyclic Solve and failed-import isolation passed");
	}
	pg_effect_inference_destroy(&work);
}

static void layout_graph(FILE *file, struct pg_graph *graph, int writing, uint64_t budget)
{
	struct pg_typing typing;
	assert(!pg_typing_init(&typing, graph));
	if (writing) {
		const size_t arities[] = {0, 1};
		const struct pg_data_layout *a = pg_data_layout(graph, 2, arities), *b = pg_data_layout(graph, 2, arities);
		const struct pg_term *zero = pg_reference(graph, pg_data_constructor(a, 0));
		const struct pg_object *x = pg_binder(graph);
		const struct pg_match_clause clauses[] = {{pg_data_constructor(a, 0), zero},
			{pg_data_constructor(a, 1), pg_lambda(graph, x, pg_reference(graph, x))}};
		const struct pg_term *successor = pg_application(graph, pg_reference(graph, pg_data_constructor(a, 1)), zero);
		const struct pg_term *match = pg_data_match(graph, a, successor, 2, clauses);
		const struct pg_term *foreign = pg_reference(graph, pg_data_constructor(b, 0));
		const struct pg_term *roots[] = {zero, successor, match, match, foreign,
			pg_data_match(graph, a, foreign, 2, clauses), pg_reference(graph, pg_data_matcher(pg_data_layout(graph, 0, NULL)))};
		const struct pg_object *family = pg_binder(graph);
		const struct pg_context *prefix = pg_context_bind(&typing, NULL, family, pg_universe(graph, 0), PG_JUDGEMENT_VALUE);
		/* Deliberately only declared, not well-typed: no family formation exists. */
		const struct pg_context *field = pg_context_bind(&typing, prefix, x,
			pg_application(graph, pg_reference(graph, family), zero), PG_JUDGEMENT_VALUE);
		const struct pg_context *contexts[] = {prefix, field, field, NULL};
		size_t terms = graph->terms.count, objects = graph->objects.count;
		assert(!pg_contexts_write_descriptors(file, 4, contexts, 7, roots, &pg_builtin_graph_codec, graph));
		assert(terms == graph->terms.count && objects == graph->objects.count && !typing.proofs.count);
	} else {
		size_t count, context_count;
		const struct pg_term *const *roots;
		const struct pg_context *const *contexts;
		assert(!pg_contexts_read_descriptors(file, &typing, 1000, 100, &pg_builtin_graph_codec,
			graph, &context_count, &contexts, &count, &roots));
		assert(count == 7 && roots[2] == roots[3] && !typing.proofs.count);
		assert(context_count == 4 && contexts[1] == contexts[2] && !contexts[3]);
		assert(contexts[1]->parent == contexts[0] && !contexts[0]->parent);
		const struct pg_term *declared = contexts[1]->declared_type;
		assert(declared->kind == PG_APPLICATION && declared->as.application.argument == roots[0]);
		assert(declared->as.application.function->as.reference == contexts[0]->binder);
		assert(roots[2]->as.application.argument->as.lambda.binder == contexts[1]->binder);
		const struct pg_data_layout *a, *b;
		size_t position, arity;
		assert(pg_data_constructor_view(roots[0]->as.reference, &a, &position, &arity) && position == 0 && arity == 0);
		assert(pg_data_layout_count(a) == 2);
		const struct pg_term *images[] = {declared->as.application.function};
		struct pg_data_constructor_input inputs[] = {{contexts[0], images}, {contexts[1], images}};
		const struct pg_data_declaration *declaration = pg_data_declaration_at_layout(graph,
			a, contexts[0], contexts[0], 2, inputs);
		assert(declaration && pg_data_declaration_layout(declaration) == a && !typing.proofs.count);
		assert(!pg_data_declaration_at_layout(graph, a, contexts[0], contexts[0], 1, inputs));
		inputs[1].fields = contexts[0];
		assert(!pg_data_declaration_at_layout(graph, a, contexts[0], contexts[0], 2, inputs));
		assert(pg_data_constructor_view(roots[4]->as.reference, &b, &position, &arity) && a != b);
		assert(!pg_data_constructor_position(a, roots[4]->as.reference, &position));
		const struct pg_term *head = roots[1]->as.application.function;
		assert(head->as.reference == pg_data_constructor(a, 1));
		assert(pg_data_constructor_view(head->as.reference, &b, &position, &arity) && b == a && position == 1 && arity == 1);
		assert(roots[1]->as.application.argument == roots[0]);
		const struct pg_data_layout *empty = pg_data_layout_view(roots[6]->as.reference);
		assert(empty && !pg_data_layout_count(empty) && !pg_data_constructor(empty, 0));
		const struct pg_term *owner = pg_reference(graph, pg_data_matcher(a));
		const uint64_t out_of_range = 2;
		assert(!pg_builtin_graph_codec.restore(graph, graph, "data-constructor/v1", 1, &owner, 1, &out_of_range));
		assert(!pg_builtin_graph_codec.restore(graph, graph, "data-constructor/v1", 1, &owner, 0, NULL));
		assert(!pg_builtin_graph_codec.restore(graph, graph, "data-layout/v1", 1, &owner, 0, NULL));
		assert(!pg_builtin_graph_codec.restore(graph, graph, "effect-row/v1", 0, NULL, 1, &out_of_range));
		struct pg_whnf_work work;
		assert(!pg_whnf_work_init(&work, graph));
		struct pg_whnf_job *match = pg_whnf_request(&work, &pg_pure_policy, roots[2]);
		while (pg_whnf_advance(match, budget) == PG_EVAL_PENDING) assert(pg_whnf_steps(match) < 1000);
		assert(pg_whnf_status(match) == PG_EVAL_WHNF && pg_whnf_result(match) == roots[0]);
		match = pg_whnf_request(&work, &pg_pure_policy, roots[5]);
		while (pg_whnf_advance(match, budget) == PG_EVAL_PENDING) assert(pg_whnf_steps(match) < 1000);
		assert(pg_whnf_status(match) == PG_EVAL_WHNF && pg_alpha_equal(pg_whnf_result(match), roots[5]) == 1);
		assert(!typing.proofs.count);
		pg_whnf_work_destroy(&work);
		rejected_prefixes(file, &pg_builtin_graph_codec);
		puts("layout image: shared contexts/constructors, inert annotations, distinct layouts and resumed iota passed");
	}
	pg_typing_destroy(&typing);
}

static void declaration_graph(FILE *file, struct pg_graph *graph, int writing)
{
	struct pg_typing typing;
	assert(!pg_typing_init(&typing, graph));
	struct pg_declaration_io io;
	assert(!pg_declaration_io_init(&io, &typing));
	if (writing) {
		const struct pg_object *self = pg_binder(graph), *n = pg_binder(graph);
		const struct pg_term *image = pg_reference(graph, self);
		const struct pg_context *parameters = pg_context_bind(&typing, NULL, self, pg_universe(graph, 0), PG_JUDGEMENT_VALUE);
		const struct pg_context *fields = pg_context_bind(&typing, parameters, n, image, PG_JUDGEMENT_VALUE);
		const struct pg_data_constructor_input constructors[] = {{parameters, &image}, {fields, &image}};
		const struct pg_data_declaration *declaration = pg_data_declaration(graph, parameters, parameters, 2, constructors);
		const struct pg_data_layout *layout = pg_data_declaration_layout(declaration);
		const struct pg_term *zero = pg_reference(graph, pg_data_constructor(layout, 0));
		const struct pg_term *successor = pg_application(graph, pg_reference(graph, pg_data_constructor(layout, 1)), zero);
		const struct pg_term *family = pg_reference(graph, pg_data_declaration_family(declaration));
		const struct pg_term *other_family = pg_reference(graph, pg_data_declaration_family(
			pg_data_declaration(graph, parameters, parameters, 2, constructors)));
		struct pg_graph storage;
		assert(!pg_graph_init(&storage));
		size_t nc, nt, before = graph->terms.count;
		const struct pg_context *const *contexts;
		const struct pg_term *const *terms;
		assert(!pg_data_declaration_pack(declaration, &storage, &nc, &contexts, &nt, &terms));
		assert(nc == 4 && nt == 3);
		const struct pg_context *selected[] = {contexts[0], contexts[1], contexts[2], contexts[3],
			pg_context_bind(&typing, NULL, pg_binder(graph), family, PG_JUDGEMENT_VALUE)};
		const struct pg_term *roots[] = {terms[0], terms[1], terms[2], successor, family, family, other_family};
		assert(!pg_contexts_write_descriptors(file, 5, selected, 7, roots, &pg_declaration_graph_codec, &io));
		assert(io.payloads.count == 2);
		assert(graph->terms.count == before && !typing.proofs.count);
		pg_graph_destroy(&storage);
	} else {
		size_t nc, nt;
		const struct pg_context *const *contexts;
		const struct pg_term *const *terms;
		assert(!pg_contexts_read_descriptors(file, &typing, 1000, 100, &pg_declaration_graph_codec,
			&io, &nc, &contexts, &nt, &terms));
		assert(nc == 5 && nt == 7 && terms[4] == terms[5] && terms[4] != terms[6] && !typing.proofs.count);
		assert(contexts[4]->declared_type == terms[4]);
		const struct pg_data_declaration *declaration = pg_data_declaration_view(terms[4]->as.reference);
		assert(declaration && !typing.proofs.count);
		const struct pg_data_layout *layout = pg_data_declaration_layout(declaration);
		assert(pg_data_declaration_layout(pg_data_declaration_view(terms[6]->as.reference)) != layout);
		assert(terms[3]->as.application.function->as.reference == pg_data_constructor(layout, 1));
		assert(terms[3]->as.application.argument->as.reference == pg_data_constructor(layout, 0));
		const struct pg_evidence *empty = pg_prove_empty_context(&typing);
		const struct pg_evidence *universe = pg_prove_universe(&typing, empty, 0);
		const struct pg_evidence *parameters = pg_prove_context_extension(&typing, empty, contexts[0]->binder, universe);
		const struct pg_evidence *self = pg_prove_variable(&typing, parameters, contexts[0]->binder);
		const struct pg_evidence *fields = pg_prove_context_extension(&typing, parameters, contexts[3]->binder, self);
		const struct pg_evidence *field_self = pg_prove_variable(&typing, fields, contexts[0]->binder);
		const struct pg_evidence *results[] = {pg_prove_substitution(&typing, parameters, parameters, 1, &self),
			pg_prove_substitution(&typing, parameters, fields, 1, &field_self)};
		const struct pg_data_signature *signature = pg_data_signature(&typing, parameters, parameters);
		const struct pg_data_schema *schema = pg_data_schema_check(&typing, declaration, signature, 2, results);
		assert(schema && pg_data_schema_layout(schema) == layout);
		const struct pg_evidence *formation = pg_prove_inductive_type(&typing, schema);
		assert(formation && pg_evidence_subject(formation)->core->as.reference == pg_data_declaration_family(declaration));
		assert(pg_prove_inductive_type(&typing, schema) == formation);
		size_t proofs = typing.proofs.count;
		const struct pg_term *wrong[] = {terms[0], contexts[0]->declared_type, terms[2]};
		const struct pg_data_declaration *changed = pg_data_declaration_unpack(graph, 4, contexts, 3, wrong);
		assert(changed && !pg_data_schema_check(&typing, changed, signature, 2, results));
		assert(!pg_data_declaration_unpack(graph, 4, contexts, 2, terms));
		assert(!pg_data_declaration_unpack(graph, 3, contexts, 3, terms));
		wrong[0] = terms[3];
		assert(!pg_data_declaration_unpack(graph, 4, contexts, 3, wrong));
		assert(typing.proofs.count == proofs);
		rejected_prefixes(file, &pg_declaration_graph_codec);
		puts("declaration image: relocated family, shared contexts/layout, local formation and changed-map rejection passed");
	}
	pg_declaration_io_destroy(&io);
	pg_typing_destroy(&typing);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int operation = !strcmp(argv[1], "operation-write") || !strcmp(argv[1], "operation-read");
	int effect = !strcmp(argv[1], "effect-write") || !strcmp(argv[1], "effect-read") || !strcmp(argv[1], "effect-read-bulk");
	int layout = !strcmp(argv[1], "layout-write") || !strcmp(argv[1], "layout-read") || !strcmp(argv[1], "layout-read-bulk");
	int declaration = !strcmp(argv[1], "declaration-write") || !strcmp(argv[1], "declaration-read");
	int writing = !strcmp(argv[1], "write") || !strcmp(argv[1], "operation-write") || !strcmp(argv[1], "effect-write") || !strcmp(argv[1], "layout-write") || !strcmp(argv[1], "declaration-write");
	assert(writing || !strcmp(argv[1], "read") || operation || effect || layout || declaration);
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	assert(file);
	struct pg_graph graph;
	assert(pg_graph_init(&graph) == 0);
	if (declaration) declaration_graph(file, &graph, writing);
	else if (layout) layout_graph(file, &graph, writing, !strcmp(argv[1], "layout-read-bulk") ? 100 : 1);
	else if (effect) effect_graph(file, &graph, writing, !strcmp(argv[1], "effect-read-bulk") ? 100 : 1);
	else if (operation) operation_graph(file, &graph, writing);
	else if (writing) write_graph(file, &graph);
	else read_graph(file, &graph);
	assert(fclose(file) == 0);
	pg_graph_destroy(&graph);
	return 0;
}
