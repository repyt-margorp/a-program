#include "derivation_io.h"
#include "graph_io.h"
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
	(void)owner;
	/* The writer consumes each label before the next callback. */
	static char buffer[64];
	const char *label = pg_classifier_name(object, buffer, sizeof(buffer));
	if (label) return label;
	label = pg_identity_name(object);
	if (label) return label;
	for (size_t i = 0; i < 3; ++i) if (object == operations[i]) return labels[i];
	return NULL;
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	const struct pg_object *object = pg_classifier_resolve(owner, label);
	if (object) return object;
	object = pg_identity_resolve(label);
	if (object) return object;
	for (size_t i = 0; i < 3; ++i) if (!strcmp(label, labels[i])) return operations[i];
	return NULL;
}

static void classifier_transport(struct pg_classifiers *source)
{
	struct pg_graph graph;
	struct pg_classifiers destination;
	assert(!pg_graph_init(&graph) && !pg_classifiers_init(&destination, &graph));
	const struct pg_object *binder = pg_binder(source->graph);
	const struct pg_term *domain = pg_universe(source, UINT64_MAX);
	const struct pg_term *codomain = pg_return_type(source, pg_reference(source->graph, binder));
	const struct pg_term *pi = pg_pi(source->graph, domain, binder, codomain);
	const struct pg_term *roots[8] = {domain, pi, pg_thunk_type(source, pi),
		pg_identity_action(source->graph, domain)};
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		roots[4 + 2 * side] = pg_identity_transport(source->graph, roots[3], domain, side);
		roots[5 + 2 * side] = pg_identity_lift(source->graph, roots[3], domain, side);
	}
	FILE *file = tmpfile();
	assert(file && !pg_graph_write(file, 8, roots, name, source));
	rewind(file);
	size_t count;
	const struct pg_term *const *loaded;
	assert(!pg_graph_read(file, &graph, 100, 100, resolve, &destination, &count, &loaded));
	assert(count == 8 && loaded[0] != domain);
	assert(loaded[0] == pg_universe(&destination, UINT64_MAX));
	const struct pg_term *d, *c, *t;
	const struct pg_object *b;
	assert(pg_pi_view(loaded[1], &d, &b, &c) && d == loaded[0] && b != binder);
	assert(pg_return_type_view(c, &t) && t->as.reference == b);
	assert(pg_thunk_type_view(loaded[2], &t) && t == loaded[1]);
	assert(loaded[3] == pg_identity_action(&graph, loaded[0]));
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		assert(loaded[4 + 2 * side] == pg_identity_transport(&graph, loaded[3], loaded[0], side));
		assert(loaded[5 + 2 * side] == pg_identity_lift(&graph, loaded[3], loaded[0], side));
	}
	assert(!pg_identity_name(binder));
	assert(!pg_identity_resolve("kernel/identity/action/v2"));
	assert(!pg_identity_resolve("kernel/identity/transport/v1"));
	const char *invalid[] = {"kernel/universe/01/v1", "kernel/universe/-1/v1",
		"kernel/universe/+1/v1", "kernel/universe/ 1/v1", "kernel/universe//v1",
		"kernel/universe/18446744073709551616/v1", "kernel/universe/1/v2",
		"kernel/universe/1/v1/extra", "kernel/pi/v2", "other/pi/v1"};
	size_t before = destination.universes.count;
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
		assert(!pg_classifier_resolve(&destination, invalid[i]));
	assert(destination.universes.count == before);
	char small[2];
	assert(!pg_classifier_name(domain->as.reference, small, sizeof(small)));
	assert(!pg_classifier_name(binder, small, sizeof(small)));
	assert(!fclose(file));
	pg_classifiers_destroy(&destination);
	pg_graph_destroy(&graph);
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
	classifier_transport(classifiers);
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *ca = pg_prove_context_extension(typing, empty, a, u);
	const struct pg_evidence *context = pg_prove_context_extension(typing, ca, b,
		pg_prove_universe(typing, classifiers, ca, 0));
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *roots[11];
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
	const struct pg_evidence *u2 = pg_prove_universe(typing, classifiers, empty, 2);
	roots[6] = pg_prove_reflexivity(typing, u2, pg_prove_type_value(typing, u1));
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *value = pg_prove_type_value(typing, u);
		roots[7 + 2 * side] = pg_prove_identity_transport(typing, classifiers, roots[6], value, side);
		roots[8 + 2 * side] = pg_prove_identity_lift(typing, classifiers, roots[6], value, side);
	}
	for (size_t i = 0; i < 11; ++i) assert(roots[i]);
	assert(pg_derivations_write(file, 11, roots, name, classifiers) == 0);
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
}

static struct pg_synthesis_job *source_use(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *producer, const char *source)
{
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = "loaded", .length = 6};
	const struct pg_source_scope *scope = pg_synthesis_name_job(synthesis,
		pg_synthesis_root(synthesis), token, producer);
	assert(scope);
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, synthesis->typing->graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, definition.expression);
	assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(job) && pg_parser_next(&parser, &definition) == 0);
	return job;
}

static void read_proofs(FILE *file, struct pg_typing *typing, struct pg_classifiers *classifiers, uint64_t chunk)
{
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(pg_derivations_read(file, typing->graph, 1000, 100, resolve, classifiers, &count, &roots) == 0);
	assert(count == 11 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(typing->proofs.count == 0);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	struct pg_synthesis synthesis;
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	struct pg_synthesis_job *jobs[11];
	for (size_t i = 0; i < count; ++i) {
		jobs[i] = pg_synthesis_derivation(&synthesis, roots[i]);
		assert(jobs[i] && pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_PENDING);
	}
	assert(jobs[0] == jobs[2] && typing->proofs.count == 0);
	struct pg_synthesis_job *consumer = source_use(&synthesis, jobs[6], "copy := loaded;");
	struct pg_synthesis_job *expect = source_use(&synthesis, jobs[6], "copy := loaded :: @;");
	/* Source scope construction establishes the empty context, not imports. */
	size_t before_fuel = typing->proofs.count;
	pg_synthesis_advance(&synthesis, 0);
	assert(typing->proofs.count == before_fuel);
	for (size_t i = 0; i < count; ++i)
		assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(jobs[i]));
	for (size_t step = 0; step < 10000 && synthesis.ready; ++step) pg_synthesis_advance(&synthesis, chunk);
	for (size_t i = 0; i < count; ++i) assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(consumer) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(consumer) == pg_synthesis_result(jobs[6]));
	assert(pg_synthesis_status(expect) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(expect));
	const struct pg_evidence *left = pg_synthesis_result(jobs[0]);
	const struct pg_evidence *right = pg_synthesis_result(jobs[1]);
	assert(pg_evidence_subject(left)->core == pg_evidence_subject(right)->core);
	assert(pg_evidence_subject(left) != pg_evidence_subject(right));
	assert(pg_evidence_classifier(left) != pg_evidence_classifier(right));
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[3]))) == PG_REDUCTION_WHNF);
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[5]))) == PG_REDUCTION_NF);
	assert(pg_evidence_rule(pg_synthesis_result(jobs[6])) == PG_REFLEXIVITY);
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *transport = pg_synthesis_result(jobs[7 + 2 * side]);
		const struct pg_evidence *lift = pg_synthesis_result(jobs[8 + 2 * side]);
		assert(pg_evidence_rule(transport) == PG_IDENTITY_TRANSPORT);
		assert(pg_evidence_rule(lift) == PG_IDENTITY_LIFT);
		assert(pg_evidence_premise(transport, 1) == pg_synthesis_result(jobs[6]));
		assert(pg_evidence_premise(lift, 1) == transport);
		struct pg_derivation_parameters parameters;
		assert(!pg_derivation_parameters(transport, &parameters) && parameters.direction == side);
	}
	printf("derivation solve: %llu steps\n", (unsigned long long)synthesis.steps);
	const struct pg_derivation_input *saved = roots[3];
	size_t bytes = sizeof(*saved) + saved->count * sizeof(*saved->premises);
	struct pg_derivation_input *wrong = pg_alloc(typing->graph, bytes);
	assert(wrong);
	memcpy(wrong, saved, bytes);
	wrong->target = saved->source;
	struct pg_synthesis_job *bad = pg_synthesis_derivation(&synthesis, wrong);
	assert(bad && pg_synthesis_status(bad) == PG_SYNTHESIS_PENDING);
	struct pg_synthesis_job *bad_consumer = source_use(&synthesis, bad, "copy := loaded;");
	pg_synthesis_advance(&synthesis, 10000);
	assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
	assert(pg_synthesis_status(bad_consumer) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad_consumer));
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
	saved = roots[7];
	bytes = sizeof(*saved) + saved->count * sizeof(*saved->premises);
	struct pg_derivation_input *wrong_direction = pg_alloc(typing->graph, bytes);
	assert(wrong_direction);
	memcpy(wrong_direction, saved, bytes);
	wrong_direction->parameters.direction = PG_IDENTITY_LEFT;
	bad = pg_synthesis_derivation(&synthesis, wrong_direction);
	assert(bad);
	pg_synthesis_advance(&synthesis, 10000);
	/* Equal endpoint types do not authorize changing the directional premise. */
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
