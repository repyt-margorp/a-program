#include "derivation_io.h"
#include "dag.h"
#include "graph_io.h"
#include "wire.h"
#include "effect_inference.h"
#include "iadt.h"
#include "context_payload.h"
#include "host.h"

#include <string.h>

static const char magic[8] = {'A', 'P', 'G', 'D', 'R', 'V', 0, 14};

static int premise(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_evidence *proof = key;
	if (index == pg_evidence_premise_count(proof)) return 0;
	*child = pg_evidence_premise(proof, index);
	return 1;
}

static int input_premise(void *unused, const void *key, size_t index, const void **child)
{
	(void)unused;
	const struct pg_derivation_input *input = key;
	if (index == input->count) return 0;
	*child = input->premises[index];
	return 1;
}

static int term_reference(FILE *file, const struct pg_term *term,
	struct pg_dag *terms)
{
	if (!term) return pg_wire_write_u64(file, 0);
	if (pg_dag_add(terms, term)) return -1;
	return pg_wire_write_u64(file, pg_dag_find(terms, term)->id);
}

int pg_derivation_input_terms(struct pg_graph *scratch,
	const struct pg_derivation_input *input,
	struct pg_derivation_payload *payload)
{
	if (!scratch || !input || !payload) return -1;
	const struct pg_derivation_parameters *p = &input->parameters;
	struct pg_derivation_payload result = {.count = PG_DERIVATION_TERM_SLOTS};
	const struct pg_term *const *extra = NULL;
	size_t extra_count = 0;
	if (p->induction) {
		if (input->rule != PG_INDUCTION_ELIM) return -1;
		const struct pg_induction_allocation *a = p->induction;
		const struct pg_term *binders[] = {pg_reference(scratch, a->recursion),
			pg_reference(scratch, a->argument), pg_reference(scratch, a->self)};
		if (pg_contexts_pack(scratch, a->count, a->clauses, 3, binders,
			&result.metadata_count, &result.metadata, &extra_count, &extra)) return -1;
	}
	if (extra_count > SIZE_MAX / sizeof(void *) - result.count) return -1;
	result.count += extra_count;
	const struct pg_term **terms = pg_alloc(scratch, result.count * sizeof(*terms));
	if (!terms) return -1;
	const struct pg_object *objects[PG_DERIVATION_TERM_SLOTS] = {
		p->binder, input->effect_parameter, NULL, NULL, p->operation_label, NULL,
		p->declaration ? pg_data_declaration_family(p->declaration) : NULL, p->constructor, p->constant
	};
	for (size_t i = 0; i < PG_DERIVATION_TERM_SLOTS; ++i) {
		terms[i] = objects[i] ? pg_reference(scratch, objects[i]) : NULL;
		if (objects[i] && !terms[i]) return -1;
	}
	if (!input->effect_parameter && p->effects) {
		terms[1] = pg_effect_reference(scratch, p->effects);
		if (!terms[1]) return -1;
	}
	terms[2] = input->source; terms[3] = input->target;
	terms[5] = pg_handler_signature_reference(scratch, p->handler);
	if (p->handler && !terms[5]) return -1;
	for (size_t i = 0; i < extra_count; ++i) terms[PG_DERIVATION_TERM_SLOTS + i] = extra[i];
	/* A reduction target may reference the same binder as a parameter wrapper.
	 * Canonicalize those wrappers in the payload arena, as image_roots_write does. */
	for (size_t i = 0; i < result.count; ++i) {
		if (!terms[i] || terms[i]->kind != PG_REFERENCE) continue;
		terms[i] = pg_reference(scratch, terms[i]->as.reference);
		if (!terms[i]) return -1;
	}
	result.terms = terms;
	*payload = result;
	return 0;
}

int pg_derivation_inputs_collect_objects(struct pg_dag *objects, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!objects || (count && !roots)) return -1;
	struct pg_dag inputs = {0}, terms = {0};
	struct pg_graph scratch = {0};
	int status = -1;
	if (pg_dag_init(&inputs, input_premise, NULL) || pg_dag_init(&terms, NULL, NULL)
		|| pg_graph_init(&scratch)) goto done;
	for (size_t i = 0; i < count; ++i) if (pg_dag_add(&inputs, roots[i])) goto done;
	for (const struct pg_dag_node *node = inputs.first; node; node = node->next) {
		struct pg_derivation_payload payload;
		if (pg_derivation_input_terms(&scratch, node->key, &payload)) goto done;
		for (size_t i = 0; i < payload.count; ++i)
			if (payload.terms[i] && pg_dag_add(&terms, payload.terms[i])) goto done;
	}
	size_t equations, effect_count;
	const struct pg_term *const *effect_roots;
	if (work) {
		if (pg_effect_inference_pack(work, &scratch, &equations, &effect_count, &effect_roots)) goto done;
		for (size_t i = 0; i < effect_count; ++i) if (pg_dag_add(&terms, effect_roots[i])) goto done;
	}
	if (terms.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_term **term_roots = pg_alloc(&scratch, terms.count * sizeof(*term_roots));
	if (!term_roots) goto done;
	for (const struct pg_dag_node *node = terms.first; node; node = node->next) term_roots[node->id - 1] = node->key;
	status = pg_graph_collect_objects(objects, terms.count, term_roots, codec, owner);
done:
	pg_graph_destroy(&scratch);
	pg_dag_destroy(&terms); pg_dag_destroy(&inputs);
	return status;
}

int pg_derivations_write(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner)
{
	const struct pg_graph_codec codec = {.name = name};
	return pg_derivations_write_descriptors(file, count, roots, &codec, owner);
}

static int write_dag(FILE *file, size_t count, const struct pg_evidence *const *proofs,
	const struct pg_derivation_input *const *inputs, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner)
{
	if (!file || (count && !proofs && !inputs)) return -1;
	struct pg_dag dag = {0};
	struct pg_dag term_roots = {0};
	struct pg_graph arena = {0};
	int status = -1;
	if (pg_dag_init(&dag, inputs ? input_premise : premise, NULL) || pg_dag_init(&term_roots, NULL, NULL)
		|| pg_graph_init(&arena)) goto done;
	for (size_t i = 0; i < count; ++i)
		if (pg_dag_add(&dag, inputs ? (const void *)inputs[i] : proofs[i])) goto done;
	if (fwrite(magic, 1, 8, file) != 8 || pg_wire_write_u64(file, dag.count) || pg_wire_write_u64(file, count)) goto done;
	for (const struct pg_dag_node *node = dag.first; node; node = node->next) {
		struct pg_derivation_input input = {0};
		if (inputs) {
			input = *(const struct pg_derivation_input *)node->key;
			if (input.parameters.conversion || input.parameters.reduction) goto done;
			if (input.effect_parameter) {
				if (input.rule != PG_RETURN_TYPE_FORM || input.parameters.effects) goto done;
				if (!pg_effect_equation_find(work, input.effect_parameter)) goto done;
			}
		} else if (pg_derivation_input_header(node->key, &input)) goto done;
		struct pg_derivation_parameters parameters = input.parameters;
		if (pg_wire_write_u64(file, input.rule)
			|| pg_wire_write_u64(file, parameters.level) || pg_wire_write_u64(file, parameters.direction)) goto done;
		if (pg_wire_write_u64(file, parameters.totality)) goto done;
		if (pg_wire_write_u64(file, input.reduction_kind)) goto done;
		struct pg_derivation_payload payload;
		if (pg_derivation_input_terms(&arena, &input, &payload)) goto done;
		for (size_t i = 0; i < PG_DERIVATION_TERM_SLOTS; ++i)
			if (term_reference(file, payload.terms[i], &term_roots)) goto done;
		if (pg_wire_write_u64(file, payload.metadata_count)
			|| pg_wire_write_u64(file, payload.count - PG_DERIVATION_TERM_SLOTS)) goto done;
		for (size_t i = 0; i < payload.metadata_count; ++i)
			if (pg_wire_write_u64(file, payload.metadata[i])) goto done;
		for (size_t i = PG_DERIVATION_TERM_SLOTS; i < payload.count; ++i)
			if (term_reference(file, payload.terms[i], &term_roots)) goto done;
		size_t arity = input.count;
		if (pg_wire_write_u64(file, arity)) goto done;
		for (size_t i = 0; i < arity; ++i) {
			const void *child;
			if (dag.child(NULL, node->key, i, &child) != 1) goto done;
			const struct pg_dag_node *p = pg_dag_find(&dag, child);
			if (!p || pg_wire_write_u64(file, p->id)) goto done;
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_dag_node *node = pg_dag_find(&dag, inputs ? (const void *)inputs[i] : proofs[i]);
		if (!node || pg_wire_write_u64(file, node->id)) goto done;
	}
	size_t equations = 0, effect_count = 0;
	const struct pg_term *const *effect_roots = NULL;
	if (work && pg_effect_inference_pack(work, &arena, &equations, &effect_count, &effect_roots)) goto done;
	if (pg_wire_write_u64(file, equations) || pg_wire_write_u64(file, effect_count)) goto done;
	for (size_t i = 0; i < effect_count; ++i)
		if (term_reference(file, effect_roots[i], &term_roots)) goto done;
	if (term_roots.count > SIZE_MAX / sizeof(void *)) goto done;
	const struct pg_term **terms = pg_alloc(&arena, term_roots.count * sizeof(*terms));
	if (!terms) goto done;
	for (const struct pg_dag_node *node = term_roots.first; node; node = node->next)
		terms[node->id - 1] = node->key;
	status = pg_graph_write_descriptors(file, term_roots.count, terms, codec, owner);
done:
	pg_graph_destroy(&arena);
	pg_dag_destroy(&term_roots);
	pg_dag_destroy(&dag);
	return status;
}

int pg_derivations_write_descriptors(FILE *file, size_t count, const struct pg_evidence *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	return write_dag(file, count, roots, NULL, NULL, codec, owner);
}

int pg_derivation_inputs_write(FILE *file, size_t count, const struct pg_derivation_input *const *roots,
	const struct pg_graph_codec *codec, void *owner)
{
	return pg_derivation_inputs_write_inference(file, count, roots, NULL, codec, owner);
}

int pg_derivation_inputs_write_inference(FILE *file, size_t count,
	const struct pg_derivation_input *const *roots, const struct pg_effect_inference *work,
	const struct pg_graph_codec *codec, void *owner)
{
	return write_dag(file, count, NULL, roots, work, codec, owner);
}

struct input_record {
	struct pg_derivation_input *input;
	uint64_t binder, effects, source, target, operation, handler, declaration, constructor, constant;
	size_t metadata_count, allocation_count;
	uint64_t *metadata, *allocation;
};

int pg_derivations_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_derivation_input *const **roots)
{
	const struct pg_graph_codec codec = {.resolve = resolve};
	return pg_derivations_read_descriptors(file, typing, limit, name_limit, &codec, owner, count, roots);
}

static int read_dag(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *work, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots)
{
	if (!typing || !typing->contexts.capacity) return -1;
	struct pg_graph *graph = typing->graph;
	if (!file || !graph || !graph->terms.capacity || !count || !roots) return -1;
	if (work && (work->rows != graph || work->sealed || work->failed || work->row_sources.count)) return -1;
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
		uint64_t rule, level, direction, totality, arity, reduction_kind;
		if (pg_wire_read_u64(file, &rule)) return -1;
		if (rule > PG_HOST_FUNCTION_INTRO) return -1;
		if (pg_wire_read_u64(file, &level) || pg_wire_read_u64(file, &direction) || direction > PG_IDENTITY_LEFT) return -1;
		if (pg_wire_read_u64(file, &totality) || totality > PG_TOTALITY_TOTAL) return -1;
		if (totality && rule != PG_RETURN_TYPE_FORM && rule != PG_RETURN_INTRO) return -1;
		if (pg_wire_read_u64(file, &reduction_kind) || reduction_kind > PG_REDUCTION_PREFIX) return -1;
		if (pg_wire_read_u64(file, &records[i].binder) || pg_wire_read_u64(file, &records[i].effects)
			|| pg_wire_read_u64(file, &records[i].source)
			|| pg_wire_read_u64(file, &records[i].target) || pg_wire_read_u64(file, &records[i].operation)
			|| pg_wire_read_u64(file, &records[i].handler)
			|| pg_wire_read_u64(file, &records[i].declaration) || pg_wire_read_u64(file, &records[i].constructor)) return -1;
		if (pg_wire_read_u64(file, &records[i].constant)) return -1;
		uint64_t nm, na;
		if (pg_wire_read_u64(file, &nm) || pg_wire_read_u64(file, &na)) return -1;
		if (nm > available || na > available - nm) return -1;
		if ((nm || na) && rule != PG_INDUCTION_ELIM) return -1;
		available -= (size_t)(nm + na);
		records[i].metadata_count = (size_t)nm;
		records[i].allocation_count = (size_t)na;
		records[i].metadata = pg_alloc(graph, (size_t)nm * sizeof(uint64_t));
		records[i].allocation = pg_alloc(graph, (size_t)na * sizeof(uint64_t));
		if (!records[i].metadata || !records[i].allocation) return -1;
		for (size_t j = 0; j < nm; ++j) if (pg_wire_read_u64(file, &records[i].metadata[j])) return -1;
		for (size_t j = 0; j < na; ++j) if (pg_wire_read_u64(file, &records[i].allocation[j])) return -1;
		if (pg_wire_read_u64(file, &arity)) return -1;
		if (arity > available || arity > (SIZE_MAX - sizeof(struct pg_derivation_input)) / sizeof(void *)) return -1;
		available -= (size_t)arity;
		struct pg_derivation_input *input = pg_alloc(graph, sizeof(*input) + (size_t)arity * sizeof(*input->premises));
		if (!input) return -1;
		records[i].input = input;
		input->rule = (enum pg_evidence_rule)rule;
		input->parameters.level = level;
		input->parameters.totality = (enum pg_totality)totality;
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
	uint64_t equations, effect_count;
	if (pg_wire_read_u64(file, &equations) || pg_wire_read_u64(file, &effect_count)) return -1;
	if (effect_count > available || equations > effect_count / 2) return -1;
	if ((effect_count - 2 * equations) % 3 || (effect_count && !work)) return -1;
	uint64_t *effect_ids = pg_alloc(graph, (size_t)effect_count * sizeof(*effect_ids));
	const struct pg_term **effect_roots = pg_alloc(graph, (size_t)effect_count * sizeof(*effect_roots));
	if (!effect_ids || !effect_roots) return -1;
	for (size_t i = 0; i < effect_count; ++i)
		if (pg_wire_read_u64(file, &effect_ids[i])) return -1;
	size_t term_count;
	const struct pg_term *const *terms;
	if (pg_graph_read_descriptors(file, graph, limit, name_limit, codec, owner, &term_count, &terms)) return -1;
	for (size_t i = 0; i < effect_count; ++i) {
		if (!effect_ids[i] || effect_ids[i] > term_count) return -1;
		effect_roots[i] = terms[effect_ids[i] - 1];
	}
	if (work && pg_effect_inference_unpack(work, (size_t)equations, (size_t)effect_count, effect_roots)) return -1;
	for (size_t i = 0; i < n; ++i) {
		const struct input_record *r = &records[i];
		if (r->metadata_count || r->allocation_count) {
			const struct pg_term **allocation = pg_alloc(graph, r->allocation_count * sizeof(*allocation));
			struct pg_induction_allocation *a = pg_alloc(graph, sizeof(*a));
			if (!allocation || !a) return -1;
			for (size_t j = 0; j < r->allocation_count; ++j) {
				uint64_t id = r->allocation[j];
				if (!id || id > term_count) return -1;
				allocation[j] = terms[id - 1];
			}
			size_t nb;
			const struct pg_term *const *binders;
			if (pg_contexts_unpack(typing, r->metadata_count, r->metadata,
				r->allocation_count, allocation, &a->count, &a->clauses, &nb, &binders)) return -1;
			if (nb != 3) return -1;
			for (size_t j = 0; j < nb; ++j)
				if (binders[j]->kind != PG_REFERENCE || binders[j]->as.reference->kind != PG_BINDER) return -1;
			a->recursion = binders[0]->as.reference;
			a->argument = binders[1]->as.reference;
			a->self = binders[2]->as.reference;
			r->input->parameters.induction = a;
		}
		if (r->binder > term_count || r->effects > term_count || r->source > term_count || r->target > term_count) return -1;
		if (r->operation > term_count || r->handler > term_count) return -1;
		if (r->declaration > term_count || r->constructor > term_count) return -1;
		if (r->constant > term_count) return -1;
		if (r->input->rule == PG_HOST_TYPE_FORM || r->input->rule == PG_HOST_VALUE_INTRO
			|| r->input->rule == PG_HOST_FUNCTION_INTRO) {
			if (!r->constant || terms[r->constant - 1]->kind != PG_REFERENCE) return -1;
			r->input->parameters.constant = terms[r->constant - 1]->as.reference;
		} else if (r->constant) return -1;
		if (r->input->rule == PG_INDUCTIVE_FORM) {
			if (!r->declaration || terms[r->declaration - 1]->kind != PG_REFERENCE) return -1;
			r->input->parameters.declaration = pg_data_declaration_view(terms[r->declaration - 1]->as.reference);
			if (!r->input->parameters.declaration) return -1;
		} else if (r->declaration) return -1;
		if (r->input->rule == PG_CONSTRUCTOR_INTRO) {
			if (!r->constructor || terms[r->constructor - 1]->kind != PG_REFERENCE) return -1;
			r->input->parameters.constructor = terms[r->constructor - 1]->as.reference;
			const struct pg_data_layout *layout;
			size_t index, arity;
			if (!pg_data_constructor_view(r->input->parameters.constructor, &layout, &index, &arity)) return -1;
		} else if (r->constructor) return -1;
		if (r->input->rule == PG_REQUEST_INTRO) {
			if (!r->operation || terms[r->operation - 1]->kind != PG_REFERENCE) return -1;
			const struct pg_term *payload, *response;
			r->input->parameters.operation_label = terms[r->operation - 1]->as.reference;
			if (!pg_operation_label_types(r->input->parameters.operation_label, &payload, &response)) return -1;
		} else if (r->operation) return -1;
		if (r->input->rule == PG_HANDLER_ELIM) {
			if (!r->handler) return -1;
			r->input->parameters.handler = pg_handler_signature_view(terms[r->handler - 1]);
			if (!r->input->parameters.handler) return -1;
		} else if (r->handler) return -1;
		if (r->input->rule == PG_RETURN_TYPE_FORM) {
			if (!r->effects) return -1;
			const struct pg_term *row = terms[r->effects - 1];
			r->input->parameters.effects = pg_effect_row_view(row);
			if (!r->input->parameters.effects) {
				if (row->kind != PG_REFERENCE || !pg_effect_equation_find(work, row->as.reference)) return -1;
				r->input->effect_parameter = row->as.reference;
			}
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

int pg_derivations_read_descriptors(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots)
{
	return pg_derivations_read_inference(file, typing, limit, name_limit, NULL, codec, owner, count, roots);
}

int pg_derivations_read_inference(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	struct pg_effect_inference *work, const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_derivation_input *const **roots)
{
	int status = read_dag(file, typing, limit, name_limit, work, codec, owner, count, roots);
	if (status && work) work->failed = 1;
	return status;
}
