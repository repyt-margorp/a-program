# Stateless Evaluator Frame Reuse, MEM1 v2

Prototype review epoch on qualified E9+E10 source128 `73fa86c9`. The two runtime
patches recycle stateless demand frames after callback return and preserve every
frame with non-NULL callback state. Return2 retains the active frame. No codec,
checking policy, Job owner or accepted implementation is changed here.

Full O2 384/0, sanitizer38/45, focused23/cross140 and ten callback controls pass.
The original strict3 remain; public52/history28 images match the parent exactly.
V1's inline-state lifetime failures remain separate despite its passing suite.

The selected180M tree cut saves872644608 bytes of net arena capacity. Existing
allocation audit removes11363962 frame requests, net2727318176 cumulative aligned
bytes including32704 added job layout bytes. Small list adds368 aligned bytes.
These results establish neither peak RSS nor speed; matched measurement needs a
new exclusive slot. Public callback self-destruction is not explicitly specified
by the header; the pinned review distinguishes current owners from arbitrary
custom callbacks. Rebuild users of embedded evaluator structs with the new
header: pg_eval grows8 bytes and aligned WHNF jobs grow16 bytes on this ABI.

See summary.json, manifest.sha256 and the separately frozen evidence manifest.
Chosen task message: `prototype: recycle stateless evaluator demand frames`.
Task branch: `parallel/performance-20261003`. Merge owns integration/promotion.
