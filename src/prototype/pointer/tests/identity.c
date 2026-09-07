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

static void converts(struct pg_whnf_work *work, const struct pg_term *left, const struct pg_term *right)
{
	struct pg_conversion comparison;
	assert(pg_conversion_init(&comparison, work, left, right) == 0);
	while (pg_conversion_advance(&comparison, 1) == PG_CONVERSION_PENDING)
		assert(pg_conversion_steps(&comparison) < 100000);
	assert(pg_conversion_status(&comparison) == PG_CONVERSION_EQUAL);
	pg_conversion_destroy(&comparison);
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
	assert(formation && pg_evidence_subject(formation)->core == pg_evidence_classifier(action));
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

static void transport_fields(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *scope, const struct pg_evidence *r, const struct pg_evidence *s,
	const struct pg_evidence *x, const struct pg_evidence *y, const struct pg_evidence *substitution)
{
	struct pg_graph *graph = typing->graph;
	struct pg_whnf_work work, whole;
	assert(pg_whnf_work_init(&work, graph) == 0);
	assert(pg_whnf_work_init(&whole, graph) == 0);
	const struct pg_evidence *a = pg_prove_identity_endpoint_type(typing, classifiers, r, PG_IDENTITY_LEFT_TYPE);
	const struct pg_evidence *a_value = pg_prove_type_value(typing, a);
	const struct pg_evidence *universe = pg_prove_classifier(typing, classifiers, scope, a_value);
	const struct pg_evidence *diagonal = pg_prove_reflexivity(typing, universe, a_value);
	const struct pg_evidence *reflexivity = pg_prove_reflexivity(typing, a, x);
	assert(diagonal && reflexivity);
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
	normalizes(&work, higher, higher);
	pg_whnf_work_destroy(&whole);
	pg_whnf_work_destroy(&work);
}

static void generated_contexts(struct pg_typing *typing, struct pg_classifiers *classifiers)
{
	struct pg_dimensions dimensions;
	assert(pg_dimensions_init(&dimensions, typing->graph) == 0);
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *universe = pg_prove_universe(typing, classifiers, empty, 0);
	const struct pg_evidence *source = pg_prove_context_extension(typing, empty, pg_binder(typing->graph), universe);
	const struct pg_evidence *initial = source;
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
	normalizes(&work, pg_identity_apply(graph, id, a, b, p), p);
	normalizes(&work, pg_identity_apply(graph, id, a, b, q), q);
	const struct pg_term *self = pg_lambda(graph, x, pg_application(graph, vx, vx));
	const struct pg_term *omega = pg_application(graph, self, self);
	/* Complex constant families prune scope even with neutral endpoints;
	 * unused boundary computations must never be demanded. */
	const struct pg_term *family = pg_identity_instance(graph, pg_identity_action(graph, a), vx, vx);
	const struct pg_term *closed = pg_identity_instance(graph, pg_identity_action(graph, a), a, b);
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
	normalizes(&work, shadowed, shadowed); /* Incomplete triples stay neutral. */
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
	generated_contexts(&typing, &classifiers);
	pg_classifiers_destroy(&classifiers);
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
	puts("identity: chosen boundary contexts, F/U action, fixed pure conversion, policy isolation and reindex passed");
	return 0;
}
