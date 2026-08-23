#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-typed-occurrence-transaction.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/typed-occurrence-transaction-check" \
	src/prototype/tests/checks/typed_occurrence_transaction_check.c

"$tmp_dir/typed-occurrence-transaction-check"
