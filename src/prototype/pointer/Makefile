CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
ROOT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT).build
SOURCES := $(ROOT)graph.c $(ROOT)dag.c $(ROOT)dimension.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)conversion.c $(ROOT)classifier.c $(ROOT)effect_inference.c $(ROOT)evidence.c $(ROOT)derivation.c $(ROOT)computation.c $(ROOT)action.c $(ROOT)identity.c $(ROOT)symmetry.c $(ROOT)iadt.c $(ROOT)prelude.c $(ROOT)descriptor_io.c $(ROOT)context_payload.c $(ROOT)declaration_io.c

SOURCES += $(ROOT)function_graph.c $(ROOT)host.c $(ROOT)execution.c
SOURCES += $(ROOT)evidence_function.c $(ROOT)evidence_cbpv.c
WITNESS_SOURCE := $(ROOT)function_witness.c
SYNTHESIS_SOURCES := $(ROOT)synthesis.c $(ROOT)synthesis_work.c $(ROOT)synthesis_effect.c $(ROOT)synthesis_handler.c $(ROOT)synthesis_operation.c $(ROOT)synthesis_cbpv.c $(ROOT)synthesis_function.c $(ROOT)synthesis_context.c $(ROOT)synthesis_iadt.c $(ROOT)synthesis_identity.c
CLI_SOURCES := $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)syntax_io.c $(ROOT)derivation_io.c $(ROOT)main.c

.PHONY: check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules check-acceptance check-eval-io check-identity-io
.PHONY: check-source-compatibility check-host check-execution check-sort-insertion check-generic-sorted check-generic-retained check-quick-result
check-examples: $(BUILD)/pointer-check
	bash $(ROOT)tests/examples_check.sh $(BUILD)/pointer-check $(wildcard $(ROOT)../../../examples/0[1-9]_*.p)

check-host: $(BUILD)/host_test
	$(BUILD)/host_test

check-execution: $(BUILD)/execution_test $(BUILD)/pointer-check
	$(BUILD)/execution_test
	bash $(ROOT)tests/execution.sh $(BUILD)/pointer-check

$(BUILD)/execution_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)tests/execution.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)tests/execution.c -o $@

$(BUILD)/host_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)tests/host.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)tests/host.c -o $@

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

check-acceptance: check-host check-execution check check-examples check-example-results check-open-families check-image-origins check-operation-origins check-handler-origins check-handler-nesting check-prepared-modules check-source-compatibility check-sort-insertion check-generic-sorted check-generic-retained check-witness-isolation

check-generic-sorted: $(BUILD)/pointer-check $(BUILD)/syntax_io_test $(BUILD)/program_test
	$(BUILD)/syntax_io_test $(ROOT)tests/acceptance/decision-explicit-result.p $(ROOT)tests/acceptance/generic-quick-sorted.p
	bash $(ROOT)tests/generic_sorted.sh $(BUILD)/pointer-check

check-generic-retained: $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ROOT)tests/generic_sorted.sh $(BUILD)/pointer-check 1
	bash $(ROOT)tests/retained_quicksort.sh $(BUILD)/pointer-check
	bash $(ROOT)tests/derived_lt.sh $(BUILD)/pointer-check

check-sort-insertion: $(BUILD)/pointer-check $(BUILD)/program_test
	bash $(ROOT)tests/sort_insertion.sh $(BUILD)/pointer-check $(BUILD)/program_test

check-eval-io: $(BUILD)/eval_io_test
	bash $(ROOT)tests/eval_io.sh $(BUILD)/eval_io_test

check-identity-io: $(BUILD)/identity_io_test
	bash $(ROOT)tests/identity_io.sh $(BUILD)/identity_io_test

check: check-eval-io check-identity-io check-quick-result

check: $(BUILD)/typed_structure_test

$(BUILD)/typed_structure_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ROOT)tests/typed_structure.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ROOT)tests/typed_structure.c -o $@

check-quick-result: $(BUILD)/pointer-check
	bash $(ROOT)tests/quick_result.sh $(BUILD)/pointer-check

$(BUILD)/identity_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)retained_io.c $(ROOT)derivation_io.c $(ROOT)reduction_io.c $(ROOT)machine_io.c $(ROOT)computation_io.c $(ROOT)identity_io.c $(ROOT)comparison_io.c $(ROOT)graph_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/identity_io.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)retained_io.c $(ROOT)derivation_io.c $(ROOT)reduction_io.c $(ROOT)machine_io.c $(ROOT)computation_io.c $(ROOT)identity_io.c $(ROOT)comparison_io.c $(ROOT)graph_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/identity_io.c -o $@

check: $(BUILD)/core_test $(BUILD)/graph_io_test $(BUILD)/graph_acceptance_test $(BUILD)/occurrence_io_test $(BUILD)/derivation_io_test $(BUILD)/syntax_io_test $(BUILD)/source_io_test $(BUILD)/reader_test $(BUILD)/parse_files $(BUILD)/synthesis_test $(BUILD)/identity_test $(BUILD)/iadt_test $(BUILD)/program_test $(BUILD)/seed_test $(BUILD)/pointer-check
	$(BUILD)/core_test
	bash $(ROOT)tests/typed_structure.sh $(BUILD)/typed_structure_test
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
	bash $(ROOT)tests/merge_composition.sh $(BUILD)/pointer-check $(BUILD)/program_test
	$(BUILD)/seed_test
	bash $(ROOT)tests/seed.sh $(BUILD)/seed_test
	bash $(ROOT)tests/cli.sh $(BUILD)/pointer-check
	bash $(ROOT)tests/import_cli.sh $(BUILD)/pointer-check
	bash $(ROOT)tests/repl.sh $(BUILD)/pointer-check $(BUILD)/source_io_test
	bash $(ROOT)tests/image_cli.sh $(BUILD)/source_io_test $(BUILD)/pointer-check $(BUILD)/program_test
	$(BUILD)/identity_test
	$(BUILD)/iadt_test
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/typed-selection-motive.p
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/indexed-formation.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-declaration.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-two-inputs.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-effects.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-effects.p spineMain spineExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-later-recovery.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-dependent.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-graph.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-copy.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/inferred-index-copy.p graphMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-wrong-family.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-earlier-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-partial-dependent.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-disagreement.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-unrecoverable.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-no-inverse.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/inferred-index-unselected-invalid.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/acc-family-parameter.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/acc-motive-wrong-field.p
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/acc-renamed-wrong-ih.p
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/acc-explicit-motive.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/acc-explicit-wrong-ih.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/lt-derived-lift.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/lt-derived-lift.p recursiveMain recursiveExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/termination.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/termination-wrong-target.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/termination-wrong-value.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-partial-source.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-partial-source.p aliasMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-partial-source-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-motive-domain.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-motive-domain-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-field-path.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-field-path.p dependent expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-dependent-field-path-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-dependent-field-evidence-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-tail-path.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-dependent-tail-path-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p specMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p emptySpecMain right
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p growMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p chooseMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-curried.p twiceMain four
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-curried-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p emptyMain emptyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p specMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p emptySpecMain emptyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p copyMain sample
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p countMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed.p pointMain one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-indexed-canonical.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-indexed-wrong.p
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/function-graph-fixed-index.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-family-parameters.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-family-parameters.p relatedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-function-field.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-function-field.p fromMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-function-field.p walkMain three
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-function-field.p proofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-function-field.p visitMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-function-field-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-exposed-match.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-exposed-match.p proofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-exposed-match.p quotedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-exposed-match.p sequencedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-exposed-match.p indexedMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-exposed-match-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-known-match.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-known-match.p proofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-known-match.p unpackMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-known-match.p skipMain zero
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-known-match-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p proofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p emptyMain zero
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p prefixMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p prefixProofMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p afterMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p nestedMain three
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p directMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p walkMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p indexedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-tree.p refinedMain one
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-branch-tree-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-branch-name-collision.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/graph-duplicate-leaf.p last one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/graph-helper-leaf.p last one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/graph-canonical-leaf-name.p main one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/graph-comparison-leaves.p smaller trueValue
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/graph-comparison-leaves.p greater falseValue
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/graph-duplicate-leaf-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/graph-duplicate-leaf-ambiguous.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p main one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p proofMain one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p twiceMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p twiceProofMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p appendMain appendExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p genericMain one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p againMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-helper-call.p againEmpty zero
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-helper-call-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p applyMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p propertyMain two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p filterMain filterExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p allMain sample
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-callable-parameter.p noneMain noneExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-callable-parameter-wrong.p
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/acc-function-motive.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/acc-function-motive-wrong-field.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-order-reflexivity.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-function-motive.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/dependent-function-motive.p dependentMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-function-motive-wrong-base.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-function-motive-wrong-argument.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/dependent-function-motive-wrong-expect.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/recursive-dependent-package.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/length-output-proof.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/length-output-proof.p emptyMain emptyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/length-output-proof.p specMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/length-output-proof.p emptySpecMain emptyExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/length-output-proof-wrong.p
	$(BUILD)/program_test --reject $(ROOT)../tests/fixtures/negative/function_graph_incompatible_recursive_property.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/computed-family-member.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/computed-family-member.p nested nestedExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/computed-family-wrong-parameter.p
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/family-parameter-recursive-field.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p recursive expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-match.p twoSteps twoExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p otherMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p dependentMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p mixedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p mixedOther boolExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-payload.p functionMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-payload-wrong-branch.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-environment.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-environment.p originalMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-environment.p dependentMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-environment.p zeroMain zeroExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-dependent-environment.p pointMain zeroExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-dependent-environment-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-rigid-refutation.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-rigid-refutation.p boolMain boolExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-rigid-refutation.p emptyMain emptyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-rigid-refutation.p functionMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-rigid-refutation.p secondIndexMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-rigid-refutation-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-field-transport.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-field-transport.p pairMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-field-transport-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-field-transport-expect.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/le-predecessor.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-later-scope.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-later-scope-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/le-transitivity.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/le-transitivity.p base baseExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/le-transitivity.p deep deepExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/le-transitivity-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/comparator-order.p base trueValue
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/comparator-order.p greater falseValue
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/comparator-order.p smaller trueValue
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/comparator-order.p same trueValue
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/comparator-order-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/le-predecessor-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/le-predecessor-invalid.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-result-transport.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-result-transport.p otherMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-result-transport.p bothMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-result-transport.p functionMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-result-transport.p functionOtherMain expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-result-transport-expect.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/induction-index-environment.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/induction-index-environment.p readMain two
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/induction-index-environment-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/acc-accessible-successor.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-block-demands.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-block-demands.p nestedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-block-demands.p selectedMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-block-demands.p readMain two
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-block-demands-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-recursive-result.p main two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-recursive-result.p emptyMain emptyExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-recursive-result-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-schema-result.p main two
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-schema-result-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/applied-family-alias.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/applied-family-alias.p aliasMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/applied-family-alias.p blockMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/applied-family-alias.p indexedMain indexedExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/applied-family-alias-wrong.p
	$(BUILD)/program_test --unsupported $(ROOT)tests/acceptance/applied-family-alias-thunk.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-refined-case.p main expected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-missing-case.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p functionMain expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p emptyMain emptyExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p tailMain input
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p tailEmpty tailExpected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-omitted-case.p tailFunctionMain input
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-tail-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/indexed-computed-type.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/computed-index-arithmetic.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nonrecursive-computed-index.p main one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nonrecursive-computed-index.p other two
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nonrecursive-open-index.p main one
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nonrecursive-open-index.p other two
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/nonrecursive-open-index-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-computed-type-wrong.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nested-index-motive.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/nested-index-motive.p recursiveMain recursiveExpected
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/nested-index-motive-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-omitted-case-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-rigid-induction-invalid.p
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/acc-accessible-successor.p lowerMain zeroExpected
	$(BUILD)/pointer-check $(ROOT)tests/acceptance/indexed-ih-environment.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/indexed-ih-environment-wrong-field.p
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
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-captured-match.p main expected
	$(BUILD)/program_test --equal $(ROOT)tests/acceptance/function-graph-captured-match.p other two
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/function-graph-captured-match-wrong.p
	$(BUILD)/program_test --reject $(ROOT)tests/acceptance/recursive-thunk-conversion-wrong.p
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

$(BUILD)/core_test: $(SOURCES) $(WITNESS_SOURCE) $(wildcard $(ROOT)*.h) $(ROOT)tests/core.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(WITNESS_SOURCE) $(ROOT)tests/core.c -o $@

$(BUILD)/eval_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/eval_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(ROOT)tests/eval_io.c -o $@

$(BUILD)/identity_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/identity.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)tests/identity.c -o $@

$(BUILD)/iadt_test: $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)tests/iadt.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)tests/iadt.c -o $@

$(BUILD)/synthesis_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)tests/synthesis.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)tests/synthesis.c -o $@

$(BUILD)/program_test: $(SOURCES) $(WITNESS_SOURCE) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/program.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(WITNESS_SOURCE) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/program.c -o $@

$(BUILD)/seed_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)seed.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/seed.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)seed.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/seed.c -o $@

$(BUILD)/syntax_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/syntax_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/syntax_io.c -o $@

$(BUILD)/source_io_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/source_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)program.c $(ROOT)source_io.c $(ROOT)retained_io.c $(ROOT)reduction_io.c $(ROOT)derivation_io.c $(ROOT)graph_io.c $(ROOT)syntax_io.c $(ROOT)wire.c $(ROOT)tests/source_io.c -o $@

$(BUILD)/graph_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/graph_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)wire.c $(ROOT)tests/graph_io.c -o $@

$(BUILD)/graph_acceptance_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)wire.c $(ROOT)tests/graph_acceptance.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)wire.c $(ROOT)tests/graph_acceptance.c -o $@

$(BUILD)/occurrence_io_test: $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)classifier.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/occurrence_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)typing.c $(ROOT)classifier.c $(ROOT)graph_io.c $(ROOT)context_io.c $(ROOT)context_payload.c $(ROOT)occurrence_io.c $(ROOT)wire.c $(ROOT)tests/occurrence_io.c -o $@

$(BUILD)/derivation_io_test: $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)graph_io.c $(ROOT)derivation_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(ROOT)tests/derivation_io.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(ROOT)reader.c $(ROOT)syntax.c $(SYNTHESIS_SOURCES) $(ROOT)graph_io.c $(ROOT)derivation_io.c $(ROOT)wire.c $(ROOT)tests/derivation_io.c -o $@

.PHONY: pointer-check
pointer-check: $(BUILD)/pointer-check

.PHONY: check-witness-isolation check-witness-packets
check-acceptance: check-witness-packets
check-witness-packets: $(BUILD)/program_test
	bash $(ROOT)tests/function_witness_packets.sh $(BUILD)/program_test

check-witness-isolation: $(BUILD)/pointer-check $(BUILD)/pointer-check-with-witness $(BUILD)/program_test
	bash $(ROOT)tests/function_witness_isolation.sh $(BUILD)/pointer-check $(BUILD)/pointer-check-with-witness $(BUILD)/program_test

# The ordinary compiler does not link the optional witness construction module.
$(BUILD)/pointer-check: $(SOURCES) $(wildcard $(ROOT)*.h) $(CLI_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(CLI_SOURCES) -o $@

# A/B control: linking the optional module must not change source acceptance.
$(BUILD)/pointer-check-with-witness: $(SOURCES) $(WITNESS_SOURCE) $(wildcard $(ROOT)*.h) $(CLI_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(WITNESS_SOURCE) $(CLI_SOURCES) -o $@

$(BUILD)/reader_test: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/reader.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/reader.c -o $@

.PHONY: parse-files
parse-files: $(BUILD)/parse_files

$(BUILD)/parse_files: $(ROOT)reader.c $(ROOT)reader.h $(ROOT)syntax.c $(ROOT)syntax.h $(ROOT)graph.c $(ROOT)graph.h $(ROOT)tests/parse_files.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)reader.c $(ROOT)syntax.c $(ROOT)graph.c $(ROOT)tests/parse_files.c -o $@
