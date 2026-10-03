include $(ARTIFACT_MAKEFILE)

$(BUILD)/rule_header_lifetime_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(HEADER_CONTROL) $(ALLOCATION_AUDIT)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(HEADER_CONTROL) $(ALLOCATION_AUDIT) -Wl,--wrap=pg_alloc -Wl,--wrap=pg_program_destroy -o $@
