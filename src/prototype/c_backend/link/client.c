#define _POSIX_C_SOURCE 200809L
#include "component.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	assert(AP_C_ISOLATED_ABI == 1);
	assert(!ap_export_first());
	assert(!ap_export_second());
	assert(!ap_export_duplicate());
	assert(!ap_export_noop());
#ifdef LINK_OTHER_COMPONENT
	assert(!ap_export_other());
#endif
#ifdef LINK_FAILURE_TEST
	int output = dup(fileno(stdout));
	assert(output >= 0);
	assert(freopen("/dev/full", "w", stdout));
	assert(ap_export_first() == 2);
	assert(dup2(output, fileno(stdout)) >= 0);
	assert(!close(output));
	clearerr(stdout);
	/* The failed invocation's allocation and jump target must not escape. */
	assert(!ap_export_second());
	assert(!ap_export_first());
#endif
	return 0;
}
