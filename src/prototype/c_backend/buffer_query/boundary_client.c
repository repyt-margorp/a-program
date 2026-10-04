#include "component.h"
#include <assert.h>

int main(void)
{
	int32_t values[300] = {0}; struct ap_c_arena arena = {0};
	const struct ap_data_Numbers32 *list; assert(!ap_from_Numbers32(&arena,values,300,&list));
	size_t written = 77; assert(ap_copy_Numbers32(list,NULL,0,&written) == 6 && written == 77);
	int32_t result = 77; assert(ap_export_length32(&arena,list,&result) == 4 && result == 77);
	assert(!arena.depth); ap_arena_Nat_destroy(&arena); return 0;
}
