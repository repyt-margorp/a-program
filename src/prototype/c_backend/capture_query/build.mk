CAPTURE_QUERY := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(CAPTURE_QUERY)../build.mk

$(BUILD)/c_capture_query_fault_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(CAPTURE_QUERY)fault_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(CAPTURE_QUERY)fault_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_term_independent -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
