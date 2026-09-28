# Callers own the binaries, budget, temporary directory and comparison image_options.
check() {
	local expected=$1 label=$2 status=0
	shift 2
	timeout 180 "$binary" --legacy-intrinsic-dot --steps "$steps" "$@" > "$directory/status" || status=$?
	printf '%s: ' "$label"
	cat "$directory/status"
	if [[ ! -s $directory/status ]]; then printf 'exit=%s\n' "$status"; fi
	[[ $status == "$expected" ]] || return 1
	case $expected in
		0) grep -q '^done steps=' "$directory/status" ;;
		1) grep -q '^rejected steps=' "$directory/status" ;;
		3) grep -q '^pending steps=' "$directory/status" ;;
	esac
}
equal() {
	timeout 180 "$compare" "${image_options[@]}" --steps "$steps" --equal-image "$@"
}
