PERFORMANCE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(PERFORMANCE)../artifact_persistence/build.mk

$(BUILD)/performance_direct_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(PERFORMANCE)direct_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(PERFORMANCE)direct_test.c -o $@

$(BUILD)/performance_head_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(PERFORMANCE)head_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(PERFORMANCE)head_test.c -o $@
