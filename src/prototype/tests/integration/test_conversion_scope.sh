#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

if rg -n 'static int match_frame_keys_equal' src/prototype/term.c; then
	echo "kernel conversion still has key-only Match-frame equality" >&2
	exit 1
fi

prototype_compile c11 werror graph \
	/tmp/a-program-conversion-scope-check \
	src/prototype/tests/checks/conversion_scope_check.c

/tmp/a-program-conversion-scope-check
rm -f /tmp/a-program-conversion-scope-check
