#define _POSIX_C_SOURCE 200809L
#include "plan.h"
#include <stdlib.h>
#include <string.h>

struct export_row {
	struct export_row *next;
	const char *name, *alias;
	int enumeration;
};

const char *pg_c_lowering_name(enum pg_c_lowering lowering)
{
	static const char *const names[] = {"structural_v1", "scalar_direct_v1", "native_direct_v1"};
	return names[lowering];
}

const char *pg_c_abi_name(enum pg_c_lowering lowering)
{
	static const char *const names[] = {"isolated_v1", "c_scalar_v1", "c_native_v1"};
	return names[lowering];
}

static int space(unsigned char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* One directive per line. Quoted text supports only escaped quote/backslash;
 * comments start with # outside quotes. Reject stray text and control bytes. */
static int tokens(struct pg_graph *storage, const char *text, size_t length, const char **out)
{
	if (memchr(text, 0, length)) return -1;
	char *w = pg_alloc(storage, length + 1);
	if (!w) return -1;
	const unsigned char *p = (const unsigned char *)text;
	int count = 0;
	while (*p) {
		while (space(*p)) ++p;
		if (!*p || *p == '#') break;
		if (count == 3) return -1;
		out[count++] = w;
		int quoted = *p == '"';
		if (quoted) ++p;
		while (*p) {
			if (quoted ? *p == '"' : space(*p) || *p == '#') break;
			if (*p < 32 || *p == 127) return -1;
			if (quoted && *p == '\\') {
				++p;
				if (*p != '\\' && *p != '"') return -1;
			}
			*w++ = (char)*p++;
		}
		if (quoted) {
			if (*p++ != '"') return -1;
			if (*p && !space(*p) && *p != '#') return -1;
		}
		*w++ = 0;
		if (!*out[count - 1]) return -1;
	}
	return count;
}

static const char *relative(struct pg_graph *storage, const char *script, const char *path)
{
	if (*path == '/') return path;
	const char *slash = strrchr(script, '/');
	size_t prefix = slash ? (size_t)(slash - script) + 1 : 0, length = strlen(path);
	if (length > SIZE_MAX - prefix - 1) return NULL;
	char *result = pg_alloc(storage, prefix + length + 1);
	if (!result) return NULL;
	memcpy(result, script, prefix);
	memcpy(result + prefix, path, length + 1);
	return result;
}

int pg_c_link_read(struct pg_c_link_plan *plan, const char *path, size_t *line, const char **error)
{
	*line = 0;
	*error = "cannot read LinkerScript";
	plan->entry = SIZE_MAX;
	if (pg_graph_init(&plan->storage)) return -1;
	FILE *file = fopen(path, "rb");
	if (!file) return -1;
	struct export_row *first = NULL, **tail = &first;
	const char *entry = NULL, *abi = NULL;
	int version = 0, lowering = 0, fallback = 0, product = 0, target = 0, status = -1;
	char *text = NULL;
	size_t capacity = 0;
	ssize_t length;
	while ((length = getline(&text, &capacity, file)) >= 0) {
		++*line;
		const char *args[3];
		int n = tokens(&plan->storage, text, (size_t)length, args);
		*error = "invalid directive or quoting";
		if (n < 0) goto done;
		if (!n) continue;
		if (!version) {
			*error = "first directive must be aplink 1";
			if (n != 2 || strcmp(args[0], "aplink") || strcmp(args[1], "1")) goto done;
			version = 1;
			continue;
		}
		if (!strcmp(args[0], "export") || !strcmp(args[0], "enum32")) {
			if (n != 3 || !pg_c_export_alias(args[2])) goto done;
			int enumeration = !strcmp(args[0], "enum32");
			size_t *count = enumeration ? &plan->enum_count : &plan->count;
			if (*count == SIZE_MAX) goto done;
			struct export_row *row = pg_alloc(&plan->storage, sizeof(*row));
			if (!row) goto done;
			row->name = args[1]; row->alias = args[2];
			row->enumeration = enumeration;
			*tail = row; tail = &row->next;
			++*count;
			continue;
		}
		if (n != 2) goto done;
		if (!strcmp(args[0], "artifact")) {
			if (plan->artifact) goto done;
			plan->artifact = relative(&plan->storage, path, args[1]);
			if (!plan->artifact) goto done;
		} else if (!strcmp(args[0], "abi")) {
			if (abi) goto done;
			abi = args[1];
		} else if (!strcmp(args[0], "lowering")) {
			if (lowering) goto done;
			if (!strcmp(args[1], "scalar_direct_v1")) plan->lowering = PG_C_SCALAR_DIRECT;
			else if (!strcmp(args[1], "native_direct_v1")) plan->lowering = PG_C_NATIVE_DIRECT;
			else if (!strcmp(args[1], "structural_v1")) plan->lowering = PG_C_STRUCTURAL;
			else goto done;
			lowering = 1;
		} else if (!strcmp(args[0], "fallback")) {
			if (fallback || strcmp(args[1], "reject")) goto done;
			fallback = 1;
		} else if (!strcmp(args[0], "target")) {
			if (target || strcmp(args[1], "host-c11")) goto done;
			target = 1;
		} else if (!strcmp(args[0], "product")) {
			if (product) goto done;
			static const char *const kinds[] = {"source", "object", "archive", "executable"};
			for (size_t i = 0; i < 4; ++i) if (!strcmp(args[1], kinds[i])) {
				plan->product = (enum pg_c_product)i;
				product = 1;
			}
			if (!product) goto done;
		} else if (!strcmp(args[0], "entry")) {
			if (entry) goto done;
			entry = args[1];
		} else if (!strcmp(args[0], "native_script")) {
			if (plan->native_script) goto done;
			plan->native_script = relative(&plan->storage, path, args[1]);
			if (!plan->native_script) goto done;
		} else goto done;
	}
	*error = "missing required directive or invalid product/entry combination";
	if (ferror(file) || !version || !abi || !product || !target || !plan->artifact || !plan->count) goto done;
	*error = "ABI/lowering mismatch or missing explicit native fallback policy";
	if (strcmp(abi, pg_c_abi_name(plan->lowering))) goto done;
	if (plan->lowering != PG_C_STRUCTURAL && !fallback) goto done;
	if (plan->enum_count && plan->lowering != PG_C_NATIVE_DIRECT) goto done;
	*error = "invalid product/entry combination";
	if (plan->product == PG_C_EXECUTABLE && !entry) goto done;
	if ((plan->product == PG_C_OBJECT || plan->product == PG_C_ARCHIVE) && entry) goto done;
	if (plan->native_script && plan->product != PG_C_EXECUTABLE) goto done;
	if (plan->count > SIZE_MAX / sizeof(*plan->exports)) goto done;
	plan->names = pg_alloc(&plan->storage, plan->count * sizeof(*plan->names));
	plan->exports = pg_alloc(&plan->storage, plan->count * sizeof(*plan->exports));
	if (!plan->names || !plan->exports) goto done;
	if (plan->enum_count > SIZE_MAX / sizeof(*plan->enums) || plan->enum_count > SIZE_MAX - plan->count) goto done;
	plan->enum_names = pg_alloc(&plan->storage, plan->enum_count * sizeof(*plan->enum_names));
	plan->enums = pg_alloc(&plan->storage, plan->enum_count * sizeof(*plan->enums));
	if (!plan->enum_names || !plan->enums) goto done;
	size_t i = 0, j = 0;
	for (const struct export_row *row = first; row; row = row->next) {
		if (row->enumeration) {
			plan->enum_names[j] = row->name; plan->enums[j++].alias = row->alias;
			continue;
		}
		plan->names[i] = row->name;
		plan->exports[i].alias = row->alias;
		if (entry && !strcmp(entry, row->alias)) plan->entry = i;
		++i;
	}
	*error = "duplicate aliases or missing entry export";
	if (!pg_c_export_names(plan->count, plan->exports)) goto done;
	if (plan->enum_count && !pg_c_export_names(plan->enum_count, plan->enums)) goto done;
	if (entry && plan->entry == SIZE_MAX) goto done;
	status = 0;
done:
	free(text);
	fclose(file);
	return status;
}

void pg_c_link_destroy(struct pg_c_link_plan *plan)
{
	pg_graph_destroy(&plan->storage);
}
