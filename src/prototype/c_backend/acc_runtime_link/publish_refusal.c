#include "../link/plan.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>

/* Candidate dispatch must refuse before artifact/output access or native tools,
	* even with otherwise incomplete caller-owned plan data. */
FILE *__wrap_fopen(const char *path, const char *mode)
{
	(void)path; (void)mode; assert(0); return NULL;
}

int __wrap_lstat(const char *path, struct stat *value)
{
	(void)path; (void)value; assert(0); return -1;
}

int main(void)
{
	const enum pg_c_lowering candidates[]={PG_C_ACC_CREATION_CANDIDATE,
		PG_C_ACC_COMPARATOR_CANDIDATE,PG_C_ACC_RUNTIME_BOOL_CANDIDATE};
	for (size_t i=0;i<sizeof(candidates)/sizeof(*candidates);++i) {
		struct pg_c_link_plan plan={.lowering=candidates[i]};
		assert(pg_c_link_publish(&plan,"absent-output","absent-cc","absent-ar",NULL,0,0)==4);
	}
	return 0;
}
