PERFORMANCE_VERIFICATION := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(PERFORMANCE_VERIFICATION)../performance/build.mk

$(BUILD)/performance_total_result_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(PERFORMANCE_VERIFICATION)total_result_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(PERFORMANCE_VERIFICATION)total_result_test.c -o $@

$(BUILD)/performance_head_cleanup_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(PERFORMANCE_VERIFICATION)head_cleanup_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(PERFORMANCE_VERIFICATION)head_cleanup_test.c -o $@
