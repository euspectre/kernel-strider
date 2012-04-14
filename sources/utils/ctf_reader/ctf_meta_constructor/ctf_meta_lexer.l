/* No wrapping after EOF */
%option noyywrap
/* These functions(macros) are not used, do not generate code for them */
%option nounput
%option noinput
%option noyylineno

/* Use state stack*/
%option stack
/* ...but without yy_top_state - do not generate code for it */
%option noyy_top_state

/* Use reentrant scanner for free it after parsing is ended. */
%option reentrant

%top{
/* Forward declaration for type of extra-data*/
struct ctf_meta_parser_state;
}
/* Extra data is really pointer to the parser state */
%option extra-type="struct ctf_meta_parser_state*"

/* 
 * No default action should be generated, so LEX check that we define
 * actions for all possible character sequences.
 */
%option nodefault
/*
 * Enable LEX warnings - for selfcontrol.
 */
%option warn
%{

#include "ctf_meta_parse.h"
#include "ctf_meta.h"

#include <stdio.h> /* printf */
#include <string.h> /* strdup, strcat...*/
#include <malloc.h> /* realloc for append string */
#include <assert.h> /* assertions */

#include <stdlib.h> /* strtoul */
#include <errno.h> /* errno variable */


/* 
 * Input: allocated string 'str'
 * Output: reallocated string equal to concatenation of 'str' and 'append';
 * no more than 'n' first characters of 'append' is readed.
 */
static char* strnappend_checked(char* str, const char* append, size_t n);

/*
 * Same as corresponded functions without '_checked' suffix, but check
 * result returned, and if it NULL, perform exit().
 */
static char* strdup_checked(const char* str);
static char* strndup_checked(const char* str, size_t n);

/*
 * Free string and set to NULL.
 */
#define clear_str(str) do {free(str); str = NULL;} while(0)

/* Add character to the collected string constant*/
#define string_add_char(c) yylval.str = strnappend_checked(yylval.str, c, 1)

/*
 * Use macro for update position inside parsed file.
 * 
 * We use fact that all our patterns either correspond to newlines only
 * or do not contain newline at all.
 */
#define YY_USER_ACTION                                  \
    yyextra->line_before_pattern    = yyextra->line;    \
    yyextra->column_before_pattern  = yyextra->column;  \
    if(yyleng && (*yytext == '\n'))                     \
    {                                                   \
        yyextra->line+= yyleng;                         \
        yyextra->column = FIRST_POS;                    \
    }                                                   \
    else                                                \
    {                                                   \
        yyextra->column += yyleng;                      \
    }

/* Output message about parse error and return error from the parser. */
#define parse_error(format, ...) do { fprintf(stderr,   \
    "%s:%d:%d: error: " format "\n",                    \
    yyextra->filename, yyextra->line_before_pattern,    \
    yyextra->column_before_pattern, ##__VA_ARGS__);     \
    return -1; } while(0)

/* Output message about parse warning. */
#define parse_warning(format, ...) fprintf(stderr,      \
    "%s:%d:%d: warning: " format "\n",                  \
    yyextra->filename, yyextra->line_before_pattern,    \
    yyextra->column_before_pattern, ##__VA_ARGS__)



/* 
 * Output message about internal parser error and terminate program.
 * 
 * Used when CTF constructor return unexpected error(may be,
 *     because of insufficient memory).
 */
#define internal_error(format, ...) do { fprintf(stderr,    \
    "Internal parser error at %s:%d while parse %s:%d:%d: " \
    format "\n", __FILE__, __LINE__,                        \
    yyextra->filename, yyextra->line_before_pattern,        \
    yyextra->column_before_pattern, ##__VA_ARGS__);         \
    exit(-1); } while(0)
%}

id                [a-zA-Z_][a-zA-Z_0-9]*

constant_dec    [1-9][0-9]*
constant_hex    0[xX][:xdigit:]+
constant_oct    0[1-7][0-7]*
constant_int    0|{constant_dec}|{constant_hex}|{constant_oct}

space            [ \t]
newline            \n

assign_position {id}(\.{id})*
/* 
 * Should corresponds to all keywords.
 *
 * May be used for reject invalid identificators.
 */
keyword        trace|stream|event|struct|integer

/* Inside comments */
%x COMMENTS

/*
 * Currently inside quotes. Outer state is stored at the top of the stack.
 * 
 * 'current_string' contains currently collected string.
 */
%x STRING

%%

<COMMENTS>{
"*/"        yy_pop_state(yyscanner);
"*"            ;
"\n"        ;
[^*\n]+        ;
<<EOF>>        parse_error("Unexpected EOF while parse comments.");
}

<STRING>{
"\""            {
    yy_pop_state(yyscanner);
    switch(YY_START)
    {
    case INITIAL:
        return STRING_LITERAL;
    break;
    default:
        internal_error("Unexpected state while parsing string is finished.");
    }
    }

"\\\""    string_add_char("\"");
"\\\\"    string_add_char("\\");
"\\n"    string_add_char("\n");
"\\"    parse_warning("Unrecognized escape sequence."); string_add_char("\\");

"\n"        |
[^\"\\\n]+    yylval.str = strnappend_checked(yylval.str, yytext, yyleng);

<<EOF>>    parse_error("Unexpected EOF while parse string in qoutes.");
}

    /* 
     * In almost all states space and newlines are ignored, same for
     * comments.
     * 
     * Cases, for which this is not true should be processed above.
     */
{space}+        ;
{newline}+      ;
"/*"            yy_push_state(COMMENTS, yyscanner);

struct          return STRUCT;
integer         return INTEGER;
enum            return ENUM;
variant         return VARIANT;

trace           return TRACE;
stream          return STREAM;
event           return EVENT;
typedef         return TYPEDEF;

"{"             |
"}"             |
"="             |
";"             |
"["             |
"]"             |
","             |
":"             |
">"             |
"<"             |
"."             return *yytext;

":="            return TYPE_ASSIGNMENT_OPERATOR;
"->"            return ARROW;
"..."           return DOTDOTDOT;

"\""            yylval.str = NULL; yy_push_state(STRING, yyscanner);

{id}            yylval.str = strndup_checked(yytext, yyleng); return ID;

{constant_int}  yylval.str = strndup_checked(yytext, yyleng); return INTEGER_CONSTANT;

.               return UNKNOWN;
<<EOF>>         return 0;
%%


int ctf_meta_lexer_state_init(yyscan_t* scanner,
    struct ctf_meta_parser_state* parser_state)
{
    if(yylex_init_extra(parser_state, scanner) != 0)
    {
        fprintf(stderr, "Failed to initialize scanner.");
        return -errno;
    }
    yyset_in(parser_state->f, *scanner);
    
    return 0;
}

void ctf_meta_lexer_state_destroy(yyscan_t scanner)
{
    yylex_destroy(scanner);
}

char* strndup_checked(const char* str, size_t n)
{
    char* result = strndup(str, n);
    if(result == NULL)
    {
        printf("Failed to duplicate string.\n");
        exit(-1);
    }
    return result;
}

char* strdup_checked(const char* str)
{
    char* result = strdup(str);
    if(result == NULL)
    {
        printf("Failed to duplicate string.\n");
        exit(-1);
    }
    return result;
}

char* strnappend_checked(char* str, const char* append, size_t n)
{
    if(str == NULL) return strndup_checked(append, n);
    
    size_t len = strlen(str);
    char* result = realloc(str, len + n + 1);
    if(result == NULL)
    {
        printf("Failed to reallocate string for append.\n");
        exit(-1);
    }
    
    strncpy(result + len, append, n);
    result[len + n] = '\0';
    return result;
}
