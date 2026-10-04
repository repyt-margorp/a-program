NATIVE_CALLBACK := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(NATIVE_CALLBACK)../build.mk

.PHONY: check-c-native-callbacks
check-c-native-callbacks: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_native_callback_inert_test
	bash $(NATIVE_CALLBACK)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_native_callback_inert_test

$(BUILD)/c_native_callback_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(NATIVE_CALLBACK)inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NATIVE_CALLBACK)inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@
