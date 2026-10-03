# C Backend Epoch10 Handoff

Date: 2026-10-03
Status: bounded ordinary-C client verification terminal; exact freeze for Root.
Branch: `parallel/c-backend-20261003`.
Parent: `60c100c9d06b314eaeb024462e8ce0e3f2da5bab`.
Chosen message: `prototype: verify distinct Nat32 aliases across C modules`.

## Problem List

1. Verify ordinary C composition of two Nat32-selecting components through the
   existing distinct target-alias contract, with explicit refusal and lifetime.

## 1. Ordinary C Modules

### Subjective (User)

2026-10-03, concise English paraphrase of the recorded human priority in the
[owning Goal](2026-10-03-C-BACKEND-GOAL.md): emit readily usable ordinary C modules
from `.a` and LinkerScript, with bounded downstream work and explicit remaining
contracts. Worker task publication is permitted; Merge owns Main integration.
The C backend has no source/producer authority and remains prototype code.

### Objective (Code)

No emitter, producer, `.a` schema, accepted code or build rule changed. Three
separate test/harness files exercise Numbers/Choice C products using existing
`NumbersNat`/`ChoiceNat` aliases. The original reviewed client/check.sh hashes
are unchanged from preparation `f29b9b92`; a small shell launcher records actual
backend/cc/ar argv and exit status. The sole owning issue table records this
bounded advance and pending publication separately from C9 Main integration.

Root's qualified review is `cce2477e3a1098103c3f98fed05c6752365dca76`, producer
`50cd56b2a52b7e370bc7ecb3436c4993e2e5d6a2`, runtime128 manifest SHA256
`9ba2b33cbe8bc68eac338073506bcb205f36d20cb0a34f9f88607d975ef07740`.
Exact input fixture/source pins:

| Git path | SHA256 |
| --- | --- |
| `src/prototype/c_backend/applied/fixture.p` | `e5b33b9aa14afdcd792a65f349463367275bcb4f9c1109f626ee35c2cc7369d1` |
| `src/prototype/c_backend/applied/numbers.aplink` | `3d6a0126bbe53230879e0674907bc8f1a9abf830d151a118fc1e95405b761432` |
| `src/prototype/c_backend/applied/choice.aplink` | `ebe91391cd8b55cb673f35b4af6b2262386338b24597f951daeae3be0c150c62` |

The retained Root binaries independently match committed `core-epoch9.json`:
`a-to-c` SHA256 `6fd5cc956cddf25a3a978a23e488e688a5c14f8d0226b58b14790453952aa362`;
`pointer-check` SHA256 `6e3407fc7f6ba8dffa0642217f00b6d2ec8454e32dc41eac6b27cce30518cd44`.
They are reused without rebuilding. Only the fixture save and Numbers/Choice
default source/object/archive products are reconstituted, all seven commands
exit 0. Only absolute artifact-path and product-mode replacements occur.
The new saved image SHA256 is
`9a40589f1a5c2e1f7838dca83739018621fb24eee48d600688d5fe5df905397b`.
These are fresh results from qualified code; deleted original products cannot
be compared and no byte-equality claim is made about them.

Both authorized serial phases are terminal exit 0: eighteen strict-O2 product/
header-order combinations, then eighteen client ASan/UBSan/leak combinations.
Each client exercises one List `[0,1,2,UINT32_MAX]` and eight magnitude/Bool-tag
Choice pairs. Shared-arena allocation/invalid-enum/active-depth output preservation,
failed-status recovery, prior List readability and either destructor lifetime
pass. Products/header orders repeat observations; they are not independent
properties. Both default-alias negative links fail with the required duplicate
`ap_arena_Nat_destroy` diagnostic and no output object.

Sanitizers instrument clients and source component inputs at O1, with leak
detection and halt-on-error. Driver-generated object/archive bodies and the
compiler/producer remain qualified O2. This is not a fully instrumented compiler
or native object/archive claim. Forty-one input files remain unchanged. All
seventy-four native-tool argv/exit records and complete shell status traces are
retained. Only the two expected duplicate-symbol controls exit 1; no active
failure, harness repair, relaxed outcome or repeated gate was needed.

Retained evidence directory:
`/tmp/a-program-c-backend-alias-qualified-c9-e8-20261003`.

| Evidence | SHA256 |
| --- | --- |
| `qualified-inputs.sha256` | `74235e2a1344fab5ec119eafff183aba235fb724cd10c3c5b820bc6e9c9496b3` |
| `run-files.sha256` | `5171878291b68fdb10afe0196a6f927b6f58a7a0005a9647c82e97f35b0f16dc` |
| `verification.json` | `c442a078f84a28433b31b1ced9153ac35c80a8d5646956cbad068651300c50f4` |
| `o2.log` | `fb5f5d90c63b71499dc2bd9410c3943e4cbb5e3588ebe75ee896d84a7f20827f` |
| `san.log` | `79685d3c570957f0174ec820c071e941b9993021c2504f32161e734bc1c16c30` |

`reconstitution.json` pins original Git blobs, exact binary/source hashes, every
reconstitution command/exit and regenerated products. `alias-commands.json`,
`commands/` and phase traces pin actual client/native-tool commands/exits; the
run manifest pins every source/header/object/archive/client binary and log.
No broader family/slice/refusal, thirteen-gate rerun, producer rebuild, E9/E10
composition or timing was executed. Earlier missing-input/preparation evidence
remains historical in the [alias note](2026-10-03-C-BACKEND-ABI-ALIAS-NOTE.md).

### Assessment

Root operational decisions: 11:15 UTC authorizes the reviewed focused client
matrix; 11:26 UTC authorizes only bounded reconstitution because qualification
cleanup deleted its products. Pinning, exact freeze and Root-owned publication
are coordinator workflow, not new human source-design requirements.

Agent conclusion: existing explicit distinct aliases solve the demonstrated
public helper collision and allow ordinary source/object/archive composition
with the common arena ABI. No new namespace, receipt registry, producer field
or source nominal equality is needed. This epoch verifies an existing target
capability; it does not add a native lowering feature or finish #44/#49.
Native indexed/callable Acc/QuickSort, broader Identity/callback/effect contracts,
general shared nominal exchange and accepted C promotion remain separate.

Exact six-file freeze, with hashes in `epoch10-files.sha256` under the evidence
directory:

```text
doc/2026-10-03-C-BACKEND-GOAL.md
doc/2026-10-03-C-BACKEND-ABI-ALIAS-NOTE.md
doc/2026-10-03-C-BACKEND-EPOCH10-HANDOFF.md
src/prototype/c_backend/scratch/natural_modules/client.c
src/prototype/c_backend/scratch/natural_modules/check.sh
src/prototype/c_backend/scratch/natural_modules/record_tool.sh
```

### Plan

- [x] Reconstitute only authorized exact qualified inputs and pin fresh outputs.
- [x] Verify the unchanged focused matrix and retained refusal/resource controls.
- [x] Check shell syntax, tabs, English comments/docs and exact input hashes.
- [x] Freeze six source/test/docs paths and send one short outbox ready notice.
- [ ] Root publishes exact task-branch bytes and reviews any integration needs;
  worker does not mutate shared Git metadata or operate on Main.
- [ ] Continue the existing Goal after release; all remaining heavy work stays
  held until scheduled authorization. No full Goal/issue closure or promotion.
