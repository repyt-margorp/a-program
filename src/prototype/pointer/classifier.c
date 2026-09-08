#include "classifier.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

static const struct pg_object_class universe_class = {"universe"};
static const struct pg_object_class pi_class = {"pi-former"};
static const struct pg_object pi_former = {PG_SEMANTIC_OBJECT, &pi_class};
static const struct pg_object_class return_type_class = {"return-type-former"};
static const struct pg_object_class thunk_type_class = {"thunk-type-former"};
static const struct pg_object return_type_former = {PG_SEMANTIC_OBJECT, &return_type_class};
static const struct pg_object thunk_type_former = {PG_SEMANTIC_OBJECT, &thunk_type_class};

static const struct {
	const struct pg_object *object;
	const char *name;
} descriptors[] = {
	{&pi_former, "kernel/pi/v1"},
	{&return_type_former, "kernel/return-type/v1"},
	{&thunk_type_former, "kernel/thunk-type/v1"}
};

struct universe_object {
	struct pg_object object;
	uint64_t level;
};
struct universe_entry {
	struct pg_index_entry index;
	struct universe_object universe;
};

const char *pg_classifier_name(const struct pg_object *object, char *buffer, size_t capacity)
{
	if (!object || !buffer || !capacity) return NULL;
	if (object->owner == &universe_class) {
		int length = snprintf(buffer, capacity, "kernel/universe/%" PRIu64 "/v1",
			((const struct universe_object *)object)->level);
		return length >= 0 && (size_t)length < capacity ? buffer : NULL;
	}
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i) {
		if (descriptors[i].object != object) continue;
		size_t length = strlen(descriptors[i].name) + 1;
		if (length > capacity) return NULL;
		memcpy(buffer, descriptors[i].name, length);
		return buffer;
	}
	return NULL;
}

const struct pg_object *pg_classifier_resolve(struct pg_classifiers *classifiers, const char *name)
{
	if (!classifiers || !name) return NULL;
	for (size_t i = 0; i < sizeof(descriptors) / sizeof(*descriptors); ++i)
		if (!strcmp(name, descriptors[i].name)) return descriptors[i].object;
	static const char prefix[] = "kernel/universe/";
	if (strncmp(name, prefix, sizeof(prefix) - 1)) return NULL;
	const char *digits = name + sizeof(prefix) - 1;
	if (*digits < '0' || *digits > '9') return NULL;
	char *end;
	errno = 0;
	uintmax_t level = strtoumax(digits, &end, 10);
	if (errno == ERANGE || level > UINT64_MAX || strcmp(end, "/v1")) return NULL;
	if (*digits == '0' && end != digits + 1) return NULL;
	const struct pg_term *term = pg_universe(classifiers, (uint64_t)level);
	return term ? term->as.reference : NULL;
}

int pg_classifiers_init(struct pg_classifiers *classifiers, struct pg_graph *graph)
{
	memset(classifiers, 0, sizeof(*classifiers));
	classifiers->graph = graph;
	return pg_index_init(&classifiers->universes);
}

void pg_classifiers_destroy(struct pg_classifiers *classifiers)
{
	pg_index_destroy(&classifiers->universes);
	memset(classifiers, 0, sizeof(*classifiers));
}

const struct pg_term *pg_universe(struct pg_classifiers *classifiers, uint64_t level)
{
	uint64_t hash = level * UINT64_C(1099511628211);
	for (struct pg_index_entry *candidate = pg_index_candidates(&classifiers->universes, hash); candidate; candidate = candidate->next) {
		if (candidate->hash != hash) continue;
		struct universe_entry *entry = (struct universe_entry *)candidate;
		if (entry->universe.level == level) return pg_reference(classifiers->graph, &entry->universe.object);
	}
	struct universe_entry *entry = pg_alloc(classifiers->graph, sizeof(*entry));
	if (!entry) return NULL;
	entry->universe = (struct universe_object){{PG_SEMANTIC_OBJECT, &universe_class}, level};
	if (pg_index_insert(&classifiers->universes, &entry->index, hash) != 0) return NULL;
	return pg_reference(classifiers->graph, &entry->universe.object);
}

int pg_universe_level(const struct pg_term *term, uint64_t *level)
{
	if (!term || term->kind != PG_REFERENCE) return 0;
	const struct pg_object *object = term->as.reference;
	if (object->owner != &universe_class) return 0;
	*level = ((const struct universe_object *)object)->level;
	return 1;
}

const struct pg_term *pg_pi(struct pg_graph *graph,
	const struct pg_term *domain, const struct pg_object *binder, const struct pg_term *codomain)
{
	const struct pg_term *family = pg_lambda(graph, binder, codomain);
	if (!family) return NULL;
	const struct pg_term *head = pg_application(graph, pg_reference(graph, &pi_former), domain);
	return pg_application(graph, head, family);
}

int pg_pi_view(const struct pg_term *term, const struct pg_term **domain,
	const struct pg_object **binder, const struct pg_term **codomain)
{
	if (!term || term->kind != PG_APPLICATION) return 0;
	const struct pg_term *family = term->as.application.argument;
	const struct pg_term *head = term->as.application.function;
	if (family->kind != PG_LAMBDA) return 0;
	if (head->kind != PG_APPLICATION) return 0;
	const struct pg_term *former = head->as.application.function;
	if (former->kind != PG_REFERENCE) return 0;
	if (former->as.reference != &pi_former) return 0;
	*domain = head->as.application.argument;
	*binder = family->as.lambda.binder;
	*codomain = family->as.lambda.body;
	return 1;
}

static const struct pg_term *unary_type(struct pg_classifiers *classifiers,
	const struct pg_object *former, const struct pg_term *argument)
{
	return pg_application(classifiers->graph, pg_reference(classifiers->graph, former), argument);
}

static int unary_view(const struct pg_term *term, const struct pg_object *former, const struct pg_term **argument)
{
	if (!term || term->kind != PG_APPLICATION) return 0;
	const struct pg_term *head = term->as.application.function;
	if (head->kind != PG_REFERENCE || head->as.reference != former) return 0;
	*argument = term->as.application.argument;
	return 1;
}

const struct pg_term *pg_return_type(struct pg_classifiers *classifiers, const struct pg_term *value_type)
{
	return unary_type(classifiers, &return_type_former, value_type);
}
const struct pg_term *pg_thunk_type(struct pg_classifiers *classifiers, const struct pg_term *computation_type)
{
	return unary_type(classifiers, &thunk_type_former, computation_type);
}
int pg_return_type_view(const struct pg_term *term, const struct pg_term **value_type)
{
	return unary_view(term, &return_type_former, value_type);
}
int pg_thunk_type_view(const struct pg_term *term, const struct pg_term **computation_type)
{
	return unary_view(term, &thunk_type_former, computation_type);
}
