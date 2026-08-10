#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 warnings graph \
	/tmp/a-program-context-category-check \
	src/prototype/tests/checks/context_category_check.c

/tmp/a-program-context-category-check
rm -f /tmp/a-program-context-category-check
