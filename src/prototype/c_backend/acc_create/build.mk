ACC_CREATE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_CREATE)../acc_indices/build.mk

$(BUILD)/c_acc_create_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CREATE)build.mk $(ACC_CREATE)emit.c $(ACC_CREATE)emit.h $(ACC_CREATE)emit_test.c $(wildcard $(ACC_CREATE)../acc_indices/*.c $(ACC_CREATE)../acc_indices/*.h) $(ACC_CREATE)../source_observer/observe.c $(ACC_CREATE)../source_observer/observe.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_CREATE)../indexed_views/view.c $(ACC_CREATE)../acc_endpoints/read.c $(ACC_CREATE)../source_observer/observe.c $(ACC_CREATE)emit.c $(ACC_CREATE)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
