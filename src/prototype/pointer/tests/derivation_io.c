#include "derivation_io.h"
#include "graph_io.h"
#include "computation.h"
#include "synthesis.h"
#include "wire.h"
#include "dag.h"

#include <assert.h>
#include <string.h>

static const char *name(void *owner, const struct pg_object *object)
{
	(void)owner;
	/* The writer consumes each label before the next callback. */
	static char buffer[64];
	const char *label = pg_classifier_name(object, buffer, sizeof(buffer));
	if (label) return label;
	label = pg_identity_name(object);
	if (label) return label;
	return pg_computation_name(object);
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	const struct pg_object *object = pg_classifier_resolve(owner, label);
	if (object) return object;
	object = pg_identity_resolve(label);
	if (object) return object;
	return pg_computation_resolve(label);
}

struct effect_owner {
	struct pg_classifiers *classifiers;
	const struct pg_object *row;
};

static const char *effect_name(void *owner, const struct pg_object *object)
{
	struct effect_owner *context = owner;
	return object == context->row ? "test/closed-effect-row/v1" : name(context->classifiers, object);
}

static const struct pg_object *effect_resolve(void *owner, const char *label)
{
	struct effect_owner *context = owner;
	return !strcmp(label, "test/closed-effect-row/v1") ? context->row : resolve(context->classifiers, label);
}

static void effect_transport(void)
{
	static const struct pg_object_class label_class = {"test-row-label"};
	static const struct pg_object label = {PG_SEMANTIC_OBJECT, &label_class};
	const struct pg_object *labels[] = {&label};
	struct pg_graph graphs[2];
	struct pg_typing typings[2];
	struct pg_classifiers classifiers[2];
	struct effect_owner owners[2];
	const struct pg_effect_row *rows[2];
	for (size_t i = 0; i < 2; ++i) {
		assert(!pg_graph_init(&graphs[i]) && !pg_typing_init(&typings[i], &graphs[i]));
		assert(!pg_classifiers_init(&classifiers[i], &graphs[i]));
		rows[i] = pg_effect_row(&graphs[i], 1, labels);
		owners[i] = (struct effect_owner){&classifiers[i], pg_effect_reference(&graphs[i], rows[i])->as.reference};
	}
	assert(rows[0] != rows[1]);
	const struct pg_evidence *universe = pg_prove_universe(&typings[0], &classifiers[0], pg_prove_empty_context(&typings[0]), 0);
	const struct pg_evidence *formation = pg_prove_effect_type(&typings[0], &classifiers[0], rows[0], universe);
	const struct pg_object *m = pg_binder(&graphs[0]), *x = pg_binder(&graphs[0]);
	const struct pg_evidence *context = pg_prove_context_extension(&typings[0],
		pg_prove_empty_context(&typings[0]), m, pg_prove_thunk_type(&typings[0], &classifiers[0], formation));
	const struct pg_evidence *source = pg_prove_force(&typings[0], pg_prove_variable(&typings[0], context, m));
	const struct pg_evidence *domain = pg_prove_projection(&typings[0], context, universe);
	const struct pg_evidence *extended = pg_prove_context_extension(&typings[0], context, x, domain);
	const struct pg_evidence *pi = pg_prove_pi(&typings[0], &classifiers[0], domain, extended,
		pg_prove_projection(&typings[0], extended, formation));
	const struct pg_evidence *continuation = pg_prove_lambda(&typings[0], pi,
		pg_prove_projection(&typings[0], extended, source));
	const struct pg_evidence *fold = pg_prove_fold(&typings[0], &classifiers[0], source, continuation);
	const struct pg_evidence *returned = pg_prove_return(&typings[0], &classifiers[0],
		pg_prove_variable(&typings[0], extended, x));
	const struct pg_evidence *widened = pg_prove_effect_subsumption(&typings[0], returned,
		pg_prove_projection(&typings[0], extended, formation));
	const struct pg_evidence *saved[] = {formation, fold, widened};
	FILE *file = tmpfile();
	assert(file && formation && fold && widened && !pg_derivations_write(file, 3, saved, effect_name, &owners[0]));
	rewind(file);
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(!pg_derivations_read(file, &graphs[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(count == 3 && roots[0]->parameters.effects == rows[1] && !typings[1].proofs.count);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, &graphs[1]));
	assert(!pg_synthesis_init(&synthesis, &typings[1], &classifiers[1], &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *job = pg_synthesis_derivation(&synthesis, roots[0]);
	struct pg_synthesis_job *fold_job = pg_synthesis_derivation(&synthesis, roots[1]);
	struct pg_synthesis_job *widening_job = pg_synthesis_derivation(&synthesis, roots[2]);
	assert(job && fold_job && widening_job);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *result = pg_synthesis_result(job);
	const struct pg_effect_row *row;
	const struct pg_term *value;
	assert(pg_effect_type_view(pg_evidence_subject(result)->core, &row, &value) && row == rows[1]);
	assert(pg_evidence_subject(pg_prove_return_content(&typings[1], result))->core == value);
	assert(pg_synthesis_status(fold_job) == PG_SYNTHESIS_DONE);
	assert(pg_effect_type_view(pg_evidence_classifier(pg_synthesis_result(fold_job)), &row, &value) && row == rows[1]);
	assert(pg_synthesis_status(widening_job) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *loaded = pg_synthesis_result(widening_job);
	assert(pg_evidence_rule(loaded) == PG_EFFECT_SUBSUMPTION);
	assert(pg_evidence_subject(loaded) == pg_evidence_subject(pg_evidence_premise(loaded, 0)));
	assert(pg_effect_type_view(pg_evidence_classifier(loaded), &row, &value) && row == rows[1]);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	/* Locate the explicit row field through the record grammar, not a proof ID. */
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t records, root_count, row_id = 0;
	assert(!pg_wire_read_u64(file, &records) && !pg_wire_read_u64(file, &root_count));
	long row_offset = -1;
	for (uint64_t i = 0; i < records; ++i) {
		uint64_t rule, ignored, arity;
		assert(!pg_wire_read_u64(file, &rule));
		for (unsigned j = 0; j < 4; ++j) assert(!pg_wire_read_u64(file, &ignored));
		long offset = ftell(file);
		assert(offset >= 0 && !pg_wire_read_u64(file, &ignored));
		if (rule == PG_RETURN_TYPE_FORM) { row_offset = offset; row_id = ignored; }
		for (unsigned j = 0; j < 2; ++j) assert(!pg_wire_read_u64(file, &ignored));
		assert(!pg_wire_read_u64(file, &arity));
		for (uint64_t j = 0; j < arity; ++j) assert(!pg_wire_read_u64(file, &ignored));
	}
	assert(root_count == 3 && row_offset >= 0 && row_id);
	assert(!fseek(file, row_offset, SEEK_SET) && !pg_wire_write_u64(file, 0));
	rewind(file);
	assert(pg_derivations_read(file, &graphs[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fseek(file, row_offset, SEEK_SET) && !pg_wire_write_u64(file, row_id));
	assert(!fseek(file, 7, SEEK_SET) && fputc(1, file) != EOF);
	rewind(file);
	assert(pg_derivations_read(file, &graphs[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fclose(file));
	for (size_t i = 0; i < 2; ++i) {
		pg_classifiers_destroy(&classifiers[i]);
		pg_typing_destroy(&typings[i]);
		pg_graph_destroy(&graphs[i]);
	}
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

static void unique_term_roots(FILE *file, struct pg_graph *graph,
	struct pg_classifiers *classifiers)
{
	rewind(file);
	char header[8];
	uint64_t records, roots, word;
	assert(fread(header, 1, sizeof(header), file) == sizeof(header));
	assert(!pg_wire_read_u64(file, &records) && !pg_wire_read_u64(file, &roots));
	size_t references = 0;
	for (uint64_t i = 0; i < records; ++i) {
		/* Rule, level, direction, reduction mode, then four Core references. */
		for (unsigned j = 0; j < 8; ++j) {
			assert(!pg_wire_read_u64(file, &word));
			if (j >= 4 && word) ++references;
		}
		uint64_t premises;
		assert(!pg_wire_read_u64(file, &premises));
		for (uint64_t j = 0; j < premises; ++j) assert(!pg_wire_read_u64(file, &word));
	}
	for (uint64_t i = 0; i < roots; ++i) assert(!pg_wire_read_u64(file, &word));
	size_t count;
	const struct pg_term *const *terms;
	assert(!pg_graph_read(file, graph, 1000, 100, resolve, classifiers, &count, &terms));
	struct pg_dag unique;
	assert(!pg_dag_init(&unique, NULL, NULL));
	for (size_t i = 0; i < count; ++i) {
		assert(!pg_dag_add(&unique, terms[i]));
		assert(unique.count == i + 1);
	}
	assert(count < references);
	pg_dag_destroy(&unique);
	rewind(file);
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
	const struct pg_evidence *roots[12];
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
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, empty, z, u1);
	const struct pg_evidence *codomain = pg_prove_return_type(typing, classifiers,
		pg_prove_universe(typing, classifiers, extended, 1));
	const struct pg_evidence *continuation = pg_prove_lambda(typing,
		pg_prove_pi(typing, classifiers, u1, extended, codomain),
		pg_prove_return(typing, classifiers, pg_prove_variable(typing, extended, z)));
	const struct pg_evidence *fold = pg_prove_fold(typing, classifiers, forced, continuation);
	assert(fold);
	job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(fold)->core);
	assert(job && pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	roots[11] = pg_prove_normalization(typing, fold, pg_whnf_certificate(job));
	for (size_t i = 0; i < 12; ++i) assert(roots[i]);
	assert(pg_derivations_write(file, 12, roots, name, classifiers) == 0);
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
	unique_term_roots(file, typing->graph, classifiers);
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(pg_derivations_read(file, typing->graph, 1000, 100, resolve, classifiers, &count, &roots) == 0);
	assert(count == 12 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(typing->proofs.count == 0);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	struct pg_synthesis synthesis;
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	struct pg_synthesis_job *jobs[12];
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
	const struct pg_evidence *fold_result = pg_synthesis_result(jobs[11]);
	assert(pg_evidence_rule(pg_evidence_premise(fold_result, 0)) == PG_FOLD_ELIM);
	assert(pg_evidence_subject(fold_result)->core == pg_evidence_subject(pg_synthesis_result(jobs[3]))->core);
	assert(!pg_computation_resolve("kernel/fold/v2"));
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
	/* Loaded rule requests may simplify; their result remains ordinary evidence. */
	struct pg_derivation_input *projection = pg_alloc(typing->graph,
		sizeof(*projection) + 2 * sizeof(*projection->premises));
	assert(projection);
	assert(roots[0]->rule == PG_LAMBDA_INTRO);
	assert(roots[0]->premises[0]->rule == PG_PI_FORM);
	const struct pg_derivation_input *extended = roots[0]->premises[0]->premises[1];
	assert(extended->rule == PG_CONTEXT_EXTEND);
	*projection = (struct pg_derivation_input){.rule = PG_CONTEXT_PROJECTION, .count = 2};
	projection->premises[0] = extended->premises[0];
	projection->premises[1] = roots[0];
	struct pg_synthesis_job *projected = pg_synthesis_derivation(&synthesis, projection);
	assert(projected);
	for (unsigned steps = 0; pg_synthesis_status(projected) == PG_SYNTHESIS_PENDING; ++steps) {
		assert(steps < 1000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(projected) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(projected) == left);
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
	effect_transport();
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
