#!/usr/bin/env bash
set -euo pipefail
backend=${1:?a-to-c binary}
compiler=${2:?pointer-check binary}
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../../.." && pwd)
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
# The existing provider still contains an explicitly legacy intrinsic spelling.
"$compiler" --legacy-intrinsic-dot --steps 5000000 --imports "$repo/tests/fixtures/sorted-proof-provider.p" \
	--save "$directory/quick.a" "$here/fixtures/quicksort.p"
sha256sum "$directory/quick.a" > "$directory/before"
status=0
"$backend" --steps 5000000 --image-limit 10000000 "$directory/quick.a" main "$directory/quick.c" \
	2> "$directory/diagnostic" || status=$?
# Keep this honest boundary until Identity transport has a target realization.
# Do not erase it or normalize the whole sort just to obtain trivial C output.
test "$status" = 4
grep -q 'identity-field' "$directory/diagnostic"
test ! -e "$directory/quick.c"
status=0
"$backend" --trust-image --steps 0 --image-limit none "$directory/quick.a" main "$directory/quick.c" \
	2> "$directory/diagnostic" || status=$?
test "$status" = 4
grep -q 'user-trusted saved completion.*steps=0' "$directory/diagnostic"
grep -q 'identity-field' "$directory/diagnostic"
test ! -e "$directory/quick.c"
"$compiler" --steps 5000000 --image-limit 10000000 --run main --load "$directory/quick.a" > "$directory/reference"
printf 'FFTT' > "$directory/expected"
cmp "$directory/reference" "$directory/expected"
sha256sum "$directory/quick.a" > "$directory/after"
cmp "$directory/before" "$directory/after"
printf 'C boundary: valid Acc QuickSort evaluates to FFTT; Identity transport explicitly unsupported\n'
