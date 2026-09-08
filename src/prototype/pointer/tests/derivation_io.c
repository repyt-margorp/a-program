#include "derivation_io.h"
#include "computation.h"
#include "synthesis.h"

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
	const struct pg_evidence *roots[6];
	const struct pg_evidence *under_lambda = NULL;
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, context, x, domain);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, extended, x));
		const struct pg_evidence *codomain = pg_prove_return_type(typing, classifiers, pg_prove_variable(typing, extended, types[i]));
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, extended, codomain);
		roots[i] = pg_prove_lambda(typing, pi, body);
		if (!i) under_lambda = pg_prove_lambda(typing, pi,
			pg_prove_force(typing, pg_prove_thunk(typing, classifiers, body)));
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
	assert(under_lambda);
	struct pg_nf_job *nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(under_lambda)->core);
	assert(nf && pg_nf_advance(nf, 10000) == PG_NF_DONE);
	roots[5] = pg_prove_normalization(typing, under_lambda, pg_nf_certificate(nf));
	assert(roots[3] && roots[4] && roots[5] && pg_derivations_write(file, 6, roots, name, classifiers) == 0);
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
}

static void read_proofs(FILE *file, struct pg_typing *typing, struct pg_classifiers *classifiers, uint64_t chunk)
{
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(pg_derivations_read(file, typing->graph, 1000, 100, resolve, classifiers, &count, &roots) == 0);
	assert(count == 6 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(typing->proofs.count == 0);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	struct pg_synthesis synthesis;
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	struct pg_synthesis_job *jobs[6];
	for (size_t i = 0; i < count; ++i) {
		jobs[i] = pg_synthesis_derivation(&synthesis, roots[i]);
		assert(jobs[i] && pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_PENDING);
	}
	assert(jobs[0] == jobs[2] && typing->proofs.count == 0);
	pg_synthesis_advance(&synthesis, 0);
	assert(typing->proofs.count == 0);
	for (size_t step = 0; step < 10000 && synthesis.ready; ++step) pg_synthesis_advance(&synthesis, chunk);
	for (size_t i = 0; i < count; ++i) assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *left = pg_synthesis_result(jobs[0]);
	const struct pg_evidence *right = pg_synthesis_result(jobs[1]);
	assert(pg_evidence_subject(left)->core == pg_evidence_subject(right)->core);
	assert(pg_evidence_subject(left) != pg_evidence_subject(right));
	assert(pg_evidence_classifier(left) != pg_evidence_classifier(right));
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[3]))) == PG_REDUCTION_WHNF);
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[5]))) == PG_REDUCTION_NF);
	printf("derivation solve: %llu steps\n", (unsigned long long)synthesis.steps);
	const struct pg_derivation_input *saved = roots[3];
	size_t bytes = sizeof(*saved) + saved->count * sizeof(*saved->premises);
	struct pg_derivation_input *wrong = pg_alloc(typing->graph, bytes);
	assert(wrong);
	memcpy(wrong, saved, bytes);
	wrong->target = saved->source;
	struct pg_synthesis_job *bad = pg_synthesis_derivation(&synthesis, wrong);
	assert(bad && pg_synthesis_status(bad) == PG_SYNTHESIS_PENDING);
	pg_synthesis_advance(&synthesis, 10000);
	assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
	saved = roots[5];
	bytes = sizeof(*saved) + saved->count * sizeof(*saved->premises);
	/* A different immutable input needs its own pointer/job identity. */
	struct pg_derivation_input *wrong_mode = pg_alloc(typing->graph, bytes);
	assert(wrong_mode);
	memcpy(wrong_mode, saved, bytes);
	wrong_mode->reduction_kind = PG_REDUCTION_WHNF;
	bad = pg_synthesis_derivation(&synthesis, wrong_mode);
	assert(bad && pg_synthesis_status(bad) == PG_SYNTHESIS_PENDING);
	pg_synthesis_advance(&synthesis, 10000);
	assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
	assert(pg_synthesis_result(jobs[0]) == left);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	rejected_prefixes(file);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	int writing = !strcmp(argv[1], "write");
	int bulk = !strcmp(argv[1], "read-bulk");
	assert(writing || bulk || !strcmp(argv[1], "read"));
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(file && pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	if (writing) write_proofs(file, &typing, &classifiers);
	else read_proofs(file, &typing, &classifiers, bulk ? 64 : 1);
	assert(fclose(file) == 0);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	return 0;
}
