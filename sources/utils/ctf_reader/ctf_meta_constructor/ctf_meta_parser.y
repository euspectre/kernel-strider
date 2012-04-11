%{
#include "ctf_meta_parse.h"
%}
/* 
 * Reentrant LEX scanner accept additional parameter
 */
%lex-param  {yyscan_t scanner}
/* Parser accept state parameter. */
%parse-param {struct ctf_meta_parser_state* state}
/* Parser accept parameter for scanner */
%parse-param {yyscan_t scanner}
%{
#include "ctf_meta.h"
#include "ctf_ast.h"

#include <stdio.h> /* printf */
#include <string.h> /* strdup, strcat...*/
#include <malloc.h> /* realloc for append string */
#include <assert.h> /* assertions */

#include <stdlib.h> /* strtoul */
#include <errno.h> /* errno variable */

#include <stdarg.h> /* va_arg */

static void yyerror(struct ctf_meta_parser_state* state, yyscan_t scanner,
    char const *s)
{
	(void)scanner;
    fprintf (stderr, "%d:%d: %s\n", state->line, state->column, s);
}

/* 
 * Macro for call inside actions for terminate parser with report about
 * insufficient memory.
 */
#define nomem() return 2

/*
 * Append formatted string to allocated one.
 */
static char* strappend_format(char* str, const char* append_format,...);

/* 
 * Initialize state of the parser. Also initialize lexer.
 */
static int ctf_meta_parser_state_init(struct ctf_meta_parser_state *state,
    struct ctf_ast* ast, const char* filename);

/* 
 * Free all resources used be the parser. Also destroy lexer.
 */
static void ctf_meta_parser_state_destroy(struct ctf_meta_parser_state* state);

/* Output message about parse error and return error from the parser. */
#define parse_error(format, ...) do { fprintf(stderr,   \
    "%s:%d:%d: error: " format "\n",                    \
    state->filename, state->line_before_pattern,        \
    state->column_before_pattern, ##__VA_ARGS__);       \
    return -1; } while(0)

/* Output message about parse warning. */
#define parse_warning(format, ...) fprintf(stderr,      \
    "%s:%d:%d: warning: " format "\n",                  \
    state->filename, state->line_before_pattern,        \
    state->column_before_pattern, ##__VA_ARGS__)



/* 
 * Output message about internal parser error and terminate program.
 * 
 * Used when CTF constructor return unexpected error(may be,
 * 	because of insufficient memory).
 */
#define internal_error(format, ...) do { fprintf(stderr,    \
    "Internal parser error at %s:%d while parse %s:%d:%d: " \
    format "\n", __FILE__, __LINE__,                        \
    state->filename, state->line_before_pattern,            \
    state->column_before_pattern, ##__VA_ARGS__);              \
	exit(-1); } while(0)
%}

/********************* Tokens ******************************************/

/* Keywords defining types */
%token ENUM STRUCT INTEGER VARIANT

%token TYPEDEF
/* Keywords defining first path-components of dynamic types */
%token TRACE STREAM EVENT
/* ":=" */
%token TYPE_ASSIGNMENT_OPERATOR
/* "->" */
%token ARROW
/* "..." */
%token DOTDOTDOT


%union {
    char* str;
}
%destructor { free($$);} <str>

%token <str> ID
%token <str> STRING_LITERAL
%token <str> INTEGER_CONSTANT

%token  UNKNOWN

/************************ Grouping ************************************/

%union {
    struct ctf_parse_scope_top* scope_top;
    struct ctf_parse_scope_struct* scope_struct;
    struct ctf_parse_scope_variant* scope_variant;
    struct ctf_parse_scope_int* scope_int;
    struct ctf_parse_scope_enum* scope_enum;
}
%destructor {ctf_parse_scope_destroy(&($$->base));} <scope_top>
%destructor {ctf_parse_scope_destroy(&($$->base));} <scope_struct>
%destructor {ctf_parse_scope_destroy(&($$->base));} <scope_variant>
%destructor {ctf_parse_scope_destroy(&($$->base));} <scope_int>
%destructor {ctf_parse_scope_destroy(&($$->base));} <scope_enum>

%union {
    struct ctf_parse_statement* statement;
    struct ctf_parse_scope_top_decl* scope_top_decl;
    struct ctf_parse_struct_decl* struct_decl;
    struct ctf_parse_variant_decl* variant_decl;
    struct ctf_parse_enum_decl* enum_decl;
    struct ctf_parse_typedef_decl* typedef_decl;
    struct ctf_parse_field_decl* field_decl;
    struct ctf_parse_param_def* param_def;
    struct ctf_parse_type_assignment* type_assignment;
}
%destructor { ctf_parse_statement_destroy($$); } <statement>
%destructor { ctf_parse_statement_destroy(&($$->base));} <scope_top_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <struct_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <variant_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <enum_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <typedef_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <field_decl>
%destructor { ctf_parse_statement_destroy(&($$->base));} <param_def>
%destructor { ctf_parse_statement_destroy(&($$->base));} <type_assignment>

%union {
    struct ctf_parse_type_spec* type_spec;
    struct ctf_parse_struct_spec* struct_spec;
    struct ctf_parse_variant_spec* variant_spec;
    struct ctf_parse_enum_spec* enum_spec;
    struct ctf_parse_type_spec_id* type_spec_id;
    struct ctf_parse_int_spec* int_spec;
}
%destructor { ctf_parse_type_spec_destroy($$);} <type_spec>
%destructor { ctf_parse_type_spec_destroy(&($$->base));} <struct_spec>
%destructor { ctf_parse_type_spec_destroy(&($$->base));} <variant_spec>
%destructor { ctf_parse_type_spec_destroy(&($$->base));} <enum_spec>
%destructor { ctf_parse_type_spec_destroy(&($$->base));} <type_spec_id>
%destructor { ctf_parse_type_spec_destroy(&($$->base));} <int_spec>

%union {
    struct ctf_parse_enum_value* enum_value;
    struct ctf_parse_enum_value_simple* enum_value_simple;
    struct ctf_parse_enum_value_presize* enum_value_presize;
    struct ctf_parse_enum_value_range* enum_value_range;
}

%destructor { ctf_parse_enum_value_destroy($$);} <enum_value>
%destructor { ctf_parse_enum_value_destroy(&$$->base);} <enum_value_simple>
%destructor { ctf_parse_enum_value_destroy(&$$->base);} <enum_value_presize>
%destructor { ctf_parse_enum_value_destroy(&$$->base);} <enum_value_range>

%union {
    struct ctf_parse_type_post_mod* type_post_mod;
    struct ctf_parse_type_post_mod_array* type_post_mod_array;
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence;
}
%destructor { ctf_parse_type_post_mod_destroy($$);} <type_post_mod>
%destructor { ctf_parse_type_post_mod_destroy(&$$->base);} <type_post_mod_array>
%destructor { ctf_parse_type_post_mod_destroy(&$$->base);} <type_post_mod_sequence>

%union {
    struct ctf_parse_type_post_mod_list* type_post_mod_list;
}
%destructor { ctf_parse_type_post_mod_list_destroy($$);} <type_post_mod_list>

%start meta

/* Name of the top-level scope */
%type <str> top_scope_name
/* Value of the parameter */
%type <str> param_value

/*
 * Reference in the type-fields hierarchy.
 * 
 * Something like "stream.event.header.several_events[4].type"
 */
%type <str> tag_reference

%type <str> tag_component

%type <scope_top> top_scope
%type <scope_struct> struct_scope
%type <scope_int> int_scope
%type <scope_enum> enum_scope
%type <scope_variant> variant_scope

%type <statement> meta_s top_scope_s struct_scope_s variant_scope_s type_decl int_scope_s
%type <scope_top_decl> top_scope_decl
%type <struct_decl> struct_decl
%type <variant_decl> variant_decl
%type <enum_decl> enum_decl
%type <typedef_decl> typedef_decl
%type <field_decl> field_decl
%type <param_def> param_def
%type <type_assignment> type_assign

%type <enum_value> enum_value
%type <enum_value_simple> enum_value_simple
%type <enum_value_presize> enum_value_presize
%type <enum_value_range> enum_value_range

%type <type_spec> type_spec
/* Specificator for struct, either existing or just created. */
%type <struct_spec> struct_spec
/* Specificator for struct, which is just created. */
/* %type <struct_spec> struct_spec_c */
%type <variant_spec> variant_spec
%type <enum_spec> enum_spec
/* Specificator for type which use only type identificator */
%type <type_spec_id> type_spec_id
%type <int_spec> int_spec

%type <type_post_mod> type_post_mod
%type <type_post_mod_array> type_post_mod_array
%type <type_post_mod_sequence> type_post_mod_sequence

%type <type_post_mod_list> type_post_mod_list

%%
meta                : /* empty */
                    | meta meta_s
                        { ctf_parse_scope_add_statement(&state->ast->scope_root->base, $2); }

meta_s              : type_decl
                    | top_scope_decl
                        { $$ = &($1->base); }
                    
top_scope_decl:     top_scope_name '{' top_scope '}' ';'
                        {
                            $$ = ctf_parse_scope_top_decl_create();
                            if($$ == NULL) nomem();
                            $$->scope_name = $1;
                            ctf_parse_scope_top_connect($3, $$);
                        }
type_decl           : struct_decl
                        { $$ = &($1->base);}
                    | variant_decl
                        { $$ = &($1->base);}
                    | enum_decl
                        { $$ = &($1->base);}
                    | typedef_decl
                        { $$ = &($1->base);}

struct_decl         : struct_spec ';'
                        { 
                            $$ = ctf_parse_struct_decl_create();
                            $$->struct_spec = $1;
                        }

struct_spec         : STRUCT ID '{' struct_scope '}'
                        { 
                            $$ = ctf_parse_struct_spec_create();
                            if($$ == NULL) nomem();
                            $$->struct_name = $2;
                            ctf_parse_scope_struct_connect($4, $$);
                        }
                    | STRUCT '{' struct_scope '}'
                        { 
                            $$ = ctf_parse_struct_spec_create();
                            if($$ == NULL) nomem();
                            $$->struct_name = NULL;
                            ctf_parse_scope_struct_connect($3, $$);
                        }
                    | STRUCT ID
                        { 
                            $$ = ctf_parse_struct_spec_create();
                            if($$ == NULL) nomem();
                            $$->struct_name = $2;
                        }

variant_decl         : variant_spec ';'
                        { 
                            $$ = ctf_parse_variant_decl_create();
                            $$->variant_spec = $1;
                        }

variant_spec         : VARIANT ID '{' variant_scope '}'
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = $2;
                            ctf_parse_scope_variant_connect($4, $$);
                        }
                    | VARIANT ID '<' tag_reference '>' '{' variant_scope '}' 
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = $2;
                            ctf_parse_scope_variant_connect($7, $$);
                            $$->variant_tag = $4;
                        }
                    | VARIANT '{' variant_scope '}'
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = NULL;
                            ctf_parse_scope_variant_connect($3, $$);
                        }
                    | VARIANT '<' tag_reference '>' '{' variant_scope '}' 
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = NULL;
                            ctf_parse_scope_variant_connect($6, $$);
                            $$->variant_tag = $3;
                        }
                    | VARIANT ID
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = $2;
                        }
                    | VARIANT ID '<' tag_reference '>'
                        { 
                            $$ = ctf_parse_variant_spec_create();
                            if($$ == NULL) nomem();
                            $$->variant_name = $2;
                            $$->variant_tag = $4;
                        }


enum_decl           : enum_spec ';'
                        { 
                            $$ = ctf_parse_enum_decl_create();
                            $$->enum_spec = $1;
                        }

enum_spec           : ENUM ID '{' enum_scope '}' ':' type_spec
                        {
                            $$ = ctf_parse_enum_spec_create();
                            if($$ == NULL) nomem();
                            $$->enum_name = $2;
                            $$->scope_enum = $4;
                            $$->type_spec_int = $7;
                        }
                    | ENUM '{' enum_scope '}' ':' type_spec
                        {
                            $$ = ctf_parse_enum_spec_create();
                            if($$ == NULL) nomem();
                            $$->scope_enum = $3;
                            $$->type_spec_int = $6;
                        }
                    | ENUM ID
                        {
                            $$ = ctf_parse_enum_spec_create();
                            if($$ == NULL) nomem();
                            $$->enum_name = $2;
                        }

enum_scope          : enum_value
                        {
                            $$ = ctf_parse_scope_enum_create();
                            if($$ == NULL) nomem();
                            ctf_parse_scope_enum_add_value($$, $1);
                        }
                    | enum_scope ',' enum_value
                        { 
                            $$ = $1;
                            ctf_parse_scope_enum_add_value($$, $3);
                        }
                        
enum_value          : enum_value_simple
                        {$$ = &$1->base;}
                    | enum_value_presize
                        {$$ = &$1->base;}
                    | enum_value_range
                        {$$ = &$1->base;}

enum_value_simple   : ID
                        {
                            $$ = ctf_parse_enum_value_simple_create();
                            if($$ == NULL) nomem();
                            $$->val_name = $1;
                        }

enum_value_presize  : ID '=' INTEGER_CONSTANT
                        {
                            $$ = ctf_parse_enum_value_presize_create();
                            if($$ == NULL) nomem();
                            $$->val_name = $1;
                            $$->int_value = $3;
                        }
enum_value_range    : ID '=' INTEGER_CONSTANT DOTDOTDOT INTEGER_CONSTANT
                        {
                            $$ = ctf_parse_enum_value_range_create();
                            if($$ == NULL) nomem();
                            $$->val_name = $1;
                            $$->int_value_start = $3;
                            $$->int_value_end = $5;
                        }


top_scope_name      : TRACE
                        {$$ = strdup("trace"); if($$ == NULL) nomem(); }
                    | STREAM
                        {$$ = strdup("stream"); if($$ == NULL) nomem(); }
                    | EVENT
                        {$$ = strdup("event"); if($$ == NULL) nomem(); }

top_scope           : /* empty */
                        { $$ = ctf_parse_scope_top_create(); if($$ == NULL) nomem();}
                    | top_scope top_scope_s
                        { $$ = $1; ctf_parse_scope_add_statement(&$$->base, $2); }

top_scope_s         : type_decl
                    | type_assign
                        {$$ = &$1->base;}

struct_scope        : /* empty */
                        { $$ = ctf_parse_scope_struct_create(); if($$ == NULL) nomem();}
                    | struct_scope struct_scope_s
                        { $$ = $1; ctf_parse_scope_add_statement(&$$->base, $2); }

struct_scope_s      : type_decl
                    | field_decl {$$ = &$1->base;}

variant_scope        : /* empty */
                        { $$ = ctf_parse_scope_variant_create(); if($$ == NULL) nomem();}
                    | variant_scope variant_scope_s
                        { $$ = $1; ctf_parse_scope_add_statement(&$$->base, $2); }

variant_scope_s      : type_decl
                    | field_decl {$$ = &$1->base;}


field_decl          : type_spec ID type_post_mod_list ';'
                        {
                            $$ = ctf_parse_field_decl_create();
                            if($$ == NULL) nomem();
                            $$->type_spec = $1;
                            $$->field_name = $2;
                            $$->type_post_mod_list = $3;
                        }
type_spec           : struct_spec {$$ = &$1->base;}
                    | variant_spec {$$ = &$1->base;}
                    | type_spec_id {$$ = &$1->base;}
                    | int_spec  {$$ = &$1->base;}
                    | enum_spec  {$$ = &$1->base;}

type_spec_id        : ID
                        {
                            $$ = ctf_parse_type_spec_id_create();
                            if($$ == NULL) nomem();
                            $$->type_name = $1;
                        }

int_spec            : INTEGER '{' int_scope '}'
                        {
                            $$ = ctf_parse_int_spec_create();
                            if($$ == NULL) nomem();
                            $$->scope_int = $3;
                        }

int_scope           : /* empty */
                        {$$ = ctf_parse_scope_int_create(); if($$ == NULL) nomem(); }
                    | int_scope int_scope_s
                        {$$ = $1; ctf_parse_scope_add_statement(&$$->base, $2); }

int_scope_s         : param_def
                        {$$ = &$1->base;}

param_def           : ID '=' param_value ';'
                        {
                            $$ = ctf_parse_param_def_create();
                            if($$ == NULL) nomem();
                            $$->param_name = $1;
                            $$->param_value = $3;
                        }
param_value         : ID
                    | STRING_LITERAL
                    | INTEGER_CONSTANT

type_assign         : tag_reference TYPE_ASSIGNMENT_OPERATOR type_spec ';'
                        { 
                            $$ = ctf_parse_type_assignment_create();
                            if($$ == NULL) nomem();
                            
                            $$->tag = $1;
                            $$->type_spec = $3;
                        }


tag_reference       : tag_component
                    | tag_reference '.' tag_component
                        {$$ = strappend_format($1, ".%s", $3); free($3); if($$ == NULL) nomem();}
                    | tag_reference ARROW tag_component
                        {$$ = strappend_format($1, ".%s", $3); free($3); if($$ == NULL) nomem();}
                    | tag_reference '[' INTEGER_CONSTANT ']'
                        {$$ = strappend_format($1, "[%s]", $3); free($3); if($$ == NULL) nomem();}

tag_component       : ID
                    | TRACE
                        {$$ = strdup("trace"); if($$ == NULL) nomem(); }
                    | STREAM
                        {$$ = strdup("stream"); if($$ == NULL) nomem(); }
                    | EVENT
                        {$$ = strdup("event"); if($$ == NULL) nomem(); }

typedef_decl        : TYPEDEF type_spec ID type_post_mod_list ';'
                    {
                        $$ = ctf_parse_typedef_decl_create();
                        if($$ == NULL) nomem();
                        $$->type_spec_base = $2;
                        $$->type_name = $3;
                        $$->type_post_mod_list = $4;
                    }

type_post_mod_list  : /* empty */
                        {
                            $$ = ctf_parse_type_post_mod_list_create();
                            if($$ == NULL) nomem();
                        }
                    | type_post_mod_list type_post_mod
                        {
                            $$ = $1;
                            ctf_parse_type_post_mod_list_add_mod($$, $2);
                        }

type_post_mod       : type_post_mod_array
                        { $$ = &$1->base;}
                    | type_post_mod_sequence
                        { $$ = &$1->base;}

type_post_mod_array : '[' INTEGER_CONSTANT ']'
                        {
                            $$ = ctf_parse_type_post_mod_array_create();
                            if($$ == NULL) { free($2); nomem();}
                            $$->array_len = $2;
                        }

type_post_mod_sequence  : '[' tag_reference ']'
                        {
                            $$ = ctf_parse_type_post_mod_sequence_create();
                            if($$ == NULL) { free($2); nomem();}
                            $$->sequence_len = $2;
                        }


%%

struct ctf_ast* ctf_meta_parse(const char* filename)
{
    struct ctf_ast* ast = ctf_ast_create();
    if(ast == NULL)
    {
        return NULL;
    }

	struct ctf_meta_parser_state state;

    int result = ctf_meta_parser_state_init(&state, ast, filename);
	if(result < 0)
	{
		ctf_ast_destroy(ast);
        return NULL;
	}
	
	result = yyparse(&state, state.scanner);
	
    if(result != 0)
	{
        ctf_meta_parser_state_destroy(&state);
        ctf_ast_destroy(ast);
		return NULL;
	}

    ctf_meta_parser_state_destroy(&state);
	
    return ast;
}

int ctf_meta_parser_state_init(struct ctf_meta_parser_state* state,
    struct ctf_ast* ast, const char* filename)
{
	state->f = fopen(filename, "r+");
	if(state->f == NULL)
	{
		fprintf(stderr, "Failed to open file '%s' for read CTF metadata.\n",
			filename);
		return -errno;
	}

    int result = ctf_meta_lexer_state_init(&state->scanner, state);
    if(result < 0)
    {
        fclose(state->f);
        return result;
    }

	state->line = 1;
	state->column = FIRST_POS;
	
	state->ast = ast;
	state->filename = filename;
	
	return 0;
}

void ctf_meta_parser_state_destroy(struct ctf_meta_parser_state* state)
{
    ctf_meta_lexer_state_destroy(state->scanner);
    fclose(state->f);
}

char* strappend_format(char* str, const char* append_format,...)
{
    char* result_str;
    va_list ap;

    va_start(ap, append_format);
    int append_len = vsnprintf(NULL, 0, append_format, ap);
    va_end(ap);
    
    int len = str ? strlen(str) : 0;
    result_str = realloc(str, len + append_len + 1);
    if(result_str == NULL)
    {
        printf("Failed to reallocate string for append.\n");
        return NULL;
    }
    
    va_start(ap, append_format);
    vsnprintf(result_str + len, append_len + 1, append_format, ap);
    va_end(ap);
    
    return result_str;
}