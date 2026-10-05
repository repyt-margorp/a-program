ACC_RUNTIME_LINK := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(ACC_RUNTIME_LINK)../acc_runtime_capture/build.mk

$(BUILD)/c-acc-runtime-plan: $(SOURCES) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h) $(ACC_RUNTIME_LINK)plan.c $(ACC_RUNTIME_LINK)build.mk $(C_BACKEND)link/plan.c $(C_BACKEND)link/plan.h $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ACC_RUNTIME_LINK)plan.c $(C_BACKEND)link/plan.c $(C_BACKEND)emit.c -o $@

$(BUILD)/c-acc-runtime-publish-refusal: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(ACC_RUNTIME_LINK)publish_refusal.c $(ACC_RUNTIME_LINK)build.mk $(LINK_SOURCES) $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPG_C_BACKEND_DIRECTORY='"$(C_BACKEND)"' -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(ACC_RUNTIME_LINK)publish_refusal.c $(LINK_SOURCES) $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=fopen -Wl,--wrap=lstat -o $@
