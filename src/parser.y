%define api.pure full
%define parse.error verbose
%locations

%code requires {
#include "parse.h"

// Forward declaration for scanner type
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
}

%parse-param { yyscan_t scanner } { struct parse_state* state }
%lex-param { yyscan_t scanner }

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"
#include "environment.h"
#include "term.h"

struct binding_context {
    int symbol_id;
    struct term* binder_term;
    struct binding_context* next;
};
%}

%union {
    int symbol_id;
    struct term* term;
}

%code {
int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, yyscan_t scanner);
void yyerror(YYLTYPE* llocp, yyscan_t scanner, struct parse_state* state, const char* msg);
}

%token <symbol_id> IDENT
%token ASSIGN LAMBDA FATARROW LPAREN RPAREN

%type <term> program program_entry term_expr lambda_expr app_expr atom_expr
%type <term> lambda_head // Lambda head with binder context

%start program

%%

program
    : program_entry { state->result_term = $1; }
    ;

program_entry
    : term_expr
    | IDENT ASSIGN term_expr {
        // global_environment_define(state->globals, $1, $3);
        $$ = $3;
    }
    ;

term_expr
    : lambda_expr
    ;

lambda_expr
    : lambda_head term_expr {
        struct term* lambda_binder = $1;
        lambda_binder->as.as_lambda.body = $2;
        
        if (state->local_binder_stack) {
            state->local_binder_stack = state->local_binder_stack->next;
        }
        $$ = lambda_binder;
    }
    | app_expr
    ;

// Parse lambda head `\x =>` and prepare binding context
lambda_head
    : LAMBDA IDENT FATARROW {
        struct term* lambda_binder;
        if (term_create_lambda(state->arena, &lambda_binder, NULL) != 0) {
            yyerror(&@$, scanner, state, "out of memory for lambda term");
            YYABORT;
        }

        struct binding_context* new_entry = (struct binding_context*)term_arena_alloc(
            state->arena, sizeof(struct binding_context), _Alignof(struct binding_context));
        if (!new_entry) { yyerror(&@$, scanner, state, "out of memory for context"); YYABORT; }
        
        new_entry->symbol_id = $2;
        new_entry->binder_term = lambda_binder;
        new_entry->next = state->local_binder_stack;
        state->local_binder_stack = new_entry;

        $$ = lambda_binder;
    }
    ;

app_expr
    : app_expr atom_expr {
        if (term_create_app(state->arena, &$$, $1, $2) != 0) {
            yyerror(&@$, scanner, state, "out of memory for app term");
            YYABORT;
        }
    }
    | atom_expr
    ;

atom_expr
    : IDENT {
        struct term* binder = NULL;
        for (struct binding_context* p = state->local_binder_stack; p; p = p->next) {
            if (p->symbol_id == $1) {
                binder = p->binder_term;
                break;
            }
        }

        if (binder) {
            if (term_create_var(state->arena, &$$, binder) != 0) {
                yyerror(&@$, scanner, state, "out of memory for var term");
                YYABORT;
            }
        } else {
            if (term_create_const(state->arena, &$$, $1) != 0) {
                yyerror(&@$, scanner, state, "out of memory for const term");
                YYABORT;
            }
        }
    }
    | LPAREN term_expr RPAREN { $$ = $2; }
    ;

%%

void yyerror(YYLTYPE* llocp, yyscan_t scanner, struct parse_state* state, const char* msg) {
    (void)scanner; // Unused parameter
    fprintf(stderr, "%s:%d:%d: %s\n",
        state->current_filename ? state->current_filename : "<input>",
        llocp->first_line, llocp->first_column, msg);
    state->error_count++;
}