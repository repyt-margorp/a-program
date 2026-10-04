#include "fold.h"

/* Expected compiler refusal: the folded callable takes SizedList, not Acc. */
int apply_wrong(const struct af_result *folded, const struct iv_acc *access, const struct iv_list **out)
{
	return af_apply(folded,access,out);
}
