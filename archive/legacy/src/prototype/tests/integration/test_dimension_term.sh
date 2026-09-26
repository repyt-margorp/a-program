#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-dimension-term.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/dimension-term-check" \
	src/prototype/tests/checks/core/dimension_term_check.c

"$tmp_dir/dimension-term-check"
