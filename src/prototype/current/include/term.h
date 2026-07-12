#ifndef __TERM_H__
#define __TERM_H__

#include <stdint.h>
#include <stddef.h> // Required for size_t

// --- Arena Allocator ---
struct term_arena {
    char*    memory;
    size_t   capacity;
    size_t   offset;
};

void* term_arena_alloc(struct term_arena* arena, size_t size, size_t align);

// --- Term Definition ---
#define TERM_TAG_VAR      1
#define TERM_TAG_CONST    2
#define TERM_TAG_APP      3
#define TERM_TAG_LAMBDA   4

struct term;

struct term_var { struct term* binder; };
struct term_const { int symbol_id; };
struct term_app { struct term* function; struct term* argument; };
struct term_lambda { struct term* body; };

struct term {
    int tag;
    union {
        struct term_var      as_var;
        struct term_const    as_const;
        struct term_app      as_app;
        struct term_lambda   as_lambda;
    } as;
};

// --- Functions ---
// Construction functions allocate memory from arena
int term_create_var(struct term_arena* arena, struct term** p_ret, struct term* binder);
int term_create_const(struct term_arena* arena, struct term** p_ret, int symbol_id);
int term_create_app(struct term_arena* arena, struct term** p_ret, struct term* func, struct term* arg);
int term_create_lambda(struct term_arena* arena, struct term** p_ret, struct term* body);

void term_print(struct term* t);
void term_print_depth(struct term* t, int max_depth);

// Evaluation and reduction functions (require arena for creating new terms)
// Returns 0 on success, -1 on error
int term_evaluate(struct term_arena* arena, struct term** p_ret, struct term* expr);
int term_evaluate_steps(struct term_arena* arena, struct term** p_ret, struct term* expr, int max_steps);
int term_reduce(struct term_arena* arena, struct term** p_ret, struct term* expr);

#endif // __TERM_H__