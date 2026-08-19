#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror compiler \
	/tmp/a-program-term-intern-index-check \
	-DPROTOTYPE_TERM_INTERN_TEST_FORCE_HASH_COLLISIONS \
	src/prototype/tests/checks/term_intern_index_check.c

/tmp/a-program-term-intern-index-check
