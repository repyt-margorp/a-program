#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

run_case() {
	name=$1
	source=$2
	printf '%s\n' "$source" >"$tmp_dir/$name.p"
	./read_file.out "$tmp_dir/$name.p" >"$tmp_dir/$name.out"
}

reject_case() {
	name=$1
	source=$2
	printf '%s\n' "$source" >"$tmp_dir/$name.p"
	if ./read_file.out "$tmp_dir/$name.p" >"$tmp_dir/$name.out" 2>&1; then
		echo "computation block unexpectedly accepted $name" >&2
		exit 1
	fi
}

run_case final-value 'main := { #1; };'
grep -q '^term main := RETURN(INT_LITERAL(1))$' "$tmp_dir/final-value.out"

run_case final-computation 'main := { (#.print #"x"); };'
grep -q '^term main := OPERATION_REQUEST(' "$tmp_dir/final-computation.out"

run_case final-binding 'main := { x := #1; };'
grep -q '^term main := RETURN(INT_LITERAL(1))$' "$tmp_dir/final-binding.out"

run_case final-binding-expression 'main := { x := #1; x; };'
grep -q '^term main := RETURN(INT_LITERAL(1))$' \
	"$tmp_dir/final-binding-expression.out"

run_case discard-computation \
	'main := { (#.int_add #1 #2); #3; };'
grep -q '^term main := COMPUTATION_FOLD(.*RETURN(INT_LITERAL(3)))' \
	"$tmp_dir/discard-computation.out"

run_case selected-cutoff \
	'main := { selected := #1; dead := missing; (#.print #"dead"); }.selected;'
grep -q '^term main := RETURN(INT_LITERAL(1))$' "$tmp_dir/selected-cutoff.out"
grep -q 'resolve_errors=0' "$tmp_dir/selected-cutoff.out"
! grep -q 'OPERATION_REQUEST' "$tmp_dir/selected-cutoff.out"
! grep -q 'missing' "$tmp_dir/selected-cutoff.out"
./read_file.out --write-artifact "$tmp_dir/selected-cutoff.apo" \
	"$tmp_dir/selected-cutoff.p" >"$tmp_dir/selected-cutoff-write.out"
./read_file.out --read-graph "$tmp_dir/selected-cutoff.apo" \
	>"$tmp_dir/selected-cutoff-read.out"
grep -q '^A_PROGRAM_ARTIFACT 76 [0-9a-f]\{64\}$' "$tmp_dir/selected-cutoff.apo"
! grep -q 'OPERATION_REQUEST' "$tmp_dir/selected-cutoff.apo"
! grep -q 'missing' "$tmp_dir/selected-cutoff.apo"

reject_case empty-block 'main := {};'
grep -q 'computation block requires at least one item' "$tmp_dir/empty-block.out"

reject_case unknown-selector 'main := { x := #1; }.missing;'
grep -q 'result must name a direct binding' "$tmp_dir/unknown-selector.out"

reject_case duplicate-binding 'main := { x := #1; x := #2; x; };'
grep -q 'duplicate computation block binding' "$tmp_dir/duplicate-binding.out"

reject_case intermediate-value 'main := { #1; #2; };'
reject_case intermediate-function \
	'main := { (\x : #.Int => x); #2; };'

run_case lambda-exit 'main := \x : #.Int => { !x; #2; };'
grep -q '^term main := LAMBDA(.*RETURN(VAR' "$tmp_dir/lambda-exit.out"
! grep -q 'INT_LITERAL(2)' "$tmp_dir/lambda-exit.out"

run_case nested-block-exit \
	'main := \x : #.Int => { { !x; }; #2; };'
grep -q '^term main := LAMBDA(.*RETURN(VAR' "$tmp_dir/nested-block-exit.out"
! grep -q 'INT_LITERAL(2)' "$tmp_dir/nested-block-exit.out"

cat >"$tmp_dir/match-exit.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
choose := \b : Bool => {
	b @true => { !Nat.zero; }
	  @false => Nat.zero;
	Nat.succ Nat.zero;
};
trueResult := choose Bool.true;
falseResult := choose Bool.false;
zeroExpected := { Nat.zero; };
succExpected := { Nat.succ Nat.zero; };
EOF
./read_file.out "$tmp_dir/match-exit.p" >"$tmp_dir/match-exit.out"
grep -q 'CASE(true -> RETURN(CONSTRUCTOR' "$tmp_dir/match-exit.out"
grep -q 'CASE(false -> COMPUTATION_FOLD(' "$tmp_dir/match-exit.out"
./read_file.out --check-source-exports-normalization-equal \
	trueResult zeroExpected --reduction-mode default "$tmp_dir/match-exit.p" \
	>"$tmp_dir/match-exit-true.out"
grep -q 'mode=default yes$' "$tmp_dir/match-exit-true.out"
./read_file.out --check-source-exports-normalization-equal \
	falseResult succExpected --reduction-mode default "$tmp_dir/match-exit.p" \
	>"$tmp_dir/match-exit-false.out"
grep -q 'mode=default yes$' "$tmp_dir/match-exit-false.out"
./read_file.out --write-artifact "$tmp_dir/match-exit.apo" \
	"$tmp_dir/match-exit.p" >"$tmp_dir/match-exit-write.out"
./read_file.out --read-graph "$tmp_dir/match-exit.apo" \
	>"$tmp_dir/match-exit-read.out"
grep -q '^A_PROGRAM_ARTIFACT 76 [0-9a-f]\{64\}$' "$tmp_dir/match-exit.apo"
./read_file.out --check-exports-normalization-equal "$tmp_dir/match-exit.apo" \
	trueResult zeroExpected --reduction-mode default \
	>"$tmp_dir/match-exit-artifact-true.out"
grep -q 'mode=default yes$' "$tmp_dir/match-exit-artifact-true.out"
./read_file.out --check-exports-normalization-equal "$tmp_dir/match-exit.apo" \
	falseResult succExpected --reduction-mode default \
	>"$tmp_dir/match-exit-artifact-false.out"
grep -q 'mode=default yes$' "$tmp_dir/match-exit-artifact-false.out"

cat >"$tmp_dir/binding-rhs-exit.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
choose := \b : Bool => {
	x := b @true => { !Nat.zero; } @false => Nat.zero;
	Nat.succ x;
};
EOF
./read_file.out "$tmp_dir/binding-rhs-exit.p" \
	>"$tmp_dir/binding-rhs-exit.out"
grep -q 'CASE(true -> RETURN(CONSTRUCTOR' "$tmp_dir/binding-rhs-exit.out"
grep -q 'CASE(false -> COMPUTATION_FOLD(' "$tmp_dir/binding-rhs-exit.out"

run_case nested-lambda-exit \
	'outer := \x : #.Int => { inner := \y : #.Int => { !y; #3; }; inner; };'
grep -q '^term outer := LAMBDA(.*LAMBDA(.*RETURN(VAR' \
	"$tmp_dir/nested-lambda-exit.out"
! grep -q 'INT_LITERAL(3)' "$tmp_dir/nested-lambda-exit.out"

reject_case lambda-exit-outside-lambda 'main := { !#1; };'
reject_case lambda-exit-under-quote \
	'main := \x : #.Int => &{ !x; };'
reject_case lambda-exit-under-handler \
	'main := \x : #.Text => ({ !x; }) @#.return z => z @#.print y k => k y;'
reject_case lambda-exit-as-app-argument \
	'id := \y : #.Int => y; main := \x : #.Int => id { !x; };'

echo 'computation block sequence tests passed'
