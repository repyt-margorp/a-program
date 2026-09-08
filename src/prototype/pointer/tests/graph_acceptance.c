#include "graph_io.h"
#include "evidence.h"
#include "computation.h"

#include <assert.h>
#include <string.h>

static const char *name(void *unused, const struct pg_object *object)
{
	(void)unused;
	return object == &pg_return_operation ? "kernel/return/v1" : NULL;
}

static const struct pg_object *resolve(void *unused, const char *label)
{
	(void)unused;
	return !strcmp(label, "kernel/return/v1") ? &pg_return_operation : NULL;
}

static void write_graph(FILE *file, struct pg_graph *graph)
{
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_term *value = pg_reference(graph, x);
	const struct pg_term *body = pg_application(graph, pg_reference(graph, &pg_return_operation), value);
	const struct pg_term *identity = pg_lambda(graph, x, body);
	const struct pg_term *roots[] = {pg_reference(graph, a), pg_reference(graph, b), value, identity, identity};
	assert(pg_graph_write(file, 5, roots, name, NULL) == 0);
}

static void read_graph(FILE *file, struct pg_graph *graph)
{
	size_t count = 0;
	const struct pg_term *const *roots = NULL;
	assert(pg_graph_read(file, graph, 100, 100, resolve, NULL, &count, &roots) == 0);
	assert(count == 5 && roots[3] == roots[4]);
	assert(roots[0]->kind == PG_REFERENCE && roots[1]->kind == PG_REFERENCE && roots[2]->kind == PG_REFERENCE);
	const struct pg_object *a = roots[0]->as.reference, *b = roots[1]->as.reference, *x = roots[2]->as.reference;
	assert(a != b && a != x && b != x);
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	/* The fixture supplies declarations, not a serialized accepted flag.
	 * Every judgement below is constructed by the ordinary kernel rules. */
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *ca = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *ua = pg_prove_universe(&typing, &classifiers, ca, 0);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, ca, b, ua);
	assert(context && !pg_prove_variable(&typing, context, x));
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *pi[2], *body[2], *identity[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(&typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(&typing, context, x, domain);
		const struct pg_evidence *codomain = pg_prove_return_type(&typing, &classifiers,
			pg_prove_variable(&typing, extended, types[i]));
		pi[i] = pg_prove_pi(&typing, &classifiers, domain, extended, codomain);
		body[i] = pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, extended, x));
		identity[i] = pg_prove_lambda(&typing, pi[i], body[i]);
		assert(identity[i] && pg_evidence_subject(identity[i])->core == roots[3]);
		assert(pg_prove_lambda(&typing, pi[i], body[i]) == identity[i]);
	}
	assert(identity[0] != identity[1]);
	assert(pg_evidence_subject(identity[0]) != pg_evidence_subject(identity[1]));
	assert(pg_evidence_classifier(identity[0]) != pg_evidence_classifier(identity[1]));
	assert(!pg_prove_lambda(&typing, pi[0], body[1]));
	assert(!pg_prove_lambda(&typing, pi[1], body[0]));
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int writing = !strcmp(argv[1], "write");
	assert(writing || !strcmp(argv[1], "read"));
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	assert(file);
	struct pg_graph graph;
	assert(pg_graph_init(&graph) == 0);
	if (writing) write_graph(file, &graph);
	else read_graph(file, &graph);
	assert(fclose(file) == 0);
	pg_graph_destroy(&graph);
	return 0;
}
