/* 
 * Common definitions for parser and lexer
 * (excluding ones in generated bison header file).
 */

#ifndef CTF_META_PARSE_H
#define CTF_META_PARSE_H

#include <stdio.h> /* FILE */
#include "ctf_meta.h" /* struct meta* */

/* 
 * Including flex-generated definition file into flex-generated source
 * make this source incorrect(YY_DO_BEFORE_ACTION became undefined).
 * 
 * Using ifndef we prevent that situation.
 * (YY_START is standard LEX macro).
 */
#ifndef YY_START
#include "ctf_meta_lexer.h"
#endif

#include "ctf_meta_parser.tab.h"

#include "linked_list.h"

/* Index of first position in line; used for output messages*/
#define FIRST_POS 0

struct ctf_meta_parser_state;
/* Initialize LEX, using parser state as extra data. */
int ctf_meta_lexer_state_init(yyscan_t* scanner,
    struct ctf_meta_parser_state* parser_state);

void ctf_meta_lexer_state_destroy(yyscan_t scanner);

/* State of the parser which also contain state of the lexer */
struct ctf_meta_parser_state
{
    /* File currently parsed */
    FILE* f;
    /* Name of the file currently parsed */
    const char* filename;

    /* Line and offset in it AFTER last pattern matching */
    int line;
    int column;
    
    /* 
     * Line and offset in it BEFORE last pattern matching.
     *
     * These values with 'filename' are used in parse error
     * reporting.
     */
    int line_before_pattern;
    int column_before_pattern;
    /* Note, that it is only reference to existed AST. AST doestn't belong to state. */
    struct ctf_ast* ast;
    /* Common state for lexer, extra data of which points to the parser state. */
    yyscan_t scanner;
};


#endif /* CTF_META_PARSE_H */