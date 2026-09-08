CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)conversion.c $(ROOT)classifier.c $(ROOT)evidence.c $(ROOT)computation.c $(ROOT)action.c $(ROOT)identity.c $(ROOT)symmetry.c $(ROOT)iadt.c $(ROOT)prelude.c

.PHONY: check
check: $(BUILD)/core_test $(BUILD)/reader_test $(BUILD)/parse_files $(BUILD)/synthesis_test $(BUILD)/identity_test $(BUILD)/iadt_test $(BUILD)/program_test
	$(BUILD)/core_test
	$(BUILD)/reader_test
	$(BUILD)/synthesis_test
	$(BUILD)/program_test
	$(BUILD)/identity_test
	$(BUILD)/iadt_test
	$(BUILD)/parse_files $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)
	bash $(ROOT)tests/syntax_inventory.sh $(BUILD)/parse_files

$(BUILD)/core_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/core.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/core.c -o $@

$(BUILD)/identity_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/identity.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/identity.c -o $@

$(BUILD)/iadt_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/iadt.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/iadt.c -o $@

$(BUILD)/synthesis_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)tests/synthesis.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)tests/synthesis.c -o $@

$(BUILD)/program_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)tests/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)tests/program.c -o $@

$(BUILD)/reader_test: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/reader.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/reader.c -o $@

.PHONY: parse-files
parse-files: $(BUILD)/parse_files

$(BUILD)/parse_files: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/parse_files.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/parse_files.c -o $@
