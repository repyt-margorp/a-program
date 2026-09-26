#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-structural-readers.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/structural-reader-check" \
	src/prototype/tests/checks/structural_reader_check.c

"$tmp_dir/structural-reader-check"
