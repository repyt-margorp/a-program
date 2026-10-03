#include "adapter.h"

/* Nominally different descriptor structs are not interchangeable. */
int main(void)
{
	struct module_provider_api api = {0};
	struct ap_enum_LFlag output;
	return ap_export_left_apply(NULL, module_right_unary(&api), 0, &output);
}
