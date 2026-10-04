ACC_ACCESSIBILITY := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_ACCESSIBILITY)../build.mk

$(BUILD)/c_acc_accessibility_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_ACCESSIBILITY)probe.c $(ACC_ACCESSIBILITY)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_ACCESSIBILITY)probe.c $(ACC_ACCESSIBILITY)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_nat_accessibility_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_ACCESSIBILITY)emit.c $(ACC_ACCESSIBILITY)emit.h $(ACC_ACCESSIBILITY)emit_test.c $(ACC_ACCESSIBILITY)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_ACCESSIBILITY)emit_test.c $(ACC_ACCESSIBILITY)emit.c $(ACC_ACCESSIBILITY)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
