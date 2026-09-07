#include "computation.h"

static const struct pg_object_class return_class = {"return"};
static const struct pg_object_class thunk_class = {"thunk"};
static const struct pg_object_class force_class = {"force"};
const struct pg_object pg_return_operation = {PG_SEMANTIC_OBJECT, &return_class};
const struct pg_object pg_thunk_operation = {PG_SEMANTIC_OBJECT, &thunk_class};
const struct pg_object pg_force_operation = {PG_SEMANTIC_OBJECT, &force_class};
