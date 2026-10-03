#!/usr/bin/env bash
set -euo pipefail
backend=$(realpath "${1:?a-to-c}")
compiler=$(realpath "${2:?pointer-check}")
if [[ -n ${3:-} ]]; then
	output=$3
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT
	output="$temporary/report"
fi
here=$(cd "$(dirname "$0")" && pwd)
cc=${CC:-cc}
read -r -a flags <<< "${C_BACKEND_CFLAGS:--std=c11 -Wall -Wextra -Werror -O2}"
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
		cat "$output/$name.out" "$output/$name.err" >&2
		printf '%s: expected %s, got %s\n' "$name" "$expected" "$actual" >&2
		exit 1
	fi
}
no_product() {
	test ! -e "$1"
	local paths=("$1".tmp.*)
	test ! -e "${paths[0]}"
}
expect_status admit-scalar 0 "$compiler" --save "$output/scalar.a" "$here/../lower/fixture.p"
expect_status admit-structural 0 "$compiler" --save "$output/structural.a" "$here/../link/fixture.p"
expect_status admit-records 0 "$compiler" --save "$output/records.a" "$here/../record_list/fixture.p"
expect_status admit-numbers 0 "$compiler" --save "$output/applied-list.a" "$here/../applied/fixture.p"
sha256sum "$output/"*.a > "$output/before.sha256"
printf 'aplink 1\nartifact scalar.a\nabi c_scalar_v1\ntarget host-c11\nproduct shared\nlowering scalar_direct_v1\nfallback reject\nexport add add\nexport add64 add64\nexport identity identity\n' > "$output/scalar.aplink"
printf 'aplink 1\nartifact structural.a\nabi isolated_v1\ntarget host-c11\nproduct shared\nexport first first\nexport second second\nexport first duplicate\nexport noop noop\n' > "$output/structural.aplink"
sed 's/product source/product shared/' "$here/../record_list/records.aplink" > "$output/records.aplink"
sed 's/product source/product shared/' "$here/../applied/numbers.aplink" > "$output/numbers.aplink"
for profile in scalar structural records numbers; do
	expect_status "emit-$profile" 0 "$backend" --link "$output/$profile.aplink" "$output/$profile"
	expect_status "symbols-$profile" 0 nm -D --defined-only "$output/$profile/library.so"
	python3 - "$output" "$profile" <<'PY'
import json, pathlib, sys
root, profile = pathlib.Path(sys.argv[1]), sys.argv[2]
receipt = json.loads((root / profile / 'link.json').read_text())
assert receipt['product'] == 'shared' and receipt['entry'] is None
assert receipt['shared_library'] == 'library.so'
assert receipt['shared_visibility'] == 'selected-public-api'
assert receipt['shared_toolchain'] == 'elf-version-script'
expected = {row['symbol'] for row in receipt['exports']}
if profile == 'records':
	expected.update(('ap_arena_Records_destroy', 'ap_from_Records', 'ap_copy_Records', 'ap_from_Reverse', 'ap_copy_Reverse'))
if profile == 'numbers':
	expected.update(('ap_arena_Nat_destroy', 'ap_from_Numbers', 'ap_copy_Numbers'))
actual = {line.split()[-1] for line in (root / f'symbols-{profile}.out').read_text().splitlines()}
assert actual == expected, (profile, sorted(actual), sorted(expected))
(root / f'{profile}-symbols.json').write_text(json.dumps(sorted(actual), indent=2) + '\n')
PY
	options=()
	case "$profile" in
	scalar) options=(-DSCALAR_CLIENT) ;;
	records) options=(-DRECORD_CLIENT) ;;
	numbers) options=(-DNAT_CLIENT) ;;
	esac
	for mode in linked dynamic; do
		inputs=()
		if [[ $mode == dynamic ]]; then
			inputs=(-DDYNAMIC_CLIENT)
		else
			inputs=("$output/$profile/library.so" "-Wl,-rpath,$output/$profile")
		fi
		expect_status "compile-$profile-$mode" 0 "$cc" "${flags[@]}" "${options[@]}" -I"$output/$profile" "$here/client.c" "${inputs[@]}" -ldl -o "$output/client-$profile-$mode"
		expect_status "client-$profile-$mode" 0 "$output/client-$profile-$mode" "$output/$profile/library.so"
		if [[ $profile == structural ]]; then
			printf '42B4242B42B42' > "$output/expected"
			cmp "$output/client-$profile-$mode.out" "$output/expected"
			grep -q 'output error' "$output/client-$profile-$mode.err"
		else
			test ! -s "$output/client-$profile-$mode.out"
		fi
	done
	for policy in repeated trusted; do
		options=()
		[[ $policy == trusted ]] && options=(--trust-image --steps 0)
		expect_status "$profile-$policy" 0 "$backend" "${options[@]}" --link "$output/$profile.aplink" "$output/$profile-$policy"
		for file in component.h symbols.map; do cmp "$output/$profile/$file" "$output/$profile-$policy/$file"; done
		# Ordinary structural admission can select a different checked Core graph.
		if [[ $policy == repeated || $profile != structural ]]; then
			cmp "$output/$profile/component.c" "$output/$profile-$policy/component.c"
		fi
		expect_status "client-$profile-$policy" 0 "$output/client-$profile-dynamic" "$output/$profile-$policy/library.so"
		cmp "$output/client-$profile-dynamic.out" "$output/client-$profile-$policy.out"
	done
	sha256sum "$output/$profile/"* > "$output/$profile-prior.sha256"
	expect_status "$profile-prior" 2 "$backend" --link "$output/$profile.aplink" "$output/$profile"
	sha256sum -c "$output/$profile-prior.sha256" > "$output/$profile-prior-check.out"
done

expect_status compile-fault 0 "$cc" -std=c11 -Wall -Wextra -Werror -O2 -fPIC -shared "$here/fault.c" -ldl -o "$output/fault.so"
for fault in stream close; do
	expect_status "map-$fault" 2 env LD_PRELOAD="$output/fault.so" C_SHARED_FAULT="$fault" "$backend" --link "$output/structural.aplink" "$output/map-$fault"
	no_product "$output/map-$fault"
	if [[ $fault == close ]]; then grep -q 'ferror=0 fclose=-1' "$output/map-$fault.err"; else grep -Eq 'ferror=[1-9][0-9]*' "$output/map-$fault.err"; fi
done
cat > "$output/fail-link" <<'SH'
#!/usr/bin/env bash
for arg in "$@"; do
	if [[ $arg == -shared ]]; then exit 91; fi
done
exec "${C_SHARED_REAL_CC:?}" "$@"
SH
chmod +x "$output/fail-link"
expect_status compile-failure 2 "$backend" --cc /bin/false --link "$output/structural.aplink" "$output/compile-failure"
expect_status link-failure 2 env C_SHARED_REAL_CC="$cc" "$backend" --cc "$output/fail-link" --link "$output/structural.aplink" "$output/link-failure"
no_product "$output/compile-failure"
no_product "$output/link-failure"
for bad in entry layout product; do
	cp "$output/scalar.aplink" "$output/$bad.aplink"
	case "$bad" in
	entry) printf 'entry add\n' >> "$output/$bad.aplink" ;;
	layout) printf 'native_script absent.ld\n' >> "$output/$bad.aplink" ;;
	product) sed -i 's/product shared/product dynamic/' "$output/$bad.aplink" ;;
	esac
	expect_status "bad-$bad" 2 "$backend" --link "$output/$bad.aplink" "$output/bad-$bad"
	no_product "$output/bad-$bad"
done
sed '/^export /d' "$output/scalar.aplink" > "$output/callback.aplink"
printf 'export higher higher\n' >> "$output/callback.aplink"
for trust in checked trusted; do
	options=()
	[[ $trust == trusted ]] && options=(--trust-image --steps 0)
	expect_status "callback-$trust" 4 "$backend" "${options[@]}" --link "$output/callback.aplink" "$output/callback-$trust"
	no_product "$output/callback-$trust"
done
sha256sum "$output/"*.a > "$output/after.sha256"
cmp "$output/before.sha256" "$output/after.sha256"
printf 'Shared C libraries: scalar, record List, applied Nat List and isolated linked/dlopen clients; exact exports, lifetime, transactional failures and staged publication pass\n'
