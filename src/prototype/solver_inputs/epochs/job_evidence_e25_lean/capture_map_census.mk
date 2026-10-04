include $(ARTIFACT_MAKEFILE)

$(BUILD)/source_capture_map_census: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(CHECKPOINT_TESTS)source_checkpoint_test.c $(SOURCE_CAPTURE_CENSUS)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(CHECKPOINT_TESTS) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(SOURCE_CAPTURE_CENSUS) -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_alloc -Wl,--wrap=pg_index_init -o $@
