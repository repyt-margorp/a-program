ACC_ENDPOINTS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_ENDPOINTS)../build.mk

$(BUILD)/c_acc_endpoints_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_ENDPOINTS)build.mk $(ACC_ENDPOINTS)emit.c $(ACC_ENDPOINTS)emit.h $(ACC_ENDPOINTS)emit_test.c $(ACC_ENDPOINTS)read.c $(ACC_ENDPOINTS)read.h $(ACC_ENDPOINTS)../acc_transport/emit.c $(ACC_ENDPOINTS)../acc_transport/emit.h $(ACC_ENDPOINTS)../acc_transport/emit_test.c $(ACC_ENDPOINTS)../acc_capture/emit.c $(ACC_ENDPOINTS)../acc_capture/emit.h $(ACC_ENDPOINTS)../acc_capture/emit_test.c $(ACC_ENDPOINTS)../acc_actions/emit.c $(ACC_ENDPOINTS)../acc_actions/emit.h $(ACC_ENDPOINTS)../indexed_views/view.c $(ACC_ENDPOINTS)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_ENDPOINTS)../indexed_views/view.c $(ACC_ENDPOINTS)read.c $(ACC_ENDPOINTS)emit.c $(ACC_ENDPOINTS)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_acc_endpoints_inspect: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_ENDPOINTS)build.mk $(ACC_ENDPOINTS)inspect.c $(ACC_ENDPOINTS)read.c $(ACC_ENDPOINTS)read.h $(ACC_ENDPOINTS)../acc_down/inspect.c $(ACC_ENDPOINTS)../indexed_views/view.c $(ACC_ENDPOINTS)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_ENDPOINTS)../indexed_views/view.c $(ACC_ENDPOINTS)read.c $(ACC_ENDPOINTS)inspect.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
