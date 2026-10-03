EPOCH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
WORKER := $(abspath $(EPOCH)../../../../..)
include $(WORKER)/src/prototype/performance_verification/build.mk

$(BUILD)/fold_spine_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(EPOCH)fold_spine_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(EPOCH)fold_spine_test.c -o $@

.PHONY: check-fold-empty-tail
check-fold-empty-tail: $(BUILD)/fold_spine_test $(BUILD)/performance_head_cleanup_test
	$(BUILD)/fold_spine_test
	$(BUILD)/performance_head_cleanup_test
