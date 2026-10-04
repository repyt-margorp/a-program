#include "adapter.h"

int main(void)
{
	struct ap_c_arena arena = {0}; struct ap_enum_LFlag answer;
	struct signed_provider_context context = {0};
#ifdef WRONG_WIDTH
	return ap_export_left_apply32(&arena, signed_left_unary64(&context), 0, &answer);
#else
	return ap_export_left_apply32(&arena, signed_right_unary32(&context), 0, &answer);
#endif
}
