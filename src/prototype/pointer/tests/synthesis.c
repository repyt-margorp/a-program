#include "synthesis.h"
#include "computation.h"
#include "identity.h"
#include "action.h"
#include "iadt.h"
#include "prelude.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Different proof paths may establish the same judgement without being interned
 * as one derivation. Check its semantic fields independently of that choice. */
static void same_judgement(const struct pg_evidence *left, const struct pg_evidence *right)
{
	assert(left && right);
	assert(pg_evidence_context(left) == pg_evidence_context(right));
	assert(pg_evidence_judgement(left) == pg_evidence_judgement(right));
	assert(pg_alpha_equal(pg_evidence_subject(left)->core, pg_evidence_subject(right)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(left), pg_evidence_classifier(right)) == 1);
}

static struct pg_synthesis_job *request(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const char *source)
{
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, synthesis->typing->graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, definition.expression);
	assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_request(synthesis, scope, definition.expression) == job);
	assert(!pg_synthesis_result(job));
	assert(pg_parser_next(&parser, &definition) == 0);
	return job;
}

static const struct pg_evidence *complete_with_budget(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, enum pg_synthesis_status expected, unsigned budget)
{
	unsigned steps = 0;
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(!pg_synthesis_result(job));
		assert(++steps < budget);
		pg_synthesis_advance(synthesis, 1);
	}
	assert(pg_synthesis_status(job) == expected);
	assert(!pg_synthesis_dependency(job));
	assert(!pg_synthesis_cycle(job));
	return pg_synthesis_result(job);
}

static const struct pg_evidence *complete(struct pg_synthesis *synthesis,
	struct pg_synthesis_job *job, enum pg_synthesis_status expected)
{
	return complete_with_budget(synthesis, job, expected, 10000);
}

static const struct pg_evidence *normalize(struct pg_synthesis *synthesis,
	const struct pg_evidence *context, const struct pg_evidence *input)
{
	return complete(synthesis, pg_synthesis_normalize(synthesis, context, input), PG_SYNTHESIS_DONE);
}

static void recursive_field_aliases(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, typing->graph));
	assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *self = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, self,
		pg_prove_universe(typing, classifiers, empty, 0));
	struct pg_token token = {.kind = PG_TOKEN_IDENT, .text = "Self", .length = 4};
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis,
		pg_synthesis_root(&synthesis), token, self, context);
	assert(scope);
	const char *sources[] = {"field := @ -> Self;", "field := Self -> @;"};
	for (size_t i = 0; i < 2; ++i) {
		struct pg_synthesis_job *producer = request(&synthesis, scope, sources[i]);
		struct pg_token alias = {.kind = PG_TOKEN_IDENT, .text = "Alias", .length = 5};
		const struct pg_source_scope *named = pg_synthesis_name_job(&synthesis, scope, alias, producer);
		assert(named);
		struct pg_synthesis_job *consumer = request(&synthesis, named, "field := Alias;");
		assert(!pg_synthesis_result(producer) && !pg_synthesis_result(consumer));
		const struct pg_evidence *proof = complete(&synthesis, consumer, PG_SYNTHESIS_DONE);
		const struct pg_term *core = pg_evidence_subject(proof)->core;
		assert(core == pg_evidence_subject(pg_synthesis_result(producer))->core);
		assert(pg_data_field_positive(core, self, 0) == (i ? 0 : 1));
	}
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static void dependent_application_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *domain = pg_prove_universe(typing, classifiers, empty, 2);
	const struct pg_object *a = pg_binder(typing->graph), *x = pg_binder(typing->graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(typing, empty, a, domain);
	const struct pg_evidence *a_type = pg_prove_value_type(typing, pg_prove_variable(typing, a_context, a));
	const struct pg_evidence *x_context = pg_prove_context_extension(typing, a_context, x, a_type);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, x_context, x));
	const struct pg_evidence *inner = pg_prove_lambda(typing,
		pg_prove_pi(typing, classifiers, a_type, x_context, pg_prove_classifier(typing, classifiers, x_context, body)), body);
	const struct pg_evidence *function = pg_prove_lambda(typing,
		pg_prove_pi(typing, classifiers, domain, a_context, pg_prove_classifier(typing, classifiers, a_context, inner)), inner);
	const struct pg_evidence *type_argument = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, empty, 1));
	const struct pg_evidence *value_argument = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, empty, 0));
	assert(function && type_argument && value_argument);
	const struct pg_evidence *expected = pg_prove_application(typing,
		pg_prove_application(typing, function, type_argument), value_argument);
	assert(expected);
	for (size_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(pg_whnf_work_init(&work, typing->graph) == 0);
		assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
		struct pg_synthesis_job *first = pg_synthesis_application(&synthesis, empty,
			pg_synthesis_normalize(&synthesis, empty, function),
			pg_synthesis_return(&synthesis, empty, pg_prove_return(typing, classifiers, type_argument)));
		struct pg_synthesis_job *second = pg_synthesis_application(&synthesis, empty, first,
			pg_synthesis_evidence(&synthesis, value_argument));
		assert(first && second && !pg_synthesis_result(first) && !pg_synthesis_result(second));
		for (size_t count = 0; pg_synthesis_status(second) == PG_SYNTHESIS_PENDING; ++count) {
			assert(count < 10000);
			pg_synthesis_advance(&synthesis, chunk);
		}
		assert(pg_synthesis_status(second) == PG_SYNTHESIS_DONE);
		same_judgement(pg_synthesis_result(second), expected);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
}

static void identity_instance_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *context = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *b = pg_binder(typing->graph);
	const struct pg_object *x = pg_binder(typing->graph), *y = pg_binder(typing->graph), *r = pg_binder(typing->graph);
	context = pg_prove_context_extension(typing, context, a, pg_prove_universe(typing, classifiers, context, 1));
	context = pg_prove_context_extension(typing, context, b, pg_prove_universe(typing, classifiers, context, 1));
	context = pg_prove_context_extension(typing, context, x, pg_prove_value_type(typing, pg_prove_variable(typing, context, a)));
	context = pg_prove_context_extension(typing, context, y, pg_prove_value_type(typing, pg_prove_variable(typing, context, b)));
	const struct pg_evidence *family_type = pg_prove_identity_type(typing, pg_prove_universe(typing, classifiers, context, 1),
		pg_prove_variable(typing, context, a), pg_prove_variable(typing, context, b));
	context = pg_prove_context_extension(typing, context, r, family_type);
	const struct pg_evidence *family = pg_prove_variable(typing, context, r);
	const struct pg_evidence *left = pg_prove_variable(typing, context, x), *right = pg_prove_variable(typing, context, y);
	const struct pg_evidence *expected = pg_prove_identity_instance(typing, classifiers, family, left, right);
	assert(expected && pg_evidence_classifier(left) != pg_evidence_classifier(right));
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	struct pg_synthesis_job *producer = pg_synthesis_return(&synthesis, context, pg_prove_return(typing, classifiers, family));
	struct pg_synthesis_job *l = pg_synthesis_evidence(&synthesis, left), *rr = pg_synthesis_evidence(&synthesis, right);
	struct pg_synthesis_job *instance = pg_synthesis_identity_instance(&synthesis, context, producer, l, rr);
	assert(instance && !pg_synthesis_result(instance));
	assert(pg_synthesis_identity_instance(&synthesis, context, producer, l, rr) == instance);
	const struct pg_evidence *result = complete(&synthesis, instance, PG_SYNTHESIS_DONE);
	same_judgement(result, expected);
	assert(pg_evidence_subject(pg_evidence_premise(result, 0))->core == pg_evidence_subject(family)->core);
	struct pg_synthesis_job *f = pg_synthesis_evidence(&synthesis, pg_synthesis_result(producer));
	struct pg_synthesis_job *canonical = pg_synthesis_identity_instance(&synthesis, context, f, l, rr);
	assert(pg_synthesis_status(canonical) == PG_SYNTHESIS_DONE && pg_synthesis_result(canonical) == result);
	/* An opaque selected family still exposes its supplied outer endpoints.
	 * It does not supply an additional Identity direction by assumption. */
	struct pg_dimensions opaque_dimensions;
	assert(pg_dimensions_init(&opaque_dimensions, typing->graph) == 0);
	struct pg_synthesis_job *opaque_formation = pg_synthesis_identity_formation(&synthesis, instance);
	assert(complete(&synthesis, opaque_formation, PG_SYNTHESIS_DONE) == result);
	for (size_t side = 0; side < 2; ++side) {
		struct pg_coordinate coordinate = {side ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
		const struct pg_dimension_map *outer = pg_dimension_map(&opaque_dimensions, 0, 1, &coordinate);
		const struct pg_evidence *endpoint = complete(&synthesis,
			pg_synthesis_identity_face_job(&synthesis, context, instance, outer), PG_SYNTHESIS_DONE);
		same_judgement(endpoint, side ? right : left);
	}
	struct pg_coordinate extra[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	complete(&synthesis, pg_synthesis_identity_face_job(&synthesis, context, instance,
		pg_dimension_map(&opaque_dimensions, 1, 2, extra)), PG_SYNTHESIS_UNSUPPORTED);
	assert(pg_synthesis_result(instance) == result);
	pg_dimensions_destroy(&opaque_dimensions);
	complete(&synthesis, pg_synthesis_identity_instance(&synthesis, context, f, rr, l), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_identity_instance(&synthesis, context, l, l, rr), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_identity_instance(&synthesis, context, f,
		pg_synthesis_evidence(&synthesis, pg_prove_return(typing, classifiers, left)), rr), PG_SYNTHESIS_REJECTED);
	/* A selected reflexive family over a line forms a square. Run the full
	 * pending instance -> recovered formation -> boundary pipeline. */
	const struct pg_evidence *base = pg_prove_universe(typing, classifiers, context, 1);
	const struct pg_evidence *point = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, context, 0));
	const struct pg_evidence *line = pg_prove_identity_type(typing, base, point, point);
	const struct pg_evidence *path = pg_prove_reflexivity(typing, base, point);
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	struct pg_coordinate coordinates[] = {{PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
	const struct pg_dimension_map *face = pg_dimension_map(&dimensions, 1, 2, coordinates);
	uint64_t steps = synthesis.steps;
	struct pg_synthesis_job *line_family = pg_synthesis_reflexivity(&synthesis, context,
		pg_synthesis_evidence(&synthesis, pg_prove_type_value(typing, line)));
	struct pg_synthesis_job *path_job = pg_synthesis_return(&synthesis, context, pg_prove_return(typing, classifiers, path));
	struct pg_synthesis_job *square = pg_synthesis_identity_instance(&synthesis, context, line_family, path_job, path_job);
	struct pg_synthesis_job *formation = pg_synthesis_identity_formation(&synthesis, square);
	struct pg_synthesis_job *boundary = pg_synthesis_identity_face_job(&synthesis, context, square, face);
	assert(square && formation && boundary && synthesis.steps == steps);
	assert(!pg_synthesis_result(square) && !pg_synthesis_result(formation) && !pg_synthesis_result(boundary));
	const struct pg_evidence *edge = complete(&synthesis, boundary, PG_SYNTHESIS_DONE);
	same_judgement(edge, path);
	const struct pg_evidence *recovered = complete(&synthesis, formation, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(recovered) == PG_IDENTITY_FORM);
	assert(pg_evidence_subject(recovered)->core == pg_evidence_subject(pg_synthesis_result(square))->core);
	struct pg_synthesis_job *shared_face = pg_synthesis_identity_face(&synthesis, context, recovered, face);
	assert(pg_synthesis_status(shared_face) == PG_SYNTHESIS_DONE && pg_synthesis_result(shared_face) == edge);
	pg_dimensions_destroy(&dimensions);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static void cube_application_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers, size_t arity)
{
	unsigned budget = arity == 1 ? 10000000 : 50000000;
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *x = pg_binder(typing->graph), *y = pg_binder(typing->graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *a_type = pg_prove_value_type(typing, pg_prove_variable(typing, a_context, a));
	const struct pg_evidence *source = pg_prove_context_extension(typing, a_context, x, a_type);
	const struct pg_evidence *domain = pg_prove_projection(typing, source, a_type);
	const struct pg_evidence *body_context = pg_prove_context_extension(typing, source, y, domain);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, body_context, y));
	if (arity == 2) {
		const struct pg_object *z = pg_binder(typing->graph);
		const struct pg_evidence *inner_domain = pg_prove_projection(typing, body_context, domain);
		const struct pg_evidence *inner_context = pg_prove_context_extension(typing, body_context, z, inner_domain);
		const struct pg_evidence *inner_body = pg_prove_projection(typing, inner_context, body);
		body = pg_prove_lambda(typing, pg_prove_pi(typing, classifiers, inner_domain, inner_context,
			pg_prove_classifier(typing, classifiers, inner_context, inner_body)), inner_body);
	}
	const struct pg_evidence *function = pg_prove_lambda(typing,
		pg_prove_pi(typing, classifiers, domain, body_context,
			pg_prove_classifier(typing, classifiers, body_context, body)), body);
	assert(function);
	for (size_t d = 1, count = 3; d <= 3; ++d, count *= 3) {
		const struct pg_binding_cube *cubes[] = {pg_binding_cube(&dimensions, d), pg_binding_cube(&dimensions, d)};
		const struct pg_dimension_map *order = pg_dimension_identity(&dimensions, d);
		const struct pg_evidence *context = pg_identity_cube_context(typing, &dimensions, source, 2, cubes, order);
		const struct pg_evidence *acted = pg_identity_cube_action(typing, classifiers, &dimensions,
			source, function, 2, cubes, order);
		assert(context && acted);
		const struct pg_evidence *arguments[27];
		const struct pg_context *cursor = pg_evidence_context(context);
		for (size_t i = count; i; --i, cursor = cursor->parent)
			arguments[i - 1] = pg_prove_variable(typing, context, cursor->binder);
		for (size_t chunk = 1; chunk <= 64; chunk *= 64) {
			struct pg_whnf_work work;
			struct pg_synthesis synthesis;
			assert(pg_whnf_work_init(&work, typing->graph) == 0);
			assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
			struct pg_synthesis_job *applied = pg_synthesis_evidence(&synthesis, acted);
			for (size_t i = 0; i < count * arity; ++i)
				applied = pg_synthesis_application(&synthesis, context, applied, pg_synthesis_evidence(&synthesis, arguments[i % count]));
			assert(applied && !pg_synthesis_result(applied));
			for (size_t steps = 0; pg_synthesis_status(applied) == PG_SYNTHESIS_PENDING; steps += chunk) {
				assert(steps < budget);
				pg_synthesis_advance(&synthesis, chunk);
			}
			assert(pg_synthesis_status(applied) == PG_SYNTHESIS_DONE);
			const struct pg_evidence *exposed = complete_with_budget(&synthesis,
				pg_synthesis_normalize_classifier(&synthesis, context, pg_synthesis_result(applied)), PG_SYNTHESIS_DONE, budget);
			const struct pg_evidence *value = complete_with_budget(&synthesis,
				pg_synthesis_return(&synthesis, context, exposed), PG_SYNTHESIS_DONE, budget);
			value = complete_with_budget(&synthesis, pg_synthesis_normalize(&synthesis, context, value), PG_SYNTHESIS_DONE, budget);
			value = complete_with_budget(&synthesis, pg_synthesis_expect(&synthesis, pg_synthesis_evidence(&synthesis, value),
				pg_synthesis_evidence(&synthesis, pg_prove_classifier(typing, classifiers, context, arguments[count - 1]))),
				PG_SYNTHESIS_DONE, budget);
			same_judgement(value, arguments[count - 1]);
			printf("cube lambda application: arity %zu, dimension %zu, chunk %zu, %llu steps\n", arity, d, chunk,
				(unsigned long long)synthesis.steps);
			pg_synthesis_destroy(&synthesis);
			pg_whnf_work_destroy(&work);
		}
	}
	pg_dimensions_destroy(&dimensions);
}

static void wait_on(struct pg_synthesis *synthesis, struct pg_synthesis_job *parent,
	struct pg_synthesis_job *child)
{
	unsigned steps = 0;
	while (pg_synthesis_dependency(parent) != child) {
		assert(pg_synthesis_status(parent) == PG_SYNTHESIS_PENDING && ++steps < 1000);
		pg_synthesis_advance(synthesis, 1);
	}
}

static void accepted_inputs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing), *proofs[2];
	const struct pg_object *x = pg_binder(typing->graph);
	for (size_t n = 0; n < 2; ++n) {
		const struct pg_evidence *domain = pg_prove_universe(typing, classifiers, empty, n);
		const struct pg_evidence *context = pg_prove_context_extension(typing, empty, x, domain);
		const struct pg_evidence *codomain = pg_prove_return_type(typing, classifiers,
			pg_prove_projection(typing, context, domain));
		proofs[n] = pg_prove_lambda(typing, pg_prove_pi(typing, classifiers, domain, context, codomain),
			pg_prove_return(typing, classifiers, pg_prove_variable(typing, context, x)));
		assert(proofs[n]);
	}
	assert(pg_evidence_subject(proofs[0])->core == pg_evidence_subject(proofs[1])->core);
	assert(pg_evidence_classifier(proofs[0]) != pg_evidence_classifier(proofs[1]));
	size_t terms = typing->graph->terms.count, evidence = typing->proofs.count;
	struct pg_synthesis_job *first = pg_synthesis_evidence(&synthesis, proofs[0]);
	struct pg_synthesis_job *second = pg_synthesis_evidence(&synthesis, proofs[1]);
	assert(first && second && first != second);
	assert(pg_synthesis_result(first) == proofs[0] && pg_synthesis_result(second) == proofs[1]);
	assert(pg_synthesis_evidence(&synthesis, proofs[0]) == first);
	assert(!pg_synthesis_evidence(&synthesis, NULL));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_synthesis_evidence(&synthesis, pg_prove_empty_context(&foreign)));
	pg_typing_destroy(&foreign);
	pg_synthesis_advance(&synthesis, 100);
	assert(!synthesis.steps && !synthesis.ready && synthesis.jobs.count == 2);
	assert(typing->graph->terms.count == terms && typing->proofs.count == evidence);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	assert(pg_synthesis_root(&synthesis) == root);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "f", .length = 1};
	for (size_t i = 0; i < 2; ++i) {
		size_t jobs = synthesis.jobs.count;
		const struct pg_source_scope *scope = pg_synthesis_name(&synthesis, root, name, proofs[i]);
		assert(scope && synthesis.jobs.count == jobs);
		assert(pg_synthesis_name(&synthesis, root, name, proofs[i]) == scope);
		assert(complete(&synthesis, request(&synthesis, scope, "main := f;"), PG_SYNTHESIS_DONE) == proofs[i]);
		/* Only the expression request is new, not another accepted producer. */
		assert(synthesis.jobs.count == jobs + 1);
		if (!i) root = scope;
	}
	const char *prefixes[] = {"Low", "High"};
	const char *references[] = {"main := Low.f;", "main := High.f;"};
	const struct pg_source_scope *scope = pg_synthesis_root(&synthesis);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_source_scope *exports = pg_synthesis_name(&synthesis,
			pg_synthesis_root(&synthesis), name, proofs[i]);
		scope = pg_synthesis_namespace(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = prefixes[i], .length = strlen(prefixes[i])}, exports);
		assert(scope);
	}
	for (size_t i = 0; i < 2; ++i)
		assert(complete(&synthesis, request(&synthesis, scope, references[i]), PG_SYNTHESIS_DONE) == proofs[i]);
	root = pg_synthesis_root(&synthesis);
	const struct pg_source_scope *typed[2];
	struct pg_synthesis_job *resolved[2];
	struct pg_parser parser;
	struct pg_definition definition;
	const char *source = "main := f;";
	pg_parser_init(&parser, typing->graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	for (size_t i = 0; i < 2; ++i) {
		typed[i] = pg_synthesis_name(&synthesis, root, name, proofs[i]);
		resolved[i] = pg_synthesis_request(&synthesis, typed[i], definition.expression);
		assert(complete(&synthesis, resolved[i], PG_SYNTHESIS_DONE) == proofs[i]);
	}
	assert(typed[0] != typed[1] && resolved[0] != resolved[1]);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, x,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_source_scope *bound = pg_synthesis_bind(&synthesis, root, name, x, context);
	struct pg_token intrinsic = {.kind = '#'};
	const struct pg_source_scope *space = pg_synthesis_namespace(&synthesis, root, intrinsic, typed[0]);
	assert(bound && bound != typed[0] && space);
	char spelling[] = "f";
	struct pg_token relocated = {.kind = PG_TOKEN_IDENT, .text = spelling, .length = 1, .line = 100, .offset = 256};
	terms = typing->graph->terms.count; evidence = typing->proofs.count;
	size_t jobs = synthesis.jobs.count, scopes = synthesis.scopes.count, reductions = work.jobs.count;
	uint64_t steps = synthesis.steps;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_root(&synthesis) == root);
		assert(pg_synthesis_name(&synthesis, root, relocated, proofs[0]) == typed[0]);
		assert(pg_synthesis_bind(&synthesis, root, relocated, x, context) == bound);
		assert(pg_synthesis_namespace(&synthesis, root, intrinsic, typed[0]) == space);
		assert(pg_synthesis_request(&synthesis, pg_synthesis_name(&synthesis, root, relocated, proofs[0]),
			definition.expression) == resolved[0]);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(typing->graph->terms.count == terms && typing->proofs.count == evidence);
	assert(synthesis.jobs.count == jobs && synthesis.scopes.count == scopes);
	assert(work.jobs.count == reductions && synthesis.steps == steps);
	assert(pg_synthesis_namespace(&synthesis, root, intrinsic, typed[1]) != space);
	assert(pg_synthesis_name(&synthesis, typed[0], name, proofs[0]) != typed[0]);
	relocated.text = "g";
	assert(pg_synthesis_name(&synthesis, root, relocated, proofs[0]) != typed[0]);
	assert(pg_synthesis_bind(&synthesis, root, relocated, x, context) != bound);
	const struct pg_evidence *other_context = pg_prove_context_extension(typing, empty, x,
		pg_prove_universe(typing, classifiers, empty, 1));
	assert(pg_synthesis_bind(&synthesis, root, name, x, other_context) != bound);
	const struct pg_object *fresh = pg_binder(typing->graph);
	other_context = pg_prove_context_extension(typing, empty, fresh,
		pg_prove_universe(typing, classifiers, empty, 0));
	assert(pg_synthesis_bind(&synthesis, root, name, fresh, other_context) != bound);
	const struct pg_evidence *argument = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *returned = pg_prove_return(typing, classifiers, argument);
	struct pg_synthesis_job *pending_function = request(&synthesis, typed[1], "main := f;");
	struct pg_synthesis_job *pending_argument = pg_synthesis_return(&synthesis, empty, returned);
	struct pg_synthesis_job *application = pg_synthesis_application(&synthesis, empty, pending_function, pending_argument);
	assert(application && pg_synthesis_application(&synthesis, empty, pending_function, pending_argument) == application);
	assert(!pg_synthesis_result(application));
	const struct pg_evidence *result = complete(&synthesis, application, PG_SYNTHESIS_DONE);
	const struct pg_evidence *computed_argument = pg_synthesis_result(pending_argument);
	same_judgement(result, pg_prove_application(typing, proofs[1], computed_argument));
	struct pg_synthesis_job *arg_job = pg_synthesis_evidence(&synthesis, computed_argument);
	struct pg_synthesis_job *canonical = pg_synthesis_application(&synthesis, empty, second, arg_job);
	assert(pg_synthesis_status(canonical) == PG_SYNTHESIS_DONE && pg_synthesis_result(canonical) == result);
	/* The same function Core at U0 cannot consume a value of U1. */
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, first, arg_job), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, arg_job, arg_job), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, second,
		pg_synthesis_evidence(&synthesis, returned)), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_application(&synthesis, context, second, arg_job), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *thunked = pg_synthesis_evidence(&synthesis, pg_prove_thunk(typing, classifiers, proofs[1]));
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, thunked, arg_job), PG_SYNTHESIS_REJECTED);
	struct pg_parser empty_parser;
	pg_parser_init(&empty_parser, typing->graph, "", 0);
	const struct pg_syntax *empty_program = pg_parser_program(&empty_parser);
	assert(empty_program);
	struct pg_synthesis_job *namespace_only = pg_synthesis_request(&synthesis, root, empty_program);
	assert(!complete(&synthesis, namespace_only, PG_SYNTHESIS_DONE));
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, namespace_only, arg_job), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_application(&synthesis, empty, second, namespace_only), PG_SYNTHESIS_REJECTED);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static struct pg_synthesis_job *program(struct pg_synthesis *synthesis,
	const struct pg_source_scope *scope, const char *source)
{
	struct pg_parser parser;
	pg_parser_init(&parser, synthesis->typing->graph, source, strlen(source));
	const struct pg_syntax *syntax = pg_parser_program(&parser);
	assert(syntax);
	struct pg_synthesis_job *job = pg_synthesis_request(synthesis, scope, syntax);
	assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_request(synthesis, scope, syntax) == job);
	return job;
}

static const struct pg_syntax *select_definition(struct pg_graph *graph,
	const struct pg_syntax *definitions, struct pg_token name)
{
	struct pg_syntax *member = pg_alloc(graph, sizeof(*member));
	struct pg_syntax *selection = pg_alloc(graph, sizeof(*selection));
	assert(member && selection);
	*member = (struct pg_syntax){.kind = PG_SYNTAX_ATOM, .token = name};
	*selection = (struct pg_syntax){.kind = PG_SYNTAX_QUALIFIED, .left = definitions, .right = member};
	return selection;
}

static void pending_names(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *export = program(&synthesis, root,
		"{{export:=&(\\A:@ => \\x:A => x);}}.export;");
	size_t jobs = synthesis.jobs.count, terms = typing->graph->terms.count, proofs = typing->proofs.count;
	const struct pg_source_scope *scope = pg_synthesis_name_job(&synthesis, root, name, export);
	assert(scope && !synthesis.steps && synthesis.jobs.count == jobs);
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
	for (size_t i = 0; i < 100; ++i) assert(pg_synthesis_name_job(&synthesis, root, name, export) == scope);
	struct pg_synthesis_job *reference = request(&synthesis, scope, "main:=id;");
	wait_on(&synthesis, reference, export);
	const struct pg_evidence *answer = complete(&synthesis, reference, PG_SYNTHESIS_DONE);
	assert(answer == pg_synthesis_result(export));
	const struct pg_source_scope *accepted = pg_synthesis_name(&synthesis, root, name, answer);
	assert(accepted == pg_synthesis_name_job(&synthesis, root, name, pg_synthesis_evidence(&synthesis, answer)));
	const struct pg_evidence *call = complete(&synthesis, request(&synthesis, scope,
		"main:=\\A:@ => \\x:A => id A x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	call = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, call), PG_SYNTHESIS_DONE);
	same_judgement(call, complete(&synthesis, request(&synthesis, root,
		"main:=\\A:@ => \\x:A => x;"), PG_SYNTHESIS_DONE));
	assert(!pg_synthesis_name_job(&synthesis, NULL, name, export));
	assert(!pg_synthesis_name_job(&synthesis, root, (struct pg_token){0}, export));
	assert(!pg_synthesis_name_job(&synthesis, root, name, NULL));
	struct pg_synthesis foreign;
	assert(pg_synthesis_init(&foreign, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	assert(!pg_synthesis_name_job(&foreign, pg_synthesis_root(&foreign), name, export));
	assert(!pg_synthesis_name_job(&synthesis, pg_synthesis_root(&foreign), name, export));
	pg_synthesis_destroy(&foreign);
	const struct pg_object *binder = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, binder,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_source_scope *open = pg_synthesis_bind(&synthesis, root, name, binder, context);
	const struct pg_source_scope *projected = pg_synthesis_name(&synthesis, open, name, answer);
	assert(projected == pg_synthesis_name_job(&synthesis, open, name, pg_synthesis_evidence(&synthesis, answer)));
	same_judgement(complete(&synthesis, request(&synthesis, projected, "main:=id;"), PG_SYNTHESIS_DONE),
		pg_prove_projection(typing, context, answer));
	struct pg_synthesis_job *escaping = request(&synthesis, open, "main:=id;");
	scope = pg_synthesis_name_job(&synthesis, root, name, escaping);
	complete(&synthesis, request(&synthesis, scope, "main:=id;"), PG_SYNTHESIS_ERROR);
	struct pg_synthesis_job *not_terms[] = {program(&synthesis, root, "x:=@;"), pg_synthesis_evidence(&synthesis, empty)};
	for (size_t i = 0; i < sizeof(not_terms) / sizeof(*not_terms); ++i) {
		scope = pg_synthesis_name_job(&synthesis, root, name, not_terms[i]);
		complete(&synthesis, request(&synthesis, scope, "main:=id;"), PG_SYNTHESIS_UNSUPPORTED);
	}
	struct pg_synthesis_job *failed = request(&synthesis, root, "main:=missing;");
	scope = pg_synthesis_name_job(&synthesis, root, name, failed);
	complete(&synthesis, request(&synthesis, scope, "main:=id;"), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *cycle = program(&synthesis, root, "{{a:=b; b:=a;}}.a;");
	scope = pg_synthesis_name_job(&synthesis, root, name, cycle);
	reference = request(&synthesis, scope, "main:=id;");
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(reference) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(reference));
	assert(!pg_synthesis_result(reference) && !synthesis.ready);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("pending names: source export reuse, checked projection, failed/non-term inputs and cycle waiting passed");
}

static void source_imports(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *provider = program(&synthesis, root,
		"{{export:=&(\\A:@ => \\x:A => x);}}.export;");
	const struct pg_source_scope *bindings = pg_synthesis_name_job(&synthesis, root, name, provider);
	const struct pg_source_scope *scope = pg_synthesis_import_scope(&synthesis, root, bindings);
	assert(scope && !synthesis.steps);
	assert(pg_synthesis_import_scope(&synthesis, root, bindings) == scope);
	assert(pg_synthesis_import_scope(&synthesis, root, root) != scope);
	struct pg_synthesis_job *client = program(&synthesis, scope,
		"import id; import id; id::(A:@)->A->A; main:=id;");
	assert(pg_synthesis_status(provider) == PG_SYNTHESIS_PENDING);
	complete(&synthesis, client, PG_SYNTHESIS_DONE);
	const struct pg_evidence *answer = pg_synthesis_result(provider);
	assert(answer && pg_synthesis_result(pg_synthesis_definition(client,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "main", .length = 4})) == answer);
	assert(!pg_synthesis_definition(client, name));
	size_t counts[2];
	const char *repeated[] = {"import id;", "import id; import id; import id;"};
	for (size_t i = 0; i < 2; ++i) {
		size_t jobs = synthesis.jobs.count;
		complete(&synthesis, program(&synthesis, scope, repeated[i]), PG_SYNTHESIS_DONE);
		counts[i] = synthesis.jobs.count - jobs;
	}
	assert(counts[0] == counts[1]);
	assert(complete(&synthesis, request(&synthesis, scope,
		"{{import id;}}.id;"), PG_SYNTHESIS_DONE) == answer);
	complete(&synthesis, request(&synthesis, scope, "main:=id;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *call = complete(&synthesis, request(&synthesis, scope,
		"{{import id; f:=&(\\A:@ => \\x:A => id A x);}}.f;"), PG_SYNTHESIS_DONE);
	call = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, call), PG_SYNTHESIS_DONE);
	same_judgement(call, answer);
	const char *shadowed[] = {
		"{{import id; id:=@;}}.id;",
		"{{id:=@; import id;}}.id;"
	};
	for (size_t i = 0; i < sizeof(shadowed) / sizeof(*shadowed); ++i)
		assert(complete(&synthesis, request(&synthesis, scope, shadowed[i]), PG_SYNTHESIS_DONE)
			== pg_prove_universe(typing, classifiers, empty, 0));
	struct pg_token module_name = {.kind = PG_TOKEN_IDENT, .text = "Client", .length = 6};
	const struct pg_source_scope *exports = pg_synthesis_module_namespace(&synthesis, root, module_name, client);
	assert(complete(&synthesis, request(&synthesis, exports, "main:=Client.main;"), PG_SYNTHESIS_DONE) == answer);
	complete(&synthesis, request(&synthesis, exports, "main:=Client.id;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, pg_synthesis_import_scope(&synthesis, root, exports),
		"import Client;"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, program(&synthesis, root, "import id;"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, program(&synthesis, pg_synthesis_import_scope(&synthesis, scope, root),
		"import id;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope, "import absent;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope, "import id; id::@;"), PG_SYNTHESIS_REJECTED);
	/* Importing a raw computation is naming, not assignment-time quotation. */
	struct pg_synthesis_job *raw = request(&synthesis, root, "main:=\\A:@ => \\x:A => x;");
	bindings = pg_synthesis_name_job(&synthesis, root, name, raw);
	const struct pg_source_scope *raw_scope = pg_synthesis_import_scope(&synthesis, root, bindings);
	const struct pg_evidence *raw_answer = complete(&synthesis, request(&synthesis, raw_scope,
		"{{import id;}}.id;"), PG_SYNTHESIS_DONE);
	assert(raw_answer == pg_synthesis_result(raw));
	assert(pg_evidence_judgement(pg_synthesis_result(raw)) == PG_JUDGEMENT_COMPUTATION);
	const struct pg_object *binder = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, binder,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_source_scope *open = pg_synthesis_bind(&synthesis, root, name, binder, context);
	const struct pg_source_scope *open_imports = pg_synthesis_import_scope(&synthesis, open,
		pg_synthesis_name(&synthesis, root, name, answer));
	same_judgement(complete(&synthesis, request(&synthesis, open_imports,
		"{{import id;}}.id;"), PG_SYNTHESIS_DONE), pg_prove_projection(typing, context, answer));
	assert(!pg_synthesis_import_scope(&synthesis, root, open));
	assert(!pg_synthesis_import_scope(&synthesis, root, NULL));
	assert(!pg_synthesis_import_scope(&synthesis, NULL, bindings));
	struct pg_synthesis foreign;
	assert(pg_synthesis_init(&foreign, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	assert(!pg_synthesis_import_scope(&synthesis, root, pg_synthesis_root(&foreign)));
	assert(!pg_synthesis_import_scope(&synthesis, pg_synthesis_root(&foreign), bindings));
	pg_synthesis_destroy(&foreign);
	struct pg_synthesis_job *invalid[] = {
		request(&synthesis, open, "main:=id;"),
		pg_synthesis_evidence(&synthesis, empty),
		program(&synthesis, root, "x:=@;"),
		request(&synthesis, root, "main:=absent;")
	};
	enum pg_synthesis_status statuses[] = {PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_UNSUPPORTED,
		PG_SYNTHESIS_UNSUPPORTED, PG_SYNTHESIS_REJECTED};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		bindings = pg_synthesis_name_job(&synthesis, root, name, invalid[i]);
		scope = pg_synthesis_import_scope(&synthesis, root, bindings);
		complete(&synthesis, program(&synthesis, scope, "import id; id:=@;"), statuses[i]);
	}
	provider = program(&synthesis, root, "{{a:=b; b:=a;}}.a;");
	bindings = pg_synthesis_name_job(&synthesis, root, name, provider);
	scope = pg_synthesis_import_scope(&synthesis, root, bindings);
	client = program(&synthesis, scope, "import id; import id; main:=id;");
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(client) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(client));
	assert(!synthesis.ready);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("source imports: selected pending symbols, repeated imports, shadowing, closure and shared dependencies passed");
}

static void definition_selections(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *scope = pg_synthesis_root(&synthesis);
	const char *source = "left:=id; right:=id; id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A;";
	struct pg_parser parser;
	pg_parser_init(&parser, typing->graph, source, strlen(source));
	const struct pg_syntax *definitions = pg_parser_program(&parser);
	assert(definitions);
	struct pg_token left = {.kind = PG_TOKEN_IDENT, .text = "left", .length = 4};
	struct pg_token right = {.kind = PG_TOKEN_IDENT, .text = "right", .length = 5};
	struct pg_synthesis_job *selected = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, left));
	struct pg_synthesis_job *module = pg_synthesis_request(&synthesis, scope, definitions);
	const struct pg_evidence *answer = complete(&synthesis, selected, PG_SYNTHESIS_DONE);
	complete(&synthesis, module, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_definition(selected, left) == pg_synthesis_definition(module, left));
	assert(pg_synthesis_definition(selected, right) == pg_synthesis_definition(module, right));
	size_t jobs = synthesis.jobs.count, scopes = synthesis.scopes.count;
	size_t terms = typing->graph->terms.count, proofs = typing->proofs.count, reductions = work.jobs.count;
	struct pg_synthesis_job *second = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, right));
	assert(complete(&synthesis, second, PG_SYNTHESIS_DONE) == answer);
	assert(pg_synthesis_definition(second, left) == pg_synthesis_definition(module, left));
	assert(synthesis.jobs.count == jobs + 1 && synthesis.scopes.count == scopes);
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs && work.jobs.count == reductions);
	const char *invalid[] = {"left:=@; right:=missing;", "left:=@; right:=@; right::@;",
		"left:=@; right:=@; left:=@;"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		pg_parser_init(&parser, typing->graph, invalid[i], strlen(invalid[i]));
		definitions = pg_parser_program(&parser);
		assert(definitions);
		module = pg_synthesis_request(&synthesis, scope, definitions);
		selected = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, left));
		second = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, right));
		complete(&synthesis, selected, PG_SYNTHESIS_REJECTED);
		complete(&synthesis, second, PG_SYNTHESIS_REJECTED);
		complete(&synthesis, module, PG_SYNTHESIS_REJECTED);
		assert(pg_synthesis_definition(selected, left) == pg_synthesis_definition(second, left));
	}
	source = "left:=@; a:=b; b:=a;";
	pg_parser_init(&parser, typing->graph, source, strlen(source));
	definitions = pg_parser_program(&parser);
	assert(definitions);
	selected = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, left));
	second = pg_synthesis_request(&synthesis, scope, select_definition(typing->graph, definitions, right));
	/* A missing member rejects even when a different definition is pending. */
	complete(&synthesis, second, PG_SYNTHESIS_REJECTED);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(selected) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(selected));
	assert(pg_synthesis_status(pg_synthesis_definition(selected, left)) == PG_SYNTHESIS_DONE);
	assert(!pg_synthesis_result(selected) && !synthesis.ready);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("definition selections: shared producers, whole-module checking and pending/missing boundaries passed");
}

static void namespaces(struct pg_synthesis *synthesis, const struct pg_source_scope *exports,
	const struct pg_identity_library *library)
{
	struct pg_token intrinsic = {.kind = '#'};
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "N", .length = 1};
	const struct pg_source_scope *root = pg_synthesis_root(synthesis);
	size_t terms = synthesis->typing->graph->terms.count, jobs = synthesis->jobs.count;
	size_t proofs = synthesis->typing->proofs.count, evaluations = synthesis->normalization->jobs.count;
	const struct pg_source_scope *scope = pg_synthesis_namespace(synthesis, root, intrinsic, exports);
	scope = pg_synthesis_namespace(synthesis, scope, name, exports);
	const struct pg_source_scope *nested = pg_synthesis_namespace(synthesis, root, name, exports);
	struct pg_token outer = {.kind = PG_TOKEN_IDENT, .text = "Outer", .length = 5};
	scope = pg_synthesis_namespace(synthesis, scope, outer, nested);
	assert(scope);
	assert(synthesis->typing->graph->terms.count == terms && synthesis->jobs.count == jobs);
	assert(synthesis->typing->proofs.count == proofs && synthesis->normalization->jobs.count == evaluations);
	const char *aliases[] = {"main := #.refl;", "main := N.refl;", "main := Outer.N.refl;"};
	for (size_t i = 0; i < sizeof(aliases) / sizeof(*aliases); ++i)
		assert(complete(synthesis, request(synthesis, scope, aliases[i]), PG_SYNTHESIS_DONE) == library->reflexivity);
	assert(synthesis->typing->graph->terms.count == terms && synthesis->normalization->jobs.count == evaluations);
	const char *good[] = {
		"main := \\A:@ => \\x:A => #.refl A x :: N.Eq A x x;",
		"{{ alias:=&#.refl; main:=&(\\A:@ => \\x:A => alias A x :: #.Eq A x x); }}.main;",
		"main := \\refl:@ => #.refl;",
		"main := \\A:@ => { f:=&Outer.N.refl; f; }.f;"
	};
	for (size_t i = 0; i < sizeof(good) / sizeof(*good); ++i)
		complete(synthesis, request(synthesis, scope, good[i]), PG_SYNTHESIS_DONE);
	const char *missing[] = {"main := #.missing;", "main := Missing.refl;", "main := Outer.refl;",
		"main := N;", "main := refl;", "main := #.Outer.N.refl;"};
	for (size_t i = 0; i < sizeof(missing) / sizeof(*missing); ++i)
		complete(synthesis, request(synthesis, scope, missing[i]), PG_SYNTHESIS_REJECTED);
	const char *shadowed[] = {"main := \\N:@ => N.refl;", "{{ N:=@; main:=N.refl; }}.main;"};
	for (size_t i = 0; i < sizeof(shadowed) / sizeof(*shadowed); ++i)
		complete(synthesis, request(synthesis, scope, shadowed[i]), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_source_scope *shadow = pg_synthesis_namespace(synthesis, scope, name, root);
	complete(synthesis, request(synthesis, shadow, "main := N.refl;"), PG_SYNTHESIS_REJECTED);
	assert(complete(synthesis, request(synthesis, scope, "main := N.refl;"), PG_SYNTHESIS_DONE) == library->reflexivity);
	shadow = pg_synthesis_name(synthesis, scope, name, library->equality);
	complete(synthesis, request(synthesis, shadow, "main := N.refl;"), PG_SYNTHESIS_UNSUPPORTED);
	assert(!pg_synthesis_namespace(synthesis, NULL, name, exports));
	assert(!pg_synthesis_namespace(synthesis, root, name, NULL));
	assert(!pg_synthesis_namespace(synthesis, root, (struct pg_token){0}, exports));
	assert(!pg_synthesis_namespace(synthesis, root, (struct pg_token){.kind = '@'}, exports));
	const struct pg_evidence *empty = pg_prove_empty_context(synthesis->typing);
	const struct pg_object *binder = pg_binder(synthesis->typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(synthesis->typing, empty, binder,
		pg_prove_universe(synthesis->typing, synthesis->classifiers, empty, 0));
	const struct pg_source_scope *open = pg_synthesis_bind(synthesis, root, name, binder, context);
	assert(open && !pg_synthesis_namespace(synthesis, root, name, open));
	assert(pg_synthesis_namespace(synthesis, open, intrinsic, exports));
	struct pg_synthesis other;
	assert(pg_synthesis_init(&other, synthesis->typing, synthesis->classifiers, synthesis->normalization,
		PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *foreign = pg_synthesis_root(&other);
	assert(!pg_synthesis_namespace(synthesis, root, name, foreign));
	assert(!pg_synthesis_namespace(synthesis, foreign, name, exports));
	pg_synthesis_destroy(&other);
	/* Qualified paths are traversed without recursive C calls or Core nodes. */
	size_t depth = 4096;
	char *source = pg_alloc(synthesis->typing->graph, depth * 2 + 32);
	assert(source);
	size_t length = (size_t)sprintf(source, "main := ");
	const struct pg_source_scope *deep = exports;
	for (size_t i = 0; i < depth; ++i) {
		deep = pg_synthesis_namespace(synthesis, root, name, deep);
		assert(deep);
		source[length++] = 'N'; source[length++] = '.';
	}
	strcpy(source + length, "refl;");
	assert(complete(synthesis, request(synthesis, deep, source), PG_SYNTHESIS_DONE) == library->reflexivity);
	puts("namespaces: shared evidence, qualified Identity, lexical shadowing, closed exports and deep paths passed");
}

static void module_namespaces(struct pg_synthesis *synthesis, const struct pg_source_scope *ambient)
{
	struct pg_graph *graph = synthesis->typing->graph;
	const struct pg_source_scope *root = pg_synthesis_root(synthesis);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "Library", .length = 7};
	struct pg_token proof_name = {.kind = PG_TOKEN_IDENT, .text = "proof", .length = 5};
	const char *source = "eq:=&Eq; proof:=&refl; id:=&(\\A:@ => \\x:A => x);";
	struct pg_parser parser;
	pg_parser_init(&parser, graph, source, strlen(source));
	const struct pg_syntax *definitions = pg_parser_program(&parser);
	assert(definitions);
	struct pg_synthesis_job *module = pg_synthesis_request(synthesis, ambient, definitions);
	size_t terms = graph->terms.count, proofs = synthesis->typing->proofs.count, jobs = synthesis->jobs.count;
	uint64_t steps = synthesis->steps;
	const struct pg_source_scope *scope = pg_synthesis_module_namespace(synthesis, root, name, module);
	assert(scope && pg_synthesis_module_namespace(synthesis, root, name, module) == scope);
	assert(graph->terms.count == terms && synthesis->typing->proofs.count == proofs);
	assert(synthesis->jobs.count == jobs && synthesis->steps == steps);
	struct pg_synthesis_job *reference = request(synthesis, scope, "main:=Library.proof;");
	pg_synthesis_advance(synthesis, 2);
	assert(pg_synthesis_status(reference) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_dependency(reference) && pg_synthesis_dependency(reference) == pg_synthesis_dependency(module));
	const struct pg_evidence *answer = complete(synthesis, reference, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(module) == PG_SYNTHESIS_DONE);
	assert(answer == pg_synthesis_result(pg_synthesis_definition(module, proof_name)));
	struct pg_synthesis_job *selection = pg_synthesis_request(synthesis, ambient,
		select_definition(graph, definitions, proof_name));
	assert(pg_synthesis_module_namespace(synthesis, root, name, selection) == scope);
	assert(complete(synthesis, selection, PG_SYNTHESIS_DONE) == answer);
	complete(synthesis, request(synthesis, scope,
		"main:=\\A:@ => \\x:A => Library.proof A x :: Library.eq A x x;"), PG_SYNTHESIS_DONE);
	complete(synthesis, request(synthesis, scope, "main:=Library.refl;"), PG_SYNTHESIS_REJECTED);
	complete(synthesis, request(synthesis, scope, "main:=refl;"), PG_SYNTHESIS_REJECTED);
	complete(synthesis, request(synthesis, scope, "main:=\\Library:@ => Library.proof;"), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *consumer_module = program(synthesis, scope, "alias:=Library.id;");
	struct pg_token consumer_name = {.kind = PG_TOKEN_IDENT, .text = "Consumer", .length = 8};
	const struct pg_source_scope *consumer = pg_synthesis_module_namespace(synthesis, root, consumer_name, consumer_module);
	assert(consumer);
	const struct pg_evidence *call = complete(synthesis, request(synthesis, consumer,
		"main:=\\A:@ => \\x:A => Consumer.alias A x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *empty = pg_prove_empty_context(synthesis->typing);
	call = complete(synthesis, pg_synthesis_nf(synthesis, empty, call), PG_SYNTHESIS_DONE);
	const struct pg_evidence *identity = complete(synthesis, request(synthesis, root,
		"main:=\\A:@ => \\x:A => x;"), PG_SYNTHESIS_DONE);
	same_judgement(call, identity);
	complete(synthesis, request(synthesis, consumer, "main:=Consumer.Library.id;"), PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_module_namespace(synthesis, root, name, NULL));
	assert(!pg_synthesis_module_namespace(synthesis, NULL, name, module));
	assert(!pg_synthesis_module_namespace(synthesis, root, (struct pg_token){0}, module));
	assert(!pg_synthesis_module_namespace(synthesis, root, name, reference));
	assert(!pg_synthesis_module_namespace(synthesis, root, name, pg_synthesis_evidence(synthesis, answer)));
	pg_parser_init(&parser, graph, source, strlen(source));
	const struct pg_syntax *fresh_definitions = pg_parser_program(&parser);
	assert(fresh_definitions);
	struct pg_synthesis_job *fresh_selection = pg_synthesis_request(synthesis, ambient,
		select_definition(graph, fresh_definitions, proof_name));
	jobs = synthesis->jobs.count;
	assert(!pg_synthesis_module_namespace(synthesis, root, (struct pg_token){0}, fresh_selection));
	assert(!pg_synthesis_module_namespace(synthesis, NULL, name, fresh_selection));
	assert(synthesis->jobs.count == jobs);
	complete(synthesis, fresh_selection, PG_SYNTHESIS_DONE);
	const struct pg_object *binder = pg_binder(graph);
	const struct pg_evidence *context = pg_prove_context_extension(synthesis->typing, empty, binder,
		pg_prove_universe(synthesis->typing, synthesis->classifiers, empty, 0));
	const struct pg_source_scope *open = pg_synthesis_bind(synthesis, root, name, binder, context);
	struct pg_synthesis_job *open_module = program(synthesis, open, "alias:=Library;");
	assert(!pg_synthesis_module_namespace(synthesis, root, name, open_module));
	complete(synthesis, open_module, PG_SYNTHESIS_DONE);
	struct pg_synthesis foreign;
	assert(pg_synthesis_init(&foreign, synthesis->typing, synthesis->classifiers, synthesis->normalization,
		PG_DEFINITION_EXPLICIT_THUNK) == 0);
	assert(!pg_synthesis_module_namespace(&foreign, pg_synthesis_root(&foreign), name, module));
	assert(!pg_synthesis_module_namespace(synthesis, pg_synthesis_root(&foreign), name, module));
	pg_synthesis_destroy(&foreign);
	module = program(synthesis, root, "good:=@; bad:=missing;");
	scope = pg_synthesis_module_namespace(synthesis, root, name, module);
	assert(scope);
	complete(synthesis, request(synthesis, scope, "main:=Library.good;"), PG_SYNTHESIS_REJECTED);
	module = program(synthesis, root, "good:=@; a:=b; b:=a;");
	scope = pg_synthesis_module_namespace(synthesis, root, name, module);
	assert(scope);
	reference = request(synthesis, scope, "main:=Library.good;");
	complete(synthesis, request(synthesis, scope, "main:=Library.absent;"), PG_SYNTHESIS_REJECTED);
	pg_synthesis_advance(synthesis, 1000);
	assert(pg_synthesis_status(reference) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(reference));
	assert(!pg_synthesis_result(reference) && !synthesis->ready);
	steps = synthesis->steps;
	pg_synthesis_advance(synthesis, 1000);
	assert(synthesis->steps == steps);
	puts("source module namespaces: pending imports, shared proofs, lexical exports and whole-module validity passed");
}

static void named_identity(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis), *scope = root;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_identity_library *library = pg_identity_library(typing, classifiers, &work, 0);
	assert(library);
	size_t library_work = work.jobs.count;
	const struct pg_evidence *functions[] = {library->equality, library->reflexivity, library->symmetry, library->composition,
		library->congruence};
	/* These are ordinary checked functions, not special source-name rules. */
	const char *names[] = {"Eq", "refl", "sym", "trans", "ap"};
	for (size_t n = 0; n < sizeof(functions) / sizeof(*functions); ++n) {
		scope = pg_synthesis_name(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[n], .length = strlen(names[n])}, functions[n]);
		assert(scope);
	}
	assert(!synthesis.steps && !synthesis.ready && work.jobs.count == library_work);
	namespaces(&synthesis, scope, library);
	module_namespaces(&synthesis, scope);
	const struct pg_evidence *named = complete(&synthesis, request(&synthesis, scope, "main := refl;"), PG_SYNTHESIS_DONE);
	assert(named == functions[1]);
	const char *sources[] = {
		"main := \\A : @ => \\x : A => refl A x :: Eq A x x;",
		"main := \\A : @ => \\x : A => \\p : Eq A x x => refl (Eq A x x) p :: Eq (Eq A x x) p p;",
		"{{ duplicate := &refl; proof := &(\\A : @ => \\x : A => duplicate A x :: Eq A x x); }}.proof;"
	};
	for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
		const struct pg_evidence *answer = complete(&synthesis, request(&synthesis, scope, sources[i]), PG_SYNTHESIS_DONE);
		struct pg_synthesis_job *bulk = request(&synthesis, scope, sources[i]);
		pg_synthesis_advance(&synthesis, 10000);
		assert(pg_synthesis_status(bulk) == PG_SYNTHESIS_DONE);
		same_judgement(answer, pg_synthesis_result(bulk));
		if (i == 0) {
			const struct pg_evidence *nf = complete(&synthesis,
				pg_synthesis_nf(&synthesis, empty, answer), PG_SYNTHESIS_DONE);
			assert(pg_alpha_equal(pg_evidence_subject(nf)->core, pg_evidence_subject(functions[1])->core) == 1);
		}
	}
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A : @ => \\x : A => \\y : A => refl A x :: Eq A x y;"), PG_SYNTHESIS_REJECTED);
	const char *path_functions[] = {
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => sym A x y p :: Eq A y x;",
		"main := \\A:@ => \\x:A => \\y:A => \\z:A => \\p:Eq A x y => \\q:Eq A y z => trans A x y z p q :: Eq A x z;"
	};
	for (size_t i = 0; i < sizeof(path_functions) / sizeof(*path_functions); ++i) {
		const struct pg_evidence *result = complete(&synthesis, request(&synthesis, scope, path_functions[i]), PG_SYNTHESIS_DONE);
		const struct pg_evidence *expected = functions[i + 2];
		assert(pg_alpha_equal(pg_evidence_classifier(result), pg_evidence_classifier(expected)) == 1);
		result = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, result), PG_SYNTHESIS_DONE);
		expected = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, expected), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_subject(result)->core, pg_evidence_subject(expected)->core) == 1);
	}
	const char *diagonals[] = {
		"main := \\A:@ => \\x:A => sym A x x (refl A x);",
		"main := \\A:@ => \\x:A => trans A x x x (refl A x) (refl A x);"
	};
	for (size_t i = 0; i < sizeof(diagonals) / sizeof(*diagonals); ++i) {
		const struct pg_evidence *result = complete(&synthesis, request(&synthesis, scope, diagonals[i]), PG_SYNTHESIS_DONE);
		result = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, result), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_subject(result)->core, pg_evidence_subject(library->reflexivity)->core) == 1);
	}
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => sym A x y p :: Eq A x y;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A:@ => \\x:A => \\y:A => \\z:A => \\p:Eq A x y => \\q:Eq A x z => trans A x y z p q;"), PG_SYNTHESIS_REJECTED);
	const char *actions[] = {
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => ap A A (&(\\t:A => t)) x y p :: Eq A x y;",
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => ap A A (&(\\t:A => x)) x y p :: Eq A x x;"
	};
	const char *action_results[] = {
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => p;",
		"main := \\A:@ => \\x:A => \\y:A => \\p:Eq A x y => refl A x;"
	};
	for (size_t i = 0; i < sizeof(actions) / sizeof(*actions); ++i) {
		const struct pg_evidence *result = complete(&synthesis, request(&synthesis, scope, actions[i]), PG_SYNTHESIS_DONE);
		const struct pg_evidence *expected = complete(&synthesis, request(&synthesis, scope, action_results[i]), PG_SYNTHESIS_DONE);
		result = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, result), PG_SYNTHESIS_DONE);
		expected = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, expected), PG_SYNTHESIS_DONE);
		same_judgement(result, expected);
	}
	/* For an unknown f, do not fabricate returned endpoints or a value proof. */
	const struct pg_evidence *generic = complete(&synthesis, request(&synthesis, scope,
		"main := \\A:@ => \\B:@ => \\f:A->B => \\x:A => \\y:A => \\p:Eq A x y => ap A B f x y p;"), PG_SYNTHESIS_DONE);
	const struct pg_term *generic_type = pg_evidence_classifier(generic), *domain;
	const struct pg_object *arguments[6];
	for (size_t i = 0; i < 6; ++i) assert(pg_pi_view(generic_type, &domain, &arguments[i], &generic_type));
	const struct pg_term *f = pg_application(typing->graph, pg_reference(typing->graph, &pg_force_operation),
		pg_reference(typing->graph, arguments[2]));
	const struct pg_term *left = pg_application(typing->graph, f, pg_reference(typing->graph, arguments[3]));
	const struct pg_term *right = pg_application(typing->graph, f, pg_reference(typing->graph, arguments[4]));
	const struct pg_term *expected_type = pg_identity_instance(typing->graph,
		pg_identity_action(typing->graph, pg_return_type(classifiers, pg_reference(typing->graph, arguments[1]))), left, right);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, generic_type, expected_type) == 0);
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A:@ => \\x:A => \\y:A => ap A A (&(\\t:A => t)) x y (refl A x);"), PG_SYNTHESIS_REJECTED);
	const struct pg_identity_library *higher = pg_identity_library(typing, classifiers, &work, 1);
	assert(higher);
	scope = pg_synthesis_name(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Eq1", .length = 3}, higher->equality);
	scope = pg_synthesis_name(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "refl1", .length = 5}, higher->reflexivity);
	scope = pg_synthesis_name(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "trr", .length = 3}, library->transport[PG_IDENTITY_RIGHT]);
	assert(scope);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A : @ => refl1 (@) A :: Eq1 (@) A A;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *round_trip = complete(&synthesis, request(&synthesis, scope,
		"main := \\A : @ => \\x : A => trr A A (refl1 (@) A) x :: A;"), PG_SYNTHESIS_DONE);
	round_trip = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, round_trip), PG_SYNTHESIS_DONE);
	const struct pg_evidence *identity = complete(&synthesis, request(&synthesis, root,
		"main := \\A : @ => \\x : A => x;"), PG_SYNTHESIS_DONE);
	same_judgement(round_trip, identity);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\A : @ => refl (@) A;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, root, "main := refl;"), PG_SYNTHESIS_REJECTED);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "refl", .length = 4};
	const struct pg_source_scope *shadow = pg_synthesis_name(&synthesis, scope, name, functions[0]);
	assert(shadow);
	assert(complete(&synthesis, request(&synthesis, shadow, "main := refl;"), PG_SYNTHESIS_DONE) == functions[0]);
	assert(complete(&synthesis, request(&synthesis, scope, "main := refl;"), PG_SYNTHESIS_DONE) == functions[1]);
	const struct pg_object *binder = pg_binder(typing->graph);
	const struct pg_evidence *open = pg_prove_context_extension(typing, empty, binder,
		pg_prove_universe(typing, classifiers, empty, 0));
	assert(!pg_synthesis_name(&synthesis, root, name, pg_prove_variable(typing, open, binder)));
	assert(!pg_synthesis_name(&synthesis, root, name, empty));
	assert(!pg_synthesis_name(&synthesis, root, (struct pg_token){0}, functions[0]));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_synthesis_name(&synthesis, root, name,
		pg_prove_universe(&foreign, classifiers, pg_prove_empty_context(&foreign), 0)));
	pg_typing_destroy(&foreign);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("source Identity: checked named functions, dependent annotations, higher reflexivity and post-check rejection passed");
}

static void library_levels(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(!pg_identity_library(NULL, classifiers, &work, 0));
	assert(!pg_identity_library(typing, NULL, &work, 0));
	assert(!pg_identity_library(typing, classifiers, NULL, 0));
	assert(!pg_identity_library(typing, classifiers, &work, UINT64_MAX));
	struct pg_graph foreign_graph = {0};
	struct pg_classifiers foreign;
	assert(pg_classifiers_init(&foreign, &foreign_graph) == 0);
	assert(!pg_identity_library(typing, &foreign, &work, 0));
	struct pg_whnf_work foreign_work;
	assert(pg_whnf_work_init(&foreign_work, &foreign_graph) == 0);
	assert(!pg_identity_library(typing, classifiers, &foreign_work, 0));
	pg_whnf_work_destroy(&foreign_work);
	pg_classifiers_destroy(&foreign);
	pg_graph_destroy(&foreign_graph);
	for (uint64_t level = 0; level < 3; ++level) {
		const struct pg_identity_library *library = pg_identity_library(typing, classifiers, &work, level);
		const struct pg_identity_library *copy = pg_identity_library(typing, classifiers, &work, level);
		assert(library && copy);
		const struct pg_evidence *exports[] = {library->equality, library->reflexivity, library->instance,
			library->transport[0], library->transport[1], library->lifting[0], library->lifting[1],
			library->symmetry, library->composition, library->congruence};
		const struct pg_evidence *copies[] = {copy->equality, copy->reflexivity, copy->instance,
			copy->transport[0], copy->transport[1], copy->lifting[0], copy->lifting[1], copy->symmetry, copy->composition, copy->congruence};
		for (size_t i = 0; i < sizeof(exports) / sizeof(*exports); ++i) {
			const struct pg_evidence *proof = exports[i];
			assert(pg_evidence_owned_by(proof, typing) && !pg_evidence_context(proof));
			assert(pg_evidence_rule(proof) == PG_LAMBDA_INTRO);
			const struct pg_evidence *pi = pg_evidence_premise(proof, 0);
			const struct pg_evidence *domain = pg_prove_pi_domain(typing, pi);
			uint64_t actual;
			assert(pg_universe_level(pg_evidence_subject(domain)->core, &actual) && actual == level);
			same_judgement(proof, copies[i]);
			assert(pg_evidence_subject(proof)->core != pg_evidence_subject(copies[i])->core);
		}
	}
	pg_whnf_work_destroy(&work);
}

static void named_transport(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *scope = pg_synthesis_root(&synthesis);
	const struct pg_identity_library *library = pg_identity_library(typing, classifiers, &work, 0);
	assert(library);
	scope = pg_synthesis_name(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "instance", .length = 8}, library->instance);
	assert(scope);
	const struct pg_evidence *empty = pg_prove_empty_context(typing), *contexts[4] = {empty};
	const struct pg_object *bindings[6];
	for (size_t i = 0; i < 6; ++i) bindings[i] = pg_binder(typing->graph);
	for (size_t i = 0; i < 2; ++i) contexts[i + 1] = pg_prove_context_extension(typing,
		contexts[i], bindings[i], pg_prove_universe(typing, classifiers, contexts[i], 0));
	const struct pg_evidence *relation = pg_prove_identity_type(typing,
		pg_prove_universe(typing, classifiers, contexts[2], 0),
		pg_prove_variable(typing, contexts[2], bindings[0]), pg_prove_variable(typing, contexts[2], bindings[1]));
	contexts[3] = pg_prove_context_extension(typing, contexts[2], bindings[2], relation);
	const char *fields[] = {"trr", "trl", "liftr", "liftl"};
	for (size_t i = 0; i < 4; ++i) {
		enum pg_identity_direction direction = i % 2 ? PG_IDENTITY_LEFT : PG_IDENTITY_RIGHT;
		const struct pg_evidence *domain = pg_prove_variable(typing, contexts[3], bindings[i % 2]);
		const struct pg_evidence *context = pg_prove_context_extension(typing, contexts[3], bindings[4], domain);
		const struct pg_evidence *family = pg_prove_variable(typing, context, bindings[2]);
		const struct pg_evidence *value = pg_prove_variable(typing, context, bindings[4]);
		const struct pg_evidence *field = i < 2 ? pg_prove_identity_transport(typing, classifiers, family, value, direction)
			: pg_prove_identity_lift(typing, classifiers, family, value, direction);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, field);
		assert(body && pg_prove_abstract(typing, classifiers, context, context, body) == body);
		const struct pg_evidence *function = pg_prove_abstract(typing, classifiers, empty, context, body);
		assert(function);
		const struct pg_evidence *export = i < 2 ? library->transport[direction] : library->lifting[direction];
		same_judgement(function, export);
		const struct pg_evidence *partial = pg_prove_abstract(typing, classifiers, contexts[2], context, body);
		assert(pg_prove_abstract(typing, classifiers, empty, contexts[2], partial) == function);
		size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
		assert(pg_prove_abstract(typing, classifiers, empty, context, body) == function);
		assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
		assert(!pg_prove_abstract(typing, classifiers, empty, contexts[3], body));
		assert(!pg_prove_abstract(typing, classifiers, empty, context, value));
		assert(!pg_prove_abstract(typing, classifiers, context, contexts[3], partial));
		const struct pg_evidence *unrelated = pg_prove_context_extension(typing, contexts[3], pg_binder(typing->graph), domain);
		assert(!pg_prove_abstract(typing, classifiers, unrelated, context, body));
		scope = pg_synthesis_name(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = fields[i], .length = strlen(fields[i])}, export);
		assert(scope);
	}
	const char *names[] = {"A", "B", "r", "s", "x", "y"};
	const struct pg_evidence *context = empty;
	for (size_t i = 0; i < 6; ++i) {
		if (i < 3) context = contexts[i + 1];
		else {
			const struct pg_evidence *type = i == 3 ? pg_prove_projection(typing, context, relation)
				: pg_prove_variable(typing, context, bindings[i - 4]);
			context = pg_prove_context_extension(typing, context, bindings[i], type);
		}
		scope = pg_synthesis_bind(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = strlen(names[i])}, bindings[i], context);
		assert(scope);
	}
	const char *sources[] = {"main := trr A B r x :: B;", "main := trl A B r y :: A;",
		"main := liftr A B r x :: instance A B r x (trr A B r x);",
		"main := liftl A B r y :: instance A B r (trl A B r y) y;"};
	const struct pg_evidence *values[4];
	for (size_t i = 0; i < 4; ++i) {
		const struct pg_evidence *result = complete(&synthesis, request(&synthesis, scope, sources[i]), PG_SYNTHESIS_DONE);
		values[i] = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
		const struct pg_evidence *r = pg_prove_variable(typing, context, bindings[2]);
		const struct pg_evidence *x = pg_prove_variable(typing, context, bindings[4 + i % 2]);
		enum pg_identity_direction direction = i % 2 ? PG_IDENTITY_LEFT : PG_IDENTITY_RIGHT;
		const struct pg_evidence *expected = i < 2 ? pg_prove_identity_transport(typing, classifiers, r, x, direction)
			: pg_prove_identity_lift(typing, classifiers, r, x, direction);
		const struct pg_evidence *normal = complete(&synthesis, pg_synthesis_nf(&synthesis, context, values[i]), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_subject(normal)->core, pg_evidence_subject(expected)->core) == 1);
		struct pg_conversion comparison;
		assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(normal), pg_evidence_classifier(expected)) == 0);
		assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
		assert(pg_prove_conversion(typing, normal, pg_prove_classifier(typing, classifiers, context, expected),
			pg_conversion_certificate(&comparison)));
		pg_conversion_destroy(&comparison);
	}
	const struct pg_evidence *selected = complete(&synthesis, request(&synthesis, scope,
		"main := \\p : instance A B r x y => p;"), PG_SYNTHESIS_DONE);
	const struct pg_term *domain, *codomain;
	const struct pg_object *selected_binder;
	assert(pg_pi_view(pg_evidence_classifier(selected), &domain, &selected_binder, &codomain));
	const struct pg_evidence *selected_type = pg_prove_identity_instance(typing, classifiers,
		pg_prove_variable(typing, context, bindings[2]), pg_prove_variable(typing, context, bindings[4]),
		pg_prove_variable(typing, context, bindings[5]));
	assert(domain == pg_evidence_subject(selected_type)->core);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\p : instance A B r x y => p :: instance A B s x y;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := liftr A B r x :: instance A B s x (trr A B r x);"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := instance A B r y x;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := instance A B (&(\\a : A => \\b : B => A)) x y;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *other = complete(&synthesis, request(&synthesis, scope, "main := trr A B s x;"), PG_SYNTHESIS_DONE);
	other = complete(&synthesis, pg_synthesis_return(&synthesis, context, other), PG_SYNTHESIS_DONE);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(values[0])->core, pg_evidence_subject(other)->core) == 0);
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&comparison);
	const struct pg_evidence *diagonal = pg_prove_reflexivity(typing,
		pg_prove_universe(typing, classifiers, context, 0), pg_prove_variable(typing, context, bindings[0]));
	scope = pg_synthesis_name(&synthesis, scope, (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "diagonal", .length = 8}, diagonal);
	assert(scope);
	const struct pg_evidence *result = complete(&synthesis, request(&synthesis, scope, "main := trr A A diagonal x :: A;"), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_nf(&synthesis, context, result), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(result)->core == pg_reference(typing->graph, bindings[4]));
	complete(&synthesis, request(&synthesis, scope, "main := trr B A r y;"), PG_SYNTHESIS_REJECTED);
	/* The action's classifier computes to Pi; its source term stays unevaluated
	 * during application synthesis, just as any other checked callee does. */
	const struct pg_evidence *identity = complete(&synthesis, request(&synthesis, scope,
		"main := \\T : @ => T;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *acted = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, context, identity), identity);
	assert(acted);
	scope = pg_synthesis_name(&synthesis, scope, (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "acted", .length = 5}, acted);
	assert(scope);
	struct pg_whnf_job *untouched = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(acted)->core);
	result = complete(&synthesis, request(&synthesis, scope, "main := acted A B r;"), PG_SYNTHESIS_DONE);
	assert(pg_whnf_steps(untouched) == 0 && pg_whnf_status(untouched) == PG_EVAL_PENDING);
	result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_nf(&synthesis, context, result), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(result)->core == pg_reference(typing->graph, bindings[2]));
	const struct pg_evidence *wrapped = acted;
	for (size_t i = 0; i < 3; ++i) {
		wrapped = i == 1 ? pg_prove_return(typing, classifiers, wrapped) : pg_prove_thunk(typing, classifiers, wrapped);
		assert(wrapped);
		scope = pg_synthesis_name(&synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "wrapped", .length = 7}, wrapped);
		untouched = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(wrapped)->core);
		result = complete(&synthesis, request(&synthesis, scope, "main := wrapped A B r;"), PG_SYNTHESIS_DONE);
		assert(pg_whnf_steps(untouched) == 0);
		result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
		result = complete(&synthesis, pg_synthesis_nf(&synthesis, context, result), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(result)->core == pg_reference(typing->graph, bindings[2]));
	}
	const struct pg_evidence *returned_a = pg_prove_return(typing, classifiers, pg_prove_variable(typing, context, bindings[0]));
	const struct pg_evidence *computed_path = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, context, returned_a), returned_a);
	scope = pg_synthesis_name(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "computed_path", .length = 13}, computed_path);
	assert(scope);
	untouched = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(computed_path)->core);
	const char *path_blocks[] = {
		"main := { v := computed_path; v; };",
		"main := { computed_path; diagonal; };",
		"main := { v := { inner := computed_path; inner; }; v; };",
		"main := { v := computed_path; missing; }.v;"
	};
	for (size_t i = 0; i < sizeof(path_blocks) / sizeof(*path_blocks); ++i) {
		result = complete(&synthesis, request(&synthesis, scope, path_blocks[i]), PG_SYNTHESIS_DONE);
		if (!i) {
			assert(pg_whnf_steps(untouched) == 0 && pg_whnf_status(untouched) == PG_EVAL_PENDING);
			assert(pg_evidence_rule(result) == PG_FOLD_ELIM);
			assert(pg_evidence_subject(pg_evidence_premise(result, 0))->core == pg_evidence_subject(computed_path)->core);
		}
		result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
		result = complete(&synthesis, pg_synthesis_nf(&synthesis, context, result), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(result)->core == pg_identity_action(typing->graph, pg_reference(typing->graph, bindings[0])));
	}
	result = complete(&synthesis, request(&synthesis, scope, "main := acted A A computed_path;"), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_nf(&synthesis, context, result), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(result)->core == pg_identity_action(typing->graph, pg_reference(typing->graph, bindings[0])));
	/* Sequencing exposes a classifier, not the contents of a quoted value. */
	result = complete(&synthesis, request(&synthesis, scope, "main := { v := &computed_path; v; };"), PG_SYNTHESIS_DONE);
	result = complete(&synthesis, pg_synthesis_return(&synthesis, context, result), PG_SYNTHESIS_DONE);
	const struct pg_term *content;
	assert(pg_thunk_type_view(pg_evidence_classifier(result), &content));
	result = complete(&synthesis, pg_synthesis_unthunk(&synthesis, context, result), PG_SYNTHESIS_DONE);
	assert(pg_alpha_equal(pg_evidence_subject(result)->core, pg_evidence_subject(computed_path)->core) == 1);
	complete(&synthesis, request(&synthesis, scope, "main := { f := acted; f; };"), PG_SYNTHESIS_UNSUPPORTED);
	size_t jobs = synthesis.jobs.count;
	struct pg_synthesis_job *exposure = pg_synthesis_normalize_classifier(&synthesis, context, acted);
	assert(exposure && pg_synthesis_status(exposure) == PG_SYNTHESIS_DONE && synthesis.jobs.count == jobs);
	assert(pg_evidence_subject(pg_synthesis_result(exposure))->core == pg_evidence_subject(acted)->core);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static void fair_work(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *context = pg_prove_empty_context(typing);
	const struct pg_evidence *value = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, context, 0));
	const struct pg_evidence *input = pg_prove_return(typing, classifiers, value);
	for (size_t i = 0; i < 256; ++i) input = pg_prove_force(typing, pg_prove_thunk(typing, classifiers, input));
	assert(input);
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *scope = pg_synthesis_root(&synthesis);
	struct pg_synthesis_job *fast = request(&synthesis, scope, "main := @;");
	struct pg_synthesis_job *slow = pg_synthesis_normalize(&synthesis, context, input);
	assert(slow && pg_synthesis_normalize(&synthesis, context, input) == slow);
	pg_synthesis_advance(&synthesis, 16);
	assert(pg_synthesis_status(fast) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(slow) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(slow));
	/* Newly arriving work must also run while an older reduction is pending. */
	struct pg_synthesis_job *later = request(&synthesis, scope, "main := \\A : @ => A;");
	struct pg_synthesis_job *consumer = pg_synthesis_reflexivity(&synthesis, context, slow);
	pg_synthesis_advance(&synthesis, 32);
	assert(pg_synthesis_status(later) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(slow) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_dependency(consumer) == slow);
	size_t advances = 0;
	while (pg_synthesis_status(consumer) == PG_SYNTHESIS_PENDING) {
		assert(++advances < 10000);
		pg_synthesis_advance(&synthesis, 1);
	}
	assert(pg_synthesis_status(consumer) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(slow) == PG_SYNTHESIS_DONE && !synthesis.ready && !synthesis.ready_tail);
	const struct pg_evidence *split_result = pg_synthesis_result(consumer);
	const struct pg_evidence *expected = pg_prove_return(typing, classifiers, value);
	same_judgement(pg_synthesis_result(slow), expected);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 10000);
	assert(synthesis.steps == steps);
	complete(&synthesis, request(&synthesis, scope, "main := @;"), PG_SYNTHESIS_DONE);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	/* A cold store and a bulk budget reach the same accepted judgement. */
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	scope = pg_synthesis_root(&synthesis);
	fast = request(&synthesis, scope, "main := @;");
	slow = pg_synthesis_normalize(&synthesis, context, input);
	later = request(&synthesis, scope, "main := \\A : @ => A;");
	consumer = pg_synthesis_reflexivity(&synthesis, context, slow);
	pg_synthesis_advance(&synthesis, 10000);
	assert(pg_synthesis_status(fast) == PG_SYNTHESIS_DONE && pg_synthesis_status(later) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(consumer) == PG_SYNTHESIS_DONE && !synthesis.ready && !synthesis.ready_tail);
	same_judgement(pg_synthesis_result(consumer), split_result);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static void endpoint_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *context = pg_prove_empty_context(typing);
	const struct pg_evidence *type = pg_prove_universe(typing, classifiers, context, 1);
	const struct pg_evidence *value = pg_prove_type_value(typing, pg_prove_universe(typing, classifiers, context, 0));
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_evidence *next = pg_prove_identity_type(typing, type, value, value);
		value = pg_prove_reflexivity(typing, type, value);
		type = next;
		assert(type && value);
	}
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	struct pg_coordinate coordinates[] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}, {PG_AXIS, 1}};
	const struct pg_dimension_map *selector = pg_dimension_map(&dimensions, 2, 3, coordinates);
	const struct pg_evidence *expected = pg_identity_face_endpoint(typing, classifiers, context, type, 2, PG_IDENTITY_LEFT);
	assert(expected);
	for (unsigned split = 0; split < 2; ++split) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(pg_whnf_work_init(&work, typing->graph) == 0);
		assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
		struct pg_synthesis_job *job = pg_synthesis_identity_endpoint(&synthesis, context, type, selector);
		assert(job && pg_synthesis_identity_endpoint(&synthesis, context, type, selector) == job);
		assert(pg_synthesis_identity_face(&synthesis, context, type, selector) == job);
		assert(!pg_synthesis_identity_face(&synthesis, NULL, type, selector));
		assert(!pg_synthesis_result(job));
		pg_synthesis_advance(&synthesis, 0);
		assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
		struct pg_synthesis_job *consumer = pg_synthesis_reflexivity(&synthesis, context, job);
		for (unsigned i = 0; pg_synthesis_status(consumer) == PG_SYNTHESIS_PENDING; ++i) {
			assert(i < 100);
			pg_synthesis_advance(&synthesis, split ? 1 : 100);
		}
		assert(pg_synthesis_status(consumer) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_result(job) == expected);
		for (unsigned code = 0; code < 26; ++code) {
			struct pg_coordinate selected[3];
			size_t axes = 0;
			unsigned digits = code;
			for (size_t i = 0; i < 3; ++i, digits /= 3) {
				selected[i] = (struct pg_coordinate){PG_ENDPOINT_ZERO, 0};
				if (digits % 3 == 1) selected[i].kind = PG_ENDPOINT_ONE;
				if (digits % 3 == 2) selected[i] = (struct pg_coordinate){PG_AXIS, axes++};
			}
			const struct pg_dimension_map *face = pg_dimension_map(&dimensions, axes, 3, selected);
			struct pg_synthesis_job *face_job = pg_synthesis_identity_face(&synthesis, context, type, face);
			assert(face_job && pg_synthesis_identity_face(&synthesis, context, type, face) == face_job);
			if (face_job != job) assert(!pg_synthesis_result(face_job));
			for (unsigned i = 0; pg_synthesis_status(face_job) == PG_SYNTHESIS_PENDING; ++i) {
				assert(i < 100);
				pg_synthesis_advance(&synthesis, split ? 1 : 100);
			}
			assert(pg_synthesis_status(face_job) == PG_SYNTHESIS_DONE);
			assert(pg_synthesis_result(face_job) == pg_identity_proper_face(typing, classifiers, context, type, face));
		}
		assert(!pg_synthesis_identity_face(&synthesis, context, type, pg_dimension_identity(&dimensions, 3)));
		static const size_t permutations[6][3] = {
			{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
		};
		struct pg_synthesis_job *producer = pg_synthesis_evidence(&synthesis, type);
		for (size_t p = 0; p < 6; ++p) {
			struct pg_coordinate axes[3];
			for (size_t i = 0; i < 3; ++i) axes[i] = (struct pg_coordinate){PG_AXIS, permutations[p][i]};
			const struct pg_dimension_map *permutation = pg_dimension_map(&dimensions, 3, 3, axes);
			for (unsigned code = 0; code < 26; ++code) {
				struct pg_coordinate selected[3];
				size_t n = 0;
				unsigned digits = code;
				for (size_t i = 0; i < 3; ++i, digits /= 3)
					selected[i] = digits % 3 == 2 ? (struct pg_coordinate){PG_AXIS, n++}
						: (struct pg_coordinate){digits % 3 ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
				const struct pg_dimension_map *face = pg_dimension_map(&dimensions, n, 3, selected);
				const struct pg_dimension_map *ordered, *orientation, *intrinsic = NULL;
				const struct pg_dimension_map *composed = pg_dimension_compose(&dimensions, permutation, face);
				assert(pg_dimension_face_factor(&dimensions, composed, &ordered, &orientation) == 0);
				struct pg_synthesis_job *source = pg_synthesis_permutation_source_face(&synthesis,
					&dimensions, context, producer, permutation, face, &intrinsic);
				assert(source == pg_synthesis_identity_face_job(&synthesis, context, producer, ordered));
				assert(intrinsic == orientation && intrinsic->source < 3);
				assert(pg_dimension_compose(&dimensions, ordered, intrinsic) == composed);
				assert(pg_synthesis_status(source) == PG_SYNTHESIS_DONE);
			}
			const struct pg_dimension_map *unchanged = permutation;
			assert(!pg_synthesis_permutation_source_face(&synthesis, &dimensions, context, producer,
				permutation, pg_dimension_identity(&dimensions, 3), &unchanged));
			assert(unchanged == permutation);
		}
		struct pg_coordinate reversed[] = {{PG_AXIS, 1}, {PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
		complete(&synthesis, pg_synthesis_identity_face(&synthesis, context, type,
			pg_dimension_map(&dimensions, 2, 3, reversed)), PG_SYNTHESIS_UNSUPPORTED);
		const struct pg_evidence *roundtrip = pg_prove_value_type(typing, pg_prove_type_value(typing, type));
		assert(pg_synthesis_identity_endpoint(&synthesis, context, roundtrip, selector) != job);
		assert(!pg_synthesis_identity_endpoint(&synthesis, context, value, selector));
		assert(!pg_synthesis_identity_endpoint(&synthesis, context, type, pg_dimension_identity(&dimensions, 3)));
		struct pg_synthesis_job *unsupported = pg_synthesis_identity_endpoint(&synthesis, context,
			pg_prove_universe(typing, classifiers, context, 0), selector);
		complete(&synthesis, unsupported, PG_SYNTHESIS_UNSUPPORTED);
		pg_synthesis_advance(&synthesis, 100);
		assert(!synthesis.ready);
		/* Release workers while face validation is still pending. */
		coordinates[0].kind = PG_ENDPOINT_ONE;
		const struct pg_evidence *cancel_type = pg_prove_value_type(typing, pg_prove_type_value(typing, roundtrip));
		struct pg_synthesis_job *cancelled = pg_synthesis_identity_endpoint(&synthesis, context, cancel_type,
			pg_dimension_map(&dimensions, 2, 3, coordinates));
		pg_synthesis_advance(&synthesis, 1);
		assert(pg_synthesis_status(cancelled) == PG_SYNTHESIS_PENDING);
		struct pg_synthesis_job *cancelled_face = pg_synthesis_identity_face(&synthesis, context, cancel_type, selector);
		pg_synthesis_advance(&synthesis, 0);
		assert(pg_synthesis_status(cancelled_face) == PG_SYNTHESIS_PENDING);
		pg_synthesis_advance(&synthesis, 6);
		assert(!pg_synthesis_result(cancelled_face));
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	pg_dimensions_destroy(&dimensions);
}

static void substitution_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *x = pg_binder(typing->graph), *b = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 1));
	source = pg_prove_context_extension(typing, source, x,
		pg_prove_value_type(typing, pg_prove_variable(typing, source, a)));
	const struct pg_evidence *destination = pg_prove_context_extension(typing, empty, b,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *b_value = pg_prove_variable(typing, destination, b);
	const struct pg_evidence *a_value = pg_prove_type_value(typing,
		pg_prove_universe(typing, classifiers, destination, 0));
	const struct pg_evidence *computation = pg_prove_return(typing, classifiers, b_value);
	for (unsigned i = 0; i < 32; ++i)
		computation = pg_prove_force(typing, pg_prove_thunk(typing, classifiers, computation));
	for (unsigned split = 0; split < 2; ++split) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(pg_whnf_work_init(&work, typing->graph) == 0);
		assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
		struct pg_synthesis_job *images[] = {pg_synthesis_evidence(&synthesis, a_value),
			pg_synthesis_return(&synthesis, destination, computation)};
		struct pg_synthesis_job *job = pg_synthesis_substitution(&synthesis, source, destination, 2, images);
		assert(job && pg_synthesis_substitution(&synthesis, source, destination, 2, images) == job);
		pg_synthesis_advance(&synthesis, 8);
		assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(job));
		for (unsigned i = 0; pg_synthesis_status(job) == PG_SYNTHESIS_PENDING; ++i) {
			assert(i < 10000);
			pg_synthesis_advance(&synthesis, split ? 1 : 10000);
		}
		assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *map = pg_synthesis_result(job);
		assert(pg_evidence_rule(map) == PG_CONTEXT_SUBSTITUTION);
		assert(pg_evidence_premise(map, 0) == source && pg_evidence_premise(map, 1) == destination);
		const struct pg_evidence *reindexed = pg_prove_reindex(typing, map, pg_prove_variable(typing, source, x));
		same_judgement(reindexed, b_value);
		struct pg_synthesis_job *wrong[] = {images[1], images[0]};
		complete(&synthesis, pg_synthesis_substitution(&synthesis, source, destination, 2, wrong), PG_SYNTHESIS_REJECTED);
		assert(!pg_synthesis_substitution(&synthesis, source, destination, 1, images));
		complete(&synthesis, pg_synthesis_substitution(&synthesis, empty, destination, 0, NULL), PG_SYNTHESIS_DONE);
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
}

static void square_template_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, pg_binder(typing->graph),
		pg_prove_universe(typing, classifiers, empty, 1));
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 2);
	struct pg_coordinate axes[] = {{PG_AXIS, 1}, {PG_AXIS, 0}};
	const struct pg_evidence *original = pg_identity_cube_context(typing, &dimensions, source, 1, &cube,
		pg_dimension_identity(&dimensions, 2));
	const struct pg_evidence *opposite = pg_identity_cube_context(typing, &dimensions, source, 1, &cube,
		pg_dimension_map(&dimensions, 2, 2, axes));
	const struct pg_evidence *destination = pg_evidence_premise(original, 0);
	const struct pg_evidence *template = pg_evidence_premise(opposite, 0);
	const struct pg_evidence *cursor = template;
	struct pg_synthesis_job *images[8];
	for (size_t i = 8; i; --i, cursor = pg_evidence_premise(cursor, 0)) {
		const struct pg_binding_face *binding = pg_binding_face_view(pg_evidence_context(cursor)->binder);
		const struct pg_dimension_map *ordered, *intrinsic;
		assert(pg_dimension_face_factor(&dimensions, binding->face, &ordered, &intrinsic) == 0);
		assert(intrinsic == pg_dimension_identity(&dimensions, ordered->source));
		images[i - 1] = pg_synthesis_identity_face(&synthesis, destination,
			pg_evidence_premise(original, 1), ordered);
		assert(images[i - 1]);
		assert(!pg_synthesis_result(images[i - 1]));
	}
	struct pg_synthesis_job *map_job = pg_synthesis_substitution(&synthesis, template, destination, 8, images);
	struct pg_synthesis_job *formation = pg_synthesis_evidence(&synthesis, pg_evidence_premise(opposite, 1));
	struct pg_synthesis_job *type_job = pg_synthesis_reindex_jobs(&synthesis, map_job, formation);
	assert(type_job && pg_synthesis_reindex_jobs(&synthesis, map_job, formation) == type_job);
	assert(!pg_synthesis_result(map_job) && !pg_synthesis_result(type_job));
	struct pg_synthesis_job *recovery = pg_synthesis_identity_formation(&synthesis, type_job);
	assert(recovery && pg_synthesis_identity_formation(&synthesis, type_job) == recovery);
	assert(!pg_synthesis_result(recovery));
	struct pg_coordinate vertex[] = {{PG_ENDPOINT_ZERO, 0}, {PG_ENDPOINT_ONE, 0}};
	const struct pg_dimension_map *face = pg_dimension_map(&dimensions, 0, 2, vertex);
	struct pg_synthesis_job *selected = pg_synthesis_identity_face_job(&synthesis, destination, type_job, face);
	assert(selected && pg_synthesis_identity_face_job(&synthesis, destination, type_job, face) == selected);
	struct pg_coordinate swapped_axes[] = {{PG_AXIS, 1}, {PG_AXIS, 0}};
	struct pg_coordinate swapped_vertex[] = {{PG_ENDPOINT_ONE, 0}, {PG_ENDPOINT_ZERO, 0}};
	const struct pg_dimension_map *intrinsic = NULL;
	uint64_t before_request = synthesis.steps;
	assert(pg_synthesis_permutation_source_face(&synthesis, &dimensions, destination, type_job,
		pg_dimension_map(&dimensions, 2, 2, swapped_axes),
		pg_dimension_map(&dimensions, 0, 2, swapped_vertex), &intrinsic) == selected);
	assert(intrinsic == pg_dimension_identity(&dimensions, 0));
	assert(synthesis.steps == before_request && !pg_synthesis_result(selected));
	const struct pg_evidence *endpoint = complete(&synthesis, selected, PG_SYNTHESIS_DONE);
	const struct pg_evidence *type = pg_synthesis_result(type_job);
	assert(type && endpoint);
	const struct pg_evidence *recovered = pg_synthesis_result(recovery);
	assert(recovered && recovered == pg_identity_formation(typing, classifiers, type));
	struct pg_synthesis_job *canonical_recovery = pg_synthesis_identity_formation(&synthesis,
		pg_synthesis_evidence(&synthesis, type));
	assert(pg_synthesis_status(canonical_recovery) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(canonical_recovery) == recovered);
	struct pg_synthesis_job *selected_canonical = pg_synthesis_identity_face(&synthesis, destination, type, face);
	assert(pg_synthesis_status(selected_canonical) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(selected_canonical) == endpoint);
	struct pg_synthesis_job *recovered_face = pg_synthesis_identity_face(&synthesis, destination, recovered, face);
	assert(pg_synthesis_status(recovered_face) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(recovered_face) == endpoint);
	const struct pg_evidence *wrapped_type = pg_prove_value_type(typing, pg_prove_type_value(typing, type));
	assert(complete(&synthesis, pg_synthesis_identity_face(&synthesis, destination, wrapped_type, face),
		PG_SYNTHESIS_DONE) == endpoint);
	const struct pg_evidence *map = pg_synthesis_result(map_job);
	assert(map);
	struct pg_synthesis_job *canonical = pg_synthesis_reindex(&synthesis, map, pg_evidence_premise(opposite, 1));
	assert(pg_synthesis_status(canonical) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(canonical) == type);
	complete(&synthesis, pg_synthesis_reindex_jobs(&synthesis, formation, formation), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_identity_face_job(&synthesis, destination, map_job, face), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_identity_formation(&synthesis, map_job), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_identity_formation(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_universe(typing, classifiers, empty, 0))), PG_SYNTHESIS_UNSUPPORTED);
	assert(pg_evidence_context(type) == pg_evidence_context(destination));
	assert(pg_evidence_judgement(type) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(type) == pg_evidence_classifier(pg_evidence_premise(original, 1)));
	assert(!pg_context_lookup(pg_evidence_context(destination), pg_evidence_context(opposite)->binder));
	/* A fully degenerate square supplies all its boundaries without a center
	 * assumption. Test the transposed classifier independently of a symmetry rule. */
	const struct pg_evidence *closed_type = pg_prove_universe(typing, classifiers, empty, 1);
	const struct pg_evidence *closed_value = pg_prove_type_value(typing,
		pg_prove_universe(typing, classifiers, empty, 0));
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *next = pg_prove_identity_type(typing, closed_type, closed_value, closed_value);
		closed_value = pg_prove_reflexivity(typing, closed_type, closed_value);
		closed_type = next;
		assert(closed_type && closed_value);
	}
	cursor = template;
	for (size_t i = 8; i; --i, cursor = pg_evidence_premise(cursor, 0)) {
		const struct pg_binding_face *binding = pg_binding_face_view(pg_evidence_context(cursor)->binder);
		const struct pg_dimension_map *ordered, *intrinsic;
		assert(pg_dimension_face_factor(&dimensions, binding->face, &ordered, &intrinsic) == 0);
		assert(intrinsic == pg_dimension_identity(&dimensions, ordered->source));
		images[i - 1] = pg_synthesis_identity_face(&synthesis, empty, closed_type, ordered);
	}
	struct pg_synthesis_job *closed_map = pg_synthesis_substitution(&synthesis, template, empty, 8, images);
	struct pg_synthesis_job *target_type = pg_synthesis_reindex_jobs(&synthesis, closed_map, formation);
	struct pg_synthesis_job *center = pg_synthesis_evidence(&synthesis, closed_value);
	struct pg_synthesis_job *checked = pg_synthesis_expect(&synthesis, center, target_type);
	assert(checked && pg_synthesis_expect(&synthesis, center, target_type) == checked);
	assert(!pg_synthesis_result(target_type));
	const struct pg_evidence *checked_center = complete(&synthesis, checked, PG_SYNTHESIS_DONE);
	const struct pg_evidence *transposed_type = pg_synthesis_result(target_type);
	assert(pg_evidence_classifier(checked_center) == pg_evidence_subject(transposed_type)->core);
	assert(pg_evidence_subject(checked_center)->core == pg_evidence_subject(closed_value)->core);
	struct pg_synthesis_job *canonical_check = pg_synthesis_expect(&synthesis, center,
		pg_synthesis_evidence(&synthesis, transposed_type));
	assert(pg_synthesis_status(canonical_check) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(canonical_check) == checked_center);
	complete(&synthesis, pg_synthesis_expect(&synthesis, target_type, target_type), PG_SYNTHESIS_REJECTED);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void dependent_cube_substitution(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *x = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	source = pg_prove_context_extension(typing, source, x,
		pg_prove_value_type(typing, pg_prove_variable(typing, source, a)));
	for (size_t dimension = 1; dimension <= 2; ++dimension) {
		struct pg_whnf_work work;
		struct pg_synthesis synthesis;
		assert(pg_whnf_work_init(&work, typing->graph) == 0);
		assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
		const struct pg_binding_cube *cubes[] = {
			pg_binding_cube(&dimensions, dimension), pg_binding_cube(&dimensions, dimension)};
		struct pg_coordinate swap[] = {{PG_AXIS, 1}, {PG_AXIS, 0}};
		const struct pg_dimension_map *order = dimension == 1 ? pg_dimension_identity(&dimensions, 1)
			: pg_dimension_map(&dimensions, 2, 2, swap);
		const struct pg_evidence *original = pg_identity_cube_context(typing, &dimensions, source, 2, cubes,
			pg_dimension_identity(&dimensions, dimension));
		const struct pg_evidence *target = pg_identity_cube_context(typing, &dimensions, source, 2, cubes, order);
		assert(original && target);
		size_t count = dimension == 1 ? 6 : 18;
		struct pg_synthesis_job *images[18];
		const struct pg_evidence *cursor = target;
		for (size_t i = count; i; --i, cursor = pg_evidence_premise(cursor, 0)) {
			const struct pg_binding_face *binding = pg_binding_face_view(pg_evidence_context(cursor)->binder);
			const struct pg_dimension_map *ordered, *intrinsic;
			assert(pg_dimension_face_factor(&dimensions, binding->face, &ordered, &intrinsic) == 0);
			/* Deliberately omit the intrinsic center permutation. Geometry alone
			 * must not certify a substitution of the dependent telescope. */
			const struct pg_binding_face *image = pg_binding_face(&dimensions, binding->cube, ordered);
			const struct pg_evidence *value = pg_prove_variable(typing, original, &image->variable);
			assert(value);
			images[i - 1] = pg_synthesis_evidence(&synthesis, value);
		}
		assert(cursor == empty);
		struct pg_synthesis_job *map = pg_synthesis_substitution(&synthesis, target, original, count, images);
		if (dimension == 1) {
			const struct pg_evidence *result = complete(&synthesis, map, PG_SYNTHESIS_DONE);
			assert(pg_evidence_premise(result, 0) == target);
		} else {
			const struct pg_evidence *type_cube = target;
			for (size_t i = 0; i < 9; ++i) type_cube = pg_evidence_premise(type_cube, 0);
			complete(&synthesis, pg_synthesis_substitution(&synthesis,
				pg_evidence_premise(type_cube, 0), original, 8, images), PG_SYNTHESIS_DONE);
			complete(&synthesis, pg_synthesis_substitution(&synthesis,
				type_cube, original, 9, images), PG_SYNTHESIS_REJECTED);
			complete(&synthesis, map, PG_SYNTHESIS_REJECTED);
			assert(!pg_synthesis_result(map));
		}
		pg_synthesis_destroy(&synthesis);
		pg_whnf_work_destroy(&work);
	}
	pg_dimensions_destroy(&dimensions);
}

static void shared_conversion_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_evidence *context = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *x = pg_binder(typing->graph), *y = pg_binder(typing->graph);
	context = pg_prove_context_extension(typing, context, a, pg_prove_universe(typing, classifiers, context, 0));
	context = pg_prove_context_extension(typing, context, x,
		pg_prove_value_type(typing, pg_prove_variable(typing, context, a)));
	context = pg_prove_context_extension(typing, context, y,
		pg_prove_value_type(typing, pg_prove_variable(typing, context, a)));
	size_t before = synthesis.jobs.count;
	struct pg_synthesis_job *type = pg_synthesis_evidence(&synthesis,
		pg_prove_value_type(typing, pg_prove_variable(typing, context, a)));
	struct pg_synthesis_job *left = pg_synthesis_expect(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, x)), type);
	struct pg_synthesis_job *right = pg_synthesis_expect(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, y)), type);
	assert(synthesis.jobs.count == before + 5);
	const struct pg_evidence *left_result = complete(&synthesis, left, PG_SYNTHESIS_DONE);
	const struct pg_evidence *right_result = complete(&synthesis, right, PG_SYNTHESIS_DONE);
	/* Three evidence producers and two typed checks share one Core comparison. */
	assert(synthesis.jobs.count == before + 6);
	assert(left_result != right_result);
	assert(pg_evidence_subject(left_result)->core == pg_reference(typing->graph, x));
	assert(pg_evidence_subject(right_result)->core == pg_reference(typing->graph, y));
	before = synthesis.jobs.count;
	struct pg_synthesis_job *wrong_type = pg_synthesis_evidence(&synthesis,
		pg_prove_universe(typing, classifiers, context, 0));
	struct pg_synthesis_job *wrong_left = pg_synthesis_expect(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, x)), wrong_type);
	struct pg_synthesis_job *wrong_right = pg_synthesis_expect(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, y)), wrong_type);
	assert(synthesis.jobs.count == before + 3);
	pg_synthesis_advance(&synthesis, 0);
	assert(pg_synthesis_status(wrong_left) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_status(wrong_right) == PG_SYNTHESIS_PENDING);
	assert(!complete(&synthesis, wrong_left, PG_SYNTHESIS_REJECTED));
	assert(!complete(&synthesis, wrong_right, PG_SYNTHESIS_REJECTED));
	/* A failed comparison is also shared, but cannot retract accepted proofs. */
	assert(synthesis.jobs.count == before + 4);
	assert(pg_synthesis_result(left) == left_result);
	assert(pg_synthesis_result(right) == right_result);
	uint64_t steps = synthesis.steps;
	assert(pg_synthesis_expect(&synthesis,
		pg_synthesis_evidence(&synthesis, pg_prove_variable(typing, context, x)), wrong_type) == wrong_left);
	pg_synthesis_advance(&synthesis, 100);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static int arbitrary_policy(struct pg_eval *machine)
{
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	return argument ? pg_eval_enter(machine, *argument, 1) : 1;
}

static void function_eta(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *identity = complete(&synthesis,
		request(&synthesis, pg_synthesis_root(&synthesis), "id := \\x : @ => x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, empty, identity);
	const struct pg_object *binder = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, binder,
		pg_prove_thunk_type(typing, classifiers, pi));
	assert(context);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "f"};
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis,
		pg_synthesis_root(&synthesis), name, binder, context);
	const struct pg_evidence *f = pg_prove_force(typing, pg_prove_variable(typing, context, binder));
	const struct pg_evidence *eta = complete(&synthesis,
		request(&synthesis, scope, "eta := \\x : @ => f x;"), PG_SYNTHESIS_DONE);
	pi = pg_prove_projection(typing, context, pi);
	assert(f && eta && pi);
	struct pg_conversion conversion;
	assert(pg_conversion_init(&conversion, &work, pg_evidence_subject(f)->core, pg_evidence_subject(eta)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	const struct pg_evidence *refl = pg_prove_reflexivity(typing, pi, f);
	const struct pg_evidence *target = pg_prove_identity_type(typing, pi, eta, f);
	assert(refl && target);
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(refl), pg_evidence_subject(target)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *witness = pg_prove_conversion(typing, refl, target, pg_conversion_certificate(&conversion));
	assert(witness && pg_prove_classifier(typing, classifiers, context, witness));
	assert(pg_evidence_classifier(witness) == pg_evidence_subject(target)->core);
	pg_conversion_destroy(&conversion);
	/* Object Identity must not add an equation to Core conversion. */
	assert(pg_conversion_init(&conversion, &work, pg_evidence_subject(f)->core, pg_evidence_subject(eta)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&conversion);
	/* The same witness cannot establish equality with an unrelated function. */
	identity = pg_prove_projection(typing, context, identity);
	const struct pg_evidence *different = pg_prove_identity_type(typing, pi, identity, f);
	assert(different);
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(refl), pg_evidence_subject(different)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_prove_conversion(typing, refl, different, pg_conversion_certificate(&conversion)));
	pg_conversion_destroy(&conversion);
	/* A neutral raw callee still satisfies ap f (refl a) = refl (f a). */
	const struct pg_object *argument = pg_binder(typing->graph);
	const struct pg_evidence *domain = pg_prove_universe(typing, classifiers, context, 0);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, context, argument, domain);
	f = pg_prove_projection(typing, extended, f);
	pi = pg_prove_projection(typing, extended, pi);
	domain = pg_prove_projection(typing, extended, domain);
	const struct pg_evidence *a = pg_prove_variable(typing, extended, argument);
	const struct pg_evidence *pa = pg_prove_reflexivity(typing, domain, a);
	refl = pg_prove_reflexivity(typing, pi, f);
	const struct pg_evidence *expanded = pg_identity_pi_type(typing, classifiers, extended, pi, f, f,
		pg_binder(typing->graph), pg_binder(typing->graph), pg_binder(typing->graph));
	assert(expanded);
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(refl), pg_evidence_subject(expanded)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *ap = pg_prove_conversion(typing, refl, expanded, pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	const struct pg_evidence *early = normalize(&synthesis, extended, ap);
	early = pg_prove_application(typing, pg_prove_application(typing, pg_prove_application(typing, early, a), a), pa);
	ap = pg_prove_application(typing, pg_prove_application(typing, pg_prove_application(typing, ap, a), a), pa);
	const struct pg_evidence *fa = pg_prove_application(typing, f, a);
	const struct pg_evidence *expected = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, extended, fa), fa);
	const struct pg_evidence *actual = normalize(&synthesis, extended, ap);
	assert(expected && pg_evidence_subject(actual)->core == pg_evidence_subject(expected)->core);
	same_judgement(actual, normalize(&synthesis, extended, early));
	assert(pg_prove_classifier(typing, classifiers, extended, actual));
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(actual), pg_evidence_classifier(expected)) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
}

static void selected_instances(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *r = pg_binder(graph);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_evidence *ambient = pg_prove_context_extension(typing, empty, a, u);
	ambient = pg_prove_context_extension(typing, ambient, b, pg_prove_projection(typing, ambient, u));
	const struct pg_evidence *rt = pg_prove_identity_type(typing, pg_prove_projection(typing, ambient, u),
		pg_prove_variable(typing, ambient, a), pg_prove_variable(typing, ambient, b));
	ambient = pg_prove_context_extension(typing, ambient, r, rt);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, ambient, x,
		pg_prove_value_type(typing, pg_prove_variable(typing, ambient, a)));
	source = pg_prove_context_extension(typing, source, y,
		pg_prove_value_type(typing, pg_prove_variable(typing, source, b)));
	const struct pg_evidence *instance = pg_prove_identity_instance(typing, classifiers,
		pg_prove_variable(typing, source, r), pg_prove_variable(typing, source, x), pg_prove_variable(typing, source, y));
	const struct pg_evidence *input = pg_prove_type_value(typing, instance);
	const struct pg_binding_face *centers[2];
	for (size_t i = 0; i < 2; ++i) centers[i] = pg_binding_face(&dimensions,
		pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *ls, *rs, *paths[2];
	const struct pg_evidence *context = pg_identity_context(typing, &dimensions, source, 2, centers, &ls, &rs, paths);
	assert(input && context);
	struct pg_synthesis_job *producer = pg_synthesis_normalize(&split, source, input);
	struct pg_synthesis_job *other_producer = pg_synthesis_normalize(&whole, source, input);
	size_t terms = graph->terms.count, proofs = typing->proofs.count;
	struct pg_synthesis_job *job = pg_synthesis_family_action(&split, producer, ls, rs, 2, paths);
	struct pg_synthesis_job *other = pg_synthesis_family_action(&whole, other_producer, ls, rs, 2, paths);
	assert(job && other && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_family_action(&split, producer, ls, rs, 2, paths) == job);
	struct pg_synthesis_job *path_jobs[] = {pg_synthesis_evidence(&split, paths[0]), pg_synthesis_evidence(&split, paths[1])};
	assert(pg_synthesis_family_action_jobs(&split, producer, ls, rs, 2, path_jobs) == job);
	assert(graph->terms.count == terms && typing->proofs.count == proofs);
	wait_on(&split, job, producer);
	assert(pg_synthesis_status(producer) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *acted = complete(&split, job, PG_SYNTHESIS_DONE);
	pg_synthesis_advance(&whole, 100000);
	assert(pg_synthesis_status(other) == PG_SYNTHESIS_DONE);
	same_judgement(acted, pg_synthesis_result(other));
	const struct pg_evidence *kind = pg_prove_classifier(typing, classifiers, source, input);
	same_judgement(acted, pg_prove_family_action(typing, kind, input, ls, rs, 2, paths));
	/* Instantiation action exposes the selected R, never replaces it by A/B. */
	const struct pg_term *expected = pg_identity_action(graph, pg_reference(graph, r));
	for (size_t i = 0; i < 2; ++i) expected = pg_application(graph,
		pg_identity_instance(graph, expected, pg_evidence_subject(pg_evidence_premise(ls, i + 5))->core,
			pg_evidence_subject(pg_evidence_premise(rs, i + 5))->core), pg_evidence_subject(paths[i])->core);
	const struct pg_evidence *normalized = normalize(&split, context, acted);
	/* WHNF can retain beta/action redexes in neutral argument positions. */
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(normalized)->core, expected) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	const struct pg_evidence *family = complete(&split,
		pg_synthesis_normalize_classifier(&split, context, normalized), PG_SYNTHESIS_DONE);
	/* Its universe Identity can be instantiated again, forming a square. */
	const struct pg_evidence *lt = pg_prove_reindex(typing, ls, instance);
	const struct pg_evidence *rr = pg_prove_reindex(typing, rs, instance);
	const struct pg_object *w0 = pg_binder(graph), *w1 = pg_binder(graph);
	const struct pg_evidence *square_context = pg_prove_context_extension(typing, context, w0, lt);
	square_context = pg_prove_context_extension(typing, square_context, w1, pg_prove_projection(typing, square_context, rr));
	family = pg_prove_projection(typing, square_context, family);
	const struct pg_evidence *square = pg_prove_identity_instance(typing, classifiers, family,
		pg_prove_variable(typing, square_context, w0), pg_prove_variable(typing, square_context, w1));
	assert(square && pg_evidence_judgement(square) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(square)->core == pg_identity_instance(graph, pg_evidence_subject(normalized)->core,
		pg_reference(graph, w0), pg_reference(graph, w1)));
	assert(!pg_prove_identity_instance(typing, classifiers, family,
		pg_prove_variable(typing, square_context, w1), pg_prove_variable(typing, square_context, w0)));
	/* A different path with the same endpoints is a different action request. */
	const struct pg_object *alternative = pg_binder(graph);
	const struct pg_evidence *choices = pg_prove_context_extension(typing, context, alternative,
		pg_prove_classifier(typing, classifiers, context, paths[0]));
	const struct pg_evidence *li[5], *ri[5];
	for (size_t i = 0; i < 5; ++i) {
		li[i] = pg_prove_projection(typing, choices, pg_evidence_premise(ls, i + 2));
		ri[i] = pg_prove_projection(typing, choices, pg_evidence_premise(rs, i + 2));
	}
	const struct pg_evidence *cl = pg_prove_substitution(typing, source, choices, 5, li);
	const struct pg_evidence *cr = pg_prove_substitution(typing, source, choices, 5, ri);
	const struct pg_evidence *selected[] = {pg_prove_projection(typing, choices, paths[0]),
		pg_prove_projection(typing, choices, paths[1])};
	struct pg_synthesis_job *original = pg_synthesis_family_action(&split, producer, cl, cr, 2, selected);
	selected[0] = pg_prove_variable(typing, choices, alternative);
	assert(!pg_prove_family_action(typing, kind, input, cl, cr, 2, selected));
	struct pg_synthesis_job *unconverted = pg_synthesis_family_action(&split, producer, cl, cr, 2, selected);
	struct pg_synthesis_job *bulk_unconverted = pg_synthesis_family_action(&whole, other_producer, cl, cr, 2, selected);
	const struct pg_evidence *automatic = complete(&split, unconverted, PG_SYNTHESIS_DONE);
	pg_synthesis_advance(&whole, 100000);
	assert(pg_synthesis_status(bulk_unconverted) == PG_SYNTHESIS_DONE);
	same_judgement(automatic, pg_synthesis_result(bulk_unconverted));
	const struct pg_evidence *converted_path = pg_evidence_premise(pg_evidence_premise(automatic, 0), 4);
	assert(pg_evidence_rule(converted_path) == PG_TYPE_CONVERSION);
	assert(pg_evidence_premise(converted_path, 0) == selected[1]);
	/* The second path's family still mentions the first chosen path, even for
	 * a constant B. The solver builds the same explicit conversion as below;
	 * the kernel rule itself still requires the converted premise. */
	const struct pg_evidence *prefix = pg_evidence_premise(source, 0);
	const struct pg_evidence *second_type = pg_prove_family_identity_type(typing, pg_evidence_premise(source, 1),
		pg_prove_substitution(typing, prefix, choices, 4, li), pg_prove_substitution(typing, prefix, choices, 4, ri),
		1, selected, li[4], ri[4]);
	assert(second_type);
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(selected[1]), pg_evidence_subject(second_type)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	selected[1] = pg_prove_conversion(typing, selected[1], second_type, pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	struct pg_synthesis_job *changed = pg_synthesis_family_action(&split, producer, cl, cr, 2, selected);
	assert(original && changed && original != changed && unconverted != changed);
	assert(pg_synthesis_family_action(&split, producer, cl, cr, 2, selected) == changed);
	const struct pg_evidence *p0 = complete(&split, original, PG_SYNTHESIS_DONE);
	const struct pg_evidence *p1 = complete(&split, changed, PG_SYNTHESIS_DONE);
	same_judgement(automatic, p1);
	struct pg_synthesis_job *pending_paths[] = {pg_synthesis_normalize(&split, choices, selected[0]),
		pg_synthesis_normalize(&split, choices, selected[1])};
	struct pg_synthesis_job *pending_action = pg_synthesis_family_action_jobs(&split, producer, cl, cr, 2, pending_paths);
	assert(pg_synthesis_status(pending_action) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(pending_action));
	same_judgement(complete(&split, pending_action, PG_SYNTHESIS_DONE), p1);
	struct pg_synthesis_job *foreign_paths[] = {other_producer, pending_paths[1]};
	assert(!pg_synthesis_family_action_jobs(&split, producer, cl, cr, 2, foreign_paths));
	assert(!pg_synthesis_family_action_jobs(&split, producer, cl, cr, 2, NULL));
	assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(p0)->core, pg_evidence_subject(p1)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&comparison);
	assert(!pg_synthesis_family_action(&split, other_producer, ls, rs, 2, paths));
	assert(!pg_synthesis_family_action(&split, producer, source, rs, 2, paths));
	assert(!pg_synthesis_family_action(&split, producer, ls, rs, 2, NULL));
	const struct pg_evidence *missing[] = {paths[0], NULL};
	assert(!pg_synthesis_family_action(&split, producer, ls, rs, 2, missing));
	struct pg_synthesis_job *wrong = pg_synthesis_family_action(&split, producer, rs, ls, 2, paths);
	assert(wrong && wrong != job);
	complete(&split, wrong, PG_SYNTHESIS_REJECTED);
	pg_synthesis_destroy(&split);
	pg_synthesis_destroy(&whole);
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void family_transport(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing), *context = empty;
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *parameter = pg_binder(typing->graph);
	context = pg_prove_context_extension(typing, context, parameter, universe);
	const struct pg_evidence *base = pg_prove_value_type(typing, pg_prove_variable(typing, context, parameter));
	const struct pg_object *bindings[5];
	const struct pg_evidence *images[5];
	for (size_t i = 0; i < 5; ++i) {
		bindings[i] = pg_binder(typing->graph);
		const struct pg_evidence *domain = pg_prove_projection(typing, context, base);
		if (i >= 3) domain = pg_prove_identity_type(typing, domain,
			pg_prove_variable(typing, context, bindings[i - 3]), pg_prove_variable(typing, context, bindings[i - 2]));
		context = pg_prove_context_extension(typing, context, bindings[i], domain);
		assert(context);
	}
	for (size_t i = 0; i < 5; ++i) images[i] = pg_prove_variable(typing, context, bindings[i]);
	base = pg_prove_projection(typing, context, base);
	const struct pg_evidence *mapping[] = {pg_prove_variable(typing, context, parameter),
		images[0], images[1], images[2], images[3], images[4]};
	const struct pg_evidence *prefix = pg_prove_substitution(typing, context, context, 6, mapping);
	const struct pg_evidence *refl = pg_prove_reflexivity(typing, base, images[0]);
	for (size_t i = 0; i < 2; ++i) {
		/* Symmetry: transport refl a along p:a=b in t |-> Id t a.
		 * Composition: transport p along q:b=c in t |-> Id a t. */
		const struct pg_object *t = pg_binder(typing->graph);
		const struct pg_evidence *source = pg_prove_context_extension(typing, context, t, base);
		const struct pg_evidence *v = pg_prove_variable(typing, source, t);
		const struct pg_evidence *a = pg_prove_projection(typing, source, images[0]);
		const struct pg_evidence *family = pg_prove_identity_type(typing,
			pg_prove_projection(typing, source, base), i ? a : v, i ? v : a);
		family = pg_prove_type_value(typing, family);
		const struct pg_evidence *kind = pg_prove_classifier(typing, classifiers, source, family);
		const struct pg_evidence *left = pg_prove_substitution_pair(typing, prefix, source, images[i]);
		const struct pg_evidence *right = pg_prove_substitution_pair(typing, prefix, source, images[i + 1]);
		const struct pg_evidence *path = images[i + 3];
		const struct pg_evidence *action = pg_prove_family_action(typing, kind, family, left, right, 1, &path);
		const struct pg_evidence *input = i ? images[3] : refl;
		assert(action && !pg_prove_identity_transport(typing, classifiers, action, input, PG_IDENTITY_RIGHT));
		struct pg_whnf_job *untouched = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(action)->core);
		struct pg_synthesis_job *job = pg_synthesis_normalize_classifier(&split, context, action);
		assert(job && pg_synthesis_status(job) == PG_SYNTHESIS_PENDING);
		assert(pg_synthesis_normalize_classifier(&split, context, action) == job);
		const struct pg_evidence *r = complete(&split, job, PG_SYNTHESIS_DONE);
		assert(pg_evidence_rule(r) == PG_TYPE_CONVERSION);
		assert(pg_evidence_subject(r)->core == pg_evidence_subject(action)->core);
		assert(pg_whnf_status(untouched) == PG_EVAL_PENDING && pg_whnf_steps(untouched) == 0);
		struct pg_synthesis_job *bulk = pg_synthesis_normalize_classifier(&whole, context, action);
		pg_synthesis_advance(&whole, 100000);
		assert(pg_synthesis_status(bulk) == PG_SYNTHESIS_DONE);
		same_judgement(pg_synthesis_result(bulk), r);
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers, r, input, PG_IDENTITY_RIGHT);
		const struct pg_evidence *expected = pg_prove_identity_type(typing, base,
			i ? images[0] : images[1], i ? images[2] : images[0]);
		assert(transport && pg_evidence_classifier(transport) == pg_evidence_subject(expected)->core);
		assert(pg_prove_classifier(typing, classifiers, context, transport));
		assert(pg_prove_identity_lift(typing, classifiers, r, input, PG_IDENTITY_RIGHT));
		/* Symmetry and composition compute on reflexivity, not only typecheck. */
		const struct pg_evidence *diagonal = pg_prove_substitution_pair(typing, prefix, source, images[0]);
		const struct pg_evidence *diagonal_action = pg_prove_family_action(typing, kind, family,
			diagonal, diagonal, 1, &refl);
		const struct pg_evidence *dr = complete(&split,
			pg_synthesis_normalize_classifier(&split, context, diagonal_action), PG_SYNTHESIS_DONE);
		const struct pg_evidence *dt = pg_prove_identity_transport(typing, classifiers, dr, refl, PG_IDENTITY_RIGHT);
		assert(dt && pg_evidence_subject(normalize(&split, context, dt))->core == pg_evidence_subject(refl)->core);
		const struct pg_evidence *dl = pg_prove_identity_lift(typing, classifiers, dr, refl, PG_IDENTITY_RIGHT);
		assert(dl && pg_evidence_subject(normalize(&split, context, dl))->core ==
			pg_identity_action(typing->graph, pg_evidence_subject(refl)->core));
		assert(complete(&split, pg_synthesis_normalize_classifier(&split, context, r), PG_SYNTHESIS_DONE) == r);
		size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
		assert(pg_synthesis_normalize_classifier(&split, context, action) == job);
		assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
		assert(!pg_synthesis_normalize_classifier(&split, empty, action));
	}
	assert(!pg_synthesis_normalize_classifier(&split, context, universe));
	assert(!pg_synthesis_normalize_classifier(&split, context, NULL));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	const struct pg_evidence *foreign_context = pg_prove_empty_context(&foreign);
	const struct pg_evidence *foreign_value = pg_prove_type_value(&foreign,
		pg_prove_universe(&foreign, classifiers, foreign_context, 0));
	assert(foreign_value && !pg_synthesis_normalize_classifier(&split, foreign_context, foreign_value));
	pg_typing_destroy(&foreign);
	pg_synthesis_destroy(&split);
	pg_synthesis_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static void source_actions(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *x = pg_binder(typing->graph);
	const struct pg_evidence *context = pg_prove_context_extension(typing, empty, x, universe);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "x"};
	const struct pg_source_scope *scope = pg_synthesis_bind(&split, pg_synthesis_root(&split), name, x, context);
	const struct pg_source_scope *other_scope = pg_synthesis_bind(&whole, pg_synthesis_root(&whole), name, x, context);
	const char *source = "answer := (\\y : @ => y) x;";
	struct pg_synthesis_job *inputs[5] = {request(&split, scope, source)};
	struct pg_synthesis_job *others[5] = {request(&whole, other_scope, source)};
	size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
	for (size_t i = 1; i < 5; ++i) {
		inputs[i] = pg_synthesis_reflexivity(&split, context, inputs[i - 1]);
		others[i] = pg_synthesis_reflexivity(&whole, context, others[i - 1]);
		assert(inputs[i] && others[i]);
		assert(pg_synthesis_reflexivity(&split, context, inputs[i - 1]) == inputs[i]);
		assert(pg_synthesis_status(inputs[i]) == PG_SYNTHESIS_PENDING);
		assert(!pg_synthesis_result(inputs[i]));
	}
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
	wait_on(&split, inputs[4], inputs[3]);
	assert(pg_synthesis_status(inputs[0]) == PG_SYNTHESIS_PENDING);
	complete(&split, inputs[4], PG_SYNTHESIS_DONE);
	pg_synthesis_advance(&whole, 100000);
	const struct pg_term *expected = pg_reference(typing->graph, x);
	for (size_t i = 1; i < 5; ++i) {
		const struct pg_evidence *acted = pg_synthesis_result(inputs[i]);
		assert(pg_evidence_rule(acted) == PG_REFLEXIVITY);
		assert(pg_evidence_premise(acted, 1) == pg_synthesis_result(inputs[i - 1]));
		same_judgement(acted, pg_synthesis_result(others[i]));
		expected = pg_identity_action(typing->graph, expected);
		const struct pg_evidence *result = complete(&split, pg_synthesis_return(&split, context, acted), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(result)->core == expected);
		assert(pg_prove_classifier(typing, classifiers, context, result));
	}
	/* Source Lambda action consumes the same checked triple as its Pi type. */
	struct pg_synthesis_job *function = request(&split, scope, "f := \\y : @ => y;");
	const struct pg_evidence *function_action = complete(&split,
		pg_synthesis_reflexivity(&split, context, function), PG_SYNTHESIS_DONE);
	const struct pg_evidence *f = pg_synthesis_result(function);
	const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, context, f);
	const struct pg_evidence *expanded = pg_identity_pi_type(typing, classifiers, context, pi, f, f,
		pg_binder(typing->graph), pg_binder(typing->graph), pg_binder(typing->graph));
	assert(expanded);
	struct pg_conversion conversion;
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(function_action),
		pg_evidence_subject(expanded)->core) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
	f = pg_prove_conversion(typing, function_action, expanded, pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	const struct pg_evidence *vx = pg_prove_variable(typing, context, x);
	const struct pg_evidence *px = pg_prove_reflexivity(typing, pg_prove_projection(typing, context, universe), vx);
	f = pg_prove_application(typing, pg_prove_application(typing, pg_prove_application(typing, f, vx), vx), px);
	const struct pg_evidence *fx = complete(&split, pg_synthesis_return(&split, context, f), PG_SYNTHESIS_DONE);
	/* RETURN is WHNF without normalizing its contained action. */
	fx = normalize(&split, context, fx);
	assert(pg_evidence_subject(fx)->core == pg_evidence_subject(px)->core);
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(fx), pg_evidence_classifier(px)) == 0);
	assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&conversion);
	terms = typing->graph->terms.count; proofs = typing->proofs.count;
	for (size_t i = 1; i < 5; ++i) assert(pg_synthesis_reflexivity(&split, context, inputs[i - 1]) == inputs[i]);
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
	assert(!pg_synthesis_reflexivity(&split, context, others[0]));
	assert(!pg_synthesis_reflexivity(&split, NULL, inputs[0]));
	assert(!pg_synthesis_reflexivity(&split, universe, inputs[0]));
	assert(!pg_synthesis_reflexivity(&split, context, NULL));
	complete(&split, pg_synthesis_reflexivity(&split, empty, inputs[0]), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *bad = request(&split, scope, "answer := missing;");
	complete(&split, pg_synthesis_reflexivity(&split, context, bad), PG_SYNTHESIS_REJECTED);
	bad = request(&split, scope, "answer := (\\y : @ => y) :: @;");
	complete(&split, pg_synthesis_reflexivity(&split, context, bad), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *type = request(&split, scope, "answer := x -> x;");
	complete(&split, pg_synthesis_reflexivity(&split, context, type), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *value = request(&split, scope, "answer := @;");
	const struct pg_evidence *universe_action = complete(&split,
		pg_synthesis_reflexivity(&split, context, value), PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(universe_action) == PG_JUDGEMENT_VALUE);
	struct pg_synthesis_job *library = program(&split, scope, "alias := x;");
	complete(&split, pg_synthesis_reflexivity(&split, context, library), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	struct pg_synthesis_job *root_value = request(&split, pg_synthesis_root(&split), "answer := @;");
	complete(&split, pg_synthesis_reflexivity(&split, pg_prove_empty_context(&foreign), root_value), PG_SYNTHESIS_REJECTED);
	pg_typing_destroy(&foreign);
	struct pg_synthesis_job *cycle = program(&split, scope, "{{ a := b; b := a; }}.a;");
	struct pg_synthesis_job *cycle_action = pg_synthesis_reflexivity(&split, context, cycle);
	pg_synthesis_advance(&split, 1000);
	assert(pg_synthesis_status(cycle_action) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_cycle(cycle_action));
	assert(!pg_synthesis_result(cycle_action));
	const struct pg_evidence *sigma = pg_prove_substitution(typing, context, context, 1, &vx);
	const struct pg_evidence *family_action = complete(&split,
		pg_synthesis_family_action(&split, function, sigma, sigma, 1, &px), PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(family_action) == PG_JUDGEMENT_COMPUTATION);
	same_judgement(family_action, pg_prove_family_action(typing, pi,
		pg_synthesis_result(function), sigma, sigma, 1, &px));
	same_judgement(complete(&split, pg_synthesis_family_action(&split, function, sigma, sigma, 0, NULL), PG_SYNTHESIS_DONE),
		pg_prove_family_action(typing, pi, pg_synthesis_result(function), sigma, sigma, 0, NULL));
	complete(&split, pg_synthesis_family_action(&split, bad, sigma, sigma, 1, &px), PG_SYNTHESIS_REJECTED);
	complete(&split, pg_synthesis_family_action(&split, type, sigma, sigma, 1, &px), PG_SYNTHESIS_UNSUPPORTED);
	assert(pg_evidence_judgement(complete(&split,
		pg_synthesis_family_action(&split, value, sigma, sigma, 1, &px), PG_SYNTHESIS_DONE)) == PG_JUDGEMENT_VALUE);
	struct pg_synthesis_job *family_cycle = pg_synthesis_family_action(&split, cycle, sigma, sigma, 1, &px);
	pg_synthesis_advance(&split, 1000);
	assert(pg_synthesis_status(family_cycle) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(family_cycle));
	struct pg_synthesis_job *path_cycle = pg_synthesis_family_action_jobs(&split, function, sigma, sigma, 1, &cycle);
	pg_synthesis_advance(&split, 1000);
	assert(pg_synthesis_status(path_cycle) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(path_cycle));
	assert(!pg_synthesis_result(path_cycle));
	uint64_t idle_steps = split.steps;
	pg_synthesis_advance(&split, 1000);
	assert(split.steps == idle_steps);
	complete(&split, pg_synthesis_family_action_jobs(&split, function, sigma, sigma, 1, &bad), PG_SYNTHESIS_REJECTED);
	complete(&split, pg_synthesis_family_action_jobs(&split, function, sigma, sigma, 1, &library), PG_SYNTHESIS_REJECTED);
	pg_synthesis_destroy(&split);
	pg_synthesis_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static void normalization_jobs(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *source, const struct pg_evidence *value)
{
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	struct pg_synthesis_job *job = pg_synthesis_normalize(&split, context, source);
	assert(job && pg_synthesis_normalize(&split, context, source) == job);
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(job));
	struct pg_whnf_job *computation = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(source)->core);
	assert(!pg_whnf_certificate(computation));
	assert(!pg_prove_normalization(typing, source, pg_whnf_certificate(computation)));
	const struct pg_evidence *answer = complete(&split, job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(answer) == PG_PURE_NORMALIZATION);
	assert(pg_evidence_premise(answer, 0) == source);
	assert(pg_evidence_classifier(answer) == pg_evidence_classifier(source));
	assert(pg_evidence_judgement(answer) == pg_evidence_judgement(source));
	assert(pg_evidence_subject(answer)->core == pg_evidence_subject(pg_prove_return(typing, classifiers, value))->core);
	const struct pg_reduction_certificate *certificate = pg_whnf_certificate(computation);
	assert(certificate && certificate == pg_evidence_normalization(answer));
	assert(pg_reduction_source(certificate) == pg_evidence_subject(source)->core);
	assert(pg_reduction_target(certificate) == pg_evidence_subject(answer)->core);
	assert(!pg_evidence_conversion(answer));
	assert(pg_prove_normalization(typing, source, certificate) == answer);
	assert(pg_prove_classifier(typing, classifiers, context, answer));
	assert(!pg_prove_return_value(typing, source));
	const struct pg_evidence *content_value = complete(&split,
		pg_synthesis_return(&split, context, answer), PG_SYNTHESIS_DONE);
	same_judgement(content_value, value);
	assert(pg_evidence_rule(content_value) == PG_RETURN_VALUE);
	assert(pg_evidence_premise(content_value, 0) == answer);
	assert(!pg_prove_thunk_computation(typing, answer));
	uint64_t steps = pg_whnf_steps(computation);
	struct pg_synthesis_job *second = pg_synthesis_normalize(&whole, context, source);
	pg_synthesis_advance(&whole, 1000);
	assert(pg_synthesis_result(second) == answer && pg_whnf_steps(computation) == steps);
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, value);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, context, pg_binder(typing->graph), type);
	const struct pg_evidence *projected = pg_prove_projection(typing, extended, source);
	const struct pg_evidence *other = complete(&split,
		pg_synthesis_normalize(&split, extended, projected), PG_SYNTHESIS_DONE);
	assert(other != answer && pg_evidence_context(other) == pg_evidence_context(extended));
	assert(pg_evidence_normalization(other) == certificate && pg_whnf_steps(computation) == steps);
	const struct pg_evidence *projected_answer = pg_prove_projection(typing, extended, answer);
	const struct pg_evidence *projected_type = pg_prove_projection(typing, extended,
		pg_prove_classifier(typing, classifiers, context, answer));
	const struct pg_evidence *recoveries[] = {
		pg_prove_classifier(typing, classifiers, extended, projected_answer),
		pg_prove_classifier(typing, classifiers, extended, other)
	};
	for (size_t i = 0; i < 2; ++i) {
		assert(recoveries[i] && pg_evidence_context(recoveries[i]) == pg_evidence_context(extended));
		assert(pg_evidence_judgement(recoveries[i]) == pg_evidence_judgement(projected_type));
		assert(pg_alpha_equal(pg_evidence_subject(recoveries[i])->core,
			pg_evidence_subject(projected_type)->core) == 1);
	}
	assert(!pg_synthesis_normalize(&split, context, projected));
	assert(!pg_synthesis_normalize(&split, context, context));
	assert(complete(&split, pg_synthesis_normalize(&split, context, value), PG_SYNTHESIS_DONE) == value);
	const struct pg_evidence *acted = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, context, source), source);
	const struct pg_evidence *action_answer = complete(&split,
		pg_synthesis_normalize(&split, context, acted), PG_SYNTHESIS_DONE);
	assert(action_answer && pg_evidence_normalization(action_answer));
	assert(pg_evidence_classifier(action_answer) == pg_evidence_classifier(acted));
	const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, context, acted);
	const struct pg_evidence *normalized_type = complete(&split,
		pg_synthesis_normalize(&split, context, formation), PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(normalized_type) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(normalized_type) == pg_evidence_classifier(formation));
	const struct pg_term *content;
	assert(pg_return_type_view(pg_evidence_subject(normalized_type)->core, &content));
	static const struct pg_eval_policy foreign_policy = {arbitrary_policy};
	struct pg_whnf_job *foreign = pg_whnf_request(&work, &foreign_policy, pg_evidence_subject(source)->core);
	assert(pg_whnf_advance(foreign, 10000) == PG_EVAL_WHNF);
	assert(pg_whnf_certificate(foreign));
	assert(!pg_prove_normalization(typing, source, pg_whnf_certificate(foreign)));
	/* Equal by beta expansion is not directed reduction from a typed value. */
	const struct pg_term *bad = pg_application(typing->graph,
		pg_lambda(typing->graph, pg_binder(typing->graph), pg_evidence_subject(value)->core),
		pg_reference(typing->graph, pg_binder(typing->graph)));
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(value)->core, bad) == 0);
	assert(pg_conversion_advance(&comparison, 10000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	struct pg_whnf_job *expansion = pg_whnf_request(&work, &pg_pure_policy, bad);
	assert(pg_whnf_advance(expansion, 10000) == PG_EVAL_WHNF);
	assert(!pg_prove_normalization(typing, value, pg_whnf_certificate(expansion)));
	struct pg_typing foreign_typing;
	assert(pg_typing_init(&foreign_typing, typing->graph) == 0);
	assert(!pg_prove_normalization(&foreign_typing, source, certificate));
	pg_typing_destroy(&foreign_typing);
	/* Same erased FORCE/THUNK identity, different annotated function domains. */
	const struct pg_evidence *domains[] = {type, pg_prove_universe(typing, classifiers, context, 0)};
	const struct pg_evidence *typed_sources[2], *typed_answers[2];
	const struct pg_object *z = pg_binder(typing->graph);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *local = pg_prove_context_extension(typing, context, z, domains[i]);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, local, z));
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domains[i], local,
			pg_prove_classifier(typing, classifiers, local, body));
		typed_sources[i] = pg_prove_force(typing,
			pg_prove_thunk(typing, classifiers, pg_prove_lambda(typing, pi, body)));
		typed_answers[i] = complete(&split, pg_synthesis_normalize(&split, context, typed_sources[i]), PG_SYNTHESIS_DONE);
		assert(pg_evidence_classifier(typed_answers[i]) == pg_evidence_classifier(typed_sources[i]));
		assert(pg_evidence_premise(typed_answers[i], 0) == typed_sources[i]);
	}
	assert(pg_evidence_subject(typed_sources[0])->core == pg_evidence_subject(typed_sources[1])->core);
	assert(pg_evidence_subject(typed_answers[0])->core == pg_evidence_subject(typed_answers[1])->core);
	assert(pg_evidence_normalization(typed_answers[0]) == pg_evidence_normalization(typed_answers[1]));
	assert(pg_evidence_classifier(typed_answers[0]) != pg_evidence_classifier(typed_answers[1]));
	/* NF traverses suspended computations; WHNF preserves their outer value. */
	const struct pg_evidence *suspended = pg_prove_thunk(typing, classifiers, source);
	const struct pg_evidence *expected = pg_prove_thunk(typing, classifiers,
		pg_prove_return(typing, classifiers, value));
	assert(complete(&split, pg_synthesis_normalize(&split, context, suspended), PG_SYNTHESIS_DONE) == suspended);
	struct pg_synthesis_job *nf = pg_synthesis_nf(&split, context, suspended);
	struct pg_nf_job *nf_core = pg_nf_request(&work, &pg_pure_policy, pg_evidence_subject(suspended)->core);
	assert(nf && nf != pg_synthesis_normalize(&split, context, suspended));
	assert(pg_synthesis_nf(&split, context, suspended) == nf);
	assert(!pg_synthesis_result(nf) && !pg_nf_certificate(nf_core) && pg_nf_steps(nf_core) == 0);
	const struct pg_evidence *nf_answer = complete(&split, nf, PG_SYNTHESIS_DONE);
	const struct pg_reduction_certificate *nf_certificate = pg_nf_certificate(nf_core);
	assert(nf_certificate && pg_evidence_normalization(nf_answer) == nf_certificate);
	assert(pg_evidence_subject(nf_answer)->core == pg_evidence_subject(expected)->core);
	assert(pg_evidence_classifier(nf_answer) == pg_evidence_classifier(suspended));
	assert(pg_evidence_rule(nf_answer) == PG_PURE_NORMALIZATION);
	assert(pg_evidence_premise(nf_answer, 0) == suspended);
	assert(pg_reduction_source(nf_certificate) == pg_evidence_subject(suspended)->core);
	assert(pg_reduction_target(nf_certificate) == pg_evidence_subject(nf_answer)->core);
	assert(pg_reduction_policy(nf_certificate) == &pg_pure_policy);
	assert(pg_prove_normalization(typing, suspended, nf_certificate) == nf_answer);
	assert(!pg_prove_normalization(typing, value, nf_certificate));
	steps = pg_nf_steps(nf_core);
	second = pg_synthesis_nf(&whole, context, suspended);
	pg_synthesis_advance(&whole, 10000);
	assert(pg_synthesis_result(second) == nf_answer && pg_nf_steps(nf_core) == steps);
	const struct pg_evidence *nf_projected = pg_prove_projection(typing, extended, suspended);
	const struct pg_evidence *nf_other = complete(&split,
		pg_synthesis_nf(&split, extended, nf_projected), PG_SYNTHESIS_DONE);
	assert(nf_other != nf_answer && pg_evidence_context(nf_other) == pg_evidence_context(extended));
	assert(pg_evidence_normalization(nf_other) == nf_certificate && pg_nf_steps(nf_core) == steps);
	assert(pg_typing_init(&foreign_typing, typing->graph) == 0);
	assert(!pg_prove_normalization(&foreign_typing, suspended, nf_certificate));
	pg_typing_destroy(&foreign_typing);
	assert(!pg_synthesis_nf(&split, context, projected));
	assert(!pg_synthesis_nf(&split, context, context));
	struct pg_nf_job *beta_nf = pg_nf_request(&work, &pg_beta_policy, pg_evidence_subject(suspended)->core);
	assert(pg_nf_advance(beta_nf, 10000) == PG_NF_DONE);
	assert(!pg_prove_normalization(typing, suspended, pg_nf_certificate(beta_nf)));
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *local = pg_prove_context_extension(typing, context, z, domains[i]);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, local, z));
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domains[i], local,
			pg_prove_classifier(typing, classifiers, local, body));
		body = pg_prove_force(typing, pg_prove_thunk(typing, classifiers, body));
		typed_sources[i] = pg_prove_lambda(typing, pi, body);
		assert(complete(&split, pg_synthesis_normalize(&split, context, typed_sources[i]), PG_SYNTHESIS_DONE) == typed_sources[i]);
		typed_answers[i] = complete(&split, pg_synthesis_nf(&split, context, typed_sources[i]), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(typed_answers[i])->core != pg_evidence_subject(typed_sources[i])->core);
		assert(pg_evidence_classifier(typed_answers[i]) == pg_evidence_classifier(typed_sources[i]));
		assert(pg_evidence_premise(typed_answers[i], 0) == typed_sources[i]);
		assert(pg_prove_classifier(typing, classifiers, context, typed_answers[i]));
	}
	assert(pg_evidence_subject(typed_sources[0])->core == pg_evidence_subject(typed_sources[1])->core);
	assert(pg_evidence_normalization(typed_answers[0]) == pg_evidence_normalization(typed_answers[1]));
	assert(pg_evidence_classifier(typed_answers[0]) != pg_evidence_classifier(typed_answers[1]));
	pg_synthesis_destroy(&whole);
	pg_synthesis_destroy(&split);
	pg_whnf_work_destroy(&work);
	assert(pg_evidence_normalization(answer) == certificate);
	assert(pg_reduction_target(certificate) == pg_evidence_subject(answer)->core);
	assert(pg_prove_normalization(typing, source, certificate) == answer);
	assert(pg_prove_normalization(typing, suspended, nf_certificate) == nf_answer);
}

static void identity_contents(struct pg_typing *typing, struct pg_classifiers *classifiers,
	struct pg_whnf_work *normalization, const struct pg_evidence *context,
	const struct pg_evidence *source, const struct pg_evidence *value,
	const struct pg_evidence *substitution)
{
	struct pg_synthesis split, whole;
	assert(pg_synthesis_init(&split, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *source_type = pg_prove_classifier(typing, classifiers, context, source);
	const struct pg_evidence *acted = pg_prove_reflexivity(typing, source_type, source);
	assert(acted);
	struct pg_synthesis_job *source_job = pg_synthesis_return(&split, context, source);
	struct pg_synthesis_job *acted_job = pg_synthesis_return(&split, context, acted);
	assert(acted_job && !pg_synthesis_result(acted_job));
	wait_on(&split, acted_job, pg_synthesis_normalize(&split, context, acted));
	const struct pg_evidence *answer = complete(&split, acted_job, PG_SYNTHESIS_DONE);
	const struct pg_term *expected = pg_identity_action(typing->graph, pg_evidence_subject(value)->core);
	assert(pg_evidence_subject(answer)->core == expected);
	assert(pg_evidence_rule(answer) == PG_RETURN_VALUE);
	assert(pg_evidence_rule(pg_evidence_premise(answer, 0)) == PG_TYPE_CONVERSION);
	complete(&split, source_job, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_return(&split, context, acted) == acted_job);
	struct pg_synthesis_job *whole_source = pg_synthesis_return(&whole, context, source);
	struct pg_synthesis_job *whole_action = pg_synthesis_return(&whole, context, acted);
	pg_synthesis_advance(&whole, 1000);
	assert(pg_synthesis_status(whole_source) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(whole_action) == PG_SYNTHESIS_DONE);
	same_judgement(pg_synthesis_result(whole_action), answer);
	/* The second scheduler shares already accepted reindex/conversion work;
	 * equal answers, not equal cold-start costs, are the invariant here. */
	assert(whole.steps <= split.steps);
	pg_synthesis_destroy(&whole);
	for (size_t dimension = 1; dimension < 4; ++dimension) {
		acted = pg_prove_reflexivity(typing,
			pg_prove_classifier(typing, classifiers, context, acted), acted);
		expected = pg_identity_action(typing->graph, expected);
		answer = complete(&split, pg_synthesis_return(&split, context, acted), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(answer)->core == expected);
		const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, answer);
		const struct pg_evidence *return_type = pg_prove_return_type(typing, classifiers, type);
		struct pg_conversion conversion;
		assert(pg_conversion_init(&conversion, normalization, pg_evidence_classifier(acted),
			pg_evidence_subject(return_type)->core) == 0);
		while (pg_conversion_advance(&conversion, 1) == PG_CONVERSION_PENDING)
			assert(pg_conversion_steps(&conversion) < 10000);
		assert(pg_conversion_status(&conversion) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *converted = pg_prove_conversion(typing, acted, return_type,
			pg_conversion_certificate(&conversion));
		pg_conversion_destroy(&conversion);
		assert(converted);
		same_judgement(complete(&split, pg_synthesis_return(&split, context, converted), PG_SYNTHESIS_DONE), answer);
	}
	const struct pg_evidence *moved = pg_prove_reindex(typing, substitution, acted);
	assert(moved);
	const struct pg_evidence *moved_answer = complete(&split,
		pg_synthesis_return(&split, context, moved), PG_SYNTHESIS_DONE);
	same_judgement(moved_answer, pg_prove_reindex(typing, substitution, answer));
	const struct pg_evidence *extension = pg_prove_context_extension(typing, context,
		pg_binder(typing->graph), pg_prove_classifier(typing, classifiers, context, value));
	const struct pg_evidence *projected = pg_prove_projection(typing, extension, acted);
	same_judgement(complete(&split, pg_synthesis_return(&split, extension, projected), PG_SYNTHESIS_DONE),
		pg_prove_projection(typing, extension, answer));
	const struct pg_evidence *new_value = pg_prove_variable(typing, extension,
		pg_evidence_context(extension)->binder);
	const struct pg_evidence *changed_images[] = {
		pg_prove_variable(typing, extension, pg_evidence_context(context)->parent->binder), new_value
	};
	const struct pg_evidence *changed = pg_prove_substitution(typing, context, extension, 2, changed_images);
	assert(changed);
	const struct pg_evidence *changed_action = pg_prove_reindex(typing, changed, acted);
	const struct pg_evidence *changed_answer = complete(&split,
		pg_synthesis_return(&split, extension, changed_action), PG_SYNTHESIS_DONE);
	same_judgement(changed_answer, pg_prove_reindex(typing, changed, answer));
	expected = pg_evidence_subject(new_value)->core;
	for (size_t dimension = 0; dimension < 4; ++dimension)
		expected = pg_identity_action(typing->graph, expected);
	assert(pg_evidence_subject(changed_answer)->core == expected);
	pg_synthesis_destroy(&split);

	/* Extracting acted THUNK code must not execute its beta-redex source. */
	assert(pg_synthesis_init(&split, typing, classifiers, normalization, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *quoted = pg_prove_thunk(typing, classifiers, source);
	const struct pg_evidence *quoted_type = pg_prove_classifier(typing, classifiers, context, quoted);
	const struct pg_evidence *quoted_action = pg_prove_reflexivity(typing, quoted_type, quoted);
	struct pg_synthesis_job *unthunk = pg_synthesis_unthunk(&split, context, quoted_action);
	pg_synthesis_advance(&split, 0);
	assert(!pg_synthesis_result(unthunk));
	const struct pg_evidence *code = complete(&split, unthunk, PG_SYNTHESIS_DONE);
	const struct pg_evidence *source_action = pg_prove_reflexivity(typing, source_type, source);
	assert(pg_evidence_subject(code)->core == pg_evidence_subject(source_action)->core);
	/* U observation retains FORCE(THUNK source) in the classifier. It is
	 * convertible to source, but is not a structurally identical endpoint. */
	struct pg_conversion code_type;
	assert(pg_conversion_init(&code_type, normalization, pg_evidence_classifier(code),
		pg_evidence_classifier(source_action)) == 0);
	assert(pg_conversion_advance(&code_type, 10000) == PG_CONVERSION_EQUAL);
	same_judgement(pg_prove_conversion(typing, code,
		pg_prove_classifier(typing, classifiers, context, source_action),
		pg_conversion_certificate(&code_type)), source_action);
	pg_conversion_destroy(&code_type);
	assert(pg_evidence_rule(code) == PG_THUNK_COMPUTATION);
	assert(!pg_synthesis_return(&split, context, quoted_action));
	assert(!pg_synthesis_unthunk(&split, context, code));
	pg_synthesis_destroy(&split);
}

static const struct pg_syntax *expression_syntax(struct pg_graph *graph, const char *source)
{
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_syntax *syntax = definition.expression;
	assert(pg_parser_next(&parser, &definition) == 0);
	return syntax;
}

static void source_telescopes(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const char *sources[] = {"f:=\\A:@ => \\x:A => x;", "f:=(A:@)->(x:A)->A;",
		"f:=\\A:@ => \\x:((\\T:@ => T) A) => x;"};
	for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
		const struct pg_syntax *syntax = expression_syntax(typing->graph, sources[i]);
		for (size_t expression_first = 0; expression_first < 2; ++expression_first) {
			struct pg_synthesis_job *expression = NULL;
			if (expression_first) {
				syntax = expression_syntax(typing->graph, sources[i]);
				expression = pg_synthesis_request(&synthesis, root, syntax);
				complete(&synthesis, expression, PG_SYNTHESIS_DONE);
			}
			struct pg_synthesis_job *job = pg_synthesis_telescope(&synthesis, root, syntax);
			struct pg_synthesis_job *binding = pg_synthesis_binding(&synthesis, root, syntax);
			const struct pg_object *reserved = pg_synthesis_binding_binder(binding);
			assert(reserved && pg_synthesis_binding(&synthesis, root, syntax) == binding);
			struct pg_synthesis_job *inner_expression = pg_synthesis_request(&synthesis,
				pg_synthesis_binding_scope(binding), syntax->right);
			assert(job && pg_synthesis_telescope(&synthesis, root, syntax) == job);
			assert(!pg_synthesis_telescope_scope(job) && !pg_synthesis_telescope_body(job));
			size_t contexts = typing->contexts.count;
			const struct pg_evidence *context = complete(&synthesis, job, PG_SYNTHESIS_DONE);
			if (expression_first) assert(typing->contexts.count == contexts);
			assert(pg_evidence_judgement(context) == PG_JUDGEMENT_CONTEXT);
			const struct pg_context *last = pg_evidence_context(context);
			assert(last && last->parent && !last->parent->parent);
			assert(last->parent->binder == reserved);
			assert(pg_evidence_context(complete(&synthesis, inner_expression, PG_SYNTHESIS_DONE)) == last->parent);
			const struct pg_source_scope *scope = pg_synthesis_telescope_scope(job);
			assert(pg_synthesis_telescope_body(job) == syntax->right->right);
			const struct pg_evidence *body = complete(&synthesis,
				pg_synthesis_request(&synthesis, scope, syntax->right->right), PG_SYNTHESIS_DONE);
			assert(pg_evidence_context(body) == last);
			contexts = typing->contexts.count;
			if (!expression) expression = pg_synthesis_request(&synthesis, root, syntax);
			const struct pg_evidence *proof = complete(&synthesis, expression, PG_SYNTHESIS_DONE);
			assert(typing->contexts.count == contexts);
			const struct pg_term *pi = i == 1 ? pg_evidence_subject(proof)->core : pg_evidence_classifier(proof);
			const struct pg_object *binders[2];
			const struct pg_term *domain;
			assert(pg_pi_view(pi, &domain, &binders[0], &pi));
			assert(pg_pi_view(pi, &domain, &binders[1], &pi));
			assert(binders[0] == last->parent->binder && binders[1] == last->binder);
			assert(!pg_synthesis_telescope_scope(expression));
		}
	}
	/* Unsolved domains must not prevent allocation of lexical identity, nor
	 * turn that identity into an accepted context/variable derivation. */
	struct pg_synthesis_job *pending_cycle = program(&synthesis, root, "{{T:=T;}}.T;");
	const struct pg_source_scope *pending_scope = pg_synthesis_name_job(&synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "T", .length = 1}, pending_cycle);
	const struct pg_syntax *pending_source = expression_syntax(typing->graph, "f:=\\x:T=>\\y:T=>x;");
	size_t before_contexts = typing->contexts.count, before_proofs = typing->proofs.count;
	uint64_t before_steps = synthesis.steps;
	struct pg_synthesis_job *pending_binding = pg_synthesis_binding(&synthesis, pending_scope, pending_source);
	const struct pg_object *pending_binder = pg_synthesis_binding_binder(pending_binding);
	assert(pending_binder && !pg_synthesis_result(pending_binding));
	assert(typing->contexts.count == before_contexts && typing->proofs.count == before_proofs);
	assert(synthesis.steps == before_steps && !pg_prove_variable(typing, empty, pending_binder));
	const struct pg_source_scope *pending_inner = pg_synthesis_binding_scope(pending_binding);
	assert(pending_inner);
	struct pg_synthesis_job *second_binding = pg_synthesis_binding(&synthesis, pending_inner, pending_source->right);
	const struct pg_source_scope *second_inner = pg_synthesis_binding_scope(second_binding);
	assert(second_inner && pg_synthesis_binding_binder(second_binding) != pending_binder);
	struct pg_synthesis_job *pending_body = pg_synthesis_request(&synthesis, second_inner, pending_source->right->right);
	struct pg_synthesis_job *pending_telescope = pg_synthesis_telescope(&synthesis, pending_scope, pending_source);
	assert(typing->contexts.count == before_contexts && typing->proofs.count == before_proofs);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(pending_binding) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(pending_binding));
	assert(pg_synthesis_binding(&synthesis, pending_scope, pending_source) == pending_binding);
	assert(pg_synthesis_binding_binder(pending_binding) == pending_binder && !pg_synthesis_result(pending_binding));
	assert(pg_synthesis_status(second_binding) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(second_binding));
	assert(pg_synthesis_status(pending_body) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(pending_body));
	assert(pg_synthesis_status(pending_telescope) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(pending_telescope));
	assert(!pg_synthesis_result(pending_body) && !pg_synthesis_telescope_scope(pending_telescope));
	struct pg_synthesis_job *structure = pg_synthesis_telescope_structure(&synthesis, pending_scope, pending_source);
	assert(pg_synthesis_status(structure) == PG_SYNTHESIS_DONE && !pg_synthesis_result(structure));
	assert(pg_synthesis_telescope_scope(structure) == second_inner);
	assert(pg_synthesis_telescope_body(structure) == pending_source->right->right);
	assert(!pg_synthesis_namespace(&synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Bad", .length = 3}, second_inner));
	struct pg_synthesis_job *invalid_binding = pg_synthesis_binding(&synthesis, root,
		expression_syntax(typing->graph, "f:=\\x:Missing=>x;"));
	const struct pg_object *invalid_binder = pg_synthesis_binding_binder(invalid_binding);
	struct pg_synthesis_job *invalid_body = pg_synthesis_request(&synthesis,
		pg_synthesis_binding_scope(invalid_binding), pending_source->right->right);
	complete(&synthesis, invalid_binding, PG_SYNTHESIS_REJECTED);
	complete(&synthesis, invalid_body, PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_result(invalid_body));
	assert(invalid_binder && invalid_binder != pending_binder);
	assert(pg_synthesis_binding_binder(invalid_binding) == invalid_binder && !pg_synthesis_result(invalid_binding));
	assert(!pg_synthesis_binding_binder(NULL) && !pg_synthesis_binding_binder(pending_cycle));
	assert(!pg_synthesis_binding(&synthesis, root, pending_source->right->right));
	/* Parameters and indices have separate scopes. Constructor fields extend
	 * parameters, not the index binders; the result map relates those contexts. */
	const struct pg_syntax *source = expression_syntax(typing->graph,
		"Box:=\\A:@ => @\\i:A => {mk:(x:A)->* x;};");
	struct pg_synthesis_job *parameters = pg_synthesis_telescope(&synthesis, root, source);
	const struct pg_evidence *parameter_context = complete(&synthesis, parameters, PG_SYNTHESIS_DONE);
	const struct pg_source_scope *parameter_scope = pg_synthesis_telescope_scope(parameters);
	const struct pg_syntax *declaration = pg_synthesis_telescope_body(parameters);
	assert(declaration->kind == PG_SYNTAX_DECLARATION);
	struct pg_synthesis_job *indices = pg_synthesis_telescope(&synthesis, parameter_scope, declaration->left);
	const struct pg_evidence *index_context = complete(&synthesis, indices, PG_SYNTHESIS_DONE);
	const struct pg_syntax *constructors = pg_synthesis_telescope_body(indices);
	assert(constructors->kind == PG_SYNTAX_CONSTRUCTORS && constructors->item_count == 1);
	struct pg_synthesis_job *fields = pg_synthesis_telescope(&synthesis, parameter_scope, constructors->items[0].expression);
	const struct pg_evidence *field_context = complete(&synthesis, fields, PG_SYNTHESIS_DONE);
	const struct pg_source_scope *field_scope = pg_synthesis_telescope_scope(fields);
	complete(&synthesis, request(&synthesis, field_scope, "bad:=i;"), PG_SYNTHESIS_REJECTED);
	const struct pg_syntax *result = pg_synthesis_telescope_body(fields);
	assert(result->kind == PG_SYNTAX_APPLICATION && result->left->token.kind == '*');
	const struct pg_evidence *image = complete(&synthesis,
		pg_synthesis_request(&synthesis, field_scope, result->right), PG_SYNTHESIS_DONE);
	const struct pg_evidence *a = pg_prove_variable(typing, field_context, pg_evidence_context(parameter_context)->binder);
	const struct pg_evidence *prefix = pg_prove_substitution(typing, parameter_context, field_context, 1, &a);
	const struct pg_evidence *map = complete(&synthesis,
		pg_synthesis_substitution_pair(&synthesis, prefix, index_context, image), PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *result_job = pg_synthesis_data_result(&synthesis, field_scope, parameter_context, index_context, result);
	assert(result_job && pg_synthesis_data_result(&synthesis, field_scope, parameter_context, index_context, result) == result_job);
	const struct pg_evidence *source_map = complete(&synthesis, result_job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(source_map) == pg_evidence_context(map));
	for (size_t i = 2; i < pg_evidence_premise_count(map); ++i)
		same_judgement(pg_evidence_premise(source_map, i), pg_evidence_premise(map, i));
	const struct pg_data_schema *schema = pg_data_schema(typing, pg_data_signature(typing, parameter_context, index_context), 1, &source_map);
	assert(schema && pg_data_schema_fields(schema, pg_data_constructor(pg_data_schema_layout(schema), 0)) == field_context);
	/* Checked telescopes do not turn this source into an admitted nominal type. */
	complete(&synthesis, pg_synthesis_request(&synthesis, root, source), PG_SYNTHESIS_UNSUPPORTED);
	const char *invalid[] = {"f:=\\x:missing => x;", "f:=\\A:@ => \\x:A => \\y:x => y;"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		struct pg_synthesis_job *job = pg_synthesis_telescope(&synthesis, root,
			expression_syntax(typing->graph, invalid[i]));
		complete(&synthesis, job, PG_SYNTHESIS_REJECTED);
		assert(!pg_synthesis_telescope_body(job) && !pg_synthesis_telescope_scope(job));
	}
	const struct pg_syntax *unresolved = expression_syntax(typing->graph, "f:=missing;");
	struct pg_synthesis_job *zero = pg_synthesis_telescope(&synthesis, root, unresolved);
	assert(complete(&synthesis, zero, PG_SYNTHESIS_DONE) == empty);
	assert(pg_synthesis_telescope_scope(zero) == root && pg_synthesis_telescope_body(zero) == unresolved);
	assert(!pg_synthesis_telescope(&synthesis, NULL, unresolved));
	assert(!pg_synthesis_telescope(&synthesis, root, NULL));
	assert(!pg_synthesis_telescope_scope(NULL) && !pg_synthesis_telescope_body(NULL));
	const struct pg_syntax *self = expression_syntax(typing->graph, "f:=*;");
	const struct pg_evidence *zero_map = complete(&synthesis,
		pg_synthesis_data_result(&synthesis, root, empty, empty, self), PG_SYNTHESIS_DONE);
	assert(zero_map == pg_prove_substitution(typing, empty, empty, 0, NULL));
	complete(&synthesis, pg_synthesis_data_result(&synthesis, field_scope, parameter_context, empty, self), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, pg_synthesis_data_result(&synthesis, root, parameter_context, index_context, result), PG_SYNTHESIS_REJECTED);
	assert(!pg_synthesis_data_result(&synthesis, root, empty, empty, NULL));
	assert(!pg_synthesis_data_result(&synthesis, NULL, empty, empty, self));
	assert(!pg_synthesis_data_result(&synthesis, root, image, empty, self));
	assert(!pg_synthesis_data_result(&synthesis, root, empty, image, self));
	struct pg_synthesis foreign;
	assert(pg_synthesis_init(&foreign, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	assert(!pg_synthesis_telescope(&synthesis, pg_synthesis_root(&foreign), unresolved));
	assert(!pg_synthesis_data_result(&synthesis, pg_synthesis_root(&foreign), empty, empty, self));
	pg_synthesis_destroy(&foreign);
	struct pg_synthesis_job *anonymous = pg_synthesis_telescope(&synthesis, root,
		expression_syntax(typing->graph, "f:=(@)->*;"));
	const struct pg_evidence *anonymous_context = complete(&synthesis, anonymous, PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(anonymous_context) && !pg_evidence_context(anonymous_context)->parent);
	assert(pg_synthesis_telescope_body(anonymous)->token.kind == '*');
	/* Iterative opening does not synthesize the unresolved tail or consume
	 * C stack per binding. The source parser has its own independent limit. */
	const struct pg_syntax *domain = expression_syntax(typing->graph, "f:=@;");
	const struct pg_syntax *chain = unresolved;
	for (size_t i = 0; i < 256; ++i) {
		struct pg_syntax *binder = pg_alloc(typing->graph, sizeof(*binder));
		assert(binder);
		*binder = (struct pg_syntax){.kind = PG_SYNTAX_PI, .left = domain, .right = chain};
		chain = binder;
	}
	struct pg_synthesis_job *deep = pg_synthesis_telescope(&synthesis, root, chain);
	const struct pg_context *context = pg_evidence_context(complete(&synthesis, deep, PG_SYNTHESIS_DONE));
	for (size_t i = 0; i < 256; ++i) { assert(context); context = context->parent; }
	assert(!context && pg_synthesis_telescope_body(deep) == unresolved);
	size_t jobs = synthesis.jobs.count, scopes = synthesis.scopes.count;
	uint64_t steps = synthesis.steps;
	assert(pg_synthesis_telescope(&synthesis, root, chain) == deep);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps && synthesis.jobs.count == jobs && synthesis.scopes.count == scopes);
	struct pg_synthesis_job *cycle = program(&synthesis, root, "{{T:=T;}}.T;");
	const struct pg_source_scope *pending = pg_synthesis_name_job(&synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "T", .length = 1}, cycle);
	struct pg_synthesis_job *waiting = pg_synthesis_telescope(&synthesis, pending,
		expression_syntax(typing->graph, "f:=\\x:T => x;"));
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(waiting) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(waiting));
	assert(!pg_synthesis_telescope_scope(waiting) && !pg_synthesis_result(waiting) && !synthesis.ready);
	struct pg_synthesis_job *waiting_result = pg_synthesis_data_result(&synthesis, pending, empty,
		anonymous_context, expression_syntax(typing->graph, "f:=* T;"));
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(waiting_result) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(waiting_result));
	assert(!pg_synthesis_result(waiting_result) && !synthesis.ready);
	steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps);
	/* An unresolved index domain does not block independent field formation. */
	const struct pg_syntax *pending_index_source = expression_syntax(typing->graph,
		"D:=@\\i:T=>{ready:(x:@)->* x;};");
	struct pg_synthesis_job *pending_index_schema = pg_synthesis_data_schema(&synthesis, pending, pending_index_source);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(pending_index_schema) == PG_SYNTHESIS_PENDING);
	jobs = synthesis.jobs.count;
	struct pg_synthesis_job *index_structure = pg_synthesis_telescope_structure(&synthesis, pending, pending_index_source->left);
	assert(pg_synthesis_status(index_structure) == PG_SYNTHESIS_DONE && !pg_synthesis_result(index_structure));
	const struct pg_syntax *independent = pg_synthesis_telescope_body(index_structure)->items[0].expression;
	struct pg_synthesis_job *independent_fields = pg_synthesis_telescope(&synthesis, pending, independent);
	assert(synthesis.jobs.count == jobs && pg_synthesis_status(independent_fields) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(independent_fields) && !pg_synthesis_schema_result(pending_index_schema));
	struct pg_synthesis_job *empty_pending = pg_synthesis_data_schema(&synthesis, pending,
		expression_syntax(typing->graph, "D:=@\\i:T=>{};"));
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(empty_pending) == PG_SYNTHESIS_PENDING && !pg_synthesis_schema_result(empty_pending));
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("source telescopes: shared Lambda/Pi binders, computed domains, parameter/index separation and schema maps passed");
}

static void source_declarations(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, typing->graph));
	assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	const char *sources[] = {
		"D:=@{};", "D:=@{zero:*; succ:*->*;};", "D:=@{pack:(A:@)->A->*;};",
		"D:=@{pack:(A:@)->A->*; next:*->*;};"};
	const uint64_t levels[] = {0, 0, 1, 1};
	for (size_t i = 0; i < sizeof(sources) / sizeof(*sources); ++i) {
		const struct pg_syntax *syntax = expression_syntax(typing->graph, sources[i]);
		struct pg_synthesis_job *job = pg_synthesis_request(&synthesis, root, syntax);
		unsigned steps = 0;
		while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
			assert(!pg_synthesis_result(job) && ++steps < 10000);
			pg_synthesis_advance(&synthesis, 1);
		}
		const struct pg_evidence *type = pg_synthesis_result(job);
		assert(type && pg_evidence_rule(type) == PG_INDUCTIVE_FORM);
		assert(!pg_evidence_context(type));
		uint64_t level;
		assert(pg_universe_level(pg_evidence_classifier(type), &level) && level == levels[i]);
		const struct pg_evidence *self = pg_evidence_premise(type, 0);
		assert(pg_universe_level(pg_evidence_context(self)->declared_type, &level) && level == levels[i]);
		assert(pg_term_independent(pg_evidence_subject(type)->core, pg_evidence_context(self)->binder) == 1);
		size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
		assert(pg_synthesis_request(&synthesis, root, syntax) == job);
		assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
		const struct pg_evidence *other = complete(&synthesis,
			pg_synthesis_request(&synthesis, root, expression_syntax(typing->graph, sources[i])), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(other)->core != pg_evidence_subject(type)->core);
	}
	complete(&synthesis, pg_synthesis_request(&synthesis, root,
		expression_syntax(typing->graph, "D:=@{bad:(* -> @)->*;};")), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, pg_synthesis_request(&synthesis, root,
		expression_syntax(typing->graph, "D:=@\\i:@=>{mk:* i;};")), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *nat_job = request(&synthesis, root, "Nat:=@{zero:*; succ:*->*;};");
	const struct pg_evidence *nat = complete(&synthesis, nat_job, PG_SYNTHESIS_DONE);
	struct pg_token nat_name = {.kind = PG_TOKEN_IDENT, .text = "Nat", .length = 3};
	const struct pg_source_scope *named = pg_synthesis_name_job(&synthesis, root, nat_name, nat_job);
	struct pg_synthesis_job *alias = request(&synthesis, named, "Alias:=Nat :: @;");
	complete(&synthesis, alias, PG_SYNTHESIS_DONE);
	named = pg_synthesis_name_job(&synthesis, named,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Alias", .length = 5}, alias);
	const struct pg_evidence *zero = complete(&synthesis, request(&synthesis, named, "r:=Alias.zero;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *succ = complete(&synthesis, request(&synthesis, named, "r:=Alias.succ;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *same_succ = complete(&synthesis, request(&synthesis, named, "r:=Nat.succ;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(zero) == PG_JUDGEMENT_VALUE);
	assert(pg_evidence_judgement(succ) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_evidence_subject(succ)->core == pg_evidence_subject(same_succ)->core);
	const struct pg_evidence *applied = complete(&synthesis,
		request(&synthesis, named, "r:=Alias.succ (Nat.succ Alias.zero);"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *field_binder = pg_binder(typing->graph);
	const struct pg_evidence *field_context = pg_prove_context_extension(typing, empty, field_binder, nat);
	const struct pg_source_scope *field_scope = pg_synthesis_bind(&synthesis, named,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "field", .length = 5}, field_binder, field_context);
	struct pg_synthesis_job *field_body = request(&synthesis, field_scope, "r:=field;");
	struct pg_synthesis_job *constant = pg_synthesis_constant_motive(&synthesis, empty, field_context, field_body);
	assert(constant && pg_synthesis_status(constant) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *constant_type = complete(&synthesis, constant, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(constant_type)->core == pg_return_type(classifiers, pg_evidence_subject(nat)->core));
	assert(pg_synthesis_constant_motive(&synthesis, empty, field_context, field_body) == constant);
	const struct pg_object *type_binder = pg_binder(typing->graph), *dependent_binder = pg_binder(typing->graph);
	const struct pg_evidence *type_context = pg_prove_context_extension(typing, empty, type_binder,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *dependent_type = pg_prove_value_type(typing, pg_prove_variable(typing, type_context, type_binder));
	const struct pg_evidence *dependent_context = pg_prove_context_extension(typing, type_context, dependent_binder, dependent_type);
	const struct pg_evidence *dependent_value = pg_prove_variable(typing, dependent_context, dependent_binder);
	complete(&synthesis, pg_synthesis_constant_motive(&synthesis, empty, dependent_context,
		pg_synthesis_evidence(&synthesis, dependent_value)), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *two = complete(&synthesis,
		pg_synthesis_return(&synthesis, empty, applied), PG_SYNTHESIS_DONE);
	assert(pg_evidence_classifier(two) == pg_evidence_subject(nat)->core);
	const struct pg_term *one_core = pg_evidence_subject(two)->core->as.application.argument;
	assert(one_core->kind == PG_APPLICATION && one_core->as.application.argument == pg_evidence_subject(zero)->core);
	/* Function provenance also follows a substituted thunk variable, without
	 * executing the body to discover which Lambda supplied its annotation. */
	const struct pg_evidence *succ_pi = pg_prove_classifier(typing, classifiers, empty, succ);
	const struct pg_object *function_binder = pg_binder(typing->graph);
	const struct pg_evidence *function_context = pg_prove_context_extension(typing, empty, function_binder,
		pg_prove_thunk_type(typing, classifiers, succ_pi));
	const struct pg_evidence *function_variable = pg_prove_force(typing,
		pg_prove_variable(typing, function_context, function_binder));
	assert(!pg_prove_application_body(typing, function_variable,
		pg_prove_projection(typing, function_context, zero)));
	const struct pg_evidence *function_image = pg_prove_thunk(typing, classifiers, succ);
	const struct pg_evidence *function_map = pg_prove_substitution(typing, function_context, empty, 1, &function_image);
	const struct pg_evidence *body = pg_prove_application_body(typing,
		pg_prove_reindex(typing, function_map, function_variable), zero);
	assert(body);
	const struct pg_evidence *body_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, empty, body), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(body_value)->core == one_core);
	struct pg_inductive_instance nat_instance;
	assert(pg_inductive_instance(typing, nat, &nat_instance));
	const struct pg_data_layout *nat_layout = pg_data_schema_layout(nat_instance.schema);
	const struct pg_evidence *motive_context = pg_prove_context_extension(typing, empty, pg_binder(typing->graph), nat);
	const struct pg_evidence *motive = pg_prove_return_type(typing, classifiers,
		pg_prove_projection(typing, motive_context, nat));
	const struct pg_syntax *induction = expression_syntax(typing->graph,
		"r:=Nat.zero @zero=>Nat.zero @succ k=>*k;");
	const struct pg_evidence *induction_branches[2];
	for (size_t i = 0; i < 2; ++i) {
		struct pg_synthesis_job *branch = pg_synthesis_induction_branch(&synthesis, named,
			nat, pg_data_constructor(nat_layout, i), nat_instance.parameters, motive_context, motive,
			induction->items[i].expression);
		induction_branches[i] = complete(&synthesis, branch, PG_SYNTHESIS_DONE);
		size_t proofs = typing->proofs.count, terms = typing->graph->terms.count;
		assert(pg_synthesis_induction_branch(&synthesis, named, nat, pg_data_constructor(nat_layout, i),
			nat_instance.parameters, motive_context, motive, induction->items[i].expression) == branch);
		assert(typing->proofs.count == proofs && typing->graph->terms.count == terms);
	}
	const struct pg_evidence *countdown = pg_prove_induction(typing, classifiers, nat,
		nat_instance.parameters, two, motive_context, motive, 2, induction_branches);
	assert(countdown);
	const struct pg_evidence *countdown_result = complete(&synthesis,
		pg_synthesis_return(&synthesis, empty, countdown), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(countdown_result)->core == pg_evidence_subject(zero)->core);
	const struct pg_syntax *copy_induction = expression_syntax(typing->graph,
		"r:=Nat.zero @zero=>Nat.zero @succ k=>Nat.succ *k;");
	for (size_t i = 0; i < 2; ++i)
		induction_branches[i] = complete(&synthesis, pg_synthesis_induction_branch(&synthesis, named,
			nat, pg_data_constructor(nat_layout, i), nat_instance.parameters, motive_context, motive,
			copy_induction->items[i].expression), PG_SYNTHESIS_DONE);
	const struct pg_evidence *copy = pg_prove_induction(typing, classifiers, nat,
		nat_instance.parameters, two, motive_context, motive, 2, induction_branches);
	assert(copy);
	const struct pg_evidence *copied = complete(&synthesis, pg_synthesis_nf(&synthesis, empty, copy), PG_SYNTHESIS_DONE);
	const struct pg_evidence *copied_value = pg_prove_return_value(typing, copied);
	assert(copied_value && pg_evidence_subject(copied_value)->core == pg_evidence_subject(two)->core);
	const struct pg_evidence *function_motive = pg_prove_projection(typing, motive_context,
		pg_prove_classifier(typing, classifiers, empty, succ));
	const struct pg_syntax *function_induction = expression_syntax(typing->graph,
		"r:=Nat.zero @zero=>(\\m:Nat=>m) @succ k=>(\\m:Nat=>*k m);");
	for (size_t i = 0; i < 2; ++i)
		induction_branches[i] = complete(&synthesis, pg_synthesis_induction_branch(&synthesis, named,
			nat, pg_data_constructor(nat_layout, i), nat_instance.parameters, motive_context, function_motive,
			function_induction->items[i].expression), PG_SYNTHESIS_DONE);
	const struct pg_evidence *recursive_function = pg_prove_induction(typing, classifiers,
		nat, nat_instance.parameters, two, motive_context, function_motive, 2, induction_branches);
	assert(recursive_function);
	const struct pg_evidence *function_result = complete(&synthesis, pg_synthesis_return(&synthesis, empty,
		pg_prove_application(typing, recursive_function, zero)), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(function_result)->core == pg_evidence_subject(zero)->core);
	const struct pg_syntax *shadow = expression_syntax(typing->graph,
		"r:=Nat.zero @succ k=>(\\k:Nat=>*k);");
	complete(&synthesis, pg_synthesis_induction_branch(&synthesis, named, nat,
		pg_data_constructor(nat_layout, 1), nat_instance.parameters, motive_context, motive,
		shadow->items[0].expression), PG_SYNTHESIS_UNSUPPORTED);
	/* The ordinary source path discovers the constant motive before opening
	 * IH assumptions. No expected type or explicit motive is supplied here. */
	const char *source_inductions[] = {
		"r:=(Nat.succ (Nat.succ Nat.zero)) @zero=>Nat.zero @succ k=>Nat.succ *k;",
		"r:=(\\n:Nat=>n @zero=>Nat.zero @succ k=>Nat.succ *k) (Nat.succ (Nat.succ Nat.zero));",
		"r:=(\\n:Nat=>n @succ k=>Nat.succ *k @zero=>Nat.zero) (Nat.succ (Nat.succ Nat.zero));",
		"r:=((\\n:Nat=>n @zero=>(\\m:Nat=>m) @succ k=>(\\m:Nat=>Nat.succ (*k m))) (Nat.succ Nat.zero)) (Nat.succ Nat.zero);",
		"r:=(\\n:Nat=>n @zero=>Nat.zero @succ k=>{rest:=*k; Nat.succ rest;}) (Nat.succ (Nat.succ Nat.zero));"
	};
	for (size_t i = 0; i < sizeof(source_inductions) / sizeof(*source_inductions); ++i) {
		struct pg_synthesis_job *job = request(&synthesis, named, source_inductions[i]);
		const struct pg_evidence *term = complete(&synthesis, job, PG_SYNTHESIS_DONE);
		const struct pg_evidence *nf = complete(&synthesis,
			pg_synthesis_nf(&synthesis, empty, term), PG_SYNTHESIS_DONE);
		const struct pg_evidence *result = pg_prove_return_value(typing, nf);
		assert(result && pg_evidence_subject(result)->core == pg_evidence_subject(two)->core);
		assert(pg_evidence_classifier(result) == pg_evidence_subject(nat)->core);
	}
	const struct pg_evidence *selected = complete(&synthesis, request(&synthesis, named,
		"r:=(\\n:Nat=>n @zero=>Nat.zero @succ k=>{answer:=Nat.zero; *k;}.answer) (Nat.succ Nat.zero);"),
		PG_SYNTHESIS_DONE);
	const struct pg_evidence *selected_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, empty, selected), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(selected_value)->core == pg_evidence_subject(zero)->core);
	complete(&synthesis, request(&synthesis, named,
		"r:=\\n:Nat=>n @zero=>(\\m:Nat=>m) @succ k=>(\\k:Nat=>*k);"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, request(&synthesis, named,
		"r:=\\n:Nat=>n @zero=>Nat.zero @succ k=>{k:=Nat.zero; *k;};"), PG_SYNTHESIS_UNSUPPORTED);
	/* Nested Match resolves the field through the accepted Self substitution;
	 * the first body uses the outer IH, the second shadows it with an inner IH. */
	const char *nested[] = {
		"r:=(\\n:Nat=>n @zero=>Nat.zero @succ k=>(k @zero=>*k @succ j=>*k)) (Nat.succ (Nat.succ Nat.zero));",
		"r:=(\\n:Nat=>n @zero=>Nat.zero @succ k=>(k @zero=>Nat.zero @succ k=>*k)) (Nat.succ (Nat.succ Nat.zero));"
	};
	for (size_t i = 0; i < sizeof(nested) / sizeof(*nested); ++i) {
		const struct pg_evidence *term = complete(&synthesis, request(&synthesis, named, nested[i]), PG_SYNTHESIS_DONE);
		const struct pg_evidence *result = complete(&synthesis,
			pg_synthesis_return(&synthesis, empty, term), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(result)->core == pg_evidence_subject(zero)->core);
	}
	complete(&synthesis, request(&synthesis, named, "r:=Nat.Alias;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, named, "r:=succ;"), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *other_job = request(&synthesis, root, "Other:=@{zero:*; succ:*->*;};");
	complete(&synthesis, other_job, PG_SYNTHESIS_DONE);
	named = pg_synthesis_name_job(&synthesis, named,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Other", .length = 5}, other_job);
	complete(&synthesis, request(&synthesis, named, "r:=Nat.succ Other.zero;"), PG_SYNTHESIS_REJECTED);
	const char *matches[] = {
		"r:={Nat.zero;} @zero=>Nat.zero @succ k=>k;",
		"r:=(Nat.succ Nat.zero) @zero=>Nat.zero @succ k=>k;",
		"r:=(\\n:Nat => n @succ k => k @zero => Nat.zero) (Nat.succ Nat.zero);",
		"r:=Nat.zero @Alias.zero => Nat.zero @Nat.succ k => k;",
		"r:=((\\n:Nat => n @zero => (\\m:Nat=>m) @succ k => (\\m:Nat=>m)) Nat.zero) Nat.zero;"
	};
	for (size_t i = 0; i < sizeof(matches) / sizeof(*matches); ++i) {
		const struct pg_evidence *term = complete(&synthesis, request(&synthesis, named, matches[i]), PG_SYNTHESIS_DONE);
		if (i < 2) assert(pg_evidence_rule(term) == PG_FOLD_ELIM);
		const struct pg_evidence *result = complete(&synthesis,
			pg_synthesis_return(&synthesis, empty, term), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(result)->core == pg_evidence_subject(zero)->core);
		assert(pg_evidence_classifier(result) == pg_evidence_subject(nat)->core);
	}
	const char *bad_matches[] = {
		"r:=Nat.zero @zero=>Nat.zero;",
		"r:=Nat.zero @zero=>Nat.zero @zero=>Nat.zero;",
		"r:=Nat.zero @zero k=>Nat.zero @succ k=>k;",
		"r:=Nat.zero @zero=>k @succ k=>k;",
		"r:=Nat.zero @Other.zero=>Nat.zero @succ k=>k;",
		"r:=Nat.zero @missing=>Nat.zero @succ k=>k;"
	};
	for (size_t i = 0; i < sizeof(bad_matches) / sizeof(*bad_matches); ++i)
		complete(&synthesis, request(&synthesis, named, bad_matches[i]), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *list = request(&synthesis, named, "List:=&(\\A:@=>@{nil:*; cons:A->*->*;});");
	complete(&synthesis, list, PG_SYNTHESIS_DONE);
	named = pg_synthesis_name_job(&synthesis, named,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "List", .length = 4}, list);
	complete(&synthesis, request(&synthesis, named,
		"r:=\\xs:List Nat => xs @nil=>Nat.zero @cons x rest=>x;"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, named, "r:=(List Nat).nil;"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, named,
		"r:=(List Nat).cons Nat.zero (List Nat).nil;"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, named,
		"r:=\\x:Nat => (List Nat).cons x (List Nat).nil;"), PG_SYNTHESIS_DONE);
	alias = request(&synthesis, named, "L:=List Nat;");
	complete(&synthesis, alias, PG_SYNTHESIS_DONE);
	named = pg_synthesis_name_job(&synthesis, named,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "L", .length = 1}, alias);
	complete(&synthesis, request(&synthesis, named, "r:=L.nil;"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, named, "r:=L.missing;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, named,
		"r:=(List Nat).cons Other.zero (List Nat).nil;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *head = complete(&synthesis, request(&synthesis, named,
		"r:=((List Nat).cons Nat.zero L.nil) @nil=>Nat.zero @cons x rest=>x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *head_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, empty, head), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(head_value)->core == pg_evidence_subject(zero)->core);
	const struct pg_evidence *length = complete(&synthesis, request(&synthesis, named,
		"r:=(\\xs:List Nat=>xs @cons x rest=>Nat.succ *rest @nil=>Nat.zero) ((List Nat).cons Nat.zero ((List Nat).cons Nat.zero L.nil));"),
		PG_SYNTHESIS_DONE);
	const struct pg_evidence *length_nf = complete(&synthesis,
		pg_synthesis_nf(&synthesis, empty, length), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(pg_prove_return_value(typing, length_nf))->core == pg_evidence_subject(two)->core);
	const struct pg_evidence *direct_length = complete(&synthesis, request(&synthesis, named,
		"r:=((List Nat).cons Nat.zero ((List Nat).cons Nat.zero L.nil)) @cons x rest=>Nat.succ *rest @nil=>Nat.zero;"),
		PG_SYNTHESIS_DONE);
	const struct pg_evidence *direct_nf = complete(&synthesis,
		pg_synthesis_nf(&synthesis, empty, direct_length), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(pg_prove_return_value(typing, direct_nf))->core == pg_evidence_subject(two)->core);
	complete(&synthesis, request(&synthesis, named,
		"r:=\\A:@=>\\x:A=>((List A).cons x ((List A).cons x (List A).nil)) @cons y rest=>Nat.succ *rest @nil=>Nat.zero;"),
		PG_SYNTHESIS_DONE);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("source declarations: nominal formation, conditional universe candidates, no early publication and reuse passed");
}

static void source_schemas(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	const struct pg_syntax *source = expression_syntax(typing->graph,
		"Box:=\\A:@ => @\\i:A => {one:(x:A)->* x; two:(x:A)->(y:A)->* y;};");
	struct pg_synthesis_job *parameters = pg_synthesis_telescope(&synthesis, root, source);
	const struct pg_evidence *parameter_context = complete(&synthesis, parameters, PG_SYNTHESIS_DONE);
	const struct pg_source_scope *scope = pg_synthesis_telescope_scope(parameters);
	const struct pg_syntax *declaration = pg_synthesis_telescope_body(parameters);
	size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
	struct pg_synthesis_job *job = pg_synthesis_data_schema(&synthesis, scope, declaration);
	assert(job && pg_synthesis_data_schema(&synthesis, scope, declaration) == job);
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
	assert(!pg_synthesis_schema_result(job));
	unsigned transitions = 0;
	while (pg_synthesis_status(job) == PG_SYNTHESIS_PENDING) {
		assert(++transitions < 10000);
		assert(!pg_synthesis_schema_result(job) && !pg_synthesis_result(job));
		pg_synthesis_advance(&synthesis, 1);
	}
	assert(pg_synthesis_status(job) == PG_SYNTHESIS_DONE && !pg_synthesis_result(job));
	const struct pg_data_schema *schema = pg_synthesis_schema_result(job);
	assert(schema);
	uint64_t field_level;
	assert(!pg_data_schema_field_level(schema, &field_level) && field_level == 0);
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	assert(pg_data_constructor(layout, 0) && pg_data_constructor(layout, 1) && !pg_data_constructor(layout, 2));
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_object *ctor = pg_data_constructor(layout, i);
		const struct pg_evidence *fields = pg_data_schema_fields(schema, ctor);
		size_t count;
		assert(pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(parameter_context), &count) == 0);
		assert(count == i + 1);
		const struct pg_evidence *map = pg_data_schema_result(schema, ctor);
		assert(pg_evidence_premise_count(map) == 4);
		assert(pg_evidence_subject(pg_evidence_premise(map, 3))->core == pg_reference(typing->graph, pg_evidence_context(fields)->binder));
	}
	terms = typing->graph->terms.count; proofs = typing->proofs.count;
	uint64_t steps = synthesis.steps;
	assert(pg_synthesis_data_schema(&synthesis, scope, declaration) == job);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_schema_result(job) == schema && synthesis.steps == steps);
	assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
	struct pg_synthesis_job *other = pg_synthesis_data_schema(&synthesis, scope,
		expression_syntax(typing->graph, "Other:=@\\i:A=>{one:(x:A)->* x; two:(x:A)->(y:A)->* y;};"));
	complete(&synthesis, other, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_schema_result(other) != schema);
	assert(pg_data_constructor(pg_data_schema_layout(pg_synthesis_schema_result(other)), 0) != pg_data_constructor(layout, 0));
	const char *empty[] = {"Empty:=@{};", "Flag:=@{off:*; on:*;};"};
	for (size_t i = 0; i < 2; ++i) {
		struct pg_synthesis_job *simple = pg_synthesis_data_schema(&synthesis, root, expression_syntax(typing->graph, empty[i]));
		complete(&synthesis, simple, PG_SYNTHESIS_DONE);
		const struct pg_data_schema *result = pg_synthesis_schema_result(simple);
		assert(result && !pg_evidence_context(pg_data_schema_indices(result)));
		assert(!pg_data_constructor(pg_data_schema_layout(result), i * 2));
	}
	/* Admission must use synthesized field formations, including pure type
	 * computations, rather than guessing a universe from source syntax. */
	const char *type_fields[] = {"D:=@{mk:@->*;};", "D:=@{mk:((\\X:@=>@) A)->*;};"};
	for (size_t i = 0; i < sizeof(type_fields) / sizeof(*type_fields); ++i) {
		struct pg_synthesis_job *typed = pg_synthesis_data_schema(&synthesis, scope,
			expression_syntax(typing->graph, type_fields[i]));
		complete(&synthesis, typed, PG_SYNTHESIS_DONE);
		const struct pg_data_schema *result = pg_synthesis_schema_result(typed);
		assert(result && !pg_synthesis_result(typed));
		assert(!pg_data_schema_field_level(result, &field_level) && field_level == 1);
	}
	const char *bad[] = {
		"D:=@{a:*; a:*;};", "D:=@{a:*; b:Missing;};", "D:=@{a:Missing->*;};",
		"D:=@\\i:A=>{a:*;};", "D:=@\\i:A=>{a:i->* i;};", "D:=@{a:\\x:A=>*;};",
		"D:=@{mk:((\\X:@=>X) (@))->*;};"
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
		struct pg_synthesis_job *failed = pg_synthesis_data_schema(&synthesis, scope, expression_syntax(typing->graph, bad[i]));
		complete(&synthesis, failed, PG_SYNTHESIS_REJECTED);
		assert(!pg_synthesis_schema_result(failed));
	}
	struct pg_synthesis_job *recursive = pg_synthesis_data_schema(&synthesis, root,
		expression_syntax(typing->graph, "Nat:=@{zero:*; succ:*->*;};"));
	complete(&synthesis, recursive, PG_SYNTHESIS_UNSUPPORTED);
	assert(!pg_synthesis_schema_result(recursive));
	/* Check recursive fields conditionally, without admitting the fixpoint. */
	const struct pg_evidence *empty_context = pg_prove_empty_context(typing);
	const struct pg_object *self = pg_binder(typing->graph);
	const struct pg_evidence *self_context = pg_prove_context_extension(typing, empty_context, self,
		pg_prove_universe(typing, classifiers, empty_context, 0));
	const struct pg_source_scope *self_scope = pg_synthesis_bind(&synthesis, root,
		(struct pg_token){.kind = '*'}, self, self_context);
	assert(self_scope && pg_synthesis_bind(&synthesis, root,
		(struct pg_token){.kind = '*', .text = "*", .length = 1}, self, self_context) == self_scope);
	const struct pg_evidence *self_type = complete(&synthesis,
		request(&synthesis, self_scope, "S:=*;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(self_type) == pg_evidence_context(self_context));
	assert(pg_evidence_subject(self_type)->core == pg_reference(typing->graph, self));
	assert(!pg_prove_projection(typing, empty_context, self_type));
	struct pg_synthesis_job *conditional = pg_synthesis_data_schema(&synthesis, self_scope,
		expression_syntax(typing->graph, "Nat:=@{zero:*; succ:*->*;};"));
	complete(&synthesis, conditional, PG_SYNTHESIS_DONE);
	const struct pg_data_schema *conditional_schema = pg_synthesis_schema_result(conditional);
	assert(conditional_schema && !pg_synthesis_result(conditional));
	assert(pg_evidence_context(pg_data_schema_indices(conditional_schema)) == pg_evidence_context(self_context));
	assert(pg_data_schema_positive(conditional_schema, self) == 1);
	assert(!pg_data_schema_field_level(conditional_schema, &field_level) && field_level == 0);
	const struct pg_object *successor = pg_data_constructor(pg_data_schema_layout(conditional_schema), 1);
	const struct pg_evidence *successor_fields = pg_data_schema_fields(conditional_schema, successor);
	assert(pg_evidence_context(successor_fields)->declared_type == pg_reference(typing->graph, self));
	assert(pg_evidence_context(successor_fields)->parent == pg_evidence_context(self_context));
	const struct pg_evidence *admitted = pg_prove_inductive_type(typing, classifiers, conditional_schema);
	assert(admitted && !pg_evidence_context(admitted));
	const struct pg_source_scope *nat_scope = pg_synthesis_name(&synthesis, root,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Nat", .length = 3}, admitted);
	const struct pg_evidence *parameter_map = pg_prove_substitution(typing, empty_context, empty_context, 0, NULL);
	const struct pg_evidence *zero_value = pg_prove_constructor(typing, admitted,
		pg_data_constructor(pg_data_schema_layout(conditional_schema), 0), parameter_map, 0, NULL);
	assert(zero_value);
	nat_scope = pg_synthesis_name(&synthesis, nat_scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "zero", .length = 4}, zero_value);
	assert(nat_scope);
	const struct pg_evidence *application = complete(&synthesis,
		request(&synthesis, nat_scope, "v:=(\\n:Nat=>n) zero;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_classifier(application) == pg_return_type(classifiers, pg_evidence_subject(admitted)->core));
	const struct pg_evidence *succ_function = pg_prove_constructor_function(typing, classifiers,
		admitted, successor, parameter_map);
	assert(succ_function);
	nat_scope = pg_synthesis_name(&synthesis, nat_scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "succ", .length = 4}, succ_function);
	assert(nat_scope);
	const struct pg_evidence *successor_call = complete(&synthesis,
		request(&synthesis, nat_scope, "v:=succ zero;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_classifier(successor_call) == pg_return_type(classifiers, pg_evidence_subject(admitted)->core));
	/* A value of Self cannot shadow the type assumption as another type. */
	const struct pg_object *element = pg_binder(typing->graph);
	const struct pg_evidence *element_context = pg_prove_context_extension(typing, self_context, element,
		pg_prove_value_type(typing, pg_prove_variable(typing, self_context, self)));
	const struct pg_source_scope *bad_self = pg_synthesis_bind(&synthesis, self_scope,
		(struct pg_token){.kind = '*'}, element, element_context);
	assert(bad_self);
	complete(&synthesis, request(&synthesis, bad_self, "S:=*;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, root, "S:=*;"), PG_SYNTHESIS_UNSUPPORTED);
	assert(!pg_synthesis_data_schema(&synthesis, NULL, declaration));
	assert(!pg_synthesis_data_schema(&synthesis, root, NULL));
	assert(!pg_synthesis_data_schema(&synthesis, root, source));
	assert(!pg_synthesis_schema_result(parameters) && !pg_synthesis_schema_result(NULL));
	struct pg_synthesis foreign;
	assert(pg_synthesis_init(&foreign, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	assert(!pg_synthesis_data_schema(&synthesis, pg_synthesis_root(&foreign), declaration));
	pg_synthesis_destroy(&foreign);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "D", .length = 1};
	const struct pg_source_scope *named = pg_synthesis_name_job(&synthesis, root, name, job);
	complete(&synthesis, request(&synthesis, named, "v:=D;"), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *cycle = program(&synthesis, root, "{{T:=T;}}.T;");
	named = pg_synthesis_name_job(&synthesis, root, (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "T", .length = 1}, cycle);
	struct pg_synthesis_job *waiting = pg_synthesis_data_schema(&synthesis, named,
		expression_syntax(typing->graph, "D:=@{ok:*; pending:T->*;};"));
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(waiting) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(waiting));
	assert(!pg_synthesis_schema_result(waiting) && !synthesis.ready);
	/* A pending first constructor must not prevent later result-map work. */
	named = pg_synthesis_name_job(&synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "T", .length = 1}, cycle);
	const struct pg_syntax *pending_source = expression_syntax(typing->graph,
		"D:=@\\i:@=>{pending:* T; ready:* A;};");
	waiting = pg_synthesis_data_schema(&synthesis, named, pending_source);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(waiting) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(waiting));
	size_t jobs = synthesis.jobs.count;
	struct pg_synthesis_job *indices = pg_synthesis_telescope(&synthesis, named, pending_source->left);
	const struct pg_syntax *ready = pg_synthesis_telescope_body(indices)->items[1].expression;
	struct pg_synthesis_job *ready_fields = pg_synthesis_telescope(&synthesis, named, ready);
	struct pg_synthesis_job *ready_map = pg_synthesis_data_result(&synthesis,
		pg_synthesis_telescope_scope(ready_fields), parameter_context, pg_synthesis_result(indices), ready);
	assert(synthesis.jobs.count == jobs && pg_synthesis_status(ready_map) == PG_SYNTHESIS_DONE);
	assert(!pg_synthesis_schema_result(waiting) && !synthesis.ready);
	steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("source schemas: complete constructor maps, shared owners, rejection and no premature datatype admission passed");
}

static void data_cases(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work work;
	struct pg_synthesis split, whole;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_synthesis_init(&split, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	assert(pg_synthesis_init(&whole, typing, classifiers, &work, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph), *p = pg_binder(graph), *i = pg_binder(graph);
	const struct pg_evidence *parameters = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *first = pg_prove_context_extension(typing, parameters, x,
		pg_prove_variable(typing, parameters, a));
	const struct pg_evidence *xv = pg_prove_variable(typing, first, x);
	const struct pg_evidence *id = pg_prove_identity_type(typing,
		pg_prove_value_type(typing, pg_prove_variable(typing, first, a)), xv, xv);
	const struct pg_evidence *fields = pg_prove_context_extension(typing, first, p, id);
	const struct pg_evidence *index_first = pg_prove_context_extension(typing, parameters, i,
		pg_prove_variable(typing, parameters, a));
	const struct pg_evidence *index_value = pg_prove_variable(typing, index_first, i);
	const struct pg_evidence *index_identity = pg_prove_identity_type(typing,
		pg_prove_value_type(typing, pg_prove_variable(typing, index_first, a)), index_value, index_value);
	const struct pg_evidence *indices = pg_prove_context_extension(typing, index_first, pg_binder(graph), index_identity);
	const struct pg_evidence *images[] = {pg_prove_variable(typing, fields, a), pg_prove_variable(typing, fields, x),
		pg_prove_variable(typing, fields, p)};
	const struct pg_evidence *result_map = pg_prove_substitution(typing, indices, fields, 3, images);
	const struct pg_data_schema *schema = pg_data_schema(typing, pg_data_signature(typing, parameters, indices), 1, &result_map);
	assert(schema);
	const struct pg_object *ctor = pg_data_constructor(pg_data_schema_layout(schema), 0);
	const struct pg_evidence *iv = pg_prove_variable(typing, indices, i);
	const struct pg_evidence *index_a = pg_prove_value_type(typing, pg_prove_variable(typing, indices, a));
	const struct pg_evidence *motive = pg_prove_return_type(typing, classifiers,
		pg_prove_identity_type(typing, index_a, iv, iv));
	struct pg_token names[] = {{.kind = PG_TOKEN_IDENT, .length = 1, .text = "A"},
		{.kind = PG_TOKEN_IDENT, .length = 1, .text = "x"}, {.kind = PG_TOKEN_IDENT, .length = 1, .text = "p"}};
	const struct pg_evidence *contexts[] = {parameters, first, fields};
	const struct pg_object *binders[] = {a, x, p};
	const struct pg_source_scope *scope = pg_synthesis_root(&split), *other_scope = pg_synthesis_root(&whole);
	for (size_t n = 0; n < 3; ++n) {
		scope = pg_synthesis_bind(&split, scope, names[n], binders[n], contexts[n]);
		other_scope = pg_synthesis_bind(&whole, other_scope, names[n], binders[n], contexts[n]);
		assert(scope && other_scope);
	}
	const char *result_sources[] = {"r:=* x p;", "r:=* ((\\z:A => z) x) p;"};
	for (size_t n = 0; n < sizeof(result_sources) / sizeof(*result_sources); ++n) {
		const struct pg_syntax *syntax = expression_syntax(graph, result_sources[n]);
		size_t prior_terms = graph->terms.count, prior_proofs = typing->proofs.count;
		struct pg_synthesis_job *result = pg_synthesis_data_result(&split, scope, parameters, indices, syntax);
		assert(result && pg_synthesis_data_result(&split, scope, parameters, indices, syntax) == result);
		assert(graph->terms.count == prior_terms && typing->proofs.count == prior_proofs);
		struct pg_synthesis_job *bulk = pg_synthesis_data_result(&whole, other_scope, parameters, indices, syntax);
		pg_synthesis_advance(&whole, 10000);
		assert(pg_synthesis_status(bulk) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *map = complete(&split, result, PG_SYNTHESIS_DONE);
		assert(pg_evidence_rule(map) == PG_CONTEXT_SUBSTITUTION && pg_evidence_context(map) == pg_evidence_context(fields));
		assert(pg_evidence_premise(map, 0) == indices && pg_evidence_premise(map, 1) == fields);
		for (size_t j = 0; j < 3; ++j) {
			same_judgement(pg_evidence_premise(map, j + 2), images[j]);
			same_judgement(pg_evidence_premise(map, j + 2), pg_evidence_premise(pg_synthesis_result(bulk), j + 2));
		}
		assert(pg_data_schema(typing, pg_data_signature(typing, parameters, indices), 1, &map));
		if (!n) {
			const struct pg_source_scope *shadow = pg_synthesis_name(&split, scope, names[0], images[1]);
			const struct pg_evidence *shadow_map = complete(&split,
				pg_synthesis_data_result(&split, shadow, parameters, indices, syntax), PG_SYNTHESIS_DONE);
			/* Parameter identity comes from its binder, not the shadowed name. */
			assert(pg_evidence_subject(pg_evidence_premise(shadow_map, 2))->core == pg_reference(graph, a));
		}
		uint64_t steps = split.steps;
		assert(pg_synthesis_data_result(&split, scope, parameters, indices, syntax) == result);
		pg_synthesis_advance(&split, 100);
		assert(split.steps == steps);
	}
	const char *invalid_results[] = {"r:=*;", "r:=* x;", "r:=* x p p;", "r:=Family x p;", "r:=* p x;", "r:=* x x;"};
	for (size_t n = 0; n < sizeof(invalid_results) / sizeof(*invalid_results); ++n)
		complete(&split, pg_synthesis_data_result(&split, scope, parameters, indices,
			expression_syntax(graph, invalid_results[n])), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *body = request(&split, scope, "body := { p; };");
	struct pg_synthesis_job *other_body = request(&whole, other_scope, "body := { p; };");
	const struct pg_evidence *prior = complete(&whole, other_body, PG_SYNTHESIS_DONE);
	size_t terms = graph->terms.count, proofs = typing->proofs.count;
	struct pg_synthesis_job *job = pg_synthesis_data_case(&split, body, schema, ctor, motive);
	struct pg_synthesis_job *other = pg_synthesis_data_case(&whole, other_body, schema, ctor, motive);
	assert(job && other && pg_synthesis_data_case(&split, body, schema, ctor, motive) == job);
	assert(graph->terms.count == terms && typing->proofs.count == proofs);
	wait_on(&split, job, body);
	assert(pg_synthesis_status(body) == PG_SYNTHESIS_PENDING);
	complete(&split, body, PG_SYNTHESIS_DONE);
	pg_synthesis_advance(&split, 1);
	struct pg_synthesis_job *mapping = pg_synthesis_reindex(&split, result_map, motive);
	assert(mapping && pg_synthesis_dependency(job) == mapping);
	pg_synthesis_advance(&split, 1);
	assert(pg_synthesis_status(mapping) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(mapping));
	const struct pg_evidence *checked = complete(&split, job, PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(mapping) == pg_data_branch_motive(typing, schema, ctor, motive));
	pg_synthesis_advance(&whole, 100000);
	assert(pg_synthesis_status(other) == PG_SYNTHESIS_DONE);
	same_judgement(checked, pg_synthesis_result(other));
	const struct pg_evidence *produced = pg_synthesis_result(body);
	same_judgement(prior, produced);
	const struct pg_evidence *leaf = checked;
	while (pg_evidence_rule(leaf) == PG_LAMBDA_INTRO) leaf = pg_evidence_premise(leaf, 1);
	assert(pg_evidence_rule(leaf) == PG_TYPE_CONVERSION && pg_evidence_premise(leaf, 0) == pg_synthesis_result(body));
	assert(pg_evidence_premise(pg_evidence_premise(leaf, 1), 0) == result_map);
	uint64_t steps = split.steps;
	assert(pg_synthesis_data_case(&split, body, schema, ctor, motive) == job);
	pg_synthesis_advance(&split, 1000);
	assert(split.steps == steps && pg_synthesis_result(job) == checked);
	const struct pg_evidence *wrong = pg_prove_return_type(typing, classifiers, index_a);
	struct pg_synthesis_job *mismatch = pg_synthesis_data_case(&split, body, schema, ctor, wrong);
	assert(mismatch && mismatch != job);
	complete(&split, mismatch, PG_SYNTHESIS_REJECTED);
	assert(pg_synthesis_result(body) == produced);
	assert(!pg_synthesis_data_case(&split, other_body, schema, ctor, motive));
	assert(!pg_synthesis_data_case(&split, body, schema, a, motive));
	assert(!pg_synthesis_data_case(&split, body, NULL, ctor, motive));
	assert(!pg_synthesis_data_case(&split, body, schema, ctor, index_a));
	assert(!pg_synthesis_data_case(&split, body, schema, ctor, pg_prove_classifier(typing, classifiers, fields, prior)));
	assert(!pg_synthesis_reindex(&split, empty, motive));
	assert(!pg_synthesis_reindex(&split, result_map, fields));
	assert(!pg_synthesis_reindex(&split, result_map, prior));
	struct pg_synthesis_job *value = request(&split, scope, "value := p;");
	complete(&split, pg_synthesis_data_case(&split, value, schema, ctor, motive), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *bad = request(&split, scope, "bad := missing;");
	complete(&split, pg_synthesis_data_case(&split, bad, schema, ctor, motive), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *outside = request(&split, pg_synthesis_root(&split), "outside := \\x : @ => x;");
	complete(&split, pg_synthesis_data_case(&split, outside, schema, ctor, motive), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *type_body = request(&split, scope, "type := @{ nil : *; };");
	complete(&split, type_body, PG_SYNTHESIS_DONE);
	complete(&split, pg_synthesis_data_case(&split, type_body, schema, ctor, motive), PG_SYNTHESIS_REJECTED);
	struct pg_synthesis_job *unsupported = request(&split, scope, "type := @\\i:A => { nil : * i; };");
	complete(&split, pg_synthesis_data_case(&split, unsupported, schema, ctor, motive), PG_SYNTHESIS_UNSUPPORTED);
	/* Assemble the acted result map through scheduled, explicit post-checks. */
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	const struct pg_binding_face *field_centers[2], *index_centers[2];
	for (size_t n = 0; n < 2; ++n) {
		field_centers[n] = pg_binding_face(&dimensions, pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
		index_centers[n] = pg_binding_face(&dimensions, pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	}
	const struct pg_evidence *left, *right, *paths[2], *index_left, *index_right, *index_paths[2];
	const struct pg_evidence *boundary = pg_identity_context(typing, &dimensions, fields, 2, field_centers, &left, &right, paths);
	const struct pg_evidence *target = pg_identity_context(typing, &dimensions, indices, 2, index_centers,
		&index_left, &index_right, index_paths);
	assert(boundary && target);
	const struct pg_evidence *acted_images[2], *direct_images[2];
	struct pg_synthesis_job *image_jobs[2], *bulk_image_jobs[2];
	terms = graph->terms.count;
	proofs = typing->proofs.count;
	steps = split.steps;
	for (size_t n = 0; n < 2; ++n) {
		const struct pg_evidence *image = pg_evidence_premise(result_map, n + 3);
		struct pg_synthesis_job *producer = pg_synthesis_evidence(&split, image);
		assert(producer && pg_synthesis_result(producer) == image);
		assert(pg_synthesis_evidence(&split, image) == producer);
		assert(!pg_synthesis_dependency(producer));
		image_jobs[n] = pg_synthesis_family_action(&split, producer, left, right, 2, paths);
		bulk_image_jobs[n] = pg_synthesis_family_action(&whole,
			pg_synthesis_evidence(&whole, image), left, right, 2, paths);
		assert(image_jobs[n] && bulk_image_jobs[n]);
		assert(pg_synthesis_family_action(&split, producer, left, right, 2, paths) == image_jobs[n]);
	}
	assert(graph->terms.count == terms && typing->proofs.count == proofs && split.steps == steps);
	pg_synthesis_advance(&split, 1);
	assert(!pg_synthesis_result(image_jobs[0]) && !pg_synthesis_result(image_jobs[1]));
	pg_synthesis_advance(&whole, 100000);
	for (size_t n = 0; n < 2; ++n) {
		acted_images[n] = complete(&split, image_jobs[n], PG_SYNTHESIS_DONE);
		same_judgement(acted_images[n], pg_synthesis_result(bulk_image_jobs[n]));
	}
	assert(pg_identity_substitution_images(typing, classifiers, result_map, left, right, 2, paths, 2, direct_images) == 0);
	for (size_t n = 0; n < 2; ++n) same_judgement(acted_images[n], direct_images[n]);
	steps = split.steps;
	pg_synthesis_advance(&split, 1000);
	assert(split.steps == steps);
	const struct pg_evidence *endpoints[] = {pg_data_result(typing, schema, ctor, left), pg_data_result(typing, schema, ctor, right)};
	const struct pg_evidence *values[7] = {pg_evidence_premise(endpoints[0], 2)};
	for (size_t n = 0; n < 2; ++n) {
		values[1 + 3 * n] = pg_evidence_premise(endpoints[0], n + 3);
		values[2 + 3 * n] = pg_evidence_premise(endpoints[1], n + 3);
		values[3 + 3 * n] = acted_images[n];
	}
	const struct pg_evidence *declarations[7], *declaration = target;
	for (size_t n = 7; n; --n) {
		declarations[n - 1] = declaration;
		declaration = pg_evidence_premise(declaration, 0);
	}
	const struct pg_evidence *sigma = pg_prove_substitution(typing, declaration, boundary, 0, NULL);
	const struct pg_evidence *bulk_sigma = sigma;
	for (size_t n = 0; n < 7; ++n) {
		terms = graph->terms.count;
		proofs = typing->proofs.count;
		struct pg_synthesis_job *pair = pg_synthesis_substitution_pair(&split, sigma, declarations[n], values[n]);
		struct pg_synthesis_job *bulk_pair = pg_synthesis_substitution_pair(&whole, bulk_sigma, declarations[n], values[n]);
		assert(pair && bulk_pair && graph->terms.count == terms && typing->proofs.count == proofs);
		assert(pg_synthesis_substitution_pair(&split, sigma, declarations[n], values[n]) == pair);
		const struct pg_evidence *prefix = sigma;
		sigma = complete(&split, pair, PG_SYNTHESIS_DONE);
		pg_synthesis_advance(&whole, 100000);
		bulk_sigma = pg_synthesis_result(bulk_pair);
		assert(bulk_sigma);
		const struct pg_evidence *converted = pg_evidence_premise(sigma, n + 2);
		assert(pg_evidence_rule(converted) == PG_TYPE_CONVERSION && pg_evidence_premise(converted, 0) == values[n]);
		assert(sigma == pg_prove_substitution_pair(typing, prefix, declarations[n], converted));
		steps = split.steps;
		assert(pg_synthesis_substitution_pair(&split, prefix, declarations[n], values[n]) == pair);
		pg_synthesis_advance(&split, 1000);
		assert(split.steps == steps);
	}
	assert(pg_evidence_context(sigma) == pg_evidence_context(bulk_sigma));
	for (size_t n = 0; n < 7; ++n)
		same_judgement(pg_evidence_premise(sigma, n + 2), pg_evidence_premise(bulk_sigma, n + 2));
	const struct pg_evidence *projections[] = {index_left, index_right};
	for (size_t side = 0; side < 2; ++side) {
		const struct pg_evidence *composite = pg_prove_substitution_compose(typing, projections[side], sigma);
		assert(composite);
		for (size_t n = 0; n < 3; ++n)
			assert(pg_alpha_equal(pg_evidence_subject(pg_evidence_premise(composite, n + 2))->core,
				pg_evidence_subject(pg_evidence_premise(endpoints[side], n + 2))->core) == 1);
	}
	assert(!pg_synthesis_substitution_pair(&split, sigma, empty, values[0]));
	assert(!pg_synthesis_substitution_pair(&split, sigma, target, produced));
	const struct pg_evidence *seed = pg_prove_substitution(typing, declaration, boundary, 0, NULL);
	complete(&split, pg_synthesis_substitution_pair(&split, seed, declarations[0], values[3]), PG_SYNTHESIS_REJECTED);
	assert(pg_evidence_premise_count(seed) == 2);
	pg_dimensions_destroy(&dimensions);
	struct pg_synthesis_job *cycle = program(&split, scope, "{{ f := g; g := f; }}.f;");
	struct pg_synthesis_job *waiting = pg_synthesis_data_case(&split, cycle, schema, ctor, motive);
	pg_synthesis_advance(&split, 1000);
	assert(pg_synthesis_status(waiting) == PG_SYNTHESIS_PENDING && pg_synthesis_cycle(waiting));
	assert(!pg_synthesis_result(waiting));
	/* Destroy while a distinct reindex job still owns suspended traversal. */
	struct pg_synthesis_job *unfinished = pg_synthesis_reindex(&split, result_map, pg_prove_type_value(typing, index_a));
	assert(unfinished);
	pg_synthesis_advance(&split, 1);
	assert(pg_synthesis_status(unfinished) == PG_SYNTHESIS_PENDING);
	pg_synthesis_destroy(&split);
	pg_synthesis_destroy(&whole);
	pg_whnf_work_destroy(&work);
	puts("case synthesis: independent producers, index motives, post-check evidence and shared scheduling passed");
}

static void synthesis_lifetime(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_whnf_work work;
	struct pg_synthesis synthesis;
	assert(!pg_whnf_work_init(&work, typing->graph));
	assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
	const struct pg_source_scope *old_scope = pg_synthesis_root(&synthesis);
	struct pg_synthesis_job *done = request(&synthesis, old_scope, "T:=@;");
	const struct pg_evidence *proof = complete(&synthesis, done, PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *pending = request(&synthesis, old_scope, "p:=\\x:@=>x;");
	assert(pg_synthesis_status(pending) == PG_SYNTHESIS_PENDING);
	pg_synthesis_destroy(&synthesis);
	assert(!pg_synthesis_init(&synthesis, typing, classifiers, &work, PG_DEFINITION_EXPLICIT_THUNK));
	const struct pg_source_scope *fresh = pg_synthesis_root(&synthesis);
	const struct pg_syntax *syntax = expression_syntax(typing->graph, "T:=@;");
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "T", .length = 1};
	size_t jobs = synthesis.jobs.count, scopes = synthesis.scopes.count;
	assert(!pg_synthesis_request(&synthesis, old_scope, syntax));
	assert(!pg_synthesis_name_job(&synthesis, fresh, name, done));
	assert(!pg_synthesis_name_job(&synthesis, fresh, name, pending));
	assert(!pg_synthesis_import_scope(&synthesis, fresh, old_scope));
	assert(synthesis.jobs.count == jobs && synthesis.scopes.count == scopes);
	/* Accepted evidence belongs to typing, not the discarded Solve queue. */
	struct pg_synthesis_job *retained = pg_synthesis_evidence(&synthesis, proof);
	assert(retained && retained != done && pg_synthesis_result(retained) == proof);
	assert(pg_synthesis_evidence(&synthesis, proof) == retained);
	assert(complete(&synthesis, pg_synthesis_request(&synthesis, fresh, syntax), PG_SYNTHESIS_DONE) == proof);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&work);
	puts("synthesis lifetime: stale scopes/jobs rejected; retained typing evidence reused");
}

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	struct pg_whnf_work beta;
	struct pg_synthesis synthesis;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	synthesis_lifetime(&typing, &classifiers);
	accepted_inputs(&typing, &classifiers);
	pending_names(&typing, &classifiers);
	source_imports(&typing, &classifiers);
	source_telescopes(&typing, &classifiers);
	source_schemas(&typing, &classifiers);
	source_declarations(&typing, &classifiers);
	definition_selections(&typing, &classifiers);
	fair_work(&typing, &classifiers);
	endpoint_jobs(&typing, &classifiers);
	substitution_jobs(&typing, &classifiers);
	square_template_jobs(&typing, &classifiers);
	dependent_cube_substitution(&typing, &classifiers);
	dependent_application_jobs(&typing, &classifiers);
	recursive_field_aliases(&typing, &classifiers);
	identity_instance_jobs(&typing, &classifiers);
	cube_application_jobs(&typing, &classifiers, 1);
	cube_application_jobs(&typing, &classifiers, 2);
	shared_conversion_jobs(&typing, &classifiers);
	library_levels(&typing, &classifiers);
	named_identity(&typing, &classifiers);
	named_transport(&typing, &classifiers);
	data_cases(&typing, &classifiers);
	source_actions(&typing, &classifiers);
	family_transport(&typing, &classifiers);
	selected_instances(&typing, &classifiers);
	function_eta(&typing, &classifiers);
	assert(pg_whnf_work_init(&beta, &graph) == 0);
	assert(pg_synthesis_init(&synthesis, &typing, &classifiers, &beta, PG_DEFINITION_IMPLICIT_THUNK) == 0);
	const struct pg_source_scope *root = pg_synthesis_root(&synthesis);
	struct pg_synthesis_job *polymorphic = request(&synthesis, root, "id := \\A : @ => \\x : A => x;");
	pg_synthesis_advance(&synthesis, 0);
	assert(pg_synthesis_status(polymorphic) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *identity = complete(&synthesis, polymorphic, PG_SYNTHESIS_DONE);
	assert(pg_evidence_judgement(identity) == PG_JUDGEMENT_COMPUTATION);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	assert(pg_pi_view(pg_evidence_classifier(identity), &domain, &binder, &codomain));
	assert(domain == pg_universe(&classifiers, 0));
	assert(pg_pi_view(codomain, &domain, &binder, &codomain));
	assert(pg_return_type_view(codomain, &codomain) && codomain == domain);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	const struct pg_evidence *universe = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *x = pg_binder(&graph);
	const struct pg_evidence *a_context = pg_prove_context_extension(&typing, empty, a, universe);
	struct pg_token a_name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "A"};
	struct pg_token x_name = {.kind = PG_TOKEN_IDENT, .length = 1, .text = "x"};
	const struct pg_source_scope *a_scope = pg_synthesis_bind(&synthesis, root, a_name, a, a_context);
	const struct pg_evidence *a_type = pg_prove_variable(&typing, a_context, a);
	const struct pg_evidence *typed_application = pg_prove_application(&typing,
		pg_prove_projection(&typing, a_context, identity), a_type);
	const struct pg_evidence *typed_reduct = normalize(&synthesis, a_context, typed_application);
	assert(typed_reduct && pg_evidence_subject(typed_reduct)->core->kind == PG_LAMBDA);
	assert(pg_evidence_subject(typed_application)->core->kind == PG_APPLICATION);
	assert(pg_alpha_equal(pg_evidence_classifier(typed_application), pg_evidence_classifier(typed_reduct)) == 1);
	size_t reduction_terms = graph.terms.count, reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) assert(normalize(&synthesis, a_context, typed_application) == typed_reduct);
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_evidence *x_context = pg_prove_context_extension(&typing, a_context, x, a_type);
	const struct pg_evidence *x_value = pg_prove_variable(&typing, x_context, x);
	const struct pg_evidence *second_application = pg_prove_application(&typing,
		pg_prove_projection(&typing, x_context, typed_reduct), x_value);
	const struct pg_evidence *second_reduct = normalize(&synthesis, x_context, second_application);
	const struct pg_evidence *return_x = pg_prove_return(&typing, &classifiers, x_value);
	assert(second_reduct && pg_evidence_subject(second_reduct)->core == pg_evidence_subject(return_x)->core);
	assert(pg_evidence_classifier(second_reduct) == pg_evidence_classifier(return_x));
	const struct pg_evidence *images[] = {pg_prove_variable(&typing, x_context, a), x_value};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, x_context, x_context, 2, images);
	const struct pg_evidence *twice_reindexed = pg_prove_reindex(&typing, sigma,
		pg_prove_projection(&typing, x_context, typed_reduct));
	const struct pg_evidence *third_application = pg_prove_application(&typing, twice_reindexed, x_value);
	const struct pg_evidence *third_reduct = normalize(&synthesis, x_context, third_application);
	assert(third_reduct && pg_evidence_subject(third_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *reindexed_application = pg_prove_reindex(&typing, sigma, second_application);
	const struct pg_evidence *reindexed_reduct = normalize(&synthesis, x_context, reindexed_application);
	assert(reindexed_reduct && pg_evidence_subject(reindexed_reduct)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *projected_application = pg_prove_projection(&typing, x_context, typed_application);
	const struct pg_evidence *projected_reduct = normalize(&synthesis, x_context, projected_application);
	assert(projected_reduct && pg_alpha_equal(pg_evidence_subject(projected_reduct)->core,
		pg_evidence_subject(typed_reduct)->core) == 1);
	reduction_terms = graph.terms.count;
	reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(normalize(&synthesis, x_context, second_application) == second_reduct);
		assert(normalize(&synthesis, x_context, third_application) == third_reduct);
		assert(normalize(&synthesis, x_context, reindexed_application) == reindexed_reduct);
		assert(normalize(&synthesis, x_context, projected_application) == projected_reduct);
	}
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	const struct pg_source_scope *scope = pg_synthesis_bind(&synthesis, a_scope, x_name, x, x_context);
	identity_contents(&typing, &classifiers, &beta, x_context, second_application, x_value, sigma);
	normalization_jobs(&typing, &classifiers, x_context, second_application, x_value);
	const struct pg_evidence *deep_body = return_x;
	for (size_t i = 0; i < 120; ++i)
		deep_body = pg_prove_return(&typing, &classifiers, pg_prove_thunk(&typing, &classifiers, deep_body));
	const struct pg_evidence *deep_pi = pg_prove_pi(&typing, &classifiers, a_type, x_context,
		pg_prove_classifier(&typing, &classifiers, x_context, deep_body));
	const struct pg_evidence *deep_function = pg_prove_projection(&typing, x_context,
		pg_prove_lambda(&typing, deep_pi, deep_body));
	const struct pg_evidence *deep_application = pg_prove_application(&typing, deep_function, x_value);
	struct pg_synthesis_job *deep_step = pg_synthesis_normalize(&synthesis, x_context, deep_application);
	assert(deep_step && !pg_synthesis_result(deep_step));
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_whnf_status(pg_whnf_request(&beta, &pg_pure_policy,
		pg_evidence_subject(deep_application)->core)) == PG_EVAL_PENDING);
	pg_synthesis_advance(&synthesis, 32);
	assert(pg_synthesis_status(deep_step) == PG_SYNTHESIS_PENDING);
	assert(!pg_synthesis_result(deep_step));
	for (size_t i = 0; pg_synthesis_status(deep_step) == PG_SYNTHESIS_PENDING; ++i) {
		assert(i < 1000);
		pg_synthesis_advance(&synthesis, 11);
	}
	const struct pg_evidence *deep_result = pg_synthesis_result(deep_step);
	assert(deep_result && pg_evidence_subject(deep_result)->core == pg_evidence_subject(deep_body)->core);
	assert(deep_result == normalize(&synthesis, x_context, deep_application));
	assert(pg_synthesis_normalize(&synthesis, x_context, deep_application) == deep_step);
	const struct pg_evidence *delayed_type = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_return_type(&typing, &classifiers, pg_prove_variable(&typing, x_context, a)));
	const struct pg_object *m = pg_binder(&graph);
	const struct pg_evidence *m_context = pg_prove_context_extension(&typing, x_context, m, delayed_type);
	const struct pg_evidence *m_value = pg_prove_variable(&typing, m_context, m);
	assert(!pg_prove_thunk_computation(&typing, m_value));
	complete(&synthesis, pg_synthesis_unthunk(&synthesis, m_context, m_value), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *neutral_force = pg_prove_force(&typing, m_value);
	assert(!pg_prove_return_value(&typing, neutral_force));
	complete(&synthesis, pg_synthesis_return(&synthesis, m_context, neutral_force), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *delayed_x = pg_prove_thunk(&typing, &classifiers, return_x);
	const struct pg_evidence *m_images[] = {pg_prove_variable(&typing, x_context, a), x_value, delayed_x};
	const struct pg_evidence *m_substitution = pg_prove_substitution(&typing, m_context, x_context, 3, m_images);
	const struct pg_evidence *substituted_m = pg_prove_reindex(&typing, m_substitution, m_value);
	same_judgement(substituted_m, delayed_x);
	assert(pg_evidence_rule(substituted_m) == PG_REINDEX);
	assert(pg_evidence_premise(substituted_m, 1) == m_value);
	const struct pg_evidence *substituted_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, x_context, substituted_m), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(substituted_code)->core == pg_evidence_subject(return_x)->core);
	const struct pg_evidence *substituted_force = pg_prove_reindex(&typing, m_substitution, pg_prove_force(&typing, m_value));
	const struct pg_evidence *distributed_force = pg_prove_force(&typing, substituted_m);
	same_judgement(substituted_force, distributed_force);
	const struct pg_evidence *distributed_application = pg_prove_application(&typing,
		pg_prove_reindex(&typing, sigma, pg_evidence_premise(second_application, 0)),
		pg_prove_reindex(&typing, sigma, x_value));
	same_judgement(reindexed_application, distributed_application);
	const struct pg_evidence *m_fold = pg_prove_fold(&typing, &classifiers, pg_prove_force(&typing, m_value),
		pg_prove_projection(&typing, m_context, typed_reduct));
	const struct pg_evidence *substituted_fold = pg_prove_reindex(&typing, m_substitution, m_fold);
	const struct pg_evidence *distributed_fold = pg_prove_fold(&typing, &classifiers, substituted_force,
		pg_prove_reindex(&typing, m_substitution, pg_evidence_premise(m_fold, 1)));
	same_judgement(substituted_fold, distributed_fold);
	struct pg_synthesis_job *substituted_return = pg_synthesis_return(&synthesis, x_context, substituted_force);
	const struct pg_evidence *substituted_result = complete(&synthesis, substituted_return, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(substituted_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(substituted_result) == pg_reference(&graph, a));
	assert(pg_evidence_rule(substituted_force) == PG_REINDEX);
	const struct pg_evidence *extended_m = pg_prove_context_extension(&typing, m_context,
		pg_binder(&graph), pg_prove_variable(&typing, m_context, a));
	const struct pg_evidence *extended_images[] = {m_images[0], m_images[1], m_images[2], x_value};
	const struct pg_evidence *extended_substitution = pg_prove_substitution(&typing,
		extended_m, x_context, 4, extended_images);
	const struct pg_evidence *weakened_force = pg_prove_projection(&typing, extended_m,
		pg_prove_force(&typing, m_value));
	const struct pg_evidence *substituted_weakening = pg_prove_reindex(&typing, extended_substitution, weakened_force);
	same_judgement(substituted_weakening, substituted_force);
	assert(pg_evidence_premise(substituted_weakening, 1) == weakened_force);
	const struct pg_evidence *weakening_result = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, substituted_weakening), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(weakening_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(weakening_result) == pg_reference(&graph, a));
	size_t weakening_proofs = typing.proofs.count, weakening_terms = graph.terms.count;
	for (size_t i = 0; i < 20; ++i)
		assert(pg_prove_reindex(&typing, extended_substitution, weakened_force) == substituted_weakening);
	assert(typing.proofs.count == weakening_proofs && graph.terms.count == weakening_terms);
	const struct pg_evidence *nested_result = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, substituted_fold), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(nested_result)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(nested_result) == pg_reference(&graph, a));
	struct pg_synthesis_job *substituted_force_step = pg_synthesis_normalize(&synthesis, x_context, substituted_force);
	const struct pg_evidence *force_step_result = complete(&synthesis, substituted_force_step, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(force_step_result)->core == pg_evidence_subject(return_x)->core);
	assert(pg_evidence_rule(substituted_force) == PG_REINDEX);
	uint64_t image_steps = synthesis.steps;
	assert(pg_synthesis_return(&synthesis, x_context, substituted_force) == substituted_return);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == image_steps);
	const struct pg_evidence *a_images[] = {a_type};
	const struct pg_evidence *a_substitution = pg_prove_substitution(&typing, a_context, a_context, 1, a_images);
	const struct pg_evidence *reindexed_polymorphic = pg_prove_reindex(&typing, a_substitution, typed_application);
	struct pg_synthesis_job *polymorphic_step = pg_synthesis_normalize(&synthesis, a_context, reindexed_polymorphic);
	const struct pg_evidence *polymorphic_result = complete(&synthesis, polymorphic_step, PG_SYNTHESIS_DONE);
	assert(pg_alpha_equal(pg_evidence_classifier(polymorphic_result), pg_evidence_classifier(reindexed_polymorphic)) == 1);
	assert(pg_alpha_equal(pg_evidence_subject(polymorphic_result)->core, pg_evidence_subject(typed_reduct)->core) == 1);
	image_steps = synthesis.steps;
	assert(pg_synthesis_normalize(&synthesis, a_context, reindexed_polymorphic) == polymorphic_step);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == image_steps);
	struct pg_synthesis_job *callee_step = pg_synthesis_normalize(&synthesis, x_context, projected_application);
	const struct pg_evidence *outer_application = pg_prove_application(&typing, projected_application, x_value);
	struct pg_synthesis_job *outer_step = pg_synthesis_normalize(&synthesis, x_context, outer_application);
	assert(callee_step && outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(!pg_synthesis_result(outer_step));
	assert(pg_synthesis_status(callee_step) == PG_SYNTHESIS_DONE);
	const struct pg_evidence *other_application = pg_prove_application(&typing, projected_application,
		pg_prove_reindex(&typing, sigma, x_value));
	struct pg_synthesis_job *other_step = pg_synthesis_normalize(&synthesis, x_context, other_application);
	assert(other_step && other_step != outer_step);
	pg_synthesis_advance(&synthesis, 1);
	assert(!pg_synthesis_result(other_step));
	const struct pg_evidence *outer_reduct = complete(&synthesis, outer_step, PG_SYNTHESIS_DONE);
	const struct pg_evidence *other_reduct = complete(&synthesis, other_step, PG_SYNTHESIS_DONE);
	assert(other_reduct != outer_reduct);
	assert(pg_evidence_normalization(other_reduct) == pg_evidence_normalization(outer_reduct));
	same_judgement(other_reduct, outer_reduct);
	assert(outer_reduct == normalize(&synthesis, x_context, outer_application));
	complete(&synthesis, callee_step, PG_SYNTHESIS_DONE);
	uint64_t shared_steps = synthesis.steps;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_normalize(&synthesis, x_context, projected_application) == callee_step);
		assert(pg_synthesis_normalize(&synthesis, x_context, outer_application) == outer_step);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(synthesis.steps == shared_steps);
	assert(!pg_synthesis_normalize(&synthesis, a_context, outer_application));
	size_t evaluation_jobs = synthesis.jobs.count;
	struct pg_synthesis_job *shared_return = pg_synthesis_return(&synthesis, x_context, second_application);
	assert(shared_return && pg_synthesis_status(shared_return) == PG_SYNTHESIS_PENDING);
	assert(synthesis.jobs.count == evaluation_jobs + 1);
	assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
	uint64_t evaluation_steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 0);
	assert(synthesis.steps == evaluation_steps && !pg_synthesis_result(shared_return));
	pg_synthesis_advance(&synthesis, 1);
	assert(pg_synthesis_status(shared_return) == PG_SYNTHESIS_PENDING);
	assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
	const struct pg_evidence *shared_value = complete(&synthesis, shared_return, PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(shared_value)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(shared_value) == pg_reference(&graph, a));
	evaluation_steps = synthesis.steps;
	reduction_terms = graph.terms.count;
	reduction_proofs = typing.proofs.count;
	for (size_t i = 0; i < 100; ++i) {
		assert(pg_synthesis_return(&synthesis, x_context, second_application) == shared_return);
		pg_synthesis_advance(&synthesis, 100);
	}
	assert(synthesis.steps == evaluation_steps);
	assert(graph.terms.count == reduction_terms && typing.proofs.count == reduction_proofs);
	assert(!pg_synthesis_return(&synthesis, a_context, second_application));
	assert(!pg_synthesis_return(&synthesis, x_context, x_value));
	assert(!pg_synthesis_return(&synthesis, NULL, second_application));
	struct pg_synthesis_job *other_return = pg_synthesis_return(&synthesis, x_context, reindexed_application);
	assert(other_return && other_return != shared_return);
	assert(pg_evidence_subject(complete(&synthesis, other_return, PG_SYNTHESIS_DONE))->core == pg_reference(&graph, x));
	const struct pg_evidence *extra_context = pg_prove_context_extension(&typing, x_context,
		pg_binder(&graph), pg_prove_variable(&typing, x_context, a));
	const struct pg_evidence *projected_input = pg_prove_projection(&typing, extra_context, second_application);
	assert(pg_evidence_subject(projected_input)->core == pg_evidence_subject(second_application)->core);
	struct pg_synthesis_job *projected_return = pg_synthesis_return(&synthesis, extra_context, projected_input);
	assert(projected_return && projected_return != shared_return);
	assert(pg_evidence_context(complete(&synthesis, projected_return, PG_SYNTHESIS_DONE)) == pg_evidence_context(extra_context));
	struct pg_typing foreign_typing;
	assert(pg_typing_init(&foreign_typing, &graph) == 0);
	const struct pg_evidence *foreign_context = pg_prove_empty_context(&foreign_typing);
	const struct pg_evidence *foreign_value = pg_prove_type_value(&foreign_typing,
		pg_prove_universe(&foreign_typing, &classifiers, foreign_context, 0));
	assert(!pg_synthesis_return(&synthesis, foreign_context,
		pg_prove_return(&foreign_typing, &classifiers, foreign_value)));
	pg_typing_destroy(&foreign_typing);
	struct pg_parser shared_parser;
	struct pg_definition shared_definition;
	const char *shared_source = "main := \\y : ((\\T : @ => T) A) => y;";
	pg_parser_init(&shared_parser, &graph, shared_source, strlen(shared_source));
	assert(pg_parser_next(&shared_parser, &shared_definition) == 1);
	struct pg_syntax *shared_copy = pg_alloc(&graph, sizeof(*shared_copy));
	assert(shared_copy);
	*shared_copy = *shared_definition.expression;
	struct pg_synthesis_job *first_consumer = pg_synthesis_request(&synthesis, scope, shared_definition.expression);
	struct pg_synthesis_job *second_consumer = pg_synthesis_request(&synthesis, scope, shared_copy);
	assert(first_consumer != second_consumer);
	complete(&synthesis, first_consumer, PG_SYNTHESIS_DONE);
	complete(&synthesis, second_consumer, PG_SYNTHESIS_DONE);
	const struct pg_evidence *shared_input = pg_synthesis_result(pg_synthesis_request(&synthesis, scope,
		shared_definition.expression->left));
	evaluation_jobs = synthesis.jobs.count;
	struct pg_synthesis_job *type_producer = pg_synthesis_return(&synthesis, x_context, shared_input);
	assert(type_producer && pg_synthesis_status(type_producer) == PG_SYNTHESIS_DONE);
	assert(synthesis.jobs.count == evaluation_jobs);
	const struct pg_evidence *computed_domain = complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\T : @ => T) A) => y;"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(computed_domain), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	const struct pg_evidence *computed_codomain = complete(&synthesis, request(&synthesis, scope,
		"main := A -> ((\\T : @ => T) A);"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_subject(computed_codomain)->core, &domain, &binder, &codomain));
	assert(pg_return_type_view(codomain, &codomain) && codomain == pg_reference(&graph, a));
	const char *checked_types[] = {
		"main := \\y : (((\\T : @ => T) A) :: @) => y;",
		"main := \\y : ((\\T : @ => (T :: @)) A) => y;",
		"main := \\y : ((\\T : @ => ((\\S : @ => S) T) :: @) A) => y;"
	};
	for (size_t i = 0; i < sizeof(checked_types) / sizeof(*checked_types); ++i) {
		const struct pg_evidence *checked_type = complete(&synthesis,
			request(&synthesis, scope, checked_types[i]), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_classifier(checked_type), pg_evidence_classifier(computed_domain)) == 1);
	}
	const struct pg_evidence *computed_expect = complete(&synthesis, request(&synthesis, scope,
		"main := x :: ((\\T : @ => T) ((\\S : @ => S) A));"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(computed_expect)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(computed_expect) == pg_reference(&graph, a));
	const struct pg_evidence *computed_function_domain = complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\T : @ => \\S : @ => S) A A) => y;"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(computed_function_domain), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	complete(&synthesis, request(&synthesis, scope,
		"main := \\y : ((\\z : A => z) x) => y;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *original_quote = pg_prove_thunk(&typing, &classifiers, computed_domain);
	const struct pg_evidence *original_return = pg_prove_return(&typing, &classifiers, original_quote);
	const struct pg_evidence *target_quote_type = pg_prove_thunk_type(&typing, &classifiers,
		pg_prove_classifier(&typing, &classifiers, x_context, computed_function_domain));
	const struct pg_evidence *target_return_type = pg_prove_return_type(&typing, &classifiers, target_quote_type);
	assert(pg_evidence_classifier(original_quote) != pg_evidence_subject(target_quote_type)->core);
	struct pg_conversion return_conversion;
	assert(pg_conversion_init(&return_conversion, &beta, pg_evidence_classifier(original_return),
		pg_evidence_subject(target_return_type)->core) == 0);
	while (pg_conversion_advance(&return_conversion, 1) == PG_CONVERSION_PENDING) {}
	assert(pg_conversion_status(&return_conversion) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted_return = pg_prove_conversion(&typing, original_return,
		target_return_type, pg_conversion_certificate(&return_conversion));
	pg_conversion_destroy(&return_conversion);
	assert(converted_return);
	struct pg_synthesis_job *converted_value_job = pg_synthesis_return(&synthesis, x_context, converted_return);
	const struct pg_evidence *converted_value = complete(&synthesis, converted_value_job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(converted_value) == PG_RETURN_VALUE);
	assert(pg_evidence_classifier(converted_value) == pg_evidence_subject(target_quote_type)->core);
	assert(pg_alpha_equal(pg_evidence_subject(converted_value)->core, pg_evidence_subject(original_quote)->core) == 1);
	assert(pg_evidence_classifier(original_quote) != pg_evidence_classifier(converted_value));
	assert(pg_evidence_premise(converted_value, 0) == converted_return);
	assert(pg_evidence_conversion(converted_return));
	struct pg_synthesis_job *unthunk_job = pg_synthesis_unthunk(&synthesis, x_context, converted_value);
	assert(unthunk_job && pg_synthesis_status(unthunk_job) == PG_SYNTHESIS_PENDING);
	const struct pg_evidence *unthunked = complete(&synthesis, unthunk_job, PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(unthunked) == PG_THUNK_COMPUTATION);
	assert(pg_alpha_equal(pg_evidence_subject(unthunked)->core, pg_evidence_subject(computed_domain)->core) == 1);
	assert(pg_thunk_type_view(pg_evidence_classifier(converted_value), &codomain));
	assert(pg_evidence_classifier(unthunked) == codomain);
	assert(pg_evidence_premise(unthunked, 0) == converted_value);
	const struct pg_evidence *force_converted = pg_prove_force(&typing, converted_value);
	const struct pg_evidence *force_result = complete(&synthesis,
		pg_synthesis_normalize(&synthesis, x_context, force_converted), PG_SYNTHESIS_DONE);
	same_judgement(force_result, unthunked);
	assert(pg_evidence_premise(force_result, 0) == force_converted);
	const struct pg_evidence *converted_application = pg_prove_application(&typing, unthunked, x_value);
	const struct pg_evidence *converted_application_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, converted_application), PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(converted_application_value)->core == pg_reference(&graph, x));
	assert(pg_evidence_classifier(converted_application_value) == pg_reference(&graph, a));
	assert(pg_evidence_classifier(force_result) == pg_evidence_classifier(force_converted));
	assert(!pg_synthesis_unthunk(&synthesis, x_context, force_converted));
	assert(!pg_synthesis_unthunk(&synthesis, a_context, converted_value));
	const struct pg_evidence *projected_conversion = pg_prove_projection(&typing, extra_context, converted_return);
	const struct pg_evidence *projected_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, extra_context, projected_conversion), PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(projected_value) == pg_evidence_context(extra_context));
	assert(pg_evidence_classifier(projected_value) == pg_evidence_classifier(converted_value));
	const struct pg_evidence *projected_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, extra_context, projected_value), PG_SYNTHESIS_DONE);
	assert(pg_evidence_context(projected_code) == pg_evidence_context(extra_context));
	assert(pg_evidence_classifier(projected_code) == pg_evidence_classifier(unthunked));
	const struct pg_evidence *reindexed_conversion = pg_prove_reindex(&typing, sigma, converted_return);
	const struct pg_evidence *reindexed_value = complete(&synthesis,
		pg_synthesis_return(&synthesis, x_context, reindexed_conversion), PG_SYNTHESIS_DONE);
	assert(pg_return_type_view(pg_evidence_classifier(reindexed_conversion), &codomain));
	assert(pg_alpha_equal(pg_evidence_classifier(reindexed_value), codomain) == 1);
	const struct pg_evidence *reindexed_code = complete(&synthesis,
		pg_synthesis_unthunk(&synthesis, x_context, reindexed_value), PG_SYNTHESIS_DONE);
	assert(pg_thunk_type_view(pg_evidence_classifier(reindexed_value), &codomain));
	assert(pg_alpha_equal(pg_evidence_classifier(reindexed_code), codomain) == 1);
	uint64_t conversion_steps = synthesis.steps;
	assert(pg_synthesis_return(&synthesis, x_context, converted_return) == converted_value_job);
	assert(pg_synthesis_unthunk(&synthesis, x_context, converted_value) == unthunk_job);
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == conversion_steps);
	struct pg_synthesis_job *batched_type = request(&synthesis, scope,
		"main := \\y : ((\\T : @ => \\S : @ => S) A A) => y;");
	pg_synthesis_advance(&synthesis, 0);
	assert(pg_synthesis_status(batched_type) == PG_SYNTHESIS_PENDING);
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(batched_type) == PG_SYNTHESIS_DONE);
	assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(batched_type))->core,
		pg_evidence_subject(computed_function_domain)->core) == 1);
	assert(pg_alpha_equal(pg_evidence_classifier(pg_synthesis_result(batched_type)),
		pg_evidence_classifier(computed_function_domain)) == 1);
	complete(&synthesis, request(&synthesis, scope,
		"main := missing :: ((\\T : @ => T) A);"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\f : @ -> @ => \\y : f A => y;"), PG_SYNTHESIS_UNSUPPORTED);
	const struct pg_evidence *application = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) x;"), PG_SYNTHESIS_DONE);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(application)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	const struct pg_term *expected = pg_application(&graph, pg_reference(&graph, &pg_return_operation), pg_reference(&graph, x));
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	const struct pg_evidence *typed_steps[] = {second_application, third_application, reindexed_application,
		outer_application, distributed_application, distributed_force, distributed_fold};
	for (size_t i = 0; i < sizeof(typed_steps) / sizeof(*typed_steps); ++i) {
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(typed_steps[i])->core);
		assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == pg_evidence_subject(return_x)->core);
		pg_eval_destroy(&machine);
	}
	const struct pg_evidence *sequenced_higher = complete(&synthesis, request(&synthesis, scope,
		"main := (\\f : A -> A => f x) { &(\\y : A => y); };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(sequenced_higher) == PG_FOLD_ELIM);
	const struct pg_evidence *sequence_body = pg_evidence_premise(pg_evidence_premise(sequenced_higher, 1), 1);
	assert(pg_evidence_rule(sequence_body) == PG_APP_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(sequence_body, 1)) == PG_TYPE_CONVERSION);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(sequenced_higher)->core);
	assert(pg_eval_advance(&machine, 300) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis, request(&synthesis, scope,
		"main := (\\f : A -> A => f x) { &(\\y : A => A); };"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *checked = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) :: A -> A;"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(checked) == PG_TYPE_CONVERSION);
	complete(&synthesis, request(&synthesis, scope, "main := (\\y : A => missing) :: A -> A;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := x :: @;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := &(\\y : A => y);"), PG_SYNTHESIS_DONE);
	complete(&synthesis, request(&synthesis, scope, "main := (&(\\y : A => y)) x;"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *higher = complete(&synthesis,
		request(&synthesis, scope, "main := (\\f : A -> A => f x) &(\\y : A => y);"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(higher) == PG_APP_ELIM);
	const struct pg_evidence *converted_argument = pg_evidence_premise(higher, 1);
	assert(pg_evidence_rule(converted_argument) == PG_TYPE_CONVERSION);
	const struct pg_conversion_certificate *conversion = pg_evidence_conversion(converted_argument);
	assert(conversion && pg_conversion_left(conversion) != pg_conversion_right(conversion));
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(higher)->core);
	assert(pg_eval_advance(&machine, 200) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis,
		request(&synthesis, scope, "main := (\\f : A -> A => f x) &(\\y : A => A);"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *sequenced = complete(&synthesis,
		request(&synthesis, scope, "main := (\\y : A => y) ((\\y : A => y) x);"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(sequenced) == PG_FOLD_ELIM);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(sequenced)->core);
	assert(pg_eval_advance(&machine, 200) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	complete(&synthesis, request(&synthesis, scope,
		"main := \\z : A => (\\y : A => y) ((\\y : A => y) z);"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *curried = complete(&synthesis, request(&synthesis, scope,
		"main := ((\\u : A => \\v : A => v) ((\\y : A => y) x)) x;"), PG_SYNTHESIS_DONE);
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(curried)->core);
	assert(pg_eval_advance(&machine, 300) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == expected);
	pg_eval_destroy(&machine);
	const struct pg_evidence *typed_programs[] = {application, higher, sequenced_higher, sequenced, curried};
	for (size_t i = 0; i < sizeof(typed_programs) / sizeof(*typed_programs); ++i) {
		const struct pg_evidence *value = complete(&synthesis,
			pg_synthesis_return(&synthesis, x_context, typed_programs[i]), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(value)->core == pg_reference(&graph, x));
		assert(pg_evidence_classifier(value) == pg_reference(&graph, a));
	}
	complete(&synthesis, request(&synthesis, root, "Nat := @{zero:*; succ:*->*;};"), PG_SYNTHESIS_DONE);
	const char *blocks[] = {
		"main := { alias := x; alias; };",
		"main := { first := (\\y : A => y) x; second := (\\y : A => y) first; second; };",
		"main := { selected := x; dead := missing; }.selected;",
		"main := { x := x; x; };",
		"main := { x; (\\y : A => y) x; };",
		"main := { result : A := (\\y : A => y) x; result; };",
		"main := { outer := { inner := x; inner; }; outer; };",
		"main := { f := &(\\y : A => y); f x; };",
		"main := { &(\\y : A => y); } x;",
		"main := (&{ &(\\y : A => y); }) x;",
		"main := { &(\\y : A => y); } { x; };",
		"main := { &(\\f : A -> A => f x); } { &(\\y : A => y); };"
	};
	for (size_t i = 0; i < sizeof(blocks) / sizeof(*blocks); ++i) {
		const struct pg_evidence *block = complete(&synthesis, request(&synthesis, scope, blocks[i]), PG_SYNTHESIS_DONE);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(block)->core);
		assert(pg_eval_advance(&machine, 500) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
		const struct pg_evidence *value = complete(&synthesis,
			pg_synthesis_return(&synthesis, x_context, block), PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(value)->core == pg_reference(&graph, x));
		assert(pg_evidence_classifier(value) == pg_reference(&graph, a));
	}
	const struct pg_evidence *discarded_values = complete(&synthesis, request(&synthesis, scope,
		"main := { x; x; x; };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(discarded_values) == PG_RETURN_INTRO);
	assert(pg_evidence_subject(discarded_values)->core == expected);
	const struct pg_evidence *discarded_computation = complete(&synthesis, request(&synthesis, scope,
		"main := { (\\y : A => y) x; x; };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(discarded_computation) == PG_FOLD_ELIM);
	complete(&synthesis, request(&synthesis, scope,
		"main := { missing; x; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { temp := x; }.missing;"), PG_SYNTHESIS_REJECTED);
	const struct pg_evidence *ordered = complete(&synthesis, request(&synthesis, scope,
		"main := { &(\\y : A => y); } { x; };"), PG_SYNTHESIS_DONE);
	assert(pg_evidence_rule(ordered) == PG_FOLD_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(ordered, 0)) == PG_RETURN_INTRO);
	const struct pg_evidence *argument_fold = pg_evidence_premise(pg_evidence_premise(ordered, 1), 1);
	assert(pg_evidence_rule(argument_fold) == PG_FOLD_ELIM);
	assert(pg_evidence_rule(pg_evidence_premise(argument_fold, 0)) == PG_CONTEXT_PROJECTION);
	const struct pg_evidence *ordered_app = pg_evidence_premise(pg_evidence_premise(argument_fold, 1), 1);
	assert(pg_evidence_rule(ordered_app) == PG_APP_ELIM);
	const struct pg_evidence *callee_projection = pg_evidence_premise(ordered_app, 0);
	assert(pg_evidence_rule(callee_projection) == PG_CONTEXT_PROJECTION);
	assert(pg_evidence_rule(pg_evidence_premise(callee_projection, 1)) == PG_FORCE_ELIM);
	complete(&synthesis, request(&synthesis, scope, "main := { x; } x;"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope,
		"main := { &(\\y : A => y); } { A; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { temp := x; temp := x; temp; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { temp : @ := x; temp; };"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, request(&synthesis, scope, "main := { !x; };"), PG_SYNTHESIS_UNSUPPORTED);
	complete(&synthesis, request(&synthesis, scope, "main := &{ x; };"), PG_SYNTHESIS_DONE);
	const struct pg_evidence *dependent_block = complete(&synthesis, request(&synthesis, scope,
		"main := { B := A; \\y : B => y; };"), PG_SYNTHESIS_DONE);
	assert(pg_pi_view(pg_evidence_classifier(dependent_block), &domain, &binder, &codomain));
	assert(domain == pg_reference(&graph, a));
	assert(pg_return_type_view(codomain, &codomain) && codomain == domain);
	const char *dependent_computations[] = {
		"main := { B := (\\T : @ => T) A; \\y : B => y; };",
		"main := { B := (\\T : @ => T) A; C := (\\T : @ => T) B; \\y : C => y; };",
		"main := (\\B : @ => \\y : B => y) ((\\T : @ => T) A);"
	};
	for (size_t i = 0; i < sizeof(dependent_computations) / sizeof(*dependent_computations); ++i) {
		const struct pg_evidence *result = complete(&synthesis,
			request(&synthesis, scope, dependent_computations[i]), PG_SYNTHESIS_DONE);
		assert(pg_alpha_equal(pg_evidence_classifier(result), pg_evidence_classifier(dependent_block)) == 1);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(result)->core);
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		const struct pg_term *function_result = pg_eval_readback(&machine, &graph);
		pg_eval_destroy(&machine);
		assert(function_result && function_result->kind == PG_LAMBDA);
		pg_computation_eval_init(&machine, &graph, pg_application(&graph, function_result, pg_reference(&graph, x)));
		assert(pg_eval_advance(&machine, 1000) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
	}
	complete(&synthesis, request(&synthesis, scope,
		"main := \\f : A -> @ => { B := f x; \\y : B => y; };"), PG_SYNTHESIS_UNSUPPORTED);
	const char *modules[] = {
		"{{ main := id x; id := \\y:A=>y; }}.main",
		"{{ main :: A -> A; main := id; id := \\y:A=>y; }}.main",
		"{{ id := \\y:Alias=>y; Alias:=A; main:=id x; }}.main",
		"{{ main:=alias x; alias:=id; id:=\\y:A=>y; }}.main",
		"{{ main:=id result; result:=id x; id:=\\y:A=>y; }}.main",
		"{{ main:=id alias; alias:=result; result:=id x; id:=\\y:A=>y; }}.main",
		"{{ main:={v:=result; id v;}; result:=id x; id:=\\y:A=>y; }}.main",
		"{{ main:=(\\f:A->A=>f x) &id; id:=\\y:A=>y; }}.main",
		"{{ main:=id (result::A); result:=id x; id:=\\y:A=>y; }}.main"
	};
	for (size_t i = 0; i < sizeof(modules) / sizeof(*modules); ++i) {
		const struct pg_evidence *result = complete(&synthesis, program(&synthesis, scope, modules[i]), PG_SYNTHESIS_DONE);
		assert(result && pg_evidence_judgement(result) == PG_JUDGEMENT_VALUE);
		const struct pg_evidence *run = pg_prove_force(&typing, result);
		if (i == 1) run = pg_prove_application(&typing, run, pg_prove_variable(&typing, x_context, x));
		assert(run);
		pg_computation_eval_init(&machine, &graph, pg_evidence_subject(run)->core);
		assert(pg_eval_advance(&machine, 500) == PG_EVAL_WHNF);
		assert(pg_eval_readback(&machine, &graph) == expected);
		pg_eval_destroy(&machine);
	}
	/* Explicitly quoted returning computations are not silently run to satisfy
	 * an argument type. Definition adaptation is not expected-type coercion. */
	complete(&synthesis, program(&synthesis, scope,
		"{{main:=id delayed; delayed:=&{x;}; id:=\\y:A=>y;}}.main"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope,
		"{{main:=id &(id x); id:=\\y:A=>y;}}.main"), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope, "id:=\\y:A=>y; id::A->A;"), PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *library = program(&synthesis, scope, "left:=id; right:=id; id:=\\y:A=>y;");
	struct pg_token left_name = {.text="left", .length=4}, right_name = {.text="right", .length=5};
	size_t before_indexing = synthesis.jobs.count, before_terms = graph.terms.count;
	pg_synthesis_advance(&synthesis, 1);
	assert(synthesis.jobs.count == before_indexing + 1 && graph.terms.count == before_terms);
	assert(!pg_synthesis_definition(library, left_name));
	pg_synthesis_advance(&synthesis, 1);
	assert(synthesis.jobs.count == before_indexing + 2 && graph.terms.count == before_terms);
	assert(pg_synthesis_definition(library, left_name));
	assert(!pg_synthesis_definition(library, right_name));
	assert(pg_synthesis_status(pg_synthesis_definition(library, left_name)) == PG_SYNTHESIS_PENDING);
	assert(!complete(&synthesis, library, PG_SYNTHESIS_DONE));
	struct pg_synthesis_job *left_alias = pg_synthesis_definition(library, left_name);
	struct pg_synthesis_job *right_alias = pg_synthesis_definition(library, right_name);
	assert(left_alias && right_alias && pg_synthesis_status(left_alias) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_result(left_alias) == pg_synthesis_result(right_alias));
	assert(!pg_synthesis_definition(library, x_name));
	const struct pg_syntax shared_body = {.kind=PG_SYNTAX_ATOM, .token=x_name};
	const struct pg_syntax_item shared_entries[] = {
		{.name=left_name, .expression=&shared_body, .operation=PG_TOKEN_ASSIGN},
		{.name=right_name, .expression=&shared_body, .operation=PG_TOKEN_ASSIGN}
	};
	const struct pg_syntax shared_root = {.kind=PG_SYNTAX_DEFINITIONS, .items=shared_entries, .item_count=2};
	struct pg_synthesis_job *shared = pg_synthesis_request(&synthesis, scope, &shared_root);
	assert(!complete(&synthesis, shared, PG_SYNTHESIS_DONE));
	assert(pg_synthesis_definition(shared, left_name) == pg_synthesis_definition(shared, right_name));
	complete(&synthesis, program(&synthesis, scope, ""), PG_SYNTHESIS_DONE);
	const char *invalid_modules[] = {
		"{{main:=x; main:=A;}}.main", "{{main:=x;}}.missing",
		"{{main:=x; other:=missing;}}.main", "{{main:=x; main::@;}}.main",
		"missing::A;", "{{main::A; main:=missing;}}.main"
	};
	for (size_t i = 0; i < sizeof(invalid_modules) / sizeof(*invalid_modules); ++i)
		complete(&synthesis, program(&synthesis, scope, invalid_modules[i]), PG_SYNTHESIS_REJECTED);
	complete(&synthesis, program(&synthesis, scope, "import Library;"), PG_SYNTHESIS_UNSUPPORTED);
	struct pg_synthesis_job *cycle = program(&synthesis, scope, "{{main:=other; other:=main;}}.main");
	pg_synthesis_advance(&synthesis, 1000);
	assert(pg_synthesis_status(cycle) == PG_SYNTHESIS_PENDING && !pg_synthesis_result(cycle));
	assert(!synthesis.ready);
	const struct pg_synthesis_job *member = pg_synthesis_cycle(cycle);
	assert(member && pg_synthesis_dependency(member));
	const struct pg_synthesis_job *cursor = member;
	unsigned cycle_length = 0;
	do {
		assert(pg_synthesis_status(cursor) == PG_SYNTHESIS_PENDING);
		assert(!pg_synthesis_result(cursor));
		cursor = pg_synthesis_dependency(cursor);
		assert(cursor && ++cycle_length < 20);
	} while (cursor != member);
	uint64_t stopped_steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 1000);
	assert(synthesis.steps == stopped_steps && pg_synthesis_cycle(cycle) == member);
	struct pg_synthesis strict;
	assert(pg_synthesis_init(&strict, &typing, &classifiers, &beta, PG_DEFINITION_EXPLICIT_THUNK) == 0);
	const struct pg_source_scope *strict_root = pg_synthesis_root(&strict);
	const struct pg_source_scope *strict_a = pg_synthesis_bind(&strict, strict_root, a_name, a, a_context);
	const struct pg_source_scope *strict_scope = pg_synthesis_bind(&strict, strict_a, x_name, x, x_context);
	complete(&strict, program(&strict, strict_scope, "{{main:={x;};}}.main"), PG_SYNTHESIS_REJECTED);
	complete(&strict, program(&strict, strict_scope, "{{main:=&{x;};}}.main"), PG_SYNTHESIS_DONE);
	complete(&strict, program(&strict, strict_scope, "{{main:=x;}}.main"), PG_SYNTHESIS_DONE);
	pg_synthesis_destroy(&strict);
	uint64_t steps = synthesis.steps;
	pg_synthesis_advance(&synthesis, 100);
	assert(synthesis.steps == steps);
	pg_synthesis_destroy(&synthesis);
	pg_whnf_work_destroy(&beta);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("source synthesis: pending jobs, raw nested lambdas, application, quotation and post-synthesis expectation passed");
}
