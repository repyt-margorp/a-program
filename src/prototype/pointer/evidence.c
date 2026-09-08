#include "evidence.h"
#include "computation.h"
#include "eval.h"
#include "identity.h"
#include "iadt.h"
#include <stdlib.h>

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

static int derived_output(enum pg_evidence_rule rule)
{
	switch (rule) {
	case PG_REINDEX: case PG_APP_ELIM: case PG_PI_CODOMAIN:
	case PG_FOLD_ELIM: case PG_PI_CONSTANT_CODOMAIN:
	case PG_FAMILY_IDENTITY_FORM: case PG_FAMILY_ACTION:
	case PG_INDUCTIVE_FORM: case PG_CONSTRUCTOR_INTRO:
		return 1;
	default: return 0;
	}
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
	hash = (hash ^ (uintptr_t)certificate ^ judgement) * UINT64_C(1099511628211);
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
		if (proof->certificate != certificate) continue;
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

const struct pg_evidence *pg_prove_inductive_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_data_schema *schema)
{
	const struct pg_evidence *self = pg_data_schema_parameters(schema);
	if (!context_proof(typing, self) || self->rule != PG_CONTEXT_EXTEND) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	const struct pg_evidence *indices = pg_data_schema_indices(schema);
	if (!context_proof(typing, indices) || indices->context != self->context) return NULL;
	uint64_t level, bound;
	if (!pg_universe_level(self->context->declared_type, &level)) return NULL;
	const struct pg_evidence *parent = self->premises[0];
	size_t count = pg_data_constructor_count(schema), parameters;
	if (count > SIZE_MAX / sizeof(void *) - 1) return NULL;
	if (pg_context_extension_size(parent->context, NULL, &parameters)) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **premises = pg_alloc(&temporary, (count + 1) * sizeof(*premises));
	if (!premises) goto done;
	premises[0] = self;
	for (size_t i = 0; i < count; ++i)
		premises[i + 1] = pg_data_schema_result(schema,
			pg_data_constructor(pg_data_schema_layout(schema), i));
	uint64_t hash;
	result = find_record(typing, PG_INDUCTIVE_FORM, PG_JUDGEMENT_VALUE_TYPE,
		parent->context, NULL, NULL, count + 1, premises, schema, &hash);
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
	const struct pg_term *universe = pg_universe(classifiers, level);
	if (!core || !universe) goto done;
	const struct pg_occurrence *subject = pg_occurrence(typing, parent->context, core, NULL, 0, NULL);
	if (!subject) goto done;
	result = accept_record(typing, PG_INDUCTIVE_FORM, PG_JUDGEMENT_VALUE_TYPE,
		parent->context, subject, universe, count + 1, premises, schema, 0, NULL);
done:
	pg_graph_destroy(&temporary);
	return result;
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
	const struct pg_evidence *self = pg_prove_type_value(typing, family);
	if (!self) return NULL;
	const struct pg_evidence *extended = pg_prove_substitution_extend(typing, parameters,
		formation->premises[0], 1, &self);
	const struct pg_evidence *instance = pg_data_instance(typing, schema, constructor, extended, count, fields);
	if (!instance) return NULL;
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

const struct pg_evidence *pg_prove_constructor_function(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *formation,
	const struct pg_object *constructor, const struct pg_evidence *parameters)
{
	if (!pg_evidence_owned_by(formation, typing) || formation->rule != PG_INDUCTIVE_FORM) return NULL;
	if (!pg_evidence_owned_by(parameters, typing) || parameters->rule != PG_CONTEXT_SUBSTITUTION) return NULL;
	if (!classifiers || classifiers->graph != typing->graph) return NULL;
	if (parameters->premises[0]->context != formation->context) return NULL;
	const struct pg_data_schema *schema = formation->certificate;
	const struct pg_evidence *fields = pg_data_schema_fields(schema, constructor);
	if (!fields) return NULL;
	const struct pg_evidence *self_context = formation->premises[0];
	size_t count;
	if (pg_context_extension_size(fields->context, self_context->context, &count)) return NULL;
	if (count > SIZE_MAX / sizeof(const struct pg_evidence *)) return NULL;
	const struct pg_evidence *family = pg_prove_reindex(typing, parameters, formation);
	const struct pg_evidence *self = pg_prove_type_value(typing, family);
	if (!self) return NULL;
	const struct pg_evidence *map = pg_prove_substitution_extend(typing, parameters, self_context, 1, &self);
	if (!map) return NULL;
	struct pg_graph temporary = {0};
	const struct pg_evidence *result = NULL;
	const struct pg_evidence **extensions = pg_alloc(&temporary, count * sizeof(*extensions));
	if (count && !extensions) goto done;
	for (size_t i = count; i; --i, fields = fields->premises[0]) extensions[i - 1] = fields;
	for (size_t i = 0; i < count; ++i) {
		map = pg_prove_substitution_lift(typing, map, extensions[i], pg_binder(typing->graph));
		if (!map) goto done;
	}
	size_t prefix = parameters->premise_count - 2;
	const struct pg_evidence *context = map->premises[1];
	const struct pg_evidence *arguments = pg_prove_substitution(typing,
		parameters->premises[0], context, prefix, map->premises + 2);
	const struct pg_evidence *body = pg_prove_constructor(typing, formation, constructor,
		arguments, count, map->premises + prefix + 3);
	body = pg_prove_return(typing, classifiers, body);
	if (!body) goto done;
	result = pg_prove_abstract(typing, classifiers, parameters->premises[1], context, body);
done:
	pg_graph_destroy(&temporary);
	return result;
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

const struct pg_evidence *pg_prove_variable(struct pg_typing *typing,
	const struct pg_evidence *context, const struct pg_object *binder)
{
	if (!context_proof(typing, context)) return NULL;
	const struct pg_context *declaration = pg_context_lookup(context->context, binder);
	if (!declaration) return NULL;
	const struct pg_term *term = pg_reference(typing->graph, binder);
	const struct pg_occurrence *subject = pg_occurrence(typing, context->context, term, NULL, 0, NULL);
	if (!subject) return NULL;
	return accept(typing, PG_VARIABLE, PG_JUDGEMENT_VALUE, context->context, subject, declaration->declared_type, 1, &context);
}

static const struct pg_evidence *unary_formation(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *argument,
	enum pg_evidence_rule rule)
{
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	enum pg_evidence_judgement output;
	const struct pg_term *term;
	if (rule == PG_RETURN_TYPE_FORM) {
		argument = pg_prove_value_type(typing, argument);
		if (!argument) return NULL;
		output = PG_JUDGEMENT_COMPUTATION_TYPE;
		term = pg_return_type(classifiers, argument->subject->core);
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

const struct pg_evidence *pg_prove_return_type(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *value_type)
{
	return unary_formation(typing, classifiers, value_type, PG_RETURN_TYPE_FORM);
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
	return unary_formation(typing, classifiers, computation_type, PG_THUNK_TYPE_FORM);
}

const struct pg_evidence *pg_prove_pi(struct pg_typing *typing, struct pg_classifiers *classifiers,
	const struct pg_evidence *domain, const struct pg_evidence *extended_context,
	const struct pg_evidence *codomain)
{
	if (classifiers->graph != typing->graph) return NULL;
	domain = pg_prove_value_type(typing, domain);
	if (!domain) return NULL;
	if (!context_proof(typing, extended_context)) return NULL;
	const struct pg_context *scope = extended_context->context;
	if (!scope || scope->parent != domain->context) return NULL;
	if (scope->declared_type != domain->subject->core) return NULL;
	if (!pg_evidence_owned_by(codomain, typing)) return NULL;
	if (codomain->judgement != PG_JUDGEMENT_COMPUTATION_TYPE) return NULL;
	if (codomain->context != scope) return NULL;
	uint64_t left, right;
	if (!pg_universe_level(domain->classifier, &left)) return NULL;
	if (!pg_universe_level(codomain->classifier, &right)) return NULL;
	const struct pg_term *bound = pg_universe(classifiers, left > right ? left : right);
	const struct pg_term *term = pg_pi(typing->graph, domain->subject->core,
		scope->binder, codomain->subject->core);
	if (!bound || !term) return NULL;
	const struct pg_occurrence *operands[] = {domain->subject, codomain->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, domain->context, term, NULL, 2, operands);
	if (!subject) return NULL;
	const struct pg_evidence *premises[] = {domain, extended_context, codomain};
	return accept(typing, PG_PI_FORM, PG_JUDGEMENT_COMPUTATION_TYPE,
		domain->context, subject, bound, 3, premises);
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
	if (!pg_evidence_owned_by(value, typing)) return NULL;
	if (value->judgement != PG_JUDGEMENT_VALUE) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	return unary_term(typing, value, &pg_return_operation,
		pg_return_type(classifiers, value->classifier), PG_RETURN_INTRO, PG_JUDGEMENT_COMPUTATION);
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
	if (body->context != pi->premises[1]->context) return NULL;
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
		if (context->rule != PG_CONTEXT_EXTEND) return NULL;
		type = pg_prove_pi(typing, classifiers, context->premises[1], context, type);
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
	if (!pg_return_type_view(computation->classifier, &classifier)) return NULL;
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


const struct pg_evidence *pg_prove_application(struct pg_typing *typing,
	const struct pg_evidence *function, const struct pg_evidence *argument)
{
	if (!pg_evidence_owned_by(function, typing)) return NULL;
	if (!pg_evidence_owned_by(argument, typing)) return NULL;
	if (function->judgement != PG_JUDGEMENT_COMPUTATION) return NULL;
	if (argument->judgement != PG_JUDGEMENT_VALUE) return NULL;
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
	const struct pg_term *classifier = pg_term_substitute(typing->graph, codomain, 1, &substitution);
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
	case PG_JUDGEMENT_VALUE_TYPE: case PG_JUDGEMENT_COMPUTATION_TYPE: break;
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
	const struct pg_context **declarations = pg_alloc(&temporary, count * sizeof(*declarations));
	struct pg_binding_value *bindings = pg_alloc(&temporary, total * sizeof(*bindings));
	const struct pg_evidence **premises = pg_alloc(&temporary, (total + 2) * sizeof(*premises));
	if (!premises) goto done;
	if (count && !declarations) goto done;
	if (total && !bindings) goto done;
	const struct pg_context *scope = source->context;
	for (size_t i = count; i; --i) { declarations[i - 1] = scope; scope = scope->parent; }
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
		if (image->judgement != PG_JUDGEMENT_VALUE) goto done;
		if (image->context != destination->context) goto done;
		premises[retained + i + 2] = image;
	}
	uint64_t hash;
	result = find_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, total + 2, premises, NULL, &hash);
	if (result) goto done;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_evidence *image = images[i];
		const struct pg_term *expected = pg_term_substitute(typing->graph, declarations[i]->declared_type, retained + i, bindings);
		if (!expected) goto done;
		if (pg_alpha_equal(expected, image->classifier) != 1) goto done;
		bindings[retained + i] = (struct pg_binding_value){declarations[i]->binder, image->subject->core};
	}
	result = accept_record(typing, PG_CONTEXT_SUBSTITUTION, PG_JUDGEMENT_SUBSTITUTION,
		destination->context, NULL, NULL, total + 2, premises, NULL, total, bindings);
done:
	pg_graph_destroy(&temporary);
	return result;
}

const struct pg_evidence *pg_prove_substitution(struct pg_typing *typing,
	const struct pg_evidence *source, const struct pg_evidence *destination,
	size_t count, const struct pg_evidence *const *images)
{
	return substitution_build(typing, source, destination, NULL, count, images);
}

struct pg_reindex_state {
	struct pg_typing *typing;
	const struct pg_evidence *premises[2];
	const struct pg_term *inputs[3], *outputs[3];
	struct pg_substitution substitution;
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
		if (!state->substitution.state) {
			size_t count = substitution->premise_count - 2;
			const struct pg_binding_value *bindings = (const struct pg_binding_value *)(substitution->premises + substitution->premise_count);
			if (pg_substitution_init(&state->substitution, typing->graph,
				state->inputs[state->next], count, bindings) != 0) return PG_REINDEX_ERROR;
			return PG_REINDEX_PENDING;
		}
		switch (pg_substitution_advance(&state->substitution, 1)) {
		case PG_SUBSTITUTION_PENDING: return PG_REINDEX_PENDING;
		case PG_SUBSTITUTION_ERROR: return PG_REINDEX_ERROR;
		case PG_SUBSTITUTION_DONE:
			state->outputs[state->next++] = pg_substitution_result(&state->substitution);
			pg_substitution_destroy(&state->substitution);
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
	pg_substitution_destroy(&work->state->substitution);
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
	pg_substitution_destroy(&state.substitution);
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
	abstraction = pg_term_substitute(typing->graph, abstraction, common, bindings);
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
	if (source_extension->rule != PG_CONTEXT_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	if (!context_proof(typing, destination)) return NULL;
	if (!pg_evidence_owned_by(image, typing)) return NULL;
	if (image->judgement != PG_JUDGEMENT_VALUE) return NULL;
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
	const struct pg_evidence *expected = pg_prove_reindex(typing, substitution, source_extension->premises[1]);
	if (!expected) goto done;
	if (pg_alpha_equal(expected->subject->core, image->classifier) != 1) goto done;
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

const struct pg_evidence *pg_prove_substitution_lift(struct pg_typing *typing,
	const struct pg_evidence *substitution, const struct pg_evidence *source_extension,
	const struct pg_object *binder)
{
	if (!substitution_proof(typing, substitution)) return NULL;
	if (!context_proof(typing, source_extension)) return NULL;
	if (source_extension->rule != PG_CONTEXT_EXTEND) return NULL;
	if (source_extension->context->parent != substitution->premises[0]->context) return NULL;
	const struct pg_evidence *domain = pg_prove_reindex(typing, substitution, source_extension->premises[1]);
	const struct pg_evidence *destination = pg_prove_context_extension(typing, substitution->premises[1], binder, domain);
	if (!destination) return NULL;
	const struct pg_evidence *image = pg_prove_variable(typing, destination, binder);
	return substitution_pair(typing, substitution, source_extension, destination, image);
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
	if (!pg_return_type_view(return_type->subject->core, &content)) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, return_type->context, content,
		NULL, 1, &return_type->subject);
	if (!subject) return NULL;
	return accept(typing, PG_RETURN_CONTENT, PG_JUDGEMENT_VALUE_TYPE,
		return_type->context, subject, return_type->classifier, 1, &return_type);
}

static const struct pg_term *constant_codomain(const struct pg_term *pi)
{
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi, &domain, &binder, &codomain)) return NULL;
	if (pg_term_independent(codomain, binder) != 1) return NULL;
	return codomain;
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
	const struct pg_term *codomain = constant_codomain(pi->subject->core);
	if (!codomain) return NULL;
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, codomain, NULL, 1, &pi->subject);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CONSTANT_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 1, &pi);
}

const struct pg_evidence *pg_prove_fold(struct pg_typing *typing,
	const struct pg_evidence *computation, const struct pg_evidence *continuation)
{
	if (!pg_evidence_owned_by(computation, typing)) return NULL;
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
	if (!pg_return_type_view(computation->classifier, &value_type)) return NULL;
	if (!pg_pi_view(continuation->classifier, &domain, &binder, &codomain)) return NULL;
	if (pg_alpha_equal(domain, value_type) != 1) return NULL;
	codomain = constant_codomain(continuation->classifier);
	if (!codomain) return NULL;
	const struct pg_term *head = pg_application(typing->graph,
		pg_reference(typing->graph, &pg_fold_operation), computation->subject->core);
	const struct pg_term *core = pg_application(typing->graph, head, continuation->subject->core);
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
	if (argument->judgement != PG_JUDGEMENT_VALUE) return NULL;
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
	const struct pg_term *type = pg_term_substitute(typing->graph, codomain, 1, &binding);
	if (!type) return NULL;
	const struct pg_occurrence *operands[] = {pi->subject, argument->subject};
	const struct pg_occurrence *subject = pg_occurrence(typing, pi->context, type, NULL, 2, operands);
	if (!subject) return NULL;
	return accept(typing, PG_PI_CODOMAIN, PG_JUDGEMENT_COMPUTATION_TYPE,
		pi->context, subject, pi->classifier, 2, premises);
}

const struct pg_evidence *pg_prove_classifier(struct pg_typing *typing,
	struct pg_classifiers *classifiers, const struct pg_evidence *context,
	const struct pg_evidence *term)
{
	if (!context_proof(typing, context)) return NULL;
	if (classifiers->graph != typing->graph) return NULL;
	if (!pg_evidence_owned_by(term, typing)) return NULL;
	if (context->context != term->context) return NULL;
	/* These steps retain the classifier. Inspect their source without copying
	 * the DAG, then project the recovered formation to the requested context. */
	for (;;) {
		if (term->rule == PG_CONTEXT_PROJECTION) term = term->premises[1];
		else if (term->rule == PG_PURE_NORMALIZATION) term = term->premises[0];
		else break;
	}
	const struct pg_evidence *formation = NULL;
	switch (term->rule) {
	case PG_REFLEXIVITY: case PG_FAMILY_ACTION:
	case PG_CONSTRUCTOR_INTRO:
	case PG_IDENTITY_TRANSPORT: case PG_IDENTITY_LIFT:
		formation = term->premises[0];
		break;
	case PG_VARIABLE: {
		const struct pg_evidence *declaration = term->premises[0];
		const struct pg_object *binder = term->subject->core->as.reference;
		while (declaration->context->binder != binder) declaration = declaration->premises[0];
		formation = declaration->premises[1];
		break;
	}
	case PG_VALUE_FROM_TYPE: {
		uint64_t level;
		if (!pg_universe_level(term->classifier, &level)) return NULL;
		return pg_prove_universe(typing, classifiers, context, level);
	}
	case PG_RETURN_INTRO:
	case PG_THUNK_INTRO: {
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[0]);
		formation = pg_prove_classifier(typing, classifiers, context, argument);
		if (term->rule == PG_RETURN_INTRO) return pg_prove_return_type(typing, classifiers, formation);
		return pg_prove_thunk_type(typing, classifiers, formation);
	}
	case PG_FORCE_ELIM: case PG_THUNK_COMPUTATION: case PG_RETURN_VALUE: {
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[0]);
		formation = pg_prove_classifier(typing, classifiers, context, argument);
		formation = term->rule == PG_RETURN_VALUE ? pg_prove_return_content(typing, formation)
			: pg_prove_thunk_content(typing, formation);
		break;
	}
	case PG_LAMBDA_INTRO:
		formation = term->premises[0];
		break;
	case PG_APP_ELIM: {
		const struct pg_evidence *function = pg_prove_projection(typing, context, term->premises[0]);
		const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, context, function);
		const struct pg_evidence *argument = pg_prove_projection(typing, context, term->premises[1]);
		formation = pg_prove_pi_codomain(typing, pi, argument);
		break;
	}
	case PG_TYPE_CONVERSION:
		formation = term->premises[1];
		break;
	case PG_FOLD_ELIM: {
		const struct pg_evidence *continuation = pg_prove_projection(typing, context, term->premises[1]);
		const struct pg_evidence *pi = pg_prove_classifier(typing, classifiers, context, continuation);
		formation = pg_prove_pi_constant_codomain(typing, pi);
		break;
	}
	case PG_REINDEX: {
		const struct pg_evidence *substitution = term->premises[0];
		formation = pg_prove_classifier(typing, classifiers, substitution->premises[0], term->premises[1]);
		formation = pg_prove_reindex(typing, substitution, formation);
		break;
	}
	default:
		return NULL;
	}
	return pg_prove_projection(typing, context, formation);
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
