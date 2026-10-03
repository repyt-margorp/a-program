include $(ARTIFACT_MAKEFILE)

.PHONY: check-source-claim
check-source-claim: $(BUILD)/source_claim_control
	$(BUILD)/source_claim_control

$(BUILD)/source_claim_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(SOURCE_CLAIM)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(SOURCE_CLAIM) -o $@

$(BUILD)/resume_policy_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(POLICY_SOURCE)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(POLICY_SOURCE) -o $@
