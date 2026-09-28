#include "conversion.h"
#include "computation.h"
#include "iadt.h"

#include <assert.h>
#include <stdio.h>

static const struct pg_term *returned(struct pg_graph *g, const struct pg_term *x)
{
	return pg_application(g, pg_reference(g, &pg_return_operation), x);
}

static uint64_t check(struct pg_graph *g, const struct pg_term *left,
	const struct pg_term *right, enum pg_conversion_status expected, uint64_t chunk,
	int without_nf)
{
	struct pg_whnf_work work = {0};
	assert(!pg_whnf_work_init(&work, g));
	struct pg_conversion conversion;
	assert(!pg_conversion_init(&conversion, &work, left, right));
	while (pg_conversion_status(&conversion) == PG_CONVERSION_PENDING &&
		pg_conversion_steps(&conversion) < 10000)
		pg_conversion_advance(&conversion, chunk);
	assert(pg_conversion_status(&conversion) == expected);
	assert(!!pg_conversion_certificate(&conversion) == (expected == PG_CONVERSION_EQUAL));
	if (without_nf) assert(work.normal_forms.count == 0);
	uint64_t steps = pg_conversion_steps(&conversion);
	pg_conversion_destroy(&conversion);
	pg_whnf_work_destroy(&work);
	return steps;
}

static void compare(struct pg_graph *g, const struct pg_term *left,
	const struct pg_term *right, enum pg_conversion_status expected, int without_nf)
{
	uint64_t split = check(g, left, right, expected, 1, without_nf);
	uint64_t whole = check(g, left, right, expected, 64, without_nf);
	if (expected != PG_CONVERSION_PENDING) assert(split == whole);
}

int main(void)
{
	struct pg_graph g = {0};
	assert(!pg_graph_init(&g));
	size_t arities[] = {0, 1};
	const struct pg_data_layout *nat = pg_data_layout(&g, 2, arities);
	const struct pg_object *x = pg_binder(&g), *n = pg_binder(&g), *r = pg_binder(&g);
	const struct pg_object *a = pg_binder(&g), *self = pg_binder(&g), *v = pg_binder(&g);
	const struct pg_term *vx = pg_reference(&g, x), *vn = pg_reference(&g, n);
	const struct pg_term *vv = pg_reference(&g, v);
	const struct pg_term *zero = pg_reference(&g, pg_data_constructor(nat, 0));
	const struct pg_term *one = pg_application(&g, pg_reference(&g, pg_data_constructor(nat, 1)), zero);
	struct pg_match_clause recursive[] = {
		{pg_data_constructor(nat, 0), returned(&g, zero)},
		{pg_data_constructor(nat, 1), pg_lambda(&g, n, pg_application(&g, pg_reference(&g, r), vn))}
	};
	const struct pg_term *source = pg_data_recursive_match(&g, nat, r, a, self, vx, 2, recursive);
	struct pg_match_clause cases[] = {
		{pg_data_constructor(nat, 0), returned(&g, one)},
		{pg_data_constructor(nat, 1), pg_lambda(&g, n, returned(&g, zero))}
	};
	const struct pg_term *continuation = pg_lambda(&g, v, pg_data_match(&g, nat, vv, 2, cases));
	const struct pg_term *fold = pg_computation_fold(&g, source, continuation, 0, NULL);
	compare(&g, fold, returned(&g, vx), PG_CONVERSION_DIFFERENT, 1);
	compare(&g, returned(&g, vx), fold, PG_CONVERSION_DIFFERENT, 1);
	compare(&g, pg_application(&g, pg_reference(&g, &pg_fold_operation), source),
		pg_reference(&g, &pg_return_operation), PG_CONVERSION_DIFFERENT, 1);
	compare(&g, pg_computation_fold(&g, returned(&g, zero), continuation, 0, NULL),
		returned(&g, one), PG_CONVERSION_EQUAL, 1);
	const struct pg_term *identity = pg_lambda(&g, n, vn);
	struct pg_match_clause equivalent[] = {
		{pg_data_constructor(nat, 0), returned(&g, pg_application(&g, identity, one))},
		cases[1]
	};
	const struct pg_term *other = pg_lambda(&g, v, pg_data_match(&g, nat, vv, 2, equivalent));
	compare(&g, fold, pg_computation_fold(&g, source, other, 0, NULL), PG_CONVERSION_EQUAL, 1);
	equivalent[0].branch = returned(&g, zero);
	other = pg_lambda(&g, v, pg_data_match(&g, nat, vv, 2, equivalent));
	compare(&g, fold, pg_computation_fold(&g, source, other, 0, NULL), PG_CONVERSION_DIFFERENT, 1);
	/* Both ways of hiding a right unit must still normalize before comparison. */
	const struct pg_term *unit = pg_lambda(&g, v, returned(&g, pg_application(&g, identity, vv)));
	compare(&g, pg_computation_fold(&g, vx, unit, 0, NULL), vx, PG_CONVERSION_EQUAL, 0);
	unit = pg_lambda(&g, v, pg_application(&g, identity, returned(&g, vv)));
	compare(&g, vx, pg_computation_fold(&g, vx, unit, 0, NULL), PG_CONVERSION_EQUAL, 0);
	unit = pg_lambda(&g, v, returned(&g, vx));
	compare(&g, pg_computation_fold(&g, vx, unit, 0, NULL), vx, PG_CONVERSION_DIFFERENT, 0);
	/* Recognizing a blocked head does not execute or equate a divergent source. */
	const struct pg_term *loop = pg_lambda(&g, n, pg_application(&g, vn, vn));
	const struct pg_term *omega = pg_application(&g, loop, loop);
	compare(&g, pg_computation_fold(&g, omega, continuation, 0, NULL),
		returned(&g, vx), PG_CONVERSION_PENDING, 1);
	pg_graph_destroy(&g);
	puts("conversion head: blocked Fold, partial application, beta/iota, deferred eta and divergence passed");
	return 0;
}
