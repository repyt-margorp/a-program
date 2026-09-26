#!/bin/sh
set -eu
binary=$1
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT HUP INT TERM
"$binary"
"$binary" write "$directory/work"
"$binary" read "$directory/work"
"$binary" write-handlers "$directory/handlers"
"$binary" read-handlers "$directory/handlers"
"$binary" write-machine "$directory/machine"
"$binary" resave-machine "$directory/machine" "$directory/machine-resaved"
"$binary" read-machine "$directory/machine-resaved"
printf '%s\n' 'Machine transport: separate writer, resaver and reader preserve pending work and exact total steps'
for stage in eval readback; do
	"$binary" "write-whnf-$stage" "$directory/whnf-$stage"
	"$binary" resave-whnf "$directory/whnf-$stage" "$directory/whnf-$stage-resaved"
	"$binary" read-whnf "$directory/whnf-$stage-resaved"
done
printf '%s\n' 'WHNF transport: separate processes preserve deferred evaluation and final readback without recomputation'
