#ifndef __PROTOTYPE_AST_INSPECT_H__
#define __PROTOTYPE_AST_INSPECT_H__

#include <stdio.h>

#include "ast.h"
#include "symbol.h"

void prototype_ast_inspect_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_ast_db* asts
);

#endif
