ACC_APPEND := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_APPEND)../build.mk

$(BUILD)/c_acc_append_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_APPEND)emit_test.c $(ACC_APPEND)emit.c $(ACC_APPEND)emit.h $(ACC_APPEND)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_APPEND)emit_test.c $(ACC_APPEND)emit.c $(ACC_APPEND)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
