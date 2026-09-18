#include "derivation_io.h"
#include "context_payload.h"
#include "effect_inference.h"
#include "graph_io.h"
#include "computation.h"
#include "synthesis.h"
#include "wire.h"
#include "dag.h"
#include "descriptor_io.h"
#include "declaration_io.h"
#include "iadt.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static unsigned char *file_bytes(FILE *file, size_t *length)
{
	assert(!fseek(file, 0, SEEK_END));
	long end = ftell(file);
	assert(end > 0 && (uintmax_t)end <= SIZE_MAX);
	*length = (size_t)end;
	unsigned char *bytes = malloc(*length);
	rewind(file);
	assert(bytes && fread(bytes, 1, *length, file) == *length && !ferror(file));
	return bytes;
}

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
	struct pg_graph *graph;
	const struct pg_object *row;
};

static const char *effect_name(void *owner, const struct pg_object *object)
{
	struct effect_owner *context = owner;
	return object == context->row ? "test/closed-effect-row/v1" : name(context->graph, object);
}

static const struct pg_object *effect_resolve(void *owner, const char *label)
{
	struct effect_owner *context = owner;
	return !strcmp(label, "test/closed-effect-row/v1") ? context->row : resolve(context->graph, label);
}

static void effect_transport(enum pg_totality totality)
{
	static const struct pg_object_class label_class = {"test-row-label"};
	static const struct pg_object label = {PG_SEMANTIC_OBJECT, &label_class};
	const struct pg_object *labels[] = {&label};
	struct pg_graph graphs[2];
	struct pg_typing typings[2];
	struct effect_owner owners[2];
	const struct pg_effect_row *rows[2];
	for (size_t i = 0; i < 2; ++i) {
		assert(!pg_graph_init(&graphs[i]) && !pg_typing_init(&typings[i], &graphs[i]));
		rows[i] = pg_effect_row(&graphs[i], 1, labels);
		owners[i] = (struct effect_owner){&graphs[i], pg_effect_reference(&graphs[i], rows[i])->as.reference};
	}
	assert(rows[0] != rows[1]);
	const struct pg_evidence *universe = pg_prove_universe(&typings[0], pg_prove_empty_context(&typings[0]), 0);
	const struct pg_evidence *formation = pg_prove_computation_type(&typings[0], totality, rows[0], universe);
	const struct pg_object *m = pg_binder(&graphs[0]), *x = pg_binder(&graphs[0]);
	const struct pg_evidence *context = pg_prove_context_extension(&typings[0],
		pg_prove_empty_context(&typings[0]), m, pg_prove_thunk_type(&typings[0], formation));
	const struct pg_evidence *source = pg_prove_force(&typings[0], pg_prove_variable(&typings[0], context, m));
	const struct pg_evidence *domain = pg_prove_projection(&typings[0], context, universe);
	const struct pg_evidence *extended = pg_prove_context_extension(&typings[0], context, x, domain);
	const struct pg_evidence *pi = pg_prove_pi(&typings[0], extended,
		pg_prove_projection(&typings[0], extended, formation));
	const struct pg_evidence *continuation = pg_prove_lambda(&typings[0], pi,
		pg_prove_projection(&typings[0], extended, source));
	const struct pg_evidence *fold = pg_prove_fold(&typings[0], source, continuation);
	const struct pg_evidence *returned = pg_prove_return_contract(&typings[0], totality,
		pg_prove_variable(&typings[0], extended, x));
	const struct pg_evidence *widened = pg_prove_effect_subsumption(&typings[0], returned,
		pg_prove_projection(&typings[0], extended, formation));
	const struct pg_evidence *saved[] = {formation, fold, widened};
	FILE *file = tmpfile();
	assert(file && formation && fold && widened && !pg_derivations_write(file, 3, saved, effect_name, &owners[0]));
	rewind(file);
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(!pg_derivations_read(file, &typings[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(count == 3 && roots[0]->parameters.effects == rows[1] && !typings[1].proofs.count);
	assert(roots[0]->parameters.totality == totality && roots[2]->premises[0]->parameters.totality == totality);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, &graphs[1]));
	assert(!pg_synthesis_init(&synthesis, &typings[1], &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *job = pg_synthesis_derivation(&synthesis, roots[0]);
	struct pg_synthesis_job *fold_job = pg_synthesis_derivation(&synthesis, roots[1]);
	struct pg_synthesis_job *widening_job = pg_synthesis_derivation(&synthesis, roots[2]);
	struct pg_synthesis_job *return_shape = pg_synthesis_classifier_structure(&synthesis,
		pg_synthesis_derivation(&synthesis, roots[2]->premises[0]));
	struct pg_synthesis_job *fold_shape = pg_synthesis_classifier_structure(&synthesis, fold_job);
	assert(job && fold_job && widening_job);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *result = pg_synthesis_result(job);
	const struct pg_effect_row *row;
	const struct pg_term *value;
	enum pg_totality grade;
	assert(pg_computation_type_view(pg_evidence_subject(result)->core, &grade, &row, &value) && row == rows[1] && grade == totality);
	assert(pg_evidence_subject(pg_prove_return_content(&typings[1], result))->core == value);
	assert(pg_synthesis_status(fold_job) == PG_SYNTHESIS_DONE);
	assert(pg_computation_type_view(pg_evidence_classifier(pg_synthesis_result(fold_job)), &grade, &row, &value) && row == rows[1] && grade == totality);
	assert(pg_synthesis_status(widening_job) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *loaded = pg_synthesis_result(widening_job);
	assert(pg_evidence_rule(loaded) == PG_EFFECT_SUBSUMPTION);
	assert(pg_evidence_subject(loaded) != pg_evidence_subject(pg_evidence_premise(loaded, 0)));
	assert(pg_evidence_subject(loaded)->core == pg_evidence_subject(pg_evidence_premise(loaded, 0))->core);
	assert(pg_evidence_subject(loaded)->classifier == pg_evidence_classifier(loaded));
	assert(pg_computation_type_view(pg_evidence_classifier(loaded), &grade, &row, &value) && row == rows[1] && grade == totality);
	assert(pg_synthesis_status(return_shape) == PG_SYNTHESIS_DONE && pg_synthesis_status(fold_shape) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_type_structure_result(return_shape) == pg_evidence_classifier(pg_evidence_premise(loaded, 0)));
	const struct pg_term *row_term;
	assert(pg_computation_type_spine_view(pg_synthesis_type_structure_result(fold_shape), &grade, &row_term, &value));
	assert(grade == totality);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	/* Locate the explicit row field through the record grammar, not a proof ID. */
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t records, root_count, row_id = 0;
	assert(!pg_wire_read_u64(file, &records) && !pg_wire_read_u64(file, &root_count));
	long row_offset = -1, grade_offset = -1, context_grade_offset = -1;
	for (uint64_t i = 0; i < records; ++i) {
		uint64_t rule, ignored, arity;
		assert(!pg_wire_read_u64(file, &rule));
		for (unsigned j = 0; j < 5; ++j) {
			if (j == 2 && rule == PG_RETURN_TYPE_FORM) grade_offset = ftell(file);
			if (j == 2 && rule == PG_CONTEXT_EMPTY) context_grade_offset = ftell(file);
			assert(!pg_wire_read_u64(file, &ignored));
		}
		long offset = ftell(file);
		assert(offset >= 0 && !pg_wire_read_u64(file, &ignored));
		if (rule == PG_RETURN_TYPE_FORM) { row_offset = offset; row_id = ignored; }
		for (unsigned j = 2; j < PG_DERIVATION_TERM_SLOTS; ++j) assert(!pg_wire_read_u64(file, &ignored));
		uint64_t metadata, allocation;
		assert(!pg_wire_read_u64(file, &metadata) && !pg_wire_read_u64(file, &allocation));
		for (uint64_t j = 0; j < metadata + allocation; ++j) assert(!pg_wire_read_u64(file, &ignored));
		assert(!pg_wire_read_u64(file, &arity));
		for (uint64_t j = 0; j < arity; ++j) assert(!pg_wire_read_u64(file, &ignored));
	}
	assert(root_count == 3 && row_offset >= 0 && row_id);
	assert(grade_offset >= 0 && context_grade_offset >= 0);
	assert(!fseek(file, grade_offset, SEEK_SET) && !pg_wire_write_u64(file, 2));
	rewind(file);
	assert(pg_derivations_read(file, &typings[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fseek(file, grade_offset, SEEK_SET) && !pg_wire_write_u64(file, totality));
	assert(!fseek(file, context_grade_offset, SEEK_SET) && !pg_wire_write_u64(file, PG_TOTALITY_TOTAL));
	rewind(file);
	assert(pg_derivations_read(file, &typings[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fseek(file, context_grade_offset, SEEK_SET) && !pg_wire_write_u64(file, 0));
	assert(!fseek(file, row_offset, SEEK_SET) && !pg_wire_write_u64(file, 0));
	rewind(file);
	assert(pg_derivations_read(file, &typings[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fseek(file, row_offset, SEEK_SET) && !pg_wire_write_u64(file, row_id));
	assert(!fseek(file, 7, SEEK_SET) && fputc(1, file) != EOF);
	rewind(file);
	assert(pg_derivations_read(file, &typings[1], 1000, 100, effect_resolve, &owners[1], &count, &roots));
	assert(!fclose(file));
	for (size_t i = 0; i < 2; ++i) {
		pg_typing_destroy(&typings[i]);
		pg_graph_destroy(&graphs[i]);
	}
}

static void classifier_transport(struct pg_graph *source)
{
	struct pg_graph graph;
	assert(!pg_graph_init(&graph));
	const struct pg_object *binder = pg_binder(source);
	const struct pg_term *domain = pg_universe(source, UINT64_MAX);
	const struct pg_term *codomain = pg_return_type(source, pg_reference(source, binder));
	const struct pg_term *pi = pg_pi(source, domain, binder, codomain);
	const struct pg_term *roots[8] = {domain, pi, pg_thunk_type(source, pi),
		pg_identity_action(source, domain)};
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		roots[4 + 2 * side] = pg_identity_transport(source, roots[3], domain, side);
		roots[5 + 2 * side] = pg_identity_lift(source, roots[3], domain, side);
	}
	FILE *file = tmpfile();
	assert(file && !pg_graph_write(file, 8, roots, name, source));
	rewind(file);
	size_t count;
	const struct pg_term *const *loaded;
	assert(!pg_graph_read(file, &graph, 100, 100, resolve, &graph, &count, &loaded));
	assert(count == 8 && loaded[0] != domain);
	assert(loaded[0] == pg_universe(&graph, UINT64_MAX));
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
	size_t before = graph.objects.count, terms_before = graph.terms.count;
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i)
		assert(!pg_classifier_resolve(&graph, invalid[i]));
	assert(graph.objects.count == before && graph.terms.count == terms_before);
	char small[2];
	assert(!pg_classifier_name(domain->as.reference, small, sizeof(small)));
	assert(!pg_classifier_name(binder, small, sizeof(small)));
	assert(!fclose(file));
	pg_graph_destroy(&graph);
}

static void rejected_prefixes(FILE *file)
{
	size_t length;
	unsigned char *bytes = file_bytes(file, &length);
	assert(length > 24);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		struct pg_typing typing;
		assert(pg_graph_init(&graph) == 0);
		assert(!pg_typing_init(&typing, &graph));
		FILE *fragment = tmpfile();
		if (cut == length) bytes[24] = 255;
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		rewind(fragment);
		size_t count = 71;
		const struct pg_derivation_input *const *roots = NULL;
		assert(pg_derivations_read(fragment, &typing, 1000, 100, resolve, &graph, &count, &roots) == -1);
		assert(count == 71 && !roots && fclose(fragment) == 0);
		pg_typing_destroy(&typing);
		pg_graph_destroy(&graph);
	}
	free(bytes);
}

static void unique_term_roots(FILE *file, struct pg_graph *graph,
	void *codec_context)
{
	rewind(file);
	char header[8];
	uint64_t records, roots, word;
	assert(fread(header, 1, sizeof(header), file) == sizeof(header));
	assert(!pg_wire_read_u64(file, &records) && !pg_wire_read_u64(file, &roots));
	size_t references = 0;
	for (uint64_t i = 0; i < records; ++i) {
		/* Rule, level, direction, totality, reduction, then Core references. */
		for (unsigned j = 0; j < 5 + PG_DERIVATION_TERM_SLOTS; ++j) {
			assert(!pg_wire_read_u64(file, &word));
			if (j >= 5 && word) ++references;
		}
		uint64_t premises;
		uint64_t metadata, allocation;
		assert(!pg_wire_read_u64(file, &metadata) && !pg_wire_read_u64(file, &allocation));
		for (uint64_t j = 0; j < metadata + allocation; ++j) assert(!pg_wire_read_u64(file, &word));
		assert(!pg_wire_read_u64(file, &premises));
		for (uint64_t j = 0; j < premises; ++j) assert(!pg_wire_read_u64(file, &word));
	}
	for (uint64_t i = 0; i < roots; ++i) assert(!pg_wire_read_u64(file, &word));
	assert(!pg_wire_read_u64(file, &word) && !word);
	assert(!pg_wire_read_u64(file, &word) && !word);
	size_t count;
	const struct pg_term *const *terms;
	assert(!pg_graph_read(file, graph, 1000, 100, resolve, codec_context, &count, &terms));
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

static void write_proofs(FILE *file, struct pg_typing *typing)
{
	classifier_transport(typing->graph);
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u = pg_prove_universe(typing, empty, 0);
	{
		const struct pg_evidence *u1 = pg_prove_universe(typing, empty, 1);
		const struct pg_operation_declaration *op = pg_operation_declaration(typing, u1, u1);
		const struct pg_object *binder = pg_binder(graph);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, empty, binder, u1);
		const struct pg_evidence *body = pg_prove_return(typing, pg_prove_variable(typing, extended, binder));
		const struct pg_evidence *continuation = pg_prove_abstract(typing, empty, extended, body);
		const struct pg_evidence *request = pg_prove_request(typing, op, pg_prove_type_value(typing, u), continuation);
		struct pg_derivation_parameters parameters;
		assert(request && !pg_derivation_parameters(request, &parameters));
		FILE *unsupported = tmpfile();
		assert(unsupported && pg_derivations_write(unsupported, 1, &request, name, typing->graph) == -1);
		fclose(unsupported);
		const struct pg_evidence *pure = pg_prove_return(typing, pg_prove_type_value(typing, u));
		const struct pg_evidence *carrier = pg_prove_classifier(typing, empty, pure);
		const struct pg_evidence *scope = pg_prove_handler_context(typing, op, empty,
			carrier, pg_binder(graph), pg_binder(graph));
		struct pg_handler_clause clause = {op, pg_prove_abstract(typing, empty,
			scope, pg_prove_projection(typing, scope, pure))};
		const struct pg_evidence *handled = pg_prove_handler(typing, pure, continuation, carrier, 1, &clause);
		assert(handled && !pg_derivation_parameters(handled, &parameters) && parameters.handler);
		unsupported = tmpfile();
		assert(unsupported && pg_derivations_write(unsupported, 1, &handled, name, typing->graph) == -1);
		fclose(unsupported);
	}
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *ca = pg_prove_context_extension(typing, empty, a, u);
	const struct pg_evidence *context = pg_prove_context_extension(typing, ca, b,
		pg_prove_universe(typing, ca, 0));
	const struct pg_object *types[] = {a, b};
	const struct pg_evidence *roots[27];
	const struct pg_evidence *under_lambda = NULL;
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_variable(typing, context, types[i]);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, context, x, domain);
		const struct pg_evidence *body = pg_prove_return(typing, pg_prove_variable(typing, extended, x));
		const struct pg_evidence *codomain = pg_prove_return_type(typing, pg_prove_variable(typing, extended, types[i]));
		const struct pg_evidence *pi = pg_prove_pi(typing, extended, codomain);
		roots[i] = pg_prove_lambda(typing, pi, body);
		if (!i) under_lambda = pg_prove_lambda(typing, pi,
			pg_prove_force(typing, pg_prove_thunk(typing, body)));
		assert(roots[i]);
	}
	roots[2] = roots[0];
	const struct pg_evidence *returned = pg_prove_return(typing, pg_prove_type_value(typing, u));
	const struct pg_evidence *forced = pg_prove_force(typing, pg_prove_thunk(typing, returned));
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(forced)->core);
	assert(job && pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	roots[3] = pg_prove_normalization(typing, forced, pg_whnf_certificate(job));
	const struct pg_evidence *u1 = pg_prove_universe(typing, empty, 1);
	struct pg_conversion conversion;
	const struct pg_term *u1_core = pg_evidence_subject(u1)->core;
	const struct pg_evidence *type_redex = pg_prove_value_type(typing, pg_prove_total_pure_value(typing,
		pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, pg_prove_type_value(typing, u1))));
	const struct pg_term *type_core = pg_evidence_subject(type_redex)->core;
	assert(pg_conversion_init(&conversion, &work, u1_core, type_core) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing,
		pg_prove_type_value(typing, u), type_redex, pg_conversion_certificate(&conversion));
	struct pg_nf_job *type_nf = pg_nf_request(&work, &pg_pure_policy, type_core);
	assert(pg_nf_advance(type_nf, 10000) == PG_NF_DONE);
	const struct pg_reduction_certificate *type_receipt = pg_nf_certificate(type_nf);
	struct pg_binding_value type_binding = {pg_binder(graph), type_core};
	const struct pg_conversion_certificate *congruence = pg_conversion_substitution(&typing->substitutions,
		type_core, u1_core, pg_reference(graph, type_binding.binder), 1, &type_binding, &type_receipt);
	/* The file carries ordinary conversion endpoints, not local authority to
	 * trust a congruence producer. Fresh-process Solve recomputes the equality. */
	roots[4] = pg_prove_conversion(typing, converted, u1, congruence);
	assert(under_lambda);
	struct pg_nf_job *nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(under_lambda)->core);
	assert(nf && pg_nf_advance(nf, 10000) == PG_NF_DONE);
	roots[5] = pg_prove_normalization(typing, under_lambda, pg_nf_certificate(nf));
	const struct pg_evidence *u2 = pg_prove_universe(typing, empty, 2);
	roots[6] = pg_prove_reflexivity(typing, u2, pg_prove_type_value(typing, u1));
	for (unsigned side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *value = pg_prove_type_value(typing, u);
		roots[7 + 2 * side] = pg_prove_identity_transport(typing, roots[6], value, side);
		roots[8 + 2 * side] = pg_prove_identity_lift(typing, roots[6], value, side);
	}
	const struct pg_object *z = pg_binder(graph);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, empty, z, u1);
	const struct pg_evidence *codomain = pg_prove_return_type(typing,
		pg_prove_universe(typing, extended, 1));
	const struct pg_evidence *continuation = pg_prove_lambda(typing,
		pg_prove_pi(typing, extended, codomain),
		pg_prove_return(typing, pg_prove_variable(typing, extended, z)));
	const struct pg_evidence *fold = pg_prove_fold(typing, forced, continuation);
	assert(fold);
	job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(fold)->core);
	assert(job && pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	roots[11] = pg_prove_normalization(typing, fold, pg_whnf_certificate(job));
	const struct pg_object *f = pg_binder(graph);
	const struct pg_evidence *fc = pg_prove_family_context_extension(typing, empty, f, ca,
		pg_prove_projection(typing, ca, u));
	const struct pg_evidence *fv = pg_prove_variable(typing, fc, f);
	roots[12] = pg_prove_family_abstraction(typing, fc, fv);
	roots[13] = pg_prove_family_application(typing, pg_prove_projection(typing, fc, roots[12]), fv);
	assert(roots[13]);
	job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(roots[13])->core);
	assert(job && pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	roots[14] = pg_prove_normalization(typing, roots[13], pg_whnf_certificate(job));
	const struct pg_object *h = pg_binder(graph);
	const struct pg_evidence *hc = pg_prove_family_context_extension(typing, empty, h, fc,
		pg_prove_projection(typing, fc, u));
	roots[15] = pg_prove_substitution_lift(typing, pg_prove_substitution_projection(typing, empty, ca), hc, h);
	roots[16] = pg_prove_reindex(typing, roots[15], pg_prove_variable(typing, hc, h));
	/* Independent readback may freshen a bound pointer. Persist the receipt
	 * against the original typed premise, not an erased-Core proof lookup. */
	const struct pg_object *renamed = pg_binder(graph);
	const struct pg_binding_value binding = {x, pg_reference(graph, renamed)};
	const struct pg_term *original = pg_evidence_subject(under_lambda)->core;
	const struct pg_term *alpha = pg_lambda(graph, renamed,
		pg_substitution_compute(&typing->substitutions, original->as.lambda.body, 1, &binding));
	assert(alpha != original && pg_alpha_equal(alpha, original) == 1);
	nf = pg_nf_request(&work, &pg_pure_policy, alpha);
	assert(nf && pg_nf_advance(nf, 10000) == PG_NF_DONE);
	roots[17] = pg_prove_normalization(typing, under_lambda, pg_nf_certificate(nf));
	const struct pg_evidence *pi = pg_evidence_premise(under_lambda, 0);
	const struct pg_term *domain, *body;
	const struct pg_object *bound;
	assert(pg_pi_view(pg_evidence_subject(pi)->core, &domain, &bound, &body) && bound == x);
	alpha = pg_pi(graph, domain, renamed,
		pg_substitution_compute(&typing->substitutions, body, 1, &binding));
	nf = pg_nf_request(&work, &pg_pure_policy, alpha);
	assert(nf && pg_nf_advance(nf, 10000) == PG_NF_DONE);
	roots[18] = pg_prove_normalization(typing, pi, pg_nf_certificate(nf));
	/* Child NF exposes the right-unit rule, then THUNK/FORCE contracts. The
	 * saved prefix must stop before that last contraction, not claim NF. */
	const struct pg_object *m = pg_binder(graph);
	const struct pg_evidence *mc = pg_prove_context_extension(typing, context, m,
		pg_prove_thunk_type(typing, pg_prove_return_type(typing, pg_prove_variable(typing, context, a))));
	const struct pg_evidence *fold_source = pg_prove_fold(typing,
		pg_prove_force(typing, pg_prove_variable(typing, mc, m)), pg_prove_projection(typing, mc, under_lambda));
	const struct pg_evidence *staged = pg_prove_thunk(typing, fold_source);
	assert(staged);
	nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(staged)->core);
	const struct pg_reduction_certificate *prefix = NULL;
	while (!prefix) {
		assert(pg_nf_steps(nf) < 10000 && pg_nf_advance(nf, 1) != PG_NF_ERROR);
		assert(pg_nf_prefix_certificate(nf, &prefix) >= 0);
	}
	assert(pg_nf_status(nf) == PG_NF_PENDING);
	roots[19] = pg_prove_normalization(typing, staged, prefix);
	assert(pg_reduction_target(prefix) != pg_reference(graph, m));
	nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(fold_source)->core);
	assert(pg_nf_status(nf) == PG_NF_DONE && !pg_reduction_head_congruence(pg_nf_certificate(nf)));
	roots[20] = pg_prove_normalization(typing, fold_source, pg_nf_certificate(nf));
	const struct pg_evidence *nested = pg_prove_fold(typing, fold_source, pg_prove_projection(typing, mc, under_lambda));
	nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(nested)->core);
	while (pg_nf_advance(nf, 1) == PG_NF_PENDING) assert(pg_nf_steps(nf) < 10000);
	assert(pg_nf_status(nf) == PG_NF_DONE);
	roots[21] = pg_prove_normalization(typing, nested, pg_nf_certificate(nf));
	/* The outer binder is unused, but the selected inner codomain depends
	 * on its own binder. Save the parent and both checked input views. */
	const struct pg_object *inner = pg_binder(graph);
	const struct pg_evidence *inner_scope = pg_prove_context_extension(typing, extended, inner,
		pg_prove_projection(typing, extended, u));
	const struct pg_evidence *variable = pg_prove_variable(typing, inner_scope, inner);
	const struct pg_evidence *path = pg_prove_identity_type(typing,
		pg_prove_projection(typing, inner_scope, u), variable, variable);
	const struct pg_evidence *dependent = pg_prove_pi(typing, inner_scope, pg_prove_return_type(typing, path));
	roots[22] = pg_prove_pi_constant_codomain(typing, pg_prove_pi(typing, extended, dependent));
	for (size_t i = 0; i < 2; ++i) {
		struct pg_typed_query *query = pg_typed_input_request(typing, roots[22], i);
		while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
		roots[23 + i] = pg_typed_query_result(query);
	}
	/* Selecting from a normalized Pi must expose its current type, not the
	 * redex still retained as the normalization's source. */
	const struct pg_evidence *redex_pi = pg_prove_pi(typing, extended,
		pg_prove_return_type(typing, pg_prove_projection(typing, extended, type_redex)));
	nf = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(redex_pi)->core);
	assert(pg_nf_advance(nf, 10000) == PG_NF_DONE);
	roots[25] = pg_prove_pi_codomain(typing,
		pg_prove_normalization(typing, redex_pi, pg_nf_certificate(nf)), pg_prove_type_value(typing, u));
	struct pg_typed_query *query = pg_typed_input_request(typing, roots[25], 0);
	while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
	roots[26] = pg_typed_query_result(query);
	assert(roots[26] && pg_evidence_subject(roots[26])->core == u1_core);
	for (size_t i = 0; i < 27; ++i) assert(roots[i]);
	assert(pg_derivations_write(file, 27, roots, name, typing->graph) == 0);
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

static void read_proofs(FILE *file, struct pg_typing *typing, uint64_t chunk)
{
	unique_term_roots(file, typing->graph, typing->graph);
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(pg_derivations_read(file, typing, 1000, 100, resolve, typing->graph, &count, &roots) == 0);
	assert(count == 27 && roots[0] == roots[2] && roots[0] != roots[1]);
	assert(typing->proofs.count == 0);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	struct pg_synthesis synthesis;
	assert(pg_synthesis_init(&synthesis, typing, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	struct pg_synthesis_job *jobs[27];
	for (size_t i = 0; i < count; ++i) {
		jobs[i] = pg_synthesis_derivation(&synthesis, roots[i]);
		assert(jobs[i] && pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_PENDING);
	}
	assert(jobs[0] == jobs[2] && typing->proofs.count == 0);
	struct pg_synthesis_job *consumer = source_use(&synthesis, jobs[6], "copy := loaded;");
	struct pg_synthesis_job *expect = source_use(&synthesis, jobs[6], "copy := loaded :: @;");
	struct pg_synthesis_job *family_use = source_use(&synthesis, jobs[12], "copy := loaded (@\\A : @ => {});");
	struct pg_synthesis_job *family_thunk = source_use(&synthesis, jobs[12], "copy := loaded &(\\A : @ => A);");
	struct pg_synthesis_job *family_expect = source_use(&synthesis, jobs[12], "copy := loaded (@\\A : @ => {}) :: @;");
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
	assert(pg_synthesis_status(family_use) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(pg_synthesis_result(family_use)) == PG_JUDGEMENT_TYPE_FAMILY);
	/* Quoted pure type functions pass the same family contract after loading;
	 * a raw thunk is still not a kernel-level logical family argument. */
	assert(pg_synthesis_status(family_thunk) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *contracted = pg_synthesis_result(family_thunk);
	assert(pg_evidence_rule(contracted) == PG_TYPE_FAMILY_APP);
	assert(pg_evidence_judgement(contracted) == PG_JUDGEMENT_TYPE_FAMILY);
	assert(pg_evidence_judgement(pg_evidence_premise(contracted, 1)) == PG_JUDGEMENT_TYPE_FAMILY);
	assert(pg_synthesis_status(family_expect) == PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *family_application = pg_synthesis_result(jobs[13]);
	assert(pg_evidence_rule(family_application) == PG_TYPE_FAMILY_APP);
	assert(pg_evidence_subject(pg_synthesis_result(jobs[14]))->core ==
		pg_evidence_subject(pg_evidence_premise(family_application, 1))->core);
	const struct pg_evidence *lifted = pg_synthesis_result(jobs[15]);
	assert(pg_evidence_rule(lifted) == PG_CONTEXT_SUBSTITUTION);
	assert(pg_evidence_judgement(pg_synthesis_result(jobs[16])) == PG_JUDGEMENT_TYPE_FAMILY);
	const struct pg_object *source_binder = pg_evidence_context(pg_evidence_premise(lifted, 0))->binder;
	assert(pg_evidence_subject(pg_synthesis_result(jobs[16]))->core ==
		pg_evidence_subject(pg_substitution_image(typing, lifted, source_binder))->core);
	const struct pg_evidence *left = pg_synthesis_result(jobs[0]);
	const struct pg_evidence *right = pg_synthesis_result(jobs[1]);
	assert(pg_evidence_subject(left)->core == pg_evidence_subject(right)->core);
	assert(pg_evidence_subject(left) != pg_evidence_subject(right));
	assert(pg_evidence_classifier(left) != pg_evidence_classifier(right));
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[3]))) == PG_REDUCTION_WHNF);
	assert(pg_reduction_kind(pg_evidence_normalization(pg_synthesis_result(jobs[5]))) == PG_REDUCTION_NF);
	const struct pg_evidence *prefix_result = pg_synthesis_result(jobs[19]);
	const struct pg_reduction_certificate *prefix = pg_evidence_normalization(prefix_result);
	assert(prefix && pg_reduction_kind(prefix) == PG_REDUCTION_PREFIX);
	assert(pg_evidence_classifier(prefix_result) == pg_evidence_classifier(pg_evidence_premise(prefix_result, 0)));
	struct pg_nf_job *prefix_nf = pg_nf_request(&work, &pg_pure_policy, pg_reduction_source(prefix));
	assert(pg_nf_status(prefix_nf) == PG_NF_PENDING && !pg_nf_certificate(prefix_nf));
	assert(pg_nf_advance(prefix_nf, 10000) == PG_NF_DONE);
	assert(pg_nf_result(prefix_nf) != pg_reduction_target(prefix));
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(jobs[17]))->core,
		pg_evidence_subject(pg_synthesis_result(jobs[5]))->core) == 1);
	assert(pg_evidence_rule(pg_synthesis_result(jobs[6])) == PG_REFLEXIVITY);
	const struct pg_evidence *fold_result = pg_synthesis_result(jobs[11]);
	assert(pg_evidence_rule(pg_evidence_premise(fold_result, 0)) == PG_FOLD_ELIM);
	assert(pg_evidence_subject(fold_result)->core == pg_evidence_subject(pg_synthesis_result(jobs[3]))->core);
	/* Restored normalization exposes typed result inputs only after ordinary
	 * Solve. A renamed NF binder needs a checked child scope action, not the
	 * original Lambda body's context copied onto its new Core. */
	const size_t normalized[] = {3, 5, 11, 17, 18, 20, 21};
	for (size_t i = 0; i < sizeof(normalized) / sizeof(*normalized); ++i) {
		const struct pg_evidence *parent = pg_synthesis_result(jobs[normalized[i]]);
		const struct pg_term *core = pg_evidence_subject(parent)->core;
		const struct pg_term *domain, *body;
		const struct pg_object *binder;
		size_t ordinal = pg_pi_view(core, &domain, &binder, &body) ? 1 : 0;
		struct pg_typed_query *input = pg_typed_input_request(typing, parent, ordinal);
		assert(input);
		while (!pg_typed_query_advance(input, chunk)) assert(pg_typed_query_steps(input) < 10000);
		const struct pg_evidence *child = pg_typed_query_result(input);
		assert(child);
		if (core->kind == PG_LAMBDA || ordinal) {
			if (!ordinal) { binder = core->as.lambda.binder; body = core->as.lambda.body; }
			assert(pg_evidence_subject(child)->core == body);
			assert(pg_evidence_context(child)->parent == pg_evidence_context(parent));
			assert(pg_evidence_context(child)->binder == binder);
		} else {
			assert(core->kind == PG_APPLICATION);
			assert(core->as.application.function == pg_reference(typing->graph,
				normalized[i] >= 20 ? &pg_force_operation : &pg_return_operation));
			assert(pg_evidence_subject(child)->core == core->as.application.argument);
			assert(pg_evidence_context(child) == pg_evidence_context(parent));
		}
		uint64_t steps = pg_typed_query_steps(input);
		assert(pg_typed_input_request(typing, parent, ordinal) == input);
		assert(pg_typed_query_advance(input, 64) == 1 && pg_typed_query_steps(input) == steps);
	}
	assert(!pg_computation_resolve("kernel/fold/v2"));
	const struct pg_evidence *selected = pg_synthesis_result(jobs[22]);
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	assert(pg_pi_view(pg_evidence_subject(selected)->core, &domain, &binder, &body));
	assert(!pg_prove_pi_constant_codomain(typing, selected));
	for (size_t i = 0; i < 2; ++i) {
		struct pg_typed_query *query = pg_typed_input_request(typing, selected, i);
		while (!pg_typed_query_advance(query, chunk)) assert(pg_typed_query_steps(query) < 10000);
		const struct pg_evidence *child = pg_typed_query_result(query), *saved = pg_synthesis_result(jobs[23 + i]);
		assert(child && pg_evidence_subject(child)->core == (i ? body : domain));
		assert(pg_evidence_subject(child)->core == pg_evidence_subject(saved)->core);
		assert(pg_evidence_context(child) == pg_evidence_context(saved));
		assert(pg_evidence_classifier(child) == pg_evidence_classifier(saved));
		if (i) {
			assert(pg_evidence_context(child)->parent == pg_evidence_context(selected));
			assert(pg_evidence_context(child)->binder == binder);
		} else assert(pg_evidence_context(child) == pg_evidence_context(selected));
	}
	struct pg_typed_query *selected_input = pg_typed_input_request(typing, pg_synthesis_result(jobs[25]), 0);
	while (!pg_typed_query_advance(selected_input, chunk)) assert(pg_typed_query_steps(selected_input) < 10000);
	const struct pg_evidence *current_type = pg_typed_query_result(selected_input);
	assert(current_type && pg_evidence_subject(current_type)->core == pg_universe(typing->graph, 1));
	assert(pg_evidence_subject(current_type)->core == pg_evidence_subject(pg_synthesis_result(jobs[26]))->core);
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
	const struct pg_derivation_input *extended = roots[0]->premises[0]->premises[0];
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
	/* A completed NF cache cannot satisfy an intermediate-prefix endpoint. */
	const struct pg_derivation_input *prefix_input = roots[19];
	size_t prefix_bytes = sizeof(*prefix_input) + prefix_input->count * sizeof(*prefix_input->premises);
	struct pg_derivation_input *overshoot = pg_alloc(typing->graph, prefix_bytes);
	assert(overshoot);
	memcpy(overshoot, prefix_input, prefix_bytes);
	overshoot->target = pg_nf_result(prefix_nf);
	bad = pg_synthesis_derivation(&synthesis, overshoot);
	assert(bad && pg_synthesis_status(bad) == PG_SYNTHESIS_PENDING);
	pg_synthesis_advance(&synthesis, 10000);
	assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
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

static void operation_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk, enum pg_totality totality)
{
	struct pg_graph *graph = typing->graph;
	if (writing) {
		const struct pg_evidence *empty = pg_prove_empty_context(typing);
		const struct pg_evidence *u0 = pg_prove_universe(typing, empty, 0);
		const struct pg_evidence *u1 = pg_prove_universe(typing, empty, 1);
		const struct pg_evidence *value = pg_prove_type_value(typing, u0);
		const struct pg_operation_declaration *operations[] = {
			pg_operation_declaration(typing, u1, u1), pg_operation_declaration(typing, u1, u1)};
		const struct pg_object *x = pg_binder(graph);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, empty, x, u1);
		const struct pg_evidence *returned = pg_prove_abstract(typing, empty, extended,
			pg_prove_return_contract(typing, totality, pg_prove_variable(typing, extended, x)));
		const struct pg_evidence *request = pg_prove_request(typing, operations[0], value, returned);
		const struct pg_evidence *carrier = pg_prove_computation_type(typing, totality,
			pg_effect_row(graph, 0, NULL), u1);
		struct pg_handler_clause clauses[2];
		for (size_t i = 0; i < 2; ++i) {
			const struct pg_object *a = pg_binder(graph), *k = pg_binder(graph);
			const struct pg_evidence *scope = pg_prove_handler_context(typing, operations[i], empty, carrier, a, k);
			const struct pg_evidence *body = pg_prove_application(typing,
				pg_prove_force(typing, pg_prove_variable(typing, scope, k)), pg_prove_variable(typing, scope, a));
			clauses[i] = (struct pg_handler_clause){operations[i], pg_prove_abstract(typing, empty, scope, body)};
		}
		const struct pg_evidence *handled = pg_prove_handler(typing, request, returned, carrier, 2, clauses);
		const struct pg_object *label = pg_operation_label(operations[0]);
		const struct pg_evidence *forward_carrier = pg_prove_computation_type(typing, totality,
			pg_effect_row(graph, 1, &label), u1);
		const struct pg_object *a = pg_binder(graph), *k = pg_binder(graph);
		const struct pg_evidence *scope = pg_prove_handler_context(typing, operations[1], empty, forward_carrier, a, k);
		const struct pg_evidence *body = pg_prove_application(typing,
			pg_prove_force(typing, pg_prove_variable(typing, scope, k)), pg_prove_variable(typing, scope, a));
		struct pg_handler_clause forward = {operations[1], pg_prove_abstract(typing, empty, scope, body)};
		const struct pg_evidence *forwarded = pg_prove_handler(typing, request, returned, forward_carrier, 1, &forward);
		assert(forwarded);
		struct pg_whnf_work work;
		assert(!pg_whnf_work_init(&work, graph));
		struct pg_whnf_job *head = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(forwarded)->core);
		while (pg_whnf_advance(head, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(head) < 10000);
		forwarded = pg_prove_normalization(typing, forwarded, pg_whnf_certificate(head));
		const struct pg_evidence *roots[] = {request, handled, handled, u0, forwarded, NULL, NULL};
		for (size_t i = 0; i < 2; ++i) {
			struct pg_typed_query *query = pg_typed_input_request(typing, forwarded, i);
			while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
			roots[5 + i] = pg_typed_query_result(query);
			assert(roots[5 + i]);
		}
		assert(request && handled && !pg_derivations_write_descriptors(file, 7, roots, &pg_builtin_graph_codec, typing->graph));
		pg_whnf_work_destroy(&work);
		return;
	}
	size_t count = 0;
	const struct pg_derivation_input *const *roots = NULL;
	assert(!pg_derivations_read_descriptors(file, typing, 10000, 100, &pg_builtin_graph_codec, typing->graph, &count, &roots));
	assert(count == 7 && roots[1] == roots[2] && !typing->proofs.count);
	const struct pg_object *label = roots[0]->parameters.operation_label;
	assert(label && pg_handler_signature_count(roots[1]->parameters.handler) == 2);
	assert(pg_handler_signature_label(roots[1]->parameters.handler, 0) == label);
	assert(pg_handler_signature_label(roots[1]->parameters.handler, 1) != label);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, graph));
	assert(!pg_synthesis_init(&synthesis, typing, &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *request = pg_synthesis_derivation(&synthesis, roots[0]);
	struct pg_synthesis_job *shape = pg_synthesis_classifier_structure(&synthesis, request);
	struct pg_synthesis_job *handled = pg_synthesis_derivation(&synthesis, roots[1]);
	struct pg_synthesis_job *forwarded[3];
	for (size_t i = 0; i < 3; ++i) forwarded[i] = pg_synthesis_derivation(&synthesis, roots[4 + i]);
	assert(handled == pg_synthesis_derivation(&synthesis, roots[2]));
	unsigned rounds = 0;
	while (pg_synthesis_status(handled) == PG_SYNTHESIS_PENDING) {
		assert(++rounds < 10000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(handled) == PG_SYNTHESIS_DONE && pg_synthesis_status(request) == PG_SYNTHESIS_DONE);
	for (size_t i = 0; i < 3; ++i) {
		while (pg_synthesis_status(forwarded[i]) == PG_SYNTHESIS_PENDING) {
			assert(++rounds < 10000);
			pg_synthesis_advance(&synthesis, chunk);
		}
		assert(pg_synthesis_status(forwarded[i]) == PG_SYNTHESIS_DONE);
	}
	for (size_t i = 0; i < 2; ++i) {
		struct pg_typed_query *query = pg_typed_input_request(typing, pg_synthesis_result(forwarded[0]), i);
		while (!pg_typed_query_advance(query, chunk)) assert(pg_typed_query_steps(query) < 10000);
		const struct pg_evidence *input = pg_typed_query_result(query), *saved = pg_synthesis_result(forwarded[i + 1]);
		assert(input && pg_evidence_context(input) == pg_evidence_context(saved));
		assert(pg_alpha_equal(pg_evidence_subject(input)->core, pg_evidence_subject(saved)->core) == 1);
		assert(pg_alpha_equal(pg_evidence_classifier(input), pg_evidence_classifier(saved)) == 1);
	}
	const struct pg_evidence *proof = pg_synthesis_result(handled);
	assert(pg_evidence_handler_signature(proof) == roots[1]->parameters.handler);
	assert(pg_operation_label(pg_evidence_request_declaration(pg_synthesis_result(request))) == label);
	const struct pg_effect_row *row;
	const struct pg_term *type;
	enum pg_totality grade;
	assert(pg_computation_type_view(pg_evidence_classifier(proof), &grade, &row, &type) && !pg_effect_count(row));
	assert(grade == totality);
	assert(pg_synthesis_status(shape) == PG_SYNTHESIS_DONE);
	const struct pg_term *row_term;
	assert(pg_computation_type_spine_view(pg_synthesis_type_structure_result(shape), &grade, &row_term, &type));
	assert(grade == totality);
	struct pg_synthesis_job *nf = pg_synthesis_nf(&synthesis, pg_prove_empty_context(typing), proof);
	while (pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING) {
		assert(++rounds < 10000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *value = pg_prove_return_value(typing, pg_synthesis_result(nf));
	assert(value && pg_evidence_subject(value)->core == pg_universe(typing->graph, 0));
	struct pg_derivation_input *wrong = pg_alloc(graph, sizeof(*wrong) + 4 * sizeof(*wrong->premises));
	assert(wrong && roots[0]->count == 4);
	memcpy(wrong, roots[0], sizeof(*wrong) + 4 * sizeof(*wrong->premises));
	wrong->premises[0] = roots[3];
	struct pg_synthesis_job *invalid = pg_synthesis_derivation(&synthesis, wrong);
	while (pg_synthesis_status(invalid) == PG_SYNTHESIS_PENDING) {
		assert(++rounds < 10000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(invalid) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(invalid));
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("operation derivations: relocated labels, shared premises, handler execution and signature rejection passed");
}

static struct pg_derivation_input *input_rule(struct pg_graph *graph, enum pg_evidence_rule rule,
	size_t count, const struct pg_derivation_input *const *premises)
{
	struct pg_derivation_input *input = pg_alloc(graph, sizeof(*input) + count * sizeof(*input->premises));
	assert(input);
	input->rule = rule;
	input->count = count;
	for (size_t i = 0; i < count; ++i) input->premises[i] = premises[i];
	return input;
}

static void unaccepted_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk)
{
	struct pg_graph *graph = typing->graph;
	if (writing) {
		const struct pg_derivation_input *empty = input_rule(graph, PG_CONTEXT_EMPTY, 0, NULL);
		const struct pg_derivation_input *universe = input_rule(graph, PG_UNIVERSE_FORM, 1, &empty);
		const struct pg_derivation_input *value = input_rule(graph, PG_VALUE_FROM_TYPE, 1, &universe);
		const struct pg_derivation_input *returned = input_rule(graph, PG_RETURN_INTRO, 1, &value);
		const struct pg_derivation_input *invalid = input_rule(graph, PG_FORCE_ELIM, 1, &value);
		struct pg_derivation_input *nf = input_rule(graph, PG_PURE_NORMALIZATION, 1, &returned);
		nf->reduction_kind = PG_REDUCTION_NF;
		nf->source = nf->target = pg_application(graph, pg_reference(graph, &pg_return_operation), pg_universe(typing->graph, 0));
		const struct pg_derivation_input *roots[] = {returned, returned, invalid, nf};
		assert(!pg_derivation_inputs_write(file, 4, roots, &pg_builtin_graph_codec, typing->graph));
		assert(!typing->proofs.count);
		struct pg_derivation_input *cycle = input_rule(graph, PG_RETURN_INTRO, 1, &value);
		cycle->premises[0] = cycle;
		const struct pg_derivation_input *cyclic = cycle;
		FILE *rejected = tmpfile();
		assert(rejected && pg_derivation_inputs_write(rejected, 1, &cyclic, &pg_builtin_graph_codec, typing->graph));
		assert(!fclose(rejected));
		struct pg_derivation_input *open = input_rule(graph, PG_RETURN_TYPE_FORM, 1, &universe);
		open->effect_parameter = pg_binder(graph);
		const struct pg_derivation_input *unresolved = open;
		rejected = tmpfile();
		assert(rejected && pg_derivation_inputs_write(rejected, 1, &unresolved, &pg_builtin_graph_codec, typing->graph));
		assert(!fclose(rejected));
		return;
	}
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(!pg_derivations_read_descriptors(file, typing, 1000, 100, &pg_builtin_graph_codec, typing->graph, &count, &roots));
	assert(count == 4 && roots[0] == roots[1] && !typing->proofs.count);
	assert(roots[3]->reduction_kind == PG_REDUCTION_NF && !roots[3]->parameters.reduction);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, graph));
	assert(!pg_synthesis_init(&synthesis, typing, &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *jobs[4];
	for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_derivation(&synthesis, roots[i]);
	assert(jobs[0] == jobs[1] && !typing->proofs.count);
	unsigned rounds = 0;
	for (size_t i = 0; i < count; ++i) {
		while (pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_PENDING) {
			assert(++rounds < 1000);
			pg_synthesis_advance(&synthesis, chunk);
		}
		assert(pg_synthesis_status(jobs[i]) == (i == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	}
	assert(!pg_synthesis_result(jobs[2]));
	const struct pg_evidence *value = pg_prove_return_value(typing, pg_synthesis_result(jobs[3]));
	assert(value && pg_evidence_subject(value)->core == pg_universe(typing->graph, 0));
	/* No-op normalization reuses the source evidence after checking the goal. */
	assert(pg_synthesis_result(jobs[3]) == pg_synthesis_result(jobs[0]));
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("unaccepted derivations: no pre-solve evidence, shared inputs, rejection and NF obligation passed");
}

static void rejected_effect_images(FILE *file)
{
	size_t length;
	unsigned char *bytes = file_bytes(file, &length);
	for (size_t cut = 0; cut <= length; ++cut) {
		struct pg_graph graph;
		struct pg_effect_inference effects;
		assert(!pg_graph_init(&graph));
		struct pg_typing typing;
		assert(!pg_typing_init(&typing, &graph));
		assert(!pg_effect_inference_init(&effects, &graph));
		FILE *fragment = tmpfile();
		assert(fragment && fwrite(bytes, 1, cut, fragment) == cut);
		/* Complete stream whose first rule now requires a missing row. This
		 * fails after definition reconstruction, not during byte decoding. */
		if (cut == length) {
			assert(!fseek(fragment, 24, SEEK_SET));
			assert(!pg_wire_write_u64(fragment, PG_RETURN_TYPE_FORM));
		}
		rewind(fragment);
		size_t count = 71;
		const struct pg_derivation_input *const *roots = NULL;
		assert(pg_derivations_read_inference(fragment, &typing, 1000, 100, &effects,
			&pg_builtin_graph_codec, &graph, &count, &roots));
		assert(count == 71 && !roots && effects.failed);
		pg_typing_destroy(&typing);
		if (cut == length) assert(effects.row_sources.count == 2);
		pg_effect_inference_seal(&effects);
		assert(pg_effect_inference_advance(&effects, 1000) == -1);
		assert(!fclose(fragment));
		pg_effect_inference_destroy(&effects);
		pg_graph_destroy(&graph);
	}
	free(bytes);
}

static void pending_effect_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk)
{
	struct pg_graph *graph = typing->graph;
	struct pg_effect_inference effects;
	assert(!pg_effect_inference_init(&effects, graph));
	const struct pg_effect_row *empty_row = pg_effect_row(graph, 0, NULL);
	if (writing) {
		const struct pg_term *u = pg_universe(typing->graph, 0);
		const struct pg_object *labels[] = {pg_operation_label_create(graph, u, u), pg_operation_label_create(graph, u, u)};
		const struct pg_effect_row *seed = pg_effect_row(graph, 2, labels);
		struct pg_effect_equation *source = pg_effect_equation(&effects, seed), *target = pg_effect_equation(&effects, empty_row);
		assert(!pg_effect_dependency(&effects, source, pg_effect_row(graph, 1, labels), target));
		assert(!pg_effect_dependency(&effects, target, empty_row, source));
		const struct pg_derivation_input *empty = input_rule(graph, PG_CONTEXT_EMPTY, 0, NULL);
		const struct pg_derivation_input *universe = input_rule(graph, PG_UNIVERSE_FORM, 1, &empty);
		struct pg_derivation_input *f = input_rule(graph, PG_RETURN_TYPE_FORM, 1, &universe);
		f->effect_parameter = pg_effect_equation_parameter(&effects, target);
		const struct pg_derivation_input *f_input = f;
		const struct pg_derivation_input *thunk = input_rule(graph, PG_THUNK_TYPE_FORM, 1, &f_input);
		struct pg_derivation_input *context = input_rule(graph, PG_CONTEXT_EXTEND, 2,
			(const struct pg_derivation_input *[]){empty, thunk});
		context->parameters.binder = pg_binder(graph);
		const struct pg_derivation_input *context_input = context;
		const struct pg_derivation_input *inner_u = input_rule(graph, PG_UNIVERSE_FORM, 1, &context_input);
		struct pg_derivation_input *inner_f = input_rule(graph, PG_RETURN_TYPE_FORM, 1, &inner_u);
		inner_f->effect_parameter = f->effect_parameter;
		const struct pg_derivation_input *pi = input_rule(graph, PG_PI_FORM, 2,
			(const struct pg_derivation_input *[]){context, inner_f});
		struct pg_derivation_input *invalid = input_rule(graph, PG_RETURN_TYPE_FORM, 1, &empty);
		invalid->effect_parameter = f->effect_parameter;
		const struct pg_derivation_input *roots[] = {f, inner_f, pi, pi, invalid};
		pg_effect_inference_seal(&effects);
		assert(!pg_effect_inference_advance(&effects, 1));
		assert(!pg_effect_inference_result(&effects, target));
		assert(!pg_derivation_inputs_write_inference(file, 5, roots, &effects, &pg_builtin_graph_codec, typing->graph));
		assert(!typing->proofs.count);
	} else {
		size_t count;
		const struct pg_derivation_input *const *roots;
		assert(!pg_derivations_read_inference(file, typing, 1000, 100, &effects, &pg_builtin_graph_codec, typing->graph, &count, &roots));
		assert(count == 5 && roots[2] == roots[3] && !typing->proofs.count && !effects.sealed);
		const struct pg_object *parameter = roots[0]->effect_parameter;
		assert(parameter && parameter == roots[1]->effect_parameter && parameter == roots[4]->effect_parameter);
		struct pg_effect_equation *target = pg_effect_equation_find(&effects, parameter);
		assert(target && pg_effect_equation_seed(&effects, target) == empty_row);
		struct pg_whnf_work normalization;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&normalization, graph));
		assert(!pg_synthesis_init(&synthesis, typing, &normalization, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job *jobs[5];
		for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_derivation_inference(&synthesis, roots[i], &effects);
		assert(jobs[2] == jobs[3] && !typing->proofs.count);
		struct pg_synthesis_job *structure = pg_synthesis_type_structure(&synthesis, jobs[2]);
		while (synthesis.ready) { assert(synthesis.steps < 2000); pg_synthesis_advance(&synthesis, chunk); }
		assert(!pg_synthesis_result(jobs[0]) && !pg_synthesis_result(jobs[2]));
		const struct pg_term *pending = pg_effect_type_spine(typing->graph, pg_reference(graph, parameter), pg_universe(typing->graph, 0));
		const struct pg_object *binder = roots[2]->premises[0]->parameters.binder;
		assert(pg_synthesis_type_structure_result(structure) == pg_pi(graph, pg_thunk_type(typing->graph, pending), binder, pending));
		pg_effect_inference_seal(&effects);
		assert(pg_synthesis_effect_inference(&synthesis, &effects));
		while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
		for (size_t i = 0; i < count; ++i)
			assert(pg_synthesis_status(jobs[i]) == (i == 4 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		const struct pg_effect_row *row = pg_effect_inference_result(&effects, target);
		assert(pg_effect_count(row) == 1);
		size_t equation_count, term_count;
		const struct pg_term *const *definitions;
		assert(!pg_effect_inference_pack(&effects, graph, &equation_count, &term_count, &definitions));
		assert(equation_count == 2 && term_count == 10);
		unsigned masks = 0;
		for (size_t i = 2 * equation_count; i < term_count; i += 3) {
			const struct pg_effect_row *mask = pg_effect_row_view(definitions[i + 1]);
			if (!pg_effect_count(mask)) continue;
			assert(pg_effect_count(mask) == 1 && !pg_effect_contains(row, pg_effect_label(mask, 0)));
			++masks;
		}
		assert(masks == 1);
		const struct pg_term *closed = pg_effect_type(typing->graph, row, pg_universe(typing->graph, 0));
		assert(pg_evidence_subject(pg_synthesis_result(jobs[2]))->core == pg_pi(graph, pg_thunk_type(typing->graph, closed), binder, closed));
		assert(pg_effect_equation_seed(&effects, target) == empty_row);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&normalization);
		rewind(file);
		assert(pg_derivations_read_descriptors(file, typing, 1000, 100, &pg_builtin_graph_codec, typing->graph, &count, &roots));
		rejected_effect_images(file);
		puts("pending effect derivations: relocated open Pi, no early evidence, masked cyclic Solve and rejection passed");
	}
	pg_effect_inference_destroy(&effects);
}

static void direct_input_dependencies(const struct pg_derivation_input *input,
	const struct pg_graph_codec *codec, void *owner)
{
	struct pg_dag contexts, terms, direct, packed;
	struct pg_graph storage = {0};
	assert(!pg_graph_init(&storage));
	assert(!pg_dag_init(&contexts, pg_context_dependency, NULL));
	assert(!pg_dag_init(&direct, NULL, NULL) && !pg_dag_init(&packed, NULL, NULL));
	assert(!pg_graph_dependencies_init(&terms, &direct, codec, owner));
	assert(!pg_derivation_input_collect(&terms, &contexts, input));
	size_t nc = contexts.count, nt = terms.count, no = direct.count;
	for (unsigned i = 0; i < 128; ++i) {
		assert(!pg_derivation_input_collect(&terms, &contexts, input));
		assert(contexts.count == nc && terms.count == nt && direct.count == no);
	}
	struct pg_derivation_payload payload;
	assert(!pg_derivation_input_terms(&storage, input, &payload));
	for (size_t i = 0; i < payload.count; ++i)
		if (payload.terms[i]) assert(!pg_graph_collect_objects(&packed, 1, &payload.terms[i], codec, owner));
	assert(direct.count == packed.count);
	for (const struct pg_dag_node *node = packed.first; node; node = node->next)
		assert(pg_dag_find(&direct, node->key));
	if (input->parameters.induction) {
		struct pg_derivation_input invalid = *input;
		invalid.rule = PG_MATCH_ELIM;
		assert(pg_derivation_input_collect(&terms, &contexts, &invalid) == -1);
	}
	pg_graph_destroy(&storage);
	pg_dag_destroy(&terms); pg_dag_destroy(&contexts);
	pg_dag_destroy(&direct); pg_dag_destroy(&packed);
}

static FILE *producer_snapshot(struct pg_synthesis *synthesis, size_t count,
	struct pg_synthesis_job **jobs, struct pg_graph *graph)
{
	struct pg_graph storage;
	struct pg_effect_inference effects;
	assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&effects, &storage));
	const struct pg_derivation_input *const *inputs;
	uint64_t steps = synthesis->steps;
	size_t proofs = synthesis->typing->proofs.count, requests = synthesis->jobs.count;
	assert(!pg_synthesis_export_rules(synthesis, count, jobs, &storage, &effects, 0, &inputs));
	for (size_t i = 0; i < count; ++i)
		direct_input_dependencies(inputs[i], &pg_builtin_graph_codec, graph);
	struct pg_dag objects = {0};
	assert(!pg_dag_init(&objects, NULL, NULL));
	assert(!pg_derivation_inputs_collect_objects(&objects, count, inputs, &effects, &pg_builtin_graph_codec, graph));
	for (size_t i = 0; i < count; ++i)
		if (inputs[i]->effect_parameter) assert(pg_dag_find(&objects, inputs[i]->effect_parameter));
	pg_dag_destroy(&objects);
	assert(synthesis->steps == steps && synthesis->typing->proofs.count == proofs && synthesis->jobs.count == requests);
	FILE *file = tmpfile();
	assert(file && !pg_derivation_inputs_write_inference(file, count, inputs, &effects, &pg_builtin_graph_codec, graph));
	pg_effect_inference_destroy(&effects);
	pg_graph_destroy(&storage);
	rewind(file);
	return file;
}

static void same_snapshot(FILE *before, FILE *after)
{
	rewind(before);
	int a, b;
	do { a = fgetc(before); b = fgetc(after); assert(a == b); } while (a != EOF);
	assert(!ferror(before) && !ferror(after) && !fclose(after));
}

static void retained_source_proof(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *source, uint64_t chunk)
{
	struct pg_graph storage;
	struct pg_effect_inference image;
	assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&image, &storage));
	const struct pg_derivation_input *const *inputs;
	assert(!pg_synthesis_export_rules(synthesis, 1, &source, &storage, &image, 1, &inputs));
	const struct pg_evidence *accepted = pg_synthesis_result(source);
	struct pg_synthesis resumed;
	assert(!pg_synthesis_init(&resumed, synthesis->typing,
		synthesis->normalization, synthesis->definition_policy));
	synthesis = &resumed;
	size_t proofs = synthesis->typing->proofs.count;
	struct pg_synthesis_job *restored = pg_synthesis_derivation_inference(synthesis, inputs[0], &image);
	assert(restored && !pg_synthesis_result(restored));
	assert(synthesis->typing->proofs.count == proofs);
	uint64_t limit = synthesis->steps + 1000;
	while (pg_synthesis_status(restored) == PG_SYNTHESIS_PENDING) {
		assert(synthesis->steps < limit);
		pg_synthesis_advance(synthesis, chunk);
	}
	assert(pg_synthesis_status(restored) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(restored) == accepted);
	assert(synthesis->typing->proofs.count == proofs);
	uint64_t steps = synthesis->steps;
	assert(pg_synthesis_derivation_inference(synthesis, inputs[0], &image) == restored);
	pg_synthesis_advance(synthesis, chunk);
	assert(synthesis->steps == steps);
	/* A retained conclusion does not authorize a different rule header. */
	const struct pg_derivation_input *original = inputs[0];
	struct pg_derivation_input *wrong = pg_alloc(&storage,
		sizeof(*wrong) + original->count * sizeof(*wrong->premises));
	assert(wrong);
	*wrong = *original;
	for (size_t i = 0; i < original->count; ++i) wrong->premises[i] = original->premises[i];
	wrong->rule = PG_APP_ELIM;
	struct pg_synthesis_job *rejected = pg_synthesis_derivation_inference(synthesis, wrong, &image);
	assert(rejected);
	limit = synthesis->steps + 1000;
	while (pg_synthesis_status(rejected) == PG_SYNTHESIS_PENDING) {
		assert(synthesis->steps < limit);
		pg_synthesis_advance(synthesis, chunk);
	}
	assert(pg_synthesis_status(rejected) == PG_SYNTHESIS_REJECTED);
	assert(pg_synthesis_result(source) == accepted && pg_synthesis_result(restored) == accepted);
	pg_synthesis_destroy(&resumed);
	pg_effect_inference_destroy(&image);
	pg_graph_destroy(&storage);
}

static void producer_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work normalization;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&normalization, graph));
	assert(!pg_synthesis_init(&synthesis, typing, &normalization, PG_DEFINITION_EXPLICIT_THUNK));
	if (writing) {
		const char source[] = "identity := \\x : @ => x;";
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, graph, source, strlen(source));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *identity = pg_synthesis_request(&synthesis, pg_synthesis_root(&synthesis), definition.expression);
		assert(identity);
		struct pg_graph storage;
		struct pg_effect_inference image;
		assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&image, &storage));
		const struct pg_derivation_input *const *inputs = NULL;
		size_t jobs = synthesis.jobs.count, proofs = typing->proofs.count;
		assert(pg_synthesis_export_rules(&synthesis, 1, &identity, &storage, &image, 0, &inputs) == 1);
		assert(!inputs && image.failed && synthesis.jobs.count == jobs && typing->proofs.count == proofs);
		pg_effect_inference_destroy(&image);
		pg_graph_destroy(&storage);
		while (pg_synthesis_status(identity) == PG_SYNTHESIS_PENDING) {
			assert(synthesis.steps < 1000);
			pg_synthesis_advance(&synthesis, chunk);
		}
		assert(pg_synthesis_status(identity) == PG_SYNTHESIS_DONE);
		retained_source_proof(&synthesis, identity, chunk);
		const struct pg_evidence *u = pg_prove_universe(typing, pg_prove_empty_context(typing), 0);
		struct pg_synthesis_job *universe = pg_synthesis_evidence(&synthesis, u);
		const struct pg_term *type = pg_evidence_subject(u)->core;
		const struct pg_object *op = pg_operation_label_create(graph, type, type);
		const struct pg_effect_row *empty = pg_effect_row(graph, 0, NULL), *seed = pg_effect_row(graph, 1, &op);
		struct pg_effect_inference first, second;
		assert(!pg_effect_inference_init(&first, graph) && !pg_effect_inference_init(&second, graph));
		struct pg_effect_equation *a = pg_effect_equation(&first, empty), *b = pg_effect_equation(&second, empty);
		assert(!pg_effect_dependency(&first, pg_effect_equation(&first, seed), empty, a));
		struct pg_derivation_input header = {.rule = PG_RETURN_TYPE_FORM, .count = 1};
		struct pg_synthesis_job *selected[] = {identity,
			pg_synthesis_rule(&synthesis, &header, &universe, &first, a),
			pg_synthesis_rule(&synthesis, &header, &universe, &second, b), identity};
		assert(selected[1] && selected[2]);
		assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&image, &storage));
		jobs = synthesis.jobs.count; proofs = typing->proofs.count;
		size_t terms = graph->terms.count;
		uint64_t steps = synthesis.steps;
		assert(!pg_synthesis_export_rules(&synthesis, 4, selected, &storage, &image, 0, &inputs));
		assert(inputs[0] == inputs[3] && inputs[1]->effect_parameter != inputs[2]->effect_parameter);
		assert(image.row_sources.count == 3 && !image.sealed);
		assert(jobs == synthesis.jobs.count && proofs == typing->proofs.count && steps == synthesis.steps && terms == graph->terms.count);
		assert(!pg_synthesis_result(selected[1]) && !pg_synthesis_result(selected[2]));
		assert(!pg_derivation_inputs_write_inference(file, 4, inputs, &image, &pg_builtin_graph_codec, typing->graph));
		pg_effect_inference_destroy(&image);
		pg_graph_destroy(&storage);
		/* Independent workers cannot define the same equation site twice. */
		struct pg_effect_inference conflict;
		assert(!pg_effect_inference_init(&conflict, graph));
		struct pg_effect_equation *duplicate = pg_effect_equation_at(&conflict, pg_effect_equation_parameter(&first, a), empty);
		struct pg_synthesis_job *overlap[] = {selected[1], pg_synthesis_rule(&synthesis, &header, &universe, &conflict, duplicate)};
		assert(!pg_graph_init(&storage) && !pg_effect_inference_init(&image, &storage));
		inputs = NULL;
		assert(pg_synthesis_export_rules(&synthesis, 2, overlap, &storage, &image, 0, &inputs) == -1 && !inputs && image.failed);
		pg_effect_inference_destroy(&image);
		pg_graph_destroy(&storage);
		const char declaration_source[] = "D := @{};";
		pg_parser_init(&parser, graph, declaration_source, strlen(declaration_source));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *declaration = pg_synthesis_request(&synthesis, pg_synthesis_root(&synthesis), definition.expression);
		uint64_t limit = synthesis.steps + 1000;
		while (pg_synthesis_status(declaration) == PG_SYNTHESIS_PENDING) {
			assert(synthesis.steps < limit);
			pg_synthesis_advance(&synthesis, chunk);
		}
		const struct pg_evidence *declared_type = pg_synthesis_result(declaration);
		assert(declared_type);
		FILE *unsupported = tmpfile();
		assert(unsupported && pg_derivations_write_descriptors(unsupported, 1, &declared_type,
			&pg_builtin_graph_codec, typing->graph) == -1);
		assert(!fclose(unsupported));
		pg_synthesis_destroy(&synthesis);
		pg_effect_inference_destroy(&conflict);
		pg_effect_inference_destroy(&first);
		pg_effect_inference_destroy(&second);
	} else {
		struct pg_effect_inference image;
		assert(!pg_effect_inference_init(&image, graph));
		size_t count;
		const struct pg_derivation_input *const *inputs;
		assert(!pg_derivations_read_inference(file, typing, 1000, 100, &image, &pg_builtin_graph_codec, typing->graph, &count, &inputs));
		assert(count == 4 && inputs[0] == inputs[3] && !typing->proofs.count);
		struct pg_synthesis_job *jobs[4];
		for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_derivation_inference(&synthesis, inputs[i], &image);
		/* Loaded inputs can be saved before any expansion or acceptance. */
		FILE *snapshot = producer_snapshot(&synthesis, count, jobs, typing->graph);
		pg_synthesis_advance(&synthesis, 1);
		same_snapshot(snapshot, producer_snapshot(&synthesis, count, jobs, typing->graph));
		while (synthesis.ready) { assert(synthesis.steps < 2000); pg_synthesis_advance(&synthesis, chunk); }
		assert(pg_synthesis_status(jobs[0]) == PG_SYNTHESIS_DONE && jobs[0] == jobs[3]);
		assert(!pg_synthesis_result(jobs[1]) && !pg_synthesis_result(jobs[2]));
		pg_effect_inference_seal(&image);
		assert(pg_synthesis_effect_inference(&synthesis, &image));
		while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
		for (size_t i = 0; i < count; ++i) assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_DONE);
		same_snapshot(snapshot, producer_snapshot(&synthesis, count, jobs, typing->graph));
		assert(!fclose(snapshot));
		struct pg_derivation_input invalid = {.rule = PG_APP_ELIM};
		struct pg_synthesis_job *rejected = pg_synthesis_derivation(&synthesis, &invalid);
		snapshot = producer_snapshot(&synthesis, 1, &rejected, typing->graph);
		while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
		assert(pg_synthesis_status(rejected) == PG_SYNTHESIS_REJECTED);
		same_snapshot(snapshot, producer_snapshot(&synthesis, 1, &rejected, typing->graph));
		assert(!fclose(snapshot));
		assert(pg_evidence_subject(pg_synthesis_result(jobs[0]))->core->kind == PG_LAMBDA);
		for (size_t i = 1; i < 3; ++i) {
			const struct pg_effect_row *row;
			const struct pg_term *type;
			assert(pg_effect_type_view(pg_evidence_subject(pg_synthesis_result(jobs[i]))->core, &row, &type));
			assert(pg_effect_count(row) == (i == 1 ? 1u : 0u) && type == pg_universe(typing->graph, 0));
		}
		struct pg_synthesis_job *use = source_use(&synthesis, jobs[0], "main := \\a : @ => loaded a;");
		while (synthesis.ready) { assert(synthesis.steps < 6000); pg_synthesis_advance(&synthesis, chunk); }
		assert(pg_synthesis_status(use) == PG_SYNTHESIS_DONE);
		struct pg_synthesis_job *nf = pg_synthesis_nf(&synthesis, pg_prove_empty_context(typing), pg_synthesis_result(use));
		assert(nf);
		while (synthesis.ready) { assert(synthesis.steps < 8000); pg_synthesis_advance(&synthesis, chunk); }
		assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(nf))->core,
			pg_evidence_subject(pg_synthesis_result(jobs[0]))->core) == 1);
		pg_synthesis_destroy(&synthesis);
		pg_effect_inference_destroy(&image);
		puts("producer image: source identity, pending rule export, worker union, no source mutation and ordinary Solve passed");
	}
	pg_whnf_work_destroy(&normalization);
}

static void nominal_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk)
{
	struct pg_graph *graph = typing->graph;
	struct pg_declaration_io io;
	assert(!pg_declaration_io_init(&io, typing));
	if (writing) {
		const struct pg_object *self = pg_binder(graph), *n = pg_binder(graph);
		const struct pg_evidence *empty = pg_prove_empty_context(typing);
		const struct pg_evidence *u = pg_prove_universe(typing, empty, 0);
		const struct pg_evidence *parameters = pg_prove_context_extension(typing, empty, self, u);
		const struct pg_evidence *self_type = pg_prove_variable(typing, parameters, self);
		const struct pg_evidence *fields = pg_prove_context_extension(typing, parameters, n, self_type);
		const struct pg_evidence *field_self = pg_prove_variable(typing, fields, self);
		const struct pg_evidence *results[] = {
			pg_prove_substitution(typing, parameters, parameters, 1, &self_type),
			pg_prove_substitution(typing, parameters, fields, 1, &field_self)};
		const struct pg_data_signature *signature = pg_data_signature(typing, parameters, parameters);
		const struct pg_data_schema *schema = pg_data_schema(typing, signature, 2, results);
		const struct pg_evidence *formation = pg_prove_inductive_type(typing, schema);
		const struct pg_data_layout *layout = pg_data_schema_layout(schema);
		const struct pg_evidence *identity = pg_prove_substitution(typing, empty, empty, 0, NULL);
		const struct pg_evidence *zero = pg_prove_constructor(typing, formation, pg_data_constructor(layout, 0), identity, 0, NULL);
		const struct pg_evidence *succ = pg_prove_constructor(typing, formation, pg_data_constructor(layout, 1), identity, 1, &zero);
		assert(formation && zero && succ);
		const struct pg_object *z = pg_binder(graph), *p = pg_binder(graph);
		const struct pg_evidence *z_context = pg_prove_context_extension(typing, empty, z, formation);
		const struct pg_evidence *p_context = pg_prove_context_extension(typing, empty, p, formation);
		const struct pg_evidence *motive = pg_prove_return_type(typing,
			pg_prove_projection(typing, z_context, formation));
		const struct pg_evidence *base = pg_prove_return(typing, zero);
		const struct pg_evidence *predecessor = pg_prove_abstract(typing, empty, p_context,
			pg_prove_return(typing, pg_prove_variable(typing, p_context, p)));
		const struct pg_evidence *branches[] = {base, predecessor};
		const struct pg_evidence *match = pg_prove_match(typing, formation,
			identity, succ, z_context, motive, 2, branches);
		const struct pg_evidence *scope = pg_prove_induction_scope(typing, formation,
			pg_data_constructor(layout, 1), identity, z_context, motive);
		assert(scope);
		const struct pg_evidence *ih_context = pg_evidence_premise(scope, 1);
		const struct pg_evidence *ih = pg_prove_variable(typing, ih_context, pg_evidence_context(ih_context)->binder);
		branches[1] = pg_prove_abstract(typing, empty, ih_context, pg_prove_force(typing, ih));
		const struct pg_evidence *twice = pg_prove_constructor(typing, formation,
			pg_data_constructor(layout, 1), identity, 1, &succ);
		const struct pg_evidence *induction = pg_prove_induction(typing, formation,
			identity, twice, z_context, motive, 2, branches);
		assert(match && induction);
		const struct pg_evidence *upper = pg_prove_universe(typing, p_context, 1);
		const struct pg_evidence *type_branches[] = {
			formation, pg_prove_family_abstraction(typing, p_context, upper)};
		const struct pg_evidence *type_zero = pg_prove_type_case(typing,
			formation, identity, zero, 2, type_branches);
		const struct pg_evidence *type_succ = pg_prove_type_case(typing,
			formation, identity, succ, 2, type_branches);
		assert(type_zero && type_succ);
		assert(pg_evidence_classifier(type_zero) == pg_universe(typing->graph, 2));
		assert(pg_evidence_classifier(type_succ) == pg_universe(typing->graph, 2));
		const struct pg_evidence *roots[] = {
			formation, zero, succ, formation, match, induction, type_zero, type_succ};
		assert(!pg_derivations_write_descriptors(file, 8, roots, &pg_declaration_graph_codec, &io));
	} else {
		size_t count;
		const struct pg_derivation_input *const *inputs;
		assert(!pg_derivations_read_descriptors(file, typing, 2000, 100,
			&pg_declaration_graph_codec, &io, &count, &inputs));
		assert(count == 8 && inputs[0] == inputs[3] && !typing->proofs.count);
		const struct pg_data_layout *layout = pg_data_declaration_layout(inputs[0]->parameters.declaration);
		assert(inputs[1]->parameters.constructor == pg_data_constructor(layout, 0));
		assert(inputs[2]->parameters.constructor == pg_data_constructor(layout, 1));
		const struct pg_induction_allocation *allocation = inputs[5]->parameters.induction;
		assert(allocation && allocation->count == 2 && !inputs[4]->parameters.induction);
		for (size_t i = 0; i < count; ++i)
			direct_input_dependencies(inputs[i], &pg_declaration_graph_codec, &io);
		struct pg_dag objects = {0};
		assert(!pg_dag_init(&objects, NULL, NULL));
		assert(!pg_derivation_inputs_collect_objects(&objects, count, inputs, NULL, &pg_declaration_graph_codec, &io));
		assert(!typing->proofs.count);
		assert(pg_dag_find(&objects, pg_data_declaration_family(inputs[0]->parameters.declaration)));
		assert(pg_dag_find(&objects, inputs[1]->parameters.constructor));
		assert(pg_dag_find(&objects, inputs[2]->parameters.constructor));
		assert(pg_dag_find(&objects, allocation->recursion));
		assert(pg_dag_find(&objects, allocation->argument));
		assert(pg_dag_find(&objects, allocation->self));
		const struct pg_context *parameters = pg_data_declaration_parameters(inputs[0]->parameters.declaration);
		assert(parameters && pg_dag_find(&objects, parameters->binder));
		pg_dag_destroy(&objects);
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(!pg_whnf_work_init(&work, graph));
		assert(!pg_synthesis_init(&synthesis, typing, &work, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job *jobs[8];
		for (size_t i = 0; i < count; ++i) jobs[i] = pg_synthesis_derivation(&synthesis, inputs[i]);
		assert(!typing->proofs.count && jobs[0] == jobs[3]);
		while (synthesis.ready) { assert(synthesis.steps < 2000); pg_synthesis_advance(&synthesis, chunk); }
		for (size_t i = 0; i < count; ++i) assert(pg_synthesis_status(jobs[i]) == PG_SYNTHESIS_DONE);
		const struct pg_induction_allocation *checked = pg_evidence_induction_allocation(pg_synthesis_result(jobs[5]));
		assert(checked && checked->recursion == allocation->recursion);
		assert(checked->argument == allocation->argument && checked->self == allocation->self);
		for (size_t i = 0; i < allocation->count; ++i) assert(checked->clauses[i] == allocation->clauses[i]);
		const struct pg_evidence *formation = pg_synthesis_result(jobs[0]);
		const struct pg_term *family = pg_evidence_subject(formation)->core;
		assert(family->as.reference == pg_data_declaration_family(inputs[0]->parameters.declaration));
		assert(pg_evidence_classifier(pg_synthesis_result(jobs[2])) == family);
		const struct pg_term *succ = pg_evidence_subject(pg_synthesis_result(jobs[2]))->core;
		assert(succ == pg_application(graph, pg_reference(graph, pg_data_constructor(layout, 1)),
			pg_evidence_subject(pg_synthesis_result(jobs[1]))->core));
		for (size_t i = 4; i < 6; ++i) {
			const struct pg_evidence *proof = pg_synthesis_result(jobs[i]);
			assert(pg_evidence_rule(proof) == (i == 4 ? PG_MATCH_ELIM : PG_INDUCTION_ELIM));
			struct pg_synthesis_job *nf = pg_synthesis_nf(&synthesis, pg_prove_empty_context(typing), proof);
			while (synthesis.ready) { assert(synthesis.steps < 3000); pg_synthesis_advance(&synthesis, chunk); }
			assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
			const struct pg_evidence *value = pg_prove_return_value(typing, pg_synthesis_result(nf));
			assert(value && pg_evidence_subject(value)->core == pg_evidence_subject(pg_synthesis_result(jobs[1]))->core);
			assert(pg_evidence_classifier(value) == family);
		}
		for (size_t i = 6; i < count; ++i) {
			const struct pg_evidence *type = pg_synthesis_result(jobs[i]);
			assert(pg_evidence_rule(type) == PG_TYPE_CASE);
			assert(pg_evidence_judgement(type) == PG_JUDGEMENT_VALUE_TYPE);
			assert(pg_evidence_classifier(type) == pg_universe(typing->graph, 2));
			struct pg_synthesis_job *nf = pg_synthesis_nf(&synthesis, pg_prove_empty_context(typing), type);
			while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
			assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
			const struct pg_term *expected = i == 6 ? family : pg_universe(typing->graph, 1);
			assert(pg_evidence_subject(pg_synthesis_result(nf))->core == expected);
		}
		/* Pointer validity does not establish arity or the recursive branch contract. */
		struct pg_induction_allocation invalid_allocation = *allocation;
		invalid_allocation.argument = invalid_allocation.recursion;
		struct pg_derivation_input *invalid = pg_alloc(graph, sizeof(*invalid) + inputs[5]->count * sizeof(void *));
		assert(invalid);
		*invalid = *inputs[5];
		memcpy(invalid->premises, inputs[5]->premises, invalid->count * sizeof(void *));
		invalid->parameters.induction = &invalid_allocation;
		struct pg_synthesis_job *invalid_job = pg_synthesis_derivation(&synthesis, invalid);
		while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
		assert(pg_synthesis_status(invalid_job) == PG_SYNTHESIS_REJECTED);
		assert(pg_synthesis_status(jobs[5]) == PG_SYNTHESIS_DONE);
		const size_t invalid_roots[] = {2, 4, 5};
		for (size_t i = 0; i < 3; ++i) {
			const struct pg_derivation_input *original = inputs[invalid_roots[i]];
			struct pg_derivation_input *wrong = pg_alloc(graph, sizeof(*wrong) + original->count * sizeof(void *));
			assert(wrong);
			*wrong = *original;
			memcpy(wrong->premises, original->premises, wrong->count * sizeof(void *));
			if (!i) wrong->parameters.constructor = pg_data_constructor(layout, 0);
			else wrong->rule = i == 1 ? PG_INDUCTION_ELIM : PG_MATCH_ELIM;
			struct pg_synthesis_job *bad = pg_synthesis_derivation(&synthesis, wrong);
			while (synthesis.ready) { assert(synthesis.steps < 4000); pg_synthesis_advance(&synthesis, chunk); }
			assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
			assert(pg_synthesis_status(jobs[invalid_roots[i]]) == PG_SYNTHESIS_DONE);
		}
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
		puts("nominal derivations: constructor/Match/IH/type-case Solve, universe bounds, iota results and rejection passed");
	}
	pg_declaration_io_destroy(&io);
}

static void termination_proofs(FILE *file, struct pg_typing *typing,
	int writing, uint64_t chunk)
{
	if (writing) {
		const struct pg_evidence *context = pg_prove_empty_context(typing);
		const struct pg_evidence *type = pg_prove_universe(typing, context, 0);
		const struct pg_evidence *f = pg_prove_computation_type(typing,
			PG_TOTALITY_TOTAL, pg_effect_row(typing->graph, 0, NULL), type);
		const struct pg_object *m = pg_binder(typing->graph);
		context = pg_prove_context_extension(typing, context, m, pg_prove_thunk_type(typing, f));
		const struct pg_evidence *suspended = pg_prove_variable(typing, context, m);
		const struct pg_evidence *formation = pg_prove_termination_type(typing,
			pg_prove_classifier(typing, context, suspended), suspended);
		const struct pg_evidence *witness = pg_prove_termination(typing, formation, suspended);
		const struct pg_evidence *result = pg_prove_total_pure_value(typing,
			pg_prove_force(typing, suspended));
		assert(formation && witness && result && !pg_derivations_write(file, 3,
			(const struct pg_evidence *[]){formation, witness, result}, name, typing->graph));
		return;
	}
	size_t count;
	const struct pg_derivation_input *const *roots;
	assert(!pg_derivations_read(file, typing, 1000, 100, resolve, typing->graph, &count, &roots));
	assert(count == 3 && !typing->proofs.count);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, typing->graph));
	assert(!pg_synthesis_init(&synthesis, typing, &work, PG_DEFINITION_EXPLICIT_THUNK));
	struct pg_synthesis_job *formation = pg_synthesis_derivation(&synthesis, roots[0]);
	struct pg_synthesis_job *witness = pg_synthesis_derivation(&synthesis, roots[1]);
	while (pg_synthesis_status(witness) == PG_SYNTHESIS_PENDING) {
		assert(synthesis.steps < 1000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(formation) == PG_SYNTHESIS_DONE && pg_synthesis_status(witness) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_classifier(pg_synthesis_result(witness)) == pg_evidence_subject(pg_synthesis_result(formation))->core);
	assert(pg_evidence_rule(pg_synthesis_result(witness)) == PG_TERMINATION_INTRO);
	struct pg_synthesis_job *result = pg_synthesis_derivation(&synthesis, roots[2]);
	while (pg_synthesis_status(result) == PG_SYNTHESIS_PENDING) {
		assert(synthesis.steps < 2000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(result) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(pg_synthesis_result(result)) == PG_TOTAL_PURE_VALUE);
	assert(pg_evidence_judgement(pg_synthesis_result(result)) == PG_JUDGEMENT_VALUE);
	struct pg_derivation_input *wrong = pg_alloc(typing->graph, sizeof(*wrong) + 2 * sizeof(*wrong->premises));
	assert(wrong);
	*wrong = *roots[1];
	wrong->premises[0] = roots[0];
	wrong->premises[1] = roots[0];
	struct pg_synthesis_job *rejected = pg_synthesis_derivation(&synthesis, wrong);
	while (pg_synthesis_status(rejected) == PG_SYNTHESIS_PENDING) {
		assert(synthesis.steps < 2000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(rejected) == PG_SYNTHESIS_REJECTED);
	struct pg_derivation_input *bad_result = pg_alloc(typing->graph, sizeof(*bad_result) + sizeof(*bad_result->premises));
	assert(bad_result);
	*bad_result = *roots[2];
	bad_result->premises[0] = roots[0];
	rejected = pg_synthesis_derivation(&synthesis, bad_result);
	while (pg_synthesis_status(rejected) == PG_SYNTHESIS_PENDING) {
		assert(synthesis.steps < 3000);
		pg_synthesis_advance(&synthesis, chunk);
	}
	assert(pg_synthesis_status(rejected) == PG_SYNTHESIS_REJECTED);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("termination: inert derivation input, ordinary Solve and retained target passed");
}

int main(int argc, char **argv)
{
	effect_transport(PG_TOTALITY_UNSPECIFIED);
	effect_transport(PG_TOTALITY_TOTAL);
	assert(argc == 3);
	int operation = !strncmp(argv[1], "operation-", 10);
	int unaccepted = !strncmp(argv[1], "input-", 6);
	int effects = !strncmp(argv[1], "effect-", 7);
	int producer = !strncmp(argv[1], "producer-", 9);
	int nominal = !strncmp(argv[1], "nominal-", 8);
	int termination = !strncmp(argv[1], "termination-", 12);
	const char *mode = termination ? argv[1] + 12 : operation ? argv[1] + 10 : unaccepted ? argv[1] + 6 : effects ? argv[1] + 7 : producer ? argv[1] + 9 : nominal ? argv[1] + 8 : argv[1];
	enum pg_totality totality = PG_TOTALITY_UNSPECIFIED;
	if (operation && !strncmp(mode, "total-", 6)) { totality = PG_TOTALITY_TOTAL; mode += 6; }
	int writing = !strcmp(mode, "write");
	int bulk = !strcmp(mode, "read-bulk");
	assert(writing || bulk || !strcmp(mode, "read"));
	FILE *file = fopen(argv[2], writing ? "wb" : "rb");
	struct pg_graph graph;
	struct pg_typing typing;
	assert(file && pg_graph_init(&graph) == 0 && pg_typing_init(&typing, &graph) == 0);
	if (termination) termination_proofs(file, &typing, writing, bulk ? 64 : 1);
	else if (nominal) nominal_proofs(file, &typing, writing, bulk ? 64 : 1);
	else if (producer) producer_proofs(file, &typing, writing, bulk ? 64 : 1);
	else if (effects) pending_effect_proofs(file, &typing, writing, bulk ? 64 : 1);
	else if (unaccepted) unaccepted_proofs(file, &typing, writing, bulk ? 64 : 1);
	else if (operation) operation_proofs(file, &typing, writing, bulk ? 64 : 1, totality);
	else if (writing) write_proofs(file, &typing);
	else read_proofs(file, &typing, bulk ? 64 : 1);
	assert(fclose(file) == 0);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	return 0;
}
