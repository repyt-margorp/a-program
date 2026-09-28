AUDIT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
COMPILER ?= $(AUDIT)../..
include $(COMPILER)/Makefile
.DEFAULT_GOAL := $(BUILD)/image_audit

$(BUILD)/image_audit: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(AUDIT)audit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $^ -Wl,--wrap=pg_graph_image_write -Wl,--wrap=pg_derivation_inputs_write_inference -Wl,--wrap=pg_reduction_archive_write -Wl,--wrap=pg_syntax_write -Wl,--wrap=pg_substitution_advance -o $@
