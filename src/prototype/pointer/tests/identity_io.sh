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
