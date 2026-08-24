#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-judgement-transaction.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror compiler \
	"$tmp_dir/judgement-transaction-check" \
	src/prototype/tests/checks/judgement_transaction_check.c

"$tmp_dir/judgement-transaction-check"

accepted_replay=src/prototype/src/kernel/typing/accepted_replay.inc
if grep -q 'prototype_judgement_candidate_premise' "$accepted_replay"; then
	echo 'accepted replay still owns solver-local candidate premise storage' >&2
	exit 1
fi
if grep -R -n '(void)prototype_judgement_db_rebuild_index' \
		src/prototype/src/kernel/typing \
		src/prototype/src/kernel/rules; then
	echo 'Judgement rollback still ignores an index rebuild result' >&2
	exit 1
fi

echo 'judgement transaction tests passed'
