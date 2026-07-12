#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <stddef.h> // Required for size_t

// Forward declarations
struct term;

/**
 * @brief A single top-level definition (pair of symbol ID and term pointer)
 */
struct definition {
    int symbol_id;
    struct term* term;
};

/**
 * @brief Global environment that stores all top-level definitions
 */
struct global_environment {
    struct definition* definitions;
    size_t capacity;
    size_t count;
};

// --- Functions ---

/**
 * @brief Initialize global environment with pre-allocated memory block
 * @param env Environment to initialize
 * @param def_array Pre-allocated array of definitions
 * @param capacity Number of elements in def_array
 */
void global_environment_init(struct global_environment* env, struct definition* def_array, size_t capacity);

/**
 * @brief Add a new definition to the environment
 * @return 0 on success, -1 on failure (full)
 */
int global_environment_define(struct global_environment* env, int symbol_id, struct term* term);

/**
 * @brief Look up a definition by symbol ID
 * @return Pointer to term if found, NULL otherwise
 */
struct term* global_environment_lookup(const struct global_environment* env, int symbol_id);


#endif // __ENVIRONMENT_H__