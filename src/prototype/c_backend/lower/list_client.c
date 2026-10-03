#include "component.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static size_t allocations;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	if (allocations == SIZE_MAX) return __real_malloc(size);
	if (!allocations) return NULL;
	--allocations;
	return __real_malloc(size);
}

static const struct ap_data_List nil = {.tag = AP_DATA_List_C0};

static void node(struct ap_data_List *out, int32_t n, uint32_t flag, const struct ap_data_List *tail)
{
	*out = (struct ap_data_List){.tag = AP_DATA_List_C1, .fields.c1 = {n, {flag}, tail}};
}

static int32_t signed_bits(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}

static const struct ap_data_List *same_prefix(const struct ap_data_List *out, const struct ap_data_List *input)
{
	while (input->tag == AP_DATA_List_C1) {
		assert(out && out->tag == AP_DATA_List_C1);
		assert(out->fields.c1.f0 == input->fields.c1.f0 && out->fields.c1.f1.tag == input->fields.c1.f1.tag);
		out = out->fields.c1.f2; input = input->fields.c1.f2;
	}
	return out;
}

static void ordinary(void)
{
	struct ap_data_List nodes[8], suffix;
	node(&suffix, INT32_MIN, 1, &nil);
	const int32_t numbers[] = {INT32_MIN, INT32_MAX, -1, 0, 1, 42, -99, 103};
	size_t cases = 0;
	for (size_t length = 0; length <= 8; ++length) for (unsigned flags = 0; flags < (1u << length); ++flags) {
		const struct ap_data_List *input = &nil;
		uint32_t bits = 0;
		for (size_t i = length; i; --i) {
			node(&nodes[i - 1], numbers[i - 1], (flags >> (i - 1)) & 1, input);
			input = &nodes[i - 1]; bits += (uint32_t)numbers[i - 1];
		}
		struct ap_c_arena arena = {0};
		int32_t scalar;
		const struct ap_data_List *out;
		assert(!ap_export_length(&arena, input, &scalar) && scalar == (int32_t)length);
		assert(!ap_export_sum(&arena, input, &scalar) && scalar == signed_bits(bits));
		assert(!ap_export_identity(&arena, input, &out) && out == input && !arena.count);
		assert(!ap_export_append(&arena, input, &suffix, &out));
		assert(same_prefix(out, input) == &suffix && arena.count == length);
		assert(!ap_export_composed(&arena, input, &scalar) && scalar == signed_bits(bits + bits));
		for (uint32_t wanted = 0; wanted < 2; ++wanted) {
			assert(!ap_export_select(&arena, input, (struct ap_enum_Bool){wanted}, &out));
			for (const struct ap_data_List *p = input; p->tag == AP_DATA_List_C1; p = p->fields.c1.f2) {
				if (p->fields.c1.f1.tag != wanted) continue;
				assert(out && out->tag == AP_DATA_List_C1 && out->fields.c1.f0 == p->fields.c1.f0);
				assert(out->fields.c1.f1.tag == wanted); out = out->fields.c1.f2;
			}
			assert(out && out->tag == AP_DATA_List_C0);
		}
		assert(!ap_export_cons(&arena, INT32_MAX, (struct ap_enum_Bool){0}, input, &out));
		assert(out->fields.c1.f0 == INT32_MAX && out->fields.c1.f2 == input);
		ap_arena_List_destroy(&arena); assert(!arena.count && !arena.first && !arena.depth);
		ap_arena_List_destroy(&arena); ++cases;
	}
	assert(cases == 511);
}

static void failures(void)
{
	struct ap_c_arena arena = {0};
	struct ap_data_List first, second;
	node(&second, 2, 1, &nil); node(&first, 1, 0, &second);
	const struct ap_data_List *out = &first;
	int32_t scalar = 77;
	assert(ap_export_length(NULL, &first, &scalar) == 1 && scalar == 77);
	assert(ap_export_length(&arena, &first, NULL) == 1);
	assert(ap_export_length(&arena, NULL, &scalar) == 2 && scalar == 77);
	second.tag = UINT32_MAX;
	assert(ap_export_sum(&arena, &first, &scalar) == 2 && scalar == 77);
	second.tag = AP_DATA_List_C1; second.fields.c1.f1.tag = 2;
	assert(ap_export_append(&arena, &first, &nil, &out) == 2 && out == &first);
	second.fields.c1.f1.tag = 1; second.fields.c1.f2 = NULL;
	assert(ap_export_identity(&arena, &first, &out) == 2 && out == &first);
	second.fields.c1.f2 = &first;
	assert(ap_export_length(&arena, &first, &scalar) == 2 && scalar == 77);
	second.fields.c1.f2 = &second;
	assert(ap_export_length(&arena, &first, &scalar) == 2 && scalar == 77);
	second.fields.c1.f2 = &nil;
	assert(ap_export_select(&arena, &first, (struct ap_enum_Bool){2}, &out) == 2 && out == &first);
	assert(!ap_export_constant(&arena, &out));
	const struct ap_data_List *saved = out;
	struct ap_c_allocation *mark = arena.first;
	allocations = 1;
	out = &first;
	assert(ap_export_append(&arena, &first, &nil, &out) == 3 && out == &first);
	assert(arena.first == mark && arena.count == 1 && saved->tag == AP_DATA_List_C0 && !arena.depth);
	allocations = SIZE_MAX;
	arena.capacity = 2;
	assert(ap_export_append(&arena, &first, &nil, &out) == 3 && out == &first && arena.count == 1);
	arena.capacity = 0; arena.depth_limit = 2;
	assert(ap_export_append(&arena, &first, &nil, &out) == 4 && out == &first && arena.count == 1 && !arena.depth);
	arena.depth_limit = 257;
	assert(ap_export_length(&arena, &nil, &scalar) == 4 && scalar == 77);
	arena.depth_limit = 0;
	assert(!ap_export_append(&arena, &first, &nil, &out));
	assert(same_prefix(out, &first) == &nil);
	ap_arena_List_destroy(&arena);
	arena.capacity = 2;
	assert(!ap_export_select(&arena, &first, (struct ap_enum_Bool){1}, &out));
	assert(arena.count == 2 && out->fields.c1.f0 == 2 && out->fields.c1.f2->tag == AP_DATA_List_C0);
	ap_arena_List_destroy(&arena); arena.capacity = 0;
	struct ap_data_List chain[257];
	const struct ap_data_List *tail = &nil;
	for (size_t i = 257; i; --i) { node(&chain[i - 1], 1, 0, tail); tail = &chain[i - 1]; }
	assert(ap_export_length(&arena, tail, &scalar) == 4 && scalar == 77 && !arena.depth);
	assert(!ap_export_length(&arena, &chain[255], &scalar) && scalar == 2);
	ap_arena_List_destroy(&arena);
}

int main(void)
{
	allocations = SIZE_MAX;
	ordinary(); failures();
	puts("Native list client: 511 flag/length cases, ownership, cycles and transactional resource failures passed");
	return 0;
}
