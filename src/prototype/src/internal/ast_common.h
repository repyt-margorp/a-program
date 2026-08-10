#ifndef A_PROGRAM_PROTOTYPE_INTERNAL_AST_COMMON_H
#define A_PROGRAM_PROTOTYPE_INTERNAL_AST_COMMON_H

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

#endif
