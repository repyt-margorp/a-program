CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dag.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)conversion.c $(ROOT)classifier.c $(ROOT)effect_inference.c $(ROOT)evidence.c $(ROOT)derivation.c $(ROOT)computation.c $(ROOT)action.c $(ROOT)identity.c $(ROOT)symmetry.c $(ROOT)iadt.c $(ROOT)prelude.c $(ROOT)descriptor_io.c $(ROOT)context_payload.c $(ROOT)declaration_io.c

.PHONY: check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules check-acceptance check-eval-io check-identity-io
check-examples: $(BUILD)/pointer-check
	bash $(ROOT)tests/examples_check.sh $(BUILD)/pointer-check $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)

check-open-families: $(BUILD)/pointer-check
	bash $(ROOT)tests/examples_check.sh $(BUILD)/pointer-check $(ROOT)tests/acceptance/closed-family.p $(ROOT)tests/acceptance/open-family.p $(ROOT)tests/acceptance/applied-open-family.p $(ROOT)tests/acceptance/sequenced-open-family.p

check-example-results: $(BUILD)/program_test
	$(BUILD)/program_test $(ROOT)../../../examples/03_main.p Bool true true 0
	$(BUILD)/program_test $(ROOT)../../../examples/04_match.p Bool true true 0
	$(BUILD)/program_test $(ROOT)../../../examples/05_bool_to_nat.p Nat zero succ 1
	$(BUILD)/program_test $(ROOT)../../../examples/06_pred.p Nat zero succ 0
	$(BUILD)/program_test $(ROOT)../../../examples/07_add.p Nat zero succ 5
	$(BUILD)/program_test $(ROOT)../../../examples/09_list_induction.p Nat zero succ 3

check-image-origins: $(BUILD)/source_io_test $(BUILD)/pointer-check
	bash $(ROOT)tests/image_origins.sh $(BUILD)/source_io_test

check-prepared-modules: $(BUILD)/source_io_test
	$(BUILD)/source_io_test prepared-module

check-operation-origins: $(BUILD)/source_io_test
	$(BUILD)/source_io_test operation-origins

check-handler-origins: $(BUILD)/source_io_test
	$(BUILD)/source_io_test handler-origins

check-handler-nesting: $(BUILD)/source_io_test
	$(BUILD)/source_io_test handler-nesting
	$(BUILD)/source_io_test handler-boundaries

check-acceptance: check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules

check-eval-io: $(BUILD)/eval_io_test
	bash $(ROOT)tests/eval_io.sh $(BUILD)/eval_io_test

check-identity-io: $(BUILD)/identity_io_test
	bash $(ROOT)tests/identity_io.sh $(BUILD)/identity_io_test

check: check-eval-io check-identity-io

$(BUILD)/identity_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)retained_io.c $(ROOT)derivation_io.c $(ROOT)reduction_io.c $(ROOT)machine_io.c $(ROOT)computation_io.c $(ROOT)identity_io.c $(ROOT)comparison_io.c $(ROOT)graph_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/identity_io.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)retained_io.c $(ROOT)derivation_io.c $(ROOT)reduction_io.c $(ROOT)machine_io.c $(ROOT)computation_io.c $(ROOT)identity_io.c $(ROOT)comparison_io.c $(ROOT)graph_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/identity_io.c -o $@

check: $(BUILD)/core_test $(BUILD)/graph_io_test $(BUILD)/graph_acceptance_test $(BUILD)/occurrence_io_test $(BUILD)/derivation_io_test $(BUILD)/syntax_io_test $(BUILD)/source_io_test $(BUILD)/reader_test $(BUILD)/parse_files $(BUILD)/synthesis_test $(BUILD)/identity_test $(BUILD)/iadt_test $(BUILD)/program_test $(BUILD)/seed_test $(BUILD)/pointer-check
	$(BUILD)/core_test
	$(BUILD)/graph_io_test
	bash $(ROOT)tests/graph_acceptance.sh $(BUILD)/graph_acceptance_test
	bash $(ROOT)tests/occurrence_io.sh $(BUILD)/occurrence_io_test
	bash $(ROOT)tests/derivation_io.sh $(BUILD)/derivation_io_test
	$(BUILD)/syntax_io_test $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)
	bash $(ROOT)tests/source_io.sh $(BUILD)/source_io_test
	$(BUILD)/source_io_test normalization
	$(BUILD)/reader_test
	$(BUILD)/synthesis_test
	$(BUILD)/program_test
	$(BUILD)/seed_test
	bash $(ROOT)tests/seed.sh $(BUILD)/seed_test
	bash $(ROOT)tests/cli.sh $(BUILD)/pointer-check
	bash $(ROOT)tests/import_cli.sh $(BUILD)/pointer-check
	bash $(ROOT)tests/repl.sh $(BUILD)/pointer-check $(BUILD)/source_io_test
	bash $(ROOT)tests/image_cli.sh $(BUILD)/source_io_test $(BUILD)/pointer-check
	$(BUILD)/identity_test
	$(BUILD)/iadt_test
	$(BUILD)/parse_files $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)
	bash $(ROOT)tests/syntax_inventory.sh $(BUILD)/parse_files

$(BUILD)/core_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/core.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/core.c -o $@

$(BUILD)/eval_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/eval_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/eval_io.c -o $@

$(BUILD)/identity_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/identity.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/identity.c -o $@

$(BUILD)/iadt_test: $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(wildcard $(ROOT)*.h) $(ROOT)tests/iadt.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)tests/iadt.c -o $@

$(BUILD)/synthesis_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)tests/synthesis.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)tests/synthesis.c -o $@

$(BUILD)/program_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)tests/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)tests/program.c -o $@

$(BUILD)/seed_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)seed.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/seed.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)seed.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/seed.c -o $@

$(BUILD)/syntax_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/syntax_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/syntax_io.c -o $@

$(BUILD)/source_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/source_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/source_io.c -o $@

$(BUILD)/graph_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/graph_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)tests/graph_io.c -o $@

$(BUILD)/graph_acceptance_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)wire.c $(ROOT)tests/graph_acceptance.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)wire.c $(ROOT)tests/graph_acceptance.c -o $@

$(BUILD)/occurrence_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)typing.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/occurrence_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)typing.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ROOT)tests/occurrence_io.c -o $@

$(BUILD)/derivation_io_test: $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)graph_io.c $(ROOT)derivation_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/derivation_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)graph_io.c $(ROOT)derivation_io.c $(ROOT)wire.c $(ROOT)tests/derivation_io.c -o $@

.PHONY: pointer-check
pointer-check: $(BUILD)/pointer-check

$(BUILD)/pointer-check: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)syntax_io.c $(ROOT)derivation_io.c $(ROOT)main.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)syntax_io.c $(ROOT)derivation_io.c $(ROOT)main.c -o $@

$(BUILD)/reader_test: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/reader.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/reader.c -o $@

.PHONY: parse-files
parse-files: $(BUILD)/parse_files

$(BUILD)/parse_files: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/parse_files.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/parse_files.c -o $@
