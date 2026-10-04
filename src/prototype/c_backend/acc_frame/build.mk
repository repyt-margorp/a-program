ACC_FRAME := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_FRAME)../acc_endpoints/build.mk

$(BUILD)/c_acc_frame_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_FRAME)build.mk $(ACC_FRAME)emit.c $(ACC_FRAME)emit.h $(ACC_FRAME)emit_test.c $(ACC_FRAME)../acc_endpoints/emit.c $(ACC_FRAME)../acc_endpoints/emit.h $(ACC_FRAME)../acc_endpoints/read.c $(ACC_FRAME)../acc_endpoints/read.h $(ACC_FRAME)../acc_transport/emit.c $(ACC_FRAME)../acc_transport/emit.h $(ACC_FRAME)../acc_capture/emit.c $(ACC_FRAME)../acc_capture/emit.h $(ACC_FRAME)../acc_capture/emit_test.c $(ACC_FRAME)../acc_actions/emit.c $(ACC_FRAME)../acc_actions/emit.h $(ACC_FRAME)../indexed_views/view.c $(ACC_FRAME)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_FRAME)../indexed_views/view.c $(ACC_FRAME)../acc_endpoints/read.c $(ACC_FRAME)emit.c $(ACC_FRAME)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
