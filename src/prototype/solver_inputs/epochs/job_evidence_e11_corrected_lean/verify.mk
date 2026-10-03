include $(ARTIFACT_MAKEFILE)

$(BUILD)/scope_lifetime_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(LIFETIME_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DBORROWED_SCOPE_CURSOR -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(LIFETIME_CONTROL) -o $@

$(BUILD)/canonical_scope_savings_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(SAVINGS_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SAVINGS_CONTROL) -o $@

$(BUILD)/scope_scratch_lifetime_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(SCRATCH_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SCRATCH_CONTROL) -o $@
