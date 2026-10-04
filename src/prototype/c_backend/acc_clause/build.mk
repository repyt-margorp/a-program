ACC_CLAUSE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_CLAUSE)../build.mk

$(BUILD)/c_acc_clause_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CLAUSE)probe.c $(ACC_CLAUSE)../indexed_views/view.c $(ACC_CLAUSE)../indexed_views/view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_CLAUSE)probe.c $(ACC_CLAUSE)../indexed_views/view.c -o $@

$(BUILD)/c_acc_clause_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_CLAUSE)emit_test.c $(ACC_CLAUSE)emit.c $(ACC_CLAUSE)emit.h $(ACC_CLAUSE)../acc_fold/emit.c $(ACC_CLAUSE)../indexed_views/view.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_CLAUSE)emit_test.c $(ACC_CLAUSE)emit.c $(ACC_CLAUSE)../acc_fold/emit.c $(ACC_CLAUSE)../indexed_views/view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_typed_query_advance -o $@
