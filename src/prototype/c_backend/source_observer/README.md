# C source owner observer

This target-side diagnostic snapshots the borrowed source graph and typing
containers, every selected index's bucket membership/header, and complete
Context/Scope/Occurrence/map payloads including trailing typed inputs. Native
admission of an existing tuple changes its payload even when owner counts stay
fixed. On the legacy producer, the whole typing-container snapshot still captures
the receipt index/count. There is no reference to the removed `typing.proofs`
member and no duplicate producer bookkeeping.

The snapshot allocates only private C diagnostic storage. It does not admit a
subject, run a checker, infer semantics or serialize a new source field. Source
owners must remain alive until the snapshot is destroyed. Container comparison
precedes bucket reads so changed index storage is detected before reading an old
array. This is an emission-inertness assertion, not a certificate of arbitrary
payload safety, source equivalence or full heap immutability.

The native control interns an empty Scope before admission, then uses the existing
public empty-context rule. Descriptive counts stay fixed while the snapshot
detects admission. It also pre-interns a Universe conclusion before ordinary
admission; `--native` checks that every typing-container byte stays fixed while
the observer detects the owner-payload change. Repeated admission leaves it
unchanged; adding a new Core reference changes it. Native and legacy producers
are tested separately. The legacy producer does not intern an empty Scope; its
empty-context admission changes the receipt store, which remains observed.
The command and recipe helper keep their source advancement interposition guards.
