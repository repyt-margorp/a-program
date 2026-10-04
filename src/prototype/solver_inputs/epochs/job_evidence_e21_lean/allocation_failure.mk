include $(ARTIFACT_MAKEFILE)

$(BUILD)/scheduler_allocation_failure: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(CHECKPOINT_TESTS)normalization_checkpoint_test.c $(SCHEDULER_FAILURE_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(CHECKPOINT_TESTS) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(SCHEDULER_FAILURE_CONTROL) -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_alloc -o $@
