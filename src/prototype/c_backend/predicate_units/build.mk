PREDICATE_UNITS := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(PREDICATE_UNITS)../build.mk
