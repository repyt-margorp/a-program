ARTIFACT_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-artifact-persistence
include $(OVERLAY)/src/Makefile

.PHONY: check-artifact-transport
check-artifact-transport: $(BUILD)/artifact_transport_test $(BUILD)/occurrence_io_test $(BUILD)/graph_io_test $(BUILD)/graph_acceptance_test
	$(BUILD)/artifact_transport_test
	$(BUILD)/graph_io_test
	bash $(TESTS)/graph_acceptance.sh $(BUILD)/graph_acceptance_test
	bash $(TESTS)/occurrence_io.sh $(BUILD)/occurrence_io_test

# A failing resume comparison is a regression, not an expected-failure pass.
PARTITION_REPORT ?= $(BUILD)/fuel-partitions
.PHONY: check-artifact-partitions
check-artifact-partitions: $(BUILD)/pointer-check
	IMAGE_AUDIT_STRICT_BYTES=1 bash $(ARTIFACT_PROTOTYPE)../image_audit/partition_fuel.sh $(BUILD)/pointer-check $(REPO)/examples/09_list_induction.p $(PARTITION_REPORT) ordinary

.PHONY: check-artifact-semantic
check-artifact-semantic: $(BUILD)/artifact_semantic_test
	$(BUILD)/artifact_semantic_test

$(BUILD)/artifact_semantic_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(ARTIFACT_PROTOTYPE)semantic_test.c $(ARTIFACT_PROTOTYPE)semantic_consumer.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ARTIFACT_PROTOTYPE)semantic_test.c $(ARTIFACT_PROTOTYPE)semantic_consumer.c -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -o $@

$(BUILD)/artifact_transport_test: $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ARTIFACT_PROTOTYPE)transport_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ARTIFACT_PROTOTYPE)transport_test.c -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -o $@
