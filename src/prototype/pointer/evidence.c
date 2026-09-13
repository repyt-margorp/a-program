#include "evidence.h"
#include "computation.h"
#include "eval.h"
#include "identity.h"
#include "iadt.h"
#include <stdlib.h>
#include <string.h>

struct pg_evidence {
	struct pg_index_entry index;
	const void *owner;
	enum pg_evidence_rule rule;
	enum pg_evidence_judgement judgement;
	const struct pg_context *context;
	const struct pg_occurrence *subject;
	const struct pg_term *classifier;
	const void *certificate;
	size_t premise_count;
	const struct pg_evidence *premises[];
};

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
	case PG_REINDEX: case PG_APP_ELIM: case PG_PI_CODOMAIN:
	case PG_FOLD_ELIM: case PG_PI_CONSTANT_CODOMAIN: case PG_EFFECT_SUBSUMPTION: case PG_REQUEST_INTRO:
	case PG_FAMILY_IDENTITY_FORM: case PG_FAMILY_ACTION:
	case PG_INDUCTIVE_FORM: case PG_CONSTRUCTOR_INTRO: case PG_MATCH_ELIM: case PG_INDUCTION_ELIM: case PG_TYPE_CASE:
		return 1;
	default: return 0;
	}
}

static const void *certificate_key(enum pg_evidence_rule rule, const void *certificate)
{
	/* Allocation records preserve the first construction of this derivation;
	 * explicit reconstruction checks conflicts before returning that proof. */
	if (rule == PG_INDUCTION_ELIM) return NULL;
	/* Schema wrappers attest premises; they do not introduce another family. */
	return rule == PG_INDUCTIVE_FORM ? pg_data_schema_declaration(certificate) : certificate;
}

static const struct pg_evidence *find_record(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const void *certificate, uint64_t *hash_out)
{
	/* For these rules, immutable premises determine the output. Check this key
	 * before substitution or independence checks allocate temporary binders. */
	if (derived_output(rule)) { subject = NULL; classifier = NULL; }
	uint64_t hash = ((uintptr_t)context ^ (uintptr_t)subject ^ (uintptr_t)classifier ^ rule) * UINT64_C(1099511628211);
	const void *key = certificate_key(rule, certificate);
	hash = (hash ^ (uintptr_t)key ^ judgement) * UINT64_C(1099511628211);
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)premises[i]) * UINT64_C(1099511628211);
	*hash_out = hash;
	for (struct pg_index_entry *candidate = pg_index_candidates(&typing->proofs, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		const struct pg_evidence *proof = (const struct pg_evidence *)candidate;
		if (proof->rule != rule) continue;
		if (proof->judgement != judgement) continue;
		if (proof->context != context) continue;
		if (!derived_output(rule)) {
			if (proof->subject != subject) continue;
			if (proof->classifier != classifier) continue;
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
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const void *certificate,
	size_t binding_count, const struct pg_binding_value *bindings)
{
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, rule, judgement, context,
		subject, classifier, count, premises, certificate, &hash);
	if (existing) return existing;
	if (count > (SIZE_MAX - sizeof(struct pg_evidence)) / sizeof(*premises)) return NULL;
	size_t size = sizeof(struct pg_evidence) + count * sizeof(*premises);
	if (binding_count > (SIZE_MAX - size) / sizeof(*bindings)) return NULL;
	struct pg_evidence *proof = pg_alloc(typing->graph, size + binding_count * sizeof(*bindings));
	if (!proof) return NULL;
	proof->owner = typing->owner_key;
	proof->rule = rule;
	proof->judgement = judgement;
	proof->context = context;
	proof->subject = subject;
	proof->classifier = classifier;
	proof->certificate = certificate;
	proof->premise_count = count;
	for (size_t i = 0; i < count; ++i) proof->premises[i] = premises[i];
	struct pg_binding_value *mapping = (struct pg_binding_value *)(proof->premises + count);
	for (size_t i = 0; i < binding_count; ++i) mapping[i] = bindings[i];
	if (pg_index_insert(&typing->proofs, &proof->index, hash) != 0) return NULL;
	return proof;
}

static const struct pg_evidence *accept_with_conversion(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises,
	const struct pg_conversion_certificate *conversion)
{
	return accept_record(typing, rule, judgement, context, subject, classifier, count, premises, conversion, 0, NULL);
}

static const struct pg_evidence *accept(struct pg_typing *typing, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement,
	const struct pg_context *context, const struct pg_occurrence *subject,
	const struct pg_term *classifier, size_t count, const struct pg_evidence *const *premises)
{
	return accept_with_conversion(typing, rule, judgement, context, subject, classifier, count, premises, NULL);
}

static int context_proof(const struct pg_typing *typing, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(proof, typing)) return 0;
	return proof->judgement == PG_JUDGEMENT_CONTEXT;
}

static const struct pg_term *family_signature(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_evidence *indices,
	const struct pg_term *universe);

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
	return evidence && evidence->rule == PG_INDUCTION_ELIM ? evidence->certificate : NULL;
}

const struct pg_evidence *pg_prove_inductive_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema)
{
	const struct pg_evidence *self = pg_data_schema_parameters(schema);
	if (!context_proof(typing, self)) return NULL;
	int indexed = self->rule == PG_CONTEXT_FAMILY_EXTEND;
	if (!indexed && self->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	const struct pg_evidence *indices = pg_data_schema_indices(schema);
	if (!context_proof(typing, indices)) return NULL;
	uint64_t level, bound;
	const struct pg_term *universe = indexed ? self->premises[2]->subject->core : self->context->declared_type;
	if (!pg_universe_level(universe, &level)) return NULL;
	if (indexed) {
		const struct pg_term *signature = family_signature(typing, self, indices, universe);
		if (!signature || pg_alpha_equal(signature, self->context->declared_type) != 1) return NULL;
	} else if (indices->context != self->context) return NULL;
	const struct pg_evidence *parent = self->premises[0];
	size_t count = pg_data_constructor_count(schema), parameters;
	size_t prefix = indexed ? 2 : 1;
	if (count > SIZE_MAX / sizeof(void *) - prefix) return NULL;
	if (pg_context_extension_size(parent->context, NULL, &parameters)) return NULL;
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
	result = find_record(typing, PG_INDUCTIVE_FORM, kind,
		parent->context, NULL, NULL, count + prefix, premises, schema, &hash);
	if (result) goto done;
	if (pg_data_schema_positive(schema, self->context->binder) != 1) goto done;
	if (pg_data_schema_field_level(schema, &bound) || bound > level) goto done;
	if (parameters > SIZE_MAX / sizeof(const struct pg_object *)) goto done;
	const struct pg_object **binders = pg_alloc(&temporary, parameters * sizeof(*binders));
	if (parameters && !binders) goto done;
	const struct pg_context *context = parent->context;
	for (size_t i = parameters; i; --i, context = context->parent) binders[i - 1] = context->binder;
	const struct pg_term *core = pg_reference(typing->graph, pg_data_family_object(schema));
	for (size_t i = 0; i < parameters; ++i)
		core = pg_application(typing->graph, core, pg_reference(typing->graph, binders[i]));
	if (!core || !universe) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, parent->context, core, NULL, 0, NULL);
	if (!subject) goto done;
	result = accept_record(typing, PG_INDUCTIVE_FORM, kind,
		parent->context, subject, self->context->declared_type, count + prefix, premises, schema, 0, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

struct evidence_frame {
	const struct pg_evidence *proof;
	struct evidence_frame *next;
};

static const struct pg_evidence *return_value_origin(struct pg_typing *typing,
	const struct pg_evidence *computation);

/* Recover an image from retained origins in a smaller context. Reconstruct
 * ordinary rules, including type/value views and substitution images. The
 * judgement kind is retained along with the subject and classifier. */
static const struct pg_evidence *rebase_image(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *image)
{
	struct image_frame {
		const struct pg_evidence *source;
		const struct pg_term *core, *classifier;
		const struct pg_evidence **inputs, **outputs;
		enum pg_evidence_judgement judgement;
		size_t parameters, count, next;
		struct image_frame *parent;
	};
	struct pg_graph temporary = {0};
	struct image_frame *frame = NULL;
	const struct pg_evidence *result = NULL, *source = NULL, *map = NULL;
	const struct pg_term *core = image->subject->core, *classifier = image->classifier;
	enum pg_evidence_judgement judgement = image->judgement;
start:
	if (core->kind == PG_REFERENCE && core->as.reference->kind == PG_BINDER) {
		const struct pg_evidence *variable = pg_prove_variable(typing, context, core->as.reference);
		if (variable && variable->judgement == judgement && pg_alpha_equal(variable->classifier, classifier) == 1) {
			result = variable; goto resolved;
		}
	}
	while (image) {
		result = pg_prove_projection(typing, context, image);
		if (result && result->judgement == judgement && pg_alpha_equal(result->subject->core, core) == 1 &&
			pg_alpha_equal(result->classifier, classifier) == 1) goto resolved;
		result = NULL;
		if (image->rule == PG_CONTEXT_PROJECTION) image = image->premises[1];
		else if (image->rule == PG_TYPE_CONVERSION) image = image->premises[0];
		else if (image->rule == PG_RETURN_VALUE) image = return_value_origin(typing, image->premises[0]);
		else if (image->rule == PG_CONSTRUCTOR_INTRO || image->rule == PG_VALUE_FROM_TYPE ||
			image->rule == PG_TYPE_FROM_VALUE || image->rule == PG_PURE_NORMALIZATION) {
			source = image; map = NULL; goto rebuild;
		}
		else if (image->rule == PG_REINDEX) {
			map = image->premises[0];
			source = image->premises[1];
			if (source->rule == PG_VARIABLE)
				image = pg_substitution_image(typing, map, source->subject->core->as.reference);
			else if (source->rule == PG_REINDEX)
				image = pg_prove_reindex(typing,
					pg_prove_substitution_compose(typing, source->premises[0], map), source->premises[1]);
			else if (source->rule == PG_CONTEXT_PROJECTION) {
				const struct pg_evidence *prefix = map->premises[0];
				size_t count = map->premise_count - 2;
				while (prefix->context != source->premises[1]->context) {
					if (!count || !prefix->context) goto done;
					prefix = prefix->premises[0];
					--count;
				}
				image = pg_prove_reindex(typing, pg_prove_substitution(typing, prefix,
					map->premises[1], count, map->premises + 2), source->premises[1]);
			} else if (source->rule == PG_TYPE_CONVERSION)
				image = pg_prove_reindex(typing, map, source->premises[0]);
			else if (source->rule == PG_RETURN_VALUE)
				image = pg_prove_reindex(typing, map, return_value_origin(typing, source->premises[0]));
			else if (source->rule == PG_CONSTRUCTOR_INTRO) goto rebuild;
			else { source = image; map = NULL; goto rebuild; }
		}
		else goto done;
	}
	goto done;
rebuild: {
	size_t parameters = 0, count = source->premise_count;
	if (source->rule == PG_CONSTRUCTOR_INTRO) {
		parameters = source->premises[2]->premise_count - 2;
		count = source->premises[3]->premise_count - 3;
	} else if (source->rule == PG_REINDEX) count = source->premises[0]->premise_count - 2;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto done;
	struct image_frame *next = pg_alloc(&temporary, sizeof(*next));
	if (!next) goto done;
	*next = (struct image_frame){.source = source, .core = core, .classifier = classifier,
		.judgement = judgement, .parameters = parameters, .count = count, .parent = frame};
	next->inputs = pg_alloc(&temporary, count * sizeof(*next->inputs));
	next->outputs = pg_alloc(&temporary, count * sizeof(*next->outputs));
	if (count && (!next->inputs || !next->outputs)) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *input;
		if (source->rule == PG_CONSTRUCTOR_INTRO)
			input = i < parameters ? source->premises[2]->premises[i + 2] : source->premises[3]->premises[i + 3];
		else if (source->rule == PG_REINDEX) input = source->premises[0]->premises[i + 2];
		else input = source->premises[i];
		next->inputs[i] = map ? pg_prove_reindex(typing, map, input) : input;
		if (!next->inputs[i]) goto done;
	}
	frame = next;
}
children:
	if (frame->next < frame->count) {
		image = frame->inputs[frame->next];
		core = image->subject->core;
		classifier = image->classifier;
		judgement = image->judgement;
		goto start;
	}
	switch (frame->source->rule) {
	case PG_CONSTRUCTOR_INTRO:
		map = pg_prove_substitution(typing, frame->source->premises[2]->premises[0],
			context, frame->parameters, frame->outputs);
		result = pg_prove_constructor(typing, frame->source->premises[1],
			pg_evidence_constructor(frame->source), map, frame->count - frame->parameters,
			frame->count > frame->parameters ? frame->outputs + frame->parameters : NULL);
		break;
	case PG_REINDEX:
		map = pg_prove_substitution(typing, frame->source->premises[0]->premises[0],
			context, frame->count, frame->outputs);
		result = pg_prove_reindex(typing, map, frame->source->premises[1]);
		break;
	case PG_VALUE_FROM_TYPE: result = pg_prove_type_value(typing, frame->outputs[0]); break;
	case PG_TYPE_FROM_VALUE: result = pg_prove_value_type(typing, frame->outputs[0]); break;
	case PG_PURE_NORMALIZATION:
		result = pg_prove_normalization(typing, frame->outputs[0], pg_evidence_normalization(frame->source));
		break;
	default: goto done;
	}
	core = frame->core;
	classifier = frame->classifier;
	judgement = frame->judgement;
	frame = frame->parent;
	if (!result) goto done;
	if (result->judgement != judgement || pg_alpha_equal(result->subject->core, core) != 1 ||
		pg_alpha_equal(result->classifier, classifier) != 1) {
		result = NULL;
		goto done;
	}
resolved:
	if (frame) {
		frame->outputs[frame->next++] = result;
		result = NULL;
		goto children;
	}
done:
	pg_graph_destroy(&temporary);
	return result;
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

static const struct pg_evidence *evidence_map_step(struct pg_typing *typing,
	const struct pg_evidence *map, const struct pg_evidence *step)
{
	if (step->rule == PG_PI_CONSTANT_CODOMAIN)
		return pg_prove_substitution_rebase(typing, step->premises[0]->premises[0]->premises[0], map);
	const struct pg_evidence *substitution = step->premises[0];
	if (step->rule == PG_CONTEXT_PROJECTION)
		substitution = pg_prove_substitution_projection(typing, map->premises[1], substitution);
	return pg_prove_substitution_compose(typing, map, substitution);
}

static const struct pg_evidence *evidence_map(struct pg_typing *typing,
	const struct pg_evidence *context, const struct evidence_frame *frames)
{
	const struct pg_evidence *map = pg_prove_substitution_projection(typing, context, context);
	for (; map && frames; frames = frames->next) map = evidence_map_step(typing, map, frames->proof);
	return map;
}

static const struct pg_evidence *evidence_image(struct pg_typing *typing,
	const struct pg_evidence *value, const struct evidence_frame *frames)
{
	for (; value && frames; frames = frames->next) {
		const struct pg_evidence *step = frames->proof;
		if (step->rule == PG_PI_CONSTANT_CODOMAIN)
			value = rebase_image(typing, step->premises[0]->premises[0]->premises[0], value);
		else if (step->rule == PG_CONTEXT_PROJECTION)
			value = pg_prove_projection(typing, step->premises[0], value);
		else value = pg_prove_reindex(typing, step->premises[0], value);
	}
	return value;
}

static const struct pg_evidence *variable_frame(struct pg_typing *typing,
	const struct pg_evidence *variable, struct evidence_frame **frames)
{
	if (!*frames) return NULL;
	const struct pg_evidence *step = (*frames)->proof;
	*frames = (*frames)->next;
	const struct pg_object *binder = variable->subject->core->as.reference;
	if (step->rule == PG_CONTEXT_PROJECTION)
		return pg_prove_variable(typing, step->premises[0], binder);
	return pg_substitution_image(typing, step->premises[0], binder);
}

/* Recover a returned value's typed origin, preserving each surrounding map.
 * This builds ordinary beta/substitution evidence, not a new reduction rule. */
static const struct pg_evidence *return_value_origin(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	struct pg_graph temporary = {0};
	struct evidence_frame *frames = NULL;
	const struct pg_evidence *result = NULL;
	while (computation) {
		switch (computation->rule) {
		case PG_RETURN_INTRO:
			result = evidence_image(typing, computation->premises[0], frames);
			goto done;
		case PG_PURE_NORMALIZATION: case PG_TYPE_CONVERSION:
			computation = computation->premises[0];
			break;
		case PG_APP_ELIM:
			computation = pg_prove_application_body(typing, computation->premises[0], computation->premises[1]);
			break;
		case PG_REINDEX: case PG_CONTEXT_PROJECTION: {
			struct evidence_frame *frame = pg_alloc(&temporary, sizeof(*frame));
			if (!frame) goto done;
			*frame = (struct evidence_frame){computation, frames};
			frames = frame;
			computation = computation->premises[1];
			break;
		}
		default: goto done;
		}
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_application_body(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(function, typing)) return NULL;
	if (function->judgement != PG_JUDGEMENT_COMPUTATION && function->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE && argument->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (function->context != argument->context) return NULL;
	struct pg_graph temporary = {0};
	struct evidence_frame *frames = NULL;
	const struct pg_evidence *result = NULL;
	struct pending_application {
		const struct pg_evidence *argument;
		struct evidence_frame *frames;
		size_t forces;
		struct pending_application *next;
	};
	struct pending_application *pending = NULL;
	size_t forces = 0;
	for (;;) {
		switch (function->rule) {
		case PG_LAMBDA_INTRO: case PG_TYPE_FAMILY_ABSTRACT: {
			if (forces) goto done;
			const struct pg_evidence *extended = function->rule == PG_LAMBDA_INTRO
				? function->premises[0]->premises[0] : function->premises[0];
			const struct pg_evidence *map = evidence_map(typing, extended->premises[0], frames);
			map = pg_prove_substitution_pair(typing, map, extended, argument);
			function = pg_prove_reindex(typing, map, function->premises[1]);
			if (!function) goto done;
			if (!pending) { result = function; goto done; }
			argument = pending->argument;
			frames = pending->frames;
			forces = pending->forces;
			pending = pending->next;
			break;
		}
		case PG_APP_ELIM: case PG_TYPE_FAMILY_APP: {
			struct pending_application *next = pg_alloc(&temporary, sizeof(*next));
			if (!next) goto done;
			*next = (struct pending_application){argument, frames, forces, pending};
			pending = next;
			argument = function->premises[1];
			function = function->premises[0];
			frames = NULL;
			forces = 0;
			break;
		}
		case PG_REINDEX: case PG_CONTEXT_PROJECTION: {
			struct evidence_frame *frame = pg_alloc(&temporary, sizeof(*frame));
			if (!frame) goto done;
			*frame = (struct evidence_frame){function, frames};
			frames = frame;
			function = function->premises[1];
			break;
		}
		case PG_PURE_NORMALIZATION: case PG_TYPE_CONVERSION:
			function = function->premises[0]; break;
		case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION:
			++forces; function = function->premises[0]; break;
		case PG_THUNK_INTRO:
			if (!forces) goto done;
			--forces; function = function->premises[0]; break;
		case PG_VARIABLE:
			function = variable_frame(typing, function, &frames);
			if (!function) goto done;
			break;
		default: goto done;
		}
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

/* Recover either component from retained Pi formation. A NULL argument in
 * codomain mode requires independence; domain recovery needs no argument.
 * Frames preserve substitution order on curried proof spines. */
static const struct pg_evidence *pi_component(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument,
	enum pg_evidence_rule component)
{
	struct pending {
		const struct pg_evidence *argument;
		struct evidence_frame *frames;
		size_t thunks;
		enum pg_evidence_rule component;
		struct pending *next;
	};
	struct pg_graph temporary = {0};
	struct pending *pending = NULL;
	struct evidence_frame *frames = NULL;
	const struct pg_evidence *result = NULL;
	size_t thunks = 0;
	while (pi) {
		switch (pi->rule) {
		case PG_REINDEX: case PG_CONTEXT_PROJECTION: {
			struct evidence_frame *frame = pg_alloc(&temporary, sizeof(*frame));
			if (!frame) goto done;
			*frame = (struct evidence_frame){pi, frames};
			frames = frame;
			pi = pi->premises[1];
			break;
		}
		case PG_TYPE_CONVERSION: case PG_PURE_NORMALIZATION:
			pi = pi->premises[0]; break;
		case PG_THUNK_CONTENT:
			++thunks; pi = pi->premises[0]; break;
		case PG_THUNK_TYPE_FORM:
			if (!thunks) goto done;
			--thunks; pi = pi->premises[0]; break;
		case PG_PI_DOMAIN: case PG_PI_CODOMAIN: {
			struct pending *next = pg_alloc(&temporary, sizeof(*next));
			if (!next) goto done;
			*next = (struct pending){argument, frames, thunks, component, pending};
			pending = next;
			frames = NULL;
			thunks = 0;
			argument = pi->rule == PG_PI_CODOMAIN ? pi->premises[1] : NULL;
			component = pi->rule;
			pi = pi->premises[0];
			break;
		}
		case PG_PI_FORM: {
			if (thunks) goto done;
			const struct pg_evidence *extended = pi->premises[0];
			const struct pg_evidence *map = evidence_map(typing, extended->premises[0], frames);
			if (component == PG_PI_DOMAIN) {
				pi = pg_prove_reindex(typing, map, extended->premises[1]);
			} else if (argument) {
				map = pg_prove_substitution_pair(typing, map, extended, argument);
				pi = pg_prove_reindex(typing, map, pi->premises[1]);
			} else pi = pg_prove_reindex(typing, map, pg_prove_pi_constant_codomain(typing, pi));
			if (!pending) { result = pi; goto done; }
			argument = pending->argument;
			frames = pending->frames;
			thunks = pending->thunks;
			component = pending->component;
			pending = pending->next;
			break;
		}
		default: goto done;
		}
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

struct inductive_argument {
	const struct pg_evidence *value;
	const struct evidence_frame *frames;
	struct inductive_argument *next;
};

struct inductive_fold {
	const struct pg_evidence *continuation;
	struct evidence_frame *frames;
	struct inductive_argument *arguments;
	size_t return_contents, return_values, thunk_contents, thunk_values;
	struct inductive_fold *parent;
};

int pg_inductive_recovery_init(struct pg_inductive_recovery *work,
	struct pg_typing *typing, const struct pg_evidence *type)
{
	*work = (struct pg_inductive_recovery){.typing = typing, .type = type, .formation = type, .status = -1};
	if (!pg_evidence_owned_by(type, typing)) return -1;
	if (type->judgement != PG_JUDGEMENT_VALUE_TYPE && type->judgement != PG_JUDGEMENT_TYPE_FAMILY) return -1;
	work->status = 0;
	return 0;
}

static void inductive_recovery_step(struct pg_inductive_recovery *work)
{
	struct pg_typing *typing = work->typing;
	const struct pg_evidence *formation = work->formation;
	if (formation->rule != PG_INDUCTIVE_FORM) {
		switch (formation->rule) {
		case PG_REINDEX: case PG_CONTEXT_PROJECTION: {
			struct evidence_frame *frame = pg_alloc(&work->temporary, sizeof(*frame));
			if (!frame) goto failed;
			*frame = (struct evidence_frame){formation, work->frames};
			work->frames = frame;
			formation = formation->premises[1];
			break;
		}
		case PG_TYPE_FROM_VALUE: case PG_VALUE_FROM_TYPE: case PG_TYPE_CONVERSION: case PG_PURE_NORMALIZATION:
			formation = formation->premises[0];
			break;
		case PG_VARIABLE:
			formation = variable_frame(typing, formation, &work->frames);
			if (!formation) goto failed;
			break;
		case PG_RETURN_VALUE:
			++work->return_values; formation = formation->premises[0]; break;
		case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION:
			++work->thunk_values; formation = formation->premises[0]; break;
		case PG_THUNK_INTRO:
			if (!work->thunk_values) goto failed;
			--work->thunk_values; formation = formation->premises[0]; break;
		case PG_RETURN_INTRO:
			if (!work->return_values) goto failed;
			if (work->return_values == 1 && work->folds) {
				struct inductive_fold *fold = work->folds;
				const struct pg_evidence *value = evidence_image(typing, formation->premises[0], work->frames);
				formation = pg_prove_application_body(typing, fold->continuation, value);
				if (!formation) goto failed;
				work->frames = fold->frames;
				work->arguments = fold->arguments;
				work->return_contents = fold->return_contents;
				work->return_values = fold->return_values;
				work->thunk_contents = fold->thunk_contents;
				work->thunk_values = fold->thunk_values;
				work->folds = fold->parent;
				break;
			}
			--work->return_values; formation = formation->premises[0]; break;
		case PG_FOLD_ELIM: {
			struct inductive_fold *fold = pg_alloc(&work->temporary, sizeof(*fold));
			if (!fold) goto failed;
			*fold = (struct inductive_fold){formation->premises[1], work->frames, work->arguments,
				work->return_contents, work->return_values, work->thunk_contents, work->thunk_values, work->folds};
			work->folds = fold;
			work->frames = NULL;
			work->arguments = NULL;
			work->return_contents = work->thunk_contents = work->thunk_values = 0;
			work->return_values = 1;
			formation = formation->premises[0];
			break;
		}
		case PG_TYPE_FAMILY_APP: {
			const struct pg_evidence *body = pg_prove_application_body(typing,
				formation->premises[0], formation->premises[1]);
			if (body) { formation = body; break; }
			struct inductive_argument *argument = pg_alloc(&work->temporary, sizeof(*argument));
			if (!argument) goto failed;
			*argument = (struct inductive_argument){formation->premises[1], work->frames, work->arguments};
			work->arguments = argument;
			formation = formation->premises[0];
			break;
		}
		case PG_APP_ELIM:
			formation = pg_prove_application_body(typing, formation->premises[0], formation->premises[1]);
			if (!formation) goto failed;
			break;
		case PG_RETURN_CONTENT:
			++work->return_contents;
			formation = formation->premises[0];
			break;
		case PG_RETURN_TYPE_FORM:
			if (!work->return_contents) goto failed;
			--work->return_contents;
			formation = formation->premises[0];
			break;
		case PG_THUNK_CONTENT:
			++work->thunk_contents;
			formation = formation->premises[0];
			break;
		case PG_THUNK_TYPE_FORM:
			if (!work->thunk_contents) goto failed;
			--work->thunk_contents;
			formation = formation->premises[0];
			break;
		case PG_PI_DOMAIN: case PG_PI_CODOMAIN: {
			formation = pi_component(typing, formation->premises[0],
				formation->rule == PG_PI_DOMAIN ? NULL : formation->premises[1], formation->rule);
			if (!formation) goto failed;
			break;
		}
		case PG_PI_CONSTANT_CODOMAIN: {
			const struct pg_evidence *pi = formation->premises[0];
			if (pi->rule != PG_PI_FORM) {
				formation = pi_component(typing, pi, NULL, PG_PI_CODOMAIN);
				if (!formation) goto failed;
				break;
			}
			struct evidence_frame *frame = pg_alloc(&work->temporary, sizeof(*frame));
			if (!frame) goto failed;
			*frame = (struct evidence_frame){formation, work->frames};
			work->frames = frame;
			formation = pi->premises[1];
			break;
		}
		default: goto failed;
		}
		work->formation = formation;
		return;
	}
	if (work->return_contents || work->return_values || work->thunk_contents || work->thunk_values || work->folds) goto failed;
	if (!work->map) {
		const struct pg_evidence *context = formation->premises[0]->premises[0];
		work->map = pg_prove_substitution_projection(typing, context, context);
		if (!work->map) goto failed;
		return;
	}
	if (work->frames) {
		work->map = evidence_map_step(typing, work->map, work->frames->proof);
		work->frames = work->frames->next;
		if (!work->map) goto failed;
		return;
	}
	if (work->map->context != work->type->context) goto failed;
	const struct pg_evidence *instance = pg_prove_reindex(typing, work->map, formation);
	const struct pg_evidence *indices = NULL;
	if (work->arguments) {
		if (!instance || instance->judgement != PG_JUDGEMENT_TYPE_FAMILY) goto failed;
		const struct pg_data_schema *schema = formation->certificate;
		const struct pg_evidence *self = formation->premises[0];
		const struct pg_evidence *prefix = pg_prove_substitution_pair(typing, work->map, self, instance);
		if (!prefix) goto failed;
		size_t count = 0;
		for (struct inductive_argument *a = work->arguments; a; a = a->next) ++count;
		if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) goto failed;
		const struct pg_evidence **values = pg_alloc(&work->temporary, count * sizeof(*values));
		if (!values) goto failed;
		size_t i = 0;
		/* Index arguments cross precisely the wrappers outside their own
		 * application, not the parameter substitutions inside its callee. */
		for (struct inductive_argument *a = work->arguments; a; a = a->next) {
			const struct pg_evidence *value = evidence_image(typing, a->value, a->frames);
			if (!value) goto failed;
			values[i++] = value;
			instance = pg_prove_family_application(typing, instance, value);
			if (!instance) goto failed;
		}
		const struct pg_data_signature *signature = pg_data_signature(typing, self, pg_data_schema_indices(schema));
		indices = pg_data_signature_instance(typing, signature, prefix, count, values);
		if (!indices || instance->judgement != PG_JUDGEMENT_VALUE_TYPE) goto failed;
	}
	if (!instance || pg_alpha_equal(instance->subject->core, work->type->subject->core) != 1) goto failed;
	work->result = (struct pg_inductive_instance){formation->certificate, formation, work->map, indices};
	work->status = 1;
	return;
failed:
	work->status = -1;
}

int pg_inductive_recovery_advance(struct pg_inductive_recovery *work, size_t steps)
{
	while (!work->status && steps--) inductive_recovery_step(work);
	return work->status;
}

void pg_inductive_recovery_destroy(struct pg_inductive_recovery *work)
{
	pg_graph_destroy(&work->temporary);
	*work = (struct pg_inductive_recovery){0};
}

int pg_inductive_instance(struct pg_typing *typing, const struct pg_evidence *type,
	struct pg_inductive_instance *output)
{
	if (!output) return 0;
	struct pg_inductive_recovery work;
	pg_inductive_recovery_init(&work, typing, type);
	while (!pg_inductive_recovery_advance(&work, 1024)) {}
	int success = work.status > 0;
	if (success) *output = work.result;
	pg_inductive_recovery_destroy(&work);
	return success;
}

const struct pg_evidence *pg_prove_constructor(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_object *constructor,
	const struct pg_evidence *parameters, size_t count,
	const struct pg_evidence *const *fields)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (parameters->premises[0]->context != formation->context) return NULL;
	const struct pg_data_schema *schema = formation->certificate;
	if (!pg_data_schema_fields(schema, constructor)) return NULL;
	const struct pg_evidence *family = pg_prove_reindex(typing, parameters, formation);
	const struct pg_evidence *self = formation->judgement == PG_JUDGEMENT_TYPE_FAMILY
		? family : pg_prove_type_value(typing, family);
	if (!self) return NULL;
	const struct pg_evidence *extended = pg_prove_substitution_extend(typing, parameters,
		formation->premises[0], 1, &self);
	const struct pg_evidence *instance = pg_data_instance(typing, schema, constructor, extended, count, fields);
	if (!instance) return NULL;
	if (formation->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
		const struct pg_evidence *result_map = pg_data_result(typing, schema, constructor, instance);
		if (!result_map) return NULL;
		for (size_t i = extended->premise_count; i < result_map->premise_count; ++i) {
			family = pg_prove_family_application(typing, family, result_map->premises[i]);
			if (!family) return NULL;
		}
		if (family->judgement != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	}
	const struct pg_evidence *premises[] = {family, formation, parameters, instance};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_CONSTRUCTOR_INTRO,
		PG_JUDGEMENT_VALUE, instance->context, NULL, NULL, 4, premises, constructor, &hash);
	if (existing) return existing;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_occurrence *)) return NULL;
	const struct pg_occurrence **operands = pg_alloc(&temporary, count * sizeof(*operands));
	if (count && !operands) goto done;
	const struct pg_term *core = pg_reference(typing->graph, constructor);
	for (size_t i = 0; i < count; ++i) {
		operands[i] = fields[i]->subject;
		core = pg_application(typing->graph, core, operands[i]->core);
	}
	if (!core) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, instance->context, core, NULL, count, operands);
	if (!subject) goto done;
	result = accept_record(typing, PG_CONSTRUCTOR_INTRO, PG_JUDGEMENT_VALUE,
		instance->context, subject, family->subject->core, 4, premises, constructor, 0, NULL);
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
	if (pg_context_extension_size(fields->context, map->premises[0]->context, &count)) return NULL;
	if (retained) {
		size_t supplied;
		if (pg_context_extension_size(allocation, map->premises[1]->context, &supplied) || supplied != count) return NULL;
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
	if (parameters->premises[0]->context != formation->context) return NULL;
	const struct pg_evidence *family = pg_prove_reindex(typing, parameters, formation);
	const struct pg_evidence *self = formation->judgement == PG_JUDGEMENT_TYPE_FAMILY
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
	const struct pg_evidence *family = pg_substitution_image(typing, map, formation->premises[0]->context->binder);
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
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (formation->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	const struct pg_evidence *map = prove_data_scope(typing, formation,
		pg_data_schema_indices(formation->certificate), parameters, NULL, 0);
	if (!map) return NULL;
	const struct pg_evidence *body = pg_prove_return_contract(typing, classifiers, PG_TOTALITY_TOTAL,
		pg_prove_type_value(typing, family_in_scope(typing, formation, parameters, map)));
	return pg_prove_abstract(typing, classifiers, parameters->premises[1], map->premises[1], body);
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
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters)
{
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	const struct pg_evidence *map = pg_prove_constructor_scope(typing, formation, constructor, parameters);
	if (!map) return NULL;
	const struct pg_evidence *body = constructor_in_scope(typing, formation, constructor, parameters, map);
	body = pg_prove_return_contract(typing, classifiers, PG_TOTALITY_TOTAL, body);
	if (!body) return NULL;
	return pg_prove_abstract(typing, classifiers, parameters->premises[1], map->premises[1], body);
}

/* Check Gamma, indices, z : Family(parameters, indices), not a fixed fiber.
 * The ordinary family application rule checks each dependent index domain. */
int pg_inductive_motive_context_valid(struct pg_typing *typing,
	const struct pg_evidence *formation, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return 0;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return 0;
	if (parameters->premises[0]->context != formation->context) return 0;
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return 0;
	const struct pg_data_schema *schema = formation->certificate;
	size_t count;
	if (pg_context_extension_size(pg_data_schema_indices(schema)->context,
		formation->premises[0]->context, &count)) return 0;
	const struct pg_evidence *indices = motive_context->premises[0], *prefix = indices;
	struct pg_graph temporary = {0};
	int valid = 0;
	if (count > SIZE_MAX / sizeof(const struct pg_object *)) return 0;
	const struct pg_object **binders = pg_alloc(&temporary, count * sizeof(*binders));
	if (count && !binders) goto done;
	for (size_t i = count; i; --i, prefix = prefix->premises[0]) {
		if (prefix->rule != PG_CONTEXT_EXTEND) goto done;
		binders[i - 1] = prefix->context->binder;
	}
	if (prefix->context != parameters->context) goto done;
	const struct pg_evidence *family = pg_prove_projection(typing, indices,
		pg_prove_reindex(typing, parameters, formation));
	for (size_t i = 0; family && i < count; ++i)
		family = pg_prove_family_application(typing, family, pg_prove_variable(typing, indices, binders[i]));
	valid = family && pg_alpha_equal(family->subject->core, motive_context->context->declared_type) == 1;
done:
	pg_graph_destroy(&temporary);
	return valid;
}

/* Pull back along (Gamma projection, actual indices, value). Constructor
 * membership retains its result map; variables retain their fiber formation.
 * This is checked substitution, not a search for an expected result type. */
const struct pg_evidence *pg_prove_inductive_motive_substitution(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *source,
	const struct pg_evidence *destination, const struct pg_evidence *value)
{
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, source)) return NULL;
	const struct pg_evidence *prefix = pg_prove_substitution_projection(typing, parameters->premises[1], destination);
	if (formation->judgement == PG_JUDGEMENT_TYPE_FAMILY) {
		const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, destination, value);
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

const struct pg_evidence *pg_prove_inductive_motive_at(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters,
	const struct pg_evidence *source, const struct pg_evidence *motive,
	const struct pg_evidence *destination, const struct pg_evidence *value)
{
	if (!context_proof(typing, source)) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || motive->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (motive->context != source->context) return NULL;
	const struct pg_evidence *map = pg_prove_inductive_motive_substitution(typing,
		classifiers, formation, parameters, source, destination, value);
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
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *motive_context,
	const struct pg_evidence *motive, const struct pg_evidence *context,
	const struct pg_evidence *field)
{
	if (!pg_evidence_owned_by(field, typing) || field->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (!context_proof(typing, context) || field->context != context->context) return NULL;
	const struct pg_term *type;
	if (!pg_thunk_type_view(field->classifier, &type)) {
		const struct pg_evidence *at = pg_prove_inductive_motive_at(typing, classifiers,
			formation, parameters, motive_context, motive, context, field);
		return pg_prove_thunk_type(typing, classifiers, at);
	}
	const struct pg_evidence *scope = context;
	const struct pg_evidence *call = pg_prove_force(typing, field);
	const struct pg_evidence *classifier = pg_prove_classifier(typing, classifiers, scope, call);
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	while (classifier && pg_pi_view(classifier->subject->core, &domain, &binder, &codomain)) {
		binder = pg_binder(typing->graph);
		const struct pg_evidence *argument_type = pg_prove_pi_domain(typing, classifier);
		scope = pg_prove_context_extension(typing, scope, binder, argument_type);
		if (!scope) return NULL;
		call = pg_prove_application(typing, pg_prove_projection(typing, scope, call),
			pg_prove_variable(typing, scope, binder));
		classifier = pg_prove_classifier(typing, classifiers, scope, call);
	}
	enum pg_totality field_totality;
	const struct pg_effect_row *effects;
	if (!classifier || !pg_computation_type_view(classifier->subject->core, &field_totality, &effects, &type)) return NULL;
	if (pg_effect_count(effects)) return NULL;
	const struct pg_evidence *at;
	if (field_totality == PG_TOTALITY_TOTAL) {
		const struct pg_evidence *result = pg_prove_total_pure_value(typing, call, pg_binder(typing->graph));
		at = pg_prove_inductive_motive_at(typing, classifiers,
			formation, parameters, motive_context, motive, scope, result);
		goto abstract;
	}
	const struct pg_evidence *returned_type = pg_prove_return_content(typing, classifier);
	binder = pg_binder(typing->graph);
	const struct pg_evidence *returned = pg_prove_context_extension(typing, scope, binder, returned_type);
	at = pg_prove_inductive_motive_at(typing, classifiers,
		formation, parameters, motive_context, motive, returned, pg_prove_variable(typing, returned, binder));
	if (!at) return NULL;
	/* Calling a recursive function field precedes recursion on its result.
	 * The IH cannot promise termination that this field does not provide. */
	enum pg_totality motive_totality;
	if (pg_computation_type_view(at->subject->core, &motive_totality, &effects, &type)) {
		at = pg_prove_computation_type(typing, classifiers, field_totality, effects,
			pg_prove_return_content(typing, at));
	} else if (!unspecified_computation_result(at->subject->core)) return NULL;
	/* Fold the returned recursive value once. Its result classifier must not
	 * escape with that value's binder; indices may depend on the Pi arguments. */
	at = pg_prove_pi_constant_codomain(typing,
		pg_prove_pi(typing, classifiers, returned, at));
abstract:
	while (at && scope->context != context->context) {
		at = pg_prove_pi(typing, classifiers, scope, at);
		scope = scope->premises[0];
	}
	return pg_prove_thunk_type(typing, classifiers, at);
}

static const struct pg_evidence *prove_induction_scope(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_context *allocation, int retained)
{
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || motive->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (motive->context != motive_context->context) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, motive_context)) return NULL;
	const struct pg_evidence *fields = pg_data_schema_fields(formation->certificate, constructor);
	if (!fields) return NULL;
	const struct pg_object *self = formation->premises[0]->context->binder;
	size_t prefix = parameters->premise_count - 2;
	size_t count;
	if (pg_context_extension_size(fields->context, formation->premises[0]->context, &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_term *)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	unsigned char *recursive_fields = pg_alloc(&temporary, count);
	if (count && !recursive_fields) goto done;
	size_t recursive_count = 0;
	for (size_t i = count; i; --i, fields = fields->premises[0]) {
		int recursive = pg_data_recursive_field(fields->context->declared_type, self);
		if (recursive < 0) goto done;
		recursive_fields[i - 1] = (unsigned char)recursive;
		recursive_count += (size_t)recursive;
	}
	const struct pg_object **ih_binders = NULL;
	if (retained) {
		size_t supplied;
		if (recursive_count > SIZE_MAX - count ||
			pg_context_extension_size(allocation, parameters->premises[1]->context, &supplied) ||
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
		const struct pg_evidence *ih = pg_prove_inductive_hypothesis_type(typing, classifiers, formation, parameters,
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
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive)
{
	return prove_induction_scope(typing, classifiers, formation, constructor, parameters,
		motive_context, motive, NULL, 0);
}

const struct pg_evidence *pg_prove_induction_scope_at(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_context *allocation)
{
	return prove_induction_scope(typing, classifiers, formation, constructor, parameters,
		motive_context, motive, allocation, 1);
}

const struct pg_evidence *pg_prove_induction_case(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *branch)
{
	const struct pg_evidence *map = pg_prove_induction_scope(typing, classifiers,
		formation, constructor, parameters, motive_context, motive);
	if (!map) return NULL;
	const struct pg_evidence *context = map->premises[1];
	const struct pg_evidence *body = pg_prove_projection(typing, context, branch);
	for (size_t i = parameters->premise_count + 1; body && i < map->premise_count; ++i)
		body = pg_prove_application(typing, body, map->premises[i]);
	return pg_prove_abstract(typing, classifiers, parameters->premises[1], context, body);
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
		types[i - 1] = fields->context->declared_type;
	const struct pg_object *self = formation->premises[0]->context->binder;
	const struct pg_context *ih_context = map->premises[1]->context;
	for (size_t i = count; i; --i) {
		int recursive = pg_data_recursive_field(types[i - 1], self);
		if (recursive < 0) goto done;
		if (!recursive) continue;
		if (!ih_context->parent) goto done;
		ih_types[i - 1] = ih_context->declared_type;
		ih_context = ih_context->parent;
	}
	const struct pg_term *body = branch->subject->core;
	for (size_t i = 0; i < count; ++i)
		body = pg_application(typing->graph, body, map->premises[offset + i]->subject->core);
	for (size_t i = 0; i < count; ++i) {
		if (!ih_types[i]) continue;
		const struct pg_term *call = induction_field_core(typing->graph, types[i], ih_types[i],
			map->premises[offset + i]->subject->core, recursion);
		if (!call) goto done;
		body = pg_application(typing->graph, body, call);
	}
	for (size_t i = count; i; --i) {
		const struct pg_term *field = map->premises[offset + i - 1]->subject->core;
		if (field->kind != PG_REFERENCE || field->as.reference->kind != PG_BINDER) goto done;
		body = pg_lambda(typing->graph, field->as.reference, body);
	}
	result = body;
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_match_branch_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	const struct pg_evidence *fields)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!pg_evidence_owned_by(fields, typing) || fields->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (fields->premise_count <= parameters->premise_count) return NULL;
	const struct pg_evidence *context = fields->premises[1];
	size_t count;
	if (pg_context_extension_size(context->context, parameters->context, &count)) return NULL;
	const struct pg_evidence *value = constructor_in_scope(typing, formation, constructor, parameters, fields);
	const struct pg_evidence *expected = pg_prove_inductive_motive_at(typing, classifiers,
		formation, parameters, motive_context, motive, context, value);
	for (size_t i = 0; expected && i < count; ++i, context = context->premises[0])
		expected = pg_prove_pi(typing, classifiers, context, expected);
	return expected;
}

static const struct pg_evidence *prove_data_elimination(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
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
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (parameters->premises[0]->context != formation->context) return NULL;
	const struct pg_evidence *destination = parameters->premises[1];
	if (allocation) {
		const struct pg_object *binders[] = {allocation->recursion, allocation->argument, allocation->self};
		for (size_t i = 0; i < 3; ++i) {
			if (pg_context_lookup(destination->context, binders[i])) return NULL;
			for (size_t j = 0; j < count; ++j)
				if (pg_context_lookup(allocation->clauses[j], binders[i])) return NULL;
		}
	}
	if (!pg_evidence_owned_by(scrutinee, typing) || scrutinee->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (scrutinee->context != destination->context) return NULL;
	if (!context_proof(typing, motive_context) || motive_context->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!pg_inductive_motive_context_valid(typing, formation, parameters, motive_context)) return NULL;
	if (!pg_evidence_owned_by(motive, typing) || motive->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (motive->context != motive_context->context) return NULL;
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
		if (branches[i]->judgement != PG_JUDGEMENT_COMPUTATION) goto done;
		if (branches[i]->context != destination->context) goto done;
		premises[i + 5] = branches[i];
	}
	const struct pg_evidence *output = pg_prove_inductive_motive_at(typing, classifiers, formation, parameters,
		motive_context, motive, destination, scrutinee);
	if (!output) goto done;
	premises[count + 5] = output;
	uint64_t hash;
	result = find_record(typing, rule, PG_JUDGEMENT_COMPUTATION,
		destination->context, NULL, NULL, count + 6, premises, NULL, &hash);
	if (result) {
		if (allocation) {
			const struct pg_induction_allocation *saved = pg_evidence_induction_allocation(result);
			if (!saved || saved->recursion != allocation->recursion || saved->argument != allocation->argument ||
				saved->self != allocation->self) { result = NULL; goto done; }
			for (size_t i = 0; i < count; ++i)
				if (saved->clauses[i] != allocation->clauses[i]) { result = NULL; break; }
		}
		goto done;
	}
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	struct pg_match_clause *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 1) * sizeof(*operands));
	if ((count && !clauses) || !operands) goto done;
	operands[0] = scrutinee->subject;
	struct pg_induction_allocation *saved = NULL;
	const struct pg_context **contexts = NULL;
	if (rule == PG_INDUCTION_ELIM) {
		saved = pg_alloc(typing->graph, sizeof(*saved));
		contexts = pg_alloc(typing->graph, count * sizeof(*contexts));
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
			? pg_prove_induction_scope_at(typing, classifiers, formation, constructor, parameters,
				motive_context, motive, allocation->clauses[i])
			: rule == PG_INDUCTION_ELIM
			? pg_prove_induction_scope(typing, classifiers, formation, constructor, parameters, motive_context, motive)
			: pg_prove_constructor_scope(typing, formation, constructor, parameters);
		if (!map) goto done;
		const struct pg_evidence *context = map->premises[1];
		if (contexts) contexts[i] = allocation ? allocation->clauses[i] : context->context;
		const struct pg_evidence *expected = pg_prove_match_branch_type(typing, classifiers,
			formation, constructor, parameters, motive_context, motive, map);
		if (!expected || pg_alpha_equal(expected->subject->core, branches[i]->classifier) != 1) goto done;
		clauses[i] = (struct pg_match_clause){constructor, branches[i]->subject->core};
		if (recursion) clauses[i].branch = induction_branch_core(typing, formation, parameters, map, branches[i], recursion);
		if (!clauses[i].branch) goto done;
		operands[i + 1] = branches[i]->subject;
	}
	const struct pg_term *core = recursion
		? pg_data_recursive_match(typing->graph, layout, recursion,
			saved->argument, saved->self, scrutinee->subject->core, count, clauses)
		: pg_data_match(typing->graph, layout, scrutinee->subject->core, count, clauses);
	if (!core) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, destination->context, core, NULL, count + 1, operands);
	if (!subject) goto done;
	result = accept_record(typing, rule, PG_JUDGEMENT_COMPUTATION,
		destination->context, subject, output->subject->core, count + 6, premises, saved, 0, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_elimination_reindex(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *substitution,
	const struct pg_evidence *elimination)
{
	if (!pg_evidence_owned_by(elimination, typing)) return NULL;
	if (elimination->rule != PG_MATCH_ELIM && elimination->rule != PG_INDUCTION_ELIM) return NULL;
	if (!pg_evidence_owned_by(substitution, typing) || substitution->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (substitution->premises[0]->context != elimination->context) return NULL;
	const struct pg_evidence *map = lift_scope(typing, substitution, elimination->premises[4], NULL, 0);
	if (!map) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	size_t count = elimination->premise_count - 6;
	const struct pg_evidence **branches = pg_alloc(&temporary, count * sizeof(*branches));
	if (count && !branches) goto done;
	for (size_t i = 0; i < count; ++i) {
		branches[i] = pg_prove_reindex(typing, substitution, elimination->premises[i + 5]);
		if (!branches[i]) goto done;
	}
	result = prove_data_elimination(typing, classifiers, elimination->premises[1],
		pg_prove_substitution_compose(typing, elimination->premises[2], substitution),
		pg_prove_reindex(typing, substitution, elimination->premises[3]), map->premises[1],
		pg_prove_reindex(typing, map, elimination->premises[0]), count, branches, elimination->rule, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_type_case(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	size_t count, const struct pg_evidence *const *branches)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (parameters->premises[0]->context != formation->context) return NULL;
	if (!pg_evidence_owned_by(scrutinee, typing) || scrutinee->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (scrutinee->context != parameters->context) return NULL;
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
		if (branches[i]->context != parameters->context) goto done;
		premises[i + 3] = branches[i];
	}
	uint64_t hash;
	result = find_record(typing, PG_TYPE_CASE, PG_JUDGEMENT_VALUE_TYPE,
		parameters->context, NULL, NULL, count + 3, premises, NULL, &hash);
	if (result) goto done;
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, parameters->premises[1], scrutinee);
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, type, &instance) || instance.formation != formation) goto done;
	if (instance.parameters->premise_count != parameters->premise_count) goto done;
	for (size_t i = 2; i < parameters->premise_count; ++i)
		if (pg_alpha_equal(instance.parameters->premises[i]->subject->core, parameters->premises[i]->subject->core) != 1) goto done;
	const struct pg_data_layout *layout = pg_data_schema_layout(schema);
	struct pg_match_clause *clauses = pg_alloc(&temporary, count * sizeof(*clauses));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 1) * sizeof(*operands));
	if (!clauses || !operands) goto done;
	operands[0] = scrutinee->subject;
	uint64_t level = 0;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_object *constructor = pg_data_constructor(layout, i);
		const struct pg_evidence *map = pg_prove_constructor_scope(typing, formation, constructor, parameters);
		if (!map) goto done;
		const struct pg_evidence *branch = pg_prove_projection(typing, map->premises[1], branches[i]);
		for (size_t j = parameters->premise_count + 1; branch && j < map->premise_count; ++j)
			branch = pg_prove_family_application(typing, branch, map->premises[j]);
		if (!branch || branch->judgement != PG_JUDGEMENT_VALUE_TYPE) goto done;
		uint64_t bound;
		if (!pg_universe_level(branch->classifier, &bound)) goto done;
		if (bound > level) level = bound;
		clauses[i] = (struct pg_match_clause){constructor, branches[i]->subject->core};
		operands[i + 1] = branches[i]->subject;
	}
	const struct pg_term *core = pg_data_match(typing->graph, layout, scrutinee->subject->core, count, clauses);
	const struct pg_term *universe = pg_universe(classifiers, level);
	if (!core || !universe) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, parameters->context, core, NULL, count + 1, operands);
	if (!subject) goto done;
	result = accept(typing, PG_TYPE_CASE, PG_JUDGEMENT_VALUE_TYPE,
		parameters->context, subject, universe, count + 3, premises);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_match(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches)
{
	return prove_data_elimination(typing, classifiers, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_MATCH_ELIM, NULL);
}

const struct pg_evidence *pg_prove_induction(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches)
{
	return prove_data_elimination(typing, classifiers, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_INDUCTION_ELIM, NULL);
}

const struct pg_evidence *pg_prove_induction_at(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *parameters, const struct pg_evidence *scrutinee,
	const struct pg_evidence *motive_context, const struct pg_evidence *motive,
	size_t count, const struct pg_evidence *const *branches,
	const struct pg_induction_allocation *allocation)
{
	if (!allocation) return NULL;
	return prove_data_elimination(typing, classifiers, formation, parameters,
		scrutinee, motive_context, motive, count, branches, PG_INDUCTION_ELIM, allocation);
}

const struct pg_evidence *pg_prove_empty_context(struct pg_typing *typing)
{
	return accept(typing, PG_CONTEXT_EMPTY, PG_JUDGEMENT_CONTEXT, NULL, NULL, NULL, 0, NULL);
}

const struct pg_evidence *pg_prove_value_type(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (value->judgement == PG_JUDGEMENT_VALUE_TYPE) return value;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	uint64_t level;
	if (!pg_universe_level(value->classifier, &level)) return NULL;
	return accept(typing, PG_TYPE_FROM_VALUE, PG_JUDGEMENT_VALUE_TYPE,
		value->context, value->subject, value->classifier, 1, &value);
}

const struct pg_evidence *pg_prove_type_value(struct pg_typing *typing, const struct pg_evidence *type)
{
	if (!pg_evidence_owned_by(type, typing)) return NULL;
	if (type->judgement != PG_JUDGEMENT_VALUE_TYPE) return NULL;
	return accept(typing, PG_VALUE_FROM_TYPE, PG_JUDGEMENT_VALUE,
		type->context, type->subject, type->classifier, 1, &type);
}

const struct pg_evidence *pg_prove_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *type)
{
	if (!context_proof(typing, parent)) return NULL;
	type = pg_prove_value_type(typing, type);
	if (!type) return NULL;
	if (!type->subject || type->context != parent->context) return NULL;
	uint64_t level;
	if (!pg_universe_level(type->classifier, &level)) return NULL;
	if (pg_context_lookup(parent->context, binder)) return NULL;
	const struct pg_context *context = pg_context_bind(typing, parent->context, binder, type->subject->core);
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, type};
	return accept(typing, PG_CONTEXT_EXTEND, PG_JUDGEMENT_CONTEXT, context, NULL, NULL, 2, premises);
}

const struct pg_evidence *pg_prove_universe(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context, uint64_t level)
{
	if (!context_proof(typing, context)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	if (level == UINT64_MAX) return NULL;
	const struct pg_term *term = pg_universe(classifiers, level);
	const struct pg_term *sort = pg_universe(classifiers, level + 1);
	if (!term || !sort) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context, term, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_UNIVERSE_FORM, PG_JUDGEMENT_VALUE_TYPE, context->context, subject, sort, 1, &context);
}

static enum pg_evidence_judgement binding_judgement(const struct pg_evidence *extension)
{
	return extension->rule == PG_CONTEXT_FAMILY_EXTEND
		? PG_JUDGEMENT_TYPE_FAMILY : PG_JUDGEMENT_VALUE;
}

/* Logical signatures share Pi syntax, including higher family parameters.
 * Each domain's value/family sort comes from its checked context extension. */
static const struct pg_term *family_signature(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_evidence *indices,
	const struct pg_term *universe)
{
	const struct pg_term *signature = universe;
	while (indices->context != parent->context) {
		if (indices->rule != PG_CONTEXT_EXTEND && indices->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
		signature = pg_pi(typing->graph, indices->context->declared_type,
			indices->context->binder, signature);
		if (!signature) return NULL;
		indices = indices->premises[0];
	}
	return signature;
}

const struct pg_evidence *pg_prove_family_context_extension(struct pg_typing *typing,
	const struct pg_evidence *parent, const struct pg_object *binder,
	const struct pg_evidence *indices, const struct pg_evidence *universe)
{
	if (!context_proof(typing, parent) || !context_proof(typing, indices)) return NULL;
	if (!pg_evidence_owned_by(universe, typing)) return NULL;
	if (universe->judgement != PG_JUDGEMENT_VALUE_TYPE || universe->context != indices->context) return NULL;
	uint64_t level;
	if (!pg_universe_level(universe->subject->core, &level)) return NULL;
	size_t count;
	if (pg_context_extension_size(indices->context, parent->context, &count) || !count) return NULL;
	if (pg_context_lookup(parent->context, binder)) return NULL;
	const struct pg_term *signature = family_signature(typing, parent, indices, universe->subject->core);
	if (!signature) return NULL;
	const struct pg_context *context = pg_context_bind(typing, parent->context, binder, signature);
	if (!context) return NULL;
	const struct pg_evidence *premises[] = {parent, indices, universe};
	return accept(typing, PG_CONTEXT_FAMILY_EXTEND, PG_JUDGEMENT_CONTEXT,
		context, NULL, NULL, 3, premises);
}

const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder)
{
	if (!context_proof(typing, context)) return NULL;
	const struct pg_context *declaration = pg_context_lookup(context->context, binder);
	if (!declaration) return NULL;
	const struct pg_term *term = pg_reference(typing->graph, binder);
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context, term, NULL, 0, NULL);
	if (!subject) return NULL;
	const struct pg_evidence *extension = context;
	while (extension->context != declaration) extension = extension->premises[0];
	return accept(typing, PG_VARIABLE, binding_judgement(extension), context->context,
		subject, declaration->declared_type, 1, &context);
}

const struct pg_evidence *pg_prove_family_application(struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_evidence *index)
{
	if (!pg_evidence_owned_by(family, typing) || !pg_evidence_owned_by(index, typing)) return NULL;
	if (family->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (index->judgement != PG_JUDGEMENT_VALUE && index->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (family->context != index->context) return NULL;
	const struct pg_term *domain, *body;
	const struct pg_object *binder;
	if (!pg_pi_view(family->classifier, &domain, &binder, &body)) return NULL;
	if (pg_alpha_equal(domain, index->classifier) != 1) return NULL;
	struct pg_binding_value binding = {binder, index->subject->core};
	const struct pg_term *classifier = pg_substitution_compute(&typing->substitutions, body, 1, &binding);
	if (!classifier) return NULL;
	uint64_t level;
	enum pg_evidence_judgement kind = pg_universe_level(classifier, &level)
		? PG_JUDGEMENT_VALUE_TYPE : PG_JUDGEMENT_TYPE_FAMILY;
	const struct pg_term *core = pg_application(typing->graph, family->subject->core, index->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {family->subject, index->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {family, index};
	return accept(typing, PG_TYPE_FAMILY_APP, kind, family->context, subject, classifier, 2, premises);
}

const struct pg_evidence *pg_prove_family_abstraction(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *body)
{
	if (!context_proof(typing, context)) return NULL;
	if (context->rule != PG_CONTEXT_EXTEND && context->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (!pg_evidence_owned_by(body, typing) || body->context != context->context) return NULL;
	if (body->judgement != PG_JUDGEMENT_VALUE_TYPE && body->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	const struct pg_context *parent = context->context->parent;
	const struct pg_term *signature = pg_pi(typing->graph, context->context->declared_type,
		context->context->binder, body->classifier);
	const struct pg_term *core = pg_lambda(typing->graph, context->context->binder, body->subject->core);
	if (!signature || !core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, parent, core, NULL, 1, &body->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {context, body};
	return accept(typing, PG_TYPE_FAMILY_ABSTRACT, PG_JUDGEMENT_TYPE_FAMILY,
		parent, subject, signature, 2, premises);
}

static const struct pg_evidence *unary_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *argument,
	enum pg_evidence_rule rule, enum pg_totality totality, const struct pg_effect_row *effects)
{
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	enum pg_evidence_judgement output;
	const struct pg_term *term;
	if (rule == PG_RETURN_TYPE_FORM) {
		argument = pg_prove_value_type(typing, argument);
		if (!argument) return NULL;
		output = PG_JUDGEMENT_COMPUTATION_TYPE;
		term = pg_computation_type(classifiers, totality, effects, argument->subject->core);
	} else {
		if (argument->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		output = PG_JUDGEMENT_VALUE_TYPE;
		term = pg_thunk_type(classifiers, argument->subject->core);
	}
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, argument->context,
		term, NULL, 1, &argument->subject);
	if (!subject) return NULL;
	return accept(typing, rule, output, argument->context, subject,
		argument->classifier, 1, &argument);
}

const struct pg_evidence *pg_prove_termination_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *type,
	const struct pg_evidence *suspended)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(type, typing) || !pg_evidence_owned_by(suspended, typing)) return NULL;
	if (type->judgement != PG_JUDGEMENT_VALUE_TYPE || suspended->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (type->context != suspended->context) return NULL;
	const struct pg_term *computation;
	if (!pg_thunk_type_view(type->subject->core, &computation)) return NULL;
	if (pg_alpha_equal(type->subject->core, suspended->classifier) != 1) return NULL;
	const struct pg_term *core = pg_termination_type(classifiers, suspended->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {type->subject, suspended->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, type->context, core, NULL, 2, operands);
	const struct pg_evidence *premises[] = {type, suspended};
	return subject ? accept(typing, PG_TERMINATION_FORM, PG_JUDGEMENT_VALUE_TYPE,
		type->context, subject, type->classifier, 2, premises) : NULL;
}

const struct pg_evidence *pg_prove_termination(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_evidence *suspended)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(formation, typing) || !pg_evidence_owned_by(suspended, typing)) return NULL;
	if (formation->judgement != PG_JUDGEMENT_VALUE_TYPE || suspended->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (formation->context != suspended->context) return NULL;
	const struct pg_term *expected, *computation, *value;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_termination_type_view(formation->subject->core, &expected)) return NULL;
	if (pg_alpha_equal(expected, suspended->subject->core) != 1) return NULL;
	if (!pg_thunk_type_view(suspended->classifier, &computation)) return NULL;
	if (!pg_computation_type_view(computation, &totality, &effects, &value)) return NULL;
	if (totality != PG_TOTALITY_TOTAL) return NULL;
	const struct pg_term *core = pg_termination_witness(classifiers, suspended->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, suspended->context, core, NULL, 1, &suspended->subject);
	const struct pg_evidence *premises[] = {formation, suspended};
	return subject ? accept(typing, PG_TERMINATION_INTRO, PG_JUDGEMENT_VALUE,
		suspended->context, subject, formation->subject->core, 2, premises) : NULL;
}

const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value_type)
{
	if (!typing) return NULL;
	return pg_prove_effect_type(typing, classifiers, pg_effect_row(typing->graph, 0, NULL), value_type);
}

const struct pg_evidence *pg_prove_effect_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_effect_row *effects,
	const struct pg_evidence *value_type)
{
	return pg_prove_computation_type(typing, classifiers, PG_TOTALITY_UNSPECIFIED, effects, value_type);
}

const struct pg_evidence *pg_prove_computation_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, enum pg_totality totality,
	const struct pg_effect_row *effects, const struct pg_evidence *value_type)
{
	if (!effects) return NULL;
	return unary_formation(typing, classifiers, value_type, PG_RETURN_TYPE_FORM, totality, effects);
}

static int endpoint(const struct pg_typing *typing, const struct pg_evidence *term,
	enum pg_evidence_judgement judgement, const struct pg_context *context,
	const struct pg_term *type)
{
	if (!pg_evidence_owned_by(term, typing)) return 0;
	if (term->judgement != judgement) return 0;
	if (term->context != context) return 0;
	return pg_alpha_equal(term->classifier, type) == 1;
}

const struct pg_evidence *pg_prove_identity_type(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *left,
	const struct pg_evidence *right)
{
	if (!pg_evidence_owned_by(type, typing)) return NULL;
	enum pg_evidence_judgement elements;
	switch (type->judgement) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!endpoint(typing, left, elements, type->context, type->subject->core)) return NULL;
	if (!endpoint(typing, right, elements, type->context, type->subject->core)) return NULL;
	const struct pg_term *family = pg_identity_action(typing->graph, type->subject->core);
	const struct pg_term *core = pg_identity_instance(typing->graph, family,
		left->subject->core, right->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {type->subject, left->subject, right->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, type->context, core, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {type, left, right};
	return accept(typing, PG_IDENTITY_FORM, type->judgement, type->context,
		subject, type->classifier, 3, premises);
}

static int universe_identity(const struct pg_typing *typing,
	const struct pg_evidence *family, const struct pg_term **left,
	const struct pg_term **right, uint64_t *level)
{
	if (!pg_evidence_owned_by(family, typing)) return 0;
	if (family->judgement != PG_JUDGEMENT_VALUE) return 0;
	const struct pg_term *universe;
	if (!pg_identity_view(family->classifier, &universe, left, right)) return 0;
	return pg_universe_level(universe, level);
}

const struct pg_evidence *pg_prove_identity_endpoint_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	enum pg_evidence_rule side)
{
	if (classifiers->graph != typing->graph) return NULL;
	const struct pg_term *left, *right, *core;
	uint64_t level;
	if (!universe_identity(typing, family, &left, &right, &level)) return NULL;
	switch (side) {
	case PG_IDENTITY_LEFT_TYPE: core = left; break;
	case PG_IDENTITY_RIGHT_TYPE: core = right; break;
	default: return NULL;
	}
	const struct pg_term *sort = pg_universe(classifiers, level);
	if (!sort) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context,
		core, NULL, 1, &family->subject);
	if (!subject) return NULL;
	return accept(typing, side, PG_JUDGEMENT_VALUE_TYPE, family->context,
		subject, sort, 1, &family);
}

const struct pg_evidence *pg_prove_identity_instance(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *left, const struct pg_evidence *right)
{
	if (classifiers->graph != typing->graph) return NULL;
	const struct pg_term *left_type, *right_type;
	uint64_t level;
	if (!universe_identity(typing, family, &left_type, &right_type, &level)) return NULL;
	if (!endpoint(typing, left, PG_JUDGEMENT_VALUE, family->context, left_type)) return NULL;
	if (!endpoint(typing, right, PG_JUDGEMENT_VALUE, family->context, right_type)) return NULL;
	const struct pg_term *core = pg_identity_instance(typing->graph, family->subject->core,
		left->subject->core, right->subject->core);
	if (!core) return NULL;
	const struct pg_term *sort = pg_universe(classifiers, level);
	if (!sort) return NULL;
	const struct pg_occurrence *operands[] = {family->subject, left->subject, right->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context, core, NULL, 3, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {family, left, right};
	return accept(typing, PG_IDENTITY_INSTANCE, PG_JUDGEMENT_VALUE_TYPE, family->context,
		subject, sort, 3, premises);
}

const struct pg_evidence *pg_prove_reflexivity(struct pg_typing *typing,
	const struct pg_evidence *type, const struct pg_evidence *term)
{
	const struct pg_evidence *identity = pg_prove_identity_type(typing, type, term, term);
	if (!identity) return NULL;
	const struct pg_term *core = pg_identity_action(typing->graph, term->subject->core);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, term->context, core, NULL, 1, &term->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {identity, term};
	return accept(typing, PG_REFLEXIVITY, term->judgement, term->context,
		subject, identity->subject->core, 2, premises);
}

const struct pg_evidence *pg_prove_identity_transport(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction)
{
	if ((unsigned)direction > PG_IDENTITY_LEFT) return NULL;
	const struct pg_term *left, *right;
	uint64_t level;
	if (!universe_identity(typing, family, &left, &right, &level)) return NULL;
	const struct pg_term *domain = direction == PG_IDENTITY_RIGHT ? left : right;
	if (!endpoint(typing, value, PG_JUDGEMENT_VALUE, family->context, domain)) return NULL;
	const struct pg_evidence *target = pg_prove_identity_endpoint_type(typing, classifiers, family,
		direction == PG_IDENTITY_RIGHT ? PG_IDENTITY_RIGHT_TYPE : PG_IDENTITY_LEFT_TYPE);
	if (!target) return NULL;
	const struct pg_term *core = pg_identity_transport(typing->graph, family->subject->core, value->subject->core, direction);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {family->subject, value->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {target, family, value};
	return accept(typing, PG_IDENTITY_TRANSPORT, PG_JUDGEMENT_VALUE, family->context,
		subject, target->subject->core, 3, premises);
}

const struct pg_evidence *pg_prove_identity_lift(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *family,
	const struct pg_evidence *value, enum pg_identity_direction direction)
{
	const struct pg_evidence *transport = pg_prove_identity_transport(typing, classifiers, family, value, direction);
	if (!transport) return NULL;
	const struct pg_evidence *left = direction == PG_IDENTITY_RIGHT ? value : transport;
	const struct pg_evidence *right = direction == PG_IDENTITY_RIGHT ? transport : value;
	const struct pg_evidence *type = pg_prove_identity_instance(typing, classifiers, family, left, right);
	if (!type) return NULL;
	const struct pg_term *core = pg_identity_lift(typing->graph, family->subject->core, value->subject->core, direction);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {family->subject, value->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, family->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {type, transport};
	return accept(typing, PG_IDENTITY_LIFT, PG_JUDGEMENT_VALUE, family->context,
		subject, type->subject->core, 2, premises);
}

const struct pg_evidence *pg_prove_thunk_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation_type)
{
	return unary_formation(typing, classifiers, computation_type, PG_THUNK_TYPE_FORM, PG_TOTALITY_UNSPECIFIED, NULL);
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
			if (!pg_universe_level(extension->premises[1]->classifier, &bound)) goto done;
		} else if (extension->rule == PG_CONTEXT_FAMILY_EXTEND) {
			if (!pg_universe_level(extension->premises[2]->classifier, &bound)) goto done;
			const struct pg_evidence *indices = extension->premises[1];
			while (indices->context != extension->premises[0]->context) {
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

const struct pg_evidence *pg_prove_pi(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain)
{
	if (classifiers->graph != typing->graph) return NULL;
	if (!context_proof(typing, extended_context)) return NULL;
	const struct pg_context *scope = extended_context->context;
	if (!scope) return NULL;
	if (!pg_evidence_owned_by(codomain, typing)) return NULL;
	if (codomain->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (codomain->context != scope) return NULL;
	const struct pg_evidence *premises[] = {extended_context, codomain};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_FORM,
		PG_JUDGEMENT_COMPUTATION_TYPE, scope->parent, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	uint64_t left, right;
	if (binding_level(extended_context, &left)) return NULL;
	if (!pg_universe_level(codomain->classifier, &right)) return NULL;
	const struct pg_term *bound = pg_universe(classifiers, left > right ? left : right);
	const struct pg_term *term = pg_pi(typing->graph, scope->declared_type,
		scope->binder, codomain->subject->core);
	if (!bound || !term) return NULL;
	const struct pg_occurrence *domain = pg_occurrence(typing, scope->parent, scope->declared_type, NULL, 0, NULL);
	if (!domain) return NULL;
	const struct pg_occurrence *operands[] = {domain, codomain->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, scope->parent, term, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_PI_FORM, PG_JUDGEMENT_COMPUTATION_TYPE,
		scope->parent, subject, bound, 2, premises);
}

static const struct pg_evidence *unary_term(struct pg_typing *typing,
	const struct pg_evidence *argument, const struct pg_object *operation,
	const struct pg_term *classifier, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement)
{
	if (!classifier) return NULL;
	const struct pg_term *term = pg_application(typing->graph,
		pg_reference(typing->graph, operation), argument->subject->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, argument->context,
		term, NULL, 1, &argument->subject);
	if (!subject) return NULL;
	return accept(typing, rule, judgement, argument->context, subject, classifier, 1, &argument);
}

const struct pg_evidence *pg_prove_return(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value)
{
	return pg_prove_return_contract(typing, classifiers, PG_TOTALITY_UNSPECIFIED, value);
}

const struct pg_evidence *pg_prove_return_contract(struct pg_typing *typing,
	struct pg_classifiers *classifiers, enum pg_totality totality,
	const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	const struct pg_term *type = pg_computation_type(classifiers, totality,
		pg_effect_row(typing->graph, 0, NULL), value->classifier);
	if (!type) return NULL;
	return unary_term(typing, value, &pg_return_operation,
		type, PG_RETURN_INTRO, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_thunk(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	return unary_term(typing, computation, &pg_thunk_operation,
		pg_thunk_type(classifiers, computation->classifier), PG_THUNK_INTRO, PG_JUDGEMENT_VALUE);
}

const struct pg_evidence *pg_prove_force(struct pg_typing *typing, const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	const struct pg_term *classifier;
	if (!pg_thunk_type_view(value->classifier, &classifier)) return NULL;
	return unary_term(typing, value, &pg_force_operation, classifier, PG_FORCE_ELIM, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_lambda(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *body)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pi->rule != PG_PI_FORM) return NULL;
	if (!pg_evidence_owned_by(body, typing)) return NULL;
	if (body->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	if (body->context != pi->premises[0]->context) return NULL;
	if (pg_alpha_equal(body->classifier, codomain) != 1) return NULL;
	const struct pg_term *term = pg_lambda(typing->graph, binder, body->subject->core);
	if (!term) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, term, domain, 1, &body->subject);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {pi, body};
	return accept(typing, PG_LAMBDA_INTRO, PG_JUDGEMENT_COMPUTATION,
		pi->context, subject, pi->subject->core, 2, premises);
}


const struct pg_evidence *pg_prove_abstract(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *prefix,
	const struct pg_evidence *context, const struct pg_evidence *body)
{
	if (classifiers->graph != typing->graph) return NULL;
	if (!context_proof(typing, prefix) || !context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(body, typing) || body->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (body->context != context->context) return NULL;
	const struct pg_evidence *type = pg_prove_classifier(typing, classifiers, context, body);
	if (!type) return NULL;
	while (context->context != prefix->context) {
		if (context->rule != PG_CONTEXT_EXTEND && context->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
		type = pg_prove_pi(typing, classifiers, context, type);
		body = pg_prove_lambda(typing, type, body);
		if (!body) return NULL;
		context = context->premises[0];
	}
	return body;
}

/* Inversion uses an accepted judgement, never an untyped constructor spine. */
static const struct pg_evidence *term_content(struct pg_typing *typing,
	const struct pg_evidence *proof, const struct pg_object *operation,
	const struct pg_term *classifier, enum pg_evidence_rule rule,
	enum pg_evidence_judgement judgement)
{
	const struct pg_term *core = proof->subject->core;
	if (core->kind != PG_APPLICATION) return NULL;
	const struct pg_term *head = core->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != operation) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, proof->context,
		core->as.application.argument, NULL, 1, &proof->subject);
	if (!subject) return NULL;
	return accept(typing, rule, judgement, proof->context, subject, classifier, 1, &proof);
}

const struct pg_evidence *pg_prove_return_value(struct pg_typing *typing,
	const struct pg_evidence *computation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (computation->rule == PG_RETURN_INTRO) return computation->premises[0];
	const struct pg_term *classifier;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(computation->classifier, &totality, &effects, &classifier) || pg_effect_count(effects)) return NULL;
	return term_content(typing, computation, &pg_return_operation, classifier,
		PG_RETURN_VALUE, PG_JUDGEMENT_VALUE);
}

const struct pg_evidence *pg_prove_thunk_computation(struct pg_typing *typing,
	const struct pg_evidence *value)
{
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (value->rule == PG_THUNK_INTRO) return value->premises[0];
	const struct pg_term *classifier;
	if (!pg_thunk_type_view(value->classifier, &classifier)) return NULL;
	return term_content(typing, value, &pg_thunk_operation, classifier,
		PG_THUNK_COMPUTATION, PG_JUDGEMENT_COMPUTATION);
}

const struct pg_evidence *pg_prove_total_pure_value(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (!binder || binder->kind != PG_BINDER) return NULL;
	const struct pg_term *type;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(computation->classifier, &totality, &effects, &type)) return NULL;
	if (totality != PG_TOTALITY_TOTAL || pg_effect_count(effects)) return NULL;
	const struct pg_term *identity = pg_lambda(typing->graph, binder, pg_reference(typing->graph, binder));
	const struct pg_term *core = pg_computation_fold(typing->graph, computation->subject->core, identity, 0, NULL);
	if (!core) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, computation->context, core, NULL, 1, &computation->subject);
	if (!subject) return NULL;
	return accept(typing, PG_TOTAL_PURE_VALUE, PG_JUDGEMENT_VALUE,
		computation->context, subject, type, 1, &computation);
}


const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(function, typing)) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (function->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE && argument->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (function->context != argument->context) return NULL;
	const struct pg_evidence *premises[] = {function, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_APP_ELIM,
		PG_JUDGEMENT_COMPUTATION, function->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(function->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, argument->classifier) != 1) return NULL;
	struct pg_binding_value substitution = {binder, argument->subject->core};
	const struct pg_term *classifier = pg_substitution_compute(&typing->substitutions, codomain, 1, &substitution);
	const struct pg_term *term = pg_application(typing->graph, function->subject->core, argument->subject->core);
	if (!classifier || !term) return NULL;
	const struct pg_occurrence *operands[] = {function->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, function->context, term, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_APP_ELIM, PG_JUDGEMENT_COMPUTATION,
		function->context, subject, classifier, 2, premises);
}

const struct pg_evidence *pg_prove_conversion(struct pg_typing *typing,
	const struct pg_evidence *term, const struct pg_evidence *target_type,
	const struct pg_conversion_certificate *certificate)
{
	if (!certificate) return NULL;
	if (!pg_evidence_owned_by(term, typing)) return NULL;
	if (!pg_evidence_owned_by(target_type, typing)) return NULL;
	if (term->context != target_type->context) return NULL;
	switch (term->judgement) {
	case PG_JUDGEMENT_VALUE:
		target_type = pg_prove_value_type(typing, target_type);
		if (!target_type) return NULL;
		break;
	case PG_JUDGEMENT_COMPUTATION:
		if (target_type->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
		break;
	default:
		return NULL;
	}
	if (pg_conversion_left(certificate) != term->classifier) return NULL;
	if (pg_conversion_right(certificate) != target_type->subject->core) return NULL;
	const struct pg_evidence *premises[] = {term, target_type};
	return accept_with_conversion(typing, PG_TYPE_CONVERSION, term->judgement,
		term->context, term->subject, target_type->subject->core, 2, premises, certificate);
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
	if (!source->subject) return NULL;
	switch (source->judgement) {
	case PG_JUDGEMENT_VALUE: case PG_JUDGEMENT_COMPUTATION:
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE:
	case PG_JUDGEMENT_TYPE_FAMILY: break;
	default: return NULL;
	}
	if (pg_reduction_policy(certificate) != &pg_pure_policy) return NULL;
	if (pg_reduction_source(certificate) != source->subject->core) return NULL;
	const struct pg_term *target = pg_reduction_target(certificate);
	if (target == source->subject->core) return source;
	const struct pg_occurrence *subject = pg_occurrence(typing, source->context,
		target, NULL, 1, &source->subject);
	if (!subject) return NULL;
	return accept_record(typing, PG_PURE_NORMALIZATION, source->judgement,
		source->context, subject, source->classifier, 1, &source, certificate, 0, NULL);
}
const struct pg_evidence *pg_prove_projection(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_evidence *proof)
{
	if (!context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(proof, typing)) return NULL;
	if (proof->judgement == PG_JUDGEMENT_CONTEXT) return NULL;
	if (proof->judgement == PG_JUDGEMENT_SUBSTITUTION) return NULL;
	const struct pg_context *cursor = context->context;
	while (cursor != proof->context) {
		if (!cursor) return NULL;
		cursor = cursor->parent;
	}
	if (context->context == proof->context) return proof;
	const struct pg_occurrence *old = proof->subject;
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context,
		old->core, old->annotation, old->operand_count, old->operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {context, proof};
	return accept(typing, PG_CONTEXT_PROJECTION, proof->judgement,
		context->context, subject, proof->classifier, 2, premises);
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
	const struct pg_context *base = prefix ? prefix->premises[0]->context : NULL;
	if (pg_context_extension_size(source->context, base, &suffix) || suffix != count) return NULL;
	if (count > SIZE_MAX - retained) return NULL;
	size_t total = retained + count;
	if (total > SIZE_MAX / sizeof(struct pg_binding_value)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_context *)) return NULL;
	if (total > SIZE_MAX / sizeof(const struct pg_evidence *) - 2) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **declarations = pg_alloc(&temporary, count * sizeof(*declarations));
	struct pg_binding_value *bindings = pg_alloc(&temporary, total * sizeof(*bindings));
	const struct pg_evidence **premises = pg_alloc(&temporary, (total + 2) * sizeof(*premises));
	if (!premises) goto done;
	if (count && !declarations) goto done;
	if (total && !bindings) goto done;
	const struct pg_evidence *scope = source;
	for (size_t i = count; i; --i) { declarations[i - 1] = scope; scope = scope->premises[0]; }
	premises[0] = source;
	premises[1] = destination;
	if (prefix) {
		const struct pg_binding_value *known = (const struct pg_binding_value *)(prefix->premises + prefix->premise_count);
		for (size_t i = 0; i < retained; ++i) {
			premises[i + 2] = prefix->premises[i + 2];
			bindings[i] = known[i];
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		if (!pg_evidence_owned_by(image, typing)) goto done;
		if (image->judgement != binding_judgement(declarations[i])) goto done;
		if (image->context != destination->context) goto done;
		premises[retained + i + 2] = image;
	}
	uint64_t hash;
	result = find_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, total + 2, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		const struct pg_term *expected = pg_substitution_compute(&typing->substitutions, declarations[i]->context->declared_type, retained + i, bindings);
		if (!expected) goto done;
		if (pg_alpha_equal(expected, image->classifier) != 1) goto done;
		bindings[retained + i] = (struct pg_binding_value){declarations[i]->context->binder, image->subject->core};
	}
	result = accept_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, total + 2, premises, NULL, total, bindings);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_substitution_image(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_object *binder)
{
	if (!pg_evidence_owned_by(substitution, typing)) return NULL;
	if (substitution->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	const struct pg_binding_value *bindings =
		(const struct pg_binding_value *)(substitution->premises + substitution->premise_count);
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
	if (pg_context_extension_size(destination->context, source->context, &count)) return NULL;
	if (pg_context_extension_size(source->context, NULL, &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence **images = malloc(count * sizeof(*images));
	if (count && !images) return NULL;
	const struct pg_context *context = source->context;
	for (size_t i = count; i; --i, context = context->parent)
		images[i - 1] = pg_prove_variable(typing, destination, context->binder);
	const struct pg_evidence *result = pg_prove_substitution(typing, source, destination, count, images);
	free(images);
	return result;
}

struct pg_reindex_state {
	struct pg_typing *typing;
	const struct pg_evidence *premises[2];
	const struct pg_term *inputs[3], *outputs[3];
	struct pg_substitution *substitution;
	size_t next;
	uint64_t steps;
	enum pg_reindex_status status;
	const struct pg_evidence *result;
};

static int reindex_prepare(struct pg_reindex_state *state, struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	if (!pg_evidence_owned_by(substitution, typing)) return -1;
	if (substitution->rule != PG_CONTEXT_SUBSTITUTION) return -1;
	if (!pg_evidence_owned_by(proof, typing)) return -1;
	if (!proof->subject) return -1;
	if (proof->context != substitution->premises[0]->context) return -1;
	state->typing = typing;
	state->premises[0] = substitution;
	state->premises[1] = proof;
	state->inputs[0] = proof->subject->core;
	state->inputs[1] = proof->classifier;
	state->inputs[2] = proof->subject->annotation;
	const struct pg_evidence *premises[] = {substitution, proof};
	uint64_t hash;
	state->result = find_record(typing, PG_REINDEX, proof->judgement,
		substitution->context, NULL, NULL, 2, premises, NULL, &hash);
	state->status = state->result ? PG_REINDEX_DONE : PG_REINDEX_PENDING;
	return 0;
}

int pg_reindex_init(struct pg_reindex *work, struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	work->state = calloc(1, sizeof(*work->state));
	if (!work->state) return -1;
	if (reindex_prepare(work->state, typing, substitution, proof) == 0) return 0;
	pg_reindex_destroy(work);
	return -1;
}

static enum pg_reindex_status reindex_step(struct pg_reindex_state *state)
{
	struct pg_typing *typing = state->typing;
	const struct pg_evidence *substitution = state->premises[0], *proof = state->premises[1];
	if (state->next < 3) {
		if (!state->inputs[state->next]) { ++state->next; return PG_REINDEX_PENDING; }
		if (!state->substitution) {
			size_t count = substitution->premise_count - 2;
			const struct pg_binding_value *bindings = (const struct pg_binding_value *)(substitution->premises + substitution->premise_count);
			state->substitution = pg_substitution_request(&typing->substitutions,
				state->inputs[state->next], count, bindings);
			if (!state->substitution) return PG_REINDEX_ERROR;
			return PG_REINDEX_PENDING;
		}
		switch (pg_substitution_advance(state->substitution, 1)) {
		case PG_SUBSTITUTION_PENDING: return PG_REINDEX_PENDING;
		case PG_SUBSTITUTION_ERROR: return PG_REINDEX_ERROR;
		case PG_SUBSTITUTION_DONE:
			state->outputs[state->next++] = pg_substitution_result(state->substitution);
			state->substitution = NULL;
			return PG_REINDEX_PENDING;
		}
	}
	const struct pg_occurrence *old = proof->subject;
	const struct pg_occurrence *subject = pg_occurrence(typing, substitution->context, state->outputs[0],
		state->outputs[2], old->operand_count, old->operands);
	if (!subject) return PG_REINDEX_ERROR;
	state->result = accept(typing, PG_REINDEX, proof->judgement, substitution->context,
		subject, state->outputs[1], 2, state->premises);
	return state->result ? PG_REINDEX_DONE : PG_REINDEX_ERROR;
}

enum pg_reindex_status pg_reindex_status(const struct pg_reindex *work)
{
	return work->state ? work->state->status : PG_REINDEX_ERROR;
}

enum pg_reindex_status pg_reindex_advance(struct pg_reindex *work, uint64_t budget)
{
	while (pg_reindex_status(work) == PG_REINDEX_PENDING && budget) {
		--budget;
		++work->state->steps;
		work->state->status = reindex_step(work->state);
	}
	return pg_reindex_status(work);
}

const struct pg_evidence *pg_reindex_result(const struct pg_reindex *work)
{
	return pg_reindex_status(work) == PG_REINDEX_DONE ? work->state->result : NULL;
}

uint64_t pg_reindex_steps(const struct pg_reindex *work)
{
	return work->state ? work->state->steps : 0;
}

void pg_reindex_destroy(struct pg_reindex *work)
{
	if (!work->state) return;
	free(work->state);
	work->state = NULL;
}

const struct pg_evidence *pg_prove_reindex(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *proof)
{
	struct pg_reindex_state state = {0};
	struct pg_reindex work = {&state};
	if (reindex_prepare(&state, typing, substitution, proof) != 0) return NULL;
	while (pg_reindex_advance(&work, UINT64_MAX) == PG_REINDEX_PENDING) {}
	const struct pg_evidence *result = pg_reindex_result(&work);
	return result;
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
	const struct pg_term *abstraction = source->subject->core;
	const struct pg_context *context = source->context;
	for (size_t i = 0; i < count; ++i, context = context->parent)
		abstraction = pg_lambda(typing->graph, context->binder, abstraction);
	const struct pg_binding_value *bindings = (const struct pg_binding_value *)(
		left->premises + left->premise_count);
	abstraction = pg_substitution_compute(&typing->substitutions, abstraction, common, bindings);
	if (!abstraction) return NULL;
	const struct pg_term *result = pg_identity_action(typing->graph, abstraction);
	for (size_t i = 0; i < count; ++i) {
		result = pg_identity_instance(typing->graph, result,
			left->premises[common + i + 2]->subject->core,
			right->premises[common + i + 2]->subject->core);
		result = pg_application(typing->graph, result, paths[i]->subject->core);
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
	switch (family->judgement) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!substitution_proof(typing, left_substitution)) return NULL;
	if (!substitution_proof(typing, right_substitution)) return NULL;
	if (left_substitution->premises[0]->context != family->context) return NULL;
	if (right_substitution->premises[0]->context != family->context) return NULL;
	const struct pg_context *context = left_substitution->context;
	if (right_substitution->context != context) return NULL;
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
	result = find_record(typing, PG_FAMILY_IDENTITY_FORM,
		family->judgement, context, NULL, NULL, count + 5, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < common; ++i) {
		if (pg_alpha_equal(left_substitution->premises[i + 2]->subject->core,
			right_substitution->premises[i + 2]->subject->core) != 1) goto done;
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
			left_substitution->premises[common + i + 2]->subject->core,
			right_substitution->premises[common + i + 2]->subject->core);
		if (!endpoint(typing, paths[i], PG_JUDGEMENT_VALUE, context, path_type)) goto done;
	}
	const struct pg_evidence *ltype = pg_prove_reindex(typing, left_substitution, family);
	const struct pg_evidence *rtype = pg_prove_reindex(typing, right_substitution, family);
	if (!ltype || !rtype) goto done;
	if (!endpoint(typing, left, elements, context, ltype->subject->core)) goto done;
	if (!endpoint(typing, right, elements, context, rtype->subject->core)) goto done;
	const struct pg_term *acted = family_action_core(typing, family,
		left_substitution, right_substitution, common, count, paths);
	const struct pg_term *core = pg_identity_instance(typing->graph, acted, left->subject->core, right->subject->core);
	if (!core) goto done;
	operands[0] = family->subject;
	for (size_t i = 0; i < count; ++i) operands[i + 1] = paths[i]->subject;
	operands[count + 1] = left->subject;
	operands[count + 2] = right->subject;
	const struct pg_occurrence *subject = pg_occurrence(typing, context, core, NULL, count + 3, operands);
	if (!subject) goto done;
	result = accept(typing, PG_FAMILY_IDENTITY_FORM, family->judgement, context,
		subject, family->classifier, count + 5, premises);
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
	switch (family->judgement) {
	case PG_JUDGEMENT_VALUE_TYPE: elements = PG_JUDGEMENT_VALUE; break;
	case PG_JUDGEMENT_COMPUTATION_TYPE: elements = PG_JUDGEMENT_COMPUTATION; break;
	default: return NULL;
	}
	if (!endpoint(typing, term, elements, family->context, family->subject->core)) return NULL;
	const struct pg_evidence *left = pg_prove_reindex(typing, left_substitution, term);
	const struct pg_evidence *right = pg_prove_reindex(typing, right_substitution, term);
	const struct pg_evidence *identity = pg_prove_family_identity_type(typing,
		family, left_substitution, right_substitution, count, paths, left, right);
	if (!identity) return NULL;
	const struct pg_evidence *premises[] = {identity, term};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_FAMILY_ACTION,
		elements, identity->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *core = family_action_core(typing, term,
		left_substitution, right_substitution, left_substitution->premise_count - 2 - count, count, paths);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {term->subject, identity->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, identity->context,
		core, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_FAMILY_ACTION, elements, identity->context,
		subject, identity->subject->core, 2, premises);
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
	if (first->context != second->premises[0]->context) return NULL;
	size_t count = first->premise_count - 2;
	struct pg_graph temporary = {0};
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	const struct pg_evidence *result = NULL;
	if (count && !images) goto done;
	for (size_t i = 0; i < count; ++i) {
		images[i] = pg_prove_reindex(typing, second, first->premises[i + 2]);
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
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND && source_extension->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	if (!context_proof(typing, destination)) return NULL;
	if (!pg_evidence_owned_by(image, typing)) return NULL;
	if (image->judgement != binding_judgement(source_extension)) return NULL;
	if (image->context != destination->context) return NULL;
	size_t count = substitution->premise_count - 2;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *) - 3) return NULL;
	if (count >= SIZE_MAX / sizeof(struct pg_binding_value)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 3) * sizeof(*premises));
	struct pg_binding_value *bindings = pg_alloc(&temporary, (count + 1) * sizeof(*bindings));
	const struct pg_evidence *result = NULL;
	if (!premises || !bindings) goto done;
	premises[0] = source_extension;
	premises[1] = destination;
	const struct pg_binding_value *prefix = (const struct pg_binding_value *)(
		substitution->premises + substitution->premise_count);
	for (size_t i = 0; i < count; ++i) {
		premises[i + 2] = pg_prove_projection(typing, destination, substitution->premises[i + 2]);
		if (!premises[i + 2]) goto done;
		bindings[i] = prefix[i];
	}
	premises[count + 2] = image;
	uint64_t hash;
	result = find_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, count + 3, premises, NULL, &hash);
	if (result) goto done;
	const struct pg_term *expected = pg_substitution_compute(&typing->substitutions,
		source_extension->context->declared_type, count, bindings);
	if (!expected) goto done;
	if (pg_alpha_equal(expected, image->classifier) != 1) goto done;
	bindings[count] = (struct pg_binding_value){source_extension->context->binder, image->subject->core};
	result = accept_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, count + 3, premises, NULL, count + 1, bindings);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_substitution_pair(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_evidence *image)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	return substitution_pair(typing, substitution, source_extension,
		substitution->premises[1], image);
}

struct lift_frame {
	struct lift_frame *parent;
	const struct pg_evidence *substitution, *extension, *map;
	const struct pg_object *binder;
	const struct pg_evidence **indices;
	size_t count, next;
};

const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND && source_extension->rule != PG_CONTEXT_FAMILY_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	if (!binder || binder->kind != PG_BINDER || pg_context_lookup(substitution->context, binder)) return NULL;
	struct pg_graph temporary = {0};
	struct lift_frame root = {.substitution = substitution, .extension = source_extension, .binder = binder};
	struct lift_frame *frame = &root;
	const struct pg_evidence *result = NULL;
	while (frame) {
		const struct pg_evidence *extension = frame->extension;
		const struct pg_evidence *destination;
		if (extension->rule == PG_CONTEXT_EXTEND) {
			const struct pg_evidence *domain = pg_prove_reindex(typing, frame->substitution, extension->premises[1]);
			destination = pg_prove_context_extension(typing, frame->substitution->premises[1], frame->binder, domain);
		} else {
			if (!frame->map) {
				const struct pg_evidence *indices = extension->premises[1];
				if (pg_context_extension_size(indices->context, extension->context->parent, &frame->count)) goto done;
				if (frame->count > SIZE_MAX / sizeof(*frame->indices)) goto done;
				frame->indices = pg_alloc(&temporary, frame->count * sizeof(*frame->indices));
				if (!frame->indices) goto done;
				for (size_t i = frame->count; i; --i, indices = indices->premises[0]) frame->indices[i - 1] = indices;
				frame->map = frame->substitution;
			}
			if (frame->next < frame->count) {
				struct lift_frame *child = pg_alloc(&temporary, sizeof(*child));
				if (!child) goto done;
				/* Signature-local binders are not declarations in the ambient
				 * destination. Allocate them once per lifted family producer. */
				*child = (struct lift_frame){.parent = frame, .substitution = frame->map,
					.extension = frame->indices[frame->next++], .binder = pg_binder(typing->graph)};
				frame = child;
				continue;
			}
			const struct pg_evidence *universe = pg_prove_reindex(typing, frame->map, extension->premises[2]);
			destination = pg_prove_family_context_extension(typing, frame->substitution->premises[1],
				frame->binder, frame->map->premises[1], universe);
		}
		if (!destination) goto done;
		const struct pg_evidence *image = pg_prove_variable(typing, destination, frame->binder);
		const struct pg_evidence *lifted = substitution_pair(typing, frame->substitution, extension, destination, image);
		if (!lifted) goto done;
		frame = frame->parent;
		if (frame) frame->map = lifted;
		else result = lifted;
	}
done:
	pg_graph_destroy(&temporary);
	return result;
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

/* Invert whole constructor-pattern indices, not their individual fields.
 * Rebuild the nominal family's checked index substitution; dependent later
 * indices must still type-check after replacement. No equality is asserted. */
static const struct pg_evidence *pattern_index_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *prefix,
	const struct pg_evidence *pattern, const struct pg_evidence *inverse,
	const struct pg_evidence *body)
{
	if (!body) return NULL;
	const struct pg_effect_row *effects;
	const struct pg_term *content;
	enum pg_totality totality;
	if (!pg_computation_type_view(body->subject->core, &totality, &effects, &content)) return body;
	struct pg_inductive_instance instance;
	if (!pg_inductive_instance(typing, pg_prove_return_content(typing, body), &instance) || !instance.indices) return body;
	size_t count = instance.indices->premise_count - 2;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = body;
	const struct pg_evidence **images = pg_alloc(&temporary, count * sizeof(*images));
	if (!images) goto done;
	for (size_t i = 0; i < count; ++i) images[i] = instance.indices->premises[i + 2];
	size_t first = instance.parameters->premise_count - 1;
	int changed = 0;
	for (size_t i = first; i < count; ++i) {
		const struct pg_evidence *selected = NULL;
		const struct pg_evidence *extension = pattern->premises[0];
		for (size_t j = pattern->premise_count - 2; extension->context != prefix->context;
			--j, extension = extension->premises[0]) {
			const struct pg_evidence *image = pattern->premises[j + 1];
			if (image->subject->core->kind == PG_REFERENCE && image->subject->core->as.reference->kind == PG_BINDER) continue;
			image = pg_prove_reindex(typing, inverse, image);
			if (!image || pg_alpha_equal(image->subject->core, images[i]->subject->core) != 1) continue;
			if (selected) goto done;
			selected = pg_prove_variable(typing, inverse->premises[1], extension->context->binder);
			if (!selected) goto done;
		}
		if (selected) { images[i] = selected; changed = 1; }
	}
	if (changed) {
		const struct pg_evidence *map = pg_prove_substitution(typing,
			instance.indices->premises[0], inverse->premises[1], count, images);
		const struct pg_evidence *type = family_in_scope(typing, instance.formation, instance.parameters, map);
		if (type) result = pg_prove_computation_type(typing, classifiers, totality, effects, type);
	}
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_pattern_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *prefix,
	const struct pg_evidence *pattern, const struct pg_evidence *body)
{
	if (!typing || !classifiers || classifiers->graph != typing->graph) return NULL;
	if (!context_proof(typing, prefix) || !substitution_proof(typing, pattern)) return NULL;
	if (!pg_evidence_owned_by(body, typing)) return NULL;
	if (body->judgement != PG_JUDGEMENT_COMPUTATION_TYPE || body->context != pattern->context) return NULL;
	const struct pg_evidence *source = pattern->premises[0], *destination = pattern->premises[1];
	size_t source_count, destination_count, common;
	if (pg_context_extension_size(source->context, prefix->context, &source_count)) return NULL;
	if (pg_context_extension_size(destination->context, prefix->context, &destination_count)) return NULL;
	if (pg_context_extension_size(prefix->context, NULL, &common)) return NULL;
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
		const struct pg_term *image = pattern->premises[common + i + 1]->subject->core;
		if (image->kind != PG_REFERENCE || image->as.reference->kind != PG_BINDER) continue;
		if (pattern_variable(&variables, image->as.reference)) goto done;
		entries[i - 1].image = image->as.reference;
		entries[i - 1].binder = extension->context->binder;
		if (pg_index_insert(&variables, &entries[i - 1].index, (uintptr_t)image->as.reference)) goto done;
	}
	for (size_t i = common; i; --i, extension = extension->premises[0]) {
		const struct pg_object *binder = extension->context->binder;
		if (pattern->premises[i + 1]->subject->core != pg_reference(typing->graph, binder)) goto done;
		if (pattern_variable(&variables, binder)) goto done;
	}
	extension = destination;
	for (size_t i = destination_count; i; --i, extension = extension->premises[0]) {
		if (extension->rule != PG_CONTEXT_EXTEND) goto done;
		fields[i - 1] = extension;
	}
	const struct pg_evidence *inverse = pg_prove_substitution_projection(typing, prefix, source);
	for (size_t i = 0; inverse && i < destination_count; ++i) {
		const struct pattern_variable *entry = pattern_variable(&variables, fields[i]->context->binder);
		if (entry) {
			const struct pg_evidence *image = pg_prove_variable(typing, inverse->premises[1], entry->binder);
			inverse = pg_prove_substitution_pair(typing, inverse, fields[i], image);
		} else inverse = pg_prove_substitution_lift(typing, inverse, fields[i], pg_binder(typing->graph));
	}
	if (!inverse) goto done;
	result = pg_prove_reindex(typing, inverse, body);
	result = pattern_index_type(typing, classifiers, prefix, pattern, inverse, result);
	/* Keep every intermediate substitution total and typed. Only then remove
	 * fresh nuisance fields, checking that the result does not depend on them. */
	for (extension = inverse->premises[1]; result && extension->context != source->context;
		extension = extension->premises[0])
		result = pg_prove_pi_constant_codomain(typing,
			pg_prove_pi(typing, classifiers, extension, result));
	if (result) {
		const struct pg_evidence *instance = pg_prove_reindex(typing, pattern, result);
		if (!instance || pg_alpha_equal(instance->subject->core, body->subject->core) != 1) result = NULL;
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
	if (!pg_thunk_type_view(thunk_type->subject->core, &content)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, thunk_type->context, content,
		NULL, 1, &thunk_type->subject);
	if (!subject) return NULL;
	return accept(typing, PG_THUNK_CONTENT, PG_JUDGEMENT_COMPUTATION_TYPE,
		thunk_type->context, subject, thunk_type->classifier, 1, &thunk_type);
}

const struct pg_evidence *pg_prove_return_content(struct pg_typing *typing,
	const struct pg_evidence *return_type)
{
	if (!pg_evidence_owned_by(return_type, typing)) return NULL;
	if (return_type->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *content;
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(return_type->subject->core, &totality, &effects, &content)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, return_type->context, content,
		NULL, 1, &return_type->subject);
	if (!subject) return NULL;
	return accept(typing, PG_RETURN_CONTENT, PG_JUDGEMENT_VALUE_TYPE,
		return_type->context, subject, return_type->classifier, 1, &return_type);
}

const struct pg_evidence *pg_prove_pi_constant_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CONSTANT_CODOMAIN,
		PG_JUDGEMENT_COMPUTATION_TYPE, pi->context, NULL, NULL, 1, &pi, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *codomain = pg_pi_constant_codomain(pi->subject->core);
	if (!codomain) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, codomain, NULL, 1, &pi->subject);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CONSTANT_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 1, &pi);
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
	if (payload_type->context || response_type->context) return NULL;
	if (payload_type->subject->core != payload || response_type->subject->core != response) return NULL;
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
	if (payload_type->context || response_type->context) return NULL;
	return pg_operation_declaration_at(typing, pg_operation_label_create(typing->graph,
		payload_type->subject->core, response_type->subject->core), payload_type, response_type);
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

const struct pg_evidence *pg_prove_request(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_operation_declaration *declaration,
	const struct pg_evidence *payload, const struct pg_evidence *continuation)
{
	if (!declaration) return NULL;
	if (!pg_evidence_owned_by(declaration->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(declaration->response_type, typing)) return NULL;
	if (!pg_evidence_owned_by(payload, typing)) return NULL;
	if (!pg_evidence_owned_by(continuation, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (payload->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (continuation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (payload->context != continuation->context) return NULL;
	const struct pg_evidence *premises[] = {declaration->payload_type, declaration->response_type, payload, continuation};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_REQUEST_INTRO,
		PG_JUDGEMENT_COMPUTATION, payload->context, NULL, NULL, 4, premises, declaration, &hash);
	if (existing) return existing;
	if (pg_alpha_equal(payload->classifier, declaration->payload_type->subject->core) != 1) return NULL;
	const struct pg_term *domain, *codomain, *result_type;
	const struct pg_object *binder;
	if (!pg_pi_view(continuation->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, declaration->response_type->subject->core) != 1) return NULL;
	codomain = pg_pi_constant_codomain(continuation->classifier);
	const struct pg_effect_row *effects;
	enum pg_totality totality;
	if (!pg_computation_type_view(codomain, &totality, &effects, &result_type)) return NULL;
	const struct pg_object *label = pg_operation_label(declaration);
	const struct pg_effect_row *row = pg_effect_union(typing->graph, pg_effect_row(typing->graph, 1, &label), effects);
	const struct pg_term *classifier = pg_computation_type(classifiers, totality, row, result_type);
	const struct pg_term *core = pg_computation_request(typing->graph, label, payload->subject->core, continuation->subject->core);
	if (!classifier || !core) return NULL;
	const struct pg_occurrence *operands[] = {payload->subject, continuation->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, payload->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	return accept_record(typing, PG_REQUEST_INTRO, PG_JUDGEMENT_COMPUTATION,
		payload->context, subject, classifier, 4, premises, declaration, 0, NULL);
}

const struct pg_evidence *pg_prove_operation_function(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_operation_declaration *declaration)
{
	if (!declaration || !classifiers) return NULL;
	if (!pg_evidence_owned_by(declaration->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(declaration->response_type, typing)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	const struct pg_evidence *empty = pg_prove_empty_context(typing);
	const struct pg_object *a = pg_binder(typing->graph), *b = pg_binder(typing->graph);
	const struct pg_evidence *scope = pg_prove_context_extension(typing, empty, a, declaration->payload_type);
	const struct pg_evidence *response_type = pg_prove_projection(typing, scope, declaration->response_type);
	const struct pg_evidence *response_scope = pg_prove_context_extension(typing, scope, b, response_type);
	const struct pg_evidence *returned = pg_prove_return_contract(typing, classifiers, PG_TOTALITY_TOTAL,
		pg_prove_variable(typing, response_scope, b));
	const struct pg_evidence *response_pi = pg_prove_pi(typing, classifiers, response_scope,
		pg_prove_classifier(typing, classifiers, response_scope, returned));
	const struct pg_evidence *continuation = pg_prove_lambda(typing, response_pi, returned);
	const struct pg_evidence *body = pg_prove_request(typing, classifiers, declaration,
		pg_prove_variable(typing, scope, a), continuation);
	const struct pg_evidence *pi = pg_prove_pi(typing, classifiers, scope,
		pg_prove_classifier(typing, classifiers, scope, body));
	return pg_prove_lambda(typing, pi, body);
}

const struct pg_evidence *pg_prove_handler_context(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_operation_declaration *operation, const struct pg_evidence *context,
	const struct pg_evidence *carrier, const struct pg_object *payload, const struct pg_object *resume)
{
	if (!operation || !context_proof(typing, context)) return NULL;
	if (!pg_evidence_owned_by(operation->payload_type, typing)) return NULL;
	if (!pg_evidence_owned_by(operation->response_type, typing)) return NULL;
	if (!pg_evidence_owned_by(carrier, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (carrier->context != context->context) return NULL;
	if (carrier->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_effect_row *effects;
	const struct pg_term *result;
	enum pg_totality totality;
	if (!pg_computation_type_view(carrier->subject->core, &totality, &effects, &result)) return NULL;
	const struct pg_evidence *response_type = pg_prove_projection(typing, context, operation->response_type);
	const struct pg_evidence *response_scope = pg_prove_context_extension(typing, context,
		pg_binder(typing->graph), response_type);
	const struct pg_evidence *resume_type = pg_prove_thunk_type(typing, classifiers,
		pg_prove_pi(typing, classifiers, response_scope,
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
	if (pg_alpha_equal(domain, operation->payload_type->subject->core) != 1) return 0;
	type = pg_pi_constant_codomain(type);
	if (!pg_pi_view(type, &domain, &binder, &codomain)) return 0;
	if (!returning_within(pg_pi_constant_codomain(type), carrier)) return 0;
	if (!pg_thunk_type_view(domain, &resume)) return 0;
	if (!pg_pi_view(resume, &domain, &binder, &codomain)) return 0;
	if (pg_alpha_equal(domain, operation->response_type->subject->core) != 1) return 0;
	return pg_alpha_equal(pg_pi_constant_codomain(resume), carrier) == 1;
}

const struct pg_evidence *pg_prove_handler(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *computation, const struct pg_evidence *returned,
	const struct pg_evidence *carrier, size_t count, const struct pg_handler_clause *clauses)
{
	if (!count) return pg_prove_effect_subsumption(typing,
		pg_prove_fold(typing, classifiers, computation, returned), carrier);
	if (!clauses || count > (SIZE_MAX - 3) / 3) return NULL;
	size_t n = 3 + 3 * count;
	if (n > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	if (count > SIZE_MAX / sizeof(struct pg_operation_clause)) return NULL;
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!pg_evidence_owned_by(returned, typing)) return NULL;
	if (!pg_evidence_owned_by(carrier, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (returned->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (carrier->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (computation->context != returned->context) return NULL;
	if (computation->context != carrier->context) return NULL;
	const struct pg_effect_row *input_effects, *output_effects;
	const struct pg_term *input_type, *result_type, *domain, *codomain;
	const struct pg_object *binder;
	enum pg_totality input_grade, output_grade;
	if (!pg_computation_type_view(computation->classifier, &input_grade, &input_effects, &input_type)) return NULL;
	if (!pg_computation_type_view(carrier->subject->core, &output_grade, &output_effects, &result_type)) return NULL;
	/* Handling cannot guarantee that an unknown prefix reaches RETURN or an
	 * operation. As in sequencing, a typed literal RETURN is already finite. */
	if (input_grade < output_grade && !pg_prove_return_value(typing, computation)) return NULL;
	if (!pg_pi_view(returned->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, input_type) != 1) return NULL;
	if (!returning_within(pg_pi_constant_codomain(returned->classifier), carrier->subject->core)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence **premises = pg_alloc(&temporary, n * sizeof(*premises));
	const struct pg_object **labels = pg_alloc(&temporary, count * sizeof(*labels));
	struct pg_operation_clause *raw = pg_alloc(&temporary, count * sizeof(*raw));
	const struct pg_occurrence **operands = pg_alloc(&temporary, (count + 2) * sizeof(*operands));
	const struct pg_evidence *result = NULL;
	if (!premises || !labels || !raw || !operands) goto done;
	premises[0] = computation; premises[1] = returned; premises[2] = carrier;
	operands[0] = computation->subject; operands[1] = returned->subject;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_operation_declaration *operation = clauses[i].operation;
		const struct pg_evidence *body = clauses[i].body;
		if (!operation || !pg_evidence_owned_by(body, typing)) goto done;
		if (!pg_evidence_owned_by(operation->payload_type, typing)) goto done;
		if (!pg_evidence_owned_by(operation->response_type, typing)) goto done;
		if (body->judgement != PG_JUDGEMENT_COMPUTATION || body->context != computation->context) goto done;
		if (!handler_clause_type(body->classifier, operation, carrier->subject->core)) goto done;
		labels[i] = pg_operation_label(operation);
		raw[i] = (struct pg_operation_clause){labels[i], body->subject->core};
		operands[i + 2] = body->subject;
		premises[3 + 3 * i] = operation->payload_type;
		premises[4 + 3 * i] = operation->response_type;
		premises[5 + 3 * i] = body;
	}
	const struct pg_effect_row *handled = pg_effect_row(typing->graph, count, labels);
	const struct pg_effect_row *forwarded = pg_effect_difference(typing->graph, input_effects, handled);
	if (pg_effect_subset(forwarded, output_effects) != 1) goto done;
	const struct pg_term *core = pg_computation_fold(typing->graph, computation->subject->core,
		returned->subject->core, count, raw);
	if (!core) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, computation->context, core, NULL, count + 2, operands);
	if (!subject) goto done;
	const struct pg_handler_signature *signature = pg_handler_signature(typing->graph, count, labels);
	if (!signature) goto done;
	result = accept_record(typing, PG_HANDLER_ELIM, PG_JUDGEMENT_COMPUTATION,
		computation->context, subject, carrier->subject->core, n, premises, signature, 0, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_effect_subsumption(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *target_type)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!pg_evidence_owned_by(target_type, typing)) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (target_type->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (computation->context != target_type->context) return NULL;
	const struct pg_evidence *premises[] = {computation, target_type};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_EFFECT_SUBSUMPTION,
		PG_JUDGEMENT_COMPUTATION, computation->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_effect_row *source_row, *target_row;
	const struct pg_term *source_value, *target_value;
	enum pg_totality source_totality, target_totality;
	if (!pg_computation_type_view(computation->classifier, &source_totality, &source_row, &source_value)) return NULL;
	if (!pg_computation_type_view(target_type->subject->core, &target_totality, &target_row, &target_value)) return NULL;
	if (source_totality < target_totality) return NULL;
	if (pg_effect_subset(source_row, target_row) != 1) return NULL;
	if (pg_alpha_equal(source_value, target_value) != 1) return NULL;
	return accept(typing, PG_EFFECT_SUBSUMPTION, PG_JUDGEMENT_COMPUTATION,
		computation->context, computation->subject, target_type->subject->core, 2, premises);
}

const struct pg_evidence *pg_prove_fold(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *computation, const struct pg_evidence *continuation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(continuation, typing)) return NULL;
	if (computation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (continuation->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (computation->context != continuation->context) return NULL;
	const struct pg_evidence *premises[] = {computation, continuation};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_FOLD_ELIM,
		PG_JUDGEMENT_COMPUTATION, computation->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *value_type, *domain, *codomain;
	const struct pg_object *binder;
	const struct pg_effect_row *effects, *following;
	enum pg_totality first_totality, next_totality;
	if (!pg_computation_type_view(computation->classifier, &first_totality, &effects, &value_type)) return NULL;
	if (!pg_pi_view(continuation->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, value_type) != 1) return NULL;
	codomain = pg_pi_constant_codomain(continuation->classifier);
	if (!codomain) return NULL;
	const struct pg_term *result_type;
	if (pg_computation_type_view(codomain, &next_totality, &following, &result_type)) {
		enum pg_totality totality = first_totality < next_totality ? first_totality : next_totality;
		codomain = pg_computation_type(classifiers, totality, pg_effect_union(typing->graph, effects, following), result_type);
		if (!codomain) return NULL;
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
		computation->subject->core, continuation->subject->core, 0, NULL);
	if (!core) return NULL;
	const struct pg_occurrence *operands[] = {computation->subject, continuation->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, computation->context, core, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_FOLD_ELIM, PG_JUDGEMENT_COMPUTATION,
		computation->context, subject, codomain, 2, premises);
}

const struct pg_evidence *pg_prove_pi_domain(struct pg_typing *typing,
	const struct pg_evidence *pi)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	const struct pg_term *index, *fiber;
	const struct pg_object *parameter;
	if (pg_pi_view(domain, &index, &parameter, &fiber)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, domain, NULL, 1, &pi->subject);
	if (!subject) return NULL;
	return accept(typing, PG_PI_DOMAIN, PG_JUDGEMENT_VALUE_TYPE,
		pi->context, subject, pi->classifier, 1, &pi);
}

const struct pg_evidence *pg_prove_pi_codomain(struct pg_typing *typing,
	const struct pg_evidence *pi, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(pi, typing)) return NULL;
	if (pi->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE && argument->judgement != PG_JUDGEMENT_TYPE_FAMILY) return NULL;
	if (pi->context != argument->context) return NULL;
	const struct pg_evidence *premises[] = {pi, argument};
	uint64_t hash;
	const struct pg_evidence *existing = find_record(typing, PG_PI_CODOMAIN,
		PG_JUDGEMENT_COMPUTATION_TYPE, pi->context, NULL, NULL, 2, premises, NULL, &hash);
	if (existing) return existing;
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi->subject->core, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, argument->classifier) != 1) return NULL;
	struct pg_binding_value binding = {binder, argument->subject->core};
	const struct pg_term *type = pg_substitution_compute(&typing->substitutions, codomain, 1, &binding);
	if (!type) return NULL;
	const struct pg_occurrence *operands[] = {pi->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, type, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 2, premises);
}

static const struct pg_evidence *classifier_leaf(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	const struct pg_evidence *formation = NULL;
	switch (term->rule) {
	case PG_REFLEXIVITY: case PG_FAMILY_ACTION:
	case PG_TERMINATION_INTRO:
	case PG_CONSTRUCTOR_INTRO:
	case PG_IDENTITY_TRANSPORT: case PG_IDENTITY_LIFT:
		formation = term->premises[0];
		break;
	case PG_VALUE_FROM_TYPE: {
		uint64_t level;
		if (!pg_universe_level(term->classifier, &level)) return NULL;
		return pg_prove_universe(typing, classifiers, context, level);
	}
	case PG_LAMBDA_INTRO:
		formation = term->premises[0];
		break;
	case PG_MATCH_ELIM: case PG_INDUCTION_ELIM:
		formation = term->premises[term->premise_count - 1];
		break;
	case PG_TYPE_CONVERSION: case PG_EFFECT_SUBSUMPTION:
		formation = term->premises[1];
		break;
	case PG_HANDLER_ELIM:
		formation = term->premises[2];
		break;
	default:
		return NULL;
	}
	return pg_prove_projection(typing, context, formation);
}

struct pg_classifier_frame {
	const struct pg_evidence *term, *context;
	struct pg_classifier_frame *next;
};

int pg_classifier_recovery_init(struct pg_classifier_recovery *work,
	struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	*work = (struct pg_classifier_recovery){.typing = typing, .classifiers = classifiers, .context = context, .term = term, .status = -1};
	if (!context_proof(typing, context)) return -1;
	if (!classifiers || classifiers->graph != typing->graph) return -1;
	if (!pg_evidence_owned_by(term, typing)) return -1;
	if (context->context != term->context) return -1;
	work->status = 0;
	return 0;
}

static void classifier_recovery_step(struct pg_classifier_recovery *work)
{
	struct pg_typing *typing = work->typing;
	struct pg_classifiers *classifiers = work->classifiers;
	const struct pg_evidence *context = work->context, *term = work->term;
	const struct pg_evidence *formation = work->result;
	if (!work->unwinding) {
		if (term->rule == PG_CONTEXT_PROJECTION) { work->term = term->premises[1]; return; }
		if (term->rule == PG_PURE_NORMALIZATION) { work->term = term->premises[0]; return; }
		if (term->rule == PG_VARIABLE) {
			if (!work->declaration) work->declaration = term->premises[0];
			if (work->declaration->context->binder != term->subject->core->as.reference) {
				work->declaration = work->declaration->premises[0];
				return;
			}
			formation = pg_prove_projection(typing, context, work->declaration->premises[1]);
			goto leaf;
		}
		const struct pg_evidence *input, *input_context = context;
		switch (term->rule) {
		case PG_RETURN_INTRO: case PG_THUNK_INTRO:
		case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION: case PG_RETURN_VALUE: case PG_TOTAL_PURE_VALUE:
		case PG_APP_ELIM:
			input = term->premises[0];
			break;
		case PG_FOLD_ELIM:
			input = term->premises[1];
			break;
		case PG_REQUEST_INTRO:
			input = term->premises[3];
			break;
		case PG_REINDEX:
			input_context = term->premises[0]->premises[0];
			input = term->premises[1];
			break;
		default:
			formation = classifier_leaf(typing, classifiers, context, term);
			goto leaf;
		}
		struct pg_classifier_frame *frame = pg_alloc(&work->temporary, sizeof(*frame));
		if (!frame) { work->status = -1; return; }
		*frame = (struct pg_classifier_frame){term, context, work->frames};
		work->frames = frame;
		work->context = input_context;
		work->term = pg_prove_projection(typing, input_context, input);
		if (!work->term) work->status = -1;
		return;
	}
	if (work->frames) {
		term = work->frames->term;
		context = work->frames->context;
		work->frames = work->frames->next;
		switch (term->rule) {
		case PG_RETURN_INTRO: {
			enum pg_totality totality;
			const struct pg_effect_row *effects;
			const struct pg_term *value;
			formation = pg_computation_type_view(term->classifier, &totality, &effects, &value)
				? pg_prove_computation_type(typing, classifiers, totality, effects, formation) : NULL;
			break;
		}
		case PG_THUNK_INTRO:
			formation = pg_prove_thunk_type(typing, classifiers, formation);
			break;
		case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION:
			formation = pg_prove_thunk_content(typing, formation);
			break;
		case PG_RETURN_VALUE: case PG_TOTAL_PURE_VALUE:
			formation = pg_prove_return_content(typing, formation);
			break;
		case PG_APP_ELIM:
			formation = pg_prove_pi_codomain(typing, formation,
				pg_prove_projection(typing, context, term->premises[1]));
			break;
		case PG_FOLD_ELIM: case PG_REQUEST_INTRO:
			formation = pg_prove_pi_constant_codomain(typing, formation);
			if (formation && formation->subject->core != term->classifier) {
				const struct pg_effect_row *effects;
				const struct pg_term *result_type;
				enum pg_totality totality;
				if (pg_computation_type_view(term->classifier, &totality, &effects, &result_type))
					formation = pg_prove_computation_type(typing, classifiers, totality, effects,
						pg_prove_return_content(typing, formation));
			}
			break;
		case PG_REINDEX:
			formation = pg_prove_reindex(typing, term->premises[0], formation);
			break;
		default:
			formation = NULL;
			break;
		}
		formation = pg_prove_projection(typing, context, formation);
	}
leaf:
	work->unwinding = 1;
	work->result = formation;
	if (!formation) work->status = -1;
	else if (!work->frames) work->status = 1;
}

int pg_classifier_recovery_advance(struct pg_classifier_recovery *work, size_t steps)
{
	while (!work->status && steps--) classifier_recovery_step(work);
	return work->status;
}

void pg_classifier_recovery_destroy(struct pg_classifier_recovery *work)
{
	pg_graph_destroy(&work->temporary);
	*work = (struct pg_classifier_recovery){0};
}

const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	struct pg_classifier_recovery work;
	pg_classifier_recovery_init(&work, typing, classifiers, context, term);
	while (!pg_classifier_recovery_advance(&work, 1024)) {}
	const struct pg_evidence *result = work.status > 0 ? work.result : NULL;
	pg_classifier_recovery_destroy(&work);
	return result;
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

enum pg_evidence_judgement pg_evidence_judgement(const struct pg_evidence *evidence) { return evidence->judgement; }
int pg_evidence_owned_by(const struct pg_evidence *evidence, const struct pg_typing *typing)
{
	return evidence && typing && typing->owner_key && evidence->owner == typing->owner_key;
}
const struct pg_context *pg_evidence_context(const struct pg_evidence *evidence) { return evidence->context; }
const struct pg_occurrence *pg_evidence_subject(const struct pg_evidence *evidence) { return evidence->subject; }
const struct pg_term *pg_evidence_classifier(const struct pg_evidence *evidence) { return evidence->classifier; }
size_t pg_evidence_premise_count(const struct pg_evidence *evidence) { return evidence->premise_count; }
const struct pg_evidence *pg_evidence_premise(const struct pg_evidence *evidence, size_t index)
{
	return index < evidence->premise_count ? evidence->premises[index] : NULL;
}
