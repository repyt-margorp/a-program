#include "adapter.h"

void wrong(const struct native_scalar_provider_context *context)
{
	(void)context;
	struct ap_c_arena arena = {0}; const struct ap_data_MNumbers32 *list = NULL;
#ifdef WRONG_ARITY
	ap_export_module_map32(&arena, native_scalar_binary32(context), list, &list);
#elif defined(WRONG_PROVIDER)
	struct ap_c_callback_i32 callback = {NULL, ap_export_provider_offset32};
	ap_export_module_map32(&arena, callback, list, &list);
#else
	ap_export_module_map32(&arena, native_scalar_unary64(context), list, &list);
#endif
}
