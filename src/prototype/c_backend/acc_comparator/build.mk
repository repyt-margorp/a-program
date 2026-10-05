ACC_COMPARATOR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_COMPARATOR)../acc_create_command/build.mk

$(BUILD)/c-acc-compare-plan: $(SOURCES) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_COMPARATOR)plan.c $(ACC_COMPARATOR)build.mk $(C_BACKEND)link/plan.c $(C_BACKEND)link/plan.h $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ACC_COMPARATOR)plan.c $(C_BACKEND)link/plan.c $(C_BACKEND)emit.c -o $@

$(BUILD)/c-acc-compare-image-fault: ACC_COMPARE_FAULT_SOURCE = $(ACC_COMPARATOR)../acc_command/fault.c
$(BUILD)/c-acc-compare-image-fault: ACC_COMPARE_FAULT_WRAPS = -Wl,--wrap=fopen -Wl,--wrap=fwrite -Wl,--wrap=fclose -Wl,--wrap=mkstemp
$(BUILD)/c-acc-compare-image-fault: $(ACC_COMPARATOR)../acc_command/fault.c

$(BUILD)/c-acc-compare-image $(BUILD)/c-acc-compare-image-fault: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_COMPARATOR)build.mk $(ACC_COMPARATOR)main.c $(ACC_BODY_SOURCES) $(ACC_BODY_OWNERS) $(ACC_CREATE_COMMAND)../acc_recipe/emit.c $(ACC_CREATE_COMMAND)../acc_indices/emit.c $(ACC_CREATE_COMMAND)../acc_frame/emit.c $(ACC_CREATE_COMMAND)../acc_endpoints/emit.c $(ACC_CREATE_COMMAND)../acc_transport/emit.c $(ACC_CREATE_COMMAND)../acc_capture/emit.c $(ACC_CREATE_COMMAND)../acc_actions/emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_BODY_SOURCES) $(ACC_COMPARATOR)main.c $(ACC_COMPARE_FAULT_SOURCE) $(ACC_COMPARE_FAULT_WRAPS) -Wl,--wrap=tmpfile -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
