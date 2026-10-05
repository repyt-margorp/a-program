ACC_COMMAND := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_COMMAND)../build.mk

ACC_BODY_SOURCES := $(ACC_COMMAND)../source_observer/observe.c $(ACC_COMMAND)../acc_recipe/emit.c $(ACC_COMMAND)../acc_accessibility/emit.c $(ACC_COMMAND)../acc_comparison/emit.c $(ACC_COMMAND)../acc_partition/emit.c $(ACC_COMMAND)../acc_append/emit.c $(ACC_COMMAND)../acc_clause/emit.c $(ACC_COMMAND)../acc_fold/emit.c $(ACC_COMMAND)../acc_measure/emit.c $(ACC_COMMAND)../acc_endpoints/read.c $(ACC_COMMAND)../indexed_views/view.c
ACC_BODY_OWNERS := $(wildcard $(ACC_COMMAND)../source_observer/*.h $(ACC_COMMAND)../acc_recipe/*.h $(ACC_COMMAND)../acc_frame/*.h $(ACC_COMMAND)../acc_endpoints/*.h $(ACC_COMMAND)../acc_transport/*.h $(ACC_COMMAND)../acc_capture/*.h $(ACC_COMMAND)../acc_actions/*.h $(ACC_COMMAND)../acc_accessibility/*.h $(ACC_COMMAND)../acc_comparison/*.h $(ACC_COMMAND)../acc_partition/*.h $(ACC_COMMAND)../acc_append/*.h $(ACC_COMMAND)../acc_clause/*.h $(ACC_COMMAND)../acc_measure/*.h $(ACC_COMMAND)../indexed_views/*.h $(ACC_COMMAND)../acc_fold/*.h)

$(BUILD)/c-acc-image-fault: ACC_FAULT_SOURCE = $(ACC_COMMAND)fault.c
$(BUILD)/c-acc-image-fault: ACC_FAULT_WRAPS = -Wl,--wrap=fopen -Wl,--wrap=fwrite -Wl,--wrap=fclose -Wl,--wrap=mkstemp
$(BUILD)/c-acc-image-fault: $(ACC_COMMAND)fault.c

$(BUILD)/c-acc-image $(BUILD)/c-acc-image-fault: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_COMMAND)build.mk $(ACC_COMMAND)main.c $(ACC_BODY_SOURCES) $(ACC_BODY_OWNERS) $(ACC_COMMAND)../acc_frame/emit.c $(ACC_COMMAND)../acc_endpoints/emit.c $(ACC_COMMAND)../acc_transport/emit.c $(ACC_COMMAND)../acc_capture/emit.c $(ACC_COMMAND)../acc_actions/emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_BODY_SOURCES) $(ACC_COMMAND)main.c $(ACC_FAULT_SOURCE) $(ACC_FAULT_WRAPS) -Wl,--wrap=tmpfile -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
