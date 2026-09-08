#include "identity.h"
#include "evidence.h"
#include "computation.h"
#include "action.h"

#include <assert.h>
#include <stdio.h>

static int first_argument(struct pg_eval *machine)
{
	const struct pg_closure *argument = pg_eval_argument(machine, 0);
	return argument ? pg_eval_enter(machine, *argument, 1) : 1;
}

static const struct pg_object_class tick_class = {"test-tick"};
static const struct pg_object tick = {PG_SEMANTIC_OBJECT, &tick_class};
static size_t ticks;

static int counted_dispatch(struct pg_eval *machine)
{
	if (machine->current.term->as.reference != &tick) return pg_pure_policy.dispatch(machine);
	++ticks;
	return first_argument(machine);
}

static void normalizes(struct pg_whnf_work *work, const struct pg_term *input, const struct pg_term *expected)
{
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, input);
	assert(job && job == pg_whnf_request(work, &pg_pure_policy, input));
	while (pg_whnf_status(job) == PG_EVAL_PENDING) {
		assert(!pg_whnf_result(job));
		pg_whnf_advance(job, 1);
		assert(pg_whnf_steps(job) < 10000);
	}
	assert(pg_whnf_status(job) == PG_EVAL_WHNF);
	assert(pg_whnf_result(job) == expected);
}

static uint64_t converts_budget(struct pg_whnf_work *work, const struct pg_term *left, const struct pg_term *right, uint64_t budget)
{
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, work, left, right) == 0);
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&comparison) < budget);
	assert(pg_conversion_status(&comparison) == PG_CONVERSION_EQUAL);
	uint64_t steps = pg_conversion_steps(&comparison);
	pg_conversion_destroy(&comparison);
	return steps;
}

static void converts(struct pg_whnf_work *work, const struct pg_term *left, const struct pg_term *right)
{
	converts_budget(work, left, right, 100000);
}

static const struct pg_evidence *action_result(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	struct pg_whnf_work *work, const struct pg_evidence *action,
	const struct pg_evidence *expected)
{
	assert(action && expected);
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, pg_evidence_subject(action)->core);
	while (pg_whnf_advance(job, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(job) < 100000);
	const struct pg_evidence *reduced = pg_prove_normalization(typing, action, pg_whnf_certificate(job));
	assert(reduced);
	const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, context, action);
	/* Independent substitution may freshen binders; formation is up to alpha,
	 * not an instruction to alpha-intern the two Core graphs. */
	assert(formation && pg_alpha_equal(pg_evidence_subject(formation)->core, pg_evidence_classifier(action)) == 1);
	const struct pg_evidence *target = pg_prove_classifier(typing, classifiers, context, expected);
	assert(target);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, work, pg_evidence_classifier(reduced), pg_evidence_subject(target)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing, reduced, target, pg_conversion_certificate(&comparison));
	assert(converted && pg_evidence_judgement(converted) == pg_evidence_judgement(expected));
	pg_conversion_destroy(&comparison);
	converts(work, pg_evidence_subject(converted)->core, pg_evidence_subject(expected)->core);
	return converted;
}

static void heterogeneous_pi(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *type_name = pg_binder(graph), *argument = pg_binder(graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, type_name,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *domain = pg_prove_value_type(typing, pg_prove_variable(typing, source, type_name));
	const struct pg_evidence *body_context = pg_prove_context_extension(typing, source, argument, domain);
	const struct pg_evidence *variable = pg_prove_variable(typing, body_context, argument);
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 1);
	const struct pg_binding_face *center = pg_binding_face(&dimensions, cube, pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *ls, *rs, *path;
	const struct pg_evidence *context = pg_identity_context(typing, &dimensions, source, 1, &center, &ls, &rs, &path);
	assert(context);
	for (size_t dependent = 0; dependent < 2; ++dependent) {
		const struct pg_evidence *value = variable;
		if (dependent) value = pg_prove_reflexivity(typing,
			pg_prove_projection(typing, body_context, domain), variable);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, value);
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, body_context,
			pg_prove_classifier(typing, classifiers, body_context, body));
		const struct pg_evidence *function = pg_prove_lambda(typing, pi, body);
		const struct pg_evidence *left = pg_prove_reindex(typing, ls, function);
		const struct pg_evidence *right = pg_prove_reindex(typing, rs, function);
		const struct pg_object *args[] = {pg_binder(graph), pg_binder(graph), pg_binder(graph)};
		const struct pg_evidence *expanded = pg_identity_family_pi_type(typing, classifiers,
			pi, ls, rs, 1, &path, left, right, args[0], args[1], args[2]);
		const struct pg_evidence *action = pg_prove_family_action(typing, pi, function, ls, rs, 1, &path);
		assert(expanded && action);
		struct pg_conversion conversion;
		assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(action), pg_evidence_subject(expanded)->core) == 0);
		assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *call = pg_prove_conversion(typing, action, expanded, pg_conversion_certificate(&conversion));
		pg_conversion_destroy(&conversion);
		const struct pg_evidence *boundary = NULL, *tail = expanded;
		for (size_t i = 0; i < 3; ++i) {
			boundary = pg_evidence_premise(tail, 1);
			tail = pg_evidence_premise(tail, 2);
		}
		call = pg_prove_projection(typing, boundary, call);
		for (size_t i = 0; i < 3; ++i)
			call = pg_prove_application(typing, call, pg_prove_variable(typing, boundary, args[i]));
		assert(call && pg_prove_classifier(typing, classifiers, boundary, call));
		if (!dependent) action_result(typing, classifiers, boundary, &work, call,
			pg_prove_return(typing, classifiers, pg_prove_variable(typing, boundary, args[2])));
		/* A path is oriented and belongs to its checked boundary, not just its endpoints' shapes. */
		assert(!pg_identity_family_pi_type(typing, classifiers, pi, rs, ls,
			1, &path, right, left, args[0], args[1], args[2]));
		assert(!pg_identity_family_pi_type(typing, classifiers, pi, ls, rs,
			0, NULL, left, right, args[0], args[1], args[2]));
		assert(!pg_identity_family_pi_type(typing, classifiers, pi, ls, rs,
			1, &path, left, right, args[0], args[0], args[2]));
		const struct pg_object *q = pg_binder(graph);
		const struct pg_evidence *choices = pg_prove_context_extension(typing, context, q,
			pg_prove_classifier(typing, classifiers, context, path));
		const struct pg_evidence *l = pg_prove_projection(typing, choices, pg_evidence_premise(ls, 2));
		const struct pg_evidence *r = pg_prove_projection(typing, choices, pg_evidence_premise(rs, 2));
		const struct pg_evidence *lsub = pg_prove_substitution(typing, source, choices, 1, &l);
		const struct pg_evidence *rsub = pg_prove_substitution(typing, source, choices, 1, &r);
		const struct pg_evidence *selected[] = {pg_prove_projection(typing, choices, path),
			pg_prove_variable(typing, choices, q)};
		const struct pg_evidence *results[2];
		for (size_t i = 0; i < 2; ++i) results[i] = pg_identity_family_pi_type(typing, classifiers,
			pi, lsub, rsub, 1, &selected[i], pg_prove_projection(typing, choices, left),
			pg_prove_projection(typing, choices, right), args[0], args[1], args[2]);
		assert(results[0] && results[1]);
		assert(pg_conversion_init(&conversion, &work, pg_evidence_subject(results[0])->core,
			pg_evidence_subject(results[1])->core) == 0);
		assert(pg_conversion_advance(&conversion, 100000) == PG_CONVERSION_DIFFERENT);
		pg_conversion_destroy(&conversion);
	}
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static const struct pg_evidence *thunk_map(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *path,
	const struct pg_evidence *input, enum pg_identity_direction direction)
{
	const struct pg_evidence *domain = pg_prove_identity_endpoint_type(typing, classifiers, path,
		direction == PG_IDENTITY_RIGHT ? PG_IDENTITY_LEFT_TYPE : PG_IDENTITY_RIGHT_TYPE);
	const struct pg_object *binder = pg_binder(typing->graph);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, context, binder, domain);
	const struct pg_evidence *field = pg_prove_identity_transport(typing, classifiers,
		pg_prove_projection(typing, extended, path), pg_prove_variable(typing, extended, binder), direction);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, field);
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, extended,
		pg_prove_classifier(typing, classifiers, extended, body));
	const struct pg_evidence *continuation = pg_prove_lambda(typing, pi, body);
	return pg_prove_thunk(typing, classifiers,
		pg_prove_fold(typing, pg_prove_force(typing, input), continuation));
}

static void thunk_transport(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *path,
	const struct pg_evidence *x, const struct pg_evidence *y)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work work, whole;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_whnf_work_init(&whole, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *ufa = pg_prove_thunk_type(typing, classifiers,
		pg_prove_return_type(typing, classifiers, pg_prove_variable(typing, source, a)));
	const struct pg_evidence *family = pg_prove_type_value(typing, ufa);
	const struct pg_evidence *types[2], *maps[2];
	for (size_t i = 0; i < 2; ++i) {
		types[i] = pg_prove_type_value(typing, pg_prove_identity_endpoint_type(typing, classifiers,
			path, i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE));
		maps[i] = pg_prove_substitution(typing, source, context, 1, &types[i]);
	}
	const struct pg_evidence *action = pg_prove_family_action(typing,
		pg_prove_classifier(typing, classifiers, source, family), family, maps[0], maps[1], 1, &path);
	const struct pg_evidence *target = pg_prove_identity_type(typing,
		pg_prove_universe(typing, classifiers, context, 0),
		pg_prove_reindex(typing, maps[0], family), pg_prove_reindex(typing, maps[1], family));
	assert(action && target);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(action), pg_evidence_subject(target)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	action = pg_prove_conversion(typing, action, target, pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	assert(action);
	for (unsigned i = 0; i < 2; ++i) {
		enum pg_identity_direction direction = (enum pg_identity_direction)i;
		const struct pg_evidence *input = i ? y : x;
		const struct pg_evidence *quoted = pg_prove_thunk(typing, classifiers, pg_prove_return(typing, classifiers, input));
		const struct pg_object *u = pg_binder(graph);
		const struct pg_evidence *extended = pg_prove_context_extension(typing, context, u,
			pg_prove_classifier(typing, classifiers, context, quoted));
		const struct pg_evidence *variable = pg_prove_variable(typing, extended, u);
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers,
			pg_prove_projection(typing, extended, action), variable, direction);
		const struct pg_evidence *mapped = thunk_map(typing, classifiers, extended,
			pg_prove_projection(typing, extended, path), variable, direction);
		assert(transport && mapped);
		action_result(typing, classifiers, extended, &work, transport, mapped);
		const struct pg_term *term = pg_evidence_subject(transport)->core;
		struct pg_whnf_job *split = pg_whnf_request(&work, &pg_pure_policy, term);
		struct pg_whnf_job *bulk = pg_whnf_request(&whole, &pg_pure_policy, term);
		assert(pg_whnf_advance(bulk, 100000) == PG_EVAL_WHNF);
		assert(pg_whnf_steps(split) == pg_whnf_steps(bulk));
		assert(pg_alpha_equal(pg_whnf_result(split), pg_whnf_result(bulk)) == 1);
		struct pg_eval machine;
		pg_eval_init(&machine, term);
		machine.output = graph;
		machine.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&machine, 100000) == PG_EVAL_WHNF);
		uint64_t steps = machine.steps;
		pg_eval_destroy(&machine);
		/* Family closure construction can be abandoned at every boundary. */
		for (uint64_t cut = 0; cut < steps; ++cut) {
			pg_eval_init(&machine, term);
			machine.output = graph;
			machine.dispatch = pg_pure_policy.dispatch;
			assert(pg_eval_advance(&machine, cut) == PG_EVAL_PENDING);
			const struct pg_term *snapshot = pg_eval_readback(&machine, graph);
			assert(snapshot);
			pg_eval_destroy(&machine);
			converts(&work, snapshot, pg_evidence_subject(mapped)->core);
			pg_eval_init(&machine, term);
			machine.output = graph;
			machine.dispatch = pg_pure_policy.dispatch;
			assert(pg_eval_advance(&machine, cut) == PG_EVAL_PENDING);
			assert(pg_eval_advance(&machine, steps - cut) == PG_EVAL_WHNF);
			assert(machine.steps == steps);
			converts(&work, pg_eval_readback(&machine, graph), pg_evidence_subject(mapped)->core);
			pg_eval_destroy(&machine);
		}
		struct pg_whnf_job *beta = pg_whnf_request(&work, &pg_beta_policy, term);
		assert(pg_whnf_advance(beta, 100000) == PG_EVAL_WHNF && pg_whnf_result(beta) == term);
		/* Substitution after mapping agrees with transport of the substituted value. */
		const struct pg_term *canonical = pg_evidence_subject(pg_prove_identity_transport(typing, classifiers,
			action, quoted, direction))->core;
		converts(&work, pg_application(graph, pg_lambda(graph, u, pg_whnf_result(split)),
			pg_evidence_subject(quoted)->core), canonical);
		converts(&work, pg_application(graph, pg_lambda(graph, u, term), pg_evidence_subject(quoted)->core), canonical);
		for (size_t lifting = 0; lifting < 2; ++lifting) {
			const struct pg_evidence *actual = lifting
				? pg_prove_identity_lift(typing, classifiers, action, quoted, direction)
				: pg_prove_identity_transport(typing, classifiers, action, quoted, direction);
			const struct pg_evidence *inner = lifting
				? pg_prove_identity_lift(typing, classifiers, path, input, direction)
				: pg_prove_identity_transport(typing, classifiers, path, input, direction);
			const struct pg_evidence *expected = pg_prove_thunk(typing, classifiers, pg_prove_return(typing, classifiers, inner));
			assert(actual && expected);
			action_result(typing, classifiers, context, &work, actual, expected);
			const struct pg_term *term = pg_evidence_subject(actual)->core;
			const struct pg_object *captured = pg_binder(graph);
			const struct pg_term *delayed = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
				pg_application(graph, pg_reference(graph, &pg_return_operation), pg_reference(graph, captured)));
			const struct pg_term *field = lifting ? pg_identity_lift(graph, pg_evidence_subject(action)->core, delayed, direction)
				: pg_identity_transport(graph, pg_evidence_subject(action)->core, delayed, direction);
			converts(&work, pg_application(graph, pg_lambda(graph, captured, field), pg_evidence_subject(input)->core), term);
			struct pg_whnf_job *split = pg_whnf_request(&work, &pg_pure_policy, term);
			struct pg_whnf_job *bulk = pg_whnf_request(&whole, &pg_pure_policy, term);
			assert(pg_whnf_advance(bulk, 100000) == PG_EVAL_WHNF);
			assert(pg_whnf_steps(split) == pg_whnf_steps(bulk));
			assert(pg_alpha_equal(pg_whnf_result(split), pg_whnf_result(bulk)) == 1);
			struct pg_whnf_job *beta = pg_whnf_request(&work, &pg_beta_policy, term);
			assert(pg_whnf_advance(beta, 100000) == PG_EVAL_WHNF && pg_whnf_result(beta) == term);
		}
	}
	/* Unknown quoted computations stay suspended; no evaluation under THUNK. */
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *v = pg_reference(graph, z);
	const struct pg_term *self = pg_lambda(graph, z, pg_application(graph, v, v));
	const struct pg_term *quote = pg_application(graph, pg_reference(graph, &pg_thunk_operation), pg_application(graph, self, self));
	const struct pg_term *transport = pg_identity_transport(graph, pg_evidence_subject(action)->core, quote, PG_IDENTITY_RIGHT);
	struct pg_whnf_job *suspended = pg_whnf_request(&work, &pg_pure_policy, transport);
	assert(pg_whnf_advance(suspended, 10000) == PG_EVAL_WHNF);
	const struct pg_term *result = pg_whnf_result(suspended);
	assert(result->kind == PG_APPLICATION && result->as.application.function == pg_reference(graph, &pg_thunk_operation));
	struct pg_whnf_job *forced = pg_whnf_request(&work, &pg_pure_policy,
		pg_application(graph, pg_reference(graph, &pg_force_operation), result));
	assert(pg_whnf_advance(forced, 1000) == PG_EVAL_PENDING);
	const struct pg_term *lift = pg_identity_lift(graph, pg_evidence_subject(action)->core, quote, PG_IDENTITY_RIGHT);
	normalizes(&work, lift, lift);
	/* A runtime probe is deliberately outside the pure memo store/evidence. */
	const struct pg_term *returned = pg_application(graph, pg_reference(graph, &pg_return_operation), pg_evidence_subject(x)->core);
	quote = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
		pg_application(graph, pg_reference(graph, &tick), returned));
	transport = pg_identity_transport(graph, pg_evidence_subject(action)->core, quote, PG_IDENTITY_RIGHT);
	struct pg_eval runtime;
	pg_computation_eval_init(&runtime, graph, transport);
	runtime.dispatch = counted_dispatch;
	ticks = 0;
	assert(pg_eval_advance(&runtime, 10000) == PG_EVAL_WHNF && ticks == 0);
	result = pg_eval_readback(&runtime, graph);
	pg_eval_destroy(&runtime);
	for (size_t i = 1; i <= 2; ++i) {
		pg_computation_eval_init(&runtime, graph, pg_application(graph, pg_reference(graph, &pg_force_operation), result));
		runtime.dispatch = counted_dispatch;
		assert(pg_eval_advance(&runtime, 10000) == PG_EVAL_WHNF && ticks == i);
		converts(&work, pg_eval_readback(&runtime, graph), pg_application(graph,
			pg_reference(graph, &pg_return_operation), pg_identity_transport(graph,
				pg_evidence_subject(path)->core, pg_evidence_subject(x)->core, PG_IDENTITY_RIGHT)));
		pg_eval_destroy(&runtime);
	}
	pg_whnf_work_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static const struct pg_evidence *convert_to_budget(struct pg_typing *typing, struct pg_whnf_work *work,
	const struct pg_evidence *value, const struct pg_evidence *type, uint64_t budget)
{
	assert(value && type);
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, work, pg_evidence_classifier(value), pg_evidence_subject(type)->core) == 0);
	assert(pg_conversion_advance(&comparison, budget) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *result = pg_prove_conversion(typing, value, type, pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	assert(result);
	return result;
}

static const struct pg_evidence *convert_to(struct pg_typing *typing, struct pg_whnf_work *work,
	const struct pg_evidence *value, const struct pg_evidence *type)
{
	return convert_to_budget(typing, work, value, type, 100000);
}

static const struct pg_evidence *expose_classifier(struct pg_typing *typing, struct pg_classifiers *classifiers,
	struct pg_whnf_work *work, const struct pg_evidence *context, const struct pg_evidence *term)
{
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, term);
	assert(type);
	struct pg_whnf_job *job = pg_whnf_request(work, &pg_pure_policy, pg_evidence_subject(type)->core);
	assert(pg_whnf_advance(job, 1000000) == PG_EVAL_WHNF);
	type = pg_prove_normalization(typing, type, pg_whnf_certificate(job));
	assert(type);
	return convert_to_budget(typing, work, term, type, 1000000);
}

/* A well-typed transport recipe is not yet an admissible computation rule. */
static void pi_transport_candidate(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *path)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work work, whole;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_whnf_work_init(&whole, graph) == 0);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *parameters = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *domain = pg_prove_value_type(typing, pg_prove_variable(typing, parameters, a));
	const struct pg_evidence *source = pg_prove_context_extension(typing, parameters, x, domain);
	const struct pg_evidence *types[2], *maps[2];
	for (size_t i = 0; i < 2; ++i) {
		types[i] = pg_prove_type_value(typing, pg_prove_identity_endpoint_type(typing, classifiers, path,
			i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE));
		maps[i] = pg_prove_substitution(typing, parameters, context, 1, &types[i]);
	}
	for (size_t dependent = 0; dependent < 2; ++dependent) {
		const struct pg_evidence *b = pg_prove_projection(typing, source, domain);
		if (dependent) b = pg_prove_identity_type(typing, b,
			pg_prove_variable(typing, source, x), pg_prove_variable(typing, source, x));
		const struct pg_evidence *family = pg_prove_type_value(typing, pg_prove_thunk_type(typing, classifiers,
			pg_prove_pi(typing, classifiers, domain, source, pg_prove_return_type(typing, classifiers, b))));
		const struct pg_evidence *action = pg_prove_family_action(typing,
			pg_prove_classifier(typing, classifiers, parameters, family), family, maps[0], maps[1], 1, &path);
		const struct pg_evidence *endpoints[2] = {pg_prove_reindex(typing, maps[0], family), pg_prove_reindex(typing, maps[1], family)};
		action = convert_to(typing, &work, action, pg_prove_identity_type(typing,
			pg_prove_universe(typing, classifiers, context, 0), endpoints[0], endpoints[1]));
		for (unsigned i = 0; i < 2; ++i) {
			enum pg_identity_direction direction = (enum pg_identity_direction)i;
			enum pg_identity_direction reverse = i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
			const struct pg_object *f = pg_binder(graph), *y = pg_binder(graph);
			const struct pg_evidence *function_context = pg_prove_context_extension(typing, context, f,
				pg_prove_value_type(typing, endpoints[i]));
			const struct pg_evidence *function = pg_prove_variable(typing, function_context, f);
			const struct pg_evidence *actual = pg_prove_identity_transport(typing, classifiers,
				pg_prove_projection(typing, function_context, action), function, direction);
			const struct pg_evidence *target_domain = pg_prove_projection(typing, function_context,
				pg_prove_value_type(typing, types[1 - i]));
			const struct pg_evidence *body_context = pg_prove_context_extension(typing, function_context, y, target_domain);
			const struct pg_evidence *r = pg_prove_projection(typing, body_context, path);
			const struct pg_evidence *target = pg_prove_variable(typing, body_context, y);
			const struct pg_evidence *argument = pg_prove_identity_transport(typing, classifiers, r, target, reverse);
			const struct pg_evidence *lift = pg_prove_identity_lift(typing, classifiers, r, target, reverse);
			const struct pg_evidence *call = pg_prove_application(typing,
				pg_prove_force(typing, pg_prove_projection(typing, body_context, function)), argument);
			const struct pg_evidence *substitutions[2], *prefixes[2];
			for (size_t j = 0; j < 2; ++j) {
				const struct pg_evidence *type = pg_prove_projection(typing, body_context, types[j]);
				prefixes[j] = pg_prove_substitution(typing, parameters, body_context, 1, &type);
				substitutions[j] = pg_prove_substitution_pair(typing, prefixes[j], source, j == i ? argument : target);
			}
			lift = convert_to(typing, &work, lift, pg_prove_family_identity_type(typing, domain,
				prefixes[0], prefixes[1], 1, &r, i ? target : argument, i ? argument : target));
			const struct pg_evidence *paths[] = {r, lift};
			const struct pg_evidence *b_value = pg_prove_type_value(typing, b);
			const struct pg_evidence *b_path = pg_prove_family_action(typing,
				pg_prove_classifier(typing, classifiers, source, b_value), b_value,
				substitutions[0], substitutions[1], 2, paths);
			b_path = convert_to(typing, &work, b_path, pg_prove_identity_type(typing,
				pg_prove_universe(typing, classifiers, body_context, 0),
				pg_prove_reindex(typing, substitutions[0], b_value), pg_prove_reindex(typing, substitutions[1], b_value)));
			const struct pg_evidence *body = pg_prove_force(typing, thunk_map(typing, classifiers, body_context,
				b_path, pg_prove_thunk(typing, classifiers, call), direction));
			const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, target_domain, body_context,
				pg_prove_classifier(typing, classifiers, body_context, body));
			const struct pg_evidence *expected = pg_prove_thunk(typing, classifiers, pg_prove_lambda(typing, pi, body));
			assert(actual && expected);
			expected = convert_to(typing, &work, expected,
				pg_prove_classifier(typing, classifiers, function_context, actual));
			const struct pg_term *term = pg_evidence_subject(expected)->core;
			const struct pg_term *reflected;
			{
				/* Application, unlike bare function comparison, can expose the
				 * transport recipe without adding function eta to conversion. */
				const struct pg_evidence *actual_call = pg_prove_application(typing,
					pg_prove_force(typing, pg_prove_projection(typing, body_context, actual)), target);
				const struct pg_evidence *recipe_call = pg_prove_application(typing,
					pg_prove_force(typing, pg_prove_projection(typing, body_context, expected)), target);
				assert(actual_call && recipe_call);
				converts(&work, pg_evidence_classifier(actual_call), pg_evidence_classifier(recipe_call));
				converts(&work, pg_evidence_subject(actual_call)->core, pg_evidence_subject(recipe_call)->core);
				const struct pg_term *call_term = pg_evidence_subject(actual_call)->core;
				struct pg_whnf_job *split = pg_whnf_request(&work, &pg_pure_policy, call_term);
				struct pg_whnf_job *bulk = pg_whnf_request(&whole, &pg_pure_policy, call_term);
				while (pg_whnf_advance(split, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(split) < 100000);
				assert(pg_whnf_advance(bulk, 100000) == PG_EVAL_WHNF);
				assert(pg_whnf_steps(split) == pg_whnf_steps(bulk));
				assert(pg_alpha_equal(pg_whnf_result(split), pg_whnf_result(bulk)) == 1);
				assert(pg_prove_normalization(typing, actual_call, pg_whnf_certificate(split)));
				const struct pg_term *callee = call_term->as.application.function;
				struct pg_whnf_job *preforce = pg_whnf_request(&work, &pg_pure_policy, callee);
				assert(pg_whnf_advance(preforce, 100000) == PG_EVAL_WHNF);
				converts(&work, pg_application(graph, pg_whnf_result(preforce), pg_evidence_subject(target)->core), call_term);
				if (!pg_identity_action_view(pg_evidence_subject(path)->core, &reflected)) {
					/* No eta expansion even when FORCE itself is precomputed. */
					assert(pg_alpha_equal(pg_whnf_result(preforce), callee) == 1);
					const struct pg_term *right_type = pg_evidence_subject(types[1])->core;
					const struct pg_term *path_term = pg_evidence_subject(path)->core;
					assert(right_type->kind == PG_REFERENCE && path_term->kind == PG_REFERENCE);
					struct pg_binding_value diagonal[] = {
						{right_type->as.reference, pg_evidence_subject(types[0])->core},
						{path_term->as.reference, pg_identity_action(graph, pg_evidence_subject(types[0])->core)}
					};
					/* Contract before restricting the path, or restrict first. */
					converts(&work, pg_term_substitute(graph, call_term, 2, diagonal),
						pg_term_substitute(graph, pg_whnf_result(split), 2, diagonal));
				}
				struct pg_whnf_job *beta_call = pg_whnf_request(&work, &pg_beta_policy, call_term);
				assert(pg_whnf_advance(beta_call, 10000) == PG_EVAL_WHNF && pg_whnf_result(beta_call) == call_term);
				if (!dependent) {
					/* Runtime-only probe: transporting a function never calls it,
					 * and each application invokes the body exactly once. */
					const struct pg_term *body = pg_application(graph, pg_reference(graph, &tick),
						pg_application(graph, pg_reference(graph, &pg_return_operation), pg_reference(graph, y)));
					struct pg_binding_value binding = {f, pg_application(graph,
						pg_reference(graph, &pg_thunk_operation), pg_lambda(graph, y, body))};
					const struct pg_term *quote = pg_term_substitute(graph, pg_evidence_subject(actual)->core, 1, &binding);
					struct pg_eval runtime;
					pg_computation_eval_init(&runtime, graph, quote);
					runtime.dispatch = counted_dispatch;
					ticks = 0;
					assert(pg_eval_advance(&runtime, 100000) == PG_EVAL_WHNF && ticks == 0);
					pg_eval_destroy(&runtime);
					const struct pg_term *invoke = pg_term_substitute(graph, call_term, 1, &binding);
					for (size_t n = 1; n <= 2; ++n) {
						pg_computation_eval_init(&runtime, graph, invoke);
						runtime.dispatch = counted_dispatch;
						assert(pg_eval_advance(&runtime, 100000) == PG_EVAL_WHNF && ticks == n);
						pg_eval_destroy(&runtime);
					}
				}
			}
			if (!dependent && pg_identity_action_view(pg_evidence_subject(path)->core, &reflected)) {
				const struct pg_term *eta = pg_application(graph, pg_reference(graph, &pg_thunk_operation),
					pg_lambda(graph, y, pg_application(graph,
						pg_application(graph, pg_reference(graph, &pg_force_operation), pg_evidence_subject(function)->core),
						pg_reference(graph, y))));
				converts(&work, term, eta);
				/* Strict diagonal transport returns f; the recipe returns its
				 * function eta expansion, intentionally not current DefEq. */
				converts(&work, pg_evidence_subject(actual)->core, pg_evidence_subject(function)->core);
				struct pg_conversion comparison;
				assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(actual)->core, term) == 0);
				assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
				assert(!pg_conversion_certificate(&comparison));
				pg_conversion_destroy(&comparison);
			}
			struct pg_whnf_job *split = pg_whnf_request(&work, &pg_pure_policy, term);
			struct pg_whnf_job *bulk = pg_whnf_request(&whole, &pg_pure_policy, term);
			while (pg_whnf_advance(split, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(split) < 100000);
			assert(pg_whnf_advance(bulk, 100000) == PG_EVAL_WHNF);
			assert(pg_whnf_steps(split) == pg_whnf_steps(bulk));
			assert(pg_alpha_equal(pg_whnf_result(split), pg_whnf_result(bulk)) == 1);
			struct pg_whnf_job *beta = pg_whnf_request(&work, &pg_beta_policy, term);
			assert(pg_whnf_advance(beta, 10000) == PG_EVAL_WHNF && pg_whnf_result(beta) == term);
		}
	}
	pg_whnf_work_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static void curried_transport(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *context, const struct pg_evidence *path, const struct pg_evidence *result_path)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph), *z = pg_binder(graph);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *parameters = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_object *b = pg_binder(graph);
	const struct pg_evidence *prefix = parameters;
	parameters = pg_prove_context_extension(typing, parameters, b,
		pg_prove_universe(typing, classifiers, parameters, 0));
	const struct pg_evidence *domain = pg_prove_value_type(typing, pg_prove_variable(typing, parameters, a));
	const struct pg_evidence *codomain = pg_prove_value_type(typing, pg_prove_variable(typing, parameters, b));
	const struct pg_evidence *first = pg_prove_context_extension(typing, parameters, x, domain);
	const struct pg_evidence *second = pg_prove_context_extension(typing, first, z,
		pg_prove_projection(typing, first, domain));
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, pg_prove_projection(typing, first, domain), second,
		pg_prove_return_type(typing, classifiers, pg_prove_projection(typing, second, codomain)));
	pi = pg_prove_pi(typing, classifiers, domain, first, pi);
	const struct pg_evidence *family = pg_prove_type_value(typing, pg_prove_thunk_type(typing, classifiers, pi));
	const struct pg_evidence *types[2], *maps[2], *endpoints[2], *prefix_maps[2], *result_types[2];
	for (size_t i = 0; i < 2; ++i) {
		types[i] = pg_prove_type_value(typing, pg_prove_identity_endpoint_type(typing, classifiers, path,
			i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE));
		result_types[i] = pg_prove_type_value(typing,
			pg_prove_identity_endpoint_type(typing, classifiers, result_path,
				i ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE));
		const struct pg_evidence *images[] = {types[i], result_types[i]};
		prefix_maps[i] = pg_prove_substitution(typing, prefix, context, 1, &types[i]);
		maps[i] = pg_prove_substitution(typing, parameters, context, 2, images);
		endpoints[i] = pg_prove_reindex(typing, maps[i], family);
	}
	const struct pg_evidence *paths[] = {path, convert_to(typing, &work, result_path,
		pg_prove_family_identity_type(typing, pg_prove_universe(typing, classifiers, prefix, 0),
			prefix_maps[0], prefix_maps[1], 1, &path, result_types[0], result_types[1]))};
	const struct pg_evidence *action = pg_prove_family_action(typing,
		pg_prove_classifier(typing, classifiers, parameters, family), family, maps[0], maps[1], 2, paths);
	action = convert_to(typing, &work, action, pg_prove_identity_type(typing,
		pg_prove_universe(typing, classifiers, context, 0), endpoints[0], endpoints[1]));
	for (unsigned i = 0; i < 2; ++i) {
		enum pg_identity_direction direction = (enum pg_identity_direction)i;
		enum pg_identity_direction reverse = i ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
		const struct pg_object *f = pg_binder(graph), *y = pg_binder(graph), *w = pg_binder(graph);
		const struct pg_evidence *scope = pg_prove_context_extension(typing, context, f, pg_prove_value_type(typing, endpoints[i]));
		scope = pg_prove_context_extension(typing, scope, y, pg_prove_projection(typing, scope, pg_prove_value_type(typing, types[1 - i])));
		scope = pg_prove_context_extension(typing, scope, w, pg_prove_projection(typing, scope, pg_prove_value_type(typing, types[1 - i])));
		const struct pg_evidence *r = pg_prove_projection(typing, scope, path);
		const struct pg_evidence *function = pg_prove_variable(typing, scope, f);
		const struct pg_evidence *left = pg_prove_variable(typing, scope, y), *right = pg_prove_variable(typing, scope, w);
		const struct pg_evidence *actual = pg_prove_force(typing, pg_prove_identity_transport(typing, classifiers,
			pg_prove_projection(typing, scope, action), function, direction));
		const struct pg_evidence *partial = pg_prove_application(typing, actual, left);
		actual = pg_prove_application(typing, partial, right);
		const struct pg_evidence *recipe = pg_prove_application(typing, pg_prove_force(typing, function),
			pg_prove_identity_transport(typing, classifiers, r, left, reverse));
		recipe = pg_prove_application(typing, recipe, pg_prove_identity_transport(typing, classifiers, r, right, reverse));
		const struct pg_evidence *source_call = recipe;
		recipe = pg_prove_force(typing, thunk_map(typing, classifiers, scope, pg_prove_projection(typing, scope, result_path),
			pg_prove_thunk(typing, classifiers, recipe), direction));
		assert(actual && recipe && partial);
		converts(&work, pg_evidence_classifier(actual), pg_evidence_classifier(recipe));
		converts(&work, pg_evidence_subject(actual)->core, pg_evidence_subject(recipe)->core);
		if (path != result_path) {
			const struct pg_evidence *wrong = pg_prove_force(typing, thunk_map(typing, classifiers, scope, r,
				pg_prove_thunk(typing, classifiers, source_call), direction));
			assert(wrong);
			struct pg_conversion comparison;
			assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(actual)->core, pg_evidence_subject(wrong)->core) == 0);
			assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
			assert(!pg_conversion_certificate(&comparison));
			pg_conversion_destroy(&comparison);
		}
		struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(partial)->core);
		while (pg_whnf_advance(job, 1) == PG_EVAL_PENDING) assert(pg_whnf_steps(job) < 100000);
		assert(pg_whnf_status(job) == PG_EVAL_WHNF);
		converts(&work, pg_application(graph, pg_whnf_result(job), pg_evidence_subject(right)->core),
			pg_evidence_subject(actual)->core);
	}
	pg_whnf_work_destroy(&work);
}

static void transport_fields(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *r, const struct pg_evidence *s,
	const struct pg_evidence *x, const struct pg_evidence *y, const struct pg_evidence *substitution)
{
	struct pg_graph *graph = typing->graph;
	thunk_transport(typing, classifiers, scope, r, x, y);
	thunk_transport(typing, classifiers, scope, s, x, y);
	pi_transport_candidate(typing, classifiers, scope, r);
	pi_transport_candidate(typing, classifiers, scope, s);
	curried_transport(typing, classifiers, scope, r, r);
	curried_transport(typing, classifiers, scope, s, s);
	curried_transport(typing, classifiers, scope, r, s);
	curried_transport(typing, classifiers, scope, s, r);
	struct pg_whnf_work work, whole;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_whnf_work_init(&whole, graph) == 0);
	const struct pg_evidence *a = pg_prove_identity_endpoint_type(typing, classifiers, r, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *a_value = pg_prove_type_value(typing, a);
	const struct pg_evidence *universe = pg_prove_classifier(typing, classifiers, scope, a_value);
	const struct pg_evidence *diagonal = pg_prove_reflexivity(typing, universe, a_value);
	const struct pg_evidence *reflexivity = pg_prove_reflexivity(typing, a, x);
	assert(diagonal && reflexivity);
	pi_transport_candidate(typing, classifiers, scope, diagonal);
	curried_transport(typing, classifiers, scope, diagonal, diagonal);
	for (unsigned i = 0; i < 2; ++i) {
		enum pg_identity_direction direction = (enum pg_identity_direction)i;
		const struct pg_evidence *input = i ? y : x;
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers, r, input, direction);
		const struct pg_evidence *lift = pg_prove_identity_lift(typing, classifiers, r, input, direction);
		assert(transport && lift);
		assert(pg_evidence_judgement(transport) == PG_JUDGEMENT_VALUE);
		assert(pg_evidence_classifier(transport) == pg_evidence_classifier(i ? x : y));
		const struct pg_evidence *type = pg_prove_identity_instance(typing, classifiers, r,
			i ? transport : input, i ? input : transport);
		assert(type && pg_evidence_classifier(lift) == pg_evidence_subject(type)->core);
		assert(pg_prove_classifier(typing, classifiers, scope, lift) == type);
		assert(pg_evidence_subject(pg_prove_classifier(typing, classifiers, scope, transport))->core
			== pg_evidence_classifier(transport));
		normalizes(&work, pg_evidence_subject(transport)->core, pg_evidence_subject(transport)->core);
		normalizes(&work, pg_evidence_subject(lift)->core, pg_evidence_subject(lift)->core);
		size_t terms = graph->terms.count, proofs = typing->proofs.count;
		assert(pg_prove_identity_transport(typing, classifiers, r, input, direction) == transport);
		assert(pg_prove_identity_lift(typing, classifiers, r, input, direction) == lift);
		assert(graph->terms.count == terms && typing->proofs.count == proofs);
		const struct pg_evidence *other = pg_prove_identity_transport(typing, classifiers, s, input, direction);
		const struct pg_evidence *other_lift = pg_prove_identity_lift(typing, classifiers, s, input, direction);
		assert(other && other_lift);
		struct pg_conversion comparison;
		assert(pg_conversion_init(&comparison, &work, pg_evidence_subject(transport)->core, pg_evidence_subject(other)->core) == 0);
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
		pg_conversion_destroy(&comparison);
		const struct pg_evidence *moved = pg_prove_reindex(typing, substitution, transport);
		assert(moved && pg_evidence_subject(moved)->core == pg_evidence_subject(other)->core);
		moved = pg_prove_reindex(typing, substitution, lift);
		assert(moved && pg_evidence_subject(moved)->core == pg_evidence_subject(other_lift)->core);
		assert(pg_alpha_equal(pg_evidence_classifier(moved), pg_evidence_classifier(other_lift)) == 1);
		assert(pg_prove_classifier(typing, classifiers, scope, moved));
		const struct pg_evidence *tr_refl = pg_prove_identity_transport(typing, classifiers, diagonal, x, direction);
		const struct pg_evidence *lift_refl = pg_prove_identity_lift(typing, classifiers, diagonal, x, direction);
		action_result(typing, classifiers, scope, &work, tr_refl, x);
		action_result(typing, classifiers, scope, &work, lift_refl, reflexivity);
		const struct pg_evidence *fields[] = {tr_refl, lift_refl};
		for (size_t j = 0; j < 2; ++j) {
			struct pg_whnf_job *split = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(fields[j])->core);
			struct pg_whnf_job *bulk = pg_whnf_request(&whole, &pg_pure_policy, pg_evidence_subject(fields[j])->core);
			assert(pg_whnf_advance(bulk, 100000) == PG_EVAL_WHNF);
			assert(pg_whnf_steps(split) == pg_whnf_steps(bulk));
			assert(pg_whnf_result(split) == pg_whnf_result(bulk));
		}
		assert(!pg_prove_identity_transport(typing, classifiers, r, i ? x : y, direction));
		assert(!pg_prove_identity_lift(typing, classifiers, r, i ? x : y, direction));
		const struct pg_term *partial = pg_evidence_subject(transport)->core->as.application.function;
		normalizes(&work, partial, partial);
		const struct pg_object *z = pg_binder(graph);
		const struct pg_term *captured = pg_lambda(graph, z, pg_identity_transport(graph,
			pg_evidence_subject(r)->core, pg_reference(graph, z), direction));
		normalizes(&work, pg_application(graph, captured, pg_evidence_subject(input)->core), pg_evidence_subject(transport)->core);
	}
	assert(!pg_prove_identity_transport(typing, classifiers, reflexivity, x, PG_IDENTITY_RIGHT));
	assert(!pg_prove_identity_lift(typing, classifiers, reflexivity, x, PG_IDENTITY_RIGHT));
	assert(!pg_prove_identity_transport(typing, classifiers, r,
		pg_prove_return(typing, classifiers, x), PG_IDENTITY_RIGHT));
	assert(!pg_prove_identity_transport(typing, classifiers, NULL, x, PG_IDENTITY_RIGHT));
	assert(!pg_prove_identity_lift(typing, classifiers, r, NULL, PG_IDENTITY_RIGHT));
	assert(!pg_prove_identity_transport(typing, classifiers, r, x, (enum pg_identity_direction)2));
	assert(!pg_identity_lift(graph, pg_evidence_subject(r)->core, pg_evidence_subject(x)->core, (enum pg_identity_direction)-1));
	const struct pg_evidence *extended = pg_prove_context_extension(typing, scope, pg_binder(graph), universe);
	assert(!pg_prove_identity_transport(typing, classifiers, r, pg_prove_projection(typing, extended, x), PG_IDENTITY_RIGHT));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, graph) == 0);
	assert(!pg_prove_identity_transport(&foreign, classifiers, r, x, PG_IDENTITY_RIGHT));
	pg_typing_destroy(&foreign);
	const struct pg_evidence *quoted = pg_prove_thunk(typing, classifiers, pg_prove_return(typing, classifiers, x));
	const struct pg_evidence *u_type = pg_prove_classifier(typing, classifiers, scope, quoted);
	const struct pg_evidence *u_value = pg_prove_type_value(typing, u_type);
	const struct pg_evidence *u_diagonal = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, scope, u_value), u_value);
	for (unsigned i = 0; i < 2; ++i) {
		enum pg_identity_direction direction = (enum pg_identity_direction)i;
		action_result(typing, classifiers, scope, &work,
			pg_prove_identity_transport(typing, classifiers, u_diagonal, quoted, direction), quoted);
		action_result(typing, classifiers, scope, &work,
			pg_prove_identity_lift(typing, classifiers, u_diagonal, quoted, direction),
			pg_prove_reflexivity(typing, u_type, quoted));
		/* The separately typed map exposes its right unit only after reducing
		 * the continuation body; comparing outer WHNF shapes is insufficient. */
		const struct pg_object *u = pg_binder(graph);
		const struct pg_evidence *context = pg_prove_context_extension(typing, scope, u, u_type);
		const struct pg_evidence *value = pg_prove_variable(typing, context, u);
		action_result(typing, classifiers, context, &work,
			pg_prove_identity_transport(typing, classifiers,
				pg_prove_projection(typing, context, u_diagonal), value, direction),
			thunk_map(typing, classifiers, context, pg_prove_projection(typing, context, diagonal), value, direction));
	}
	/* Unknown families and quoted computations are not execution requests. */
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *vz = pg_reference(graph, z);
	const struct pg_term *self = pg_lambda(graph, z, pg_application(graph, vz, vz));
	const struct pg_term *omega = pg_application(graph, self, self);
	const struct pg_term *neutral = pg_identity_transport(graph, pg_evidence_subject(r)->core, omega, PG_IDENTITY_RIGHT);
	normalizes(&work, neutral, neutral);
	const struct pg_term *quote = pg_application(graph, pg_reference(graph, &pg_thunk_operation), omega);
	normalizes(&work, pg_identity_transport(graph, pg_evidence_subject(diagonal)->core, quote, PG_IDENTITY_RIGHT), quote);
	/* Do not pretend an unknown transport field is an ordinary Pi function. */
	const struct pg_term *higher = pg_identity_apply(graph, pg_lambda(graph, z,
		pg_identity_transport(graph, pg_evidence_subject(r)->core, vz, PG_IDENTITY_RIGHT)),
		pg_evidence_subject(x)->core, pg_evidence_subject(x)->core, pg_evidence_subject(reflexivity)->core);
	normalizes(&work, higher, pg_identity_action(graph,
		pg_identity_transport(graph, pg_evidence_subject(r)->core, pg_evidence_subject(x)->core, PG_IDENTITY_RIGHT)));
	higher = pg_identity_apply(graph, pg_lambda(graph, z,
		pg_identity_transport(graph, pg_evidence_subject(r)->core, vz, PG_IDENTITY_RIGHT)),
		pg_evidence_subject(x)->core, pg_evidence_subject(x)->core, pg_reference(graph, pg_binder(graph)));
	normalizes(&work, higher, higher);
	pg_whnf_work_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static void square_transposition_boundary(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, pg_binder(graph),
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 2);
	const struct pg_evidence *contexts[2];
	struct pg_coordinate swap_axes[] = {{PG_AXIS, 1}, {PG_AXIS, 0}};
	const struct pg_dimension_map *swap = pg_dimension_map(&dimensions, 2, 2, swap_axes);
	const struct pg_binding_face *transposed = pg_binding_face(&dimensions, cube, swap);
	contexts[0] = pg_identity_cube_context(typing, &dimensions, source, 1, &cube, pg_dimension_identity(&dimensions, 2));
	contexts[1] = pg_identity_cube_context(typing, &dimensions, source, 1, &cube, swap);
	assert(contexts[0] && contexts[1]);
	/* Instantiate the opposite orientation's boundary template using only the
	 * original proper faces. Neither context supplies a center inhabitant. */
	const struct pg_evidence *proper_context = pg_evidence_premise(contexts[0], 0);
	const struct pg_evidence *original_type = pg_evidence_premise(contexts[0], 1);
	const struct pg_evidence *opposite_extensions[9], *opposite_cursor = contexts[1];
	for (size_t i = 9; i; --i) {
		opposite_extensions[i - 1] = opposite_cursor;
		opposite_cursor = pg_evidence_premise(opposite_cursor, 0);
	}
	const struct pg_evidence *boundary_map = pg_prove_substitution(typing, empty, proper_context, 0, NULL);
	for (size_t i = 0; i < 8; ++i) {
		const struct pg_binding_face *face = pg_binding_face_view(pg_evidence_context(opposite_extensions[i])->binder);
		const struct pg_dimension_map *ordered, *intrinsic;
		assert(pg_dimension_face_factor(&dimensions, face->face, &ordered, &intrinsic) == 0);
		assert(intrinsic == pg_dimension_identity(&dimensions, ordered->source));
		const struct pg_evidence *image = pg_identity_proper_face(typing, classifiers, proper_context, original_type, ordered);
		assert(image);
		const struct pg_evidence *required = pg_prove_reindex(typing, boundary_map,
			pg_evidence_premise(opposite_extensions[i], 1));
		image = convert_to(typing, &work, image, required);
		boundary_map = pg_prove_substitution_pair(typing, boundary_map, opposite_extensions[i], image);
		assert(boundary_map);
	}
	const struct pg_evidence *opposite_type = pg_prove_reindex(typing, boundary_map,
		pg_evidence_premise(contexts[1], 1));
	assert(opposite_type && pg_evidence_context(opposite_type) == pg_evidence_context(proper_context));
	assert(pg_evidence_judgement(opposite_type) == pg_evidence_judgement(original_type));
	assert(pg_evidence_classifier(opposite_type) == pg_evidence_classifier(original_type));
	assert(!pg_context_lookup(pg_evidence_context(proper_context), &transposed->variable));
	assert(!pg_context_lookup(pg_evidence_context(proper_context),
		&pg_binding_face(&dimensions, cube, pg_dimension_identity(&dimensions, 2))->variable));
	struct pg_conversion opposite_comparison;
	assert(pg_conversion_init(&opposite_comparison, &work,
		pg_evidence_subject(original_type)->core, pg_evidence_subject(opposite_type)->core) == 0);
	assert(pg_conversion_advance(&opposite_comparison, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&opposite_comparison);
	for (size_t orientation = 0; orientation < 2; ++orientation) {
		const struct pg_evidence *declaration = contexts[orientation];
		for (size_t i = 0; i < 9; ++i) {
			const struct pg_binding_face *face = pg_binding_face_view(pg_evidence_context(declaration)->binder);
			assert(face && face->cube == cube);
			const struct pg_evidence *value = pg_prove_variable(typing, contexts[orientation], &face->variable);
			assert(value && pg_evidence_subject(value)->core == pg_reference(graph, &face->variable));
			assert(pg_prove_classifier(typing, classifiers, contexts[orientation], value));
			declaration = pg_evidence_premise(declaration, 0);
		}
		assert(declaration == empty);
	}
	size_t terms = graph->terms.count, proofs = typing->proofs.count;
	assert(pg_identity_cube_context(typing, &dimensions, source, 1, &cube, swap) == contexts[1]);
	assert(graph->terms.count == terms && typing->proofs.count == proofs);
	const struct pg_evidence *extensions[9], *cursor = contexts[0];
	for (size_t i = 9; i; --i) {
		extensions[i - 1] = cursor;
		cursor = pg_evidence_premise(cursor, 0);
	}
	const struct pg_evidence *map = pg_prove_substitution(typing, empty, contexts[1], 0, NULL);
	const struct pg_dimension_map *inverse = pg_dimension_inverse(&dimensions, swap);
	for (size_t i = 0; i < 8; ++i) {
		/* Match geometric corners/edges, not declaration-list positions. */
		const struct pg_binding_face *face = pg_binding_face_view(pg_evidence_context(extensions[i])->binder);
		const struct pg_dimension_map *ordered, *intrinsic;
		assert(pg_dimension_face_factor(&dimensions, pg_dimension_compose(&dimensions, inverse, face->face),
			&ordered, &intrinsic) == 0);
		const struct pg_binding_face *image = pg_binding_face(&dimensions, cube,
			pg_dimension_compose(&dimensions, swap, ordered));
		assert(intrinsic == pg_dimension_identity(&dimensions, face->face->source));
		assert(pg_dimension_compose(&dimensions, image->face, intrinsic) == face->face);
		const struct pg_evidence *value = pg_prove_variable(typing, contexts[1], &image->variable);
		assert(value);
		value = convert_to(typing, &work, value, pg_prove_reindex(typing, map, pg_evidence_premise(extensions[i], 1)));
		map = pg_prove_substitution_pair(typing, map, extensions[i], value);
		assert(map);
	}
	const struct pg_evidence *center = pg_prove_variable(typing, contexts[1], &transposed->variable);
	const struct pg_dimension_map *center_face, *center_orientation;
	assert(pg_dimension_face_factor(&dimensions, inverse, &center_face, &center_orientation) == 0);
	assert(center_face == pg_dimension_identity(&dimensions, 2));
	assert(center_orientation == swap);
	const struct pg_evidence *expected = pg_prove_reindex(typing, map, pg_evidence_premise(extensions[8], 1));
	assert(center && expected);
	const struct pg_evidence *recovered = pg_identity_formation(typing, classifiers, expected);
	assert(recovered && pg_evidence_rule(recovered) == PG_FAMILY_IDENTITY_FORM);
	assert(pg_evidence_context(recovered) == pg_evidence_context(expected));
	assert(pg_evidence_premise(recovered, 0) == pg_evidence_premise(pg_evidence_premise(extensions[8], 1), 0));
	assert(pg_alpha_equal(pg_evidence_subject(recovered)->core, pg_evidence_subject(expected)->core) == 1);
	assert(pg_identity_formation(typing, classifiers, expected) == recovered);
	for (size_t depth = 0; depth < 2; ++depth) {
		for (unsigned side = 0; side < 2; ++side) {
			enum pg_identity_direction direction = side ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
			const struct pg_evidence *before = pg_identity_face_endpoint(typing, classifiers,
				pg_evidence_premise(map, 0), pg_evidence_premise(extensions[8], 1), depth, direction);
			const struct pg_evidence *after = pg_identity_face_endpoint(typing, classifiers,
				contexts[1], expected, depth, direction);
			assert(action_result(typing, classifiers, contexts[1], &work, after,
				pg_prove_reindex(typing, map, before)));
		}
	}
	const struct pg_evidence *type_value = pg_prove_type_value(typing, expected);
	assert(type_value && !pg_identity_formation(typing, classifiers, type_value));
	assert(pg_identity_formation(typing, classifiers, pg_prove_value_type(typing, type_value)) == recovered);
	const struct pg_evidence *source_value = pg_prove_type_value(typing, pg_evidence_premise(extensions[8], 1));
	const struct pg_evidence *mapped_value = pg_prove_reindex(typing, map, source_value);
	assert(pg_identity_formation(typing, classifiers, pg_prove_value_type(typing, mapped_value)) == recovered);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, contexts[1], pg_binder(graph),
		pg_prove_universe(typing, classifiers, contexts[1], 0));
	const struct pg_evidence *projected = pg_prove_projection(typing, extended, expected);
	const struct pg_evidence *projected_boundary = pg_identity_formation(typing, classifiers, projected);
	assert(projected_boundary && pg_evidence_context(projected_boundary) == pg_evidence_context(extended));
	assert(pg_alpha_equal(pg_evidence_subject(projected_boundary)->core, pg_evidence_subject(projected)->core) == 1);
	const struct pg_evidence *projected_value = pg_prove_projection(typing, extended, mapped_value);
	assert(pg_identity_formation(typing, classifiers, pg_prove_value_type(typing, projected_value)) == projected_boundary);
	assert(!pg_identity_formation(typing, classifiers, center));
	assert(!pg_identity_formation(typing, classifiers, pg_prove_universe(typing, classifiers, empty, 0)));
	/* Reuse the selected boundary for computation Identity, not a value-side
	 * encoding or a conversion of its polarity. */
	const struct pg_evidence *original = pg_evidence_premise(extensions[8], 1);
	struct pg_identity_boundary original_boundary;
	assert(pg_identity_boundary_view(original, &original_boundary));
	for (size_t i = 0; i < original_boundary.path_count; ++i)
		assert(original_boundary.paths[i] == pg_evidence_premise(original, i + 3));
	assert(!pg_identity_boundary_view(center, &original_boundary));
	assert(!pg_identity_boundary_view(NULL, &original_boundary));
	assert(original_boundary.family == pg_evidence_premise(original, 0));
	const struct pg_evidence *computation = pg_prove_family_identity_type(typing,
		pg_prove_return_type(typing, classifiers, original_boundary.family),
		original_boundary.left_substitution, original_boundary.right_substitution,
		original_boundary.path_count, original_boundary.paths,
		pg_prove_return(typing, classifiers, original_boundary.left),
		pg_prove_return(typing, classifiers, original_boundary.right));
	assert(computation && pg_evidence_judgement(computation) == PG_JUDGEMENT_COMPUTATION_TYPE);
	const struct pg_evidence *computation_source = computation;
	computation = pg_prove_reindex(typing, map, computation);
	const struct pg_evidence *computation_boundary = pg_identity_formation(typing, classifiers, computation);
	assert(computation_boundary && pg_evidence_judgement(computation_boundary) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_evidence_classifier(computation_boundary) == pg_evidence_classifier(computation));
	assert(pg_alpha_equal(pg_evidence_subject(computation_boundary)->core, pg_evidence_subject(computation)->core) == 1);
	assert(!pg_prove_type_value(typing, computation_boundary));
	struct pg_whnf_job *normalized_job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(computation)->core);
	assert(pg_whnf_advance(normalized_job, 100000) == PG_EVAL_WHNF);
	const struct pg_evidence *normalized = pg_prove_normalization(typing, computation, pg_whnf_certificate(normalized_job));
	assert(normalized && pg_evidence_rule(normalized) == PG_PURE_NORMALIZATION);
	assert(pg_identity_formation(typing, classifiers, normalized) == computation_boundary);
	assert(pg_evidence_subject(normalized)->core != pg_evidence_subject(computation_boundary)->core);
	converts(&work, pg_evidence_subject(normalized)->core, pg_evidence_subject(computation_boundary)->core);
	normalized_job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(computation_source)->core);
	assert(pg_whnf_advance(normalized_job, 100000) == PG_EVAL_WHNF);
	const struct pg_evidence *normalized_source = pg_prove_normalization(typing, computation_source, pg_whnf_certificate(normalized_job));
	const struct pg_evidence *normalized_then_mapped = pg_prove_reindex(typing, map, normalized_source);
	assert(normalized_then_mapped && pg_identity_formation(typing, classifiers, normalized_then_mapped) == computation_boundary);
	converts(&work, pg_evidence_subject(normalized_then_mapped)->core, pg_evidence_subject(normalized)->core);
	assert(!pg_prove_substitution_pair(typing, map, extensions[8], center));
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(center), pg_evidence_subject(expected)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void uniform_transport(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = typing->graph;
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	assert(pg_dimensions_init(&dimensions, graph) == 0);
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(graph), *b = pg_binder(graph), *r = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 0));
	source = pg_prove_context_extension(typing, source, b, pg_prove_universe(typing, classifiers, source, 0));
	const struct pg_evidence *relation = pg_prove_identity_type(typing, pg_prove_universe(typing, classifiers, source, 0),
		pg_prove_variable(typing, source, a), pg_prove_variable(typing, source, b));
	source = pg_prove_context_extension(typing, source, r, relation);
	const struct pg_evidence *r_value = pg_prove_variable(typing, source, r);
	const struct pg_evidence *square_type = pg_prove_identity_type(typing,
		pg_prove_projection(typing, source, relation), r_value, r_value);
	assert(square_type);
	for (unsigned side = 0; side < 2; ++side) {
		const struct pg_evidence *endpoint = pg_identity_face_endpoint(typing, classifiers,
			source, square_type, 1, side ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT);
		const struct pg_evidence *expected = pg_prove_reflexivity(typing,
			pg_prove_universe(typing, classifiers, source, 0),
			pg_prove_variable(typing, source, side ? b : a));
		assert(action_result(typing, classifiers, source, &work, endpoint, expected));
	}
	assert(!pg_identity_face_endpoint(typing, classifiers, empty, square_type, 0, PG_IDENTITY_LEFT));
	assert(!pg_identity_face_endpoint(typing, classifiers, source, square_type, 0, (enum pg_identity_direction)2));
	assert(!pg_identity_face_endpoint(typing, classifiers, source, square_type, SIZE_MAX, PG_IDENTITY_LEFT));
	struct pg_identity_endpoint_work *unfinished = pg_identity_endpoint_init(typing, classifiers,
		source, square_type, 1, PG_IDENTITY_LEFT);
	assert(unfinished && pg_identity_endpoint_advance(unfinished, 1) == 0);
	assert(!pg_identity_endpoint_result(unfinished));
	pg_identity_endpoint_destroy(unfinished);
	unfinished = pg_identity_endpoint_init(typing, classifiers, source, square_type, SIZE_MAX, PG_IDENTITY_LEFT);
	assert(unfinished && pg_identity_endpoint_advance(unfinished, 3) == 0);
	assert(pg_identity_endpoint_advance(unfinished, 100) == -1);
	assert(!pg_identity_endpoint_result(unfinished));
	assert(pg_identity_endpoint_advance(unfinished, 0) == -1);
	pg_identity_endpoint_destroy(unfinished);
	assert(pg_identity_endpoint_advance(NULL, 0) == -1);
	pg_identity_endpoint_destroy(NULL);
	const struct pg_binding_cube *square = pg_binding_cube(&dimensions, 2);
	struct pg_coordinate coordinates[2] = {{PG_ENDPOINT_ZERO, 0}, {PG_AXIS, 0}};
	const struct pg_binding_face *centers[4];
	centers[0] = pg_binding_face(&dimensions, square, pg_dimension_map(&dimensions, 1, 2, coordinates));
	coordinates[0].kind = PG_ENDPOINT_ONE;
	centers[1] = pg_binding_face(&dimensions, square, pg_dimension_map(&dimensions, 1, 2, coordinates));
	centers[2] = pg_binding_face(&dimensions, square, pg_dimension_identity(&dimensions, 2));
	centers[3] = pg_binding_face(&dimensions, pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	for (unsigned side = 0; side < 2; ++side) {
		const struct pg_evidence *input_type = pg_prove_value_type(typing, pg_prove_variable(typing, source, side ? b : a));
		const struct pg_evidence *input_context = pg_prove_context_extension(typing, source, x, input_type);
		const struct pg_evidence *homogeneous = pg_prove_projection(typing, input_context, relation);
		const struct pg_evidence *homogeneous_boundary = pg_identity_formation(typing, classifiers, homogeneous);
		assert(homogeneous_boundary && pg_evidence_rule(homogeneous_boundary) == PG_IDENTITY_FORM);
		struct pg_identity_boundary view;
		assert(pg_identity_boundary_view(homogeneous_boundary, &view));
		assert(!view.path_count && !view.paths && !view.left_substitution && !view.right_substitution);
		assert(pg_evidence_judgement(view.family) == PG_JUDGEMENT_VALUE_TYPE);
		assert(pg_alpha_equal(pg_evidence_subject(homogeneous_boundary)->core, pg_evidence_subject(homogeneous)->core) == 1);
		assert(!pg_identity_formation(typing, classifiers, input_type));
		assert(pg_identity_formation(typing, classifiers,
			pg_prove_value_type(typing, pg_prove_type_value(typing, homogeneous))) == homogeneous_boundary);
		const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers,
			pg_prove_variable(typing, input_context, r), pg_prove_variable(typing, input_context, x),
			(enum pg_identity_direction)side);
		const struct pg_evidence *left, *right, *paths[4];
		const struct pg_evidence *boundary = pg_identity_context(typing, &dimensions, input_context, 4,
			centers, &left, &right, paths);
		assert(boundary && transport);
		const struct pg_evidence *acted = pg_prove_family_action(typing,
			pg_prove_classifier(typing, classifiers, input_context, transport), transport, left, right, 4, paths);
		assert(acted);
		/* The destination edge relates the transported corners. The square
		 * center is an assumption; this does not manufacture a square filler. */
		size_t destination = side ? 0 : 1;
		const struct pg_evidence *edge = convert_to(typing, &work, paths[destination],
			pg_prove_identity_type(typing, pg_prove_universe(typing, classifiers, boundary, 0),
				pg_evidence_premise(left, destination + 2), pg_evidence_premise(right, destination + 2)));
		const struct pg_evidence *expected = pg_prove_identity_instance(typing, classifiers, edge,
			pg_prove_reindex(typing, left, transport), pg_prove_reindex(typing, right, transport));
		assert(expected);
		assert(pg_identity_formation(typing, classifiers, expected) == expected);
		assert(pg_identity_face_endpoint(typing, classifiers, boundary, expected, 0, PG_IDENTITY_LEFT)
			== pg_evidence_premise(expected, 1));
		assert(!pg_identity_face_endpoint(typing, classifiers, boundary, expected, 1, PG_IDENTITY_LEFT));
		assert(pg_identity_formation(typing, classifiers,
			pg_prove_value_type(typing, pg_prove_type_value(typing, expected))) == expected);
		const struct pg_evidence *extra_context = pg_prove_context_extension(typing, boundary, pg_binder(graph),
			pg_prove_universe(typing, classifiers, boundary, 0));
		const struct pg_evidence *extra_instance = pg_prove_projection(typing, extra_context, expected);
		const struct pg_evidence *instance_boundary = pg_identity_formation(typing, classifiers, extra_instance);
		assert(instance_boundary && pg_evidence_rule(instance_boundary) == PG_IDENTITY_INSTANCE);
		assert(pg_identity_boundary_view(instance_boundary, &view));
		assert(!view.path_count && !view.paths && !view.left_substitution && !view.right_substitution);
		assert(pg_evidence_judgement(view.family) == PG_JUDGEMENT_VALUE);
		assert(pg_alpha_equal(pg_evidence_subject(instance_boundary)->core, pg_evidence_subject(extra_instance)->core) == 1);
		const struct pg_evidence *checked = convert_to(typing, &work, acted, expected);
		assert(checked && pg_evidence_judgement(checked) == PG_JUDGEMENT_VALUE);
		assert(pg_evidence_subject(checked)->core == pg_evidence_subject(acted)->core);
		assert(!pg_prove_identity_instance(typing, classifiers, edge,
			pg_prove_reindex(typing, right, transport), pg_prove_reindex(typing, left, transport)));
		const struct pg_evidence *lift = pg_prove_identity_lift(typing, classifiers,
			pg_prove_variable(typing, input_context, r), pg_prove_variable(typing, input_context, x),
			(enum pg_identity_direction)side);
		assert(lift);
		const struct pg_evidence *fields[] = {transport, lift};
		for (size_t dimension = 1; dimension <= 2; ++dimension) {
			const struct pg_binding_cube *cubes[4];
			for (size_t i = 0; i < 4; ++i) cubes[i] = pg_binding_cube(&dimensions, dimension);
			for (size_t swap = 0; swap < dimension; ++swap) {
				struct pg_coordinate axes[2] = {{PG_AXIS, swap}, {PG_AXIS, 1 - swap}};
				const struct pg_dimension_map *order = pg_dimension_map(&dimensions, dimension, dimension, axes);
				const struct pg_evidence *cube_context = pg_identity_cube_context(typing, &dimensions,
					input_context, 4, cubes, order);
				assert(cube_context);
				for (size_t field = 0; field < 2; ++field) {
					const struct pg_evidence *higher = pg_identity_cube_action(typing, classifiers, &dimensions,
						input_context, fields[field], 4, cubes, order);
					assert(higher && pg_evidence_context(higher) == pg_evidence_context(cube_context));
					assert(pg_evidence_judgement(higher) == PG_JUDGEMENT_VALUE);
					assert(pg_prove_classifier(typing, classifiers, cube_context, higher));
					struct pg_whnf_job *normal = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(higher)->core);
					assert(pg_whnf_advance(normal, 100000) == PG_EVAL_WHNF);
					assert(pg_prove_normalization(typing, higher, pg_whnf_certificate(normal)));
					assert(pg_identity_cube_action(typing, classifiers, &dimensions,
						input_context, fields[field], 4, cubes, order) == higher);
				}
			}
		}
	}
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void reflexive_instance_boundary(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 1);
	const struct pg_evidence *point = pg_prove_type_value(typing,
		pg_prove_universe(typing, classifiers, empty, 0));
	const struct pg_evidence *line = pg_prove_identity_type(typing, universe, point, point);
	const struct pg_evidence *path = pg_prove_reflexivity(typing, universe, point);
	const struct pg_evidence *line_value = pg_prove_type_value(typing, line);
	const struct pg_evidence *family = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, empty, line_value), line_value);
	const struct pg_evidence *instance = pg_prove_identity_instance(typing, classifiers, family, path, path);
	assert(instance);
	const struct pg_evidence *recovered = pg_identity_formation(typing, classifiers, instance);
	assert(recovered && pg_evidence_rule(recovered) == PG_IDENTITY_FORM);
	assert(pg_evidence_subject(recovered)->core == pg_evidence_subject(instance)->core);
	assert(pg_identity_face_endpoint(typing, classifiers, empty, instance, 0, PG_IDENTITY_LEFT) == path);
	for (enum pg_identity_direction side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *endpoint = pg_identity_face_endpoint(typing, classifiers, empty, instance, 1, side);
		assert(endpoint && pg_evidence_subject(endpoint)->core == pg_evidence_subject(path)->core);
	}
	assert(!pg_identity_face_endpoint(typing, classifiers, empty, instance, 2, PG_IDENTITY_LEFT));
	const struct pg_evidence *wrapped = instance;
	for (size_t i = 0; i < 64; ++i)
		wrapped = pg_prove_value_type(typing, pg_prove_type_value(typing, wrapped));
	assert(!pg_identity_formation_init(typing, classifiers, path));
	for (uint64_t split = 0; split <= 129; ++split) {
		struct pg_identity_formation_work *pending = pg_identity_formation_init(typing, classifiers, wrapped);
		assert(pending && !pg_identity_formation_result(pending));
		assert(pg_identity_formation_advance(pending, split) == (split == 129));
		assert(pg_identity_formation_result(pending) == (split == 129 ? recovered : NULL));
		assert(pg_identity_formation_advance(pending, 129 - split) == 1);
		assert(pg_identity_formation_result(pending) == recovered);
		pg_identity_formation_destroy(pending);
	}
	struct pg_identity_formation_work *unsupported = pg_identity_formation_init(typing, classifiers, universe);
	assert(unsupported && pg_identity_formation_advance(unsupported, 0) == 0);
	assert(pg_identity_formation_advance(unsupported, 1) == -1);
	assert(pg_identity_formation_advance(unsupported, 0) == -1);
	assert(!pg_identity_formation_result(unsupported));
	pg_identity_formation_destroy(unsupported);
	for (uint64_t split = 0; split <= 128; ++split) {
		struct pg_identity_endpoint_work *pending = pg_identity_endpoint_init(typing, classifiers,
			empty, wrapped, 0, PG_IDENTITY_LEFT);
		assert(pending && pg_identity_endpoint_advance(pending, split) == 0);
		assert(!pg_identity_endpoint_result(pending));
		assert(pg_identity_endpoint_advance(pending, 128 - split) == 0);
		assert(pg_identity_endpoint_advance(pending, 1) == 1);
		assert(pg_identity_endpoint_result(pending) == path);
		pg_identity_endpoint_destroy(pending);
	}
	struct pg_coordinate coordinate = {PG_ENDPOINT_ZERO, 0};
	struct pg_dimension_map face = {0, 1, &coordinate};
	assert(!pg_identity_face_init(typing, classifiers, empty, path, &face));
	for (uint64_t split = 0; split <= 131; ++split) {
		struct pg_identity_face_work *pending = pg_identity_face_init(typing, classifiers, empty, wrapped, &face);
		assert(pending && pg_identity_face_advance(pending, split) == (split == 131));
		assert(pg_identity_face_result(pending) == (split == 131 ? path : NULL));
		assert(pg_identity_face_advance(pending, 131 - split) == 1);
		assert(pg_identity_face_result(pending) == path);
		pg_identity_face_destroy(pending);
	}
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	const struct pg_evidence *identity_map = pg_prove_substitution(typing, empty, empty, 0, NULL);
	const struct pg_evidence *acted_family = pg_prove_family_action(typing,
		pg_prove_classifier(typing, classifiers, empty, line_value), line_value,
		identity_map, identity_map, 0, NULL);
	const struct pg_evidence *family_type = pg_prove_identity_type(typing,
		pg_prove_classifier(typing, classifiers, empty, line_value), line_value, line_value);
	const struct pg_evidence *converted_family = family;
	for (size_t i = 0; i < 64; ++i) converted_family = convert_to(typing, &work, converted_family, family_type);
	const struct pg_evidence *converted_instance = pg_prove_identity_instance(typing, classifiers,
		converted_family, path, path);
	assert(converted_instance);
	for (uint64_t split = 0; split <= 64; ++split) {
		struct pg_identity_endpoint_work *pending = pg_identity_endpoint_init(typing, classifiers,
			empty, converted_instance, 0, PG_IDENTITY_LEFT);
		assert(pending && pg_identity_endpoint_advance(pending, split) == 0);
		assert(!pg_identity_endpoint_result(pending));
		assert(pg_identity_endpoint_advance(pending, 64 - split) == 0);
		assert(pg_identity_endpoint_advance(pending, 1) == 1);
		assert(pg_identity_endpoint_result(pending) == path);
		pg_identity_endpoint_destroy(pending);
	}
	acted_family = convert_to(typing, &work, acted_family, family_type);
	const struct pg_evidence *acted_instance = pg_prove_identity_instance(typing, classifiers, acted_family, path, path);
	assert(acted_instance);
	const struct pg_evidence *acted_formation = pg_identity_formation(typing, classifiers, acted_instance);
	assert(acted_formation && pg_evidence_rule(acted_formation) == PG_FAMILY_IDENTITY_FORM);
	assert(pg_evidence_subject(acted_formation)->core == pg_evidence_subject(acted_instance)->core);
	assert(pg_identity_face_endpoint(typing, classifiers, empty, acted_instance, 1, PG_IDENTITY_LEFT));
	pg_whnf_work_destroy(&work);
	const struct pg_evidence *extended = pg_prove_context_extension(typing, empty,
		pg_binder(typing->graph), universe);
	const struct pg_evidence *projected_path = pg_prove_projection(typing, extended, path);
	const struct pg_evidence *projection = pg_prove_substitution(typing, empty, extended, 0, NULL);
	const struct pg_evidence *families[] = {
		pg_prove_projection(typing, extended, family),
		pg_prove_reindex(typing, projection, family),
		pg_prove_projection(typing, extended, acted_family),
		pg_prove_reindex(typing, projection, acted_family)};
	for (size_t i = 0; i < 4; ++i) {
		const struct pg_evidence *moved = pg_prove_identity_instance(typing, classifiers,
			families[i], projected_path, projected_path);
		const struct pg_evidence *restored = pg_identity_formation(typing, classifiers, moved);
		assert(restored && pg_evidence_context(restored) == pg_evidence_context(extended));
		assert(pg_evidence_rule(restored) == (i < 2 ? PG_IDENTITY_FORM : PG_FAMILY_IDENTITY_FORM));
		assert(pg_evidence_subject(restored)->core == pg_evidence_subject(moved)->core);
		assert(pg_identity_face_endpoint(typing, classifiers, extended, moved, 1, PG_IDENTITY_LEFT));
	}
}

static void dependent_instance_boundary(struct pg_typing *typing, struct pg_classifiers *classifiers, int value_dependency)
{
	struct pg_dimensions dimensions;
	struct pg_whnf_work work;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, classifiers, empty, 1));
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, source, 1);
	const struct pg_evidence *point = pg_prove_variable(typing, source, a);
	if (value_dependency) {
		const struct pg_object *x = pg_binder(typing->graph);
		const struct pg_evidence *type = pg_prove_value_type(typing, point);
		source = pg_prove_context_extension(typing, source, x, type);
		universe = pg_prove_projection(typing, source, type);
		point = pg_prove_variable(typing, source, x);
	}
	const struct pg_evidence *line = pg_prove_identity_type(typing, universe, point, point);
	const struct pg_evidence *line_value = pg_prove_type_value(typing, line);
	const struct pg_evidence *path = pg_prove_reflexivity(typing, universe, point);
	size_t count = value_dependency ? 2 : 1;
	const struct pg_binding_face *centers[2];
	for (size_t i = 0; i < count; ++i) centers[i] = pg_binding_face(&dimensions,
		pg_binding_cube(&dimensions, 1), pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *left, *right, *paths[2];
	const struct pg_evidence *context = pg_identity_context(typing, &dimensions, source, count,
		centers, &left, &right, paths);
	assert(context);
	const struct pg_evidence *family = pg_prove_family_action(typing,
		pg_prove_classifier(typing, classifiers, source, line_value), line_value, left, right, count, paths);
	const struct pg_evidence *family_type = pg_prove_identity_type(typing,
		pg_prove_reindex(typing, left, pg_prove_classifier(typing, classifiers, source, line_value)),
		pg_prove_reindex(typing, left, line_value), pg_prove_reindex(typing, right, line_value));
	family = convert_to(typing, &work, family, family_type);
	const struct pg_evidence *instance = pg_prove_identity_instance(typing, classifiers, family,
		pg_prove_reindex(typing, left, path), pg_prove_reindex(typing, right, path));
	assert(instance);
	const struct pg_evidence *formation = pg_identity_formation(typing, classifiers, instance);
	struct pg_identity_boundary boundary;
	assert(pg_identity_boundary_view(formation, &boundary));
	assert(pg_evidence_rule(formation) == PG_FAMILY_IDENTITY_FORM);
	assert(boundary.path_count == count);
	for (size_t i = 0; i < count; ++i) assert(boundary.paths[i] == paths[i]);
	assert(boundary.left_substitution == left && boundary.right_substitution == right);
	assert(pg_alpha_equal(pg_evidence_classifier(boundary.left), pg_evidence_classifier(boundary.right)) == 0);
	assert(pg_evidence_subject(formation)->core == pg_evidence_subject(instance)->core);
	for (enum pg_identity_direction side = PG_IDENTITY_RIGHT; side <= PG_IDENTITY_LEFT; ++side) {
		const struct pg_evidence *endpoint = pg_identity_face_endpoint(typing, classifiers, context, instance, 1, side);
		assert(endpoint && pg_evidence_context(endpoint) == pg_evidence_context(context));
	}
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void generated_contexts(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	reflexive_instance_boundary(typing, classifiers);
	dependent_instance_boundary(typing, classifiers, 0);
	dependent_instance_boundary(typing, classifiers, 1);
	square_transposition_boundary(typing, classifiers);
	uniform_transport(typing, classifiers);
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, pg_binder(typing->graph), universe);
	const struct pg_evidence *initial = source;
	uint64_t comparison_max = 0;
	struct pg_whnf_work cube_work;
	assert(pg_whnf_work_init(&cube_work, typing->graph) == 0);
	const struct pg_evidence *input = pg_prove_variable(typing, initial, pg_evidence_context(initial)->binder);
	const struct pg_evidence *domain = pg_prove_value_type(typing, input);
	const struct pg_object *argument = pg_binder(typing->graph);
	const struct pg_evidence *body_context = pg_prove_context_extension(typing, initial, argument, domain);
	const struct pg_evidence *body_variable = pg_prove_variable(typing, body_context, argument);
	/* A vertex can already denote a path. Geometric binder dimension is not
	 * the number of instantiated Identity directions in its classifier. */
	const struct pg_evidence *path_type = pg_prove_identity_type(typing,
		pg_prove_projection(typing, body_context, domain), body_variable, body_variable);
	const struct pg_evidence *path_source = pg_prove_context_extension(typing, body_context,
		pg_binder(typing->graph), path_type);
	for (size_t d = 0; d < 2; ++d) {
		const struct pg_binding_cube *path_cube = pg_binding_cube(&dimensions, d);
		const struct pg_dimension_map *order = pg_dimension_identity(&dimensions, d);
		const struct pg_binding_face *path_center = pg_binding_face(&dimensions, path_cube, order);
		const struct pg_evidence *path_context = pg_identity_cube_context(typing, &dimensions, path_source, 1, &path_cube, order);
		assert(path_context);
		const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, path_context,
			pg_prove_variable(typing, path_context, &path_center->variable));
		const struct pg_evidence *recovered = pg_identity_formation(typing, classifiers, formation);
		struct pg_identity_boundary boundary;
		assert(recovered && pg_identity_boundary_view(recovered, &boundary));
		assert(path_center->face->source == d);
		if (d == 0) {
			assert(pg_evidence_rule(recovered) == PG_IDENTITY_FORM);
			assert(pg_evidence_subject(boundary.left)->core == pg_evidence_subject(body_variable)->core);
		} else {
			assert(pg_evidence_rule(recovered) == PG_FAMILY_IDENTITY_FORM);
			assert(pg_identity_formation(typing, classifiers, boundary.family));
			struct pg_coordinate zero = {PG_ENDPOINT_ZERO, 0};
			const struct pg_evidence *selected = pg_identity_proper_face(typing, classifiers, path_context,
				formation, pg_dimension_map(&dimensions, 0, 1, &zero));
			assert(selected == boundary.left);
			assert(pg_identity_formation(typing, classifiers,
				pg_prove_classifier(typing, classifiers, path_context, selected)));
		}
	}
	const struct pg_evidence *functions[2];
	for (size_t dependent = 0; dependent < 2; ++dependent) {
		const struct pg_evidence *value = dependent ? pg_prove_reflexivity(typing,
			pg_prove_projection(typing, body_context, domain), body_variable) : body_variable;
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, value);
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, body_context,
			pg_prove_classifier(typing, classifiers, body_context, body));
		functions[dependent] = pg_prove_lambda(typing, pi, body);
		assert(functions[dependent]);
	}
	static const size_t permutations[6][3] = {
		{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
	};
	const struct pg_evidence *source_call = pg_prove_application(typing,
		pg_prove_projection(typing, body_context, functions[0]), body_variable);
	assert(source_call);
	const struct pg_evidence *function_type = pg_prove_thunk_type(typing, classifiers,
		pg_prove_classifier(typing, classifiers, initial, functions[0]));
	const struct pg_object *function_name = pg_binder(typing->graph);
	const struct pg_evidence *neutral_source = pg_prove_context_extension(typing, body_context, function_name,
		pg_prove_projection(typing, body_context, function_type));
	const struct pg_evidence *neutral_call = pg_prove_application(typing,
		pg_prove_force(typing, pg_prove_variable(typing, neutral_source, function_name)),
		pg_prove_projection(typing, neutral_source, body_variable));
	assert(neutral_call);
	for (size_t dimension = 0, expected_count = 1; dimension <= 3; ++dimension, expected_count *= 3) {
		const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, dimension);
		for (size_t p = 0; p < (dimension == 3 ? 6u : 1u); ++p) {
			struct pg_coordinate coordinates[3];
			for (size_t i = 0; i < dimension; ++i) coordinates[i] = (struct pg_coordinate){PG_AXIS, permutations[p][i]};
			const struct pg_dimension_map *order = pg_dimension_map(&dimensions, dimension, dimension, coordinates);
			const struct pg_evidence *boundary = pg_identity_cube_context(typing, &dimensions, initial, 1, &cube, order);
			assert(boundary);
			size_t count = 0;
			for (const struct pg_context *c = pg_evidence_context(boundary); c; c = c->parent) {
				++count;
				assert(pg_prove_classifier(typing, classifiers, boundary, pg_prove_variable(typing, boundary, c->binder)));
			}
			assert(count == expected_count);
			const struct pg_binding_face *center = pg_binding_face(&dimensions, cube, order);
			assert(pg_evidence_context(boundary)->binder == &center->variable);
			size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
			assert(pg_identity_cube_context(typing, &dimensions, initial, 1, &cube, order) == boundary);
			assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
			const struct pg_evidence *action = pg_identity_cube_action(typing, classifiers, &dimensions, initial, input, 1, &cube, order);
			assert(action && pg_evidence_context(action) == pg_evidence_context(boundary));
			const struct pg_evidence *variable = pg_prove_variable(typing, boundary, &center->variable);
			converts(&cube_work, pg_evidence_classifier(action), pg_evidence_classifier(variable));
			converts(&cube_work, pg_evidence_subject(action)->core, pg_evidence_subject(variable)->core);
			assert(pg_identity_cube_action(typing, classifiers, &dimensions, initial, input, 1, &cube, order) == action);
			const struct pg_evidence *returned = pg_identity_cube_action(typing, classifiers, &dimensions, initial,
				pg_prove_return(typing, classifiers, input), 1, &cube, order);
			const struct pg_evidence *expected_return = pg_prove_return(typing, classifiers, variable);
			assert(returned && pg_evidence_judgement(returned) == PG_JUDGEMENT_COMPUTATION);
			converts(&cube_work, pg_evidence_classifier(returned), pg_evidence_classifier(expected_return));
			converts(&cube_work, pg_evidence_subject(returned)->core, pg_evidence_subject(expected_return)->core);
			for (size_t dependent = 0; dependent < 2; ++dependent) {
				const struct pg_evidence *function = pg_identity_cube_action(typing, classifiers, &dimensions,
					initial, functions[dependent], 1, &cube, order);
				assert(function && pg_evidence_judgement(function) == PG_JUDGEMENT_COMPUTATION);
				assert(pg_prove_classifier(typing, classifiers, boundary, function));
				const struct pg_evidence *thunk = pg_identity_cube_action(typing, classifiers, &dimensions, initial,
					pg_prove_thunk(typing, classifiers, functions[dependent]), 1, &cube, order);
				const struct pg_evidence *expected_thunk = pg_prove_thunk(typing, classifiers, function);
				assert(thunk && expected_thunk && pg_evidence_judgement(thunk) == PG_JUDGEMENT_VALUE);
				uint64_t steps = converts_budget(&cube_work, pg_evidence_classifier(thunk), pg_evidence_classifier(expected_thunk), 1000000);
				if (steps > comparison_max) comparison_max = steps;
				steps = converts_budget(&cube_work, pg_evidence_subject(thunk)->core, pg_evidence_subject(expected_thunk)->core, 1000000);
				if (steps > comparison_max) comparison_max = steps;
			}
			const struct pg_binding_cube *cubes[] = {cube, pg_binding_cube(&dimensions, dimension)};
			const struct pg_evidence *joint = pg_identity_cube_context(typing, &dimensions, body_context, 2, cubes, order);
			assert(joint);
			const struct pg_context *prefix = pg_evidence_context(joint);
			for (size_t i = 0; i < expected_count; ++i) prefix = prefix->parent;
			assert(prefix == pg_evidence_context(boundary));
			const struct pg_binding_face *value_center = pg_binding_face(&dimensions, cubes[1], order);
			const struct pg_evidence *joint_variable = pg_prove_variable(typing, joint, &value_center->variable);
			const struct pg_evidence *joint_action = pg_identity_cube_action(typing, classifiers, &dimensions,
				body_context, body_variable, 2, cubes, order);
			assert(joint_action && pg_evidence_context(joint_action) == pg_evidence_context(joint));
			converts(&cube_work, pg_evidence_classifier(joint_action), pg_evidence_classifier(joint_variable));
			converts(&cube_work, pg_evidence_subject(joint_action)->core, pg_evidence_subject(joint_variable)->core);
			assert(pg_term_independent(pg_evidence_classifier(joint_variable), &center->variable) == 0);
			assert(pg_identity_cube_context(typing, &dimensions, body_context, 2, cubes, order) == joint);
			const struct pg_evidence *call_action = pg_identity_cube_action(typing, classifiers, &dimensions,
				body_context, source_call, 2, cubes, order);
			const struct pg_evidence *call_expected = pg_prove_return(typing, classifiers, joint_variable);
			assert(call_action && pg_evidence_judgement(call_action) == PG_JUDGEMENT_COMPUTATION);
			assert(action_result(typing, classifiers, joint, &cube_work, call_action, call_expected));
			const struct pg_binding_cube *all_cubes[] = {cube, cubes[1], pg_binding_cube(&dimensions, dimension)};
			const struct pg_evidence *all = pg_identity_cube_context(typing, &dimensions, neutral_source, 3, all_cubes, order);
			assert(all);
			for (const struct pg_context *c = pg_evidence_context(all); c; c = c->parent) {
				const struct pg_binding_face *face = pg_binding_face_view(c->binder);
				assert(face);
				const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, all,
					pg_prove_variable(typing, all, c->binder));
				const struct pg_evidence *recovered = pg_identity_formation(typing, classifiers, formation);
				if (!face->face->source) { assert(!recovered); continue; }
				assert(recovered && pg_evidence_context(recovered) == pg_evidence_context(all));
				assert(pg_evidence_rule(recovered) == PG_FAMILY_IDENTITY_FORM);
				struct pg_identity_boundary boundary_view;
				assert(pg_identity_boundary_view(recovered, &boundary_view));
				assert(pg_evidence_classifier(recovered) == pg_evidence_classifier(formation));
				assert(pg_alpha_equal(pg_evidence_subject(recovered)->core, pg_evidence_subject(formation)->core) == 1);
				size_t d = face->face->source;
				if (d == dimension) {
					/* Two orders of fixing coordinates must agree as typed terms,
					 * not merely as maps between geometric binder labels. */
					for (size_t a = 0; a < d; ++a) {
						for (size_t b = a + 1; b < d; ++b) {
							for (unsigned choices = 0; choices < 4; ++choices) {
								enum pg_identity_direction sa = choices & 1 ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
								enum pg_identity_direction sb = choices & 2 ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT;
								const struct pg_evidence *first_a = pg_identity_face_endpoint(typing, classifiers,
									all, formation, d - 1 - a, sa);
								const struct pg_evidence *first_b = pg_identity_face_endpoint(typing, classifiers,
									all, formation, d - 1 - b, sb);
								assert(first_a && first_b);
								const struct pg_evidence *ab = pg_identity_face_endpoint(typing, classifiers, all,
									pg_prove_classifier(typing, classifiers, all, first_a), d - 1 - b, sb);
								const struct pg_evidence *ba = pg_identity_face_endpoint(typing, classifiers, all,
									pg_prove_classifier(typing, classifiers, all, first_b), d - 2 - a, sa);
								assert(action_result(typing, classifiers, all, &cube_work, ab, ba));
							}
						}
					}
					size_t cases = 1;
					for (size_t i = 0; i < d; ++i) cases *= 3;
					for (size_t code = 0; code + 1 < cases; ++code) {
						struct pg_coordinate coordinates[3];
						size_t digits = code, axes = 0;
						for (size_t i = 0; i < d; ++i, digits /= 3)
							coordinates[i] = digits % 3 == 2 ? (struct pg_coordinate){PG_AXIS, axes++}
								: (struct pg_coordinate){digits % 3 ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
						const struct pg_dimension_map *selection = pg_dimension_map(&dimensions, axes, d, coordinates);
						const struct pg_binding_face *selected = pg_binding_restrict(&dimensions, face, selection);
						const struct pg_evidence *value = pg_identity_proper_face(typing, classifiers, all, formation, selection);
						assert(value && selected);
						assert(action_result(typing, classifiers, all, &cube_work, value,
							pg_prove_variable(typing, all, &selected->variable)));
					}
					assert(!pg_identity_proper_face(typing, classifiers, all, formation,
						pg_dimension_identity(&dimensions, d)));
					if (d == 3) {
						struct pg_coordinate permuted[] = {{PG_AXIS, 1}, {PG_AXIS, 0}, {PG_ENDPOINT_ZERO, 0}};
						assert(!pg_identity_proper_face(typing, classifiers, all, formation,
							pg_dimension_map(&dimensions, 2, 3, permuted)));
					}
					struct pg_coordinate excessive[4] = {{PG_ENDPOINT_ZERO, 0}};
					assert(!pg_identity_proper_face(typing, classifiers, all, formation,
						pg_dimension_map(&dimensions, 0, d + 1, excessive)));
				}
				for (size_t depth = 0; depth < d; ++depth) {
					for (unsigned side = 0; side < 2; ++side) {
						struct pg_coordinate coordinates[3];
						size_t axis = 0;
						for (size_t i = 0; i < d; ++i)
							coordinates[i] = i == d - 1 - depth
								? (struct pg_coordinate){side ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0}
								: (struct pg_coordinate){PG_AXIS, axis++};
						const struct pg_binding_face *selected = pg_binding_restrict(&dimensions, face,
							pg_dimension_map(&dimensions, d - 1, d, coordinates));
						const struct pg_evidence *endpoint = pg_identity_face_endpoint(typing, classifiers, all,
							formation, depth, side ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT);
						assert(endpoint && selected);
						if (d == dimension) {
							struct pg_identity_endpoint_work *measured = pg_identity_endpoint_init(typing,
								classifiers, all, formation, depth, side ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT);
							assert(measured);
							uint64_t steps = 0;
							int measured_status;
							do {
								assert(++steps < 1000);
								measured_status = pg_identity_endpoint_advance(measured, 1);
							} while (!measured_status);
							assert(measured_status == 1 && pg_identity_endpoint_result(measured) == endpoint);
							pg_identity_endpoint_destroy(measured);
							for (uint64_t cut = 0; cut <= steps; ++cut) {
								struct pg_identity_endpoint_work *pending = pg_identity_endpoint_init(typing,
									classifiers, all, formation, depth, side ? PG_IDENTITY_RIGHT : PG_IDENTITY_LEFT);
								assert(pending && !pg_identity_endpoint_result(pending));
								int status = pg_identity_endpoint_advance(pending, cut);
								assert(status == (cut == steps));
								assert(pg_identity_endpoint_result(pending) == (status ? endpoint : NULL));
								uint64_t consumed = cut;
								while (!status) { status = pg_identity_endpoint_advance(pending, 1); ++consumed; }
								assert(status == 1 && consumed == steps);
								assert(pg_identity_endpoint_result(pending) == endpoint);
								assert(pg_identity_endpoint_advance(pending, 0) == 1);
								pg_identity_endpoint_destroy(pending);
							}
						}
						assert(action_result(typing, classifiers, all, &cube_work, endpoint,
							pg_prove_variable(typing, all, &selected->variable)));
					}
				}
				assert(!pg_identity_face_endpoint(typing, classifiers, all, formation, d, PG_IDENTITY_LEFT));
				struct pg_coordinate endpoint_coordinates[3];
				for (size_t i = 0; i + 1 < d; ++i) endpoint_coordinates[i] = (struct pg_coordinate){PG_AXIS, i};
				for (size_t side = 0; side < 2; ++side) {
					endpoint_coordinates[d - 1] = (struct pg_coordinate){side ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
					const struct pg_binding_face *endpoint = pg_binding_restrict(&dimensions, face,
						pg_dimension_map(&dimensions, d - 1, d, endpoint_coordinates));
					const struct pg_evidence *value = side ? boundary_view.right : boundary_view.left;
					assert(endpoint && pg_evidence_subject(value)->core == pg_reference(typing->graph, &endpoint->variable));
				}
			}
			const struct pg_binding_face *fcenter = pg_binding_face(&dimensions, all_cubes[2], order);
			const struct pg_evidence *applied = expose_classifier(typing, classifiers, &cube_work, all,
				pg_prove_variable(typing, all, &fcenter->variable));
			applied = pg_prove_force(typing, applied);
			assert(applied);
			const struct pg_context *arguments[27], *cursor = pg_evidence_context(joint);
			for (size_t i = expected_count; i; --i) { arguments[i - 1] = cursor; cursor = cursor->parent; }
			for (size_t i = 0; i < expected_count; ++i) {
				applied = expose_classifier(typing, classifiers, &cube_work, all, applied);
				const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, all, applied);
				const struct pg_evidence *arg = convert_to(typing, &cube_work,
					pg_prove_variable(typing, all, arguments[i]->binder), pg_prove_pi_domain(typing, pi));
				applied = pg_prove_application(typing, applied, arg);
				assert(applied);
			}
			const struct pg_evidence *neutral_action = pg_identity_cube_action(typing, classifiers, &dimensions,
				neutral_source, neutral_call, 3, all_cubes, order);
			assert(neutral_action);
			converts_budget(&cube_work, pg_evidence_classifier(neutral_action), pg_evidence_classifier(applied), 1000000);
			converts_budget(&cube_work, pg_evidence_subject(neutral_action)->core, pg_evidence_subject(applied)->core, 1000000);
			cubes[1] = cube;
			assert(!pg_identity_cube_context(typing, &dimensions, body_context, 2, cubes, order));
		}
	}
	pg_whnf_work_destroy(&cube_work);
	printf("cube function action: maximum comparison steps %llu (limit 1000000)\n", (unsigned long long)comparison_max);
	const struct pg_dimension_map *order = pg_dimension_identity(&dimensions, 1);
	const struct pg_binding_cube *single = pg_binding_cube(&dimensions, 1);
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 0, &single, order));
	assert(!pg_identity_cube_action(typing, classifiers, &dimensions, initial, NULL, 1, &single, order));
	assert(!pg_identity_cube_action(typing, classifiers, &dimensions, initial, empty, 1, &single, order));
	assert(!pg_identity_cube_context(typing, &dimensions, empty, 1, &single, order));
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, NULL, order));
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, &single, NULL));
	single = pg_binding_cube(&dimensions, 2);
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, &single, order));
	struct pg_coordinate constant = {PG_ENDPOINT_ZERO, 0};
	single = pg_binding_cube(&dimensions, 1);
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, &single,
		pg_dimension_map(&dimensions, 1, 1, &constant)));
	const struct pg_binding_cube excessive = {SIZE_MAX};
	single = &excessive;
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, &single, order));
	const struct pg_binding_cube large = {32};
	size_t proofs_before = typing->proofs.count, faces_before = dimensions.binding_faces.count;
	single = &large;
	assert(!pg_identity_cube_context(typing, &dimensions, initial, 1, &single, order));
	assert(typing->proofs.count == proofs_before && dimensions.binding_faces.count == faces_before);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	const struct pg_binding_face *centers[9];
	const struct pg_evidence *paths[9], *repeated_paths[9], *left, *right, *rl, *rr;
	size_t count = 1;
	for (size_t dimension = 1; dimension <= 3; ++dimension, count *= 3) {
		const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, dimension);
		for (size_t i = 0; i < count; ++i) {
			struct pg_coordinate coordinates[3];
			size_t axes = 0, divisor = count / 3;
			for (size_t j = 0; j + 1 < dimension; ++j, divisor /= 3) {
				size_t digit = (i / divisor) % 3;
				coordinates[j] = digit == 2 ? (struct pg_coordinate){PG_AXIS, axes++}
					: (struct pg_coordinate){digit ? PG_ENDPOINT_ONE : PG_ENDPOINT_ZERO, 0};
			}
			coordinates[dimension - 1] = (struct pg_coordinate){PG_AXIS, axes++};
			centers[i] = pg_binding_face(&dimensions, cube, pg_dimension_map(&dimensions, axes, dimension, coordinates));
			assert(centers[i]);
		}
		const struct pg_evidence *target = pg_identity_context(typing, &dimensions, source,
			count, centers, &left, &right, paths);
		assert(target);
		size_t declarations = 0;
		for (const struct pg_context *c = pg_evidence_context(target); c; c = c->parent) ++declarations;
		assert(declarations == 3 * count);
		assert(pg_evidence_premise(left, 0) == source && pg_evidence_premise(right, 0) == source);
		for (size_t i = 0; i < count; ++i) {
			assert(pg_evidence_context(paths[i]) == pg_evidence_context(target));
			assert(pg_evidence_subject(paths[i])->core == pg_reference(typing->graph, &centers[i]->variable));
			const struct pg_evidence *formation = pg_prove_classifier(typing, classifiers, target, paths[i]);
			assert(formation && pg_evidence_subject(formation)->core == pg_evidence_classifier(paths[i]));
		}
		/* Include higher centers, whose types were formed in shorter prefixes. */
		const struct pg_context *declaration = pg_evidence_context(source);
		for (size_t i = count; i; --i, declaration = declaration->parent) {
			const struct pg_evidence *variable = pg_prove_variable(typing, source, declaration->binder);
			const struct pg_evidence *acted = pg_prove_family_action(typing,
				pg_prove_classifier(typing, classifiers, source, variable), variable, left, right, count, paths);
			if (dimension > 1 && i == count)
				assert(pg_alpha_equal(pg_evidence_classifier(acted), pg_evidence_classifier(paths[i - 1])) == 0);
			action_result(typing, classifiers, target, &work, acted, paths[i - 1]);
		}
		size_t terms = typing->graph->terms.count, proofs = typing->proofs.count;
		assert(pg_identity_context(typing, &dimensions, source, count, centers, &rl, &rr, repeated_paths) == target);
		assert(left == rl && right == rr);
		for (size_t i = 0; i < count; ++i) assert(paths[i] == repeated_paths[i]);
		assert(typing->graph->terms.count == terms && typing->proofs.count == proofs);
		source = target;
	}
	assert(pg_identity_context(typing, &dimensions, source, 0, NULL, &left, &right, NULL) == source);
	assert(left == right);
	/* Only the suffix e:Z varies; the existing Z declaration stays shared. */
	const struct pg_evidence *ztype = pg_prove_value_type(typing,
		pg_prove_variable(typing, initial, pg_evidence_context(initial)->binder));
	const struct pg_object *e = pg_binder(typing->graph);
	const struct pg_evidence *dependent = pg_prove_context_extension(typing, initial, e, ztype);
	single = pg_binding_cube(&dimensions, 2);
	const struct pg_evidence *dependent_cube = pg_identity_cube_context(typing, &dimensions, dependent,
		1, &single, pg_dimension_identity(&dimensions, 2));
	assert(dependent_cube);
	const struct pg_context *ambient = pg_evidence_context(dependent_cube);
	for (size_t i = 0; i < 9; ++i) ambient = ambient->parent;
	assert(ambient == pg_evidence_context(initial));
	const struct pg_binding_cube *line = pg_binding_cube(&dimensions, 1);
	centers[0] = pg_binding_face(&dimensions, line, pg_dimension_identity(&dimensions, 1));
	const struct pg_evidence *suffix = pg_identity_context(typing, &dimensions, dependent,
		1, centers, &left, &right, paths);
	assert(suffix && pg_evidence_context(suffix)->parent->parent->parent == pg_evidence_context(initial));
	assert(pg_evidence_subject(pg_evidence_premise(left, 2))->core
		== pg_evidence_subject(pg_evidence_premise(right, 2))->core);
	const struct pg_evidence *ev = pg_prove_variable(typing, dependent, e);
	action_result(typing, classifiers, suffix, &work,
		pg_prove_family_action(typing, pg_prove_projection(typing, dependent, ztype), ev, left, right, 1, paths), paths[0]);
	const struct pg_evidence *saved_left = left, *saved_right = right;
	centers[1] = centers[0];
	assert(!pg_identity_context(typing, &dimensions, dependent, 2, centers, &left, &right, paths));
	assert(!pg_identity_context(typing, &dimensions, initial, 2, centers, &left, &right, paths));
	assert(!pg_identity_context(typing, &dimensions, initial, 1, NULL, &left, &right, paths));
	const struct pg_binding_cube *point = pg_binding_cube(&dimensions, 0);
	centers[0] = pg_binding_face(&dimensions, point, pg_dimension_identity(&dimensions, 0));
	assert(!pg_identity_context(typing, &dimensions, initial, 1, centers, &left, &right, paths));
	assert(left == saved_left && right == saved_right);
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_identity_context(&foreign, &dimensions, source, 0, NULL, &left, &right, NULL));
	pg_typing_destroy(&foreign);
	pg_whnf_work_destroy(&work);
	pg_dimensions_destroy(&dimensions);
}

static void neutral_thunks(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *scope = pg_prove_empty_context(typing);
	const struct pg_evidence *a = pg_prove_universe(typing, classifiers, scope, 0);
	const struct pg_evidence *c = pg_prove_return_type(typing, classifiers, a);
	const struct pg_evidence *u = pg_prove_thunk_type(typing, classifiers, c);
	const struct pg_object *v0 = pg_binder(typing->graph), *v1 = pg_binder(typing->graph);
	const struct pg_object *p = pg_binder(typing->graph);
	scope = pg_prove_context_extension(typing, scope, v0, u);
	scope = pg_prove_context_extension(typing, scope, v1, pg_prove_projection(typing, scope, u));
	u = pg_prove_projection(typing, scope, u);
	const struct pg_evidence *left = pg_prove_variable(typing, scope, v0);
	const struct pg_evidence *right = pg_prove_variable(typing, scope, v1);
	const struct pg_evidence *identity = pg_prove_identity_type(typing, u, left, right);
	scope = pg_prove_context_extension(typing, scope, p, identity);
	u = pg_prove_projection(typing, scope, u);
	left = pg_prove_variable(typing, scope, v0);
	right = pg_prove_variable(typing, scope, v1);
	const struct pg_evidence *path = pg_prove_variable(typing, scope, p);
	identity = pg_prove_identity_type(typing, u, left, right);
	const struct pg_evidence *expanded = pg_identity_thunk_type(typing, classifiers, u, left, right);
	assert(expanded && pg_evidence_judgement(expanded) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_identity_thunk_type(typing, classifiers, u, left, right) == expanded);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	normalizes(&work, pg_evidence_subject(identity)->core, pg_evidence_subject(expanded)->core);
	struct pg_conversion conversion;
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(path), pg_evidence_subject(expanded)->core) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing, path, expanded, pg_conversion_certificate(&conversion));
	pg_conversion_destroy(&conversion);
	const struct pg_evidence *forced = pg_prove_force(typing, converted);
	const struct pg_evidence *observations = pg_prove_identity_type(typing,
		pg_prove_projection(typing, scope, c), pg_prove_force(typing, left), pg_prove_force(typing, right));
	assert(forced && pg_evidence_classifier(forced) == pg_evidence_subject(observations)->core);
	assert(!pg_prove_force(typing, path)); /* Conversion remains an explicit premise. */
	normalizes(&work, pg_evidence_subject(forced)->core, pg_evidence_subject(forced)->core);
	const struct pg_evidence *refl = pg_prove_reflexivity(typing, u, left);
	const struct pg_evidence *refl_type = pg_identity_thunk_type(typing, classifiers, u, left, left);
	assert(pg_conversion_init(&conversion, &work, pg_evidence_classifier(refl), pg_evidence_subject(refl_type)->core) == 0);
	assert(pg_conversion_advance(&conversion, 10000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *refl_force = pg_prove_force(typing,
		pg_prove_conversion(typing, refl, refl_type, pg_conversion_certificate(&conversion)));
	pg_conversion_destroy(&conversion);
	const struct pg_evidence *force_refl = pg_prove_reflexivity(typing,
		pg_prove_projection(typing, scope, c), pg_prove_force(typing, left));
	action_result(typing, classifiers, scope, &work, force_refl, refl_force);
	const struct pg_object *x = pg_binder(typing->graph);
	const struct pg_term *force = pg_reference(typing->graph, &pg_force_operation);
	const struct pg_term *body = pg_application(typing->graph, force, pg_reference(typing->graph, x));
	/* Same computation rule under a scoped action, retaining the chosen path. */
	normalizes(&work, pg_identity_apply(typing->graph, pg_lambda(typing->graph, x, body),
		pg_evidence_subject(left)->core, pg_evidence_subject(right)->core, pg_evidence_subject(path)->core),
		pg_evidence_subject(forced)->core);
	assert(!pg_identity_thunk_type(typing, classifiers, pg_prove_projection(typing, scope, a), left, right));
	assert(!pg_identity_thunk_type(typing, classifiers, u, forced, right));
	assert(!pg_identity_thunk_type(typing, classifiers, u, left, NULL));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_identity_thunk_type(&foreign, classifiers, u, left, right));
	pg_typing_destroy(&foreign);
	pg_whnf_work_destroy(&work);
}

static void action_scope_exchange(struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = classifiers->graph;
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *a = pg_reference(graph, pg_binder(graph));
	const struct pg_term *boundary[2][3];
	for (size_t i = 0; i < 2; ++i)
		for (size_t j = 0; j < 3; ++j) boundary[i][j] = pg_reference(graph, pg_binder(graph));
	const struct pg_term *body = pg_identity_instance(graph, pg_identity_action(graph, a),
		pg_reference(graph, x), pg_reference(graph, y));
	const struct pg_term *sources[] = {
		pg_lambda(graph, x, pg_lambda(graph, y, body)),
		pg_lambda(graph, y, pg_lambda(graph, x, body))
	};
	/* Exchange of two independent declarations, not a swap of cube axes.
	 * Both ordinary endpoint substitutions agree before action is compared. */
	for (size_t side = 0; side < 2; ++side) {
		const struct pg_term *left = pg_application(graph,
			pg_application(graph, sources[0], boundary[0][side]), boundary[1][side]);
		const struct pg_term *right = pg_application(graph,
			pg_application(graph, sources[1], boundary[1][side]), boundary[0][side]);
		converts(&work, left, right);
	}
	const struct pg_term *acted[2];
	for (size_t order = 0; order < 2; ++order) {
		acted[order] = pg_identity_action(graph, sources[order]);
		for (size_t i = 0; i < 2; ++i)
			for (size_t j = 0; j < 3; ++j)
				acted[order] = pg_application(graph, acted[order], boundary[i ^ order][j]);
	}
	converts(&work, acted[0], acted[1]);
	struct pg_eval split;
	pg_eval_init(&split, acted[0]);
	split.output = graph;
	split.dispatch = pg_pure_policy.dispatch;
	while (!split.task) {
		assert(pg_eval_advance(&split, 1) == PG_EVAL_PENDING);
		assert(split.steps < 10000);
	}
	const struct pg_term *suspended = pg_eval_readback(&split, graph);
	assert(suspended);
	/* Destroy an actual suspended Identity traversal, then resume its graph
	 * through the ordinary evaluator. No task state is encoded as evidence. */
	pg_eval_destroy(&split);
	converts(&work, suspended, acted[0]);
	/* Cancel at every machine boundary, independent of task ordering. */
	pg_eval_init(&split, acted[1]);
	split.output = graph;
	split.dispatch = pg_pure_policy.dispatch;
	assert(pg_eval_advance(&split, 10000) == PG_EVAL_WHNF);
	uint64_t polls = split.steps;
	pg_eval_destroy(&split);
	for (uint64_t cut = 0; cut < polls; ++cut) {
		pg_eval_init(&split, acted[1]);
		split.output = graph;
		split.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&split, cut) == PG_EVAL_PENDING);
		assert(split.steps == cut);
		suspended = pg_eval_readback(&split, graph);
		assert(suspended);
		pg_eval_destroy(&split);
		converts(&work, suspended, acted[1]);
	}
	const struct pg_object *u = pg_binder(graph), *v = pg_binder(graph);
	const struct pg_binding_value rename[] = {
		{x, pg_reference(graph, u)}, {y, pg_reference(graph, v)}
	};
	const struct pg_term *renamed = pg_term_substitute(graph, body, 2, rename);
	renamed = pg_identity_action(graph, pg_lambda(graph, v, pg_lambda(graph, u, renamed)));
	for (size_t i = 2; i; --i)
		for (size_t j = 0; j < 3; ++j) renamed = pg_application(graph, renamed, boundary[i - 1][j]);
	converts(&work, acted[0], renamed);
	/* A nested binder shadows x only within its own body. The repeated
	 * subterm is reached both as function content and as an argument. */
	const struct pg_term *shared = pg_application(graph, pg_reference(graph, y), pg_reference(graph, x));
	const struct pg_term *nested = pg_identity_instance(graph, pg_identity_action(graph, a),
		pg_lambda(graph, x, shared), pg_application(graph, shared, shared));
	const struct pg_term *shadow_actions[2];
	for (size_t order = 0; order < 2; ++order) {
		const struct pg_object *binders[] = {x, y};
		shadow_actions[order] = pg_identity_action(graph,
			pg_lambda(graph, binders[order], pg_lambda(graph, binders[order ^ 1], nested)));
		for (size_t i = 0; i < 2; ++i)
			for (size_t j = 0; j < 3; ++j)
				shadow_actions[order] = pg_application(graph, shadow_actions[order], boundary[i ^ order][j]);
	}
	converts(&work, shadow_actions[0], shadow_actions[1]);
	const struct pg_term *deep = pg_reference(graph, y);
	for (size_t i = 0; i < 64; ++i) deep = pg_lambda(graph, pg_binder(graph), deep);
	deep = pg_identity_instance(graph, pg_identity_action(graph, a), deep, pg_reference(graph, x));
	deep = pg_identity_action(graph, pg_lambda(graph, x, pg_lambda(graph, y, deep)));
	for (size_t i = 0; i < 2; ++i)
		for (size_t j = 0; j < 3; ++j) deep = pg_application(graph, deep, boundary[i][j]);
	const uint64_t cuts[] = {0, 1, 7, 31};
	for (size_t i = 0; i < sizeof(cuts) / sizeof(*cuts); ++i) {
		pg_eval_init(&split, deep);
		split.output = graph;
		split.dispatch = pg_pure_policy.dispatch;
		while (!split.task) {
			assert(pg_eval_advance(&split, 1) == PG_EVAL_PENDING);
			assert(split.steps < 10000);
		}
		uint64_t before = split.steps;
		assert(pg_eval_advance(&split, cuts[i]) == PG_EVAL_PENDING);
		assert(split.steps == before + cuts[i]);
		suspended = pg_eval_readback(&split, graph);
		assert(suspended);
		pg_eval_destroy(&split);
		converts(&work, suspended, deep);
	}
	/* Environment exchange must not forget the selected center proof. */
	const struct pg_term *changed = pg_application(graph, acted[0]->as.application.function,
		pg_reference(graph, pg_binder(graph)));
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, &work, acted[0], changed) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&comparison);
	/* Grow the transient source index while retaining only its outermost and
	 * innermost declarations. Unused boundary computations must not run. */
	const struct pg_object *many[96];
	for (size_t i = 0; i < 96; ++i) many[i] = pg_binder(graph);
	const struct pg_term *large = pg_identity_instance(graph, pg_identity_action(graph, a),
		pg_reference(graph, many[0]), pg_reference(graph, many[95]));
	for (size_t i = 96; i; --i) large = pg_lambda(graph, many[i - 1], large);
	large = pg_identity_action(graph, large);
	const struct pg_term *self = pg_lambda(graph, x,
		pg_application(graph, pg_reference(graph, x), pg_reference(graph, x)));
	const struct pg_term *unused = pg_application(graph, self, self);
	for (size_t i = 0; i < 96; ++i)
		for (size_t j = 0; j < 3; ++j) {
			const struct pg_term *argument = unused;
			if (i == 0) argument = boundary[0][j];
			if (i == 95) argument = boundary[1][j];
			large = pg_application(graph, large, argument);
		}
	converts(&work, large, acted[0]);
	pg_whnf_work_destroy(&work);
}

static void iterated_lambda_suspension(struct pg_graph *graph, struct pg_whnf_work *work,
	const struct pg_term *function, size_t binders, size_t selected)
{
	const struct pg_term *source = pg_identity_action(graph, pg_identity_action(graph, function));
	normalizes(work, source, source);
	const struct pg_term *expected = NULL;
	for (size_t i = 0; i < 9 * binders; ++i) {
		const struct pg_term *argument = pg_reference(graph, pg_binder(graph));
		if (i == 9 * selected + 8) expected = argument;
		source = pg_application(graph, source, argument);
		if (i < 2) normalizes(work, source, source);
	}
	struct pg_eval whole;
	pg_eval_init(&whole, source);
	whole.output = graph;
	whole.dispatch = pg_pure_policy.dispatch;
	assert(pg_eval_advance(&whole, 10000) == PG_EVAL_WHNF);
	converts(work, pg_eval_readback(&whole, graph), expected);
	uint64_t steps = whole.steps;
	pg_eval_destroy(&whole);
	for (uint64_t cut = 0; cut < steps; ++cut) {
		struct pg_eval split;
		pg_eval_init(&split, source);
		split.output = graph;
		split.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&split, cut) == PG_EVAL_PENDING);
		const struct pg_term *snapshot = pg_eval_readback(&split, graph);
		assert(snapshot);
		converts(work, snapshot, expected);
		assert(pg_eval_advance(&split, steps - cut) == PG_EVAL_WHNF);
		assert(split.steps == steps);
		converts(work, pg_eval_readback(&split, graph), expected);
		pg_eval_destroy(&split);
		converts(work, snapshot, expected);
	}
}

static void lambda_actions(struct pg_classifiers *classifiers)
{
	struct pg_graph *graph = classifiers->graph;
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, graph) == 0);
	const struct pg_object *x = pg_binder(graph), *y = pg_binder(graph);
	const struct pg_term *vx = pg_reference(graph, x), *vy = pg_reference(graph, y);
	const struct pg_term *p = pg_reference(graph, pg_binder(graph));
	const struct pg_term *q = pg_reference(graph, pg_binder(graph));
	const struct pg_term *a = pg_reference(graph, pg_binder(graph));
	const struct pg_term *b = pg_reference(graph, pg_binder(graph));
	const struct pg_term *id = pg_lambda(graph, x, vx);
	iterated_lambda_suspension(graph, &work, id, 1, 0);
	iterated_lambda_suspension(graph, &work, pg_lambda(graph, x, pg_lambda(graph, y, vx)), 2, 0);
	iterated_lambda_suspension(graph, &work, pg_lambda(graph, x, pg_lambda(graph, y, vy)), 2, 1);
	normalizes(&work, pg_identity_apply(graph, id, a, b, p), p);
	normalizes(&work, pg_identity_apply(graph, id, a, b, q), q);
	/* Reusing a binder pointer still selects its innermost complete triple. */
	const struct pg_term *rebound = pg_identity_apply(graph, pg_lambda(graph, x, id), a, b, p);
	rebound = pg_application(graph, pg_identity_instance(graph, rebound, a, b), q);
	normalizes(&work, rebound, q);
	const struct pg_term *outer = pg_identity_apply(graph, pg_lambda(graph, x, pg_lambda(graph, y, vx)), a, b, p);
	outer = pg_application(graph, pg_identity_instance(graph, outer, a, b), q);
	normalizes(&work, outer, p);
	const struct pg_term *returned_y = pg_application(graph, pg_reference(graph, &pg_return_operation), vy);
	const struct pg_term *inner = pg_identity_apply(graph, pg_lambda(graph, y, returned_y), a, b, vx);
	const struct pg_term *nested_action = pg_identity_apply(graph, pg_lambda(graph, x, inner), a, b, p);
	const struct pg_term *nested_expected = pg_application(graph, pg_reference(graph, &pg_return_operation), p);
	struct pg_eval nested_whole;
	pg_eval_init(&nested_whole, nested_action);
	nested_whole.output = graph;
	nested_whole.dispatch = pg_pure_policy.dispatch;
	assert(pg_eval_advance(&nested_whole, 10000) == PG_EVAL_WHNF);
	converts(&work, pg_eval_readback(&nested_whole, graph), nested_expected);
	uint64_t nested_steps = nested_whole.steps;
	pg_eval_destroy(&nested_whole);
	/* Cancel through comparison and rebuilding of the acted source prefix. */
	for (uint64_t cut = 0; cut < nested_steps; ++cut) {
		struct pg_eval split;
		pg_eval_init(&split, nested_action);
		split.output = graph;
		split.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&split, cut) == PG_EVAL_PENDING);
		const struct pg_term *snapshot = pg_eval_readback(&split, graph);
		assert(snapshot);
		pg_eval_destroy(&split);
		converts(&work, snapshot, nested_expected);
		/* Retaining the task must agree with discarding its private work. */
		pg_eval_init(&split, nested_action);
		split.output = graph;
		split.dispatch = pg_pure_policy.dispatch;
		assert(pg_eval_advance(&split, cut) == PG_EVAL_PENDING);
		assert(pg_eval_advance(&split, nested_steps - cut) == PG_EVAL_WHNF);
		assert(split.steps == nested_steps);
		converts(&work, pg_eval_readback(&split, graph), nested_expected);
		pg_eval_destroy(&split);
	}
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	/* Complex constant families prune scope even with neutral endpoints;
	 * unused boundary computations must never be demanded. */
	const struct pg_term *family = pg_identity_instance(graph, pg_identity_action(graph, a), vx, vx);
	const struct pg_term *closed = pg_identity_instance(graph, pg_identity_action(graph, a), a, b);
	/* Reflexivity commutes with substitution, including an acted source. */
	const struct pg_term *refl_a = pg_identity_action(graph, a);
	const struct pg_term *diagonal_family = pg_lambda(graph, x, family);
	const struct pg_term *diagonal = pg_identity_apply(graph, diagonal_family, a, a, refl_a);
	const struct pg_term *diagonal_result = pg_identity_action(graph,
		pg_identity_instance(graph, pg_identity_action(graph, a), a, a));
	normalizes(&work, diagonal, diagonal_result);
	const struct pg_term *chosen_loop = pg_identity_apply(graph, diagonal_family, a, a, p);
	normalizes(&work, chosen_loop, chosen_loop);
	const struct pg_term *wrong_endpoint = pg_identity_apply(graph, diagonal_family, a, b, refl_a);
	normalizes(&work, wrong_endpoint, wrong_endpoint);
	const struct pg_term *two = pg_lambda(graph, x, pg_lambda(graph, y,
		pg_identity_instance(graph, pg_identity_action(graph, a), vx, vy)));
	const struct pg_term *two_partial = pg_identity_instance(graph, pg_identity_action(graph, two), a, a);
	normalizes(&work, two_partial, two_partial);
	const struct pg_term *two_action = pg_application(graph, two_partial, refl_a);
	converts(&work, two_action, pg_identity_action(graph, pg_lambda(graph, y,
		pg_identity_instance(graph, pg_identity_action(graph, a), a, vy))));
	two_action = pg_application(graph, pg_identity_instance(graph, two_action, b, b), pg_identity_action(graph, b));
	normalizes(&work, two_action, pg_identity_action(graph, closed));
	/* Neutral application is the normal form, not an expansion/retraction loop. */
	const struct pg_term *neutral_call = pg_application(graph, q, a);
	const struct pg_term *neutral_refl = pg_identity_action(graph, neutral_call);
	normalizes(&work, neutral_refl, neutral_refl);
	normalizes(&work, pg_identity_apply(graph, q, a, a, refl_a), neutral_refl);
	const struct pg_term *force_q = pg_application(graph, pg_reference(graph, &pg_force_operation), q);
	const struct pg_term *forced_refl = pg_identity_action(graph, pg_application(graph, force_q, a));
	normalizes(&work, pg_identity_apply(graph, force_q, a, a, refl_a), forced_refl);
	const struct pg_term *observed_refl = pg_application(graph, pg_reference(graph, &pg_force_operation),
		pg_identity_action(graph, q));
	normalizes(&work, observed_refl, pg_identity_action(graph, force_q));
	normalizes(&work, pg_application(graph, pg_identity_instance(graph, observed_refl, a, a), refl_a), forced_refl);
	const struct pg_term *neutral_loop = pg_identity_apply(graph, q, a, a, p);
	normalizes(&work, neutral_loop, neutral_loop);
	/* Equal syntax under different environments is not an equal endpoint. */
	const struct pg_term *prefix = pg_application(graph, pg_lambda(graph, x,
		pg_application(graph, pg_identity_action(graph, diagonal_family), vx)), a);
	const struct pg_term *capture_diagonal = pg_application(graph, pg_application(graph, prefix, vx),
		pg_identity_action(graph, vx));
	converts(&work, capture_diagonal, pg_identity_apply(graph, diagonal_family, a, vx, pg_identity_action(graph, vx)));
	struct pg_whnf_work diagonal_whole;
	assert(pg_whnf_work_init(&diagonal_whole, graph) == 0);
	struct pg_whnf_job *diagonal_job = pg_whnf_request(&diagonal_whole, &pg_pure_policy, diagonal);
	assert(pg_whnf_advance(diagonal_job, 100000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(diagonal_job) == diagonal_result);
	assert(pg_whnf_steps(diagonal_job) == pg_whnf_steps(pg_whnf_request(&work, &pg_pure_policy, diagonal)));
	pg_whnf_work_destroy(&diagonal_whole);
	normalizes(&work, pg_identity_apply(graph, pg_lambda(graph, x, closed), omega, omega, omega),
		pg_identity_action(graph, closed));
	const struct pg_term *f_type = pg_return_type(classifiers, a);
	converts(&work, pg_identity_instance(graph,
		pg_identity_apply(graph, pg_lambda(graph, x, f_type), omega, omega, omega), a, b),
		pg_identity_instance(graph, pg_identity_action(graph, f_type), a, b));
	const struct pg_object *z = pg_binder(graph);
	const struct pg_term *scoped = pg_identity_apply(graph,
		pg_lambda(graph, y, pg_lambda(graph, x, pg_lambda(graph, z, family))), omega, omega, omega);
	scoped = pg_application(graph, pg_identity_instance(graph, scoped, a, b), p);
	scoped = pg_application(graph, pg_identity_instance(graph, scoped, omega, omega), omega);
	const struct pg_term *retained = pg_identity_apply(graph, pg_lambda(graph, x, family), a, b, p);
	converts(&work, scoped, retained);
	const struct pg_term *shadowed = pg_identity_apply(graph,
		pg_lambda(graph, x, pg_lambda(graph, x, family)), omega, omega, omega);
	/* The complete outer triple is unused because the inner binder shadows it. */
	converts(&work, shadowed, pg_identity_action(graph, pg_lambda(graph, x, family)));
	const struct pg_term *incomplete = pg_application(graph,
		pg_identity_action(graph, pg_lambda(graph, x, family)), a);
	normalizes(&work, incomplete, incomplete);
	incomplete = pg_application(graph, incomplete, b);
	normalizes(&work, incomplete, incomplete);
	shadowed = pg_application(graph, pg_identity_instance(graph, shadowed, a, b), p);
	converts(&work, shadowed, retained);
	struct pg_whnf_work pruning_whole;
	assert(pg_whnf_work_init(&pruning_whole, graph) == 0);
	struct pg_whnf_job *pruned = pg_whnf_request(&pruning_whole, &pg_pure_policy, scoped);
	assert(pg_whnf_advance(pruned, 100000) == PG_EVAL_WHNF);
	struct pg_whnf_job *pruned_split = pg_whnf_request(&work, &pg_pure_policy, scoped);
	assert(pg_whnf_steps(pruned) == pg_whnf_steps(pruned_split));
	converts(&work, pg_whnf_result(pruned), pg_whnf_result(pruned_split));
	pg_whnf_work_destroy(&pruning_whole);
	normalizes(&work, pg_identity_apply(graph, id, omega, omega, p), p);
	normalizes(&work, pg_identity_apply(graph, pg_lambda(graph, x, a), omega, omega, q), pg_identity_action(graph, a));
	const struct pg_term *first = pg_lambda(graph, x, pg_lambda(graph, y, vx));
	const struct pg_term *partial = pg_identity_apply(graph, first, a, b, p);
	normalizes(&work, partial, partial);
	const struct pg_term *both = pg_application(graph, pg_identity_instance(graph, partial, a, b), q);
	normalizes(&work, both, p);
	const struct pg_term *second = pg_lambda(graph, x, pg_lambda(graph, y, vy));
	partial = pg_identity_apply(graph, second, a, b, p);
	normalizes(&work, pg_application(graph, pg_identity_instance(graph, partial, a, b), q), q);
	const struct pg_term *composed = pg_lambda(graph, x, pg_application(graph, pg_lambda(graph, y, vy), vx));
	normalizes(&work, pg_identity_apply(graph, composed, a, b, p), p);
	const struct pg_term *apply = pg_lambda(graph, x, pg_application(graph, a, vx));
	converts(&work, pg_identity_apply(graph, apply, a, b, p),
		pg_application(graph, pg_identity_instance(graph, pg_identity_action(graph, a), a, b), p));
	const struct pg_term *ret = pg_reference(graph, &pg_return_operation);
	const struct pg_term *returned = pg_identity_apply(graph,
		pg_lambda(graph, x, pg_application(graph, ret, vx)), a, b, p);
	converts(&work, returned, pg_application(graph, ret, p));
	const struct pg_term *thunk = pg_reference(graph, &pg_thunk_operation);
	converts(&work, pg_identity_apply(graph, pg_lambda(graph, x,
		pg_application(graph, thunk, pg_application(graph, ret, vx))), a, b, p),
		pg_application(graph, thunk, pg_application(graph, ret, p)));
	/* The outer closure captures x, but only the action's y binder varies. */
	const struct pg_term *captured = pg_lambda(graph, x,
		pg_identity_apply(graph, pg_lambda(graph, y, vx), a, b, p));
	normalizes(&work, pg_application(graph, captured, a), pg_identity_action(graph, a));
	struct pg_whnf_work whole;
	assert(pg_whnf_work_init(&whole, graph) == 0);
	struct pg_whnf_job *split_job = pg_whnf_request(&work, &pg_pure_policy, returned);
	struct pg_whnf_job *whole_job = pg_whnf_request(&whole, &pg_pure_policy, returned);
	assert(pg_whnf_advance(whole_job, 100000) == PG_EVAL_WHNF);
	assert(pg_whnf_steps(whole_job) == pg_whnf_steps(split_job));
	converts(&work, pg_whnf_result(whole_job), pg_whnf_result(split_job));
	pg_whnf_work_destroy(&whole);
	const size_t arity = 2048;
	const struct pg_term *deep = id;
	for (size_t i = 1; i < arity; ++i) deep = pg_lambda(graph, pg_binder(graph), deep);
	deep = pg_identity_action(graph, deep);
	for (size_t i = 0; i < arity; ++i)
		deep = pg_application(graph, pg_identity_instance(graph, deep, a, b), i + 1 == arity ? q : p);
	struct pg_whnf_job *deep_job = pg_whnf_request(&work, &pg_pure_policy, deep);
	assert(pg_whnf_advance(deep_job, 1000000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(deep_job) == q);
	pg_whnf_work_destroy(&work);
}

static void boundary_context(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *p, const struct pg_evidence *q,
	const struct pg_evidence *substitution)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_binding_cube *cube = pg_binding_cube(&dimensions, 1);
	const struct pg_coordinate zero = {PG_ENDPOINT_ZERO, 0}, one = {PG_ENDPOINT_ONE, 0};
	const struct pg_binding_face *left = pg_binding_face(&dimensions, cube,
		pg_dimension_map(&dimensions, 0, 1, &zero));
	const struct pg_binding_face *right = pg_binding_face(&dimensions, cube,
		pg_dimension_map(&dimensions, 0, 1, &one));
	const struct pg_binding_face *center = pg_binding_face(&dimensions, cube,
		pg_dimension_identity(&dimensions, 1));
	assert(left && right && center);
	const struct pg_evidence *lp = pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *rp = pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_RIGHT_TYPE);
	const struct pg_evidence *lq = pg_prove_identity_endpoint_type(typing, classifiers, q, PG_IDENTITY_LEFT_TYPE);
	assert(lp && rp && lq && lp != lq);
	assert(pg_evidence_premise(lp, 0) == p && pg_evidence_premise(lq, 0) == q);
	assert(pg_evidence_judgement(lp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(lp)->core == pg_evidence_subject(lq)->core);
	assert(pg_prove_identity_endpoint_type(typing, classifiers, p, PG_IDENTITY_LEFT_TYPE) == lp);
	assert(!pg_prove_identity_endpoint_type(typing, classifiers, p, PG_PI_DOMAIN));
	assert(!pg_prove_identity_endpoint_type(typing, classifiers, NULL, PG_IDENTITY_LEFT_TYPE));
	const struct pg_evidence *cp = pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable);
	const struct pg_evidence *cq = pg_identity_context_extend(typing, classifiers, scope, q,
		&left->variable, &right->variable, &center->variable);
	assert(cp && cq && cp != cq);
	assert(pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable) == cp);
	const struct pg_context *context = pg_evidence_context(cp);
	assert(context->parent->parent->parent == pg_evidence_context(scope));
	assert(context->parent->parent->declared_type == pg_evidence_subject(lp)->core);
	assert(context->parent->declared_type == pg_evidence_subject(rp)->core);
	assert(context->declared_type == pg_identity_instance(typing->graph, pg_evidence_subject(p)->core,
		pg_reference(typing->graph, &left->variable), pg_reference(typing->graph, &right->variable)));
	assert(context->declared_type != pg_evidence_context(cq)->declared_type);
	const struct pg_evidence *variable = pg_prove_variable(typing, cp, &center->variable);
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, cp, variable);
	assert(type && pg_evidence_classifier(type) == pg_evidence_classifier(lp));
	assert(pg_evidence_subject(type)->core == context->declared_type);
	assert(pg_evidence_classifier(variable) == context->declared_type);
	const struct pg_evidence *extensions[] = {
		pg_evidence_premise(pg_evidence_premise(cp, 0), 0), pg_evidence_premise(cp, 0), cp
	};
	const struct pg_object *binders[] = {&left->variable, &right->variable, &center->variable};
	const struct pg_evidence *lifted = substitution;
	for (size_t i = 0; i < 3; ++i) {
		lifted = pg_prove_substitution_lift(typing, lifted, extensions[i], binders[i]);
		assert(lifted);
	}
	assert(pg_evidence_context(pg_evidence_premise(lifted, 1)) == pg_evidence_context(cq));
	const struct pg_evidence *moved = pg_prove_reindex(typing, lifted, variable);
	assert(moved && pg_evidence_classifier(moved) == pg_evidence_context(cq)->declared_type);
	assert(pg_evidence_subject(moved)->core == pg_evidence_subject(variable)->core);
	/* The dependent boundary uses the existing raw computation Pi/Lambda rules. */
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, variable);
	for (size_t i = 3; i; --i) {
		const struct pg_evidence *codomain = pg_prove_classifier(typing, classifiers, extensions[i - 1], body);
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers,
			pg_evidence_premise(extensions[i - 1], 1), extensions[i - 1], codomain);
		body = pg_prove_lambda(typing, pi, body);
		assert(body && pg_evidence_judgement(body) == PG_JUDGEMENT_COMPUTATION);
	}
	assert(pg_evidence_context(body) == pg_evidence_context(scope));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &left->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		&left->variable, &right->variable, &right->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		pg_evidence_context(scope)->binder, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, p,
		NULL, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, pg_prove_empty_context(typing), p,
		&left->variable, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope,
		pg_prove_return(typing, classifiers, p), &left->variable, &right->variable, &center->variable));
	assert(!pg_identity_context_extend(typing, classifiers, scope, variable,
		&left->variable, &right->variable, &center->variable));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_identity_context_extend(&foreign, classifiers, scope, p,
		&left->variable, &right->variable, &center->variable));
	pg_typing_destroy(&foreign);
	pg_dimensions_destroy(&dimensions);
}

static void dependent_families(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *a, const struct pg_evidence *b,
	const struct pg_evidence *x, const struct pg_evidence *y,
	const struct pg_evidence *p, const struct pg_evidence *q, const struct pg_evidence *refl_a)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *z = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, z, universe);
	const struct pg_evidence *family = pg_prove_value_type(typing, pg_prove_variable(typing, source, z));
	const struct pg_evidence *ls = pg_prove_substitution(typing, source, scope, 1, &a);
	const struct pg_evidence *rs = pg_prove_substitution(typing, source, scope, 1, &b);
	assert(ls && rs && family);
	const struct pg_evidence *vp = pg_prove_family_identity_type(typing, family, ls, rs, 1, &p, x, y);
	const struct pg_evidence *vq = pg_prove_family_identity_type(typing, family, ls, rs, 1, &q, x, y);
	assert(vp && vq && vp != vq);
	assert(pg_evidence_classifier(vp) == pg_universe(classifiers, 0));
	assert(pg_evidence_judgement(vp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_subject(vp)->core != pg_evidence_subject(vq)->core);
	assert(pg_evidence_premise(vp, 0) == family && pg_evidence_premise(vp, 3) == p);
	assert(pg_prove_family_identity_type(typing, family, ls, rs, 1, &p, x, y) == vp);
	const struct pg_term *abstraction = pg_lambda(typing->graph, z, pg_evidence_subject(family)->core);
	const struct pg_term *expected = pg_identity_instance(typing->graph,
		pg_identity_apply(typing->graph, abstraction, pg_evidence_subject(a)->core,
			pg_evidence_subject(b)->core, pg_evidence_subject(p)->core),
		pg_evidence_subject(x)->core, pg_evidence_subject(y)->core);
	assert(pg_evidence_subject(vp)->core == expected);
	const struct pg_evidence *cf = pg_prove_return_type(typing, classifiers, family);
	const struct pg_evidence *rx = pg_prove_return(typing, classifiers, x);
	const struct pg_evidence *ry = pg_prove_return(typing, classifiers, y);
	const struct pg_evidence *cp = pg_prove_family_identity_type(typing, cf, ls, rs, 1, &p, rx, ry);
	assert(cp && pg_evidence_judgement(cp) == PG_JUDGEMENT_COMPUTATION_TYPE);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	/* Contextual action of z:Universe retains the selected p, not just A/B. */
	const struct pg_evidence *zvalue = pg_prove_variable(typing, source, z);
	const struct pg_evidence *zsort = pg_prove_projection(typing, source, universe);
	const struct pg_evidence *za = pg_prove_family_action(typing, zsort, zvalue, ls, rs, 1, &p);
	const struct pg_evidence *zq = pg_prove_family_action(typing, zsort, zvalue, ls, rs, 1, &q);
	assert(za && zq && za != zq && pg_evidence_rule(za) == PG_FAMILY_ACTION);
	assert(pg_evidence_premise(za, 1) == zvalue);
	assert(pg_evidence_premise(pg_evidence_premise(za, 0), 3) == p);
	size_t terms_before = typing->graph->terms.count, proofs_before = typing->proofs.count;
	assert(pg_prove_family_action(typing, zsort, zvalue, ls, rs, 1, &p) == za);
	assert(typing->graph->terms.count == terms_before && typing->proofs.count == proofs_before);
	action_result(typing, classifiers, scope, &work, za, p);
	action_result(typing, classifiers, scope, &work, zq, q);
	const struct pg_evidence *rz = pg_prove_return(typing, classifiers, zvalue);
	const struct pg_evidence *rza = pg_prove_family_action(typing,
		pg_prove_return_type(typing, classifiers, zsort), rz, ls, rs, 1, &p);
	action_result(typing, classifiers, scope, &work, rza, pg_prove_return(typing, classifiers, p));
	const struct pg_evidence *tz = pg_prove_thunk(typing, classifiers, rz);
	const struct pg_evidence *tza = pg_prove_family_action(typing,
		pg_prove_classifier(typing, classifiers, source, tz), tz, ls, rs, 1, &p);
	action_result(typing, classifiers, scope, &work, tza,
		pg_prove_thunk(typing, classifiers, pg_prove_return(typing, classifiers, p)));
	/* Heterogeneous U(F Z) observations retain the chosen family, even when
	 * neither endpoint exposes THUNK or RETURN. */
	const struct pg_evidence *uf = pg_prove_thunk_type(typing, classifiers, cf);
	const struct pg_object *m0 = pg_binder(typing->graph), *m1 = pg_binder(typing->graph);
	const struct pg_evidence *mscope = pg_prove_context_extension(typing, scope, m0,
		pg_prove_reindex(typing, ls, uf));
	mscope = pg_prove_context_extension(typing, mscope, m1,
		pg_prove_projection(typing, mscope, pg_prove_reindex(typing, rs, uf)));
	const struct pg_evidence *ma = pg_prove_projection(typing, mscope, a);
	const struct pg_evidence *mb = pg_prove_projection(typing, mscope, b);
	const struct pg_evidence *mls = pg_prove_substitution(typing, source, mscope, 1, &ma);
	const struct pg_evidence *mrs = pg_prove_substitution(typing, source, mscope, 1, &mb);
	const struct pg_evidence *mp = pg_prove_projection(typing, mscope, p);
	const struct pg_evidence *ml = pg_prove_variable(typing, mscope, m0);
	const struct pg_evidence *mr = pg_prove_variable(typing, mscope, m1);
	const struct pg_evidence *mid = pg_prove_family_identity_type(typing, uf, mls, mrs, 1, &mp, ml, mr);
	const struct pg_evidence *mexpanded = pg_prove_thunk_type(typing, classifiers,
		pg_prove_family_identity_type(typing, cf, mls, mrs, 1, &mp,
			pg_prove_force(typing, ml), pg_prove_force(typing, mr)));
	assert(mid && mexpanded);
	converts(&work, pg_evidence_subject(mid)->core, pg_evidence_subject(mexpanded)->core);
	assert(!pg_prove_family_action(typing, family, zvalue, ls, rs, 1, &p));
	assert(!pg_prove_family_action(typing, zsort, rz, ls, rs, 1, &p));
	assert(!pg_prove_family_action(typing, zsort, zvalue, rs, ls, 1, &p));
	assert(!pg_prove_family_action(typing, zsort, zvalue, ls, rs, 1, &x));
	assert(!pg_prove_family_action(typing, zsort, zvalue, ls, rs, 1, NULL));
	assert(!pg_prove_family_action(typing, zsort, NULL, ls, rs, 1, &p));
	assert(!pg_prove_family_action(typing, zsort, a, ls, rs, 1, &p));
	assert(!pg_prove_family_action(typing, zsort, zvalue, NULL, rs, 1, &p));
	const struct pg_evidence *instance = pg_prove_identity_instance(typing, classifiers, p, x, y);
	converts(&work, pg_evidence_subject(vp)->core, pg_evidence_subject(instance)->core);
	converts(&work, pg_evidence_subject(cp)->core,
		pg_evidence_subject(pg_prove_return_type(typing, classifiers, instance))->core);
	/* Act on C(Z) = Pi e:Z. F Z across distinct A/B and the selected p. */
	const struct pg_object *e = pg_binder(typing->graph);
	const struct pg_evidence *element_scope = pg_prove_context_extension(typing, source, e, family);
	/* The second declaration e:Z varies over the first path p:A=B. Its
	 * center must inhabit that chosen family, not an unrelated Id A x y. */
	const struct pg_object *element_path = pg_binder(typing->graph);
	const struct pg_evidence *telescope_scope = pg_prove_context_extension(typing, scope, element_path, vp);
	const struct pg_evidence *tleft[] = {pg_prove_projection(typing, telescope_scope, a),
		pg_prove_projection(typing, telescope_scope, x)};
	const struct pg_evidence *tright[] = {pg_prove_projection(typing, telescope_scope, b),
		pg_prove_projection(typing, telescope_scope, y)};
	const struct pg_evidence *tls = pg_prove_substitution(typing, element_scope, telescope_scope, 2, tleft);
	const struct pg_evidence *trs = pg_prove_substitution(typing, element_scope, telescope_scope, 2, tright);
	const struct pg_evidence *paths[] = {pg_prove_projection(typing, telescope_scope, p),
		pg_prove_variable(typing, telescope_scope, element_path)};
	const struct pg_evidence *etype = pg_prove_projection(typing, element_scope, family);
	const struct pg_evidence *element = pg_prove_variable(typing, element_scope, e);
	const struct pg_evidence *ta = pg_prove_family_action(typing, etype, element, tls, trs, 2, paths);
	assert(ta);
	const struct pg_evidence *ta_type = pg_evidence_premise(ta, 0);
	assert(pg_evidence_premise_count(ta_type) == 7);
	assert(pg_evidence_premise(ta_type, 3) == paths[0]);
	assert(pg_evidence_premise(ta_type, 4) == paths[1]);
	action_result(typing, classifiers, telescope_scope, &work, ta, paths[1]);
	terms_before = typing->graph->terms.count;
	proofs_before = typing->proofs.count;
	assert(pg_prove_family_action(typing, etype, element, tls, trs, 2, paths) == ta);
	assert(typing->graph->terms.count == terms_before && typing->proofs.count == proofs_before);
	const struct pg_evidence *ereturn = pg_prove_return(typing, classifiers, element);
	const struct pg_evidence *rta = pg_prove_family_action(typing,
		pg_prove_return_type(typing, classifiers, etype), ereturn, tls, trs, 2, paths);
	action_result(typing, classifiers, telescope_scope, &work, rta,
		pg_prove_return(typing, classifiers, paths[1]));
	const struct pg_evidence *ethunk = pg_prove_thunk(typing, classifiers, ereturn);
	action_result(typing, classifiers, telescope_scope, &work,
		pg_prove_family_action(typing, pg_prove_classifier(typing, classifiers, element_scope, ethunk),
			ethunk, tls, trs, 2, paths),
		pg_prove_thunk(typing, classifiers, pg_prove_return(typing, classifiers, paths[1])));
	/* Diagonal action of the dependent family (Z,e:Z) |-> Id Z e e. */
	const struct pg_evidence *diagonal_paths[2];
	for (size_t i = 0; i < 2; ++i) diagonal_paths[i] = pg_prove_reflexivity(typing,
		pg_prove_classifier(typing, classifiers, telescope_scope, tleft[i]), tleft[i]);
	const struct pg_evidence *diagonal_prefix = pg_prove_substitution(typing, source, telescope_scope, 1, tleft);
	const struct pg_evidence *path_type = pg_prove_family_identity_type(typing,
		family, diagonal_prefix, diagonal_prefix, 1, diagonal_paths, tleft[1], tleft[1]);
	struct pg_conversion diagonal_conversion;
	assert(pg_conversion_init(&diagonal_conversion, &work, pg_evidence_classifier(diagonal_paths[1]),
		pg_evidence_subject(path_type)->core) == 0);
	assert(pg_conversion_advance(&diagonal_conversion, 100000) == PG_CONVERSION_EQUAL);
	diagonal_paths[1] = pg_prove_conversion(typing, diagonal_paths[1], path_type,
		pg_conversion_certificate(&diagonal_conversion));
	pg_conversion_destroy(&diagonal_conversion);
	const struct pg_evidence *id_family = pg_prove_type_value(typing, pg_prove_identity_type(typing, etype, element, element));
	const struct pg_evidence *id_sort = pg_prove_classifier(typing, classifiers, element_scope, id_family);
	const struct pg_evidence *id_instance = pg_prove_reindex(typing, tls, id_family);
	action_result(typing, classifiers, telescope_scope, &work,
		pg_prove_family_action(typing, id_sort, id_family, tls, tls, 2, diagonal_paths),
		pg_prove_reflexivity(typing, pg_prove_classifier(typing, classifiers, telescope_scope, id_instance), id_instance));
	/* Zero varied declarations is diagonal action after a common substitution. */
	action_result(typing, classifiers, telescope_scope, &work,
		pg_prove_family_action(typing, etype, element, tls, tls, 0, NULL),
		pg_prove_reflexivity(typing, pg_prove_classifier(typing, classifiers, telescope_scope, tleft[1]), tleft[1]));
	const struct pg_evidence *empty_map = pg_prove_substitution(typing, empty, empty, 0, NULL);
	const struct pg_evidence *uvalue = pg_prove_type_value(typing, universe);
	action_result(typing, classifiers, empty, &work,
		pg_prove_family_action(typing, pg_prove_classifier(typing, classifiers, empty, uvalue),
			uvalue, empty_map, empty_map, 0, NULL),
		pg_prove_reflexivity(typing, pg_prove_classifier(typing, classifiers, empty, uvalue), uvalue));
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 0, NULL));
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 1, paths));
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 3, paths));
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, SIZE_MAX, paths));
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 2, NULL));
	paths[1] = paths[0];
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 2, paths));
	assert(pg_evidence_premise(ta_type, 4) != paths[1]);
	paths[1] = pg_prove_variable(typing, telescope_scope, element_path);
	paths[0] = pg_prove_projection(typing, telescope_scope, q);
	assert(!pg_prove_family_action(typing, etype, element, tls, trs, 2, paths));
	paths[0] = pg_prove_projection(typing, telescope_scope, p);
	assert(pg_prove_family_action(typing, etype, element, tls, trs, 2, paths) == ta);
	/* Extend the dependent telescope to eight declarations in the same
	 * direction. Every new center is checked over all preceding centers. */
	const struct pg_evidence *many_paths[8] = {paths[0], paths[1]};
	const struct pg_evidence *many_left[8] = {tleft[0], tleft[1]};
	const struct pg_evidence *many_right[8] = {tright[0], tright[1]};
	const struct pg_evidence *many_source = element_scope, *many_destination = telescope_scope;
	const struct pg_evidence *many_type = etype, *many_ls = tls, *many_rs = trs;
	for (size_t i = 2; i < 8; ++i) {
		const struct pg_evidence *center_type = pg_prove_family_identity_type(typing,
			many_type, many_ls, many_rs, i, many_paths, many_left[1], many_right[1]);
		const struct pg_object *center_binder = pg_binder(typing->graph), *source_binder = pg_binder(typing->graph);
		many_destination = pg_prove_context_extension(typing, many_destination, center_binder, center_type);
		assert(many_destination);
		for (size_t j = 0; j < i; ++j) {
			many_paths[j] = pg_prove_projection(typing, many_destination, many_paths[j]);
			many_left[j] = pg_prove_projection(typing, many_destination, many_left[j]);
			many_right[j] = pg_prove_projection(typing, many_destination, many_right[j]);
		}
		many_paths[i] = pg_prove_variable(typing, many_destination, center_binder);
		many_left[i] = many_left[1];
		many_right[i] = many_right[1];
		many_source = pg_prove_context_extension(typing, many_source, source_binder, many_type);
		many_type = pg_prove_projection(typing, many_source, many_type);
		many_ls = pg_prove_substitution(typing, many_source, many_destination, i + 1, many_left);
		many_rs = pg_prove_substitution(typing, many_source, many_destination, i + 1, many_right);
		const struct pg_evidence *many_action = pg_prove_family_action(typing, many_type,
			pg_prove_variable(typing, many_source, source_binder), many_ls, many_rs, i + 1, many_paths);
		action_result(typing, classifiers, many_destination, &work, many_action, many_paths[i]);
	}
	const struct pg_evidence *pi_family = pg_prove_pi(typing, classifiers, family, element_scope,
		pg_prove_return_type(typing, classifiers, pg_prove_projection(typing, element_scope, family)));
	const struct pg_evidence *types[] = {a, b}, *functions[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *domain = pg_prove_value_type(typing, types[i]);
		const struct pg_evidence *local = pg_prove_context_extension(typing, scope, e, domain);
		const struct pg_evidence *body = pg_prove_return(typing, classifiers, pg_prove_variable(typing, local, e));
		const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, local,
			pg_prove_classifier(typing, classifiers, local, body));
		functions[i] = pg_prove_lambda(typing, pi, body);
		assert(functions[i]);
	}
	assert(pg_evidence_subject(functions[0])->core == pg_evidence_subject(functions[1])->core);
	const struct pg_evidence *acted_pi = pg_prove_family_identity_type(typing, pi_family, ls, rs, 1, &p, functions[0], functions[1]);
	assert(acted_pi && pg_evidence_judgement(acted_pi) == PG_JUDGEMENT_COMPUTATION_TYPE);
	const struct pg_object *x0 = pg_binder(typing->graph), *x1 = pg_binder(typing->graph), *center = pg_binder(typing->graph);
	const struct pg_evidence *boundary = pg_identity_context_extend(typing, classifiers, scope, p, x0, x1, center);
	const struct pg_evidence *path_scope = boundary;
	const struct pg_evidence *output = pg_prove_return_type(typing, classifiers,
		pg_prove_classifier(typing, classifiers, boundary, pg_prove_variable(typing, boundary, center)));
	for (size_t i = 0; i < 3; ++i) {
		output = pg_prove_pi(typing, classifiers, pg_evidence_premise(boundary, 1), boundary, output);
		boundary = pg_evidence_premise(boundary, 0);
	}
	assert(output);
	converts(&work, pg_evidence_subject(acted_pi)->core, pg_evidence_subject(output)->core);
	/* A raw dependent Lambda acts without a value-side function constructor. */
	const struct pg_evidence *function = pg_prove_lambda(typing, pi_family,
		pg_prove_return(typing, classifiers, pg_prove_variable(typing, element_scope, e)));
	const struct pg_evidence *function_action = pg_prove_family_action(typing, pi_family, function, ls, rs, 1, &p);
	assert(function_action);
	struct pg_conversion pi_conversion;
	assert(pg_conversion_init(&pi_conversion, &work, pg_evidence_classifier(function_action), pg_evidence_subject(output)->core) == 0);
	assert(pg_conversion_advance(&pi_conversion, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *applied = pg_prove_conversion(typing, function_action, output, pg_conversion_certificate(&pi_conversion));
	pg_conversion_destroy(&pi_conversion);
	applied = pg_prove_projection(typing, path_scope, applied);
	const struct pg_object *arguments[] = {x0, x1, center};
	for (size_t i = 0; i < 3; ++i)
		applied = pg_prove_application(typing, applied, pg_prove_variable(typing, path_scope, arguments[i]));
	action_result(typing, classifiers, path_scope, &work, applied,
		pg_prove_return(typing, classifiers, pg_prove_variable(typing, path_scope, center)));
	const struct pg_evidence *other_pi = pg_prove_family_identity_type(typing, pi_family, ls, rs, 1, &q, functions[0], functions[1]);
	struct pg_conversion distinct;
	assert(pg_conversion_init(&distinct, &work, pg_evidence_subject(other_pi)->core, pg_evidence_subject(output)->core) == 0);
	assert(pg_conversion_advance(&distinct, 100000) == PG_CONVERSION_DIFFERENT);
	pg_conversion_destroy(&distinct);
	pg_whnf_work_destroy(&work);
	assert(pg_evidence_classifier(cp) == pg_evidence_classifier(vp));
	assert(!pg_prove_type_value(typing, cp));
	assert(!pg_prove_context_extension(typing, scope, pg_binder(typing->graph), cp));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, &p, y, x));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, &x, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, &refl_a, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, &p, rx, ry));
	assert(!pg_prove_family_identity_type(typing, cf, ls, rs, 1, &p, x, y));
	assert(!pg_prove_family_identity_type(typing, family, rs, ls, 1, &p, y, x));
	assert(!pg_prove_family_identity_type(typing, universe, ls, rs, 1, &p, a, b));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, NULL, x, y));
	assert(!pg_prove_family_identity_type(typing, family, ls, rs, 1, &p, NULL, y));

	/* A dependent family of identifications, with a common ambient type t:
	 * C(z) = Id Universe t z. Vary z along p, retaining t := A on both faces. */
	const struct pg_object *t = pg_binder(typing->graph);
	const struct pg_evidence *prefix = pg_prove_context_extension(typing, empty, t, universe);
	source = pg_prove_context_extension(typing, prefix, z, pg_prove_projection(typing, prefix, universe));
	const struct pg_evidence *higher = pg_prove_identity_type(typing,
		pg_prove_projection(typing, source, universe),
		pg_prove_variable(typing, source, t), pg_prove_variable(typing, source, z));
	const struct pg_evidence *li[] = {a, a}, *ri[] = {a, b}, *wrong[] = {b, b};
	ls = pg_prove_substitution(typing, source, scope, 2, li);
	rs = pg_prove_substitution(typing, source, scope, 2, ri);
	const struct pg_evidence *hs = pg_prove_family_identity_type(typing, higher, ls, rs, 1, &p, refl_a, p);
	assert(hs && pg_evidence_classifier(hs) == pg_universe(classifiers, 1));
	assert(pg_prove_family_identity_type(typing, higher, ls, rs, 1, &p, refl_a, p) == hs);
	const struct pg_evidence *bad = pg_prove_substitution(typing, source, scope, 2, wrong);
	assert(bad && !pg_prove_family_identity_type(typing, higher, ls, bad, 1, &p, refl_a, p));
	/* The unvaried ambient term t maps to A on both faces: its action is refl A. */
	zsort = pg_prove_projection(typing, source, universe);
	const struct pg_evidence *ambient = pg_prove_variable(typing, source, t);
	const struct pg_evidence *constant_action = pg_prove_family_action(typing, zsort, ambient, ls, rs, 1, &p);
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	action_result(typing, classifiers, scope, &work, constant_action, refl_a);
	/* An ambient image may use the varied binder's pointer freely in the
	 * destination. Closing the source before substitution must not capture it. */
	const struct pg_evidence *capture_scope = pg_prove_context_extension(typing, scope, z,
		pg_prove_projection(typing, scope, universe));
	const struct pg_evidence *free_z = pg_prove_variable(typing, capture_scope, z);
	const struct pg_evidence *cl[] = {free_z, pg_prove_projection(typing, capture_scope, a)};
	const struct pg_evidence *cr[] = {free_z, pg_prove_projection(typing, capture_scope, b)};
	const struct pg_evidence *capture_path = pg_prove_projection(typing, capture_scope, p);
	const struct pg_evidence *capture_action = pg_prove_family_action(typing, zsort, ambient,
		pg_prove_substitution(typing, source, capture_scope, 2, cl),
		pg_prove_substitution(typing, source, capture_scope, 2, cr),
		1, &capture_path);
	action_result(typing, classifiers, capture_scope, &work, capture_action,
		pg_prove_reflexivity(typing, pg_prove_projection(typing, capture_scope, universe), free_z));
	pg_whnf_work_destroy(&work);
	assert(!pg_prove_family_action(typing, zsort, ambient, ls, bad, 1, &p));
	const struct pg_term *ambient_type = pg_identity_instance(typing->graph,
		pg_identity_action(typing->graph, pg_universe(classifiers, 0)),
		pg_evidence_subject(a)->core, pg_reference(typing->graph, z));
	expected = pg_identity_instance(typing->graph,
		pg_identity_apply(typing->graph, pg_lambda(typing->graph, z, ambient_type),
			pg_evidence_subject(a)->core, pg_evidence_subject(b)->core, pg_evidence_subject(p)->core),
		pg_evidence_subject(refl_a)->core, pg_evidence_subject(p)->core);
	assert(pg_alpha_equal(pg_evidence_subject(hs)->core, expected) == 1);
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, typing->graph) == 0);
	assert(!pg_prove_family_identity_type(&foreign, higher, ls, rs, 1, &p, refl_a, p));
	assert(!pg_prove_family_action(&foreign, zsort, ambient, ls, rs, 1, &p));
	pg_typing_destroy(&foreign);
}

static void dependent_pi_action(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_object *z = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, z, universe);
	const struct pg_evidence *value = pg_prove_variable(typing, source, z);
	const struct pg_evidence *refl = pg_prove_reflexivity(typing,
		pg_prove_projection(typing, source, universe), value);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, refl);
	const struct pg_evidence *codomain = pg_prove_classifier(typing, classifiers, source, body);
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, universe, source, codomain);
	const struct pg_evidence *function = pg_prove_lambda(typing, pi, body);
	assert(function);
	const struct pg_evidence *witness = pg_prove_reflexivity(typing, pi, function);
	const struct pg_object *x0 = pg_binder(typing->graph), *x1 = pg_binder(typing->graph), *p = pg_binder(typing->graph);
	const struct pg_evidence *expanded = pg_identity_pi_type(typing, classifiers, empty, pi,
		function, function, x0, x1, p);
	assert(expanded && pg_evidence_judgement(expanded) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(pg_identity_pi_type(typing, classifiers, empty, pi, function, function, x0, x1, p) == expanded);
	assert(pg_evidence_classifier(expanded) == pg_evidence_classifier(pi));
	const struct pg_evidence *inner = expanded;
	for (size_t i = 0; i < 3; ++i) {
		assert(pg_evidence_rule(inner) == PG_PI_FORM);
		inner = pg_evidence_premise(inner, 2);
	}
	assert(pg_evidence_rule(inner) == PG_FAMILY_IDENTITY_FORM);
	assert(pg_evidence_subject(pg_evidence_premise(inner, 3))->core == pg_reference(typing->graph, p));
	struct pg_whnf_work work;
	struct pg_conversion comparison;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(witness),
		pg_evidence_subject(expanded)->core) == 0);
	assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing, witness, expanded,
		pg_conversion_certificate(&comparison));
	assert(converted);
	pg_conversion_destroy(&comparison);
	const struct pg_object *a = pg_binder(typing->graph);
	const struct pg_evidence *arguments = pg_prove_context_extension(typing, empty, a, universe);
	const struct pg_evidence *av = pg_prove_variable(typing, arguments, a);
	const struct pg_evidence *ap = pg_prove_reflexivity(typing,
		pg_prove_projection(typing, arguments, universe), av);
	const struct pg_evidence *application = pg_prove_projection(typing, arguments, converted);
	application = pg_prove_application(typing, application, av);
	application = pg_prove_application(typing, application, av);
	assert(application && !pg_prove_application(typing, application, av));
	application = pg_prove_application(typing, application, ap);
	assert(application && pg_evidence_judgement(application) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_prove_classifier(typing, classifiers, arguments, application));
	const struct pg_term *input = pg_evidence_classifier(witness);
	struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, input);
	while (pg_whnf_status(job) == PG_EVAL_PENDING) pg_whnf_advance(job, 1);
	assert(pg_whnf_status(job) == PG_EVAL_WHNF);
	const struct pg_term *domain, *tail;
	const struct pg_object *binder;
	assert(pg_pi_view(pg_whnf_result(job), &domain, &binder, &tail));
	assert(domain == pg_evidence_subject(universe)->core);
	struct pg_whnf_job *beta = pg_whnf_request(&work, &pg_beta_policy, input);
	assert(pg_whnf_advance(beta, 1000) == PG_EVAL_WHNF && pg_whnf_result(beta) == input);
	/* The type rule retains endpoint computations, even nonterminating ones. */
	const struct pg_term *v = pg_reference(typing->graph, z);
	const struct pg_term *self = pg_lambda(typing->graph, z, pg_application(typing->graph, v, v));
	const struct pg_term *omega = pg_application(typing->graph, self, self);
	input = pg_identity_instance(typing->graph, pg_identity_action(typing->graph,
		pg_evidence_subject(pi)->core), omega, omega);
	job = pg_whnf_request(&work, &pg_pure_policy, input);
	assert(pg_whnf_advance(job, 10000) == PG_EVAL_WHNF);
	assert(pg_pi_view(pg_whnf_result(job), &domain, &binder, &tail));
	assert(!pg_identity_pi_type(typing, classifiers, empty, pi, function, function, x0, x0, p));
	assert(!pg_identity_pi_type(typing, classifiers, empty, pi, function, value, x0, x1, p));
	assert(!pg_identity_pi_type(typing, classifiers, empty, universe, function, function, x0, x1, p));
	pg_whnf_work_destroy(&work);
}

static void typed_lambda_action(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *domain, const struct pg_evidence *value)
{
	const struct pg_object *z = pg_binder(typing->graph);
	const struct pg_evidence *source = pg_prove_context_extension(typing, scope, z, domain);
	const struct pg_evidence *variable = pg_prove_variable(typing, source, z);
	const struct pg_evidence *body = pg_prove_return(typing, classifiers, variable);
	const struct pg_evidence *codomain = pg_prove_classifier(typing, classifiers, source, body);
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, domain, source, codomain);
	const struct pg_evidence *id = pg_prove_lambda(typing, pi, body);
	const struct pg_evidence *composition = pg_prove_lambda(typing, pi,
		pg_prove_application(typing, pg_prove_projection(typing, source, id), variable));
	const struct pg_evidence *functions[] = {id, composition};
	const struct pg_object *y = pg_binder(typing->graph), *r = pg_binder(typing->graph);
	const struct pg_evidence *arguments = pg_prove_context_extension(typing, scope, y, domain);
	const struct pg_evidence *path_type = pg_prove_identity_type(typing,
		pg_prove_projection(typing, arguments, domain), pg_prove_projection(typing, arguments, value),
		pg_prove_variable(typing, arguments, y));
	arguments = pg_prove_context_extension(typing, arguments, r, path_type);
	const struct pg_evidence *a0 = pg_prove_projection(typing, arguments, value);
	const struct pg_evidence *a1 = pg_prove_variable(typing, arguments, y);
	const struct pg_evidence *path = pg_prove_variable(typing, arguments, r);
	const struct pg_evidence *expected = pg_prove_return(typing, classifiers, path);
	const struct pg_evidence *expected_type = pg_prove_classifier(typing, classifiers, arguments, expected);
	assert(expected_type);
	struct pg_whnf_work work;
	assert(pg_whnf_work_init(&work, typing->graph) == 0);
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *witness = pg_prove_reflexivity(typing, pi, functions[i]);
		const struct pg_evidence *expanded = pg_identity_pi_type(typing, classifiers, scope, pi,
			functions[i], functions[i], pg_binder(typing->graph), pg_binder(typing->graph), pg_binder(typing->graph));
		assert(witness && expanded);
		struct pg_conversion comparison;
		assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(witness), pg_evidence_subject(expanded)->core) == 0);
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *function = pg_prove_conversion(typing, witness, expanded, pg_conversion_certificate(&comparison));
		pg_conversion_destroy(&comparison);
		const struct pg_evidence *applied = pg_prove_projection(typing, arguments, function);
		applied = pg_prove_application(typing, applied, a0);
		applied = pg_prove_application(typing, applied, a1);
		applied = pg_prove_application(typing, applied, path);
		assert(applied);
		assert(pg_conversion_init(&comparison, &work, pg_evidence_classifier(applied), pg_evidence_subject(expected_type)->core) == 0);
		assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
		const struct pg_evidence *checked = pg_prove_conversion(typing, applied, expected_type,
			pg_conversion_certificate(&comparison));
		assert(checked);
		pg_conversion_destroy(&comparison);
		converts(&work, pg_evidence_subject(applied)->core, pg_evidence_subject(expected)->core);
		struct pg_whnf_job *job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(checked)->core);
		assert(pg_whnf_advance(job, 100000) == PG_EVAL_WHNF);
		const struct pg_evidence *normal = pg_prove_normalization(typing, checked, pg_whnf_certificate(job));
		const struct pg_evidence *result = pg_prove_return_value(typing, normal);
		assert(result && pg_evidence_rule(result) == PG_RETURN_VALUE);
		assert(pg_evidence_premise(result, 0) == normal);
		assert(pg_evidence_classifier(result) == pg_evidence_classifier(path));
		assert(pg_prove_classifier(typing, classifiers, arguments, result));
		job = pg_whnf_request(&work, &pg_pure_policy, pg_evidence_subject(result)->core);
		assert(pg_whnf_advance(job, 100000) == PG_EVAL_WHNF);
		result = pg_prove_normalization(typing, result, pg_whnf_certificate(job));
		assert(result && pg_evidence_subject(result)->core == pg_evidence_subject(path)->core);
		assert(pg_evidence_classifier(result) == pg_evidence_classifier(path));
	}
	pg_whnf_work_destroy(&work);
}

int main(void)
{
	struct pg_graph graph;
	struct pg_typing typing;
	struct pg_classifiers classifiers;
	assert(pg_graph_init(&graph) == 0);
	assert(pg_typing_init(&typing, &graph) == 0);
	assert(pg_classifiers_init(&classifiers, &graph) == 0);
	lambda_actions(&classifiers);
	const struct pg_evidence *empty = pg_prove_empty_context(&typing);
	dependent_pi_action(&typing, &classifiers);
	heterogeneous_pi(&typing, &classifiers);
	const struct pg_evidence *u0 = pg_prove_universe(&typing, &classifiers, empty, 0);
	const struct pg_object *a = pg_binder(&graph), *b = pg_binder(&graph);
	const struct pg_object *p = pg_binder(&graph), *q = pg_binder(&graph);
	const struct pg_object *x = pg_binder(&graph), *y = pg_binder(&graph);
	const struct pg_evidence *scope = pg_prove_context_extension(&typing, empty, a, u0);
	scope = pg_prove_context_extension(&typing, scope, b, pg_prove_projection(&typing, scope, u0));
	const struct pg_evidence *left_type = pg_prove_variable(&typing, scope, a);
	const struct pg_evidence *right_type = pg_prove_variable(&typing, scope, b);
	const struct pg_evidence *universe = pg_prove_projection(&typing, scope, u0);
	const struct pg_evidence *family_type = pg_prove_identity_type(&typing, universe, left_type, right_type);
	assert(family_type && pg_evidence_judgement(family_type) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(family_type) == pg_universe(&classifiers, 1));
	scope = pg_prove_context_extension(&typing, scope, p, family_type);
	scope = pg_prove_context_extension(&typing, scope, q, pg_prove_projection(&typing, scope, family_type));
	scope = pg_prove_context_extension(&typing, scope, x, pg_prove_projection(&typing, scope, left_type));
	scope = pg_prove_context_extension(&typing, scope, y, pg_prove_projection(&typing, scope, right_type));
	assert(scope);
	const struct pg_evidence *pp = pg_prove_variable(&typing, scope, p);
	const struct pg_evidence *qq = pg_prove_variable(&typing, scope, q);
	const struct pg_evidence *xx = pg_prove_variable(&typing, scope, x);
	const struct pg_evidence *yy = pg_prove_variable(&typing, scope, y);
	const struct pg_evidence *rp = pg_prove_identity_instance(&typing, &classifiers, pp, xx, yy);
	const struct pg_evidence *rq = pg_prove_identity_instance(&typing, &classifiers, qq, xx, yy);
	assert(rp && rq && rp != rq);
	assert(pg_evidence_judgement(rp) == PG_JUDGEMENT_VALUE_TYPE);
	assert(pg_evidence_classifier(rp) == pg_universe(&classifiers, 0));
	assert(pg_evidence_subject(rp)->core != pg_evidence_subject(rq)->core);
	assert(pg_alpha_equal(pg_evidence_subject(rp)->core, pg_evidence_subject(rq)->core) == 0);
	assert(pg_evidence_premise(rp, 0) == pp && pg_evidence_premise(rq, 0) == qq);
	assert(pg_prove_identity_instance(&typing, &classifiers, pp, xx, yy) == rp);
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp, yy, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, xx, xx, yy));
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp, left_type, yy));
	assert(!pg_prove_identity_instance(&typing, &classifiers, pp,
		pg_prove_return(&typing, &classifiers, xx), yy));
	assert(!pg_prove_application(&typing, pp, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, NULL, xx, yy));
	assert(pg_prove_context_extension(&typing, scope, pg_binder(&graph), rp));

	/* Id A and refl A are the same graph action, not independent former tags. */
	const struct pg_evidence *a_value = pg_prove_projection(&typing, scope, left_type);
	const struct pg_evidence *a_type = pg_prove_value_type(&typing, a_value);
	typed_lambda_action(&typing, &classifiers, scope, a_type, xx);
	universe = pg_prove_projection(&typing, scope, u0);
	const struct pg_evidence *id_a = pg_prove_reflexivity(&typing, universe, a_value);
	const struct pg_evidence *diagonal = pg_prove_identity_type(&typing, a_type, xx, xx);
	const struct pg_evidence *via_family = pg_prove_identity_instance(&typing, &classifiers, id_a, xx, xx);
	assert(diagonal && via_family);
	assert(pg_evidence_subject(diagonal)->core == pg_evidence_subject(via_family)->core);
	assert(pg_evidence_classifier(diagonal) == pg_evidence_classifier(via_family));
	const struct pg_evidence *refl_x = pg_prove_reflexivity(&typing, a_type, xx);
	assert(refl_x && pg_evidence_judgement(refl_x) == PG_JUDGEMENT_VALUE);
	assert(pg_evidence_classifier(refl_x) == pg_evidence_subject(diagonal)->core);
	assert(pg_prove_classifier(&typing, &classifiers, scope, refl_x) == diagonal);
	assert(!pg_prove_identity_type(&typing, a_type, xx, yy));
	assert(!pg_prove_identity_type(&typing, a_value, xx, xx));
	assert(!pg_prove_reflexivity(&typing, universe, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers, refl_x, xx, xx));
	const struct pg_term *base, *left, *right;
	assert(pg_identity_action_view(pg_evidence_subject(refl_x)->core, &base));
	assert(base == pg_evidence_subject(xx)->core);
	assert(!pg_identity_action_view(pg_evidence_subject(diagonal)->core, &base));
	assert(!pg_identity_view(pg_evidence_subject(refl_x)->core, &base, &left, &right));
	assert(!pg_identity_view(NULL, &base, &left, &right));
	assert(pg_identity_view(pg_evidence_classifier(refl_x), &base, &left, &right));
	assert(base == pg_evidence_subject(a_type)->core);
	assert(left == pg_evidence_subject(xx)->core && right == left);
	assert(!pg_identity_view(pg_evidence_subject(rp)->core, &base, &left, &right));
	const struct pg_evidence *p_type = pg_prove_classifier(&typing, &classifiers, scope, pp);
	const struct pg_evidence *refl_p = pg_prove_reflexivity(&typing, p_type, pp);
	const struct pg_evidence *p_to_q = pg_prove_identity_type(&typing, p_type, pp, qq);
	struct pg_whnf_work beta;
	struct pg_conversion comparison;
	assert(pg_whnf_work_init(&beta, &graph) == 0);
	assert(pg_conversion_init(&comparison, &beta, pg_evidence_classifier(refl_p),
		pg_evidence_subject(p_to_q)->core) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_prove_conversion(&typing, refl_p, p_to_q, pg_conversion_certificate(&comparison)));
	pg_conversion_destroy(&comparison);
	pg_whnf_work_destroy(&beta);
	const struct pg_object *argument = pg_binder(&graph);
	const struct pg_evidence *body_scope = pg_prove_context_extension(&typing, scope, argument, a_type);
	const struct pg_evidence *body = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, body_scope, argument));
	const struct pg_evidence *body_type = pg_prove_return_type(&typing, &classifiers,
		pg_prove_projection(&typing, body_scope, a_type));
	const struct pg_evidence *pi = pg_prove_pi(&typing, &classifiers, a_type, body_scope, body_type);
	const struct pg_evidence *function = pg_prove_lambda(&typing, pi, body);
	assert(function);
	assert(!pg_prove_identity_instance(&typing, &classifiers, function, xx, xx));
	assert(!pg_prove_identity_instance(&typing, &classifiers,
		pg_prove_thunk(&typing, &classifiers, function), xx, xx));
	const struct pg_evidence *b_type = pg_prove_value_type(&typing, pg_prove_projection(&typing, scope, right_type));
	const struct pg_evidence *other_scope = pg_prove_context_extension(&typing, scope, argument, b_type);
	const struct pg_evidence *other_body = pg_prove_return(&typing, &classifiers,
		pg_prove_variable(&typing, other_scope, argument));
	const struct pg_evidence *other_pi = pg_prove_pi(&typing, &classifiers, b_type, other_scope,
		pg_prove_return_type(&typing, &classifiers, pg_prove_projection(&typing, other_scope, b_type)));
	const struct pg_evidence *other_function = pg_prove_lambda(&typing, other_pi, other_body);
	assert(other_function && pg_evidence_subject(function)->core == pg_evidence_subject(other_function)->core);
	const struct pg_evidence *function_action = pg_prove_reflexivity(&typing, pi, function);
	const struct pg_evidence *other_action = pg_prove_reflexivity(&typing, other_pi, other_function);
	assert(function_action && other_action);
	assert(pg_evidence_subject(function_action)->core == pg_evidence_subject(other_action)->core);
	assert(pg_evidence_classifier(function_action) != pg_evidence_classifier(other_action));
	assert(!pg_prove_identity_type(&typing, pi, function, other_function));

	/* Higher symbolic diagonal witnesses retain their preceding classifier and
	 * term. This is not yet the full higher boundary-instantiation algorithm. */
	const struct pg_evidence *witness = pp;
	for (size_t dimension = 0; dimension < 3; ++dimension) {
		const struct pg_evidence *type = pg_prove_classifier(&typing, &classifiers, scope, witness);
		const struct pg_evidence *next = pg_prove_reflexivity(&typing, type, witness);
		assert(next && pg_evidence_judgement(next) == PG_JUDGEMENT_VALUE);
		assert(pg_identity_view(pg_evidence_classifier(next), &base, &left, &right));
		assert(base == pg_evidence_classifier(witness));
		assert(left == pg_evidence_subject(witness)->core && right == left);
		assert(pg_evidence_premise(next, 1) == witness);
		witness = next;
	}

	/* Computation families remain computation types and never become context
	 * values just because they have a universe bound. No endpoint is executed. */
	const struct pg_evidence *fa = pg_prove_return_type(&typing, &classifiers, a_type);
	const struct pg_evidence *returned = pg_prove_return(&typing, &classifiers, xx);
	const struct pg_evidence *cid = pg_prove_identity_type(&typing, fa, returned, returned);
	const struct pg_evidence *crefl = pg_prove_reflexivity(&typing, fa, returned);
	assert(cid && pg_evidence_judgement(cid) == PG_JUDGEMENT_COMPUTATION_TYPE);
	assert(crefl && pg_evidence_judgement(crefl) == PG_JUDGEMENT_COMPUTATION);
	assert(pg_prove_classifier(&typing, &classifiers, scope, crefl) == cid);
	assert(!pg_prove_type_value(&typing, cid));
	assert(!pg_prove_context_extension(&typing, scope, pg_binder(&graph), cid));
	assert(!pg_prove_identity_type(&typing, fa, xx, xx));
	assert(!pg_prove_identity_type(&typing, a_type, returned, returned));
	assert(!pg_prove_identity_instance(&typing, &classifiers, crefl, xx, xx));
	const struct pg_evidence *quoted = pg_prove_thunk(&typing, &classifiers, crefl);
	assert(quoted && pg_evidence_judgement(quoted) == PG_JUDGEMENT_VALUE);
	const struct pg_evidence *return_refl = pg_prove_return(&typing, &classifiers, refl_x);
	struct pg_eval machine;
	pg_computation_eval_init(&machine, &graph, pg_evidence_subject(crefl)->core);
	assert(pg_eval_advance(&machine, 100) == PG_EVAL_WHNF);
	assert(pg_eval_readback(&machine, &graph) == pg_evidence_subject(return_refl)->core);
	pg_eval_destroy(&machine);
	struct pg_whnf_work normalization;
	assert(pg_whnf_work_init(&normalization, &graph) == 0);
	const struct pg_term *input = pg_evidence_subject(crefl)->core;
	struct pg_whnf_job *beta_job = pg_whnf_request(&normalization, &pg_beta_policy, input);
	assert(pg_whnf_advance(beta_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(beta_job) == input);
	normalizes(&normalization, input, pg_evidence_subject(return_refl)->core);
	assert(beta_job != pg_whnf_request(&normalization, &pg_pure_policy, input));
	assert(pg_whnf_result(beta_job) == input);
	assert(!pg_whnf_request(&normalization, NULL, input));
	const struct pg_evidence *return_identity = pg_prove_return_type(&typing, &classifiers, diagonal);
	normalizes(&normalization, pg_evidence_subject(cid)->core, pg_evidence_subject(return_identity)->core);
	assert(pg_conversion_init(&comparison, &normalization, pg_evidence_classifier(crefl),
		pg_evidence_subject(return_identity)->core) == 0);
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&comparison) < 1000);
	assert(pg_conversion_status(&comparison) == PG_CONVERSION_EQUAL);
	assert(pg_prove_conversion(&typing, crefl, return_identity, pg_conversion_certificate(&comparison)));
	pg_conversion_destroy(&comparison);
	const struct pg_evidence *ufa = pg_prove_thunk_type(&typing, &classifiers, fa);
	const struct pg_evidence *delayed = pg_prove_thunk(&typing, &classifiers, returned);
	const struct pg_evidence *urefl = pg_prove_reflexivity(&typing, ufa, delayed);
	normalizes(&normalization, pg_evidence_subject(urefl)->core, pg_evidence_subject(quoted)->core);
	const struct pg_evidence *uid = pg_prove_identity_type(&typing, ufa, delayed, delayed);
	const struct pg_evidence *ucid = pg_prove_thunk_type(&typing, &classifiers, cid);
	const struct pg_evidence *expanded_uid = pg_identity_thunk_type(&typing, &classifiers, ufa, delayed, delayed);
	normalizes(&normalization, pg_evidence_subject(uid)->core, pg_evidence_subject(expanded_uid)->core);
	converts(&normalization, pg_evidence_subject(expanded_uid)->core, pg_evidence_subject(ucid)->core);
	assert(pg_conversion_init(&comparison, &normalization, pg_evidence_classifier(urefl),
		pg_evidence_classifier(quoted)) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
	/* WHNF respects suspension. Normalizing a family never needs an unhandled
	 * operation's answer or the contents of a THUNK to choose this equation. */
	const struct pg_object *v = pg_binder(&graph);
	const struct pg_term *vv = pg_reference(&graph, v);
	/* Another evaluator policy cannot authorize the checker's conversion. */
	static const struct pg_eval_policy other_policy = {first_argument};
	const struct pg_term *opaque_app = pg_application(&graph, vv, pg_evidence_subject(xx)->core);
	struct pg_whnf_job *other_job = pg_whnf_request(&normalization, &other_policy, opaque_app);
	assert(pg_whnf_advance(other_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(other_job) == pg_evidence_subject(xx)->core);
	assert(pg_conversion_init(&comparison, &normalization, opaque_app, pg_evidence_subject(xx)->core) == 0);
	assert(pg_conversion_advance(&comparison, 1000) == PG_CONVERSION_DIFFERENT);
	assert(!pg_conversion_certificate(&comparison));
	pg_conversion_destroy(&comparison);
	const struct pg_term *self = pg_lambda(&graph, v, pg_application(&graph, vv, vv));
	const struct pg_term *omega = pg_application(&graph, self, self);
	const struct pg_term *thunk = pg_reference(&graph, &pg_thunk_operation);
	const struct pg_term *suspended = pg_application(&graph, thunk, omega);
	normalizes(&normalization, pg_identity_action(&graph, suspended),
		pg_application(&graph, thunk, pg_identity_action(&graph, omega)));
	const struct pg_term *ufamily = pg_identity_action(&graph, pg_evidence_subject(ufa)->core);
	const struct pg_term *force = pg_reference(&graph, &pg_force_operation);
	const struct pg_term *force_suspended = pg_application(&graph, force, suspended);
	normalizes(&normalization, pg_identity_instance(&graph, ufamily, suspended, suspended),
		pg_thunk_type(&classifiers, pg_identity_instance(&graph,
			pg_identity_action(&graph, pg_evidence_subject(fa)->core), force_suspended, force_suspended)));
	/* Even an untyped divergent endpoint is not demanded by U formation. */
	normalizes(&normalization, pg_identity_instance(&graph, ufamily, omega, vv),
		pg_thunk_type(&classifiers, pg_identity_instance(&graph,
			pg_identity_action(&graph, pg_evidence_subject(fa)->core),
			pg_application(&graph, force, omega), pg_application(&graph, force, vv))));
	const struct pg_term *ffamily = pg_identity_action(&graph, pg_evidence_subject(fa)->core);
	const struct pg_term *neutral = pg_identity_instance(&graph, ffamily, vv, omega);
	normalizes(&normalization, neutral, neutral);
	normalizes(&normalization, ffamily, ffamily);
	normalizes(&normalization, pg_evidence_subject(rp)->core, pg_evidence_subject(rp)->core);
	/* Beta exposure and lexical capture use the same evaluator demand frames. */
	const struct pg_term *ret = pg_reference(&graph, &pg_return_operation);
	const struct pg_term *captured = pg_lambda(&graph, v,
		pg_identity_instance(&graph, ffamily, pg_application(&graph, ret, vv), pg_evidence_subject(returned)->core));
	normalizes(&normalization, pg_application(&graph, captured, pg_evidence_subject(xx)->core),
		pg_evidence_subject(return_identity)->core);
	struct pg_whnf_work unsplit;
	assert(pg_whnf_work_init(&unsplit, &graph) == 0);
	struct pg_whnf_job *whole_job = pg_whnf_request(&unsplit, &pg_pure_policy, input);
	assert(pg_whnf_advance(whole_job, 1000) == PG_EVAL_WHNF);
	assert(pg_whnf_result(whole_job) == pg_whnf_result(pg_whnf_request(&normalization, &pg_pure_policy, input)));
	assert(pg_whnf_steps(whole_job) == pg_whnf_steps(pg_whnf_request(&normalization, &pg_pure_policy, input)));
	pg_whnf_work_destroy(&unsplit);
	pg_whnf_work_destroy(&normalization);

	/* No new authority for reindexing Identity: the family parameter stays a
	 * visible Core operand and is substituted by the existing traversal. */
	size_t count = 6;
	const struct pg_evidence *images[] = {
		a_value, pg_prove_projection(&typing, scope, right_type), qq, qq, xx, yy
	};
	const struct pg_evidence *sigma = pg_prove_substitution(&typing, scope, scope, count, images);
	transport_fields(&typing, &classifiers, scope, pp, qq, xx, yy, sigma);
	assert(sigma);
	boundary_context(&typing, &classifiers, scope, pp, qq, sigma);
	dependent_families(&typing, &classifiers, scope, a_value,
		pg_prove_projection(&typing, scope, right_type), xx, yy, pp, qq, id_a);
	const struct pg_evidence *moved = pg_prove_reindex(&typing, sigma, rp);
	assert(moved && pg_evidence_subject(moved)->core == pg_evidence_subject(rq)->core);
	const struct pg_evidence *moved_witness = pg_prove_reindex(&typing, sigma, refl_x);
	assert(moved_witness);
	assert(pg_evidence_subject(pg_prove_classifier(&typing, &classifiers, scope, moved_witness))->core
		== pg_evidence_classifier(moved_witness));
	struct pg_typing foreign;
	assert(pg_typing_init(&foreign, &graph) == 0);
	assert(!pg_prove_identity_type(&foreign, a_type, xx, xx));
	assert(!pg_prove_reflexivity(&foreign, a_type, xx));
	assert(!pg_prove_identity_instance(&foreign, &classifiers, pp, xx, yy));
	pg_typing_destroy(&foreign);
	neutral_thunks(&typing, &classifiers);
	action_scope_exchange(&classifiers);
	generated_contexts(&typing, &classifiers);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("identity: chosen boundary contexts, F/U action, fixed pure conversion, policy isolation and reindex passed");
	return 0;
}
