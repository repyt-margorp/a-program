#include "classifier.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>

static const struct pg_object_class universe_class = {"universe"};
static const struct pg_object_class pi_class = {"pi-former"};
static const struct pg_object pi_former = {PG_SEMANTIC_OBJECT, &pi_class};
static const struct pg_object_class return_type_class = {"return-type-former"};
static const struct pg_object_class thunk_type_class = {"thunk-type-former"};
static const struct pg_object return_type_former = {PG_SEMANTIC_OBJECT, &return_type_class};
static const struct pg_object thunk_type_former = {PG_SEMANTIC_OBJECT, &thunk_type_class};
static const struct pg_object_class termination_class = {"termination"};
static const struct pg_object termination_formers[] = {
	{PG_SEMANTIC_OBJECT, &termination_class}, {PG_SEMANTIC_OBJECT, &termination_class}
};
static const struct pg_object_class totality_class = {"computation-totality"};
static const struct pg_object totality_objects[] = {
	{PG_SEMANTIC_OBJECT, &totality_class}, {PG_SEMANTIC_OBJECT, &totality_class}
};
static const struct pg_object_class effect_row_class = {"closed-effect-row"};
static const struct pg_object_class effect_join_class = {"effect-row-union"};
static const struct pg_object effect_join = {PG_SEMANTIC_OBJECT, &effect_join_class};

struct pg_effect_row {
	struct pg_object_entry base;
	size_t count;
	const struct pg_object **labels;
};
static const struct pg_effect_row empty_effects = {
	.base.object = {PG_SEMANTIC_OBJECT, &effect_row_class}
};

static const struct {
	const struct pg_object *object;
	const char *name;
} descriptors[] = {
	{&pi_former, "kernel/pi/v1"},
	{&termination_formers[0], "kernel/termination-type/v1"},
	{&termination_formers[1], "kernel/termination-witness/v1"},
	{&effect_join, "solver/effect-union/v1"},
	{&return_type_former, "kernel/return-type/v3"},
	{&totality_objects[PG_TOTALITY_UNSPECIFIED], "kernel/totality/unspecified/v1"},
	{&totality_objects[PG_TOTALITY_TOTAL], "kernel/totality/total/v1"},
	{&empty_effects.base.object, "kernel/effect-row/empty/v1"},
	{&thunk_type_former, "kernel/thunk-type/v1"}
};

static int compare_effects(const void *left, const void *right)
{
	uintptr_t a = (uintptr_t)*(const struct pg_object *const *)left;
	uintptr_t b = (uintptr_t)*(const struct pg_object *const *)right;
	return a < b ? -1 : a != b;
}

static const struct pg_effect_row *intern_effects(struct pg_graph *graph,
	size_t count, const struct pg_object *const *labels)
{
	if (!count) return &empty_effects;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = UINT64_C(1469598103934665603) ^ count;
	for (size_t i = 0; i < count; ++i) hash = (hash ^ (uintptr_t)labels[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		const struct pg_object_entry *base = (const struct pg_object_entry *)p;
		if (base->object.owner != &effect_row_class) continue;
		const struct pg_effect_row *row = (const struct pg_effect_row *)base;
		if (row->count == count && !memcmp(row->labels, labels, count * sizeof(*labels))) return row;
	}
	struct pg_effect_row *row = pg_alloc(graph, sizeof(*row));
	if (!row) return NULL;
	row->labels = pg_alloc(graph, count * sizeof(*row->labels));
	if (!row->labels) return NULL;
	memcpy(row->labels, labels, count * sizeof(*labels));
	row->count = count;
	row->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &effect_row_class};
	return pg_index_insert(&graph->objects, &row->base.index, hash) ? NULL : row;
}

const struct pg_effect_row *pg_effect_row(struct pg_graph *graph,
	size_t count, const struct pg_object *const *labels)
{
	if (!graph || (count && !labels) || count > SIZE_MAX / sizeof(*labels)) return NULL;
	if (!count) return &empty_effects;
	const struct pg_object **sorted = malloc(count * sizeof(*sorted));
	if (!sorted) return NULL;
	const struct pg_effect_row *result = NULL;
	for (size_t i = 0; i < count; ++i) {
		if (!labels[i] || labels[i]->kind != PG_SEMANTIC_OBJECT) goto done;
		sorted[i] = labels[i];
	}
	qsort(sorted, count, sizeof(*sorted), compare_effects);
	size_t unique = 1;
	for (size_t i = 1; i < count; ++i) if (sorted[i] != sorted[unique - 1]) sorted[unique++] = sorted[i];
	result = intern_effects(graph, unique, sorted);
done:
	free(sorted);
	return result;
}

const struct pg_effect_row *pg_effect_union(struct pg_graph *graph,
	const struct pg_effect_row *left, const struct pg_effect_row *right)
{
	if (!graph || !left || !right) return NULL;
	if (left == right) return left;
	if (!left->count) return right;
	if (!right->count) return left;
	if (left->count > SIZE_MAX - right->count) return NULL;
	size_t capacity = left->count + right->count;
	if (capacity > SIZE_MAX / sizeof(const struct pg_object *)) return NULL;
	const struct pg_object **labels = malloc(capacity * sizeof(*labels));
	if (!labels) return NULL;
	size_t i = 0, j = 0, count = 0;
	while (i < left->count && j < right->count) {
		const struct pg_object *a = left->labels[i], *b = right->labels[j];
		if (a == b) { labels[count++] = a; ++i; ++j; }
		else if ((uintptr_t)a < (uintptr_t)b) { labels[count++] = a; ++i; }
		else { labels[count++] = b; ++j; }
	}
	while (i < left->count) labels[count++] = left->labels[i++];
	while (j < right->count) labels[count++] = right->labels[j++];
	const struct pg_effect_row *result = intern_effects(graph, count, labels);
	free(labels);
	return result;
}

const struct pg_effect_row *pg_effect_difference(struct pg_graph *graph,
	const struct pg_effect_row *left, const struct pg_effect_row *right)
{
	if (!graph || !left || !right) return NULL;
	if (left == right) return pg_effect_row(graph, 0, NULL);
	if (!right->count || !left->count) return left;
	const struct pg_object **labels = malloc(left->count * sizeof(*labels));
	if (!labels) return NULL;
	size_t i = 0, j = 0, count = 0;
	while (i < left->count) {
		while (j < right->count && (uintptr_t)right->labels[j] < (uintptr_t)left->labels[i]) ++j;
		if (j == right->count || right->labels[j] != left->labels[i]) labels[count++] = left->labels[i];
		++i;
	}
	const struct pg_effect_row *result = intern_effects(graph, count, labels);
	free(labels);
	return result;
}

size_t pg_effect_count(const struct pg_effect_row *row) { return row ? row->count : SIZE_MAX; }
const struct pg_object *pg_effect_label(const struct pg_effect_row *row, size_t index)
{
	return row && index < row->count ? row->labels[index] : NULL;
}

int pg_effect_subset(const struct pg_effect_row *left, const struct pg_effect_row *right)
{
	if (!left || !right) return -1;
	if (left == right) return 1;
	if (left->count > right->count) return 0;
	size_t i = 0, j = 0;
	while (i < left->count && j < right->count) {
		if (left->labels[i] == right->labels[j]) { ++i; ++j; }
		else if ((uintptr_t)left->labels[i] < (uintptr_t)right->labels[j]) return 0;
		else ++j;
	}
	return i == left->count;
}

int pg_effect_contains(const struct pg_effect_row *row, const struct pg_object *label)
{
	if (!row || !label) return -1;
	size_t low = 0, high = row->count;
	while (low < high) {
		size_t middle = low + (high - low) / 2;
		if (row->labels[middle] == label) return 1;
		if ((uintptr_t)row->labels[middle] < (uintptr_t)label) low = middle + 1;
		else high = middle;
	}
	return 0;
}

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

const struct pg_term *pg_pi_constant_codomain(const struct pg_term *pi)
{
	const struct pg_term *domain, *codomain;
	const struct pg_object *binder;
	if (!pg_pi_view(pi, &domain, &binder, &codomain)) return NULL;
	if (pg_term_independent(codomain, binder) != 1) return NULL;
	return codomain;
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
	return pg_effect_type(classifiers, &empty_effects, value_type);
}

const struct pg_term *pg_termination_type(struct pg_classifiers *classifiers, const struct pg_term *suspended)
{
	return classifiers && suspended ? unary_type(classifiers, &termination_formers[0], suspended) : NULL;
}

const struct pg_term *pg_termination_witness(struct pg_classifiers *classifiers, const struct pg_term *suspended)
{
	return classifiers && suspended ? unary_type(classifiers, &termination_formers[1], suspended) : NULL;
}

int pg_termination_type_view(const struct pg_term *term, const struct pg_term **suspended)
{
	return suspended && unary_view(term, &termination_formers[0], suspended);
}

const struct pg_term *pg_effect_type(struct pg_classifiers *classifiers,
	const struct pg_effect_row *effects, const struct pg_term *value_type)
{
	return pg_computation_type(classifiers, PG_TOTALITY_UNSPECIFIED, effects, value_type);
}

const struct pg_term *pg_effect_type_spine(struct pg_classifiers *classifiers,
	const struct pg_term *effects, const struct pg_term *value_type)
{
	return pg_computation_type_spine(classifiers, PG_TOTALITY_UNSPECIFIED, effects, value_type);
}

const struct pg_term *pg_computation_type(struct pg_classifiers *classifiers,
	enum pg_totality totality, const struct pg_effect_row *effects, const struct pg_term *value_type)
{
	if (!classifiers) return NULL;
	return pg_computation_type_spine(classifiers, totality,
		pg_effect_reference(classifiers->graph, effects), value_type);
}

const struct pg_term *pg_computation_type_spine(struct pg_classifiers *classifiers,
	enum pg_totality totality, const struct pg_term *effects, const struct pg_term *value_type)
{
	if (!classifiers || !effects || !value_type || (unsigned)totality > PG_TOTALITY_TOTAL) return NULL;
	const struct pg_term *grade = pg_reference(classifiers->graph, &totality_objects[totality]);
	const struct pg_term *head = unary_type(classifiers, &return_type_former, grade);
	head = pg_application(classifiers->graph, head, effects);
	return pg_application(classifiers->graph, head, value_type);
}

const struct pg_term *pg_effect_reference(struct pg_graph *graph, const struct pg_effect_row *row)
{
	return graph && row ? pg_reference(graph, &row->base.object) : NULL;
}

const struct pg_effect_row *pg_effect_row_view(const struct pg_term *term)
{
	if (!term || term->kind != PG_REFERENCE || term->as.reference->owner != &effect_row_class) return NULL;
	return (const struct pg_effect_row *)((const char *)term->as.reference - offsetof(struct pg_object_entry, object));
}

int pg_effect_type_spine_view(const struct pg_term *term,
	const struct pg_term **effects, const struct pg_term **value_type)
{
	enum pg_totality totality;
	const struct pg_term *row, *value;
	if (!effects || !value_type || !pg_computation_type_spine_view(term, &totality, &row, &value)) return 0;
	if (totality != PG_TOTALITY_UNSPECIFIED) return 0;
	*effects = row; *value_type = value;
	return 1;
}

int pg_computation_type_spine_view(const struct pg_term *term,
	enum pg_totality *totality, const struct pg_term **effects, const struct pg_term **value_type)
{
	if (!term || !totality || !effects || !value_type || term->kind != PG_APPLICATION) return 0;
	const struct pg_term *row = term->as.application.function, *grade;
	if (row->kind != PG_APPLICATION || !unary_view(row->as.application.function, &return_type_former, &grade)) return 0;
	if (grade->kind != PG_REFERENCE) return 0;
	if (grade->as.reference == &totality_objects[PG_TOTALITY_UNSPECIFIED]) *totality = PG_TOTALITY_UNSPECIFIED;
	else if (grade->as.reference == &totality_objects[PG_TOTALITY_TOTAL]) *totality = PG_TOTALITY_TOTAL;
	else return 0;
	*effects = row->as.application.argument;
	*value_type = term->as.application.argument;
	return 1;
}

const struct pg_term *pg_effect_join_term(struct pg_graph *graph,
	const struct pg_term *left, const struct pg_term *right)
{
	if (!graph || !left || !right) return NULL;
	return pg_application(graph, pg_application(graph, pg_reference(graph, &effect_join), left), right);
}

int pg_effect_join_view(const struct pg_term *term,
	const struct pg_term **left, const struct pg_term **right)
{
	if (!term || !left || !right || term->kind != PG_APPLICATION) return 0;
	if (!unary_view(term->as.application.function, &effect_join, left)) return 0;
	*right = term->as.application.argument;
	return 1;
}

int pg_effect_type_view(const struct pg_term *term,
	const struct pg_effect_row **effects, const struct pg_term **value_type)
{
	enum pg_totality totality;
	const struct pg_effect_row *row;
	const struct pg_term *value;
	if (!effects || !value_type || !pg_computation_type_view(term, &totality, &row, &value)) return 0;
	if (totality != PG_TOTALITY_UNSPECIFIED) return 0;
	*effects = row; *value_type = value;
	return 1;
}

int pg_computation_type_view(const struct pg_term *term,
	enum pg_totality *totality, const struct pg_effect_row **effects, const struct pg_term **value_type)
{
	enum pg_totality grade;
	const struct pg_term *row, *value;
	if (!totality || !effects || !value_type || !pg_computation_type_spine_view(term, &grade, &row, &value)) return 0;
	const struct pg_effect_row *found = pg_effect_row_view(row);
	if (!found) return 0;
	*totality = grade;
	*effects = found;
	*value_type = value;
	return 1;
}
const struct pg_term *pg_thunk_type(struct pg_classifiers *classifiers, const struct pg_term *computation_type)
{
	return unary_type(classifiers, &thunk_type_former, computation_type);
}
int pg_pure_computation_type_view(const struct pg_term *term,
	enum pg_totality *totality, const struct pg_term **value_type)
{
	enum pg_totality grade;
	const struct pg_effect_row *effects;
	const struct pg_term *value;
	if (!totality || !value_type || !pg_computation_type_view(term, &grade, &effects, &value)) return 0;
	if (effects->count) return 0;
	*totality = grade; *value_type = value;
	return 1;
}
int pg_return_type_view(const struct pg_term *term, const struct pg_term **value_type)
{
	enum pg_totality grade;
	const struct pg_term *value;
	if (!value_type || !pg_pure_computation_type_view(term, &grade, &value)) return 0;
	if (grade != PG_TOTALITY_UNSPECIFIED) return 0;
	*value_type = value;
	return 1;
}
int pg_thunk_type_view(const struct pg_term *term, const struct pg_term **computation_type)
{
	return unary_view(term, &thunk_type_former, computation_type);
}
