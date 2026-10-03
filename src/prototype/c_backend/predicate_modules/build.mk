NATIVE_PREDICATE_MODULES := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
include $(NATIVE_PREDICATE_MODULES)../build.mk
