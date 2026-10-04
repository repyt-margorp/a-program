NATIVE_CALLBACK_UNITS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(NATIVE_CALLBACK_UNITS)../build.mk

.PHONY: check-c-native-callback-units
check-c-native-callback-units: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(NATIVE_CALLBACK_UNITS)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check
