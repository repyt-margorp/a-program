#!/usr/bin/env bash
set -euo pipefail
if (( $# < 4 )); then
	printf 'usage: %s BINARY SOURCE.p NEW_DIRECTORY ordinary|retained [LEFT:RIGHT ...]\n' "$0" >&2
	exit 2
fi
binary=$1 source=$2 directory=$3 mode=$4
shift 4
options=()
case $mode in ordinary) ;; retained) options+=(--retain-reductions);; *) exit 2;; esac
strict=${IMAGE_AUDIT_STRICT_BYTES:-0}
case $strict in 0|1) ;; *) exit 2;; esac
parts=("$@")
completion=0
if (( $# == 0 )); then
	parts=(0:0 0:20 20:0 10:10 1:19 100:100 1000:1000 1600:1600)
	completion=1
fi
for part in "${parts[@]}"; do
	[[ $part =~ ^(0|[1-9][0-9]{0,14}):(0|[1-9][0-9]{0,14})$ ]] || exit 2
done
mkdir "$directory"
sha256sum "$binary" "$source" > "$directory/inputs.sha256"
report="$directory/partitions.tsv"
printf 'partition\tpath\tbudget\tused\tstatus\tbytes\tdelta\tsize_equal\tbyte_equal\tstatus_equal\tfuel_equal\n' > "$report"
failed=0

run() {
	local name=$1 load=$2 input=$3 checkpoint=$4
	shift 4
	local args=(--legacy-intrinsic-dot --steps 0 --repl "${options[@]}")
	if [[ $load == yes ]]; then args+=(--load); fi
	image="$directory/$name.a"
	local first=1 fuel code=0
	{
		for fuel in "$@"; do
			printf ':solve %s\n' "$fuel"
			if [[ $first == 1 && -n $checkpoint ]]; then printf ':save %s\n' "$checkpoint"; fi
			first=0
		done
		printf ':save %s\n:quit\n' "$image"
	} | timeout "${IMAGE_AUDIT_TIMEOUT:-180}" "$binary" "${args[@]}" "$input" \
		> "$directory/$name.stdout" 2> "$directory/$name.stderr" || code=$?
	if (( code != 0 )); then printf '%s: invocation failed (%s)\n' "$name" "$code" >&2; exit 1; fi
	local budgets=(0 "$@") index=0 previous=0 saved=0 line
	first_used=0
	while IFS= read -r line; do
		if [[ $line == saved ]]; then saved=$((saved+1)); continue; fi
		if [[ ! $line =~ ^(done|pending|rejected)' steps='([0-9]+)$ ]]; then
			printf '%s: unexpected status/output\n' "$name" >&2; exit 1
		fi
		status=${BASH_REMATCH[1]} used=${BASH_REMATCH[2]}
		(( index < ${#budgets[@]} )) || exit 1
		if (( used < previous || used-previous > budgets[index] )); then
			printf '%s: counter outside supplied chunk budget\n' "$name" >&2; exit 1
		fi
		if (( index == 1 )); then first_used=$used; fi
		previous=$used
		index=$((index+1))
	done < "$directory/$name.stdout"
	local expected_saves=1
	if [[ -n $checkpoint ]]; then expected_saves=2; fi
	[[ $index == "${#budgets[@]}" && $saved == "$expected_saves" && -s $image ]] || exit 1
	bytes=$(wc -c < "$image")
	sha256sum "$image" >> "$directory/images.sha256"
}

compare() {
	local path=$1 actual=$2 same_size=no same_bytes=no same_status=no same_fuel=no
	if [[ $bytes == "$reference_bytes" ]]; then same_size=yes; fi
	if cmp -s "$image" "$reference"; then same_bytes=yes; fi
	if [[ $status == "$reference_status" ]]; then same_status=yes; fi
	if [[ $actual == "$reference_used" ]]; then same_fuel=yes; fi
	printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$part" "$path" "$total" "$actual" \
		"$status" "$bytes" "$((bytes-reference_bytes))" "$same_size" "$same_bytes" "$same_status" "$same_fuel" >> "$report"
	if [[ $same_size != yes || $status != "$reference_status" || $actual != "$reference_used" ]]; then
		printf '%s %s: size/status/used-fuel mismatch\n' "$part" "$path" >&2
		failed=1
	fi
	if [[ $strict == 1 && $same_bytes != yes ]]; then
		printf '%s %s: byte mismatch\n' "$part" "$path" >&2
		failed=1
	fi
}

# Every path starts from the same zero-fuel image, not source versus restored input.
run seed no "$source" '' 0
seed=$image
if [[ $completion == 1 ]]; then
	# Probe a real terminal boundary; do not assume a fixed example step count.
	terminal_budget=${IMAGE_AUDIT_COMPLETION_BUDGET:-100000}
	[[ $terminal_budget =~ ^[1-9][0-9]{0,14}$ ]] || exit 2
	run completion yes "$seed" '' "$terminal_budget"
	if [[ $status != done && $status != rejected ]]; then
		printf 'completion probe remains pending; set IMAGE_AUDIT_COMPLETION_BUDGET explicitly\n' >&2
		exit 1
	fi
	parts+=("$used:0" "0:$used")
fi
for part in "${parts[@]}"; do
	left=${part%:*} right=${part#*:} total=$((${part%:*}+${part#*:}))
	name=${part/:/-}
	run "$name-single" yes "$seed" '' "$total"
	reference=$image reference_bytes=$bytes reference_status=$status reference_used=$used
	compare single "$used"
	run "$name-memory" yes "$seed" '' "$left" "$right"
	compare memory "$used"
	checkpoint="$directory/$name-checkpoint.a"
	run "$name-save" yes "$seed" "$checkpoint" "$left" "$right"
	before_reload=$first_used
	compare save-without-reload "$used"
	run "$name-reload" yes "$checkpoint" '' "$right"
	compare reload "$((before_reload+used))"
done
printf 'partition/image measurements: %s (failed=%s, strict_bytes=%s)\n' "$report" "$failed" "$strict"
exit "$failed"
