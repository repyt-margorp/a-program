#!/usr/bin/env bash
set -uo pipefail
case ${0##*/} in
a-to-c) binary=${C_CLIENT_REAL_BACKEND:?qualified backend} ;;
cc) binary=${C_CLIENT_REAL_CC:?host C compiler} ;;
ar) binary=${C_CLIENT_REAL_AR:?host archiver} ;;
*) printf 'unknown client recording tool\n' >&2; exit 2 ;;
esac
record=$(mktemp "${C_CLIENT_TRACE_DIRECTORY:?command directory}/tool.XXXXXX") || exit 2
printf 'argv: ' > "$record"
printf '%q ' "$binary" "$@" >> "$record"
printf '\n' >> "$record"
status=0
"$binary" "$@" || status=$?
printf 'exit: %s\n' "$status" >> "$record"
exit "$status"
