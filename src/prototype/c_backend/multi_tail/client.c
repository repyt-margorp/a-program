#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static size_t allocations, fail_at;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	if (++allocations == fail_at) return NULL;
	return __real_malloc(size);
}

static int32_t signed_bits(uint32_t value)
{
	return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)(UINT32_MAX - value);
}

static uint32_t sum(const struct ap_data_Tree *tree)
{
	return tree->tag == 0 ? (uint32_t)tree->fields.c0.f0 : sum(tree->fields.c1.f1) + sum(tree->fields.c1.f2);
}

static void same(const struct ap_data_Tree *input, const struct ap_data_Tree *out, int mirror, int32_t delta)
{
	assert(input->tag == out->tag);
	if (!input->tag) { assert(out->fields.c0.f0 == signed_bits((uint32_t)input->fields.c0.f0 + (uint32_t)delta)); return; }
	assert(input->fields.c1.f0.tag == out->fields.c1.f0.tag);
	same(input->fields.c1.f1, mirror ? out->fields.c1.f2 : out->fields.c1.f1, mirror, delta);
	same(input->fields.c1.f2, mirror ? out->fields.c1.f1 : out->fields.c1.f2, mirror, delta);
}

static const struct ap_data_Reverse *reverse(const struct ap_data_Tree *tree, struct ap_data_Reverse *nodes, size_t *count)
{
	struct ap_data_Reverse *node = &nodes[(*count)++]; assert(*count <= 7);
	if (!tree->tag) *node = (struct ap_data_Reverse){.tag = 1, .fields.c1 = {tree->fields.c0.f0}};
	else *node = (struct ap_data_Reverse){.tag = 0, .fields.c0 = {
		reverse(tree->fields.c1.f1, nodes, count), tree->fields.c1.f0, reverse(tree->fields.c1.f2, nodes, count)}};
	return node;
}

static void finite_cases(void)
{
	struct ap_data_Tree pool[210] = {{.tag = 0}, {.tag = 0, .fields.c0 = {1}}}; size_t count = 2;
	for (size_t bound = 2; bound <= 10; bound += 8)
		for (unsigned flag = 0; flag < 2; ++flag) for (size_t left = 0; left < bound; ++left) for (size_t right = 0; right < bound; ++right)
			pool[count++] = (struct ap_data_Tree){.tag = 1, .fields.c1 = {{flag}, &pool[left], &pool[right]}};
	assert(count == 210); size_t cases = 0;
	for (size_t i = 0; i < count; ++i) {
		if (i >= 2 && i < 10) continue;
		struct ap_c_arena arena = {0}; const struct ap_data_Tree *out; int32_t value;
		arena.depth_limit = 1;
		assert(!ap_export_identity(&arena, &pool[i], &out) && out == &pool[i]);
		arena.depth_limit = 0;
		assert(!ap_export_sum(&arena, &pool[i], &value) && value == signed_bits(sum(&pool[i])));
		assert(!ap_export_mirror(&arena, &pool[i], &out)); same(&pool[i], out, 1, 0);
		assert(!ap_export_shift(&arena, &pool[i], -3, &out)); same(&pool[i], out, 0, -3);
		struct ap_data_Reverse reversed[7]; size_t used = 0;
		const struct ap_data_Reverse *input = reverse(&pool[i], reversed, &used), *result;
		assert(!ap_export_reverse_identity(&arena, input, &result) && result == input);
		assert(!ap_export_reverse_sum(&arena, input, &value) && value == signed_bits(sum(&pool[i])));
		assert(!ap_export_reverse_mirror(&arena, input, &result));
		assert(!ap_export_reverse_fingerprint(&arena, result, &value));
		int32_t other;
		assert(!ap_export_mirror(&arena, &pool[i], &out));
		assert(!ap_export_fingerprint(&arena, out, &other) && value == other);
		assert(!arena.depth); ap_arena_Tree_destroy(&arena); ++cases;
	}
	assert(cases == 202);
}

static void invalid_cases(struct ap_c_arena *arena, const struct ap_data_Tree *sentinel)
{
	struct ap_data_Tree leaf = {.tag = 0, .fields.c0 = {7}}, bad = {.tag = 77};
	struct ap_data_Tree root = {.tag = 1, .fields.c1 = {{0}, &leaf, &leaf}};
	const struct ap_data_Tree *out = sentinel;
	assert(ap_export_identity(NULL, &root, &out) == 1 && out == sentinel);
	assert(ap_export_identity(arena, &root, NULL) == 1);
	assert(ap_export_identity(arena, NULL, &out) == 2 && out == sentinel);
	assert(ap_export_identity(arena, &bad, &out) == 2 && out == sentinel);
	for (unsigned side = 0; side < 2; ++side) {
		const struct ap_data_Tree **child = side ? &root.fields.c1.f2 : &root.fields.c1.f1;
		*child = &bad; assert(ap_export_identity(arena, &root, &out) == 2 && out == sentinel);
		*child = NULL; assert(ap_export_identity(arena, &root, &out) == 2 && out == sentinel);
		*child = &root; assert(ap_export_identity(arena, &root, &out) == 2 && out == sentinel);
		*child = &leaf;
	}
	root.fields.c1.f0.tag = 2;
	assert(ap_export_identity(arena, &root, &out) == 2 && out == sentinel);
	root.fields.c1.f0.tag = 0;
	struct ap_data_Tree other = {.tag = 1, .fields.c1 = {{0}, &leaf, &root}};
	root.fields.c1.f2 = &other;
	assert(ap_export_identity(arena, &root, &out) == 2 && out == sentinel);
	root.fields.c1.f2 = &leaf;
	struct ap_data_Reverse rleaf = {.tag = 1, .fields.c1 = {4}}, rroot = {.tag = 0, .fields.c0 = {&rleaf,{1},&rleaf}};
	const struct ap_data_Reverse *rout = &rleaf;
	rroot.fields.c0.f0 = &rroot;
	assert(ap_export_reverse_identity(arena, &rroot, &rout) == 2 && rout == &rleaf);
	rroot.fields.c0.f0 = &rleaf; rroot.fields.c0.f2 = NULL;
	assert(ap_export_reverse_identity(arena, &rroot, &rout) == 2 && rout == &rleaf);
	rroot.fields.c0.f2 = &rleaf; rroot.fields.c0.f1.tag = 2;
	assert(ap_export_reverse_identity(arena, &rroot, &rout) == 2 && rout == &rleaf);
}

static void resource_cases(void)
{
	struct ap_c_arena arena = {0}; const struct ap_data_Tree *prior, *out;
	assert(!ap_export_leaf(&arena, 9, &prior)); out = prior;
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	invalid_cases(&arena, prior); assert(arena.first == mark && arena.count == count);
	struct ap_data_Tree leaf = {.tag = 0, .fields.c0 = {INT32_MAX}};
	struct ap_data_Tree root = {.tag = 1, .fields.c1 = {{1}, &leaf, &leaf}};
	int32_t value;
	assert(!ap_export_sum(&arena, &root, &value) && value == -2);
	assert(!ap_export_shift(&arena, &leaf, 1, &out) && out->fields.c0.f0 == INT32_MIN);
	mark = arena.first; count = arena.count;
	out = prior; arena.capacity = count + 1;
	assert(ap_export_mirror(&arena, &root, &out) == 3 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 1;
	assert(ap_export_mirror(&arena, &root, &out) == 4 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.depth_limit = 0;
	allocations = 0; assert(!ap_export_mirror(&arena, &root, &out)); size_t calls = allocations;
	mark = arena.first; count = arena.count;
	for (size_t failure = 1; failure <= calls; ++failure) {
		allocations = 0; fail_at = failure; out = prior;
		assert(ap_export_mirror(&arena, &root, &out) == 3 && out == prior);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_at = 0;
	}
	assert(!ap_export_fork(&arena, (struct ap_enum_Bool){1}, prior, prior, &out));
	assert(out->fields.c1.f1 == prior && out->fields.c1.f2 == prior);
	struct ap_data_Tree deep[400]; const struct ap_data_Tree *input = &leaf;
	for (size_t i = 0; i < 400; ++i) {
		deep[i] = (struct ap_data_Tree){.tag = 1, .fields.c1 = {{0}, input, &leaf}}; input = &deep[i];
	}
	arena.depth_limit = 1; allocations = 0;
	assert(!ap_export_identity(&arena, input, &out) && out == input); calls = allocations;
	mark = arena.first; count = arena.count;
	for (size_t failure = 1; failure <= calls; ++failure) {
		allocations = 0; fail_at = failure; out = prior; arena.status = 77;
		assert(ap_export_identity(&arena, input, &out) == 3 && out == prior && arena.status == 77);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_at = 0;
	}
	arena.depth_limit = 0; out = prior;
	assert(ap_export_mirror(&arena, input, &out) == 4 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	for (size_t i = 0; i < 400; ++i) deep[i].fields.c1.f2 = deep[i].fields.c1.f1;
	arena.depth_limit = 1;
	assert(!ap_export_identity(&arena, input, &out) && out == input);
	assert(prior->fields.c0.f0 == 9); ap_arena_Tree_destroy(&arena);
}

int main(void)
{
	finite_cases(); resource_cases();
	struct ap_data_Tree a = {.tag = 0, .fields.c0 = {2}}, b = {.tag = 0, .fields.c0 = {3}}, c = {.tag = 0, .fields.c0 = {4}};
	struct ap_data_Tree inner = {.tag = 1, .fields.c1 = {{0}, &b, &c}}, root = {.tag = 1, .fields.c1 = {{1}, &a, &inner}};
	struct ap_c_arena arena = {0}; const struct ap_data_Tree *out; int32_t value;
	assert(!ap_export_sum(&arena, &root, &value)); printf("%" PRId32, value);
	assert(!ap_export_mirror(&arena, &root, &out)); assert(!ap_export_sum(&arena, out, &value)); printf("%" PRId32, value);
	assert(!ap_export_shift(&arena, &root, 1, &out)); assert(!ap_export_sum(&arena, out, &value)); printf("%" PRId32, value);
	assert(!ap_export_identity(&arena, &root, &out)); assert(!ap_export_sum(&arena, out, &value)); printf("%" PRId32, value);
	struct ap_data_Reverse nodes[7]; size_t used = 0; const struct ap_data_Reverse *input = reverse(&root, nodes, &used), *result;
	assert(!ap_export_reverse_sum(&arena, input, &value)); printf("%" PRId32, value);
	assert(!ap_export_reverse_mirror(&arena, input, &result)); assert(!ap_export_reverse_sum(&arena, result, &value)); printf("%" PRId32, value);
	ap_arena_Tree_destroy(&arena); return 0;
}
