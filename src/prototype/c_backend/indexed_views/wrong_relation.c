#include "indexed.h"

/* Expected compiler refusal: Acc.down takes an LT value, not a Nat value. */
int invoke(const struct iv_acc *access, const struct iv_nat *value, const struct iv_acc **result)
{
	return access->fields.c0.field1.call(access->fields.c0.field1.context,value,value,result);
}
