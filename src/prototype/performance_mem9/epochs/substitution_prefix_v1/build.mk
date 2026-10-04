MEM9 := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(MEM9)../performance_verification/build.mk

$(BUILD)/substitution_prefix_test: $(ROOT)graph.c $(ROOT)support.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(wildcard $(ROOT)*.h) $(MEM9)substitution_prefix_test.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(ROOT) $(ROOT)graph.c $(ROOT)support.c $(ROOT)dag.c $(ROOT)eval.c $(ROOT)symmetry.c $(ROOT)graph_io.c $(ROOT)comparison_io.c $(ROOT)eval_io.c $(ROOT)wire.c $(MEM9)substitution_prefix_test.c -o $@
