#!/usr/bin/env bash
set -euo pipefail
binary=$1
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
steps=100000
source "$here/../finite_sorting/check-functions.sh"
# Gate assertions must also fail when called from an if condition.
if check 2 helper-exit-mismatch "$here/sample.p"; then exit 1; fi
if (binary=true; check 0 helper-output-mismatch); then exit 1; fi
check 0 source --save "$directory/full.a" "$here/sample.p"
"$binary" --help > "$directory/help"
grep -q -- '--image-limit N' "$directory/help"
for value in '' 0 -1 +1 ' 1' 1x 1.5 18446744073709551616; do
	check 2 invalid-option --load --image-limit "$value" "$directory/full.a" 2> "$directory/status.err"
	grep -q '^usage:' "$directory/status.err"
done
check 2 missing-option --load "$directory/full.a" --image-limit
check 2 duplicate-option --load --image-limit 1000 --image-limit 2000 "$directory/full.a"
check 2 source-option --image-limit 1000 "$here/sample.p"
check 2 insufficient-bound --load --image-limit 1 "$directory/full.a"
check 2 representation-bound --load --image-limit 18446744073709551615 "$directory/full.a"
check 0 default-load --load "$directory/full.a"
cp "$directory/status" "$directory/default.status"
check 0 explicit-load --load --image-limit 1000000 "$directory/full.a"
cmp "$directory/default.status" "$directory/status"
check 0 stdin-load --load --image-limit 1000000 - < "$directory/full.a"
check 0 explicit-root --load --root 1 --image-limit 1000000 "$directory/full.a"
check 2 wrong-root --load --root 2 --image-limit 1000000 "$directory/full.a"
check 0 normalize --load --image-limit 1000000 --nf main "$directory/full.a"
printf ':status\n:quit\n' | "$binary" --load --image-limit 1000000 --repl "$directory/full.a" > "$directory/repl.status"
grep -q '^done steps=' "$directory/repl.status"
for storage in ordinary retained; do
	options=()
	if [[ $storage == retained ]]; then options+=(--retain-reductions); fi
	for budget in 0 1 100000; do
		expected=3
		if [[ $budget == 100000 ]]; then expected=0; fi
		check "$expected" "$storage-save-$budget" --steps "$budget" "${options[@]}" --save "$directory/input.a" "$here/sample.p"
		check 3 "$storage-inert-$budget" --load --image-limit 1000000 --steps 0 "${options[@]}" --save "$directory/copy.a" "$directory/input.a"
		cmp "$directory/input.a" "$directory/copy.a"
		check 0 "$storage-resume-$budget" --load --image-limit 1000000 "$directory/copy.a"
	done
	check 3 "$storage-invalid-pending" --steps 0 "${options[@]}" --save "$directory/bad.a" "$here/invalid.p"
	check 1 "$storage-invalid-resume" --load --image-limit 1000000 "$directory/bad.a"
done
printf '%s\n' 'image limit: parsing, unchanged default, bounded load, ordinary/retained resume and rejection passed'
