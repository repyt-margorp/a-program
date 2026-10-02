IMPORT_AUDIT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(IMPORT_AUDIT)../artifact_persistence/build.mk

$(BUILD)/import_allocation: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(IMPORT_AUDIT)import_allocation.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(IMPORT_AUDIT)import_allocation.c -Wl,--wrap=pg_alloc -Wl,--wrap=pg_program_allocate_empty -o $@
