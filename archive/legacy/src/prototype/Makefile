include src/prototype/build/sources.mk

CC ?= cc
CPPFLAGS += -I src/prototype/include -I src/prototype
CFLAGS += -std=c11 -Wall -Wextra

PROTOTYPE_COMPILER_IDENTITY := $(shell $(CC) --version | sed -n '1p')
PROTOTYPE_SOURCE_IDENTITY := $(shell find src/prototype/src src/prototype/include \
	-type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) -print | \
	LC_ALL=C sort | xargs sha256sum | sha256sum | awk '{ print $$1 }')
PROTOTYPE_BUILD_CONFIGURATION := $(shell printf '%s\n' \
	'compiler=$(PROTOTYPE_COMPILER_IDENTITY) cppflags=$(CPPFLAGS) cflags=$(CFLAGS) source=$(PROTOTYPE_SOURCE_IDENTITY)' | \
	sha256sum | awk '{ print $$1 }')
PROTOTYPE_OBJECT_BASE ?= $(if $(A_PROGRAM_TEST_OBJECT_ROOT), \
	$(A_PROGRAM_TEST_OBJECT_ROOT),build/prototype)
PROTOTYPE_OBJECT_ROOT := $(PROTOTYPE_OBJECT_BASE)/$(PROTOTYPE_BUILD_CONFIGURATION)
PROTOTYPE_READER_OBJECTS := $(addprefix $(PROTOTYPE_OBJECT_ROOT)/, \
	$(PROTOTYPE_READER_SOURCES:.c=.o))
PROTOTYPE_REPL_OBJECTS := $(addprefix $(PROTOTYPE_OBJECT_ROOT)/, \
	$(PROTOTYPE_REPL_SOURCES:.c=.o))
PROTOTYPE_DEPENDENCIES := $(sort \
	$(PROTOTYPE_READER_OBJECTS:.o=.d) $(PROTOTYPE_REPL_OBJECTS:.o=.d))

.PHONY: all reader clean benchmark-if8-single-compile
.PHONY: test-type-infer-and-check test-integration
.PHONY: print-kernel-sources print-graph-sources
.PHONY: print-compiler-sources
.PHONY: print-hott-sources print-repl-sources print-reader-sources

all: a.out

reader: read_file.out

a.out: $(PROTOTYPE_REPL_OBJECTS)
	$(CC) $(PROTOTYPE_REPL_OBJECTS) -o $@

read_file.out: $(PROTOTYPE_READER_OBJECTS)
	$(CC) $(PROTOTYPE_READER_OBJECTS) -o $@

$(PROTOTYPE_OBJECT_ROOT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(PROTOTYPE_DEPENDENCIES)

clean:
	rm -f a.out read_file.out
	rm -rf build/prototype

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
