#ifndef __ACTUAL_ACC_MODULE_H__
#define __ACTUAL_ACC_MODULE_H__

#include "../acc_quicksort_mockup/mockup.h"

/* Private candidate entry, same signature already used by C36/C37.
	* Actual Acc/partition/append bodies are emitted; remaining runtime,
	* accessibility/comparison/measure/outer orchestration remains manual.
	* Borrowed Nat32 arrays; failure preserves buffer/written. Depth256,
	* node65536; statuses1 pointer,2 metadata,3 allocation,4 depth,
	* 5 Nat32 overflow,6 capacity. No public ap_export/native profile change. */
int gs_sort(const uint32_t *, size_t, uint32_t *, size_t, size_t *, struct qs_trace *);

#endif
