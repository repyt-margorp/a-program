%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations
%option extra-type="struct parse_state*"

%{
#include <string.h>
#include "parse.h"
#include "symbol.h"
#include "parser.tab.h"

#undef YY_USER_ACTION
#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; i < yyleng; i++) { \
        if(yytext[i] == '\n') { \
            yylloc->last_line++; \
            yylloc->last_column = 1; \
        } else { \
            yylloc->last_column++; \
        } \
    }
%}

%%

[[:space:]]+    { /* skip whitespace */ }
"//".*         { /* skip line comments */ }

"\\"            { return LAMBDA; }
"=>"            { return FATARROW; }
"("             { return LPAREN; }
")"             { return RPAREN; }
":="            { return ASSIGN; }

[a-zA-Z_][a-zA-Z0-9_]* {
    struct parse_state* state = yyget_extra(yyscanner);
    yylval->symbol_id = symbol_intern(state->symbols, yytext, yyleng);
    return IDENT;
}

.               {
    struct parse_state* state = yyget_extra(yyscanner);
    fprintf(stderr, "%s:%d:%d: Unknown character '%s'\n",
        state->current_filename ? state->current_filename : "<input>",
        yylloc->first_line, yylloc->first_column, yytext);
    state->error_count++;
}

%%
