#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-storage.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cc -I src/prototype/include -std=c11 -Wall -Wextra \
	src/prototype/tests/checks/storage_infrastructure_check.c \
	src/prototype/src/support/storage.c -o "$TMP_DIR/storage-check"
"$TMP_DIR/storage-check"

if rg -n 'static int reserve_slot\(' \
	src/prototype/src/core src/prototype/src/frontend src/prototype/src/graph \
	src/prototype/src/kernel >"$TMP_DIR/local-reserve.out"; then
	cat "$TMP_DIR/local-reserve.out" >&2
	echo "typed stores retain duplicate reserve_slot implementations" >&2
	exit 1
fi

if rg -n '^static struct prototype_(term|context|judgement|typed_occurrence)' \
	src/prototype/src/driver/repl.c >"$TMP_DIR/repl-storage.out"; then
	cat "$TMP_DIR/repl-storage.out" >&2
	echo "REPL bypasses prototype_program_storage ownership" >&2
	exit 1
fi
