#include "numbers.h"
#include "flags.h"

int main(void)
{
	struct ap_c_arena arena = {0};
	const struct ap_data_Numbers *numbers = NULL;
	const struct ap_data_Flags *flags = NULL;
	return ap_export_identity_flags(&arena, numbers, &flags);
}
