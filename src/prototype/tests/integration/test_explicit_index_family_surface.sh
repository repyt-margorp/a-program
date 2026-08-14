#!/bin/sh
set -eu

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror graph \
	"$tmp_dir/lifted-ih-runtime-check" \
	src/prototype/tests/checks/lifted_ih_runtime_check.c
"$tmp_dir/lifted-ih-runtime-check"

./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_vec_check.p \
	>"$tmp_dir/vec.out"
grep -q '^type (Vec A) constructors=2$' "$tmp_dir/vec.out"
grep -q '^term empty := CONSTRUCTOR' "$tmp_dir/vec.out"
grep -q '^term single := APP(APP(APP(CONSTRUCTOR' "$tmp_dir/vec.out"

./read_file.out --write-artifact "$tmp_dir/Vec.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_vec_check.p \
	>"$tmp_dir/vec-write.out"
./read_file.out --read-graph "$tmp_dir/Vec.apo" \
	>"$tmp_dir/vec-read.out"
grep -q '^interface type Vec .* constructors=2 ' "$tmp_dir/vec-read.out"
grep -q '^interface constructor type_export#[0-9][0-9]*\.nil ordinal=0 fields=0 curried_classifier_cache=' \
	"$tmp_dir/vec-read.out"
grep -q '^interface constructor type_export#[0-9][0-9]*\.cons ordinal=1 fields=3 curried_classifier_cache=' \
	"$tmp_dir/vec-read.out"

./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_fin_check.p \
	>"$tmp_dir/fin.out"
grep -q '^type Fin constructors=2$' "$tmp_dir/fin.out"
grep -q '^term first := APP(CONSTRUCTOR' "$tmp_dir/fin.out"
grep -q '^term second := APP(APP(CONSTRUCTOR' "$tmp_dir/fin.out"

for fixture in \
	explicit_index_family_head_check \
	explicit_index_family_tail_check \
	explicit_index_family_tail_infer \
	explicit_index_family_map_check \
	explicit_index_family_acc_check \
	explicit_index_family_acc_eliminator_check \
	explicit_index_family_acc_parameter_specialization_check \
	explicit_index_family_acc_full_specialization_check \
	explicit_index_family_acc_concrete_check
do
	./read_file.out \
		"src/prototype/tests/fixtures/typing/$fixture.p" \
		>"$tmp_dir/$fixture.out"
done

./read_file.out --write-artifact "$tmp_dir/AccSpecialized.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_full_specialization_check.p \
	>"$tmp_dir/acc-specialized-write.out"
./read_file.out --read-graph "$tmp_dir/AccSpecialized.apo" \
	>"$tmp_dir/acc-specialized-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-specialized-read.out"
grep -q '^operation_occurrences=' "$tmp_dir/acc-specialized-read.out"

./read_file.out --write-artifact "$tmp_dir/Acc.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_eliminator_check.p \
	>"$tmp_dir/acc-write.out"
./read_file.out --read-graph "$tmp_dir/Acc.apo" \
	>"$tmp_dir/acc-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-read.out"
grep -q '^operation_occurrences=' "$tmp_dir/acc-read.out"

./read_file.out --write-artifact "$tmp_dir/AccConcrete.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_concrete_check.p \
	>"$tmp_dir/acc-concrete-write.out"
./read_file.out --read-graph "$tmp_dir/AccConcrete.apo" \
	>"$tmp_dir/acc-concrete-read.out"
grep -q '^interface term accFalse ' "$tmp_dir/acc-concrete-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-concrete-read.out"
grep -q '^operation_occurrences=[1-9][0-9]* operation_cases=2 verification_obligations=0$' \
	"$tmp_dir/acc-concrete-read.out"

cat >"$tmp_dir/reachable-branch-mismatch.p" <<'EOF'
Bool := @{ false : *; true : *; };
Nat := @{ zero : *; succ : * -> *; };
Precedes := @\left : Bool => @\right : Bool => {
	falseBeforeTrue : * Bool.false Bool.true;
};
Holder := @{ hold : ((edge : Precedes Bool.false Bool.true) -> Nat) -> *; };
bad := Holder.hold &(\edge : Precedes Bool.false Bool.true =>
	edge @falseBeforeTrue => Bool.false);
EOF
if ./read_file.out "$tmp_dir/reachable-branch-mismatch.p" \
	>"$tmp_dir/reachable-branch-mismatch.out" \
	2>"$tmp_dir/reachable-branch-mismatch.err"
then
	echo "reachable indexed branch with the wrong result type unexpectedly passed" >&2
	exit 1
fi

if ./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_named_self_negative.p \
	>"$tmp_dir/named-self.out" 2>"$tmp_dir/named-self.err"
then
	echo "named recursive self reference unexpectedly passed" >&2
	exit 1
fi

if ./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_negative.p \
	>"$tmp_dir/negative-acc.out" 2>"$tmp_dir/negative-acc.err"
then
	echo "negative recursive Acc occurrence unexpectedly passed" >&2
	exit 1
fi

cat >"$tmp_dir/missing-index-binder.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
Bad := @n : Nat => { make : Bad Nat.zero; };
EOF
if ./read_file.out "$tmp_dir/missing-index-binder.p" \
	>"$tmp_dir/missing.out" 2>"$tmp_dir/missing.err"; then
	echo "malformed index binder unexpectedly passed" >&2
	exit 1
fi
grep -q "expected '\\\\' after '@' in index binder" \
	"$tmp_dir/missing.err"

cat >"$tmp_dir/parameter-after-index.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
Bad := @\n : Nat => \A : @ => @{ make : Bad n A; };
EOF
if ./read_file.out "$tmp_dir/parameter-after-index.p" \
	>"$tmp_dir/order.out" 2>"$tmp_dir/order.err"; then
	echo "parameter after index unexpectedly passed" >&2
	exit 1
fi
