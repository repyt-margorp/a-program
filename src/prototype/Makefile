include src/prototype/build/sources.mk

CC ?= cc
CPPFLAGS += -I src/prototype/include -I src/prototype
CFLAGS += -std=c11 -Wall -Wextra

PROTOTYPE_IMPLEMENTATION_DEPENDENCIES := $(shell find \
	src/prototype/include src/prototype/src -type f \
	\( -name '*.h' -o -name '*.inc' \)) \
	src/prototype/calculus.h

.PHONY: all reader clean benchmark-if8-single-compile
.PHONY: test-type-infer-and-check test-integration
.PHONY: print-kernel-sources print-graph-sources
.PHONY: print-compiler-sources
.PHONY: print-hott-sources print-repl-sources print-reader-sources

all: a.out

reader: read_file.out

a.out: $(PROTOTYPE_REPL_SOURCES) $(PROTOTYPE_IMPLEMENTATION_DEPENDENCIES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROTOTYPE_REPL_SOURCES) -o $@

read_file.out: $(PROTOTYPE_READER_SOURCES) $(PROTOTYPE_IMPLEMENTATION_DEPENDENCIES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROTOTYPE_READER_SOURCES) -o $@

clean:
	rm -f a.out read_file.out

test-type-infer-and-check: reader
	sh src/prototype/tests/run_integration_suite.sh \
		--test-name test_type_infer_and_check_examples

test-integration: reader a.out
	sh src/prototype/tests/checks/test_timing_runner.sh
	sh src/prototype/tests/run_integration_suite.sh

benchmark-if8-single-compile: reader
	sh src/prototype/tests/performance/benchmark_if8_single_compile.sh

print-kernel-sources:
	@printf '%s\n' $(PROTOTYPE_KERNEL_SOURCES)

print-graph-sources:
	@printf '%s\n' $(PROTOTYPE_GRAPH_SOURCES)

print-compiler-sources:
	@printf '%s\n' $(PROTOTYPE_COMPILER_SOURCES)

print-hott-sources:
	@printf '%s\n' $(PROTOTYPE_HOTT_SOURCES)

print-repl-sources:
	@printf '%s\n' $(PROTOTYPE_REPL_SOURCES)

print-reader-sources:
	@printf '%s\n' $(PROTOTYPE_READER_SOURCES)
