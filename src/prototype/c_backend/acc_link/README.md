# Actual Acc QuickSort Candidate LinkerScript

This opt-in target command selects the executable actual Acc candidate through
the repository's LinkerScript parser. It is not generalized native lowering.
Its C54 gs_sort signature/storage/roles/manual array adapter remain unchanged;
Acc/down/index/partition/capture bodies and creation recipes come from the
invocation's admitted saved image through the C55 driver.

Use actual.aplink with artifact pointing to the qualified actual fixture image:

```
aplink 1
artifact "actual image.a"
abi c_acc_candidate_v1
target host-c11
product archive
lowering acc_creation_candidate_v1
fallback reject
export outer_fn gs_sort
export succ_access successor
```

Run python3 compose.py SCRIPT.aplink NEW_DIRECTORY --plan-driver
BUILD/c-acc-link-plan --driver QUALIFIED/c-acc-create-image. Add --cc TOOL,
--ar TOOL, --cflag=-O2 as needed. Build the parse-only helper with build.mk and
the same qualified producer source tree as the image driver. Product source,
object or archive yields component.c, component.h, provenance.json, product.json
and the requested component.o/library.a. An ordinary C client includes
component.h and links the source, object or archive; gs_sort is the sole public
definition. It accepts borrowed Nat32 input and transactional output with the
existing private trace. Read the header for runtime statuses and lifetimes.

Artifact paths are relative to the script, including quoted paths with spaces,
using link/plan.c. Export order is irrelevant. outer_fn must map to gs_sort;
the other source export maps to successor. The latter may select an exact source
alias, but admission/body/recipe checks still refuse a different implementation.
The remaining eight type/twelve body roles retain the C55 fixture names; this
profile does not promise arbitrary bindings, relation/algorithm substitution,
source erasure or .a extension. Entry/native_script/type-layout directives and
executable/shared products are unsupported for the candidate command. Trust
mode is absent; --steps is explicitly the existing per-selector allowance,
not the ordinary backend's cumulative budget. Unsupported well-formed plans
return4, malformed schema/read/publication failures2, pending admission3,
source rejection/compiler refusal1. The ordinary a-to-c and direct publication
API explicitly refuse this profile rather than infer native/scalar success.

The no-replace Linux directory operation retains atomic visibility/cleanup and
prior-output preservation; it does not promise crash durability. Receipt hashes
pin the declarative script, image, parser, emitter, all eight fresh emitted
inputs and product outputs before publication. Native indexed/callable Acc,
full Scope/source equivalence/generalized bindings/full #61 remain open.
Immediate-prior checking requires a valid borrowed entire immutable graph.
Manual target storage/roles/closures/array interpretation, Nat32/depth256/
node65536/nonoverlap/private-token lifetime/finite SAN limits remain explicit.
No accepted adoption or performance cost is measured.

check.py PLAN_DRIVER BACKEND IMAGE_DRIVER FAULT_DRIVER IMAGE POINTER CORE_CASES
NEW_RESULTS runs focused serial products/client/source/Core/resource/refusal/
I-O/cleanup controls. CLIENT_CFLAGS selects O2 or whole affected generated-source/
client SAN; build the parser/backend with matching flags. Qualified C55 image
drivers are reused, not regenerated or inferred from the new plan alone.
