#!/usr/bin/env bash
set -euo pipefail
binary=${1:?usage: metrics.sh METRICS_BINARY FUEL_REPORT_DIRECTORY}
directory=${2:?usage: metrics.sh METRICS_BINARY FUEL_REPORT_DIRECTORY}
[[ -f $directory/measurements.tsv ]]
for file in "$directory"/*.a; do
	"$binary" "$file" > "$file.metrics"
done
for file in "$directory"/*-zero-[123].a; do
	base=${file%-zero-*}.a
	cmp "$base.metrics" "$file.metrics"
done
# Parse-only images must not acquire a typed result by observation.
grep -Eq '^typed roots=0 nodes=0 bytes=[0-9]+$' "$directory/source-0.a.metrics"
printf 'semantic metrics: inert counts and bytes agree for every zero-fuel generation\n'
