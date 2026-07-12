# Agent Development Rules

This repository uses a strict separation between accepted implementation code and
AI-developed prototype code. Follow these rules before making any code change.

## Source Layout

Production and accepted code lives in:

- `src/`
- `include/`

Prototype and AI-developed code lives in:

- `src/prototype/`

The intended contrast is:

- `src/`: reviewed, accepted, project code.
- `src/prototype/`: experimental code, drafts, sketches, alternative designs, and
  AI-agent implementation work that has not been accepted yet.

## Default Write Boundary for AI Agents

AI agents must write code only under `src/prototype/` unless the user explicitly
approves writing elsewhere.

Without explicit approval, do not create, edit, move, or delete implementation
files under:

- `src/` outside `src/prototype/`
- `include/`
- parser or lexer inputs used by the accepted build, such as `src/parser.y` and
  `src/lexer.l`
- build files that affect accepted code, such as `Makefile`

Documentation-only changes may be made outside `src/prototype/` when the user
asks for documentation or repository policy updates.

## Promotion Rule

Code in `src/prototype/` becomes accepted code only when the user explicitly says
to adopt, promote, accept, merge, or otherwise move that prototype into the main
implementation.

When promoting prototype code:

1. Treat the promotion as a separate, intentional change.
2. Compare the prototype against the current accepted implementation.
3. Move only the approved pieces into `src/` or `include/`.
4. Keep the diff small enough to review.
5. Update tests, examples, and build rules only as needed for the accepted change.

Ambiguous requests should remain inside `src/prototype/`. If the user asks for an
implementation but does not say it is accepted for production, implement it as a
prototype.

## Existing Project Style

Follow `CODING_STYLE.md` for C, Flex, Bison, and documentation conventions.

In particular:

- use snake_case identifiers
- avoid typedefs in user-authored code
- use C-style comments only: `// ...` and `/* ... */`
- do not introduce `(* ... *)` comments in `.p` examples or parser fixtures
- use tabs for leading indentation in source files
- keep comments and documentation in English
- keep headers declarative and implementation in source files

## Generated Files

Do not edit generated files by hand. Regenerate them from their source inputs.

Generated parser and lexer outputs include:

- `parser.tab.c`
- `parser.tab.h`
- `lex.yy.c`

Build outputs such as `a.out` are not source code and should not be treated as
accepted implementation.
