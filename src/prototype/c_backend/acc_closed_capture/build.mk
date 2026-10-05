ACC_CLOSED := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_CLOSED)../acc_create_command/build.mk

$(BUILD)/c-acc-closed-plan: $(SOURCES) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CLOSED)plan.c $(ACC_CLOSED)build.mk $(C_BACKEND)link/plan.c $(C_BACKEND)link/plan.h $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ACC_CLOSED)plan.c $(C_BACKEND)link/plan.c $(C_BACKEND)emit.c -o $@

$(BUILD)/c-acc-closed-image-fault: ACC_CLOSED_FAULT_SOURCE = $(ACC_CLOSED)../acc_command/fault.c
$(BUILD)/c-acc-closed-image-fault: ACC_CLOSED_FAULT_WRAPS = -Wl,--wrap=fopen -Wl,--wrap=fwrite -Wl,--wrap=fclose -Wl,--wrap=mkstemp
$(BUILD)/c-acc-closed-image-fault: $(ACC_CLOSED)../acc_command/fault.c

$(BUILD)/c-acc-closed-image $(BUILD)/c-acc-closed-image-fault: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CLOSED)main.c $(ACC_CLOSED)emit.c $(ACC_CLOSED)emit.h $(ACC_CLOSED)build.mk $(ACC_BODY_SOURCES) $(ACC_BODY_OWNERS)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_BODY_SOURCES) $(ACC_CLOSED)emit.c $(ACC_CLOSED)main.c $(ACC_CLOSED_FAULT_SOURCE) $(ACC_CLOSED_FAULT_WRAPS) -Wl,--wrap=tmpfile -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
