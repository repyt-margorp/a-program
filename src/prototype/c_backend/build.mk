C_BACKEND := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERLAY ?= /tmp/a-program-c-backend-base
include $(C_BACKEND)../artifact_persistence/build.mk

LINK_SOURCES := $(C_BACKEND)link/plan.c $(C_BACKEND)link/driver.c
LOWER_SOURCES := $(C_BACKEND)lower/scalar.c $(C_BACKEND)lower/representation.c $(C_BACKEND)lower/nodes.c $(C_BACKEND)selection.c
$(BUILD)/a-to-c: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)main.c $(C_BACKEND)emit.c $(C_BACKEND)emit.h $(C_BACKEND)runtime.h $(LINK_SOURCES) $(LOWER_SOURCES)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPG_C_BACKEND_DIRECTORY='"$(C_BACKEND)"' -I$(ROOT) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)main.c $(C_BACKEND)emit.c $(LINK_SOURCES) $(LOWER_SOURCES) -o $@

.PHONY: check-c-link
check-c-link: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)link/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-shared
check-c-shared: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)shared/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-callbacks
check-c-callbacks: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback_inert_test
	bash $(C_BACKEND)callback/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback_inert_test

.PHONY: check-c-callback-modules
check-c-callback-modules: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback_inert_test
	bash $(C_BACKEND)callback_modules/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback_inert_test

.PHONY: check-c-callbacks2
check-c-callbacks2: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback2_inert_test
	bash $(C_BACKEND)callback2/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_callback2_inert_test

.PHONY: check-c-native-predicates
check-c-native-predicates: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_predicate_native_inert_test
	bash $(C_BACKEND)predicate_native/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_predicate_native_inert_test

.PHONY: check-c-native-predicate-modules
check-c-native-predicate-modules: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)predicate_modules/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-native-predicate-units
check-c-native-predicate-units: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)predicate_units/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-integer-predicate-boundary
check-c-integer-predicate-boundary: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)predicate_integer/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-recursive-captures
check-c-recursive-captures: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_recursive_capture_inert_test
	bash $(C_BACKEND)recursive_capture/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_recursive_capture_inert_test

$(BUILD)/c_recursive_capture_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)recursive_capture/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)recursive_capture/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

.PHONY: check-c-nested-captures
check-c-nested-captures: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_nested_capture_inert_test
	bash $(C_BACKEND)nested_capture/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_nested_capture_inert_test

$(BUILD)/c_nested_capture_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)nested_capture/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)nested_capture/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

.PHONY: check-c-applied-types
check-c-applied-types: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_applied_type_inert_test
	bash $(C_BACKEND)applied_types/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_applied_type_inert_test

$(BUILD)/c_applied_type_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)applied_types/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)applied_types/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

.PHONY: check-c-source-sort
check-c-source-sort: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_source_sort_inert_test
	bash $(C_BACKEND)source_sort/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_source_sort_inert_test

$(BUILD)/c_source_sort_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)source_sort/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)source_sort/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_predicate_native_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)predicate_native/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)predicate_native/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_callback2_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)callback2/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)callback2/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_callback_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)callback/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)callback/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

.PHONY: check-c-scalar
check-c-scalar: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_scalar_test
	bash $(C_BACKEND)lower/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_scalar_test

.PHONY: check-c-enum
check-c-enum: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/enum_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-data
check-c-data: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/data_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-list
check-c-list: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/list_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-numeric-list
check-c-numeric-list: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/numeric_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-static-functions
check-c-static-functions: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_static_oracle_test
	bash $(C_BACKEND)lower/static_function_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_static_oracle_test

.PHONY: check-c-transitive-functions
check-c-transitive-functions: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_transitive_oracle_test
	bash $(C_BACKEND)lower/transitive_function_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_transitive_oracle_test

$(BUILD)/c_transitive_oracle_test: $(SOURCES) $(wildcard $(ROOT)*.h $(C_BACKEND)*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)lower/transitive_oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND)lower $(SOURCES) $(C_BACKEND)lower/transitive_oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

.PHONY: check-c-value-records
check-c-value-records: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/nested_record_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-enum-list
check-c-enum-list: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)lower/enum_list_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-record-list
check-c-record-list: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)record_list/check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

.PHONY: check-c-applied-families
check-c-applied-families: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_applied_inert_test
	bash $(C_BACKEND)applied/gate.sh $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_applied_inert_test

$(BUILD)/c_applied_inert_test: $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(wildcard $(ROOT)*.h $(ROOT)artifact/*.h $(C_BACKEND)*.h $(C_BACKEND)link/*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)applied/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND) $(SOURCES) $(filter-out $(ROOT)main.c,$(CLI_SOURCES)) $(C_BACKEND)applied/inert_test.c $(C_BACKEND)link/plan.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -Wl,--wrap=pg_whnf_advance -Wl,--wrap=pg_typed_query_advance -o $@

$(BUILD)/c_static_oracle_test: $(SOURCES) $(wildcard $(ROOT)*.h $(C_BACKEND)*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)lower/static_oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) -I$(C_BACKEND)lower $(SOURCES) $(C_BACKEND)lower/static_oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -o $@

$(BUILD)/c_scalar_test: $(SOURCES) $(wildcard $(ROOT)*.h $(C_BACKEND)*.h $(C_BACKEND)lower/*.h) $(C_BACKEND)lower/oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c $(C_BACKEND)emit.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(C_BACKEND)lower/oracle_test.c $(LOWER_SOURCES) $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -o $@

.PHONY: check-c-backend
check-c-backend: $(BUILD)/a-to-c $(BUILD)/pointer-check $(BUILD)/c_oracle_test
	bash $(C_BACKEND)check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)oracle_check.sh $(BUILD)/c_oracle_test

.PHONY: check-c-sorting-boundary
check-c-sorting-boundary: $(BUILD)/a-to-c $(BUILD)/pointer-check
	bash $(C_BACKEND)sorting_check.sh $(BUILD)/a-to-c $(BUILD)/pointer-check

$(BUILD)/c_oracle_test: $(SOURCES) $(wildcard $(ROOT)*.h) $(C_BACKEND)oracle_test.c $(C_BACKEND)emit.c $(C_BACKEND)emit.h $(C_BACKEND)runtime.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(SOURCES) $(C_BACKEND)oracle_test.c $(C_BACKEND)emit.c -Wl,--wrap=pg_eval_advance -Wl,--wrap=pg_substitution_advance -o $@
