#!/usr/bin/env bash
set -euo pipefail
binary=$1
root=$(dirname "${BASH_SOURCE[0]}")
provider="$root/fixtures/sorted-proof-provider.p"
theorem="$root/acceptance/generic-quick-sorted-result.p"
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

check() {
	local expected=$1 label=$2 code=0
	shift 2
	"$binary" --steps 5000000 "$@" > "$directory/status" || code=$?
	if [[ $code != "$expected" ]]; then
		printf '%s: expected %s, got %s\n' "$label" "$expected" "$code" >&2
		cat "$directory/status" >&2
		exit 1
	fi
	printf '%s: ' "$label"
	cat "$directory/status"
}

# The theorem must not depend on the optional internal packet generator.
nm "$binary" > "$directory/symbols"
if grep -Eq 'pg_function_graph_(witness|packet)' "$directory/symbols"; then
	exit 1
fi
check 0 computation-motive "$root/acceptance/computation-motive.p"
check 0 normalized-index --save "$directory/index.a" "$root/acceptance/indexed-normalized-transport.p"
check 0 normalized-index-reloaded --load "$directory/index.a"
check 1 wrong-normalized-index "$root/acceptance/indexed-normalized-transport-wrong.p"
check 0 general-result --legacy-intrinsic-dot --imports "$provider" \
	--save "$directory/complete.a" "$theorem"
check 0 general-result-reloaded --load "$directory/complete.a"
check 3 pending-result --legacy-intrinsic-dot --imports "$provider" --steps 100 \
	--save "$directory/pending.a" "$theorem"
check 0 resumed-result --load "$directory/pending.a"
check 0 retained-result --legacy-intrinsic-dot --imports "$provider" --retain-reductions \
	--save "$directory/retained.a" "$theorem"
check 0 retained-result-reloaded --load "$directory/retained.a"

# Change only the final post-check. No extra graph/adequacy premise is added.
sed '$s/general_sorted A R (quickSort A (\&le) xs);/general_sorted A R xs;/' "$theorem" > "$directory/wrong.p"
if cmp -s "$theorem" "$directory/wrong.p"; then exit 1; fi
check 1 wrong-result --legacy-intrinsic-dot --imports "$provider" "$directory/wrong.p"
check 3 pending-wrong-result --legacy-intrinsic-dot --imports "$provider" --steps 100 \
	--save "$directory/wrong.a" "$directory/wrong.p"
check 1 resumed-wrong-result --load "$directory/wrong.a"
sed 's/@true => (\\x:Nat => \\y:Nat => y)/@true => (\\x:Nat => \\y:Nat => Bool.true)/' \
	"$root/acceptance/computation-motive.p" > "$directory/wrong-motive.p"
check 1 wrong-computation-motive "$directory/wrong-motive.p"
printf '%s\n' 'general QuickSort ordinary-result theorem and negative/image controls passed'
