CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dag.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)conversion.c $(ROOT)classifier.c $(ROOT)effect_inference.c $(ROOT)evidence.c $(ROOT)derivation.c $(ROOT)computation.c $(ROOT)action.c $(ROOT)identity.c $(ROOT)symmetry.c $(ROOT)iadt.c $(ROOT)prelude.c $(ROOT)descriptor_io.c $(ROOT)context_payload.c $(ROOT)declaration_io.c

SOURCES += $(ROOT)function_graph.c

.PHONY: check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules check-acceptance check-eval-io check-identity-io
.PHONY: check-source-compatibility
check-examples: $(BUILD)/pointer-check
	bash $(ROOT)tests/examples_check.sh $(BUILD)/pointer-check $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)

check-source-compatibility: $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ROOT)tests/compatibility.sh $(BUILD)/pointer-check $(BUILD)/program_test $(ROOT)../tests/fixtures

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

check-acceptance: check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules check-source-compatibility

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
	bash $(ROOT)tests/image_cli.sh $(BUILD)/source_io_test $(BUILD)/pointer-check $(BUILD)/program_test
	$(BUILD)/identity_test
	$(BUILD)/iadt_test
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/indexed-formation.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/acc-family-parameter.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/acc-motive-wrong-field.p
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/acc-function-motive.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/acc-function-motive-wrong-field.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/recursive-dependent-package.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/computed-family-member.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/computed-family-member.p nested nestedExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/computed-family-wrong-parameter.p
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/family-parameter-recursive-field.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p recursive expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p twoSteps twoExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-field-induction.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-field-induction.p mixed expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-field-induction.p indexed indexedExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/family-functions.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/family-functions.p explicit expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/family-functions.p inline expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/family-functions.p parameterized parameterExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/family-functions.p applied parameterExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p certifiedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p aliasMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p proofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p baseMain baseExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p directMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p directProof expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p shadowMain baseExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph.p nestedProof baseExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-packet-wrong-output.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-packet-shadow.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-packet-imitation.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-named-fields.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-named-fields.p aliasMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-named-fields.p graphMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-named-fields.p valueMain expected
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/function-graph-named-not-call.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-named-duplicate.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-named-wrong-proof.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/order-reflexivity.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/order-reflexivity.p certified expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/order-reflexivity.p observed two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/order-reflexivity.p zeroCertified zeroExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-range-wrong-index.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-function-graph-mismatch.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-graph-motive.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-graph-motive.p reorderedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-graph-motive.p certifiedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-type-case.p nat expectedNat
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-type-case.p bool expectedBool
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-type-case.p selectedNat expectedNat
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-type-case.p selectedBool expectedBool
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-type-case-rejected.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p unusedMain unusedExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p orderedMain orderedExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p originalMain originalExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p originalOrdered orderedExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p originalRight leaf
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p propertyMain propertyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-call-sites.p cutMain cutExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-call-sites-rejected.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-motive-conflict.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-motive-expect-rejected.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/motive-computed-callee.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-function-witness-mismatch.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-function-graph-parameter-mismatch.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-function-graph-case-mismatch.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/generated-function-graph-direct.p certified expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/generated-function-graph-direct-forgery.p
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

$(BUILD)/program_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)synthesis.c $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/program.c -o $@

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

$(BUILD)/occurrence_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/occurrence_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ROOT)tests/occurrence_io.c -o $@

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
