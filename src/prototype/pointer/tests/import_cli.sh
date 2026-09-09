#!/usr/bin/env bash
set -euo pipefail
binary=$1
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
printf '%s\n' 'id:=&(\A:@ => \x:A => x); id::(A:@)->A->A;' > "$directory/provider.p"
printf '%s\n' 'import id; import id; main:=id;' > "$directory/client.p"
"$binary" --imports "$directory/provider.p" --save "$directory/client.a" "$directory/client.p" > "$directory/status"
grep -q '^done steps=' "$directory/status"
"$binary" --load "$directory/client.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
expect() {
	local expected=$1
	shift
	local code=0
	"$binary" "$@" > "$directory/status" 2>&1 || code=$?
	test "$code" = "$expected"
}
expect 3 --steps 0 --imports "$directory/provider.p" --save "$directory/pending.a" "$directory/client.p"
grep -q '^pending steps=0$' "$directory/status"
"$binary" --load "$directory/pending.a" > "$directory/status"
grep -q '^done steps=' "$directory/status"
printf '%s\n' 'main:=id;' > "$directory/hidden.p"
expect 1 --imports "$directory/provider.p" "$directory/hidden.p"
printf '%s\n' 'import missing;' > "$directory/missing.p"
expect 1 --imports "$directory/provider.p" "$directory/missing.p"
printf '%s\n' 'bad:=missing;' >> "$directory/provider.p"
expect 1 --imports "$directory/provider.p" "$directory/client.p"
grep -q '^rejected steps=' "$directory/status"
expect 2 --imports "$directory/absent.p" "$directory/client.p"
expect 2 --load --imports "$directory/provider.p" "$directory/client.a"
expect 2 --imports "$directory/provider.p" --imports "$directory/provider.p" "$directory/client.p"
printf '%s\n' 'broken:=' > "$directory/syntax.p"
expect 1 --imports "$directory/syntax.p" "$directory/client.p"
grep -q 'syntax.p:[0-9][0-9]*:[0-9][0-9]*:' "$directory/status"
printf '%s\n' 'Bool:=@{false:*; true:*;};' > "$directory/data.p"
printf '%s\n' 'import Bool; main:=Bool.true;' > "$directory/data-client.p"
"$binary" --imports "$directory/data.p" --nf main --save "$directory/data.a" "$directory/data-client.p" > "$directory/direct"
rm "$directory/data.p"
"$binary" --load --nf main "$directory/data.a" > "$directory/restored"
sed '1d' "$directory/direct" > "$directory/direct-value"
sed '1d' "$directory/restored" > "$directory/restored-value"
cmp "$directory/direct-value" "$directory/restored-value"
echo 'import cli: explicit symbols, hidden providers, pending images and whole-provider checking passed'
