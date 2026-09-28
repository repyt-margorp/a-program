#!/usr/bin/env bash
set -euo pipefail
if (( $# < 4 )); then
	printf 'usage: %s BINARY SOURCE.p NEW_OUTPUT_DIRECTORY ordinary|retained [FUELS...]\n' "$0" >&2
	exit 2
fi
binary=$1
source=$2
directory=$3
mode=$4
shift 4
expected=${IMAGE_AUDIT_EXPECT:-0}
case $expected in 0|1) ;; *) exit 2;; esac
options=()
case $mode in ordinary) ;; retained) options+=(--retain-reductions);; *) exit 2;; esac
fuels=(0 "$@")
if (( $# == 0 )); then fuels=(0 1 100 100000); fi
for fuel in "${fuels[@]}"; do [[ $fuel =~ ^(0|[1-9][0-9]{0,14})$ ]] || exit 2; done
mkdir "$directory"
report="$directory/measurements.tsv"
printf 'case\tfuel\treported_steps\texit\tprevious_bytes\tbytes\tdelta\tsha256\tbyte_equal\n' > "$report"
sha256sum "$binary" "$source" > "$directory/inputs.sha256"
failed=0

measure() {
	local name=$1 fuel=$2 input=$3 load=$4 previous=$5
	local previous_bytes=0 equal=na digest steps output
	local args=(--legacy-intrinsic-dot --steps "$fuel" "${options[@]}")
	if [[ $load == yes ]]; then args+=(--load); fi
	if [[ -n $previous ]]; then previous_bytes=$(wc -c < "$previous"); fi
	current="$directory/$name.a"
	code=0
	timeout "${IMAGE_AUDIT_TIMEOUT:-180}" "$binary" "${args[@]}" --save "$current" "$input" \
		> "$directory/$name.stdout" 2> "$directory/$name.stderr" || code=$?
	case $code in
	0|1|3|4) ;;
	*)
		printf '%s\t%s\tna\t%s\t%s\tna\tna\tna\tna\n' \
			"$name" "$fuel" "$code" "$previous_bytes" >> "$report"
		printf '%s: compiler/reader failure %s; see %s.stderr\n' "$name" "$code" "$directory/$name" >&2
		exit 1;;
	esac
	output=$(< "$directory/$name.stdout")
	if [[ ! $output =~ ^(done|pending|rejected|unsupported)' steps='([0-9]+)$ ]]; then
		printf '%s: unexpected output; source/load must not execute host output\n' "$name" >&2
		exit 1
	fi
	steps=${BASH_REMATCH[2]}
	case "$code:${BASH_REMATCH[1]}" in
	0:done|1:rejected|3:pending|4:unsupported) ;;
	*) printf '%s: status disagrees with process exit\n' "$name" >&2; exit 1;;
	esac
	[[ -s $current ]] || exit 1
	if (( steps > fuel )); then
		printf '%s: reported work exceeds supplied fuel\n' "$name" >&2
		failed=1
	fi
	if [[ $fuel == 0 && $code != 3 ]]; then
		printf '%s: zero-fuel input advanced admission\n' "$name" >&2
		failed=1
	fi
	bytes=$(wc -c < "$current")
	digest=$(sha256sum "$current")
	digest=${digest%% *}
	if [[ -n $previous ]]; then
		equal=no
		if cmp -s "$previous" "$current"; then equal=yes; fi
	fi
	printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$fuel" "$steps" "$code" \
		"$previous_bytes" "$bytes" "$((bytes-previous_bytes))" "$digest" "$equal" >> "$report"
	if [[ $load == yes && $fuel == 0 ]]; then
		if [[ $equal != yes || $steps != 0 || $code != 3 ]]; then
			printf '%s: zero-fuel reload changed same-format bytes or advanced admission\n' "$name" >&2
			failed=1
		fi
	fi
}

inert_rounds() {
	local name=$1 input=$2 round
	for round in 1 2 3; do
		measure "$name-zero-$round" 0 "$input" yes "$input"
		input=$current
	done
}

index=0
for fuel in "${fuels[@]}"; do
	measure "source-$index" "$fuel" "$source" no ''
	seed=$current
	seed_code=$code
	seed_bytes=$bytes
	inert_rounds "source-$index" "$seed"
	index=$((index+1))
done

# The last source budget must reach the expected judgement. Cycles add no demand.
if [[ $seed_code != "$expected" ]]; then
	printf 'final source judgement: expected exit %s, got %s\n' "$expected" "$seed_code" >&2
	exit 1
fi
input=$seed
for round in 1 2 3; do
	measure "cycle-$round" "$fuel" "$input" yes "$input"
	if [[ $code != "$seed_code" || $bytes -gt $seed_bytes ]]; then
		printf 'cycle-%s: changed judgement or accumulated data after completed Solve\n' "$round" >&2
		failed=1
	fi
	input=$current
	inert_rounds "cycle-$round" "$input"
done
printf 'fuel/image measurements: %s (failed=%s)\n' "$report" "$failed"
exit "$failed"
