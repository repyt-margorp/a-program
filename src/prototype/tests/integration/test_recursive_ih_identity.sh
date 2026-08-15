#!/bin/sh
set -eu

# Boundary audit: ISSUE-5-TYPED-IH-OCCURRENCE

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-recursive-ih.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

SOURCE=src/prototype/tests/fixtures/typing/recursive_ih_field_identity_check.p

./read_file.out "$SOURCE" >"$TMP_DIR/source.out"
grep -q 'induction-hypothesis .* case=1 field=0 ' "$TMP_DIR/source.out"
grep -q 'induction-hypothesis .* case=1 field=1 ' "$TMP_DIR/source.out"

for pair in \
	'leftResult leftExpected' \
	'rightResult rightExpected' \
	'bothResult leftExpected' \
	'sizeResult sizeExpected' \
	'choiceLeftResult leftExpected' \
	'choiceRightResult rightExpected'
do
	set -- $pair
	./read_file.out --check-source-exports-normalization-equal \
		"$1" "$2" --reduction-mode default "$SOURCE" \
		>"$TMP_DIR/$1.out"
	grep -q 'mode=default yes$' "$TMP_DIR/$1.out"
done

./read_file.out --write-artifact "$TMP_DIR/tree.apo" "$SOURCE" \
	>"$TMP_DIR/write.out"
./read_file.out --read-graph "$TMP_DIR/tree.apo" >"$TMP_DIR/read.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/tree.apo" \
	rightResult rightExpected --reduction-mode default \
	>"$TMP_DIR/artifact-right.out"
grep -q 'mode=default yes$' "$TMP_DIR/artifact-right.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/tree.apo" \
	sizeResult sizeExpected --reduction-mode default \
	>"$TMP_DIR/artifact-size.out"
grep -q 'mode=default yes$' "$TMP_DIR/artifact-size.out"

# The two recursive ChoiceTree cases occupy the same telescope position but
# introduce distinct persistent bindings. Each IH argument Operation, its Core
# VAR, and the authoritative operation-case binder must agree exactly.
awk '
	$1 == "term_node" && $3 == 1 { var_binding[$2] = $4 }
	$1 == "term_node" && $3 == 9 { ih_argument[$2] = $5 }
	$1 == "operation" {
		op_tag[$2] = $3
		op_core[$2] = $6
		op_binding[$2] = $12
		op_argument[$2] = $14
		if ($3 == 7) {
			match_first_case[$2] = $25
		}
		if ($3 == 8) {
			ih_count++
			ih_operation[ih_count] = $2
			ih_owner[ih_count] = $16
			ih_case[ih_count] = $19
			ih_field[ih_count] = $20
			ih_ast_binder[ih_count] = $11
		}
	}
	$1 == "operation_case_binders" {
		case_binder_count[$2] = $3
		for (i = 0; i < $3; ++i) {
			case_ast_binder[$2, i] = $(4 + i * 2)
			case_binding[$2, i] = $(5 + i * 2)
		}
	}
	END {
		for (i = 1; i <= ih_count; ++i) {
			case_id = match_first_case[ih_owner[i]] + ih_case[i]
			argument_operation = op_argument[ih_operation[i]]
			argument_binding = op_binding[argument_operation]
			core_binding = var_binding[ih_argument[op_core[ih_operation[i]]]]
			if (case_binder_count[case_id] <= ih_field[i] ||
				argument_binding == "" || core_binding == "" ||
				argument_binding != core_binding ||
				argument_binding != case_binding[case_id, ih_field[i]] ||
				ih_ast_binder[i] != case_ast_binder[case_id, ih_field[i]]) {
				exit 1
			}
			for (j = i + 1; j <= ih_count; ++j) {
				left_binding = op_binding[op_argument[ih_operation[i]]]
				right_binding = op_binding[op_argument[ih_operation[j]]]
				if (ih_owner[i] == ih_owner[j] && ih_case[i] != ih_case[j] &&
					ih_field[i] == 0 && ih_field[j] == 0 &&
					ih_ast_binder[i] != ih_ast_binder[j] &&
					left_binding != right_binding) {
					found = 1
				}
			}
		}
		if (!found) exit 1
	}
' "$TMP_DIR/tree.apo"

# Operation identity is wire authority. Changing an IH from fork.right to
# leaf.value must fail before accepted proof replay can use the forged edge.
awk '
	$1 == "operation" && $3 == 8 && $19 == 1 && $20 == 1 && !changed {
		$19 = 0
		$20 = 0
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$TMP_DIR/tree.apo" >"$TMP_DIR/wrong-edge.apo"
if ./read_file.out --read-graph "$TMP_DIR/wrong-edge.apo" \
	>"$TMP_DIR/wrong-edge.out" 2>"$TMP_DIR/wrong-edge.err"; then
	echo 'forged IH case/field edge was accepted' >&2
	exit 1
fi

awk '
	$1 == "operation" && $3 == 8 && !changed {
		$18 = $18 + 1
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$TMP_DIR/tree.apo" >"$TMP_DIR/wrong-frame.apo"
if ./read_file.out --read-graph "$TMP_DIR/wrong-frame.apo" \
	>"$TMP_DIR/wrong-frame.out" 2>"$TMP_DIR/wrong-frame.err"; then
	echo 'forged IH frame was accepted' >&2
	exit 1
fi

# A frame's recursive subject binding controls iota substitution after
# readback. It must equal the binding carried by that frame's Match scrutinee,
# not merely be a numerically valid BindingId.
awk '
	$1 == "match_frame" && !changed {
		$4 = $4 + 1
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$TMP_DIR/tree.apo" >"$TMP_DIR/wrong-subject-binding.apo"
if ./read_file.out --read-graph "$TMP_DIR/wrong-subject-binding.apo" \
	>"$TMP_DIR/wrong-subject-binding.out" \
	2>"$TMP_DIR/wrong-subject-binding.err"; then
	echo 'forged IH recursive subject binding was accepted' >&2
	exit 1
fi

# The proof payload must agree with the typed edge, even when its erased Core
# VAR happens to alpha-intern with a field in another case.
awk '
	$1 == "payload" && $2 == "induction" && $5 == 1 && !changed {
		$5 = 0
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$TMP_DIR/tree.apo" >"$TMP_DIR/wrong-proof.apo"
if ./read_file.out --read-graph "$TMP_DIR/wrong-proof.apo" \
	>"$TMP_DIR/wrong-proof.out" 2>"$TMP_DIR/wrong-proof.err"; then
	echo 'forged IH proof field was accepted' >&2
	exit 1
fi

NONRECURSIVE_SOURCE=src/prototype/tests/fixtures/negative/nonrecursive_induction_hypothesis.p
if ./read_file.out "$NONRECURSIVE_SOURCE" \
	>"$TMP_DIR/nonrecursive.out" 2>"$TMP_DIR/nonrecursive.err"; then
	echo 'IH over a nonrecursive constructor field was accepted' >&2
	exit 1
fi
grep -Eq 'compile-diagnostic diagnostic-code=ih-ownership .*span=[1-9][0-9]*:[1-9][0-9]*' \
	"$TMP_DIR/nonrecursive.err"

echo 'recursive IH field identity tests passed'
