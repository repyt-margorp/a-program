include $(ARTIFACT_MAKEFILE)

$(BUILD)/constructor_scope_lifetime_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(CONSTRUCTOR_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(CONSTRUCTOR_CONTROL_FLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(CONSTRUCTOR_CONTROL) -o $@
