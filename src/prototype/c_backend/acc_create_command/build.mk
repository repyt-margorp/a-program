ACC_CREATE_COMMAND := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_CREATE_COMMAND)../build.mk

ACC_BODY_SOURCES := $(ACC_CREATE_COMMAND)../source_observer/observe.c $(ACC_CREATE_COMMAND)../acc_create/emit.c $(ACC_CREATE_COMMAND)../acc_accessibility/emit.c $(ACC_CREATE_COMMAND)../acc_comparison/emit.c $(ACC_CREATE_COMMAND)../acc_partition/emit.c $(ACC_CREATE_COMMAND)../acc_append/emit.c $(ACC_CREATE_COMMAND)../acc_clause/emit.c $(ACC_CREATE_COMMAND)../acc_fold/emit.c $(ACC_CREATE_COMMAND)../acc_measure/emit.c $(ACC_CREATE_COMMAND)../acc_endpoints/read.c $(ACC_CREATE_COMMAND)../indexed_views/view.c
ACC_BODY_OWNERS := $(wildcard $(ACC_CREATE_COMMAND)../acc_create/*.h $(ACC_CREATE_COMMAND)../acc_indices/*.h $(ACC_CREATE_COMMAND)../source_observer/*.h $(ACC_CREATE_COMMAND)../acc_recipe/*.h $(ACC_CREATE_COMMAND)../acc_frame/*.h $(ACC_CREATE_COMMAND)../acc_endpoints/*.h $(ACC_CREATE_COMMAND)../acc_transport/*.h $(ACC_CREATE_COMMAND)../acc_capture/*.h $(ACC_CREATE_COMMAND)../acc_actions/*.h $(ACC_CREATE_COMMAND)../acc_accessibility/*.h $(ACC_CREATE_COMMAND)../acc_comparison/*.h $(ACC_CREATE_COMMAND)../acc_partition/*.h $(ACC_CREATE_COMMAND)../acc_append/*.h $(ACC_CREATE_COMMAND)../acc_clause/*.h $(ACC_CREATE_COMMAND)../acc_measure/*.h $(ACC_CREATE_COMMAND)../indexed_views/*.h $(ACC_CREATE_COMMAND)../acc_fold/*.h)

$(BUILD)/c-acc-create-image-fault: ACC_FAULT_SOURCE = $(ACC_CREATE_COMMAND)../acc_command/fault.c
$(BUILD)/c-acc-create-image-fault: ACC_FAULT_WRAPS = -Wl,--wrap=fopen -Wl,--wrap=fwrite -Wl,--wrap=fclose -Wl,--wrap=mkstemp
$(BUILD)/c-acc-create-image-fault: $(ACC_CREATE_COMMAND)../acc_command/fault.c

$(BUILD)/c-acc-create-image $(BUILD)/c-acc-create-image-fault: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CREATE_COMMAND)build.mk $(ACC_CREATE_COMMAND)main.c $(ACC_BODY_SOURCES) $(ACC_BODY_OWNERS) $(ACC_CREATE_COMMAND)../acc_recipe/emit.c $(ACC_CREATE_COMMAND)../acc_indices/emit.c $(ACC_CREATE_COMMAND)../acc_frame/emit.c $(ACC_CREATE_COMMAND)../acc_endpoints/emit.c $(ACC_CREATE_COMMAND)../acc_transport/emit.c $(ACC_CREATE_COMMAND)../acc_capture/emit.c $(ACC_CREATE_COMMAND)../acc_actions/emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_BODY_SOURCES) $(ACC_CREATE_COMMAND)main.c $(ACC_FAULT_SOURCE) $(ACC_FAULT_WRAPS) -Wl,--wrap=tmpfile -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
