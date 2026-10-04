INDEXED_VIEWS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(INDEXED_VIEWS)../build.mk

$(BUILD)/c_indexed_view_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(INDEXED_VIEWS)probe.c $(INDEXED_VIEWS)view.c $(INDEXED_VIEWS)view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(INDEXED_VIEWS)probe.c $(INDEXED_VIEWS)view.c -o $@

$(BUILD)/c_indexed_view_emit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(INDEXED_VIEWS)emit_test.c $(INDEXED_VIEWS)emit.c $(INDEXED_VIEWS)emit.h $(INDEXED_VIEWS)view.c $(INDEXED_VIEWS)view.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(INDEXED_VIEWS)emit_test.c $(INDEXED_VIEWS)emit.c $(INDEXED_VIEWS)view.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@
