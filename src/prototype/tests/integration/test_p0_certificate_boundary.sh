#!/bin/sh
set -eu

# Boundary audit: ISSUE-17-ACCEPTED-REPLAY-AUTHORITY

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-p0-certificate.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make reader >/dev/null

JUDGEMENT_TYPES=src/prototype/include/a_program/kernel/judgement/types.h
ACCEPTED_REPLAY=src/prototype/src/kernel/typing/accepted_replay.inc
candidate_premise=$(sed -n \
	'/struct prototype_judgement_candidate_premise {/,/^};/p' \
	"$JUDGEMENT_TYPES")
printf '%s\n' "$candidate_premise" | grep -q \
	'const struct prototype_judgement_proposition\* proposition;'
printf '%s\n' "$candidate_premise" | grep -q \
	'struct prototype_judgement_proposition\* builder_proposition;'
rule_view=$(sed -n \
	'/struct prototype_judgement_rule_application_view {/,/^};/p' \
	"$JUDGEMENT_TYPES")
printf '%s\n' "$rule_view" | grep -q \
	'struct prototype_judgement_premise_view'
if printf '%s\n' "$rule_view" | grep -q \
	'prototype_judgement_candidate_premise'; then
	echo 'kernel rule view still exposes candidate storage' >&2
	exit 1
fi
accepted_adapter=$(sed -n \
	'/static int accepted_derivation_rule_application_view(/,/^}/p' \
	"$ACCEPTED_REPLAY")
if printf '%s\n' "$accepted_adapter" | grep -q \
	'prototype_judgement_candidate_premise\|proposition_store_kind'; then
	echo 'accepted replay still reconstructs candidate premise storage' >&2
	exit 1
fi
if grep -q '(struct prototype_judgement_proposition\*)premise' \
	"$ACCEPTED_REPLAY"; then
	echo 'accepted replay still casts an immutable Proposition to mutable' >&2
	exit 1
fi

# Derivation IDs are storage identities, not preferred-proof identities. Reverse
# every accepted Derivation ID and require readback/grounding to preserve the
# semantic Claim and Derivation multisets.
./read_file.out --write-artifact "$TMP_DIR/original.apo" examples/07_add.p \
	>"$TMP_DIR/original.out"
awk '
	FNR == NR && $1 == "derivation" {
		if ($2 > maximum_derivation_id) maximum_derivation_id = $2
		next
	}
	FNR != NR && $1 == "derivation" {
		$2 = maximum_derivation_id - $2
	}
	FNR != NR && $1 == "universe_constraint" {
		$10 = maximum_derivation_id - $10
	}
	FNR != NR && $1 == "universe_obligation" {
		$4 = maximum_derivation_id - $4
	}
	FNR != NR { print }
' "$TMP_DIR/original.apo" "$TMP_DIR/original.apo" \
	>"$TMP_DIR/reversed-derivations.apo"
./read_file.out --aggregate-artifact "$TMP_DIR/regrounded.apo" \
	"$TMP_DIR/reversed-derivations.apo" >"$TMP_DIR/regrounded.out"
cmp "$TMP_DIR/original.apo" "$TMP_DIR/regrounded.apo"
awk '$1 == "claim" { $2 = ""; print }' "$TMP_DIR/original.apo" | sort \
	>"$TMP_DIR/original-claims.txt"
awk '$1 == "claim" { $2 = ""; print }' "$TMP_DIR/regrounded.apo" | sort \
	>"$TMP_DIR/regrounded-claims.txt"
cmp "$TMP_DIR/original-claims.txt" "$TMP_DIR/regrounded-claims.txt"
canonicalize_derivations() {
	awk '
		function flush() {
			if (line != "") print line
			line = ""
		}
		$1 == "derivation" {
			flush()
			line = $3 "|" $4 ":" $5 "|" $6 ":" $7
			next
		}
		line != "" && ($1 == "payload" || $1 == "action" || $1 == "premise") {
			line = line "|" $0
			next
		}
		line != "" { flush() }
		END { flush() }
	' "$1"
}
canonicalize_derivations "$TMP_DIR/original.apo" | sort \
	>"$TMP_DIR/original-derivations.txt"
canonicalize_derivations "$TMP_DIR/regrounded.apo" | sort \
	>"$TMP_DIR/regrounded-derivations.txt"
cmp "$TMP_DIR/original-derivations.txt" "$TMP_DIR/regrounded-derivations.txt"

# A Match branch premise is certified by its exact refinement/projection
# Substitution edge. Do not duplicate that action as a separate weakening proof.
cat >"$TMP_DIR/context-weaken.p" <<'EOF_CONTEXT_WEAKEN'
Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

identityBool :: Bool -> Bool;
identityBool := \x : Bool => x;

identityNat :: Nat -> Nat;
identityNat := \x : Nat => x;

higherBool := \f : Bool -> Bool => f;
useHigherBool := higherBool &identityBool;

main := {
	b := (identityBool :: Bool -> Bool) Bool.true;
	b @true => (identityNat :: Nat -> Nat) Nat.zero
	  @false => (identityNat :: Nat -> Nat) (Nat.succ Nat.zero);
};
EOF_CONTEXT_WEAKEN
./read_file.out --write-artifact "$TMP_DIR/context-weaken.apo" \
	"$TMP_DIR/context-weaken.p" >"$TMP_DIR/context-weaken.out"
awk '
	$1 == "derivation" { match_elim = ($3 == 15) }
	match_elim && $1 == "premise" && $4 == "action" &&
		$5 == 1 && $6 != 4294967295 { found = 1 }
	END { if (!found) exit 1 }
' "$TMP_DIR/context-weaken.apo"
awk '
	$1 == "derivation" { match_elim = ($3 == 15) }
	match_elim && $1 == "premise" && $4 == "action" &&
		$5 == 1 && !done {
		$6 = 4294967295
		done = 1
		match_elim = 0
	}
	{ print }
	END { if (!done) exit 1 }
' "$TMP_DIR/context-weaken.apo" >"$TMP_DIR/forged-context-weaken.apo"
if ./read_file.out --read-graph "$TMP_DIR/forged-context-weaken.apo" \
		>"$TMP_DIR/forged-context-weaken.out" \
		2>"$TMP_DIR/forged-context-weaken.err"; then
	echo "context weakening without its projection authority unexpectedly passed" >&2
	exit 1
fi

# Serialized Universe constraints are checked projections. Injecting another
# numerically equal edge is rejected even when it cites an accepted Claim.
./read_file.out --write-artifact "$TMP_DIR/shared-core.apo" \
	src/prototype/tests/fixtures/artifact/shared_core_universe_provenance_check.p \
	>"$TMP_DIR/shared-core.out"
id1_claim=$(awk '
	$1 == "term" && $2 == "id1" {
		for (i = 1; i <= NF; ++i) if ($i == "evidence" && $(i + 1) == 1) print $(i + 2)
	}
' "$TMP_DIR/shared-core.apo")
id2_claim=$(awk '
	$1 == "term" && $2 == "id2" {
		for (i = 1; i <= NF; ++i) if ($i == "evidence" && $(i + 1) == 1) print $(i + 2)
	}
' "$TMP_DIR/shared-core.apo")
test -n "$id1_claim"
test -n "$id2_claim"
test "$id1_claim" != "$id2_claim"
awk '
	$1 == "SECTION" && $2 == "universe" { in_universe = 1 }
	in_universe && $1 == "counts" {
		$13 = $13 + 1
	}
	in_universe && $1 == "universe_constraints" {
		$2 = $2 + 1
	}
	in_universe && $1 == "universe_constraint" && !done {
		print
		$2 = $2 + 1
		done = 1
	}
	{ print }
	END { if (!done) exit 1 }
' "$TMP_DIR/shared-core.apo" \
	>"$TMP_DIR/two-provenances.apo"
if ./read_file.out --read-graph "$TMP_DIR/two-provenances.apo" \
		>"$TMP_DIR/two-provenances-read.out" 2>&1; then
	echo "artifact accepted an injected Universe constraint projection" >&2
	exit 1
fi

printf '%s\n' "P0 certificate boundary tests passed"
