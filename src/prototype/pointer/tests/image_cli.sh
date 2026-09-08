#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
fixture=$1
binary=$2
"$fixture" nominal-write "$directory/multiple.a"
"$binary" --load --root 4 --save "$directory/saved.a" "$directory/multiple.a" > "$directory/status"
cmp "$directory/multiple.a" "$directory/saved.a"
# The selected root does not erase the separately rejected root or its inputs.
code=0
"$binary" --load --root 2 "$directory/saved.a" > "$directory/status" || code=$?
test "$code" = 1
grep -q '^rejected steps=' "$directory/status"
"$fixture" nominal-read "$directory/saved.a"
code=0
"$binary" --load --root 9 "$directory/multiple.a" > "$directory/status" 2>&1 || code=$?
test "$code" = 2
grep -q 'root index out of range: 9 (count 8)' "$directory/status"
printf '%s\n' 'image cli: multi-root selection, retained obligations and range rejection passed'
