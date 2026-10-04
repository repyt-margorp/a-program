#include "nodes.h"

void pg_c_nodes_measure_declarations(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i]; size_t cell, payload;
		if (!pg_c_representation_list(r, &cell, &payload)) continue;
		fputs("/* Query a validated finite List extent before allocating a copy buffer.\n"
			"\t* Input and count must not overlap. No arena, allocation or depth limit.\n"
			"\t* 1: null count; 2: invalid chain; 6: size overflow. Failures preserve count. */\n", out);
		fprintf(out, "int ap_measure_%s(const struct ap_data_%s *, size_t *count);\n", r->alias, r->alias);
	}
}

void pg_c_nodes_measure_implementation(FILE *out, const struct pg_c_representations *table)
{
	for (size_t i = 0; i < table->count; ++i) {
		const struct pg_c_representation *r = table->types[i]; size_t cell, payload;
		if (!pg_c_representation_list(r, &cell, &payload)) continue;
		size_t tail = r->constructors[cell].tail;
		fprintf(out, "int ap_measure_%s(const struct ap_data_%s *input, size_t *count)\n{\n"
			"\tif (!count) return 1;\n\tif (!ap_validate_%s(input)) return 2;\n"
			"\tsize_t size = 0;\n\tconst struct ap_data_%s *node = input;\n"
			"\twhile (node->tag == %zu) {\n\t\tif (size == SIZE_MAX) return 6;\n"
			"\t\t++size; node = node->fields.c%zu.f%zu;\n\t}\n"
			"\t*count = size;\n\treturn 0;\n}\n", r->alias, r->alias, r->alias, r->alias, cell, cell, tail);
	}
}

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
		if (pg_c_representation_branching(r))
			fputs("/* Branching inputs: readable finite acyclic nodes; shared subtrees allowed.\n"
				"\t* Validation uses temporary allocation (status 3), freed before execution. */\n", out);
		size_t cell, payload;
		if (!pg_c_representation_list(r, &cell, &payload)) continue;
		fputs("/* Copy finite List to a separate writable buffer. Inputs, buffer and\n"
			"\t* written must not overlap. 6: insufficient capacity; failures preserve\n"
			"\t* buffer and written. Empty List accepts a null buffer. */\n", out);
		fprintf(out, "int ap_copy_%s(const struct ap_data_%s *, ", r->alias, r->alias);
		pg_c_representation_type(out, r->constructors[cell].fields[payload]);
		fputs(" *, size_t capacity, size_t *written);\n", out);
		fputs("/* Copy an array slice into arena-owned List nodes, including its terminal.\n"
			"\t* Input, output and arena metadata must not overlap. Empty input accepts\n"
			"\t* a null array. 1: null argument; 2: invalid active tag; 3: allocation; 4: active arena; 6: length\n"
			"\t* overflow. Failures preserve output and prior arena allocations. */\n", out);
		fprintf(out, "int ap_from_%s(struct ap_c_arena *, const ", r->alias);
		pg_c_representation_type(out, r->constructors[cell].fields[payload]);
		fprintf(out, " *, size_t count, const struct ap_data_%s **out);\n", r->alias);
	}
}

static void branching_validator(FILE *out, const struct pg_c_representation *r)
{
	fprintf(out, "static const struct ap_data_%s *ap_child_%s(const struct ap_data_%s *node, size_t edge, int *valid)\n{\n"
		"\tswitch (node->tag) {\n", r->alias, r->alias, r->alias);
	for (size_t j = 0; j < r->count; ++j) {
		const struct pg_c_constructor_representation *c = &r->constructors[j];
		fprintf(out, "\tcase %zu:\n", j);
		for (size_t k = 0; k < c->count; ++k) {
			const struct pg_c_representation *f = c->fields[k];
			if (!f->layout || f->recursive || f->natural) continue;
			if (f->constructors)
				fprintf(out, "\t\tif (!ap_valid_value_%s(node->fields.c%zu.f%zu)) *valid = 0;\n", f->alias, j, k);
			else fprintf(out, "\t\tif ((uint64_t)node->fields.c%zu.f%zu.tag >= UINT64_C(%zu)) *valid = 0;\n", j, k, f->count);
		}
		fputs("\t\tswitch (edge) {\n", out);
		size_t edge = 0;
		for (size_t k = 0; k < c->count; ++k) if (c->fields[k] == r)
			fprintf(out, "\t\tcase %zu:\n\t\t\tif (!node->fields.c%zu.f%zu) *valid = 0;\n"
				"\t\t\treturn node->fields.c%zu.f%zu;\n", edge++, j, k, j, k);
		fputs("\t\tdefault: return NULL;\n\t\t}\n", out);
	}
	fputs("\tdefault: *valid = 0; return NULL;\n\t}\n}\n", out);
	fprintf(out, "static int ap_validate_graph_%s(const struct ap_data_%s *node)\n{\n"
		"\tstruct frame { const struct ap_data_%s *node; size_t parent, edge; int active; };\n"
		"\tif (!node) return 2;\n\tsize_t count = 1, capacity = 16, current = 0;\n"
		"\tstruct frame *frames = malloc(capacity * sizeof(*frames));\n\tif (!frames) return 3;\n"
		"\tframes[0] = (struct frame){.node = node, .parent = SIZE_MAX, .active = 1};\n"
		"\twhile (current != SIZE_MAX) {\n\t\tint valid = 1;\n"
		"\t\tconst struct ap_data_%s *child = ap_child_%s(frames[current].node, frames[current].edge++, &valid);\n"
		"\t\tif (!valid) { free(frames); return 2; }\n"
		"\t\tif (!child) { frames[current].active = 0; current = frames[current].parent; continue; }\n"
		"\t\tsize_t seen = 0;\n\t\twhile (seen < count && frames[seen].node != child) ++seen;\n"
		"\t\tif (seen < count) {\n\t\t\tif (frames[seen].active) { free(frames); return 2; }\n\t\t\tcontinue;\n\t\t}\n"
		"\t\tif (count == capacity) {\n"
		"\t\t\tif (capacity > SIZE_MAX / 2 / sizeof(*frames)) { free(frames); return 3; }\n"
		"\t\t\tsize_t next = capacity * 2;\n\t\t\tstruct frame *grown = malloc(next * sizeof(*grown));\n"
		"\t\t\tif (!grown) { free(frames); return 3; }\n"
		"\t\t\tfor (size_t i = 0; i < count; ++i) grown[i] = frames[i];\n"
		"\t\t\tfree(frames); frames = grown; capacity = next;\n\t\t}\n"
		"\t\tframes[count] = (struct frame){.node = child, .parent = current, .active = 1};\n"
		"\t\tcurrent = count++;\n\t}\n\tfree(frames); return 0;\n}\n",
		r->alias, r->alias, r->alias, r->alias, r->alias);
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
		if (pg_c_representation_branching(r)) { branching_validator(out, r); continue; }
		fprintf(out, "static const struct ap_data_%s *ap_step_%s(const struct ap_data_%s *node, int *valid)\n{\n"
			"\tif (!node) { *valid = 0; return NULL; }\n\tswitch (node->tag) {\n", r->alias, r->alias, r->alias);
		for (size_t j = 0; j < r->count; ++j) {
			const struct pg_c_constructor_representation *c = &r->constructors[j];
			fprintf(out, "\tcase %zu:\n", j);
			for (size_t k = 0; k < c->count; ++k) {
				const struct pg_c_representation *f = c->fields[k];
				if (!f->layout || f->recursive || f->natural) continue;
				if (f->constructors)
					fprintf(out, "\t\tif (!ap_valid_value_%s(node->fields.c%zu.f%zu)) *valid = 0;\n", f->alias, j, k);
				else fprintf(out, "\t\tif ((uint64_t)node->fields.c%zu.f%zu.tag >= UINT64_C(%zu)) *valid = 0;\n", j, k, f->count);
			}
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
		fprintf(out, "int ap_from_%s(struct ap_c_arena *arena, const ", r->alias);
		pg_c_representation_type(out, r->constructors[cell].fields[payload]);
		fprintf(out, " *input, size_t count, const struct ap_data_%s **out)\n{\n"
			"\tif (!arena || !out || (count && !input)) return 1;\n"
			"\tif (arena->depth) return 4;\n\tif (count == SIZE_MAX) return 6;\n", r->alias);
		const struct pg_c_representation *element = r->constructors[cell].fields[payload];
		if (element->constructors && !element->natural)
			fprintf(out, "\tfor (size_t i = 0; i < count; ++i)\n"
				"\t\tif (!ap_valid_value_%s(input[i])) return 2;\n", element->alias);
		else if (element->layout && !element->natural)
			fprintf(out, "\tfor (size_t i = 0; i < count; ++i)\n"
				"\t\tif ((uint64_t)input[i].tag >= UINT64_C(%zu)) return 2;\n", element->count);
		fprintf(out,
			"\tstruct ap_c_allocation *mark = arena->first;\n\tarena->status = 0;\n"
			"\tstruct ap_data_%s *node = ap_allocate(arena, sizeof(*node));\n"
			"\tif (node) *node = (struct ap_data_%s){.tag = %zu};\n"
			"\tconst struct ap_data_%s *result = node;\n"
			"\twhile (count && !arena->status) {\n"
			"\t\tnode = ap_allocate(arena, sizeof(*node));\n"
			"\t\tif (node) {\n\t\t\t--count;\n"
			"\t\t\t*node = (struct ap_data_%s){.tag = %zu, .fields.c%zu = {.f%zu = input[count], .f%zu = result}};\n"
			"\t\t\tresult = node;\n\t\t}\n\t}\n"
			"\tif (arena->status) { ap_release(arena, mark); return arena->status; }\n"
			"\t*out = result;\n\treturn 0;\n}\n",
			r->alias, r->alias, 1 - cell, r->alias, r->alias, cell, cell, payload, tail);
	}
}
