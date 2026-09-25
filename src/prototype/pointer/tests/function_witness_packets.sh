#!/usr/bin/env bash
set -euo pipefail

compare=$1
root=$(dirname "${BASH_SOURCE[0]}")
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT

# The table preserves producer coverage removed from public *f examples.
# All aliases below are ordinary source values; only the C test calls the
# optional producer. Source property/graph consumers stay in their own tests.
while IFS='|' read -r file function expected expressions; do
	cat "$root/acceptance/$file.p" > "$directory/input.p"
	printf '\npacket_expected := %s;\n' "$expected" >> "$directory/input.p"
	IFS='|' read -ra values <<< "$expressions"
	arguments=()
	for i in "${!values[@]}"; do
		printf 'packet_arg_%s := %s;\n' "$i" "${values[$i]}" >> "$directory/input.p"
		name="packet_arg_$i"
		if [[ ${values[$i]} == '&'* ]]; then name="&$name"; fi
		arguments+=("$name")
	done
	printf 'packet fixture: %s / %s\n' "$file" "$function"
	"$compare" --packet "$directory/input.p" "$function" packet_expected "${arguments[@]}"
done <<'CASES'
generated-function-graph|length|expected|one
generated-function-graph|alias|Nat.zero|NatList.nil
generated-function-graph-direct|first|expected|expected|other
length-output-proof|length|expected|input
length-output-proof|length|emptyExpected|NatList.nil
function-graph-branch-name-collision|length|expected|sample
function-graph-branch-tree|length|expected|sample
function-graph-branch-tree|length|zero|List.nil
function-graph-branch-tree|prefix|expected|sample
function-graph-branch-tree|after|expected|sample
function-graph-branch-tree|nested|three|nestedInput
function-graph-branch-tree|direct|expected|sample
function-graph-branch-tree|walk|expected|tree
function-graph-branch-tree|countIndexed|expected|expected|indexed
function-graph-branch-tree|lastSize|one|Root.root Bool.true|expected|vector|&(\v:Vec expected => Nat.zero)
function-graph-call-sites|twice|two|two
function-graph-call-sites|cutoff|Nat.zero|two
function-graph-call-sites|rightOnly|Tree.leaf|sample
function-graph-call-sites|orderedMirror|orderedExpected|sample
function-graph-callable-parameter|apply|two|&Nat.succ|one
function-graph-callable-parameter|map|expected|&Nat.succ|sample
function-graph-callable-parameter|filter|filterExpected|&isZero|sample
function-graph-callable-parameter|filter|sample|&always|sample
function-graph-callable-parameter|filter|noneExpected|&never|sample
function-graph-captured-match|choose|one|Bool.yes|Box.mk one
function-graph-captured-match|choose|two|Bool.no|Box.mk one
function-graph-curried|append|expected|Nat|left|right
function-graph-curried|append|right|Nat|(List Nat).nil|right
function-graph-curried|grow|two|two|Nat.zero
function-graph-curried|choose|two|two|Nat|two
function-graph-curried|twice|four|two|Nat.zero
function-graph-exposed-match|length|expected|sample
function-graph-exposed-match|quoted|expected|sample
function-graph-exposed-match|sequenced|expected|sample
function-graph-exposed-match|indexed|Nat.succ Nat.zero|Nat|Nat.succ Nat.zero|vector
function-graph-known-match|length|expected|sample
function-graph-known-match|unpack|expected|sample
function-graph-known-match|skip|zero|sample
function-graph-indexed|length|two|Nat|two|sample
function-graph-indexed|length|emptyExpected|Nat|Nat.zero|(Vec Nat).nil
function-graph-indexed|copy|sample|Nat|two|sample
function-graph-indexed|countFrom|two|Nat|two|sample|Nat.zero
function-graph-indexed|constant|one|Nat|two|Point.at Nat two
function-graph-indexed-canonical|length|two|two|sample
function-graph-indexed-canonical|length|emptyExpected|Nat.zero|Vec.nil
function-graph-family-parameters|length|two|two|sample
function-graph-family-parameters|read|two|two|(Related Nat Diagonal).at two (Diagonal.same two)
function-graph-function-field|steps|expected|Nat.zero|sample
function-graph-function-field|from|expected|Nat.zero|sample|Nat.zero
function-graph-function-field|walk|three|tree|Nat.zero
function-graph-function-field|visit|expected|Branch.node &(\k:Nat => \p:SameNat k k => Branch.leaf)
function-graph-helper-call|tailLength|one|sample
function-graph-helper-call|twice|two|sample
function-graph-helper-call|dropAppend|appendExpected|sample|List.cons Nat.zero List.nil
function-graph-helper-call|genericTail|one|Nat|genericSample
function-graph-helper-call|lengthAgain|two|sample
function-graph-helper-call|lengthAgain|zero|List.nil
function-graph-partial-source|lengthNat|expected|sample
function-graph-partial-source|alias|expected|sample
function-graph-named-fields|length|expected|two
function-graph-refined-case|length|expected|input
inferred-index-copy|copy|sample|Nat|two|sample
inferred-index-graph|length|two|Nat|two|sample
graph-canonical-leaf-name|f|zero|Input.case1
graph-canonical-leaf-name|f|one|Input.other
graph-duplicate-leaf|f|zero|zero
graph-duplicate-leaf|f|zero|one
graph-duplicate-leaf|f|one|two
graph-helper-leaf|f|zero|zero
graph-helper-leaf|f|zero|one
graph-helper-leaf|f|one|two
graph-comparison-leaves|natLessOrEqual|Bool.true|zero|two
graph-comparison-leaves|natLessOrEqual|Bool.false|two|one
graph-comparison-leaves|natLessOrEqual|Bool.true|one|two
graph-comparison-leaves|natLessOrEqual|Bool.true|two|two
CASES
printf '%s\n' 'optional witness packets: migrated producer cases passed at chunks 1 and 64'
