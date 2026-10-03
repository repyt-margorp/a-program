EPOCH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
WORKER := $(abspath $(EPOCH)../../../../..)
FOLD_TEST := $(WORKER)/src/prototype/performance_mem3/epochs/fold_empty_tail_v1/fold_spine_test.c
include $(WORKER)/src/prototype/performance_verification/build.mk

$(BUILD)/fold_spine_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(FOLD_TEST)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(FOLD_TEST) -o $@

$(BUILD)/fold_cursor_boundary_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(EPOCH)fold_cursor_boundary_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(EPOCH)fold_cursor_boundary_test.c -o $@

.PHONY: check-fold-cursor
check-fold-cursor: $(BUILD)/fold_spine_test $(BUILD)/fold_cursor_boundary_test $(BUILD)/performance_head_cleanup_test
	$(BUILD)/fold_spine_test
	$(BUILD)/fold_cursor_boundary_test
	$(BUILD)/performance_head_cleanup_test
