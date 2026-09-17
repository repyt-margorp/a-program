#include "evidence.h"
#include "computation.h"
#include "eval.h"
#include "identity.h"
#include "iadt.h"
#include "host.h"
#include "dag.h"
#include <stdlib.h>
#include <string.h>

struct pg_evidence {
	struct pg_index_entry index;
	struct pg_evidence *next_conclusion;
	const void *owner;
	enum pg_evidence_rule rule;
	union {
		const struct pg_occurrence *subject;
		const struct pg_context *context;
		const struct pg_context_map *map;
	} conclusion;
	const void *certificate;
	size_t premise_count;
	const struct pg_evidence *premises[];
};

static const struct pg_evidence *formed_classifier(struct pg_typing *typing,
	const struct pg_evidence *term);

struct evidence_conclusion {
	struct pg_index_entry index;
	enum pg_evidence_judgement judgement;
	const void *key;
	struct pg_evidence *first, *last;
};

static struct evidence_conclusion *conclusion_find(const struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const void *key)
{
	uint64_t hash = ((uintptr_t)key ^ judgement) * UINT64_C(1099511628211);
	if (!typing || !typing->evidence_conclusions.capacity) return NULL;
	for (struct pg_index_entry *p = pg_index_candidates(&typing->evidence_conclusions, hash); p; p = p->next) {
		struct evidence_conclusion *entry = (void *)p;
		if (p->hash == hash && entry->judgement == judgement && entry->key == key) return entry;
	}
	return NULL;
}

static struct evidence_conclusion *conclusion_prepare(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const void *key)
{
	struct evidence_conclusion *entry = conclusion_find(typing, judgement, key);
	if (entry) return entry;
	entry = pg_alloc(typing->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->judgement = judgement;
	entry->key = key;
	uint64_t hash = ((uintptr_t)key ^ judgement) * UINT64_C(1099511628211);
	return pg_index_insert(&typing->evidence_conclusions, &entry->index, hash) ? NULL : entry;
}

const struct pg_evidence *pg_evidence_for_subject(const struct pg_typing *typing,
	const struct pg_occurrence *subject, const struct pg_evidence *after)
{
	if (!subject) return NULL;
	if (after) {
		if (!pg_evidence_owned_by(after, typing) || pg_evidence_subject(after) != subject) return NULL;
		return after->next_conclusion;
	}
	const struct evidence_conclusion *entry = conclusion_find(typing, subject->judgement, subject);
	return entry ? entry->first : NULL;
}

struct pg_operation_declaration {
	struct pg_object_entry base;
	const struct pg_object *label;
	const struct pg_evidence *payload_type, *response_type;
};

static const struct pg_object_class operation_label_class = {"operation-label"};
static const struct pg_object_class operation_declaration_class = {"operation-declaration"};
struct operation_label {
	struct pg_object object;
	const struct pg_term *payload, *response;
};
static const struct pg_object_class handler_signature_class = {"handler-signature"};
struct pg_handler_signature {
	struct pg_object_entry base;
	size_t count;
	const struct pg_object *labels[];
};

const struct pg_handler_signature *pg_handler_signature(struct pg_graph *graph,
	size_t count, const struct pg_object *const *labels)
{
	if (!graph || !count || !labels) return NULL;
	if (count > (SIZE_MAX - sizeof(struct pg_handler_signature)) / sizeof(*labels)) return NULL;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = UINT64_C(1469598103934665603) ^ count;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_term *payload, *response;
		if (!pg_operation_label_types(labels[i], &payload, &response)) return NULL;
		hash = (hash ^ (uintptr_t)labels[i]) * UINT64_C(1099511628211);
	}
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		const struct pg_object_entry *base = (const struct pg_object_entry *)p;
		if (base->object.owner != &handler_signature_class) continue;
		const struct pg_handler_signature *signature = (const void *)base;
		if (signature->count == count && !memcmp(signature->labels, labels, count * sizeof(*labels))) return signature;
	}
	struct pg_handler_signature *signature = pg_alloc(graph, sizeof(*signature) + count * sizeof(*labels));
	if (!signature) return NULL;
	signature->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &handler_signature_class};
	signature->count = count;
	memcpy(signature->labels, labels, count * sizeof(*labels));
	return pg_index_insert(&graph->objects, &signature->base.index, hash) ? NULL : signature;
}

size_t pg_handler_signature_count(const struct pg_handler_signature *signature)
{
	return signature ? signature->count : 0;
}

const struct pg_object *pg_handler_signature_label(
	const struct pg_handler_signature *signature, size_t index)
{
	return signature && index < signature->count ? signature->labels[index] : NULL;
}

const struct pg_handler_signature *pg_evidence_handler_signature(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_HANDLER_ELIM ? evidence->certificate : NULL;
}

const struct pg_term *pg_handler_signature_reference(struct pg_graph *graph, const struct pg_handler_signature *signature)
{
	return signature ? pg_reference(graph, &signature->base.object) : NULL;
}

const struct pg_handler_signature *pg_handler_signature_view(const struct pg_term *term)
{
	if (!term || term->kind != PG_REFERENCE || term->as.reference->owner != &handler_signature_class) return NULL;
	return (const void *)((const char *)term->as.reference - offsetof(struct pg_object_entry, object));
}

static int derived_output(enum pg_evidence_rule rule)
{
	switch (rule) {
	case PG_REINDEX: case PG_CONTEXT_PROJECTION: case PG_APP_ELIM: case PG_PI_CODOMAIN:
	case PG_FOLD_ELIM: case PG_PI_CONSTANT_CODOMAIN: case PG_EFFECT_SUBSUMPTION: case PG_REQUEST_INTRO:
	case PG_FAMILY_IDENTITY_FORM: case PG_FAMILY_ACTION:
	case PG_INDUCTIVE_FORM: case PG_CONSTRUCTOR_INTRO: case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: case PG_TYPE_CASE:
		return 1;
	default: return 0;
	}
}

static const void *certificate_key(enum pg_evidence_rule rule, const void *certificate)
{
	/* Schema wrappers attest premises; they do not introduce another family. */
	return rule == PG_INDUCTIVE_FORM ? pg_data_schema_declaration(certificate) : certificate;
}

static const struct pg_evidence *find_record(struct pg_typing *typing, enum pg_evidence_rule rule,
	const struct pg_context *context, const struct pg_occurrence *subject, size_t count, const struct pg_evidence *const *premises,
	const void *certificate, uint64_t *hash_out)
{
	/* Premises and, for induction, explicit lexical allocation determine these
	 * outputs. Check before constructing scopes with temporary fresh binders. */
	const struct pg_induction_allocation *allocation = rule == PG_INDUCTION_ELIM && subject ? subject->induction : NULL;
	if (derived_output(rule)) subject = NULL;
	uint64_t hash = ((uintptr_t)context ^ (uintptr_t)subject ^ rule) * UINT64_C(1099511628211);
	if (rule == PG_INDUCTION_ELIM) hash ^= pg_induction_allocation_hash(allocation);
	const void *key = certificate_key(rule, certificate);
	hash = (hash ^ (uintptr_t)key) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	*hash_out = hash;
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->proofs, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_evidence *proof = (const struct pg_evidence *)candidate;
		if (proof->rule != rule) continue;
		if (rule == PG_INDUCTION_ELIM &&
			!pg_induction_allocation_equal(pg_evidence_subject(proof)->induction, allocation)) continue;
		if (pg_evidence_context(proof) != context) continue;
		if (!derived_output(rule)) {
			if (pg_evidence_subject(proof) != subject) continue;
		}
		if (certificate_key(rule, proof->certificate) != key) continue;
		if (proof->premise_count != count) continue;
		size_t i = 0;
		while (i < count && proof->premises[i] == premises[i]) ++i;
		if (i == count) return proof;
	}
	return NULL;
}

static const struct pg_evidence *accept_record(struct pg_typing *typing, enum pg_evidence_rule rule,
	const struct pg_context *context, const struct pg_occurrence *subject, size_t count, const struct pg_evidence *const *premises,
	const void *certificate,
	const struct pg_context_map *map)
{
	if (subject && subject->context != context) return NULL;
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, rule, context,
		subject, count, premises, certificate, &hash);
	if (existing) return existing;
	if (count > (SIZE_MAX - sizeof(struct pg_evidence)) / sizeof(*premises)) return NULL;
	/* Allocate the secondary key before publishing either index entry. An
	 * empty key after allocation failure contains no accepted derivation. */
	enum pg_evidence_judgement judgement = map ? PG_JUDGEMENT_SUBSTITUTION
		: subject ? subject->judgement : PG_JUDGEMENT_CONTEXT;
	const void *key = map ? (const void *)map : subject ? (const void *)subject : (const void *)context;
	struct evidence_conclusion *conclusion = conclusion_prepare(typing, judgement, key);
	if (!conclusion) return NULL;
	size_t size = sizeof(struct pg_evidence) + count * sizeof(*premises);
	struct pg_evidence *proof = pg_alloc(typing->graph, size);
	if (!proof) return NULL;
	proof->owner = typing->owner_key;
	proof->rule = rule;
	if (map) proof->conclusion.map = map;
	else if (subject) proof->conclusion.subject = subject;
	else proof->conclusion.context = context;
	proof->certificate = certificate;
	proof->premise_count = count;
	for (size_t i = 0; i < count; ++i) proof->premises[i] = premises[i];
	if (pg_index_insert(&typing->proofs, &proof->index, hash) != 0) return NULL;
	if (conclusion->last) conclusion->last->next_conclusion = proof;
	else conclusion->first = proof;
	conclusion->last = proof;
	return proof;
}

static const struct pg_evidence *accept_with_conversion(struct pg_typing *typing, enum pg_evidence_rule rule,
	const struct pg_context *context, const struct pg_occurrence *subject, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion)
{
	return accept_record(typing, rule, context, subject, count, premises, conversion, NULL);
}

static const struct pg_evidence *accept(struct pg_typing *typing, enum pg_evidence_rule rule,
	const struct pg_context *context, const struct pg_occurrence *subject, size_t count, const struct pg_evidence *const *premises)
{
	return accept_with_conversion(typing, rule, context, subject, count, premises, NULL);
}

static int context_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(proof, typing)) return 0;
	return pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT;
}

static const struct pg_evidence *conclusion_first(const struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const void *key)
{
	const struct evidence_conclusion *entry = conclusion_find(typing, judgement, key);
	return entry ? entry->first : NULL;
}

static const struct pg_evidence *substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *destination, const struct pg_evidence *image);

/* Only an exact structural lifting can supply an unchecked destination scope.
 * Remove the final variable image and cancel the prefix images' weakenings. */
static const struct pg_context_map *map_lift_prefix(struct pg_typing *typing,
	const struct pg_context_map *map)
{
	if (!map->source || !map->destination || !map->count) return NULL;
	const struct pg_occurrence *variable = map->images[map->count - 1];
	if (variable->core != pg_reference(typing->graph, map->destination->binder)) return NULL;
	if (variable->classifier != map->destination->declared_type) return NULL;
	if (map->count > SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_occurrence **images = malloc((map->count - 1) * sizeof(*images));
	if (map->count > 1 && !images) return NULL;
	for (size_t i = 0; i + 1 < map->count; ++i) {
		images[i] = pg_occurrence_unproject(typing, map->images[i], map->destination->parent);
		if (!images[i]) { free(images); return NULL; }
	}
	const struct pg_context_map *prefix = pg_context_map(typing,
		map->source->parent, map->destination->parent, map->count - 1, images);
	free(images);
	return prefix;
}

static struct pg_context_lift *map_lift_work(struct pg_typing *typing,
	const struct pg_context_map *map, const struct pg_context_map *prefix)
{
	/* A checked destination permits ordinary image checking. Do not require
	 * strengthening those images merely because the map has a lift shape. */
	if (conclusion_first(typing, PG_JUDGEMENT_CONTEXT, map->destination) &&
		!conclusion_first(typing, PG_JUDGEMENT_SUBSTITUTION, prefix)) return NULL;
	struct pg_context_lift *work = pg_context_lift_request(typing, prefix, map->source, map->destination->binder);
	while (pg_context_lift_advance(work, 1024) == PG_SUBSTITUTION_PENDING) {}
	return pg_context_lift_result(work) == map ? work : NULL;
}

static const struct pg_evidence *lift_destination(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *prefix, struct pg_context_lift *work)
{
	const struct pg_context_map *map = pg_context_lift_result(work);
	if (!map) return NULL;
	const struct pg_evidence *destination = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, map->destination);
	if (!destination && source->rule == PG_CONTEXT_EXTEND) {
		destination = pg_prove_context_extension(typing, prefix->premises[1], map->destination->binder,
			pg_prove_reindex(typing, prefix, source->premises[1]));
	} else if (!destination && source->rule == PG_CONTEXT_FAMILY_EXTEND) {
		const struct pg_evidence *indices = pg_prove_context_map(typing, pg_context_lift_indices(work));
		if (!indices) return NULL;
		destination = pg_prove_family_context_extension(typing, prefix->premises[1], map->destination->binder,
			indices->premises[1], pg_prove_reindex(typing, indices, source->premises[2]));
	}
	return destination && pg_evidence_context(destination) == map->destination ? destination : NULL;
}

static int map_dependency(void *owner, const void *key, size_t index, const void **child)
{
	struct pg_typing *typing = owner;
	const struct pg_context_map *map = key;
	if (conclusion_first(typing, PG_JUDGEMENT_SUBSTITUTION, map)) return 0;
	const struct pg_evidence *source = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, map->source);
	const struct pg_evidence *destination = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, map->destination);
	if (!source) return -1;
	const struct pg_context_map *prefix_map = map_lift_prefix(typing, map);
	struct pg_context_lift *work = prefix_map ? map_lift_work(typing, map, prefix_map) : NULL;
	const struct pg_evidence *accepted;
	if (work) {
		if (!index) { *child = prefix_map; return 1; }
		if (index == 1 && !destination) {
			*child = pg_context_lift_indices(work);
			if (*child) return 1;
		}
		const struct pg_evidence *prefix = conclusion_first(typing, PG_JUDGEMENT_SUBSTITUTION, prefix_map);
		if (!prefix) return -1;
		destination = lift_destination(typing, source, prefix, work);
		if (!destination) return -1;
		accepted = substitution_pair(typing, prefix, source, destination,
			pg_prove_variable(typing, destination, map->destination->binder));
	} else {
		if (!destination || map->count > SIZE_MAX / sizeof(const struct pg_evidence *)) return -1;
		const struct pg_evidence **images = malloc(map->count * sizeof(*images));
		if (map->count && !images) return -1;
		for (size_t i = 0; i < map->count; ++i) images[i] = pg_prove_structural_subject(typing, map->images[i]);
		accepted = pg_prove_substitution(typing, source, destination, map->count, images);
		free(images);
	}
	return pg_evidence_context_map(accepted) == map ? 0 : -1;
}

static int structural_dependency(void *owner, const void *key, size_t index, const void **child)
{
	struct pg_typing *typing = owner;
	const struct pg_occurrence *subject = key;
	if (pg_evidence_for_subject(typing, subject, NULL)) return 0;
	const struct pg_evidence *proof = NULL;
	if (subject->map) {
		if (!index) { *child = subject->origin; return 1; }
		/* The origin may establish the source scope of this map. Certify it
		 * before asking the ordinary map rule to check its lifted destination
		 * and images, rather than requiring those scopes during collection. */
		proof = pg_prove_reindex(typing, pg_prove_context_map(typing, subject->map),
			pg_evidence_for_subject(typing, subject->origin, NULL));
	} else if (subject->core->kind == PG_REFERENCE && subject->core->as.reference->kind == PG_BINDER) {
		const struct pg_evidence *context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, subject->context);
		proof = pg_prove_variable(typing, context, subject->core->as.reference);
	}
	return proof && pg_evidence_subject(proof) == subject ? 0 : -1;
}

/* Each callback checks its rule once dependencies finish. There is no second
 * interpretation pass over the collected graph, nor acceptance from shape. */
static const struct pg_evidence *prove_structural(struct pg_typing *typing,
	enum pg_evidence_judgement judgement, const void *key,
	int (*dependency)(void *, const void *, size_t, const void **))
{
	if (!typing || !key) return NULL;
	const struct pg_evidence *result = conclusion_first(typing, judgement, key);
	if (result) return result;
	struct pg_dag dag = {0};
	if (pg_dag_init(&dag, dependency, typing)) return NULL;
	if (!pg_dag_add(&dag, key)) result = conclusion_first(typing, judgement, key);
	pg_dag_destroy(&dag);
	return result;
}

const struct pg_evidence *pg_prove_context_map(struct pg_typing *typing,
	const struct pg_context_map *map)
{
	return prove_structural(typing, PG_JUDGEMENT_SUBSTITUTION, map, map_dependency);
}

/* Typed structure selects the construction; ordinary rules certify its maps.
 * No search by erased Core or interpretation of a receipt's history occurs. */
const struct pg_evidence *pg_prove_structural_subject(struct pg_typing *typing,
	const struct pg_occurrence *subject)
{
	return subject ? prove_structural(typing, subject->judgement, subject, structural_dependency) : NULL;
}

const struct pg_data_declaration *pg_evidence_inductive_declaration(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_INDUCTIVE_FORM ? pg_data_schema_declaration(evidence->certificate) : NULL;
}

const struct pg_object *pg_evidence_constructor(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_CONSTRUCTOR_INTRO ? evidence->certificate : NULL;
}

const struct pg_induction_allocation *pg_evidence_induction_allocation(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_INDUCTION_ELIM ? pg_evidence_subject(evidence)->induction : NULL;
}

const struct pg_evidence *pg_prove_inductive_type(struct pg_typing *typing,
	const struct pg_data_schema *schema)
{
	const struct pg_evidence *self = pg_data_schema_parameters(schema);
	if (!context_proof(typing, self)) return NULL;
	int indexed = self->rule == PG_CONTEXT_FAMILY_EXTEND;
	if (!indexed && self->rule != PG_CONTEXT_EXTEND) return NULL;
	const struct pg_evidence *indices = pg_data_schema_indices(schema);
	if (!context_proof(typing, indices)) return NULL;
	uint64_t level, bound;
	const struct pg_term *universe = indexed ? pg_evidence_subject(self->premises[2])->core : pg_evidence_context(self)->declared_type;
	if (!pg_universe_level(universe, &level)) return NULL;
	if (indexed) {
		const struct pg_term *signature = pg_context_signature(typing->graph,
			pg_evidence_context(self), pg_evidence_context(indices), universe);
		if (!signature || pg_alpha_equal(signature, pg_evidence_context(self)->declared_type) != 1) return NULL;
	} else if (pg_evidence_context(indices) != pg_evidence_context(self)) return NULL;
	const struct pg_evidence *parent = self->premises[0];
	size_t count = pg_data_constructor_count(schema), parameters;
	size_t prefix = indexed ? 2 : 1;
	if (count > SIZE_MAX / sizeof(void *) - prefix) return NULL;
	if (pg_context_extension_size(pg_evidence_context(parent), NULL, &parameters)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + prefix) * sizeof(*premises));
	if (!premises) goto done;
	premises[0] = self;
	if (indexed) premises[1] = indices;
	for (size_t i = 0; i < count; ++i)
		premises[i + prefix] = pg_data_schema_result(schema,
			pg_data_constructor(pg_data_schema_layout(schema), i));
	uint64_t hash;
	enum pg_evidence_judgement kind = indexed ? PG_JUDGEMENT_TYPE_FAMILY : PG_JUDGEMENT_VALUE_TYPE;
	result = find_record(typing, PG_INDUCTIVE_FORM,
		pg_evidence_context(parent), NULL, count + prefix, premises, schema, &hash);
	if (result) goto done;
	if (pg_data_schema_positive(schema, pg_evidence_context(self)->binder) != 1) goto done;
	if (pg_data_schema_field_level(schema, &bound) || bound > level) goto done;
	if (parameters > SIZE_MAX / sizeof(const struct pg_object *)) goto done;
	const struct pg_object **binders = pg_alloc(&temporary, parameters * sizeof(*binders));
	if (parameters && !binders) goto done;
	const struct pg_context *context = pg_evidence_context(parent);
	for (size_t i = parameters; i; --i, context = context->parent) binders[i - 1] = context->binder;
	const struct pg_term *core = pg_reference(typing->graph, pg_data_family_object(schema));
	for (size_t i = 0; i < parameters; ++i)
		core = pg_application(typing->graph, core, pg_reference(typing->graph, binders[i]));
	if (!core || !universe) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, kind, pg_evidence_context(parent), core, pg_evidence_context(self)->declared_type, NULL, 0, NULL);
	if (!subject) goto done;
	result = accept_record(typing, PG_INDUCTIVE_FORM,
		pg_evidence_context(parent), subject, count + prefix, premises, schema, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* A scope action is either a total substitution or a Pi component whose
 * independent inputs must be recovered in its smaller context. */
struct scope_frame {
	const struct pg_context_map *map;
	const struct pg_occurrence *restriction;
	const struct scope_frame *next;
};

static struct scope_frame *scope_frame(struct pg_graph *storage,
	const struct pg_context_map *map, const struct pg_occurrence *restriction,
	const struct scope_frame *next)
{
	if (!!map == !!restriction) return NULL;
	struct scope_frame *frame = pg_alloc(storage, sizeof(*frame));
	if (frame) *frame = (struct scope_frame){map, restriction, next};
	return frame;
}

struct constructor_structure {
	const struct pg_evidence *formation, *parameters;
	const struct pg_object *constructor;
	size_t count;
	const struct pg_evidence **fields;
};

/* Inspect the current typed head, without evaluating or following provenance.
 * Declaration identity and arity are checked before exposing any field. */
static int constructor_view(struct pg_typing *typing, const struct pg_evidence *value,
	struct constructor_structure *view)
{
	if (!pg_evidence_owned_by(value, typing) || pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return 0;
	const struct pg_term *head = pg_evidence_subject(value)->core;
	size_t count = 0, position, arity;
	for (; head->kind == PG_APPLICATION; head = head->as.application.function) ++count;
	const struct pg_data_layout *layout;
	if (head->kind != PG_REFERENCE || !pg_data_constructor_view(head->as.reference, &layout, &position, &arity)) return 0;
	if (count != arity) return -1;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, formed_classifier(typing, value), &instance)) return -1;
	if (layout != pg_data_schema_layout(instance.schema)) return -1;
	*view = (struct constructor_structure){.formation = instance.formation,
		.parameters = instance.parameters, .constructor = head->as.reference, .count = count};
	return 1;
}

static int structural_input(struct pg_typing *typing, const struct pg_occurrence *source,
	size_t index, const struct pg_occurrence **result);

static const struct pg_reduction_certificate *subject_normalization(struct pg_typing *typing,
	const struct pg_occurrence *subject)
{
	if (!subject->origin || subject->map || subject->selection) return NULL;
	/* A boundary conversion does not replace the original reduction receipt. */
	const struct pg_occurrence *normalized = pg_occurrence_derived(typing, subject->origin,
		subject->origin->judgement, subject->core, subject->origin->classifier);
	for (const struct pg_evidence *proof = pg_evidence_for_subject(typing, normalized, NULL);
		proof; proof = pg_evidence_for_subject(typing, normalized, proof)) {
		const struct pg_reduction_certificate *receipt = pg_evidence_normalization(proof);
		if (receipt) return receipt;
	}
	return NULL;
}

static const struct pg_evidence *image_boundary(struct pg_typing *typing,
	const struct pg_evidence *result, const struct pg_occurrence *expected)
{
	if (!result) return NULL;
	if (pg_evidence_judgement(result) != expected->judgement) {
		if (expected->judgement == PG_JUDGEMENT_VALUE_TYPE) result = pg_prove_value_type(typing, result);
		else if (expected->judgement == PG_JUDGEMENT_VALUE) result = pg_prove_type_value(typing, result);
	}
	if (!result || pg_evidence_judgement(result) != expected->judgement) return NULL;
	if (pg_alpha_equal(pg_evidence_subject(result)->core, expected->core) != 1) return NULL;
	if (pg_alpha_equal(pg_evidence_classifier(result), expected->classifier) != 1) return NULL;
	return result;
}

/* Restrict construction inputs, then check the ordinary introduction/map in
 * the target context. No inference follows a chosen receipt's wrapper chain. */
static const struct pg_evidence *rebase_image(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *image)
{
	struct pg_typed_query *work = pg_rebase_request(typing, context, image);
	while (!pg_typed_query_advance(work, UINT64_MAX)) {}
	return pg_typed_query_result(work);
}

const struct pg_evidence *pg_prove_substitution_rebase(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *map)
{
	if (!context_proof(typing, context) || !pg_evidence_owned_by(map, typing)) return NULL;
	if (map->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	size_t count = map->premise_count - 2;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	for (size_t i = 0; i < count; ++i) images[i] = rebase_image(typing, context, map->premises[i + 2]);
	const struct pg_evidence *result = pg_prove_substitution(typing, map->premises[0], context, count, images);
	free(images);
	return result;
}

static const struct pg_evidence *scope_map_step(struct pg_typing *typing,
	const struct pg_evidence *map, const struct scope_frame *frame)
{
	if (frame->map)
		return pg_prove_substitution_compose(typing, map, pg_prove_context_map(typing, frame->map));
	return pg_prove_substitution_rebase(typing,
		conclusion_first(typing, PG_JUDGEMENT_CONTEXT, frame->restriction->context), map);
}

static const struct pg_evidence *scope_image(struct pg_typing *typing,
	const struct pg_evidence *value, const struct scope_frame *frames)
{
	for (; value && frames; frames = frames->next) {
		if (frames->map) value = pg_prove_reindex(typing, pg_prove_context_map(typing, frames->map), value);
		else value = rebase_image(typing,
			conclusion_first(typing, PG_JUDGEMENT_CONTEXT, frames->restriction->context), value);
	}
	return value;
}

static const struct pg_evidence *variable_frame(struct pg_typing *typing,
	const struct pg_evidence *variable, const struct scope_frame **frames)
{
	if (!*frames) return NULL;
	const struct scope_frame *frame = *frames;
	*frames = (*frames)->next;
	const struct pg_object *binder = pg_evidence_subject(variable)->core->as.reference;
	if (frame->map) {
		const struct pg_occurrence *image = pg_context_map_image(frame->map, binder);
		if (!image) return NULL;
		const struct pg_term *core = image->core;
		if (core->kind != PG_REFERENCE || core->as.reference->kind != PG_BINDER)
			return pg_prove_structural_subject(typing, image);
		binder = core->as.reference;
		return pg_prove_variable(typing,
			conclusion_first(typing, PG_JUDGEMENT_CONTEXT, frame->map->destination), binder);
	}
	return pg_prove_variable(typing,
		conclusion_first(typing, PG_JUDGEMENT_CONTEXT, frame->restriction->context), binder);
}

static const struct pg_occurrence *returned_computation(const struct pg_occurrence *subject)
{
	if (subject->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (subject->origin)
		return subject->origin->judgement == PG_JUDGEMENT_COMPUTATION ? subject->origin : NULL;
	if (subject->operand_count != 1 || subject->core->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = subject->core->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != &pg_total_result_operation) return NULL;
	const struct pg_occurrence *input = subject->operands[0];
	return input->judgement == PG_JUDGEMENT_COMPUTATION &&
		input->core == subject->core->as.application.argument ? input : NULL;
}

struct typed_query_wait {
	struct pg_typed_query *work;
	struct typed_query_wait *parent;
};

enum typed_query_kind { TYPED_BODY, TYPED_INPUT, TYPED_HEAD, TYPED_ELIMINATION, TYPED_ORIGIN, TYPED_CLASSIFIER, TYPED_REBASE, TYPED_INDUCTIVE, TYPED_SELECTION, TYPED_PHASE };
enum typed_query_resume { TYPED_RESUME_NONE, TYPED_RESUME_BODY, TYPED_RESUME_INPUT, TYPED_RESUME_PHASE,
	TYPED_RESUME_SELECTION, TYPED_RESUME_SELECTED_INPUT };

struct typed_elimination {
	const struct pg_evidence *branch;
	const struct pg_evidence **arguments;
	size_t count, next;
};

struct typed_field {
	const struct pg_evidence *map;
	const struct pg_evidence **declarations;
	struct pg_binding_value *bindings;
	const struct pg_reduction_certificate **reductions;
	size_t prefix, next;
};

struct typed_rebase {
	const struct pg_evidence *source, *substitution;
	struct constructor_structure constructor;
	const struct pg_reduction_certificate *reduction;
	const struct pg_evidence **outputs;
	size_t parameters, count, next;
};

struct typed_inductive {
	const struct scope_frame *frames;
	struct inductive_argument *arguments;
	struct pg_inductive_instance result;
};

struct scope_sequence {
	const struct scope_frame *first;
	struct scope_frame *last;
};

struct selection_pending {
	const struct pg_evidence *argument;
	struct scope_sequence frames;
	size_t index;
	struct selection_pending *next;
};

struct typed_selection {
	struct scope_sequence frames, lifted;
	struct selection_pending *pending;
	const struct pg_evidence *argument, *extended, *body;
	const struct pg_occurrence *child;
	const struct pg_object *binder;
	const struct scope_frame *cursor;
	size_t index;
};

struct typed_association {
	const struct pg_evidence *outer, *inner, *source, *first, *last;
	const struct pg_term *inner_target;
	const struct pg_object *binder;
	unsigned next;
};

struct typed_fold {
	const struct pg_evidence *outer, *handler, *carrier, *head, *payload, *resume, *origin;
	const struct pg_evidence **inputs, **waiting;
	size_t count, next;
};

struct pg_typed_query {
	struct pg_index_entry index;
	struct pg_typing *typing;
	const struct pg_occurrence *source, *current;
	/* Argument subject, rebase Context, head Core, outer scopes, or NF phase. */
	const void *argument_key;
	const struct pg_evidence *argument, *environment, *value, *result;
	enum typed_query_resume resume;
	struct pg_typed_query *dependency;
	struct typed_query_wait *waiting;
	struct pg_occurrence_input *input;
	struct pg_context_lift *lift;
	struct pg_occurrence_action *action;
	struct typed_elimination *elimination;
	struct typed_field *field;
	struct typed_rebase *rebase;
	struct typed_inductive *inductive;
	struct typed_selection *selection;
	struct typed_association *association;
	struct typed_fold *fold;
	const struct pg_reduction_certificate *reduction;
	const struct pg_reduction_certificate *input_reduction;
	enum typed_query_kind kind;
	size_t ordinal;
	size_t forces;
	uint64_t steps;
	int input_resumed;
	int status;
};

static int typed_body_match(struct pg_typed_query *work);
static int typed_input_step(struct pg_typed_query *work);
static int typed_elimination_step(struct pg_typed_query *work);
static int typed_classifier_step(struct pg_typed_query *work);
static int typed_inductive_step(struct pg_typed_query *work);
static int typed_selection_step(struct pg_typed_query *work);
static const struct pg_evidence *unary_term_content(struct pg_typing *typing,
	const struct pg_evidence *proof, const struct pg_occurrence *child);

static struct pg_typed_query *typed_query_request(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_evidence *argument,
	enum typed_query_kind kind, size_t ordinal, const void *key)
{
	const void *value = key ? key : argument ? (kind == TYPED_REBASE
		? (const void *)pg_evidence_context(argument) : (const void *)pg_evidence_subject(argument)) : NULL;
	uint64_t hash = ((uintptr_t)source * UINT64_C(1099511628211) ^ (uintptr_t)value) * UINT64_C(1099511628211);
	hash = (hash ^ ordinal) * UINT64_C(1099511628211);
	hash = (hash ^ kind) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->typed_queries, hash); p; p = p->next) {
		struct pg_typed_query *work = (void *)p;
		if (p->hash == hash && work->source == source && work->argument_key == value &&
			work->kind == kind && work->ordinal == ordinal) return work;
	}
	struct pg_typed_query *work = pg_alloc(typing->graph, sizeof(*work));
	if (!work) return NULL;
	*work = (struct pg_typed_query){.typing = typing, .source = source,
		.argument_key = value, .current = source, .argument = argument, .kind = kind, .ordinal = ordinal};
	return pg_index_insert(&typing->typed_queries, &work->index, hash) ? NULL : work;
}

struct pg_typed_query *pg_application_body_request(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(function, typing)) return NULL;
	if (pg_evidence_judgement(function) != PG_JUDGEMENT_COMPUTATION && pg_evidence_judgement(function) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (pg_evidence_judgement(argument) != PG_JUDGEMENT_VALUE && pg_evidence_judgement(argument) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pg_evidence_context(function) != pg_evidence_context(argument)) return NULL;
	return typed_query_request(typing, pg_evidence_subject(function), argument, TYPED_BODY, 0, NULL);
}

struct pg_typed_query *pg_return_body_request(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing) || pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	return typed_query_request(typing, pg_evidence_subject(computation), NULL, TYPED_BODY, 0, NULL);
}

struct pg_typed_query *pg_elimination_body_request(struct pg_typing *typing,
	const struct pg_evidence *elimination)
{
	if (!pg_evidence_owned_by(elimination, typing)) return NULL;
	if (elimination->rule != PG_MATCH_ELIM && elimination->rule != PG_INDUCTION_ELIM) return NULL;
	return typed_query_request(typing, pg_evidence_subject(elimination), NULL, TYPED_ELIMINATION, 0, NULL);
}

static struct pg_typed_query *typed_head_request(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_reduction_certificate *receipt)
{
	if (!pg_evidence_owned_by(source, typing) || !pg_evidence_subject(source)) return NULL;
	if (receipt) {
		if (pg_reduction_policy(receipt) != &pg_pure_policy) return NULL;
		if (pg_alpha_equal(pg_evidence_subject(source)->core, pg_reduction_source(receipt)) != 1) return NULL;
	}
	/* Without a receipt the caller requests constructor exposure, not a
	 * prediction of the evaluator's normal form. */
	return typed_query_request(typing, pg_evidence_subject(source), NULL, TYPED_HEAD, 0, receipt ? pg_reduction_target(receipt) : NULL);
}

/* Reuse the accepted source and its finite reduction chain. Different input
 * queries share these intermediate conclusions; this is not another AST. */
static struct pg_typed_query *typed_phase_request(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_reduction_phase *phase)
{
	return typed_query_request(typing, source, NULL, TYPED_PHASE, 0, phase);
}

static int typed_phase_step(struct pg_typed_query *work)
{
	const struct pg_reduction_phase *phase = work->argument_key;
	const struct pg_evidence *source;
	if (phase->previous) {
		if (!work->dependency) {
			work->dependency = typed_phase_request(work->typing, work->source, phase->previous);
			return work->dependency ? 0 : -1;
		}
		if (!work->dependency->status) return 0;
		source = pg_typed_query_result(work->dependency);
	} else source = pg_prove_structural_subject(work->typing, work->source);
	const struct pg_reduction_certificate *receipt = phase->children[0]
		? pg_reduction_prefix(work->typing->graph, phase->head, phase->children[0], phase->children[1]) : phase->head;
	work->result = pg_prove_normalization(work->typing, source, receipt);
	return work->result ? 1 : -1;
}

struct pg_typed_query *pg_typed_input_request(struct pg_typing *typing,
	const struct pg_evidence *source, size_t index)
{
	if (!pg_evidence_owned_by(source, typing) || !pg_evidence_subject(source) || index == SIZE_MAX) return NULL;
	return typed_query_request(typing, pg_evidence_subject(source), NULL, TYPED_INPUT, index, NULL);
}

struct pg_typed_query *pg_construction_origin_request(struct pg_typing *typing,
	const struct pg_evidence *source)
{
	if (!pg_evidence_owned_by(source, typing) || !pg_evidence_subject(source)) return NULL;
	return typed_query_request(typing, pg_evidence_subject(source), NULL, TYPED_ORIGIN, 0, NULL);
}

struct pg_typed_query *pg_rebase_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *source)
{
	if (!context_proof(typing, context) || !pg_evidence_owned_by(source, typing) || !pg_evidence_subject(source)) return NULL;
	return typed_query_request(typing, pg_evidence_subject(source), context, TYPED_REBASE, 0, NULL);
}

static int typed_rebase_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_evidence *context = work->argument;
	if (work->resume == TYPED_RESUME_BODY) {
		const struct pg_evidence *value = pg_typed_query_result(work->dependency);
		if (work->environment) value = pg_prove_reindex(typing, work->environment, value);
		work->current = value ? pg_evidence_subject(value) : NULL;
		work->environment = NULL;
		work->dependency = NULL;
		work->resume = TYPED_RESUME_NONE;
		return work->current ? 0 : -1;
	}
	if (!work->rebase) {
		const struct pg_occurrence *subject = work->current;
		if (work->source->core->kind == PG_REFERENCE && work->source->core->as.reference->kind == PG_BINDER)
			work->result = image_boundary(typing, pg_prove_variable(typing, context, work->source->core->as.reference), work->source);
		if (!work->result) work->result = image_boundary(typing, pg_prove_projection(typing, context,
			pg_prove_structural_subject(typing, subject)), work->source);
		if (work->result) return 1;
		if (subject->origin && !subject->selection) {
			if (subject->map) {
				const struct pg_evidence *inner = pg_prove_context_map(typing, subject->map);
				work->environment = work->environment ? pg_prove_substitution_compose(typing, inner, work->environment) : inner;
				if (!work->environment) return -1;
				work->current = subject->origin;
				return 0;
			}
			if (subject->core == subject->origin->core && subject->classifier == subject->origin->classifier) {
				work->current = subject->origin;
				return 0;
			}
			if (subject->origin->judgement == PG_JUDGEMENT_COMPUTATION && subject->judgement != PG_JUDGEMENT_COMPUTATION) {
				work->dependency = pg_return_body_request(typing, pg_prove_structural_subject(typing, subject->origin));
				work->resume = TYPED_RESUME_BODY;
				return work->dependency ? 0 : -1;
			}
		}
		if (work->environment && subject->core->kind == PG_REFERENCE && subject->core->as.reference->kind == PG_BINDER) {
			const struct pg_evidence *value = pg_substitution_image(typing, work->environment, subject->core->as.reference);
			work->current = value ? pg_evidence_subject(value) : NULL;
			work->environment = NULL;
			return work->current ? 0 : -1;
		}
		struct typed_rebase *state = pg_alloc(typing->graph, sizeof(*state));
		if (!state) return -1;
		state->source = pg_prove_structural_subject(typing, subject);
		if (!state->source) return -1;
		int constructor = subject->origin ? 0 : constructor_view(typing, state->source, &state->constructor);
		if (constructor < 0) return -1;
		if (constructor) {
			if (state->constructor.count != subject->operand_count) return -1;
			state->parameters = pg_evidence_context_map(state->constructor.parameters)->count;
			if (subject->operand_count > SIZE_MAX - state->parameters) return -1;
			state->count = state->parameters + subject->operand_count;
		} else if (!subject->origin && subject->operand_count == 2 && subject->core->kind == PG_APPLICATION &&
			subject->operands[0]->judgement == PG_JUDGEMENT_TYPE_FAMILY &&
			subject->operands[0]->core == subject->core->as.application.function &&
			subject->operands[1]->core == subject->core->as.application.argument) state->count = 2;
		else if (work->environment) {
			state->substitution = work->environment;
			state->count = pg_evidence_context_map(state->substitution)->count;
			work->environment = NULL;
		} else if (subject->origin && !subject->selection) {
			state->reduction = subject_normalization(typing, subject);
			if (!state->reduction) return -1;
			state->count = 1;
		} else return -1;
		if (state->count > SIZE_MAX / sizeof(*state->outputs)) return -1;
		state->outputs = pg_alloc(typing->graph, state->count * sizeof(*state->outputs));
		if (state->count && !state->outputs) return -1;
		work->rebase = state;
	}
	struct typed_rebase *state = work->rebase;
	if (work->dependency) {
		const struct pg_evidence *result = pg_typed_query_result(work->dependency);
		if (!result) return -1;
		state->outputs[state->next++] = result;
		work->dependency = NULL;
	}
	if (state->next < state->count) {
		const struct pg_occurrence *input;
		if (state->constructor.constructor && state->next < state->parameters)
			input = pg_evidence_context_map(state->constructor.parameters)->images[state->next];
		else if (state->substitution) input = pg_evidence_context_map(state->substitution)->images[state->next];
		else input = state->reduction ? work->current->origin : work->current->operands[state->next - state->parameters];
		const struct pg_evidence *value = pg_prove_structural_subject(typing, input);
		if (work->environment) value = pg_prove_reindex(typing, work->environment, value);
		work->dependency = pg_rebase_request(typing, context, value);
		return work->dependency ? 0 : -1;
	}
	if (state->constructor.constructor) {
		const struct pg_evidence *map = pg_prove_substitution(typing, state->constructor.parameters->premises[0],
			context, state->parameters, state->outputs);
		work->result = pg_prove_constructor(typing, state->constructor.formation,
			state->constructor.constructor, map, state->count - state->parameters,
			state->count > state->parameters ? state->outputs + state->parameters : NULL);
	} else if (state->substitution) {
		const struct pg_evidence *map = pg_prove_substitution(typing, state->substitution->premises[0],
			context, state->count, state->outputs);
		work->result = pg_prove_reindex(typing, map, state->source);
	} else if (state->reduction) work->result = pg_prove_normalization(typing, state->outputs[0], state->reduction);
	else work->result = pg_prove_family_application(typing, state->outputs[0], state->outputs[1]);
	work->result = image_boundary(typing, work->result, work->source);
	return work->result ? 1 : -1;
}

static int typed_origin_step(struct pg_typed_query *work)
{
	const struct pg_occurrence *subject = work->source;
	if (subject->origin && !subject->selection && (subject->map || subject->judgement == subject->origin->judgement)) {
		if (!work->dependency) {
			work->dependency = pg_construction_origin_request(work->typing,
				pg_prove_structural_subject(work->typing, subject->origin));
			return work->dependency ? 0 : -1;
		}
		if (!work->dependency->status) return 0;
		work->result = pg_typed_query_result(work->dependency);
		if (!work->result) return -1;
		work->environment = pg_construction_origin_environment(work->dependency);
		if (subject->map) {
			const struct pg_evidence *step = pg_prove_context_map(work->typing, subject->map);
			work->environment = work->environment
				? pg_prove_substitution_compose(work->typing, work->environment, step) : step;
			if (!work->environment) return -1;
		}
		return 1;
	}
	work->result = pg_prove_structural_subject(work->typing, subject);
	return work->result ? 1 : -1;
}

const struct pg_evidence *pg_construction_origin_environment(const struct pg_typed_query *work)
{
	return work && work->kind == TYPED_ORIGIN && work->status == 1 ? work->environment : NULL;
}

static int typed_body_enter(struct pg_typed_query *work, const struct pg_occurrence *current,
	const struct pg_evidence *argument)
{
	const struct pg_evidence *source = pg_prove_structural_subject(work->typing, current);
	/* A captured callee may itself be the image of a binder. Apply the same
	 * pending scope to both operands before asking shared application work. */
	if (work->environment) {
		source = pg_prove_reindex(work->typing, work->environment, source);
		if (argument) {
			argument = pg_prove_reindex(work->typing, work->environment, argument);
			if (!argument) return -1;
		}
		work->environment = NULL;
	}
	work->dependency = argument ? pg_application_body_request(work->typing, source, argument)
		: pg_return_body_request(work->typing, source);
	work->resume = TYPED_RESUME_BODY;
	return work->dependency ? 0 : -1;
}

static int exposed_head(const struct pg_occurrence *subject)
{
	const struct pg_term *core = subject->core, *payload, *resume;
	const struct pg_object *label;
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION &&
		core->kind == PG_APPLICATION && core->as.application.function->kind == PG_REFERENCE &&
		core->as.application.function->as.reference == &pg_return_operation) return 1;
	/* A normalized request still needs its typed construction, not merely
	 * its printed head, to retain the operation's accepted declaration. */
	if (subject->judgement == PG_JUDGEMENT_COMPUTATION && !subject->origin &&
		pg_computation_request_view(core, &label, &payload, &resume)) return 1;
	if (subject->judgement != PG_JUDGEMENT_VALUE) return 0;
	const struct pg_term *head = subject->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	const struct pg_data_layout *layout;
	size_t position, arity;
	return head->kind == PG_REFERENCE && pg_data_constructor_view(head->as.reference, &layout, &position, &arity);
}

static const struct pg_term *fold_source(const struct pg_term *core)
{
	if (!core || core->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = core->as.application.function;
	if (head->kind != PG_APPLICATION || head->as.application.function->kind != PG_REFERENCE) return NULL;
	return head->as.application.function->as.reference == &pg_fold_operation ? head->as.application.argument : NULL;
}

/* Read the evaluator's association shape to demand the inner head first.
 * The shape guides work only; ordinary rules establish every typed result. */
static int typed_association_start(struct pg_typed_query *work)
{
	if (work->kind != TYPED_HEAD || !work->argument_key || work->forces) return 0;
	if (!fold_source(fold_source(work->current->core))) return 0;
	const struct pg_term *target = work->argument_key, *source = fold_source(target);
	if (!source) return 0;
	const struct pg_term *lambda = target->as.application.argument;
	if (lambda->kind != PG_LAMBDA) return 0;
	const struct pg_term *call = fold_source(lambda->as.lambda.body);
	if (!call || call->kind != PG_APPLICATION) return 0;
	const struct pg_term *argument = call->as.application.argument;
	if (argument->kind != PG_REFERENCE || argument->as.reference != lambda->as.lambda.binder) return 0;
	struct typed_association *state = pg_alloc(work->typing->graph, sizeof(*state));
	if (!state) return -1;
	*state = (struct typed_association){0};
	state->outer = pg_prove_structural_subject(work->typing, work->current);
	if (work->environment) state->outer = pg_prove_reindex(work->typing, work->environment, state->outer);
	state->inner_target = pg_computation_fold(work->typing->graph, source, call->as.application.function, 0, NULL);
	state->binder = lambda->as.lambda.binder;
	if (!state->outer || !state->inner_target) return -1;
	work->association = state;
	work->environment = NULL;
	return 1;
}

static int typed_association_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	struct typed_association *state = work->association;
	const struct pg_evidence **outputs[] = {&state->inner, &state->inner, &state->source, &state->first, &state->last};
	if (state->next < 5) {
		if (!work->dependency) {
			switch (state->next) {
			case 0: work->dependency = pg_typed_input_request(typing, state->outer, 0); break;
			case 1: work->dependency = typed_query_request(typing, pg_evidence_subject(state->inner), NULL,
				TYPED_HEAD, 0, state->inner_target); break;
			case 2: work->dependency = pg_typed_input_request(typing, state->inner, 0); break;
			case 3: work->dependency = pg_typed_input_request(typing, state->inner, 1); break;
			case 4: work->dependency = pg_typed_input_request(typing, state->outer, 1); break;
			}
			return work->dependency ? 0 : -1;
		}
		if (!work->dependency->status) return 0;
		const struct pg_evidence *input = pg_typed_query_result(work->dependency);
		if (!input) return -1;
		*outputs[state->next++] = input;
		work->dependency = NULL;
		return 0;
	}
	const struct pg_evidence *context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, pg_evidence_context(state->outer));
	const struct pg_evidence *domain = pg_prove_return_content(typing, pg_prove_classifier(typing, context, state->source));
	const struct pg_evidence *extended = pg_prove_context_extension(typing, context, state->binder, domain);
	const struct pg_evidence *call = pg_prove_application(typing, pg_prove_projection(typing, extended, state->first),
		pg_prove_variable(typing, extended, state->binder));
	const struct pg_evidence *body = pg_prove_fold(typing, call, pg_prove_projection(typing, extended, state->last));
	const struct pg_evidence *pi = pg_prove_pi(typing, extended, pg_prove_classifier(typing, extended, body));
	const struct pg_evidence *result = pg_prove_fold(typing, state->source, pg_prove_lambda(typing, pi, body));
	if (!result) return -1;
	work->current = pg_evidence_subject(result);
	work->association = NULL;
	return 0;
}

static int typed_fold_start(struct pg_typed_query *work)
{
	const struct pg_occurrence *source = work->current;
	const struct pg_term *head = source->core;
	size_t supplied = source->operand_count, count = 0;
	if (supplied < 2) return 0;
	if (supplied == 2 && !fold_source(head)) return 0;
	for (size_t i = 0; i < supplied; ++i) {
		if (head->kind != PG_APPLICATION) return 0;
		head = head->as.application.function;
	}
	if (head->kind != PG_REFERENCE) return 0;
	const struct pg_clause_position *positions;
	if (head->as.reference != &pg_fold_operation &&
		!pg_computation_handler_view(head->as.reference, &count, &positions)) return 0;
	if (count > SIZE_MAX - 2 || supplied != count + 2) return -1;
	if (supplied > SIZE_MAX / sizeof(const struct pg_evidence *)) return -1;
	struct typed_fold *state = pg_alloc(work->typing->graph, sizeof(*state));
	if (!state) return -1;
	*state = (struct typed_fold){.count = count};
	state->inputs = pg_alloc(work->typing->graph, supplied * sizeof(*state->inputs));
	if (!state->inputs) return -1;
	/* Signature formation is logical evidence. Program operands come from
	 * the typed structure; no traversal of premise wrappers recovers them. */
	if (count) {
		for (state->handler = pg_evidence_for_subject(work->typing, source, NULL);
			state->handler && state->handler->rule != PG_HANDLER_ELIM;
			state->handler = pg_evidence_for_subject(work->typing, source, state->handler)) {}
		if (!state->handler) return -1;
	}
	state->outer = pg_prove_structural_subject(work->typing, source);
	if (work->environment) state->outer = pg_prove_reindex(work->typing, work->environment, state->outer);
	if (!state->outer) return -1;
	if (count) {
		state->carrier = formed_classifier(work->typing, state->outer);
		if (!state->carrier) return -1;
	}
	work->environment = NULL;
	work->fold = state;
	return 1;
}

static const struct pg_evidence *typed_fold_rebuild(struct pg_typing *typing,
	const struct typed_fold *state, const struct pg_evidence *context, const struct pg_evidence *input)
{
	const struct pg_evidence *returned = pg_prove_projection(typing, context, state->inputs[1]);
	if (!state->count) return pg_prove_fold(typing, input, returned);
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	if (state->count > SIZE_MAX / sizeof(struct pg_handler_clause)) return NULL;
	struct pg_handler_clause *clauses = pg_alloc(&temporary, state->count * sizeof(*clauses));
	if (!clauses) goto done;
	for (size_t i = 0; i < state->count; ++i) {
		clauses[i].operation = pg_operation_declaration_at(typing,
			pg_handler_signature_label(state->handler->certificate, i),
			state->handler->premises[3 + 3 * i], state->handler->premises[4 + 3 * i]);
		clauses[i].body = pg_prove_projection(typing, context, state->inputs[i + 2]);
	}
	result = pg_prove_handler(typing, input, returned,
		pg_prove_projection(typing, context, state->carrier), state->count, clauses);
done:
	pg_graph_destroy(&temporary);
	return result;
}

static int typed_fold_wait(struct pg_typed_query *work,
	const struct pg_evidence **slot, struct pg_typed_query *dependency)
{
	work->fold->waiting = slot;
	work->dependency = dependency;
	return dependency ? 0 : -1;
}

static int typed_fold_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	struct typed_fold *state = work->fold;
	if (state->waiting) {
		if (!work->dependency->status) return 0;
		*state->waiting = pg_typed_query_result(work->dependency);
		if (!*state->waiting) return -1;
		state->waiting = NULL;
		work->dependency = NULL;
		return 0;
	}
	if (state->next < state->count + 2) {
		size_t i = state->next++;
		return typed_fold_wait(work, &state->inputs[i], pg_typed_input_request(typing, state->outer, i));
	}
	if (!state->head)
		return typed_fold_wait(work, &state->head, typed_head_request(typing, state->inputs[0], NULL));
	const struct pg_term *head = pg_evidence_subject(state->head)->core;
	const struct pg_evidence *result;
	if (head->kind == PG_APPLICATION && head->as.application.function == pg_reference(typing->graph, &pg_return_operation)) {
		if (!state->payload)
			return typed_fold_wait(work, &state->payload, pg_return_body_request(typing, state->inputs[0]));
		result = pg_prove_application(typing, state->inputs[1], state->payload);
	} else {
		const struct pg_term *payload, *resume;
		const struct pg_object *label;
		if (!pg_computation_request_view(head, &label, &payload, &resume)) return -1;
		if (!state->payload)
			return typed_fold_wait(work, &state->payload, pg_typed_input_request(typing, state->head, 0));
		if (!state->resume)
			return typed_fold_wait(work, &state->resume, pg_typed_input_request(typing, state->head, 1));
		if (!state->origin)
			return typed_fold_wait(work, &state->origin, pg_construction_origin_request(typing, state->head));
		const struct pg_operation_declaration *operation = pg_evidence_request_declaration(state->origin);
		if (!operation || pg_operation_label(operation) != label) return -1;
		const struct pg_evidence *context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, pg_evidence_context(state->outer));
		const struct pg_object *binder = pg_binder(typing->graph);
		const struct pg_evidence *scope = pg_prove_context_extension(typing, context, binder,
			pg_prove_projection(typing, context, pg_operation_response_type(operation)));
		const struct pg_evidence *call = pg_prove_application(typing, pg_prove_projection(typing, scope, state->resume),
			pg_prove_variable(typing, scope, binder));
		const struct pg_evidence *body = typed_fold_rebuild(typing, state, scope, call);
		const struct pg_evidence *pi = pg_prove_pi(typing, scope, formed_classifier(typing, body));
		const struct pg_evidence *continuation = pg_prove_lambda(typing, pi, body);
		size_t i = 0;
		while (i < state->count && pg_handler_signature_label(state->handler->certificate, i) != label) ++i;
		result = i == state->count ? pg_prove_request(typing, operation, state->payload, continuation)
			: pg_prove_application(typing, pg_prove_application(typing, state->inputs[i + 2], state->payload),
				pg_prove_thunk(typing, continuation));
	}
	if (!result) return -1;
	work->current = pg_evidence_subject(result);
	work->fold = NULL;
	return 0;
}

static int typed_return_step(struct pg_typed_query *work)
{
	if (!work->dependency) {
		work->dependency = typed_head_request(work->typing,
			pg_prove_structural_subject(work->typing, work->source), NULL);
		return work->dependency ? 0 : -1;
	}
	if (!work->dependency->status) return 0;
	const struct pg_evidence *result = pg_typed_query_result(work->dependency);
	if (!result) return -1;
	if (work->resume == TYPED_RESUME_INPUT) {
		work->result = result;
		return 1;
	}
	const struct pg_term *core = pg_evidence_subject(result)->core;
	if (core->kind != PG_APPLICATION || core->as.application.function !=
		pg_reference(work->typing->graph, &pg_return_operation)) return -1;
	work->dependency = pg_typed_input_request(work->typing, result, 0);
	work->resume = TYPED_RESUME_INPUT;
	return work->dependency ? 0 : -1;
}

static int typed_body_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_occurrence *current = work->current;
	if (work->kind == TYPED_BODY && !work->argument) return typed_return_step(work);
	if (work->association) return typed_association_step(work);
	if (work->fold) return typed_fold_step(work);
	if (work->resume == TYPED_RESUME_BODY) {
		if (!work->dependency->status) return 0;
		const struct pg_evidence *result = pg_typed_query_result(work->dependency);
		if (!result) return -1;
		work->current = pg_evidence_subject(result);
		work->dependency = NULL;
		work->resume = TYPED_RESUME_NONE;
		return 0;
	}
	if (work->value) {
		work->result = work->environment ? pg_prove_reindex(typing, work->environment, work->value) : work->value;
		return work->result ? 1 : -1;
	}
	/* A checked result is usable structure, even when its origin is a
	 * computation whose construction cannot be exposed by this query. */
	const struct pg_term *core = current->core;
	/* The retained reduction specifies the head. Constructor guesses cannot
	 * replace that boundary, especially while a context action is pending. */
	if (work->kind == TYPED_HEAD && !work->forces &&
		(work->argument_key || work->action || exposed_head(current))) {
		const struct pg_occurrence *image = current;
		if (work->environment) {
			if (!work->action) work->action = pg_occurrence_action_request(typing,
				pg_evidence_context_map(work->environment), current);
			enum pg_substitution_status status = pg_occurrence_action_advance(work->action, 1);
			if (status == PG_SUBSTITUTION_PENDING) return 0;
			if (status == PG_SUBSTITUTION_ERROR) return -1;
			image = pg_occurrence_action_result(work->action);
			work->action = NULL;
		}
		if (image->context == work->source->context &&
			(!work->argument_key || pg_alpha_equal(image->core, work->argument_key) == 1)) {
			work->value = pg_prove_structural_subject(typing, current);
			if (work->environment) work->value = pg_prove_reindex(typing, work->environment, work->value);
			work->environment = NULL;
			return work->value ? 0 : -1;
		}
	}
	const struct pg_occurrence *returned = returned_computation(current);
	if (returned) return typed_body_enter(work, returned, NULL);
	/* Child normalization can reveal right unit. Select the current typed
	 * input before following provenance, including under a context action. */
	if (core->kind == PG_APPLICATION && core->as.application.function->kind == PG_APPLICATION) {
		const struct pg_term *head = core->as.application.function;
		if (head->as.application.function == pg_reference(typing->graph, &pg_fold_operation) &&
			pg_computation_eta(typing->graph, core) == head->as.application.argument) {
			work->dependency = pg_typed_input_request(typing, pg_prove_structural_subject(typing, current), 0);
			work->resume = TYPED_RESUME_BODY;
			return work->dependency ? 0 : -1;
		}
	}
	int association = typed_association_start(work);
	if (association) return association < 0 ? -1 : 0;
	if (current->origin) {
		if (current->selection) return -1;
		if (current->map) {
			const struct pg_evidence *step = pg_prove_context_map(typing, current->map);
			work->environment = work->environment ? pg_prove_substitution_compose(typing, step, work->environment) : step;
			if (!work->environment) return -1;
		} else if (current->judgement != current->origin->judgement) {
			if (current->judgement != PG_JUDGEMENT_COMPUTATION || current->origin->judgement != PG_JUDGEMENT_VALUE) return -1;
			++work->forces;
		}
		work->current = current->origin;
		return 0;
	}
	if (current->map_count == 1)
		return typed_body_match(work);
	int fold = typed_fold_start(work);
	if (fold) return fold < 0 ? -1 : 0;
	if (core->kind == PG_LAMBDA) {
		if (!work->argument || work->forces || current->operand_count != 1) return -1;
		const struct pg_occurrence *body = pg_occurrence_scoped_input(current, 0);
		if (!body) return -1;
		const struct pg_evidence *extended = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, body->context);
		if (!extended) return -1;
		const struct pg_evidence *map = work->environment ? work->environment
			: pg_prove_substitution_projection(typing, extended->premises[0], extended->premises[0]);
		map = pg_prove_substitution_pair(typing, map, extended, work->argument);
		work->result = pg_prove_reindex(typing, map, pg_prove_structural_subject(typing, body));
		return work->result ? 1 : -1;
	}
	if (core->kind == PG_APPLICATION) {
		const struct pg_term *head = core->as.application.function;
		if (head->kind == PG_REFERENCE &&
			(head->as.reference == &pg_force_operation || head->as.reference == &pg_thunk_operation)) {
			if (current->operand_count != 1 || current->operands[0]->core != core->as.application.argument) return -1;
			if (head->as.reference == &pg_force_operation) ++work->forces;
			else if (work->forces) --work->forces;
			else return -1;
			work->current = current->operands[0];
			return 0;
		}
		if (current->operand_count != 2 || current->operands[1]->core != core->as.application.argument) return -1;
		if (current->operands[0]->core != head) return -1;
		const struct pg_evidence *right = pg_prove_structural_subject(typing, current->operands[1]);
		if (!right) return -1;
		return typed_body_enter(work, current->operands[0], right);
	}
	if (core->kind != PG_REFERENCE || core->as.reference->kind != PG_BINDER || !work->environment) return -1;
	const struct pg_evidence *image = pg_substitution_image(typing, work->environment, core->as.reference);
	if (!image) return -1;
	work->current = pg_evidence_subject(image);
	work->environment = NULL;
	return work->current ? 0 : -1;
}

int pg_typed_query_advance(struct pg_typed_query *work, uint64_t budget)
{
	if (!work) return -1;
	while (!work->status && budget--) {
		struct pg_typed_query *current = work->waiting ? work->waiting->work : work;
		++work->steps;
		if (!current->status) {
			if (current != work) ++current->steps;
			switch (current->kind) {
			case TYPED_INPUT: current->status = typed_input_step(current); break;
			case TYPED_ELIMINATION: current->status = typed_elimination_step(current); break;
			case TYPED_ORIGIN: current->status = typed_origin_step(current); break;
			case TYPED_CLASSIFIER: current->status = typed_classifier_step(current); break;
			case TYPED_REBASE: current->status = typed_rebase_step(current); break;
			case TYPED_INDUCTIVE: current->status = typed_inductive_step(current); break;
			case TYPED_SELECTION: current->status = typed_selection_step(current); break;
			case TYPED_PHASE: current->status = typed_phase_step(current); break;
			default: current->status = typed_body_step(current); break;
			}
			if (current->kind == TYPED_INPUT && current->status == 1 && !current->result && !current->ordinal)
				current->result = unary_term_content(current->typing,
					pg_prove_structural_subject(current->typing, current->source), NULL);
		}
		if (current->status) {
			if (work->waiting) work->waiting = work->waiting->parent;
		} else if (current->dependency && !current->dependency->status) {
			struct typed_query_wait *frame = pg_alloc(work->typing->graph, sizeof(*frame));
			if (!frame) { work->status = -1; break; }
			*frame = (struct typed_query_wait){current->dependency, work->waiting};
			work->waiting = frame;
		}
	}
	return work->status;
}

const struct pg_evidence *pg_typed_query_result(const struct pg_typed_query *work)
{
	return work && work->status == 1 ? work->result : NULL;
}

uint64_t pg_typed_query_steps(const struct pg_typed_query *work)
{
	return work ? work->steps : 0;
}

const struct pg_evidence *pg_prove_application_body(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	struct pg_typed_query *work = pg_application_body_request(typing, function, argument);
	while (!pg_typed_query_advance(work, UINT64_MAX)) {}
	return pg_typed_query_result(work);
}

/* Select the retained formation before transporting its required parameters.
 * Rebuilding the whole Pi context would demand images for unused binders
 * already removed by constant-codomain projection. Each wrapper/selection or
 * lifted frame is one shared query transition, not a caller-owned traversal. */
static struct pg_typed_query *selection_request(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct scope_frame *outer)
{
	if (!source || !source->classifier) return NULL;
	return typed_query_request(typing, source, NULL, TYPED_SELECTION, 0, outer);
}

/* Only link a query's private prefix. Published tails are immutable and can
 * be borrowed by another query without copying or scanning their frames. */
static void scope_append(struct scope_sequence *prefix, struct scope_sequence suffix)
{
	if (!suffix.first) return;
	if (prefix->last) prefix->last->next = suffix.first;
	else prefix->first = suffix.first;
	prefix->last = suffix.last;
}

/* Transport an open input's scope before its body. An explicit argument
 * closes the lifted binder; without one the input remains an open body. */
static int selection_lift_step(struct pg_typing *typing, struct typed_selection *state)
{
	const struct scope_frame *frame = state->cursor;
	const struct pg_evidence *map = NULL, *restricted = NULL;
	const struct pg_evidence *extended = state->extended;
	if (!frame && state->argument) {
		map = pg_prove_substitution_projection(typing, extended->premises[0], extended->premises[0]);
		map = pg_prove_substitution_pair(typing, map, extended, state->argument);
	} else if (frame) {
		const struct pg_context *destination = frame->map ? frame->map->destination : frame->restriction->context;
		const struct pg_object *binder = !frame->next && state->binder ? state->binder
			: pg_evidence_context(extended)->binder;
		if (pg_context_lookup(destination, binder)) binder = pg_binder(typing->graph);
		if (frame->map) {
			map = pg_prove_substitution_lift(typing, pg_prove_context_map(typing, frame->map), extended, binder);
			if (!map) return -1;
			state->extended = map->premises[1];
		} else {
			const struct pg_evidence *step = pg_prove_structural_subject(typing, frame->restriction);
			const struct pg_evidence *context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, destination);
			state->extended = pg_prove_context_extension(typing, context, binder, pg_prove_pi_domain(typing, step));
			if (!state->extended) return -1;
			restricted = pg_prove_pi_codomain(typing, pg_prove_projection(typing, state->extended, step),
				pg_prove_variable(typing, state->extended, binder));
		}
	}
	if (frame || state->argument) {
		struct scope_frame *lifted = scope_frame(typing->graph, pg_evidence_context_map(map), restricted ? pg_evidence_subject(restricted) : NULL, NULL);
		if (!lifted) return -1;
		scope_append(&state->lifted, (struct scope_sequence){lifted, lifted});
		/* An open codomain is exactly the checked component described by
		 * the restriction. No independent strengthening of its body is needed. */
		if (state->body) {
			state->body = map ? pg_prove_reindex(typing, map, state->body) : restricted;
			if (!state->body) return -1;
		}
	}
	if (frame) { state->cursor = frame->next; return 0; }
	state->extended = NULL;
	state->frames = state->lifted;
	state->lifted = (struct scope_sequence){0};
	return 1;
}

static int typed_selection_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	if (!work->selection) {
		work->selection = pg_alloc(typing->graph, sizeof(*work->selection));
		if (!work->selection) return -1;
		work->selection->index = SIZE_MAX;
	}
	struct typed_selection *state = work->selection;
	const struct pg_occurrence *current = work->current;
	if (state->extended) {
		int status = selection_lift_step(typing, state);
		if (status <= 0) return status;
	}
	if (state->child) {
		work->current = state->child;
		state->child = NULL;
		scope_append(&state->frames, state->pending->frames);
		state->argument = state->pending->argument;
		state->index = state->pending->index;
		state->pending = state->pending->next;
		/* Do not chase the computation producing this selected type. */
		if (state->index != SIZE_MAX) return 0;
	} else if (current->map) {
		struct scope_frame *frame = scope_frame(typing->graph, current->map, NULL, state->frames.first);
		if (!frame) return -1;
		state->frames.first = frame;
		if (!state->frames.last) state->frames.last = frame;
		work->current = current->origin;
		return 0;
	} else if (current->selection) {
		struct selection_pending *next = pg_alloc(typing->graph, sizeof(*next));
		if (!next) return -1;
		*next = (struct selection_pending){state->argument, state->frames, state->index, state->pending};
		state->pending = next;
		state->frames = (struct scope_sequence){0};
		state->argument = current->operand_count ? pg_prove_structural_subject(typing, current->operands[0]) : NULL;
		if (current->operand_count && !state->argument) return -1;
		state->index = current->selection - 1;
		work->current = current->origin;
		return 0;
	} else if (current->origin && current->core == current->origin->core) {
		work->current = current->origin;
		return 0;
	} else if (state->index != SIZE_MAX) {
		const struct pg_occurrence *child;
		if (current->origin) {
			/* A changed head needs its checked current inputs. Following the
			 * old construction would silently select a pre-normalized child. */
			if (!work->dependency) work->dependency = pg_typed_input_request(typing,
				pg_prove_structural_subject(typing, current), state->index);
			if (!work->dependency) return -1;
			if (!work->dependency->status) return 0;
			const struct pg_evidence *input = pg_typed_query_result(work->dependency);
			if (!input) return -1;
			child = pg_evidence_subject(input);
			work->dependency = NULL;
		} else {
			if (state->index >= current->operand_count) return -1;
			child = current->operands[state->index];
		}
		if (child->context != current->context) {
			const struct pg_term *body;
			const struct pg_object *binder = pg_occurrence_input_binder(current->core, state->index, &body);
			if (!binder || !child->context || child->context->parent != current->context ||
				child->context->binder != binder || child->core != body) return -1;
			if (state->argument) {
				state->extended = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, child->context);
				if (!state->extended) return -1;
				state->cursor = state->frames.first;
			} else {
				const struct pg_evidence *constant = pg_prove_pi_constant_codomain(typing, pg_prove_structural_subject(typing, current));
				struct scope_frame *frame = scope_frame(typing->graph, NULL, pg_evidence_subject(constant), state->frames.first);
				if (!frame) return -1;
				state->frames.first = frame;
				if (!state->frames.last) state->frames.last = frame;
			}
		} else if (state->argument) return -1;
		state->child = child;
		return 0;
	}
	/* The supplied suffix is borrowed only after this query stops mutating
	 * its own prefix. No consumer may append to the published sequence. */
	if (state->frames.last) state->frames.last->next = work->argument_key;
	else state->frames.first = work->argument_key;
	work->result = pg_prove_structural_subject(typing, work->current);
	return work->result ? 1 : -1;
}

struct inductive_argument {
	const struct pg_evidence *value;
	const struct scope_frame *frames;
	struct inductive_argument *next;
};

struct pg_typed_query *pg_inductive_request(struct pg_typing *typing,
	const struct pg_evidence *type)
{
	if (!pg_evidence_owned_by(type, typing)) return NULL;
	if (pg_evidence_judgement(type) != PG_JUDGEMENT_VALUE_TYPE && pg_evidence_judgement(type) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	return typed_query_request(typing, pg_evidence_subject(type), NULL, TYPED_INDUCTIVE, 0, NULL);
}

/* The nominal owner fixes the formation context and classifier. Look up that
 * exact typed declaration, not any judgement sharing the erased Core. */
static const struct pg_evidence *nominal_formation(struct pg_typing *typing,
	const struct pg_occurrence *subject)
{
	if (subject->operand_count) return NULL;
	const struct pg_term *head = subject->core;
	while (head->kind == PG_APPLICATION) head = head->as.application.function;
	if (head->kind != PG_REFERENCE) return NULL;
	const struct pg_data_declaration *declaration = pg_data_declaration_view(head->as.reference);
	if (!declaration) return NULL;
	const struct pg_context *self = pg_data_declaration_parameters(declaration);
	if (!self || self->parent != subject->context) return NULL;
	enum pg_evidence_judgement judgement = self->judgement == PG_JUDGEMENT_TYPE_FAMILY
		? PG_JUDGEMENT_TYPE_FAMILY : PG_JUDGEMENT_VALUE_TYPE;
	const struct pg_occurrence *formed = pg_occurrence(typing, judgement, self->parent,
		subject->core, self->declared_type, NULL, 0, NULL);
	for (const struct pg_evidence *proof = pg_evidence_for_subject(typing, formed, NULL);
		proof; proof = pg_evidence_for_subject(typing, formed, proof))
		if (proof->rule == PG_INDUCTIVE_FORM && pg_data_schema_declaration(proof->certificate) == declaration) return proof;
	return NULL;
}

static int typed_inductive_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	if (!work->inductive) work->inductive = pg_alloc(typing->graph, sizeof(*work->inductive));
	struct typed_inductive *state = work->inductive;
	if (!state) return -1;
	const struct pg_occurrence *subject = work->current;
	const struct pg_evidence *formation = pg_prove_structural_subject(typing, subject);
	if (!formation) return -1;
	/* Scope traversal belongs to selection work even when there is no
	 * selected component. Nominal resolution only consumes its scoped head. */
	if (subject->map || subject->selection || (subject->origin && subject->core == subject->origin->core)) {
		if (!work->dependency) work->dependency = selection_request(typing, subject, state->frames);
		if (!work->dependency) return -1;
		if (!work->dependency->status) return 0;
		formation = pg_typed_query_result(work->dependency);
		if (!formation) return -1;
		state->frames = work->dependency->selection->frames.first;
		work->dependency = NULL;
		goto advanced;
	}
	if (subject->origin) {
		formation = pg_prove_structural_subject(typing, subject->origin);
		if (subject->origin->judgement == PG_JUDGEMENT_COMPUTATION && subject->judgement != PG_JUDGEMENT_COMPUTATION) {
			if (!work->dependency) work->dependency = pg_return_body_request(typing, formation);
			if (work->dependency && !work->dependency->status) return 0;
			formation = pg_typed_query_result(work->dependency);
			work->dependency = NULL;
		}
		goto advanced;
	}
	const struct pg_term *core = subject->core;
	if (core->kind == PG_REFERENCE && core->as.reference->kind == PG_BINDER) {
		formation = variable_frame(typing, formation, &state->frames);
		goto advanced;
	}
	formation = nominal_formation(typing, subject);
	if (!formation) {
		if (core->kind != PG_APPLICATION || subject->operand_count != 2) return -1;
		const struct pg_term *head = core->as.application.function;
		const struct pg_evidence *left = pg_prove_structural_subject(typing, subject->operands[0]);
		if (!left) return -1;
		if (subject->operands[1]->core != core->as.application.argument) return -1;
		const struct pg_evidence *right = pg_prove_structural_subject(typing, subject->operands[1]);
		if (!right) return -1;
		if (subject->operands[0]->core != head) return -1;
		if (!work->dependency) work->dependency = pg_application_body_request(typing, left, right);
		if (work->dependency && !work->dependency->status) return 0;
		formation = pg_typed_query_result(work->dependency);
		work->dependency = NULL;
		if (!formation && pg_evidence_judgement(left) == PG_JUDGEMENT_TYPE_FAMILY) {
			struct inductive_argument *argument = pg_alloc(typing->graph, sizeof(*argument));
			if (!argument) return -1;
			*argument = (struct inductive_argument){right, state->frames, state->arguments};
			state->arguments = argument;
			formation = left;
		}
		goto advanced;
	}
	work->current = pg_evidence_subject(formation);
	if (!work->environment) {
		const struct pg_evidence *context = formation->premises[0]->premises[0];
		work->environment = pg_prove_substitution_projection(typing, context, context);
		return work->environment ? 0 : -1;
	}
	if (state->frames) {
		work->environment = scope_map_step(typing, work->environment, state->frames);
		state->frames = state->frames->next;
		return work->environment ? 0 : -1;
	}
	if (pg_evidence_context(work->environment) != work->source->context) return -1;
	const struct pg_evidence *instance = pg_prove_reindex(typing, work->environment, formation);
	const struct pg_evidence *indices = NULL;
	if (state->arguments) {
		if (!instance || pg_evidence_judgement(instance) != PG_JUDGEMENT_TYPE_FAMILY) return -1;
		const struct pg_data_schema *schema = formation->certificate;
		const struct pg_evidence *self = formation->premises[0];
		const struct pg_evidence *prefix = pg_prove_substitution_pair(typing, work->environment, self, instance);
		if (!prefix) return -1;
		size_t count = 0;
		for (struct inductive_argument *a = state->arguments; a; a = a->next) ++count;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return -1;
		const struct pg_evidence **values = pg_alloc(typing->graph, count * sizeof(*values));
		if (!values) return -1;
		size_t i = 0;
		/* Index arguments cross precisely the wrappers outside their own
		 * application, not the parameter substitutions inside its callee. */
		for (struct inductive_argument *a = state->arguments; a; a = a->next) {
			const struct pg_evidence *value = scope_image(typing, a->value, a->frames);
			if (!value) return -1;
			values[i++] = value;
			instance = pg_prove_family_application(typing, instance, value);
			if (!instance) return -1;
		}
		const struct pg_data_signature *signature = pg_data_signature(typing, self, pg_data_schema_indices(schema));
		indices = pg_data_signature_instance(typing, signature, prefix, count, values);
		if (!indices || pg_evidence_judgement(instance) != PG_JUDGEMENT_VALUE_TYPE) return -1;
	}
	if (!instance || pg_alpha_equal(pg_evidence_subject(instance)->core, work->source->core) != 1) return -1;
	state->result = (struct pg_inductive_instance){formation->certificate, formation, work->environment, indices};
	work->result = instance;
	return 1;
advanced:
	if (!formation) return -1;
	work->current = pg_evidence_subject(formation);
	return 0;
}

const struct pg_inductive_instance *pg_inductive_query_result(const struct pg_typed_query *work)
{
	return work && work->kind == TYPED_INDUCTIVE && work->status == 1 ? &work->inductive->result : NULL;
}

int pg_inductive_instance(struct pg_typing *typing, const struct pg_evidence *type,
	struct pg_inductive_instance *output)
{
	if (!output) return 0;
	struct pg_typed_query *work = pg_inductive_request(typing, type);
	while (!pg_typed_query_advance(work, 1024)) {}
	const struct pg_inductive_instance *result = pg_inductive_query_result(work);
	if (result) *output = *result;
	return result != NULL;
}

const struct pg_evidence *pg_prove_constructor(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *fields)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(parameters->premises[0]) != pg_evidence_context(formation)) return NULL;
	const struct pg_data_schema *schema = formation->certificate;
	if (!pg_data_schema_fields(schema, constructor)) return NULL;
	const struct pg_evidence *family = pg_prove_reindex(typing, parameters, formation);
	const struct pg_evidence *self = pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY
		? family : pg_prove_type_value(typing, family);
	if (!self) return NULL;
	const struct pg_evidence *extended = pg_prove_substitution_extend(typing, parameters,
		formation->premises[0], 1, &self);
	const struct pg_evidence *instance = pg_data_instance(typing, schema, constructor, extended, count, fields);
	if (!instance) return NULL;
	if (pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY) {
		const struct pg_evidence *result_map = pg_data_result(typing, schema, constructor, instance);
		if (!result_map) return NULL;
		for (size_t i = extended->premise_count; i < result_map->premise_count; ++i) {
			family = pg_prove_family_application(typing, family, result_map->premises[i]);
			if (!family) return NULL;
		}
		if (pg_evidence_judgement(family) != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	}
	const struct pg_evidence *premises[] = {family, formation, parameters, instance};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_CONSTRUCTOR_INTRO, pg_evidence_context(instance), NULL, 4, premises, constructor, &hash);
	if (existing) return existing;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_occurrence **operands = pg_alloc(&temporary, count * sizeof(*operands));
	if (count && !operands) goto done;
	const struct pg_term *core = pg_reference(typing->graph, constructor);
	for (size_t i = 0; i < count; ++i) {
		operands[i] = pg_evidence_subject(fields[i]);
		core = pg_application(typing->graph, core, operands[i]->core);
	}
	if (!core) goto done;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(family), NULL, count, operands);
	if (!subject) goto done;
	result = accept_record(typing, PG_CONSTRUCTOR_INTRO,
		pg_evidence_context(instance), subject, 4, premises, constructor, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *lift_scope(struct pg_typing *typing,
	const struct pg_evidence *map, const struct pg_evidence *fields,
	const struct pg_context *allocation, int retained)
{
	if (!pg_evidence_owned_by(map, typing) || map->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!context_proof(typing, fields)) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(map->premises[0]), &count)) return NULL;
	if (retained) {
		size_t supplied;
		if (pg_context_extension_size(allocation, pg_evidence_context(map->premises[1]), &supplied) || supplied != count) return NULL;
	}
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	const struct pg_object **binders = retained ? pg_alloc(&temporary, count * sizeof(*binders)) : NULL;
	if (count && (!extensions || (retained && !binders))) goto done;
	for (size_t i = count; i; --i, fields = fields->premises[0]) {
		extensions[i - 1] = fields;
		if (retained) { binders[i - 1] = allocation->binder; allocation = allocation->parent; }
	}
	for (size_t i = 0; i < count; ++i) {
		map = pg_prove_substitution_lift(typing, map, extensions[i],
			retained ? binders[i] : pg_binder(typing->graph));
		if (!map) goto done;
	}
	result = map;
done:
	pg_graph_destroy(&temporary);
	return result;
}

static const struct pg_evidence *prove_data_scope(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *fields, const struct pg_evidence *parameters,
	const struct pg_context *allocation, int retained)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(parameters->premises[0]) != pg_evidence_context(formation)) return NULL;
	const struct pg_evidence *family = pg_prove_reindex(typing, parameters, formation);
	const struct pg_evidence *self = pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY
		? family : pg_prove_type_value(typing, family);
	if (!self) return NULL;
	const struct pg_evidence *map = pg_prove_substitution_extend(typing, parameters, formation->premises[0], 1, &self);
	return lift_scope(typing, map, fields, allocation, retained);
}

const struct pg_evidence *pg_prove_constructor_scope(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	return prove_data_scope(typing, formation,
		pg_data_schema_fields(formation->certificate, constructor), parameters, NULL, 0);
}

const struct pg_evidence *pg_prove_constructor_scope_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_context *allocation)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	return prove_data_scope(typing, formation,
		pg_data_schema_fields(formation->certificate, constructor), parameters, allocation, 1);
}

static const struct pg_evidence *family_in_scope(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *map)
{
	if (!map) return NULL;
	const struct pg_evidence *family = pg_substitution_image(typing, map, pg_evidence_context(formation->premises[0])->binder);
	for (size_t i = parameters->premise_count + 1; i < map->premise_count; ++i) {
		family = pg_prove_family_application(typing, family, map->premises[i]);
		if (!family) return NULL;
	}
	return pg_prove_value_type(typing, family);
}

const struct pg_evidence *pg_prove_inductive_motive_context(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	const struct pg_evidence *map = prove_data_scope(typing, formation,
		pg_data_schema_indices(formation->certificate), parameters, NULL, 0);
	if (!map) return NULL;
	return pg_prove_context_extension(typing, map->premises[1], binder,
		family_in_scope(typing, formation, parameters, map));
}

const struct pg_evidence *pg_prove_inductive_family_function(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (pg_evidence_judgement(formation) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	const struct pg_evidence *map = prove_data_scope(typing, formation,
		pg_data_schema_indices(formation->certificate), parameters, NULL, 0);
	if (!map) return NULL;
	const struct pg_evidence *body = pg_prove_return_contract(typing, PG_TOTALITY_TOTAL,
		pg_prove_type_value(typing, family_in_scope(typing, formation, parameters, map)));
	return pg_prove_abstract(typing, parameters->premises[1], map->premises[1], body);
}

static const struct pg_evidence *constructor_in_scope(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, const struct pg_evidence *map)
{
	size_t prefix = parameters->premise_count - 2;
	size_t count = map->premise_count - prefix - 3;
	const struct pg_evidence *context = map->premises[1];
	const struct pg_evidence *arguments = pg_prove_substitution(typing,
		parameters->premises[0], context, prefix, map->premises + 2);
	return pg_prove_constructor(typing, formation, constructor,
		arguments, count, map->premises + prefix + 3);
}

const struct pg_evidence *pg_prove_constructor_function(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters)
{
	const struct pg_evidence *map = pg_prove_constructor_scope(typing, formation, constructor, parameters);
	if (!map) return NULL;
	const struct pg_evidence *body = constructor_in_scope(typing, formation, constructor, parameters, map);
	body = pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, body);
	if (!body) return NULL;
	return pg_prove_abstract(typing, parameters->premises[1], map->premises[1], body);
}

/* Check Gamma, indices, z : Family(parameters, indices), not a fixed fiber.
 * The ordinary family application rule checks each dependent index domain. */
int pg_inductive_motive_context_valid(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return 0;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return 0;
	if (pg_evidence_context(parameters->premises[0]) != pg_evidence_context(formation)) return 0;
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return 0;
	const struct pg_data_schema *schema = formation->certificate;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(pg_data_schema_indices(schema)),
		pg_evidence_context(formation->premises[0]), &count)) return 0;
	const struct pg_evidence *indices = motive_context->premises[0], *prefix = indices;
	struct pg_graph temporary = {0};
	int valid = 0;
	if (count > SIZE_MAX / sizeof(const struct pg_object *)) return 0;
	const struct pg_object **binders = pg_alloc(&temporary, count * sizeof(*binders));
	if (count && !binders) goto done;
	for (size_t i = count; i; --i, prefix = prefix->premises[0]) {
		if (prefix->rule != PG_CONTEXT_EXTEND) goto done;
		binders[i - 1] = pg_evidence_context(prefix)->binder;
	}
	if (pg_evidence_context(prefix) != pg_evidence_context(parameters)) goto done;
	const struct pg_evidence *family = pg_prove_projection(typing, indices,
		pg_prove_reindex(typing, parameters, formation));
	for (size_t i = 0; family && i < count; ++i)
		family = pg_prove_family_application(typing, family, pg_prove_variable(typing, indices, binders[i]));
	valid = family && pg_alpha_equal(pg_evidence_subject(family)->core, pg_evidence_context(motive_context)->declared_type) == 1;
done:
	pg_graph_destroy(&temporary);
	return valid;
}

/* Pull back along (Gamma projection, actual indices, value). Constructor
 * membership retains its result map; variables retain their fiber formation.
 * This is checked substitution, not a search for an expected result type. */
const struct pg_evidence *pg_prove_inductive_motive_substitution(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *source,
	const struct pg_evidence *destination, const struct pg_evidence *value)
{
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, source)) return NULL;
	const struct pg_evidence *prefix = pg_prove_substitution_projection(typing, parameters->premises[1], destination);
	if (pg_evidence_judgement(formation) == PG_JUDGEMENT_TYPE_FAMILY) {
		const struct pg_evidence *type = pg_prove_classifier(typing, destination, value);
		struct pg_inductive_instance instance;
		if (!pg_inductive_instance(typing, type, &instance)) return NULL;
		if (instance.formation != formation || !instance.indices) return NULL;
		size_t offset = instance.parameters->premise_count + 1;
		size_t count = instance.indices->premise_count - offset;
		prefix = pg_prove_substitution_extend(typing, prefix, source->premises[0],
			count, instance.indices->premises + offset);
	}
	return pg_prove_substitution_pair(typing, prefix, source, value);
}

struct refinement_binding {
	struct pg_index_entry index;
	const struct pg_object *binder;
	const struct pg_evidence *image;
	int seen;
};

static struct refinement_binding *refinement_find(const struct pg_index *index,
	const struct pg_object *binder)
{
	for (struct pg_index_entry *entry = pg_index_candidates(index, (uintptr_t)binder); entry; entry = entry->next) {
		struct refinement_binding *binding = (struct refinement_binding *)entry;
		if (binding->binder == binder) return binding;
	}
	return NULL;
}

const struct pg_evidence *pg_prove_constructor_refinement(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *scrutinee, const struct pg_object *constructor)
{
	if (!context_proof(typing, context) || !pg_evidence_owned_by(scrutinee, typing)) return NULL;
	if (pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE || pg_evidence_context(scrutinee) != pg_evidence_context(context)) return NULL;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_prove_classifier(typing, context, scrutinee), &instance)) return NULL;
	size_t first = instance.parameters->premise_count + 1;
	size_t indices = instance.indices ? instance.indices->premise_count - first : 0;
	if (indices >= SIZE_MAX / sizeof(struct refinement_binding)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index replacements;
	if (pg_index_init(&replacements)) return NULL;
	const struct pg_evidence *result = NULL;
	struct refinement_binding *bindings = pg_alloc(&temporary, (indices + 1) * sizeof(*bindings));
	if (!bindings) goto done;
	for (size_t i = 0; i <= indices; ++i) {
		const struct pg_term *term = i == indices ? pg_evidence_subject(scrutinee)->core : pg_evidence_subject(instance.indices->premises[first + i])->core;
		if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) goto done;
		const struct pg_object *binder = term->as.reference;
		if (refinement_find(&replacements, binder)) goto done;
		bindings[i].binder = binder;
		if (pg_index_insert(&replacements, &bindings[i].index, (uintptr_t)binder)) goto done;
	}
	const struct pg_evidence *prefix = context;
	size_t remaining = indices + 1, count = 0;
	while (remaining) {
		if (!pg_evidence_context(prefix) || count == SIZE_MAX) goto done;
		struct refinement_binding *binding = refinement_find(&replacements, pg_evidence_context(prefix)->binder);
		if (binding && !binding->seen) { binding->seen = 1; --remaining; }
		++count;
		prefix = prefix->premises[0];
	}
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	if (!extensions) goto done;
	const struct pg_evidence *extension = context;
	for (size_t i = count; i; --i, extension = extension->premises[0]) extensions[i - 1] = extension;
	const struct pg_evidence *parameters = pg_prove_substitution_rebase(typing, prefix, instance.parameters);
	const struct pg_evidence *fields = pg_prove_constructor_scope(typing, instance.formation, constructor, parameters);
	if (!fields) goto done;
	const struct pg_evidence *value = constructor_in_scope(typing, instance.formation, constructor, parameters, fields);
	const struct pg_evidence *fiber = pg_data_result(typing, instance.schema, constructor, fields);
	if (!value || !fiber || fiber->premise_count != first + indices) goto done;
	for (size_t i = 0; i < indices; ++i) bindings[i].image = fiber->premises[first + i];
	bindings[indices].image = value;
	const struct pg_evidence *map = pg_prove_substitution_projection(typing, prefix, fields->premises[1]);
	for (size_t i = 0; map && i < count; ++i) {
		extension = extensions[i];
		struct refinement_binding *binding = refinement_find(&replacements, pg_evidence_context(extension)->binder);
		if (binding) map = pg_prove_substitution_pair(typing, map, extension,
			pg_prove_projection(typing, map->premises[1], binding->image));
		else map = pg_prove_substitution_lift(typing, map, extension, pg_binder(typing->graph));
	}
	result = map;
done:
	pg_index_destroy(&replacements);
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_inductive_motive_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters,
	const struct pg_evidence *source, const struct pg_evidence *motive,
	const struct pg_evidence *destination, const struct pg_evidence *value)
{
	if (!context_proof(typing, source)) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(motive) != pg_evidence_context(source)) return NULL;
	const struct pg_evidence *map = pg_prove_inductive_motive_substitution(typing,
		formation, parameters, source, destination, value);
	return pg_prove_reindex(typing, map, motive);
}

static int unspecified_computation_result(const struct pg_term *type)
{
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	while (pg_pi_view(type, &domain, &binder, &body)) type = body;
	enum pg_totality totality;
	const struct pg_effect_row *effects;
	return pg_computation_type_view(type, &totality, &effects, &body) && totality == PG_TOTALITY_UNSPECIFIED;
}

const struct pg_evidence *pg_prove_inductive_hypothesis_type(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive, const struct pg_evidence *context,
	const struct pg_evidence *field)
{
	if (!pg_evidence_owned_by(field, typing) || pg_evidence_judgement(field) != PG_JUDGEMENT_VALUE) return NULL;
	if (!context_proof(typing, context) || pg_evidence_context(field) != pg_evidence_context(context)) return NULL;
	const struct pg_term *type;
	if (!pg_thunk_type_view(pg_evidence_subject(field)->classifier, &type)) {
		const struct pg_evidence *at = pg_prove_inductive_motive_at(typing,
			formation, parameters, motive_context, motive, context, field);
		return pg_prove_thunk_type(typing, at);
	}
	const struct pg_evidence *scope = context;
	const struct pg_evidence *call = pg_prove_force(typing, field);
	const struct pg_evidence *classifier = pg_prove_classifier(typing, scope, call);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	while (classifier && pg_pi_view(pg_evidence_subject(classifier)->core, &domain, &binder, &codomain)) {
		binder = pg_binder(typing->graph);
		const struct pg_evidence *argument_type = pg_prove_pi_domain(typing, classifier);
		scope = pg_prove_context_extension(typing, scope, binder, argument_type);
		if (!scope) return NULL;
		call = pg_prove_application(typing, pg_prove_projection(typing, scope, call),
			pg_prove_variable(typing, scope, binder));
		classifier = pg_prove_classifier(typing, scope, call);
	}
	enum pg_totality field_totality;
	const struct pg_effect_row *effects;
	if (!classifier || !pg_computation_type_view(pg_evidence_subject(classifier)->core, &field_totality, &effects, &type)) return NULL;
	if (pg_effect_count(effects)) return NULL;
	const struct pg_evidence *at;
	if (field_totality == PG_TOTALITY_TOTAL) {
		const struct pg_evidence *result = pg_prove_total_pure_value(typing, call);
		at = pg_prove_inductive_motive_at(typing,
			formation, parameters, motive_context, motive, scope, result);
		goto abstract;
	}
	const struct pg_evidence *returned_type = pg_prove_return_content(typing, classifier);
	binder = pg_binder(typing->graph);
	const struct pg_evidence *returned = pg_prove_context_extension(typing, scope, binder, returned_type);
	at = pg_prove_inductive_motive_at(typing,
		formation, parameters, motive_context, motive, returned, pg_prove_variable(typing, returned, binder));
	if (!at) return NULL;
	/* Calling a recursive function field precedes recursion on its result.
	 * The IH cannot promise termination that this field does not provide. */
	enum pg_totality motive_totality;
	if (pg_computation_type_view(pg_evidence_subject(at)->core, &motive_totality, &effects, &type)) {
		at = pg_prove_computation_type(typing, field_totality, effects,
			pg_prove_return_content(typing, at));
	} else if (!unspecified_computation_result(pg_evidence_subject(at)->core)) return NULL;
	/* Fold the returned recursive value once. Its result classifier must not
	 * escape with that value's binder; indices may depend on the Pi arguments. */
	at = pg_prove_pi_constant_codomain(typing,
		pg_prove_pi(typing, returned, at));
abstract:
	while (at && pg_evidence_context(scope) != pg_evidence_context(context)) {
		at = pg_prove_pi(typing, scope, at);
		scope = scope->premises[0];
	}
	return pg_prove_thunk_type(typing, at);
}

static const struct pg_evidence *prove_induction_scope(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_context *allocation, int retained)
{
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(motive) != pg_evidence_context(motive_context)) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, motive_context)) return NULL;
	const struct pg_evidence *fields = pg_data_schema_fields(formation->certificate, constructor);
	if (!fields) return NULL;
	const struct pg_object *self = pg_evidence_context(formation->premises[0])->binder;
	size_t prefix = parameters->premise_count - 2;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(fields), pg_evidence_context(formation->premises[0]), &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_term *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	unsigned char *recursive_fields = pg_alloc(&temporary, count);
	if (count && !recursive_fields) goto done;
	size_t recursive_count = 0;
	for (size_t i = count; i; --i, fields = fields->premises[0]) {
		int recursive = pg_data_recursive_field(pg_evidence_context(fields)->declared_type, self);
		if (recursive < 0) goto done;
		recursive_fields[i - 1] = (unsigned char)recursive;
		recursive_count += (size_t)recursive;
	}
	const struct pg_object **ih_binders = NULL;
	if (retained) {
		size_t supplied;
		if (recursive_count > SIZE_MAX - count ||
			pg_context_extension_size(allocation, pg_evidence_context(parameters->premises[1]), &supplied) ||
			supplied != count + recursive_count) goto done;
		ih_binders = pg_alloc(&temporary, recursive_count * sizeof(*ih_binders));
		if (recursive_count && !ih_binders) goto done;
		for (size_t i = recursive_count; i; --i, allocation = allocation->parent)
			ih_binders[i - 1] = allocation->binder;
	}
	const struct pg_evidence *map = retained
		? pg_prove_constructor_scope_at(typing, formation, constructor, parameters, allocation)
		: pg_prove_constructor_scope(typing, formation, constructor, parameters);
	if (!map) goto done;
	const struct pg_evidence *context = map->premises[1];
	size_t next_ih = 0;
	for (size_t i = 0; i < count; ++i) {
		if (!recursive_fields[i]) continue;
		const struct pg_evidence *field = pg_prove_projection(typing, context, map->premises[prefix + 3 + i]);
		const struct pg_evidence *ih = pg_prove_inductive_hypothesis_type(typing, formation, parameters,
			motive_context, motive, context, field);
		const struct pg_object *binder = retained ? ih_binders[next_ih++] : pg_binder(typing->graph);
		context = pg_prove_context_extension(typing, context, binder, ih);
		if (!context) goto done;
	}
	const struct pg_evidence *projection = pg_prove_substitution_projection(typing, map->premises[1], context);
	result = pg_prove_substitution_compose(typing, map, projection);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_induction_scope(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive)
{
	return prove_induction_scope(typing, formation, constructor, parameters,
		motive_context, motive, NULL, 0);
}

const struct pg_evidence *pg_prove_induction_scope_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_context *allocation)
{
	return prove_induction_scope(typing, formation, constructor, parameters,
		motive_context, motive, allocation, 1);
}

const struct pg_evidence *pg_prove_induction_case(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *branch)
{
	const struct pg_evidence *map = pg_prove_induction_scope(typing,
		formation, constructor, parameters, motive_context, motive);
	if (!map) return NULL;
	const struct pg_evidence *context = map->premises[1];
	const struct pg_evidence *body = pg_prove_projection(typing, context, branch);
	for (size_t i = parameters->premise_count + 1; body && i < map->premise_count; ++i)
		body = pg_prove_application(typing, body, map->premises[i]);
	return pg_prove_abstract(typing, parameters->premises[1], context, body);
}

static const struct pg_term *induction_field_core(struct pg_graph *graph,
	const struct pg_term *type, const struct pg_term *ih_type,
	const struct pg_term *field, const struct pg_object *recursion)
{
	const struct pg_term *call = pg_reference(graph, recursion);
	if (!pg_thunk_type_view(type, &type))
		return pg_application(graph, pg_reference(graph, &pg_thunk_operation),
			pg_application(graph, call, field));
	if (!pg_thunk_type_view(ih_type, &ih_type)) return NULL;
	struct argument { const struct pg_object *binder; struct argument *previous; };
	struct pg_graph temporary = {0};
	struct argument *arguments = NULL;
	const struct pg_term *body = pg_application(graph, pg_reference(graph, &pg_force_operation), field);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	while (pg_pi_view(type, &domain, &binder, &codomain)) {
		type = codomain;
		if (!pg_pi_view(ih_type, &domain, &binder, &ih_type)) { body = NULL; goto done; }
		struct argument *argument = pg_alloc(&temporary, sizeof(*argument));
		if (!argument) { body = NULL; goto done; }
		*argument = (struct argument){binder, arguments};
		arguments = argument;
		body = pg_application(graph, body, pg_reference(graph, binder));
	}
	body = pg_computation_fold(graph, body, call, 0, NULL);
	for (; body && arguments; arguments = arguments->previous)
		body = pg_lambda(graph, arguments->binder, body);
	body = pg_application(graph, pg_reference(graph, &pg_thunk_operation), body);
done:
	pg_graph_destroy(&temporary);
	return body;
}

static const struct pg_term *induction_branch_core(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *map, const struct pg_evidence *branch,
	const struct pg_object *recursion)
{
	size_t offset = parameters->premise_count + 1;
	size_t count = map->premise_count - offset;
	struct pg_graph temporary = {0};
	const struct pg_term **types = pg_alloc(&temporary, count * sizeof(*types));
	const struct pg_term **ih_types = pg_alloc(&temporary, count * sizeof(*ih_types));
	const struct pg_term *result = NULL;
	if (count && (!types || !ih_types)) goto done;
	const struct pg_evidence *fields = map->premises[0];
	for (size_t i = count; i; --i, fields = fields->premises[0])
		types[i - 1] = pg_evidence_context(fields)->declared_type;
	const struct pg_object *self = pg_evidence_context(formation->premises[0])->binder;
	const struct pg_context *ih_context = pg_evidence_context(map->premises[1]);
	for (size_t i = count; i; --i) {
		int recursive = pg_data_recursive_field(types[i - 1], self);
		if (recursive < 0) goto done;
		if (!recursive) continue;
		if (!ih_context->parent) goto done;
		ih_types[i - 1] = ih_context->declared_type;
		ih_context = ih_context->parent;
	}
	const struct pg_term *body = pg_evidence_subject(branch)->core;
	for (size_t i = 0; i < count; ++i)
		body = pg_application(typing->graph, body, pg_evidence_subject(map->premises[offset + i])->core);
	for (size_t i = 0; i < count; ++i) {
		if (!ih_types[i]) continue;
		const struct pg_term *call = induction_field_core(typing->graph, types[i], ih_types[i],
			pg_evidence_subject(map->premises[offset + i])->core, recursion);
		if (!call) goto done;
		body = pg_application(typing->graph, body, call);
	}
	for (size_t i = count; i; --i) {
		const struct pg_term *field = pg_evidence_subject(map->premises[offset + i - 1])->core;
		if (field->kind != PG_REFERENCE || field->as.reference->kind != PG_BINDER) goto done;
		body = pg_lambda(typing->graph, field->as.reference, body);
	}
	result = body;
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_match_branch_type(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *fields)
{
	if (!typing) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(fields, typing) || fields->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (fields->premise_count <= parameters->premise_count) return NULL;
	const struct pg_evidence *context = fields->premises[1];
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(parameters), &count)) return NULL;
	const struct pg_evidence *value = constructor_in_scope(typing, formation, constructor, parameters, fields);
	const struct pg_evidence *expected = pg_prove_inductive_motive_at(typing,
		formation, parameters, motive_context, motive, context, value);
	for (size_t i = 0; expected && i < count; ++i, context = context->premises[0])
		expected = pg_prove_pi(typing, context, expected);
	return expected;
}

struct induction_request {
	struct pg_index_entry index;
	const struct pg_evidence *result;
};

/* Omitting lexical allocation is a construction request, not an equivalence
 * of all conclusions with the same premises. Explicit allocations must still
 * be checked independently and retain their exact Core/typed structure. */
static struct induction_request *induction_request(struct pg_typing *typing,
	size_t count, const struct pg_evidence *const *premises)
{
	if (!typing->induction_requests.capacity && pg_index_init(&typing->induction_requests)) return NULL;
	uint64_t hash = count;
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&typing->induction_requests, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		struct induction_request *request = (void *)p;
		const struct pg_evidence *result = request->result;
		if (result->premise_count != count) continue;
		size_t i = 0;
		while (i < count && result->premises[i] == premises[i]) ++i;
		if (i == count) return request;
	}
	struct induction_request *request = pg_alloc(typing->graph, sizeof(*request));
	if (request) request->index.hash = hash;
	return request;
}

static const struct pg_evidence *prove_data_elimination(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches, enum pg_evidence_rule rule,
	const struct pg_induction_allocation *allocation)
{
	if (allocation) {
		if (rule != PG_INDUCTION_ELIM || allocation->count != count || (count && !allocation->clauses)) return NULL;
		if (!allocation->recursion || allocation->recursion->kind != PG_BINDER) return NULL;
		if (!allocation->argument || allocation->argument->kind != PG_BINDER) return NULL;
		if (!allocation->self || allocation->self->kind != PG_BINDER) return NULL;
		if (allocation->recursion == allocation->argument || allocation->recursion == allocation->self ||
			allocation->argument == allocation->self) return NULL;
	}
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(parameters->premises[0]) != pg_evidence_context(formation)) return NULL;
	const struct pg_evidence *destination = parameters->premises[1];
	if (allocation) {
		const struct pg_object *binders[] = {allocation->recursion, allocation->argument, allocation->self};
		for (size_t i = 0; i < 3; ++i) {
			if (pg_context_lookup(pg_evidence_context(destination), binders[i])) return NULL;
			for (size_t j = 0; j < count; ++j)
				if (pg_context_lookup(allocation->clauses[j], binders[i])) return NULL;
		}
	}
	if (!pg_evidence_owned_by(scrutinee, typing) || pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_context(scrutinee) != pg_evidence_context(destination)) return NULL;
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, motive_context)) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(motive) != pg_evidence_context(motive_context)) return NULL;
	const struct pg_data_schema *schema = formation->certificate;
	if (count != pg_data_constructor_count(schema)) return NULL;
	if (count && !branches) return NULL;
	if (count > SIZE_MAX / sizeof(void *) - 6) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_match_clause)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 6) * sizeof(*premises));
	if (!premises) goto done;
	/* Lookup precedes fresh field contexts. Reusing a rule never rebuilds them. */
	premises[0] = motive; premises[1] = formation; premises[2] = parameters;
	premises[3] = scrutinee; premises[4] = motive_context;
	for (size_t i = 0; i < count; ++i) {
		if (!pg_evidence_owned_by(branches[i], typing)) goto done;
		if (pg_evidence_judgement(branches[i]) != PG_JUDGEMENT_COMPUTATION) goto done;
		if (pg_evidence_context(branches[i]) != pg_evidence_context(destination)) goto done;
		premises[i + 5] = branches[i];
	}
	const struct pg_evidence *output = pg_prove_inductive_motive_at(typing, formation, parameters,
		motive_context, motive, destination, scrutinee);
	if (!output) goto done;
	premises[count + 5] = output;
	struct induction_request *request = NULL;
	if (rule == PG_INDUCTION_ELIM) {
		if (!allocation) {
			request = induction_request(typing, count + 6, premises);
			if (!request) goto done;
			result = request->result;
		} else {
			uint64_t hash;
			struct pg_occurrence selector = {.induction = allocation};
			result = find_record(typing, rule, pg_evidence_context(destination),
				&selector, count + 6, premises, NULL, &hash);
		}
	} else {
		uint64_t hash;
		result = find_record(typing, rule,
			pg_evidence_context(destination), NULL, count + 6, premises, NULL, &hash);
	}
	if (result) goto done;
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	struct pg_match_clause *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 3) * sizeof(*operands));
	if ((count && !clauses) || !operands) goto done;
	operands[0] = pg_evidence_subject(scrutinee);
	operands[count + 1] = pg_evidence_subject(motive);
	operands[count + 2] = pg_evidence_subject(formation);
	struct pg_induction_allocation *saved = NULL;
	const struct pg_context **contexts = NULL;
	if (rule == PG_INDUCTION_ELIM) {
		saved = pg_alloc(&temporary, sizeof(*saved));
		contexts = pg_alloc(&temporary, count * sizeof(*contexts));
		if (!saved || (count && !contexts)) goto done;
		*saved = (struct pg_induction_allocation){
			.recursion = allocation ? allocation->recursion : pg_binder(typing->graph),
			.argument = allocation ? allocation->argument : pg_binder(typing->graph),
			.self = allocation ? allocation->self : pg_binder(typing->graph),
			.count = count, .clauses = contexts};
		if (!saved->recursion || !saved->argument || !saved->self) goto done;
	}
	const struct pg_object *recursion = saved ? saved->recursion : NULL;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_object *constructor = pg_data_constructor(layout, i);
		const struct pg_evidence *map = allocation
			? pg_prove_induction_scope_at(typing, formation, constructor, parameters,
				motive_context, motive, allocation->clauses[i])
			: rule == PG_INDUCTION_ELIM
			? pg_prove_induction_scope(typing,
		formation, constructor, parameters, motive_context, motive)
			: pg_prove_constructor_scope(typing, formation, constructor, parameters);
		if (!map) goto done;
		const struct pg_evidence *context = map->premises[1];
		if (contexts) contexts[i] = allocation ? allocation->clauses[i] : pg_evidence_context(context);
		const struct pg_evidence *expected = pg_prove_match_branch_type(typing,
			formation, constructor, parameters, motive_context, motive, map);
		if (!expected || pg_alpha_equal(pg_evidence_subject(expected)->core, pg_evidence_subject(branches[i])->classifier) != 1) goto done;
		clauses[i] = (struct pg_match_clause){constructor, pg_evidence_subject(branches[i])->core};
		if (recursion) clauses[i].branch = induction_branch_core(typing, formation, parameters, map, branches[i], recursion);
		if (!clauses[i].branch) goto done;
		operands[i + 1] = pg_evidence_subject(branches[i]);
	}
	const struct pg_term *core = recursion
		? pg_data_recursive_match(typing->graph, layout, recursion,
			saved->argument, saved->self, pg_evidence_subject(scrutinee)->core, count, clauses)
		: pg_data_match(typing->graph, layout, pg_evidence_subject(scrutinee)->core, count, clauses);
	if (!core) goto done;
	const struct pg_context_map *parameter_map = pg_evidence_context_map(parameters);
	const struct pg_occurrence *subject = pg_occurrence_intern(typing, &(struct pg_occurrence){
		.judgement = PG_JUDGEMENT_COMPUTATION, .context = pg_evidence_context(destination),
		.core = core, .classifier = pg_evidence_subject(output)->core, .type = pg_evidence_subject(output),
		.operand_count = count + 3, .map_count = 1, .induction = saved}, operands, &parameter_map);
	if (!subject) goto done;
	result = accept_record(typing, rule,
		pg_evidence_context(destination), subject, count + 6, premises, NULL, NULL);
	if (result && request) {
		request->result = result;
		if (pg_index_insert(&typing->induction_requests, &request->index, request->index.hash)) result = NULL;
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* The checked rule identifies the eliminator theorem. Its program inputs and
 * lexical scopes come from the conclusion, not the premise array layout. */
struct elimination_structure {
	const struct pg_occurrence *subject;
	const struct pg_evidence *formation, *parameters, *motive_context, *motive, *scrutinee;
	size_t count;
};

static int elimination_inputs(struct pg_typing *typing,
	const struct pg_occurrence *subject, struct elimination_structure *view)
{
	if (subject->operand_count < 3 || subject->map_count != 1) return -1;
	size_t count = subject->operand_count - 3;
	*view = (struct elimination_structure){.subject = subject, .count = count,
		.formation = pg_prove_structural_subject(typing, subject->operands[count + 2]),
		.parameters = pg_prove_context_map(typing, pg_occurrence_maps(subject)[0]),
		.motive = pg_prove_structural_subject(typing, subject->operands[count + 1]),
		.motive_context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, subject->operands[count + 1]->context),
		.scrutinee = pg_prove_structural_subject(typing, subject->operands[0])};
	if (!view->formation || view->formation->rule != PG_INDUCTIVE_FORM) return -1;
	return view->parameters && view->motive && view->motive_context && view->scrutinee ? 0 : -1;
}

static int elimination_structure(struct pg_typing *typing,
	const struct pg_evidence *proof, struct elimination_structure *view)
{
	if (!pg_evidence_owned_by(proof, typing)) return -1;
	if (proof->rule != PG_MATCH_ELIM && proof->rule != PG_INDUCTION_ELIM) return -1;
	return elimination_inputs(typing, pg_evidence_subject(proof), view);
}

const struct pg_evidence *pg_prove_construction_origin(struct pg_typing *typing,
	const struct pg_evidence *proof,
	const struct pg_evidence **environment)
{
	if (!typing || !environment) return NULL;
	struct pg_typed_query *work = pg_construction_origin_request(typing, proof);
	while (!pg_typed_query_advance(work, 1024)) {}
	const struct pg_evidence *result = pg_typed_query_result(work);
	if (result) *environment = pg_construction_origin_environment(work);
	return result;
}

static const struct pg_evidence *elimination_instance(struct pg_typing *typing,
	const struct pg_evidence *substitution,
	const struct pg_evidence *elimination, const struct pg_evidence *scrutinee)
{
	struct elimination_structure view;
	if (elimination_structure(typing, elimination, &view)) return NULL;
	if (!pg_evidence_owned_by(substitution, typing) || substitution->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(substitution->premises[0]) != pg_evidence_context(elimination)) return NULL;
	struct pg_occurrence_input *query = pg_occurrence_input_mapped_request(typing,
		view.subject, view.count + 1, pg_evidence_context_map(substitution));
	while (pg_occurrence_input_advance(query, 1024) == PG_INPUT_PENDING) {}
	const struct pg_occurrence *input = pg_occurrence_input_result(query);
	const struct pg_evidence *motive = input ? pg_prove_structural_subject(typing, input) : NULL;
	if (!motive) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	size_t count = view.count;
	const struct pg_evidence **branches = pg_alloc(&temporary, count * sizeof(*branches));
	if (count && !branches) goto done;
	for (size_t i = 0; i < count; ++i) {
		branches[i] = pg_prove_reindex(typing, substitution,
			pg_prove_structural_subject(typing, view.subject->operands[i + 1]));
		if (!branches[i]) goto done;
	}
	result = prove_data_elimination(typing, view.formation,
		pg_prove_substitution_compose(typing, view.parameters, substitution),
		scrutinee ? scrutinee : pg_prove_reindex(typing, substitution, view.scrutinee),
		conclusion_first(typing, PG_JUDGEMENT_CONTEXT, input->context), motive,
		count, branches, elimination->rule, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_elimination_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution,
	const struct pg_evidence *elimination)
{
	return elimination_instance(typing, substitution, elimination, NULL);
}

static const struct pg_evidence *constructor_head(struct pg_typing *typing,
	const struct pg_evidence *value, struct constructor_structure *view,
	struct pg_typed_query **dependency)
{
	if (!pg_evidence_owned_by(value, typing) || pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_context *context = pg_evidence_context(value);
	struct pg_typed_query *head_query = typed_head_request(typing, value, NULL);
	if (!head_query) return NULL;
	if (dependency) {
		*dependency = head_query;
		if (!head_query->status) return NULL;
		*dependency = NULL;
	} else while (!pg_typed_query_advance(head_query, 1024)) {}
	value = pg_typed_query_result(head_query);
	if (!value || pg_evidence_context(value) != context) return NULL;
	return constructor_view(typing, value, view) == 1 ? value : NULL;
}

static int constructor_structure(struct pg_typing *typing, struct pg_graph *temporary,
	const struct pg_evidence *value, struct constructor_structure *view,
	struct pg_typed_query **dependency)
{
	value = constructor_head(typing, value, view, dependency);
	if (!value) return 0;
	const struct pg_occurrence *current = pg_evidence_subject(value);
	size_t count = view->count;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return 0;
	const struct pg_evidence **fields = pg_alloc(temporary, count * sizeof(*fields));
	if (count && !fields) return 0;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *input;
		if (!structural_input(typing, current, i, &input)) return 0;
		fields[i] = pg_prove_structural_subject(typing, input);
		if (!fields[i]) return 0;
	}
	view->fields = fields;
	return 1;
}

const struct pg_evidence *pg_prove_constructor_field(struct pg_typing *typing,
	const struct pg_evidence *value, const struct pg_object *field)
{
	struct constructor_structure view;
	value = constructor_head(typing, value, &view, NULL);
	if (!value) return NULL;
	const struct pg_occurrence *subject = pg_evidence_subject(value);
	const struct pg_context *scope = pg_evidence_context(pg_data_schema_fields(view.formation->certificate, view.constructor));
	const struct pg_term *spine = subject->core;
	for (size_t i = view.count; i; --i, scope = scope->parent, spine = spine->as.application.function) {
		if (scope->binder != field) continue;
		const struct pg_occurrence *input;
		if (!structural_input(typing, subject, i - 1, &input) || !input) return NULL;
		if (input->context != subject->context || pg_alpha_equal(input->core, spine->as.application.argument) != 1) return NULL;
		return pg_prove_structural_subject(typing, input);
	}
	return NULL;
}

static const struct pg_evidence *elimination_branch(struct pg_typing *typing,
	struct pg_graph *temporary, const struct elimination_structure *view,
	struct constructor_structure *value,
	struct pg_typed_query **dependency)
{
	if (!constructor_structure(typing, temporary, view->scrutinee, value, dependency)) return NULL;
	if (pg_evidence_subject(value->formation) != pg_evidence_subject(view->formation)) return NULL;
	const struct pg_data_schema *schema = view->formation->certificate;
	size_t position;
	if (!pg_data_constructor_position(pg_data_schema_layout(schema), value->constructor, &position)) return NULL;
	if (position >= view->count) return NULL;
	return pg_prove_structural_subject(typing, view->subject->operands[position + 1]);
}

static int typed_body_match(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_evidence *source = pg_prove_structural_subject(typing, work->current);
	if (work->environment)
		source = pg_prove_elimination_reindex(typing, work->environment, source);
	work->environment = NULL;
	work->dependency = pg_elimination_body_request(typing, source);
	work->resume = TYPED_RESUME_BODY;
	return work->dependency ? 0 : -1;
}

static const struct pg_evidence *induction_field_body(struct pg_typing *typing,
	const struct pg_evidence *elimination,
	const struct elimination_structure *view, const struct pg_evidence *field)
{
	const struct pg_term *type;
	if (!pg_thunk_type_view(pg_evidence_subject(field)->classifier, &type)) {
		struct pg_graph temporary = {0};
		const struct pg_evidence **branches = pg_alloc(&temporary, view->count * sizeof(*branches));
		const struct pg_evidence *result = NULL;
		if (branches) {
			for (size_t i = 0; i < view->count; ++i)
				branches[i] = pg_prove_structural_subject(typing, view->subject->operands[i + 1]);
			result = pg_prove_induction_at(typing, view->formation, view->parameters,
				field, view->motive_context, view->motive, view->count, branches,
				pg_evidence_induction_allocation(elimination));
		}
		pg_graph_destroy(&temporary);
		return result;
	}
	const struct pg_evidence *context = view->parameters->premises[1], *scope = context;
	const struct pg_evidence *call = pg_prove_force(typing, field);
	const struct pg_evidence *classifier = pg_prove_classifier(typing, scope, call);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	while (classifier && pg_pi_view(pg_evidence_subject(classifier)->core, &domain, &binder, &codomain)) {
		binder = pg_binder(typing->graph);
		scope = pg_prove_context_extension(typing, scope, binder, pg_prove_pi_domain(typing, classifier));
		if (!scope) return NULL;
		call = pg_prove_application(typing, pg_prove_projection(typing, scope, call),
			pg_prove_variable(typing, scope, binder));
		classifier = pg_prove_classifier(typing, scope, call);
	}
	const struct pg_evidence *returned = pg_prove_context_extension(typing, scope,
		pg_binder(typing->graph), pg_prove_return_content(typing, classifier));
	if (!returned) return NULL;
	const struct pg_evidence *map = pg_prove_substitution_projection(typing, context, returned);
	const struct pg_evidence *value = pg_prove_variable(typing, returned, pg_evidence_context(returned)->binder);
	if (!value) return NULL;
	const struct pg_evidence *child = elimination_instance(typing, map, elimination,
		value);
	/* Demand the field result before recursion, exactly as induction_field_core.
	 * Fold rejects a result classifier escaping with the returned-value binder. */
	const struct pg_evidence *continuation = pg_prove_abstract(typing, scope, returned, child);
	const struct pg_evidence *body = pg_prove_fold(typing, call, continuation);
	return pg_prove_abstract(typing, context, scope, body);
}

static int typed_elimination_prepare(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_evidence *elimination = pg_prove_structural_subject(typing, work->source);
	struct elimination_structure view;
	if (elimination_structure(typing, elimination, &view)) return -1;
	struct pg_graph temporary = {0};
	struct constructor_structure value;
	int status = -1;
	const struct pg_evidence *branch = elimination_branch(typing, &temporary, &view, &value, &work->dependency);
	if (!branch) {
		status = work->dependency ? 0 : -1;
		goto done;
	}
	const struct pg_data_schema *schema = view.formation->certificate;
	size_t count = value.count;
	if (count > SIZE_MAX / 2 / sizeof(const struct pg_evidence *)) goto done;
	struct typed_elimination *state = pg_alloc(typing->graph, sizeof(*state));
	if (!state) goto done;
	*state = (struct typed_elimination){.branch = branch, .count = count};
	state->arguments = pg_alloc(typing->graph, 2 * count * sizeof(*state->arguments));
	if (count && !state->arguments) goto done;
	for (size_t i = 0; i < count; ++i) state->arguments[i] = value.fields[i];
	if (elimination->rule == PG_INDUCTION_ELIM) {
		const struct pg_context *declaration = pg_evidence_context(pg_data_schema_fields(schema, value.constructor));
		const struct pg_object *self = pg_evidence_context(view.formation->premises[0])->binder;
		for (size_t i = count; i; --i, declaration = declaration->parent) {
			int kind = pg_data_recursive_field(declaration->declared_type, self);
			if (kind < 0) goto done;
			state->arguments[count + i - 1] = kind ? value.fields[i - 1] : NULL;
		}
		state->count += count;
		for (size_t i = count; i < state->count; ++i) {
			if (!state->arguments[i]) continue;
			const struct pg_evidence *call = induction_field_body(typing, elimination, &view, state->arguments[i]);
			state->arguments[i] = pg_prove_thunk(typing, call);
			if (!state->arguments[i]) goto done;
		}
	}
	work->elimination = state;
	status = 0;
done:
	pg_graph_destroy(&temporary);
	return status;
}

static int typed_elimination_step(struct pg_typed_query *work)
{
	if (!work->elimination) return typed_elimination_prepare(work);
	struct typed_elimination *state = work->elimination;
	if (work->dependency) {
		if (!work->dependency->status) return 0;
		state->branch = pg_typed_query_result(work->dependency);
		work->dependency = NULL;
		if (!state->branch) return -1;
	}
	if (state->next == state->count) {
		work->result = state->branch;
		return 1;
	}
	const struct pg_evidence *argument = state->arguments[state->next++];
	if (!argument) return 0;
	work->dependency = pg_application_body_request(work->typing, state->branch, argument);
	return work->dependency ? 0 : -1;
}

const struct pg_evidence *pg_prove_elimination_body(struct pg_typing *typing,
	const struct pg_evidence *elimination)
{
	if (!typing) return NULL;
	struct pg_typed_query *work = pg_elimination_body_request(typing, elimination);
	while (!pg_typed_query_advance(work, 1024)) {}
	return pg_typed_query_result(work);
}

static int factor_binding(struct pg_graph *temporary, struct pg_index *index,
	const struct pg_term *pattern, const struct pg_evidence *image)
{
	if (pattern->kind != PG_REFERENCE || pattern->as.reference->kind != PG_BINDER) return 0;
	struct refinement_binding *binding = refinement_find(index, pattern->as.reference);
	if (binding) return pg_alpha_equal(pg_evidence_subject(binding->image)->core, pg_evidence_subject(image)->core) == 1 ? 0 : -1;
	binding = pg_alloc(temporary, sizeof(*binding));
	if (!binding) return -1;
	binding->binder = pattern->as.reference; binding->image = image;
	return pg_index_insert(index, &binding->index, (uintptr_t)binding->binder);
}

const struct pg_evidence *pg_prove_refinement_factor(struct pg_typing *typing,
	const struct pg_evidence *refinement, const struct pg_evidence *instance,
	const struct pg_object *scrutinee)
{
	if (!pg_evidence_owned_by(refinement, typing) || refinement->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(instance, typing) || instance->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(refinement->premises[0]) != pg_evidence_context(instance->premises[0])) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index bindings;
	if (pg_index_init(&bindings)) return NULL;
	const struct pg_evidence *result = NULL;
	struct constructor_structure pattern, value;
	if (!constructor_structure(typing, &temporary, pg_substitution_image(typing, refinement, scrutinee), &pattern, NULL) ||
		!constructor_structure(typing, &temporary, pg_substitution_image(typing, instance, scrutinee), &value, NULL)) goto done;
	if (pattern.constructor != value.constructor || pattern.formation != value.formation || pattern.count != value.count) goto done;
	for (size_t i = 2; i < refinement->premise_count; ++i)
		if (factor_binding(&temporary, &bindings, pg_evidence_subject(refinement->premises[i])->core, instance->premises[i])) goto done;
	for (size_t i = 0; i < pattern.count; ++i)
		if (factor_binding(&temporary, &bindings, pg_evidence_subject(pattern.fields[i])->core, value.fields[i])) goto done;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(refinement), NULL, &count)) goto done;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	if (count && !images) goto done;
	const struct pg_context *scope = pg_evidence_context(refinement);
	for (size_t i = count; i; --i, scope = scope->parent) {
		struct refinement_binding *binding = refinement_find(&bindings, scope->binder);
		if (!binding) goto done;
		images[i - 1] = binding->image;
	}
	const struct pg_evidence *map = pg_prove_substitution(typing, refinement->premises[1], instance->premises[1], count, images);
	const struct pg_evidence *composite = pg_prove_substitution_compose(typing, refinement, map);
	if (!composite || composite->premise_count != instance->premise_count) goto done;
	for (size_t i = 2; i < composite->premise_count; ++i)
		if (pg_alpha_equal(pg_evidence_subject(composite->premises[i])->core, pg_evidence_subject(instance->premises[i])->core) != 1) goto done;
	result = map;
done:
	pg_index_destroy(&bindings);
	pg_graph_destroy(&temporary);
	return result;
}

/* A positional correspondence is only a candidate. The ordinary substitution
 * constructor checks every image against the preceding dependent telescope. */
static const struct pg_evidence *telescope_correspondence(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination)
{
	size_t count, other;
	if (pg_context_extension_size(pg_evidence_context(source), NULL, &count) ||
		pg_context_extension_size(pg_evidence_context(destination), NULL, &other) || count != other) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	const struct pg_context *scope = pg_evidence_context(destination);
	for (size_t i = count; i; --i, scope = scope->parent)
		images[i - 1] = pg_prove_variable(typing, destination, scope->binder);
	const struct pg_evidence *map = pg_prove_substitution(typing, source, destination, count, images);
	free(images);
	return map;
}

const struct pg_evidence *pg_prove_refined_match(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *scrutinee, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *refinements,
	const struct pg_evidence *const *branches)
{
	if (!context_proof(typing, context) || !pg_evidence_owned_by(motive, typing)) return NULL;
	if (pg_evidence_judgement(motive) != PG_JUDGEMENT_COMPUTATION_TYPE || pg_evidence_context(motive) != pg_evidence_context(context)) return NULL;
	if (!pg_evidence_owned_by(scrutinee, typing) || pg_evidence_context(scrutinee) != pg_evidence_context(context)) return NULL;
	if (count && (!refinements || !branches)) return NULL;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_prove_classifier(typing, context, scrutinee), &instance)) return NULL;
	if (count != pg_data_constructor_count(instance.schema)) return NULL;
	const struct pg_evidence *mc = pg_prove_inductive_motive_context(typing,
		instance.formation, instance.parameters, pg_binder(typing->graph));
	if (!mc) return NULL;
	if (!count) return pg_prove_match(typing, instance.formation, instance.parameters,
		scrutinee, mc, pg_prove_projection(typing, mc, motive), 0, NULL);
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index replacements;
	if (pg_index_init(&replacements)) return NULL;
	const struct pg_evidence *result = NULL, *prefix = NULL;
	const struct pg_evidence **functions = pg_alloc(&temporary, count * sizeof(*functions));
	if (!functions) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *map = refinements[i], *body = branches[i];
		if (!pg_evidence_owned_by(map, typing) || map->rule != PG_CONTEXT_SUBSTITUTION) goto done;
		if (pg_evidence_context(map->premises[0]) != pg_evidence_context(context)) goto done;
		if (!pg_evidence_owned_by(body, typing) || pg_evidence_context(body) != pg_evidence_context(map)) goto done;
		const struct pg_evidence *expected = pg_prove_constructor_refinement(typing, context,
			scrutinee, pg_data_constructor(pg_data_schema_layout(instance.schema), i));
		if (!expected) goto done;
		if (!i) {
			size_t left, right;
			prefix = context;
			const struct pg_evidence *target = expected->premises[1];
			if (pg_context_extension_size(pg_evidence_context(prefix), NULL, &left) ||
				pg_context_extension_size(pg_evidence_context(target), NULL, &right)) goto done;
			while (left > right) { prefix = prefix->premises[0]; --left; }
			while (right > left) { target = target->premises[0]; --right; }
			while (pg_evidence_context(prefix) != pg_evidence_context(target)) {
				prefix = prefix->premises[0]; target = target->premises[0];
			}
		}
		const struct pg_evidence *rename = telescope_correspondence(typing, expected->premises[1], map->premises[1]);
		expected = pg_prove_substitution_compose(typing, expected, rename);
		if (!expected || expected->premise_count != map->premise_count) goto done;
		for (size_t j = 2; j < map->premise_count; ++j)
			if (pg_alpha_equal(pg_evidence_subject(expected->premises[j])->core, pg_evidence_subject(map->premises[j])->core) != 1) goto done;
		const struct pg_evidence *lift = lift_scope(typing,
			pg_prove_substitution_projection(typing, prefix, context), map->premises[1], NULL, 0);
		if (!lift) goto done;
		functions[i] = pg_prove_abstract(typing, context, lift->premises[1],
			pg_prove_reindex(typing, lift, body));
		if (!functions[i]) goto done;
	}
	struct pg_inductive_instance generic;
	if (!pg_inductive_instance(typing, mc->premises[1], &generic)) goto done;
	size_t first = instance.parameters->premise_count + 1;
	size_t indices = instance.indices ? instance.indices->premise_count - first : 0;
	if (indices >= SIZE_MAX / sizeof(struct refinement_binding)) goto done;
	struct refinement_binding *bindings = pg_alloc(&temporary, (indices + 1) * sizeof(*bindings));
	if (!bindings) goto done;
	for (size_t i = 0; i <= indices; ++i) {
		const struct pg_term *term = i == indices ? pg_evidence_subject(scrutinee)->core : pg_evidence_subject(instance.indices->premises[first + i])->core;
		if (term->kind != PG_REFERENCE || term->as.reference->kind != PG_BINDER) goto done;
		bindings[i].binder = term->as.reference;
		bindings[i].image = i == indices ? pg_prove_variable(typing, mc, pg_evidence_context(mc)->binder)
			: pg_prove_projection(typing, mc, generic.indices->premises[first + i]);
		if (!bindings[i].image || refinement_find(&replacements, bindings[i].binder)) goto done;
		if (pg_index_insert(&replacements, &bindings[i].index, (uintptr_t)bindings[i].binder)) goto done;
	}
	size_t suffix;
	if (pg_context_extension_size(pg_evidence_context(context), pg_evidence_context(prefix), &suffix)) goto done;
	if (suffix > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	const struct pg_evidence **extensions = pg_alloc(&temporary, suffix * sizeof(*extensions));
	if (suffix && !extensions) goto done;
	const struct pg_evidence *scope = context;
	for (size_t i = suffix; i; --i, scope = scope->premises[0]) extensions[i - 1] = scope;
	const struct pg_evidence *map = pg_prove_substitution_projection(typing, prefix, mc);
	for (size_t i = 0; map && i < suffix; ++i) {
		struct refinement_binding *binding = refinement_find(&replacements, pg_evidence_context(extensions[i])->binder);
		if (binding) map = pg_prove_substitution_pair(typing, map, extensions[i],
			pg_prove_projection(typing, map->premises[1], binding->image));
		else map = pg_prove_substitution_lift(typing, map, extensions[i], pg_binder(typing->graph));
	}
	if (!map) goto done;
	const struct pg_evidence *generalized = pg_prove_reindex(typing, map, motive);
	for (scope = map->premises[1]; generalized && pg_evidence_context(scope) != pg_evidence_context(mc); scope = scope->premises[0])
		generalized = pg_prove_pi(typing, scope, generalized);
	result = pg_prove_match(typing, instance.formation, instance.parameters,
		scrutinee, mc, generalized, count, functions);
	for (size_t i = 0; result && i < suffix; ++i) {
		const struct pg_object *binder = pg_evidence_context(extensions[i])->binder;
		if (!refinement_find(&replacements, binder))
			result = pg_prove_application(typing, result, pg_prove_variable(typing, context, binder));
	}
done:
	pg_index_destroy(&replacements);
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_type_case(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	size_t count, const struct pg_evidence *const *branches)
{
	if (!typing) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(parameters->premises[0]) != pg_evidence_context(formation)) return NULL;
	if (!pg_evidence_owned_by(scrutinee, typing) || pg_evidence_judgement(scrutinee) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_context(scrutinee) != pg_evidence_context(parameters)) return NULL;
	const struct pg_data_schema *schema = formation->certificate;
	if (!count || count != pg_data_constructor_count(schema) || !branches) return NULL;
	if (count > SIZE_MAX / sizeof(void *) - 3 || count > SIZE_MAX / sizeof(struct pg_match_clause)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 3) * sizeof(*premises));
	if (!premises) goto done;
	premises[0] = formation; premises[1] = parameters; premises[2] = scrutinee;
	for (size_t i = 0; i < count; ++i) {
		if (!pg_evidence_owned_by(branches[i], typing)) goto done;
		if (pg_evidence_context(branches[i]) != pg_evidence_context(parameters)) goto done;
		premises[i + 3] = branches[i];
	}
	uint64_t hash;
	result = find_record(typing, PG_TYPE_CASE,
		pg_evidence_context(parameters), NULL, count + 3, premises, NULL, &hash);
	if (result) goto done;
	const struct pg_evidence *type = pg_prove_classifier(typing, parameters->premises[1], scrutinee);
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, type, &instance) || instance.formation != formation) goto done;
	if (instance.parameters->premise_count != parameters->premise_count) goto done;
	for (size_t i = 2; i < parameters->premise_count; ++i)
		if (pg_alpha_equal(pg_evidence_subject(instance.parameters->premises[i])->core, pg_evidence_subject(parameters->premises[i])->core) != 1) goto done;
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	struct pg_match_clause *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 1) * sizeof(*operands));
	if (!clauses || !operands) goto done;
	operands[0] = pg_evidence_subject(scrutinee);
	uint64_t level = 0;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_object *constructor = pg_data_constructor(layout, i);
		const struct pg_evidence *map = pg_prove_constructor_scope(typing, formation, constructor, parameters);
		if (!map) goto done;
		const struct pg_evidence *branch = pg_prove_projection(typing, map->premises[1], branches[i]);
		for (size_t j = parameters->premise_count + 1; branch && j < map->premise_count; ++j)
			branch = pg_prove_family_application(typing, branch, map->premises[j]);
		if (!branch || pg_evidence_judgement(branch) != PG_JUDGEMENT_VALUE_TYPE) goto done;
		uint64_t bound;
		if (!pg_universe_level(pg_evidence_subject(branch)->classifier, &bound)) goto done;
		if (bound > level) level = bound;
		clauses[i] = (struct pg_match_clause){constructor, pg_evidence_subject(branches[i])->core};
		operands[i + 1] = pg_evidence_subject(branches[i]);
	}
	const struct pg_term *core = pg_data_match(typing->graph, layout, pg_evidence_subject(scrutinee)->core, count, clauses);
	const struct pg_term *universe = pg_universe(typing->graph, level);
	if (!core || !universe) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(parameters), core, universe, NULL, count + 1, operands);
	if (!subject) goto done;
	result = accept(typing, PG_TYPE_CASE,
		pg_evidence_context(parameters), subject, count + 3, premises);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_match(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches)
{
	return prove_data_elimination(typing, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_MATCH_ELIM, NULL);
}

const struct pg_evidence *pg_prove_induction(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches)
{
	return prove_data_elimination(typing, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_INDUCTION_ELIM, NULL);
}

const struct pg_evidence *pg_prove_induction_at(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches,
	const struct pg_induction_allocation *allocation)
{
	if (!allocation) return NULL;
	return prove_data_elimination(typing, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_INDUCTION_ELIM, allocation);
}

const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing)
{
	return accept(typing, PG_CONTEXT_EMPTY, NULL, NULL, 0, NULL);
}

const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (pg_evidence_judgement(value) == PG_JUDGEMENT_VALUE_TYPE) return value;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return NULL;
	uint64_t level;
	if (!pg_universe_level(pg_evidence_subject(value)->classifier, &level)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_boundary(typing, pg_evidence_subject(value),
		PG_JUDGEMENT_VALUE_TYPE, pg_evidence_subject(value)->classifier);
	return subject ? accept(typing, PG_TYPE_FROM_VALUE,
		pg_evidence_context(value), subject, 1, &value) : NULL;
}

const struct pg_evidence *pg_prove_type_value(struct pg_typing *typing, const struct pg_evidence *type)
{
	if (!pg_evidence_owned_by(type, typing)) return NULL;
	if (pg_evidence_judgement(type) != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_boundary(typing, pg_evidence_subject(type),
		PG_JUDGEMENT_VALUE, pg_evidence_subject(type)->classifier);
	return subject ? accept(typing, PG_VALUE_FROM_TYPE,
		pg_evidence_context(type), subject, 1, &type) : NULL;
}

const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type)
{
	if (!context_proof(typing, parent)) return NULL;
	type = pg_prove_value_type(typing, type);
	if (!type) return NULL;
	if (!pg_evidence_subject(type) || pg_evidence_context(type) != pg_evidence_context(parent)) return NULL;
	uint64_t level;
	if (!pg_universe_level(pg_evidence_subject(type)->classifier, &level)) return NULL;
	if (pg_context_lookup(pg_evidence_context(parent), binder)) return NULL;
	const struct pg_context *context = pg_context_bind(typing, pg_evidence_context(parent), binder,
		pg_evidence_subject(type)->core, PG_JUDGEMENT_VALUE);
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, type};
	return accept(typing, PG_CONTEXT_EXTEND, context, NULL, 2, premises);
}

const struct pg_evidence *pg_prove_universe(struct pg_typing *typing,
	const struct pg_evidence *context, uint64_t level)
{
	if (!context_proof(typing, context)) return NULL;
	if (level == UINT64_MAX) return NULL;
	const struct pg_term *term = pg_universe(typing->graph, level);
	const struct pg_term *sort = pg_universe(typing->graph, level + 1);
	if (!term || !sort) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(context), term, sort, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_UNIVERSE_FORM, pg_evidence_context(context), subject, 1, &context);
}

const struct pg_evidence *pg_prove_host_type(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_object *type)
{
	if (!context_proof(typing, context)) return NULL;
	if (!pg_host_type_name(type)) return NULL;
	const struct pg_term *core = pg_reference(typing->graph, type);
	const struct pg_term *universe = pg_universe(typing->graph, 0);
	if (!core || !universe) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(context), core, universe, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_HOST_TYPE_FORM, pg_evidence_context(context), subject, 1, &context);
}

const struct pg_evidence *pg_prove_host_value(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_object *value)
{
	if (!pg_evidence_owned_by(type, typing) || pg_evidence_judgement(type) != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	const struct pg_object *descriptor;
	size_t count;
	const unsigned char *bytes;
	if (!pg_host_literal_view(value, &descriptor, &count, &bytes)) return NULL;
	if (pg_evidence_subject(type)->core->kind != PG_REFERENCE || pg_evidence_subject(type)->core->as.reference != descriptor) return NULL;
	const struct pg_term *core = pg_reference(typing->graph, value);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(type), NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_HOST_VALUE_INTRO, pg_evidence_context(type), subject, 1, &type);
}

const struct pg_evidence *pg_prove_host_function(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_object *function)
{
	if (!pg_evidence_owned_by(type, typing) || pg_evidence_judgement(type) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_object *host_domain, *host_result;
	size_t arity;
	if (!pg_host_function_view(function, &host_domain, &host_result, &arity)) return NULL;
	const struct pg_term *signature = pg_evidence_subject(type)->core;
	for (size_t i = 0; i < arity; ++i) {
		const struct pg_term *domain, *codomain;
		const struct pg_object *binder;
		if (!pg_pi_view(signature, &domain, &binder, &codomain)) return NULL;
		if (domain->kind != PG_REFERENCE || domain->as.reference != host_domain) return NULL;
		signature = codomain;
	}
	enum pg_totality totality;
	const struct pg_term *result;
	if (!pg_pure_computation_type_view(signature, &totality, &result) || totality != PG_TOTALITY_TOTAL) return NULL;
	if (result->kind != PG_REFERENCE || result->as.reference != host_result) return NULL;
	const struct pg_term *core = pg_reference(typing->graph, function);
	const struct pg_occurrence *subject = core ? pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, core, pg_evidence_subject(type), NULL, 0, NULL) : NULL;
	return subject ? accept(typing, PG_HOST_FUNCTION_INTRO,
		pg_evidence_context(type), subject, 1, &type) : NULL;
}

static enum pg_evidence_judgement binding_judgement(const struct pg_evidence *extension)
{
	return pg_evidence_context(extension)->judgement;
}

const struct pg_evidence *pg_prove_family_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *indices, const struct pg_evidence *universe)
{
	if (!context_proof(typing, parent) || !context_proof(typing, indices)) return NULL;
	if (!pg_evidence_owned_by(universe, typing)) return NULL;
	if (pg_evidence_judgement(universe) != PG_JUDGEMENT_VALUE_TYPE || pg_evidence_context(universe) != pg_evidence_context(indices)) return NULL;
	uint64_t level;
	if (!pg_universe_level(pg_evidence_subject(universe)->core, &level)) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(indices), pg_evidence_context(parent), &count) || !count) return NULL;
	if (pg_context_lookup(pg_evidence_context(parent), binder)) return NULL;
	const struct pg_term *signature = pg_context_signature(typing->graph,
		pg_evidence_context(parent), pg_evidence_context(indices), pg_evidence_subject(universe)->core);
	if (!signature) return NULL;
	const struct pg_context *context = pg_context_intern(typing, &(struct pg_context){
		.parent = pg_evidence_context(parent), .binder = binder, .declared_type = signature,
		.judgement = PG_JUDGEMENT_TYPE_FAMILY, .indices = pg_evidence_context(indices)});
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, indices, universe};
	return accept(typing, PG_CONTEXT_FAMILY_EXTEND,
		context, NULL, 3, premises);
}

const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder)
{
	if (!context_proof(typing, context)) return NULL;
	const struct pg_context *declaration = pg_context_lookup(pg_evidence_context(context), binder);
	if (!declaration) return NULL;
	const struct pg_term *term = pg_reference(typing->graph, binder);
	const struct pg_occurrence *subject = pg_occurrence(typing, declaration->judgement, pg_evidence_context(context),
		term, declaration->declared_type, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_VARIABLE, pg_evidence_context(context),
		subject, 1, &context);
}

const struct pg_evidence *pg_prove_family_application(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *index)
{
	if (!pg_evidence_owned_by(family, typing) || !pg_evidence_owned_by(index, typing)) return NULL;
	if (pg_evidence_judgement(family) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pg_evidence_judgement(index) != PG_JUDGEMENT_VALUE && pg_evidence_judgement(index) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pg_evidence_context(family) != pg_evidence_context(index)) return NULL;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(family)->classifier, &domain, &binder, &body)) return NULL;
	if (pg_alpha_equal(domain, pg_evidence_subject(index)->classifier) != 1) return NULL;
	struct pg_binding_value binding = {binder, pg_evidence_subject(index)->core};
	const struct pg_term *classifier = pg_substitution_compute(&typing->substitutions, body, 1, &binding);
	if (!classifier) return NULL;
	uint64_t level;
	enum pg_evidence_judgement kind = pg_universe_level(classifier, &level)
		? PG_JUDGEMENT_VALUE_TYPE : PG_JUDGEMENT_TYPE_FAMILY;
	const struct pg_term *core = pg_application(typing->graph, pg_evidence_subject(family)->core, pg_evidence_subject(index)->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(family), pg_evidence_subject(index)};
	const struct pg_occurrence *subject = pg_occurrence(typing, kind, pg_evidence_context(family), core, classifier, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {family, index};
	return accept(typing, PG_TYPE_FAMILY_APP, pg_evidence_context(family), subject, 2, premises);
}

const struct pg_evidence *pg_prove_family_abstraction(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *body)
{
	if (!context_proof(typing, context)) return NULL;
	if (context->rule != PG_CONTEXT_EXTEND && context->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (!pg_evidence_owned_by(body, typing) || pg_evidence_context(body) != pg_evidence_context(context)) return NULL;
	if (pg_evidence_judgement(body) != PG_JUDGEMENT_VALUE_TYPE && pg_evidence_judgement(body) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	const struct pg_context *parent = pg_evidence_context(context)->parent;
	const struct pg_term *signature = pg_pi(typing->graph, pg_evidence_context(context)->declared_type,
		pg_evidence_context(context)->binder, pg_evidence_subject(body)->classifier);
	const struct pg_term *core = pg_lambda(typing->graph, pg_evidence_context(context)->binder, pg_evidence_subject(body)->core);
	if (!signature || !core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_TYPE_FAMILY, parent, core, signature, NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(body)});
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {context, body};
	return accept(typing, PG_TYPE_FAMILY_ABSTRACT,
		parent, subject, 2, premises);
}

static const struct pg_evidence *unary_formation(struct pg_typing *typing,
	const struct pg_evidence *argument,
	enum pg_evidence_rule rule, enum pg_totality totality, const struct pg_effect_row *effects)
{
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	enum pg_evidence_judgement output;
	const struct pg_term *term;
	if (rule == PG_RETURN_TYPE_FORM) {
		argument = pg_prove_value_type(typing, argument);
		if (!argument) return NULL;
		output = PG_JUDGEMENT_COMPUTATION_TYPE;
		term = pg_computation_type(typing->graph, totality, effects, pg_evidence_subject(argument)->core);
	} else {
		if (pg_evidence_judgement(argument) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		output = PG_JUDGEMENT_VALUE_TYPE;
		term = pg_thunk_type(typing->graph, pg_evidence_subject(argument)->core);
	}
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, output, pg_evidence_context(argument),
		term, pg_evidence_subject(argument)->classifier, NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(argument)});
	if (!subject) return NULL;
	return accept(typing, rule, pg_evidence_context(argument), subject, 1, &argument);
}

const struct pg_evidence *pg_prove_termination_type(struct pg_typing *typing,
	const struct pg_evidence *type,
	const struct pg_evidence *suspended)
{
	if (!typing) return NULL;
	if (!pg_evidence_owned_by(type, typing) || !pg_evidence_owned_by(suspended, typing)) return NULL;
	if (pg_evidence_judgement(type) != PG_JUDGEMENT_VALUE_TYPE || pg_evidence_judgement(suspended) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_context(type) != pg_evidence_context(suspended)) return NULL;
	const struct pg_term *computation;
	if (!pg_thunk_type_view(pg_evidence_subject(type)->core, &computation)) return NULL;
	if (pg_alpha_equal(pg_evidence_subject(type)->core, pg_evidence_subject(suspended)->classifier) != 1) return NULL;
	const struct pg_term *core = pg_termination_type(typing->graph, pg_evidence_subject(suspended)->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(type), pg_evidence_subject(suspended)};
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(type), core, pg_evidence_subject(type)->classifier, NULL, 2, operands);
	const struct pg_evidence *premises[] = {type, suspended};
	return subject ? accept(typing, PG_TERMINATION_FORM,
		pg_evidence_context(type), subject, 2, premises) : NULL;
}

const struct pg_evidence *pg_prove_termination(struct pg_typing *typing,
	const struct pg_evidence *formation,
	const struct pg_evidence *suspended)
{
	if (!typing) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || !pg_evidence_owned_by(suspended, typing)) return NULL;
	if (pg_evidence_judgement(formation) != PG_JUDGEMENT_VALUE_TYPE || pg_evidence_judgement(suspended) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_context(formation) != pg_evidence_context(suspended)) return NULL;
	const struct pg_term *expected, *computation, *value;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_termination_type_view(pg_evidence_subject(formation)->core, &expected)) return NULL;
	if (pg_alpha_equal(expected, pg_evidence_subject(suspended)->core) != 1) return NULL;
	if (!pg_thunk_type_view(pg_evidence_subject(suspended)->classifier, &computation)) return NULL;
	if (!pg_computation_type_view(computation, &totality, &effects, &value)) return NULL;
	if (totality != PG_TOTALITY_TOTAL) return NULL;
	const struct pg_term *core = pg_termination_witness(typing->graph, pg_evidence_subject(suspended)->core);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(formation), NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(suspended)});
	const struct pg_evidence *premises[] = {formation, suspended};
	return subject ? accept(typing, PG_TERMINATION_INTRO,
		pg_evidence_context(suspended), subject, 2, premises) : NULL;
}

const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	const struct pg_evidence *value_type)
{
	if (!typing) return NULL;
	return pg_prove_effect_type(typing, pg_effect_row(typing->graph, 0, NULL), value_type);
}

const struct pg_evidence *pg_prove_effect_type(struct pg_typing *typing,
	const struct pg_effect_row *effects,
	const struct pg_evidence *value_type)
{
	return pg_prove_computation_type(typing, PG_TOTALITY_UNSPECIFIED, effects, value_type);
}

const struct pg_evidence *pg_prove_computation_type(struct pg_typing *typing,
	enum pg_totality totality,
	const struct pg_effect_row *effects, const struct pg_evidence *value_type)
{
	if (!effects) return NULL;
	return unary_formation(typing, value_type, PG_RETURN_TYPE_FORM, totality, effects);
}

static int endpoint(const struct pg_typing *typing, const struct pg_evidence *term,
	enum pg_evidence_judgement judgement, const struct pg_context *context,
	const struct pg_term *type)
{
	if (!pg_evidence_owned_by(term, typing)) return 0;
	if (pg_evidence_judgement(term) != judgement) return 0;
	if (pg_evidence_context(term) != context) return 0;
	return pg_alpha_equal(pg_evidence_subject(term)->classifier, type) == 1;
}

const struct pg_evidence *pg_prove_identity_type(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *left,
	const struct pg_evidence *right)
{
	if (!pg_evidence_owned_by(type, typing)) return NULL;
	enum pg_evidence_judgement elements;
	switch (pg_evidence_judgement(type)) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!endpoint(typing, left, elements, pg_evidence_context(type), pg_evidence_subject(type)->core)) return NULL;
	if (!endpoint(typing, right, elements, pg_evidence_context(type), pg_evidence_subject(type)->core)) return NULL;
	const struct pg_term *family = pg_identity_action(typing->graph, pg_evidence_subject(type)->core);
	const struct pg_term *core = pg_identity_instance(typing->graph, family,
		pg_evidence_subject(left)->core, pg_evidence_subject(right)->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(type), pg_evidence_subject(left), pg_evidence_subject(right)};
	const struct pg_occurrence *subject = pg_occurrence(typing, pg_evidence_judgement(type), pg_evidence_context(type), core, pg_evidence_subject(type)->classifier, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {type, left, right};
	return accept(typing, PG_IDENTITY_FORM, pg_evidence_context(type),
		subject, 3, premises);
}

static int universe_identity(const struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_term **left,
	const struct pg_term **right, uint64_t *level)
{
	if (!pg_evidence_owned_by(family, typing)) return 0;
	if (pg_evidence_judgement(family) != PG_JUDGEMENT_VALUE) return 0;
	const struct pg_term *universe;
	if (!pg_identity_view(pg_evidence_subject(family)->classifier, &universe, left, right)) return 0;
	return pg_universe_level(universe, level);
}

const struct pg_evidence *pg_prove_identity_endpoint_type(struct pg_typing *typing,
	const struct pg_evidence *family,
	enum pg_evidence_rule side)
{
	const struct pg_term *left, *right, *core;
	uint64_t level;
	if (!universe_identity(typing, family, &left, &right, &level)) return NULL;
	switch (side) {
	case PG_IDENTITY_LEFT_TYPE: core = left; break;
	case PG_IDENTITY_RIGHT_TYPE: core = right; break;
	default: return NULL;
	}
	const struct pg_term *sort = pg_universe(typing->graph, level);
	if (!sort) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(family),
		core, sort, NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(family)});
	if (!subject) return NULL;
	return accept(typing, side, pg_evidence_context(family),
		subject, 1, &family);
}

const struct pg_evidence *pg_prove_identity_instance(struct pg_typing *typing,
	const struct pg_evidence *family,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	const struct pg_term *left_type, *right_type;
	uint64_t level;
	if (!universe_identity(typing, family, &left_type, &right_type, &level)) return NULL;
	if (!endpoint(typing, left, PG_JUDGEMENT_VALUE, pg_evidence_context(family), left_type)) return NULL;
	if (!endpoint(typing, right, PG_JUDGEMENT_VALUE, pg_evidence_context(family), right_type)) return NULL;
	const struct pg_term *core = pg_identity_instance(typing->graph, pg_evidence_subject(family)->core,
		pg_evidence_subject(left)->core, pg_evidence_subject(right)->core);
	if (!core) return NULL;
	const struct pg_term *sort = pg_universe(typing->graph, level);
	if (!sort) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(family), pg_evidence_subject(left), pg_evidence_subject(right)};
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_VALUE_TYPE, pg_evidence_context(family), core, sort, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {family, left, right};
	return accept(typing, PG_IDENTITY_INSTANCE, pg_evidence_context(family),
		subject, 3, premises);
}

const struct pg_evidence *pg_prove_reflexivity(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *term)
{
	const struct pg_evidence *identity = pg_prove_identity_type(typing, type, term, term);
	if (!identity) return NULL;
	const struct pg_term *core = pg_identity_action(typing->graph, pg_evidence_subject(term)->core);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, pg_evidence_judgement(term), core, pg_evidence_subject(identity), NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(term)});
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {identity, term};
	return accept(typing, PG_REFLEXIVITY, pg_evidence_context(term),
		subject, 2, premises);
}

const struct pg_evidence *pg_prove_identity_transport(struct pg_typing *typing,
	const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	const struct pg_term *left, *right;
	uint64_t level;
	if (!universe_identity(typing, family, &left, &right, &level)) return NULL;
	const struct pg_term *domain = direction == PG_IDENTITY_RIGHT ? left : right;
	if (!endpoint(typing, value, PG_JUDGEMENT_VALUE, pg_evidence_context(family), domain)) return NULL;
	const struct pg_evidence *target = pg_prove_identity_endpoint_type(typing, family,
		direction == PG_IDENTITY_RIGHT ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE);
	if (!target) return NULL;
	const struct pg_term *core = pg_identity_transport(typing->graph, pg_evidence_subject(family)->core, pg_evidence_subject(value)->core, direction);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(family), pg_evidence_subject(value)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(target), NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {target, family, value};
	return accept(typing, PG_IDENTITY_TRANSPORT, pg_evidence_context(family),
		subject, 3, premises);
}

const struct pg_evidence *pg_prove_identity_lift(struct pg_typing *typing,
	const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction)
{
	const struct pg_evidence *transport = pg_prove_identity_transport(typing, family, value, direction);
	if (!transport) return NULL;
	const struct pg_evidence *left = direction == PG_IDENTITY_RIGHT ? value : transport;
	const struct pg_evidence *right = direction == PG_IDENTITY_RIGHT ? transport : value;
	const struct pg_evidence *type = pg_prove_identity_instance(typing, family, left, right);
	if (!type) return NULL;
	const struct pg_term *core = pg_identity_lift(typing->graph, pg_evidence_subject(family)->core, pg_evidence_subject(value)->core, direction);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(family), pg_evidence_subject(value)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(type), NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {type, transport};
	return accept(typing, PG_IDENTITY_LIFT, pg_evidence_context(family),
		subject, 2, premises);
}

const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	const struct pg_evidence *computation_type)
{
	return unary_formation(typing, computation_type, PG_THUNK_TYPE_FORM, PG_TOTALITY_UNSPECIFIED, NULL);
}

/* A logical signature has the bound of its telescope and terminal Universe.
 * It is not itself a value type. Walk checked declarations, not raw Pi nodes. */
static int binding_level(const struct pg_evidence *extension, uint64_t *level)
{
	struct level_work { const struct pg_evidence *extension; struct level_work *next; };
	struct pg_graph temporary = {0};
	struct level_work first = {extension, NULL}, *work = &first;
	uint64_t maximum = 0;
	int status = -1;
	while (work) {
		extension = work->extension;
		work = work->next;
		uint64_t bound;
		if (extension->rule == PG_CONTEXT_EXTEND) {
			if (!pg_universe_level(pg_evidence_subject(extension->premises[1])->classifier, &bound)) goto done;
		} else if (extension->rule == PG_CONTEXT_FAMILY_EXTEND) {
			if (!pg_universe_level(pg_evidence_subject(extension->premises[2])->classifier, &bound)) goto done;
			const struct pg_evidence *indices = extension->premises[1];
			while (pg_evidence_context(indices) != pg_evidence_context(extension->premises[0])) {
				struct level_work *next = pg_alloc(&temporary, sizeof(*next));
				if (!next) goto done;
				*next = (struct level_work){indices, work};
				work = next;
				indices = indices->premises[0];
			}
		} else goto done;
		if (bound > maximum) maximum = bound;
	}
	*level = maximum;
	status = 0;
done:
	pg_graph_destroy(&temporary);
	return status;
}

const struct pg_evidence *pg_prove_pi(struct pg_typing *typing,
	const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain)
{
	if (!context_proof(typing, extended_context)) return NULL;
	const struct pg_context *scope = pg_evidence_context(extended_context);
	if (!scope) return NULL;
	if (!pg_evidence_owned_by(codomain, typing)) return NULL;
	if (pg_evidence_judgement(codomain) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(codomain) != scope) return NULL;
	const struct pg_evidence *premises[] = {extended_context, codomain};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_FORM, scope->parent, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	uint64_t left, right;
	if (binding_level(extended_context, &left)) return NULL;
	if (!pg_universe_level(pg_evidence_subject(codomain)->classifier, &right)) return NULL;
	const struct pg_term *bound = pg_universe(typing->graph, left > right ? left : right);
	const struct pg_term *term = pg_pi(typing->graph, scope->declared_type,
		scope->binder, pg_evidence_subject(codomain)->core);
	if (!bound || !term) return NULL;
	/* A logical family signature is a binding declaration, not a value type.
	 * Keep its variable in the extended scope rather than invent a type proof. */
	const struct pg_occurrence *domain = scope->judgement == PG_JUDGEMENT_VALUE
		? pg_evidence_subject(extended_context->premises[1])
		: pg_occurrence(typing, scope->judgement, scope, pg_reference(typing->graph, scope->binder),
			scope->declared_type, NULL, 0, NULL);
	if (!domain) return NULL;
	const struct pg_occurrence *operands[] = {domain, pg_evidence_subject(codomain)};
	const struct pg_occurrence *subject = pg_occurrence(typing, PG_JUDGEMENT_COMPUTATION_TYPE, scope->parent, term, bound, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_PI_FORM,
		scope->parent, subject, 2, premises);
}

static const struct pg_evidence *unary_term(struct pg_typing *typing,
	const struct pg_evidence *argument, const struct pg_object *operation,
	const struct pg_evidence *type, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement)
{
	if (!type) return NULL;
	const struct pg_term *term = pg_application(typing->graph,
		pg_reference(typing->graph, operation), pg_evidence_subject(argument)->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, judgement,
		term, pg_evidence_subject(type), NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(argument)});
	if (!subject) return NULL;
	return accept(typing, rule, pg_evidence_context(argument), subject, 1, &argument);
}

const struct pg_evidence *pg_prove_return(struct pg_typing *typing,
	const struct pg_evidence *value)
{
	return pg_prove_return_contract(typing, PG_TOTALITY_UNSPECIFIED, value);
}

const struct pg_evidence *pg_prove_return_contract(struct pg_typing *typing,
	enum pg_totality totality,
	const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_evidence *type = pg_prove_computation_type(typing, totality,
		pg_effect_row(typing->graph, 0, NULL), formed_classifier(typing, value));
	if (!type) return NULL;
	return unary_term(typing, value, &pg_return_operation,
		type, PG_RETURN_INTRO, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	return unary_term(typing, computation, &pg_thunk_operation,
		pg_prove_thunk_type(typing, formed_classifier(typing, computation)), PG_THUNK_INTRO, PG_JUDGEMENT_VALUE);
}

const struct pg_evidence *pg_prove_force(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_term *classifier;
	if (!pg_thunk_type_view(pg_evidence_subject(value)->classifier, &classifier)) return NULL;
	return unary_term(typing, value, &pg_force_operation,
		pg_prove_thunk_content(typing, formed_classifier(typing, value)), PG_FORCE_ELIM, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_lambda(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *body)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pi->rule != PG_PI_FORM) return NULL;
	if (!pg_evidence_owned_by(body, typing)) return NULL;
	if (pg_evidence_judgement(body) != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(pi)->core, &domain, &binder, &codomain)) return NULL;
	if (pg_evidence_context(body) != pg_evidence_context(pi->premises[0])) return NULL;
	if (pg_alpha_equal(pg_evidence_subject(body)->classifier, codomain) != 1) return NULL;
	const struct pg_term *term = pg_lambda(typing->graph, binder, pg_evidence_subject(body)->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, term, pg_evidence_subject(pi), domain, 1, (const struct pg_occurrence *[]){pg_evidence_subject(body)});
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {pi, body};
	return accept(typing, PG_LAMBDA_INTRO,
		pg_evidence_context(pi), subject, 2, premises);
}

const struct pg_evidence *pg_prove_abstract(struct pg_typing *typing,
	const struct pg_evidence *prefix,
	const struct pg_evidence *context, const struct pg_evidence *body)
{
	if (!context_proof(typing, prefix) || !context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(body, typing) || pg_evidence_judgement(body) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_context(body) != pg_evidence_context(context)) return NULL;
	const struct pg_evidence *type = pg_prove_classifier(typing, context, body);
	if (!type) return NULL;
	while (pg_evidence_context(context) != pg_evidence_context(prefix)) {
		if (context->rule != PG_CONTEXT_EXTEND && context->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
		type = pg_prove_pi(typing, context, type);
		body = pg_prove_lambda(typing, type, body);
		if (!body) return NULL;
		context = context->premises[0];
	}
	return body;
}

/* Align a scoped input with readback's binder using the same lifted context
 * action as ordinary substitution. Free bindings are never alpha-renamed. */
static int typed_input_align(struct pg_typed_query *work, const struct pg_evidence *input,
	const struct pg_term *head)
{
	const struct pg_occurrence *child = pg_evidence_subject(input);
	const struct pg_term *body;
	const struct pg_object *binder = pg_occurrence_input_binder(head, work->ordinal, &body);
	if (!binder || !child->context || child->context->binder == binder) {
		work->argument = input;
		return 1;
	}
	if (child->context->parent != work->current->context) return -1;
	if (!work->lift) {
		const struct pg_context_map *identity = pg_context_map_projection(work->typing,
			child->context->parent, child->context->parent);
		work->lift = pg_context_lift_request(work->typing, identity, child->context, binder);
	}
	enum pg_substitution_status status = pg_context_lift_advance(work->lift, 1);
	if (status == PG_SUBSTITUTION_PENDING) return 0;
	if (status == PG_SUBSTITUTION_ERROR) return -1;
	if (!work->action) work->action = pg_occurrence_action_request(work->typing,
		pg_context_lift_result(work->lift), child);
	status = pg_occurrence_action_advance(work->action, 1);
	if (status == PG_SUBSTITUTION_PENDING) return 0;
	if (status == PG_SUBSTITUTION_ERROR) return -1;
	work->argument = pg_prove_structural_subject(work->typing, pg_occurrence_action_result(work->action));
	return work->argument ? 1 : -1;
}

/* Reinstantiate a dependent field's declared type with the current preceding
 * fields. Their retained reductions justify conversion of the old instance;
 * checking the new substitution remains the ordinary dependent map rule. */
static int typed_field_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	if (!work->field) {
		if (!work->ordinal) return 1;
		struct constructor_structure view;
		int constructor = constructor_view(typing, pg_prove_structural_subject(typing, work->current), &view);
		if (constructor <= 0) return constructor ? -1 : 1;
		if (work->ordinal >= view.count) return -1;
		const struct pg_evidence *family = pg_prove_reindex(typing, view.parameters, view.formation);
		if (!family) return -1;
		const struct pg_evidence *self = pg_evidence_judgement(family) == PG_JUDGEMENT_TYPE_FAMILY
			? family : pg_prove_type_value(typing, family);
		const struct pg_evidence *map = pg_prove_substitution_pair(typing, view.parameters,
			view.formation->premises[0], self);
		if (!map) return -1;
		struct typed_field *field = pg_alloc(typing->graph, sizeof(*field));
		if (!field) return -1;
		*field = (struct typed_field){.map = map, .prefix = pg_evidence_context_map(map)->count};
		if (work->ordinal >= SIZE_MAX / sizeof(*field->declarations) ||
			field->prefix > SIZE_MAX / sizeof(*field->bindings) - work->ordinal) return -1;
		field->declarations = pg_alloc(typing->graph, (work->ordinal + 1) * sizeof(*field->declarations));
		field->bindings = pg_alloc(typing->graph, (field->prefix + work->ordinal) * sizeof(*field->bindings));
		field->reductions = pg_alloc(typing->graph, (field->prefix + work->ordinal) * sizeof(*field->reductions));
		if (!field->declarations || !field->bindings || !field->reductions) return -1;
		memcpy(field->bindings, pg_context_map_bindings(pg_evidence_context_map(map)), field->prefix * sizeof(*field->bindings));
		const struct pg_evidence *scope = pg_data_schema_fields(view.formation->certificate, view.constructor);
		for (size_t i = view.count; i; --i, scope = scope->premises[0])
			if (i <= work->ordinal + 1) field->declarations[i - 1] = scope;
		work->field = field;
		work->dependency = NULL;
	}
	struct typed_field *field = work->field;
	if (field->next < work->ordinal) {
		if (!work->dependency) {
			const struct pg_occurrence *blocked = pg_occurrence_input_blocked_source(work->input);
			work->dependency = pg_typed_input_request(typing, pg_prove_structural_subject(typing, blocked), field->next);
			return work->dependency ? 0 : -1;
		}
		if (!work->dependency->status) return 0;
		const struct pg_evidence *value = pg_typed_query_result(work->dependency);
		const struct pg_occurrence *original;
		if (!value || !structural_input(typing, work->current, field->next, &original) || !original) return -1;
		const struct pg_evidence *declaration = field->declarations[field->next];
		size_t slot = field->prefix + field->next;
		field->bindings[slot] = (struct pg_binding_value){pg_evidence_context(declaration)->binder, original->core};
		field->reductions[slot] = work->dependency->input_reduction;
		field->map = pg_prove_substitution_pair(typing, field->map, declaration, value);
		if (!field->map) return -1;
		++field->next;
		work->dependency = NULL;
		return 0;
	}
	const struct pg_evidence *declared_type = field->declarations[work->ordinal]->premises[1];
	const struct pg_evidence *type = pg_prove_reindex(typing, field->map, declared_type);
	if (!type) return -1;
	const struct pg_term *left = pg_evidence_classifier(work->value), *right = pg_evidence_subject(type)->core;
	if (pg_alpha_equal(left, right) == 1) return 1;
	const struct pg_conversion_certificate *conversion = pg_conversion_substitution(&typing->substitutions,
		left, right, pg_evidence_subject(declared_type)->core, field->prefix + work->ordinal, field->bindings, field->reductions);
	work->value = pg_prove_conversion(typing, work->value, type, conversion);
	return work->value ? 1 : -1;
}

static int typed_input_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_occurrence *source = work->source;
	if (work->value) {
		if (!work->input_resumed) {
			int status = work->reduction ? typed_field_step(work) : 1;
			if (status <= 0) return status;
			work->input_resumed = 1;
			work->input = pg_occurrence_input_resume_request(typing, work->input, pg_evidence_subject(work->value));
			return work->input ? 0 : -1;
		}
		enum pg_occurrence_input_status status = pg_occurrence_input_advance(work->input, 1);
		if (status == PG_INPUT_PENDING) return 0;
		if (status == PG_INPUT_ERROR) return -1;
		work->result = pg_prove_structural_subject(typing, pg_occurrence_input_result(work->input));
		return 1;
	}
	if (!work->dependency) {
		if (!work->input) work->input = pg_occurrence_input_request(typing, source, work->ordinal);
		enum pg_occurrence_input_status status = pg_occurrence_input_advance(work->input, 1);
		if (status == PG_INPUT_PENDING) return 0;
		if (status == PG_INPUT_ERROR) return -1;
		work->result = pg_prove_structural_subject(typing, pg_occurrence_input_result(work->input));
		if (work->result) return 1;
		const struct pg_occurrence *blocked = pg_occurrence_input_blocked_source(work->input);
		if (!blocked) return 1;
		if (blocked->selection) {
			work->dependency = selection_request(typing, blocked, NULL);
			work->resume = TYPED_RESUME_SELECTION;
			return work->dependency ? 0 : -1;
		}
		work->reduction = subject_normalization(typing, blocked);
		if (!work->reduction) return 1;
		const struct pg_reduction_phase *phase = pg_reduction_head_congruence(work->reduction);
		const struct pg_evidence *origin = pg_prove_structural_subject(typing, blocked->origin);
		work->current = blocked->origin;
		const struct pg_reduction_phase *last = pg_reduction_phases(work->reduction);
		if (!phase && last && last->previous) {
			work->dependency = typed_phase_request(typing, blocked->origin, last->previous);
			work->reduction = last->head;
			work->resume = TYPED_RESUME_PHASE;
		} else if (phase && pg_reduction_source(phase->head) == pg_reduction_target(phase->head))
			work->dependency = pg_typed_input_request(typing, origin, work->ordinal);
		else {
			work->dependency = typed_head_request(typing, origin, phase ? phase->head : work->reduction);
			work->resume = TYPED_RESUME_INPUT;
		}
		return work->dependency ? 0 : -1;
	}
	if (!work->dependency->status) return 0;
	if (work->dependency->status < 0) {
		/* Checked exposure may be unavailable even for an accepted type.
		 * Inversion can still use its own rule; no child is fabricated. */
		switch (work->resume) {
		case TYPED_RESUME_INPUT: case TYPED_RESUME_SELECTION: case TYPED_RESUME_SELECTED_INPUT:
			return 1;
		default: return -1;
		}
	}
	const struct pg_evidence *input = work->dependency->result;
	if (!input) return 1;
	if (work->resume == TYPED_RESUME_SELECTION) {
		work->selection = pg_alloc(typing->graph, sizeof(*work->selection));
		if (!work->selection) return -1;
		work->selection->frames.first = work->dependency->selection->frames.first;
		work->current = pg_evidence_subject(input);
		work->dependency = pg_typed_input_request(typing, input, work->ordinal);
		work->resume = TYPED_RESUME_SELECTED_INPUT;
		return work->dependency ? 0 : -1;
	}
	if (work->resume == TYPED_RESUME_SELECTED_INPUT) {
		struct typed_selection *state = work->selection;
		if (!state->child) {
			state->child = pg_evidence_subject(input);
			if (state->child->context != work->current->context) {
				if (pg_occurrence_scoped_input(work->current, work->ordinal) != state->child) return 1;
				const struct pg_occurrence *blocked = pg_occurrence_input_blocked_source(work->input);
				const struct pg_term *body;
				state->binder = pg_occurrence_input_binder(blocked->core, work->ordinal, &body);
				state->extended = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, state->child->context);
				if (!state->binder || !state->extended) return 1;
				state->cursor = state->frames.first;
				state->body = input;
			}
		}
		if (state->extended) {
			int status = selection_lift_step(typing, state);
			if (status <= 0) return status;
		}
		work->value = state->body ? state->body : scope_image(typing, input, state->frames.first);
		return work->value ? 0 : 1;
	}
	if (work->resume == TYPED_RESUME_PHASE) {
		work->current = pg_evidence_subject(input);
		work->dependency = typed_head_request(typing, input, work->reduction);
		work->resume = TYPED_RESUME_INPUT;
		return work->dependency ? 0 : -1;
	}
	if (work->resume == TYPED_RESUME_INPUT) {
		const struct pg_reduction_phase *phase = pg_reduction_head_congruence(work->reduction);
		const struct pg_occurrence *blocked = pg_occurrence_input_blocked_source(work->input);
		const struct pg_term *head = pg_reduction_target(phase ? phase->head : work->reduction);
		/* Scoped children are subsequently transported to readback's binders;
		 * alpha comparison here cannot change any free variable. */
		if (pg_alpha_equal(pg_evidence_subject(input)->core, head) != 1 ||
			pg_evidence_context(input) != blocked->context) return 1;
		work->current = pg_evidence_subject(input);
		work->dependency = pg_typed_input_request(typing, input, work->ordinal);
		work->resume = TYPED_RESUME_NONE;
		return work->dependency ? 0 : -1;
	}
	if (!work->argument) {
		const struct pg_reduction_phase *phase = pg_reduction_head_congruence(work->reduction);
		int status = typed_input_align(work, input, pg_reduction_target(phase ? phase->head : work->reduction));
		if (status <= 0) return status < 0 ? 1 : 0;
	}
	input = work->argument;
	/* Follow the semantic input boundary along the erased APP spine,
	 * one congruence phase per transition, not arbitrary descendants. */
	const struct pg_reduction_phase *phase = pg_reduction_head_congruence(work->reduction);
	if (!phase) {
		if (pg_alpha_equal(work->current->core, pg_reduction_target(work->reduction)) != 1) return 1;
		work->value = input;
		return 0;
	}
	if (pg_reduction_source(phase->head) != pg_reduction_target(phase->head) &&
		pg_alpha_equal(work->current->core, pg_reduction_target(phase->head)) != 1) return 1;
	const struct pg_term *core = pg_evidence_subject(input)->core;
	const struct pg_reduction_certificate *child = NULL;
	if (pg_alpha_equal(pg_reduction_source(phase->children[0]), core) == 1) child = phase->children[0];
	else if (phase->children[1] && pg_alpha_equal(pg_reduction_source(phase->children[1]), core) == 1) child = phase->children[1];
	else {
		const struct pg_term *head = pg_reduction_target(phase->head), *body;
		if (head->kind != PG_APPLICATION) return 1;
		const struct pg_object *binder = pg_occurrence_input_binder(head, work->ordinal, &body);
		/* Pi's semantic codomain is beneath its right-hand Lambda, not
		 * the erased APP's left spine. Its scope was aligned above. */
		work->reduction = binder ? phase->children[1] : phase->children[0];
		if (!work->reduction) return 1;
		return 0;
	}
	work->value = pg_prove_normalization(typing, input, child);
	if (!work->value) return 1;
	work->input_reduction = child;
	return 0;
}

static int structural_input(struct pg_typing *typing, const struct pg_occurrence *source,
	size_t index, const struct pg_occurrence **result)
{
	struct pg_typed_query *work = pg_typed_input_request(typing, pg_prove_structural_subject(typing, source), index);
	int status;
	do status = pg_typed_query_advance(work, 1024); while (!status);
	const struct pg_evidence *proof = pg_typed_query_result(work);
	*result = proof ? pg_evidence_subject(proof) : NULL;
	return status > 0;
}

/* A selected F/U formation retains its source when context action cannot
 * expose the child directly. Selection is not itself typing acceptance. */
static const struct pg_occurrence *content_subject(struct pg_typing *typing,
	const struct pg_occurrence *source, const struct pg_term *core,
	const struct pg_term *classifier, enum pg_evidence_judgement judgement)
{
	const struct pg_occurrence *child;
	if (!structural_input(typing, source, 0, &child)) return NULL;
	if (child && child->context == source->context && child->core == core && child->judgement == judgement)
		return pg_occurrence_boundary(typing, child, judgement, classifier);
	return pg_occurrence_selected(typing, source, 0, NULL, judgement, core, classifier);
}

/* Inversion uses an accepted judgement, never an untyped constructor spine. */
static const struct pg_evidence *unary_term_content(struct pg_typing *typing,
	const struct pg_evidence *proof, const struct pg_occurrence *child)
{
	if (!pg_evidence_owned_by(proof, typing)) return NULL;
	const struct pg_object *operation;
	const struct pg_term *classifier;
	enum pg_evidence_rule rule;
	enum pg_evidence_judgement judgement;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_COMPUTATION) {
		const struct pg_effect_row *effects;
		enum pg_totality totality;
		if (!pg_computation_type_view(pg_evidence_classifier(proof), &totality, &effects, &classifier) || pg_effect_count(effects)) return NULL;
		operation = &pg_return_operation;
		rule = PG_RETURN_VALUE;
		judgement = PG_JUDGEMENT_VALUE;
	} else if (pg_evidence_judgement(proof) == PG_JUDGEMENT_VALUE) {
		if (!pg_thunk_type_view(pg_evidence_classifier(proof), &classifier)) return NULL;
		operation = &pg_thunk_operation;
		rule = PG_THUNK_COMPUTATION;
		judgement = PG_JUDGEMENT_COMPUTATION;
	} else return NULL;
	const struct pg_term *core = pg_evidence_subject(proof)->core;
	if (core->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = core->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != operation) return NULL;
	/* Inverting an accepted unary result does not require reconstructing
	 * the redex which produced it. Retain a checked structural child when
	 * available, but never query this result recursively to invent one. */
	const struct pg_occurrence *subject;
	if (child && child->context == pg_evidence_context(proof) &&
		child->core == core->as.application.argument && child->judgement == judgement)
		subject = pg_occurrence_boundary(typing, child, judgement, classifier);
	else subject = pg_occurrence_derived(typing, pg_evidence_subject(proof),
		judgement, core->as.application.argument, classifier);
	if (!subject) return NULL;
	if (!subject->type && !pg_evidence_for_subject(typing, subject, NULL)) {
		const struct pg_evidence *formation = formed_classifier(typing, proof);
		formation = operation == &pg_return_operation ? pg_prove_return_content(typing, formation)
			: pg_prove_thunk_content(typing, formation);
		if (!formation) return NULL;
		struct pg_occurrence header = *subject;
		header.type = pg_evidence_subject(formation);
		header.classifier = header.type->core;
		subject = pg_occurrence_intern(typing, &header, subject->operands, pg_occurrence_maps(subject));
		if (!subject) return NULL;
	}
	return accept(typing, rule, pg_evidence_context(proof), subject, 1, &proof);
}

const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (computation->rule == PG_RETURN_INTRO) return computation->premises[0];
	const struct pg_occurrence *child;
	if (!structural_input(typing, pg_evidence_subject(computation), 0, &child)) return NULL;
	return unary_term_content(typing, computation, child);
}

const struct pg_evidence *pg_prove_thunk_computation(struct pg_typing *typing,
	const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (pg_evidence_judgement(value) != PG_JUDGEMENT_VALUE) return NULL;
	if (value->rule == PG_THUNK_INTRO) return value->premises[0];
	const struct pg_occurrence *child;
	if (!structural_input(typing, pg_evidence_subject(value), 0, &child)) return NULL;
	return unary_term_content(typing, value, child);
}

const struct pg_evidence *pg_prove_total_pure_value(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_term *type;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(pg_evidence_subject(computation)->classifier, &totality, &effects, &type)) return NULL;
	if (totality != PG_TOTALITY_TOTAL || pg_effect_count(effects)) return NULL;
	const struct pg_term *core = pg_application(typing->graph,
		pg_reference(typing->graph, &pg_total_result_operation), pg_evidence_subject(computation)->core);
	if (!core) return NULL;
	const struct pg_evidence *formation = pg_prove_return_content(typing, formed_classifier(typing, computation));
	if (!formation) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_VALUE, core, pg_evidence_subject(formation), NULL, 1, (const struct pg_occurrence *[]){pg_evidence_subject(computation)});
	if (!subject) return NULL;
	return accept(typing, PG_TOTAL_PURE_VALUE,
		pg_evidence_context(computation), subject, 1, &computation);
}

const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(function, typing)) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (pg_evidence_judgement(function) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_judgement(argument) != PG_JUDGEMENT_VALUE && pg_evidence_judgement(argument) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pg_evidence_context(function) != pg_evidence_context(argument)) return NULL;
	const struct pg_evidence *premises[] = {function, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_APP_ELIM, pg_evidence_context(function), NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(function)->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, pg_evidence_subject(argument)->classifier) != 1) return NULL;
	const struct pg_evidence *type = pg_prove_pi_codomain(typing, formed_classifier(typing, function), argument);
	const struct pg_term *term = pg_application(typing->graph, pg_evidence_subject(function)->core, pg_evidence_subject(argument)->core);
	if (!type || !term) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(function), pg_evidence_subject(argument)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, term, pg_evidence_subject(type), NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_APP_ELIM,
		pg_evidence_context(function), subject, 2, premises);
}

const struct pg_evidence *pg_prove_conversion(struct pg_typing *typing,
	const struct pg_evidence *term, const struct pg_evidence *target_type,
	const struct pg_conversion_certificate *certificate)
{
	if (!certificate) return NULL;
	if (!pg_evidence_owned_by(term, typing)) return NULL;
	if (!pg_evidence_owned_by(target_type, typing)) return NULL;
	if (pg_evidence_context(term) != pg_evidence_context(target_type)) return NULL;
	switch (pg_evidence_judgement(term)) {
	case PG_JUDGEMENT_VALUE:
		target_type = pg_prove_value_type(typing, target_type);
		if (!target_type) return NULL;
		break;
	case PG_JUDGEMENT_COMPUTATION:
		if (pg_evidence_judgement(target_type) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		break;
	default:
		return NULL;
	}
	if (pg_conversion_left(certificate) != pg_evidence_subject(term)->classifier) return NULL;
	if (pg_conversion_right(certificate) != pg_evidence_subject(target_type)->core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_reclassified(typing, pg_evidence_subject(term), pg_evidence_subject(target_type));
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {term, target_type};
	return accept_with_conversion(typing, PG_TYPE_CONVERSION,
		pg_evidence_context(term), subject, 2, premises, certificate);
}

const struct pg_conversion_certificate *pg_evidence_conversion(const struct pg_evidence *evidence)
{
	return evidence->rule == PG_TYPE_CONVERSION ? evidence->certificate : NULL;
}

const struct pg_reduction_certificate *pg_evidence_normalization(const struct pg_evidence *evidence)
{
	return evidence->rule == PG_PURE_NORMALIZATION ? evidence->certificate : NULL;
}

const struct pg_evidence *pg_prove_normalization(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_reduction_certificate *certificate)
{
	if (!pg_evidence_owned_by(source, typing) || !certificate) return NULL;
	if (!pg_evidence_subject(source)) return NULL;
	switch (pg_evidence_judgement(source)) {
	case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION:
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE:
	case PG_JUDGEMENT_TYPE_FAMILY: break;
	default: return NULL;
	}
	if (pg_reduction_policy(certificate) != &pg_pure_policy) return NULL;
	/* Context action and evaluator readback can independently freshen bound
	 * pointers. Alpha conversion preserves the typed source; it does not intern
	 * the two graphs together or change any free binding or classifier. */
	if (pg_alpha_equal(pg_reduction_source(certificate), pg_evidence_subject(source)->core) != 1) return NULL;
	const struct pg_term *target = pg_reduction_target(certificate);
	if (target == pg_evidence_subject(source)->core) return source;
	const struct pg_occurrence *subject = pg_occurrence_derived(typing, pg_evidence_subject(source),
		pg_evidence_judgement(source), target, pg_evidence_subject(source)->classifier);
	if (!subject) return NULL;
	return accept_record(typing, PG_PURE_NORMALIZATION,
		pg_evidence_context(source), subject, 1, &source, certificate, NULL);
}

const struct pg_evidence *pg_prove_normalization_input(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_reduction_certificate *certificate,
	size_t index)
{
	if (!pg_evidence_owned_by(source, typing) || !certificate) return NULL;
	const struct pg_occurrence *subject = pg_evidence_subject(source);
	if (!subject) return NULL;
	if (pg_reduction_policy(certificate) != &pg_pure_policy || pg_reduction_source(certificate) != subject->core) return NULL;
	const struct pg_evidence *normal = pg_prove_normalization(typing, source, certificate);
	struct pg_typed_query *work = pg_typed_input_request(typing, normal, index);
	while (!pg_typed_query_advance(work, 1024)) {}
	return pg_typed_query_result(work);
}

const struct pg_evidence *pg_prove_projection(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(proof, typing)) return NULL;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_CONTEXT) return NULL;
	if (pg_evidence_judgement(proof) == PG_JUDGEMENT_SUBSTITUTION) return NULL;
	if (pg_evidence_context(proof) == pg_evidence_context(context)) return proof;
	const struct pg_evidence *premises[] = {context, proof};
	uint64_t hash;
	const struct pg_evidence *found = find_record(typing, PG_CONTEXT_PROJECTION,
		pg_evidence_context(context), NULL, 2, premises, NULL, &hash);
	if (found) return found;
	const struct pg_context_map *map = pg_context_map_projection(typing,
		pg_evidence_context(proof), pg_evidence_context(context));
	const struct pg_occurrence *subject = pg_occurrence_projection(typing, map, pg_evidence_subject(proof));
	if (!subject) return NULL;
	return accept(typing, PG_CONTEXT_PROJECTION,
		pg_evidence_context(context), subject, 2, premises);
}

static const struct pg_evidence *substitution_build(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	const struct pg_evidence *prefix,
	size_t count, const struct pg_evidence *const *images)
{
	if (!context_proof(typing, source)) return NULL;
	if (!context_proof(typing, destination)) return NULL;
	if (count && !images) return NULL;
	size_t retained = prefix ? prefix->premise_count - 2 : 0, suffix;
	const struct pg_context *base = prefix ? pg_evidence_context(prefix->premises[0]) : NULL;
	if (pg_context_extension_size(pg_evidence_context(source), base, &suffix) || suffix != count) return NULL;
	if (count > SIZE_MAX - retained) return NULL;
	size_t total = retained + count;
	if (total > SIZE_MAX / sizeof(struct pg_binding_value)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_context *)) return NULL;
	if (total > SIZE_MAX / sizeof(const struct pg_evidence *) - 2) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **declarations = pg_alloc(&temporary, count * sizeof(*declarations));
	const struct pg_occurrence **typed_images = pg_alloc(&temporary, total * sizeof(*typed_images));
	const struct pg_evidence **premises = pg_alloc(&temporary, (total + 2) * sizeof(*premises));
	if (!premises) goto done;
	if (count && !declarations) goto done;
	if (total && !typed_images) goto done;
	const struct pg_evidence *scope = source;
	for (size_t i = count; i; --i) { declarations[i - 1] = scope; scope = scope->premises[0]; }
	premises[0] = source;
	premises[1] = destination;
	/* A lifted prefix is the same checked map in a larger destination.
	 * Only the new suffix requires dependent classifier substitution. */
	for (size_t i = 0; i < retained; ++i) {
		premises[i + 2] = pg_prove_projection(typing, destination, prefix->premises[i + 2]);
		if (!premises[i + 2]) goto done;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		if (!pg_evidence_owned_by(image, typing)) goto done;
		if (pg_evidence_judgement(image) != binding_judgement(declarations[i])) goto done;
		if (pg_evidence_context(image) != pg_evidence_context(destination)) goto done;
		premises[retained + i + 2] = image;
	}
	uint64_t hash;
	result = find_record(typing, PG_CONTEXT_SUBSTITUTION,
		pg_evidence_context(destination), NULL, total + 2, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < total; ++i) typed_images[i] = pg_evidence_subject(premises[i + 2]);
	const struct pg_context_map *map = pg_context_map(typing, pg_evidence_context(source),
		pg_evidence_context(destination), total, typed_images);
	if (!map) goto done;
	const struct pg_binding_value *bindings = pg_context_map_bindings(map);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		const struct pg_term *expected = pg_substitution_compute(&typing->substitutions, pg_evidence_context(declarations[i])->declared_type, retained + i, bindings);
		if (!expected) goto done;
		if (pg_alpha_equal(expected, pg_evidence_subject(image)->classifier) != 1) goto done;
	}
	result = accept_record(typing, PG_CONTEXT_SUBSTITUTION,
		pg_evidence_context(destination), NULL, total + 2, premises, NULL, map);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_context_map *pg_evidence_context_map(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_CONTEXT_SUBSTITUTION ? evidence->conclusion.map : NULL;
}

const struct pg_evidence *pg_substitution_image(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(substitution, typing)) return NULL;
	if (substitution->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	const struct pg_binding_value *bindings =
		pg_context_map_bindings(substitution->conclusion.map);
	for (size_t i = 0; i < substitution->premise_count - 2; ++i)
		if (bindings[i].binder == binder) return substitution->premises[i + 2];
	return NULL;
}

const struct pg_evidence *pg_prove_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, const struct pg_evidence *const *images)
{
	return substitution_build(typing, source, destination, NULL, count, images);
}

const struct pg_evidence *pg_prove_substitution_projection(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination)
{
	if (!context_proof(typing, source) || !context_proof(typing, destination)) return NULL;
	size_t count;
	if (pg_context_extension_size(pg_evidence_context(destination), pg_evidence_context(source), &count)) return NULL;
	if (pg_context_extension_size(pg_evidence_context(source), NULL, &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	const struct pg_context *context = pg_evidence_context(source);
	for (size_t i = count; i; --i, context = context->parent)
		images[i - 1] = pg_prove_variable(typing, destination, context->binder);
	const struct pg_evidence *result = pg_prove_substitution(typing, source, destination, count, images);
	free(images);
	return result;
}

const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(substitution, typing)) return NULL;
	if (substitution->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(proof, typing) || !pg_evidence_subject(proof)) return NULL;
	if (pg_evidence_context(proof) != pg_evidence_context(substitution->premises[0])) return NULL;
	const struct pg_evidence *premises[] = {substitution, proof};
	uint64_t hash;
	const struct pg_evidence *result = find_record(typing, PG_REINDEX,
		pg_evidence_context(substitution), NULL, 2, premises, NULL, &hash);
	if (result) return result;
	struct pg_occurrence_action *action = pg_occurrence_action_request(typing,
		substitution->conclusion.map, pg_evidence_subject(proof));
	while (pg_occurrence_action_advance(action, UINT64_MAX) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_occurrence *subject = pg_occurrence_action_result(action);
	return subject ? accept(typing, PG_REINDEX, subject->context, subject, 2, premises) : NULL;
}

static int substitution_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(proof, typing)) return 0;
	return proof->rule == PG_CONTEXT_SUBSTITUTION;
}

/* Close the varied suffix before substituting the common prefix. One action
 * then consumes every boundary triple; this is not iterated reflexivity. */
static const struct pg_term *family_action_core(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *left,
	const struct pg_evidence *right, size_t common, size_t count,
	const struct pg_evidence *const *paths)
{
	const struct pg_term *abstraction = pg_evidence_subject(source)->core;
	const struct pg_context *context = pg_evidence_context(source);
	for (size_t i = 0; i < count; ++i, context = context->parent)
		abstraction = pg_lambda(typing->graph, context->binder, abstraction);
	const struct pg_binding_value *bindings = pg_context_map_bindings(left->conclusion.map);
	abstraction = pg_substitution_compute(&typing->substitutions, abstraction, common, bindings);
	if (!abstraction) return NULL;
	const struct pg_term *result = pg_identity_action(typing->graph, abstraction);
	for (size_t i = 0; i < count; ++i) {
		result = pg_identity_instance(typing->graph, result,
			pg_evidence_subject(left->premises[common + i + 2])->core,
			pg_evidence_subject(right->premises[common + i + 2])->core);
		result = pg_application(typing->graph, result, pg_evidence_subject(paths[i])->core);
	}
	return result;
}

const struct pg_evidence *pg_prove_family_identity_type(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (!pg_evidence_owned_by(family, typing)) return NULL;
	enum pg_evidence_judgement elements;
	switch (pg_evidence_judgement(family)) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!substitution_proof(typing, left_substitution)) return NULL;
	if (!substitution_proof(typing, right_substitution)) return NULL;
	if (pg_evidence_context(left_substitution->premises[0]) != pg_evidence_context(family)) return NULL;
	if (pg_evidence_context(right_substitution->premises[0]) != pg_evidence_context(family)) return NULL;
	const struct pg_context *context = pg_evidence_context(left_substitution);
	if (pg_evidence_context(right_substitution) != context) return NULL;
	size_t arity = left_substitution->premise_count - 2;
	if (count > arity) return NULL;
	if (count && !paths) return NULL;
	if (count > SIZE_MAX / sizeof(const void *) - 5) return NULL;
	size_t common = arity - count;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 5) * sizeof(*premises));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 3) * sizeof(*operands));
	const struct pg_evidence **declarations = pg_alloc(&temporary, count * sizeof(*declarations));
	if (!premises || !operands || (count && !declarations)) goto done;
	premises[0] = family;
	premises[1] = left_substitution;
	premises[2] = right_substitution;
	for (size_t i = 0; i < count; ++i) premises[i + 3] = paths[i];
	premises[count + 3] = left;
	premises[count + 4] = right;
	uint64_t hash;
	result = find_record(typing, PG_FAMILY_IDENTITY_FORM, context, NULL, count + 5, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < common; ++i) {
		if (pg_alpha_equal(pg_evidence_subject(left_substitution->premises[i + 2])->core,
			pg_evidence_subject(right_substitution->premises[i + 2])->core) != 1) goto done;
	}
	const struct pg_evidence *declaration = left_substitution->premises[0];
	for (size_t i = count; i; --i) {
		declarations[i - 1] = declaration->premises[1];
		declaration = declaration->premises[0];
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_term *acted = family_action_core(typing, declarations[i],
			left_substitution, right_substitution, common, i, paths);
		const struct pg_term *path_type = pg_identity_instance(typing->graph, acted,
			pg_evidence_subject(left_substitution->premises[common + i + 2])->core,
			pg_evidence_subject(right_substitution->premises[common + i + 2])->core);
		if (!endpoint(typing, paths[i], PG_JUDGEMENT_VALUE, context, path_type)) goto done;
	}
	const struct pg_evidence *ltype = pg_prove_reindex(typing, left_substitution, family);
	const struct pg_evidence *rtype = pg_prove_reindex(typing, right_substitution, family);
	if (!ltype || !rtype) goto done;
	if (!endpoint(typing, left, elements, context, pg_evidence_subject(ltype)->core)) goto done;
	if (!endpoint(typing, right, elements, context, pg_evidence_subject(rtype)->core)) goto done;
	const struct pg_term *acted = family_action_core(typing, family,
		left_substitution, right_substitution, common, count, paths);
	const struct pg_term *core = pg_identity_instance(typing->graph, acted, pg_evidence_subject(left)->core, pg_evidence_subject(right)->core);
	if (!core) goto done;
	operands[0] = pg_evidence_subject(family);
	for (size_t i = 0; i < count; ++i) operands[i + 1] = pg_evidence_subject(paths[i]);
	operands[count + 1] = pg_evidence_subject(left);
	operands[count + 2] = pg_evidence_subject(right);
	const struct pg_context_map *maps[] = {pg_evidence_context_map(left_substitution), pg_evidence_context_map(right_substitution)};
	const struct pg_occurrence *subject = pg_occurrence_intern(typing, &(struct pg_occurrence){
		.judgement = pg_evidence_judgement(family), .context = context, .core = core,
		.classifier = pg_evidence_classifier(family), .operand_count = count + 3,
		.map_count = 2}, operands, maps);
	if (!subject) goto done;
	result = accept(typing, PG_FAMILY_IDENTITY_FORM, context,
		subject, count + 5, premises);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_family_action(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *term,
	const struct pg_evidence *left_substitution,
	const struct pg_evidence *right_substitution, size_t count,
	const struct pg_evidence *const *paths)
{
	if (!pg_evidence_owned_by(family, typing)) return NULL;
	enum pg_evidence_judgement elements;
	switch (pg_evidence_judgement(family)) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!endpoint(typing, term, elements, pg_evidence_context(family), pg_evidence_subject(family)->core)) return NULL;
	const struct pg_evidence *left = pg_prove_reindex(typing, left_substitution, term);
	const struct pg_evidence *right = pg_prove_reindex(typing, right_substitution, term);
	const struct pg_evidence *identity = pg_prove_family_identity_type(typing,
		family, left_substitution, right_substitution, count, paths, left, right);
	if (!identity) return NULL;
	const struct pg_evidence *premises[] = {identity, term};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_FAMILY_ACTION, pg_evidence_context(identity), NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *core = family_action_core(typing, term,
		left_substitution, right_substitution, left_substitution->premise_count - 2 - count, count, paths);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(term), pg_evidence_subject(identity)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, elements,
		core, pg_evidence_subject(identity), NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_FAMILY_ACTION, pg_evidence_context(identity),
		subject, 2, premises);
}

const struct pg_evidence *pg_prove_substitution_extend(struct pg_typing *typing,
	const struct pg_evidence *prefix, const struct pg_evidence *source,
	size_t count, const struct pg_evidence *const *values)
{
	if (!pg_evidence_owned_by(prefix, typing) || prefix->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	return substitution_build(typing, source, prefix->premises[1], prefix, count, values);
}

const struct pg_evidence *pg_prove_substitution_compose(struct pg_typing *typing,
	const struct pg_evidence *first, const struct pg_evidence *second)
{
	if (!substitution_proof(typing, first)) return NULL;
	if (!substitution_proof(typing, second)) return NULL;
	if (pg_evidence_context(first) != pg_evidence_context(second->premises[0])) return NULL;
	size_t count = first->premise_count - 2;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	const struct pg_evidence *result = NULL;
	if (count && !images) goto done;
	for (size_t i = 0; i < count; ++i) {
		struct pg_occurrence_action *action = pg_occurrence_action_request(typing,
			pg_evidence_context_map(second), pg_evidence_context_map(first)->images[i]);
		while (pg_occurrence_action_advance(action, UINT64_MAX) == PG_SUBSTITUTION_PENDING) {}
		images[i] = pg_prove_structural_subject(typing, pg_occurrence_action_result(action));
		if (!images[i]) goto done;
	}
	result = pg_prove_substitution(typing, first->premises[0], second->premises[1], count, images);
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* Pairing and lifting differ only in the destination and final image. Prefix
 * images are already checked; projection preserves their terms/classifiers. */
static const struct pg_evidence *substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *destination, const struct pg_evidence *image)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	return substitution_build(typing, source_extension, destination, substitution, 1, &image);
}

const struct pg_evidence *pg_prove_substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *image)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	return substitution_pair(typing, substitution, source_extension,
		substitution->premises[1], image);
}

const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND && source_extension->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (pg_evidence_context(source_extension)->parent != pg_evidence_context(substitution->premises[0])) return NULL;
	if (!binder || binder->kind != PG_BINDER || pg_context_lookup(pg_evidence_context(substitution), binder)) return NULL;
	struct pg_context_lift *work = pg_context_lift_request(typing,
		pg_evidence_context_map(substitution), pg_evidence_context(source_extension), binder);
	while (pg_context_lift_advance(work, 1024) == PG_SUBSTITUTION_PENDING) {}
	const struct pg_evidence *destination = lift_destination(typing, source_extension, substitution, work);
	if (!destination) return NULL;
	return substitution_pair(typing, substitution, source_extension, destination,
		pg_prove_variable(typing, destination, binder));
}

struct pattern_variable {
	struct pg_index_entry index;
	const struct pg_object *image, *binder;
};

static const struct pattern_variable *pattern_variable(const struct pg_index *index,
	const struct pg_object *image)
{
	for (struct pg_index_entry *p = pg_index_candidates(index, (uintptr_t)image); p; p = p->next) {
		const struct pattern_variable *entry = (const void *)p;
		if (entry->image == image) return entry;
	}
	return NULL;
}

enum pattern_type_phase { PATTERN_TYPE_ENTER, PATTERN_TYPE_RETURN, PATTERN_TYPE_THUNK,
	PATTERN_TYPE_PI, PATTERN_TYPE_ARGUMENTS, PATTERN_TYPE_PARAMETER };

struct pattern_type_frame {
	struct pattern_type_frame *parent;
	const struct pg_evidence *body, *context, *extension;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	struct pg_inductive_instance instance;
	const struct pg_evidence **images;
	size_t count, parameters, next;
	enum pattern_type_phase phase;
};

/* Invert whole constructor images through F/U, Pi results and nominal type
 * arguments. Rebuild with ordinary formation rules, never raw replacement in
 * a claimed type. The caller checks substitution back to the original body. */
static const struct pg_evidence *pattern_index_type(struct pg_typing *typing,
	const struct pg_evidence *prefix,
	const struct pg_evidence *pattern, const struct pg_evidence *inverse,
	const struct pg_evidence *body)
{
	if (!body) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	struct pattern_type_frame root = {.body = body, .context = inverse->premises[1]};
	struct pattern_type_frame *frame = &root;
	while (frame) {
		const struct pg_evidence *child = NULL, *child_context = frame->context;
		if (frame->phase == PATTERN_TYPE_ENTER) {
			const struct pg_term *core = pg_evidence_subject(frame->body)->core, *content, *domain;
			const struct pg_object *binder;
			if (pg_computation_type_view(core, &frame->totality, &frame->effects, &content)) {
				frame->phase = PATTERN_TYPE_RETURN;
				child = pg_prove_return_content(typing, frame->body);
			} else if (pg_thunk_type_view(core, &content)) {
				frame->phase = PATTERN_TYPE_THUNK;
				child = pg_prove_thunk_content(typing, frame->body);
			} else if (pg_pi_view(core, &domain, &binder, &content)) {
				frame->phase = PATTERN_TYPE_PI;
				frame->extension = pg_prove_context_extension(typing, frame->context, binder,
					pg_prove_pi_domain(typing, frame->body));
				child_context = frame->extension;
				child = pg_prove_pi_codomain(typing,
					pg_prove_projection(typing, child_context, frame->body),
					pg_prove_variable(typing, child_context, binder));
			} else if (pg_inductive_instance(typing, frame->body, &frame->instance)) {
				frame->phase = PATTERN_TYPE_ARGUMENTS;
				frame->parameters = frame->instance.parameters->premise_count - 2;
				const struct pg_evidence *map = frame->instance.indices
					? frame->instance.indices : frame->instance.parameters;
				frame->count = map->premise_count - 2;
				if (frame->count > SIZE_MAX / sizeof(*frame->images)) goto fail;
				frame->images = pg_alloc(&temporary, frame->count * sizeof(*frame->images));
				if (frame->count && !frame->images) goto fail;
				for (size_t i = 0; i < frame->count; ++i) frame->images[i] = map->premises[i + 2];
				continue;
			} else {
				result = frame->body;
				frame = frame->parent;
				continue;
			}
			if (!child) goto fail;
		} else if (frame->phase <= PATTERN_TYPE_PI) {
			if (frame->phase == PATTERN_TYPE_RETURN) result = pg_prove_computation_type(typing,
				frame->totality, frame->effects, result);
			else if (frame->phase == PATTERN_TYPE_THUNK) result = pg_prove_thunk_type(typing, result);
			else result = pg_prove_pi(typing, frame->extension, result);
			if (!result) goto fail;
			frame = frame->parent;
			continue;
		} else {
			if (frame->phase == PATTERN_TYPE_PARAMETER) {
				frame->images[frame->next - 1] = pg_prove_type_value(typing, result);
				if (!frame->images[frame->next - 1]) goto fail;
				frame->phase = PATTERN_TYPE_ARGUMENTS;
			}
			if (frame->next == frame->count) {
				const struct pg_evidence *parameters = pg_prove_substitution(typing,
					frame->instance.parameters->premises[0], frame->context,
					frame->parameters, frame->images);
				result = pg_prove_reindex(typing, parameters, frame->instance.formation);
				for (size_t i = frame->parameters + 1; result && i < frame->count; ++i)
					result = pg_prove_family_application(typing, result, frame->images[i]);
				if (!result) goto fail;
				frame = frame->parent;
				continue;
			}
			size_t i = frame->next++;
			if (i == frame->parameters) continue; /* Rebuild Self from the new parameters. */
			const struct pg_evidence *selected = NULL;
			const struct pg_evidence *extension = pattern->premises[0];
			for (size_t j = pattern->premise_count - 2; pg_evidence_context(extension) != pg_evidence_context(prefix);
				--j, extension = extension->premises[0]) {
				const struct pg_evidence *image = pattern->premises[j + 1];
				if (pg_evidence_subject(image)->core->kind == PG_REFERENCE && pg_evidence_subject(image)->core->as.reference->kind == PG_BINDER) continue;
				image = pg_prove_reindex(typing, inverse, image);
				if (!image || pg_alpha_equal(pg_evidence_subject(image)->core, pg_evidence_subject(frame->images[i])->core) != 1) continue;
				if (selected) goto fail;
				selected = pg_prove_variable(typing, frame->context, pg_evidence_context(extension)->binder);
				if (!selected) goto fail;
			}
			if (selected) { frame->images[i] = selected; continue; }
			child = pg_prove_value_type(typing, frame->images[i]);
			if (!child) continue;
			frame->phase = PATTERN_TYPE_PARAMETER;
		}
		struct pattern_type_frame *next = pg_alloc(&temporary, sizeof(*next));
		if (!next) goto fail;
		*next = (struct pattern_type_frame){.parent = frame, .body = child, .context = child_context};
		frame = next;
	}
	pg_graph_destroy(&temporary);
	return result;
fail:
	pg_graph_destroy(&temporary);
	return NULL;
}

const struct pg_evidence *pg_prove_pattern_type(struct pg_typing *typing,
	const struct pg_evidence *prefix,
	const struct pg_evidence *pattern, const struct pg_evidence *body)
{
	if (!typing) return NULL;
	if (!context_proof(typing, prefix) || !substitution_proof(typing, pattern)) return NULL;
	if (!pg_evidence_owned_by(body, typing)) return NULL;
	if (pg_evidence_judgement(body) != PG_JUDGEMENT_COMPUTATION_TYPE || pg_evidence_context(body) != pg_evidence_context(pattern)) return NULL;
	const struct pg_evidence *source = pattern->premises[0], *destination = pattern->premises[1];
	size_t source_count, destination_count, common;
	if (pg_context_extension_size(pg_evidence_context(source), pg_evidence_context(prefix), &source_count)) return NULL;
	if (pg_context_extension_size(pg_evidence_context(destination), pg_evidence_context(prefix), &destination_count)) return NULL;
	if (pg_context_extension_size(pg_evidence_context(prefix), NULL, &common)) return NULL;
	if (source_count > SIZE_MAX / sizeof(struct pattern_variable)) return NULL;
	if (destination_count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	struct pg_graph temporary = {0};
	struct pg_index variables = {0};
	const struct pg_evidence *result = NULL;
	struct pattern_variable *entries = pg_alloc(&temporary, source_count * sizeof(*entries));
	const struct pg_evidence **fields = pg_alloc(&temporary, destination_count * sizeof(*fields));
	if ((source_count && !entries) || (destination_count && !fields)) goto done;
	if (pg_index_init(&variables)) goto done;
	const struct pg_evidence *extension = source;
	for (size_t i = source_count; i; --i, extension = extension->premises[0]) {
		if (extension->rule != PG_CONTEXT_EXTEND) goto done;
		const struct pg_term *image = pg_evidence_subject(pattern->premises[common + i + 1])->core;
		if (image->kind != PG_REFERENCE || image->as.reference->kind != PG_BINDER) continue;
		if (pattern_variable(&variables, image->as.reference)) goto done;
		entries[i - 1].image = image->as.reference;
		entries[i - 1].binder = pg_evidence_context(extension)->binder;
		if (pg_index_insert(&variables, &entries[i - 1].index, (uintptr_t)image->as.reference)) goto done;
	}
	for (size_t i = common; i; --i, extension = extension->premises[0]) {
		const struct pg_object *binder = pg_evidence_context(extension)->binder;
		if (pg_evidence_subject(pattern->premises[i + 1])->core != pg_reference(typing->graph, binder)) goto done;
		if (pattern_variable(&variables, binder)) goto done;
	}
	extension = destination;
	for (size_t i = destination_count; i; --i, extension = extension->premises[0]) {
		if (extension->rule != PG_CONTEXT_EXTEND) goto done;
		fields[i - 1] = extension;
	}
	const struct pg_evidence *inverse = pg_prove_substitution_projection(typing, prefix, source);
	for (size_t i = 0; inverse && i < destination_count; ++i) {
		const struct pattern_variable *entry = pattern_variable(&variables, pg_evidence_context(fields[i])->binder);
		if (entry) {
			const struct pg_evidence *image = pg_prove_variable(typing, inverse->premises[1], entry->binder);
			const struct pg_evidence *domain = pg_prove_reindex(typing, inverse, fields[i]->premises[1]);
			if (!image || !domain) goto done;
			if (pg_alpha_equal(pg_evidence_subject(image)->classifier, pg_evidence_subject(domain)->core) == 1) {
				inverse = pg_prove_substitution_pair(typing, inverse, fields[i], image);
				continue;
			}
		}
		/* A dependent field may still mention a constructed index. Retain
		 * it until the inferred type is checked independent of that field. */
		inverse = pg_prove_substitution_lift(typing, inverse, fields[i], pg_binder(typing->graph));
	}
	if (!inverse) goto done;
	result = pg_prove_reindex(typing, inverse, body);
	result = pattern_index_type(typing, prefix, pattern, inverse, result);
	/* Keep every intermediate substitution total and typed. Only then remove
	 * fresh nuisance fields, checking that the result does not depend on them. */
	for (extension = inverse->premises[1]; result && pg_evidence_context(extension) != pg_evidence_context(source);
		extension = extension->premises[0])
		result = pg_prove_pi_constant_codomain(typing,
			pg_prove_pi(typing, extension, result));
	if (result) {
		const struct pg_evidence *instance = pg_prove_reindex(typing, pattern, result);
		if (!instance || pg_alpha_equal(pg_evidence_subject(instance)->core, pg_evidence_subject(body)->core) != 1) result = NULL;
	}
done:
	pg_index_destroy(&variables);
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_thunk_content(struct pg_typing *typing,
	const struct pg_evidence *thunk_type)
{
	thunk_type = pg_prove_value_type(typing, thunk_type);
	if (!thunk_type) return NULL;
	const struct pg_term *content;
	if (!pg_thunk_type_view(pg_evidence_subject(thunk_type)->core, &content)) return NULL;
	const struct pg_occurrence *subject = content_subject(typing, pg_evidence_subject(thunk_type),
		content, pg_evidence_subject(thunk_type)->classifier, PG_JUDGEMENT_COMPUTATION_TYPE);
	if (!subject) return NULL;
	return accept(typing, PG_THUNK_CONTENT,
		pg_evidence_context(thunk_type), subject, 1, &thunk_type);
}

const struct pg_evidence *pg_prove_return_content(struct pg_typing *typing,
	const struct pg_evidence *return_type)
{
	if (!pg_evidence_owned_by(return_type, typing)) return NULL;
	if (pg_evidence_judgement(return_type) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *content;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(pg_evidence_subject(return_type)->core, &totality, &effects, &content)) return NULL;
	const struct pg_occurrence *subject = content_subject(typing, pg_evidence_subject(return_type),
		content, pg_evidence_subject(return_type)->classifier, PG_JUDGEMENT_VALUE_TYPE);
	if (!subject) return NULL;
	return accept(typing, PG_RETURN_CONTENT,
		pg_evidence_context(return_type), subject, 1, &return_type);
}

const struct pg_evidence *pg_prove_pi_constant_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pg_evidence_judgement(pi) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CONSTANT_CODOMAIN, pg_evidence_context(pi), NULL, 1, &pi, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *codomain = pg_pi_constant_codomain(pg_evidence_subject(pi)->core);
	if (!codomain) return NULL;
	const struct pg_occurrence *subject;
	if (!structural_input(typing, pg_evidence_subject(pi), 1, &subject)) return NULL;
	subject = pg_occurrence_unproject(typing, subject, pg_evidence_context(pi));
	if (subject) {
		if (subject->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		if (pg_alpha_equal(subject->core, codomain) != 1) return NULL;
		subject = pg_occurrence_boundary(typing, subject, PG_JUDGEMENT_COMPUTATION_TYPE,
			pg_evidence_classifier(pi));
	} else subject = pg_occurrence_selected(typing, pg_evidence_subject(pi), 1, NULL,
		PG_JUDGEMENT_COMPUTATION_TYPE, codomain, pg_evidence_classifier(pi));
	if (!subject) return NULL;
	return accept(typing, PG_PI_CONSTANT_CODOMAIN,
		pg_evidence_context(pi), subject, 1, &pi);
}

const struct pg_object *pg_operation_label_create(struct pg_graph *graph,
	const struct pg_term *payload, const struct pg_term *response)
{
	if (!graph || !payload || !response) return NULL;
	struct operation_label *label = pg_alloc(graph, sizeof(*label));
	if (!label) return NULL;
	*label = (struct operation_label){
		.object = {PG_SEMANTIC_OBJECT, &operation_label_class}, .payload = payload, .response = response};
	return &label->object;
}

int pg_operation_label_types(const struct pg_object *object,
	const struct pg_term **payload, const struct pg_term **response)
{
	if (pg_host_operation_types(object, payload, response)) return 1;
	if (!object || object->owner != &operation_label_class || object->kind != PG_SEMANTIC_OBJECT) return 0;
	if (!payload || !response) return 0;
	const struct operation_label *label = (const void *)object;
	*payload = label->payload;
	*response = label->response;
	return 1;
}

const struct pg_operation_declaration *pg_operation_declaration_at(struct pg_typing *typing,
	const struct pg_object *label, const struct pg_evidence *payload_type,
	const struct pg_evidence *response_type)
{
	const struct pg_term *payload, *response;
	if (!pg_operation_label_types(label, &payload, &response)) return NULL;
	payload_type = pg_prove_value_type(typing, payload_type);
	response_type = pg_prove_value_type(typing, response_type);
	if (!payload_type || !response_type) return NULL;
	if (pg_evidence_context(payload_type) || pg_evidence_context(response_type)) return NULL;
	if (pg_evidence_subject(payload_type)->core != payload || pg_evidence_subject(response_type)->core != response) return NULL;
	struct pg_index *index = &typing->graph->objects;
	if (!index->capacity && pg_index_init(index)) return NULL;
	uint64_t hash = ((uintptr_t)label ^ (uintptr_t)payload_type) * UINT64_C(1099511628211) ^ (uintptr_t)response_type;
	for (struct pg_index_entry *p = pg_index_candidates(index, hash); p; p = p->next) {
		const struct pg_operation_declaration *old = (const void *)p;
		if (old->base.object.owner != &operation_declaration_class) continue;
		if (old->label == label && old->payload_type == payload_type && old->response_type == response_type) return old;
	}
	struct pg_operation_declaration *declaration = pg_alloc(typing->graph, sizeof(*declaration));
	if (!declaration) return NULL;
	*declaration = (struct pg_operation_declaration){
		.base.object = {PG_SEMANTIC_OBJECT, &operation_declaration_class}, .label = label,
		.payload_type = payload_type, .response_type = response_type};
	return pg_index_insert(index, &declaration->base.index, hash) ? NULL : declaration;
}

const struct pg_operation_declaration *pg_operation_declaration(struct pg_typing *typing,
	const struct pg_evidence *payload_type, const struct pg_evidence *response_type)
{
	payload_type = pg_prove_value_type(typing, payload_type);
	response_type = pg_prove_value_type(typing, response_type);
	if (!payload_type || !response_type) return NULL;
	if (pg_evidence_context(payload_type) || pg_evidence_context(response_type)) return NULL;
	return pg_operation_declaration_at(typing, pg_operation_label_create(typing->graph,
		pg_evidence_subject(payload_type)->core, pg_evidence_subject(response_type)->core), payload_type, response_type);
}

const struct pg_evidence *pg_operation_payload_type(const struct pg_operation_declaration *declaration)
{
	return declaration ? declaration->payload_type : NULL;
}

const struct pg_evidence *pg_operation_response_type(const struct pg_operation_declaration *declaration)
{
	return declaration ? declaration->response_type : NULL;
}

const struct pg_object *pg_operation_label(const struct pg_operation_declaration *declaration)
{
	return declaration ? declaration->label : NULL;
}

const struct pg_operation_declaration *pg_evidence_request_declaration(const struct pg_evidence *evidence)
{
	return evidence && evidence->rule == PG_REQUEST_INTRO ? evidence->certificate : NULL;
}

const struct pg_evidence *pg_prove_request(struct pg_typing *typing,
	const struct pg_operation_declaration *declaration,
	const struct pg_evidence *payload, const struct pg_evidence *continuation)
{
	if (!declaration) return NULL;
	if (!pg_evidence_owned_by(declaration->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(declaration->response_type, typing)) return NULL;
	if (!pg_evidence_owned_by(payload, typing)) return NULL;
	if (!pg_evidence_owned_by(continuation, typing)) return NULL;
	if (pg_evidence_judgement(payload) != PG_JUDGEMENT_VALUE) return NULL;
	if (pg_evidence_judgement(continuation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_context(payload) != pg_evidence_context(continuation)) return NULL;
	const struct pg_evidence *premises[] = {declaration->payload_type, declaration->response_type, payload, continuation};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_REQUEST_INTRO, pg_evidence_context(payload), NULL, 4, premises, declaration, &hash);
	if (existing) return existing;
	if (pg_alpha_equal(pg_evidence_subject(payload)->classifier, pg_evidence_subject(declaration->payload_type)->core) != 1) return NULL;
	const struct pg_term *domain, *codomain, *result_type;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(continuation)->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, pg_evidence_subject(declaration->response_type)->core) != 1) return NULL;
	codomain = pg_pi_constant_codomain(pg_evidence_subject(continuation)->classifier);
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(codomain, &totality, &effects, &result_type)) return NULL;
	const struct pg_object *label = pg_operation_label(declaration);
	const struct pg_effect_row *row = pg_effect_union(typing->graph, pg_effect_row(typing->graph, 1, &label), effects);
	const struct pg_evidence *type = pg_prove_pi_constant_codomain(typing, formed_classifier(typing, continuation));
	type = pg_prove_computation_type(typing, totality, row, pg_prove_return_content(typing, type));
	const struct pg_term *core = pg_computation_request(typing->graph, label, pg_evidence_subject(payload)->core, pg_evidence_subject(continuation)->core);
	if (!type || !core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(payload), pg_evidence_subject(continuation)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, core, pg_evidence_subject(type), NULL, 2, operands);
	if (!subject) return NULL;
	return accept_record(typing, PG_REQUEST_INTRO,
		pg_evidence_context(payload), subject, 4, premises, declaration, NULL);
}

const struct pg_evidence *pg_prove_operation_function(struct pg_typing *typing,
	const struct pg_operation_declaration *declaration)
{
	if (!declaration) return NULL;
	if (!pg_evidence_owned_by(declaration->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(declaration->response_type, typing)) return NULL;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *b = pg_binder(typing->graph);
	const struct pg_evidence *scope = pg_prove_context_extension(typing, empty, a, declaration->payload_type);
	const struct pg_evidence *response_type = pg_prove_projection(typing, scope, declaration->response_type);
	const struct pg_evidence *response_scope = pg_prove_context_extension(typing, scope, b, response_type);
	const struct pg_evidence *returned = pg_prove_return_contract(typing, PG_TOTALITY_TOTAL,
		pg_prove_variable(typing, response_scope, b));
	const struct pg_evidence *response_pi = pg_prove_pi(typing, response_scope,
		pg_prove_classifier(typing, response_scope, returned));
	const struct pg_evidence *continuation = pg_prove_lambda(typing, response_pi, returned);
	const struct pg_evidence *body = pg_prove_request(typing, declaration,
		pg_prove_variable(typing, scope, a), continuation);
	const struct pg_evidence *pi = pg_prove_pi(typing, scope,
		pg_prove_classifier(typing, scope, body));
	return pg_prove_lambda(typing, pi, body);
}

const struct pg_evidence *pg_prove_handler_context(struct pg_typing *typing,
	const struct pg_operation_declaration *operation, const struct pg_evidence *context,
	const struct pg_evidence *carrier, const struct pg_object *payload, const struct pg_object *resume)
{
	if (!operation || !context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(operation->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(operation->response_type, typing)) return NULL;
	if (!pg_evidence_owned_by(carrier, typing)) return NULL;
	if (pg_evidence_context(carrier) != pg_evidence_context(context)) return NULL;
	if (pg_evidence_judgement(carrier) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_effect_row *effects;
	const struct pg_term *result;
	enum pg_totality totality;
	if (!pg_computation_type_view(pg_evidence_subject(carrier)->core, &totality, &effects, &result)) return NULL;
	const struct pg_evidence *response_type = pg_prove_projection(typing, context, operation->response_type);
	const struct pg_evidence *response_scope = pg_prove_context_extension(typing, context,
		pg_binder(typing->graph), response_type);
	const struct pg_evidence *resume_type = pg_prove_thunk_type(typing,
		pg_prove_pi(typing, response_scope,
			pg_prove_projection(typing, response_scope, carrier)));
	const struct pg_evidence *payload_scope = pg_prove_context_extension(typing, context, payload,
		pg_prove_projection(typing, context, operation->payload_type));
	return pg_prove_context_extension(typing, payload_scope, resume,
		pg_prove_projection(typing, payload_scope, resume_type));
}

static int returning_within(const struct pg_term *actual, const struct pg_term *carrier)
{
	const struct pg_effect_row *effects, *allowed;
	const struct pg_term *value, *result;
	enum pg_totality actual_grade, carrier_grade;
	if (!pg_computation_type_view(actual, &actual_grade, &effects, &value)) return 0;
	if (!pg_computation_type_view(carrier, &carrier_grade, &allowed, &result)) return 0;
	if (actual_grade < carrier_grade) return 0;
	if (pg_effect_subset(effects, allowed) != 1) return 0;
	return pg_alpha_equal(value, result) == 1;
}

static int handler_clause_type(const struct pg_term *type,
	const struct pg_operation_declaration *operation, const struct pg_term *carrier)
{
	const struct pg_term *domain, *codomain, *resume;
	const struct pg_object *binder;
	if (!pg_pi_view(type, &domain, &binder, &codomain)) return 0;
	if (pg_alpha_equal(domain, pg_evidence_subject(operation->payload_type)->core) != 1) return 0;
	type = pg_pi_constant_codomain(type);
	if (!pg_pi_view(type, &domain, &binder, &codomain)) return 0;
	if (!returning_within(pg_pi_constant_codomain(type), carrier)) return 0;
	if (!pg_thunk_type_view(domain, &resume)) return 0;
	if (!pg_pi_view(resume, &domain, &binder, &codomain)) return 0;
	if (pg_alpha_equal(domain, pg_evidence_subject(operation->response_type)->core) != 1) return 0;
	return pg_alpha_equal(pg_pi_constant_codomain(resume), carrier) == 1;
}

const struct pg_evidence *pg_prove_handler(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *returned,
	const struct pg_evidence *carrier, size_t count, const struct pg_handler_clause *clauses)
{
	if (!count) return pg_prove_effect_subsumption(typing,
		pg_prove_fold(typing, computation, returned), carrier);
	if (!clauses || count > (SIZE_MAX - 3) / 3) return NULL;
	size_t n = 3 + 3 * count;
	if (n > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_operation_clause)) return NULL;
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!pg_evidence_owned_by(returned, typing)) return NULL;
	if (!pg_evidence_owned_by(carrier, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_judgement(returned) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_judgement(carrier) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(computation) != pg_evidence_context(returned)) return NULL;
	if (pg_evidence_context(computation) != pg_evidence_context(carrier)) return NULL;
	const struct pg_effect_row *input_effects, *output_effects;
	const struct pg_term *input_type, *result_type, *domain, *codomain;
	const struct pg_object *binder;
	enum pg_totality input_grade, output_grade;
	if (!pg_computation_type_view(pg_evidence_subject(computation)->classifier, &input_grade, &input_effects, &input_type)) return NULL;
	if (!pg_computation_type_view(pg_evidence_subject(carrier)->core, &output_grade, &output_effects, &result_type)) return NULL;
	/* Handling cannot guarantee that an unknown prefix reaches RETURN or an
	 * operation. As in sequencing, a typed literal RETURN is already finite. */
	if (input_grade < output_grade && !pg_prove_return_value(typing, computation)) return NULL;
	if (!pg_pi_view(pg_evidence_subject(returned)->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, input_type) != 1) return NULL;
	if (!returning_within(pg_pi_constant_codomain(pg_evidence_subject(returned)->classifier), pg_evidence_subject(carrier)->core)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **premises = pg_alloc(&temporary, n * sizeof(*premises));
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	struct pg_operation_clause *raw = pg_alloc(&temporary, count * sizeof(*raw));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 2) * sizeof(*operands));
	const struct pg_evidence *result = NULL;
	if (!premises || !labels || !raw || !operands) goto done;
	premises[0] = computation; premises[1] = returned; premises[2] = carrier;
	operands[0] = pg_evidence_subject(computation); operands[1] = pg_evidence_subject(returned);
	for (size_t i = 0; i < count; ++i) {
		const struct pg_operation_declaration *operation = clauses[i].operation;
		const struct pg_evidence *body = clauses[i].body;
		if (!operation || !pg_evidence_owned_by(body, typing)) goto done;
		if (!pg_evidence_owned_by(operation->payload_type, typing)) goto done;
		if (!pg_evidence_owned_by(operation->response_type, typing)) goto done;
		if (pg_evidence_judgement(body) != PG_JUDGEMENT_COMPUTATION || pg_evidence_context(body) != pg_evidence_context(computation)) goto done;
		if (!handler_clause_type(pg_evidence_subject(body)->classifier, operation, pg_evidence_subject(carrier)->core)) goto done;
		labels[i] = pg_operation_label(operation);
		raw[i] = (struct pg_operation_clause){labels[i], pg_evidence_subject(body)->core};
		operands[i + 2] = pg_evidence_subject(body);
		premises[3 + 3 * i] = operation->payload_type;
		premises[4 + 3 * i] = operation->response_type;
		premises[5 + 3 * i] = body;
	}
	const struct pg_effect_row *handled = pg_effect_row(typing->graph, count, labels);
	const struct pg_effect_row *forwarded = pg_effect_difference(typing->graph, input_effects, handled);
	if (pg_effect_subset(forwarded, output_effects) != 1) goto done;
	const struct pg_term *core = pg_computation_fold(typing->graph, pg_evidence_subject(computation)->core,
		pg_evidence_subject(returned)->core, count, raw);
	if (!core) goto done;
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, core, pg_evidence_subject(carrier), NULL, count + 2, operands);
	if (!subject) goto done;
	const struct pg_handler_signature *signature = pg_handler_signature(typing->graph, count, labels);
	if (!signature) goto done;
	result = accept_record(typing, PG_HANDLER_ELIM,
		pg_evidence_context(computation), subject, n, premises, signature, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_effect_subsumption(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *target_type)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!pg_evidence_owned_by(target_type, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_judgement(target_type) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (pg_evidence_context(computation) != pg_evidence_context(target_type)) return NULL;
	const struct pg_evidence *premises[] = {computation, target_type};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_EFFECT_SUBSUMPTION, pg_evidence_context(computation), NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_effect_row *source_row, *target_row;
	const struct pg_term *source_value, *target_value;
	enum pg_totality source_totality, target_totality;
	if (!pg_computation_type_view(pg_evidence_subject(computation)->classifier, &source_totality, &source_row, &source_value)) return NULL;
	if (!pg_computation_type_view(pg_evidence_subject(target_type)->core, &target_totality, &target_row, &target_value)) return NULL;
	if (source_totality < target_totality) return NULL;
	if (pg_effect_subset(source_row, target_row) != 1) return NULL;
	if (pg_alpha_equal(source_value, target_value) != 1) return NULL;
	const struct pg_occurrence *subject = pg_occurrence_reclassified(typing, pg_evidence_subject(computation),
		pg_evidence_subject(target_type));
	if (!subject) return NULL;
	return accept(typing, PG_EFFECT_SUBSUMPTION,
		pg_evidence_context(computation), subject, 2, premises);
}

const struct pg_evidence *pg_prove_fold(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *continuation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!pg_evidence_owned_by(continuation, typing)) return NULL;
	if (pg_evidence_judgement(computation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_judgement(continuation) != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (pg_evidence_context(computation) != pg_evidence_context(continuation)) return NULL;
	const struct pg_evidence *premises[] = {computation, continuation};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_FOLD_ELIM, pg_evidence_context(computation), NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *value_type, *domain, *codomain;
	const struct pg_object *binder;
	const struct pg_effect_row *effects, *following;
	enum pg_totality first_totality, next_totality;
	if (!pg_computation_type_view(pg_evidence_subject(computation)->classifier, &first_totality, &effects, &value_type)) return NULL;
	if (!pg_pi_view(pg_evidence_subject(continuation)->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, value_type) != 1) return NULL;
	codomain = pg_pi_constant_codomain(pg_evidence_subject(continuation)->classifier);
	if (!codomain) return NULL;
	const struct pg_evidence *type = pg_prove_pi_constant_codomain(typing, formed_classifier(typing, continuation));
	if (!type) return NULL;
	const struct pg_term *result_type;
	if (pg_computation_type_view(codomain, &next_totality, &following, &result_type)) {
		enum pg_totality totality = first_totality < next_totality ? first_totality : next_totality;
		type = pg_prove_computation_type(typing, totality,
			pg_effect_union(typing->graph, effects, following), pg_prove_return_content(typing, type));
		if (!type) return NULL;
	} else {
		if (pg_effect_count(effects)) return NULL;
		/* A possibly divergent prefix cannot inherit a total latent result
		 * merely because the continuation immediately produces a Lambda. */
		/* Typed RETURN inversion also supplies a finite prefix without
		 * evaluating its value (which may itself be a suspended computation). */
		if (first_totality == PG_TOTALITY_UNSPECIFIED && !pg_prove_return_value(typing, computation)) {
			if (!unspecified_computation_result(codomain)) return NULL;
		}
	}
	const struct pg_term *core = pg_computation_fold(typing->graph,
		pg_evidence_subject(computation)->core, pg_evidence_subject(continuation)->core, 0, NULL);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {pg_evidence_subject(computation), pg_evidence_subject(continuation)};
	const struct pg_occurrence *subject = pg_occurrence_typed(typing, PG_JUDGEMENT_COMPUTATION, core, pg_evidence_subject(type), NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_FOLD_ELIM,
		pg_evidence_context(computation), subject, 2, premises);
}

const struct pg_evidence *pg_prove_pi_domain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pg_evidence_judgement(pi) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(pi)->core, &domain, &binder, &codomain)) return NULL;
	const struct pg_term *index, *fiber;
	const struct pg_object *parameter;
	if (pg_pi_view(domain, &index, &parameter, &fiber)) return NULL;
	const struct pg_occurrence *subject;
	if (!structural_input(typing, pg_evidence_subject(pi), 0, &subject)) return NULL;
	if (subject && subject->judgement == PG_JUDGEMENT_VALUE_TYPE && subject->context == pg_evidence_context(pi)) {
		uint64_t bound, level;
		if (!pg_universe_level(pg_evidence_classifier(pi), &bound)) return NULL;
		if (!pg_universe_level(subject->classifier, &level) || level > bound) return NULL;
		if (pg_alpha_equal(subject->core, domain) != 1) return NULL;
		return accept(typing, PG_PI_DOMAIN, subject->context, subject, 1, &pi);
	}
	subject = pg_occurrence_selected(typing, pg_evidence_subject(pi), 0, NULL,
		PG_JUDGEMENT_VALUE_TYPE, domain, pg_evidence_classifier(pi));
	if (!subject) return NULL;
	return accept(typing, PG_PI_DOMAIN,
		pg_evidence_context(pi), subject, 1, &pi);
}

const struct pg_evidence *pg_prove_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pg_evidence_judgement(pi) != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (pg_evidence_judgement(argument) != PG_JUDGEMENT_VALUE && pg_evidence_judgement(argument) != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pg_evidence_context(pi) != pg_evidence_context(argument)) return NULL;
	const struct pg_evidence *premises[] = {pi, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CODOMAIN, pg_evidence_context(pi), NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pg_evidence_subject(pi)->core, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, pg_evidence_subject(argument)->classifier) != 1) return NULL;
	struct pg_binding_value binding = {binder, pg_evidence_subject(argument)->core};
	const struct pg_term *type = pg_substitution_compute(&typing->substitutions, codomain, 1, &binding);
	const struct pg_occurrence *subject = pg_occurrence_selected(typing, pg_evidence_subject(pi), 1,
		pg_evidence_subject(argument), PG_JUDGEMENT_COMPUTATION_TYPE, type, pg_evidence_classifier(pi));
	if (!subject) return NULL;
	return accept(typing, PG_PI_CODOMAIN,
		pg_evidence_context(pi), subject, 2, premises);
}

struct pg_typed_query *pg_classifier_request(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *term)
{
	if (!context_proof(typing, context) || !pg_evidence_owned_by(term, typing)) return NULL;
	if (pg_evidence_context(context) != pg_evidence_context(term)) return NULL;
	if (pg_evidence_judgement(term) != PG_JUDGEMENT_VALUE &&
		pg_evidence_judgement(term) != PG_JUDGEMENT_COMPUTATION) return NULL;
	return typed_query_request(typing, pg_evidence_subject(term), NULL, TYPED_CLASSIFIER, 0, NULL);
}

static int typed_classifier_step(struct pg_typed_query *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_occurrence *subject = work->source;
	uint64_t level;
	if (subject->type) work->result = pg_prove_structural_subject(typing, subject->type);
	else if (subject->judgement == PG_JUDGEMENT_VALUE &&
		pg_universe_level(subject->classifier, &level)) {
		work->result = pg_prove_universe(typing,
			conclusion_first(typing, PG_JUDGEMENT_CONTEXT, subject->context), level);
	} else if (subject->judgement == PG_JUDGEMENT_VALUE && subject->core->kind == PG_REFERENCE &&
		subject->core->as.reference->kind == PG_BINDER) {
		const struct pg_context *declaration = pg_context_lookup(subject->context, subject->core->as.reference);
		const struct pg_evidence *scope = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, declaration);
		if (scope && scope->rule == PG_CONTEXT_EXTEND)
			work->result = pg_prove_projection(typing,
				conclusion_first(typing, PG_JUDGEMENT_CONTEXT, subject->context), scope->premises[1]);
	} else {
		if (!work->input) work->input = pg_occurrence_type_request(typing, subject);
		enum pg_occurrence_input_status status = pg_occurrence_input_advance(work->input, 1);
		if (status == PG_INPUT_PENDING) return 0;
		work->result = pg_prove_structural_subject(typing, pg_occurrence_input_result(work->input));
	}
	if (!work->result) return -1;
	return pg_evidence_context(work->result) == subject->context &&
		pg_alpha_equal(pg_evidence_subject(work->result)->core, subject->classifier) == 1 ? 1 : -1;
}

/* Universe successors are formed lazily in the typing store's graph. */
static const struct pg_evidence *formed_classifier(struct pg_typing *typing,
	const struct pg_evidence *term)
{
	if (!pg_evidence_owned_by(term, typing)) return NULL;
	const struct pg_evidence *context = conclusion_first(typing, PG_JUDGEMENT_CONTEXT, pg_evidence_context(term));
	return pg_prove_classifier(typing, context, term);
}

const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	struct pg_typed_query *work = pg_classifier_request(typing, context, term);
	while (!pg_typed_query_advance(work, 1024)) {}
	return pg_typed_query_result(work);
}

enum pg_evidence_rule pg_evidence_rule(const struct pg_evidence *evidence) { return evidence->rule; }
int pg_identity_boundary_view(const struct pg_evidence *formation, struct pg_identity_boundary *output)
{
	if (!formation || !output) return 0;
	struct pg_identity_boundary view = {0};
	switch (formation->rule) {
	case PG_IDENTITY_FORM: case PG_IDENTITY_INSTANCE:
		view.left = formation->premises[1];
		view.right = formation->premises[2];
		break;
	case PG_FAMILY_IDENTITY_FORM:
		view.path_count = formation->premise_count - 5;
		view.paths = formation->premises + 3;
		view.left_substitution = formation->premises[1];
		view.right_substitution = formation->premises[2];
		view.left = formation->premises[view.path_count + 3];
		view.right = formation->premises[view.path_count + 4];
		break;
	default: return 0;
	}
	view.family = formation->premises[0];
	*output = view;
	return 1;
}

enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence)
{
	const struct pg_occurrence *subject = pg_evidence_subject(evidence);
	return subject ? subject->judgement : evidence->rule == PG_CONTEXT_SUBSTITUTION
		? PG_JUDGEMENT_SUBSTITUTION : PG_JUDGEMENT_CONTEXT;
}
int pg_evidence_owned_by(const struct pg_evidence *evidence, const struct pg_typing *typing)
{
	return evidence && typing && typing->owner_key && evidence->owner == typing->owner_key;
}
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence)
{
	const struct pg_occurrence *subject = pg_evidence_subject(evidence);
	if (subject) return subject->context;
	return evidence->rule == PG_CONTEXT_SUBSTITUTION ? evidence->conclusion.map->destination : evidence->conclusion.context;
}
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence)
{
	switch (evidence->rule) {
	case PG_CONTEXT_EMPTY: case PG_CONTEXT_EXTEND: case PG_CONTEXT_FAMILY_EXTEND:
	case PG_CONTEXT_SUBSTITUTION: return NULL;
	default: return evidence->conclusion.subject;
	}
}
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence)
{
	const struct pg_occurrence *subject = pg_evidence_subject(evidence);
	return subject ? subject->classifier : NULL;
}
size_t pg_evidence_premise_count(const struct pg_evidence *evidence) { return evidence->premise_count; }
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index)
{
	return index < evidence->premise_count ? evidence->premises[index] : NULL;
}
