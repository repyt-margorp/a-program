#!/usr/bin/env bash
set -eu
binary=$1
check() {
	local expected=$1 status=$2 source=$3 code=0
	shift 3
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
check 3 'pending steps=' 'x:=x;'
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
# Source and image Solve use the same literal introduction. Text contents do
# not resolve as names; integer width is synthesized, never supplied by ::.
for literal in '#42' '#-2147483648' '#"hello"' '#""'; do
	input="hello:=@{unit:*;}; main:=$literal;"
	check 0 'done steps=' "$input"
	for budget in 0 100000; do
		if [ "$budget" = 0 ]; then
			check 3 'pending steps=0' "$input" --steps "$budget" --save "$directory/literal.a"
		else
			check 0 'done steps=' "$input" --steps "$budget" --save "$directory/literal.a"
		fi
		code=0
		output=$("$binary" --load "$directory/literal.a") || code=$?
		test "$code" = 0
		case "$output" in 'done steps='*) ;; *) exit 1 ;; esac
	done
done
check 0 'done steps=' 'a:=#2147483647; a::#Int; a::#Int32;'
check 1 'rejected steps=' 'a:=#42; a::#Int64;'
check 1 'rejected steps=' 'a:=#2147483648; a::#Int64;'
check 1 'rejected steps=' 'a:=#-2147483649;'
check 1 'rejected steps=' 'a:=#-9223372036854775808;'
check 1 'rejected steps=' 'a:=#"hello"; a::#Int;'
check 1 'rejected steps=' 'a:=#42; a::#Text;'
check 1 'rejected steps=' 'a:=#int64_add #1 #2;'
check 1 'rejected steps=' 'a:=#int_neg #"hello";'
check 1 'rejected steps=' 'a:=#int_add #1 #2 #3;'
check 1 'rejected steps=' 'f:=#int_add; f::#Int64->#Int64->#Int64;'
check 0 'done steps=' 'add:=#int_add; main:=add #20 #22;'
check 0 'done steps=' 'add:=#.int_add; main:=add #20 #22;' --legacy-intrinsic-dot
check 0 'done steps=' 'main:=#print #"not-a-compiler-output";'
test "$output" = "${output%%$'\n'*}"
check 0 'done steps=' 'main:=#.print #"not-a-compiler-output";' --legacy-intrinsic-dot
test "$output" = "${output%%$'\n'*}"
check 1 'rejected steps=' 'main:=#print #42;'
check 1 'rejected steps=' 'main:=(#print #"x") @#print req k=>k #42 @#return x=>x;'
check 1 'rejected steps=' 'main:=(#print #"x") @#print req k=>{k #42; k req;} @#return x=>x;'
check 1 'rejected steps=' 'main:=(#print #"x") @#print req k=>#missing req @#return x=>x;'
check 1 'rejected steps=' 'main:=(#print #"x") @#print req k=>missing req @#return x=>x;'
check 0 'done steps=' 'Bool:=@{true:*;false:*;}; main:={x:#Text:=#print #"m";Bool.false;};'
check 1 'rejected steps=' 'main:={x:#Int:=#print #"m";#42;};'
check 1 'rejected steps=' 'main:={x:#Int:=#print #"m";};'
check 1 'rejected steps=' 'main:={x:#Int:=#print #"m";missing;}.x;'
check 1 'rejected steps=' 'main:={x:#Int64:=#2147483648;};'
check 1 'rejected steps=' 'main:={x:#Text:=#print #"m";x;}; main::#Text;'
check 1 'rejected steps=' 'main:={x:=#print #"m";x;}; main::#Text;'
check 1 'rejected steps=' 'main:=(#print #"m") @#print req k=>{x:#Int:=k req;req;} @#return x=>x;'
for mode in --whnf --nf; do
	check 0 'done steps=' 'main:=#print #"not-a-compiler-output";' "$mode" main
	case "$output" in *'not-a-compiler-output'*) exit 1 ;; esac
done
for budget in 0 100000; do
	if [ "$budget" = 0 ]; then expected=3; status='pending steps='; else expected=1; status='rejected steps='; fi
	for invalid_source in 'main:=#int64_add #1 #2;' \
		'main:=(#print #"m") @#print req k=>{x:#Int:=k req;req;} @#return x=>x;'; do
		check "$expected" "$status" "$invalid_source" --steps "$budget" --save "$directory/wrong.a"
		code=0
		"$binary" --load "$directory/wrong.a" > "$directory/wrong.out" || code=$?
		test "$code" = 1
		grep -q '^rejected steps=' "$directory/wrong.out"
	done
done
check 1 '-:1:' 'a:=#.Int;'
case "$output" in *'--legacy-intrinsic-dot'*) ;; *) exit 1 ;; esac
check 1 '-:1:' 'a:=#print; b:=#.print;'
check 1 '-:1:' 'a:={#42;} @#.return x=>x;'
check 0 'done steps=' 'a:=#42; a::#.Int; a::#Int32;' --legacy-intrinsic-dot
check 0 'done steps=' 'a:={#42;} @#.return x=>x;' --legacy-intrinsic-dot
check 0 'done steps=' 'a:={#42;} @#return x=>x;'
check 0 'done steps=' 'T:=@{c:*;}; value:=T.c;'
printf '%s' 'import T; value:=#42; value::T;' > "$directory/client.p"
printf '%s' 'T:=#.Int;' > "$directory/provider.p"
code=0
"$binary" --imports "$directory/provider.p" "$directory/client.p" > "$directory/import.out" 2>&1 || code=$?
test "$code" = 1
grep -q -- '--legacy-intrinsic-dot' "$directory/import.out"
"$binary" --legacy-intrinsic-dot --imports "$directory/provider.p" "$directory/client.p" > "$directory/import.out"
check 3 'pending steps=0' 'value:=#42; value::#.Int;' --legacy-intrinsic-dot --steps 0 --save "$directory/spelling.a"
# Loading an already parsed image does not parse dotted source again.
"$binary" --load "$directory/spelling.a" > "$directory/spelling.out"
grep -q '^done steps=' "$directory/spelling.out"
printf 'a:=#.Int;\n:quit\n' | "$binary" --repl "$directory/client.p" > "$directory/repl.out" 2>&1
grep -q -- '--legacy-intrinsic-dot' "$directory/repl.out"
printf 'a:=#.Int;\n:quit\n' | "$binary" --legacy-intrinsic-dot --repl "$directory/provider.p" > "$directory/repl-legacy.out" 2>&1
if grep -q -- 'requires --legacy-intrinsic-dot' "$directory/repl-legacy.out"; then exit 1; fi
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
# Restored namespace derivations are ordinary Solve inputs. Their checking
# work need not equal source setup, but the evaluated result must agree.
test "${restored#*$'\n'}" = "${nf#*$'\n'}"
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
