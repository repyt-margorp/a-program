# Actual Acc saved-image C product command

This command regenerates the seven actual-source Acc QuickSort bodies and the
source map/endpoint/frame/down-action recipe from an admitted materialized `.a` image.
It packages the reviewed fixed candidate as ordinary C source, an object, or an
archive. The algorithm retains actual Acc/down recursion, indices, partition,
append, comparison and captured parameters; it does not substitute a C sorter.
The [readable reviewed module](../acc_recipe/example/component.c) and
[declarative header](../acc_recipe/example/component.h) are exactly the command's
generated C/header. The [actual admitted fixture](../acc_actions/fixture.p) names
the source selections; source algorithm and private target representation remain
distinct in the section provenance.

It is a bounded candidate command. The unchanged packer requires the qualified
body/recipe bytes and retains manual target role/storage/closure interpretation.
Actual source constructor fields, down-domain/result indices and field directions
drive the bounded action recipe. The command builds an intermediate parent from
its own fresh bodies; historical output modules are byte qualifiers only.
It does not change Native LinkerScript admission, implement arbitrary artifact
lowering, invent source erasure, or prove complete checked Scope/source semantics.
The ordinary native Acc refusal remains separate.

Build against an explicitly selected producer tree, serially:

```sh
make -j1 -f src/prototype/c_backend/acc_command/build.mk \
  OVERLAY=/absolute/qualified/tree BUILD=/absolute/owned/build \
  CFLAGS='-std=c11 -Wall -Wextra -Werror -O2' \
  /absolute/owned/build/c-acc-image
```

Use the qualified actual materialized image, not an inputs-only image. The driver
selects the fixture's existing types and source exports through public admission,
using a separate five-million-step budget per selector. It loads the image once;
source graph/typing owners and source advancement stay inert during emission.
The [source observer](../source_observer/README.md) checks owner membership and
native admission payloads as well as counts, retaining the legacy count guard
without requiring the removed receipt store. Native and legacy image formats are
qualified separately; no compatibility claim is made for an old image rejected
by a newer producer. The unchanged fixture uses `--legacy-intrinsic-dot` when
building a fresh image because its provider still contains that syntax.
Public admission itself may advance. The staging directory supplies anonymous
disk temporary streams; set compiler `TMPDIR` to owned disk storage as needed.

```sh
python3 src/prototype/c_backend/acc_command/compose.py \
  /absolute/actual-materialized.a /absolute/new-product \
  --driver /absolute/owned/build/c-acc-image --product archive --cflag=-O2
cc -std=c11 -I/absolute/new-product ordinary_client.c \
  /absolute/new-product/library.a -o ordinary_client
```

`--product source` is the default; `object` supplies `component.o`, and `archive`
supplies `library.a`. Each product includes readable `component.c`, declarative
`component.h`, the unchanged section provenance, and a supplemental `product.json`
with artifact/driver/generated-input/output hashes and child argv/status. Receipt
paths and compiler/driver/mode differ across runs; receipt byte equality is not
claimed. All products expose only the existing `gs_sort` entry.

The complete destination directory must be new. Private sibling staging and Linux
`renameat2(RENAME_NOREPLACE)` publish the set without replacing prior directories
or symlinks. Unsupported hosts refuse without an overwriting fallback. Failed
admission, unsupported bodies, compiler/archive errors or output I/O leave no
requested product; cleanup only removes this invocation's private staging/body
files. This is atomic visibility, not crash-durable storage publication.

Command statuses preserve rejected `1`, I/O `2`, pending `3`, unsupported `4` and
success `0`; compiler failure `1` is reported as such. Argument parser errors are
`2`. Runtime contracts remain Nat32, depth256, node65536, borrowed immutable
lifetimes, nonoverlap and transactional output/status1–6. Finite sanitizer runs
do not prove arbitrary pointer safety or source equivalence.

Focused qualification uses `check.py DRIVER FAULT_DRIVER IMAGE POINTER
CORE_CASES NEW_OUTPUT --recipe-helper HELPER --observer-test OBSERVER`.
Use `--native-owners` for the native admission-only owner-payload control.
Build `c-acc-image-fault` only for these output-open,
copy-write, close and temporary-stream injection controls. `CLIENT_CFLAGS` selects
O2 or whole-generated-source/client sanitizer compilation; matching instrumented
drivers are built separately. Core cases are retained producer-oracle outputs,
not a new hand-authored source oracle. No comparative measurement is performed.
