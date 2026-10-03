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
	size_t enum_count, const struct pg_c_export *enums,
	size_t data_count, const struct pg_c_export *data)
{
	if (pg_index_init(&table->lookup)) return -1;
	if (data_count > SIZE_MAX - enum_count) return -1;
	size_t count = enum_count + data_count;
	if (count > SIZE_MAX / sizeof(*table->types)) return -1;
	if (enum_count && !pg_c_export_names(enum_count, enums)) return -1;
	if (data_count && !pg_c_export_names(data_count, data)) return -1;
	table->count = count;
	table->types = pg_alloc(storage, count * sizeof(*table->types));
	if (!table->types) return -1;
	for (size_t i = 0; i < count; ++i) {
		const struct pg_c_export *selection = i < enum_count ? &enums[i] : &data[i - enum_count];
		const struct pg_occurrence *s = selection->subject;
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
		for (size_t j = 0; j < n && i < enum_count; ++j) {
			const struct pg_data_layout *owner;
			size_t position, arity;
			if (pg_data_declaration_fields(d, j) != prefix) return -1;
			if (!pg_data_constructor_view(pg_data_constructor(layout, j), &owner, &position, &arity) || arity) return -1;
		}
		struct pg_c_representation *r = pg_alloc(storage, sizeof(*r));
		if (!r) return -1;
		*r = (struct pg_c_representation){.width = 32, .count = n, .alias = selection->alias, .layout = layout};
		table->types[i] = r;
		if (record(table, storage, s->core->as.reference, r) || record(table, storage, pg_data_matcher(layout), r)) return -1;
		if (i < enum_count) continue;
		if (n > SIZE_MAX / sizeof(*r->constructors)) return -1;
		r->constructors = pg_alloc(storage, n * sizeof(*r->constructors));
		if (!r->constructors) return -1;
		for (size_t j = 0; j < n; ++j) {
			struct pg_c_constructor_representation *c = &r->constructors[j];
			const struct pg_context *fields = pg_data_declaration_fields(d, j);
			const struct pg_data_layout *owner;
			size_t position, arity;
			if (pg_context_extension_size(fields, prefix, &c->count) ||
				!pg_data_constructor_view(pg_data_constructor(layout, j), &owner, &position, &arity) ||
				arity != c->count || c->count > SIZE_MAX / sizeof(*c->fields)) return -1;
			c->fields = pg_alloc(storage, c->count * sizeof(*c->fields));
			if (!c->fields) return -1;
			for (size_t k = c->count; k; --k, fields = fields->parent) {
				/* This epoch admits scalar/enum fields only. A nominal Self,
				 * dependent field, callback or nested aggregate is unsupported. */
				if (fields->judgement != PG_JUDGEMENT_VALUE || !fields->declared_type ||
					fields->declared_type->kind != PG_REFERENCE) return -1;
				const struct pg_c_representation *f = pg_c_representation_find(table, fields->declared_type->as.reference);
				if (!f || f->constructors) return -1;
				c->fields[k - 1] = f;
			}
		}
	}
	return 0;
}

void pg_c_representations_destroy(struct pg_c_representations *table)
{
	pg_index_destroy(&table->lookup);
}

void pg_c_representation_type(FILE *out, const struct pg_c_representation *type)
{
	if (type->layout) fprintf(out, "struct ap_%s_%s", type->constructors ? "data" : "enum", type->alias);
	else fprintf(out, "int%zu_t", type->width);
}

void pg_c_representation_private_type(FILE *out, const struct pg_c_representation *type)
{
	if (type->constructors) pg_c_representation_type(out, type);
	else fputs("uint64_t", out);
}

void pg_c_representation_declarations(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i];
		pg_c_representation_type(out, r); fputs(" { uint32_t tag;", out);
		if (r->constructors) {
			fputs(" union {\n", out);
			for (size_t j = 0; j < r->count; ++j) {
				const struct pg_c_constructor_representation *c = &r->constructors[j];
				if (!c->count) continue;
				fputs("\tstruct {", out);
				for (size_t k = 0; k < c->count; ++k) {
					fputc(' ', out); pg_c_representation_type(out, c->fields[k]); fprintf(out, " f%zu;", k);
				}
				fprintf(out, " } c%zu;\n", j);
			}
			fputs("\tunsigned char empty;\n} fields;", out);
		}
		fputs(" };\n", out);
		for (size_t j = 0; j < r->count; ++j)
			fprintf(out, "#define AP_%s_%s_C%zu UINT32_C(%zu)\n", r->constructors ? "DATA" : "ENUM", r->alias, j, j);
	}
}
