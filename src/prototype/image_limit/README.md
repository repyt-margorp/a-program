# Explicit Image Reader Bound

## Problem List

1. F3's full retained sorting image needs a configurable reader bound without
   accepting invalid typing evidence when the allowance increases.

## Subjective (User)

Continue F3/F4 and verify saved/resumed programs without weakening independent
synthesis, assertion-only `::`, or the common source-Solve authority. Experimental
changes stay under `src/prototype/` until explicitly accepted for promotion.

## Objective (Code)

Baseline `cf1081e`. `main.c` passes the literal 1,000,000 to `pg_sources_read`.
The API already accepts a caller-selected bound. Its consumers check table
counts, reference storage and representable allocation sizes; this is neither
a total-memory limit nor a Solve-step limit. The previous prototype gate failed
at retained-image load, while the same image passed the typed reader API at
3,000,000. Ordinary source checking and RECOMPUTE images already passed.

## Assessment

Agent implementation decision: add `--image-limit N` only for `--load`, leaving
the default and all reader validation unchanged. Accept a positive representable
integer, reject duplicate/missing/invalid arguments and do not interpret zero
as unlimited. The bound is invocation-local, not saved authority or proof fuel.
Retained data still enters ordinary source Solve; no new admission path exists.

On load failure, report the configured bound without claiming that it caused
the failure. The same API can fail for invalid input or allocation failure.
Do not silently increase the default, truncate retained records or retry with
an unbounded reader. The `.a` format and writer are unchanged.

## Plan

Adoption is on hold after the user's 2026-09-28 review. The
[growth audit](../../../doc/2026-09-28-RETAINED-IMAGE-GROWTH-AUDIT.md) shows
retained history accumulation across completed Solve/save generations, eventually
exceeding even 3M. The passing tests below do not resolve that ownership issue.
The [active artifact plan](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md)
first specifies required semantic content, then removes unused history. It does
not build a cache consumer to justify the current archive. Promotion of this
option is not an F5 requirement; preserve its meaningful input-validation tests
where applicable during the image migration.

- [x] Isolated CLI patch, composed with the verified readback/conversion candidate.
- [x] Parsing, default/explicit agreement, insufficient/representation bounds,
  stdin/root/NF/REPL checks and ordinary/retained 0/1/completed-image resume.
- [x] Preserve bytes on zero-step retained rewrites and reject an invalid
  post-check after resume; focused O2 and ASan/UBSan pass.
- [x] Quick/Insertion full `all` at an explicit 3-million bound: O2 **151 s**.
- [x] Full-sized rejected retained image: default bound fails, explicit bound
  loads but rejects at **5,948,767** steps, including byte-identical zero-step
  rewrite and resume. O2 and ASan/UBSan pass.
- [x] Quick/Insertion full `all` under ASan/UBSan (**482 s**, leak checking and
  halt-on-error enabled); this is not the sanitized full compiler suite.
- [x] Aggregate five-backend gate exits 0; the large rejected-image gate above
  ran separately, before its addition to the aggregate prerequisites.
- [x] Full compiler `check-acceptance` exits 0, including 63/63 source
  compatibility, four LT/partition variants, Local/Strong, Fin/Vec, images,
  witness isolation and the readback/conversion/CLI prototype tests.
- [ ] Promote only after approval; add the accepted-build F5 targets then.

```sh
bash src/prototype/image_limit/overlay.sh /tmp/a-program-image-limit
make -f src/prototype/image_limit/build.mk BUILD=/tmp/a-program-image-limit/build check-image-limit
make -f src/prototype/image_limit/build.mk BUILD=/tmp/a-program-image-limit/build check-sorting-backends
make -f src/prototype/image_limit/build.mk BUILD=/tmp/a-program-image-limit/build check-acceptance
```

`check-sorting-backends` reuses the existing typed comparator and source suites.
It includes Quick/Insertion, concrete value transport, legacy Nat MergeSort,
TreeSort, transitive BubbleSort with its nontransitive counterexample, and the
same-domain wrong-function regression. It adds no sorting checker or new Eq rule.
`SORTING_IMAGE_LIMIT=3000000` selects the bound for the Quick/Insertion script
when called directly with `finite_sort_image_compare`. Omitting that setting
still exercises the old default; it is not silently raised by the script.

The [active SOAP plan](../../../doc/2026-09-27-FINITE-POSITION-SORTING-SOAP-PLAN.md)
tracks completion, nonclaims, measurements and per-file accounting.
