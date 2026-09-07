CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)conversion.c $(ROOT)classifier.c $(ROOT)evidence.c

.PHONY: check
check: $(BUILD)/core_test $(BUILD)/reader_test $(BUILD)/parse_files
	$(BUILD)/core_test
	$(BUILD)/reader_test
	$(BUILD)/parse_files $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)
	bash $(ROOT)tests/syntax_inventory.sh $(BUILD)/parse_files

$(BUILD)/core_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/core.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/core.c -o $@

$(BUILD)/reader_test: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/reader.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/reader.c -o $@

.PHONY: parse-files
parse-files: $(BUILD)/parse_files

$(BUILD)/parse_files: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/parse_files.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/parse_files.c -o $@
