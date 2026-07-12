#include "symbol.h"
#include <stdlib.h>
#include <string.h>

// FNV-1a hash function
static uint32_t fnv1a(const char* s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) {
        h ^= (unsigned char)s[i];
        h *= 16777619u;
    }
    return h;
}

void symbol_table_init(
    struct symbol_table* t,
    int* map_ids_array,
    uint32_t* map_hashes_array,
    size_t map_capacity,
    char** storage_strings_array,
    size_t storage_capacity
) {
    t->map.symbol_ids = map_ids_array;
    t->map.hashes = map_hashes_array;
    t->map.capacity = map_capacity;
    t->map.count = 0;

    // Initialize map buckets as "empty" (-1)
    for (size_t i = 0; i < map_capacity; ++i) {
        t->map.symbol_ids[i] = -1;
    }

    t->storage.strings = storage_strings_array;
    t->storage.capacity = storage_capacity;
    t->storage.count = 0;
}

void symbol_table_free(struct symbol_table* t) {
    if (!t) return;

    // Free strings allocated by storage (malloc'd inside symbol_intern)
    for (size_t i = 0; i < t->storage.count; ++i) {
        if (t->storage.strings[i]) {
            free(t->storage.strings[i]);
        }
    }

    // Zero-clear the structure itself
    memset(t, 0, sizeof(struct symbol_table));
}

int symbol_intern(struct symbol_table* t, const char* s, size_t n) {
    if (t->map.capacity == 0) return -1; // Uninitialized

    // Check occupancy rate
    if (t->map.count >= t->map.capacity) {
        return -1; // Table is full
    }

    uint32_t h = fnv1a(s, n);
    size_t index = h & (t->map.capacity - 1);

    for (size_t i = 0; i < t->map.capacity; ++i) {
        size_t current_index = (index + i) & (t->map.capacity - 1);

        if (t->map.symbol_ids[current_index] == -1) { // Empty slot
            if (t->storage.count >= t->storage.capacity) {
                return -1; // Storage is full
            }

            char* new_string = (char*)malloc(n + 1);
            if (!new_string) return -1;
            memcpy(new_string, s, n);
            new_string[n] = '\0';

            int new_id = (int)t->storage.count;
            t->storage.strings[new_id] = new_string;
            t->storage.count++;

            t->map.symbol_ids[current_index] = new_id;
            t->map.hashes[current_index] = h;
            t->map.count++;

            return new_id;
        }

        if (t->map.hashes[current_index] == h) {
            int existing_id = t->map.symbol_ids[current_index];
            const char* existing_string = t->storage.strings[existing_id];
            if (strncmp(existing_string, s, n) == 0 && existing_string[n] == '\0') {
                return existing_id;
            }
        }
    }

    return -1; // Table is full
}

const char* symbol_to_string(const struct symbol_table* t, int id) {
    if (id < 0 || (size_t)id >= t->storage.count) {
        return NULL;
    }
    return t->storage.strings[id];
}

int symbol_map_is_used_at(const struct symbol_map* map, size_t index) {
    if (index >= map->capacity) {
        return 0;
    }
    return map->symbol_ids[index] != -1;
}
