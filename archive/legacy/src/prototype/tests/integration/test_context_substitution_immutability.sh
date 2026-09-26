#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

binary=${TMPDIR:-/tmp}/a-program-context-substitution-immutability-check
prototype_compile c11 werror kernel "$binary" \
	src/prototype/tests/checks/context_substitution_immutability_check.c
"$binary"
rm -f "$binary"
