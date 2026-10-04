ACCEPTED_FRAME := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(OVERLAY)/src/Makefile
FRAME_FIXTURES ?= $(ACCEPTED_FRAME)fixtures
FRAME_MACHINE_IO := $(addprefix $(ROOT),machine_io.c computation_io.c identity_io.c comparison_io.c eval_io.c)
FRAME_CALLBACK_TESTS := frame_retirement_test stateless_head_cleanup_test callback_failure_cleanup_test
FRAME_CODEC_TESTS := fold_spine_test materialized_fields_test machine_resave_probe readback_transport_bounds_test

$(addprefix $(BUILD)/,$(FRAME_CALLBACK_TESTS)): $(BUILD)/%: $(SOURCES) $(wildcard $(ROOT)*.h) $(FRAME_FIXTURES)/%.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(FRAME_FIXTURES)/$*.c -o $@

$(addprefix $(BUILD)/,$(FRAME_CODEC_TESTS)): $(BUILD)/%: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(FRAME_MACHINE_IO) $(wildcard $(ROOT)*.h) $(FRAME_FIXTURES)/%.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(FRAME_MACHINE_IO) $(FRAME_FIXTURES)/$*.c -o $@
