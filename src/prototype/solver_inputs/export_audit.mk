EXPORT_AUDIT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(EXPORT_AUDIT)../artifact_persistence/build.mk

$(BUILD)/export_allocation_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(EXPORT_AUDIT)export_allocation_audit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(EXPORT_AUDIT)export_allocation_audit.c -Wl,--wrap=pg_alloc -o $@
