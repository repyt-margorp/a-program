CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c

.PHONY: check
check: $(BUILD)/core_test
	$(BUILD)/core_test

$(BUILD)/core_test: $(SOURCES) $(ROOT)graph.h $(ROOT)dimension.h $(ROOT)eval.h $(ROOT)typing.h $(ROOT)tests/core.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/core.c -o $@
