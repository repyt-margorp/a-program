ACC_TRANSPORT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_TRANSPORT)../build.mk

$(BUILD)/c_acc_transport_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_TRANSPORT)build.mk $(ACC_TRANSPORT)emit.c $(ACC_TRANSPORT)emit.h $(ACC_TRANSPORT)emit_test.c $(ACC_TRANSPORT)../acc_capture/emit.c $(ACC_TRANSPORT)../acc_capture/emit.h $(ACC_TRANSPORT)../acc_capture/emit_test.c $(ACC_TRANSPORT)../acc_actions/emit.c $(ACC_TRANSPORT)../acc_actions/emit.h $(ACC_TRANSPORT)../indexed_views/view.c $(ACC_TRANSPORT)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_TRANSPORT)../indexed_views/view.c $(ACC_TRANSPORT)emit.c $(ACC_TRANSPORT)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_acc_transport_inspect: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_TRANSPORT)build.mk $(ACC_TRANSPORT)inspect.c $(ACC_TRANSPORT)../acc_down/inspect.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_TRANSPORT)inspect.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
