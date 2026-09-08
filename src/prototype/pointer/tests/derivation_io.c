#include "derivation_io.h"
#include "computation.h"
#include "dag.h"

#include <assert.h>
#include <string.h>

static const struct pg_object *const operations[] = {
	&pg_return_operation, &pg_thunk_operation, &pg_force_operation
};
static const char *const labels[] = {"kernel/return/v1", "kernel/thunk/v1", "kernel/force/v1"};

static const char *name(void *owner, const struct pg_object *object)
{
	if (object == pg_universe(owner, 0)->as.reference) return "kernel/universe/0/v1";
	if (object == pg_universe(owner, 1)->as.reference) return "kernel/universe/1/v1";
	for (size_t i = 0; i < 3; ++i) if (object == operations[i]) return labels[i];
	return NULL;
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	if (!strcmp(label, "kernel/universe/0/v1")) return pg_universe(owner, 0)->as.reference;
	if (!strcmp(label, "kernel/universe/1/v1")) return pg_universe(owner, 1)->as.reference;
	for (size_t i = 0; i < 3; ++i) if (!strcmp(label, labels[i])) return operations[i];
	return NULL;
}

static void rejected_prefixes(FILE *file)
{
	unsigned char bytes[8192];
	rewind(file);
	size_t length = fread(bytes, 1, sizeof(bytes), file);
	assert(feof(file) && !ferror(file) && length > 24);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		struct pg_classifiers classifiers;
		assert(pg_graph_init(&graph) == 0 && pg_classifiers_init(&classifiers, &graph) == 0);
		FILE *fragment = tmpfile();
		if (cut == length) bytes[24] = 255;
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		size_t count = 71;
		const struct pg_derivation_input *const *roots = NULL;
		assert(pg_derivations_read(fragment, &graph, 1000, 100, resolve, &classifiers, &count, &roots) == -1);
		assert(count == 71 && !roots && fclose(fragment) == 0);
		pg_classifiers_destroy(&classifiers);
		pg_graph_destroy(&graph);
	}
}

static void write_proofs(FILE *file, struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *ca = pg_prove_context_extension(typing, empty, a, u);
	const struct pg_evidence *context = pg_prove_context_extension(typing, ca, b,
		pg_prove_universe(typing, classifiers, ca, 0));
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *roots[5];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, context, x, domain);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, extended, x));
		const struct pg_evidence *codomain = pg_prove_return_type(typing, classifiers, pg_prove_variable(typing, extended, types[i]));
		roots[i] = pg_prove_lambda(typing, pg_prove_pi(typing, classifiers, domain, extended, codomain), body);
		assert(roots[i]);
	}
	roots[2] = roots[0];
	const struct pg_evidence *returned = pg_prove_return(typing, classifiers, pg_prove_type_value(typing, u));
	const struct pg_evidence *forced = pg_prove_force(typing, pg_prove_thunk(typing, classifiers, returned));
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(forced)->core);
	assert(job && pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	roots[3] = pg_prove_normalization(typing, forced, pg_whnf_certificate(job));
	const struct pg_evidence *u1 = pg_prove_universe(typing, classifiers, empty, 1);
	struct pg_conversion conversion;
	const struct pg_term *u1_core = pg_evidence_subject(u1)->core;
	assert(pg_conversion_init(&conversion, &work, u1_core, u1_core) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	roots[4] = pg_prove_conversion(typing, pg_prove_type_value(typing, u), u1, pg_conversion_certificate(&conversion));
	assert(roots[3] && roots[4] && pg_derivations_write(file, 5, roots, name, classifiers) == 0);
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
}

static int child(void *unused, const void *key, size_t index, const void **output)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (index == input->count) return 0;
	*output = input->premises[index];
	return 1;
}

static void read_proofs(FILE *file, struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(pg_derivations_read(file, typing->graph, 1000, 100, resolve, classifiers, &count, &roots) == 0);
	assert(count == 5 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(typing->proofs.count == 0);
	struct pg_dag dag;
	assert(pg_dag_init(&dag, child, NULL) == 0);
	for (size_t i = 0; i < count; ++i) assert(pg_dag_add(&dag, roots[i]) == 0);
	const struct pg_evidence **proofs = pg_alloc(typing->graph, dag.count * sizeof(*proofs));
	const struct pg_evidence **premises = pg_alloc(typing->graph, dag.count * sizeof(*premises));
	assert(proofs && premises);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct pg_derivation_input *input = node->key;
		for (size_t i = 0; i < input->count; ++i) premises[i] = proofs[pg_dag_find(&dag, input->premises[i])->id - 1];
		struct pg_derivation_parameters parameters = input->parameters;
		assert(!parameters.conversion && !parameters.reduction);
		if (input->rule == PG_PURE_NORMALIZATION) {
			assert(!pg_prove_derivation(typing, classifiers, input->rule, &parameters, input->count, premises));
			assert(input->source == pg_evidence_subject(premises[0])->core);
			struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input->source);
			assert(job && pg_whnf_advance(job, 0) == PG_EVAL_PENDING);
			assert(pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
			assert(pg_whnf_result(job) == input->target);
			parameters.reduction = pg_whnf_certificate(job);
		} else if (input->rule == PG_TYPE_CONVERSION) {
			assert(!pg_prove_derivation(typing, classifiers, input->rule, &parameters, input->count, premises));
			assert(input->source == pg_evidence_classifier(premises[0]));
			assert(input->target == pg_evidence_subject(premises[1])->core);
			struct pg_conversion conversion;
			assert(pg_conversion_init(&conversion, &work, input->source, input->target) == 0);
			assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
			parameters.conversion = pg_conversion_certificate(&conversion);
			pg_conversion_destroy(&conversion);
		} else assert(!input->source && !input->target);
		proofs[node->id - 1] = pg_prove_derivation(typing, classifiers, input->rule, &parameters, input->count, premises);
		assert(proofs[node->id - 1]);
	}
	const struct pg_evidence *left = proofs[pg_dag_find(&dag, roots[0])->id - 1];
	const struct pg_evidence *right = proofs[pg_dag_find(&dag, roots[1])->id - 1];
	assert(pg_evidence_subject(left)->core == pg_evidence_subject(right)->core);
	assert(pg_evidence_subject(left) != pg_evidence_subject(right));
	assert(pg_evidence_classifier(left) != pg_evidence_classifier(right));
	pg_whnf_work_destroy(&work);
	pg_dag_destroy(&dag);
	rejected_prefixes(file);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int writing = !strcmp(argv[1], "write");
	assert(writing || !strcmp(argv[1], "read"));
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(file && pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	if (writing) write_proofs(file, &typing, &classifiers);
	else read_proofs(file, &typing, &classifiers);
	assert(fclose(file) == 0);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	return 0;
}
