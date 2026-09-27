#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../../.." && pwd)
fixtures="$root/tests/fixtures"
sed '/^import /d' "$fixtures/sorted-proof-provider.p" "$fixtures/local-strong-sorted.p" \
	"$fixtures/quick-sort-proof-common.p" "$root/tests/acceptance/generic-quick-local-sorted-result.p" \
	"$fixtures/finite_positions.p" "$fixtures/finite_vectors.p" "$fixtures/finite_list_views.p" \
	"$fixtures/generic_sorted/content-proof.p" "$fixtures/generic_sorted/content-result-proof.p" \
	"$fixtures/finite_permutation_views.p" "$here/common.p" "$here/insertion.p" "$@"
