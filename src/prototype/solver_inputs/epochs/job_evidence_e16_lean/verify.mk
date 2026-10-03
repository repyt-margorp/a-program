ARTIFACT_MAKEFILE ?= /home/repyt/workspace/a-program-workers/job-evidence/src/prototype/artifact_persistence/build.mk
include $(ARTIFACT_MAKEFILE)
OWNER_CONTROL ?= /home/repyt/workspace/a-program-workers/job-evidence/src/prototype/solver_inputs/epochs/job_evidence_e16/pending_owner_control.c

$(BUILD)/pending_owner_control: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(OWNER_CONTROL)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(OWNER_CONTROL) -o $@
