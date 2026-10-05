#include "component.h"

static enum gs_bool_mode selected_mode;

static int resource_sort(const uint32_t *input, size_t count, uint32_t *buffer,
	size_t capacity, size_t *written, struct qs_trace *trace)
{
	return gs_sort_mode(selected_mode,input,count,buffer,capacity,written,trace);
}

/* Reuse the actual allocation/depth/capacity/rollback controls; select source
	* data at the caller boundary, never a replacement comparison provider. */
#define gs_sort resource_sort
#define main retained_resource_main
#include "../acc_closed_capture/resource_client.c"
#undef main
#undef gs_sort

int main(int argc, char **argv)
{
	assert(argc==3);
	selected_mode=!strcmp(argv[1],"first") ? GS_BOOL_FIRST : GS_BOOL_SECOND;
	char *selected[] = {argv[0],argv[2]};
	return retained_resource_main(2,selected);
}
