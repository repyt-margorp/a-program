ACC_LINK := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_LINK)../build.mk

$(BUILD)/c-acc-link-plan: $(SOURCES) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_LINK)plan.c $(ACC_LINK)build.mk $(C_BACKEND)link/plan.c $(C_BACKEND)link/plan.h $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ACC_LINK)plan.c $(C_BACKEND)link/plan.c $(C_BACKEND)emit.c -o $@
