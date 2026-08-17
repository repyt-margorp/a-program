#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror compiler \
	/tmp/a-program-dimension-term-check \
	src/prototype/tests/checks/core/dimension_term_check.c

/tmp/a-program-dimension-term-check
rm -f /tmp/a-program-dimension-term-check
