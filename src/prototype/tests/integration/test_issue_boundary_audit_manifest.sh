#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
MANIFEST=src/prototype/tests/audit/github_issues_3_10.tsv
TAB=$(printf '\t')

cd "$ROOT_DIR"

expected_header="issue${TAB}boundary_id${TAB}runner${TAB}evidence${TAB}marker${TAB}summary"
IFS= read -r actual_header < "$MANIFEST"
if [ "$actual_header" != "$expected_header" ]; then
	echo 'issue boundary audit manifest has an invalid header' >&2
	exit 1
fi

tail -n +2 "$MANIFEST" |
while IFS="$TAB" read -r issue boundary_id runner evidence marker summary extra; do
	if [ -z "$issue" ] || [ -z "$boundary_id" ] || [ -z "$runner" ] ||
		[ -z "$evidence" ] || [ -z "$marker" ] || [ -z "$summary" ] ||
		[ -n "$extra" ]; then
		echo "malformed issue boundary row: $boundary_id" >&2
		exit 1
	fi
	case "$issue" in
		3|4|5|6|7|8|9|10) ;;
		*)
			echo "unexpected issue in boundary manifest: $issue" >&2
			exit 1
			;;
	esac
	case "$marker" in
		"ISSUE-$issue-"*) ;;
		*)
			echo "boundary marker does not belong to issue $issue: $marker" >&2
			exit 1
			;;
	esac
	case "$runner" in
		src/prototype/tests/integration/test_*.sh) ;;
		*)
			echo "boundary runner is outside integration discovery: $runner" >&2
			exit 1
			;;
	esac
	if [ ! -f "$runner" ]; then
		echo "missing boundary runner: $runner" >&2
		exit 1
	fi
	if [ ! -f "$evidence" ]; then
		echo "missing boundary evidence: $evidence" >&2
		exit 1
	fi
	if ! grep -Fq "# Boundary audit: $marker" "$runner"; then
		echo "boundary marker $marker is absent from $runner" >&2
		exit 1
	fi
done

awk -F '\t' '
	NR == 1 { next }
	{
		if (seen_boundary[$2]++) {
			printf "duplicate boundary id: %s\n", $2 > "/dev/stderr"
			exit 1
		}
		issue_count[$1]++
	}
	END {
		for (issue = 3; issue <= 10; ++issue) {
			if (!issue_count[issue]) {
				printf "issue %d has no permanent boundary test\n", issue > "/dev/stderr"
				exit 1
			}
		}
	}
' "$MANIFEST"

echo 'GitHub issue boundary audit manifest tests passed'
