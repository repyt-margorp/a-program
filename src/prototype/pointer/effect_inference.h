#ifndef A_PROGRAM_POINTER_EFFECT_INFERENCE_H
#define A_PROGRAM_POINTER_EFFECT_INFERENCE_H

#include "classifier.h"

struct pg_effect_equation;
struct pg_effect_dependency;
struct pg_effect_inference {
	struct pg_graph arena;
	struct pg_graph *rows;
	struct pg_index dependencies;
	struct pg_effect_equation *head, *tail, *current;
	struct pg_effect_dependency *cursor;
	int sealed, failed;
};

/* Finite positive set equations: target = seed union all (source \ mask)
 * incoming contributions. All seeds/masks are closed rows. Equation sites
 * have fresh pointer identity; identical dependency tuples are shared.
 * This is least-effect inference, NOT unification of arbitrary open row metas.
 * Seal only after every contribution is known. No result is exposed before
 * sealing and convergence; these results are not typing certificates.
 * rows and supplied seed/mask rows outlive this work; equation handles live
 * until destroy. Do not relocate/reinitialize work while handles are live. */
int pg_effect_inference_init(struct pg_effect_inference *work, struct pg_graph *rows);
void pg_effect_inference_destroy(struct pg_effect_inference *work);
struct pg_effect_equation *pg_effect_equation(struct pg_effect_inference *work,
	const struct pg_effect_row *seed);
int pg_effect_dependency(struct pg_effect_inference *work,
	struct pg_effect_equation *source, const struct pg_effect_row *mask,
	struct pg_effect_equation *target);
/* Deep-handler effect equation. Only the input edge subtracts handled labels;
 * return/clause effects escape this handler. A clause may depend on target
 * through its resumption. All equation handles must belong to work. */
int pg_effect_handler_dependencies(struct pg_effect_inference *work,
	struct pg_effect_equation *target, struct pg_effect_equation *input,
	const struct pg_effect_row *handled, struct pg_effect_equation *returned,
	size_t count, struct pg_effect_equation *const *clauses);
void pg_effect_inference_seal(struct pg_effect_inference *work);
/* -1 failure, 0 pending, 1 converged. One transition processes one edge or
 * one empty adjacency list. Row set operations are not wall-time bounded. */
int pg_effect_inference_advance(struct pg_effect_inference *work, uint64_t budget);
const struct pg_effect_row *pg_effect_inference_result(const struct pg_effect_inference *work,
	const struct pg_effect_equation *equation);

#endif
