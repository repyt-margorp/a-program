#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
cd "$ROOT_DIR"
. src/prototype/build/test_support.sh

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/a-program-spec-consistency.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

artifact_schema=src/prototype/spec/artifact_v83.schema
hott_schema=src/prototype/spec/hott_fragment_v6.schema
calculus_header=src/prototype/calculus.h
artifact_header=src/prototype/include/a_program/artifact/interface.h

artifact_version=$(awk 'NR == 1 && $1 == "A_PROGRAM_ARTIFACT" { print $2 }' "$artifact_schema")
header_version=$(awk '/^#define PROTOTYPE_ARTIFACT_FORMAT_VERSION / { print $3 }' "$artifact_header")
if [ -z "$artifact_version" ] || [ "$artifact_version" != "$header_version" ]; then
	echo "artifact schema and reader version disagree" >&2
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
		grep -Eiv "v${artifact_version}$" >/dev/null; then
		echo "$readme advertises an obsolete artifact version" >&2
		exit 1
	fi
	if grep -Eio 'A_PROGRAM_ARTIFACT [0-9]+' "$readme" |
		grep -Eiv " ${artifact_version}$" >/dev/null; then
		echo "$readme contains an obsolete artifact header" >&2
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
	! grep -q 'a PENDING obligation is a runtime contract and never a Claim' "$artifact_schema"; then
	echo "artifact manifest omits the current Identity or verification boundary" >&2
	exit 1
fi

prototype_compile c11 werror kernel \
	"$tmp_dir/spec-enum-check" \
	src/prototype/tests/checks/spec_enum_check.c
"$tmp_dir/spec-enum-check"

echo "current specification consistency tests passed"
