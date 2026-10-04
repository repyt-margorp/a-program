include $(ARTIFACT_MAKEFILE)

$(BUILD)/source_frontier_copied_owner: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(CHECKPOINT_TESTS)source_checkpoint_test.c $(SOURCE_OWNER_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(CHECKPOINT_TESTS) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(SOURCE_OWNER_CONTROL) -Wl,--wrap=pg_synthesis_advance -o $@
