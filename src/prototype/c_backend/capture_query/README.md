# Current Native Capture Discovery

The owned scalar lowerer now uses existing `pg_term_independent` to discover free
lexical captures, preserving its error result and the target's stable order/
delayed/recursive dependency queue. The accepted query is a readonly syntax walk
with no normalization or typing certificate. No producer support fields, cached
trie, new source walker/checker/IR/schema/.a or authority are introduced.

`fault_test.c` emits the existing transitive-capture fixture normally, then injects
an error at each capture query. Every fault must refuse before writing either
source/header stream and preserve input graph/typing owner counts. Source eval/
WHNF/substitution/typed-query advancement remains forbidden during target emission.

Current accepted `--save` preserves recomputable inputs, so trusted export tests
must explicitly choose existing `--save-materialized`. The test-only adapter
records transformed argv and changes only that save flag; admission/run inputs
and expected target/refusal behavior are unchanged. It does not alter a producer
or pretend an inputs-only image has completed exports. Both profiles and initial
pending trusted result are retained in the C46 evidence.

Affected static/transitive/recursive/nested capture and ordinary source/object/
archive clients retain their supported and unsupported contracts. The actual Acc
candidate remains source-specific executable CODE; the main native backend still
refuses indexed/callable Acc. Existing action/capture correspondence, general
Identity/effects/dynamic closures, accepted adoption/cost/full #61/Goal completion
remain separate. The correction restores current compilation, not generalized
lowering. Historical C32-C45 snapshots remain immutable.
