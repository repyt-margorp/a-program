#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <stddef.h>
#include <stdint.h>

// Symbol table data structures
struct symbol_map {
    int*      symbol_ids;
    uint32_t* hashes;
    size_t    capacity;
    size_t    count;
};
struct symbol_storage {
    char** strings;
    size_t count;
    size_t capacity;
};
struct symbol_table {
    struct symbol_map     map;
    struct symbol_storage storage;
};

// --- Functions ---

/**
 * @brief Initialize symbol table with pre-allocated memory block
 */
void symbol_table_init(
    struct symbol_table* t,
    int* map_ids_array,
    uint32_t* map_hashes_array,
    size_t map_capacity,
    char** storage_strings_array,
    size_t storage_capacity
);

void symbol_table_free(struct symbol_table* t);
int symbol_intern(struct symbol_table* t, const char* s, size_t n);
const char* symbol_to_string(const struct symbol_table* t, int id);
int symbol_map_is_used_at(const struct symbol_map* map, size_t index);

#endif // __SYMBOL_H__
