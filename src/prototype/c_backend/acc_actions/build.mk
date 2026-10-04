ACC_ACTIONS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_ACTIONS)../build.mk

$(BUILD)/c_acc_actions_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_ACTIONS)emit.c $(ACC_ACTIONS)emit.h $(ACC_ACTIONS)emit_test.c $(ACC_ACTIONS)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_ACTIONS)../indexed_views/view.c $(ACC_ACTIONS)emit.c $(ACC_ACTIONS)emit_test.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
