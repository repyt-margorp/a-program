#include "graph_io.h"
#include "context_io.h"
#include "evidence.h"
#include "derivation.h"
#include "computation.h"

#include <assert.h>
#include <string.h>

static const char *name(void *owner, const struct pg_object *object)
{
	if (object == pg_universe(owner, 0)->as.reference) return "kernel/universe/0/v1";
	return object == &pg_return_operation ? "kernel/return/v1" : NULL;
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	if (!strcmp(label, "kernel/universe/0/v1")) return pg_universe(owner, 0)->as.reference;
	return !strcmp(label, "kernel/return/v1") ? &pg_return_operation : NULL;
}

static void rejected_prefixes(FILE *file)
{
	unsigned char bytes[4096];
	rewind(file);
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file) && length > 32);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		struct pg_typing typing;
		struct pg_classifiers classifiers;
		assert(pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
		assert(pg_classifiers_init(&classifiers, &graph) == 0);
		FILE *fragment = tmpfile();
		/* The complete-sized final case has an invalid parent reference. */
		if (cut == length) bytes[32] = 255;
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		size_t nc = 71, nt = 72;
		const struct pg_context *const *contexts = NULL;
		const struct pg_term *const *terms = NULL;
		assert(pg_contexts_read(fragment, &typing, 100, 100, resolve, &classifiers,
			&nc, &contexts, &nt, &terms) == -1);
		assert(nc == 71 && nt == 72 && !contexts && !terms && typing.proofs.count == 0);
		assert(fclose(fragment) == 0);
		pg_classifiers_destroy(&classifiers);
		pg_typing_destroy(&typing);
		pg_graph_destroy(&graph);
	}
}

static void context_boundaries(struct pg_typing *typing, const struct pg_term *type)
{
	struct pg_context cycle = {NULL, pg_binder(typing->graph), type};
	cycle.parent = &cycle;
	const struct pg_context *root = &cycle;
	FILE *file = tmpfile();
	assert(file && pg_contexts_write(file, 1, &root, 0, NULL, NULL, NULL) == -1);
	assert(fclose(file) == 0);
	root = NULL;
	for (size_t i = 0; i < 10000; ++i)
		root = pg_context_bind(typing, root, pg_binder(typing->graph), type);
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
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0 && pg_classifiers_init(&classifiers, graph) == 0);
	const struct pg_term *u = pg_universe(&classifiers, 0);
	const struct pg_context *ca = pg_context_bind(&typing, NULL, a, u);
	const struct pg_context *cb = pg_context_bind(&typing, ca, b, u);
	const struct pg_context *xa = pg_context_bind(&typing, cb, x, roots[0]);
	const struct pg_context *xb = pg_context_bind(&typing, cb, x, roots[1]);
	const struct pg_context *contexts[] = {ca, cb, xa, xb, NULL, xa};
	assert(pg_contexts_write(file, 6, contexts, 5, roots, name, &classifiers) == 0);
	context_boundaries(&typing, roots[0]);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
}

static void read_graph(FILE *file, struct pg_graph *graph)
{
	size_t count = 0;
	const struct pg_term *const *roots = NULL;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_typing_init(&typing, graph) == 0);
	assert(pg_classifiers_init(&classifiers, graph) == 0);
	size_t context_count = 0;
	const struct pg_context *const *contexts = NULL;
	assert(pg_contexts_read(file, &typing, 100, 100, resolve, &classifiers,
		&context_count, &contexts, &count, &roots) == 0);
	assert(context_count == 6 && contexts[4] == NULL && contexts[2] == contexts[5]);
	assert(contexts[2] != contexts[3] && contexts[2]->parent == contexts[1]);
	assert(contexts[3]->parent == contexts[1] && contexts[1]->parent == contexts[0]);
	assert(typing.proofs.count == 0);
	assert(count == 5 && roots[3] == roots[4]);
	assert(roots[0]->kind == PG_REFERENCE && roots[1]->kind == PG_REFERENCE && roots[2]->kind == PG_REFERENCE);
	const struct pg_object *a = roots[0]->as.reference, *b = roots[1]->as.reference, *x = roots[2]->as.reference;
	assert(a != b && a != x && b != x);
	/* The fixture supplies declarations, not a serialized accepted flag.
	 * Every judgement below is constructed by the ordinary kernel rules. */
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *u = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_evidence *ca = pg_prove_context_extension(&typing, empty, a, u);
	const struct pg_evidence *ua = pg_prove_universe(&typing, &classifiers, ca, 0);
	const struct pg_evidence *context = pg_prove_context_extension(&typing, ca, b, ua);
	assert(context && !pg_prove_variable(&typing, context, x));
	assert(pg_evidence_context(context) == contexts[1]);
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *pi[2], *body[2], *identity[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(&typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(&typing, context, x, domain);
		assert(pg_evidence_context(extended) == contexts[2 + i]);
		const struct pg_evidence *codomain = pg_prove_return_type(&typing, &classifiers,
			pg_prove_variable(&typing, extended, types[i]));
		pi[i] = pg_prove_pi(&typing, &classifiers, domain, extended, codomain);
		body[i] = pg_prove_return(&typing, &classifiers, pg_prove_variable(&typing, extended, x));
		const struct pg_evidence *premises[] = {pi[i], body[i]};
		struct pg_derivation_parameters parameters = {0};
		identity[i] = pg_prove_derivation(&typing, &classifiers, PG_LAMBDA_INTRO, &parameters, 2, premises);
		assert(identity[i] && pg_evidence_subject(identity[i])->core == roots[3]);
		assert(pg_prove_lambda(&typing, pi[i], body[i]) == identity[i]);
	}
	assert(identity[0] != identity[1]);
	assert(pg_evidence_subject(identity[0]) != pg_evidence_subject(identity[1]));
	assert(pg_evidence_classifier(identity[0]) != pg_evidence_classifier(identity[1]));
	assert(!pg_prove_lambda(&typing, pi[0], body[1]));
	assert(!pg_prove_lambda(&typing, pi[1], body[0]));
	rejected_prefixes(file);
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
