#ifndef A_PROGRAM_C_LINK_PLAN_H
#define A_PROGRAM_C_LINK_PLAN_H

#include "../emit.h"

enum pg_c_product { PG_C_SOURCE, PG_C_OBJECT, PG_C_ARCHIVE, PG_C_EXECUTABLE };
enum pg_c_lowering { PG_C_STRUCTURAL, PG_C_SCALAR_DIRECT, PG_C_NATIVE_DIRECT };
struct pg_c_link_plan {
	struct pg_graph storage;
	const char *artifact, *native_script;
	enum pg_c_product product;
	enum pg_c_lowering lowering;
	size_t count, entry;
	const char **names;
	struct pg_c_export *exports;
	size_t enum_count;
	const char **enum_names;
	struct pg_c_export *enums;
	size_t data_count;
	const char **data_names;
	struct pg_c_export *data;
	size_t natural_count;
	const char **natural_names;
	struct pg_c_export *naturals;
};

/* Zero-initialize before read; destroy on either success or failure. Paths are
	* relative to the script, not the working directory. No artifact is opened,
	* no code executed, and no admission policy is accepted from script data. */
int pg_c_link_read(struct pg_c_link_plan *, const char *path, size_t *line, const char **error);
void pg_c_link_destroy(struct pg_c_link_plan *);
const char *pg_c_lowering_name(enum pg_c_lowering);
const char *pg_c_abi_name(enum pg_c_lowering);

/* Publish a new directory only after emission and any native tools succeed.
	* Existing directories/files are never replaced. Tools are explicit argv[0]
	* values, not shell commands. The supported target profile is host-c11. */
int pg_c_link_publish(const struct pg_c_link_plan *, const char *directory,
	const char *cc, const char *ar, const char *script, int trusted, uint64_t spent);

#endif
