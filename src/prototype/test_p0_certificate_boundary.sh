#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-p0-certificate.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make reader >/dev/null

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
	FNR != NR { print }
' "$TMP_DIR/original.apo" "$TMP_DIR/original.apo" \
	>"$TMP_DIR/reversed-derivations.apo"
./read_file.out --aggregate-artifact "$TMP_DIR/regrounded.apo" \
	"$TMP_DIR/reversed-derivations.apo" >"$TMP_DIR/regrounded.out"
awk '$1 == "claim" { $2 = ""; print }' "$TMP_DIR/original.apo" | sort \
	>"$TMP_DIR/original-claims.txt"
awk '$1 == "claim" { $2 = ""; print }' "$TMP_DIR/regrounded.apo" | sort \
	>"$TMP_DIR/regrounded-claims.txt"
cmp "$TMP_DIR/original-claims.txt" "$TMP_DIR/regrounded-claims.txt"
canonicalize_derivations() {
	awk '
		FNR == NR && $1 == "claim" {
			claim[$2] = $3 ":" $5 ":" $6 ":" $8 ":" $10 ":" $11 ":" $12
			next
		}
		FNR != NR && $1 == "derivation" {
			line = $3 "|" claim[$5] "|" $7
			for (i = 8; i <= 14; ++i) line = line "|" $i
			premise_count = $15
			position = 16
			line = line "|premises=" premise_count
			for (i = 0; i < premise_count; ++i) {
				if ($position != "premise") exit 1
				premise_id = $(position + 1)
				premise = premise_id == 4294967295 ? premise_id : claim[premise_id]
				line = line "|" premise ":" $(position + 2) ":" \
					$(position + 3) ":" $(position + 4) ":" $(position + 5)
				position += 6
			}
			if ($position != "sources") exit 1
			source_count = $(position + 1)
			line = line "|sources=" source_count
			for (i = 0; i < source_count; ++i) {
				line = line "|" claim[$(position + 2 + i)]
			}
			print line
		}
	' "$1" "$1"
}
canonicalize_derivations "$TMP_DIR/original.apo" | sort \
	>"$TMP_DIR/original-derivations.txt"
canonicalize_derivations "$TMP_DIR/regrounded.apo" | sort \
	>"$TMP_DIR/regrounded-derivations.txt"
cmp "$TMP_DIR/original-derivations.txt" "$TMP_DIR/regrounded-derivations.txt"

# Equal numerical Universe edges retain separate exact source Claims. The two
# identity exports share erased Core and classifier terms, but their Operation
# authorities remain distinct.
./read_file.out --write-artifact "$TMP_DIR/shared-core.apo" \
	src/prototype/shared_core_universe_provenance_check.p \
	>"$TMP_DIR/shared-core.out"
id1_claim=$(awk '
	$1 == "term" && $2 == "id1" {
		for (i = 1; i <= NF; ++i) if ($i == "claim") print $(i + 1)
	}
' "$TMP_DIR/shared-core.apo")
id2_claim=$(awk '
	$1 == "term" && $2 == "id2" {
		for (i = 1; i <= NF; ++i) if ($i == "claim") print $(i + 1)
	}
' "$TMP_DIR/shared-core.apo")
test -n "$id1_claim"
test -n "$id2_claim"
test "$id1_claim" != "$id2_claim"
awk -v id1_claim="$id1_claim" -v id2_claim="$id2_claim" '
	FNR == NR && $1 == "claim" {
		authority_kind[$2] = $5
		authority_id[$2] = $6
		subject[$2] = $11
		classifier[$2] = $12
		next
	}
	FNR != NR && $1 == "SECTION" && $2 == "universe" {
		in_universe = 1
		print
		next
	}
	FNR != NR && in_universe && $1 == "counts" {
		$13 = $13 + 1
		print
		next
	}
	FNR != NR && $1 == "universe_constraints" {
		$2 = $2 + 1
		print
		next
	}
	FNR != NR && $1 == "END" && $2 == "universe" {
		in_universe = 0
		print
		next
	}
	FNR != NR && $1 == "universe_constraint" && !done {
		$9 = id1_claim
		$10 = authority_kind[id1_claim]
		$11 = authority_id[id1_claim]
		$12 = subject[id1_claim]
		$13 = classifier[id1_claim]
		print
		$2 = $2 + 1
		$9 = id2_claim
		$10 = authority_kind[id2_claim]
		$11 = authority_id[id2_claim]
		$12 = subject[id2_claim]
		$13 = classifier[id2_claim]
		print
		done = 1
		next
	}
	FNR != NR { print }
	END { if (!done) exit 1 }
' "$TMP_DIR/shared-core.apo" "$TMP_DIR/shared-core.apo" \
	>"$TMP_DIR/two-provenances.apo"
# Readback validates and retains both provenance records. Aggregation is not
# used here because it intentionally recollects constraints from the accepted
# proof graph instead of preserving injected serialized constraints.
./read_file.out --read-graph "$TMP_DIR/two-provenances.apo" \
	>"$TMP_DIR/two-provenances-read.out"
grep -q 'universe_constraints=2' "$TMP_DIR/two-provenances-read.out"
awk '
	$1 == "universe_constraint" {
		numerical = $3 SUBSEP $4 SUBSEP $5
		count[numerical]++
		source[numerical SUBSEP $9] = 1
	}
	END {
		for (key in count) {
			if (count[key] != 2) continue
			source_count = 0
			for (entry in source) {
				split(entry, parts, SUBSEP)
				if (parts[1] SUBSEP parts[2] SUBSEP parts[3] == key) source_count++
			}
			if (source_count == 2) found = 1
		}
		if (!found) exit 1
	}
' "$TMP_DIR/two-provenances.apo"

printf '%s\n' "P0 certificate boundary tests passed"
