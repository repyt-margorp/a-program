BUFFER_QUERY := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(BUFFER_QUERY)../build.mk

.PHONY: check-c-buffer-query
check-c-buffer-query: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_buffer_query_inert_test
	bash $(BUFFER_QUERY)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_buffer_query_inert_test

$(BUILD)/c_buffer_query_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(BUFFER_QUERY)inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(BUFFER_QUERY)inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@
