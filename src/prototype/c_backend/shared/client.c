#define _POSIX_C_SOURCE 200809L
#include "component.h"
#include <assert.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef DYNAMIC_CLIENT
static void *symbol(void *library, const char *name)
{
	dlerror();
	void *address = dlsym(library, name);
	assert(address && !dlerror());
	return address;
}
#define ADDRESS(name) symbol(library, #name)
#else
#define ADDRESS(name) (&name)
#endif

int main(int argc, char **argv)
{
#ifdef DYNAMIC_CLIENT
	assert(argc == 2);
	void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	assert(library);
	assert(!dlsym(library, "ap_apply"));
	assert(!dlsym(library, "pg_eval_advance"));
#else
	(void)argc; (void)argv;
#endif
#ifdef SCALAR_CLIENT
	int (*add)(int32_t, int32_t, int32_t *) = ADDRESS(ap_export_add);
	int (*add64)(int64_t, int64_t, int64_t *) = ADDRESS(ap_export_add64);
	int (*identity)(int32_t, int32_t *) = ADDRESS(ap_export_identity);
	int32_t out = 17;
	int64_t wide = 19;
	assert(add(1, 2, NULL) == 1);
	assert(!add(INT32_MAX, 1, &out) && out == INT32_MIN);
	assert(!add64(INT64_MAX, 1, &wide) && wide == INT64_MIN);
	for (int32_t n = -32; n <= 32; ++n) {
		assert(!add(n, 7, &out) && out == n + 7);
		assert(!identity(n, &out) && out == n);
		assert(!add64(INT64_MIN, n, &wide));
		assert((uint64_t)wide == (uint64_t)INT64_MIN + (uint64_t)(int64_t)n);
	}
#elif defined(RECORD_CLIENT)
	void (*destroy)(struct ap_c_arena *) = ADDRESS(ap_arena_Records_destroy);
	int (*from)(struct ap_c_arena *, const struct ap_data_Envelope *, size_t,
		const struct ap_data_Records **) = ADDRESS(ap_from_Records);
	int (*copy)(const struct ap_data_Records *, struct ap_data_Envelope *, size_t, size_t *) = ADDRESS(ap_copy_Records);
	int (*length)(struct ap_c_arena *, const struct ap_data_Records *, int32_t *) = ADDRESS(ap_export_length);
	int (*sum)(struct ap_c_arena *, const struct ap_data_Records *, int32_t *) = ADDRESS(ap_export_sum);
	int (*append)(struct ap_c_arena *, const struct ap_data_Records *, const struct ap_data_Records *,
		const struct ap_data_Records **) = ADDRESS(ap_export_append);
	struct ap_data_Envelope values[9] = {0}, copied[18] = {0}, prior[18];
	for (size_t i = 0; i < 9; ++i) {
		values[i].tag = AP_DATA_Envelope_C1;
		values[i].fields.c1.f1 = 3;
		struct ap_data_Packet *packet = &values[i].fields.c1.f0;
		packet->tag = i % 2 ? AP_DATA_Packet_C2 : AP_DATA_Packet_C1;
		if (i % 2) {
			packet->fields.c2.f0 = i % 4 == 1 ? INT64_MIN : INT64_MAX;
			packet->fields.c2.f1 = (int32_t)i;
		} else {
			packet->fields.c1.f0 = (int32_t)i;
			packet->fields.c1.f1.tag = AP_ENUM_Bool_C1;
		}
	}
	for (size_t n = 0; n <= 9; ++n) {
		struct ap_c_arena arena = {0};
		const struct ap_data_Records *xs = NULL, *twice = NULL;
		assert(!from(&arena, values, n, &xs));
		int32_t result = -1, expected = 0;
		for (size_t i = 0; i < n; ++i) expected += (int32_t)i + 3 + (i % 2 ? 0 : 1);
		assert(!length(&arena, xs, &result) && result == (int32_t)n);
		assert(!sum(&arena, xs, &result) && result == expected);
		assert(!append(&arena, xs, xs, &twice));
		size_t written = 99;
		assert(!copy(twice, copied, 18, &written) && written == 2 * n);
		for (size_t i = 0; i < written; ++i) {
			assert(copied[i].tag == values[i % n].tag);
			const struct ap_data_Packet *p = &copied[i].fields.c1.f0;
			const struct ap_data_Packet *v = &values[i % n].fields.c1.f0;
			assert(p->tag == v->tag && copied[i].fields.c1.f1 == 3);
			if (i % n % 2) assert(p->fields.c2.f0 == v->fields.c2.f0 && p->fields.c2.f1 == v->fields.c2.f1);
			else assert(p->fields.c1.f0 == v->fields.c1.f0 && p->fields.c1.f1.tag == AP_ENUM_Bool_C1);
		}
		if (n) {
			memcpy(prior, copied, sizeof(prior)); written = 99;
			assert(copy(twice, copied, 2 * n - 1, &written) == 6);
			assert(written == 99 && !memcmp(prior, copied, sizeof(prior)));
		}
		size_t count = arena.count;
		struct ap_c_allocation *first = arena.first;
		const struct ap_data_Records *unchanged = xs;
		arena.capacity = count + 1;
		assert(from(&arena, values, 2, &unchanged) == 3);
		assert(unchanged == xs && arena.count == count && arena.first == first);
		arena.capacity = 0;
		values[0].fields.c1.f0.fields.c1.f1.tag = 99;
		assert(from(&arena, values, 1, &unchanged) == 2);
		assert(unchanged == xs && arena.count == count && arena.first == first);
		values[0].fields.c1.f0.fields.c1.f1.tag = AP_ENUM_Bool_C1;
		if (n > 2) {
			arena.depth_limit = 1;
			assert(length(&arena, xs, &result) == 4);
			assert(result == expected && arena.count == count && arena.first == first);
			assert(append(&arena, xs, xs, &unchanged) == 4);
			assert(unchanged == xs && arena.count == count && arena.first == first);
			arena.depth_limit = 0;
		}
		assert(!length(&arena, xs, &result) && result == (int32_t)n);
		destroy(&arena); destroy(&arena);
		assert(!arena.count && !arena.first);
	}
	struct ap_data_Records cycle = {.tag = AP_DATA_Records_C1};
	cycle.fields.c1.f1 = &cycle;
	size_t written = 99;
	memcpy(prior, copied, sizeof(prior));
	assert(copy(&cycle, copied, 18, &written) == 2);
	assert(written == 99 && !memcmp(prior, copied, sizeof(prior)));
#elif defined(NAT_CLIENT)
	void (*destroy)(struct ap_c_arena *) = ADDRESS(ap_arena_Nat_destroy);
	int (*from)(struct ap_c_arena *, const uint32_t *, size_t,
		const struct ap_data_Numbers **) = ADDRESS(ap_from_Numbers);
	int (*copy)(const struct ap_data_Numbers *, uint32_t *, size_t, size_t *) = ADDRESS(ap_copy_Numbers);
	int (*length)(struct ap_c_arena *, const struct ap_data_Numbers *, int32_t *) = ADDRESS(ap_export_length);
	struct ap_c_arena arena = {0};
	uint32_t values[] = {0, UINT32_MAX, 17}, copied[3] = {0};
	const struct ap_data_Numbers *xs = NULL;
	assert(!from(&arena, values, 3, &xs));
	int32_t result = -1;
	assert(!length(&arena, xs, &result) && result == 3);
	size_t written = 99;
	assert(!copy(xs, copied, 3, &written) && written == 3);
	assert(!memcmp(values, copied, sizeof(values)));
	destroy(&arena);
#else
	int (*first)(void) = ADDRESS(ap_export_first);
	int (*second)(void) = ADDRESS(ap_export_second);
	int (*duplicate)(void) = ADDRESS(ap_export_duplicate);
	int (*noop)(void) = ADDRESS(ap_export_noop);
	for (int i = 0; i < 2; ++i) {
		assert(!first() && !second() && !duplicate() && !noop());
	}
	assert(!fflush(stdout));
	int output = dup(fileno(stdout));
	assert(output >= 0 && freopen("/dev/full", "w", stdout));
	assert(first() == 2);
	assert(dup2(output, fileno(stdout)) >= 0 && !close(output));
	clearerr(stdout);
	assert(!second() && !first());
#endif
#ifdef DYNAMIC_CLIENT
	/* Calls and arena destruction above finish before releasing code storage. */
	assert(!dlclose(library));
#endif
	return 0;
}
