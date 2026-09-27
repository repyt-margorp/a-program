PROBE_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(PROBE_ROOT)../../Makefile
.DEFAULT_GOAL := $(BUILD)/finite_sort_probe

$(BUILD)/finite_sort_image_compare: $(SOURCES) $(WITNESS_SOURCE) $(wildcard $(ROOT)*.h) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(PROBE_ROOT)image_compare.c $(PROBE_ROOT)../../../tests/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(WITNESS_SOURCE) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(PROBE_ROOT)image_compare.c -o $@

$(BUILD)/finite_sort_probe: $(SOURCES) $(wildcard $(ROOT)*.h) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(PROBE_ROOT)probe.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(PROBE_ROOT)probe.c -o $@
