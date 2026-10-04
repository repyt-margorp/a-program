ACC_FOLD := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_FOLD)../build.mk

$(BUILD)/c_acc_fold_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_FOLD)probe.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_FOLD)probe.c -o $@

$(BUILD)/c_acc_fold_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_FOLD)emit_test.c $(ACC_FOLD)emit.c $(ACC_FOLD)emit.h $(ACC_FOLD)../indexed_views/emit.c $(ACC_FOLD)../indexed_views/emit.h $(ACC_FOLD)../indexed_views/view.c $(ACC_FOLD)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_FOLD)emit_test.c $(ACC_FOLD)emit.c $(ACC_FOLD)../indexed_views/emit.c $(ACC_FOLD)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@
