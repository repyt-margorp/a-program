#include "environment.h"
#include <stddef.h> // for NULL

void global_environment_init(struct global_environment* env, struct definition* def_array, size_t capacity) {
    env->definitions = def_array;
    env->capacity = capacity;
    env->count = 0;
}

int global_environment_define(struct global_environment* env, int symbol_id, struct term* term) {
    // Return error if environment is full
    if (env->count >= env->capacity) {
        return -1;
    }

    // Check if a definition with the same name already exists (allow simple overwrite)
    for (size_t i = 0; i < env->count; ++i) {
        if (env->definitions[i].symbol_id == symbol_id) {
            env->definitions[i].term = term;
            return 0;
        }
    }

    // Add new definition
    env->definitions[env->count].symbol_id = symbol_id;
    env->definitions[env->count].term = term;
    env->count++;

    return 0;
}

struct term* global_environment_lookup(const struct global_environment* env, int symbol_id) {
    // Linear search for symbol ID
    for (size_t i = 0; i < env->count; ++i) {
        if (env->definitions[i].symbol_id == symbol_id) {
            return env->definitions[i].term;
        }
    }
    return NULL; // Not found
}
