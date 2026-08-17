#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-dimension.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cc -I src/prototype/include -std=c11 -Wall -Wextra \
	src/prototype/tests/checks/dimension/dimension_action_check.c \
	src/prototype/src/dimension/operator.c \
	src/prototype/src/dimension/face.c \
	src/prototype/src/support/storage.c \
	-o "$TMP_DIR/dimension-action-check"
"$TMP_DIR/dimension-action-check"

if rg -n '\[(8|9|576)\]' \
	src/prototype/include/a_program/dimension \
	src/prototype/src/dimension >"$TMP_DIR/fixed-face-array.out"; then
	cat "$TMP_DIR/fixed-face-array.out" >&2
	echo "dimension module contains a fixed higher-face array" >&2
	exit 1
fi
