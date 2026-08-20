#!/bin/sh
set -eu

# Boundary audit: ISSUE-3-CWF-CANDIDATE-CERTIFICATE
# Boundary audit: ISSUE-3-CWF-ARTIFACT-COVERAGE

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"
. src/prototype/build/test_support.sh
TIMING_TMP=$(mktemp -d "${TMPDIR:-/tmp}/a-program-hott-timing.XXXXXX")
trap 'rm -rf "$TIMING_TMP"' EXIT
prototype_test_timing_initialize "$TIMING_TMP"

prototype_test_phase compile
manifest_fingerprint=$(sha256sum src/prototype/spec/hott_fragment_v6.schema | awk '{print $1}')
header_fingerprint=$(awk '
	/PROTOTYPE_HOTT_CALCULUS_FINGERPRINT/ {
		getline
		gsub(/[\"\\]/, "")
		gsub(/[[:space:]]/, "")
		print
	}
' src/prototype/calculus.h)
if [ "$manifest_fingerprint" != "$header_fingerprint" ]; then
	echo "HOTT calculus fingerprint does not match hott_fragment_v6.schema" >&2
	exit 1
fi
changed_fingerprint=$(
	{ cat src/prototype/spec/hott_fragment_v6.schema; printf '\nsemantic-change\n'; } |
		sha256sum | awk '{print $1}'
)
if [ "$changed_fingerprint" = "$header_fingerprint" ]; then
	echo "HOTT semantic change did not invalidate the fingerprint" >&2
	exit 1
fi

prototype_compile c11 werror hott \
	/tmp/a-program-hott-goal-check \
	src/prototype/tests/checks/hott/main.c

prototype_test_phase execute_publish
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
			if ($2 != count || ($6 != 1 && $6 != 2 && $6 != 3 && $6 != 5)) {
				exit 1
			}
			key = $6 ":" ($5 == 4294967295 ? "family" : "witness")
			shapes[key]++
			count++
		}
		END {
			if (declared != 16 || count != declared ||
				shapes["1:family"] != 3 || shapes["1:witness"] != 5 ||
				shapes["2:family"] != 1 || shapes["2:witness"] != 1 ||
				shapes["3:family"] != 1 || shapes["3:witness"] != 3 ||
				shapes["5:family"] != 1 || shapes["5:witness"] != 1) {
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

prototype_test_phase readback
make reader >/dev/null
./read_file.out --read-interface "$identity_artifact" >/dev/null
cwf_inspection=/tmp/a-program-hott-cwf-inspection.out
./read_file.out --read-graph "$identity_artifact" >"$cwf_inspection"
grep -q '^#### Static Context and Substitution ####$' "$cwf_inspection"
grep -Eq '^context#[1-9][0-9]* parent=context#[0-9]+ depth=[1-9][0-9]* binding=binding#[0-9]+ classifier=term#[0-9]+$' \
	"$cwf_inspection"
grep -Eq '^substitution#[0-9]+ kind=extend .* evidence=claim#[0-9]+$' \
	"$cwf_inspection"
grep -Eq '^  core-value term#[0-9]+ = ' "$cwf_inspection"
grep -q '^#### Runtime Environment Boundary ####$' "$cwf_inspection"
grep -Eq '^intrinsic-environment fingerprint=[1-9][0-9]* default-integer=#\.Int32$' \
	"$cwf_inspection"
prototype_test_phase forgery
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
	echo "artifact reader accepted a source constructor with a forged ordinal" >&2
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
forged_missing_substitution_evidence_artifact=\
/tmp/a-program-hott-forged-missing-substitution-evidence.apo
awk '
	!changed && $1 == "substitution" && $3 == 4 {
		$10 = 4294967295
		changed = 1
	}
	{ print }
	END { if (!changed) exit 1 }
' "$identity_artifact" >"$forged_missing_substitution_evidence_artifact"
if ./read_file.out --read-graph "$forged_missing_substitution_evidence_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted EXTEND without its exact evidence Claim" >&2
	exit 1
fi
forged_substitution_claim_classifier_artifact=\
/tmp/a-program-hott-forged-substitution-claim-classifier.apo
awk '
	FNR == NR {
		if ($1 == "claim") {
			claim_proposition[$2] = $4
		}
		if (!found && $1 == "substitution" && $3 == 4) {
			target_claim = $10
			target_proposition = claim_proposition[target_claim]
			found = 1
		}
		next
	}
	$1 == "proposition" && $2 == target_proposition && !changed {
		$9 = $9 == 2 ? 3 : 2
		changed = 1
	}
	{ print }
	END { if (!found || !changed) exit 1 }
' "$identity_artifact" "$identity_artifact" \
	>"$forged_substitution_claim_classifier_artifact"
if ./read_file.out --read-graph "$forged_substitution_claim_classifier_artifact" \
	>/dev/null 2>&1; then
	echo "artifact reader accepted an EXTEND Claim with a forged classifier" >&2
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
for proof_kind in 12 14; do
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
v78_artifact=/tmp/a-program-hott-v78.apo
awk '
	$1 == "A_PROGRAM_ARTIFACT" { $2 = 78 }
	{ print }
' "$identity_artifact" >"$v78_artifact"
if ./read_file.out --read-interface "$v78_artifact" >/dev/null 2>&1; then
	echo "artifact reader accepted v78 through a fallback parser" >&2
	exit 1
fi
prototype_test_phase aggregate_link
./read_file.out --aggregate-artifact "$aggregate_artifact" "$identity_artifact" >/dev/null
./read_file.out --read-graph "$aggregate_artifact" >/dev/null
if ! identity_root_shape_is_valid "$aggregate_artifact" ||
	! grep -qx 'dependency fixture_8 namespace fixture_7' "$aggregate_artifact"; then
	echo "HOTT identity root was not preserved by artifact aggregation" >&2
	exit 1
fi
rm -f /tmp/a-program-hott-goal-check
prototype_test_phase_finish
