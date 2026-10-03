SURFACE_ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/ap-surface-20261003-e1
include $(OVERLAY)/src/Makefile

$(BUILD)/surface_migrate: $(ROOT)graph.c $(ROOT)support.c $(ROOT)reader.c $(ROOT)syntax.c $(wildcard $(ROOT)*.h) $(SURFACE_ROOT)migrate.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)support.c $(ROOT)reader.c $(ROOT)syntax.c $(SURFACE_ROOT)migrate.c -o $@

$(BUILD)/surface_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(SURFACE_ROOT)test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SURFACE_ROOT)test.c -o $@

$(BUILD)/surface_scope_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(SURFACE_ROOT)scope_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c $(ROOT)synthesis.c,$(CLI_SOURCES)) $(SURFACE_ROOT)scope_test.c -o $@

.PHONY: check-surface
check-surface: $(BUILD)/surface_test $(BUILD)/surface_scope_test $(BUILD)/surface_migrate $(BUILD)/program_test $(BUILD)/pointer-check
	$(BUILD)/surface_test
	$(BUILD)/surface_scope_test
	bash $(SURFACE_ROOT)check_migration.sh $(BUILD)/surface_migrate
	bash $(SURFACE_ROOT)check.sh $(BUILD)/pointer-check $(BUILD)/program_test
