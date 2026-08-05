#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

# Examples from 10 onward exercise the unfinished general Match-motive and
# constraint-solving work. They are not CBPV surface regressions yet.

./read_file.out src/prototype/computation_block_check.p \
	>"$tmp_dir/computation-block.out"
grep -q '^term main := COMPUTATION_FOLD(' "$tmp_dir/computation-block.out"
grep -q '^term quotedIdentity := THUNK(LAMBDA(' \
	"$tmp_dir/computation-block.out"
grep -q '^term constructorBlock := COMPUTATION_FOLD(' "$tmp_dir/computation-block.out"
grep -q '^term matchBlock := MATCH(.*CASE(zero -> COMPUTATION_FOLD(' \
	"$tmp_dir/computation-block.out"

./read_file.out src/prototype/runtime_strict_value_check.p \
	>"$tmp_dir/runtime-strict-value.out"
grep -q '^term appArgument := COMPUTATION_FOLD(' "$tmp_dir/runtime-strict-value.out"
grep -q '^term constructorArgument := COMPUTATION_FOLD(.*RETURN(APP(CONSTRUCTOR' \
	"$tmp_dir/runtime-strict-value.out"
grep -q '^term matchScrutinee := COMPUTATION_FOLD(.*MATCH(VAR' \
	"$tmp_dir/runtime-strict-value.out"
./read_file.out --write-artifact "$tmp_dir/runtime-strict-value.apo" \
	src/prototype/runtime_strict_value_check.p \
	>"$tmp_dir/runtime-strict-value-write.out"
./read_file.out --read-graph "$tmp_dir/runtime-strict-value.apo" \
	>"$tmp_dir/runtime-strict-value-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=6 verification_obligations=0' \
	"$tmp_dir/runtime-strict-value-read.out"

./read_file.out src/prototype/computation_reference_type_check.p \
	>"$tmp_dir/computation-reference.out"
grep -q '^term run := LAMBDA(.*COMPUTATION_FOLD(FORCE(VAR' \
	"$tmp_dir/computation-reference.out"
grep -q '^term preserve := LAMBDA(.*RETURN(VAR' \
	"$tmp_dir/computation-reference.out"

./read_file.out src/prototype/nested_computation_reference_check.p \
	>"$tmp_dir/nested-computation-reference.out"
grep -q '^term nest := LAMBDA(.*RETURN(THUNK(RETURN(VAR' \
	"$tmp_dir/nested-computation-reference.out"
grep -q 'EFFECT_ROW_FORALL(.*EFFECT_ROW_FORALL' \
	"$tmp_dir/nested-computation-reference.out"

cat >"$tmp_dir/removed-bind-intrinsic.p" <<'EOF'
bad := #.bind (perform (#.print #"x")) (\x : #.Text => x);
EOF
if ./read_file.out "$tmp_dir/removed-bind-intrinsic.p" \
	>"$tmp_dir/removed-bind-intrinsic.out" 2>&1; then
	echo 'removed #.bind surface intrinsic was accepted' >&2
	exit 1
fi

cat >"$tmp_dir/removed-force-intrinsic.p" <<'EOF'
bad := #.force &{ #1; };
EOF
if ./read_file.out "$tmp_dir/removed-force-intrinsic.p" \
	>"$tmp_dir/removed-force-intrinsic.out" 2>&1; then
	echo 'removed #.force surface intrinsic was accepted' >&2
	exit 1
fi

if ./read_file.out src/prototype/computation_block_invalid_argument.p \
	>"$tmp_dir/computation-argument.out" 2>&1; then
	echo 'higher-order application accepted a raw function without quotation' >&2
	exit 1
fi
if ./read_file.out src/prototype/computation_block_invalid_quote.p \
	>"$tmp_dir/value-quote.out" 2>&1; then
	echo 'quotation accepted an ordinary value' >&2
	exit 1
fi

cat >"$tmp_dir/block-final-binding.p" <<'EOF'
main := { x := #1; };
EOF
./read_file.out "$tmp_dir/block-final-binding.p" \
	>"$tmp_dir/block-final-binding.out"

cat >"$tmp_dir/block-old-terminal.p" <<'EOF'
main := { x := #1; x };
EOF
if ./read_file.out "$tmp_dir/block-old-terminal.p" \
	>"$tmp_dir/block-old-terminal.out" 2>&1; then
	echo 'computation block accepted the removed bare terminal form' >&2
	exit 1
fi

cat >"$tmp_dir/block-duplicate.p" <<'EOF'
main := { x := #1; x := #2; x; };
EOF
if ./read_file.out "$tmp_dir/block-duplicate.p" \
	>"$tmp_dir/block-duplicate.out" 2>&1; then
	echo 'computation block accepted a duplicate local name' >&2
	exit 1
fi

cat >"$tmp_dir/quote-effects.p" <<'EOF'
delayed := &(perform (#.print #"x"));
once := {
	x := delayed;
	x;
};
twice := {
	x := delayed;
	y := delayed;
	y;
};
EOF
{
	cat "$tmp_dir/quote-effects.p"
	printf 'once\ntwice\n:q\n'
} | ./a.out >"$tmp_dir/quote-effects.out"
grep -qx 'x' "$tmp_dir/quote-effects.out"
grep -qx 'xx' "$tmp_dir/quote-effects.out"

cp src/prototype/runtime_strict_effects_check.p \
	"$tmp_dir/runtime-strict-effects.p"

./read_file.out "$tmp_dir/runtime-strict-effects.p" \
	>"$tmp_dir/runtime-strict-effects.out"
grep -q '^term leftToRight := COMPUTATION_FOLD(.*COMPUTATION_FOLD(' \
	"$tmp_dir/runtime-strict-effects.out"
grep -q '^term performArgument := COMPUTATION_FOLD(OPERATION_REQUEST' \
	"$tmp_dir/runtime-strict-effects.out"
{
	cat "$tmp_dir/runtime-strict-effects.p"
	printf '%s\n' leftToRight repeat shared constructorOrder \
		performArgument matchScrutineeEffect ':q'
} | ./a.out >"$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 'ab' "$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 'rr' "$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 's' "$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 'cd' "$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 'm' "$tmp_dir/runtime-strict-effects-eval.out"
grep -qx 'ee' "$tmp_dir/runtime-strict-effects-eval.out"

cat >"$tmp_dir/runtime-strict-dependent.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := { x : #.Text := perform (#.print #"x"); Bool.true; };
select := \b : Bool =>
	b @true => Nat.zero @false => Bool.true;
main := select m;
EOF
./read_file.out "$tmp_dir/runtime-strict-dependent.p" \
	>"$tmp_dir/runtime-strict-dependent.out"
grep -q '^term main := COMPUTATION_FOLD(COMPUTATION_FOLD(OPERATION_REQUEST' \
	"$tmp_dir/runtime-strict-dependent.out"
grep -q 'compile-budget .* residual=1 incomplete=0' \
	"$tmp_dir/runtime-strict-dependent.out"
./read_file.out --write-artifact "$tmp_dir/runtime-strict-dependent.apo" \
	"$tmp_dir/runtime-strict-dependent.p" \
	>"$tmp_dir/runtime-strict-dependent-write.out"
grep -q '^verification_obligations 1$' \
	"$tmp_dir/runtime-strict-dependent.apo"

cat >"$tmp_dir/runtime-strict-type-view.p" <<'EOF'
Bool := @{ true : *; false : *; };
Two := @{ one : *; zero : *; };
produceTwo := \x : Two => x;
consumeBool := \x : Bool => x;
main := consumeBool (produceTwo Two.one);
EOF
if ./read_file.out "$tmp_dir/runtime-strict-type-view.p" \
	>"$tmp_dir/runtime-strict-type-view.out" 2>&1; then
	echo 'runtime sequencing erased distinct surface TypeViews' >&2
	exit 1
fi

cat >"$tmp_dir/raw-function.p" <<'EOF'
id :: #.Int -> #.Int;
id := \x : #.Int => x;
main := id #1;
EOF

./read_file.out "$tmp_dir/raw-function.p" >"$tmp_dir/raw-function.out"
grep -q '^term id := LAMBDA(.*RETURN(VAR' "$tmp_dir/raw-function.out"
grep -q '^term main := APP(LAMBDA(.*INT_LITERAL(1))' "$tmp_dir/raw-function.out"
! grep -q '^term id := THUNK(' "$tmp_dir/raw-function.out"
grep -q 'has-type LAMBDA(.*PI(PRIMITIVE(Int).*COMPUTATION_TYPE(EFFECT_LABEL(0), PRIMITIVE(Int))' \
	"$tmp_dir/raw-function.out"

cat >"$tmp_dir/higher-order-function.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
id := \x : Nat => x;
apply := \f : Nat -> Nat => f Nat.zero;
main := apply &id;
EOF

./read_file.out "$tmp_dir/higher-order-function.p" >"$tmp_dir/higher-order-function.out"
grep -q '^term apply := LAMBDA(.*APP(FORCE(VAR' \
	"$tmp_dir/higher-order-function.out"
grep -q '^term main := APP(LAMBDA(.*THUNK(LAMBDA' \
	"$tmp_dir/higher-order-function.out"

cat >"$tmp_dir/ascribed-raw-function.p" <<'EOF'
id := ((\x : #.Int => x) :: #.Int -> #.Int);
main := id #1;
EOF

./read_file.out "$tmp_dir/ascribed-raw-function.p" >"$tmp_dir/ascribed-raw-function.out"
grep -q '^term id := LAMBDA(.*RETURN(VAR' "$tmp_dir/ascribed-raw-function.out"
! grep -q '^term id := THUNK(' "$tmp_dir/ascribed-raw-function.out"
grep -q '^term main := APP(LAMBDA(.*INT_LITERAL(1))' \
	"$tmp_dir/ascribed-raw-function.out"

cat >"$tmp_dir/inline-ascribed-application.p" <<'EOF'
main := ((\x : #.Int => x) :: #.Int -> #.Int) #1;
EOF

./read_file.out "$tmp_dir/inline-ascribed-application.p" \
	>"$tmp_dir/inline-ascribed-application.out"
grep -q '^term main := APP(LAMBDA(.*INT_LITERAL(1))' \
	"$tmp_dir/inline-ascribed-application.out"

cat >"$tmp_dir/effect-function.p" <<'EOF'
main :: #.Nat -> #.Text;
main := \n : #.Nat => perform (#.print #"x");
EOF

./read_file.out "$tmp_dir/effect-function.p" >"$tmp_dir/effect-function.out"
grep -q '^term main := LAMBDA(' "$tmp_dir/effect-function.out"
grep -q 'has-type LAMBDA(.*COMPUTATION_TYPE(EFFECT_LABEL(1), PRIMITIVE(Text))' \
	"$tmp_dir/effect-function.out"

grep -q '^term id := LAMBDA(' "$tmp_dir/higher-order-function.out"
grep -q '^term apply := LAMBDA(.*FORCE(VAR' "$tmp_dir/higher-order-function.out"
grep -q '^term main := APP(LAMBDA(.*THUNK(LAMBDA' \
	"$tmp_dir/higher-order-function.out"
grep -q 'has-type THUNK(LAMBDA(.*\[thunk-intro\]' \
	"$tmp_dir/higher-order-function.out"
./read_file.out --write-artifact "$tmp_dir/higher-order-function.apo" \
	"$tmp_dir/higher-order-function.p" >"$tmp_dir/higher-order-function-write.out"
./read_file.out --read-graph "$tmp_dir/higher-order-function.apo" \
	>"$tmp_dir/higher-order-function-read.out"
grep -q 'interface term main ' "$tmp_dir/higher-order-function-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=0 verification_obligations=0' \
	"$tmp_dir/higher-order-function-read.out"

cat >"$tmp_dir/dependent-fold-residual.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := { x : #.Text := perform (#.print #"x"); Bool.true; };
main := { b : Bool := m; b @true => Nat.zero @false => Bool.true; };
EOF

./read_file.out "$tmp_dir/dependent-fold-residual.p" \
	>"$tmp_dir/dependent-fold-residual.out"
grep -q '^term main := COMPUTATION_FOLD(' "$tmp_dir/dependent-fold-residual.out"
grep -q 'compile-budget .* residual=1 incomplete=0' \
	"$tmp_dir/dependent-fold-residual.out"
./read_file.out --write-artifact "$tmp_dir/dependent-fold-residual.apo" \
	"$tmp_dir/dependent-fold-residual.p" >"$tmp_dir/dependent-fold-residual-write.out"
grep -Eq '^compile_policy 2 [1-9][0-9]* ' "$tmp_dir/dependent-fold-residual.apo"
if ./read_file.out --policy strict "$tmp_dir/dependent-fold-residual.p" \
	>"$tmp_dir/dependent-fold-strict.out" 2>"$tmp_dir/dependent-fold-strict.err"; then
	echo 'strict policy accepted a residual dependent COMPUTATION_FOLD' >&2
	exit 1
fi
grep -q 'failed to compile AST graph' "$tmp_dir/dependent-fold-strict.err"
residual_fold_operation=$(awk '
	/^operation#/ && $2 == "computation-fold" && $NF == "name=main" {
		id = $1
		sub(/^operation#/, "", id)
		print id
		exit
	}
' "$tmp_dir/dependent-fold-residual.out")
[ -n "$residual_fold_operation" ]
./read_file.out --write-artifact "$tmp_dir/dependent-fold-residual.apo" \
	"$tmp_dir/dependent-fold-residual.p" >"$tmp_dir/dependent-fold-residual-write.out"
awk '$1 == "verification" { print $5; exit }' \
	"$tmp_dir/dependent-fold-residual.apo" | grep -qx "$residual_fold_operation"
grep -q '^verification_obligations 1$' "$tmp_dir/dependent-fold-residual.apo"
./read_file.out --read-graph "$tmp_dir/dependent-fold-residual.apo" \
	>"$tmp_dir/dependent-fold-residual-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=2 verification_obligations=1' \
	"$tmp_dir/dependent-fold-residual-read.out"
./read_file.out --check-backend c "$tmp_dir/dependent-fold-residual.apo" \
	>"$tmp_dir/dependent-fold-residual-c.out"
grep -q '^backend c compatible yes$' \
	"$tmp_dir/dependent-fold-residual-c.out"
if ./read_file.out --check-backend verilog \
	"$tmp_dir/dependent-fold-residual.apo" \
	>"$tmp_dir/dependent-fold-residual-verilog.out" \
	2>"$tmp_dir/dependent-fold-residual-verilog.err"; then
	echo 'verilog backend accepted a residual verifier obligation' >&2
	exit 1
fi
grep -q 'backend verilog is incompatible' \
	"$tmp_dir/dependent-fold-residual-verilog.err"
./read_file.out --aggregate-artifact "$tmp_dir/residual-link.apo" \
	"$tmp_dir/dependent-fold-residual.apo" \
	>"$tmp_dir/higher-order-function-residual-link.out" \
	2>"$tmp_dir/higher-order-function-residual-link.err"
./read_file.out --read-graph "$tmp_dir/residual-link.apo" \
	>"$tmp_dir/residual-link-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=2 verification_obligations=1' \
	"$tmp_dir/residual-link-read.out"
cat >"$tmp_dir/link-base.p" <<'EOF'
root := #1;
EOF
./read_file.out --write-artifact "$tmp_dir/link-base.apo" "$tmp_dir/link-base.p" \
	>"$tmp_dir/link-base-write.out"
./read_file.out --aggregate-artifact "$tmp_dir/provider-residual-link.apo" \
	"$tmp_dir/link-base.apo" "$tmp_dir/dependent-fold-residual.apo" \
	>"$tmp_dir/provider-residual-link.out"
./read_file.out --read-graph "$tmp_dir/provider-residual-link.apo" \
	>"$tmp_dir/provider-residual-link-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=2 verification_obligations=1' \
	"$tmp_dir/provider-residual-link-read.out"

# A residual obligation is immutable artifact data. At evaluation, the input
# computation runs once, its returned value discharges only a local frame, and
# the continuation is then evaluated with that value.
printf '%s\n' \
	'Bool := @{ true : *; false : *; };' \
	'Nat := @{ zero : *; succ : * -> *; };' \
	'm := { x : #.Text := perform (#.print #"x"); Bool.true; };' \
	'main := { b : Bool := m; b @true => Nat.zero @false => Bool.true; };' \
	'main' \
	'main' \
	':q' | ./a.out >"$tmp_dir/dependent-fold-residual-eval.out"
[ "$(grep -cx 'x' "$tmp_dir/dependent-fold-residual-eval.out")" -eq 2 ]
[ "$(grep -cx 'verification main := discharged' \
	"$tmp_dir/dependent-fold-residual-eval.out")" -eq 2 ]
grep -q '^verification main := discharged$' \
	"$tmp_dir/dependent-fold-residual-eval.out"


[ "$(grep -Ec '^value main := RETURN\(CONSTRUCTOR\(' \
	"$tmp_dir/dependent-fold-residual-eval.out")" -eq 2 ]

# A residual family can remain symbolic through a nested computation fold. Pointwise
# computation classifiers factor the common effect row while retaining the
# Match result family, and the runtime follows occurrence edges through both
# BINDs before discharging the outer frame.
cat >"$tmp_dir/nested-dependent-fold-residual.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := { x : #.Text := perform (#.print #"x"); Bool.false; };
main := {
	b : Bool := m;
	x : #.Text := perform (#.print #"y");
	b @true => Nat.zero @false => Bool.true;
};
EOF
./read_file.out "$tmp_dir/nested-dependent-fold-residual.p" \
	>"$tmp_dir/nested-dependent-fold-residual.out"
grep -q 'compile-budget .* residual=1 incomplete=0' \
	"$tmp_dir/nested-dependent-fold-residual.out"
{
	cat "$tmp_dir/nested-dependent-fold-residual.p"
	printf '%s\n' main ':q'
} | ./a.out >"$tmp_dir/nested-dependent-fold-residual-eval.out"
grep -q '^xy$' "$tmp_dir/nested-dependent-fold-residual-eval.out"
grep -q '^verification main := discharged$' \
	"$tmp_dir/nested-dependent-fold-residual-eval.out"
grep -q '^value main := RETURN(CONSTRUCTOR(' \
	"$tmp_dir/nested-dependent-fold-residual-eval.out"

cat >"$tmp_dir/effect-forwarding.p" <<'EOF'
forward := \f : #.Text -> #.Text => f #"x";
printer := \text : #.Text => perform (#.print text);
main := forward &printer;
EOF

./read_file.out "$tmp_dir/effect-forwarding.p" >"$tmp_dir/effect-forwarding.out"
grep -q 'has-type LAMBDA(.*EFFECT_ROW_FORALL' "$tmp_dir/effect-forwarding.out"
grep -q 'has-type APP(LAMBDA(.*COMPUTATION_TYPE(EFFECT_LABEL(1), PRIMITIVE(Text))' \
	"$tmp_dir/effect-forwarding.out"
./read_file.out --write-artifact "$tmp_dir/effect-forwarding.apo" \
	"$tmp_dir/effect-forwarding.p" >"$tmp_dir/effect-forwarding-write.out"
./read_file.out --read-graph "$tmp_dir/effect-forwarding.apo" \
	>"$tmp_dir/effect-forwarding-read.out"
grep -q 'interface term main ' "$tmp_dir/effect-forwarding-read.out"

cat >"$tmp_dir/effect-union-forwarding.p" <<'EOF'
both := \f : #.Text -> #.Text => {
	x : #.Text := f #"x";
	perform (#.print x);
};
printer := \text : #.Text => perform (#.print text);
main := both &printer;
EOF

./read_file.out "$tmp_dir/effect-union-forwarding.p" >"$tmp_dir/effect-union-forwarding.out"
grep -q 'has-type LAMBDA(.*EFFECT_ROW_FORALL' "$tmp_dir/effect-union-forwarding.out"
grep -q 'EFFECT_ROW_UNION(EFFECT_ROW_VAR.*EFFECT_LABEL(1)' \
	"$tmp_dir/effect-union-forwarding.out"
grep -q 'has-type APP(LAMBDA(.*COMPUTATION_TYPE(EFFECT_LABEL(1), PRIMITIVE(Text))' \
	"$tmp_dir/effect-union-forwarding.out"
./read_file.out --write-artifact "$tmp_dir/effect-union-forwarding.apo" \
	"$tmp_dir/effect-union-forwarding.p" >"$tmp_dir/effect-union-forwarding-write.out"
./read_file.out --read-graph "$tmp_dir/effect-union-forwarding.apo" \
	>"$tmp_dir/effect-union-forwarding-read.out"
grep -q 'interface term main ' "$tmp_dir/effect-union-forwarding-read.out"

cat >"$tmp_dir/curried-function.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
const :: Nat -> Nat -> Nat;
const := \x : Nat => \y : Nat => x;
main := (const Nat.zero) Nat.zero;
EOF

./read_file.out "$tmp_dir/curried-function.p" >"$tmp_dir/curried-function.out"
grep -q '^term const := LAMBDA(.*LAMBDA(.*RETURN(VAR' \
	"$tmp_dir/curried-function.out"
grep -q '^term main := APP(APP(LAMBDA' \
	"$tmp_dir/curried-function.out"
grep -q 'has-type APP(APP(LAMBDA.*\[app-elim\]' "$tmp_dir/curried-function.out"

cat >"$tmp_dir/raw-match-branch.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
main := \n : Nat => (n @zero => (\m : Nat => m) @succ k => (\m : Nat => m));
EOF

./read_file.out "$tmp_dir/raw-match-branch.p" >"$tmp_dir/raw-match-branch.out"
grep -q '^term main := LAMBDA(.*MATCH(.*CASE(zero -> LAMBDA' \
	"$tmp_dir/raw-match-branch.out"
! grep -q 'RETURN(THUNK(LAMBDA' "$tmp_dir/raw-match-branch.out"
grep -q 'has-type MATCH(.*PI(' "$tmp_dir/raw-match-branch.out"

cat >"$tmp_dir/raw-match-function-call.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
choose := \n : Nat => n @zero => (\m : Nat => m) @succ k => (\m : Nat => m);
main := (choose Nat.zero) Nat.zero;
EOF

./read_file.out "$tmp_dir/raw-match-function-call.p" \
	>"$tmp_dir/raw-match-function-call.out"
grep -q '^term choose := LAMBDA(.*MATCH(.*CASE(zero -> LAMBDA' \
	"$tmp_dir/raw-match-function-call.out"
grep -q '^term main := APP(APP(LAMBDA' \
	"$tmp_dir/raw-match-function-call.out"
grep -q 'has-type APP(APP(LAMBDA.*\[app-elim\]' \
	"$tmp_dir/raw-match-function-call.out"

./read_file.out training/dependent_match.p >"$tmp_dir/dependent-match.out"
grep -q 'has-type MATCH(.*\[solved-match-motive\]' \
	"$tmp_dir/dependent-match.out"

./read_file.out training/recursive_dependent_match.p \
	>"$tmp_dir/recursive-dependent-match.out"
grep -q 'has-type MATCH(.*\[solved-match-motive\]' \
	"$tmp_dir/recursive-dependent-match.out"
./read_file.out --write-artifact "$tmp_dir/recursive-dependent-match.apo" \
	training/recursive_dependent_match.p \
	>"$tmp_dir/recursive-dependent-match-write.out"
grep -Eq '^operation_case_binders [0-9]+ 1 [0-9]+$' \
	"$tmp_dir/recursive-dependent-match.apo"
./read_file.out --read-graph "$tmp_dir/recursive-dependent-match.apo" \
	>"$tmp_dir/recursive-dependent-match-read.out"

cat >"$tmp_dir/bind.p" <<'EOF'
main := { x : #.Int64 := { #1; }; x; };
EOF

./read_file.out "$tmp_dir/bind.p" >"$tmp_dir/bind.out"
grep -q 'term main := COMPUTATION_FOLD(' "$tmp_dir/bind.out"
grep -q '\[computation-fold-elim\]' "$tmp_dir/bind.out"

cat >"$tmp_dir/bind-requires-computation.p" <<'EOF'
main := { x : #.Int64 := { #1; }; \y : #.Int64 => x; };
EOF

if ./read_file.out "$tmp_dir/bind-requires-computation.p" \
	>"$tmp_dir/bind-requires-computation.out" 2>&1; then
	echo "COMPUTATION_FOLD unexpectedly accepted a raw Pi result" >&2
	exit 1
fi

./read_file.out src/prototype/computed_match_execution_check.p \
	>"$tmp_dir/computed-match.out"
grep -q 'term main := COMPUTATION_FOLD(RETURN(CONSTRUCTOR' "$tmp_dir/computed-match.out"
grep -q 'MATCH(VAR' "$tmp_dir/computed-match.out"
grep -q '\[computation-fold-elim\]' "$tmp_dir/computed-match.out"

cat >"$tmp_dir/lambda-bind.p" <<'EOF'
main := \n : #.Nat => { x : #.Nat := { n; }; x; };
EOF

./read_file.out "$tmp_dir/lambda-bind.p" >"$tmp_dir/lambda-bind.out"
grep -q 'term main := LAMBDA(.*COMPUTATION_FOLD(RETURN(VAR' "$tmp_dir/lambda-bind.out"
grep -q '\[computation-fold-elim\]' "$tmp_dir/lambda-bind.out"
grep -q '\[lambda-intro\]' "$tmp_dir/lambda-bind.out"

cat >"$tmp_dir/sigma-dependent-field.p" <<'EOF'
Sigma := \A : @ => \B : A -> @ => @{ mk : (a : A) -> B a -> *; };
Nat := @{ zero : *; succ : * -> *; };
ConstNat := \x : Nat => Nat;
main := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
EOF

./read_file.out "$tmp_dir/sigma-dependent-field.p" >"$tmp_dir/sigma-dependent-field.out"
grep -q '^term ConstNat := LAMBDA(' "$tmp_dir/sigma-dependent-field.out"
grep -q '^term main := APP(APP(CONSTRUCTOR' "$tmp_dir/sigma-dependent-field.out"
grep -q '\[constructor-spine-formation\]' "$tmp_dir/sigma-dependent-field.out"
./read_file.out --write-artifact "$tmp_dir/sigma-dependent-field.apo" \
	"$tmp_dir/sigma-dependent-field.p" >"$tmp_dir/sigma-dependent-field-write.out"
./read_file.out --read-graph "$tmp_dir/sigma-dependent-field.apo" \
	>"$tmp_dir/sigma-dependent-field-read.out"
grep -q 'interface constructor .*mk ordinal=0 fields=2' \
	"$tmp_dir/sigma-dependent-field-read.out"

cat >"$tmp_dir/effectful-type-family.p" <<'EOF'
Sigma := \A : @ => \B : A -> @ => @{ mk : (a : A) -> B a -> *; };
Nat := @{ zero : *; succ : * -> *; };
Bad := \x : Nat => perform (#.print #"x");
main := (Sigma Nat Bad).mk Nat.zero Nat.zero;
EOF

if ./read_file.out "$tmp_dir/effectful-type-family.p" \
	>"$tmp_dir/effectful-type-family.out" 2>"$tmp_dir/effectful-type-family.err"; then
	echo "effectful type family unexpectedly compiled" >&2
	exit 1
fi
grep -q 'failed to compile AST graph' "$tmp_dir/effectful-type-family.err"

cat >"$tmp_dir/perform.p" <<'EOF'
main := perform (#.print #"x");
EOF

./read_file.out "$tmp_dir/perform.p" >"$tmp_dir/perform.out"
grep -q 'term main := OPERATION_REQUEST(' "$tmp_dir/perform.out"
grep -q '\[operation-request-intro\]' "$tmp_dir/perform.out"

cat >"$tmp_dir/pure-operation.p" <<'EOF'
direct := ((#.int64_add #1) #2);
EOF

./read_file.out "$tmp_dir/pure-operation.p" >"$tmp_dir/pure-operation.out"
grep -q 'term direct := APP(APP(PURE_PRIMITIVE(int64_add), INT_LITERAL(1)), INT_LITERAL(2))' \
	"$tmp_dir/pure-operation.out"
printf '%s\n' \
	'direct := ((#.int64_add #1) #2);' \
	'direct' \
	':q' | ./a.out >"$tmp_dir/pure-operation-eval.out"
grep -q 'value direct := RETURN(INT_LITERAL(3))' \
	"$tmp_dir/pure-operation-eval.out"
./read_file.out --write-artifact "$tmp_dir/pure-operation.apo" "$tmp_dir/pure-operation.p" \
	>"$tmp_dir/pure-operation-write.out"
./read_file.out --read-graph "$tmp_dir/pure-operation.apo" \
	>"$tmp_dir/pure-operation-read.out"
grep -q 'interface term direct ' "$tmp_dir/pure-operation-read.out"
grep -Eq '^term_node [0-9]+ [0-9]+ int64_add -$' "$tmp_dir/pure-operation.apo"

cat >"$tmp_dir/invalid-perform-intrinsic.p" <<'EOF'
bad := perform ((#.int64_add #1) #2);
EOF
if ./read_file.out "$tmp_dir/invalid-perform-intrinsic.p" \
	>"$tmp_dir/invalid-perform-intrinsic.out" \
	2>"$tmp_dir/invalid-perform-intrinsic.err"; then
	echo 'perform accepted a pure primitive' >&2
	exit 1
fi

cat >"$tmp_dir/invalid-handle-intrinsic.p" <<'EOF'
bad := (perform (#.print #"x")) @#.return y => y @#.int_neg x k => k x;
EOF
if ./read_file.out "$tmp_dir/invalid-handle-intrinsic.p" \
	>"$tmp_dir/invalid-handle-intrinsic.out" \
	2>"$tmp_dir/invalid-handle-intrinsic.err"; then
	echo 'handler accepted a pure primitive as an effect operation' >&2
	exit 1
fi

cat >"$tmp_dir/handle.p" <<'EOF'
main := (perform (#.print #"x")) @#.return y => y @#.print x k => k x;
EOF

./read_file.out "$tmp_dir/handle.p" >"$tmp_dir/handle.out"
grep -q 'term main := COMPUTATION_FOLD(' "$tmp_dir/handle.out"
grep -q '\[computation-fold-elim\]' "$tmp_dir/handle.out"
./read_file.out --write-artifact "$tmp_dir/handle.apo" "$tmp_dir/handle.p" >"$tmp_dir/handle-write.out"
grep -q '^compile_policy 2 14 ' "$tmp_dir/handle.apo"
grep -Eq '^effect_constraint [0-9]+ 2 2 ' "$tmp_dir/handle.apo"
./read_file.out --read-graph "$tmp_dir/handle.apo" >"$tmp_dir/handle-read.out"
grep -q 'interface term main ' "$tmp_dir/handle-read.out"
./read_file.out --check-backend c "$tmp_dir/handle.apo" \
	>"$tmp_dir/handle-c.out"
grep -q '^backend c compatible yes$' "$tmp_dir/handle-c.out"
if ./read_file.out --check-backend verilog "$tmp_dir/handle.apo" \
	>"$tmp_dir/handle-verilog.out" 2>"$tmp_dir/handle-verilog.err"; then
	echo 'verilog backend accepted handler runtime requirements' >&2
	exit 1
fi

printf '%s\n' \
	'main := (perform (#.print #"x")) @#.return y => y @#.print x k => k x;' \
	'main' \
	':q' | ./a.out >"$tmp_dir/handle-eval.out"
grep -q 'value main := RETURN(TEXT_LITERAL("x"))' "$tmp_dir/handle-eval.out"

cat >"$tmp_dir/return-only-fold.p" <<'EOF'
main := ({ #1; }) @#.return x => x;
EOF
./read_file.out "$tmp_dir/return-only-fold.p" >"$tmp_dir/return-only-fold.out"
grep -q '^term main := COMPUTATION_FOLD(.*LAMBDA(.*RETURN(VAR' \
	"$tmp_dir/return-only-fold.out"

cat >"$tmp_dir/operation-alias-fold.p" <<'EOF'
output := #.print;
main := (perform (#.print #"x"))
	@#.return y => y
	@output x k => k x;
EOF
./read_file.out "$tmp_dir/operation-alias-fold.p" \
	>"$tmp_dir/operation-alias-fold.out"
{
	cat "$tmp_dir/operation-alias-fold.p"
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/operation-alias-fold-eval.out"
grep -q 'value main := RETURN(TEXT_LITERAL("x"))' \
	"$tmp_dir/operation-alias-fold-eval.out"

cat >"$tmp_dir/duplicate-operation-fold.p" <<'EOF'
output := #.print;
bad := (perform (#.print #"x"))
	@#.return y => y
	@#.print x k => k x
	@output x k => k x;
EOF
if ./read_file.out "$tmp_dir/duplicate-operation-fold.p" \
	>"$tmp_dir/duplicate-operation-fold.out" 2>&1; then
	echo 'computation fold accepted duplicate nominal operation clauses' >&2
	exit 1
fi

cat >"$tmp_dir/duplicate-return-fold.p" <<'EOF'
bad := ({ #1; }) @#.return x => x @#.return y => y;
EOF
if ./read_file.out "$tmp_dir/duplicate-return-fold.p" \
	>"$tmp_dir/duplicate-return-fold.out" 2>&1; then
	echo 'computation fold accepted duplicate return clauses' >&2
	exit 1
fi

cat >"$tmp_dir/perform-return-label.p" <<'EOF'
bad := perform (#.return #1);
EOF
if ./read_file.out "$tmp_dir/perform-return-label.p" \
	>"$tmp_dir/perform-return-label.out" 2>&1; then
	echo '#.return was accepted as an effect operation' >&2
	exit 1
fi

cat >"$tmp_dir/handle-bind.p" <<'EOF'
main := ({ y : #.Text := perform (#.print #"x"); y; })
	@#.return y => y
	@#.print x k => k x;
EOF

./read_file.out "$tmp_dir/handle-bind.p" >"$tmp_dir/handle-bind.out"
{
	cat "$tmp_dir/handle-bind.p"
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/handle-bind-eval.out"
grep -q 'value main := RETURN(TEXT_LITERAL("x"))' "$tmp_dir/handle-bind-eval.out"

cat >"$tmp_dir/deep-handle-bind.p" <<'EOF'
main := ({ y : #.Text := perform (#.print #"x"); perform (#.print y); })
	@#.return y => y
	@#.print x k => k x;
EOF

./read_file.out "$tmp_dir/deep-handle-bind.p" >"$tmp_dir/deep-handle-bind.out"
{
	cat "$tmp_dir/deep-handle-bind.p"
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/deep-handle-bind-eval.out"
grep -q 'value main := RETURN(TEXT_LITERAL("x"))' \
	"$tmp_dir/deep-handle-bind-eval.out"

cp src/prototype/dependent_handler_result_check.p \
	"$tmp_dir/dependent-handler-result.p"

./read_file.out "$tmp_dir/dependent-handler-result.p" \
	>"$tmp_dir/dependent-handler-result.out"
grep -q 'compile-budget .* residual=1 incomplete=0' \
	"$tmp_dir/dependent-handler-result.out"
./read_file.out --write-artifact "$tmp_dir/dependent-handler-result.apo" \
	"$tmp_dir/dependent-handler-result.p" \
	>"$tmp_dir/dependent-handler-result-write.out"
grep -q '^verification_obligations 1$' \
	"$tmp_dir/dependent-handler-result.apo"
./read_file.out --read-graph "$tmp_dir/dependent-handler-result.apo" \
	>"$tmp_dir/dependent-handler-result-read.out"
grep -q 'verification_obligations=1' \
	"$tmp_dir/dependent-handler-result-read.out"
./read_file.out --check-backend c "$tmp_dir/dependent-handler-result.apo" \
	>"$tmp_dir/dependent-handler-result-c.out"
grep -q '^backend c compatible yes$' \
	"$tmp_dir/dependent-handler-result-c.out"
if ./read_file.out --check-backend verilog \
	"$tmp_dir/dependent-handler-result.apo" \
	>"$tmp_dir/dependent-handler-result-verilog.out" \
	2>"$tmp_dir/dependent-handler-result-verilog.err"; then
	echo 'verilog backend accepted a dependent handler verifier' >&2
	exit 1
fi
./read_file.out --aggregate-artifact \
	"$tmp_dir/dependent-handler-result-linked.apo" \
	"$tmp_dir/dependent-handler-result.apo" \
	>"$tmp_dir/dependent-handler-result-link.out"
./read_file.out --read-graph "$tmp_dir/dependent-handler-result-linked.apo" \
	>"$tmp_dir/dependent-handler-result-linked-read.out"
grep -q 'verification_obligations=1' \
	"$tmp_dir/dependent-handler-result-linked-read.out"
if ./read_file.out --policy strict "$tmp_dir/dependent-handler-result.p" \
	>"$tmp_dir/dependent-handler-result-strict.out" \
	2>"$tmp_dir/dependent-handler-result-strict.err"; then
	echo 'strict policy accepted a residual dependent handler result' >&2
	exit 1
fi
{
	cat "$tmp_dir/dependent-handler-result.p"
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/dependent-handler-result-eval.out"
grep -q '^verification main := discharged$' \
	"$tmp_dir/dependent-handler-result-eval.out"

cat >"$tmp_dir/negative-handle-raw-function.p" <<'EOF'
bad := (\x : #.Int => x) @#.return y => y @#.print x k => k x;
EOF

if ./read_file.out "$tmp_dir/negative-handle-raw-function.p" \
	>"$tmp_dir/negative-handle-raw-function.out" \
	2>"$tmp_dir/negative-handle-raw-function.err"; then
	echo 'handler accepted a raw Pi computation' >&2
	exit 1
fi
grep -q 'failed to compile AST graph' \
	"$tmp_dir/negative-handle-raw-function.err"

cat >"$tmp_dir/lambda-handle.p" <<'EOF'
main := \n : #.Nat =>
	(perform (#.print #"x")) @#.return y => y @#.print x k => k x;
EOF

./read_file.out "$tmp_dir/lambda-handle.p" >"$tmp_dir/lambda-handle.out"
grep -q 'term main := LAMBDA(.*COMPUTATION_FOLD(' "$tmp_dir/lambda-handle.out"
grep -q '\[computation-fold-elim\]' "$tmp_dir/lambda-handle.out"
grep -q '\[lambda-intro\]' "$tmp_dir/lambda-handle.out"
./read_file.out --write-artifact "$tmp_dir/lambda-handle.apo" "$tmp_dir/lambda-handle.p" \
	>"$tmp_dir/lambda-handle-write.out"
./read_file.out --read-graph "$tmp_dir/lambda-handle.apo" \
	>"$tmp_dir/lambda-handle-read.out"
grep -q 'interface term main ' "$tmp_dir/lambda-handle-read.out"

# A higher-order operation uses the ordinary request argument to carry a
# thunked computation. The inner operation remains latent in the request.
./read_file.out src/prototype/higher_order_operation_check.p \
	>"$tmp_dir/higher-order-operation.out"
grep -q 'EFFECT_OPERATION(scope_text)' "$tmp_dir/higher-order-operation.out"
grep -q 'THUNK(OPERATION_REQUEST(EFFECT_OPERATION(print)' \
	"$tmp_dir/higher-order-operation.out"
grep -q '\[operation-request-intro\]' "$tmp_dir/higher-order-operation.out"
./read_file.out --write-artifact "$tmp_dir/higher-order-operation.apo" \
	src/prototype/higher_order_operation_check.p \
	>"$tmp_dir/higher-order-operation-write.out"
./read_file.out --read-graph "$tmp_dir/higher-order-operation.apo" \
	>"$tmp_dir/higher-order-operation-read.out"
grep -q 'interface term main ' "$tmp_dir/higher-order-operation-read.out"

# A handler receives the quoted inner computation as an opaque value. This
# clause discards it, so the nested print must not execute.
./read_file.out src/prototype/higher_order_operation_handler_check.p \
	>"$tmp_dir/higher-order-operation-handler.out"
grep -q '^term main := COMPUTATION_FOLD(' \
	"$tmp_dir/higher-order-operation-handler.out"
grep -q 'OP_CLAUSE(EFFECT_OPERATION(scope_text)' \
	"$tmp_dir/higher-order-operation-handler.out"
grep -q '\[computation-fold-elim\]' \
	"$tmp_dir/higher-order-operation-handler.out"
grep -q '\[computation-fold-elim\] proof#[0-9][0-9]* premises=6' \
	"$tmp_dir/higher-order-operation-handler.out"
{
	cat src/prototype/higher_order_operation_handler_check.p
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/higher-order-operation-handler-eval.out"
grep -q '^value main := RETURN(TEXT_LITERAL("handled"))$' \
	"$tmp_dir/higher-order-operation-handler-eval.out"
if grep -qx 'inner' "$tmp_dir/higher-order-operation-handler-eval.out"; then
	echo 'discarded higher-order operation argument was executed' >&2
	exit 1
fi
./read_file.out --write-artifact "$tmp_dir/higher-order-operation-handler.apo" \
	src/prototype/higher_order_operation_handler_check.p \
	>"$tmp_dir/higher-order-operation-handler-write.out"
./read_file.out --read-graph "$tmp_dir/higher-order-operation-handler.apo" \
	>"$tmp_dir/higher-order-operation-handler-read.out"
grep -q 'interface term main ' \
	"$tmp_dir/higher-order-operation-handler-read.out"

# A computation-block binding is an execution demand. Once the handler
# argument receives its declaration-owned Thunk(Comp(E, Text)) classifier,
# lowering inserts FORCE and sequences the result. The current effect-row
# solver does not yet connect the operation's forall E with the concrete thunk
# row, so partial policy preserves residual constraints and strict rejects it.
./read_file.out src/prototype/higher_order_operation_force_once_check.p \
	>"$tmp_dir/higher-order-operation-force-once.out"
grep -q 'FORCE(VAR' "$tmp_dir/higher-order-operation-force-once.out"
grep -Eq 'compile-budget .* residual=[1-9][0-9]* incomplete=0' \
	"$tmp_dir/higher-order-operation-force-once.out"
if ./read_file.out --policy strict \
	src/prototype/higher_order_operation_force_once_check.p \
	>"$tmp_dir/higher-order-operation-force-once-strict.out" \
	2>"$tmp_dir/higher-order-operation-force-once-strict.err"; then
	echo 'strict policy accepted unresolved higher-order force effects' >&2
	exit 1
fi
{
	cat src/prototype/higher_order_operation_force_once_check.p
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/higher-order-operation-force-once-eval.out"
test "$(grep -c '^inner$' "$tmp_dir/higher-order-operation-force-once-eval.out")" -eq 1
grep -q '^value main := RETURN(TEXT_LITERAL("inner"))$' \
	"$tmp_dir/higher-order-operation-force-once-eval.out"
./read_file.out --write-artifact \
	"$tmp_dir/higher-order-operation-force-once.apo" \
	src/prototype/higher_order_operation_force_once_check.p \
	>"$tmp_dir/higher-order-operation-force-once-write.out"
grep -Eq '^compile_policy .* [1-9][0-9]* 0$' \
	"$tmp_dir/higher-order-operation-force-once.apo"
./read_file.out --read-graph \
	"$tmp_dir/higher-order-operation-force-once.apo" \
	>"$tmp_dir/higher-order-operation-force-once-read.out"
grep -q 'interface term main .* classifier#4294967295 ' \
	"$tmp_dir/higher-order-operation-force-once-read.out"

./read_file.out src/prototype/higher_order_operation_force_twice_check.p \
	>"$tmp_dir/higher-order-operation-force-twice.out"
grep -q 'FORCE(VAR' "$tmp_dir/higher-order-operation-force-twice.out"
{
	cat src/prototype/higher_order_operation_force_twice_check.p
	printf 'main\n:q\n'
} | ./a.out >"$tmp_dir/higher-order-operation-force-twice-eval.out"
grep -q '^innerinner$' "$tmp_dir/higher-order-operation-force-twice-eval.out"
grep -q '^value main := RETURN(TEXT_LITERAL("inner"))$' \
	"$tmp_dir/higher-order-operation-force-twice-eval.out"
