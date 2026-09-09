#!/usr/bin/env bash
set -euo pipefail
binary=$1
fixture=$2
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
printf '%s\n' 'id:=&(\A:@ => \x:A => x);' > "$directory/source.p"
printf ':nf id;\n:quit\n' | "$binary" --repl "$directory/source.p" > "$directory/interactive"
"$binary" --nf id "$directory/source.p" > "$directory/batch"
sed '1,2d' "$directory/interactive" > "$directory/interactive-value"
sed '1d' "$directory/batch" > "$directory/batch-value"
cmp "$directory/interactive-value" "$directory/batch-value"
printf ':status\n:save %s\n:solve 10000\n:status\n:quit\n' "$directory/pending.a" |
	"$binary" --steps 0 --repl "$directory/source.p" > "$directory/resume"
test "$(grep -c '^pending steps=0$' "$directory/resume")" = 2
test "$(grep -c '^done steps=' "$directory/resume")" = 2
"$binary" --load "$directory/pending.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
printf ':nf missing\n:solve bad\nunknown\n:whnf id\n:quit\n' |
	"$binary" --load --repl "$directory/pending.a" > "$directory/status" 2> "$directory/errors"
grep -q '^definition not found$' "$directory/errors"
grep -q '^invalid step budget$' "$directory/errors"
grep -q '^expected :solve' "$directory/errors"
test "$(grep -c '^done steps=' "$directory/status")" = 2
"$fixture" nominal-write "$directory/multiple.a"
printf ':root 2\n:root 0\n:root 9\n:status\n:root 4\n:save %s\n:quit\n' "$directory/all.a" |
	"$binary" --load --root 4 --repl "$directory/multiple.a" > "$directory/status" 2> "$directory/errors"
test "$(grep -c '^rejected steps=' "$directory/status")" = 2
test "$(grep -c '^done steps=' "$directory/status")" = 2
test "$(grep -c '^root index must be in 1..8$' "$directory/errors")" = 2
"$fixture" nominal-read "$directory/all.a"
echo 'repl: shared normalization, pending resume/save and command recovery passed'
