#ifndef A_PROGRAM_POINTER_OCCURRENCE_IO_H
#define A_PROGRAM_POINTER_OCCURRENCE_IO_H

#include "typing.h"
#include "scope.h"
#include "graph_io.h"
#include <stdio.h>

/* APGOCC8 transports descriptive typed inputs, not evidence. Contexts, Core,
 * classifiers and annotations share relocation tables. Sorts and operand order
 * and structural maps are retained. Derived origins are distinct from direct
 * construction inputs. Selected construction maps retain their source scopes
 * and typed images. Input selection retains its ordinal and optional argument;
 * neither a selected result nor a lifted child view certifies a judgement.
 * Recursive elimination retains its binder allocation and
 * clause scopes through the same relocation tables. Optional annotations and
 * classifiers occupy a reference slot only when present. Earlier images reject.
 * Descriptor naming/resolution obeys graph_io.h's owner contract. */
int pg_occurrences_write(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
/* Same descriptive format with nominal payloads and enclosing-image sharing.
 * The name/resolve entry points below are adapters, not another codec. */
int pg_occurrences_write_descriptors(FILE *file, size_t count, const struct pg_occurrence *const *roots,
	const struct pg_graph_codec *codec, void *owner);
/* Initialized typing required. limit bounds nodes, roots, operand edges and
 * selected maps and induction inputs here, and each nested section independently. Failed reads publish no
 * roots but may allocate unused arena data. No evidence is constructed. */
int pg_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_occurrence *const **roots);
int pg_occurrences_read_descriptors(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_occurrence *const **roots);

/* APGSCP2 pairs each open root with its selected typed declaration scope.
 * Scope prefixes and formation Terms share the same relocation tables as the
 * roots. Reading remains inert; check with pg_prove_scoped_subject afterwards.
 * Earlier images reject. The unscoped API retains APGOCC8 and does not silently
 * drop a scoped input. */
int pg_scoped_occurrences_write(FILE *file, size_t count,
	const struct pg_scope *const *scopes, const struct pg_occurrence *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *owner);
int pg_scoped_occurrences_write_descriptors(FILE *file, size_t count,
	const struct pg_scope *const *scopes, const struct pg_occurrence *const *roots,
	const struct pg_graph_codec *codec, void *owner);
int pg_scoped_occurrences_read(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *owner,
	size_t *count, const struct pg_scope *const **scopes, const struct pg_occurrence *const **roots);
int pg_scoped_occurrences_read_descriptors(FILE *file, struct pg_typing *typing, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner,
	size_t *count, const struct pg_scope *const **scopes, const struct pg_occurrence *const **roots);

#endif
