EPOCH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
WORKER := $(abspath $(EPOCH)../../../../..)
include $(WORKER)/src/prototype/performance_verification/build.mk

.PHONY: check-iadt-empty-tail
check-iadt-empty-tail: $(BUILD)/performance_direct_test $(BUILD)/performance_head_cleanup_test
	$(BUILD)/performance_direct_test
	$(BUILD)/performance_head_cleanup_test
