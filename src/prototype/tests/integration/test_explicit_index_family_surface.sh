#!/bin/sh
set -eu

# Boundary audit: ISSUE-16-IADT-CONCRETE-CONFORMANCE

. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

prototype_compile c11 werror graph \
	"$tmp_dir/lifted-ih-runtime-check" \
	src/prototype/tests/checks/lifted_ih_runtime_check.c
"$tmp_dir/lifted-ih-runtime-check"

matrix=src/prototype/tests/audit/iadts_conformance.tsv
awk -F '\t' '
	NR == 1 {
		expected = "case\tclassification\tformation\tconstruction\tsynthesis\tposthoc_expectation\telimination\tih\tartifact\treplay\truntime\tdiagnostic\tevidence"
		if ($0 != expected) exit 1
		next
	}
	NF != 13 || seen[$1]++ { exit 1 }
	$2 != "supported" && $2 != "residual" && $2 != "unsupported" && $2 != "invalid" { exit 1 }
	END { if (NR < 18) exit 1 }
' "$matrix"
while IFS="$(printf '\t')" read -r case_name classification formation construction \
	synthesis posthoc elimination ih artifact replay runtime diagnostic evidence
do
	if [ "$case_name" != case ] && [ ! -f "$evidence" ]; then
		echo "IADT conformance evidence is missing for $case_name: $evidence" >&2
		exit 1
	fi
done <"$matrix"

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
	./read_file.out --write-artifact "$tmp_dir/$fixture.apo" \
		"src/prototype/tests/fixtures/typing/$fixture.p" \
		>"$tmp_dir/$fixture-write.out"
	./read_file.out --read-graph "$tmp_dir/$fixture.apo" \
		>"$tmp_dir/$fixture-read.out"
done

for source in \
	src/prototype/tests/fixtures/typing/iadts_general_conformance_check.p \
	src/prototype/tests/fixtures/typing/iadts_box_perfect_construction_check.p
do
	fixture=$(basename "$source" .p)
	./read_file.out \
		"$source" \
		>"$tmp_dir/$fixture.out"
	./read_file.out --write-artifact "$tmp_dir/$fixture.apo" \
		"$source" \
		>"$tmp_dir/$fixture-write.out"
	./read_file.out --read-graph "$tmp_dir/$fixture.apo" \
		>"$tmp_dir/$fixture-read.out"
done

grep -q '^interface type Matrix ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface type DependentIndex ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface type Diagonal ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface term matrixZero ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface term matrixElement ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface term matrixZeroElement ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface term dependentZero ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"
grep -q '^interface term sameZero ' \
	"$tmp_dir/iadts_general_conformance_check-read.out"

awk 'BEGIN { OFS = " " }
	$1 == "type_constructor" && $3 == "matrix" { $11 = 3 }
	{ print }
' "$tmp_dir/iadts_general_conformance_check.apo" \
	>"$tmp_dir/iadts-forged-result-classifier.apo"
if ./read_file.out --read-graph "$tmp_dir/iadts-forged-result-classifier.apo" \
	>"$tmp_dir/iadts-forged-result-classifier.out" \
	2>"$tmp_dir/iadts-forged-result-classifier.err"; then
	echo "artifact accepted a forged indexed constructor result classifier" >&2
	exit 1
fi

awk 'BEGIN { OFS = " " }
	$1 == "type_constructor" && $3 == "matrix" { $10 = 0 }
	{ print }
' "$tmp_dir/iadts_general_conformance_check.apo" \
	>"$tmp_dir/iadts-forged-field-context.apo"
if ./read_file.out --read-graph "$tmp_dir/iadts-forged-field-context.apo" \
	>"$tmp_dir/iadts-forged-field-context.out" \
	2>"$tmp_dir/iadts-forged-field-context.err"; then
	echo "artifact accepted a forged indexed constructor field Context" >&2
	exit 1
fi

grep -q '^interface type Box ' \
	"$tmp_dir/iadts_box_perfect_construction_check-read.out"
grep -q '^interface type Perfect ' \
	"$tmp_dir/iadts_box_perfect_construction_check-read.out"
grep -q '^interface term natBox ' \
	"$tmp_dir/iadts_box_perfect_construction_check-read.out"
grep -q '^interface term nodeNat ' \
	"$tmp_dir/iadts_box_perfect_construction_check-read.out"

./read_file.out --write-artifact "$tmp_dir/AccSpecialized.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_full_specialization_check.p \
	>"$tmp_dir/acc-specialized-write.out"
./read_file.out --read-graph "$tmp_dir/AccSpecialized.apo" \
	>"$tmp_dir/acc-specialized-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-specialized-read.out"
grep -q '^typed_occurrences=' "$tmp_dir/acc-specialized-read.out"

./read_file.out --write-artifact "$tmp_dir/Acc.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_eliminator_check.p \
	>"$tmp_dir/acc-write.out"
./read_file.out --read-graph "$tmp_dir/Acc.apo" \
	>"$tmp_dir/acc-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-read.out"
grep -q '^typed_occurrences=' "$tmp_dir/acc-read.out"

awk 'BEGIN { OFS = " " }
	$1 == "payload" && $2 == "induction" { $6 = 0 }
	{ print }
' "$tmp_dir/Acc.apo" >"$tmp_dir/Acc-forged-ih.apo"
if ./read_file.out --read-graph "$tmp_dir/Acc-forged-ih.apo" \
	>"$tmp_dir/Acc-forged-ih.out" 2>"$tmp_dir/Acc-forged-ih.err"; then
	echo "artifact accepted a forged lifted indexed IH field" >&2
	exit 1
fi

./read_file.out --write-artifact "$tmp_dir/AccConcrete.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_acc_concrete_check.p \
	>"$tmp_dir/acc-concrete-write.out"
./read_file.out --read-graph "$tmp_dir/AccConcrete.apo" \
	>"$tmp_dir/acc-concrete-read.out"
grep -q '^interface term accFalse ' "$tmp_dir/acc-concrete-read.out"
grep -q '^interface type Acc ' "$tmp_dir/acc-concrete-read.out"
grep -q '^typed_occurrences=[1-9][0-9]* occurrence_match_cases=2 verification_obligations=0$' \
	"$tmp_dir/acc-concrete-read.out"

constant=src/prototype/tests/fixtures/typing/residual_index_equation_negative.p
./read_file.out --write-artifact "$tmp_dir/constant.apo" "$constant" \
	>"$tmp_dir/constant.out"
./read_file.out --read-graph "$tmp_dir/constant.apo" \
	>"$tmp_dir/constant-read.out"
grep -q 'refinement-status=4' "$tmp_dir/constant.out"
awk '
	$1 == "occurrence_match_case" && $4 == 4 && $5 == 4294967295 {
		found = 1
	}
	END { exit !found }
' "$tmp_dir/constant.apo"

if ./read_file.out \
	src/prototype/tests/fixtures/typing/dependent_residual_index_equation_negative.p \
	>"$tmp_dir/dependent-residual.out" 2>"$tmp_dir/dependent-residual.err"
then
	echo "equality-dependent residual Match unexpectedly compiled" >&2
	exit 1
fi
grep -q 'diagnostic-code=branch-refinement-residual' \
	"$tmp_dir/dependent-residual.err"

if ./read_file.out examples/type-infer-and-check/level2/03_rose.p \
	>"$tmp_dir/rose.out" 2>"$tmp_dir/rose.err"
then
	echo "unsupported nested-positive family unexpectedly compiled" >&2
	exit 1
fi
grep -q 'diagnostic-code=unsupported-nested-recursion' "$tmp_dir/rose.err"

./read_file.out \
	src/prototype/tests/fixtures/typing/explicit_index_family_append_check.p \
	>"$tmp_dir/append.out"
grep -q '^term append := LAMBDA' "$tmp_dir/append.out"
grep -q '^term main := APP' "$tmp_dir/append.out"
grep -q 'expected-type-exposure proof#' "$tmp_dir/append.out"
./read_file.out --write-artifact "$tmp_dir/append.apo" \
	src/prototype/tests/fixtures/typing/explicit_index_family_append_check.p \
	>"$tmp_dir/append-write.out"
./read_file.out --read-graph "$tmp_dir/append.apo" \
	>"$tmp_dir/append-read.out"
grep -q '^interface term append ' "$tmp_dir/append-read.out"
grep -q '^interface term main ' "$tmp_dir/append-read.out"

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
