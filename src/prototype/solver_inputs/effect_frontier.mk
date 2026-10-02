SOLVER_INPUTS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(SOLVER_INPUTS)../artifact_persistence/build.mk

$(BUILD)/effect_frontier_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(SOLVER_INPUTS)effect_frontier_probe.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SOLVER_INPUTS)effect_frontier_probe.c -o $@
