# Function Graph binder prototype

This lane implements Issue #57 over the committed `solver_inputs` overlay at
`2d747ccfec844e8afc72d408385ceb79a7c01808`. It changes source parsing, lexical
scans, source-case selection and unresolved syntax transport. Core checking,
graph generation and solver ownership remain the existing implementations.

The user authorized this prototype and its verification. The following brace,
ordering and compatibility choices are agent prototype decisions for review;
they are not approved production language policy. See the
[active SOAP plan](../../../doc/2026-10-03-SURFACE-GOAL.md).

```ap
graph
	@cons {{
		t := tail;
		recursive := tailLength;
	}} => Nat.succ *recursive
```

The LHS introduces a local branch binder. The RHS addresses source-origin
metadata in this function graph and constructor case. It is not an ordinary
lexical expression or a global record label. `name;` abbreviates `name := name;`.
One origin may expose a value, `@local` graph witness and `*local` IH, according
to its existing checked roles. A local shadow introduces a new binder and
cannot inherit another binder's graph or IH association.

| Form | Prototype contract |
| --- | --- |
| `@case a b c => body` | Existing positional syntax |
| `@case { local := source; ... } => body` | Nonempty named list; selected canonical field ordinals strictly increase |
| `@case {{ local := source; ... }} => body` | Nonempty named set; textual order is ignored |

Both named forms preserve the entire canonical dependent telescope, including
omitted fields and IH construction. Ordered lists are subsequences, not full
enumerations. Unknown or multiply selected origins and duplicate local binders
reject. Selectors are unsigiled identifiers; roles appear in the body. Mixing
named and positional entries, empty named forms and mismatched braces reject.
Ordinary computation blocks and program definition blocks keep their grammar.

The ordered restriction is a deliberate additional compatibility cost; old
single-brace permutations must migrate to double braces. Canonical field order
and source-origin spelling are proof-facing interfaces. Renaming source pattern
or recursive-result binders can require changing named proofs. No stable proof
label mechanism is introduced by this correction.

Compilation always reads `local := source`, including when both names are valid
source selectors. It never tries the reverse interpretation. Legacy source
migration is explicit and token-based:

```sh
surface_migrate --legacy-source OLD.p > NEW.p
```

The tool swaps identifier tokens within parsed named clauses and doubles their
braces. It preserves comments, strings, identity shorthand and all other bytes.
It rejects a second migration of already doubled forms. Its explicit flag is
an assertion that the input uses the historical contract; applying it to a new
single-brace source would reverse that source, so use it only on known legacy
inputs. Normal compilation does not invoke it.

`migration.tsv` pins all eight active `.p` consumers and their 13 named clauses
to old/new SHA-256 hashes. Their replacement files live under `migrated/` and
are copied together into a private overlay; accepted fixtures remain untouched.
The parser test's two inline legacy samples are migrated with a separate test
patch. Historical audits and frozen legacy fixtures remain unchanged. The
compatibility runner already classifies its historical named-case fixture as
rejected; it gains no implicit opposite-direction mode.

The syntax payload is `APGSYN\2` and includes an explicit clause mode. `APGSYN\1`
payloads reject, including old program images containing that payload. This
conservative policy also rejects old images without named clauses; rebuild from
source. Semantic graph identity and owner-relative metadata are unchanged. This
is not an exact saved-Solve-resume repair: source reload uses the overlay's
ordinary rechecking policy and existing Core checkpoint limits still apply.

Reproduce in a new private directory:

```sh
ARTIFACT_SOURCE="$PWD/src" bash src/prototype/surface/overlay.sh /tmp/ap-surface-review
make -j2 -f src/prototype/surface/build.mk \
	OVERLAY=/tmp/ap-surface-review BUILD=/tmp/ap-surface-review/build check-surface
```

`test.c` compares accepted terms and classifiers before normalization or closed
application across brace modes, permutations, positional patterns and hidden
dependencies. It inspects direct induction allocations: four canonical fields
plus one IH. It tests exact syntax bytes, structural mode validation, version
rejection, header-index lexical scanning and unsolved/solved source-image reload
at chunks 1 and 64. `scope_test.c` compiles the actual source module once to test
its internal nested-clause lexical scanner; it does not implement a checker.
`check.sh` covers distinct renames, owner/case and lexical lookup boundaries,
both-valid names, roles, legal shadowing, invalid shadowed roles, malformed braces,
wrong proofs and rejected-input reload. `check_migration.sh` verifies the pinned
migration and token fidelity. Detailed current results are recorded by the
epoch handoff; a focused pass is not a full acceptance/checkpoint/sanitizer pass.

The three audits were critically read. The fresh current-head review confirms
the inherited polarity and identifies scope/persistence consumers, without
claiming unsound proof acceptance. This prototype adopts its no-heuristic and
canonical-telescope recommendations. It prototypes the supplied brace proposal
while explicitly preserving lexical shadowing, which current code allows.
Selector-specific compiler diagnostics are deferred: this lane preserves the
existing rejection reporting instead of adding a new Job diagnostic contract.
The performance report's other revision, absent products, adapted tests and
historical timings do not establish current performance; no optimization or
checking change is adopted here.
