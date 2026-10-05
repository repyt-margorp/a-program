ACC_RUNTIME := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_RUNTIME)../acc_closed_capture/build.mk

$(BUILD)/c-acc-runtime-image-fault: ACC_RUNTIME_FAULT_SOURCE = $(ACC_RUNTIME)../acc_command/fault.c
$(BUILD)/c-acc-runtime-image-fault: ACC_RUNTIME_FAULT_WRAPS = -Wl,--wrap=fopen -Wl,--wrap=fwrite -Wl,--wrap=fclose -Wl,--wrap=mkstemp
$(BUILD)/c-acc-runtime-image-fault: $(ACC_RUNTIME)../acc_command/fault.c

$(BUILD)/c-acc-runtime-image $(BUILD)/c-acc-runtime-image-fault: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_RUNTIME)main.c $(ACC_RUNTIME)emit.c $(ACC_RUNTIME)emit.h $(ACC_RUNTIME)build.mk $(ACC_BODY_SOURCES) $(ACC_BODY_OWNERS) $(ACC_CLOSED)emit.c $(ACC_CLOSED)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_BODY_SOURCES) $(ACC_RUNTIME)emit.c $(ACC_RUNTIME)main.c $(ACC_RUNTIME_FAULT_SOURCE) $(ACC_RUNTIME_FAULT_WRAPS) -Wl,--wrap=tmpfile -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
