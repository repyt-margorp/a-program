ARTIFACT_PROTOTYPE := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-artifact-persistence
include $(OVERLAY)/src/Makefile

.PHONY: check-artifact-transport
check-artifact-transport: $(BUILD)/artifact_transport_test $(BUILD)/occurrence_io_test $(BUILD)/graph_io_test $(BUILD)/graph_acceptance_test
	$(BUILD)/artifact_transport_test
	$(BUILD)/graph_io_test
	bash $(TESTS)/graph_acceptance.sh $(BUILD)/graph_acceptance_test
	bash $(TESTS)/occurrence_io.sh $(BUILD)/occurrence_io_test

$(BUILD)/artifact_transport_test: $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ARTIFACT_PROTOTYPE)transport_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ARTIFACT_PROTOTYPE)transport_test.c -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_substitution_advance -o $@
