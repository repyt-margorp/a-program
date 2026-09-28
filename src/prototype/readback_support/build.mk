SUPPORT_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-readback-support
include $(OVERLAY)/src/Makefile

.PHONY: check-support
check-acceptance: check-support
check-support: $(BUILD)/support_test $(BUILD)/support_resume_test
	$(BUILD)/support_test
	$(BUILD)/support_resume_test

$(BUILD)/support_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(SUPPORT_PROTOTYPE)support_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPG_SUPPORT_CANDIDATE -I$(ROOT) $(SOURCES) $(SUPPORT_PROTOTYPE)support_test.c -o $@

$(BUILD)/support_resume_test: $(ROOT)graph.c $(ROOT)support.c $(ROOT)eval.c $(wildcard $(ROOT)*.h) $(SUPPORT_PROTOTYPE)resume_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPG_SUPPORT_CANDIDATE -I$(ROOT) $(ROOT)graph.c $(ROOT)support.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(SUPPORT_PROTOTYPE)resume_test.c -o $@

$(BUILD)/finite_sort_image_compare: $(SOURCES) $(WITNESS_SOURCE) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(SUPPORT_PROTOTYPE)../finite_sorting/image_compare.c $(TESTS)/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(WITNESS_SOURCE) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SUPPORT_PROTOTYPE)../finite_sorting/image_compare.c -o $@
