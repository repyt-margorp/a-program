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
"$compiler" --steps 5000000 --image-limit 10000000 --run main --load "$directory/quick.a" > "$directory/reference"
printf 'FFTT' > "$directory/expected"
cmp "$directory/reference" "$directory/expected"
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
for mode in checked trusted; do
	options=(--steps 5000000 --image-limit none)
	if [[ $mode == trusted ]]; then options=(--trust-image --steps 0 --image-limit none); fi
	"$backend" "${options[@]}" "$directory/quick.a" main "$directory/quick.c" 2> "$directory/diagnostic"
	if [[ $mode == trusted ]]; then grep -q 'user-trusted saved completion.*steps=0' "$directory/diagnostic"; fi
	"${CC:-cc}" "${flags[@]}" -I"$here" "$directory/quick.c" "$here/runtime.c" -o "$directory/run"
	"$directory/run" > "$directory/actual"
	cmp "$directory/reference" "$directory/actual"
	cp "$directory/quick.c" "$directory/first.c"
	"$backend" "${options[@]}" "$directory/quick.a" main "$directory/quick.c" 2> "$directory/diagnostic"
	cmp "$directory/first.c" "$directory/quick.c"
done
sha256sum "$directory/quick.a" > "$directory/after"
cmp "$directory/before" "$directory/after"
printf 'C differential: Acc QuickSort evaluates to FFTT in checked and trusted standalone executables\n'
