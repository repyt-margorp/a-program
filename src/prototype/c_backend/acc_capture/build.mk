ACC_CAPTURE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_CAPTURE)../build.mk

$(BUILD)/c_acc_capture_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CAPTURE)emit.c $(ACC_CAPTURE)emit.h $(ACC_CAPTURE)emit_test.c $(ACC_CAPTURE)../acc_actions/emit.c $(ACC_CAPTURE)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_CAPTURE)../indexed_views/view.c $(ACC_CAPTURE)emit.c $(ACC_CAPTURE)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
