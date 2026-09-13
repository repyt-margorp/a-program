#!/usr/bin/env bash
set -eu
binary=$1
check() {
	expected=$1
	status=$2
	source=$3
	shift 3
	code=0
	output=$(printf '%s' "$source" | "$binary" "$@" - 2>&1) || code=$?
	if [ "$code" != "$expected" ]; then
		printf 'expected exit %s, got %s: %s\n' "$expected" "$code" "$output" >&2
		exit 1
	fi
	case "$output" in
		"$status"*) ;;
		*) printf 'unexpected output: %s\n' "$output" >&2; exit 1 ;;
	esac
}
check 0 'done steps=' 'id:=&(\A:@ => \x:A => x);' --strict-thunks
check 0 'done steps=' 'm:=&(\A:@=>A); alias:=&&m;' --strict-thunks
check 1 'rejected steps=' 'bad:=&@;' --strict-thunks
check 1 'rejected steps=' 'Nat:=@{zero:*;}; bad:=&Nat.zero;' --strict-thunks
check 3 'pending steps=0' 'id:=&(\A:@ => \x:A => x);' --steps 0
check 3 'pending steps=' 'x:=x;' --steps 100
# An exhausted queue is not fuel exhaustion or a completed proof.
case "$output" in *'pending: no runnable synthesis work;'*) ;; *) exit 1 ;; esac
check 3 'pending steps=0' 'x:=x;' --steps 0
test "$output" = 'pending steps=0'
check 1 'rejected steps=' 'x:=missing;'
check 1 '-:1:' 'x:='
check 2 'usage:' '' --steps -1
check 2 'usage:' '' --steps 18446744073709551616
check 2 'usage:' '' --steps 2x
check 2 'usage:' '' --unknown
check 2 'usage:' '' --save
check 2 'usage:' '' --save -
check 2 'usage:' '' --load --strict-thunks
check 2 'usage:' '' --root 1
check 2 'usage:' '' --load --root 0
check 2 'usage:' '' --load --root 1 --root 2
check 2 '-: cannot read or initialize input' 'not an image' --load
check 2 'usage:' '' --nf
check 2 'usage:' '' --nf main --whnf main
check 3 'pending steps=0' 'main:=&(\A:@=>A);' --nf main --steps 0
check 1 '-: definition not found: missing' 'main:=&(\A:@=>A);' --nf missing
check 0 'done steps=' 'main:=&(\A:@=>A);' --nf main
source='main:=\A:@=> (\B:@=>B) A;'
whnf=$(printf '%s' "$source" | "$binary" --whnf main -)
nf=$(printf '%s' "$source" | "$binary" --nf main -)
case "$nf" in *'root := n'*) ;; *) exit 1 ;; esac
case "$whnf" in *'root := n'*) ;; *) exit 1 ;; esac
# WHNF stops at the Lambda; NF also normalizes its body.
test "${whnf#*$'\n'}" != "${nf#*$'\n'}"
steps=${nf%%$'\n'*}
steps=${steps#'done steps='}
check 3 "pending steps=$((steps - 1))" "$source" --nf main --steps "$((steps - 1))"
check 0 "done steps=$steps" "$source" --nf main --steps "$steps"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
# Preserve the all-refuted indexed-Match limitation, including ordinary image
# reload. The post-check must not be used to guess the missing motive.
fixture=$(dirname "$0")/../../tests/fixtures/typing/impossible_index_branch_check.p
code=0
"$binary" --steps 100000 --save "$directory/refuted.a" "$fixture" > "$directory/refuted.out" 2> "$directory/refuted.err" || code=$?
test "$code" = 3
grep -q '^pending: no runnable synthesis work;' "$directory/refuted.err"
code=0
"$binary" --steps 1000000 "$fixture" > "$directory/more.out" 2> "$directory/more.err" || code=$?
test "$code" = 3
cmp "$directory/refuted.out" "$directory/more.out"
cmp "$directory/refuted.err" "$directory/more.err"
code=0
"$binary" --load "$directory/refuted.a" > "$directory/reloaded.out" 2> "$directory/reloaded.err" || code=$?
test "$code" = 3
grep -q '^pending: no runnable synthesis work;' "$directory/reloaded.err"
check 3 'pending steps=0' "$source" --steps 0 --save "$directory/pending.a"
restored=$("$binary" --load --nf main "$directory/pending.a")
test "$restored" = "$nf"
check 0 "done steps=$steps" "$source" --nf main --save "$directory/solved.a"
# Solving may allocate source binders which become retained graph inputs.
# Compare execution, not bytes or scheduling, against the unresolved image.
restored=$("$binary" --load --nf main "$directory/solved.a")
test "${restored#*$'\n'}" = "${nf#*$'\n'}"
code=0
output=$("$binary" --load --steps 0 --save "$directory/copied.a" "$directory/solved.a") || code=$?
test "$code" = 3 && test "$output" = 'pending steps=0'
restored=$("$binary" --load --nf main "$directory/copied.a")
test "${restored#*$'\n'}" = "${nf#*$'\n'}"
check 3 'pending steps=0' 'main:=&(\A:@=>A);' --strict-thunks --steps 0 --save "$directory/strict.a"
"$binary" --load --nf main "$directory/strict.a" > "$directory/result"
check 1 'rejected steps=' 'main:=missing;' --save "$directory/rejected.a"
code=0
output=$("$binary" --load "$directory/rejected.a") || code=$?
test "$code" = 1
case "$output" in 'rejected steps='*) ;; *) exit 1 ;; esac
printf '%s\n' 'cli: bounded source checking, pending, rejection and argument diagnostics passed'
