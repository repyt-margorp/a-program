#include "../link/plan.h"
#include <stdio.h>
#include <string.h>

static void json_string(const char *value)
{
	fputc('"', stdout);
	for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
		if (*p == '"' || *p == '\\') fputc('\\', stdout);
		if (*p < 32) fprintf(stdout, "\\u%04x", *p);
		else fputc(*p, stdout);
	}
	fputc('"', stdout);
}

/* Parse-only target adapter. Source admission remains in the existing image
	* driver; neither this plan nor its ABI authorizes source erasure. */
int main(int argc, char **argv)
{
	if (argc != 2) {
		fputs("usage: c-acc-link-plan SCRIPT.aplink\n", stderr);
		return 2;
	}
	struct pg_c_link_plan plan = {0};
	size_t line;
	const char *error;
	int status = 2;
	if (pg_c_link_read(&plan, argv[1], &line, &error)) {
		fprintf(stderr, "%s:%zu: %s\n", argv[1], line, error);
		goto done;
	}
	status = 4;
	if (plan.lowering != PG_C_ACC_CREATION_CANDIDATE || plan.product > PG_C_ARCHIVE ||
		plan.count != 2 || plan.entry != SIZE_MAX || plan.native_script ||
		plan.enum_count || plan.natural_count || plan.data_count) goto unsupported;
	const char *successor = NULL;
	int outer = 0;
	for (size_t i = 0; i < plan.count; ++i) {
		if (!strcmp(plan.exports[i].alias, "gs_sort") && !strcmp(plan.names[i], "outer_fn")) outer = 1;
		else if (!strcmp(plan.exports[i].alias, "successor")) successor = plan.names[i];
		else goto unsupported;
	}
	if (!outer || !successor) goto unsupported;
	static const char *const products[] = {"source", "object", "archive"};
	fputs("{\"artifact\":", stdout); json_string(plan.artifact);
	fputs(",\"successor\":", stdout); json_string(successor);
	fputs(",\"product\":", stdout); json_string(products[plan.product]);
	fputs(",\"abi\":", stdout); json_string(pg_c_abi_name(plan.lowering));
	fputs(",\"lowering\":", stdout); json_string(pg_c_lowering_name(plan.lowering));
	fputs(",\"target\":\"host-c11\",\"fallback\":\"reject\",\"source\":\"outer_fn\",\"symbol\":\"gs_sort\"}\n", stdout);
	status = ferror(stdout) || fflush(stdout) ? 2 : 0;
	goto done;
unsupported:
	fputs("Acc candidate LinkerScript requires source/object/archive, outer_fn gs_sort and a successor selection; no generalized native lowering\n", stderr);
done:
	pg_c_link_destroy(&plan);
	return status;
}
