#include "host.h"

#include <string.h>

static const struct pg_object_class type_class = {"host-type"};
static const struct pg_object_class literal_class = {"host-literal"};
static const struct pg_object int32_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct pg_object int64_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct pg_object text_type = {PG_SEMANTIC_OBJECT, &type_class};
static const struct {
	const struct pg_object *type;
	const char *source, *descriptor;
} types[] = {
	{&int32_type, "Int32", "host/int32/v1"},
	{&int64_type, "Int64", "host/int64/v1"},
	{&text_type, "Text", "host/text-bytes/v1"}
};

struct host_literal {
	struct pg_object_entry base;
	const struct pg_object *type;
	size_t count;
	unsigned char bytes[];
};

const struct pg_object *pg_host_type(const char *name)
{
	if (!name) return NULL;
	if (!strcmp(name, "Int")) return &int32_type;
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (!strcmp(name, types[i].source)) return types[i].type;
	return NULL;
}

const char *pg_host_type_name(const struct pg_object *type)
{
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (type == types[i].type) return types[i].descriptor;
	return NULL;
}

const struct pg_object *pg_host_type_resolve(const char *name)
{
	if (!name) return NULL;
	for (size_t i = 0; i < sizeof(types) / sizeof(*types); ++i)
		if (!strcmp(name, types[i].descriptor)) return types[i].type;
	return NULL;
}

const struct pg_object *pg_host_literal(struct pg_graph *graph,
	const struct pg_object *type, size_t count, const unsigned char *bytes)
{
	if (!graph || !pg_host_type_name(type) || (count && !bytes)) return NULL;
	if (type == &int32_type && count != 4) return NULL;
	if (type == &int64_type && count != 8) return NULL;
	if (count > SIZE_MAX - sizeof(struct host_literal)) return NULL;
	if (!graph->objects.capacity && pg_index_init(&graph->objects)) return NULL;
	uint64_t hash = (uintptr_t)type ^ count;
	for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
	for (struct pg_index_entry *p = pg_index_candidates(&graph->objects, hash); p; p = p->next) {
		if (p->hash != hash) continue;
		const struct pg_object_entry *base = (const void *)p;
		if (base->object.owner != &literal_class) continue;
		const struct host_literal *value = (const void *)p;
		if (value->type == type && value->count == count && (!count || !memcmp(value->bytes, bytes, count)))
			return &value->base.object;
	}
	struct host_literal *value = pg_alloc(graph, sizeof(*value) + count);
	if (!value) return NULL;
	value->base.object = (struct pg_object){PG_SEMANTIC_OBJECT, &literal_class};
	value->type = type;
	value->count = count;
	if (count) memcpy(value->bytes, bytes, count);
	return pg_index_insert(&graph->objects, &value->base.index, hash) ? NULL : &value->base.object;
}

int pg_host_literal_view(const struct pg_object *object,
	const struct pg_object **type, size_t *count, const unsigned char **bytes)
{
	if (!object || object->owner != &literal_class || !type || !count || !bytes) return 0;
	const struct host_literal *value = (const void *)((const char *)object - offsetof(struct pg_object_entry, object));
	*type = value->type; *count = value->count; *bytes = value->bytes;
	return 1;
}

const struct pg_object *pg_host_integer(struct pg_graph *graph,
	const struct pg_object *type, int64_t value)
{
	size_t count;
	if (type == &int32_type) {
		if (value < INT32_MIN || value > INT32_MAX) return NULL;
		count = 4;
	} else if (type == &int64_type) count = 8;
	else return NULL;
	unsigned char bytes[8];
	uint64_t bits = (uint64_t)value;
	for (size_t i = count; i; --i, bits >>= 8) bytes[i - 1] = (unsigned char)(bits & 255);
	return pg_host_literal(graph, type, count, bytes);
}

int pg_host_integer_view(const struct pg_object *object, int64_t *value)
{
	const struct pg_object *type;
	size_t count;
	const unsigned char *bytes;
	if (!value || !pg_host_literal_view(object, &type, &count, &bytes)) return 0;
	if (type != &int32_type && type != &int64_type) return 0;
	uint64_t bits = 0;
	for (size_t i = 0; i < count; ++i) bits = (bits << 8) | bytes[i];
	uint64_t maximum = count == 4 ? UINT32_MAX : UINT64_MAX;
	*value = bytes[0] & 128 ? -1 - (int64_t)(maximum - bits) : (int64_t)bits;
	return 1;
}
