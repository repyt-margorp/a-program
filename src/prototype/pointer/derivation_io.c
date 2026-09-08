#include "derivation_io.h"
#include "dag.h"
#include "graph_io.h"
#include "wire.h"

#include <string.h>

static const char magic[8] = {'A', 'P', 'G', 'D', 'R', 'V', 0, 2};

static int premise(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_evidence *proof = key;
	if (index == pg_evidence_premise_count(proof)) return 0;
	*child = pg_evidence_premise(proof, index);
	return 1;
}

static int term_reference(FILE *file, const struct pg_term *term,
	struct pg_dag *terms)
{
	if (!term) return pg_wire_write_u64(file, 0);
	if (pg_dag_add(terms, term)) return -1;
	return pg_wire_write_u64(file, pg_dag_find(terms, term)->id);
}

int pg_derivations_write(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	if (!file || (count && !roots)) return -1;
	struct pg_dag dag = {0};
	struct pg_dag term_roots = {0};
	struct pg_graph arena = {0};
	int status = -1;
	if (pg_dag_init(&dag, premise, NULL) || pg_dag_init(&term_roots, NULL, NULL)
		|| pg_graph_init(&arena)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&dag, roots[i])) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		const struct pg_evidence *proof = node->key;
		struct pg_derivation_parameters parameters;
		if (pg_derivation_parameters(proof, &parameters)) goto done;
		/* Operation descriptors do not yet have a relocatable image section. */
		if (parameters.operation) goto done;
		if (pg_wire_write_u64(file, pg_evidence_rule(proof))
			|| pg_wire_write_u64(file, parameters.level) || pg_wire_write_u64(file, parameters.direction)) goto done;
		if (pg_wire_write_u64(file, parameters.reduction ? pg_reduction_kind(parameters.reduction) : PG_REDUCTION_WHNF)) goto done;
		const struct pg_term *binder = parameters.binder ? pg_reference(&arena, parameters.binder) : NULL;
		if (parameters.binder && !binder) goto done;
		const struct pg_term *effects = parameters.effects ? pg_effect_reference(&arena, parameters.effects) : NULL;
		if (parameters.effects && !effects) goto done;
		const struct pg_term *source = NULL, *target = NULL;
		if (parameters.conversion) {
			source = pg_conversion_left(parameters.conversion);
			target = pg_conversion_right(parameters.conversion);
		}
		if (parameters.reduction) {
			source = pg_reduction_source(parameters.reduction);
			target = pg_reduction_target(parameters.reduction);
		}
		if (term_reference(file, binder, &term_roots) || term_reference(file, effects, &term_roots)
			|| term_reference(file, source, &term_roots)
			|| term_reference(file, target, &term_roots)) goto done;
		size_t arity = pg_evidence_premise_count(proof);
		if (pg_wire_write_u64(file, arity)) goto done;
		for (size_t i = 0; i < arity; ++i) {
			const struct pg_dag_node *p = pg_dag_find(&dag, pg_evidence_premise(proof, i));
			if (!p || pg_wire_write_u64(file, p->id)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *node = pg_dag_find(&dag, roots[i]);
		if (!node || pg_wire_write_u64(file, node->id)) goto done;
	}
	if (term_roots.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_term **terms = pg_alloc(&arena, term_roots.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = term_roots.first; node; node = node->next)
		terms[node->id - 1] = node->key;
	status = pg_graph_write(file, term_roots.count, terms, name, owner);
done:
	pg_graph_destroy(&arena);
	pg_dag_destroy(&term_roots);
	pg_dag_destroy(&dag);
	return status;
}

struct input_record {
	struct pg_derivation_input *input;
	uint64_t binder, effects, source, target;
};

int pg_derivations_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_derivation_input *const **roots)
{
	if (!file || !graph || !graph->terms.capacity || !count || !roots) return -1;
	char header[8];
	uint64_t n, nr;
	if (fread(header, 1, 8, file) != 8 || memcmp(header, magic, 8)) return -1;
	if (pg_wire_read_u64(file, &n) || pg_wire_read_u64(file, &nr)) return -1;
	if (n > limit || nr > limit - n || limit > SIZE_MAX / sizeof(struct input_record)) return -1;
	struct input_record *records = pg_alloc(graph, (size_t)n * sizeof(*records));
	const struct pg_derivation_input **result = pg_alloc(graph, (size_t)nr * sizeof(*result));
	if (!records || !result) return -1;
	size_t available = limit - (size_t)n - (size_t)nr;
	for (size_t i = 0; i < n; ++i) {
		uint64_t rule, level, direction, arity, reduction_kind;
		if (pg_wire_read_u64(file, &rule)) return -1;
		if (rule > PG_IDENTITY_LIFT && rule != PG_EFFECT_SUBSUMPTION) return -1;
		if (pg_wire_read_u64(file, &level) || pg_wire_read_u64(file, &direction) || direction > PG_IDENTITY_LEFT) return -1;
		if (pg_wire_read_u64(file, &reduction_kind) || reduction_kind > PG_REDUCTION_NF) return -1;
		if (pg_wire_read_u64(file, &records[i].binder) || pg_wire_read_u64(file, &records[i].effects)
			|| pg_wire_read_u64(file, &records[i].source)
			|| pg_wire_read_u64(file, &records[i].target) || pg_wire_read_u64(file, &arity)) return -1;
		if (arity > available || arity > (SIZE_MAX - sizeof(struct pg_derivation_input)) / sizeof(void *)) return -1;
		available -= (size_t)arity;
		struct pg_derivation_input *input = pg_alloc(graph, sizeof(*input) + (size_t)arity * sizeof(*input->premises));
		if (!input) return -1;
		records[i].input = input;
		input->rule = (enum pg_evidence_rule)rule;
		input->parameters.level = level;
		input->parameters.direction = (enum pg_identity_direction)direction;
		input->reduction_kind = (enum pg_reduction_kind)reduction_kind;
		input->count = (size_t)arity;
		for (size_t j = 0; j < arity; ++j) {
			uint64_t id;
			if (pg_wire_read_u64(file, &id) || !id || id > i) return -1;
			input->premises[j] = records[id - 1].input;
		}
	}
	for (size_t i = 0; i < nr; ++i) {
		uint64_t id;
		if (pg_wire_read_u64(file, &id) || !id || id > n) return -1;
		result[i] = records[id - 1].input;
	}
	size_t term_count;
	const struct pg_term *const *terms;
	if (pg_graph_read(file, graph, limit, name_limit, resolve, owner, &term_count, &terms)) return -1;
	for (size_t i = 0; i < n; ++i) {
		const struct input_record *r = &records[i];
		if (r->binder > term_count || r->effects > term_count || r->source > term_count || r->target > term_count) return -1;
		if (r->input->rule == PG_RETURN_TYPE_FORM) {
			if (!r->effects) return -1;
			r->input->parameters.effects = pg_effect_row_view(terms[r->effects - 1]);
			if (!r->input->parameters.effects) return -1;
		} else if (r->effects) return -1;
		if (r->binder) {
			const struct pg_term *binder = terms[r->binder - 1];
			if (binder->kind != PG_REFERENCE || binder->as.reference->kind != PG_BINDER) return -1;
			r->input->parameters.binder = binder->as.reference;
		}
		r->input->source = r->source ? terms[r->source - 1] : NULL;
		r->input->target = r->target ? terms[r->target - 1] : NULL;
		switch (r->input->rule) {
		case PG_TYPE_CONVERSION: case PG_PURE_NORMALIZATION:
			if (!r->source || !r->target) return -1;
			break;
		default:
			if (r->source || r->target) return -1;
			break;
		}
	}
	*count = (size_t)nr;
	*roots = result;
	return 0;
}
