#!/usr/bin/env bash
set -eu
binary=$1
check() {
	expected=$1
	status=$2
	source=$3
	shift 3
	code=0
	output=$(printf '%s' "$source" | "$binary" "$@" - 2>&1) || code=$?
	if [ "$code" != "$expected" ]; then
		printf 'expected exit %s, got %s: %s\n' "$expected" "$code" "$output" >&2
		exit 1
	fi
	case "$output" in
		"$status"*) ;;
		*) printf 'unexpected output: %s\n' "$output" >&2; exit 1 ;;
	esac
}
check 0 'done steps=' 'id:=&(\A:@ => \x:A => x);' --strict-thunks
check 3 'pending steps=0' 'id:=&(\A:@ => \x:A => x);' --steps 0
check 3 'pending steps=' 'x:=x;' --steps 100
check 1 'rejected steps=' 'x:=missing;'
check 1 '-:1:' 'x:='
check 2 'usage:' '' --steps -1
check 2 'usage:' '' --steps 18446744073709551616
check 2 'usage:' '' --steps 2x
check 2 'usage:' '' --unknown
printf '%s\n' 'cli: bounded source checking, pending, rejection and argument diagnostics passed'
