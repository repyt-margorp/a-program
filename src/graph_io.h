#ifndef A_PROGRAM_POINTER_GRAPH_IO_H
#define A_PROGRAM_POINTER_GRAPH_IO_H

#include "graph.h"
#include <stdio.h>

struct pg_dag;
struct pg_graph_codec;

/* Raw structural comparison/independence progress, not accepted equality.
 * Normalizing comparisons require their owning evaluator state and are rejected
 * by this component. Read into an unused handle; destroy it before graph.
 * Stored work resumes through pg_comparison_advance with no alternate walker.
 * Reading performs no comparison and grants no trust to saved results.
 * Additional owner roots share the work's Term/binder relocation table.
 * Reader outputs are cleared on failure and owned by graph on success. */
/* Owning metadata (for example scope arrays) can wrap the final Term table.
 * Callbacks preserve root order/sharing and release any separately restored
 * resources on an outer read failure. The comparison walker is unchanged. */
int pg_comparison_write_with(FILE *file, const struct pg_comparison *work,
	size_t count, const struct pg_term *const *roots,
	int (*write_terms)(FILE *, size_t, const struct pg_term *const *, void *), void *owner);
int pg_comparison_read_with(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	int (*read_terms)(FILE *, struct pg_graph *, size_t, size_t, size_t *, const struct pg_term *const **, void *),
	void *owner, struct pg_comparison *work, size_t *count, const struct pg_term *const **roots);
int pg_comparison_write(FILE *file, const struct pg_comparison *work,
	size_t count, const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *owner);
int pg_comparison_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *owner, struct pg_comparison *work,
	size_t *count, const struct pg_term *const **roots);

/* Diagnostic shared-DAG listing. IDs are local display labels, not semantic
 * identities or addresses. No reduction, descriptor execution or graph edits. */
int pg_graph_print(FILE *file, const struct pg_term *root);

/* Optional immutable descriptor payloads in the same Term relocation table.
 * child: 1 child, 0 end, -1 error, -2 external object (index zero only).
 * name identifies an external object or, for a payload, its descriptor format.
 * child receives a scratch arena for temporary reference terms; it must not
 * change the source graph. restore constructs an inert object, never accepted
 * evidence or host effects.
 * Repeated format names may create distinct nominal objects. Payload references
 * must be acyclic with the containing Term graph. restore may retain the terms,
 * not its temporary input arrays. scalar enumerates unsigned metadata with
 * 1 item, 0 end, -1 error; NULL means none. Scalars are not Term dependencies
 * or type evidence. Limits include descriptor edges and metadata items. */
/* Optional enclosing-image ownership of ordered Term roots. Descriptor
 * callbacks still receive their original object context. The enclosing format
 * owns stream boundaries and the lifetime of borrowed root graphs. */
struct pg_graph_root_codec {
	int (*write)(FILE *, size_t, const struct pg_term *const *, void *);
	int (*read)(FILE *, struct pg_graph *, size_t, size_t, size_t *, const struct pg_term *const **, void *);
	void *context;
};

struct pg_graph_codec {
	const char *(*name)(void *, const struct pg_object *);
	const struct pg_object *(*resolve)(void *, const char *);
	int (*child)(void *, struct pg_graph *, const struct pg_object *, size_t, const struct pg_term **);
	int (*scalar)(void *, const struct pg_object *, size_t, uint64_t *);
	const struct pg_object *(*restore)(void *, struct pg_graph *, const char *, size_t,
		const struct pg_term *const *, size_t, const uint64_t *);
	const struct pg_graph_root_codec *roots;
};

/* Seekable enclosing image: version[8], terminal table offset, owner payload,
 * then one shared Term/object table. Payload codecs use the supplied codec
 * instead of emitting independent tables. Source graphs outlive writing;
 * temporary Reference wrappers are retained, other source nodes are borrowed.
 * A successful payload read may still fail the enclosing boundary check: the
 * owner must withhold publication and clean up its resources on any failure.
 * No evaluation, proof acceptance or saved-status trust is performed here. */
int pg_graph_image_write(FILE *file, const char version[8],
	const struct pg_graph_codec *codec, void *object_owner,
	int (*payload)(FILE *, const struct pg_graph_codec *, void *), void *state);
int pg_graph_image_read(FILE *file, const char version[8], struct pg_graph *graph,
	size_t limit, size_t name_limit, const struct pg_graph_codec *codec, void *object_owner,
	int (*payload)(FILE *, struct pg_graph *, size_t, size_t, const struct pg_graph_codec *, void *), void *state);
/* Add reachable objects to an initialized leaf DAG using the exact transport
 * dependency traversal, including descriptor payloads. No bytes, evaluation or
 * graph mutations. Keys borrow objects from roots and codec-owned storage. */
int pg_graph_collect_objects(struct pg_dag *objects, size_t count,
	const struct pg_term *const *roots, const struct pg_graph_codec *codec, void *context);
/* Initialize a persistent transport dependency DAG. Add roots with pg_dag_add;
 * shared terms are visited once across additions. objects must already be an
 * initialized leaf DAG. Destroy terms before objects and codec storage. */
int pg_graph_dependencies_init(struct pg_dag *terms, struct pg_dag *objects,
	const struct pg_graph_codec *codec, void *context);
int pg_graph_write_descriptors(FILE *file, size_t count, const struct pg_term *const *roots,
	const struct pg_graph_codec *codec, void *context);
int pg_graph_read_descriptors(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_graph_codec *codec, void *context,
	size_t *count, const struct pg_term *const **roots);

/* Raw acyclic Core graph transport, not typing evidence or a program image.
 * Plain binders are relocated freshly. Owned binders and semantic objects need
 * stable, versioned descriptor names supplied by the owner. The codec never
 * interprets a name as a host address. Names must identify distinct objects;
 * callbacks and referenced objects must outlive the call/output graph.
 * No evaluation or alpha interning takes place. Streams are caller-owned. */
int pg_graph_write(FILE *file, size_t count, const struct pg_term *const *roots,
	const char *(*name)(void *, const struct pg_object *), void *context);
/* graph must be initialized. Limits bound record count and descriptor length.
 * Output roots are owned by graph and published only on success. Failure may
 * leave unused arena allocations but cannot publish evidence. Unknown descriptors fail closed.
 * Distinct object records resolving to one pointer are rejected, not merged.
 * Recursive declaration payloads and accepted evidence are not encoded here. */
int pg_graph_read(FILE *file, struct pg_graph *graph, size_t limit, size_t name_limit,
	const struct pg_object *(*resolve)(void *, const char *), void *context,
	size_t *count, const struct pg_term *const **roots);

#endif
