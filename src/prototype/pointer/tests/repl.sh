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
printf ':nf missing\n:solve bad\n:unknown\n:whnf id\n:quit\n' |
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
printf 'copy:=id;\nbroken:=\n:status\n:save %s\n:root 1\n:whnf id\n:quit\n' "$directory/history.a" |
	"$binary" --repl "$directory/source.p" > "$directory/history" 2> "$directory/errors"
grep -q '^<interactive>:' "$directory/errors"
test "$(grep -c '^done steps=' "$directory/history")" = 5
printf 'next:=copy;\n:nf next\n:quit\n' |
	"$binary" --load --root 2 --repl "$directory/history.a" > "$directory/continued"
test "$(grep -c '^done steps=' "$directory/continued")" = 3
printf 'copy:=id;\nnext:=copy;\n:save %s\n:quit\n' "$directory/unfinished.a" |
	"$binary" --steps 0 --repl "$directory/source.p" > "$directory/status"
test "$(grep -c '^pending steps=0$' "$directory/status")" = 3
"$binary" --load --root 3 --nf next "$directory/unfinished.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
printf 'bad:=missing;\n:root 1\n:save %s\n:quit\n' "$directory/rejected.a" |
	"$binary" --repl "$directory/source.p" > "$directory/status"
code=0
"$binary" --load --root 2 "$directory/rejected.a" > "$directory/status" || code=$?
test "$code" = 1
grep -q '^rejected steps=' "$directory/status"
printf 'id::(A:@)->A->A;\n:save %s\n:root 1\nid::@;\n:root 1\n:whnf id\n:quit\n' "$directory/expect.a" |
	"$binary" --repl "$directory/source.p" > "$directory/status"
test "$(grep -c '^rejected steps=' "$directory/status")" = 1
test "$(grep -c '^done steps=' "$directory/status")" = 5
"$binary" --load --root 2 "$directory/expect.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
printf ':solve 10000\n:nf id\n:save %s\n:root 2\n:status\n:solve 10000\n:save %s\n:root 1\ncopy:=id;\n:save %s\n:quit\n' \
	"$directory/nf-pending.a" "$directory/nf-done.a" "$directory/nf-history.a" |
	"$binary" --steps 0 --repl "$directory/source.p" > "$directory/status" 2> "$directory/errors"
test ! -s "$directory/errors"
test "$(grep -c '^saved$' "$directory/status")" = 3
for image in nf-pending nf-done; do
	code=0
	"$binary" --load --root 2 --steps 0 --save "$directory/resaved.a" "$directory/$image.a" > "$directory/status" || code=$?
	test "$code" = 3
	grep -q '^pending steps=0$' "$directory/status"
	"$binary" --load --root 2 "$directory/resaved.a" > "$directory/status"
	grep -q '^done steps=' "$directory/status"
done
"$binary" --load --root 3 --nf copy "$directory/nf-history.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
"$binary" --nf id --save "$directory/batch-nf.a" "$directory/source.p" > "$directory/status"
"$binary" --load --root 2 "$directory/batch-nf.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
echo 'repl: shared normalization, pending resume/save, retained request roots and command recovery passed'
