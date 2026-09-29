C_BACKEND := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-c-backend-base
include $(C_BACKEND)../artifact_persistence/build.mk

$(BUILD)/a-to-c: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(C_BACKEND)main.c $(C_BACKEND)emit.c $(C_BACKEND)emit.h $(C_BACKEND)runtime.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)main.c $(C_BACKEND)emit.c -o $@

.PHONY: check-c-backend
check-c-backend: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_oracle_test
	bash $(C_BACKEND)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)oracle_check.sh $(BUILD)/c_oracle_test

.PHONY: check-c-sorting-boundary
check-c-sorting-boundary: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)sorting_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

$(BUILD)/c_oracle_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(C_BACKEND)oracle_test.c $(C_BACKEND)emit.c $(C_BACKEND)emit.h $(C_BACKEND)runtime.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(C_BACKEND)oracle_test.c $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -o $@
