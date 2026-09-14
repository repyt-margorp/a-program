#!/usr/bin/env bash
set -euo pipefail
binary=$1
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

invoke() {
	local expected=$1
	shift
	local status=0
	"$binary" "$@" > "$directory/out" 2> "$directory/err" || status=$?
	if [[ $status != "$expected" ]]; then
		cat "$directory/out" "$directory/err" >&2
		printf 'execution: expected status %s, got %s\n' "$expected" "$status" >&2
		exit 1
	fi
}

printf 'second:=\\x:#Text=>\\y:#Text=>y; partial:=second (#print #"a"); main:={f:=partial;#print #"between";f (#print #"b");}; unused:={f:=partial;#"done";}; shared:={f:=partial;f (#print #"b");f (#print #"c");}; repeated:={partial;partial;#"done";}; quoted:={&{#print #"hidden";};}; function:=\\x:#Text=>x;\n' > "$directory/source.p"
for steps in 0 100000; do
	if [[ $steps == 0 ]]; then expected=3; else expected=0; fi
	invoke "$expected" --steps "$steps" --save "$directory/image-$steps.a" "$directory/source.p"
	for mode in source image; do
		input=("$directory/source.p")
		if [[ $mode == image ]]; then input=(--load "$directory/image-$steps.a"); fi
		for pair in main:abetweenb unused:a shared:abc repeated:aa quoted:; do
			name=${pair%%:*}
			printf '%s' "${pair#*:}" > "$directory/expected"
			invoke 0 --run "$name" "${input[@]}"
			cmp "$directory/out" "$directory/expected"
			grep -q 'run: done steps=' "$directory/err"
		done
		invoke 4 --run function "${input[@]}"
		test ! -s "$directory/out"
		invoke 3 --run main --run-steps 0 "${input[@]}"
		test ! -s "$directory/out"
	done
done

# A fresh run of the same image is not a previously executed receipt.
for attempt in 1 2; do
	invoke 0 --run main --load --save "$directory/resaved.a" "$directory/image-100000.a"
	printf 'abetweenb' > "$directory/expected"
	cmp "$directory/out" "$directory/expected"
done
invoke 0 --run main --load "$directory/resaved.a"
cmp "$directory/out" "$directory/expected"

# Byte-exact output, including NUL and no implicit newline; no escape decoding.
printf 'main:=#print #"a\0b";\n' > "$directory/bytes.p"
printf 'a\0b' > "$directory/expected"
invoke 0 --run main --save "$directory/bytes.a" "$directory/bytes.p"
cmp "$directory/out" "$directory/expected"
invoke 0 --run main --load "$directory/bytes.a"
cmp "$directory/out" "$directory/expected"

# Arithmetic -> fixed ASCII formatting -> host output uses ordinary Apply/Fold.
printf 'main:=#print (#int_to_text (#int_add #2147483647 #1));\n' > "$directory/decimal.p"
printf '%s' '-2147483648' > "$directory/expected"
invoke 0 --run main --save "$directory/decimal.a" "$directory/decimal.p"
cmp "$directory/out" "$directory/expected"
invoke 0 --run main --load "$directory/decimal.a"
cmp "$directory/out" "$directory/expected"

# Whole-module and imported-sibling rejection happens before any output.
printf 'main:=#print #"forbidden"; bad:=#int_add #"wrong" #1;\n' > "$directory/bad.p"
invoke 1 --run main "$directory/bad.p"
test ! -s "$directory/out"
printf 'import main;\n' > "$directory/client.p"
invoke 1 --run main --imports "$directory/bad.p" "$directory/client.p"
test ! -s "$directory/out"
invoke 3 --steps 0 --run main "$directory/bytes.p"
test ! -s "$directory/out"
invoke 2 --run main --nf main "$directory/bytes.p"
invoke 2 --run-steps 1 "$directory/bytes.p"
invoke 2 --run main --run-steps -1 "$directory/bytes.p"

# Literal values can be returning entries; raw Pi requires an argument.
printf 'main:=#42;\n' > "$directory/value.p"
invoke 0 --run main "$directory/value.p"
test ! -s "$directory/out"

# Buffered write failure must be reported at the invocation, not lost on exit.
if [[ -c /dev/full ]]; then
	status=0
	"$binary" --run main "$directory/bytes.p" > /dev/full 2> "$directory/err" || status=$?
	test "$status" = 2
	grep -q 'output error (not retried)' "$directory/err"
fi
printf 'execution CLI: source/image runs, exact bytes, ordering, checked entry and I/O failure passed\n'
