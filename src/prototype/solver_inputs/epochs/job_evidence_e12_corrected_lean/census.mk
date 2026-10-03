include $(ARTIFACT_MAKEFILE)

$(BUILD)/binding_storage_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(BINDING_AUDIT)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(BINDING_AUDIT_FLAGS) -DMETADATA_SOURCE_OWNERS -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(BINDING_AUDIT) -o $@
