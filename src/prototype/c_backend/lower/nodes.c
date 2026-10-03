#include "nodes.h"

void pg_c_nodes_declarations(FILE *out, const struct pg_c_representations *table)
{
	if (!table->arena_alias) return;
	fputs("#include <stddef.h>\n#ifndef AP_C_ARENA_ABI\n#define AP_C_ARENA_ABI 1\n"
		"struct ap_c_allocation;\n"
		"/* Zero initialize. Borrowed input nodes outlive every result sharing them.\n"
		"\t* Result storage is separate from inputs and arena metadata.\n"
		"\t* capacity 0 is unlimited; depth_limit 0 selects 256, maximum 256. */\n"
		"struct ap_c_arena { struct ap_c_allocation *first; size_t count, capacity, depth, depth_limit; int status; };\n"
		"#elif AP_C_ARENA_ABI != 1\n#error incompatible_A_Program_arena_ABI\n#endif\n", out);
	fprintf(out, "void ap_arena_%s_destroy(struct ap_c_arena *);\n", table->arena_alias);
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i];
		size_t cell, payload;
		if (!pg_c_representation_list(r, &cell, &payload)) continue;
		fputs("/* Copy finite List to a separate writable buffer. Inputs, buffer and\n"
			"\t* written must not overlap. 6: insufficient capacity; failures preserve\n"
			"\t* buffer and written. Empty List accepts a null buffer. */\n", out);
		fprintf(out, "int ap_copy_%s(const struct ap_data_%s *, ", r->alias, r->alias);
		pg_c_representation_type(out, r->constructors[cell].fields[payload]);
		fputs(" *, size_t capacity, size_t *written);\n", out);
	}
}

void pg_c_nodes_implementation(FILE *out, const struct pg_c_representations *table)
{
	if (!table->arena_alias) return;
	fputs("struct ap_c_allocation { struct ap_c_allocation *next; union { max_align_t alignment; unsigned char bytes[1]; } payload; };\n"
		"static void ap_release(struct ap_c_arena *arena, struct ap_c_allocation *mark)\n{\n"
		"\twhile (arena->first != mark) {\n\t\tstruct ap_c_allocation *node = arena->first;\n"
		"\t\tarena->first = node->next; free(node); --arena->count;\n\t}\n}\n"
		"static void *ap_allocate(struct ap_c_arena *arena, size_t size)\n{\n"
		"\tif (arena->status) return NULL;\n"
		"\tif (size > SIZE_MAX - sizeof(struct ap_c_allocation) || arena->count == SIZE_MAX ||\n"
		"\t\t(arena->capacity && arena->count >= arena->capacity)) { arena->status = 3; return NULL; }\n"
		"\tstruct ap_c_allocation *node = malloc(sizeof(*node) + size);\n"
		"\tif (!node) { arena->status = 3; return NULL; }\n"
		"\tnode->next = arena->first; arena->first = node; ++arena->count;\n"
		"\treturn node->payload.bytes;\n}\n", out);
	fprintf(out, "void ap_arena_%s_destroy(struct ap_c_arena *arena)\n{\n"
		"\tif (arena) { ap_release(arena, NULL); arena->status = 0; arena->depth = 0; }\n}\n", table->arena_alias);
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i];
		if (!r->recursive) continue;
		fprintf(out, "static const struct ap_data_%s *ap_step_%s(const struct ap_data_%s *node, int *valid)\n{\n"
			"\tif (!node) { *valid = 0; return NULL; }\n\tswitch (node->tag) {\n", r->alias, r->alias, r->alias);
		for (size_t j = 0; j < r->count; ++j) {
			const struct pg_c_constructor_representation *c = &r->constructors[j];
			fprintf(out, "\tcase %zu:\n", j);
			for (size_t k = 0; k < c->count; ++k) if (c->fields[k]->layout && !c->fields[k]->recursive && !c->fields[k]->natural)
				fprintf(out, "\t\tif ((uint64_t)node->fields.c%zu.f%zu.tag >= UINT64_C(%zu)) *valid = 0;\n", j, k, c->fields[k]->count);
			if (c->tail == SIZE_MAX) fputs("\t\treturn NULL;\n", out);
			else fprintf(out, "\t\tif (!node->fields.c%zu.f%zu) *valid = 0;\n\t\treturn node->fields.c%zu.f%zu;\n", j, c->tail, j, c->tail);
		}
		fputs("\tdefault: *valid = 0; return NULL;\n\t}\n}\n", out);
		fprintf(out, "static int ap_validate_%s(const struct ap_data_%s *node)\n{\n"
			"\tconst struct ap_data_%s *slow = node, *fast = node;\n\tint valid = 1;\n\tif (!node) return 0;\n"
			"\twhile (fast && valid) {\n\t\tslow = ap_step_%s(slow, &valid);\n"
			"\t\tfast = ap_step_%s(fast, &valid);\n\t\tif (fast && valid) fast = ap_step_%s(fast, &valid);\n"
			"\t\tif (fast && slow == fast) return 0;\n\t}\n\treturn valid;\n}\n",
			r->alias, r->alias, r->alias, r->alias, r->alias, r->alias);
		size_t cell, payload;
		if (!pg_c_representation_list(r, &cell, &payload)) continue;
		size_t tail = r->constructors[cell].tail;
		fprintf(out, "int ap_copy_%s(const struct ap_data_%s *input, ", r->alias, r->alias);
		pg_c_representation_type(out, r->constructors[cell].fields[payload]);
		fprintf(out, " *buffer, size_t capacity, size_t *written)\n{\n"
			"\tif (!written) return 1;\n\tif (!ap_validate_%s(input)) return 2;\n"
			"\tsize_t count = 0;\n\tconst struct ap_data_%s *node = input;\n"
			"\twhile (node->tag == %zu) {\n\t\tif (count == SIZE_MAX) return 6;\n"
			"\t\t++count; node = node->fields.c%zu.f%zu;\n\t}\n"
			"\tif (count > capacity) return 6;\n\tif (count && !buffer) return 1;\n"
			"\tnode = input;\n\tfor (size_t i = 0; i < count; ++i) {\n"
			"\t\tbuffer[i] = node->fields.c%zu.f%zu; node = node->fields.c%zu.f%zu;\n"
			"\t}\n\t*written = count;\n\treturn 0;\n}\n",
			r->alias, r->alias, cell, cell, tail, cell, payload, cell, tail);
	}
}
