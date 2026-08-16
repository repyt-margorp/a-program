#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

prototype_compile c11 werror compiler \
	/tmp/a-program-term-child-role-check \
	src/prototype/tests/checks/term_child_role_check.c

/tmp/a-program-term-child-role-check
rm -f /tmp/a-program-term-child-role-check
