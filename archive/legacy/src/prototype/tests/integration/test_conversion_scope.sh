#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-conversion-scope.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

if rg -n 'static int match_frame_keys_equal' src/prototype/src/core/term.c; then
	echo "kernel conversion still has key-only Match-frame equality" >&2
	exit 1
fi

prototype_compile c11 werror graph \
	"$tmp_dir/conversion-scope-check" \
	src/prototype/tests/checks/conversion_scope_check.c

"$tmp_dir/conversion-scope-check"
