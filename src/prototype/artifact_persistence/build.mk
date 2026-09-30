ARTIFACT_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CHECKPOINT_TESTS ?= $(ARTIFACT_PROTOTYPE)
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

.PHONY: check-artifact-normalization-checkpoint
check-artifact-normalization-checkpoint: $(BUILD)/artifact_normalization_checkpoint_test
	$(BUILD)/artifact_normalization_checkpoint_test

NORMALIZATION_IO := $(addprefix $(ROOT),machine_io.c computation_io.c identity_io.c comparison_io.c eval_io.c artifact/schedule.c)
.PHONY: check-artifact-source-checkpoint
check-artifact-source-checkpoint: $(BUILD)/artifact_source_checkpoint_test
	$(BUILD)/artifact_source_checkpoint_test

$(BUILD)/artifact_source_checkpoint_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(wildcard $(ROOT)*.h) $(wildcard $(ROOT)artifact/*.h) $(CHECKPOINT_TESTS)source_checkpoint_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(ROOT)artifact/source.c $(CHECKPOINT_TESTS)source_checkpoint_test.c -Wl,--wrap=pg_synthesis_advance -o $@

.PHONY: check-artifact-derivation-checkpoint
check-artifact-derivation-checkpoint: $(BUILD)/artifact_derivation_checkpoint_test
	$(BUILD)/artifact_derivation_checkpoint_test

$(BUILD)/artifact_derivation_checkpoint_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(wildcard $(ROOT)*.h) $(wildcard $(ROOT)artifact/*.h) $(CHECKPOINT_TESTS)derivation_checkpoint_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(ROOT)artifact/derivation.c $(CHECKPOINT_TESTS)derivation_checkpoint_test.c -Wl,--wrap=pg_synthesis_advance -o $@

.PHONY: check-artifact-definition-checkpoint
check-artifact-definition-checkpoint: $(BUILD)/artifact_definition_checkpoint_test
	$(BUILD)/artifact_definition_checkpoint_test

# Pending namespace member at the checked-field/pre-body continuation boundary.
.PHONY: check-artifact-namespace-frontier
check-artifact-namespace-frontier: $(BUILD)/artifact_definition_checkpoint_test
	$(BUILD)/artifact_definition_checkpoint_test --pending-namespace

# Started abstraction: preserve ordinary Lambda/Pi/body rule cursors and queue.
.PHONY: check-artifact-namespace-body-frontier
check-artifact-namespace-body-frontier: $(BUILD)/artifact_definition_checkpoint_test
	$(BUILD)/artifact_definition_checkpoint_test --advanced-namespace

.PHONY: check-artifact-constructor-checkpoint
check-artifact-constructor-checkpoint: $(BUILD)/artifact_constructor_checkpoint_test
	$(BUILD)/artifact_constructor_checkpoint_test

$(BUILD)/artifact_constructor_checkpoint_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(CHECKPOINT_TESTS)constructor_checkpoint_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(CHECKPOINT_TESTS)constructor_checkpoint_test.c -Wl,--wrap=pg_synthesis_advance -o $@

$(BUILD)/artifact_definition_checkpoint_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(wildcard $(ROOT)*.h) $(CHECKPOINT_TESTS)definition_checkpoint_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ROOT)artifact/schedule.c $(CHECKPOINT_TESTS)definition_checkpoint_test.c -Wl,--wrap=pg_synthesis_advance -o $@

$(BUILD)/artifact_normalization_checkpoint_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(wildcard $(ROOT)*.h) $(CHECKPOINT_TESTS)normalization_checkpoint_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(NORMALIZATION_IO) $(CHECKPOINT_TESTS)normalization_checkpoint_test.c -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_whnf_advance -o $@

HISTORY_REPORT ?= $(BUILD)/history-fuel
.PHONY: check-artifact-history
check-artifact-history: check-artifact-semantic $(BUILD)/pointer-check $(BUILD)/source_io_test $(BUILD)/identity_io_test $(BUILD)/program_test
	bash $(ARTIFACT_PROTOTYPE)file_policy.sh $(BUILD)/pointer-check
	bash $(TESTS)/source_io.sh $(BUILD)/source_io_test
	$(BUILD)/source_io_test normalization
	bash $(TESTS)/identity_io.sh $(BUILD)/identity_io_test
	bash $(TESTS)/image_cli.sh $(BUILD)/source_io_test $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ARTIFACT_PROTOTYPE)../image_audit/fuel_curve.sh $(BUILD)/pointer-check $(REPO)/examples/09_list_induction.p $(HISTORY_REPORT) ordinary

.PHONY: check-artifact-metrics
check-artifact-metrics: check-artifact-history $(BUILD)/artifact_metrics
	bash $(ARTIFACT_PROTOTYPE)metrics.sh $(BUILD)/artifact_metrics $(HISTORY_REPORT)

.PHONY: check-artifact-sorting
check-artifact-sorting: $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ARTIFACT_PROTOTYPE)../finite_sorting/check.sh $(BUILD)/pointer-check $(BUILD)/program_test all
	bash $(ARTIFACT_PROTOTYPE)../finite_sorting/value-check.sh $(BUILD)/pointer-check
	bash $(ARTIFACT_PROTOTYPE)../finite_sorting/merge-check.sh $(BUILD)/pointer-check $(BUILD)/program_test views
	bash $(ARTIFACT_PROTOTYPE)../finite_sorting/tree-check.sh $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ARTIFACT_PROTOTYPE)../finite_sorting/bubble-check.sh $(BUILD)/pointer-check $(BUILD)/program_test

$(BUILD)/artifact_semantic_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ARTIFACT_PROTOTYPE)semantic_test.c $(ARTIFACT_PROTOTYPE)semantic_consumer.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ARTIFACT_PROTOTYPE)semantic_test.c $(ARTIFACT_PROTOTYPE)semantic_consumer.c -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_effect_inference_advance -o $@

$(BUILD)/artifact_transport_test: $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ARTIFACT_PROTOTYPE)transport_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ARTIFACT_PROTOTYPE)transport_test.c -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -o $@

$(BUILD)/artifact_metrics: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ARTIFACT_PROTOTYPE)metrics.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ARTIFACT_PROTOTYPE)metrics.c -Wl,--wrap=pg_graph_write_descriptors -Wl,--wrap=pg_occurrences_write_descriptors -Wl,--wrap=pg_contexts_write_descriptors -Wl,--wrap=pg_retained_write_semantic_with -Wl,--wrap=pg_synthesis_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_effect_inference_advance -o $@

$(BUILD)/artifact_state_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h) $(ARTIFACT_PROTOTYPE)state_audit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ARTIFACT_PROTOTYPE)state_audit.c -o $@
