ACC_PARTITION := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_PARTITION)../build.mk

$(BUILD)/c_acc_partition_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_PARTITION)probe.c $(ACC_PARTITION)../indexed_views/view.c $(ACC_PARTITION)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_PARTITION)probe.c $(ACC_PARTITION)../indexed_views/view.c -o $@

$(BUILD)/c_acc_partition_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_PARTITION)emit_test.c $(ACC_PARTITION)emit.c $(ACC_PARTITION)emit.h $(ACC_PARTITION)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_PARTITION)emit_test.c $(ACC_PARTITION)emit.c $(ACC_PARTITION)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
