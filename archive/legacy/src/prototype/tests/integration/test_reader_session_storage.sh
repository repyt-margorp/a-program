#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-reader-session.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

./read_file.out --audit-session-reentry examples/01_bool.p \
	>"$tmp_dir/reentry.out" 2>"$tmp_dir/reentry.err"

if rg -n '^static .+\[[^]]+\]' src/prototype/src/driver/read_file.c \
		>"$tmp_dir/static-storage.out"; then
	cat "$tmp_dir/static-storage.out" >&2
	echo 'reader retains file-static backing arrays' >&2
	exit 1
fi

if rg -n 'sizeof\(storage->reachable_external_refs\)' \
		src/prototype/src/driver/read_file.c; then
	echo 'reader treats an owned reachability pointer as an inline array' >&2
	exit 1
fi

echo 'reader session storage tests passed'
