#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-context-category.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 warnings graph \
	"$tmp_dir/context-category-check" \
	src/prototype/tests/checks/context_category_check.c

"$tmp_dir/context-category-check"
