EPOCH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
WORKER := $(abspath $(EPOCH)../../../../..)
include $(WORKER)/src/prototype/performance_verification/build.mk

$(BUILD)/materialized_fields_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(EPOCH)materialized_fields_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(EPOCH)materialized_fields_test.c -o $@

.PHONY: check-materialized-fields
check-materialized-fields: $(BUILD)/materialized_fields_test $(BUILD)/performance_direct_test $(BUILD)/performance_head_cleanup_test
	$(BUILD)/materialized_fields_test
	$(BUILD)/performance_direct_test
	$(BUILD)/performance_head_cleanup_test
