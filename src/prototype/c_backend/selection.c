#include "selection.h"
#include "classifier.h"

const struct pg_occurrence *pg_c_value_classifier(const struct pg_occurrence *value)
{
	if (!value || value->context || value->judgement != PG_JUDGEMENT_VALUE || !value->core ||
		!value->classifier || !value->type || !value->type->core ||
		value->type->context || value->type->judgement != PG_JUDGEMENT_VALUE_TYPE ||
		value->classifier != value->type->core) return NULL;
	return value->type;
}

int pg_c_applied_selection_read(struct pg_c_applied_selection *out, struct pg_graph *storage,
	const struct pg_c_representations *representations, const struct pg_occurrence *type)
{
	uint64_t level;
	if (!out || !storage || !representations || !type || type->context ||
		type->judgement != PG_JUDGEMENT_VALUE_TYPE || !type->core ||
		!pg_universe_level(type->classifier, &level)) return -1;
	const struct pg_term *head = type->core;
	size_t count = 0;
	while (head->kind == PG_APPLICATION) {
		if (count == SIZE_MAX) return -1;
		++count;
		head = head->as.application.function;
	}
	if (!count || head->kind != PG_REFERENCE) return -1;
	const struct pg_data_declaration *declaration = pg_data_declaration_view(head->as.reference);
	if (!declaration) return -1;
	const struct pg_context *prefix = pg_data_declaration_parameters(declaration);
	if (!prefix || pg_data_declaration_indices(declaration) != prefix ||
		!pg_universe_level(prefix->declared_type, &level)) return -1;
	size_t parameters;
	if (pg_context_extension_size(prefix->parent, NULL, &parameters) || parameters != count ||
		count > SIZE_MAX / sizeof(const struct pg_c_representation *)) return -1;
	const struct pg_c_representation **arguments = pg_alloc(storage, count * sizeof(*arguments));
	if (!arguments) return -1;
	const struct pg_term *application = type->core;
	for (size_t i = count; i; --i) {
		const struct pg_term *argument = application->as.application.argument;
		if (argument->kind != PG_REFERENCE ||
			!(arguments[i - 1] = pg_c_representation_find(representations, argument->as.reference))) return -1;
		application = application->as.application.function;
	}
	*out = (struct pg_c_applied_selection){.declaration = declaration, .type = type->core,
		.parameters = prefix->parent, .count = count, .arguments = arguments};
	return 0;
}

const struct pg_c_representation *pg_c_applied_parameter(
	const struct pg_c_applied_selection *selection, const struct pg_object *binder)
{
	if (!selection || !binder) return NULL;
	const struct pg_context *parameter = selection->parameters;
	for (size_t i = selection->count; i && parameter; --i, parameter = parameter->parent)
		if (parameter->binder == binder) return selection->arguments[i - 1];
	return NULL;
}
