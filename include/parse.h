#ifndef __PARSE_H__
#define __PARSE_H__

// Forward declarations
struct symbol_table;
struct global_environment;
struct term;
struct term_arena;
struct binding_context; // Stack of binding contexts used inside the parser

/**
 * @brief Structure that manages the state of the entire parsing process.
 *        A pointer to this structure is passed to the lexer and parser.
 */
struct parse_state {
    // Pointers to shared state for reading and writing
    struct symbol_table*       symbols;
    struct global_environment* globals;
    struct term_arena*         arena;

    // Top of temporary stack for locally nameless binding resolution
    struct binding_context*    local_binder_stack;

    // Location to store the result of parsing (generated term)
    struct term*               result_term;

    // Auxiliary information for error reporting
    const char*                current_filename;
    int                        error_count;
};

/**
 * @brief Entry point function to start the parsing process
 */
int parse_file(const char* path, struct parse_state* state);


#endif // __PARSE_H__