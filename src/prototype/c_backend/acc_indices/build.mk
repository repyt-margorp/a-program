ACC_INDICES := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_INDICES)../acc_recipe/build.mk

$(BUILD)/c_acc_index_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_INDICES)build.mk $(ACC_INDICES)emit.c $(ACC_INDICES)emit.h $(ACC_INDICES)emit_test.c $(ACC_INDICES)../acc_recipe/emit.c $(ACC_INDICES)../acc_recipe/emit_test.c $(ACC_INDICES)../source_observer/observe.c $(ACC_INDICES)../source_observer/observe.h $(ACC_INDICES)../indexed_views/view.c $(ACC_INDICES)../acc_endpoints/read.c $(ACC_INDICES)../acc_frame/emit.c $(ACC_INDICES)../acc_endpoints/emit.c $(ACC_INDICES)../acc_transport/emit.c $(ACC_INDICES)../acc_capture/emit.c $(ACC_INDICES)../acc_actions/emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_INDICES)../indexed_views/view.c $(ACC_INDICES)../acc_endpoints/read.c $(ACC_INDICES)../source_observer/observe.c $(ACC_INDICES)emit.c $(ACC_INDICES)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
