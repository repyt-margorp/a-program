#include "emit.h"
#include "runtime.h"
#include "dag.h"
#include "classifier.h"
#include "computation.h"
#include "host.h"
#include "iadt.h"
#include "identity.h"
#include "evidence.h"
#include <inttypes.h>
#include <string.h>

static const struct pg_term *diagonal_transport(const struct pg_term *term)
{
	const struct pg_term *family, *value, *type;
	int lift;
	if (!pg_identity_field_view(term, &family, &value, NULL, &lift) || lift) return NULL;
	if (!pg_identity_action_view(family, &type) || type->kind != PG_REFERENCE) return NULL;
	const struct pg_object *object = type->as.reference;
	/* field_answer consumes Act A only after demanding its head. These fixed
	 * references leave that action neutral. A binder or arbitrary Oracle does
	 * not: even a syntactic Act can unfold into a non-diagonal family. */
	if (pg_classifier_rigid(object) || pg_host_type_name(object) || pg_data_declaration_view(object)) return value;
	return NULL;
}

static int term_child(void *unused, const void *key, size_t slot, const void **child)
{
	(void)unused;
	const struct pg_term *term = key;
	const struct pg_term *transported = diagonal_transport(term);
	if (transported) {
		if (slot) return 0;
		*child = transported; return 1;
	}
	switch (term->kind) {
	case PG_LAMBDA:
		if (slot) return 0;
		*child = term->as.lambda.body; return 1;
	case PG_APPLICATION:
		if (slot > 1) return 0;
		*child = slot ? term->as.application.argument : term->as.application.function; return 1;
	case PG_REFERENCE: return 0;
	}
	return -1;
}

static size_t object_id(struct pg_dag *objects, const struct pg_object *object)
{
	if (pg_dag_add(objects, object)) return 0;
	return pg_dag_find(objects, object)->id;
}

static const char *operation_name(enum ap_operation operation)
{
	static const char *const names[] = {
		"AP_RETURN", "AP_THUNK", "AP_FORCE", "AP_TOTAL_RESULT", "AP_FOLD", "AP_REQUEST",
		"AP_ADD", "AP_SUBTRACT", "AP_MULTIPLY", "AP_NEGATE", "AP_DECIMAL",
		"AP_CONSTRUCTOR", "AP_MATCH", "AP_LABEL", "AP_ATOM"
	};
	return names[operation];
}

static int describe(struct pg_dag *objects, const struct pg_object *object,
	struct ap_descriptor *descriptor)
{
	*descriptor = (struct ap_descriptor){0};
	static const struct {
		const struct pg_object *object;
		enum ap_operation operation;
		size_t arity;
	} fixed[] = {
		{&pg_return_operation, AP_RETURN, 1}, {&pg_thunk_operation, AP_THUNK, 1},
		{&pg_force_operation, AP_FORCE, 1}, {&pg_total_result_operation, AP_TOTAL_RESULT, 1},
		{&pg_fold_operation, AP_FOLD, 2}, {&pg_request_operation, AP_REQUEST, 3}
	};
	for (size_t i = 0; i < sizeof(fixed) / sizeof(*fixed); ++i) {
		if (object != fixed[i].object) continue;
		descriptor->operation = fixed[i].operation;
		descriptor->arity = fixed[i].arity;
		return 0;
	}
	static const struct { const char *name; enum ap_operation operation; size_t width, arity; } host[] = {
		{"host/int32/add/v1", AP_ADD, 4, 2}, {"host/int32/sub/v1", AP_SUBTRACT, 4, 2},
		{"host/int32/mul/v1", AP_MULTIPLY, 4, 2}, {"host/int32/neg/v1", AP_NEGATE, 4, 1},
		{"host/int64/add/v1", AP_ADD, 8, 2}, {"host/int64/sub/v1", AP_SUBTRACT, 8, 2},
		{"host/int64/mul/v1", AP_MULTIPLY, 8, 2}, {"host/int64/neg/v1", AP_NEGATE, 8, 1},
		{"host/int32/decimal-ascii/v1", AP_DECIMAL, 4, 1},
		{"host/int64/decimal-ascii/v1", AP_DECIMAL, 8, 1}
	};
	const char *name = pg_host_function_descriptor(object);
	for (size_t i = 0; name && i < sizeof(host) / sizeof(*host); ++i) {
		if (strcmp(name, host[i].name)) continue;
		descriptor->operation = host[i].operation;
		descriptor->width = host[i].width;
		descriptor->arity = host[i].arity;
		return 0;
	}
	const struct pg_clause_position *clauses;
	size_t count;
	if (pg_computation_handler_view(object, &count, &clauses)) {
		if (count > SIZE_MAX - 2) return -1;
		if (count > SIZE_MAX / sizeof(size_t)) return -1;
		size_t *labels = pg_alloc(&objects->storage, count * sizeof(*labels));
		if (count > SIZE_MAX / sizeof(const struct pg_object *)) return -1;
		const struct pg_object **ordered = pg_alloc(&objects->storage, count * sizeof(*ordered));
		if (!labels || !ordered) return -1;
		for (size_t i = 0; i < count; ++i) {
			if (clauses[i].position >= count) return -1;
			ordered[clauses[i].position] = clauses[i].label;
		}
		for (size_t i = 0; i < count; ++i) {
			labels[i] = object_id(objects, ordered[i]);
			if (!labels[i]) return -1;
		}
		descriptor->operation = AP_FOLD;
		descriptor->arity = count + 2;
		descriptor->clause_count = count;
		descriptor->labels = labels;
		return 0;
	}
	const struct pg_term *payload, *response;
	if (pg_operation_label_types(object, &payload, &response)) {
		descriptor->operation = AP_LABEL;
		descriptor->family = object_id(objects, object);
		descriptor->host_print = pg_host_operation_descriptor(object) != NULL;
		return descriptor->family ? 0 : -1;
	}
	const struct pg_data_layout *layout;
	if (pg_data_constructor_view(object, &layout, &descriptor->position, &descriptor->arity)) {
		descriptor->operation = AP_CONSTRUCTOR;
		descriptor->family = object_id(objects, pg_data_matcher(layout));
		return descriptor->family ? 0 : -1;
	}
	layout = pg_data_layout_view(object);
	if (layout) {
		descriptor->operation = AP_MATCH;
		descriptor->arity = pg_data_layout_count(layout) + 1;
		descriptor->family = object_id(objects, object);
		return descriptor->family ? 0 : -1;
	}
	/* These owners guarantee no root contraction, at any application arity.
	 * Retain the nominal token; do not identify two runtime type arguments. */
	if (pg_classifier_rigid(object) || pg_host_type_name(object) || pg_data_declaration_view(object)) {
		descriptor->operation = AP_ATOM;
		descriptor->family = object_id(objects, object);
		return descriptor->family ? 0 : -1;
	}
	return -1;
}

static int entry_mode(const struct pg_occurrence *root)
{
	if (!root || root->context || !root->core || !root->classifier) return -1;
	const struct pg_term *type = root->classifier, *content;
	int mode = 0;
	if (root->judgement == PG_JUDGEMENT_VALUE) {
		if (!pg_thunk_type_view(type, &content)) return 2;
		type = content;
		mode = 1;
	} else if (root->judgement != PG_JUDGEMENT_COMPUTATION) return -1;
	enum pg_totality totality;
	const struct pg_effect_row *effects;
	return pg_computation_type_view(type, &totality, &effects, &content) ? mode : -1;
}

int pg_c_emit(FILE *output, const struct pg_occurrence *root, const char **error)
{
	if (!output || !error) return -1;
	*error = "expected a closed returning entry";
	int mode = entry_mode(root);
	if (mode < 0) return -1;
	struct pg_dag terms, objects;
	if (pg_dag_init(&terms, term_child, NULL)) return -1;
	if (pg_dag_init(&objects, NULL, NULL)) { pg_dag_destroy(&terms); return -1; }
	int result = -1;
	*error = "cannot collect Core DAG";
	if (pg_dag_add(&terms, root->core)) goto done;
	/* Validate the complete reachable subset before writing any C. */
	for (const struct pg_dag_node *node = terms.first; node; node = node->next) {
		const struct pg_term *term = node->key;
		if (term->kind == PG_LAMBDA && !object_id(&objects, term->as.lambda.binder)) goto done;
		if (term->kind == PG_REFERENCE && !object_id(&objects, term->as.reference)) goto done;
	}
	if (terms.count >= SIZE_MAX / sizeof(struct ap_descriptor)) goto done;
	struct ap_descriptor *descriptors = pg_alloc(&terms.storage, (terms.count + 1) * sizeof(*descriptors));
	if (!descriptors) goto done;
	for (const struct pg_dag_node *node = terms.first; node; node = node->next) {
		const struct pg_term *term = node->key;
		if (term->kind != PG_REFERENCE || term->as.reference->kind == PG_BINDER) continue;
		const struct pg_object *type;
		size_t count;
		const unsigned char *bytes;
		if (pg_host_literal_view(term->as.reference, &type, &count, &bytes)) continue;
		if (describe(&objects, term->as.reference, &descriptors[node->id])) {
			*error = term->as.reference->owner ? term->as.reference->owner->name : "unknown Oracle";
			goto done;
		}
	}
	fputs("/* A Program C backend: structural closures; acceptance belongs to the caller. */\n#include \"runtime.h\"\n#include <stdlib.h>\n_Static_assert(AP_C_RUNTIME_ABI == 1, \"incompatible A Program C runtime\");\n\n", output);
	for (const struct pg_dag_node *node = terms.first; node; node = node->next)
		fprintf(output, "static struct ap_value *t%zu(struct ap_runtime *, const struct ap_env *);\n", node->id);
	for (const struct pg_dag_node *node = terms.first; node; node = node->next) {
		const struct pg_term *term = node->key;
		fprintf(output, "\nstatic struct ap_value *t%zu(struct ap_runtime *r, const struct ap_env *e)\n{\n\t(void)e;\n", node->id);
		const struct pg_term *transported = diagonal_transport(term);
		if (transported) {
			fprintf(output, "\treturn t%zu(r, e);\n}\n", pg_dag_find(&terms, transported)->id);
			continue;
		}
		switch (term->kind) {
		case PG_LAMBDA:
			fprintf(output, "\treturn ap_function(r, %zu, t%zu, e);\n", object_id(&objects, term->as.lambda.binder), pg_dag_find(&terms, term->as.lambda.body)->id);
			break;
		case PG_APPLICATION:
			fprintf(output, "\tstruct ap_value *f = t%zu(r, e);\n\treturn ap_apply(r, f, ap_delay(r, t%zu, e));\n", pg_dag_find(&terms, term->as.application.function)->id, pg_dag_find(&terms, term->as.application.argument)->id);
			break;
		case PG_REFERENCE: {
			const struct pg_object *object = term->as.reference, *type;
			size_t count;
			const unsigned char *bytes;
			if (object->kind == PG_BINDER) {
				fprintf(output, "\treturn ap_lookup(r, e, %zu);\n", object_id(&objects, object));
			} else if (pg_host_literal_view(object, &type, &count, &bytes)) {
				if (type == pg_host_type("Text")) {
					fputs("\tstatic const unsigned char bytes[] = {", output);
					for (size_t i = 0; i < count; ++i) fprintf(output, "0x%02x,", bytes[i]);
					fprintf(output, "0};\n\treturn ap_text(r, bytes, %zu);\n", count);
				} else {
					uint64_t bits = 0;
					for (size_t i = 0; i < count; ++i) bits = (bits << 8) | bytes[i];
					fprintf(output, "\treturn ap_integer(r, UINT64_C(0x%" PRIx64 "), %zu);\n", bits, count);
				}
			} else {
				const struct ap_descriptor *descriptor = &descriptors[node->id];
				fputs("\tstatic const size_t labels[] = {", output);
				for (size_t i = 0; i < descriptor->clause_count; ++i) fprintf(output, "%zu,", descriptor->labels[i]);
				fputs("0};\n", output);
				fprintf(output, "\tstatic const struct ap_descriptor d = {%s, %zu, %zu, %zu, %zu, %zu, labels, %d};\n\treturn ap_primitive(r, &d);\n",
					operation_name(descriptor->operation), descriptor->arity, descriptor->width, descriptor->family,
					descriptor->position, descriptor->clause_count, descriptor->host_print);
			}
			break;
		}
		}
		fputs("}\n", output);
	}
	fprintf(output, "\nint main(void)\n{\n\tstruct ap_runtime *r = calloc(1, sizeof(*r));\n\tif (!r) return 2;\n\tif (!setjmp(r->failure)) ap_run(r, t%zu(r, NULL), %d);\n\tint status = r->status;\n\tap_destroy(r);\n\tfree(r);\n\treturn status;\n}\n", pg_dag_find(&terms, root->core)->id, mode);
	*error = "cannot write C output";
	result = ferror(output) ? -1 : 0;
done:
	pg_dag_destroy(&objects);
	pg_dag_destroy(&terms);
	return result;
}
