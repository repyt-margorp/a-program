ACC_MEASURE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_MEASURE)../build.mk

$(BUILD)/c_acc_measure_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_MEASURE)emit.c $(ACC_MEASURE)emit.h $(ACC_MEASURE)emit_test.c $(ACC_MEASURE)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_MEASURE)emit_test.c $(ACC_MEASURE)emit.c $(ACC_MEASURE)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
