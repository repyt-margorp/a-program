include $(ARTIFACT_MAKEFILE)

$(BUILD)/schema_input_control.o: $(ROOT)synthesis_schema.c $(wildcard $(ROOT)*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -Dpg_alloc=pg_schema_control_alloc -c $(ROOT)synthesis_schema.c -o $@

$(BUILD)/schema_input_allocation_failure: $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(TESTS)/synthesis.c $(SCHEMA_INPUT_CONTROL) $(BUILD)/schema_input_control.o
	$(CC) $(CFLAGS) -I$(ROOT) -I$(TESTS) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(filter-out $(ROOT)synthesis_schema.c,$(SYNTHESIS_SOURCES)) $(BUILD)/schema_input_control.o $(SCHEMA_INPUT_CONTROL) -o $@
