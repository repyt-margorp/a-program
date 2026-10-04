#include "indexed.h"
#include <assert.h>

/* This ordinary client exercises extracted representation only. Its callback
	* supplies one known LT.step case; it is not QuickSort or a proof checker. */
static int step_down(void *context, const struct iv_nat *y,
	const struct iv_lt *edge, const struct iv_acc **result)
{
	const struct iv_acc *child = context;
	if (!child || !edge || !result || edge->tag != 0 || edge->index0 != y || child->index0 != y) return 2;
	*result = child; return 0;
}

int main(void)
{
	static const unsigned char nat_id, lt_id, acc_id, sized_id, measured_id, partition_id, list_id;
	const struct iv_nat zero = {.self_identity=&nat_id,.tag=0};
	const struct iv_nat one = {.self_identity=&nat_id,.tag=1,.fields.c1={&zero}};
	const struct iv_lt edge = {.self_identity=&lt_id,.index0=&zero,.index1=&one,.tag=0,.fields.c0={&zero}};
	struct iv_acc child = {.self_identity=&acc_id,.parameter0=&nat_id,.parameter1=&lt_id,.index0=&zero,.tag=0,.fields.c0={.field0=&zero}};
	const struct iv_acc parent = {.self_identity=&acc_id,.parameter0=&nat_id,.parameter1=&lt_id,.index0=&one,.tag=0,.fields.c0={&one,{step_down,&child}}};
	const struct iv_acc *result = &parent;
	assert(!parent.fields.c0.field1.call(parent.fields.c0.field1.context,&zero,&edge,&result));
	assert(result == &child && result->index0 == &zero);
	result = &parent;
	assert(parent.fields.c0.field1.call(parent.fields.c0.field1.context,&one,&edge,&result) == 2 && result == &parent);
	const struct iv_sized empty = {.self_identity=&sized_id,.parameter0=&nat_id,.index0=&zero,.tag=0};
	const struct iv_sized values = {.self_identity=&sized_id,.parameter0=&nat_id,.index0=&one,.tag=1,.fields.c1={&zero,&one,&empty}};
	const struct iv_measured measured = {.self_identity=&measured_id,.parameter0=&nat_id,.tag=0,.fields.c0={&one,&values}};
	const struct iv_partition partition = {.self_identity=&partition_id,.parameter0=&nat_id,.parameter1=&zero,.tag=0,.fields.c0={&zero,&empty,&zero,&empty,&edge,&edge}};
	const struct iv_list nil = {.self_identity=&list_id,.parameter0=&nat_id,.tag=0};
	const struct iv_list cons = {.self_identity=&list_id,.parameter0=&nat_id,.tag=1,.fields.c1={&one,&nil}};
	assert(measured.fields.c0.field0 == measured.fields.c0.field1->index0);
	assert(partition.parameter1 == &zero && partition.fields.c0.field4->index1 == &one);
	assert(cons.fields.c1.field1 == &nil && values.fields.c1.field2->index0 == values.fields.c1.field0);
	return 0;
}
