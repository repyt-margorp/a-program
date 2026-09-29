#include "representation.h"
#include "classifier.h"
#include "host.h"

struct entry {
	struct pg_index_entry index;
	const struct pg_object *key;
	const struct pg_c_representation *representation;
};

const struct pg_c_representation *pg_c_representation_find(const struct pg_c_representations *table,
	const struct pg_object *object)
{
	static const struct pg_c_representation int32 = {.width = 32}, int64 = {.width = 64};
	if (object == pg_host_type("Int32")) return &int32;
	if (object == pg_host_type("Int64")) return &int64;
	for (struct pg_index_entry *i = pg_index_candidates(&table->lookup, (uintptr_t)object); i; i = i->next) {
		const struct entry *e = (const struct entry *)i;
		if (e->key == object) return e->representation;
	}
	return NULL;
}

static int record(struct pg_c_representations *table, struct pg_graph *storage,
	const struct pg_object *key, const struct pg_c_representation *representation)
{
	/* Erased layouts cannot choose between distinct selected nominal families. */
	if (pg_c_representation_find(table, key)) return -1;
	struct entry *e = pg_alloc(storage, sizeof(*e));
	if (!e) return -1;
	*e = (struct entry){.key = key, .representation = representation};
	return pg_index_insert(&table->lookup, &e->index, (uintptr_t)key);
}

int pg_c_representations_init(struct pg_c_representations *table, struct pg_graph *storage,
	size_t count, const struct pg_c_export *selections)
{
	if (pg_index_init(&table->lookup)) return -1;
	if (count > SIZE_MAX / sizeof(*table->enums)) return -1;
	if (count && !pg_c_export_names(count, selections)) return -1;
	table->count = count;
	table->enums = pg_alloc(storage, count * sizeof(*table->enums));
	if (!table->enums) return -1;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_occurrence *s = selections[i].subject;
		uint64_t level;
		if (!s || s->context || s->judgement != PG_JUDGEMENT_VALUE_TYPE || !s->core || s->core->kind != PG_REFERENCE) return -1;
		if (!pg_universe_level(s->classifier, &level)) return -1;
		const struct pg_data_declaration *d = pg_data_declaration_view(s->core->as.reference);
		if (!d) return -1;
		/* A closed selected type can retain its declaration's ambient prefix.
		 * Reject an index or field extension, not the prefix itself. */
		const struct pg_context *prefix = pg_data_declaration_parameters(d);
		if (pg_data_declaration_indices(d) != prefix) return -1;
		const struct pg_data_layout *layout = pg_data_declaration_layout(d);
		size_t n = pg_data_layout_count(layout);
		if (!n || (uint64_t)(n - 1) > UINT32_MAX) return -1;
		for (size_t j = 0; j < n; ++j) {
			const struct pg_data_layout *owner;
			size_t position, arity;
			if (pg_data_declaration_fields(d, j) != prefix) return -1;
			if (!pg_data_constructor_view(pg_data_constructor(layout, j), &owner, &position, &arity) || arity) return -1;
		}
		struct pg_c_representation *r = pg_alloc(storage, sizeof(*r));
		if (!r) return -1;
		*r = (struct pg_c_representation){.width = 32, .count = n, .alias = selections[i].alias, .layout = layout};
		table->enums[i] = r;
		if (record(table, storage, s->core->as.reference, r) || record(table, storage, pg_data_matcher(layout), r)) return -1;
	}
	return 0;
}

void pg_c_representations_destroy(struct pg_c_representations *table)
{
	pg_index_destroy(&table->lookup);
}

void pg_c_representation_type(FILE *out, const struct pg_c_representation *type)
{
	if (type->layout) fprintf(out, "struct ap_enum_%s", type->alias);
	else fprintf(out, "int%zu_t", type->width);
}

void pg_c_representation_declarations(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->enums[i];
		fprintf(out, "struct ap_enum_%s { uint32_t tag; };\n", r->alias);
		for (size_t j = 0; j < r->count; ++j)
			fprintf(out, "#define AP_ENUM_%s_C%zu UINT32_C(%zu)\n", r->alias, j, j);
	}
}
