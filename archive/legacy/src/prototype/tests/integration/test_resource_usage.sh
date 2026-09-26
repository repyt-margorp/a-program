#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-resource-usage.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror kernel \
	"$tmp_dir/resource-usage-check" \
	src/prototype/tests/checks/resource_usage_check.c

"$tmp_dir/resource-usage-check"
