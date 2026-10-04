#define ap_export_direct_keep32 ap_export_known_keep32
#define ap_export_direct_drop32 ap_export_known_drop32
#define ap_export_direct_keep64 ap_export_known_keep64
#define ap_export_direct_drop64 ap_export_known_drop64
#define ap_export_direct_filter32 ap_export_captured_filter32
#define ap_export_direct_filter64 ap_export_captured_filter64
#define ap_export_direct_select32 ap_export_captured_select32
#define ap_export_direct_select64 ap_export_captured_select64
#define ap_arena_Numbers32_destroy ap_arena_Nat_destroy
#define main integer_boundary_main
#include "../predicate_integer/client.c"
#undef main

static int32_t signed32(uint32_t bits)
{
	return bits <= INT32_MAX ? (int32_t)bits : INT32_MIN + (int32_t)(bits - UINT32_C(0x80000000));
}

static int64_t signed64(uint64_t bits)
{
	return bits <= INT64_MAX ? (int64_t)bits : INT64_MIN + (int64_t)(bits - UINT64_C(0x8000000000000000));
}

static void captures_and_maps(void)
{
	struct ap_c_arena arena = {0};
	const int32_t digits32[] = {INT32_MIN, -1, 0, INT32_MAX};
	const int64_t digits64[] = {INT64_MIN, -1, 0, INT64_MAX};
	const int32_t offsets32[] = {0, -1, 1, INT32_MIN, INT32_MAX};
	const int64_t offsets64[] = {0, -1, 1, INT64_MIN, INT64_MAX};
	size_t cases = 0, combinations = 1;
	for (size_t length = 0; length <= 3; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		int32_t values32[3], buffer32[4];
		int64_t values64[3], buffer64[4];
		size_t digits = code, written;
		for (size_t i = 0; i < length; ++i) { values32[i] = digits32[digits % 4]; values64[i] = digits64[digits % 4]; digits /= 4; }
		const struct ap_data_Numbers32 *input32, *out32;
		const struct ap_data_Numbers64 *input64, *out64;
		assert(!ap_from_Numbers32(&arena, values32, length, &input32));
		assert(!ap_from_Numbers64(&arena, values64, length, &input64));
		for (unsigned flag = 0; flag < 2; ++flag) for (unsigned other = 0; other < 2; ++other) {
			for (unsigned form = 0; form < 3; ++form) {
				int status = !form ? ap_export_chain_filter32(&arena, (struct ap_enum_Bool){flag}, (struct ap_enum_Bool){other}, input32, &out32) :
					form == 1 ? ap_export_shadow_filter32(&arena, (struct ap_enum_Bool){flag}, (struct ap_enum_Bool){other}, input32, &out32) :
					ap_export_unused_effect(&arena, (struct ap_enum_Bool){flag}, input32, &out32);
				assert(!status && !arena.depth);
				assert(!ap_copy_Numbers32(out32, buffer32, 3, &written) && written == (flag ? length : 0));
				if (flag) for (size_t i = 0; i < length; ++i) assert(buffer32[i] == values32[i]);
			}
		}
		for (size_t first = 0; first < 5; ++first) {
			assert(!ap_export_shift32(&arena, offsets32[first], input32, &out32));
			assert(!ap_copy_Numbers32(out32, buffer32, 3, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer32[i] == signed32((uint32_t)values32[i] + (uint32_t)offsets32[first]));
			assert(!ap_export_shift64(&arena, offsets64[first], input64, &out64));
			assert(!ap_copy_Numbers64(out64, buffer64, 3, &written) && written == length);
			for (size_t i = 0; i < length; ++i) assert(buffer64[i] == signed64((uint64_t)values64[i] + (uint64_t)offsets64[first]));
			for (size_t second = 0; second < 5; ++second) {
				assert(!ap_export_chain_map32(&arena, offsets32[first], offsets32[second], input32, &out32));
				assert(!ap_copy_Numbers32(out32, buffer32, 3, &written) && written == length);
				for (size_t i = 0; i < length; ++i)
					assert(buffer32[i] == signed32((uint32_t)values32[i] + (uint32_t)offsets32[first] + (uint32_t)offsets32[second]));
			}
		}
		assert(!ap_copy_Numbers32(input32, buffer32, 3, &written) && written == length);
		for (size_t i = 0; i < length; ++i) assert(buffer32[i] == values32[i]);
		assert(!ap_copy_Numbers64(input64, buffer64, 3, &written) && written == length);
		for (size_t i = 0; i < length; ++i) assert(buffer64[i] == values64[i]);
		ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 85);
}

static void natural_predicates(void)
{
	struct ap_c_arena arena = {0};
	size_t cases = 0, combinations = 1;
	for (size_t length = 0; length <= 3; ++length, combinations *= 4) for (size_t code = 0; code < combinations; ++code) {
		uint32_t values[3], buffer[4] = {77, 77, 77, 77};
		size_t digits = code, written, expected = 0;
		for (size_t i = 0; i < length; ++i) { values[i] = digits % 4; digits /= 4; }
		const struct ap_data_Naturals *input, *out;
		assert(!ap_from_Naturals(&arena, values, length, &input));
		assert(!ap_export_known_filter_nat(&arena, input, &out));
		assert(!ap_copy_Naturals(out, buffer, 3, &written));
		for (size_t i = 0; i < length; ++i) if (values[i]) assert(buffer[expected++] == values[i]);
		assert(written == expected && buffer[3] == 77);
		for (uint32_t pivot = 0; pivot <= 4; ++pivot) {
			assert(!ap_export_known_select_nat(&arena, input, pivot, &out));
			assert(!ap_copy_Naturals(out, buffer, 3, &written)); expected = 0;
			for (size_t i = 0; i < length; ++i) if (values[i] <= pivot) assert(buffer[expected++] == values[i]);
			assert(written == expected && buffer[3] == 77 && !arena.depth);
		}
		assert(!ap_copy_Naturals(input, buffer, 3, &written) && written == length);
		for (size_t i = 0; i < length; ++i) assert(buffer[i] == values[i]);
		ap_arena_Nat_destroy(&arena); ++cases;
	}
	assert(cases == 85);
	const uint32_t values[] = {UINT32_MAX, 0};
	const struct ap_data_Naturals *input, *out;
	static const struct ap_data_Naturals nil = {.tag = AP_DATA_Naturals_C0};
	assert(!ap_from_Naturals(&arena, values, 2, &input));
	struct ap_c_allocation *mark = arena.first;
	size_t count = arena.count, written;
	out = &nil;
	assert(ap_export_known_select_nat(&arena, input, UINT32_MAX, &out) == 4 && out == &nil);
	assert(arena.first == mark && arena.count == count && !arena.depth);
	assert(!ap_export_known_select_nat(&arena, input, 0, &out));
	uint32_t buffer[2];
	assert(!ap_copy_Naturals(out, buffer, 2, &written) && written == 1 && !buffer[0]);
	assert(!ap_export_known_filter_nat(&arena, input, &out));
	assert(!ap_copy_Naturals(out, buffer, 2, &written) && written == 1 && buffer[0] == UINT32_MAX);
	ap_arena_Nat_destroy(&arena);
}

static void capture_reference(void)
{
	struct ap_c_arena arena = {0};
	const int32_t values[] = {-1, 0, 1};
	const struct ap_data_Numbers32 *input, *out;
	assert(!ap_from_Numbers32(&arena, values, 3, &input));
	for (unsigned form = 0; form < 7; ++form) {
		int status;
		if (form < 2) status = ap_export_chain_filter32(&arena, (struct ap_enum_Bool){!form}, (struct ap_enum_Bool){form}, input, &out);
		else if (form < 4) status = ap_export_shadow_filter32(&arena, (struct ap_enum_Bool){form == 2}, (struct ap_enum_Bool){form == 3}, input, &out);
		else if (form == 4) status = ap_export_shift32(&arena, INT32_MAX, input, &out);
		else if (form == 5) status = ap_export_chain_map32(&arena, INT32_MAX, 1, input, &out);
		else status = ap_export_unused_effect(&arena, (struct ap_enum_Bool){1}, input, &out);
		assert(!status);
		int32_t buffer[3];
		size_t written;
		assert(!ap_copy_Numbers32(out, buffer, 3, &written));
		for (size_t i = 0; i < written; ++i) printf("%" PRId32 ",", buffer[i]);
		putchar('|');
	}
	const uint32_t natural_values[] = {0, 2, 1, 2};
	const struct ap_data_Naturals *natural_input, *natural_out;
	assert(!ap_from_Naturals(&arena, natural_values, 4, &natural_input));
	for (unsigned form = 0; form < 3; ++form) {
		int status = !form ? ap_export_known_filter_nat(&arena, natural_input, &natural_out) : ap_export_known_select_nat(&arena, natural_input, form, &natural_out);
		assert(!status);
		uint32_t buffer[4];
		size_t written;
		assert(!ap_copy_Naturals(natural_out, buffer, 4, &written));
		for (size_t i = 0; i < written; ++i) printf("%u,", (unsigned)buffer[i]);
		putchar('|');
	}
	ap_arena_Nat_destroy(&arena);
}

int main(void)
{
	assert(!integer_boundary_main());
	captures_and_maps(); natural_predicates(); capture_reference();
	return 0;
}
