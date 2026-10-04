#include "component.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t attempts, fail_on;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	return ++attempts == fail_on ? NULL : __real_malloc(size);
}

static struct ap_data_Envelope value(size_t choice)
{
	struct ap_data_Envelope e; memset(&e, 0xff, sizeof(e));
	if (!choice) { e.tag = 0; return e; }
	e.tag = 1; struct ap_data_Packet *p = &e.fields.c1.f0;
	if (choice == 1) { p->tag = 0; e.fields.c1.f1 = -1; }
	else if (choice < 4) {
		p->tag = 1; p->fields.c1.f0 = choice == 2 ? INT32_MAX : -7;
		p->fields.c1.f1.tag = choice == 2; e.fields.c1.f1 = choice == 2 ? 0 : 3;
	} else {
		p->tag = 2; p->fields.c2.f0 = choice == 4 ? INT64_MIN : INT64_MAX;
		p->fields.c2.f1 = choice == 4 ? 23 : INT32_MAX; e.fields.c1.f1 = choice == 4 ? -6 : 1;
	}
	return e;
}

static void same_value(struct ap_data_Envelope a, struct ap_data_Envelope b)
{
	assert(a.tag == b.tag); if (!a.tag) return;
	assert(a.fields.c1.f1 == b.fields.c1.f1);
	struct ap_data_Packet x = a.fields.c1.f0, y = b.fields.c1.f0;
	assert(x.tag == y.tag);
	if (x.tag == 1) assert(x.fields.c1.f0 == y.fields.c1.f0 && x.fields.c1.f1.tag == y.fields.c1.f1.tag);
	if (x.tag == 2) assert(x.fields.c2.f0 == y.fields.c2.f0 && x.fields.c2.f1 == y.fields.c2.f1);
}

static uint32_t measure(struct ap_data_Envelope e)
{
	if (!e.tag) return 0;
	uint32_t n = (uint32_t)e.fields.c1.f1; struct ap_data_Packet p = e.fields.c1.f0;
	if (p.tag == 1) n += (uint32_t)p.fields.c1.f0 + p.fields.c1.f1.tag;
	if (p.tag == 2) n += (uint32_t)p.fields.c2.f1;
	return n;
}

static int32_t signed_bits(uint32_t n)
{
	return n <= INT32_MAX ? (int32_t)n : -1 - (int32_t)(UINT32_MAX - n);
}

static void same(const struct ap_data_Tree *a, const struct ap_data_Tree *b)
{
	assert(a->tag == b->tag);
	if (!a->tag) { same_value(a->fields.c0.f0, b->fields.c0.f0); return; }
	same_value(a->fields.c1.f1, b->fields.c1.f1);
	same(a->fields.c1.f0, b->fields.c1.f2); same(a->fields.c1.f2, b->fields.c1.f0);
}

static void ordinary(void)
{
	struct ap_data_Tree leaves[6]; struct ap_c_arena arena = {0}; size_t cases = 0;
	for (size_t i = 0; i < 6; ++i) leaves[i] = (struct ap_data_Tree){.tag = 0, .fields.c0 = {value(i)}};
	for (size_t left = 0; left < 6; ++left) for (size_t payload = 0; payload < 6; ++payload)
		for (size_t right = 0; right < 6; ++right) {
			struct ap_data_Tree tree = {.tag = 1, .fields.c1 = {&leaves[left], value(payload), &leaves[right]}};
			struct ap_data_Reverse a = {.tag = 1, .fields.c1 = {value(left)}}, b = {.tag = 1, .fields.c1 = {value(right)}};
			struct ap_data_Reverse reversed = {.tag = 0, .fields.c0 = {&a, &b, value(payload)}};
			const struct ap_data_Tree *out; const struct ap_data_Reverse *rout; int32_t number;
			assert(!ap_export_identity(&arena, &tree, &out) && out == &tree);
			assert(!ap_export_mirror(&arena, &tree, &out)); same(&tree, out);
			assert(!ap_export_sum(&arena, out, &number));
			assert(number == signed_bits(measure(value(left)) + measure(value(payload)) + measure(value(right))));
			assert(!ap_export_reverse_identity(&arena, &reversed, &rout) && rout == &reversed);
			assert(!ap_export_reverse_mirror(&arena, &reversed, &rout));
			same_value(rout->fields.c0.f2, value(payload));
			same_value(rout->fields.c0.f0->fields.c1.f0, value(right));
			same_value(rout->fields.c0.f1->fields.c1.f0, value(left));
			int32_t other; assert(!ap_export_reverse_sum(&arena, rout, &other) && other == number);
			ap_arena_Tree_destroy(&arena); ++cases;
		}
	assert(cases == 216);
	for (size_t i = 0; i < 6; ++i) {
		const struct ap_data_Tree *out; struct ap_data_Envelope input = value(i);
		assert(!ap_export_leaf(&arena, input, &out)); input.tag = 99; same_value(value(i), out->fields.c0.f0);
		assert(!ap_export_mirror(&arena, &leaves[i], &out)); same(&leaves[i], out);
		assert(!ap_export_fork(&arena, &leaves[i], value(i), &leaves[i], &out));
		assert(out->fields.c1.f0 == &leaves[i] && out->fields.c1.f2 == &leaves[i]); same_value(value(i), out->fields.c1.f1);
		struct ap_data_Reverse reversed = {.tag = 1, .fields.c1 = {value(i)}}; const struct ap_data_Reverse *rout;
		assert(!ap_export_reverse_identity(&arena, &reversed, &rout) && rout == &reversed);
		assert(!ap_export_reverse_mirror(&arena, &reversed, &rout)); same_value(value(i), rout->fields.c1.f0);
		ap_arena_Tree_destroy(&arena);
	}
	for (unsigned i = 0; i < 2; ++i) {
		struct ap_data_Packet packet; struct ap_data_Envelope e;
		assert(!ap_export_wide_packet(&arena, i ? INT64_MAX : INT64_MIN, -7, &packet));
		assert(packet.tag == 2 && packet.fields.c2.f0 == (i ? INT64_MAX : INT64_MIN) && packet.fields.c2.f1 == -7);
		assert(!ap_export_envelope(&arena, packet, 3, &e) && e.fields.c1.f0.fields.c2.f0 == packet.fields.c2.f0);
	}
	ap_arena_Tree_destroy(&arena);
}

static void failures(void)
{
	struct ap_c_arena arena = {0}; const struct ap_data_Tree *prior, *out;
	assert(!ap_export_leaf(&arena, value(4), &prior)); out = prior;
	struct ap_c_allocation *mark = arena.first; size_t count = arena.count;
	struct ap_data_Tree leaf = {.tag = 0, .fields.c0 = {value(2)}};
	struct ap_data_Tree root = {.tag = 1, .fields.c1 = {&leaf, value(2), &leaf}};
	assert(ap_export_identity(NULL, &root, &out) == 1 && out == prior);
	assert(ap_export_identity(&arena, &root, NULL) == 1);
	assert(ap_export_identity(&arena, NULL, &out) == 2 && out == prior);
	for (size_t where = 0; where < 3; ++where) for (size_t depth = 0; depth < 3; ++depth) {
		struct ap_data_Envelope *e = where == 0 ? &leaf.fields.c0.f0 : where == 1 ? &root.fields.c1.f1 : NULL;
		struct ap_data_Envelope saved = value(2), bad = saved;
		if (!depth) bad.tag = 99;
		else if (depth == 1) bad.fields.c1.f0.tag = 99;
		else bad.fields.c1.f0.fields.c1.f1.tag = 99;
		if (e) *e = bad;
		int32_t number = 77; size_t before = attempts;
		if (e) {
			assert(ap_export_identity(&arena, &root, &out) == 2 && out == prior);
			assert(ap_export_sum(&arena, &root, &number) == 2 && number == 77);
		} else assert(ap_export_leaf(&arena, bad, &out) == 2 && out == prior && attempts == before);
		assert(arena.first == mark && arena.count == count && !arena.depth); if (e) *e = saved;
	}
	struct ap_data_Tree bad = {.tag = 99};
	for (unsigned side = 0; side < 2; ++side) {
		const struct ap_data_Tree **child = side ? &root.fields.c1.f2 : &root.fields.c1.f0;
		*child = &bad; assert(ap_export_identity(&arena, &root, &out) == 2 && out == prior);
		*child = NULL; assert(ap_export_identity(&arena, &root, &out) == 2 && out == prior);
		*child = &root; assert(ap_export_identity(&arena, &root, &out) == 2 && out == prior); *child = &leaf;
	}
	struct ap_data_Reverse rleaf = {.tag = 1, .fields.c1 = {value(2)}};
	struct ap_data_Reverse reversed = {.tag = 0, .fields.c0 = {&rleaf, &rleaf, value(2)}};
	const struct ap_data_Reverse *rout = &rleaf;
	reversed.fields.c0.f2.fields.c1.f0.fields.c1.f1.tag = 99;
	assert(ap_export_reverse_identity(&arena, &reversed, &rout) == 2 && rout == &rleaf);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = count + 1;
	assert(ap_export_mirror(&arena, &root, &out) == 3 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	arena.capacity = 0; arena.depth_limit = 1;
	assert(ap_export_mirror(&arena, &root, &out) == 4 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth); arena.depth_limit = 0;
	attempts = 0; assert(!ap_export_mirror(&arena, &root, &out)); size_t calls = attempts;
	mark = arena.first; count = arena.count;
	for (size_t i = 1; i <= calls; ++i) {
		attempts = 0; fail_on = i; out = prior;
		assert(ap_export_mirror(&arena, &root, &out) == 3 && out == prior);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_on = 0;
	}
	struct ap_data_Tree deep[400]; const struct ap_data_Tree *input = &leaf;
	for (size_t i = 0; i < 400; ++i) {
		deep[i] = (struct ap_data_Tree){.tag = 1, .fields.c1 = {input, value(i % 6), input}}; input = &deep[i];
	}
	arena.depth_limit = 1; attempts = 0;
	assert(!ap_export_identity(&arena, input, &out) && out == input); calls = attempts;
	for (size_t i = 1; i <= calls; ++i) {
		attempts = 0; fail_on = i; out = prior; arena.status = 77;
		assert(ap_export_identity(&arena, input, &out) == 3 && out == prior && arena.status == 77);
		assert(arena.first == mark && arena.count == count && !arena.depth); fail_on = 0;
	}
	arena.depth_limit = 0; out = prior;
	assert(ap_export_mirror(&arena, input, &out) == 4 && out == prior);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	same_value(prior->fields.c0.f0, value(4)); ap_arena_Tree_destroy(&arena);
}

int main(void)
{
	ordinary(); failures();
	struct ap_data_Envelope e1 = {.tag = 1, .fields.c1 = {{.tag = 1, .fields.c1 = {7,{1}}},3}};
	struct ap_data_Envelope e2 = {.tag = 1, .fields.c1 = {{.tag = 0},5}};
	struct ap_data_Tree a = {.tag = 0, .fields.c0 = {e1}}, b = {.tag = 0};
	struct ap_data_Tree root = {.tag = 1, .fields.c1 = {&a,e2,&b}};
	struct ap_data_Reverse ra = {.tag = 1, .fields.c1 = {e1}}, rb = {.tag = 1};
	struct ap_data_Reverse rroot = {.tag = 0, .fields.c0 = {&ra,&rb,e2}};
	struct ap_c_arena arena = {0}; const struct ap_data_Tree *out; const struct ap_data_Reverse *rout; int32_t n;
	assert(!ap_export_sum(&arena, &root, &n)); printf("%" PRId32, n);
	assert(!ap_export_mirror(&arena, &root, &out) && !ap_export_sum(&arena, out, &n)); printf("%" PRId32, n);
	assert(!ap_export_reverse_sum(&arena, &rroot, &n)); printf("%" PRId32, n);
	assert(!ap_export_reverse_mirror(&arena, &rroot, &rout) && !ap_export_reverse_sum(&arena, rout, &n)); printf("%" PRId32, n);
	ap_arena_Tree_destroy(&arena); return 0;
}
