CONVERSION_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-conversion-head
include $(CONVERSION_PROTOTYPE)../readback_support/build.mk

.PHONY: check-conversion-head
check-acceptance: check-conversion-head
check-conversion-head: $(BUILD)/conversion_head_test
	$(BUILD)/conversion_head_test

$(BUILD)/conversion_head_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(CONVERSION_PROTOTYPE)test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(CONVERSION_PROTOTYPE)test.c -o $@

$(BUILD)/conversion_probe: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(CONVERSION_PROTOTYPE)probe.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(filter-out $(ROOT)conversion.c,$(SOURCES)) $(filter-out $(ROOT)main.c $(ROOT)synthesis_conversion.c,$(CLI_SOURCES)) $(CONVERSION_PROTOTYPE)probe.c -o $@
