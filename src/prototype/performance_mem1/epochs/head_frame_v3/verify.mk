MEM1_TESTS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(VERIFICATION_MAKEFILE)

$(BUILD)/frame_retirement_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(MEM1_TESTS)frame_retirement_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(MEM1_TESTS)frame_retirement_test.c -o $@

$(BUILD)/stateless_head_cleanup_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(MEM1_TESTS)stateless_head_cleanup_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(MEM1_TESTS)stateless_head_cleanup_test.c -o $@

$(BUILD)/callback_failure_cleanup_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(MEM1_TESTS)callback_failure_cleanup_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(MEM1_TESTS)callback_failure_cleanup_test.c -o $@
