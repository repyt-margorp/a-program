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

int pg_c_representations_native(struct pg_c_representations *table, struct pg_graph *storage,
	size_t enum_count, const struct pg_c_export *enums,
	size_t natural_count, const struct pg_c_export *naturals,
	size_t data_count, const struct pg_c_export *data)
{
	if (pg_index_init(&table->lookup)) return -1;
	if (data_count > SIZE_MAX - enum_count) return -1;
	if (natural_count > SIZE_MAX - enum_count - data_count) return -1;
	size_t count = enum_count + natural_count + data_count;
	if (count > SIZE_MAX / sizeof(*table->types)) return -1;
	if (enum_count && !pg_c_export_names(enum_count, enums)) return -1;
	if (natural_count && !pg_c_export_names(natural_count, naturals)) return -1;
	if (data_count && !pg_c_export_names(data_count, data)) return -1;
	table->count = count;
	table->types = pg_alloc(storage, count * sizeof(*table->types));
	if (!table->types) return -1;
	for (size_t i = 0; i < count; ++i) {
		int natural = i >= enum_count && i < enum_count + natural_count;
		const struct pg_c_export *selection = i < enum_count ? &enums[i] : natural ?
			&naturals[i - enum_count] : &data[i - enum_count - natural_count];
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
		*r = (struct pg_c_representation){.width = 32, .count = n, .alias = selection->alias, .layout = layout, .natural = natural};
		table->types[i] = r;
		if (record(table, storage, s->core->as.reference, r) || record(table, storage, pg_data_matcher(layout), r)) return -1;
		if (i < enum_count) continue;
		if (n > SIZE_MAX / sizeof(*r->constructors)) return -1;
		r->constructors = pg_alloc(storage, n * sizeof(*r->constructors));
		if (!r->constructors) return -1;
		for (size_t j = 0; j < n; ++j) {
			struct pg_c_constructor_representation *c = &r->constructors[j];
			c->tail = SIZE_MAX;
			const struct pg_context *fields = pg_data_declaration_fields(d, j);
			const struct pg_data_layout *owner;
			size_t position, arity;
			if (pg_context_extension_size(fields, prefix, &c->count) ||
				!pg_data_constructor_view(pg_data_constructor(layout, j), &owner, &position, &arity) ||
				arity != c->count || c->count > SIZE_MAX / sizeof(*c->fields)) return -1;
			c->fields = pg_alloc(storage, c->count * sizeof(*c->fields));
			if (!c->fields) return -1;
			for (size_t k = c->count; k; --k, fields = fields->parent) {
				/* The declaration prefix ends in its dedicated Self binder.
					* Only a direct, single recursive field has a node contract. */
				if (fields->judgement != PG_JUDGEMENT_VALUE || !fields->declared_type ||
					fields->declared_type->kind != PG_REFERENCE) return -1;
				const struct pg_c_representation *f = pg_c_representation_find(table, fields->declared_type->as.reference);
				if (!f && prefix && fields->declared_type->as.reference == prefix->binder &&
					pg_universe_level(prefix->declared_type, &level)) {
					if (c->tail != SIZE_MAX) return -1;
					c->tail = k - 1; r->recursive = 1; f = r;
				}
				if (!f || (f == r && c->tail != k - 1) ||
					(f->constructors && !f->natural && f != r && f->recursive)) return -1;
				c->fields[k - 1] = f;
			}
		}
		/* Nested value data must already be selected and complete. Recursive
			* nodes retain only scalar/enum/natural payloads and their direct tail. */
		if (r->recursive) for (size_t j = 0; j < n; ++j)
			for (size_t k = 0; k < r->constructors[j].count; ++k) {
				const struct pg_c_representation *f = r->constructors[j].fields[k];
				if (f != r && f->constructors && !f->natural) return -1;
			}
		if (natural) {
			/* Positions come from the selected declaration, never constructor names. */
			if (n != 2) return -1;
			size_t zero = r->constructors[0].count ? 1 : 0, successor = 1 - zero;
			if (r->constructors[zero].count || r->constructors[successor].count != 1 ||
				r->constructors[successor].tail != 0) return -1;
			r->zero = zero; r->recursive = 0;
		}
		if ((r->recursive || natural) && !table->arena_alias) table->arena_alias = r->alias;
	}
	return 0;
}

int pg_c_representations_init(struct pg_c_representations *table, struct pg_graph *storage,
	size_t enum_count, const struct pg_c_export *enums,
	size_t data_count, const struct pg_c_export *data)
{
	return pg_c_representations_native(table, storage, enum_count, enums, 0, NULL, data_count, data);
}

void pg_c_representations_destroy(struct pg_c_representations *table)
{
	pg_index_destroy(&table->lookup);
}

void pg_c_representation_type(FILE *out, const struct pg_c_representation *type)
{
	if (type->natural) fputs("uint32_t", out);
	else if (type->layout) fprintf(out, "%sstruct ap_%s_%s%s", type->recursive ? "const " : "",
		type->constructors ? "data" : "enum", type->alias, type->recursive ? " *" : "");
	else fprintf(out, "int%zu_t", type->width);
}

void pg_c_representation_private_type(FILE *out, const struct pg_c_representation *type)
{
	if (type->constructors && !type->natural) pg_c_representation_type(out, type);
	else fputs("uint64_t", out);
}

void pg_c_representation_declarations(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i];
		if (r->natural) {
			fprintf(out, "/* %s: zero = 0, successor = checked increment; status 5 on overflow. */\n", r->alias);
			continue;
		}
		fprintf(out, "struct ap_%s_%s { uint32_t tag;", r->constructors ? "data" : "enum", r->alias);
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

int pg_c_representation_list(const struct pg_c_representation *r, size_t *cell, size_t *payload)
{
	if (!r->recursive || r->count != 2) return 0;
	size_t position = r->constructors[0].count ? 0 : 1;
	const struct pg_c_constructor_representation *c = &r->constructors[position];
	if (r->constructors[1 - position].count || c->count != 2 || c->tail > 1) return 0;
	size_t field = 1 - c->tail;
	const struct pg_c_representation *type = c->fields[field];
	if (type->layout && !type->natural) return 0;
	*cell = position; *payload = field;
	return 1;
}
