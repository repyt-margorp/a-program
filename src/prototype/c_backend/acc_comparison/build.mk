ACC_COMPARISON := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_COMPARISON)../build.mk

$(BUILD)/c_acc_comparison_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_COMPARISON)emit.c $(ACC_COMPARISON)emit.h $(ACC_COMPARISON)emit_test.c $(ACC_COMPARISON)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_COMPARISON)emit_test.c $(ACC_COMPARISON)emit.c $(ACC_COMPARISON)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_acc_comparison_oracle: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_COMPARISON)oracle_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_COMPARISON)oracle_test.c -o $@
