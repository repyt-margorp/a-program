#!/bin/sh
set -eu

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

# Examples from 10 onward exercise the unfinished general Match-motive and
# constraint-solving work. They are not CBPV surface regressions yet.

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

# The default surface inserts the CBPV RETURN required by a value lambda body.
# Strict mode disables all implicit value/computation boundaries; the raw CBPV
# spelling remains valid when the source provides RETURN itself.
! ./read_file.out --no-automatic-cbpv-coercions "$tmp_dir/raw-function.p" \
	>"$tmp_dir/raw-function-strict-implicit.out" 2>&1

cat >"$tmp_dir/raw-function-strict.p" <<'EOF'
id := \x : #.Int => return x;
main := id #1;
EOF

./read_file.out --no-automatic-cbpv-coercions "$tmp_dir/raw-function-strict.p" \
	>"$tmp_dir/raw-function-strict.out"
grep -q '^term id := LAMBDA(.*RETURN(VAR' "$tmp_dir/raw-function-strict.out"
grep -q '^term main := APP(LAMBDA(.*INT_LITERAL(1))' "$tmp_dir/raw-function-strict.out"

cat >"$tmp_dir/higher-order-function-strict.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
id := \x : Nat => return x;
apply := \f : Nat -> Nat => (force f) Nat.zero;
main := apply (thunk id);
EOF

./read_file.out --no-automatic-cbpv-coercions \
	"$tmp_dir/higher-order-function-strict.p" >"$tmp_dir/higher-order-function-strict.out"
grep -q '^term apply := LAMBDA(.*APP(FORCE(VAR' \
	"$tmp_dir/higher-order-function-strict.out"
grep -q '^term main := APP(LAMBDA(.*THUNK(LAMBDA' \
	"$tmp_dir/higher-order-function-strict.out"

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

cat >"$tmp_dir/higher-order-function.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
id := \x : Nat => x;
apply := \f : Nat -> Nat => f Nat.zero;
main := apply id;
EOF

./read_file.out "$tmp_dir/higher-order-function.p" >"$tmp_dir/higher-order-function.out"
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

cat >"$tmp_dir/dependent-bind-residual.p" <<'EOF'
Bool := @{ true : *; false : *; };
Nat := @{ zero : *; succ : * -> *; };
m := #.bind (perform (#.print #"x")) (\x : #.Text => return Bool.true);
main := #.bind m (\b : Bool => b @true => Nat.zero @false => Bool.true);
EOF

./read_file.out "$tmp_dir/dependent-bind-residual.p" \
	>"$tmp_dir/dependent-bind-residual.out"
grep -q '^term main := BIND(' "$tmp_dir/dependent-bind-residual.out"
residual_bind_operation=$(awk '
	/^operation#/ && $2 == "bind" && $NF == "name=main" {
		id = $1
		sub(/^operation#/, "", id)
		print id
		exit
	}
' "$tmp_dir/dependent-bind-residual.out")
[ -n "$residual_bind_operation" ]
./read_file.out --write-artifact "$tmp_dir/dependent-bind-residual.apo" \
	"$tmp_dir/dependent-bind-residual.p" >"$tmp_dir/dependent-bind-residual-write.out"
awk '$1 == "verification" { print $5; exit }' \
	"$tmp_dir/dependent-bind-residual.apo" | grep -qx "$residual_bind_operation"
grep -q '^verification_obligations 1$' "$tmp_dir/dependent-bind-residual.apo"
./read_file.out --read-graph "$tmp_dir/dependent-bind-residual.apo" \
	>"$tmp_dir/dependent-bind-residual-read.out"
grep -Eq 'operation_occurrences=[1-9][0-9]* operation_cases=2 verification_obligations=1' \
	"$tmp_dir/dependent-bind-residual-read.out"
./read_file.out --aggregate-artifact "$tmp_dir/residual-link.apo" \
	"$tmp_dir/dependent-bind-residual.apo" \
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
	"$tmp_dir/link-base.apo" "$tmp_dir/dependent-bind-residual.apo" \
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
	'm := #.bind (perform (#.print #"x")) (\x : #.Text => return Bool.true);' \
	'main := #.bind m (\b : Bool => b @true => Nat.zero @false => Bool.true);' \
	'main' \
	'main' \
	':q' | ./a.out >"$tmp_dir/dependent-bind-residual-eval.out"
[ "$(grep -cx 'x' "$tmp_dir/dependent-bind-residual-eval.out")" -eq 2 ]
[ "$(grep -cx 'verification main := discharged' \
	"$tmp_dir/dependent-bind-residual-eval.out")" -eq 2 ]
grep -q '^verification main := discharged$' \
	"$tmp_dir/dependent-bind-residual-eval.out"


[ "$(grep -Ec '^value main := RETURN\(CONSTRUCTOR\(' \
	"$tmp_dir/dependent-bind-residual-eval.out")" -eq 2 ]

cat >"$tmp_dir/effect-forwarding.p" <<'EOF'
forward := \f : #.Text -> #.Text => f #"x";
printer := \text : #.Text => perform (#.print text);
main := forward printer;
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
both := \f : #.Text -> #.Text => #.bind (f #"x") (\x : #.Text => perform (#.print x));
printer := \text : #.Text => perform (#.print text);
main := both printer;
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

cat >"$tmp_dir/bind.p" <<'EOF'
main := #.bind (return #1) (\x : #.Int64 => return x);
EOF

./read_file.out "$tmp_dir/bind.p" >"$tmp_dir/bind.out"
grep -q 'term main := BIND(' "$tmp_dir/bind.out"
grep -q '\[bind-intro\]' "$tmp_dir/bind.out"

cat >"$tmp_dir/bind-requires-computation.p" <<'EOF'
main := #.bind (return #1) (\x : #.Int64 => \y : #.Int64 => x);
EOF

if ./read_file.out "$tmp_dir/bind-requires-computation.p" \
	>"$tmp_dir/bind-requires-computation.out" 2>&1; then
	echo "BIND unexpectedly accepted a raw Pi result" >&2
	exit 1
fi

cat >"$tmp_dir/computed-match.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
main := (force (thunk (return Nat.zero))) @zero => Nat.zero @succ k => Nat.succ k;
EOF

./read_file.out "$tmp_dir/computed-match.p" >"$tmp_dir/computed-match.out"
grep -q 'term main := BIND(FORCE(THUNK(RETURN(CONSTRUCTOR' "$tmp_dir/computed-match.out"
grep -q 'MATCH(VAR' "$tmp_dir/computed-match.out"
grep -q '\[bind-intro\]' "$tmp_dir/computed-match.out"

cat >"$tmp_dir/lambda-bind.p" <<'EOF'
main := \n : #.Nat => #.bind (return n) (\x : #.Nat => return x);
EOF

./read_file.out "$tmp_dir/lambda-bind.p" >"$tmp_dir/lambda-bind.out"
grep -q 'term main := LAMBDA(.*BIND(RETURN(VAR' "$tmp_dir/lambda-bind.out"
grep -q '\[bind-intro\]' "$tmp_dir/lambda-bind.out"
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
grep -q '\[app-elim\]' "$tmp_dir/sigma-dependent-field.out"
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
main := perform ((#.int64_add #1) #2);
EOF

./read_file.out "$tmp_dir/pure-operation.p" >"$tmp_dir/pure-operation.out"
grep -q 'term direct := APP(APP(OPERATION(int64_add), INT_LITERAL(1)), INT_LITERAL(2))' \
	"$tmp_dir/pure-operation.out"
grep -q 'term main := OPERATION_REQUEST(APP(OPERATION(int64_add), INT_LITERAL(1)), INT_LITERAL(2),' \
	"$tmp_dir/pure-operation.out"
grep -q 'OPERATION_REQUEST.*COMPUTATION_TYPE(EFFECT_LABEL(0), PRIMITIVE(Int64)) \[operation-request-intro\]' \
	"$tmp_dir/pure-operation.out"
printf '%s\n' \
	'direct := ((#.int64_add #1) #2);' \
	'direct' \
	':q' | ./a.out >"$tmp_dir/pure-operation-eval.out"
grep -q 'value direct := RETURN(INT_LITERAL(3))' \
	"$tmp_dir/pure-operation-eval.out"
printf '%s\n' \
	'main := perform ((#.int64_add #1) #2);' \
	'main' \
	':q' | ./a.out >"$tmp_dir/pure-operation-perform.out"
grep -q 'value main := RETURN(INT_LITERAL(3))' "$tmp_dir/pure-operation-perform.out"
printf '%s\n' \
	'main := #.bind (perform ((#.int64_add #1) #2)) (\x : #.Int64 => return x);' \
	'main' \
	':q' | ./a.out >"$tmp_dir/pure-operation-bind.out"
grep -q 'term main := BIND(OPERATION_REQUEST(' "$tmp_dir/pure-operation-bind.out"
grep -q 'value main := RETURN(INT_LITERAL(3))' "$tmp_dir/pure-operation-bind.out"
./read_file.out --write-artifact "$tmp_dir/pure-operation.apo" "$tmp_dir/pure-operation.p" \
	>"$tmp_dir/pure-operation-write.out"
./read_file.out --read-graph "$tmp_dir/pure-operation.apo" \
	>"$tmp_dir/pure-operation-read.out"
grep -q 'interface term main ' "$tmp_dir/pure-operation-read.out"

cat >"$tmp_dir/handle.p" <<'EOF'
main := handle (perform (#.print #"x")) with (#.print) x k => k x; return y => return y;
EOF

./read_file.out "$tmp_dir/handle.p" >"$tmp_dir/handle.out"
grep -q 'term main := HANDLE(HANDLER(' "$tmp_dir/handle.out"
grep -q '\[handler-intro\]' "$tmp_dir/handle.out"
grep -q '\[handle-elim\]' "$tmp_dir/handle.out"
./read_file.out --write-artifact "$tmp_dir/handle.apo" "$tmp_dir/handle.p" >"$tmp_dir/handle-write.out"
./read_file.out --read-graph "$tmp_dir/handle.apo" >"$tmp_dir/handle-read.out"
grep -q 'interface term main ' "$tmp_dir/handle-read.out"

printf '%s\n' \
	'main := handle (perform (#.print #"x")) with (#.print) x k => k x; return y => return y;' \
	'main' \
	':q' | ./a.out >"$tmp_dir/handle-eval.out"
grep -q 'value main := RETURN(TEXT_LITERAL("x"))' "$tmp_dir/handle-eval.out"

cat >"$tmp_dir/negative-handle-raw-function.p" <<'EOF'
bad := handle (\x : #.Int => x) with (#.print) x k => k x; return y => return y;
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
main := \n : #.Nat => handle (perform (#.print #"x")) with (#.print) x k => k x; return y => return y;
EOF

./read_file.out "$tmp_dir/lambda-handle.p" >"$tmp_dir/lambda-handle.out"
grep -q 'term main := LAMBDA(.*HANDLE(HANDLER(' "$tmp_dir/lambda-handle.out"
grep -q '\[handle-elim\]' "$tmp_dir/lambda-handle.out"
grep -q '\[lambda-intro\]' "$tmp_dir/lambda-handle.out"
./read_file.out --write-artifact "$tmp_dir/lambda-handle.apo" "$tmp_dir/lambda-handle.p" \
	>"$tmp_dir/lambda-handle-write.out"
./read_file.out --read-graph "$tmp_dir/lambda-handle.apo" \
	>"$tmp_dir/lambda-handle-read.out"
grep -q 'interface term main ' "$tmp_dir/lambda-handle-read.out"
