#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "$1")
image=$(realpath "$2")
native=$(realpath "$3")
output=$4
immediate_status=${5:-2}
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
test -c /dev/full
test ! -e "$output"
mkdir -p "$output"
output=$(realpath "$output")

expect_status() {
	local name=$1 expected=$2 actual=0
	shift 2
	printf '%q ' "$@" > "$output/$name.command"
	printf '\n' >> "$output/$name.command"
	"$@" > "$output/$name.out" 2> "$output/$name.err" || actual=$?
	printf '%s\t%s\t%s\n' "$name" "$expected" "$actual" >> "$output/status.tsv"
	if [[ $actual != "$expected" ]]; then
		cat "$output/$name.err" >&2
		printf '%s: expected %s, got %s\n' "$name" "$expected" "$actual" >&2
		exit 1
	fi
}

no_product() {
	test ! -e "$1"
	local paths=("$1".tmp.*)
	test ! -e "${paths[0]}"
}

fault() {
	local name=$1 expected=$2 mode=$3
	shift 3
	expect_status "$name" "$expected" env LD_PRELOAD="$output/fault.so" C_PUBLICATION_FAULT="$mode" "$backend" "$@"
	grep -q 'publication test: .* /dev/full stream' "$output/$name.err"
	if [[ $mode == *-close ]]; then
		grep -q 'publication test: ferror=0 fclose=-1' "$output/$name.err"
	else
		grep -Eq 'publication test: ferror=[1-9][0-9]* fclose=' "$output/$name.err"
	fi
}

expect_status shim 0 "$cc" -std=c11 -Wall -Wextra -Werror -O2 -fPIC -shared "$here/publication_io_fault.c" -ldl -o "$output/fault.so"
cp "$native" "$output/native.aplink"
printf 'aplink 1\nartifact "%s"\nabi isolated_v1\ntarget host-c11\nproduct source\nexport empty empty\n' "$image" > "$output/structural.aplink"
sed '/^export /d' "$native" > "$output/unsupported.aplink"
printf 'export callback callback\n' >> "$output/unsupported.aplink"
for profile in native structural; do
	expect_status "$profile-good" 0 "$backend" --link "$output/$profile.aplink" "$output/$profile-good"
	for mode in source header source-close header-close receipt; do
		expected=$immediate_status
		[[ $mode == *-close || $mode == receipt ]] && expected=2
		fault "$profile-$mode" "$expected" "$mode" --link "$output/$profile.aplink" "$output/$profile-$mode"
		no_product "$output/$profile-$mode"
	done
done
for mode in direct direct-close; do
	printf 'prior output canary\n' > "$output/$mode.c"
	cp "$output/$mode.c" "$output/$mode-prior.c"
	expected=$immediate_status
	[[ $mode == *-close ]] && expected=2
	fault "$mode" "$expected" "$mode" "$image" empty "$output/$mode.c"
	cmp "$output/$mode.c" "$output/$mode-prior.c"
	paths=("$output/$mode.c".tmp.*)
	test ! -e "${paths[0]}"
done
expect_status direct-good 0 "$backend" "$image" empty "$output/direct-good.c"
for trust in checked trusted; do
	options=()
	[[ $trust == trusted ]] && options=(--trust-image --steps 0)
	expect_status "unsupported-$trust" 4 "$backend" "${options[@]}" --link "$output/unsupported.aplink" "$output/unsupported-$trust"
	no_product "$output/unsupported-$trust"
done
sha256sum "$output/native-good/"* > "$output/prior-product.sha256"
expect_status prior-product 2 "$backend" --link "$output/native.aplink" "$output/native-good"
sha256sum -c "$output/prior-product.sha256" > "$output/prior-product-check.out"
printf 'publication I/O, unsupported status and atomic preservation controls pass\n'
