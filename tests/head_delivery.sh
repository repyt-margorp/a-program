#!/usr/bin/env bash
set -euo pipefail
build=${1:?usage: head_delivery.sh BUILD_DIRECTORY}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/a-program-head-delivery.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$build/direct_test"
"$build/head_test"
"$build/head_cleanup_test"
for cut in {0..8}; do
	"$build/head_test" write "$work/head-$cut.machine" "$cut"
	"$build/head_test" read "$work/head-$cut.machine"
done
python3 -B "$here/total_result_cuts.py" "$build/total_result_test" "$work/total-result"
