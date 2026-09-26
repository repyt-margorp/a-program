#include "occurrence_io.h"
#include "evidence.h"
#include "computation.h"
#include "action.h"

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
	const struct pg_occurrence *roots[] = {pg_evidence_subject(function), pg_evidence_subject(application),
		pg_evidence_subject(forced), pg_evidence_subject(mapped), pg_evidence_subject(identity),
		pg_evidence_subject(shared[0]), pg_evidence_subject(shared[1]),
		pg_evidence_subject(returned), pg_evidence_subject(quoted),
		pg_evidence_subject(family), pg_evidence_subject(nested), pg_evidence_subject(family_map),
		pg_evidence_subject(logical), pg_evidence_subject(logical_partial), pg_evidence_subject(logical_applied),
		pg_evidence_subject(logical_family), pg_evidence_subject(logical_nested), pg_evidence_subject(logical_map)};
	assert(!pg_occurrences_write(file, 18, roots, name, NULL));
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
	assert(wrong && !pg_prove_structural_subject(typing, wrong));
	assert(!pg_evidence_for_subject(typing, wrong, NULL));
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
	assert(count == 18 && !typing.proofs.count);
	const struct pg_evidence *checked = root == 4
		? pg_identity_boundary_type(&typing, roots[root])
		: pg_prove_structural_subject(&typing, roots[root]);
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
	if (root == 5) {
		const struct pg_evidence *other = pg_prove_structural_subject(&typing, roots[6]);
		assert(other && other != checked && pg_evidence_subject(other) == roots[6]);
		assert(roots[5] != roots[6] && roots[5]->core == roots[6]->core);
		assert(roots[5]->classifier != roots[6]->classifier);
	}
	pg_typing_destroy(&typing);
	pg_graph_destroy(&graph);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	if (!strcmp(argv[1], "write")) {
		FILE *file = fopen(argv[2], "wb");
		struct pg_graph graph;
		struct pg_typing typing;
		assert(file && !pg_graph_init(&graph) && !pg_typing_init(&typing, &graph));
		write_inputs(file, &typing);
		pg_typing_destroy(&typing);
		pg_graph_destroy(&graph);
		assert(!fclose(file));
	} else {
		assert(!strcmp(argv[1], "read"));
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		for (size_t root = 0; root < 18; ++root) read_inputs(file, root);
		assert(!fclose(file));
		puts("typed-only images: dependent Lambda/APP, logical families, family signatures, F/U, context action and Identity boundary checked without old evidence");
	}
	return 0;
}
