#include "occurrence_io.h"
#include "evidence.h"
#include "computation.h"
#include "action.h"
#include "derivation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *name(void *owner, const struct pg_object *object)
{
	(void)owner;
	static char buffer[96];
	const char *label = pg_computation_name(object);
	if (!label) label = pg_identity_name(object);
	return label ? label : pg_classifier_name(object, buffer, sizeof(buffer));
}

static const struct pg_object *resolve(void *owner, const char *label)
{
	const struct pg_object *object = pg_computation_resolve(label);
	if (!object) object = pg_identity_resolve(label);
	return object ? object : pg_classifier_resolve(owner, label);
}

static const struct pg_evidence *family_function(struct pg_typing *typing, int nested, int logical)
{
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *indices = pg_prove_context_extension(typing, empty, pg_binder(graph),
		pg_prove_universe(typing, empty, 1));
	indices = pg_prove_context_extension(typing, indices, pg_binder(graph),
		pg_prove_variable(typing, indices, pg_evidence_context(indices)->binder));
	const struct pg_evidence *scope = pg_prove_family_context_extension(typing, empty, pg_binder(graph),
		indices, pg_prove_universe(typing, indices, 0));
	if (nested) scope = pg_prove_family_context_extension(typing, empty, pg_binder(graph),
		scope, pg_prove_universe(typing, scope, 0));
	if (logical) return pg_prove_family_abstraction(typing, scope, pg_prove_universe(typing, scope, 0));
	const struct pg_evidence *body = pg_prove_return(typing,
		pg_prove_type_value(typing, pg_prove_universe(typing, scope, 0)));
	return pg_prove_lambda(typing, pg_prove_pi(typing, scope,
		pg_prove_classifier(typing, scope, body)), body);
}

static void write_inputs(FILE *file, struct pg_typing *typing)
{
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u0 = pg_prove_universe(typing, empty, 0);
	const struct pg_evidence *u1 = pg_prove_universe(typing, empty, 1);
	const struct pg_evidence *u2 = pg_prove_universe(typing, empty, 2);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *types = pg_prove_context_extension(typing, empty, a, u2);
	const struct pg_evidence *domain = pg_prove_value_type(typing, pg_prove_variable(typing, types, a));
	const struct pg_evidence *scope = pg_prove_context_extension(typing, types, x, domain);
	const struct pg_evidence *body = pg_prove_return(typing, pg_prove_variable(typing, scope, x));
	const struct pg_evidence *pi = pg_prove_pi(typing, scope, pg_prove_classifier(typing, scope, body));
	const struct pg_evidence *inner = pg_prove_lambda(typing, pi, body);
	const struct pg_evidence *outer_pi = pg_prove_pi(typing, types, pi);
	const struct pg_evidence *function = pg_prove_lambda(typing, outer_pi, inner);
	const struct pg_evidence *partial = pg_prove_application(typing, function, pg_prove_type_value(typing, u1));
	const struct pg_evidence *application = pg_prove_application(typing, partial, pg_prove_type_value(typing, u0));
	const struct pg_evidence *forced = pg_prove_force(typing, pg_prove_thunk(typing, application));
	const struct pg_evidence *extended = pg_prove_context_extension(typing, empty, pg_binder(graph), u0);
	const struct pg_evidence *mapped = pg_prove_lambda(typing,
		pg_prove_pi(typing, extended, pg_prove_projection(typing, extended, outer_pi)),
		pg_prove_projection(typing, extended, function));
	const struct pg_evidence *identity = pg_prove_identity_type(typing, outer_pi, function, function);
	assert(function && application && forced && mapped && identity);
	const struct pg_evidence *shared[2];
	for (size_t i = 0; i < 2; ++i) {
		const struct pg_evidence *local = pg_prove_context_extension(typing, empty, x, i ? u2 : u1);
		const struct pg_evidence *returned = pg_prove_return(typing, pg_prove_variable(typing, local, x));
		shared[i] = pg_prove_lambda(typing,
			pg_prove_pi(typing, local, pg_prove_classifier(typing, local, returned)), returned);
		assert(shared[i]);
	}
	assert(pg_evidence_subject(shared[0])->core == pg_evidence_subject(shared[1])->core);
	const struct pg_evidence *returned = pg_prove_return(typing, pg_prove_type_value(typing, u0));
	const struct pg_evidence *quoted = pg_prove_thunk(typing, returned);
	assert(returned && quoted);
	const struct pg_evidence *family = family_function(typing, 0, 0);
	const struct pg_evidence *nested = family_function(typing, 1, 0);
	const struct pg_evidence *family_map = pg_prove_lambda(typing,
		pg_prove_pi(typing, extended, pg_prove_projection(typing, extended,
			pg_prove_classifier(typing, empty, nested))),
		pg_prove_projection(typing, extended, nested));
	assert(family && nested && family_map);
	const struct pg_evidence *logical = pg_prove_family_abstraction(typing, types,
		pg_prove_family_abstraction(typing, scope, pg_prove_value_type(typing, pg_prove_variable(typing, scope, a))));
	const struct pg_evidence *logical_partial = pg_prove_family_application(typing, logical, pg_prove_type_value(typing, u1));
	const struct pg_evidence *logical_applied = pg_prove_family_application(typing, logical_partial, pg_prove_type_value(typing, u0));
	const struct pg_evidence *logical_family = family_function(typing, 0, 1);
	const struct pg_evidence *logical_nested = family_function(typing, 1, 1);
	const struct pg_evidence *logical_map = pg_prove_family_abstraction(typing, extended,
		pg_prove_projection(typing, extended, logical_nested));
	assert(logical && logical_partial && logical_applied && logical_family && logical_nested && logical_map);
	const struct pg_evidence *unused = pg_prove_context_extension(typing, empty, pg_binder(graph),
		pg_prove_universe(typing, empty, 5));
	const struct pg_evidence *closed = pg_prove_return_type(typing, pg_prove_thunk_type(typing, outer_pi));
	const struct pg_evidence *wide = pg_prove_pi(typing, unused, pg_prove_projection(typing, unused, closed));
	const struct pg_evidence *constant = pg_prove_pi_constant_codomain(typing, wide);
	const struct pg_evidence *content = pg_prove_return_content(typing, constant);
	const struct pg_evidence *unquoted = pg_prove_thunk_content(typing, content);
	const struct pg_evidence *wide_scope = pg_prove_context_extension(typing, empty, pg_binder(graph), content);
	const struct pg_evidence *wide_pi = pg_prove_pi(typing, wide_scope,
		pg_prove_return_type(typing, pg_prove_universe(typing, wide_scope, 0)));
	const struct pg_evidence *wide_family = pg_prove_family_abstraction(typing, wide_scope,
		pg_prove_universe(typing, wide_scope, 0));
	const struct pg_evidence *wide_map = pg_prove_lambda(typing,
		pg_prove_pi(typing, extended, pg_prove_projection(typing, extended, unquoted)),
		pg_prove_projection(typing, extended, function));
	const struct pg_evidence *family_scope = pg_prove_family_context_extension(typing, empty, pg_binder(graph),
		wide_scope, pg_prove_universe(typing, wide_scope, 0));
	const struct pg_evidence *family_pi = pg_prove_pi(typing, family_scope,
		pg_prove_return_type(typing, pg_prove_universe(typing, family_scope, 0)));
	const struct pg_evidence *family_logical = pg_prove_family_abstraction(typing, family_scope,
		pg_prove_universe(typing, family_scope, 0));
	const struct pg_evidence *wide_identity = pg_prove_identity_type(typing, unquoted, function, function);
	assert(constant && content && unquoted && wide_pi && wide_family);
	assert(wide_map && family_pi && family_logical && wide_identity);
	const struct pg_occurrence *roots[] = {pg_evidence_subject(function), pg_evidence_subject(application),
		pg_evidence_subject(forced), pg_evidence_subject(mapped), pg_evidence_subject(identity),
		pg_evidence_subject(shared[0]), pg_evidence_subject(shared[1]),
		pg_evidence_subject(returned), pg_evidence_subject(quoted),
		pg_evidence_subject(family), pg_evidence_subject(nested), pg_evidence_subject(family_map),
		pg_evidence_subject(logical), pg_evidence_subject(logical_partial), pg_evidence_subject(logical_applied),
		pg_evidence_subject(logical_family), pg_evidence_subject(logical_nested), pg_evidence_subject(logical_map),
		pg_evidence_subject(partial)->type, pg_evidence_subject(constant), pg_evidence_subject(content),
		pg_evidence_subject(unquoted), pg_evidence_subject(wide_pi), pg_evidence_subject(wide_family),
		pg_evidence_subject(wide_map), pg_evidence_subject(family_pi), pg_evidence_subject(family_logical),
		pg_evidence_subject(wide_identity)};
	assert(!pg_occurrences_write(file, 28, roots, name, NULL));
}

static void unary_input(struct pg_typing *typing, const struct pg_evidence *parent)
{
	const struct pg_occurrence *subject = pg_evidence_subject(parent);
	assert(subject->operand_count == 1);
	const struct pg_evidence *child = pg_evidence_for_subject(typing, subject->operands[0], NULL);
	const struct pg_evidence *type = pg_prove_structural_subject(typing, subject->type);
	assert(child && type);
	struct pg_whnf_work work;
	struct pg_conversion conversion;
	assert(!pg_whnf_work_init(&work, typing->graph));
	assert(!pg_conversion_init(&conversion, &work, subject->classifier, subject->classifier));
	assert(pg_conversion_advance(&conversion, 64) == PG_CONVERSION_EQUAL);
	const struct pg_evidence *converted = pg_prove_conversion(typing, parent, type,
		pg_conversion_certificate(&conversion));
	assert(converted && converted != parent && pg_evidence_subject(converted) == subject);
	size_t proofs = typing->proofs.count, occurrences = typing->occurrences.count;
	/* An alternative checking history is not a new object proof. Both the
	 * original introduction and conversion expose the same checked child. */
	const struct pg_evidence *parents[] = {converted, parent, converted};
	for (size_t i = 0; i < 3; ++i) {
		const struct pg_evidence *result = subject->judgement == PG_JUDGEMENT_VALUE
			? pg_prove_thunk_computation(typing, parents[i]) : pg_prove_return_value(typing, parents[i]);
		assert(result == child);
		assert(typing->proofs.count == proofs && typing->occurrences.count == occurrences);
	}
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
}

static void result_allocation(struct pg_typing *typing, const struct pg_occurrence *partial)
{
	const struct pg_occurrence *type = partial->type;
	const struct pg_term *domain, *body;
	const struct pg_object *binder, *renamed = pg_binder(typing->graph);
	assert(pg_pi_view(type->core, &domain, &binder, &body));
	struct pg_binding_value binding = {binder, pg_reference(typing->graph, renamed)};
	const struct pg_term *alpha = pg_pi(typing->graph, domain, renamed,
		pg_term_substitute(typing->graph, body, 1, &binding));
	const struct pg_term *wrong = pg_pi(typing->graph, domain, renamed,
		pg_return_type(typing->graph, pg_reference(typing->graph, pg_binder(typing->graph))));
	for (size_t i = 0; i < 4; ++i) {
		struct pg_occurrence header = *type;
		header.core = i == 1 ? wrong : alpha;
		if (i == 2) header.classifier = pg_universe(typing->graph, 17);
		if (i == 3) header.origin = partial;
		const struct pg_occurrence *result_type = pg_occurrence_intern(typing, &header, type->operands, NULL);
		assert(result_type);
		header = *partial;
		header.type = result_type;
		header.classifier = result_type->core;
		const struct pg_occurrence *result = pg_occurrence_intern(typing, &header, partial->operands, NULL);
		assert(result);
		const struct pg_evidence *checked = pg_prove_structural_subject(typing, result);
		if (!i) {
			assert(checked && pg_evidence_subject(checked) == result);
			assert(result != partial && result->core == partial->core);
			assert(pg_alpha_equal(result->classifier, partial->classifier) == 1);
		} else {
			assert(!checked && !pg_evidence_for_subject(typing, result, NULL));
		}
	}
}

static void reject_changes(struct pg_typing *typing, const struct pg_occurrence *subject)
{
	struct pg_occurrence header = *subject;
	header.classifier = pg_universe(typing->graph, 17);
	header.type = NULL;
	const struct pg_occurrence *wrong = pg_occurrence_intern(typing, &header, subject->operands, pg_occurrence_maps(subject));
	assert(wrong && !pg_prove_structural_subject(typing, wrong));
	assert(!pg_evidence_for_subject(typing, wrong, NULL));
	header = *subject;
	header.core = pg_reference(typing->graph, pg_binder(typing->graph));
	wrong = pg_occurrence_intern(typing, &header, subject->operands, pg_occurrence_maps(subject));
	assert(wrong && !pg_prove_structural_subject(typing, wrong));
	assert(!pg_evidence_for_subject(typing, wrong, NULL));
	header = *subject;
	header.context = pg_context_bind(typing, NULL, pg_binder(typing->graph),
		pg_universe(typing->graph, 0), PG_JUDGEMENT_VALUE);
	header.type = NULL;
	wrong = pg_occurrence_intern(typing, &header, subject->operands, pg_occurrence_maps(subject));
	/* A selected origin already constrains scope during interning. */
	assert(!pg_prove_structural_subject(typing, wrong));
	assert(!pg_evidence_for_subject(typing, wrong, NULL));
}

static void selected_formation(struct pg_typing *typing, const struct pg_occurrence *subject)
{
	assert(subject->selection && subject->origin);
	for (size_t variant = 0; variant < 3; ++variant) {
		struct pg_occurrence header = *subject;
		if (!variant) header.selection = 3;
		if (variant == 1) header.classifier = pg_universe(typing->graph, 0);
		if (variant == 2) header.operand_count = subject->operand_count ? 0 : 1;
		const struct pg_occurrence *argument = subject->origin;
		const struct pg_occurrence *changed = pg_occurrence_intern(typing, &header,
			variant == 2 ? &argument : subject->operands, NULL);
		assert(changed && !pg_prove_structural_subject(typing, changed));
		assert(!pg_evidence_for_subject(typing, changed, NULL));
	}
	if (!subject->operand_count) return;
	const struct pg_term *domain, *body;
	const struct pg_object *binder, *renamed = pg_binder(typing->graph);
	assert(pg_pi_view(subject->core, &domain, &binder, &body));
	struct pg_binding_value binding = {binder, pg_reference(typing->graph, renamed)};
	struct pg_occurrence header = *subject;
	header.core = pg_pi(typing->graph, domain, renamed,
		pg_term_substitute(typing->graph, body, 1, &binding));
	const struct pg_occurrence *alpha = pg_occurrence_intern(typing, &header, subject->operands, NULL);
	const struct pg_evidence *checked = pg_prove_structural_subject(typing, alpha);
	assert(checked && pg_evidence_subject(checked) == alpha && alpha != subject);
	size_t proofs = typing->proofs.count, terms = typing->graph->terms.count;
	assert(pg_prove_structural_subject(typing, alpha) == checked);
	assert(typing->proofs.count == proofs && typing->graph->terms.count == terms);
}

static void reject_signature_changes(struct pg_typing *typing, const struct pg_occurrence *pi, size_t terminal_index)
{
	assert(pi->operand_count == 4);
	const struct pg_occurrence *inputs[5];
	memcpy(inputs, pi->operands, 4 * sizeof(*inputs));
	inputs[4] = inputs[0];
	for (size_t variant = 0; variant < 4; ++variant) {
		struct pg_occurrence header = *pi;
		if (variant < 2) header.operand_count = variant ? 5 : 3;
		if (variant == 2) { inputs[2] = pi->operands[3]; inputs[3] = pi->operands[2]; }
		if (variant == 3) {
			memcpy(inputs, pi->operands, 4 * sizeof(*inputs));
			struct pg_occurrence terminal = *inputs[terminal_index];
			terminal.context = inputs[3]->context;
			inputs[terminal_index] = pg_occurrence_intern(typing, &terminal,
				terminal.operand_count ? inputs[terminal_index]->operands : NULL, NULL);
			assert(inputs[terminal_index]);
		}
		const struct pg_occurrence *wrong = pg_occurrence_intern(typing, &header, inputs, NULL);
		assert(wrong && !pg_prove_structural_subject(typing, wrong));
		assert(!pg_evidence_for_subject(typing, wrong, NULL));
	}
}

static void family_result(struct pg_typing *typing, const struct pg_occurrence *subject)
{
	const struct pg_evidence *family = pg_evidence_for_subject(typing, subject->operands[0], NULL);
	const struct pg_evidence *index = pg_evidence_for_subject(typing, subject->operands[1], NULL);
	assert(family && index);
	struct pg_typed_query *query = pg_application_body_request(typing, family, index);
	while (!pg_typed_query_advance(query, 1)) {}
	assert(pg_typed_query_result(query));
	const struct pg_term *domain, *body;
	const struct pg_object *binder, *renamed = pg_binder(typing->graph);
	if (!pg_pi_view(subject->classifier, &domain, &binder, &body)) return;
	struct pg_binding_value binding = {binder, pg_reference(typing->graph, renamed)};
	const struct pg_term *alpha = pg_pi(typing->graph, domain, renamed,
		pg_term_substitute(typing->graph, body, 1, &binding));
	for (size_t variant = 0; variant < 3; ++variant) {
		struct pg_occurrence header = *subject;
		header.classifier = variant == 1 ? pg_pi(typing->graph, domain, renamed, pg_universe(typing->graph, 17))
			: variant == 2 ? pg_pi(typing->graph, domain, renamed, pg_reference(typing->graph, pg_binder(typing->graph))) : alpha;
		const struct pg_occurrence *changed = pg_occurrence_intern(typing, &header, subject->operands, NULL);
		assert(changed && changed != subject);
		const struct pg_evidence *checked = pg_prove_structural_subject(typing, changed);
		if (variant) assert(!checked && !pg_evidence_for_subject(typing, changed, NULL));
		else {
			assert(checked && pg_evidence_subject(checked) == changed);
			const struct pg_evidence *canonical = pg_prove_family_application(typing, family, index);
			assert(canonical && pg_evidence_subject(canonical) != changed);
			size_t proofs = typing->proofs.count;
			assert(pg_prove_structural_subject(typing, changed) == checked);
			assert(pg_prove_family_application(typing, family, index) == canonical);
			assert(typing->proofs.count == proofs);
		}
	}
}

static void read_inputs(FILE *file, size_t root)
{
	struct pg_graph graph;
	struct pg_typing typing;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	size_t count;
	const struct pg_occurrence *const *roots;
	rewind(file);
	assert(!pg_occurrences_read(file, &typing, 10000, 256, resolve, &graph, &count, &roots));
	assert(count == 28 && !typing.proofs.count);
	const struct pg_evidence *checked = root == 4 || root == 27
		? pg_identity_boundary_type(&typing, roots[root])
		: pg_prove_structural_subject(&typing, roots[root]);
	if (!checked || pg_evidence_subject(checked) != roots[root])
		fprintf(stderr, "typed-only root %zu failed structural checking\n", root);
	assert(checked && pg_evidence_subject(checked) == roots[root]);
	size_t proofs = typing.proofs.count, terms = graph.terms.count;
	assert(pg_prove_structural_subject(&typing, roots[root]) == checked);
	assert(typing.proofs.count == proofs && graph.terms.count == terms);
	reject_changes(&typing, roots[root]);
	if (root == 4) {
		struct pg_identity_boundary boundary;
		assert(pg_identity_boundary_view(roots[root], &boundary));
		assert(boundary.left == boundary.right && boundary.left == roots[0]);
	}
	if (root == 1) result_allocation(&typing, roots[1]->operands[0]);
	if (root == 9) reject_signature_changes(&typing, roots[9]->type, 0);
	if (root == 15) reject_signature_changes(&typing, roots[15], 1);
	if (root == 13 || root == 14) family_result(&typing, roots[root]);
	if (root == 7 || root == 8) unary_input(&typing, checked);
	if (root >= 18 && root <= 21) selected_formation(&typing, roots[root]);
	if (root == 21) {
		const struct pg_evidence *domain = pg_prove_pi_domain(&typing, checked);
		assert(domain && pg_evidence_subject(domain) == roots[0]->type->operands[0]);
		assert(pg_evidence_classifier(domain) != pg_evidence_classifier(checked));
	}
	if (root == 5) {
		const struct pg_evidence *other = pg_prove_structural_subject(&typing, roots[6]);
		assert(other && other != checked && pg_evidence_subject(other) == roots[6]);
		assert(roots[5] != roots[6] && roots[5]->core == roots[6]->core);
		assert(roots[5]->classifier != roots[6]->classifier);
	}
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

static void context_inputs(struct pg_typing *typing, const struct pg_evidence *context)
{
	struct pg_typing foreign;
	assert(!pg_typing_init(&foreign, typing->graph));
	size_t proofs = typing->proofs.count, terms = typing->graph->terms.count;
	size_t scopes = typing->scopes.count, queries = typing->typed_queries.count;
	for (;;) {
		const struct pg_scope *scope = pg_evidence_scope(context);
		struct pg_derivation_input input;
		assert(!pg_derivation_input_header(context, &input));
		assert(!pg_evidence_premise_count(context) && !pg_evidence_premise(context, 0));
		assert(input.count == (scope ? (scope->indices ? 3 : 2) : 0));
		const struct pg_evidence *child = NULL;
		assert(pg_derivation_input_dependency(typing, context, input.count, &child) == 0 && !child);
		assert(pg_derivation_input_dependency(&foreign, context, 0, &child) == -1 && !child);
		assert(!pg_context_parent_input(&foreign, context));
		assert(!pg_context_indices_input(&foreign, context));
		assert(!pg_context_declared_input(&foreign, context));
		if (!scope) break;
		const struct pg_evidence *declared = pg_context_declared_input(typing, context);
		assert(declared && pg_evidence_subject(declared) == scope->type);
		assert(pg_derivation_input_dependency(typing, context, input.count - 1, &child) == 1 && child == declared);
		const struct pg_evidence *indices = pg_context_indices_input(typing, context);
		assert(scope->indices ? indices && pg_evidence_scope(indices) == scope->indices : !indices);
		if (indices) assert(pg_derivation_input_dependency(typing, context, 1, &child) == 1 && child == indices);
		const struct pg_evidence *parent = pg_context_parent_input(typing, context);
		assert(parent && pg_evidence_scope(parent) == scope->parent);
		assert(pg_derivation_input_dependency(typing, context, 0, &child) == 1 && child == parent);
		context = parent;
	}
	assert(typing->proofs.count == proofs && typing->graph->terms.count == terms);
	assert(typing->scopes.count == scopes && typing->typed_queries.count == queries);
	assert(!foreign.proofs.count);
	pg_typing_destroy(&foreign);
}

static void write_scoped(FILE *file, struct pg_typing *typing)
{
	struct pg_graph *graph = typing->graph;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_evidence *u0 = pg_prove_universe(typing, empty, 0);
	const struct pg_object *a = pg_binder(graph), *x = pg_binder(graph);
	const struct pg_evidence *types = pg_prove_context_extension(typing, empty, a,
		pg_prove_universe(typing, empty, 1));
	const struct pg_evidence *scope = pg_prove_context_extension(typing, types, x,
		pg_prove_variable(typing, types, a));
	const struct pg_evidence *value = pg_prove_variable(typing, scope, x);
	const struct pg_evidence *body = pg_prove_return(typing, value);
	const struct pg_evidence *pi = pg_prove_pi(typing, scope, pg_prove_classifier(typing, scope, body));
	const struct pg_evidence *lambda = pg_prove_lambda(typing, pi, body);
	const struct pg_evidence *family = pg_prove_family_context_extension(typing, empty, pg_binder(graph),
		scope, pg_prove_universe(typing, scope, 0));
	const struct pg_evidence *nested = pg_prove_family_context_extension(typing, empty, pg_binder(graph),
		family, pg_prove_universe(typing, family, 0));
	const struct pg_evidence *unused = pg_prove_context_extension(typing, empty, pg_binder(graph),
		pg_prove_universe(typing, empty, 5));
	const struct pg_evidence *widened = pg_prove_return_content(typing,
		pg_prove_pi_constant_codomain(typing, pg_prove_pi(typing, unused,
			pg_prove_return_type(typing, pg_prove_universe(typing, unused, 0)))));
	const struct pg_object *b = pg_binder(graph);
	const struct pg_evidence *low = pg_prove_context_extension(typing, empty, b, u0);
	const struct pg_evidence *high = pg_prove_context_extension(typing, empty, b, widened);
	const struct pg_evidence *low_value = pg_prove_variable(typing, low, b);
	const struct pg_evidence *high_value = pg_prove_variable(typing, high, b);
	assert(pg_evidence_context(low) == pg_evidence_context(high));
	assert(pg_evidence_subject(low_value) == pg_evidence_subject(high_value));
	assert(pg_evidence_scope(low) != pg_evidence_scope(high));
	const struct pg_evidence *extended = pg_prove_context_extension(typing, scope, pg_binder(graph),
		pg_prove_universe(typing, scope, 0));
	const struct pg_evidence *deep = empty;
	for (size_t i = 0; i < 1024; ++i)
		deep = pg_prove_context_extension(typing, deep, pg_binder(graph), pg_prove_universe(typing, deep, 0));
	const struct pg_evidence *contexts[] = {scope, types, family, nested, low, high, extended, empty, deep};
	const struct pg_evidence *proofs[] = {value, lambda,
		pg_prove_variable(typing, family, pg_evidence_context(family)->binder),
		pg_prove_variable(typing, nested, pg_evidence_context(nested)->binder), low_value, high_value,
		pg_prove_projection(typing, extended, body), u0,
		pg_prove_variable(typing, deep, pg_evidence_context(deep)->binder)};
	const struct pg_scope *scopes[9];
	const struct pg_occurrence *roots[9];
	for (size_t i = 0; i < 9; ++i) {
		assert(contexts[i] && proofs[i]);
		context_inputs(typing, contexts[i]);
		scopes[i] = pg_evidence_scope(contexts[i]);
		roots[i] = pg_evidence_subject(proofs[i]);
	}
	assert(!pg_scoped_occurrences_write(file, 9, scopes, roots, name, NULL));
}

static void read_scoped(FILE *file, size_t root)
{
	struct pg_graph graph;
	struct pg_typing typing;
	assert(!pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
	size_t count = SIZE_MAX;
	const struct pg_occurrence *const *roots = NULL;
	const struct pg_scope *const *scopes = NULL;
	rewind(file);
	assert(pg_scoped_occurrences_read(file, &typing, 1, 256, resolve, &graph, &count, &scopes, &roots));
	assert(count == SIZE_MAX && !scopes && !roots && !typing.proofs.count);
	rewind(file);
	assert(pg_occurrences_read(file, &typing, 50000, 256, resolve, &graph, &count, &roots));
	assert(count == SIZE_MAX && !roots);
	rewind(file);
	assert(!pg_scoped_occurrences_read(file, &typing, 50000, 256, resolve, &graph, &count, &scopes, &roots));
	assert(count == 9 && !typing.proofs.count);
	if (root != 7) {
		assert(!pg_prove_structural_subject(&typing, roots[root]));
		assert(!pg_evidence_for_subject(&typing, roots[root], NULL));
	}
	const struct pg_evidence *checked = pg_prove_scoped_subject(&typing, scopes[root], roots[root]);
	if (!checked) fprintf(stderr, "scoped root %zu failed\n", root);
	assert(checked && pg_evidence_subject(checked) == roots[root]);
	assert(pg_evidence_scope(pg_prove_scope(&typing, scopes[root])) == scopes[root]);
	context_inputs(&typing, pg_evidence_for_scope(&typing, scopes[root]));
	size_t proofs = typing.proofs.count, terms = graph.terms.count, inputs = typing.scopes.count;
	assert(pg_prove_scoped_subject(&typing, scopes[root], roots[root]) == checked);
	assert(typing.proofs.count == proofs && graph.terms.count == terms && typing.scopes.count == inputs);
	assert(!pg_prove_scoped_subject(&typing, scopes[root ? 0 : 1], roots[root]));
	if (root == 4 || root == 5) {
		assert(scopes[4] != scopes[5] && scopes[4]->context == scopes[5]->context);
		assert(roots[4] == roots[5]);
		for (size_t i = 4; i < 6; ++i) {
			const struct pg_evidence *scope = pg_prove_scope(&typing, scopes[i]);
			assert(scope && pg_evidence_scope(scope) == scopes[i]);
			const struct pg_evidence *pi = pg_prove_pi(&typing, scope,
				pg_prove_return_type(&typing, pg_prove_universe(&typing, scope, 0)));
			assert(pi && pg_evidence_classifier(pi) == scopes[i]->type->classifier);
		}
	}
	/* A declaration graph can describe a false formation; it is not accepted.
	 * Keep the Core/raw Context while changing only its claimed Universe. */
	if (scopes[root]) {
		const struct pg_scope *input = scopes[root];
		struct pg_occurrence header = *input->type;
		header.classifier = pg_universe(&graph, 9);
		const struct pg_occurrence *bad_type = pg_occurrence_intern(&typing, &header,
			input->type->operands, pg_occurrence_maps(input->type));
		assert(bad_type);
		const struct pg_scope *bad = pg_scope_intern(&typing, input->context, input->parent, input->indices, bad_type);
		assert(bad && !pg_prove_scope(&typing, bad));
		assert(!pg_evidence_for_scope(&typing, bad));
		assert(!pg_prove_scoped_subject(&typing, bad, roots[root]));
		if (input->indices) {
			const struct pg_evidence *other_terminal = pg_prove_universe(&typing,
				pg_prove_scope(&typing, input->indices), 1);
			bad = pg_scope_intern(&typing, input->context, input->parent, input->indices,
				pg_evidence_subject(other_terminal));
			assert(bad && !pg_prove_scope(&typing, bad));
			assert(!pg_evidence_for_scope(&typing, bad));
		}
	}
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	if (!strcmp(argv[1], "write") || !strcmp(argv[1], "scoped-write")) {
		FILE *file = fopen(argv[2], "wb");
		struct pg_graph graph;
		struct pg_typing typing;
		assert(file && !pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
		if (!strcmp(argv[1], "write")) write_inputs(file, &typing);
		else write_scoped(file, &typing);
		pg_typing_destroy(&typing);
		pg_graph_destroy(&graph);
		assert(!fclose(file));
	} else {
		assert(!strcmp(argv[1], "read") || !strcmp(argv[1], "scoped-read"));
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		if (!strcmp(argv[1], "read")) {
			for (size_t root = 0; root < 28; ++root) read_inputs(file, root);
			puts("typed-only images: dependent Lambda/APP, logical families, selected formations, F/U, context action and Identity boundary checked without old evidence");
		} else {
			for (size_t root = 0; root < 9; ++root) read_scoped(file, root);
			puts("scoped images: open variables/functions/families, selected bounds, projection and 1024 declarations checked without old evidence");
		}
		assert(!fclose(file));
	}
	return 0;
}
