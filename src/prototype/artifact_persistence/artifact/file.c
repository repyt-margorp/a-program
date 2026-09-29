#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct pg_program *pg_artifact_read_file(FILE *file, size_t limit,
	size_t *count, struct pg_synthesis_job *const **roots)
{
	if (!file || !count || !roots) return NULL;
	if (ftell(file) >= 0) return pg_sources_read(file, limit, count, roots);
	/* The shared relocation table requires seeks, not a size-derived quota. */
	FILE *staged = tmpfile();
	if (!staged) return NULL;
	struct pg_program *program = NULL;
	unsigned char buffer[8192];
	for (;;) {
		size_t got = fread(buffer, 1, sizeof(buffer), file);
		if (fwrite(buffer, 1, got, staged) != got) goto done;
		if (got < sizeof(buffer)) {
			if (ferror(file)) goto done;
			break;
		}
	}
	if (fseek(staged, 0, SEEK_SET)) goto done;
	program = pg_sources_read(staged, limit, count, roots);
done:
	fclose(staged);
	return program;
}

int pg_artifact_save_file(const char *path, const struct pg_program *program,
	size_t count, struct pg_synthesis_job *const *roots, enum pg_artifact_contents contents)
{
	if (!path || !program) return -1;
	int (*write_image)(FILE *, const struct pg_synthesis *, size_t, struct pg_synthesis_job *const *);
	switch (contents) {
	case PG_ARTIFACT_MATERIALIZED: write_image = pg_sources_write; break;
	case PG_ARTIFACT_INPUTS: write_image = pg_sources_write_inputs; break;
	default: return -1;
	}
	static const char suffix[] = ".tmp.XXXXXX";
	size_t length = strlen(path);
	if (length > SIZE_MAX - sizeof(suffix)) return -1;
	char *temporary = malloc(length + sizeof(suffix));
	if (!temporary) return -1;
	memcpy(temporary, path, length);
	memcpy(temporary + length, suffix, sizeof(suffix));
	int status = -1;
	int descriptor = mkstemp(temporary);
	if (descriptor < 0) { free(temporary); return -1; }
	FILE *file = fdopen(descriptor, "wb");
	if (file) {
		status = write_image(file, &program->synthesis, count, roots);
		if (fclose(file)) status = -1;
	} else close(descriptor);
	if (!status) status = rename(temporary, path);
	if (status) unlink(temporary);
	free(temporary);
	return status;
}
