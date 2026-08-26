#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"
. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-spec-consistency.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

artifact_schema=src/prototype/spec/artifact_v86.schema
checked_artifact_schema=src/prototype/spec/checked_artifact_v87.schema
hott_schema=src/prototype/spec/hott_fragment_v6.schema
calculus_header=src/prototype/calculus.h
artifact_header=src/prototype/include/a_program/artifact/interface.h
checked_artifact_header=src/prototype/include/a_program/checker/container.h
checked_artifact_source=src/prototype/src/checker/container.c
term_header=src/prototype/include/a_program/core/term.h

artifact_version=$(awk 'NR == 1 && $1 == "A_PROGRAM_ARTIFACT" { print $2 }' "$artifact_schema")
header_version=$(awk '/^#define PROTOTYPE_ARTIFACT_FORMAT_VERSION / { print $3 }' "$artifact_header")
if [ -z "$artifact_version" ] || [ "$artifact_version" != "$header_version" ]; then
	echo "artifact schema and reader version disagree" >&2
	exit 1
fi

checked_artifact_version=$(awk '
	NR == 1 && $1 == "A_PROGRAM_CHECKED_ARTIFACT" { print $2 }
' "$checked_artifact_schema")
checked_header_version=$(awk '
	/^#define PROTOTYPE_CHECKED_ARTIFACT_VERSION / { print $3 }
' "$checked_artifact_header")
if [ -z "$checked_artifact_version" ] ||
	[ "$checked_artifact_version" != "$checked_header_version" ]; then
	echo "checked artifact schema and reader version disagree" >&2
	exit 1
fi
if ! grep -q '^MAGIC is the exact eight-byte sequence `APCHK087`\.$' \
	"$checked_artifact_schema" ||
	! grep -q '^#define CHECKED_WIRE_MAGIC "APCHK087"$' \
	"$checked_artifact_source" ||
	! grep -q '^section_kind SEMANTIC=1 CONTRACTS=2 PRODUCER=3 DEBUG=4$' \
	"$checked_artifact_schema"; then
	echo "checked artifact schema and wire header disagree" >&2
	exit 1
fi

awk '
	/^enum prototype_term_tag/ { inside = 1; next }
	inside && /^};/ { exit }
	inside && /PROTOTYPE_TERM_[A-Z0-9_]+ = [0-9]+/ {
		name = $1
		sub(/^PROTOTYPE_TERM_/, "", name)
		value = $3
		sub(/,/, "", value)
		print name "=" value
	}
' "$term_header" | sort >"$tmp_dir/header-term-tags"
awk '
	$1 == "term_tag" {
		for (i = 2; i <= NF; ++i) print $i
	}
' "$checked_artifact_schema" | sort >"$tmp_dir/schema-term-tags"
if ! cmp -s "$tmp_dir/header-term-tags" "$tmp_dir/schema-term-tags"; then
	echo "checked artifact schema term tags disagree with Core" >&2
	diff -u "$tmp_dir/header-term-tags" "$tmp_dir/schema-term-tags" >&2 || true
	exit 1
fi

compare_checked_enum() {
	header=$1
	enum_name=$2
	prefix=$3
	schema_name=$4
	awk -v enum_name="$enum_name" -v prefix="$prefix" '
		$0 ~ "^enum " enum_name " \\{" { inside = 1; next }
		inside && /^};/ { exit }
		inside && index($0, prefix) != 0 && $0 ~ /= *[0-9]+,?$/ {
			line = $0
			gsub(/^[ \t]+|[ \t]+$/, "", line)
			split(line, parts, "=")
			name = parts[1]
			value = parts[2]
			gsub(/[ \t]+/, "", name)
			gsub(/[ \t,]+/, "", value)
			sub("^" prefix, "", name)
			print name "=" value
		}
	' "$header" | sort >"$tmp_dir/header-enum"
	awk -v schema_name="$schema_name" '
		$1 == schema_name {
			for (i = 2; i <= NF; ++i) print $i
		}
	' "$checked_artifact_schema" | sort >"$tmp_dir/schema-enum"
	if ! cmp -s "$tmp_dir/header-enum" "$tmp_dir/schema-enum"; then
		echo "checked artifact schema enum disagrees: $schema_name" >&2
		diff -u "$tmp_dir/header-enum" "$tmp_dir/schema-enum" >&2 || true
		exit 1
	fi
}

compare_checked_enum "$term_header" prototype_term_category \
	PROTOTYPE_TERM_CATEGORY_ term_category
compare_checked_enum "$term_header" prototype_term_computation_kind \
	PROTOTYPE_TERM_COMPUTATION_KIND_ term_computation_kind
compare_checked_enum "$term_header" prototype_term_application_role \
	PROTOTYPE_TERM_APPLICATION_ term_application_role
compare_checked_enum "$term_header" prototype_term_normalization_profile \
	PROTOTYPE_TERM_NORMALIZATION_ normalization_profile
compare_checked_enum "$term_header" prototype_computation_totality \
	PROTOTYPE_COMPUTATION_TOTALITY_ computation_totality
compare_checked_enum "$term_header" prototype_pure_primitive_id \
	PROTOTYPE_PURE_PRIMITIVE_ pure_primitive
compare_checked_enum "$term_header" prototype_effect_operation_id \
	PROTOTYPE_EFFECT_OPERATION_ effect_operation
compare_checked_enum "$term_header" prototype_effect_operation_classifier_schema \
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_ effect_operation_classifier
compare_checked_enum "$term_header" prototype_effect_operation_inner_policy \
	PROTOTYPE_EFFECT_OPERATION_INNER_ effect_operation_inner_policy
compare_checked_enum "$term_header" \
	prototype_effect_operation_resumption_multiplicity \
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ effect_operation_resumption
compare_checked_enum "$term_header" prototype_host_type_id \
	PROTOTYPE_HOST_TYPE_ host_type
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_context_extension_kind \
	PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_ context_extension
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_substitution_kind \
	PROTOTYPE_SEMANTIC_SUBSTITUTION_ substitution_kind
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_classifier_evidence_kind \
	PROTOTYPE_SEMANTIC_CLASSIFIER_ classifier_evidence
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_occurrence_kind \
	PROTOTYPE_SEMANTIC_OCCURRENCE_ occurrence_kind
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_match_refinement_kind \
	PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_ match_refinement
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_contract_kind \
	PROTOTYPE_SEMANTIC_CONTRACT_ contract_kind
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_effect_constraint_kind \
	PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_ effect_constraint_kind
compare_checked_enum src/prototype/include/a_program/checker/module.h \
	prototype_semantic_export_transparency \
	PROTOTYPE_SEMANTIC_EXPORT_ export_transparency
compare_checked_enum src/prototype/include/a_program/dimension/types.h \
	prototype_dimension_axis_image_kind \
	PROTOTYPE_DIMENSION_AXIS_ dimension_axis_image

for enum_line in \
	'function_graph_origin_role VALUE=1 GRAPH=2 IH=4' \
	'runtime_capability COMPUTATION_FOLD_RESULT_VERIFIER=1 OPERATION_DISPATCH=2 HANDLER=4 TERMINAL=8'
do
	if ! grep -Fxq "$enum_line" "$checked_artifact_schema"; then
		echo "checked artifact schema omits persisted flags: $enum_line" >&2
		exit 1
	fi
done

if [ -e src/prototype/spec/artifact_v83.schema ] ||
	grep -R -q 'spec/artifact_v83\.schema' \
		src/prototype/include src/prototype/src; then
	echo "archived artifact v83 is still an active implementation dependency" >&2
	exit 1
fi

artifact_fingerprint=$(sha256sum "$artifact_schema" | awk '{ print $1 }')
hott_fingerprint=$(sha256sum "$hott_schema" | awk '{ print $1 }')
header_artifact_fingerprint=$(awk '
	/PROTOTYPE_CALCULUS_FINGERPRINT/ {
		getline
		gsub(/["\\]/, "")
		gsub(/[[:space:]]/, "")
		print
	}
' "$calculus_header")
header_hott_fingerprint=$(awk '
	/PROTOTYPE_HOTT_CALCULUS_FINGERPRINT/ {
		getline
		gsub(/["\\]/, "")
		gsub(/[[:space:]]/, "")
		print
	}
' "$calculus_header")
if [ "$artifact_fingerprint" != "$header_artifact_fingerprint" ] ||
	[ "$hott_fingerprint" != "$header_hott_fingerprint" ]; then
	echo "semantic manifest fingerprint is stale" >&2
	exit 1
fi

for readme in README.md src/prototype/README.md; do
	if grep -Eio 'artifact( format)? v[0-9]+' "$readme" |
		grep -Eiv "v(${artifact_version}|${checked_artifact_version})$" >/dev/null; then
		echo "$readme advertises an obsolete artifact version" >&2
		exit 1
	fi
	if grep -Eio 'A_PROGRAM_ARTIFACT [0-9]+' "$readme" |
		grep -Eiv " ${artifact_version}$" >/dev/null; then
		echo "$readme contains an obsolete artifact header" >&2
		exit 1
	fi
	if ! grep -Eiq 'accepted proof artifact (format )?v86' "$readme" ||
		! grep -Eiq 'checked semantic container (format )?v87' "$readme"; then
		echo "$readme does not distinguish accepted v86 from checked v87" >&2
		exit 1
	fi
done

if [ "$(awk 'NR == 1 && $1 == "A_PROGRAM_HOTT_FRAGMENT" { print $2 }' "$hott_schema")" != 6 ] ||
	grep -Eq 'fragment version [0-5]|A1\.T0|MATCH_ELIM=13' "$hott_schema"; then
	echo "HOTT fragment manifest contains a stale version or proof number" >&2
	exit 1
fi
if ! grep -q 'MATCH_TYPE_FORMATION_INTRO=13 MATCH_ELIM=14' "$hott_schema"; then
	echo "HOTT object-action proof vocabulary disagrees with the kernel" >&2
	exit 1
fi

if ! grep -q 'canonical dimension operator embedded in the acted Term' "$artifact_schema" ||
	! grep -q '^universe_levels N$' "$artifact_schema" ||
	! grep -q '^universe_level ID LEVEL KNOWN$' "$artifact_schema" ||
	! grep -q 'a PENDING computation-fold obligation is a runtime contract and never a' "$artifact_schema" ||
	! grep -q 'effect obligation is never an' "$artifact_schema"; then
	echo "artifact manifest omits the current Identity or verification boundary" >&2
	exit 1
fi

prototype_compile c11 werror kernel \
	"$tmp_dir/spec-enum-check" \
	src/prototype/tests/checks/spec_enum_check.c
"$tmp_dir/spec-enum-check"

echo "current specification consistency tests passed"
