#!/usr/bin/env bash
set -euo pipefail
binary=${1:?usage: file_policy.sh POINTER_CHECK [LARGE_CURRENT_IMAGE]}
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
expect() {
	local expected=$1 status=0
	shift
	"$binary" "$@" > "$directory/status" 2> "$directory/error" || status=$?
	[[ $status == "$expected" ]]
}
printf 'main := #0;\n' > "$directory/source.p"
expect 3 --steps 0 --save "$directory/source.a" "$directory/source.p"
expect 3 --load --steps 0 --save "$directory/copy.a" "$directory/source.a"
cmp "$directory/source.a" "$directory/copy.a"
expect 0 --save-inputs "$directory/inputs.a" "$directory/source.p"
expect 3 --load --steps 0 --save "$directory/copy.a" "$directory/inputs.a"
cmp "$directory/inputs.a" "$directory/copy.a"
expect 0 --load "$directory/inputs.a"
expect 2 --save "$directory/a.a" --save-inputs "$directory/b.a" "$directory/source.p"
expect 0 --nf main --save-inputs "$directory/normalization.a" "$directory/source.p"
expect 3 --load --root 2 --steps 0 --save "$directory/copy.a" "$directory/normalization.a"
cmp "$directory/normalization.a" "$directory/copy.a"
expect 0 --load --root 2 "$directory/normalization.a"
expect 2 --load --image-limit 1 --steps 0 "$directory/source.a"
expect 3 --load --image-limit 1000000 --steps 0 --save "$directory/copy.a" - < <(cat "$directory/source.a")
cmp "$directory/source.a" "$directory/copy.a"
expect 2 --load --image-limit 1 --steps 0 - < <(cat "$directory/source.a")
expect 3 --load --image-limit none --steps 0 --save "$directory/copy.a" "$directory/source.a"
cmp "$directory/source.a" "$directory/copy.a"
expect 3 --load --image-limit none --steps 0 --save "$directory/copy.a" - < <(cat "$directory/source.a")
cmp "$directory/source.a" "$directory/copy.a"
for value in -1 0 invalid 18446744073709551616; do
	expect 2 --load --image-limit "$value" --steps 0 "$directory/source.a"
done
if [[ $# == 2 ]]; then
	# This measured fixture exceeds the default. Overrides are explicit, not compaction.
	expect 2 --load --steps 0 "$2"
	expect 3 --load --image-limit 10000000 --steps 0 --save "$directory/large.a" "$2"
	cmp "$2" "$directory/large.a"
	expect 3 --load --image-limit none --steps 0 --save "$directory/unlimited.a" "$2"
	cmp "$2" "$directory/unlimited.a"
	expect 3 --load --image-limit 10000000 --steps 0 --save "$directory/pipe.a" - < <(cat "$2")
	cmp "$2" "$directory/pipe.a"
	expect 3 --load --image-limit 10000000 --steps 0 --save-inputs "$directory/inputs.a" "$2"
	[[ $(wc -c < "$directory/inputs.a") -lt $(wc -c < "$2") ]]
	expect 3 --load --steps 0 --save "$directory/copy.a" "$directory/inputs.a"
	cmp "$directory/inputs.a" "$directory/copy.a"
	printf 'artifact profiles: retained_bytes=%s inputs_bytes=%s conversion_steps=0\n' \
		"$(wc -c < "$2")" "$(wc -c < "$directory/inputs.a")"
fi
printf 'artifact policy: fixed/unbounded quota, inert file/pipe loading, explicit larger allowance passed\n'
