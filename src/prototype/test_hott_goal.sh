#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT_DIR"
. src/prototype/build/test_support.sh

manifest_fingerprint=$(sha256sum src/prototype/hott_fragment_v2.schema | awk '{print $1}')
header_fingerprint=$(awk '
	/PROTOTYPE_HOTT_CALCULUS_FINGERPRINT/ {
		getline
		gsub(/[\"\\]/, "")
		gsub(/[[:space:]]/, "")
		print
	}
' src/prototype/calculus.h)
if [ "$manifest_fingerprint" != "$header_fingerprint" ]; then
	echo "HOTT calculus fingerprint does not match hott_fragment_v2.schema" >&2
	exit 1
fi
changed_fingerprint=$(
	{ cat src/prototype/hott_fragment_v2.schema; printf '\nsemantic-change\n'; } |
		sha256sum | awk '{print $1}'
)
if [ "$changed_fingerprint" = "$header_fingerprint" ]; then
	echo "HOTT semantic change did not invalidate the fingerprint" >&2
	exit 1
fi

prototype_compile c11 werror hott \
	/tmp/a-program-hott-goal-check \
	src/prototype/hott_goal_check.c

/tmp/a-program-hott-goal-check

identity_artifact=/tmp/a-program-hott-identity-root.apo
perturbed_identity_artifact=/tmp/a-program-hott-identity-root-perturbed.apo
aggregate_artifact=/tmp/a-program-hott-identity-root-aggregate.apo
identity_root_shape_is_valid() {
	awk '
		$1 == "identity_roots" {
			declared = $2
			next
		}
		$1 == "identity_root" {
			if ($2 != count || ($6 != 1 && $6 != 2 && $6 != 3)) {
				exit 1
			}
			key = $6 ":" ($5 == 4294967295 ? "family" : "witness")
			shapes[key]++
			count++
		}
		END {
			if (declared != 16 || count != declared ||
				shapes["1:family"] != 4 || shapes["1:witness"] != 6 ||
				shapes["2:family"] != 1 || shapes["2:witness"] != 1 ||
				shapes["3:family"] != 1 || shapes["3:witness"] != 3) {
				exit 1
			}
		}
	' "$1"
}
if ! cmp -s "$identity_artifact" "$perturbed_identity_artifact"; then
	echo "HOTT identity artifact depends on unrelated allocation history" >&2
	exit 1
fi
cp "$identity_artifact" /tmp/a-program-hott-identity-root-before-binding-history.apo
A_PROGRAM_HOTT_PREALLOCATE_BINDING=1 /tmp/a-program-hott-goal-check
if ! cmp -s \
	/tmp/a-program-hott-identity-root-before-binding-history.apo \
	"$identity_artifact"; then
	echo "HOTT identity artifact depends on reachable Binding allocation history" >&2
	exit 1
fi
cp "$identity_artifact" /tmp/a-program-hott-identity-root-before-type-order.apo
A_PROGRAM_HOTT_REVERSE_INDEPENDENT_TYPES=1 /tmp/a-program-hott-goal-check
if ! cmp -s \
	/tmp/a-program-hott-identity-root-before-type-order.apo \
	"$identity_artifact"; then
	echo "HOTT identity artifact depends on independent type allocation order" >&2
	exit 1
fi
cp "$identity_artifact" /tmp/a-program-hott-identity-root-before-derivation-order.apo
A_PROGRAM_HOTT_REVERSE_DERIVATIONS=1 /tmp/a-program-hott-goal-check
if ! cmp -s \
	/tmp/a-program-hott-identity-root-before-derivation-order.apo \
	"$identity_artifact"; then
	echo "HOTT identity artifact depends on Derivation arena order" >&2
	exit 1
fi
if ! identity_root_shape_is_valid "$identity_artifact" ||
	! grep -qx 'dependency fixture_8 namespace fixture_7' "$identity_artifact"; then
	echo "HOTT identity root was not published" >&2
	exit 1
fi
if grep -Eq '^term_node [0-9]+ (32|33)( |$)' "$identity_artifact"; then
	echo "legacy observation terms leaked into the identity-root artifact" >&2
	exit 1
fi
if grep -Eq '^(action_request|action_result|work_result|bridge|certificate|fuel|graph_revision) ' \
	"$identity_artifact" || grep -Fq "$header_fingerprint" "$identity_artifact"; then
	echo "compiler-local HOTT execution state leaked into the artifact" >&2
	exit 1
fi

make reader >/dev/null
./read_file.out --read-interface "$identity_artifact" >/dev/null
./read_file.out --read-graph "$identity_artifact" >/dev/null
forged_source_artifact=/tmp/a-program-hott-forged-identity-source.apo
awk '
	$1 == "identity_root" && $2 == 0 { $3 = $4 }
	{ print }
' "$identity_artifact" >"$forged_source_artifact"
if ./read_file.out --read-graph "$forged_source_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an identity root with a forged source Claim" >&2
	exit 1
fi
forged_rule_artifact=/tmp/a-program-hott-forged-identity-rule.apo
awk '
	$1 == "identity_root" && $2 == 0 { $6 = 2 }
	{ print }
' "$identity_artifact" >"$forged_rule_artifact"
if ./read_file.out --read-graph "$forged_rule_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an identity computation rule for the wrong type former" >&2
	exit 1
fi
awk '
	$1 == "identity_root" && $2 == 0 { $6 = 3 }
	{ print }
' "$identity_artifact" >"$forged_rule_artifact"
if ./read_file.out --read-graph "$forged_rule_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an unverified Pi identity computation rule" >&2
	exit 1
fi
awk '
	$1 == "identity_root" && $2 == 0 { $6 = 6 }
	{ print }
' "$identity_artifact" >"$forged_rule_artifact"
if ./read_file.out --read-graph "$forged_rule_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an incomplete Universe correspondence as identity" >&2
	exit 1
fi
forged_constructor_artifact=/tmp/a-program-hott-forged-identity-constructor.apo
awk '
	$1 == "type_constructor" && $2 == 2 { $5 = 1 }
	{ print }
' "$identity_artifact" >"$forged_constructor_artifact"
if ./read_file.out --read-graph "$forged_constructor_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted a generated identity constructor with a forged ordinal" >&2
	exit 1
fi
forged_witness_artifact=/tmp/a-program-hott-forged-identity-witness.apo
awk '
	$1 == "proposition" && $2 == 9 { $9 = 0 }
	{ print }
' "$identity_artifact" >"$forged_witness_artifact"
if ./read_file.out --read-graph "$forged_witness_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an identity witness with the wrong classifier" >&2
	exit 1
fi
forged_context_artifact=/tmp/a-program-hott-forged-identity-context.apo
awk '
	$1 == "context" && $2 == 1 { $3 = 999999 }
	{ print }
' "$identity_artifact" >"$forged_context_artifact"
if ./read_file.out --read-graph "$forged_context_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an out-of-range identity proof Context" >&2
	exit 1
fi
forged_substitution_artifact=/tmp/a-program-hott-forged-identity-substitution.apo
awk '
	!changed && $1 == "action" && $2 == 1 { $3 = 999999; changed = 1 }
	{ print }
' "$identity_artifact" >"$forged_substitution_artifact"
if ./read_file.out --read-graph "$forged_substitution_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an out-of-range proof Substitution action" >&2
	exit 1
fi
forged_substitution_classifier_artifact=\
/tmp/a-program-hott-forged-substitution-classifier.apo
awk '
	!changed && $1 == "substitution" && $3 == 4 {
		$9 = $9 == 0 ? 1 : 0
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$identity_artifact" >"$forged_substitution_classifier_artifact"
if ./read_file.out --read-graph "$forged_substitution_classifier_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted a forged identity substitution classifier" >&2
	exit 1
fi
forged_proof_kind_artifact=/tmp/a-program-hott-forged-proof-kind.apo
awk '
	$1 == "derivation" && $2 == 0 { $3 = 999999 }
	{ print }
' "$identity_artifact" >"$forged_proof_kind_artifact"
if ./read_file.out --read-graph "$forged_proof_kind_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted an unknown persistent proof kind" >&2
	exit 1
fi
forged_observation_proof_artifact=/tmp/a-program-hott-forged-observation-proof.apo
awk '
	$1 == "derivation" && !changed { $3 = 33; changed = 1 }
	{ print }
	END { if (!changed) exit 1 }
' "$identity_artifact" >"$forged_observation_proof_artifact"
if ./read_file.out --read-graph "$forged_observation_proof_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted compiler-local observation proof authority" >&2
	exit 1
fi
forged_observation_term_artifact=/tmp/a-program-hott-forged-observation-term.apo
awk '
	$1 == "term_node" && !changed { $3 = 32; changed = 1 }
	{ print }
	END { if (!changed) exit 1 }
' "$identity_artifact" >"$forged_observation_term_artifact"
if ./read_file.out --read-graph "$forged_observation_term_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted compiler-local observation term authority" >&2
	exit 1
fi
for reference_kind in proposition claim derivation premise term; do
	forged_reference_artifact=/tmp/a-program-hott-forged-$reference_kind-reference.apo
	awk -v reference_kind="$reference_kind" '
		reference_kind == "proposition" && $1 == "proposition" && !changed {
			$8 = 999999; changed = 1
		}
		reference_kind == "claim" && $1 == "claim" && !changed {
			$4 = 999999; changed = 1
		}
		reference_kind == "derivation" && $1 == "derivation" && !changed {
			$5 = 999999; changed = 1
		}
		reference_kind == "premise" && $1 == "premise" && $2 == "claim" && !changed {
			$3 = 999999; changed = 1
		}
		reference_kind == "term" && $1 == "term_node" && $3 == 3 && !changed {
			$4 = 999999; changed = 1
		}
		{ print }
		END { if (!changed) exit 1 }
	' "$identity_artifact" >"$forged_reference_artifact"
	if ./read_file.out --read-graph "$forged_reference_artifact" \
		>/dev/null 2>&1; then
		echo "artifact reader accepted out-of-range $reference_kind reference" >&2
		exit 1
	fi
done
forged_premise_order_artifact=/tmp/a-program-hott-forged-premise-order.apo
awk '
	$1 == "derivation" && $3 == 6 && $7 == 2 && !selected {
		selected = 1
	}
	selected && $1 == "premise" && !first_seen {
		first = $0
		first_seen = 1
		next
	}
	selected && first_seen && $1 == "premise" && !changed {
		print
		print first
		changed = 1
		next
	}
	{ print }
	END { if (!changed) exit 1 }
' "$identity_artifact" >"$forged_premise_order_artifact"
if ./read_file.out --read-graph "$forged_premise_order_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted reordered Lambda premises" >&2
	exit 1
fi
for proof_kind in 12 14 16; do
	forged_rule_artifact="/tmp/a-program-hott-forged-rule-$proof_kind.apo"
	awk '
		$1 == "derivation" && $3 == proof_kind && !changed {
			$3 = 7
			changed = 1
		}
		{ print }
		END { if (!changed) exit 1 }
	' proof_kind="$proof_kind" "$identity_artifact" >"$forged_rule_artifact"
	if ./read_file.out --read-graph "$forged_rule_artifact" >/dev/null 2>&1; then
		echo "artifact reader accepted proof kind $proof_kind relabelled as APP" >&2
		exit 1
	fi
done
v69_artifact=/tmp/a-program-hott-v69.apo
awk '
	$1 == "A_PROGRAM_ARTIFACT" { $2 = 69 }
	{ print }
' "$identity_artifact" >"$v69_artifact"
if ./read_file.out --read-interface "$v69_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted v69 through a fallback parser" >&2
	exit 1
fi
./read_file.out --aggregate-artifact "$aggregate_artifact" "$identity_artifact" >/dev/null
./read_file.out --read-graph "$aggregate_artifact" >/dev/null
if ! identity_root_shape_is_valid "$aggregate_artifact" ||
	! grep -qx 'dependency fixture_8 namespace fixture_7' "$aggregate_artifact"; then
	echo "HOTT identity root was not preserved by artifact aggregation" >&2
	exit 1
fi
rm -f /tmp/a-program-hott-goal-check
