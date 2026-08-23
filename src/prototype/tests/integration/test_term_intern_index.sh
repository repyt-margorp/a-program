#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-term-intern-index.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/term-intern-index-check" \
	-DPROTOTYPE_TERM_INTERN_TEST_FORCE_HASH_COLLISIONS \
	src/prototype/tests/checks/term_intern_index_check.c

"$tmp_dir/term-intern-index-check"
