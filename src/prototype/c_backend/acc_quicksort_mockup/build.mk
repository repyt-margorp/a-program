ACC_MOCKUP := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_MOCKUP)../build.mk

$(BUILD)/c_acc_mockup_oracle: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_MOCKUP)oracle_test.c $(C_BACKEND)selection.c $(C_BACKEND)lower/representation.c $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_MOCKUP)oracle_test.c $(C_BACKEND)selection.c $(C_BACKEND)lower/representation.c $(C_BACKEND)emit.c -o $@

.PHONY: check-c-acc-mockup
check-c-acc-mockup: $(BUILD)/pointer-check $(BUILD)/c_acc_mockup_oracle
	bash $(ACC_MOCKUP)check.sh $(BUILD)/pointer-check $(BUILD)/c_acc_mockup_oracle
