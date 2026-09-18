#include "source_io.h"
#include "derivation.h"
#include "wire.h"
#include "computation.h"
#include "computation_io.h"
#include "eval_internal.h"
#include "iadt.h"
#include "syntax_io.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct pg_synthesis_job *parse(struct pg_program *program,
	const struct pg_source_scope *scope, const char *text)
{
	struct pg_parser parser;
	struct pg_synthesis_job *job = pg_program_source(program, scope, text, strlen(text), &parser);
	assert(job && !parser.error);
	return job;
}

static void indexed_ih_fiber(struct pg_program *p, const struct pg_evidence *formation)
{
	const struct pg_evidence *self = pg_evidence_premise(formation, 0);
	const struct pg_evidence *context = pg_evidence_premise(self, 0);
	const struct pg_evidence *parameters = pg_prove_substitution_projection(&p->typing, context, context);
	const struct pg_evidence *mc = pg_prove_inductive_motive_context(&p->typing,
		formation, parameters, pg_binder(&p->graph));
	assert(mc);
	const struct pg_evidence *motive = pg_prove_computation_type(&p->typing, PG_TOTALITY_TOTAL,
		pg_effect_row(&p->graph, 0, NULL),
		pg_prove_projection(&p->typing, mc, pg_evidence_premise(mc, 1)));
	struct pg_inductive_instance instance;
	assert(motive && pg_inductive_instance(&p->typing, formation, &instance));
	const struct pg_object *next = pg_data_constructor(pg_data_schema_layout(instance.schema), 1);
	const struct pg_evidence *scope = pg_prove_induction_scope(&p->typing,
		formation, next, parameters, mc, motive);
	assert(scope);
	const struct pg_evidence *field = pg_evidence_premise(scope, pg_evidence_premise_count(scope) - 1);
	const struct pg_context *with_ih = pg_evidence_context(pg_evidence_premise(scope, 1));
	const struct pg_term *expected = pg_thunk_type(&p->graph,
		pg_computation_type(&p->graph, PG_TOTALITY_TOTAL, pg_effect_row(&p->graph, 0, NULL), pg_evidence_classifier(field)));
	const struct pg_term *function;
	if (pg_thunk_type_view(pg_evidence_classifier(field), &function)) {
		expected = pg_evidence_classifier(field);
		const struct pg_evidence *z = pg_prove_variable(&p->typing, mc, pg_evidence_context(mc)->binder);
		const struct pg_evidence *dependent = pg_prove_return_type(&p->typing,
			pg_prove_identity_type(&p->typing, pg_prove_classifier(&p->typing, mc, z), z, z));
		assert(dependent);
		/* Source arrows promise TOTAL; the child can now remain a symbolic
		 * pure result in the motive instead of escaping as a free binder. */
		assert(pg_prove_induction_scope(&p->typing, formation, next, parameters, mc, dependent));
	}
	assert(pg_alpha_equal(with_ih->declared_type, expected) == 1);
}

static void indexed_family_roundtrip(const char *source)
{
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		struct pg_synthesis_job *initial[] = {p->root};
		struct pg_synthesis_job *const *roots = initial;
		size_t count = 1;
		/* Save both unelaborated syntax and accepted declaration/constructor
		 * origins. Restoring either schedules the same ordinary Solve rules. */
		for (size_t round = 0; round < 3; ++round) {
			if (round) {
				while (pg_synthesis_status(roots[0]) == PG_SYNTHESIS_PENDING) {
					assert(p->synthesis.steps < 100000);
					pg_synthesis_advance(&p->synthesis, chunk);
				}
				assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
			}
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
			pg_program_destroy(p);
			rewind(file);
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
			assert(!fclose(file));
		}
		while (pg_synthesis_status(roots[0]) == PG_SYNTHESIS_PENDING) {
			assert(p->synthesis.steps < 100000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *family = pg_synthesis_result(pg_synthesis_definition(roots[0],
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "D", .length = 1}));
		assert(family);
		assert(pg_evidence_judgement(family) == PG_JUDGEMENT_TYPE_FAMILY);
		assert(!pg_prove_type_value(&p->typing, family));
		while (pg_evidence_rule(family) == PG_TYPE_FAMILY_ABSTRACT) family = pg_evidence_premise(family, 1);
		assert(pg_evidence_rule(family) == PG_INDUCTIVE_FORM);
		struct pg_synthesis_job *fiber = pg_synthesis_definition(roots[0],
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Fiber", .length = 5});
		assert(fiber && pg_synthesis_result(fiber));
		struct pg_synthesis_job *normalized = pg_synthesis_normalize(&p->synthesis,
			pg_prove_empty_context(&p->typing), pg_synthesis_result(fiber));
		struct pg_synthesis_job *recovery = pg_synthesis_inductive_instance(&p->synthesis, normalized);
		assert(recovery);
		while (pg_synthesis_status(recovery) == PG_SYNTHESIS_PENDING) {
			assert(p->synthesis.steps < 100000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		struct pg_inductive_instance instance;
		assert(pg_synthesis_inductive_instance_result(recovery, &instance) && instance.indices);
		assert(instance.formation == family);
		if (pg_data_constructor_count(instance.schema) == 2) indexed_ih_fiber(p, family);
		if (pg_synthesis_definition(roots[0], (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "expected", .length = 8})) {
			const char *names[] = {"main", "expected"};
			const struct pg_evidence *results[2];
			for (size_t i = 0; i < 2; ++i) {
				struct pg_synthesis_job *nf = pg_program_evaluate_name(p, roots[0],
					(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = strlen(names[i])}, 1);
				assert(nf);
				while (pg_synthesis_status(nf) == PG_SYNTHESIS_PENDING) {
					assert(p->synthesis.steps < 100000);
					pg_synthesis_advance(&p->synthesis, chunk);
				}
				results[i] = pg_synthesis_result(nf);
				if (pg_evidence_judgement(results[i]) != PG_JUDGEMENT_VALUE)
					results[i] = pg_prove_return_value(&p->typing, results[i]);
				assert(results[i]);
			}
			/* Typed normalization preserves the original classifier. Family
			 * instantiation can retain a beta-redex there without changing it. */
			struct pg_conversion comparison;
			assert(!pg_conversion_init(&comparison, &p->evaluation,
				pg_evidence_classifier(results[0]), pg_evidence_classifier(results[1])));
			assert(pg_conversion_advance(&comparison, 100000) == PG_CONVERSION_EQUAL);
			pg_conversion_destroy(&comparison);
			assert(pg_alpha_equal(pg_evidence_subject(results[0])->core, pg_evidence_subject(results[1])->core) == 1);
		}
		pg_program_destroy(p);
	}
}

static void indexed_family_sources(void)
{
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"LT:=@\\x:Nat=>@\\y:Nat=>{step:(n:Nat)->* n (Nat.succ n);};"
		"D:=\\A:@=>\\R:A->A->@=>@\\subject:A=>"
		"{acc:(x:A)->((y:A)->R y x->* y)->* x;};"
		"elim:=\\A:@=>\\R:A->A->@=>\\P:A->@=>"
		"\\step:(x:A)->((y:A)->R y x->P y)->P x=>"
		"\\subject:A=>\\proof:D A R subject=>proof @acc x down=>"
		"{stepAtX:=step x; stepAtX *down;};"
		"Bool:=@{true:*;false:*;};"
		"walk:=\\A:@=>\\R:A->A->@=>\\P:A->@=>\\values:(x:A)->P x=>\\B:@=>\\base:B=>"
		"\\step:(x:A)->((y:A)->R y x->B)->B=>"
		"\\subject:A=>\\proof:D A R subject=>proof @acc x down=>"
		"(\\item:P x=>\\flag:Bool=>flag @true=>base @false=>step x &(\\y:A=>\\rel:R y x=>*down y rel (values y) flag));"
		"Fiber:=D Nat LT Nat.zero; main:=Nat.zero; expected:=Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:(k:Nat)->* k->*(Nat.succ k);};"
		"main:=D.next Nat.zero (D.mk Nat.zero); main::D (Nat.succ Nat.zero); Fiber:=D (Nat.succ Nat.zero);");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=\\A:@=>@\\i:Nat=>{mk:(k:Nat)->A->* k;next:(k:Nat)->* k->*(Nat.succ k);};"
		"main:=(D Nat).next Nat.zero ((D Nat).mk Nat.zero Nat.zero); main::D Nat (Nat.succ Nat.zero); Fiber:=D Nat (Nat.succ Nat.zero);");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=\\A:@=>\\B:@=>@\\i:Nat=>{mk:(k:Nat)->A->B->* k;};"
		"main:=(D Nat Nat).mk Nat.zero Nat.zero Nat.zero; main::D Nat Nat Nat.zero; Fiber:=D Nat Nat Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:(k:Nat)->* k->*(Nat.succ k);};"
		"get:=\\i:Nat=>\\v:D i=>v @mk k=>k @next k rest=>Nat.succ k;"
		"main:=get (Nat.succ Nat.zero) (D.next Nat.zero (D.mk Nat.zero)); Fiber:=D (Nat.succ Nat.zero);");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:(k:Nat)->* k->*(Nat.succ k);};"
		"steps:=\\i:Nat=>\\v:D i=>v @mk k=>Nat.zero @next k rest=>Nat.succ *rest;"
		"main:=steps (Nat.succ Nat.zero) (D.next Nat.zero (D.mk Nat.zero)); Fiber:=D (Nat.succ Nat.zero);");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:((k:Nat)->* k)->* Nat.zero;};"
		"steps:=\\i:Nat=>\\v:D i=>v @mk k=>Nat.zero @next down=>Nat.succ (*down Nat.zero);"
		"main:=steps Nat.zero (D.next &(\\k:Nat=>D.mk k)); expected:=Nat.succ Nat.zero; Fiber:=D Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;};"
		"choose:=\\F:Nat->@=>F Nat.zero; id:=\\x:choose D=>x;"
		"main:=id (D.mk Nat.zero); expected:=D.mk Nat.zero; Fiber:=D Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;};"
		"choose:=\\F:Nat->@=>F Nat.zero; delayed:=&(\\n:Nat=>D n);"
		"id:=\\x:choose delayed=>x; main:=id (D.mk Nat.zero);"
		"expected:=D.mk Nat.zero; Fiber:=D Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=@\\i:Nat=>{mk:(k:Nat)->* k;}; quoted:=&D;"
		"id:=\\x:quoted Nat.zero=>x; main:=id (D.mk Nat.zero);"
		"expected:=D.mk Nat.zero; Fiber:=D Nat.zero;");
	indexed_family_roundtrip("Nat:=@{zero:*;succ:*->*;};"
		"D:=\\A:@=>@\\i:Nat=>{mk:(k:Nat)->A->* k;};"
		"choose:=\\F:@->Nat->@=>F Nat Nat.zero; id:=\\x:choose D=>x;"
		"chooseOne:=\\F:Nat->@=>F Nat.zero; idOne:=\\x:chooseOne (D Nat)=>x;"
		"main:=idOne (id ((D Nat).mk Nat.zero Nat.zero));"
		"expected:=(D Nat).mk Nat.zero Nat.zero; Fiber:=D Nat Nat.zero;");
	const char *invalid[] = {
		"Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;};"
			"choose:=\\F:Nat->Nat->@=>F Nat.zero Nat.zero; bad:=choose D;",
		"Nat:=@{zero:*;succ:*->*;}; choose:=\\F:Nat->@=>F Nat.zero;"
			"delayed:=&(\\n:Nat=>Nat.zero); bad:=choose delayed;",
		"Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:(k:Nat)->* k->*(Nat.succ k);};"
			"bad:=D.next Nat.zero (D.mk (Nat.succ Nat.zero));",
		"Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;}; bad:=\\x:D=>x;",
		"Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;}; bad:=D Nat.zero Nat.zero;",
		"Nat:=@{zero:*;succ:*->*;}; Bool:=@{false:*;true:*;};"
			"D:=\\A:@=>@\\i:Nat=>{mk:(k:Nat)->A->* k;};"
			"bad:=(D Nat).mk Nat.zero Nat.zero; bad::D Bool Nat.zero;",
		"Nat:=@{zero:*;succ:*->*;}; Bool:=@{false:*;true:*;};"
			"D:=@\\i:Bool=>{mk:(k:Bool)->* k;};"
			"choose:=\\F:Nat->@=>F Nat.zero; bad:=choose D;",
		"Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;};"
			"id:=\\n:Nat=>n; bad:=id D;"
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
		struct pg_program *p = pg_program_create(invalid[i], strlen(invalid[i]), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		while (pg_synthesis_status(p->root) == PG_SYNTHESIS_PENDING) {
			assert(p->synthesis.steps < 100000);
			pg_synthesis_advance(&p->synthesis, 1);
		}
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_REJECTED);
		pg_program_destroy(p);
	}
	puts("indexed source: scoped family formation, recursive constructors and inert resaves use ordinary Solve");
}

static void family_context_scopes(void)
{
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		struct pg_typing *t = &p->typing;
		const struct pg_evidence *empty = pg_prove_empty_context(t);
		const struct pg_evidence *u = pg_prove_universe(t, empty, 0);
		const struct pg_object *a = pg_binder(&p->graph), *r = pg_binder(&p->graph);
		const struct pg_evidence *ac = pg_prove_context_extension(t, empty, a, u);
		const struct pg_evidence *xc = pg_prove_context_extension(t, ac, pg_binder(&p->graph), pg_prove_variable(t, ac, a));
		const struct pg_evidence *yc = pg_prove_context_extension(t, xc, pg_binder(&p->graph), pg_prove_variable(t, xc, a));
		const struct pg_evidence *rc = pg_prove_family_context_extension(t, ac, r, yc, pg_prove_projection(t, yc, u));
		assert(rc);
		const struct pg_source_scope *scope = pg_synthesis_bind_context(&p->synthesis, pg_synthesis_root(&p->synthesis),
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "A", .length = 1}, a, pg_synthesis_evidence(&p->synthesis, ac));
		scope = pg_synthesis_bind_context(&p->synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "R", .length = 1}, r, pg_synthesis_evidence(&p->synthesis, rc));
		assert(scope);
		const char *source[] = {"relation := R;",
			"Acc := @\\subject : A => { acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x; };",
			"bad := R A A;"};
		struct pg_synthesis_job *initial[3];
		for (size_t i = 0; i < 3; ++i) {
			struct pg_parser parser;
			struct pg_definition definition;
			pg_parser_init(&parser, &p->graph, source[i], strlen(source[i]));
			assert(pg_parser_next(&parser, &definition) == 1);
			initial[i] = pg_synthesis_request(&p->synthesis, scope, definition.expression);
			assert(initial[i]);
		}
		struct pg_synthesis_job *const *roots = initial;
		size_t count = 3;
		for (unsigned round = 0; round < 4; ++round) {
			if (round == 3) {
				while (p->synthesis.ready && p->synthesis.steps < 10000) pg_synthesis_advance(&p->synthesis, chunk);
				assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
			}
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
			pg_program_destroy(p);
			rewind(file);
			p = pg_sources_read(file, 10000, &count, &roots);
			assert(p && count == 3 && !p->synthesis.steps);
			for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
			assert(!fclose(file));
		}
		while (p->synthesis.ready && p->synthesis.steps < 10000) pg_synthesis_advance(&p->synthesis, chunk);
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		assert(pg_evidence_judgement(pg_synthesis_result(roots[0])) == PG_JUDGEMENT_TYPE_FAMILY);
		assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
		struct pg_inductive_instance instance;
		assert(pg_inductive_instance(&p->typing, pg_synthesis_result(roots[1]), &instance));
		assert(pg_data_constructor_count(instance.schema) == 1);
		assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED);
		/* Fresh nested binders may change the field Context without changing
		 * its type. Check it rather than trusting or mutating saved evidence. */
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&p->synthesis, roots[1], &scope, &syntax));
		const char renamed[] = "Acc := @\\subject : A => { acc : (x:A) -> ((z:A) -> R z x -> * z) -> * x; };";
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &p->graph, renamed, sizeof(renamed) - 1);
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *equivalent = pg_synthesis_declaration_at(&p->synthesis, scope,
			definition.expression, pg_data_schema_declaration(instance.schema));
		assert(equivalent);
		while (p->synthesis.ready && p->synthesis.steps < 20000) pg_synthesis_advance(&p->synthesis, chunk);
		assert(pg_synthesis_status(equivalent) == PG_SYNTHESIS_DONE);
		assert(pg_data_schema_declaration(pg_evidence_inductive_schema(pg_synthesis_result(equivalent))) == pg_data_schema_declaration(instance.schema));
		/* The recursive field is absent from the result index. Rechecking only
		 * result images would miss this incompatible field type. */
		const char changed[] = "Acc := @\\subject : A => { acc : (x:A) -> ((y:A) -> R x y -> * y) -> * x; };";
		pg_parser_init(&parser, &p->graph, changed, sizeof(changed) - 1);
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *invalid = pg_synthesis_declaration_at(&p->synthesis, scope,
			definition.expression, pg_data_schema_declaration(instance.schema));
		assert(invalid);
		while (p->synthesis.ready && p->synthesis.steps < 20000) pg_synthesis_advance(&p->synthesis, chunk);
		assert(pg_synthesis_status(invalid) == PG_SYNTHESIS_REJECTED);
		assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
		/* An explicitly selected theorem remains a checking root, but neither
		 * source declaration needs it as an allocation producer. */
		struct pg_synthesis_job *selected[] = {equivalent,
			pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(equivalent)), invalid};
		roots = selected;
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, 3, roots));
			pg_program_destroy(p);
			rewind(file);
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 3 && !p->synthesis.steps);
			for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
			assert(!fclose(file));
		}
		while (p->synthesis.ready && p->synthesis.steps < 40000) pg_synthesis_advance(&p->synthesis, chunk);
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED);
		assert(pg_data_schema_declaration(pg_evidence_inductive_schema(pg_synthesis_result(roots[0]))) ==
			pg_data_schema_declaration(pg_evidence_inductive_schema(pg_synthesis_result(roots[1]))));
		pg_program_destroy(p);
	}
	puts("source families: logical R/Acc scopes, unsolved and solved resaves, wrong argument rejection passed");
}

static void associated_context_scope(int graph)
{
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	struct pg_typing *t = &p->typing;
	const struct pg_evidence *empty = pg_prove_empty_context(t);
	const struct pg_object *a = pg_binder(&p->graph), *x = pg_binder(&p->graph), *ih = pg_binder(&p->graph);
	const struct pg_evidence *ac = pg_prove_context_extension(t, empty, a,
		pg_prove_universe(t, empty, 0));
	const struct pg_evidence *at = pg_prove_value_type(t, pg_prove_variable(t, ac, a));
	const struct pg_evidence *xc = pg_prove_context_extension(t, ac, x, at);
	const struct pg_evidence *it = pg_prove_thunk_type(t,
		pg_prove_return_type(t, pg_prove_projection(t, xc, at)));
	const struct pg_evidence *ic = pg_prove_context_extension(t, xc, ih, it);
	const struct pg_source_scope *scope = pg_synthesis_root(&p->synthesis);
	scope = pg_synthesis_bind_context(&p->synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "A", .length = 1}, a, pg_synthesis_evidence(&p->synthesis, ac));
	scope = pg_synthesis_bind_context(&p->synthesis, scope,
		(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "x", .length = 1}, x, pg_synthesis_evidence(&p->synthesis, xc));
	scope = graph ? pg_synthesis_bind_graph(&p->synthesis, scope, x, ih, pg_synthesis_evidence(&p->synthesis, ic))
		: pg_synthesis_bind_hypothesis(&p->synthesis, scope, x, ih, pg_synthesis_evidence(&p->synthesis, ic));
	assert(scope);
	struct pg_parser parser;
	struct pg_definition definition;
	const char *source = graph ? "r:=@x;" : "r:=*x;";
	pg_parser_init(&parser, &p->graph, source, strlen(source));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *initial[] = {pg_synthesis_request(&p->synthesis, scope, definition.expression)};
	struct pg_synthesis_job *const *roots = initial;
	size_t count = 1;
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		assert(file && !p->synthesis.steps && !pg_sources_write(file, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
		assert(!fclose(file));
	}
	const struct pg_syntax *syntax;
	assert(!pg_synthesis_source_input(&p->synthesis, roots[0], &scope, &syntax));
	struct pg_source_environment environment, field;
	assert(!pg_synthesis_environment_input(&p->synthesis, scope, &environment));
	assert(environment.context && environment.associated);
	assert(environment.association == (graph ? PG_SOURCE_GRAPH : PG_SOURCE_HYPOTHESIS));
	assert(!pg_synthesis_environment_input(&p->synthesis, environment.associated, &field));
	assert(field.binder && field.context);
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_evidence *result = pg_synthesis_result(roots[0]);
	const struct pg_term *expected = pg_reference(&p->graph, environment.binder);
	if (!graph) expected = pg_application(&p->graph, pg_reference(&p->graph, &pg_force_operation), expected);
	assert(result && pg_evidence_subject(result)->core == expected);
	assert(pg_context_lookup(pg_evidence_context(result), field.binder));
	pg_program_destroy(p);
}

static void source_alias_targets(void)
{
	for (unsigned scenario = 0; scenario < 16; ++scenario) {
		uint64_t chunk = scenario & 1 ? 64 : 1;
		struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		struct pg_typing *t = &p->typing;
		const struct pg_evidence *empty = pg_prove_empty_context(t);
		const struct pg_object *a = pg_binder(&p->graph), *x = pg_binder(&p->graph);
		const struct pg_object *y = pg_binder(&p->graph), *z = pg_binder(&p->graph);
		const struct pg_evidence *ac = pg_prove_context_extension(t, empty, a, pg_prove_universe(t, empty, 0));
		const struct pg_evidence *av = pg_prove_variable(t, ac, a);
		const struct pg_evidence *source = pg_prove_context_extension(t, ac, x, av);
		const struct pg_evidence *yc = pg_prove_context_extension(t, ac, y, av);
		const struct pg_evidence *destination = pg_prove_context_extension(t, yc, z, pg_prove_projection(t, yc, av));
		assert(source && destination);
		const struct pg_evidence *images[] = {pg_prove_variable(t, destination, a), pg_prove_variable(t, destination, y)};
		const struct pg_evidence *left = pg_prove_substitution(t, source, destination, 2, images);
		images[1] = pg_prove_variable(t, destination, z);
		const struct pg_evidence *right = pg_prove_substitution(t, source, destination, 2, images);
		const struct pg_context_map *lm = pg_evidence_context_map(left), *rm = pg_evidence_context_map(right);
		assert(lm && rm && lm != rm && lm->source == rm->source && lm->destination == rm->destination);
		assert(pg_context_map_image(lm, x) != pg_context_map_image(rm, x));
		images[0] = pg_prove_type_value(t, pg_prove_value_type(t, images[0]));
		images[1] = pg_substitution_image(t, left, x);
		const struct pg_evidence *alternate = pg_prove_substitution(t, source, destination, 2, images);
		assert(alternate && alternate != left && pg_evidence_context_map(alternate) == lm);
		assert(pg_context_map_image(lm, x) == pg_evidence_subject(pg_substitution_image(t, alternate, x)));
		const struct pg_source_scope *scope = pg_synthesis_root(&p->synthesis);
		const struct pg_object *binders[] = {a, y, z};
		const struct pg_evidence *contexts[] = {ac, yc, destination};
		const char *names[] = {"A", "y", "z"};
		for (size_t i = 0; i < 3; ++i)
			scope = pg_synthesis_bind_context(&p->synthesis, scope,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = 1},
				binders[i], pg_synthesis_evidence(&p->synthesis, contexts[i]));
		assert(scope);
		struct pg_synthesis_job *direct = pg_synthesis_evidence(&p->synthesis, images[1]);
		struct pg_effect_inference effects;
		assert(!pg_effect_inference_init(&effects, &p->graph));
		const struct pg_derivation_input *const *inputs;
		assert(!pg_synthesis_export_rules(&p->synthesis, 1, &direct, &p->graph, &effects, 1, &inputs));
		assert(inputs[0]->rule == PG_VARIABLE && inputs[0]->count == 1);
		struct pg_derivation_input *invalid = pg_alloc(&p->graph, sizeof(*invalid) + sizeof(void *));
		assert(invalid);
		*invalid = *inputs[0]; invalid->premises[0] = inputs[0]->premises[0];
		invalid->parameters.binder = x;
		struct pg_synthesis_job *producers[4] = {direct, NULL,
			pg_synthesis_evidence(&p->synthesis, pg_substitution_image(t, right, x)), NULL};
		for (size_t n = 0; n < 2; ++n) {
			size_t i = scenario & 2 ? 1 - n : n;
			producers[2 * i + 1] = pg_synthesis_derivation(&p->synthesis, i ? invalid : inputs[0]);
			assert(producers[2 * i + 1] && !pg_synthesis_result(producers[2 * i + 1]));
			if (scenario & 4) {
				while (p->synthesis.ready) {
					assert(p->synthesis.steps < 10000);
					pg_synthesis_advance(&p->synthesis, chunk);
				}
			}
		}
		assert(producers[1] != direct);
		struct pg_syntax syntax = {.kind = PG_SYNTAX_ATOM,
			.token = {.kind = PG_TOKEN_IDENT, .text = "v", .length = 1, .text_length = 1}};
		struct pg_synthesis_job *uses[4];
		struct pg_syntax domain = {.kind = PG_SYNTAX_ATOM,
			.token = {.kind = PG_TOKEN_IDENT, .text = "A", .length = 1, .text_length = 1}};
		struct pg_syntax lambda = {.kind = PG_SYNTAX_LAMBDA,
			.token = {.kind = PG_TOKEN_IDENT, .text = "w", .length = 1, .text_length = 1},
			.left = &domain, .right = &syntax};
		const struct pg_object *allocation = scenario & 8 ? NULL : pg_binder(&p->graph);
		struct pg_synthesis_job *functions[5];
		const struct pg_source_scope *scopes[4];
		for (size_t n = 0; n < 4; ++n) {
			size_t i = scenario & 2 ? 3 - n : n;
			const struct pg_source_scope *named = pg_synthesis_name_job(&p->synthesis, scope, syntax.token, producers[i]);
			assert(named);
			scopes[i] = named;
			struct pg_source_environment environment;
			assert(!pg_synthesis_environment_input(&p->synthesis, named, &environment));
			assert(environment.parent == scope && environment.producer == producers[i]);
			uses[i] = pg_synthesis_request(&p->synthesis, named, &syntax);
			assert(uses[i]);
			/* Allocation identity may be shared without sharing acceptance. */
			struct pg_synthesis_job *binding = scenario & 8
				? pg_synthesis_binding(&p->synthesis, named, &lambda)
				: pg_synthesis_binding_at(&p->synthesis, named, &lambda, allocation);
			assert(binding);
			if (!allocation) allocation = pg_synthesis_binding_binder(binding);
			assert(pg_synthesis_binding_binder(binding) == allocation);
			functions[i] = pg_synthesis_request(&p->synthesis, named, &lambda);
			assert(functions[i]);
		}
		const struct pg_source_scope *shadow = pg_synthesis_name_job(&p->synthesis, scopes[0], syntax.token, producers[2]);
		struct pg_synthesis_job *shadowed = pg_synthesis_request(&p->synthesis, shadow, &syntax);
		assert(shadowed);
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 10000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		assert(pg_synthesis_result(producers[1]) == pg_synthesis_result(direct));
		for (size_t i = 0; i < 4; ++i)
			assert(pg_synthesis_name_job(&p->synthesis, scope, syntax.token, producers[i]) == scopes[i]);
		for (size_t i = 0; i < 3; ++i) {
			const struct pg_evidence *result = pg_synthesis_result(uses[i]);
			assert(result && pg_evidence_subject(result) == pg_context_map_image(i == 2 ? rm : lm, x));
		}
		assert(pg_synthesis_status(uses[3]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(uses[3]));
		const struct pg_occurrence *function_subjects[3];
		for (size_t i = 0; i < 3; ++i) {
			const struct pg_evidence *result = pg_synthesis_result(functions[i]);
			assert(result);
			function_subjects[i] = pg_evidence_subject(result);
			assert(function_subjects[i]->core->kind == PG_LAMBDA);
			assert(function_subjects[i]->core->as.lambda.binder == allocation);
		}
		assert(functions[0] != functions[1]);
		assert(function_subjects[0]->core == function_subjects[1]->core);
		assert(function_subjects[0]->core != function_subjects[2]->core);
		assert(pg_synthesis_status(functions[3]) == PG_SYNTHESIS_REJECTED);
		assert(!pg_synthesis_result(functions[3]));
		assert(pg_evidence_subject(pg_synthesis_result(shadowed)) == pg_context_map_image(rm, x));
		assert(pg_evidence_subject(pg_synthesis_result(uses[0])) == pg_context_map_image(lm, x));
		const struct pg_evidence *type = pg_prove_variable(t, destination, a);
		assert(type != images[0] && pg_evidence_subject(type) == pg_evidence_subject(images[0]));
		const struct pg_evidence *first = scenario & 2 ? images[0] : type;
		const struct pg_evidence *second = scenario & 2 ? type : images[0];
		const struct pg_source_scope *known = pg_synthesis_name(&p->synthesis, scope, syntax.token, first);
		assert(known && pg_synthesis_name(&p->synthesis, scope, syntax.token, second) == known);
		assert(pg_synthesis_name(&p->synthesis, scope, syntax.token, images[1]) != known);
		assert(!pg_synthesis_name(&p->synthesis, scope, syntax.token, pg_synthesis_result(producers[3])));
		pg_effect_inference_destroy(&effects);
		/* Retain a typed reference to the allocation, not just its source AST. */
		functions[4] = pg_synthesis_evidence(&p->synthesis,
			pg_synthesis_result(pg_synthesis_binding_at(&p->synthesis, scopes[0], &lambda, allocation)));
		assert(functions[4]);
		struct pg_synthesis_job *const *roots = functions;
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, 5, roots));
			pg_program_destroy(p);
			rewind(file);
			size_t count;
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 5 && !p->synthesis.steps);
			for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
			assert(!fclose(file));
		}
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 10000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		for (size_t i = 0; i < 3; ++i) {
			assert(pg_synthesis_result(roots[i]));
			function_subjects[i] = pg_evidence_subject(pg_synthesis_result(roots[i]));
		}
		assert(function_subjects[0]->core == function_subjects[1]->core);
		assert(function_subjects[0]->core != function_subjects[2]->core);
		assert(pg_synthesis_result(roots[4]));
		assert(function_subjects[0]->core->as.lambda.binder ==
			pg_evidence_context(pg_synthesis_result(roots[4]))->binder);
		assert(pg_synthesis_status(roots[3]) == PG_SYNTHESIS_REJECTED);
		pg_program_destroy(p);
	}
	puts("source aliases: typed maps determine exact targets; alternate checks and invalid inputs stay independent");
}

static void mapped_alias_producers(void)
{
	const char *source = "Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;};";
	for (unsigned scenario = 0; scenario < 8; ++scenario) {
		uint64_t chunk = scenario & 1 ? 64 : 1;
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
		assert(p && p->root);
		const struct pg_source_scope *scope = pg_program_exports(p, p->scope, p->root);
		struct pg_parser parser;
		struct pg_definition definition;
		const char *function = "f:=\\n:Nat=>\\v:D n=>v @mk k=>alias;";
		pg_parser_init(&parser, &p->graph, function, strlen(function));
		assert(scope && pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *n = pg_synthesis_binding(&p->synthesis, scope, definition.expression);
		struct pg_synthesis_job *v = pg_synthesis_binding(&p->synthesis,
			pg_synthesis_binding_scope(n), definition.expression->right);
		assert(n && v);
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 30000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		const struct pg_evidence *alias = pg_prove_variable(&p->typing,
			pg_synthesis_result(v), pg_synthesis_binding_binder(n));
		assert(alias);
		struct pg_synthesis_job *direct = pg_synthesis_evidence(&p->synthesis, alias);
		struct pg_effect_inference effects;
		assert(!pg_effect_inference_init(&effects, &p->graph));
		const struct pg_derivation_input *const *inputs;
		assert(!pg_synthesis_export_rules(&p->synthesis, 1, &direct, &p->graph, &effects, 1, &inputs));
		struct pg_synthesis_job *imported = pg_synthesis_derivation(&p->synthesis, inputs[0]);
		if (scenario & 2) {
			struct pg_derivation_input project = {.rule = PG_CONTEXT_PROJECTION, .count = 2};
			for (size_t i = 0; i < 512; ++i)
				imported = pg_synthesis_rule(&p->synthesis, &project,
					(struct pg_synthesis_job *[]){v, imported}, NULL, NULL);
			assert(imported && pg_synthesis_status(imported) == PG_SYNTHESIS_PENDING);
		} else {
			while (p->synthesis.ready) {
				assert(p->synthesis.steps < 30000);
				pg_synthesis_advance(&p->synthesis, chunk);
			}
			assert(pg_synthesis_result(imported) == alias);
		}
		assert(imported != direct);
		assert(inputs[0]->rule == PG_VARIABLE && inputs[0]->count == 1);
		struct pg_derivation_input *invalid = pg_alloc(&p->graph, sizeof(*invalid) + sizeof(void *));
		assert(invalid);
		*invalid = *inputs[0]; invalid->premises[0] = inputs[0]->premises[0];
		invalid->parameters.binder = pg_binder(&p->graph);
		struct pg_synthesis_job *producers[] = {direct, imported, pg_synthesis_derivation(&p->synthesis, invalid)};
		struct pg_synthesis_job *bodies[3];
		for (size_t j = 0; j < 3; ++j) {
			size_t i = scenario & 4 ? 2 - j : j;
			const struct pg_source_scope *named = pg_synthesis_name_job(&p->synthesis,
				pg_synthesis_binding_scope(v), (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "alias", .length = 5}, producers[i]);
			bodies[i] = pg_synthesis_request(&p->synthesis, named, definition.expression->right->right);
			assert(bodies[i]);
		}
		pg_effect_inference_destroy(&effects);
		struct pg_synthesis_job *const *roots = bodies;
		for (unsigned round = 0; round < (scenario & 4 ? 2U : 0U); ++round) {
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, 3, roots));
			pg_program_destroy(p);
			rewind(file);
			size_t count;
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 3 && !p->synthesis.steps);
			for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
			assert(!fclose(file));
		}
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 30000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		for (size_t i = 0; i < 2; ++i) {
			if (pg_synthesis_status(roots[i]) != PG_SYNTHESIS_DONE)
				fprintf(stderr, "mapped alias: producer=%zu chunk=%llu status=%d\n", i,
					(unsigned long long)chunk, pg_synthesis_status(roots[i]));
			assert(pg_synthesis_status(roots[i]) == PG_SYNTHESIS_DONE);
		}
		assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(roots[0]))->core,
			pg_evidence_subject(pg_synthesis_result(roots[1]))->core) == 1);
		assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(roots[2]));
		pg_program_destroy(p);
	}
	puts("mapped aliases: producer completion/order, inert resaves and failed obligations preserve context action");
}

static void invalid_binding_rule(FILE *file)
{
	assert(!fseek(file, 16, SEEK_SET));
	uint64_t count;
	assert(!pg_wire_read_u64(file, &count) && !fseek(file, 56, SEEK_SET));
	int found = 0;
	for (size_t i = 0; i < count; ++i) {
		long start = ftell(file);
		uint64_t words[10];
		assert(start >= 0);
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &words[j]));
		if (words[0] == 6) { /* The wire BINDING record now carries no theorem. */
			assert(!words[7]);
			assert(!fseek(file, start + 7 * 8, SEEK_SET) && !pg_wire_write_u64(file, 1));
			rewind(file);
			size_t root_count = 42;
			struct pg_synthesis_job *const *roots = NULL;
			assert(!pg_sources_read(file, 10000, &root_count, &roots));
			assert(root_count == 42 && !roots);
			assert(!fseek(file, start + 7 * 8, SEEK_SET) && !pg_wire_write_u64(file, 0));
			found = 1;
		}
		assert(!fseek(file, start + 80 + (long)words[6], SEEK_SET));
	}
	assert(found);
}

static void source_binding_annotations(void)
{
	const char *sources[] = {"f:=\\x:@=>x;", "f:=\\x:missing=>x;"};
	for (size_t kind = 0; kind < 2; ++kind) {
		for (size_t progress = 0; progress < 3; ++progress) {
			struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
			assert(p);
			struct pg_parser parser;
			struct pg_definition definition;
			pg_parser_init(&parser, &p->graph, sources[kind], strlen(sources[kind]));
			assert(pg_parser_next(&parser, &definition) == 1);
			struct pg_synthesis_job *binding = pg_synthesis_binding(&p->synthesis,
				pg_synthesis_root(&p->synthesis), definition.expression);
			assert(binding);
			struct pg_synthesis_job *body = pg_synthesis_request(&p->synthesis,
				pg_synthesis_binding_scope(binding), definition.expression->right);
			assert(body);
			pg_synthesis_advance(&p->synthesis, progress == 2 ? 1000 : progress);
			struct pg_synthesis_job *const *roots = &body;
			for (size_t round = 0; round < 2; ++round) {
				FILE *file = tmpfile();
				uint64_t steps = p->synthesis.steps;
				size_t proofs = p->typing.proofs.count;
				assert(file && !pg_sources_write(file, &p->synthesis, 1, roots));
				assert(p->synthesis.steps == steps && p->typing.proofs.count == proofs);
				invalid_binding_rule(file);
				pg_program_destroy(p);
				rewind(file);
				size_t count;
				p = pg_sources_read(file, 10000, &count, &roots);
				assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
				assert(!fclose(file));
			}
			while (p->synthesis.ready) {
				assert(p->synthesis.steps < 1000);
				pg_synthesis_advance(&p->synthesis, progress == 1 ? 64 : 1);
			}
			assert(pg_synthesis_status(roots[0]) == (kind ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
			if (kind) assert(!pg_synthesis_result(roots[0]));
			else {
				const struct pg_evidence *result = pg_synthesis_result(roots[0]);
				const struct pg_context *context = pg_evidence_context(result);
				assert(context && !context->parent);
				assert(pg_evidence_subject(result)->core == pg_reference(&p->graph, context->binder));
			}
			pg_program_destroy(p);
		}
	}
	puts("source bindings: syntax and symbols survive inert saves; annotations are checked only by ordinary Solve");
}

static void context_scopes(void)
{
	source_binding_annotations();
	source_alias_targets();
	mapped_alias_producers();
	family_context_scopes();
	associated_context_scope(0);
	associated_context_scope(1);
	puts("source contexts: pending field, IH and graph associations survive inert resaves and ordinary lookup");
}

static struct pg_synthesis_job *handler_change(struct pg_program *p,
	struct pg_synthesis_job *original, const char *text)
{
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	assert(!pg_synthesis_source_input(&p->synthesis, original, &scope, &syntax));
	struct pg_parser parser;
	struct pg_definition definition;
	pg_parser_init(&parser, &p->graph, text, strlen(text));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(&p->synthesis, scope, definition.expression);
	assert(carrier && !pg_synthesis_result(carrier));
	struct pg_synthesis_job *changed = pg_synthesis_request(&p->synthesis, scope, definition.expression);
	assert(changed && !pg_synthesis_result(changed));
	assert(pg_synthesis_request(&p->synthesis, scope, definition.expression) == changed);
	assert(carrier == pg_synthesis_source_handler_carrier(&p->synthesis, scope, definition.expression));
	return changed;
}

static void reserve_handler_carrier(struct pg_program *p,
	const struct pg_source_scope *scope, const struct pg_syntax *syntax)
{
	uint64_t steps = p->synthesis.steps;
	struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(&p->synthesis, scope, syntax);
	assert(carrier && !pg_synthesis_result(carrier));
	assert(carrier == pg_synthesis_source_handler_carrier(&p->synthesis, scope, syntax));
	assert(p->synthesis.steps == steps);
}

static void invalid_handler_binding(FILE *file)
{
	assert(!fflush(file) && !fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		long position = ftell(file);
		uint64_t record[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &record[j]));
		assert(!fseek(file, (long)record[6], SEEK_CUR));
		if (!record[8]) continue;
		const size_t fields[] = {9, 3, 8, 7};
		const uint64_t invalid[] = {2, record[4], 0, 1};
		assert(!record[7]);
		for (size_t j = 0; j < 4; ++j) {
			assert(!fseek(file, position + (long)(8 * fields[j]), SEEK_SET));
			assert(!pg_wire_write_u64(file, invalid[j]));
			rewind(file);
			size_t count;
			struct pg_synthesis_job *const *roots;
			assert(!pg_sources_read(file, 100000, &count, &roots));
			assert(!fseek(file, position + (long)(8 * fields[j]), SEEK_SET));
			assert(!pg_wire_write_u64(file, record[fields[j]]));
		}
		return;
	}
	assert(0);
}

static struct pg_program *handler_base(unsigned operations, const struct pg_source_scope **output)
{
	const char *text = "D:=@{z:*;}; d:=D.z;";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_source_scope *scope = p->scope;
	const struct pg_evidence *type = NULL;
	for (size_t i = 0; i < 2; ++i) {
		struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = i ? "d" : "D", .length = 1};
		struct pg_synthesis_job *value = pg_synthesis_definition(p->root, name);
		assert(pg_synthesis_result(value));
		if (!i) type = pg_synthesis_result(value);
		scope = pg_synthesis_name_job(&p->synthesis, scope, name, value);
	}
	if (operations) {
		const struct pg_operation_declaration *operation = pg_operation_declaration(&p->typing, type, type);
		assert(operation);
		scope = pg_synthesis_name_job(&p->synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "ask", .length = 3},
			pg_synthesis_operation(&p->synthesis, operation));
		assert(scope);
	}
	if (operations > 1) {
		const struct pg_operation_declaration *operation = pg_operation_declaration(&p->typing, type, type);
		assert(operation);
		scope = pg_synthesis_name_job(&p->synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "other", .length = 5},
			pg_synthesis_operation(&p->synthesis, operation));
		assert(scope);
	}
	*output = scope;
	return p;
}

static int handler_scopes(int mode)
{
	const struct pg_source_scope *scope;
	struct pg_program *p = handler_base(mode >= 5 ? 2 : mode >= 2 ? 1 : 0, &scope);
	struct pg_parser parser;
	struct pg_definition definition;
	const char *text = mode >= 2 ? "h:=(ask d) @ask req k=>k req @#return x=>x;" : "h:=d @#return x=>x;";
	if (mode >= 5) text = "h:=(other (ask d)) @ask req k=>k req @other req k=>k req @#return x=>x;";
	if (mode >= 6) text = "h:=(other (ask d)) @ask req k=>(\\v:D=>k v) req @other req k=>k req @#return x=>x;";
	if (mode >= 7) text = "h:=(other (ask d)) @ask req k=>k ((\\v:D=>v) req) @other req k=>k req @#return x=>x;";
	pg_parser_init(&parser, &p->graph, text, strlen(text));
	assert(pg_parser_next(&parser, &definition) == 1);
	const struct pg_source_scope *inner = pg_synthesis_handler_scope(&p->synthesis, scope, definition.expression);
	assert(inner && inner == pg_synthesis_handler_scope(&p->synthesis, scope, definition.expression));
	struct pg_source_environment environment;
	assert(!pg_synthesis_environment_input(&p->synthesis, inner, &environment));
	assert(environment.parent == scope && environment.handler == definition.expression);
	if (mode >= 4) reserve_handler_carrier(p, scope, definition.expression);
	struct pg_synthesis_job *initial[] = {
		parse(p, scope, "{{x:=d;}}.x"), parse(p, inner, "{{f:=&(\\x:D=>x);x:=d;}}.x"), NULL, NULL};
	if (mode >= 4) initial[2] = pg_synthesis_request(&p->synthesis, scope, definition.expression);
	size_t count = mode >= 4 ? 4 : 2;
	struct pg_synthesis_job *const *roots = initial;
	if (mode == 1 || mode == 2 || mode >= 4) {
		pg_synthesis_advance(&p->synthesis, 10000);
		assert(pg_synthesis_result(roots[0]) && pg_synthesis_result(roots[1]));
		struct pg_synthesis_job *original = pg_synthesis_handler(&p->synthesis, scope, NULL, definition.expression);
		if (!pg_synthesis_result(original)) {
			fprintf(stderr, "handler source before save: mode=%d status=%d ready=%d steps=%llu\n",
				mode, pg_synthesis_status(original), p->synthesis.ready != NULL,
				(unsigned long long)p->synthesis.steps);
			pg_program_destroy(p);
			return 1;
		}
		if (mode >= 4) {
			assert(pg_synthesis_result(initial[2]));
			initial[3] = pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(initial[2]));
		}
	}
	if (mode >= 6) {
		/* Keep a real clause-scope root for malformed binding-record checks;
		 * unused internal Lambda scopes need not be retained by the writer. */
		struct pg_synthesis_job *carrier = pg_synthesis_source_handler_carrier(&p->synthesis, scope, definition.expression);
		struct pg_synthesis_job *clause = pg_synthesis_handler_clause(&p->synthesis, inner,
			carrier, definition.expression->items[0].expression);
		const struct pg_source_scope *clause_scope = pg_synthesis_handler_clause_scope(clause);
		assert(clause_scope);
		initial[0] = parse(p, clause_scope, "{{x:=req;}}.x");
	}
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
		assert(p->synthesis.steps == steps);
		if (mode >= 6) invalid_handler_binding(file);
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == (mode >= 4 ? 4u : 2u) && !p->synthesis.steps);
		assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
		assert(!fclose(file));
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&p->synthesis, roots[1], &inner, &syntax));
		assert(!pg_synthesis_environment_input(&p->synthesis, inner, &environment));
		assert(environment.handler && inner == pg_synthesis_handler_scope(&p->synthesis,
			environment.parent, environment.handler));
		if (mode >= 4) reserve_handler_carrier(p, environment.parent, environment.handler);
	}
	struct pg_synthesis_job *changed = NULL, *invalid = NULL;
	if (mode == 4) {
		changed = handler_change(p, roots[2], "h:=(ask d) @ask req k=>req @#return x=>x;");
		invalid = handler_change(p, roots[2], "h:=(ask d) @ask req k=>k k @#return x=>x;");
		assert(!p->synthesis.steps);
	}
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	struct pg_synthesis_job *handler = pg_synthesis_handler(&p->synthesis, environment.parent, NULL, environment.handler);
	const struct pg_evidence *handled = pg_synthesis_result(handler), *value = pg_synthesis_result(roots[1]);
	if (!handled || !value) fprintf(stderr, "handler scopes: handler %d, roots %d %d, steps %llu\n",
		pg_synthesis_status(handler), pg_synthesis_status(roots[0]), pg_synthesis_status(roots[1]),
		(unsigned long long)p->synthesis.steps);
	assert(handled && value);
	if (changed) {
		assert(pg_synthesis_status(changed) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_status(invalid) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(invalid));
		const struct pg_evidence *new_proof = pg_synthesis_result(changed), *old_proof = pg_synthesis_result(roots[3]);
		assert(pg_evidence_subject(new_proof)->core != pg_evidence_subject(old_proof)->core);
		assert(pg_evidence_classifier(new_proof) == pg_evidence_classifier(old_proof));
		const struct pg_source_scope *scope;
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&p->synthesis, changed, &scope, &syntax));
		assert(!pg_synthesis_restore_elimination(&p->synthesis, scope, syntax, NULL));
	}
	int mismatch = 0;
	if (mode >= 4) {
		assert(pg_synthesis_result(roots[2]) && pg_synthesis_result(roots[3]));
		const struct pg_evidence *fresh = pg_synthesis_result(roots[2]), *saved = pg_synthesis_result(roots[3]);
		assert(pg_evidence_rule(fresh) == PG_HANDLER_ELIM && pg_evidence_rule(saved) == PG_HANDLER_ELIM);
		assert(pg_evidence_premise_count(fresh) == pg_evidence_premise_count(saved));
		mismatch = pg_evidence_subject(fresh)->core != pg_evidence_subject(saved)->core;
		fprintf(stderr, "handler origins: operations=%d nested=%d exact_core=%d\n", mode >= 5 ? 2 : 1, mode >= 6, !mismatch);
		for (size_t i = 1; i < pg_evidence_premise_count(saved); i = i == 1 ? 5 : i + 3) {
			const struct pg_evidence *a = pg_evidence_premise(fresh, i), *b = pg_evidence_premise(saved, i);
			fprintf(stderr, "  clause premise %zu: exact_core=%d exact_classifier=%d\n", i,
				pg_evidence_subject(a)->core == pg_evidence_subject(b)->core,
				pg_evidence_classifier(a) == pg_evidence_classifier(b));
			mismatch |= pg_evidence_classifier(a) != pg_evidence_classifier(b);
		}
	}
	const struct pg_effect_row *effects;
	const struct pg_term *result_type;
	assert(pg_effect_type_view(pg_evidence_classifier(handled), &effects, &result_type));
	assert(!pg_effect_count(effects) && result_type == pg_evidence_classifier(value));
	struct pg_nf_job *normal = pg_nf_request(&p->evaluation, &pg_pure_policy, pg_evidence_subject(handled)->core);
	assert(normal && pg_nf_advance(normal, 10000) == PG_NF_DONE);
	assert(pg_nf_result(normal) == pg_application(&p->graph,
		pg_reference(&p->graph, &pg_return_operation), pg_evidence_subject(value)->core));
	pg_program_destroy(p);
	puts("handler scopes: inert owner reconstruction, shared boundary and ordinary effect inference passed");
	return mismatch;
}

static int handler_return_binding(void *owner, const struct pg_source_binding *input)
{
	const struct pg_object **binder = owner;
	if (input->binder != *binder) return 0;
	assert(input->syntax && !input->constructor && !input->slot);
	assert(input->syntax->kind == PG_SYNTAX_CLAUSE && input->syntax->item_count == 1);
	*binder = NULL;
	return 0;
}

static void check_handler_return_binding(struct pg_program *p, struct pg_synthesis_job *job)
{
	const struct pg_occurrence *handled = pg_evidence_subject(pg_synthesis_result(job));
	assert(handled->operand_count >= 2 && handled->operands[1]->core->kind == PG_LAMBDA);
	const struct pg_object *binder = handled->operands[1]->core->as.lambda.binder;
	assert(!pg_synthesis_visit_source_bindings(&p->synthesis, handler_return_binding, &binder) && !binder);
	assert(!pg_synthesis_allocation_object(job));
}

static void handler_save_boundaries(void)
{
	const char *sources[] = {
		"h:=(other (ask d)) @ask req k=>(\\v:D=>k v) req @other req k=>k req @#return x=>x;",
		"h:=(other (ask d)) @ask req k=>k ((\\v:D=>v) req) @other req k=>k req @#return x=>x;",
		"h:=(ask d) @ask req k=>k k @#return x=>x;",
		"h:=(other (ask d)) @ask req k=>k req @#return x=>x;"
	};
	for (size_t test = 0; test < sizeof(sources) / sizeof(*sources); ++test) {
		const struct pg_source_scope *scope;
		struct pg_program *original = handler_base(2, &scope);
		struct pg_parser parser;
		struct pg_definition definition;
		pg_parser_init(&parser, &original->graph, sources[test], strlen(sources[test]));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *selected[] = {
			pg_synthesis_request(&original->synthesis, scope, definition.expression),
			pg_synthesis_definition(original->root, (struct pg_token){.kind = PG_TOKEN_IDENT, .text = "d", .length = 1})};
		assert(selected[0] && selected[1]);
		size_t boundary = 0;
		for (;;) {
			FILE *file = tmpfile();
			int saved = file ? pg_sources_write(file, &original->synthesis, 2, selected) : -1;
			if (saved) fprintf(stderr, "handler boundary: write case=%zu step=%zu status=%d\n",
				test, boundary, pg_synthesis_status(selected[0]));
			assert(file && !saved);
			struct pg_program *loaded = NULL;
			struct pg_synthesis_job *const *roots;
			size_t count;
			for (unsigned round = 0; round < 2; ++round) {
				rewind(file);
				loaded = pg_sources_read(file, 100000, &count, &roots);
				if (!loaded) fprintf(stderr, "handler boundary: read case=%zu step=%zu round=%u\n", test, boundary, round);
				assert(loaded && count == 2 && !loaded->synthesis.steps);
				assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
				assert(!fclose(file));
				if (round) break;
				file = tmpfile();
				assert(file && !pg_sources_write(file, &loaded->synthesis, count, roots));
				pg_program_destroy(loaded);
			}
			while (loaded->synthesis.ready) {
				assert(loaded->synthesis.steps < 10000);
				pg_synthesis_advance(&loaded->synthesis, 1);
			}
			enum pg_synthesis_status expected = test == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE;
			if (pg_synthesis_status(roots[0]) != expected)
				fprintf(stderr, "handler boundary: solve case=%zu step=%zu status=%d expected=%d\n",
					test, boundary, pg_synthesis_status(roots[0]), expected);
			assert(pg_synthesis_status(roots[0]) == expected);
			assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
			if (expected == PG_SYNTHESIS_DONE) {
				const struct pg_evidence *proof = pg_synthesis_result(roots[0]), *value = pg_synthesis_result(roots[1]);
				check_handler_return_binding(loaded, roots[0]);
				const struct pg_effect_row *effects;
				const struct pg_term *result;
				assert(pg_effect_type_view(pg_evidence_classifier(proof), &effects, &result));
				assert(pg_effect_count(effects) == (test == 3 ? 1u : 0u));
				assert(result == pg_evidence_classifier(value));
				struct pg_nf_job *normal = pg_nf_request(&loaded->evaluation, &pg_pure_policy, pg_evidence_subject(proof)->core);
				assert(normal && pg_nf_advance(normal, 10000) == PG_NF_DONE);
				if (test == 3) {
					const struct pg_object *label;
					const struct pg_term *payload, *continuation;
					assert(pg_computation_request_view(pg_nf_result(normal), &label, &payload, &continuation));
					assert(label == pg_effect_label(effects, 0) && payload == pg_evidence_subject(value)->core);
					normal = pg_nf_request(&loaded->evaluation, &pg_pure_policy,
						pg_application(&loaded->graph, continuation, payload));
					assert(normal && pg_nf_advance(normal, 10000) == PG_NF_DONE);
				}
				assert(pg_nf_result(normal) == pg_application(&loaded->graph,
					pg_reference(&loaded->graph, &pg_return_operation), pg_evidence_subject(value)->core));
			}
			pg_program_destroy(loaded);
			if (!original->synthesis.ready) break;
			assert(boundary++ < 10000);
			pg_synthesis_advance(&original->synthesis, 1);
		}
		assert(pg_synthesis_status(selected[0]) == (test == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		if (test != 2) check_handler_return_binding(original, selected[0]);
		printf("handler save boundaries: case %zu, %zu snapshots preserve status and effects\n", test, boundary + 1);
		pg_program_destroy(original);
	}
}

static void declaration_members(void)
{
	for (unsigned mode = 0; mode < 4; ++mode) {
		struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		struct pg_parser parser;
		struct pg_definition definition;
		const char *text = "Nat:=@{zero:*;succ:*->*;};";
		pg_parser_init(&parser, &p->graph, text, strlen(text));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *family = pg_synthesis_request(&p->synthesis, p->scope, definition.expression);
		pg_synthesis_advance(&p->synthesis, 10000);
		struct pg_inductive_instance instance;
		assert(pg_inductive_instance(&p->typing, pg_synthesis_result(family), &instance));
		struct pg_constructor_allocation allocation;
		assert(pg_synthesis_declaration_member_input(&p->synthesis, family, 1, &allocation) == 1);
		assert(allocation.fields && !allocation.prefix);
		const struct pg_object *binder = allocation.fields->binder;
		if (mode == 1) allocation.fields = pg_context_bind(&p->typing, NULL, binder, pg_universe(&p->graph, 0), PG_JUDGEMENT_VALUE);
		if (mode == 2) allocation.constructor = pg_data_constructor(pg_data_schema_layout(instance.schema), 0);
		if (mode == 3) allocation.fields = NULL;
		pg_synthesis_destroy(&p->synthesis);
		assert(!pg_synthesis_init(&p->synthesis, &p->typing, &p->evaluation, PG_DEFINITION_EXPLICIT_THUNK));
		p->scope = pg_synthesis_root(&p->synthesis);
		family = pg_synthesis_declaration_at(&p->synthesis, p->scope, definition.expression,
			pg_data_schema_declaration(instance.schema));
		assert(family && !pg_synthesis_declaration_member_at(&p->synthesis, family, 1, &allocation));
		assert(!pg_synthesis_declaration_member_at(&p->synthesis, family, 1, &allocation));
		assert(pg_synthesis_declaration_member_at(&p->synthesis, family, 2, &allocation));
		struct pg_constructor_allocation conflicting = allocation;
		conflicting.constructor = pg_binder(&p->graph);
		assert(pg_synthesis_declaration_member_at(&p->synthesis, family, 1, &conflicting));
		struct pg_constructor_allocation retained;
		assert(pg_synthesis_declaration_member_input(&p->synthesis, family, 1, &retained) == 1);
		assert(retained.fields == allocation.fields && !p->synthesis.steps);
		p->scope = pg_synthesis_name_job(&p->synthesis, p->scope, definition.name, family);
		const char *selection = "r:=Nat.succ;";
		pg_parser_init(&parser, &p->graph, selection, strlen(selection));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *use = pg_synthesis_request(&p->synthesis, p->scope, definition.expression);
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 10000);
			pg_synthesis_advance(&p->synthesis, mode & 1 ? 64 : 1);
		}
		assert(pg_synthesis_status(use) == (mode < 2 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED));
		if (mode < 2) {
			const struct pg_evidence *formation = pg_synthesis_result(family);
			const struct pg_evidence *empty = pg_prove_empty_context(&p->typing);
			struct pg_synthesis_job *member = pg_synthesis_constructor_value(&p->synthesis, formation, allocation.constructor,
				pg_prove_substitution_projection(&p->typing, empty, empty));
			assert(pg_synthesis_status(member) == PG_SYNTHESIS_DONE);
			const struct pg_term *core = pg_evidence_subject(pg_synthesis_result(member))->core;
			assert(core->kind == PG_LAMBDA && core->as.lambda.binder == binder);
			assert(pg_evidence_subject(pg_synthesis_result(use))->core == core);
		}
		struct pg_constructor_allocation zero = {pg_data_constructor(pg_data_schema_layout(instance.schema), 0), NULL, NULL};
		assert(pg_synthesis_declaration_member_at(&p->synthesis, family, 0, &zero));
		pg_program_destroy(p);
	}
	puts("source member allocations: pre-publication restore, evidence sharing, type recheck and invalid inputs passed");
}

static long source_binding_section(FILE *file, size_t *count)
{
	assert(!fflush(file) && !fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		assert(!fseek(file, (long)scope[6], SEEK_CUR));
	}
	assert(!fseek(file, (long)(header[2] + 6 * header[4] + 3 * header[5] + 3 * header[3]) * 8, SEEK_CUR));
	struct pg_graph graph = {0};
	assert(!pg_graph_init(&graph));
	const struct pg_syntax *const *syntax;
	assert(!pg_syntax_read(file, &graph, 10000, count, &syntax));
	pg_graph_destroy(&graph);
	long offset = ftell(file);
	assert(offset >= 0);
	return offset;
}

static void invalid_source_bindings(FILE *file)
{
	size_t syntax_count;
	long offset = source_binding_section(file, &syntax_count);
	uint64_t count;
	assert(!pg_wire_read_u64(file, &count) && count);
	for (size_t i = 0; i < count; ++i) {
		uint64_t record[3];
		long start = offset + 8 + (long)i * 24;
		assert(!fseek(file, start, SEEK_SET));
		for (size_t j = 0; j < 3; ++j) assert(!pg_wire_read_u64(file, &record[j]));
		const uint64_t invalid[] = {syntax_count + 1, UINT64_MAX, UINT64_MAX};
		for (size_t j = 0; j < (record[0] ? 2u : 3u); ++j) {
			assert(!fseek(file, start + (long)j * 8, SEEK_SET) && !pg_wire_write_u64(file, invalid[j]));
			rewind(file);
			size_t roots_count = 42;
			struct pg_synthesis_job *const *roots = NULL;
			assert(!pg_sources_read(file, 100000, &roots_count, &roots));
			assert(roots_count == 42 && !roots);
			assert(!fseek(file, start + (long)j * 8, SEEK_SET) && !pg_wire_write_u64(file, record[j]));
		}
	}
}

static void invalid_declaration_members(FILE *file)
{
	size_t count;
	source_binding_section(file, &count);
	uint64_t bindings, matches, metadata, members;
	assert(!pg_wire_read_u64(file, &bindings) && !fseek(file, (long)bindings * 3 * 8, SEEK_CUR));
	assert(!pg_wire_read_u64(file, &matches) && !fseek(file, (long)matches * 8, SEEK_CUR));
	assert(!fseek(file, 8, SEEK_CUR)); /* Nominal declaration reference count. */
	assert(!pg_wire_read_u64(file, &metadata) && !fseek(file, (long)metadata * 8, SEEK_CUR));
	long count_offset = ftell(file);
	assert(count_offset >= 0 && !pg_wire_read_u64(file, &members) && members == 2);
	uint64_t record[2];
	assert(!pg_wire_read_u64(file, &record[0]) && !pg_wire_read_u64(file, &record[1]));
	const long offsets[] = {count_offset, count_offset + 8, count_offset + 16};
	const uint64_t originals[] = {members, record[0], record[1]};
	const uint64_t invalid[] = {UINT64_MAX, 0, 2};
	for (size_t i = 0; i < 3; ++i) {
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, invalid[i]));
		rewind(file);
		struct pg_synthesis_job *const *roots = NULL;
		count = 42;
		assert(!pg_sources_read(file, 10000, &count, &roots));
		assert(count == 42 && !roots);
		assert(!fseek(file, offsets[i], SEEK_SET) && !pg_wire_write_u64(file, originals[i]));
	}
}

static void invalid_match_allocations(FILE *file)
{
	size_t count;
	source_binding_section(file, &count);
	uint64_t bindings, matches, branches;
	assert(!pg_wire_read_u64(file, &bindings) && !fseek(file, (long)bindings * 24, SEEK_CUR));
	long offset = ftell(file);
	assert(offset >= 0 && !pg_wire_read_u64(file, &matches) && matches);
	assert(!pg_wire_read_u64(file, &branches));
	const uint64_t values[] = {matches, branches};
	for (size_t i = 0; i < 2; ++i) {
		assert(!fseek(file, offset + (long)i * 8, SEEK_SET) && !pg_wire_write_u64(file, UINT64_MAX));
		rewind(file);
		struct pg_synthesis_job *const *roots = NULL;
		count = 42;
		assert(!pg_sources_read(file, 100000, &count, &roots) && count == 42 && !roots);
		assert(!fseek(file, offset + (long)i * 8, SEEK_SET) && !pg_wire_write_u64(file, values[i]));
	}
}

static void declaration_member_images(void)
{
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	struct pg_parser parser;
	struct pg_definition definition;
	const char *text = "Nat:=@{zero:*;succ:*->*;};";
	pg_parser_init(&parser, &p->graph, text, strlen(text));
	assert(pg_parser_next(&parser, &definition) == 1);
	struct pg_synthesis_job *initial[] = {pg_synthesis_request(&p->synthesis, p->scope, definition.expression)};
	struct pg_synthesis_job *const *roots = initial;
	size_t count = 1;
	pg_synthesis_advance(&p->synthesis, 10000);
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
		assert(p->synthesis.steps == steps);
		if (!round) invalid_declaration_members(file);
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
		assert(!fclose(file));
		for (size_t i = 0; i < 2; ++i) {
			struct pg_constructor_allocation input;
			assert(pg_synthesis_declaration_member_input(&p->synthesis, roots[0], i, &input) == 1);
			assert(!input.prefix && (input.fields != NULL) == (i == 1));
		}
	}
	struct pg_constructor_allocation input;
	assert(pg_synthesis_declaration_member_input(&p->synthesis, roots[0], 1, &input) == 1);
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *formation = pg_synthesis_result(roots[0]);
	const struct pg_evidence *empty = pg_prove_empty_context(&p->typing);
	struct pg_synthesis_job *member = pg_synthesis_constructor_value(&p->synthesis, formation, input.constructor,
		pg_prove_substitution_projection(&p->typing, empty, empty));
	assert(member && pg_synthesis_status(member) == PG_SYNTHESIS_DONE);
	const struct pg_term *core = pg_evidence_subject(pg_synthesis_result(member))->core;
	assert(core->kind == PG_LAMBDA && core->as.lambda.binder == input.fields->binder);
	pg_program_destroy(p);
	puts("source member images: declaration-only roots retain nullary/field allocations through three inert resaves");
}

static void constructor_inputs(void)
{
	for (unsigned mode = 0; mode < 5; ++mode) {
		struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		struct pg_parser parser;
		struct pg_definition definition;
		const char *text = "Nat:=@{zero:*;succ:*->*;};";
		pg_parser_init(&parser, &p->graph, text, strlen(text));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *family = pg_synthesis_request(&p->synthesis,
			pg_synthesis_root(&p->synthesis), definition.expression);
		pg_synthesis_advance(&p->synthesis, 10000);
		const struct pg_evidence *formation = pg_synthesis_result(family);
		struct pg_inductive_instance instance;
		assert(formation && pg_inductive_instance(&p->typing, formation, &instance));
		const struct pg_object *constructor = mode == 3 ? pg_binder(&p->graph)
			: pg_data_constructor(pg_data_schema_layout(instance.schema), mode == 4 ? 0 : 1);
		/* A distinct pending parameter producer also avoids reusing the member
		 * eagerly created by source declaration publication. */
		const struct pg_evidence *empty = pg_prove_empty_context(&p->typing);
		struct pg_synthesis_job *parameters = pg_synthesis_substitution(&p->synthesis, empty, empty, 0, NULL);
		struct pg_synthesis_job *initial[] = {pg_synthesis_constructor_value_jobs(&p->synthesis, family, constructor, parameters)};
		if (mode == 1) {
			const struct pg_evidence *map = pg_prove_constructor_scope(&p->typing, formation, constructor, instance.parameters);
			assert(map);
			const struct pg_context *fields = pg_context_bind(&p->typing, NULL,
				pg_evidence_context(map)->binder, pg_universe(&p->graph, 0), PG_JUDGEMENT_VALUE);
			assert(map && pg_synthesis_constructor_scope_at(&p->synthesis, family, constructor, parameters,
				NULL, fields));
		}
		if (mode == 2) pg_synthesis_advance(&p->synthesis, 10000);
		struct pg_synthesis_job *const *roots = initial;
		size_t count = 1;
		for (unsigned round = 0; round < 3; ++round) {
			uint64_t steps = p->synthesis.steps;
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
			assert(p->synthesis.steps == steps);
			pg_program_destroy(p);
			rewind(file);
			p = pg_sources_read(file, 10000, &count, &roots);
			assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
			assert(!fclose(file));
		}
		struct pg_constructor_input input;
		assert(!pg_synthesis_constructor_input(&p->synthesis, roots[0], &input));
		assert(input.allocated == (mode == 1 || mode == 2));
		pg_synthesis_advance(&p->synthesis, 10000);
		if (mode == 3) {
			assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_REJECTED);
		} else if (mode == 4) {
			const struct pg_evidence *result = pg_synthesis_result(roots[0]);
			assert(result && pg_evidence_judgement(result) == PG_JUDGEMENT_VALUE);
			assert(pg_evidence_subject(result)->core == pg_reference(&p->graph, input.constructor));
		} else {
			const struct pg_evidence *result = pg_synthesis_result(roots[0]);
			assert(result);
			const struct pg_term *core = pg_evidence_subject(result)->core;
			assert(core->kind == PG_LAMBDA);
			if (input.allocated) assert(core->as.lambda.binder == input.fields->binder);
			const struct pg_term *field = pg_reference(&p->graph, core->as.lambda.binder);
			const struct pg_term *body = pg_application(&p->graph, pg_reference(&p->graph, input.constructor), field);
			body = pg_application(&p->graph, pg_reference(&p->graph, &pg_return_operation), body);
			assert(core == pg_lambda(&p->graph, core->as.lambda.binder, body));
			const struct pg_term *domain, *codomain;
			const struct pg_object *binder;
			assert(pg_pi_view(pg_evidence_classifier(result), &domain, &binder, &codomain));
			assert(binder == core->as.lambda.binder);
			assert(domain == pg_evidence_subject(pg_synthesis_result(input.formation))->core);
			assert(codomain == pg_computation_type(&p->graph, PG_TOTALITY_TOTAL, pg_effect_row(&p->graph, 0, NULL), domain));
		}
		pg_program_destroy(p);
	}
	puts("source constructor inputs: pending/completed owners, field allocation and invalid labels survive inert resaves");
	declaration_members();
	declaration_member_images();
}

static void invalid_normalization_mode(FILE *file)
{
	assert(!fflush(file) && !fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		assert(!fseek(file, (long)scope[6], SEEK_CUR));
	}
	assert(!fseek(file, (long)(8 * header[2]), SEEK_CUR));
	for (size_t i = 0; i < header[4]; ++i) {
		long position = ftell(file);
		uint64_t record[6];
		for (size_t j = 0; j < 6; ++j) assert(!pg_wire_read_u64(file, &record[j]));
		if (record[0] || !record[4] || !record[5]) continue;
		assert(record[1] == 1 || record[1] == 2);
		assert(!fseek(file, position + 8, SEEK_SET) && !pg_wire_write_u64(file, 99));
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		assert(!pg_sources_read(file, 100000, &count, &roots));
		assert(!fseek(file, position + 8, SEEK_SET) && !pg_wire_write_u64(file, record[1]));
		return;
	}
	assert(0);
}

static void normalization_origin(void)
{
	const char *text = "{{ id:=&(\\A:@ => \\x:A => x); }}.id";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_evidence *proof = pg_synthesis_result(p->root);
	assert(proof);
	const struct pg_evidence *forced = pg_prove_force(&p->typing, proof);
	assert(forced);
	struct pg_synthesis_job *initial[] = {p->root, pg_synthesis_evidence(&p->synthesis, proof),
		pg_synthesis_evidence(&p->synthesis, forced)};
	struct pg_synthesis_job *const *roots = initial;
	size_t count = 3;
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 3 && !p->synthesis.steps);
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		assert(!fclose(file));
		if (!round) continue; /* Resave before any Solve or proof admission. */
		pg_synthesis_advance(&p->synthesis, 10000);
		const struct pg_evidence *source = pg_synthesis_result(roots[0]);
		const struct pg_evidence *rule = pg_synthesis_result(roots[1]);
		assert(source && rule);
		assert(pg_evidence_subject(source)->core == pg_evidence_subject(rule)->core);
		assert(pg_evidence_classifier(source) == pg_evidence_classifier(rule));
		const struct pg_evidence *actual = pg_prove_force(&p->typing, source);
		const struct pg_evidence *retained = pg_synthesis_result(roots[2]);
		assert(actual && retained);
		assert(pg_evidence_subject(actual)->core == pg_evidence_subject(retained)->core);
	}
	pg_program_destroy(p);
	puts("source normalization origin: source and retained rule share exact Core after destroying resaves");
}

static struct pg_program *retained_program(const char *text)
{
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_evidence *forced = pg_prove_force(&p->typing, pg_synthesis_result(p->root));
	assert(forced);
	struct pg_nf_job *computed = pg_nf_request(&p->evaluation, &pg_pure_policy, pg_evidence_subject(forced)->core);
	assert(pg_nf_advance(computed, 10000) == PG_NF_DONE);
	/* Local snapshot assembly uses the existing private archive layout. */
	struct pg_reduction_archive *archive = pg_alloc(&p->graph, sizeof(*archive));
	const struct pg_reduction_certificate **root = pg_alloc(&p->graph, sizeof(*root));
	assert(archive && root);
	*root = pg_nf_certificate(computed);
	*archive = (struct pg_reduction_archive){.count = 1, .roots = root};
	p->retained_reductions = archive;
	return p;
}

struct binding_check {
	struct pg_synthesis *synthesis;
	size_t count;
	enum pg_syntax_kind kind;
};

static int check_source_allocation(void *owner, const struct pg_source_binding *input)
{
	struct binding_check *check = owner;
	if (input->syntax && input->syntax->kind == check->kind) {
		assert(input->slot == 0 && !input->scope_count);
		++check->count;
	}
	assert(input->binder && input->binder->kind == PG_BINDER);
	uint64_t steps = check->synthesis->steps;
	size_t jobs = check->synthesis->jobs.count, proofs = check->synthesis->typing->proofs.count;
	assert(pg_synthesis_source_binding(check->synthesis, input) == input);
	struct pg_source_binding conflict = *input;
	conflict.binder = pg_binder(check->synthesis->typing->graph);
	assert(!pg_synthesis_source_binding(check->synthesis, &conflict));
	if (input->syntax && input->syntax->kind == PG_SYNTAX_CLAUSE) {
		conflict = *input;
		conflict.slot = 3;
		assert(!pg_synthesis_source_binding(check->synthesis, &conflict));
		if (input->syntax->item_count == 1) {
			conflict.slot = 1;
			assert(!pg_synthesis_source_binding(check->synthesis, &conflict));
		}
	}
	assert(check->synthesis->steps == steps && check->synthesis->jobs.count == jobs);
	assert(check->synthesis->typing->proofs.count == proofs);
	return 0;
}

static void application_origins(void)
{
	struct pg_program *p = retained_program("{{ Nat:=@{zero:*;succ:*->*;}; r:=&(Nat.succ (Nat.succ Nat.zero)); }}.r");
	for (unsigned round = 0; round < 3; ++round) {
		struct binding_check check = {&p->synthesis, 0, PG_SYNTAX_APPLICATION};
		assert(!pg_synthesis_visit_source_bindings(&p->synthesis, check_source_allocation, &check) && check.count == 1);
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write_retained(file, &p->synthesis, 1, &p->root, p->retained_reductions));
		assert(p->synthesis.steps == steps);
		invalid_source_bindings(file);
		pg_program_destroy(p);
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 1 && !fclose(file));
		assert(!p->synthesis.steps && !pg_synthesis_result(p->root));
	}
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *proof = pg_synthesis_result(p->root);
	assert(proof);
	proof = pg_prove_force(&p->typing, proof);
	assert(proof && pg_evidence_subject(proof)->core == pg_reduction_source(p->retained_reductions->roots[0]));
	pg_program_destroy(p);
	puts("application origins: raw sequencing allocations survive inert resaves; ordinary Solve preserves exact Core");
}

static void fold_origins(void)
{
	struct pg_program *p = retained_program("{{ Nat:=@{zero:*;succ:*->*;}; r:=&((Nat.succ Nat.zero) @#return x=>Nat.succ x); }}.r");
	for (unsigned round = 0; round < 3; ++round) {
		struct binding_check check = {&p->synthesis, 0, PG_SYNTAX_CLAUSE};
		assert(!pg_synthesis_visit_source_bindings(&p->synthesis, check_source_allocation, &check) && check.count == 1);
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write_retained(file, &p->synthesis, 1, &p->root, p->retained_reductions));
		assert(p->synthesis.steps == steps);
		pg_program_destroy(p);
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(p->root) && !fclose(file));
	}
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *forced = pg_prove_force(&p->typing, pg_synthesis_result(p->root));
	assert(forced && pg_evidence_subject(forced)->core == pg_reduction_source(p->retained_reductions->roots[0]));
	struct pg_synthesis_job *bad = parse(p, p->scope,
		"{{ Nat:=@{zero:*;succ:*->*;}; r:=&((Nat.succ Nat.zero) @#return x=>(x :: @)); }}.r");
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(bad));
	pg_program_destroy(p);
	puts("Fold bindings: exact continuation allocation, inert resaves and symbol/type rejection passed");
}

struct match_origin_check { struct pg_program *program; size_t count; int solved; };

static const struct pg_evidence *before_branch_conversion(const struct pg_evidence *proof)
{
	while (pg_evidence_rule(proof) == PG_TYPE_CONVERSION || pg_evidence_rule(proof) == PG_EFFECT_SUBSUMPTION)
		proof = pg_evidence_premise(proof, 0);
	return proof;
}

static void induction_scope_inputs(struct pg_program *p, const struct pg_evidence *proof)
{
	const struct pg_evidence *formation = pg_evidence_premise(proof, 1), *parameters = pg_evidence_premise(proof, 2);
	const struct pg_evidence *motive_context = pg_evidence_premise(proof, 4), *motive = pg_evidence_premise(proof, 0);
	const struct pg_evidence *branch = before_branch_conversion(pg_evidence_premise(proof, 6));
	assert(pg_evidence_rule(branch) == PG_LAMBDA_INTRO);
	const struct pg_context *fields = pg_evidence_context(pg_evidence_premise(pg_evidence_premise(branch, 0), 0));
	branch = before_branch_conversion(pg_evidence_premise(branch, 1));
	assert(pg_evidence_rule(branch) == PG_LAMBDA_INTRO);
	const struct pg_context *end = pg_evidence_context(pg_evidence_premise(pg_evidence_premise(branch, 0), 0));
	assert(end->parent == fields);
	struct pg_inductive_instance instance;
	assert(pg_inductive_instance(&p->typing, formation, &instance));
	const struct pg_object *constructor = pg_data_constructor(pg_data_schema_layout(instance.schema), 1);
	for (unsigned mode = 0; mode < 5; ++mode) {
		struct pg_synthesis restored;
		assert(!pg_synthesis_init(&restored, &p->typing, &p->evaluation, PG_DEFINITION_EXPLICIT_THUNK));
		struct pg_synthesis_job *f = pg_synthesis_evidence(&restored, formation), *ps = pg_synthesis_evidence(&restored, parameters);
		struct pg_synthesis_job *mc = pg_synthesis_evidence(&restored, motive_context), *m = pg_synthesis_evidence(&restored, motive);
		const struct pg_context *prefix = fields, *allocation = end;
		if (mode == 1) allocation = pg_context_bind(&p->typing, fields, end->binder, pg_universe(&p->graph, 0), PG_JUDGEMENT_VALUE);
		if (mode == 2) allocation = fields;
		if (mode == 3) allocation = pg_context_bind(&p->typing, end, pg_binder(&p->graph), end->declared_type, PG_JUDGEMENT_VALUE);
		if (mode == 4) prefix = fields->parent;
		assert(pg_synthesis_constructor_scope_at(&restored, f, constructor, ps, fields->parent, fields));
		struct pg_synthesis_job *job = pg_synthesis_induction_scope_at(&restored, f, constructor, ps, mc, m, prefix, allocation);
		assert(job && !restored.steps && !pg_synthesis_result(job));
		assert(pg_synthesis_induction_scope(&restored, f, constructor, ps, mc, m) == job);
		assert(pg_synthesis_induction_scope_at(&restored, f, constructor, ps, mc, m, prefix, allocation) == job);
		while (restored.ready) {
			assert(restored.steps < 10000);
			pg_synthesis_advance(&restored, mode & 1 ? 64 : 1);
		}
		assert(pg_synthesis_status(job) == (mode < 2 ? PG_SYNTHESIS_DONE : PG_SYNTHESIS_REJECTED));
		if (mode < 2) assert(pg_evidence_context(pg_evidence_premise(pg_synthesis_result(job), 1)) == end);
		const struct pg_context *other = pg_context_bind(&p->typing, fields, pg_binder(&p->graph), end->declared_type, PG_JUDGEMENT_VALUE);
		assert(!pg_synthesis_induction_scope_at(&restored, f, constructor, ps, mc, m, fields, other));
		pg_synthesis_destroy(&restored);
	}
}

static int check_match_origin(void *owner, struct pg_synthesis_job *job)
{
	struct match_origin_check *check = owner;
	struct pg_program *p = check->program;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(&p->synthesis, job, &scope, &syntax) || syntax->kind != PG_SYNTAX_ELIMINATION) return 0;
	++check->count;
	struct pg_graph storage = {0};
	const struct pg_match_allocation *allocation = pg_synthesis_match_allocation(job, &storage);
	assert(allocation);
	if (!check->solved) {
		assert(!pg_synthesis_result(job));
		assert(pg_synthesis_restore_elimination(&p->synthesis, scope, syntax, allocation) == job);
		assert(!pg_synthesis_restore_elimination(&p->synthesis, scope, syntax, NULL));
		struct pg_match_allocation bad = *allocation;
		bad.induction.count = SIZE_MAX;
		assert(!pg_synthesis_restore_elimination(&p->synthesis, scope, syntax, &bad));
		bad = *allocation;
		bad.motive = NULL;
		assert(!pg_synthesis_restore_elimination(&p->synthesis, scope, syntax, &bad));
		pg_graph_destroy(&storage);
		return 0;
	}
	const struct pg_induction_allocation *expected = &allocation->induction;
	const struct pg_induction_allocation *actual = pg_evidence_induction_allocation(pg_synthesis_result(job));
	assert(expected && actual && actual->count == expected->count);
	assert(actual->self == expected->self && actual->argument == expected->argument && actual->recursion == expected->recursion);
	for (size_t i = 0; i < actual->count; ++i) assert(actual->clauses[i] == expected->clauses[i]);
	const struct pg_evidence *fresh = pg_synthesis_result(job);
	struct pg_elimination_inputs view;
	assert(!pg_elimination_view(&p->typing, fresh, &view));
	const struct pg_evidence *motive = pg_prove_thunk_content(&p->typing,
		pg_prove_thunk_type(&p->typing, view.motive));
	assert(motive && motive != view.motive);
	assert(pg_evidence_subject(motive) == pg_evidence_subject(view.motive));
	const struct pg_evidence **branches = pg_alloc(&storage, view.count * sizeof(*branches));
	assert(branches);
	for (size_t i = 0; i < view.count; ++i) branches[i] = pg_evidence_premise(fresh, i + 5);
	const struct pg_evidence *alternate = pg_prove_induction_at(&p->typing,
		view.formation, view.parameters, view.scrutinee, view.motive_context, motive,
		view.count, branches, pg_evidence_induction_allocation(fresh));
	assert(alternate && alternate != fresh);
	assert(pg_evidence_subject(alternate) == pg_evidence_subject(fresh));
	assert(pg_evidence_premise(alternate, 0) == motive);
	size_t proofs = p->typing.proofs.count, subjects = p->typing.occurrences.count;
	size_t jobs = p->synthesis.jobs.count;
	size_t queries = p->typing.typed_queries.count, actions = p->typing.occurrence_actions.count;
	size_t contexts = p->typing.contexts.count, maps = p->typing.context_maps.count;
	const struct pg_typing *typing = &p->typing;
	const struct pg_evidence *variants[] = {fresh, alternate};
	for (unsigned read = 0; read < 2; ++read) {
		const struct pg_evidence *proof = variants[read];
		assert(!pg_elimination_view(typing, proof, &view));
		assert(view.subject == pg_evidence_subject(proof) && view.count == actual->count);
		assert(pg_evidence_subject(view.motive) == view.subject->operands[view.count + 1]);
		assert(pg_evidence_context(view.motive_context) == pg_evidence_context(view.motive));
		assert(pg_evidence_subject(view.formation) == view.subject->operands[view.count + 2]);
		assert(pg_evidence_subject(view.scrutinee) == view.subject->operands[0]);
		assert(pg_evidence_context_map(view.parameters) == pg_occurrence_maps(view.subject)[0]);
	}
	assert(p->typing.proofs.count == proofs && p->typing.occurrences.count == subjects);
	assert(p->synthesis.jobs.count == jobs);
	assert(p->typing.typed_queries.count == queries && p->typing.occurrence_actions.count == actions);
	assert(p->typing.contexts.count == contexts && p->typing.context_maps.count == maps);
	assert(pg_elimination_view(&p->typing, view.formation, &view));
	assert(pg_elimination_view(&p->typing, NULL, &view));
	assert(pg_elimination_view(&p->typing, fresh, NULL));
	struct pg_typing foreign;
	assert(!pg_typing_init(&foreign, &p->graph));
	assert(pg_elimination_view(&foreign, fresh, &view));
	pg_typing_destroy(&foreign);
	for (size_t i = 0; i < actual->count; ++i) {
		size_t count;
		assert(!pg_context_extension_size(allocation->branches[i], allocation->prefix, &count));
		const struct pg_occurrence *branch = pg_evidence_subject(fresh)->operands[i + 1];
		for (size_t j = 0; j < count; ++j) {
			branch = pg_occurrence_scoped_input(branch, 0);
			assert(branch);
		}
		const struct pg_context *a = allocation->branches[i], *b = branch->context;
		for (size_t j = 0; j < count; ++j, a = a->parent, b = b->parent) assert(a->binder == b->binder);
	}
	induction_scope_inputs(p, fresh);
	pg_graph_destroy(&storage);
	return 0;
}

static void match_motive_authority(void)
{
	for (unsigned scenario = 0; scenario < 4; ++scenario) {
		uint64_t chunk = scenario & 1 ? 64 : 1;
		const char *source = "Nat:=@{zero:*;succ:*->*;};";
		struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
		assert(p && p->root);
		const struct pg_source_scope *scope = pg_program_exports(p, p->scope, p->root);
		struct pg_parser parser;
		struct pg_definition definition;
		const char *function = "f:=\\n:Nat=>n @zero=>Nat.zero @succ k=>Nat.succ *k;";
		pg_parser_init(&parser, &p->graph, function, strlen(function));
		assert(scope && pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *binding = pg_synthesis_binding(&p->synthesis, scope, definition.expression);
		assert(binding);
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 30000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		struct pg_typing *typing = &p->typing;
		const struct pg_evidence *formation = pg_synthesis_result(pg_synthesis_definition(p->root,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "Nat", .length = 3}));
		const struct pg_evidence *context = pg_synthesis_result(binding);
		const struct pg_evidence *empty = pg_prove_empty_context(typing);
		const struct pg_evidence *parameters = pg_prove_substitution_projection(typing, empty, context);
		const struct pg_evidence *scrutinee = pg_prove_variable(typing, context, pg_synthesis_binding_binder(binding));
		const struct pg_evidence *mc = pg_prove_inductive_motive_context(typing, formation, parameters, pg_binder(&p->graph));
		const struct pg_evidence *nat = pg_prove_reindex(typing, parameters, formation);
		const struct pg_effect_row *effects = pg_effect_row(&p->graph, 0, NULL);
		const struct pg_evidence *motive = pg_prove_computation_type(typing, PG_TOTALITY_UNSPECIFIED, effects,
			pg_prove_projection(typing, mc, nat));
		const struct pg_data_layout *layout = pg_data_schema_layout(pg_evidence_inductive_schema(formation));
		assert(motive && layout);
		const struct pg_evidence *branches[2];
		for (size_t i = 0; i < 2; ++i) {
			const struct pg_evidence *map = pg_prove_induction_scope(typing, formation,
				pg_data_constructor(layout, i), parameters, mc, motive);
			const struct pg_evidence *fields = pg_evidence_premise(map, 1);
			const struct pg_evidence *zero = pg_prove_constructor(typing, formation, pg_data_constructor(layout, 0),
				pg_prove_substitution_projection(typing, empty, fields), 0, NULL);
			const struct pg_evidence *type = pg_prove_computation_type(typing, PG_TOTALITY_UNSPECIFIED, effects,
				pg_prove_projection(typing, fields, nat));
			branches[i] = pg_prove_abstract(typing, context, fields,
				pg_prove_effect_subsumption(typing, pg_prove_return_contract(typing, PG_TOTALITY_TOTAL, zero), type));
			assert(branches[i]);
		}
		/* The saved theorem is deliberately weaker than the source program.
		 * Its allocation is reusable; its result is not an expected type. */
		const struct pg_evidence *saved = pg_prove_induction(typing, formation, parameters, scrutinee, mc, motive, 2, branches);
		assert(saved);
		struct pg_synthesis_job *receipt = pg_synthesis_evidence(&p->synthesis, saved);
		struct pg_effect_inference inference;
		assert(!pg_effect_inference_init(&inference, &p->graph));
		const struct pg_derivation_input *const *inputs;
		assert(!pg_synthesis_export_rules(&p->synthesis, 1, &receipt, &p->graph, &inference, 1, &inputs));
		struct pg_synthesis_job *origin = pg_synthesis_derivation(&p->synthesis, inputs[0]);
		const struct pg_match_allocation *allocation = pg_synthesis_match_allocation(receipt, &p->graph);
		struct pg_synthesis_job *job = pg_synthesis_restore_elimination(&p->synthesis,
			pg_synthesis_binding_scope(binding), definition.expression->right, allocation);
		assert(job && !pg_synthesis_result(job));
		pg_effect_inference_destroy(&inference);
		struct pg_synthesis_job *context_job = pg_synthesis_evidence(&p->synthesis, context);
		struct pg_derivation_input invalid = {.rule = PG_VARIABLE,
			.parameters.binder = pg_binder(&p->graph), .count = 1};
		struct pg_synthesis_job *bad = pg_synthesis_rule(&p->synthesis, &invalid, &context_job, NULL, NULL);
		assert(bad);
		struct pg_synthesis_job *initial[] = {job, origin, bad};
		struct pg_synthesis_job *const *roots = initial;
		for (unsigned round = 0; round < (scenario & 2 ? 2U : 0U); ++round) {
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &p->synthesis, 3, roots));
			pg_program_destroy(p);
			rewind(file);
			size_t count;
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 3 && !p->synthesis.steps && !fclose(file));
			assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
		}
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 30000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED);
		const struct pg_evidence *result = pg_synthesis_result(roots[0]);
		enum pg_totality totality;
		const struct pg_term *value, *saved_value;
		assert(pg_computation_type_view(pg_evidence_classifier(result), &totality, &effects, &value));
		assert(totality == PG_TOTALITY_TOTAL);
		if (!(scenario & 2)) assert(pg_synthesis_result(roots[1]) == saved);
		saved = pg_synthesis_result(roots[1]);
		assert(saved && pg_computation_type_view(pg_evidence_classifier(saved), &totality, &effects, &saved_value));
		assert(totality == PG_TOTALITY_UNSPECIFIED && value == saved_value);
		pg_program_destroy(p);
	}
	puts("source Match: saved allocation does not impose its motive on independent synthesis");
}

static void match_origins(void)
{
	match_motive_authority();
	const char *text = "{{ Nat:=@{zero:*;succ:*->*;}; r:=&{(\\n:Nat=>n @zero=>Nat.zero @succ k=>Nat.succ *k) (Nat.succ Nat.zero);}; }}.r";
	struct pg_program *p = retained_program(text);
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		size_t jobs = p->synthesis.jobs.count, proofs = p->typing.proofs.count;
		assert(file && !pg_sources_write_retained(file, &p->synthesis, 1, &p->root, p->retained_reductions));
		assert(p->synthesis.steps == steps && p->synthesis.jobs.count == jobs && p->typing.proofs.count == proofs);
		invalid_match_allocations(file);
		pg_program_destroy(p);
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(p->root));
		assert(!fclose(file));
		struct match_origin_check check = {p, 0, 0};
		assert(!pg_synthesis_visit_source_allocations(&p->synthesis, check_match_origin, &check) && check.count == 1);
	}
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	const struct pg_evidence *forced = pg_prove_force(&p->typing, pg_synthesis_result(p->root));
	assert(forced && pg_evidence_subject(forced)->core == pg_reduction_source(p->retained_reductions->roots[0]));
	struct match_origin_check check = {p, 0, 1};
	assert(!pg_synthesis_visit_source_allocations(&p->synthesis, check_match_origin, &check) && check.count == 1);
	pg_program_destroy(p);
	puts("source Match origin: unsolved resaves preserve induction binders and clause allocations; ordinary source checking passed");
}

struct member_origin {
	struct pg_synthesis *synthesis;
	struct pg_synthesis_job *job;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
};

static int find_member_origin(void *owner, struct pg_synthesis_job *job)
{
	struct member_origin *found = owner;
	const struct pg_source_scope *scope;
	const struct pg_syntax *syntax;
	if (pg_synthesis_source_input(found->synthesis, job, &scope, &syntax)) return 0;
	if (syntax->kind != PG_SYNTAX_QUALIFIED) return 0;
	assert(!found->job);
	found->job = job; found->scope = scope; found->syntax = syntax;
	return 0;
}

static void inferred_constructor_roots(void)
{
	const char *text = "Nat:=@{zero:*;succ:*->*;};"
		"Vec:=\\A:@=>@\\n:Nat=>{nil:* Nat.zero;cons:A->* n->* (Nat.succ n);};"
		"cons:=&(Vec Nat).cons;zero:=Nat.zero;nil:=(Vec Nat).nil;Single:=Vec Nat (Nat.succ Nat.zero);";
	const char *names[] = {"Nat", "Vec", "cons", "zero", "nil", "Single"};
	for (unsigned typed = 0; typed < 2; ++typed) {
		struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_IMPLICIT_THUNK);
		assert(p && p->root);
		pg_synthesis_advance(&p->synthesis, 10000);
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		struct pg_synthesis_job *initial[6];
		for (size_t i = 0; i < 6; ++i) {
			initial[i] = pg_synthesis_definition(p->root,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = strlen(names[i])});
			if (typed) initial[i] = pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(initial[i]));
			assert(initial[i]);
		}
		struct pg_synthesis_job *const *roots = initial;
		size_t count = 6;
		for (unsigned round = 0; round < 2; ++round) {
			FILE *file = tmpfile();
			uint64_t steps = p->synthesis.steps;
			assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
			assert(p->synthesis.steps == steps);
			pg_program_destroy(p);
			rewind(file);
			p = pg_sources_read(file, 100000, &count, &roots);
			assert(p && count == 6 && !p->synthesis.steps && !fclose(file));
		}
		const struct pg_source_scope *scope = pg_synthesis_root(&p->synthesis);
		for (size_t i = 0; i < count; ++i)
			scope = pg_synthesis_name_job(&p->synthesis, scope,
				(struct pg_token){.kind = PG_TOKEN_IDENT, .text = names[i], .length = strlen(names[i])}, roots[i]);
		assert(scope);
		struct pg_parser parser;
		struct pg_definition definition;
		/* Raw proof exports contain the explicit Pi, not a source interface.
		 * Keeping a checked source producer retains its calling convention. */
		const char *use = typed ? "result:=cons zero zero nil :: Single;"
			: "result:=cons Nat.zero (Vec Nat).nil :: Vec Nat (Nat.succ Nat.zero);";
		pg_parser_init(&parser, &p->graph, use, strlen(use));
		assert(pg_parser_next(&parser, &definition) == 1);
		struct pg_synthesis_job *result = pg_synthesis_request(&p->synthesis, scope, definition.expression);
		assert(result);
		while (pg_synthesis_status(result) == PG_SYNTHESIS_PENDING && p->synthesis.ready) {
			assert(p->synthesis.steps < 100000);
			pg_synthesis_advance(&p->synthesis, typed ? 64 : 1);
		}
		assert(pg_synthesis_status(result) == PG_SYNTHESIS_DONE);
		if (typed) {
			const char *missing = "bad:=cons zero nil;";
			pg_parser_init(&parser, &p->graph, missing, strlen(missing));
			assert(pg_parser_next(&parser, &definition) == 1);
			struct pg_synthesis_job *bad = pg_synthesis_request(&p->synthesis, scope, definition.expression);
			assert(bad);
			pg_synthesis_advance(&p->synthesis, 100000);
			assert(pg_synthesis_status(bad) == PG_SYNTHESIS_REJECTED);
		}
		pg_program_destroy(p);
	}
	puts("constructor roots: checked source interfaces retain inference; bare proofs keep explicit Pi contracts");
}

static void member_use_origins(void)
{
	const char *text = "{{ Nat:=@{zero:*;}; Box:=&(\\A:@=>@{mk:A->*;}); r:=&(Box Nat).mk; }}.r";
	struct pg_program *p = retained_program(text);
	struct member_origin found = {.synthesis = &p->synthesis};
	assert(!pg_synthesis_visit_source_allocations(&p->synthesis, find_member_origin, &found) && found.job);
	const struct pg_context *prefix, *fields;
	assert(!pg_synthesis_member_allocation(&p->synthesis, found.job, &prefix, &fields));
	const struct pg_object *binder = fields->binder;
	assert(pg_synthesis_allocation_object(found.job) == binder);
	/* The same source shape at a new lexical use must check the allocation
	 * through both atomic preparation and ordinary reference resolution. */
	struct pg_syntax copies[3] = {*found.syntax, *found.syntax, *found.syntax};
	uint64_t steps = p->synthesis.steps;
	size_t proofs = p->typing.proofs.count;
	struct pg_synthesis_job *restored = pg_synthesis_member_at(&p->synthesis, found.scope, copies, prefix, fields);
	assert(restored && restored == pg_synthesis_member_at(&p->synthesis, found.scope, copies, prefix, fields));
	assert(p->synthesis.steps == steps && p->typing.proofs.count == proofs);
	assert(pg_synthesis_allocation_object(restored) == binder);
	assert(!pg_synthesis_member_at(&p->synthesis, found.scope, found.syntax, prefix, fields));
	assert(!pg_synthesis_member_at(&p->synthesis, found.scope, found.syntax->left, prefix, fields));
	assert(!pg_synthesis_member_at(&p->synthesis, found.scope, copies, fields, fields));
	const struct pg_context *extra = pg_context_bind(&p->typing, fields, pg_binder(&p->graph),
		fields->declared_type, PG_JUDGEMENT_VALUE);
	struct pg_synthesis_job *rejected = pg_synthesis_member_at(&p->synthesis, found.scope, copies + 1, prefix, extra);
	assert(rejected);
	/* Allocation is not a typing theorem: a stored annotation cannot replace
	 * the freshly synthesized field type, even with the same binder. */
	const struct pg_context *untrusted = pg_context_bind(&p->typing, prefix, binder,
		pg_universe(&p->graph, 0), PG_JUDGEMENT_VALUE);
	struct pg_synthesis_job *retyped = pg_synthesis_member_at(&p->synthesis, found.scope, copies + 2, prefix, untrusted);
	assert(retyped);
	struct pg_synthesis_job *selected[] = {p->root, restored, rejected, retyped};
	FILE *pending = tmpfile();
	assert(pending && !pg_sources_write_retained(pending, &p->synthesis, 4, selected, p->retained_reductions));
	assert(p->synthesis.steps == steps && !pg_synthesis_result(restored) && !pg_synthesis_result(rejected));
	pg_synthesis_advance(&p->synthesis, 10000);
	assert(pg_synthesis_status(restored) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_allocation_object(restored) == binder);
	assert(pg_evidence_subject(pg_synthesis_result(restored)) == pg_evidence_subject(pg_synthesis_result(found.job)));
	assert(pg_synthesis_status(rejected) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(rejected));
	assert(pg_synthesis_status(retyped) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(pg_synthesis_result(retyped)) == pg_evidence_subject(pg_synthesis_result(found.job)));
	pg_program_destroy(p);
	rewind(pending);
	size_t count;
	struct pg_synthesis_job *const *roots;
	p = pg_sources_read(pending, 100000, &count, &roots);
	assert(p && count == 4 && !p->synthesis.steps && !pg_synthesis_result(roots[1]));
	assert(!fclose(pending));
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		steps = p->synthesis.steps;
		assert(file && !pg_sources_write_retained(file, &p->synthesis, count, roots, p->retained_reductions));
		assert(p->synthesis.steps == steps);
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 4 && !p->synthesis.steps && !pg_synthesis_result(p->root));
		assert(!fclose(file));
	}
	pg_synthesis_advance(&p->synthesis, 10000);
	assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE && pg_synthesis_status(roots[3]) == PG_SYNTHESIS_DONE);
	assert(pg_evidence_subject(pg_synthesis_result(roots[1])) == pg_evidence_subject(pg_synthesis_result(roots[3])));
	assert(pg_synthesis_status(roots[2]) == PG_SYNTHESIS_REJECTED && !pg_synthesis_result(roots[2]));
	const struct pg_evidence *forced = pg_prove_force(&p->typing, pg_synthesis_result(p->root));
	assert(forced && pg_evidence_subject(forced)->core == pg_reduction_source(p->retained_reductions->roots[0]));
	pg_program_destroy(p);
	puts("source member uses: raw allocation resaves, field type synthesis and invalid/late/conflicting scopes passed");
}

static void check_retained_program(struct pg_program *p, struct pg_synthesis_job *root, int reuse)
{
	const struct pg_reduction_archive *reductions = p->retained_reductions;
	assert(reductions && reductions->count == 1 && !p->evaluation.jobs.count && !p->evaluation.normal_forms.count);
	const struct pg_reduction_certificate *receipt = reductions->roots[0];
	if (reuse) {
		struct pg_reduction_check check;
		assert(!pg_reduction_check_init(&check, &p->evaluation, reductions));
		assert(!pg_reduction_check_certificate(&check, 0));
		while (pg_reduction_check_advance(&check, 1) == PG_COMPARISON_PENDING)
			assert(pg_reduction_check_steps(&check) < 10000);
		receipt = pg_reduction_check_certificate(&check, 0);
		assert(receipt && !pg_nf_remember(&p->evaluation, receipt));
		pg_reduction_check_destroy(&check);
	}
	assert(!p->synthesis.steps && !pg_synthesis_result(root));
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_evidence *proof = pg_synthesis_result(root);
	assert(proof);
	const struct pg_evidence *forced = pg_prove_force(&p->typing, proof);
	if (forced && pg_evidence_subject(forced)->core != pg_reduction_source(receipt)) {
		fprintf(stderr, "retained source mismatch: reuse=%d alpha_equal=%d\n", reuse,
			pg_alpha_equal(pg_evidence_subject(forced)->core, pg_reduction_source(receipt)));
		const struct pg_term *left = pg_evidence_subject(forced)->core, *right = pg_reduction_source(receipt);
		fputs("first exact difference (f=function, a=argument, b=body): ", stderr);
		for (size_t depth = 0; depth < 128 && left != right; ++depth) {
			if (left->kind != right->kind) { fputs("node kind", stderr); break; }
			if (left->kind == PG_REFERENCE) { fputs("reference", stderr); break; }
			if (left->kind == PG_LAMBDA) {
				if (left->as.lambda.binder != right->as.lambda.binder) { fputs("binder", stderr); break; }
				fputc('b', stderr); left = left->as.lambda.body; right = right->as.lambda.body;
			} else if (left->as.application.function != right->as.application.function) {
				fputc('f', stderr); left = left->as.application.function; right = right->as.application.function;
			} else {
				fputc('a', stderr); left = left->as.application.argument; right = right->as.application.argument;
			}
		}
		fputc('\n', stderr);
	}
	assert(forced && pg_evidence_subject(forced)->core == pg_reduction_source(receipt));
	struct pg_nf_job *cached = pg_nf_request(&p->evaluation, &pg_pure_policy, pg_reduction_source(receipt));
	assert(pg_nf_status(cached) == (reuse ? PG_NF_DONE : PG_NF_PENDING));
	assert(!pg_nf_steps(cached));
	struct pg_synthesis_job *nf = pg_program_normalize(p, proof, 1);
	assert(nf);
	pg_synthesis_advance(&p->synthesis, 10000);
	const struct pg_evidence *result = pg_synthesis_result(nf);
	assert(result && pg_alpha_equal(pg_evidence_subject(result)->core, pg_reduction_target(receipt)) == 1);
	assert(pg_evidence_classifier(result) == pg_evidence_classifier(forced));
	assert((pg_nf_steps(cached) == 0) == !!reuse);
}

static void retained_normalization(void)
{
	struct pg_program *p = retained_program("{{ id:=&(\\A:@ => \\x:A => x); }}.id");
	const struct pg_reduction_archive *reductions = p->retained_reductions;
	struct pg_synthesis_job *selected[] = {p->root,
		pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(p->root))};
	/* Reduction/proof edges can introduce the first source-bound references
	 * after the ordinary source closure has already been collected. */
	for (unsigned mode = 0; mode < 3; ++mode) {
		FILE *file = tmpfile();
		assert(file && !pg_sources_write_retained(file, &p->synthesis, mode == 2 ? 2 : 1,
			selected, mode == 1 ? reductions : NULL));
		size_t syntax_count;
		source_binding_section(file, &syntax_count);
		uint64_t bindings;
		assert(!pg_wire_read_u64(file, &bindings) && (mode ? bindings > 0 : bindings == 0));
		assert(!fclose(file));
	}
	struct pg_synthesis_job *const *roots = &p->root;
	size_t count = 1;
	for (unsigned round = 0; round < 3; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_sources_write_retained(file, &p->synthesis, count, roots, reductions));
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(roots[0]));
		assert(!fclose(file));
		reductions = p->retained_reductions;
		assert(reductions && reductions->count == 1 && !p->evaluation.jobs.count && !p->evaluation.normal_forms.count);
		if (!round) continue;
		check_retained_program(p, roots[0], round == 1);
	}
	pg_program_destroy(p);
	puts("source retained NF: inert read/resave, explicit checking, shared input origins and typed cache reuse passed");
}

static void normalization_requests(void)
{
	normalization_origin();
	retained_normalization();
	const char *text = "{{ id:=&(\\A:@ => \\x:A => x); }}.id";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	struct pg_derivation_input empty = {.rule = PG_CONTEXT_EMPTY};
	struct pg_synthesis_job *context = pg_synthesis_derivation(&p->synthesis, &empty);
	struct pg_synthesis_job *nf = pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, PG_REDUCTION_NF);
	struct pg_synthesis_job *whnf = pg_synthesis_normalize_jobs(&p->synthesis, context, p->root, PG_REDUCTION_WHNF);
	struct pg_synthesis_job *bad = pg_synthesis_normalize_jobs(&p->synthesis, p->root, p->root, PG_REDUCTION_NF);
	struct pg_synthesis_job *roots[] = {p->root, nf, whnf, nf, bad,
		pg_synthesis_evaluate_jobs(&p->synthesis, context, p->root, PG_REDUCTION_NF),
		pg_synthesis_evaluate_jobs(&p->synthesis, context, p->root, PG_REDUCTION_WHNF)};
	assert(roots[5] && roots[6]);
	assert(context && nf && whnf && bad);
	for (size_t snapshot = 0;; ++snapshot) {
		assert(snapshot < 1000);
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write(file, &p->synthesis, 7, roots));
		assert(p->synthesis.steps == steps);
		if (!snapshot) invalid_normalization_mode(file);
		for (size_t round = 0; round < 2; ++round) {
			rewind(file);
			size_t count;
			struct pg_synthesis_job *const *loaded;
			struct pg_program *q = pg_sources_read(file, 100000, &count, &loaded);
			assert(q && count == 7 && !q->synthesis.steps && loaded[1] == loaded[3]);
			struct pg_synthesis_job *c, *term;
			enum pg_reduction_kind kind;
			assert(!pg_synthesis_normalization_input(&q->synthesis, loaded[1], &c, &term, &kind, NULL));
			assert(term == loaded[0] && kind == PG_REDUCTION_NF);
			assert(!pg_synthesis_normalization_input(&q->synthesis, loaded[2], &c, &term, &kind, NULL));
			assert(term == loaded[0] && kind == PG_REDUCTION_WHNF);
			int force;
			assert(!pg_synthesis_normalization_input(&q->synthesis, loaded[5], &c, &term, &kind, &force));
			assert(term == loaded[0] && kind == PG_REDUCTION_NF && force);
			assert(!pg_synthesis_normalization_input(&q->synthesis, loaded[6], &c, &term, &kind, &force));
			assert(term == loaded[0] && kind == PG_REDUCTION_WHNF && force);
			for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(loaded[i]));
			FILE *again = tmpfile();
			assert(again && !pg_sources_write(again, &q->synthesis, count, loaded));
			assert(!q->synthesis.steps);
			while (q->synthesis.ready) {
				assert(q->synthesis.steps < 10000);
				pg_synthesis_advance(&q->synthesis, round ? 64 : 1);
			}
			for (size_t i = 0; i < count; ++i)
				assert(pg_synthesis_status(loaded[i]) == (i == 4 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
			if (pg_synthesis_status(nf) == PG_SYNTHESIS_DONE)
				assert(pg_alpha_equal(pg_evidence_subject(pg_synthesis_result(nf))->core,
					pg_evidence_subject(pg_synthesis_result(loaded[1]))->core) == 1);
			assert(pg_evidence_judgement(pg_synthesis_result(loaded[1])) == PG_JUDGEMENT_VALUE);
			assert(pg_evidence_judgement(pg_synthesis_result(loaded[5])) == PG_JUDGEMENT_COMPUTATION);
			assert(pg_evidence_judgement(pg_synthesis_result(loaded[6])) == PG_JUDGEMENT_COMPUTATION);
			pg_program_destroy(q);
			assert(!fclose(file));
			file = again;
		}
		assert(!fclose(file));
		if (!p->synthesis.ready) break;
		pg_synthesis_advance(&p->synthesis, 1);
	}
	pg_program_destroy(p);
	puts("normalization requests: pending/settled inputs, modes, aliases, invalid context and unsolved resave passed");
}

static void definition_boundaries(void)
{
	const char *texts[] = {"good:=@; bad:=missing;", "good:=@; a:=b; b:=a;", "good:=@; good:=@;"};
	for (size_t i = 0; i < 3; ++i) {
		struct pg_program *original = pg_program_create(texts[i], strlen(texts[i]), PG_DEFINITION_EXPLICIT_THUNK);
		assert(original && original->root);
		const struct pg_source_scope *scope;
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&original->synthesis, original->root, &scope, &syntax));
		const struct pg_source_scope *local = pg_synthesis_definition_scope(&original->synthesis, scope, syntax);
		assert(local && !original->synthesis.steps);
		assert(local == pg_synthesis_definition_scope(&original->synthesis, scope, syntax));
		struct pg_source_environment environment;
		assert(!pg_synthesis_environment_input(&original->synthesis, local, &environment));
		assert(environment.parent == scope && environment.definitions == syntax);
		struct pg_synthesis_job *roots[] = {original->root,
			pg_synthesis_definition_request(&original->synthesis, scope, syntax, syntax->items[0].expression),
			parse(original, local, "{{ main:=good; }}.main")};
		assert(roots[1]);
		FILE *file = tmpfile();
		assert(file && !pg_sources_write(file, &original->synthesis, 3, roots));
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *restored;
		struct pg_program *loaded = pg_sources_read(file, 10000, &count, &restored);
		assert(loaded && count == 3 && !loaded->synthesis.steps);
		struct pg_program *programs[] = {original, loaded};
		for (size_t j = 0; j < 2; ++j) {
			struct pg_synthesis *s = &programs[j]->synthesis;
			struct pg_synthesis_job *const *selected = j ? restored : roots;
			while (s->ready) { assert(s->steps < 10000); pg_synthesis_advance(s, 1); }
			assert(pg_synthesis_status(selected[0]) == (i == 1 ? PG_SYNTHESIS_PENDING : PG_SYNTHESIS_REJECTED));
			assert(pg_synthesis_status(selected[1]) == (i == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
			assert(pg_synthesis_status(selected[2]) == (i == 2 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		}
		assert(!fclose(file));
		pg_program_destroy(loaded);
		pg_program_destroy(original);
	}
}

static void rule_environments(void)
{
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *context = pg_synthesis_evidence(&p->synthesis, pg_prove_empty_context(&p->typing));
	struct pg_derivation_input universe = {.rule = PG_UNIVERSE_FORM, .count = 1};
	struct pg_synthesis_job *type = pg_synthesis_rule(&p->synthesis, &universe, &context, NULL, NULL);
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "T", .length = 1};
	const struct pg_source_scope *scope = pg_synthesis_name_job(&p->synthesis, p->scope, name, type);
	assert(scope);
	struct pg_synthesis_job *consumer = parse(p, scope, "{{ main:=&(\\x:T => x); }}.main");
	const struct pg_effect_row *empty = pg_effect_row(&p->graph, 0, NULL);
	struct pg_effect_equation *equation = pg_effect_equation(&p->imported_effects, empty);
	struct pg_derivation_input carrier = {.rule = PG_RETURN_TYPE_FORM, .count = 1};
	struct pg_synthesis_job *result = pg_synthesis_rule(&p->synthesis, &carrier, &type, &p->imported_effects, equation);
	struct pg_derivation_input invalid = {.rule = PG_APP_ELIM};
	struct pg_synthesis_job *bad = pg_synthesis_derivation(&p->synthesis, &invalid);
	struct pg_synthesis_job *selected[] = {consumer, type, result, bad, type};
	FILE *file = tmpfile();
	assert(file && pg_sources_write(file, &p->synthesis, 5, selected) == -1 && ftell(file) == 0);
	pg_effect_inference_seal(&p->imported_effects);
	assert(!pg_sources_write(file, &p->synthesis, 5, selected));
	assert(!p->synthesis.steps && !pg_synthesis_result(type));
	pg_program_destroy(p);
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	p = pg_sources_read(file, 10000, &count, &roots);
	assert(p && count == 5 && roots[1] == roots[4] && !p->synthesis.steps);
	for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 1); }
	for (size_t i = 0; i < count; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 3 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	const struct pg_effect_row *row;
	const struct pg_term *value;
	assert(pg_effect_type_view(pg_evidence_subject(pg_synthesis_result(roots[2]))->core, &row, &value));
	assert(!pg_effect_count(row) && value == pg_universe(&p->graph, 0));
	assert(!fclose(file));
	pg_program_destroy(p);
}

static void read_sources(FILE *file, uint64_t chunk);

static void write_sources(FILE *file)
{
	const char text[] = "id:=&(\\A:@ => \\x:A => x);";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	const struct pg_source_scope *provider_scope;
	const struct pg_syntax *provider;
	assert(!pg_synthesis_source_input(&p->synthesis, p->root, &provider_scope, &provider));
	struct pg_synthesis_job *reserved = pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider->items[0].expression);
	assert(reserved == pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider->items[0].expression));
	struct pg_synthesis_job *missing = pg_synthesis_definition_request(&p->synthesis,
		provider_scope, provider, provider);
	assert(reserved && missing && !pg_synthesis_result(reserved));
	struct pg_token lib = {.kind = PG_TOKEN_IDENT, .text = "lib", .length = 3};
	const struct pg_source_scope *namespace = pg_synthesis_module_namespace(&p->synthesis, p->scope, lib, p->root);
	assert(namespace);
	struct pg_synthesis_job *client = parse(p, namespace, "{{ main:=lib.id; }}.main");
	struct pg_token alias = {.kind = PG_TOKEN_IDENT, .text = "alias", .length = 5};
	const struct pg_source_scope *exports = pg_synthesis_name_job(&p->synthesis, p->scope, alias, reserved);
	const struct pg_source_scope *imports = pg_synthesis_import_scope(&p->synthesis, p->scope, exports);
	struct pg_synthesis_job *consumer = parse(p, imports, "{{ import alias; main:=alias; }}.main");
	struct pg_token public = {.kind = PG_TOKEN_IDENT, .text = "public", .length = 6};
	const struct pg_source_scope *published = pg_synthesis_namespace(&p->synthesis, p->scope, public, exports);
	struct pg_synthesis_job *member = parse(p, published, "{{ main:=public.alias; }}.main");
	struct pg_synthesis_job *roots[] = {client, consumer, p->root, client, member, reserved, missing};
	size_t jobs = p->synthesis.jobs.count, scopes = p->synthesis.scopes.count, proofs = p->typing.proofs.count;
	assert(!pg_sources_write(file, &p->synthesis, 7, roots));
	assert(!p->synthesis.steps && p->synthesis.jobs.count == jobs && p->synthesis.scopes.count == scopes);
	assert(p->typing.proofs.count == proofs);
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	struct pg_synthesis_job *definition;
	while (!(definition = pg_synthesis_definition(p->root, id))) {
		assert(p->synthesis.steps < 1000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	assert(!pg_synthesis_result(definition));
	assert(definition == reserved);
	const struct pg_source_scope *ambient;
	const struct pg_syntax *definitions, *expression;
	assert(!pg_synthesis_definition_input(&p->synthesis, definition, &ambient, &definitions, &expression));
	assert(ambient == provider_scope && definitions == provider && expression == provider->items[0].expression);
	/* The source view is independent of accepted/failed progress state. */
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 64); }
	for (size_t i = 0; i < 7; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 6 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	assert(pg_synthesis_definition(p->root, id) == definition);
	assert(!pg_synthesis_definition_input(&p->synthesis, definition, &ambient, &definitions, &expression));
	assert(ambient == provider_scope && definitions == provider && expression == provider->items[0].expression);
	FILE *accepted = tmpfile();
	assert(accepted && !pg_sources_write(accepted, &p->synthesis, 7, roots));
	/* Source inputs stay the same, but allocated binders are now retained too. */
	rewind(accepted);
	read_sources(accepted, 1);
	assert(!ferror(accepted) && !fclose(accepted));
	FILE *retained = tmpfile();
	assert(retained);
	struct pg_synthesis_job *evidence = pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(client));
	assert(!pg_sources_write(retained, &p->synthesis, 1, &evidence));
	assert(!fclose(retained));
	pg_program_destroy(p);
}

static void read_sources(FILE *file, uint64_t chunk)
{
	size_t count;
	struct pg_synthesis_job *const *roots;
	struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
	assert(p && count == 7 && p->root == roots[0] && roots[0] == roots[3]);
	assert(!p->synthesis.steps && !p->parser.reader.input);
	for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
	while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
	for (size_t i = 0; i < count; ++i)
		assert(pg_synthesis_status(roots[i]) == (i == 6 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[1]));
	assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[4]));
	struct pg_token id = {.kind = PG_TOKEN_IDENT, .text = "id", .length = 2};
	assert(pg_synthesis_definition(roots[2], id) == roots[5]);
	assert(pg_synthesis_result(roots[5]) == pg_synthesis_result(roots[0]));
	printf("source image: shared modules, explicit imports and ordinary Solve passed (%llu steps)\n", (unsigned long long)p->synthesis.steps);
	pg_program_destroy(p);
}

static void nominal_sources(FILE *file, int writing, uint64_t chunk, int origins)
{
	if (writing) {
		const char text[] = "D:=@{z:*;}; E:=@{z:*;}; d:=D.z; e:=E.z;";
		struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
		assert(p && p->root);
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 64); }
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const char *names[] = {"D", "E", "d", "e"};
		struct pg_synthesis_job *values[4];
		const struct pg_source_scope *scope = p->scope;
		for (size_t i = 0; i < 4; ++i) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = names[i], .length = 1};
			const struct pg_evidence *proof = pg_synthesis_result(pg_synthesis_definition(p->root, name));
			assert(proof);
			values[i] = pg_synthesis_evidence(&p->synthesis, proof);
			scope = pg_synthesis_name_job(&p->synthesis, scope, name, values[i]);
			assert(scope);
		}
		const struct pg_evidence *type = pg_synthesis_result(values[0]);
		const struct pg_operation_declaration *declaration = pg_operation_declaration(&p->typing, type, type);
		const struct pg_evidence *function = pg_prove_operation_function(&p->typing, declaration);
		assert(function);
		struct pg_synthesis_job *operation = pg_synthesis_evidence(&p->synthesis, function);
		scope = pg_synthesis_name_job(&p->synthesis, scope,
			(struct pg_token){.kind = PG_TOKEN_IDENT, .text = "ask", .length = 3}, operation);
		assert(scope);
		struct pg_synthesis_job *roots[] = {
			parse(p, scope, "{{ main:=d; main::D; }}.main"),
			parse(p, scope, "{{ main:=e; main::D; }}.main"),
			parse(p, scope, "{{ main:=&(ask d); }}.main"), values[0], values[1], values[2], values[3], operation, p->root};
		assert(!pg_sources_write(file, &p->synthesis, origins ? 9 : 8, roots));
		pg_program_destroy(p);
	} else {
		size_t count;
		struct pg_synthesis_job *const *roots;
		struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == (origins ? 9u : 8u) && !p->synthesis.steps);
		if (origins) {
			FILE *pending = tmpfile();
			assert(pending && !pg_sources_write(pending, &p->synthesis, count, roots));
			assert(!p->synthesis.steps);
			pg_program_destroy(p);
			rewind(pending);
			p = pg_sources_read(pending, 10000, &count, &roots);
			assert(p && count == 9 && !p->synthesis.steps);
			assert(!fclose(pending));
		}
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
		for (size_t i = 0; i < count; ++i)
			assert(pg_synthesis_status(roots[i]) == (i == 1 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		assert(pg_synthesis_result(roots[0]) == pg_synthesis_result(roots[5]));
		const struct pg_term *d = pg_evidence_subject(pg_synthesis_result(roots[3]))->core;
		const struct pg_term *e = pg_evidence_subject(pg_synthesis_result(roots[4]))->core;
		assert(d != e);
		assert(pg_evidence_classifier(pg_synthesis_result(roots[5])) == d);
		assert(pg_evidence_classifier(pg_synthesis_result(roots[6])) == e);
		const struct pg_term *computation, *value;
		const struct pg_effect_row *effects;
		assert(pg_thunk_type_view(pg_evidence_classifier(pg_synthesis_result(roots[2])), &computation));
		enum pg_totality totality;
		assert(pg_computation_type_view(computation, &totality, &effects, &value));
		assert(totality == PG_TOTALITY_TOTAL);
		assert(pg_effect_count(effects) == 1 && value == d);
		if (origins) {
			struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "D", .length = 1};
			const struct pg_evidence *source_type = pg_synthesis_result(pg_synthesis_definition(roots[8], name));
			assert(source_type);
			if (pg_evidence_subject(source_type)->core != d) {
				fputs("source image: source and retained evidence split one nominal declaration\n", stderr);
				pg_program_destroy(p);
				exit(1);
			}
		}
		pg_program_destroy(p);
		puts("source image: nominal external names, distinct declarations and operation wrapper passed");
	}
}

static int find_declaration(void *owner, struct pg_synthesis_job *job)
{
	if (pg_synthesis_binding_binder(job)) return 0;
	struct pg_synthesis_job **found = owner;
	assert(!*found);
	*found = job;
	return 0;
}

static void parameter_origins(void)
{
	const char *text = "Box:=&(\\A:@=>\\B:@=>@{mk:A->B->*;});";
	struct pg_program *p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	while (p->synthesis.ready) { assert(p->synthesis.steps < 20000); pg_synthesis_advance(&p->synthesis, 1); }
	assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *declaration = NULL;
	assert(!pg_synthesis_visit_source_allocations(&p->synthesis, find_declaration, &declaration));
	assert(declaration && pg_synthesis_result(declaration));
	struct pg_synthesis_job *selected[] = {p->root,
		pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(declaration))};
	struct pg_synthesis_job *const *roots = selected;
	size_t count = 2;
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		assert(file && !pg_sources_write(file, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(file);
		p = pg_sources_read(file, 50000, &count, &roots);
		assert(p && count == 2 && !p->synthesis.steps);
		assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
		assert(!fclose(file));
	}
	while (p->synthesis.ready) { assert(p->synthesis.steps < 30000); pg_synthesis_advance(&p->synthesis, 1); }
	assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
	assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
	declaration = NULL;
	assert(!pg_synthesis_visit_source_allocations(&p->synthesis, find_declaration, &declaration));
	assert(declaration && pg_synthesis_result(declaration) == pg_synthesis_result(roots[1]));
	pg_program_destroy(p);
	puts("source image: dependent parameter scopes retain declaration identity through unsolved resave");
}

static void function_origins(FILE *file, int writing)
{
	struct pg_program *p;
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "f", .length = 1};
	if (writing) {
		const char *text = "f:=&(\\A:@=>\\x:A=>x);";
		p = pg_program_create(text, strlen(text), PG_DEFINITION_EXPLICIT_THUNK);
		assert(p && p->root);
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 1); }
		assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(p->root, name));
		assert(function);
		struct pg_synthesis_job *roots[] = {p->root, pg_synthesis_evidence(&p->synthesis, function)};
		assert(roots[1] && !pg_sources_write(file, &p->synthesis, 2, roots));
	} else {
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == 2 && !p->synthesis.steps);
		FILE *pending = tmpfile();
		assert(pending && !pg_sources_write(pending, &p->synthesis, count, roots));
		assert(!p->synthesis.steps);
		pg_program_destroy(p);
		rewind(pending);
		p = pg_sources_read(pending, 10000, &count, &roots);
		assert(p && count == 2 && !p->synthesis.steps);
		assert(!pg_synthesis_result(roots[0]) && !pg_synthesis_result(roots[1]));
		assert(!fclose(pending));
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, 1); }
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE && pg_synthesis_status(roots[1]) == PG_SYNTHESIS_DONE);
		const struct pg_evidence *function = pg_synthesis_result(pg_synthesis_definition(roots[0], name));
		assert(function);
		if (pg_evidence_subject(function)->core != pg_evidence_subject(pg_synthesis_result(roots[1]))->core) {
			fputs("source image: source and retained function evidence split binder identity\n", stderr);
			pg_program_destroy(p);
			exit(1);
		}
		puts("source image: standalone function binder identity retained");
	}
	pg_program_destroy(p);
}

static void invalid_annotation_edges(FILE *file)
{
	assert(!fflush(file) && !fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		assert(!fseek(file, (long)scope[6], SEEK_CUR));
	}
	assert(!fseek(file, (long)header[2] * 8, SEEK_CUR));
	for (size_t i = 0; i < header[4]; ++i) {
		long position = ftell(file);
		uint64_t fields[6];
		for (size_t j = 0; j < 6; ++j) assert(!pg_wire_read_u64(file, &fields[j]));
		if (!fields[4]) continue;
		/* Cyclic operand, missing operand, and mixed source/annotation record. */
		const size_t slots[] = {4, 5, 1};
		const uint64_t values[] = {i + 1, 0, 1};
		for (size_t j = 0; j < 3; ++j) {
			long offset = position + (long)slots[j] * 8;
			assert(!fseek(file, offset, SEEK_SET) && !pg_wire_write_u64(file, values[j]));
			assert(!fflush(file));
			rewind(file);
			size_t count;
			struct pg_synthesis_job *const *roots;
			assert(!pg_sources_read(file, 10000, &count, &roots));
			assert(!fseek(file, offset, SEEK_SET) && !pg_wire_write_u64(file, fields[slots[j]]));
			assert(!fflush(file));
		}
		assert(!fseek(file, 0, SEEK_END));
		return;
	}
	assert(!"missing annotation record");
}

static void invalid_named_cycle(FILE *file)
{
	assert(!fseek(file, 8, SEEK_SET));
	uint64_t header[6], original = 0;
	long link = -1;
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	for (size_t i = 0; i < header[1]; ++i) {
		long position = ftell(file);
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		if (scope[0] == 1 && link < 0) { link = position + 7 * 8; original = scope[7]; }
		assert(!fseek(file, (long)scope[6], SEEK_CUR));
	}
	assert(link >= 0 && header[2] == 8);
	uint64_t consumer;
	for (size_t i = 0; i <= 6; ++i) assert(!pg_wire_read_u64(file, &consumer));
	/* The lexical name now points to its own consumer: scope -> job -> scope. */
	assert(!fseek(file, link, SEEK_SET) && !pg_wire_write_u64(file, consumer) && !fflush(file));
	rewind(file);
	size_t count;
	struct pg_synthesis_job *const *roots;
	assert(!pg_sources_read(file, 10000, &count, &roots));
	assert(!fseek(file, link, SEEK_SET) && !pg_wire_write_u64(file, original) && !fflush(file));
	assert(!fseek(file, 0, SEEK_END));
}

static void prepared_scope_chain(void)
{
	const size_t depth = 512;
	struct pg_program *p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
	assert(p);
	struct pg_synthesis_job *context = pg_synthesis_evidence(&p->synthesis, pg_prove_empty_context(&p->typing));
	struct pg_derivation_input universe = {.rule = PG_UNIVERSE_FORM, .count = 1, .parameters.level = 1};
	struct pg_synthesis_job *type = pg_synthesis_rule(&p->synthesis, &universe, &context, NULL, NULL);
	const struct pg_source_scope *base = pg_synthesis_root(&p->synthesis);
	struct pg_synthesis_job *term = parse(p, base, "{{main:=@;}}.main");
	struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "T", .length = 1};
	struct pg_syntax reference = {.kind = PG_SYNTAX_ATOM, .token = name};
	reference.token.text_length = name.length;
	const struct pg_source_scope *scope = base;
	for (size_t i = 0; i < depth; ++i) {
		struct pg_synthesis_job *check = pg_synthesis_source_expect(&p->synthesis, scope, term, type);
		scope = pg_synthesis_name_job(&p->synthesis, scope, name, check);
		term = pg_synthesis_request(&p->synthesis, scope, &reference);
		assert(check && scope && term);
	}
	FILE *file = tmpfile();
	struct pg_synthesis_job *selected[] = {term, term};
	assert(file && !pg_sources_write(file, &p->synthesis, 2, selected));
	assert(!p->synthesis.steps && !pg_synthesis_result(term));
	assert(!fseek(file, 16, SEEK_SET));
	uint64_t scope_count, root_count, origin_count, producer_count;
	assert(!pg_wire_read_u64(file, &scope_count) && scope_count == depth + 1);
	assert(!pg_wire_read_u64(file, &root_count) && root_count == 2);
	assert(!pg_wire_read_u64(file, &origin_count) && origin_count == 0);
	assert(!pg_wire_read_u64(file, &producer_count) && producer_count == 2 * depth + 2);
	pg_program_destroy(p);
	uint64_t steps = 0;
	for (uint64_t chunk = 1; chunk <= 64; chunk *= 64) {
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 20000, &count, &roots);
		assert(p && count == 2 && roots[0] == roots[1] && !p->synthesis.steps);
		term = roots[0];
		struct pg_synthesis_job *shared_type = NULL;
		const struct pg_syntax *shared_reference = NULL;
		for (size_t i = 0; i < depth; ++i) {
			const struct pg_syntax *syntax;
			assert(!pg_synthesis_source_input(&p->synthesis, term, &scope, &syntax));
			if (!shared_reference) shared_reference = syntax;
			assert(syntax == shared_reference);
			struct pg_source_environment environment;
			assert(!pg_synthesis_environment_input(&p->synthesis, scope, &environment));
			assert(environment.producer && environment.parent);
			assert(!pg_synthesis_source_expect_input(&p->synthesis, environment.producer, &scope, &term, &type));
			assert(scope == environment.parent);
			if (!shared_type) shared_type = type;
			assert(type == shared_type);
		}
		while (p->synthesis.ready) {
			assert(p->synthesis.steps < 100000);
			pg_synthesis_advance(&p->synthesis, chunk);
		}
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		assert(pg_evidence_subject(pg_synthesis_result(roots[0]))->core == pg_universe(&p->graph, 0));
		if (chunk == 1) steps = p->synthesis.steps;
		assert(p->synthesis.steps == steps);
		pg_program_destroy(p);
	}
	assert(!fclose(file));
	puts("source image: 512 alternating lexical/annotation dependencies retain shared syntax and split-budget results");
}

static void annotation_sources(FILE *file, int writing, uint64_t chunk)
{
	struct pg_program *p;
	size_t count;
	struct pg_synthesis_job *const *roots;
	if (writing) {
		prepared_scope_chain();
		p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		struct pg_synthesis_job *term = parse(p, p->scope, "{{main:=@;}}.main");
		struct pg_synthesis_job *context = pg_synthesis_evidence(&p->synthesis, pg_prove_empty_context(&p->typing));
		struct pg_derivation_input header = {.rule = PG_UNIVERSE_FORM, .count = 1, .parameters.level = 1};
		struct pg_synthesis_job *type = pg_synthesis_rule(&p->synthesis, &header, &context, NULL, NULL);
		header.parameters.level = 0;
		struct pg_synthesis_job *wrong = pg_synthesis_rule(&p->synthesis, &header, &context, NULL, NULL);
		struct pg_synthesis_job *valid = pg_synthesis_source_expect(&p->synthesis, p->scope, term, type);
		struct pg_synthesis_job *invalid = pg_synthesis_source_expect(&p->synthesis, p->scope, term, wrong);
		struct pg_synthesis_job *nested = pg_synthesis_source_expect(&p->synthesis, p->scope, valid, type);
		struct pg_token name = {.kind = PG_TOKEN_IDENT, .text = "T", .length = 1};
		const struct pg_source_scope *good_scope = pg_synthesis_name_job(&p->synthesis, p->scope, name, valid);
		const struct pg_source_scope *bad_scope = pg_synthesis_name_job(&p->synthesis, p->scope, name, invalid);
		assert(good_scope && bad_scope);
		struct pg_synthesis_job *selected[] = {nested, invalid, valid, term, valid, type,
			parse(p, good_scope, "{{main:=T;}}.main"), parse(p, bad_scope, "{{main:=T;}}.main")};
		assert(valid && invalid && nested && !p->synthesis.steps);
		assert(!pg_sources_write(file, &p->synthesis, 8, selected));
		assert(!p->synthesis.steps && !pg_synthesis_result(valid));
		invalid_annotation_edges(file);
		invalid_named_cycle(file);
		/* Only consumers are roots; lexical edges must discover both checks. */
		FILE *names = tmpfile();
		assert(names && !pg_sources_write(names, &p->synthesis, 2, selected + 6));
		rewind(names);
		struct pg_program *restored = pg_sources_read(names, 10000, &count, &roots);
		assert(restored && count == 2 && !restored->synthesis.steps);
		assert(!fclose(names));
		while (restored->synthesis.ready) {
			assert(restored->synthesis.steps < 10000);
			pg_synthesis_advance(&restored->synthesis, 1);
		}
		assert(pg_synthesis_status(roots[0]) == PG_SYNTHESIS_DONE);
		assert(pg_synthesis_status(roots[1]) == PG_SYNTHESIS_REJECTED);
		pg_program_destroy(restored);
		assert(!p->synthesis.steps && !pg_synthesis_result(valid));
	} else {
		p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == 8 && roots[2] == roots[4] && !p->synthesis.steps);
		/* Program initialization proves only the ordinary empty context. */
		assert(p->typing.proofs.count == 1);
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		const struct pg_source_scope *scope;
		struct pg_synthesis_job *term, *type;
		assert(!pg_synthesis_source_expect_input(&p->synthesis, roots[0], &scope, &term, &type));
		assert(term == roots[2] && type == roots[5]);
		assert(!pg_synthesis_source_expect_input(&p->synthesis, roots[2], &scope, &term, &type));
		assert(term == roots[3] && type == roots[5]);
		const struct pg_syntax *syntax;
		struct pg_source_environment environment;
		assert(!pg_synthesis_source_input(&p->synthesis, roots[6], &scope, &syntax));
		assert(!pg_synthesis_environment_input(&p->synthesis, scope, &environment));
		assert(environment.producer == roots[2]);
		assert(!pg_synthesis_source_input(&p->synthesis, roots[7], &scope, &syntax));
		assert(!pg_synthesis_environment_input(&p->synthesis, scope, &environment));
		assert(environment.producer == roots[1]);
		FILE *pending = tmpfile();
		assert(pending && !pg_sources_write(pending, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(pending);
		p = pg_sources_read(pending, 10000, &count, &roots);
		assert(p && count == 8 && !p->synthesis.steps && p->typing.proofs.count == 1);
		assert(!fclose(pending));
		while (p->synthesis.ready) { assert(p->synthesis.steps < 10000); pg_synthesis_advance(&p->synthesis, chunk); }
		for (size_t i = 0; i < count; ++i)
			assert(pg_synthesis_status(roots[i]) == (i == 1 || i == 7 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
		assert(roots[2] == roots[4]);
		assert(pg_evidence_subject(pg_synthesis_result(roots[0]))->core == pg_universe(&p->graph, 0));
		puts("source image: named prepared annotations retain sharing, reject dependency cycles and check targets through ordinary Solve");
	}
	pg_program_destroy(p);
}

static void module_annotation_sources(FILE *file, int writing, uint64_t chunk, int settled)
{
	const char *sources[] = {
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A;",
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; bad:=missing;",
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; a:=b; b:=a;",
		"id:=&(\\A:@ => \\x:A => x); id::@;"
	};
	struct pg_program *p;
	struct pg_synthesis_job *selected[12];
	struct pg_synthesis_job *const *roots = selected;
	size_t count = 12;
	if (writing) {
		p = pg_program_allocate(PG_DEFINITION_EXPLICIT_THUNK);
		assert(p);
		for (size_t i = 0; i < 4; ++i) {
			selected[3 * i] = parse(p, p->scope, sources[i]);
			const struct pg_syntax_item *item;
			struct pg_synthesis_job *check = NULL;
			while (!check) {
				assert(p->synthesis.ready && p->synthesis.steps < 10000);
				pg_synthesis_advance(&p->synthesis, 1);
				assert(pg_synthesis_definition_entry(selected[3 * i], 1, &item, &check) == 1);
			}
			assert(item->operation == PG_TOKEN_EXPECT);
			selected[3 * i + 1] = check;
			assert(pg_synthesis_definition_entry(selected[3 * i], 0, &item, &selected[3 * i + 2]) == 1);
			assert(selected[3 * i + 2]);
		}
		if (settled) {
			while (p->synthesis.ready) {
				assert(p->synthesis.steps < 20000);
				pg_synthesis_advance(&p->synthesis, chunk);
			}
		}
		uint64_t steps = p->synthesis.steps;
		assert(!pg_sources_write(file, &p->synthesis, count, roots));
		assert(p->synthesis.steps == steps);
	} else {
		p = pg_sources_read(file, 20000, &count, &roots);
		assert(p && count == 12 && !p->synthesis.steps);
		FILE *pending = tmpfile();
		assert(pending && !pg_sources_write(pending, &p->synthesis, count, roots));
		pg_program_destroy(p);
		rewind(pending);
		p = pg_sources_read(pending, 20000, &count, &roots);
		assert(p && count == 12 && !p->synthesis.steps);
		assert(!fclose(pending));
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		while (p->synthesis.ready) { assert(p->synthesis.steps < 20000); pg_synthesis_advance(&p->synthesis, chunk); }
		const enum pg_synthesis_status status[] = {PG_SYNTHESIS_DONE, PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_PENDING, PG_SYNTHESIS_REJECTED};
		for (size_t i = 0; i < 4; ++i) {
			assert(pg_synthesis_status(roots[3 * i]) == status[i]);
			assert(pg_synthesis_status(roots[3 * i + 1]) == (i == 3 ? PG_SYNTHESIS_REJECTED : PG_SYNTHESIS_DONE));
			assert(pg_synthesis_status(roots[3 * i + 2]) == PG_SYNTHESIS_DONE);
			const struct pg_syntax_item *item;
			struct pg_synthesis_job *check, *definition;
			assert(pg_synthesis_definition_entry(roots[3 * i], 1, &item, &check) == 1);
			assert(pg_synthesis_definition_entry(roots[3 * i], 0, &item, &definition) == 1);
			assert(check == roots[3 * i + 1] && definition == roots[3 * i + 2]);
		}
		puts("source image: restored module obligations reuse prepared producers without hiding rejected or cyclic siblings");
	}
	pg_program_destroy(p);
}

static void invalid_module_entries(FILE *file)
{
	assert(!fflush(file) && !fseek(file, 8, SEEK_SET));
	uint64_t header[6];
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &header[i]));
	assert(header[5] == 2);
	for (size_t i = 0; i < header[1]; ++i) {
		uint64_t scope[10];
		for (size_t j = 0; j < 10; ++j) assert(!pg_wire_read_u64(file, &scope[j]));
		assert(!fseek(file, (long)scope[6], SEEK_CUR));
	}
	assert(!fseek(file, (long)(header[2] + 6 * header[4]) * 8, SEEK_CUR));
	uint64_t entries[6];
	long start = ftell(file);
	for (size_t i = 0; i < 6; ++i) assert(!pg_wire_read_u64(file, &entries[i]));
	assert(entries[1] == 0 && entries[4] == 1);
	/* Invalid owner, source index and cyclic edge fail during reconstruction.
	 * A well-formed edge to the wrong producer remains unaccepted until Solve. */
	const size_t slots[] = {3, 4, 5, 5};
	const uint64_t values[] = {0, 2, entries[3], entries[2]};
	for (size_t i = 0; i < 4; ++i) {
		long offset = start + (long)slots[i] * 8;
		assert(!fseek(file, offset, SEEK_SET) && !pg_wire_write_u64(file, values[i]));
		assert(!fflush(file));
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		struct pg_program *p = pg_sources_read(file, 10000, &count, &roots);
		if (i < 3) assert(!p);
		else {
			assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(p->root));
			while (p->synthesis.ready) {
				assert(p->synthesis.steps < 10000);
				pg_synthesis_advance(&p->synthesis, 1);
			}
			assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_REJECTED);
			pg_program_destroy(p);
		}
		assert(!fseek(file, offset, SEEK_SET) && !pg_wire_write_u64(file, entries[slots[i]]));
		assert(!fflush(file));
	}
}

static void prepared_module_checkpoint(void)
{
	const char source[] = "id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A;";
	struct pg_program *p = pg_program_create(source, strlen(source), PG_DEFINITION_EXPLICIT_THUNK);
	assert(p && p->root);
	const struct pg_syntax_item *item;
	struct pg_synthesis_job *check = NULL;
	while (!check) {
		assert(p->synthesis.ready && p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
		assert(pg_synthesis_definition_entry(p->root, 1, &item, &check) == 1);
	}
	for (size_t round = 0; round < 2; ++round) {
		FILE *file = tmpfile();
		uint64_t steps = p->synthesis.steps;
		assert(file && !pg_sources_write(file, &p->synthesis, 1, &p->root));
		assert(p->synthesis.steps == steps);
		invalid_module_entries(file);
		pg_program_destroy(p);
		rewind(file);
		size_t count;
		struct pg_synthesis_job *const *roots;
		p = pg_sources_read(file, 10000, &count, &roots);
		assert(p && count == 1 && !p->synthesis.steps && !pg_synthesis_result(p->root));
		assert(!fclose(file));
		const struct pg_source_scope *scope;
		const struct pg_syntax *syntax;
		assert(!pg_synthesis_source_input(&p->synthesis, p->root, &scope, &syntax));
		size_t jobs = p->synthesis.jobs.count;
		const struct pg_source_scope *local = pg_synthesis_definition_scope(&p->synthesis, scope, syntax);
		struct pg_synthesis_job *definition = pg_synthesis_definition_request(&p->synthesis,
			scope, syntax, syntax->items[0].expression);
		struct pg_synthesis_job *type = pg_synthesis_request(&p->synthesis, local, syntax->items[1].expression);
		check = pg_synthesis_source_expect(&p->synthesis, local, definition, type);
		assert(check);
		if (p->synthesis.jobs.count != jobs) {
			fprintf(stderr, "prepared module round %zu: reconstructed %zu missing producer inputs\n",
				round, p->synthesis.jobs.count - jobs);
			pg_program_destroy(p);
			exit(1);
		}
	}
	while (p->synthesis.ready) {
		assert(p->synthesis.steps < 10000);
		pg_synthesis_advance(&p->synthesis, 1);
	}
	assert(pg_synthesis_status(p->root) == PG_SYNTHESIS_DONE);
	struct pg_synthesis_job *registered;
	assert(pg_synthesis_definition_entry(p->root, 1, &item, &registered) == 1 && registered == check);
	pg_program_destroy(p);
	puts("prepared module: module-only roots retain shared obligations across unsolved resave");
}

static void module_save_boundaries(void)
{
	const char *sources[] = {
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A;",
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; bad:=missing;",
		"id:=&(\\A:@ => \\x:A => x); id::(A:@)->A->A; a:=b; b:=a;"
	};
	const enum pg_synthesis_status expected[] = {PG_SYNTHESIS_DONE, PG_SYNTHESIS_REJECTED, PG_SYNTHESIS_PENDING};
	for (size_t case_index = 0; case_index < 3; ++case_index) {
		struct pg_program *source = pg_program_create(sources[case_index], strlen(sources[case_index]), PG_DEFINITION_EXPLICIT_THUNK);
		assert(source);
		size_t snapshots = 0;
		for (;;) {
			FILE *file = tmpfile();
			assert(file && !pg_sources_write(file, &source->synthesis, 1, &source->root));
			rewind(file);
			size_t count;
			struct pg_synthesis_job *const *roots;
			struct pg_program *restored = pg_sources_read(file, 10000, &count, &roots);
			assert(restored && count == 1 && !restored->synthesis.steps);
			assert(!pg_synthesis_result(restored->root) && !fclose(file));
			while (restored->synthesis.ready) {
				assert(restored->synthesis.steps < 10000);
				pg_synthesis_advance(&restored->synthesis, snapshots % 2 ? 1 : 64);
			}
			assert(pg_synthesis_status(restored->root) == expected[case_index]);
			pg_program_destroy(restored);
			++snapshots;
			if (!source->synthesis.ready) break;
			assert(source->synthesis.steps < 10000);
			pg_synthesis_advance(&source->synthesis, 1);
		}
		assert(pg_synthesis_status(source->root) == expected[case_index]);
		printf("module save boundaries: case %zu, %zu snapshots preserve status\n", case_index, snapshots);
		pg_program_destroy(source);
	}
}

static void retained_process(int argc, char **argv)
{
	assert(argc >= 3);
	struct pg_program *p;
	struct pg_synthesis_job *const *roots;
	size_t count;
	if (!strcmp(argv[1], "retained-write") || !strcmp(argv[1], "retained-write-typed")) {
		assert(argc == 4);
		const char *text;
		if (!strcmp(argv[3], "lambda")) text = "{{ id:=&(\\A:@ => \\x:A => x); }}.id";
		else if (!strcmp(argv[3], "family"))
			text = "{{ id:=&(\\A:@ => \\R:A->@ => \\x:A => \\p:R x => p); }}.id";
		else if (!strcmp(argv[3], "append"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; List:=&(\\A:@=>@{nil:*;cons:A->*->*;});"
				"append:=&(\\A:@=>\\xs:List A=>xs @nil=>(\\ys:List A=>ys)"
				"@cons x rest=>(\\ys:List A=>(List A).cons x (*rest ys)));"
				"r:=&{append Nat ((List Nat).cons Nat.zero (List Nat).nil) (List Nat).nil;}; }}.r";
		else if (!strcmp(argv[3], "function-field"))
			text = "{{ Nat:=@{zero:*;succ:*->*;};"
				"D:=@\\i:Nat=>{mk:(k:Nat)->* k;next:((k:Nat)->* k)->* Nat.zero;};"
				"steps:=&(\\i:Nat=>\\v:D i=>v @mk k=>Nat.zero @next down=>Nat.succ (*down Nat.zero));"
				"r:=&{steps Nat.zero (D.next &(\\k:Nat=>D.mk k));}; }}.r";
		else if (!strcmp(argv[3], "index-alias"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; D:=@\\i:Nat=>{mk:(k:Nat)->* k;};"
				"f:=&(\\i:Nat=>\\v:D i=>v @mk k=>Nat.succ (Nat.succ i));"
				"r:=&{f Nat.zero (D.mk Nat.zero);}; }}.r";
		else if (!strcmp(argv[3], "nullary"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; Other:=@{zero:*;succ:*->*;}; r:=&{Nat.zero;}; }}.r";
		else if (!strcmp(argv[3], "constructor"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; r:=&{Nat.succ (Nat.succ Nat.zero);}; }}.r";
		else if (!strcmp(argv[3], "application"))
			text = "{{ Nat:=@{zero:*;}; f:=&(\\x:Nat=>x); r:=&{f (f Nat.zero);}; }}.r";
		else if (!strcmp(argv[3], "match"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; r:=&{(\\n:Nat=>n @zero=>Nat.zero @succ k=>Nat.succ *k) (Nat.succ (Nat.succ Nat.zero));}; }}.r";
		else if (!strcmp(argv[3], "fold"))
			text = "{{ Nat:=@{zero:*;succ:*->*;}; r:=&((Nat.succ Nat.zero) @#return x=>Nat.succ x); }}.r";
		else {
			assert(!strcmp(argv[3], "nominal"));
			text = "{{ Nat:=@{zero:*;succ:*->*;}; Other:=@{zero:*;succ:*->*;}; id:=&(\\x:Nat=>x); }}.id";
		}
		p = retained_program(text);
		FILE *file = fopen(argv[2], "w+b");
		struct pg_synthesis_job *selected[] = {p->root,
			pg_synthesis_evidence(&p->synthesis, pg_synthesis_result(p->root))};
		count = !strcmp(argv[1], "retained-write-typed") ? 2 : 1;
		assert(selected[1] && file && !pg_sources_write_retained(file, &p->synthesis, count, selected, p->retained_reductions));
		assert(!fclose(file));
	} else {
		FILE *file = fopen(argv[2], "rb");
		assert(file);
		p = pg_sources_read(file, 100000, &count, &roots);
		assert(p && (count == 1 || count == 2) && p->retained_reductions && !p->synthesis.steps);
		assert(!fclose(file) && !pg_synthesis_result(roots[0]));
		if (!strcmp(argv[1], "retained-resave")) {
			assert(argc == 4);
			file = fopen(argv[3], "w+b");
			assert(file && !pg_sources_write_retained(file, &p->synthesis, count, roots, p->retained_reductions));
			assert(!fclose(file) && !p->synthesis.steps && !p->evaluation.jobs.count);
		} else {
			assert(argc == 3);
			int reuse = !strcmp(argv[1], "retained-check");
			assert(reuse || !strcmp(argv[1], "retained-recompute"));
			check_retained_program(p, roots[count - 1], reuse);
			assert(pg_synthesis_result(roots[0]));
			const struct pg_evidence *source = pg_prove_force(&p->typing, pg_synthesis_result(roots[0]));
			assert(source && pg_evidence_subject(source)->core == pg_reduction_source(p->retained_reductions->roots[0]));
		}
	}
	pg_program_destroy(p);
}

int main(int argc, char **argv)
{
	if (argc == 3 && (!strcmp(argv[1], "retention-summary") || !strcmp(argv[1], "retention-check"))) {
		FILE *file = fopen(argv[2], "rb");
		size_t count;
		struct pg_synthesis_job *const *roots;
		assert(file);
		struct pg_program *p = pg_sources_read(file, 1000000, &count, &roots);
		assert(p && !fclose(file) && !p->synthesis.steps);
		const struct pg_reduction_archive *archive = p->retained_reductions;
		printf("retained=%d reductions=%zu phases=%zu steps=0\n", archive != NULL,
			archive ? archive->count : 0, archive ? archive->phase_count : 0);
		if (!strcmp(argv[1], "retention-check")) {
			assert(archive && archive->count);
			struct pg_reduction_check check;
			assert(!pg_reduction_check_init(&check, &p->evaluation, archive));
			while (pg_reduction_check_advance(&check, 1) == PG_COMPARISON_PENDING)
				assert(pg_reduction_check_steps(&check) < 1000000);
			for (size_t i = 0; i < archive->count; ++i) assert(pg_reduction_check_certificate(&check, i));
			pg_reduction_check_destroy(&check);
		}
		assert(!p->synthesis.steps);
		for (size_t i = 0; i < count; ++i) assert(!pg_synthesis_result(roots[i]));
		pg_program_destroy(p);
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "context-scopes")) { context_scopes(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "indexed-families")) { indexed_family_sources(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "constructor-inputs")) {
		constructor_inputs(); member_use_origins(); inferred_constructor_roots(); return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "match-origins")) { match_origins(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "fold-origins")) { fold_origins(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "application-origins")) { application_origins(); return 0; }
	if (argc == 2 && !strcmp(argv[1], "handler-scopes")) {
		int failed = handler_scopes(0);
		return failed | handler_scopes(1);
	}
	if (argc == 2 && !strcmp(argv[1], "operation-origins")) {
		int failed = handler_scopes(2);
		return failed | handler_scopes(3);
	}
	if (argc == 2 && !strcmp(argv[1], "handler-origins")) {
		int failed = handler_scopes(4);
		failed |= handler_scopes(5);
		return failed;
	}
	if (argc == 2 && !strcmp(argv[1], "handler-nesting")) {
		int failed = handler_scopes(6);
		return failed | handler_scopes(7);
	}
	if (argc == 2 && !strcmp(argv[1], "handler-boundaries")) {
		handler_save_boundaries();
		return 0;
	}
	if (argc > 1 && !strncmp(argv[1], "retained-", 9)) {
		retained_process(argc, argv);
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "normalization")) {
		normalization_requests();
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "prepared-module")) {
		prepared_module_checkpoint();
		module_save_boundaries();
		return 0;
	}
	assert(argc == 3);
	int origins = !strncmp(argv[1], "origin-", 7);
	int annotations = !strncmp(argv[1], "annotation-", 11);
	int module_annotations = !strncmp(argv[1], "module-annotation-", 18);
	int settled = !strcmp(argv[1], "module-annotation-write-settled");
	int functions = !strncmp(argv[1], "function-", 9);
	int nominal = origins || !strncmp(argv[1], "nominal-", 8);
	int writing = !strcmp(argv[1], "write") || !strcmp(argv[1], "nominal-write") || !strcmp(argv[1], "origin-write") || !strcmp(argv[1], "function-write") || !strcmp(argv[1], "annotation-write") || !strcmp(argv[1], "module-annotation-write");
	if (settled) writing = 1;
	if (writing) { definition_boundaries(); rule_environments(); parameter_origins(); }
	FILE *file = fopen(argv[2], writing ? "w+b" : "rb");
	assert(file);
	if (module_annotations) module_annotation_sources(file, writing, !strcmp(argv[1], "module-annotation-read-bulk") ? 64 : 1, settled);
	else if (annotations) annotation_sources(file, writing, !strcmp(argv[1], "annotation-read-bulk") ? 64 : 1);
	else if (functions) function_origins(file, writing);
	else if (nominal) nominal_sources(file, writing, !strcmp(argv[1], "nominal-read-bulk") ? 64 : 1, origins);
	else if (writing) write_sources(file);
	else read_sources(file, !strcmp(argv[1], "read-bulk") ? 64 : 1);
	assert(!fclose(file));
	return 0;
}
