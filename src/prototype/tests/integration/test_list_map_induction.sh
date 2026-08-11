#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-list-map.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader >/dev/null

PURE_SOURCE=src/prototype/tests/fixtures/typing/list_map_induction_check.p
EFFECT_SOURCE=src/prototype/tests/fixtures/effects/list_map_effect_check.p

./read_file.out "$PURE_SOURCE" >"$TMP_DIR/pure.out"
grep -q 'EFFECT_ROW_FORALL' "$TMP_DIR/pure.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/pure.out"
grep -q '\[ih-elim proof#' "$TMP_DIR/pure.out"

for pair in \
	'directMain expectedMulti' \
	'helperMain expectedMulti' \
	'monomorphicEmpty expectedEmpty' \
	'monomorphicSingle expectedSingle' \
	'monomorphicMulti expectedMulti' \
	'polymorphicMain expectedMulti' \
	'sequencedMain expectedMulti'
do
	set -- $pair
	./read_file.out --check-source-exports-normalization-equal \
		"$1" "$2" --reduction-mode default "$PURE_SOURCE" \
		>"$TMP_DIR/$1.out"
	grep -q 'mode=default yes$' "$TMP_DIR/$1.out"
done

./read_file.out --write-artifact "$TMP_DIR/map.apo" "$PURE_SOURCE" \
	>"$TMP_DIR/map-write.out"
./read_file.out --read-graph "$TMP_DIR/map.apo" >"$TMP_DIR/map-read.out"
./read_file.out --check-exports-normalization-equal "$TMP_DIR/map.apo" \
	polymorphicMain expectedMulti --reduction-mode default \
	>"$TMP_DIR/map-equal.out"
grep -q 'mode=default yes$' "$TMP_DIR/map-equal.out"

./read_file.out "$EFFECT_SOURCE" >"$TMP_DIR/effect.out"
grep -q 'EFFECT_ROW_OPERATION' "$TMP_DIR/effect.out"
grep -q '\[solved-match-motive proof#' "$TMP_DIR/effect.out"
./read_file.out --write-artifact "$TMP_DIR/effect.apo" "$EFFECT_SOURCE" \
	>"$TMP_DIR/effect-write.out"
./read_file.out --read-graph "$TMP_DIR/effect.apo" >"$TMP_DIR/effect-read.out"

solved_match_kind=$(awk '
	/enum prototype_judgement_proof_kind/ { in_enum = 1; value = 0; next }
	in_enum && /^};/ { exit }
	in_enum {
		line = $0
		sub(/\/\/.*/, "", line)
		gsub(/,/, "", line)
		gsub(/^[ \t]+|[ \t]+$/, "", line)
		if (line == "") next
		split(line, parts, "=")
		name = parts[1]
		gsub(/[ \t]+/, "", name)
		current = parts[2] != "" ? parts[2] + 0 : value
		if (name == "PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE") {
			print current
			exit
		}
		value = current + 1
	}
' src/prototype/include/a_program/kernel/judgement/types.h)

awk -v proof_kind="$solved_match_kind" '
	FNR == NR {
		if ($1 == "proposition") classifier[$2] = $9
		if ($1 == "claim") claim_proposition[$2] = $4
		if ($1 == "derivation" && $3 == proof_kind && !target_claim) {
			target_claim = $5
		}
		if ($1 == "term_node") {
			tag[$2] = $3
			arg0[$2] = $4
			arg1[$2] = $5
			if ($3 == 20 && !empty_row) empty_row = $2
		}
		next
	}
	FNR == 1 {
		match_classifier = classifier[claim_proposition[target_claim]]
		motive_lambda = arg0[match_classifier]
		motive_body = arg1[motive_lambda]
		if (!target_claim || tag[match_classifier] != 3 ||
			tag[motive_lambda] != 4 || tag[motive_body] != 24 || !empty_row) {
			exit 2
		}
	}
	$1 == "term_node" && $2 == motive_body && !changed {
		$4 = empty_row
		changed = 1
	}
	{ print }
	END { if (!changed) exit 3 }
' "$TMP_DIR/effect.apo" "$TMP_DIR/effect.apo" >"$TMP_DIR/forged-row.apo"

if ./read_file.out --read-graph "$TMP_DIR/forged-row.apo" \
	>"$TMP_DIR/forged-row.out" 2>"$TMP_DIR/forged-row.err"; then
	echo 'artifact with a forged Match effect row was accepted' >&2
	exit 1
fi

cat >"$TMP_DIR/unquoted-function.p" <<'EOF'
Nat := @{ zero : *; succ : * -> *; };
apply := \f : Nat -> Nat => f Nat.zero;
identity := \x : Nat => x;
main := apply identity;
EOF
if ./read_file.out "$TMP_DIR/unquoted-function.p" \
	>"$TMP_DIR/unquoted.out" 2>"$TMP_DIR/unquoted.err"; then
	echo 'raw higher-order function was accepted without quotation' >&2
	exit 1
fi

echo 'List map induction tests passed'
