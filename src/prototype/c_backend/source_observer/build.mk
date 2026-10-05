C_SOURCE_OBSERVER := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(C_SOURCE_OBSERVER)../build.mk

$(BUILD)/c_source_observer_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(C_SOURCE_OBSERVER)build.mk $(C_SOURCE_OBSERVER)observe.c $(C_SOURCE_OBSERVER)observe.h $(C_SOURCE_OBSERVER)observe_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(C_SOURCE_OBSERVER)observe.c $(C_SOURCE_OBSERVER)observe_test.c -o $@
