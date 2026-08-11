#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
SUITE_DIR=examples/type-infer-and-check
MANIFEST=$SUITE_DIR/manifest.tsv
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/a-program-type-suite.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"
make -f src/prototype/Makefile reader all >/dev/null

awk -F '\t' '
	NR == 1 {
		if ($0 != "path\texpectation\tdiagnostic_code\tmain_pattern\truntime_pattern\tartifact\tdescription") {
			exit 1
		}
		next
	}
	NF != 7 || $1 == "" || seen[$1]++ { exit 1 }
	END { if (NR != 17) exit 1 }
' "$MANIFEST"

find "$SUITE_DIR" -mindepth 2 -maxdepth 2 -type f -name '*.p' -printf '%P\n' |
	LC_ALL=C sort >"$TMP_DIR/files"
awk -F '\t' 'NR > 1 { print $1 }' "$MANIFEST" |
	LC_ALL=C sort >"$TMP_DIR/manifest-files"
if ! cmp -s "$TMP_DIR/files" "$TMP_DIR/manifest-files"; then
	echo 'type-infer-and-check manifest has missing or unlisted .p files' >&2
	diff -u "$TMP_DIR/files" "$TMP_DIR/manifest-files" >&2 || true
	exit 1
fi

diagnostic_matches() {
	code=$1
	file=$2
	case "$code" in
		unsupported-nested-recursion)
			grep -q 'diagnostic-code=unsupported-nested-recursion' "$file"
			;;
		unsupported-indexed-family)
			grep -q 'diagnostic-code=unsupported-indexed-family' "$file"
			;;
		*)
			echo "unknown manifest diagnostic code: $code" >&2
			return 1
			;;
	esac
}

tab=$(printf '\t')
tail -n +2 "$MANIFEST" |
while IFS="$tab" read -r relative expectation diagnostic main_pattern runtime_pattern artifact description; do
	source=$SUITE_DIR/$relative
	stem=$(printf '%s' "$relative" | tr '/.' '__')
	stdout=$TMP_DIR/$stem.out
	stderr=$TMP_DIR/$stem.err
	artifact_path=$TMP_DIR/$stem.apo

	if [ "$expectation" = pass ]; then
		if ! ./read_file.out "$source" >"$stdout" 2>"$stderr"; then
			echo "expected pass: $relative ($description)" >&2
			cat "$stderr" >&2
			exit 1
		fi
		grep -q "$main_pattern" "$stdout"
		if [ "$runtime_pattern" = - ]; then
			echo "passing manifest row has no runtime contract: $relative" >&2
			exit 1
		fi
		printf ':q\n' | ./a.out "$source" >"$TMP_DIR/$stem.runtime" \
			2>"$TMP_DIR/$stem.runtime.err"
		grep -Eq "$runtime_pattern" "$TMP_DIR/$stem.runtime"
		if [ "$artifact" = yes ]; then
			./read_file.out --write-artifact "$artifact_path" "$source" \
				>"$TMP_DIR/$stem.write" 2>"$TMP_DIR/$stem.write.err"
			./read_file.out --read-graph "$artifact_path" \
				>"$TMP_DIR/$stem.read" 2>"$TMP_DIR/$stem.read.err"
		fi
	elif [ "$expectation" = fail ]; then
		if ./read_file.out "$source" >"$stdout" 2>"$stderr"; then
			echo "expected failure: $relative ($description)" >&2
			exit 1
		fi
		diagnostic_matches "$diagnostic" "$stderr"
		grep -Eq "${relative##*/}:[0-9]+:[0-9]+:|span=[0-9]+:[0-9]+" "$stderr"
		test "$artifact" = no
		test "$runtime_pattern" = -
	else
		echo "invalid manifest expectation '$expectation' for $relative" >&2
		exit 1
	fi
done

{
	echo '<!-- BEGIN MANIFEST STATUS -->'
	echo '| File | Description | Expected result | Diagnostic | Runtime | Artifact replay |'
	echo '| --- | --- | --- | --- | --- | --- |'
	awk -F '\t' 'NR > 1 {
		result = $2 == "pass" ? "pass" : "expected failure"
		runtime = $5 == "-" ? "-" : "checked"
		printf "| `%s` | %s | %s | `%s` | %s | %s |\n", $1, $7, result, $3, runtime, $6
	}' "$MANIFEST"
	echo '<!-- END MANIFEST STATUS -->'
} >"$TMP_DIR/readme-table"

sed -n '/<!-- BEGIN MANIFEST STATUS -->/,/<!-- END MANIFEST STATUS -->/p' \
	"$SUITE_DIR/README.md" >"$TMP_DIR/readme-current"
if ! cmp -s "$TMP_DIR/readme-table" "$TMP_DIR/readme-current"; then
	echo 'README status table is not generated from manifest.tsv' >&2
	diff -u "$TMP_DIR/readme-current" "$TMP_DIR/readme-table" >&2 || true
	exit 1
fi

echo 'type inference and checking example manifest tests passed'
