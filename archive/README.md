# Archived Implementations

- `legacy/`: the previous compiler, moved without source changes from commit
  `2f16becdb01bff9d54202930cf2a467b477b47af`. Its original `src/prototype/`
  layout and root Makefile are preserved inside that directory. The current
  compiler's compatibility tests read its `.p` fixtures, not its C implementation.
- `pointer-experiments/`: historical trial patches, not part of the active build.

Use root `make` for the current compiler in `src/`. Old commands/documents refer
to their original revision; this archive does not promise that every historical
integration script runs in its new location. For the complete earlier checkout,
use tag `old-version/2026-09-14-main`; the archive above is a later source snapshot.

No archived code is silently promoted or linked. Keep this tree unchanged except
for an explicitly requested archival correction.
