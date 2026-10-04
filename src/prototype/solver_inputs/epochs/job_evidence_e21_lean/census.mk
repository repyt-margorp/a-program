include $(ARTIFACT_MAKEFILE)

$(BUILD)/binding_storage_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(BINDING_AUDIT) $(ALLOCATION_AUDIT)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DBINDING_SCOPE_BORROWING -DMETADATA_SOURCE_OWNERS -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(BINDING_AUDIT) $(ALLOCATION_AUDIT) -Wl,--wrap=pg_alloc -Wl,--wrap=pg_program_destroy -o $@
