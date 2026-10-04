SIGNED_UNITS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(SIGNED_UNITS)../build.mk

.PHONY: check-c-signed-predicate-units
check-c-signed-predicate-units: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(SIGNED_UNITS)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check
