include $(ARTIFACT_MAKEFILE)

$(BUILD)/premise_storage_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(AUDIT_SOURCE)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DMETADATA_SOURCE_OWNERS -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(AUDIT_SOURCE) -o $@
