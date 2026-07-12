# Theory Reading Map for A-Program

Date: 2026-07-12

Status: living bibliography and reading plan

## Purpose

This note records the literature consulted during the design discussions and a
larger reading map for the next design stages. It is not a claim that every
listed theory should be imported into A-Program. Each item is tied to a
specific question that the implementation must answer.

The current project questions are:

1. What belongs to kernel conversion, and what belongs to optimisation?
2. How should graph sharing, typed views, source names, and erasure relate?
3. How should classifier constraints and recursive Match motives be solved?
4. How should indexed inductive families and constructor telescopes be added?
5. What is the boundary between judgemental equality, propositional equality,
   explicit rewriting, and future higher equality?
6. How can a pure normalizer coexist with host intrinsics and effects?
7. How can proved equalities become optional certified optimisation rules?

## Reading Order

Read the following sequence before making large kernel changes:

1. Normalization and algorithmic equality.
2. Elaboration and constraint solving.
3. Dependent pattern matching and proof-relevant unification.
4. Equality, transport, and cubical computation.
5. Erasure and effects.
6. Equality saturation and proof-carrying optimisation.

## Sources Already Used in Discussions

### 1. Homotopy Type Theory: Univalent Foundations of Mathematics

- Authors: The Univalent Foundations Program
- Access: Open Access book
- Link: <https://homotopytypetheory.org/book/>
- A-Program use: distinguish judgemental equality from identity/path types;
  understand `refl`, transport, `J`, equivalence, and univalence.
- Read first: Chapters 1 and 2.
- Do not infer from this book that every path equality should become kernel
  conversion. The distinction between judgemental equality and identity types
  is exactly the relevant point.

### 2. Normalization by Evaluation for Martin-Lof Type Theory

- Authors: Peter Dybjer and Andrzej Filinski
- Access: author-hosted PDF
- Link: <https://www.cse.chalmers.se/~peterd/papers/NbeMLTTEqualityJudgements.pdf>
- A-Program use: algorithmic equality, normalization, and the relation between
  a declarative type theory and a decision procedure for conversion.
- Read for: the rule that kernel equality must be a fixed, decidable
  normalization relation, not an optimiser-selected equivalence class.

### 3. Elaboration in Dependent Type Theory

- Authors: Leonardo de Moura, Jeremy Avigad, Soonho Kong, Cody Roux
- Access: Open Access preprint / repository copy
- Link: <https://kilthub.cmu.edu/articles/journal_contribution/Elaboration_in_Dependent_Type_Theory/6491486>
- A-Program use: separate source expressions, metavariables/constraints, and
  elaborated core terms. This is directly relevant to the planned split of
  `ConstraintDB` from `JudgementDB`.
- Read for: delayed constraints, ambiguous source names, implicit information,
  and elaboration as a phase before kernel checking.

### 4. Unifiers as Equivalences: Proof-Relevant Unification of Dependently Typed Data

- Authors: Jesper Cockx, Dominique Devriese, Frank Piessens
- Venue: ICFP 2016
- Access: author PDF
- Link: <https://jesper.cx/files/unifiers-as-equivalences.pdf>
- A-Program use: indexed constructors, dependent Match, and the principle that
  a unifier should produce correctness evidence rather than silently mutate a
  typing context.
- Read for: the future Motive/constraint solver and later Vec/IADT work.

### 5. Cubical Type Theory: A Constructive Interpretation of the Univalence Axiom

- Authors: Cyril Cohen, Thierry Coquand, Simon Huber, Anders Mortberg
- Venue: TYPES 2015 / LIPIcs 2018
- Access: Open Access
- Link: <https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.TYPES.2015.5>
- A-Program use: understand why cubical equality is not obtained merely by
  adding `Eq`, `refl`, or a graph equality cache. It requires interval
  dimensions and computational rules for paths and universes.
- Read after ordinary intensional equality and transport are understood.

### 6. Irrelevance, Heterogeneous Equality, and Call-by-value Dependent Type Systems

- Authors: Vilhelm Sjöberg, Chris Casinghino, Ki Yung Ahn, Nathan Collins,
  Harley D. Eades III, Peng Fu, Garrin Kimmell, Tim Sheard, Aaron Stump,
  Stephanie Weirich
- Access: arXiv
- Link: <https://arxiv.org/abs/1202.2923>
- A-Program use: type/proof erasure, nontermination, equality, and CBV
  operational semantics. Relevant to deciding which typed-view information is
  runtime relevant.

### 7. egg: Fast and Extensible Equality Saturation

- Authors: Max Willsey, Chandrakana Nandi, Yisu Remy Wang, Oliver Flatt,
  Zachary Tatlock, and others
- Venue: POPL 2021
- Access: author PDF and arXiv
- Links: <https://homes.cs.washington.edu/~cnandi/docs/popl21-cr.pdf>
  and <https://arxiv.org/abs/2004.03082>
- A-Program use: separate proof-validated rewrite rules from extraction/cost
  selection. E-graphs are an optimisation layer, not kernel DefEq.
- Caveat: ordinary e-graphs do not solve binding, dependent typing, or
  transport automatically.

### 8. Foundational Proof-Carrying Code

- Author: Andrew W. Appel
- Venue: LICS 2001
- Access: author PDF
- Link: <https://www.cs.princeton.edu/~appel/papers/fpcc.pdf>
- A-Program use: model for an untrusted optimiser/linker producing artefacts
  checked by a smaller trusted kernel. This is relevant if rewrite certificates
  are later serialized with object artefacts.

## Additional Open Access Reading

### Constraint Solving and Higher-Order Patterns

#### Unification of Simply Typed Lambda-Terms as Logic Programming

- Author: Dale Miller
- Access: Open Access technical report
- Link: <https://www.lfcs.inf.ed.ac.uk/reports/91/ECS-LFCS-91-160/>
- Why: full higher-order unification is not the desired target. This explains
  the difference between arbitrary function metavariables and the restricted
  higher-order pattern fragment.

#### Unification of Higher-Order Patterns in Linear Time and Space

- Author: Zhaohui Qian
- Venue: Journal of Logic and Computation, 1996
- Access: publisher page; the PDF may require subscription or library access
- Link: <https://academic.oup.com/logcom/article-pdf/6/3/315/3804488/6-3-315.pdf>
- Why: the pattern fragment is the plausible formal target for a restricted
  A-Program Motive solver. Do not introduce a general higher-order solver
  before defining which equations the language can generate.

### Dependent Pattern Matching and IADT

#### Eliminating Dependent Pattern Matching without K

- Authors: Jesper Cockx, Dominique Devriese, Frank Piessens
- Venue: Journal of Functional Programming, 2016
- Access: publisher page; author/preprint availability should be checked
- Link: <https://www.cambridge.org/core/journals/journal-of-functional-programming/article/div-classtitleeliminating-dependent-pattern-matching-without-kdiv/4BC4EA2D02D801E5ABED264FE5FB177A>
- Why: dependent pattern matching has metatheoretic consequences for equality.
  Read before adding Vec/IADT matching together with HoTT or cubical equality.

#### Proof-Relevant Unification: Dependent Pattern Matching with Only the Axioms of Your Type Theory

- Authors: Jesper Cockx, Dominique Devriese, Frank Piessens
- Access: publication record; look for an author preprint
- Link: <https://imec-publications.be/entities/publication/6c7d3f77-d540-4f7b-a072-4cc71e22943f>
- Why: follow-up on proof-relevant unification and its interaction with the
  equality theory actually chosen by the language.

### Erasure and Staging

#### Type Theory With Erasure

- Authors: Constantine Theocharis, Edwin Brady
- Date: 2026 preprint
- Access: arXiv
- Link: <https://arxiv.org/abs/2605.00655>
- Why: explicit phase distinction between runtime-relevant and erased terms.
  This is directly relevant to typed views over shared core graphs.

#### A Graded Modal Dependent Type Theory with Erasure, Formalized

- Authors: Andreas Abel, Nils Anders Danielsson, Oskar Eriksson
- Date: 2026 preprint
- Access: arXiv
- Link: <https://arxiv.org/abs/2603.29716>
- Why: a modern formalized account of erasure, normalization, decidability of
  definitional equality, and sound extraction.

#### A Dependent Dependency Calculus

- Access: author/preprint availability should be checked
- Link: <https://www.researchgate.net/publication/358142454_A_Dependent_Dependency_Calculus_Extended_Version>
- Why: separates compile-time and runtime irrelevance. Read critically; the
  design space is relevant even if A-Program adopts a different calculus.

### Effects and Dependent Types

#### A Dependently Typed Language with Delimited Control Operators

- Authors: Youyou Cong, Kenichi Asai
- Venue: Computer Software, 2019
- Access: Open Access article
- Link: <https://www.jstage.jst.go.jp/article/jssst/36/2/36_2_47/_article/-char/en>
- Why: effects and dependent typing require restrictions because dynamic
  behaviour flowing into types threatens static determination. Relevant before
  merging `Perform` with kernel normalization.

## Useful Sources That May Require Subscription or Library Access

These are included because they are historically or technically important.
Access status depends on an institution and may change; search for legal author
preprints before using a publisher copy.

### The Undecidability of Unification in Third Order Logic

- Author: Gerard Huet
- Venue: Information and Control, 1973
- Publisher record: <https://www.sciencedirect.com/science/article/pii/S001999587390301X>
- Why: the classical boundary showing why the project must not silently expand
  its restricted Motive equations into general higher-order unification.

### Intuitionistic Type Theory

- Author: Per Martin-Lof
- Form: book / lecture notes, 1984
- Why: foundational account of judgements, type formation, identity, and
  definitional equality. Often library-only in print editions.
- Reading purpose: establish vocabulary before treating `HasType`, conversion,
  and `Eq` as compiler data.

### Practical Foundations for Programming Languages

- Author: Robert Harper
- Form: book, second edition
- Why: operational semantics, type safety, and language-design methodology.
  It is useful for the distinction between a static typing judgement and a
  dynamic transition system.

### Type Theory and Formal Proof: An Introduction

- Authors: Rob Nederpelt, Herman Geuvers
- Form: book
- Why: a slower route to dependent type theory and the Calculus of
  Constructions than the research literature; useful when reading the papers
  above becomes too compressed.

## A-Program Design Rules Derived from This Reading

The bibliography supports the following current directions.

```text
TermDB:
  shared core calculation graph

TypedView / Binding:
  source name, classifier, proof, namespace, and core graph reference

ConstraintDB:
  unresolved classifier equations, Motive variables, universe constraints,
  and solver provenance

JudgementDB:
  solved and checkable typing derivations only

Kernel conversion:
  fixed pure reduction policy; no user Eq theorem becomes DefEq automatically

Optimizer:
  optional, typed, proof-validated rewrite registry and cost-based extraction

Runtime:
  starts from validated typed roots, then executes an erased/relevant core
  machine without consulting arbitrary JudgementDB alternatives
```

## Reading Notes Procedure

For every paper read, append a short entry to this note or a dated companion
note with:

```text
Citation:
Question investigated:
Claims adopted:
Claims rejected or deferred:
Required implementation consequence:
Open question:
```

Do not turn a paper result into a kernel rule without also recording the exact
calculus assumptions under which that result holds.
