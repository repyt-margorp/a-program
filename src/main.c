#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"
#include "environment.h"
#include "parse.h"
#include "term.h"

// --- Static Memory Allocation ---
#define SYMBOL_MAP_CAPACITY      8192
#define SYMBOL_STORAGE_CAPACITY  4096
#define GLOBAL_ENV_CAPACITY      256
#define TERM_ARENA_SIZE          (1024 * 1024) // 1MB

// For symbol table
static int      g_map_symbol_ids[SYMBOL_MAP_CAPACITY];
static uint32_t g_map_hashes[SYMBOL_MAP_CAPACITY];
static char*    g_storage_strings[SYMBOL_STORAGE_CAPACITY];

// For global environment
static struct definition g_definitions[GLOBAL_ENV_CAPACITY];

// For term arena
static char g_term_arena_memory[TERM_ARENA_SIZE];


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[1];

    // 1. Declare all components
    struct symbol_table       syms;
    struct global_environment globals;
    struct term_arena         arena;
    struct parse_state        state;

    // 2. Initialize each component with static arrays
    symbol_table_init(&syms,
        g_map_symbol_ids, g_map_hashes, SYMBOL_MAP_CAPACITY,
        g_storage_strings, SYMBOL_STORAGE_CAPACITY);

    global_environment_init(&globals, g_definitions, GLOBAL_ENV_CAPACITY);

    arena.memory = g_term_arena_memory;
    arena.capacity = TERM_ARENA_SIZE;
    arena.offset = 0;

    // 3. Bundle all components into parse_state
    memset(&state, 0, sizeof(state));
    state.symbols = &syms;
    state.globals = &globals;
    state.arena = &arena;
    state.current_filename = filename;

    printf("--- System initialized ---\n");

    // 4. Invoke the parser
    if (parse_file(filename, &state) != 0 || state.error_count > 0) {
        fprintf(stderr, "Parse failed.\n");
        symbol_table_free(&syms);
        return 1;
    }

    // 5. Process the result
    if (state.result_term) {
        printf("Parsed Term: ");
        term_print(state.result_term);
        printf("\n");

        struct term* result;
        if (term_evaluate(&arena, &result, state.result_term) == 0) {
            printf("Evaluated: ");
            term_print(result);
            printf("\n");
        }
    } else {
        printf("No term parsed.\n");
    }

    // 6. Cleanup
    symbol_table_free(&syms);

    printf("--- System finished ---\n");
    return 0;
}
