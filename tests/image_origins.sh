#!/usr/bin/env bash
set -euo pipefail
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
"$1" origin-write "$directory/origins.a"
"$1" origin-read "$directory/origins.a"
"$1" function-write "$directory/functions.a"
"$1" function-read "$directory/functions.a"
# A source-only snapshot need not retain every intermediate branch scope.
binary="$(dirname "$1")/pointer-check"
example="$(dirname "${BASH_SOURCE[0]}")/../examples/07_add.p"
"$binary" --nf main --save "$directory/add.a" "$example" > "$directory/source"
"$binary" --load --nf main "$directory/add.a" > "$directory/restored"
sed '1d' "$directory/source" > "$directory/source-value"
sed '1d' "$directory/restored" > "$directory/restored-value"
cmp "$directory/source-value" "$directory/restored-value"
# Return-only Fold must retain its own continuation binder, not merely an
# alpha-equivalent re-elaboration. Keep this full-acceptance gate explicit.
"$1" retained-write "$directory/fold.a" fold
"$1" retained-resave "$directory/fold.a" "$directory/fold-again.a"
"$1" retained-resave "$directory/fold-again.a" "$directory/fold-final.a"
"$1" retained-check "$directory/fold-final.a"
"$1" retained-recompute "$directory/fold-final.a"
