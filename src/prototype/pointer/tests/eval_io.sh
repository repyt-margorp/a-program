#!/bin/sh
set -eu
binary=$1
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT HUP INT TERM
"$binary"
"$binary" write "$directory/configuration"
"$binary" read "$directory/configuration"
"$binary" write-substitution "$directory/substitution"
"$binary" read-substitution "$directory/substitution"
"$binary" write-materialization "$directory/materialization"
"$binary" read-materialization "$directory/materialization"
