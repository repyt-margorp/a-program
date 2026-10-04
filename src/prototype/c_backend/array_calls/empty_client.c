#include "component.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int fail_all;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t size)
{
	return fail_all ? NULL : __real_malloc(size);
}

int main(void)
{
	size_t n = 77; int32_t out = 77;
	assert(ap_buffer_empty(&out,1,NULL) == 1 && out == 77);
	assert(!ap_buffer_empty(NULL,0,&n) && !n);
	struct ap_enum_Bool flags[2] = {{77},{77}};
	assert(!ap_buffer_prepend_flag((struct ap_enum_Bool){1},NULL,0,flags,2,&n) && n == 1 && flags[0].tag == 1);
	n = 77; assert(ap_buffer_prepend_flag((struct ap_enum_Bool){77},NULL,0,flags,2,&n) == 2 && n == 77 && flags[0].tag == 1);
	struct ap_data_Packet packet = {.tag = 1,.fields.c1 = {INT32_MIN,{0}}}, packets[2];
	assert(!ap_buffer_prepend_packet(packet,NULL,0,packets,2,&n) && n == 1 && packets[0].fields.c1.f0 == INT32_MIN);
	struct ap_data_Packet saved[2]; memcpy(saved,packets,sizeof(saved)); packet.fields.c1.f1.tag = 77; n = 77;
	assert(ap_buffer_prepend_packet(packet,NULL,0,packets,2,&n) == 2 && n == 77 && !memcmp(saved,packets,sizeof(saved)));
	uint32_t input[] = {0,UINT32_MAX}, copy[3];
	assert(!ap_buffer_prepend_succ(UINT32_MAX-1,input,2,copy,3,&n) && n == 3 && copy[0] == UINT32_MAX && copy[1] == 0 && copy[2] == UINT32_MAX);
	n = 77; assert(ap_buffer_prepend_succ(UINT32_MAX,input,2,copy,3,&n) == 5 && n == 77 && copy[0] == UINT32_MAX && copy[1] == 0 && copy[2] == UINT32_MAX);
#ifndef ARRAY_SHARED
	fail_all = 1; n = 77; assert(ap_buffer_empty(&out,1,&n) == 3 && out == 77 && n == 77); fail_all = 0;
#endif
	return 0;
}
