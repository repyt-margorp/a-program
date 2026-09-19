#include "program.h"
#include "source_io.h"
#include "derivation.h"
#include "computation.h"
#include "function_graph.h"
#include "host.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void solve(struct pg_program *program, uint64_t budget)
{
	for (size_t turns = 0; pg_synthesis_status(program->root) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 10000);
		pg_synthesis_advance(&program->synthesis, budget);
	}
}

static void pending_normalization(void)
{
	const char *text = "{{ id:=&(\\A:@ => \\x:A => x); }}.id";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	struct pg_derivation_input empty = {.rule = PG_CONTEXT_EMPTY};
	struct pg_synthesis_job *context = pg_synthesis_derivation(&p->synthesis, &empty);
	assert(context);
	struct pg_synthesis_job *nf = pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, PG_REDUCTION_NF);
	struct pg_synthesis_job *whnf = pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, PG_REDUCTION_WHNF);
	assert(nf && whnf && nf != whnf && !p->synthesis.steps);
	assert(nf == pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, PG_REDUCTION_NF));
	struct pg_synthesis_job *c, *proof;
	enum pg_reduction_kind kind;
	assert(!pg_synthesis_normalization_input(&p->synthesis, nf, &c, &proof, &kind, NULL));
	assert(c == context && proof == p->root && kind == PG_REDUCTION_NF);
	assert(pg_synthesis_normalization_input(&p->synthesis, p->root, &c, &proof, &kind, NULL));
	struct pg_synthesis_job *invalid = pg_synthesis_normalize_jobs(&p->synthesis, p->root, p->root, PG_REDUCTION_NF);
	assert(invalid && !pg_synthesis_result(nf));
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whnf) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(invalid) == PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *ctx = pg_synthesis_result(context), *input = pg_synthesis_result(p->root);
	struct pg_synthesis_job *accepted = pg_synthesis_nf(&p->synthesis, ctx, input);
	assert(accepted == pg_synthesis_normalize_jobs(&p->synthesis,
		pg_synthesis_evidence(&p->synthesis, ctx), pg_synthesis_evidence(&p->synthesis, input), PG_REDUCTION_NF));
	pg_synthesis_advance(&p->synthesis, 10000);
	assert(pg_synthesis_result(accepted) == pg_synthesis_result(nf));
	assert(!pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, (enum pg_reduction_kind)99));
	pg_program_destroy(p);
}

static void binding_contracts(void)
{
	const char *expressions[] = {"#\"m\"", "#print #\"m\""};
	for (size_t effectful = 0; effectful < 2; ++effectful) {
		enum pg_totality inferred = PG_TOTALITY_UNSPECIFIED;
		for (size_t annotated = 0; annotated < 2; ++annotated) {
			char source[128];
			int size = snprintf(source, sizeof(source), "{{ m:={x%s:=%s;x;}; }}.m",
				annotated ? ":#Text" : "", expressions[effectful]);
			assert(size > 0 && (size_t)size < sizeof(source));
			struct pg_program *p = pg_program_create(source, (size_t)size, PG_DEFINITION_IMPLICIT_THUNK);
			assert(p && p->root);
			solve(p, 1);
			assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
			const struct pg_evidence *computation = pg_prove_force(&p->typing, pg_synthesis_result(p->root));
			assert(computation);
			enum pg_totality totality;
			const struct pg_effect_row *effects;
			const struct pg_term *content;
			assert(pg_computation_type_view(pg_evidence_classifier(computation), &totality, &effects, &content));
			assert(content == pg_reference(&p->graph, pg_host_type("Text")));
			assert(pg_effect_count(effects) == effectful);
			if (effectful) assert(pg_effect_label(effects, 0) == pg_host_print(&p->graph));
			if (!annotated) inferred = totality;
			assert(inferred == totality);
			pg_program_destroy(p);
		}
	}
	puts("block annotations: inferred result, effect label and totality preserved");
}

static void remembered_normalization(void)
{
	const char *text = "{{ id:=&(\\A:@ => \\x:A => x); }}.id";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	struct pg_program *q = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && q && p->root && q->root);
	solve(p, 1);
	solve(q, 1);
	const struct pg_evidence *proof = pg_synthesis_result(p->root);
	const struct pg_evidence *other_proof = pg_synthesis_result(q->root);
	assert(proof && other_proof);
	const struct pg_evidence *forced = pg_prove_force(&p->typing, proof);
	const struct pg_evidence *other_forced = pg_prove_force(&q->typing, other_proof);
	assert(forced && other_forced);
	const struct pg_term *input = pg_evidence_subject(forced)->core;
	const struct pg_term *other_input = pg_evidence_subject(other_forced)->core;
	assert(input != other_input && pg_alpha_equal(input, other_input) == 1);
	struct pg_whnf_work producer;
	assert(!pg_whnf_work_init(&producer, &p->graph));
	struct pg_nf_job *computed = pg_nf_request(&producer, &pg_pure_policy, input);
	assert(pg_nf_advance(computed, 10000) == PG_NF_DONE);
	const struct pg_reduction_certificate *receipt = pg_nf_certificate(computed);
	pg_whnf_work_destroy(&producer);
	struct pg_synthesis_job *nf = pg_program_normalize(p, proof, 1);
	assert(nf && !pg_synthesis_result(nf));
	struct pg_nf_job *cached = pg_nf_request(&p->evaluation, &pg_pure_policy, input);
	assert(pg_nf_status(cached) == PG_NF_PENDING);
	assert(!pg_nf_remember(&p->evaluation, receipt));
	uint64_t steps = pg_nf_steps(cached);
	for (size_t i = 0; pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *result = pg_synthesis_result(nf);
	assert(result && pg_evidence_judgement(result) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_evidence_classifier(result) == pg_evidence_classifier(forced));
	assert(pg_evidence_subject(result)->core == pg_reduction_target(receipt));
	assert(pg_nf_steps(cached) == steps && pg_nf_certificate(cached) == receipt);
	/* A different source elaboration is not the same exact-pointer cache key.
	 * Sharing an erased result also never transfers typing evidence ownership. */
	assert(!pg_nf_remember(&q->evaluation, receipt));
	struct pg_nf_job *other = pg_nf_request(&q->evaluation, &pg_pure_policy, other_input);
	assert(pg_nf_status(other) == PG_NF_PENDING && !pg_nf_certificate(other));
	assert(!pg_program_normalize(q, proof, 1));
	pg_program_destroy(q);
	pg_program_destroy(p);
	puts("program normalization: accepted NF reuse preserves classifier, exact Core keys and typing ownership");
}

static void modules(void)
{
	const char *library = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *program = pg_program_create(library, strlen(library), PG_DEFINITION_EXPLICIT_THUNK);
	assert(program && program->root);
	const struct pg_source_scope *visible = pg_program_exports(program, program->scope, program->root);
	assert(visible && !program->synthesis.steps);
	size_t jobs = program->synthesis.jobs.count;
	size_t scopes = program->synthesis.scopes.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_program_exports(program, program->scope, program->root) == visible);
	assert(program->synthesis.jobs.count == jobs);
	assert(program->synthesis.scopes.count == scopes && !program->synthesis.steps);
	struct pg_parser extension_parser;
	const char *extension = "copy:=id;";
	struct pg_synthesis_job *extended = pg_program_source(program, visible,
		extension, strlen(extension), &extension_parser);
	assert(extended && !extension_parser.error && !program->synthesis.steps);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *scope = pg_synthesis_module_namespace(&program->synthesis,
		program->scope, name, program->root);
	struct pg_parser parser;
	char client[] = "{{ main:=lib.id; }}.main";
	struct pg_synthesis_job *root = pg_program_source(program, scope, client, strlen(client), &parser);
	assert(root && !parser.error && !program->synthesis.steps);
	const struct pg_source_scope *qualified = pg_program_exports(program, program->scope, root);
	assert(qualified && qualified != visible);
	assert(pg_program_exports(program, program->scope, root) == qualified);
	const struct pg_source_scope *nested = pg_program_exports(program, visible, program->root);
	assert(nested && nested != visible);
	assert(pg_program_exports(program, visible, program->root) == nested);
	struct pg_program *foreign = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(foreign);
	assert(!pg_program_exports(program, foreign->scope, program->root));
	assert(!pg_program_exports(foreign, foreign->scope, program->root));
	pg_program_destroy(foreign);
	memset(client, '?', sizeof(client));
	for (size_t i = 0; pg_synthesis_status(root) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&program->synthesis, 1);
	}
	assert(pg_synthesis_status(root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	assert(pg_program_exports(program, program->scope, program->root) == visible);
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *exported = pg_synthesis_definition(program->root, id);
	assert(exported && pg_synthesis_result(exported) == pg_synthesis_result(root));
	for (size_t i = 0; pg_synthesis_status(extended) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&program->synthesis, 1);
	}
	assert(pg_synthesis_status(extended) == PG_SYNTHESIS_DONE);
	struct pg_token copy = {.kind = PG_TOKEN_IDENT, .text = "copy", .length = 4};
	assert(pg_synthesis_result(pg_synthesis_definition(extended, copy)) == pg_synthesis_result(exported));
	assert(!pg_program_exports(program, NULL, program->root));
	assert(!pg_program_exports(program, visible, NULL));
	const struct pg_evidence *accepted = pg_synthesis_result(root);
	assert(!pg_program_source(program, scope, "bad:=", 5, &parser));
	assert(parser.error);
	assert(pg_synthesis_result(root) == accepted && !program->parser.error);
	pg_program_destroy(program);
}

static int allow_legacy_intrinsic_dot;
static uint64_t comparison_steps = 1000000;

static struct pg_program *load_program(const char *path)
{
	FILE *file = fopen(path, "rb");
	assert(file && !fseek(file, 0, SEEK_END));
	long size = ftell(file);
	assert(size >= 0 && !fseek(file, 0, SEEK_SET));
	char *source = malloc((size_t)size + 1);
	assert(source && fread(source, 1, (size_t)size, file) == (size_t)size);
	assert(!fclose(file));
	struct pg_program *program = pg_program_allocate(PG_DEFINITION_IMPLICIT_THUNK);
	assert(program);
	program->allow_legacy_intrinsic_dot = allow_legacy_intrinsic_dot;
	program->root = pg_program_source(program, program->scope, source, (size_t)size, &program->parser);
	free(source);
	assert(program && program->root);
	return program;
}

static int equal_results(struct pg_program *p, const char *label,
	const char *left, const char *right, uint64_t chunk)
{
	struct pg_token a = {.kind = PG_TOKEN_IDENT, .text = left, .length = strlen(left)};
	struct pg_token b = {.kind = PG_TOKEN_IDENT, .text = right, .length = strlen(right)};
	struct pg_synthesis_job *l = pg_program_evaluate_name(p, p->root, a, 1);
	struct pg_synthesis_job *r = pg_program_evaluate_name(p, p->root, b, 1);
	if (!l || !r) {
		fprintf(stderr, "export results: %s missing export %s or %s\n", label, left, right);
		pg_program_destroy(p);
		return 1;
	}
	while (p->synthesis.ready && p->synthesis.steps < comparison_steps)
		pg_synthesis_advance(&p->synthesis, chunk);
	const struct pg_evidence *x = pg_synthesis_result(l), *y = pg_synthesis_result(r);
	if (x && pg_evidence_judgement(x) == PG_JUDGEMENT_COMPUTATION) x = pg_prove_return_value(&p->typing, x);
	if (y && pg_evidence_judgement(y) == PG_JUDGEMENT_COMPUTATION) y = pg_prove_return_value(&p->typing, y);
	int equal = pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE && x && y;
	if (equal) {
		/* Term normalization preserves its original classifier. A computed
		 * type therefore needs conversion, not structural comparison. */
		struct pg_conversion conversion = {0};
		equal = !pg_conversion_init(&conversion, &p->evaluation,
			pg_evidence_classifier(x), pg_evidence_classifier(y));
		for (uint64_t budget = 0; equal && pg_conversion_status(&conversion) == PG_CONVERSION_PENDING && budget < comparison_steps;
			budget += chunk) pg_conversion_advance(&conversion, chunk);
		equal = equal && pg_conversion_status(&conversion) == PG_CONVERSION_EQUAL;
		pg_conversion_destroy(&conversion);
		if (equal) equal = pg_alpha_equal(pg_evidence_subject(x)->core, pg_evidence_subject(y)->core) == 1;
	}
	printf("export results: %s %s %s equal=%d chunk=%llu steps=%llu\n", label, left, right,
		equal, (unsigned long long)chunk, (unsigned long long)p->synthesis.steps);
	pg_program_destroy(p);
	return !equal;
}

static int equal_exports(const char *path, const char *left, const char *right, uint64_t chunk, int image)
{
	if (!image) return equal_results(load_program(path), path, left, right, chunk);
	FILE *file = fopen(path, "rb");
	assert(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_sources_read(file, 1000000, &count, &roots);
	assert(p && count == 1 && !p->synthesis.steps && !fclose(file));
	return equal_results(p, path, left, right, chunk);
}

static void result_comparison_checks(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;}; Other:=@{zero:*;succ:*->*;};"
		"main:={Nat.succ Nat.zero;}; same:=Nat.succ Nat.zero;"
		"different:=Nat.zero; wrongType:=Other.succ Other.zero;";
	const char *names[] = {"same", "different", "wrongType", "missing"};
	for (size_t i = 0; i < 4; ++i) {
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		assert(equal_results(p, "comparison-check", "main", names[i], 1) == (i != 0));
	}
}

static void execute_example(const char *path, const char *type_name,
	const char *base_name, const char *step_name, size_t count, uint64_t budget)
{
	struct pg_program *program = load_program(path);
	solve(program, budget);
	assert(pg_synthesis_status(program->root) == PG_SYNTHESIS_DONE);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "main", .length = 4};
	const struct pg_evidence *main = pg_synthesis_result(pg_synthesis_definition(program->root, name));
	assert(main);
	if (pg_evidence_judgement(main) == PG_JUDGEMENT_VALUE) main = pg_prove_force(&program->typing, main);
	assert(main && pg_evidence_judgement(main) == PG_JUDGEMENT_COMPUTATION);
	const struct pg_evidence *context = pg_prove_empty_context(&program->typing);
	struct pg_synthesis_job *nf = pg_synthesis_nf(&program->synthesis, context, main);
	assert(nf);
	for (size_t turns = 0; pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 100000);
		pg_synthesis_advance(&program->synthesis, budget);
	}
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *result = pg_prove_return_value(&program->typing, pg_synthesis_result(nf));
	assert(result);
	name.text = type_name; name.length = strlen(type_name);
	struct pg_synthesis_job *type_job = pg_synthesis_definition(program->root, name);
	const struct pg_evidence *type = pg_synthesis_result(type_job);
	assert(type && pg_evidence_classifier(result) == pg_evidence_subject(type)->core);
	/* Resolve labels in this declaration, never hard-code constructor ordinals
	 * or accept an erased shape belonging to a different nominal type. */
	const struct pg_source_scope *scope = pg_synthesis_name_job(&program->synthesis,
		program->scope, name, type_job);
	char expression[256];
	int length = snprintf(expression, sizeof(expression), "expected:=%s.%s;", type_name, base_name);
	assert(length > 0 && (size_t)length < sizeof(expression));
	struct pg_parser parser;
	struct pg_synthesis_job *expected_job = pg_program_source(program, scope, expression, (size_t)length, &parser);
	assert(expected_job);
	pg_synthesis_advance(&program->synthesis, 100000);
	assert(pg_synthesis_status(expected_job) == PG_SYNTHESIS_DONE);
	name.text = "expected"; name.length = 8;
	const struct pg_evidence *expected = pg_synthesis_result(pg_synthesis_definition(expected_job, name));
	assert(expected && pg_evidence_judgement(expected) == PG_JUDGEMENT_VALUE);
	const struct pg_term *core = pg_evidence_subject(expected)->core;
	if (count) {
		length = snprintf(expression, sizeof(expression), "step:=%s.%s;", type_name, step_name);
		assert(length > 0 && (size_t)length < sizeof(expression));
		struct pg_synthesis_job *step_job = pg_program_source(program, scope, expression, (size_t)length, &parser);
		assert(step_job);
		pg_synthesis_advance(&program->synthesis, 100000);
		assert(pg_synthesis_status(step_job) == PG_SYNTHESIS_DONE);
		/* Applying the checked constructor, rather than decoding Nat numerals,
		 * keeps the expected result within the same source declaration. */
		name.text = "step"; name.length = 4;
		const struct pg_evidence *step = pg_prove_force(&program->typing,
			pg_synthesis_result(pg_synthesis_definition(step_job, name)));
		assert(step);
		for (size_t i = 0; i < count; ++i) {
			struct pg_synthesis_job *value = pg_synthesis_return(&program->synthesis, context,
				pg_prove_application(&program->typing, step, expected));
			assert(value);
			pg_synthesis_advance(&program->synthesis, 100000);
			assert(pg_synthesis_status(value) == PG_SYNTHESIS_DONE);
			expected = pg_synthesis_result(value);
		}
		core = pg_evidence_subject(expected)->core;
	}
	assert(pg_evidence_subject(result)->core == core);
	printf("executed %s: %s.%s + %zu %s, chunk=%llu\n", path,
		type_name, base_name, count, step_name, (unsigned long long)budget);
	pg_program_destroy(program);
}

static const struct pg_evidence *export_value(struct pg_program *p, const char *name)
{
	struct pg_synthesis_job *job = pg_program_evaluate_name(p, p->root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = name, .length = strlen(name)}, 1);
	assert(job);
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(p->synthesis.steps < 100000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *value = pg_synthesis_result(job);
	if (value && pg_evidence_judgement(value) == PG_JUDGEMENT_COMPUTATION)
		value = pg_prove_return_value(&p->typing, value);
	assert(value);
	return value;
}

static void graded_function_graph(struct pg_program *p, const struct pg_evidence *function,
	const struct pg_evidence *argument, uint64_t chunk)
{
	function = pg_function_graph_source(&p->typing, function);
	assert(function && pg_evidence_rule(function) == PG_LAMBDA_INTRO);
	const struct pg_evidence *scope = pg_evidence_premise(pg_evidence_premise(function, 0), 0);
	const struct pg_evidence *value = pg_prove_return_value(&p->typing, pg_evidence_premise(function, 1));
	assert(value);
	/* Head exposure must not complete an entire typed beta query in one turn.
	 * Cancel at each boundary; its shared query remains resumable independently. */
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		const struct pg_evidence *inner = pg_prove_context_extension(&p->typing, scope,
			pg_binder(&p->graph), pg_prove_classifier(&p->typing, scope, value));
		const struct pg_evidence *callee = pg_prove_projection(&p->typing, inner, function);
		const struct pg_evidence *input = pg_prove_variable(&p->typing, inner, pg_evidence_context(inner)->binder);
		const struct pg_evidence *call = pg_prove_application(&p->typing, callee, input);
		const struct pg_evidence *lambda = pg_prove_abstract(&p->typing, scope, inner, call);
		const struct pg_evidence *projected_lambda = pg_prove_projection(&p->typing, inner, lambda);
		struct pg_typed_query *source = pg_construction_origin_request(&p->typing, projected_lambda);
		assert(source && !pg_typed_query_steps(source));
		struct pg_function_source_cursor invalid_owner = {.function = projected_lambda, .query = source};
		assert(pg_function_source_advance(NULL, &invalid_owner) == -1 && !pg_typed_query_steps(source));
		assert(pg_function_source_advance(&p->typing, NULL) == -1);
		size_t proofs = p->typing.proofs.count, queries = p->typing.typed_queries.count;
		struct pg_function_graph_work work;
		assert(lambda && !pg_function_graph_init(&work, &p->typing, &p->evaluation, projected_lambda));
		assert(p->typing.proofs.count == proofs && p->typing.typed_queries.count == queries);
		assert(!pg_typed_query_steps(source));
		assert(pg_function_graph_advance(&work, 0) == PG_FUNCTION_GRAPH_PENDING);
		struct pg_typed_query *body = pg_application_body_request(&p->typing, callee, input);
		uint64_t steps = pg_typed_query_steps(body);
		uint64_t source_steps = 0;
		assert(body && !steps);
		enum pg_function_graph_status status = PG_FUNCTION_GRAPH_PENDING;
		for (size_t turns = 0; turns < limit && status == PG_FUNCTION_GRAPH_PENDING; ++turns) {
			status = pg_function_graph_advance(&work, 1);
			assert(pg_typed_query_steps(source) - source_steps <= 1);
			source_steps = pg_typed_query_steps(source);
			assert(pg_typed_query_steps(body) - steps <= 1);
			steps = pg_typed_query_steps(body);
		}
		pg_function_graph_destroy(&work);
		assert(!work.state);
		while (!pg_typed_query_advance(source, chunk)) assert(pg_typed_query_steps(source) < 100000);
		assert(pg_function_graph_source(&p->typing, projected_lambda) == lambda);
		while (!pg_typed_query_advance(body, chunk)) assert(pg_typed_query_steps(body) < 100000);
		assert(pg_typed_query_result(body));
		assert(pg_evidence_subject(pg_typed_query_result(body))->core ==
			pg_evidence_subject(pg_prove_return(&p->typing, input))->core);
		if (status != PG_FUNCTION_GRAPH_PENDING) {
			assert(status == PG_FUNCTION_GRAPH_DONE && steps);
			break;
		}
	}
	/* Helper inspection must yield while its shared origin query is pending. */
	const struct pg_evidence *application = pg_prove_application(&p->typing,
		pg_prove_projection(&p->typing, scope, function), value);
	/* Fresh mapped inputs exercise cancellation at each position, even though
	 * previously completed shared queries remain available in the typing store. */
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		const struct pg_evidence *helper_scope = pg_prove_context_extension(&p->typing,
			scope, pg_binder(&p->graph), pg_prove_classifier(&p->typing, scope, value));
		const struct pg_evidence *projected = pg_prove_projection(&p->typing, helper_scope, application);
		const struct pg_evidence *sequence = pg_prove_fold(&p->typing, projected,
			pg_prove_projection(&p->typing, helper_scope, function));
		const struct pg_evidence *applied = pg_prove_lambda(&p->typing,
			pg_prove_pi(&p->typing, helper_scope, pg_prove_classifier(&p->typing, helper_scope, sequence)), sequence);
		assert(applied);
		struct pg_function_graph_work helper;
		assert(!pg_function_graph_init(&helper, &p->typing, &p->evaluation, applied));
		struct pg_typed_query *origin = pg_construction_origin_request(&p->typing, projected);
		uint64_t origin_steps = pg_typed_query_steps(origin);
		assert(!origin_steps);
		struct pg_typed_query *returned = pg_application_body_request(&p->typing,
			pg_prove_projection(&p->typing, helper_scope, function),
			pg_prove_projection(&p->typing, helper_scope, value));
		uint64_t returned_steps = pg_typed_query_steps(returned);
		assert(returned && !returned_steps);
		enum pg_function_graph_status status = PG_FUNCTION_GRAPH_PENDING;
		for (size_t turns = 0; turns < limit && status == PG_FUNCTION_GRAPH_PENDING; ++turns) {
			status = pg_function_graph_advance(&helper, 1);
			assert(pg_typed_query_steps(origin) - origin_steps <= 1);
			origin_steps = pg_typed_query_steps(origin);
			assert(pg_typed_query_steps(returned) - returned_steps <= 1);
			returned_steps = pg_typed_query_steps(returned);
		}
		pg_function_graph_destroy(&helper);
		assert(!helper.state);
		while (!pg_typed_query_advance(origin, chunk)) assert(pg_typed_query_steps(origin) < 100000);
		assert(pg_typed_query_result(origin));
		while (!pg_typed_query_advance(returned, chunk)) assert(pg_typed_query_steps(returned) < 100000);
		assert(pg_typed_query_result(returned));
		if (status != PG_FUNCTION_GRAPH_PENDING) {
			assert(status == PG_FUNCTION_GRAPH_DONE && origin_steps && returned_steps && pg_typed_query_result(origin));
			break;
		}
	}
	for (enum pg_totality grade = PG_TOTALITY_UNSPECIFIED; grade <= PG_TOTALITY_TOTAL; ++grade) {
		const struct pg_evidence *body = pg_prove_return_contract(&p->typing, grade, value);
		const struct pg_evidence *type = pg_prove_classifier(&p->typing, scope, body);
		const struct pg_evidence *lambda = pg_prove_lambda(&p->typing,
			pg_prove_pi(&p->typing, scope, type), body);
		assert(lambda && pg_evidence_subject(lambda)->core == pg_evidence_subject(function)->core);
		struct pg_function_graph_work work;
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, lambda));
		struct pg_typed_query *input = pg_typed_input_request(&p->typing, body, 0);
		uint64_t steps = pg_typed_query_steps(input);
		assert(pg_function_graph_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING);
		assert(pg_typed_query_steps(input) - steps <= 1);
		for (size_t turns = 0; pg_function_graph_witness_advance(&work, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
			assert(turns < 100000);
		assert(pg_function_graph_witness_advance(&work, 0) == PG_FUNCTION_GRAPH_DONE);
		const struct pg_evidence *call = pg_prove_application(&p->typing, pg_function_graph_witness(&work), argument);
		enum pg_totality actual;
		const struct pg_term *result;
		assert(call && pg_pure_computation_type_view(pg_evidence_classifier(call), &actual, &result));
		assert(actual == grade);
		struct pg_nf_job *nf = pg_nf_request(&p->evaluation, &pg_pure_policy, pg_evidence_subject(call)->core);
		for (size_t turns = 0; pg_nf_advance(nf, chunk) == PG_NF_PENDING; ++turns) assert(turns < 100000);
		assert(pg_nf_status(nf) == PG_NF_DONE);
		const struct pg_evidence *packet = pg_prove_return_value(&p->typing,
			pg_prove_normalization(&p->typing, call, pg_nf_certificate(nf)));
		assert(packet);
		const struct pg_term *core = pg_evidence_subject(packet)->core;
		assert(core->kind == PG_APPLICATION && core->as.application.function->kind == PG_APPLICATION);
		assert(core->as.application.function->as.application.argument == pg_evidence_subject(argument)->core);
		pg_function_graph_destroy(&work);
		/* A neutral result needs a total contract, not just an empty row. */
		const struct pg_evidence *neutral_scope = pg_prove_context_extension(&p->typing,
			scope, pg_binder(&p->graph), pg_prove_thunk_type(&p->typing, type));
		assert(neutral_scope);
		const struct pg_object *neutral_binder = pg_evidence_context(neutral_scope)->binder;
		const struct pg_evidence *domain = pg_prove_classifier(&p->typing, scope, value);
		const struct pg_evidence *inner = pg_prove_context_extension(&p->typing, neutral_scope,
			pg_binder(&p->graph), pg_prove_projection(&p->typing, neutral_scope, domain));
		const struct pg_evidence *neutral = pg_prove_force(&p->typing,
			pg_prove_variable(&p->typing, inner, neutral_binder));
		const struct pg_evidence *neutral_type = pg_prove_classifier(&p->typing, inner, neutral);
		const struct pg_evidence *neutral_lambda = pg_prove_lambda(&p->typing,
			pg_prove_pi(&p->typing, inner, neutral_type), neutral);
		assert(neutral_lambda);
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, neutral_lambda));
		for (size_t turns = 0; pg_function_graph_witness_advance(&work, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
			assert(turns < 100000);
		assert(pg_function_graph_witness_advance(&work, 0) == (grade == PG_TOTALITY_TOTAL
			? PG_FUNCTION_GRAPH_DONE : PG_FUNCTION_GRAPH_UNSUPPORTED));
		pg_function_graph_destroy(&work);
		/* TOTAL is not permission to run effectful code during graph formation. */
		const struct pg_evidence *u0 = pg_prove_universe(&p->typing,
			pg_prove_empty_context(&p->typing), 0);
		const struct pg_object *label = pg_operation_label(pg_operation_declaration(&p->typing, u0, u0));
		type = pg_prove_computation_type(&p->typing, grade,
			pg_effect_row(&p->graph, 1, &label), pg_prove_return_content(&p->typing, type));
		body = pg_prove_effect_subsumption(&p->typing, body, type);
		lambda = pg_prove_lambda(&p->typing, pg_prove_pi(&p->typing, scope, type), body);
		assert(lambda);
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, lambda));
		for (size_t turns = 0; pg_function_graph_advance(&work, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
			assert(turns < 100000 && !pg_function_graph_formation(&work));
		assert(pg_function_graph_advance(&work, 0) == PG_FUNCTION_GRAPH_UNSUPPORTED);
		pg_function_graph_destroy(&work);
	}
}

static const struct pg_evidence *graph_witness_result(struct pg_program *p,
	struct pg_function_graph_work *work, const struct pg_evidence *input,
	const struct pg_evidence *expected, uint64_t chunk)
{
	for (size_t turns = 0; pg_function_graph_witness_advance(work, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
		assert(turns < 100000);
	assert(pg_function_graph_witness_advance(work, 0) == PG_FUNCTION_GRAPH_DONE);
	assert(pg_function_graph_packet(work));
	const struct pg_evidence *call = pg_prove_application(&p->typing, pg_function_graph_witness(work), input);
	assert(call);
	struct pg_nf_job *nf = pg_nf_request(&p->evaluation, &pg_pure_policy, pg_evidence_subject(call)->core);
	for (size_t turns = 0; pg_nf_advance(nf, chunk) == PG_NF_PENDING; ++turns) assert(turns < 100000);
	assert(pg_nf_status(nf) == PG_NF_DONE);
	const struct pg_evidence *packet = pg_prove_return_value(&p->typing,
		pg_prove_normalization(&p->typing, call, pg_nf_certificate(nf)));
	assert(packet);
	const struct pg_term *core = pg_evidence_subject(packet)->core;
	assert(core->kind == PG_APPLICATION && core->as.application.function->kind == PG_APPLICATION);
	assert(pg_alpha_equal(core->as.application.function->as.application.argument, pg_evidence_subject(expected)->core) == 1);
	return packet;
}

static void function_graph_aliases(struct pg_program *p,
	const struct pg_evidence *function, uint64_t chunk)
{
	struct pg_typing *typing = &p->typing;
	const struct pg_evidence *raw = pg_function_graph_source(typing, function);
	assert(raw && pg_evidence_rule(raw) == PG_LAMBDA_INTRO);
	const struct pg_evidence *pi = pg_evidence_premise(raw, 0);
	struct pg_conversion conversion;
	assert(!pg_conversion_init(&conversion, &p->evaluation, pg_evidence_classifier(raw), pg_evidence_classifier(raw)));
	assert(pg_conversion_advance(&conversion, 64) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *alternate = pg_prove_conversion(typing, raw, pi, pg_conversion_certificate(&conversion));
	assert(alternate && alternate != raw && pg_evidence_subject(alternate) == pg_evidence_subject(raw));
	assert(pg_evidence_subject(pg_function_graph_source(typing, alternate)) == pg_evidence_subject(raw));
	size_t proofs = typing->proofs.count, subjects = typing->occurrences.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_evidence_subject(pg_function_graph_source(typing, alternate)) == pg_evidence_subject(raw));
	assert(typing->proofs.count == proofs && typing->occurrences.count == subjects);
	assert(!pg_function_graph_source(NULL, raw));
	const struct pg_evidence *outer = pg_evidence_premise(pg_evidence_premise(pi, 0), 0);
	const struct pg_evidence *scope = pg_prove_context_extension(typing, outer, pg_binder(&p->graph), pg_prove_pi_domain(typing, pi));
	const struct pg_evidence *projected = pg_prove_projection(typing, scope, raw);
	assert(projected && pg_function_graph_source(typing, projected) == raw);
	proofs = typing->proofs.count; subjects = typing->occurrences.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_function_graph_source(typing, projected) == raw);
	assert(typing->proofs.count == proofs && typing->occurrences.count == subjects);
	/* Specialization is not projection: replace a captured variable and
	 * deliberately collide with the Lambda binder in the destination. */
	const struct pg_object *binder = pg_binder(&p->graph);
	const struct pg_evidence *domain = pg_prove_pi_domain(typing, pi);
	const struct pg_evidence *inner = pg_prove_context_extension(typing, scope, binder,
		pg_prove_projection(typing, scope, domain));
	const struct pg_evidence *captured = pg_prove_variable(typing, scope, pg_evidence_context(scope)->binder);
	const struct pg_evidence *constant = pg_prove_abstract(typing, scope, inner,
		pg_prove_return(typing, pg_prove_projection(typing, inner, captured)));
	const struct pg_evidence *destination = pg_prove_context_extension(typing, outer, binder, domain);
	const struct pg_evidence *image = pg_prove_variable(typing, destination, binder);
	const struct pg_evidence *map = pg_prove_substitution_pair(typing,
		pg_prove_substitution_projection(typing, outer, destination), scope, image);
	const struct pg_evidence *mapped = pg_prove_reindex(typing, map, constant);
	assert(mapped && pg_evidence_subject(mapped)->core->kind == PG_LAMBDA);
	assert(pg_evidence_subject(mapped)->core->as.lambda.binder != binder);
	struct pg_typed_query *input = pg_typed_input_request(typing, mapped, 0);
	while (!pg_typed_query_advance(input, chunk)) assert(pg_typed_query_steps(input) < 100000);
	const struct pg_evidence *specialized = pg_function_graph_source(typing, mapped);
	assert(specialized && pg_evidence_subject(specialized)->core == pg_evidence_subject(mapped)->core);
	assert(pg_evidence_subject(pg_evidence_premise(specialized, 1)) ==
		pg_evidence_subject(pg_typed_query_result(input)));
	struct pg_context_lift *lift = pg_context_lift_request(typing, pg_evidence_context_map(map),
		pg_evidence_context(inner), pg_evidence_subject(mapped)->core->as.lambda.binder);
	assert(pg_context_lift_result(lift));
	proofs = typing->proofs.count; subjects = typing->occurrences.count;
	uint64_t input_steps = pg_typed_query_steps(input), lift_steps = pg_context_lift_steps(lift);
	size_t maps = typing->context_maps.count, lifts = typing->context_lifts.count;
	for (size_t i = 0; i < 100; ++i)
		assert(pg_function_graph_source(typing, mapped) == specialized);
	assert(typing->proofs.count == proofs && typing->occurrences.count == subjects);
	assert(typing->context_maps.count == maps && typing->context_lifts.count == lifts);
	assert(pg_typed_query_steps(input) == input_steps && pg_context_lift_steps(lift) == lift_steps);
	const struct pg_evidence *applied = pg_prove_application_body(typing, specialized, image);
	assert(applied && pg_evidence_subject(applied)->core == pg_evidence_subject(pg_prove_return(typing, image))->core);
	/* Surface graph references share the same bounded source inspection. */
	const struct pg_evidence *wrapped = raw;
	struct pg_typed_query *origins[6];
	for (size_t i = 0; i < 6; ++i) {
		wrapped = pg_prove_force(typing, pg_prove_thunk(typing, wrapped));
		origins[i] = pg_construction_origin_request(typing, wrapped);
		assert(origins[i] && !pg_typed_query_steps(origins[i]));
	}
	const struct pg_source_scope *delayed = pg_synthesis_name(&p->synthesis, p->scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "delayed", .length = 7}, wrapped);
	const char *inspection = "relation:=@delayed; witness:=*delayed;";
	struct pg_parser inspection_parser;
	struct pg_synthesis_job *inspected = pg_program_source(p, delayed, inspection,
		strlen(inspection), &inspection_parser);
	assert(inspected);
	uint64_t previous = 0;
	for (size_t turn = 0; pg_synthesis_status(inspected) == PG_SYNTHESIS_PENDING; ++turn) {
		assert(turn < 100000);
		pg_synthesis_advance(&p->synthesis, 1);
		uint64_t steps = 0;
		for (size_t i = 0; i < 6; ++i) steps += pg_typed_query_steps(origins[i]);
		assert(steps - previous <= 1);
		previous = steps;
	}
	assert(previous == 6 && pg_synthesis_status(inspected) == PG_SYNTHESIS_DONE);
	const struct pg_source_scope *names = pg_synthesis_name(&p->synthesis, p->scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "original", .length = 8}, raw);
	names = pg_synthesis_name(&p->synthesis, names,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "alternate", .length = 9}, alternate);
	const char *source = "left:=@original;right:=@alternate;leftBase:=(@original).nil;rightBase:=(@alternate).nil;";
	struct pg_parser parser;
	struct pg_synthesis_job *module = pg_program_source(p, names, source, strlen(source), &parser);
	assert(module);
	for (size_t turns = 0; pg_synthesis_status(module) == PG_SYNTHESIS_PENDING; ++turns) {
		assert(turns < 100000);
		pg_synthesis_advance(&p->synthesis, chunk);
	}
	assert(pg_synthesis_status(module) == PG_SYNTHESIS_DONE);
	const char *exports[] = {"left", "right", "leftBase", "rightBase"};
	const struct pg_occurrence *results[4];
	for (size_t i = 0; i < 4; ++i) {
		const struct pg_evidence *proof = pg_synthesis_result(pg_synthesis_definition(module,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = exports[i], .length = strlen(exports[i])}));
		assert(proof);
		results[i] = pg_evidence_subject(proof);
	}
	assert(results[0] == results[1] && results[2] == results[3]);
	pg_conversion_destroy(&conversion);
}

static void graph_index_preparation(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;};"
		"Vec:=\\A:@=>@\\n:Nat=>{nil:* Nat.zero;cons:(k:Nat)->A->* k->* (Nat.succ k);};"
		"length:=\\A:@=>\\n:Nat=>\\xs:Vec A n=>xs @nil=>Nat.zero @cons k h t=>Nat.succ *t;";
	/* Each cutoff has cold graph preparation; completing a previous owner's
	 * shared query must not hide eager work in the next initialization. */
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, 1);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "length", .length = 6}));
		function = pg_function_graph_source(&p->typing, function);
		const struct pg_evidence *inner = function;
		while (pg_evidence_rule(pg_evidence_premise(inner, 1)) == PG_LAMBDA_INTRO)
			inner = pg_evidence_premise(inner, 1);
		const struct pg_evidence *scope = pg_evidence_premise(pg_evidence_premise(inner, 0), 0);
		const struct pg_evidence *prefix = pg_evidence_premise(pg_evidence_premise(scope, 0), 0);
		struct pg_inductive_instance instance;
		assert(pg_inductive_instance(&p->typing, pg_evidence_premise(scope, 1), &instance));
		struct pg_typed_query *query = pg_substitution_rebase_request(&p->typing, prefix, instance.parameters);
		assert(query && !pg_typed_query_steps(query));
		struct pg_function_graph_work work;
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
		assert(!pg_typed_query_steps(query) && !pg_function_graph_prepared(&work));
		assert(pg_function_graph_advance(&work, 0) == PG_FUNCTION_GRAPH_PENDING);
		uint64_t steps = 0;
		for (size_t turn = 0; turn < limit && !pg_function_graph_prepared(&work); ++turn) {
			assert(pg_function_graph_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING);
			assert(pg_typed_query_steps(query) - steps <= 1);
			steps = pg_typed_query_steps(query);
		}
		int prepared = pg_function_graph_prepared(&work);
		if (prepared) assert(steps && pg_typed_query_advance(query, 0) == 1);
		assert(!pg_function_graph_formation(&work));
		pg_function_graph_destroy(&work);
		assert(!work.state);
		while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
		const struct pg_evidence *map = pg_typed_query_result(query);
		assert(map && pg_evidence_premise(map, 1) == prefix);
		assert(pg_substitution_rebase_request(&p->typing, prefix, instance.parameters) == query);
		pg_program_destroy(p);
		if (prepared) break;
	}
	puts("graph preparation: indexed parameters suspend, retain their prefix and survive owner cancellation");
}

static void function_graphs(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;}; NatList:=@{nil:*;cons:Nat->*->*;};"
		"length:=\\xs:NatList=>xs @nil=>Nat.zero @cons head tail=>Nat.succ *tail;"
		"tailLength:=\\xs:NatList=>xs @nil=>Nat.zero @cons head tail=>length tail;"
		"identity:=\\n:Nat=>n; zero:=Nat.zero; nil:=NatList.nil;"
		"one:=NatList.cons Nat.zero NatList.nil; successor:=Nat.succ Nat.zero;"
		"Tree:=@{leaf:*;fork:*->*->*;};"
		"mirror:=\\t:Tree=>t @leaf=>Tree.leaf @fork l r=>Tree.fork *r *l;"
		"duplicate:=\\t:Tree=>t @leaf=>Tree.leaf @fork l r=>Tree.fork *l *l;"
		"leaf:=Tree.leaf; pair:=Tree.fork Tree.leaf Tree.leaf;"
		"leftTree:=Tree.fork Tree.leaf pair; rightTree:=Tree.fork pair Tree.leaf;"
		"duplicateExpected:=Tree.fork pair pair;";
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, chunk);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *zero = export_value(p, "zero"), *nil = export_value(p, "nil");
		const struct pg_evidence *one = export_value(p, "one"), *successor = export_value(p, "successor");
		const struct pg_evidence *leaf = export_value(p, "leaf"), *pair = export_value(p, "pair");
		const char *names[] = {"identity", "length", "mirror"};
		for (size_t i = 0; i < 3; ++i) {
			const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = strlen(names[i])}));
			if (!i) graded_function_graph(p, function, successor, chunk);
			if (i == 1) function_graph_aliases(p, function, chunk);
			struct pg_function_graph_work work;
			assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
			assert(pg_function_graph_advance(&work, 0) == PG_FUNCTION_GRAPH_PENDING);
			for (size_t turns = 0; pg_function_graph_advance(&work, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
				assert(turns < 100000);
			assert(pg_function_graph_advance(&work, chunk) == PG_FUNCTION_GRAPH_DONE);
			const struct pg_evidence *formation = pg_function_graph_formation(&work);
			assert(formation && pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY);
			struct pg_inductive_instance graph;
			assert(pg_inductive_instance(&p->typing, formation, &graph));
			assert(pg_data_constructor_count(graph.schema) == (i ? 2u : 1u));
			const struct pg_data_layout *layout = pg_data_schema_layout(graph.schema);
			const struct pg_evidence *empty = pg_prove_empty_context(&p->typing);
			const struct pg_evidence *parameters = pg_prove_substitution_projection(&p->typing, empty, empty);
			const struct pg_evidence *base = pg_prove_constructor(&p->typing, formation,
				pg_data_constructor(layout, 0), parameters, i ? 0 : 1, i ? NULL : &zero);
			assert(base);
			const struct pg_evidence *type = pg_prove_family_application(&p->typing, formation, i == 2 ? leaf : i ? nil : zero);
			type = pg_prove_family_application(&p->typing, type, i == 2 ? leaf : zero);
			assert(type && pg_alpha_equal(pg_evidence_subject(type)->core, pg_evidence_classifier(base)) == 1);
			if (i == 1) {
				const struct pg_evidence *fields[] = {zero, nil, zero, base};
				const struct pg_evidence *step = pg_prove_constructor(&p->typing, formation,
					pg_data_constructor(layout, 1), parameters, 4, fields);
				assert(step);
				type = pg_prove_family_application(&p->typing, formation, one);
				type = pg_prove_family_application(&p->typing, type, successor);
				assert(type && pg_alpha_equal(pg_evidence_subject(type)->core, pg_evidence_classifier(step)) == 1);
				fields[2] = successor;
				assert(!pg_prove_constructor(&p->typing, formation, pg_data_constructor(layout, 1), parameters, 4, fields));
			} else if (i == 2) {
				const struct pg_object *fork = pg_data_constructor(layout, 1);
				const struct pg_evidence *fields[] = {leaf, leaf, leaf, base, leaf, base};
				const struct pg_evidence *paired = pg_prove_constructor(&p->typing, formation, fork, parameters, 6, fields);
				assert(paired);
				fields[1] = pair; fields[4] = pair; fields[5] = paired;
				const struct pg_evidence *step = pg_prove_constructor(&p->typing, formation, fork, parameters, 6, fields);
				assert(step);
				type = pg_prove_family_application(&p->typing, formation, export_value(p, "leftTree"));
				type = pg_prove_family_application(&p->typing, type, export_value(p, "rightTree"));
				assert(type && pg_alpha_equal(pg_evidence_subject(type)->core, pg_evidence_classifier(step)) == 1);
				fields[5] = base;
				assert(!pg_prove_constructor(&p->typing, formation, fork, parameters, 6, fields));
				for (size_t mode = 0; mode < 3; ++mode) {
					struct pg_function_graph_work ordered;
					assert(!pg_function_graph_init(&ordered, &p->typing, &p->evaluation, function));
					assert(!pg_function_graph_prepared(&ordered));
					assert(!pg_function_graph_case_input(&ordered));
					assert(pg_function_graph_advance(&ordered, 0) == PG_FUNCTION_GRAPH_PENDING);
					assert(!pg_function_graph_prepared(&ordered));
					for (size_t turns = 0; !pg_function_graph_prepared(&ordered); ++turns) {
						assert(turns < 1000);
						assert(pg_function_graph_advance(&ordered, 1) == PG_FUNCTION_GRAPH_PENDING);
					}
					size_t slots[] = {1, mode == 2 ? 1 : 0};
					struct pg_function_graph_order order[] = {{0, NULL}, {mode ? 2 : 1, slots}};
					assert(pg_function_graph_source_order(&ordered, 1, order));
					assert(!pg_function_graph_source_order(&ordered, 2, order));
					assert(pg_function_graph_source_order(&ordered, 2, order));
					for (size_t turns = 0; pg_function_graph_advance(&ordered, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
						assert(turns < 100000);
					assert(pg_function_graph_source_order(&ordered, 2, order));
					if (mode == 1) {
						assert(pg_function_graph_advance(&ordered, 0) == PG_FUNCTION_GRAPH_DONE);
						const struct pg_evidence *ordered_type = pg_function_graph_formation(&ordered);
						struct pg_inductive_instance instance;
						assert(pg_inductive_instance(&p->typing, ordered_type, &instance));
						const struct pg_data_layout *ordered_layout = pg_data_schema_layout(instance.schema);
						const struct pg_object *ordered_fork = pg_data_constructor(ordered_layout, 1);
						const struct pg_evidence *ordered_base = pg_prove_constructor(&p->typing, ordered_type,
							pg_data_constructor(ordered_layout, 0), parameters, 0, NULL);
						const struct pg_evidence *ordered_fields[] = {leaf, leaf, leaf, ordered_base, leaf, ordered_base};
						const struct pg_evidence *ordered_pair = pg_prove_constructor(&p->typing, ordered_type,
							ordered_fork, parameters, 6, ordered_fields);
						assert(ordered_pair);
						ordered_fields[1] = pair; ordered_fields[2] = pair; ordered_fields[3] = ordered_pair;
						const struct pg_evidence *ordered_step = pg_prove_constructor(&p->typing, ordered_type,
							ordered_fork, parameters, 6, ordered_fields);
						assert(ordered_step);
						ordered_fields[3] = ordered_base;
						assert(!pg_prove_constructor(&p->typing, ordered_type, ordered_fork, parameters, 6, ordered_fields));
						const struct pg_evidence *packet = graph_witness_result(p, &ordered,
							export_value(p, "leftTree"), export_value(p, "rightTree"), chunk);
						assert(pg_alpha_equal(pg_evidence_subject(packet)->core->as.application.argument,
							pg_evidence_subject(ordered_step)->core) == 1);
					} else {
						assert(pg_function_graph_advance(&ordered, 0) == PG_FUNCTION_GRAPH_UNSUPPORTED);
						assert(!pg_function_graph_formation(&ordered));
					}
					pg_function_graph_destroy(&ordered);
				}
			}
			assert(pg_function_graph_witness_advance(&work, 0) == PG_FUNCTION_GRAPH_PENDING);
			const struct pg_evidence *input = i == 2 ? export_value(p, "leftTree") : i ? one : successor;
			const struct pg_evidence *expected = i == 2 ? export_value(p, "rightTree") : successor;
			graph_witness_result(p, &work, input, expected, chunk);
			pg_function_graph_destroy(&work);
			assert(pg_evidence_owned_by(formation, &p->typing));
			struct pg_inductive_instance retained;
			assert(pg_inductive_instance(&p->typing, formation, &retained) && retained.schema == graph.schema);
			assert(pg_evidence_owned_by(pg_data_schema_result(retained.schema, pg_data_constructor(layout, 0)), &p->typing));
		}
		/* Two calls of one field must retain two ordered outputs, not queue links. */
		const struct pg_evidence *duplicate = pg_synthesis_result(pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "duplicate", .length = 9}));
		for (size_t invalid = 0; invalid < 2; ++invalid) {
			struct pg_function_graph_work repeated;
			assert(!pg_function_graph_init(&repeated, &p->typing, &p->evaluation, duplicate));
			for (size_t turns = 0; !pg_function_graph_prepared(&repeated); ++turns) {
				assert(turns < 1000);
				assert(pg_function_graph_advance(&repeated, 1) == PG_FUNCTION_GRAPH_PENDING);
			}
			size_t slots[] = {0, invalid ? 1 : 0};
			struct pg_function_graph_order order[] = {{0, NULL}, {2, slots}};
			assert(!pg_function_graph_source_order(&repeated, 2, order));
			for (size_t turns = 0; pg_function_graph_advance(&repeated, chunk) == PG_FUNCTION_GRAPH_PENDING; ++turns)
				assert(turns < 100000);
			if (invalid) {
				assert(pg_function_graph_advance(&repeated, 0) == PG_FUNCTION_GRAPH_UNSUPPORTED);
				assert(!pg_function_graph_formation(&repeated));
			} else graph_witness_result(p, &repeated, export_value(p, "rightTree"),
				export_value(p, "duplicateExpected"), chunk);
			pg_function_graph_destroy(&repeated);
		}
		const char *dependencies[] = {"tailLength", "length", "identity"};
		struct pg_function_graph_work works[3];
		const struct pg_evidence *functions[3];
		for (size_t i = 0; i < 3; ++i) {
			functions[i] = pg_synthesis_result(pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = dependencies[i], .length = strlen(dependencies[i])}));
			assert(!pg_function_graph_init(&works[i], &p->typing, &p->evaluation, functions[i]));
		}
		for (size_t turns = 0; !pg_function_graph_dependency(&works[0]); ++turns) {
			assert(turns < 100000);
			assert(pg_function_graph_advance(&works[0], chunk) == PG_FUNCTION_GRAPH_PENDING);
		}
		assert(pg_evidence_subject(pg_function_graph_dependency(&works[0])) ==
			pg_evidence_subject(pg_function_graph_source(&p->typing, functions[1])));
		assert(pg_function_graph_advance(&works[0], 0) == PG_FUNCTION_GRAPH_PENDING);
		assert(pg_function_graph_supply(&works[0], &works[0]));
		assert(pg_function_graph_supply(&works[0], &works[1]));
		graph_witness_result(p, &works[2], successor, successor, chunk);
		assert(pg_function_graph_supply(&works[0], &works[2]));
		graph_witness_result(p, &works[1], one, successor, chunk);
		assert(!pg_function_graph_supply(&works[0], &works[1]));
		assert(pg_function_graph_supply(&works[0], &works[1]));
		graph_witness_result(p, &works[0], one, zero, chunk);
		for (size_t i = 0; i < 3; ++i) pg_function_graph_destroy(&works[i]);
		pg_program_destroy(p);
		/* A non-recursive helper can end in an opaque host callee. A missing
		 * construction view is not an induction dependency or a typing failure. */
		const char *host_helper = "Unit:=@{unit:*;}; helper:=\\x:Unit=>#int_add;"
			"f:=\\x:Unit=>x @unit=>helper x #1 #2; graph:=@f; proof:=*f; main:=f Unit.unit;";
		p = pg_program_create(host_helper, strlen(host_helper), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, chunk);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_term *answer = pg_evidence_subject(export_value(p, "main"))->core;
		int64_t integer;
		assert(answer->kind == PG_REFERENCE && pg_host_integer_view(answer->as.reference, &integer) && integer == 3);
		pg_program_destroy(p);
	}
	puts("function graphs: ordinary indexed schemas and result witnesses preserve identity/length/mirror results");
}

static void suspended_helper_application(void)
{
	const char *source = "Bool:=@{true:*;false:*;}; Nat:=@{zero:*;succ:*->*;};"
		"helper:=\\b:Bool=>\\n:Nat=>b @true=>n @false=>Nat.zero;"
		"f:=\\b:Bool=>\\n:Nat=>{r:=helper b n;r;};";
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, 1);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *helper = pg_function_graph_source(&p->typing,
			pg_synthesis_result(pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "helper", .length = 6})));
		const struct pg_evidence *function = pg_function_graph_source(&p->typing,
			pg_synthesis_result(pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "f", .length = 1})));
		assert(helper && function);
		const struct pg_evidence *inner = pg_evidence_premise(function, 1);
		const struct pg_evidence *context = pg_evidence_premise(pg_evidence_premise(inner, 0), 0);
		const struct pg_evidence *sequence = pg_evidence_premise(inner, 1);
		assert(pg_evidence_rule(sequence) == PG_FOLD_ELIM);
		const struct pg_evidence *call = pg_evidence_premise(sequence, 0);
		assert(pg_evidence_rule(call) == PG_APP_ELIM);
		const struct pg_evidence *n = pg_evidence_premise(call, 1);
		call = pg_evidence_premise(call, 0);
		assert(pg_evidence_rule(call) == PG_APP_ELIM);
		const struct pg_evidence *b = pg_evidence_premise(call, 1);
		struct pg_typed_query *queries[2] = {
			pg_application_body_request(&p->typing, pg_prove_projection(&p->typing, context, helper), b), NULL
		};
		assert(queries[0] && !pg_typed_query_steps(queries[0]));
		struct pg_function_graph_work work;
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
		for (size_t turn = 0; turn < limit && !pg_function_graph_dependency(&work); ++turn) {
			uint64_t steps[] = {pg_typed_query_steps(queries[0]), pg_typed_query_steps(queries[1])};
			assert(pg_function_graph_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING);
			if (!queries[1] && pg_typed_query_result(queries[0]))
				queries[1] = pg_application_body_request(&p->typing, pg_typed_query_result(queries[0]), n);
			for (size_t i = 0; i < 2; ++i) assert(pg_typed_query_steps(queries[i]) - steps[i] <= 1);
		}
		const struct pg_evidence *dependency = pg_function_graph_dependency(&work);
		int complete = dependency != NULL;
		if (dependency) {
			assert(pg_evidence_subject(dependency) == pg_evidence_subject(helper));
			assert(queries[1] && pg_typed_query_steps(queries[0]) && pg_typed_query_steps(queries[1]));
		}
		pg_function_graph_destroy(&work);
		/* Cancelled owners release only their cursor, never shared query work. */
		for (size_t i = 0; i < 2; ++i) {
			if (i) queries[i] = pg_application_body_request(&p->typing, pg_typed_query_result(queries[0]), n);
			while (!pg_typed_query_advance(queries[i], 1)) assert(pg_typed_query_steps(queries[i]) < 100000);
			assert(pg_typed_query_result(queries[i]));
		}
		pg_program_destroy(p);
		if (complete) break;
	}
	puts("helper application: typed beta queries yield and survive cancellation at every boundary");
}

static void suspended_helper_schema(void)
{
	const char *source = "Bool:=@{true:*;false:*;}; Nat:=@{zero:*;succ:*->*;};"
		"helper:=\\A:@=>\\b:Bool=>\\n:A=>b @true=>n @false=>n;"
		"f:=\\b:Bool=>\\n:Nat=>{r:=helper Nat b n;s:=helper Nat b r;s;};";
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, 1);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "f", .length = 1}));
		struct pg_function_graph_work work, helper = {0};
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
		enum pg_function_graph_status status = PG_FUNCTION_GRAPH_PENDING;
		for (size_t turn = 0; turn < limit && status == PG_FUNCTION_GRAPH_PENDING; ++turn) {
			status = pg_function_graph_advance(&work, 1);
			const struct pg_evidence *dependency = pg_function_graph_dependency(&work);
			if (dependency) {
				if (!helper.state) {
					assert(!pg_function_graph_init(&helper, &p->typing, &p->evaluation, dependency));
					for (size_t steps = 0; pg_function_graph_witness_advance(&helper, 64) == PG_FUNCTION_GRAPH_PENDING; ++steps)
						assert(steps < 1000 && !pg_function_graph_dependency(&helper));
				}
				assert(!pg_function_graph_supply(&work, &helper));
			}
		}
		size_t proofs = p->typing.proofs.count, queries = p->typing.typed_queries.count;
		assert(pg_function_graph_advance(&work, 0) == status);
		assert(p->typing.proofs.count == proofs && p->typing.typed_queries.count == queries);
		if (status != PG_FUNCTION_GRAPH_PENDING) {
			assert(status == PG_FUNCTION_GRAPH_DONE && helper.state && pg_function_graph_formation(&work));
			assert(!pg_function_graph_dependency(&work) && pg_function_graph_supply(&work, &helper));
			for (size_t steps = 0; pg_function_graph_witness_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING; ++steps)
				assert(steps < 10000);
			assert(pg_function_graph_witness(&work));
		}
		pg_function_graph_destroy(&work);
		pg_function_graph_destroy(&helper);
		pg_program_destroy(p);
		if (status != PG_FUNCTION_GRAPH_PENDING) break;
	}
	puts("helper schema: dependent calls resume, tolerate cancellation, and reject late dependency supply");
}

static void suspended_match_body(void)
{
	const char *source = "Bool:=@{true:*;false:*;};"
		"f:=\\x:Bool=>{r:=Bool.true @true=>x @false=>Bool.false;r;};";
	for (size_t limit = 0;; ++limit) {
		assert(limit < 1000);
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, 1);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_function_graph_source(&p->typing,
			pg_synthesis_result(pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "f", .length = 1})));
		assert(function && pg_evidence_rule(function) == PG_LAMBDA_INTRO);
		const struct pg_evidence *sequence = pg_evidence_premise(function, 1);
		assert(pg_evidence_rule(sequence) == PG_FOLD_ELIM);
		const struct pg_evidence *match = pg_evidence_premise(sequence, 0);
		assert(pg_evidence_rule(match) == PG_MATCH_ELIM);
		struct pg_typed_query *query = pg_elimination_body_request(&p->typing, match);
		assert(query && !pg_typed_query_steps(query));
		struct pg_function_graph_work work;
		assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
		enum pg_function_graph_status status = PG_FUNCTION_GRAPH_PENDING;
		for (size_t turn = 0; turn < limit && status == PG_FUNCTION_GRAPH_PENDING; ++turn) {
			uint64_t steps = pg_typed_query_steps(query);
			status = pg_function_graph_advance(&work, 1);
			assert(pg_typed_query_steps(query) - steps <= 1);
		}
		pg_function_graph_destroy(&work);
		while (!pg_typed_query_advance(query, 1)) assert(pg_typed_query_steps(query) < 10000);
		assert(pg_typed_query_result(query));
		pg_program_destroy(p);
		if (status != PG_FUNCTION_GRAPH_PENDING) { assert(status == PG_FUNCTION_GRAPH_DONE); break; }
	}
	puts("Match planning: typed iota yields and remains resumable after graph cancellation");
}

static void suspended_case_scopes(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;}; Fields:=@{mk:Nat->Nat->Nat->Nat->*;};"
		"first:=\\f:Fields=>f @mk a b c d=>a; zero:=Nat.zero;"
		"sample:=Fields.mk Nat.zero Nat.zero Nat.zero Nat.zero;";
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, chunk);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "first", .length = 5}));
		const struct pg_evidence *sample = export_value(p, "sample"), *zero = export_value(p, "zero");
		for (size_t limit = 0;; ++limit) {
			assert(limit < 1000);
			struct pg_function_graph_work work;
			assert(!pg_function_graph_init(&work, &p->typing, &p->evaluation, function));
			for (size_t turn = 0; !pg_function_graph_prepared(&work); ++turn) {
				assert(turn < 1000);
				assert(pg_function_graph_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING);
			}
			size_t proofs = p->typing.proofs.count, queries = p->typing.typed_queries.count;
			assert(pg_function_graph_advance(&work, 1) == PG_FUNCTION_GRAPH_PENDING);
			assert(p->typing.proofs.count == proofs && p->typing.typed_queries.count == queries);
			enum pg_function_graph_status status = PG_FUNCTION_GRAPH_PENDING;
			for (size_t turn = 0; turn < limit && status == PG_FUNCTION_GRAPH_PENDING; ++turn)
				status = pg_function_graph_advance(&work, 1);
			if (status != PG_FUNCTION_GRAPH_PENDING) {
				assert(status == PG_FUNCTION_GRAPH_DONE);
				graph_witness_result(p, &work, sample, zero, chunk);
			}
			pg_function_graph_destroy(&work);
			assert(!work.state);
			if (status != PG_FUNCTION_GRAPH_PENDING) break;
		}
		pg_program_destroy(p);
	}
	puts("case telescopes: descriptive setup, incremental scopes and cancellation preserve graph witnesses");
}

static void ambiguous_source_calls(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;}; keep:=\\a:Nat=>\\b:Nat=>a;"
		"twice:=\\n:Nat=>n @zero=>Nat.zero @succ k=>keep *k *k; main:=@twice;";
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		solve(p, chunk);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_UNSUPPORTED);
		struct pg_synthesis_job *function = pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "twice", .length = 5});
		assert(function && pg_synthesis_status(function) == PG_SYNTHESIS_DONE);
		pg_program_destroy(p);
	}
	puts("function graph source layout: ambiguous repeated argument sites stay unsupported, not interchangeable");
}

static void application_result_constraints(void)
{
	const char *prefix = "Bool:=@{true:*;false:*;}; Nat:=@{zero:*;succ:*->*;};"
		"Only:=@\\b:Bool=>{only:* Bool.true;};";
	const char *cases[] = {
		"use:=\\f:Only Bool.false->Bool=>Bool.true;"
		"main:=use &(\\p:Only Bool.false=>p @only=>Nat.zero);",
		"use:=\\f:Only Bool.false->Nat=>Nat.zero;"
		"main:=use &(\\p:Only Bool.false=>p @only=>Bool.true);",
		"use:=\\f:Only Bool.false->(Nat->Nat)=>Nat.zero;"
		"main:=use &(\\p:Only Bool.false=>p @only=>Bool.true);",
		"use:=\\f:Only Bool.true->Bool=>Bool.true;"
		"main:=use &(\\p:Only Bool.true=>p @only=>Nat.zero);",
		"main:=\\p:Only Bool.false=>p @only=>Nat.zero;"
		"main::Only Bool.false->Bool;",
		"use:=\\f:Only Bool.false->Bool=>Bool.true;"
		"main:=use &(\\p:Only Bool.true=>p @only=>Bool.true);",
	};
	const enum pg_synthesis_status expected[] = {PG_SYNTHESIS_DONE, PG_SYNTHESIS_DONE,
		PG_SYNTHESIS_DONE, PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_PENDING, PG_SYNTHESIS_REJECTED};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		char *source = malloc(strlen(prefix) + strlen(cases[i]) + 1);
		assert(source);
		strcpy(source, prefix); strcat(source, cases[i]);
		for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
			struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
			assert(p && p->root && !p->parser.error);
			while (p->synthesis.ready && p->synthesis.steps < 100000)
				pg_synthesis_advance(&p->synthesis, chunk);
			assert(!p->synthesis.ready && pg_synthesis_status(p->root) == expected[i]);
			if (expected[i] != PG_SYNTHESIS_DONE) assert(!pg_synthesis_result(p->root));
			pg_program_destroy(p);
		}
		free(source);
	}
	puts("application constraints: empty elimination receives typed domains; reachable branches and post-checks remain independent");
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "--legacy-intrinsic-dot")) {
		allow_legacy_intrinsic_dot = 1;
		++argv; --argc;
	}
	if (argc > 2 && !strcmp(argv[1], "--steps")) {
		char *end;
		errno = 0;
		uintmax_t limit = strtoumax(argv[2], &end, 10);
		if (errno || end == argv[2] || *end || argv[2][0] == '-' || !limit || limit > UINT64_MAX - 64) {
			fputs("invalid comparison step limit\n", stderr);
			return 2;
		}
		comparison_steps = (uint64_t)limit;
		argv += 2; argc -= 2;
	}
	if (argc == 3 && (!strcmp(argv[1], "--reject") || !strcmp(argv[1], "--unsupported"))) {
		struct pg_program *p = load_program(argv[2]);
		while (p->synthesis.ready && p->synthesis.steps < 1000000)
			pg_synthesis_advance(&p->synthesis, 64);
		enum pg_synthesis_status expected = !strcmp(argv[1], "--reject") ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_UNSUPPORTED;
		int rejected = pg_synthesis_status(p->root) == expected;
		printf("parsed source status: %s %s matched=%d\n", argv[2], argv[1], rejected);
		pg_program_destroy(p);
		return !rejected;
	}
	if (argc == 5 && (!strcmp(argv[1], "--equal") || !strcmp(argv[1], "--equal-image"))) {
		int image = !strcmp(argv[1], "--equal-image");
		int failed = equal_exports(argv[2], argv[3], argv[4], 1, image);
		return equal_exports(argv[2], argv[3], argv[4], 64, image) | failed;
	}
	if (argc != 1) {
		assert(argc == 6);
		char *end;
		unsigned long count = strtoul(argv[5], &end, 10);
		assert(*argv[5] && !*end && count < 1000);
		execute_example(argv[1], argv[2], argv[3], argv[4], count, 1);
		execute_example(argv[1], argv[2], argv[3], argv[4], count, 10000);
		return 0;
	}
	modules();
	binding_contracts();
	result_comparison_checks();
	pending_normalization();
	remembered_normalization();
	graph_index_preparation();
	function_graphs();
	suspended_helper_application();
	suspended_helper_schema();
	suspended_match_body();
	suspended_case_scopes();
	ambiguous_source_calls();
	application_result_constraints();
	char source[] = "{{ id := &(\\A:@ => \\x:A => x); id :: (A:@)->A->A; }}.id";
	struct pg_program *split = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	struct pg_program *whole = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	assert(split && whole && split->root && whole->root);
	memset(source, '?', sizeof(source));
	assert(!split->synthesis.steps && !whole->synthesis.steps);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(split->root));
	pg_synthesis_advance(&split->synthesis, 0);
	assert(!split->synthesis.steps);
	solve(split, 1);
	solve(whole, 10000);
	assert(pg_synthesis_status(split->root) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whole->root) == PG_SYNTHESIS_DONE);
	assert(split->synthesis.steps == whole->synthesis.steps);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(split->root))->core,
		pg_evidence_subject(pg_synthesis_result(whole->root))->core) == 1);
	uint64_t steps = split->synthesis.steps;
	pg_synthesis_advance(&split->synthesis, 10000);
	assert(split->synthesis.steps == steps);
	const struct pg_evidence *proof = pg_synthesis_result(split->root);
	assert(!pg_program_normalize(NULL, proof, 0));
	assert(!pg_program_normalize(split, NULL, 0));
	assert(!pg_program_normalize(whole, proof, 0));
	const struct pg_evidence *empty = pg_prove_empty_context(&split->typing);
	const struct pg_evidence *universe = pg_prove_universe(&split->typing, empty, 0);
	const struct pg_object *binder = pg_binder(&split->graph);
	const struct pg_evidence *open = pg_prove_context_extension(&split->typing, empty, binder, universe);
	const struct pg_evidence *variable = pg_prove_variable(&split->typing, open, binder);
	assert(variable && !pg_program_normalize(split, variable, 0));
	struct pg_synthesis_job *whnf = pg_program_normalize(split, proof, 0);
	struct pg_synthesis_job *nf = pg_program_normalize(split, proof, 1);
	assert(whnf && nf && split->synthesis.steps == steps);
	assert(whnf == pg_program_normalize(split, proof, 0));
	assert(nf == pg_program_normalize(split, proof, 1));
	for (size_t i = 0; pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 10000);
		pg_synthesis_advance(&split->synthesis, 1);
	}
	assert(pg_synthesis_status(whnf) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(nf) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(pg_synthesis_result(nf)) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_evidence_subject(pg_synthesis_result(whnf))->core->kind == PG_LAMBDA);
	assert(pg_program_normalize(split, proof, 1) == nf);
	pg_program_destroy(split);
	pg_program_destroy(whole);
	const char *invalid = "main := ";
	struct pg_program *bad = pg_program_create(invalid, strlen(invalid), PG_DEFINITION_EXPLICIT_THUNK);
	assert(bad && !bad->root && bad->parser.error);
	assert(!bad->synthesis.steps);
	pg_program_destroy(bad);
	struct pg_program *pending = pg_program_create("x:=x;", 5, PG_DEFINITION_EXPLICIT_THUNK);
	assert(pending && pending->root);
	pg_synthesis_advance(&pending->synthesis, 100);
	assert(pg_synthesis_status(pending->root) == PG_SYNTHESIS_PENDING);
	pg_program_destroy(pending);
	assert(!pg_program_create(NULL, 1, PG_DEFINITION_EXPLICIT_THUNK));
	assert(!pg_program_create("", 0, (enum pg_definition_policy)99));
	pg_program_destroy(NULL);
	puts("program: owned source, unresolved root, split-budget solve and pending destruction passed");
	return 0;
}
