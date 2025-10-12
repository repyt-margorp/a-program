#include "term.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 

// --- Arena Allocator ---

// Helper to round up memory address to specified alignment
static size_t align_up(size_t offset, size_t align) {
    return (offset + align - 1) & ~(align - 1);
}

void* term_arena_alloc(struct term_arena* arena, size_t size, size_t align) {
    if (!arena || !arena->memory) {
        return NULL;
    }
    
    size_t aligned_offset = align_up(arena->offset, align);

    if (aligned_offset + size > arena->capacity) {
        // Arena is full
        return NULL;
    }
    
    void* ptr = arena->memory + aligned_offset;
    arena->offset = aligned_offset + size;
    
    return ptr;
}

// --- Forward Declarations ---
static int term_substitute(struct term_arena* arena, struct term** p_ret, struct term* expr, struct term* binder_to_replace, struct term* replacement);

// --- Print functions ---

// Maximum default depth for printing (prevents stack overflow and excessive output)
// Set conservatively to handle deeply nested lambda terms
#define TERM_PRINT_DEFAULT_MAX_DEPTH 100

// Stack frame for iterative term printing
struct print_frame {
    struct term* t;
    int remaining_depth;
    enum { BEFORE_CHILDREN, AFTER_FUNC, AFTER_ARGS } phase;
};

void term_print_depth(struct term* t, int max_depth) {
    if (!t) {
        printf("(null)");
        return;
    }

    // Calculate required stack size based on max_depth
    // Worst case: binary tree with depth max_depth requires 2*max_depth frames
    // Conservative estimate: 4*max_depth to handle continuation frames
    size_t stack_capacity = (max_depth > 0) ? (4 * max_depth) : 16;

    // Allocate stack for iterative traversal
    struct print_frame* stack = (struct print_frame*)malloc(sizeof(struct print_frame) * stack_capacity);
    if (!stack) {
        fprintf(stderr, "\n[Error: failed to allocate print stack]\n");
        return;
    }

    int stack_top = 0;

    // Push initial frame
    stack[stack_top++] = (struct print_frame){t, max_depth, BEFORE_CHILDREN};

    while (stack_top > 0) {
        if ((size_t)stack_top >= stack_capacity) {
            fprintf(stderr, "\n[Error: print stack overflow - depth exceeds calculation]\n");
            free(stack);
            return;
        }

        struct print_frame frame = stack[--stack_top];

        if (!frame.t) {
            printf("(null)");
            continue;
        }

        if (frame.remaining_depth <= 0) {
            printf("...");
            continue;
        }

        switch (frame.t->tag) {
            case TERM_TAG_VAR:
                printf("v@%p", (void*)frame.t->as.as_var.binder);
                break;

            case TERM_TAG_CONST:
                printf("c#%d", frame.t->as.as_const.symbol_id);
                break;

            case TERM_TAG_APP:
                if (frame.phase == BEFORE_CHILDREN) {
                    printf("(");
                    // Push continuation frame to print ")" after children
                    stack[stack_top++] = (struct print_frame){frame.t, frame.remaining_depth, AFTER_ARGS};
                    // Push argument
                    stack[stack_top++] = (struct print_frame){frame.t->as.as_app.argument, frame.remaining_depth - 1, BEFORE_CHILDREN};
                    // Push separator
                    stack[stack_top++] = (struct print_frame){frame.t, frame.remaining_depth, AFTER_FUNC};
                    // Push function
                    stack[stack_top++] = (struct print_frame){frame.t->as.as_app.function, frame.remaining_depth - 1, BEFORE_CHILDREN};
                } else if (frame.phase == AFTER_FUNC) {
                    printf(" ");
                } else { // AFTER_ARGS
                    printf(")");
                }
                break;

            case TERM_TAG_LAMBDA:
                if (frame.phase == BEFORE_CHILDREN) {
                    printf("(\\ @%p => ", (void*)frame.t);
                    // Push continuation to print ")"
                    stack[stack_top++] = (struct print_frame){frame.t, frame.remaining_depth, AFTER_ARGS};
                    // Push body
                    stack[stack_top++] = (struct print_frame){frame.t->as.as_lambda.body, frame.remaining_depth - 1, BEFORE_CHILDREN};
                } else { // AFTER_ARGS
                    printf(")");
                }
                break;

            default:
                printf("?");
                break;
        }
    }

    // Clean up
    free(stack);
}

void term_print(struct term* t) {
    term_print_depth(t, TERM_PRINT_DEFAULT_MAX_DEPTH);
}

// --- Create functions (using Arena) ---
int term_create_var(struct term_arena* arena, struct term** p_ret, struct term* binder) {
    struct term* nt = (struct term*)term_arena_alloc(arena, sizeof(struct term), _Alignof(struct term));
    if (!nt) return -1;
    nt->tag = TERM_TAG_VAR;
    nt->as.as_var.binder = binder;
    *p_ret = nt;
    return 0;
}

int term_create_const(struct term_arena* arena, struct term** p_ret, int symbol_id) {
    struct term* nt = (struct term*)term_arena_alloc(arena, sizeof(struct term), _Alignof(struct term));
    if (!nt) return -1;
    nt->tag = TERM_TAG_CONST;
    nt->as.as_const.symbol_id = symbol_id;
    *p_ret = nt;
    return 0;
}

int term_create_app(struct term_arena* arena, struct term** p_ret, struct term* func, struct term* arg) {
    struct term* nt = (struct term*)term_arena_alloc(arena, sizeof(struct term), _Alignof(struct term));
    if (!nt) return -1;
    nt->tag = TERM_TAG_APP;
    nt->as.as_app.function = func;
    nt->as.as_app.argument = arg;
    *p_ret = nt;
    return 0;
}

int term_create_lambda(struct term_arena* arena, struct term** p_ret, struct term* body) {
    struct term* nt = (struct term*)term_arena_alloc(arena, sizeof(struct term), _Alignof(struct term));
    if (!nt) return -1;
    nt->tag = TERM_TAG_LAMBDA;
    nt->as.as_lambda.body = body;
    *p_ret = nt;
    return 0;
}

// --- Computation functions (Ported and Adapted for Arena) ---

static int term_substitute(struct term_arena* arena, struct term** p_ret, struct term* expr, struct term* binder_to_replace, struct term* replacement) {
    if (!expr) { *p_ret = NULL; return 0; }
    switch (expr->tag) {
        case TERM_TAG_VAR:
            if (expr->as.as_var.binder == binder_to_replace) { *p_ret = replacement; return 0; }
            *p_ret = expr;
            return 0;
        case TERM_TAG_APP: {
            struct term *new_func, *new_arg;
            if (term_substitute(arena, &new_func, expr->as.as_app.function, binder_to_replace, replacement) != 0) return 1;
            if (term_substitute(arena, &new_arg, expr->as.as_app.argument, binder_to_replace, replacement) != 0) return 1;
            if (new_func == expr->as.as_app.function && new_arg == expr->as.as_app.argument) { *p_ret = expr; return 0; }
            return term_create_app(arena, p_ret, new_func, new_arg);
        }
        case TERM_TAG_LAMBDA: {
            if (expr == binder_to_replace) { *p_ret = expr; return 0; }
            struct term *new_body;
            if (term_substitute(arena, &new_body, expr->as.as_lambda.body, binder_to_replace, replacement) != 0) return 1;
            if (new_body == expr->as.as_lambda.body) { *p_ret = expr; return 0; }
            return term_create_lambda(arena, p_ret, new_body);
        }
        case TERM_TAG_CONST: default: *p_ret = expr; return 0;
    }
}

int term_reduce(struct term_arena* arena, struct term **p_ret, struct term *expr) {
    if (!expr) { *p_ret = NULL; return 0; }
    switch (expr->tag) {
        case TERM_TAG_APP: {
            struct term *reduced_func, *reduced_arg;
            if (term_reduce(arena, &reduced_func, expr->as.as_app.function) != 0) return 1;
            if (term_reduce(arena, &reduced_arg, expr->as.as_app.argument) != 0) return 1;
            if (reduced_func->tag == TERM_TAG_LAMBDA) { 
                return term_substitute(arena, p_ret, reduced_func->as.as_lambda.body, reduced_func, reduced_arg);
            }
            if (reduced_func != expr->as.as_app.function || reduced_arg != expr->as.as_app.argument) {
                return term_create_app(arena, p_ret, reduced_func, reduced_arg);
            }
            *p_ret = expr; return 0;
        }
        case TERM_TAG_LAMBDA: {
            struct term *new_body;
            if (term_reduce(arena, &new_body, expr->as.as_lambda.body) != 0) return 1;
            if (new_body != expr->as.as_lambda.body) { return term_create_lambda(arena, p_ret, new_body); }
            *p_ret = expr; return 0;
        }
        case TERM_TAG_VAR: case TERM_TAG_CONST: default: *p_ret = expr; return 0;
    }
}

// Maximum evaluation steps (prevents infinite loops on non-terminating terms)
// Examples of non-terminating terms: Omega combinator (\x.x x)(\x.x x)
// Set high enough for complex reductions (Church numerals, list operations)
#define TERM_EVAL_DEFAULT_MAX_STEPS 100000

int term_evaluate_steps(struct term_arena* arena, struct term **p_ret, struct term *expr, int max_steps) {
    struct term *current = expr;
    struct term *next;
    if (!expr) { *p_ret = NULL; return 0; }

    for (int step = 0; step < max_steps; step++) {
        if (term_reduce(arena, &next, current) != 0) return -1;
        if (next == current) {
            // Reached normal form
            *p_ret = current;
            return 0;
        }
        current = next;
    }

    // Evaluation did not converge within step limit
    fprintf(stderr, "Warning: evaluation exceeded %d steps (possible non-terminating term)\n", max_steps);
    *p_ret = current;
    return -1;
}

int term_evaluate(struct term_arena* arena, struct term **p_ret, struct term *expr) {
    return term_evaluate_steps(arena, p_ret, expr, TERM_EVAL_DEFAULT_MAX_STEPS);
}