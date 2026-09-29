#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include "synthesis_work.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int pg_artifact_limit_argument(const char *argument, size_t *limit)
{
	if (!argument || !*argument || !limit) return -1;
	if (!strcmp(argument, "none")) { *limit = SIZE_MAX; return 0; }
	size_t value = 0;
	for (const unsigned char *p = (const unsigned char *)argument; *p; ++p) {
		if (*p < '0' || *p > '9') return -1;
		unsigned digit = *p - '0';
		if (value > (SIZE_MAX - digit) / 10) return -1;
		value = value * 10 + digit;
	}
	if (!value) return -1;
	*limit = value;
	return 0;
}

int pg_artifact_trusted_export(const struct pg_program *program,
	const struct pg_synthesis_job *module, struct pg_token name,
	const struct pg_occurrence **subject)
{
	if (!program || !module || !subject || !name.text || !name.length) return -1;
	const struct pg_synthesis *synthesis = &program->synthesis;
	if (synthesis->steps) return 0;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(synthesis, module, &scope, &syntax)) return 0;
	if (!syntax || syntax->kind != PG_SYNTAX_DEFINITIONS) return 0;
	if (!pg_synthesis_saved_complete(synthesis, module)) return 0;
	const struct pg_occurrence *selected = NULL;
	for (size_t i = 0; i < syntax->item_count; ++i) {
		const struct pg_syntax_item *item;
		struct pg_synthesis_job *entry;
		if (pg_synthesis_definition_entry(module, i, &item, &entry) != 1) return 0;
		if (!pg_synthesis_saved_complete(synthesis, entry)) return 0;
		if (item->operation != PG_TOKEN_ASSIGN) continue;
		if (item->name.length != name.length || memcmp(item->name.text, name.text, name.length)) continue;
		if (selected || pg_synthesis_materialized(entry, &selected) != 1) return 0;
	}
	if (!selected || selected->context || !selected->core || !selected->classifier) return 0;
	*subject = selected;
	return 1;
}

enum pg_synthesis_status pg_artifact_revalidate(struct pg_program *program,
	struct pg_synthesis_job *target, uint64_t total_budget, uint64_t validation_limit,
	uint64_t *spent)
{
	if (!program || !target || !spent) return PG_SYNTHESIS_ERROR;
	struct pg_synthesis *synthesis = &program->synthesis;
	if (target->owner != synthesis->owner_key) return PG_SYNTHESIS_ERROR;
	uint64_t before = synthesis->steps;
	uint64_t available = total_budget < validation_limit ? total_budget : validation_limit;
	if (available > UINT64_MAX - before) available = UINT64_MAX - before;
	while (available && synthesis->ready && pg_synthesis_status(target) == PG_SYNTHESIS_PENDING) {
		pg_synthesis_advance(synthesis, 1);
		--available;
	}
	*spent = synthesis->steps - before;
	return pg_synthesis_status(target);
}

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
