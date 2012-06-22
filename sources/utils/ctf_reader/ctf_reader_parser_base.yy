/* Generate C++ parser */
%skeleton "lalr1.cc"
%require "2.4"
%defines

%locations

%code requires{
#include "ctf_ast.h"
#include <memory>
class CTFReaderScanner;
}

%code {
#include "location.hh"
static int yylex(yy::parser::semantic_type* yylval,
    yy::location* yylloc, CTFReaderScanner* scanner);
    
/* Helper for wrap pointer into auto_ptr object of corresponded type.*/
template<class T> std::auto_ptr<T> ptr(T* t) {return std::auto_ptr<T>(t);}

/* NULL-string as auto-ptr */
std::auto_ptr<std::string> nullStr(void)
    {return std::auto_ptr<std::string>();}
/* NULL variant scope as auto-ptr */
std::auto_ptr<CTFASTScopeVariant> nullScopeVariant()
    {return std::auto_ptr<CTFASTScopeVariant>();}
/* NULL enum scope as auto-ptr */
std::auto_ptr<CTFASTScopeEnum> nullScopeEnum()
    {return std::auto_ptr<CTFASTScopeEnum>();}
/* NULL type spec as auto-ptr */
std::auto_ptr<CTFASTTypeSpec> nullTypeSpec()
    {return std::auto_ptr<CTFASTTypeSpec>();}
}
/* 
 * Reentrant LEX scanner accept additional parameter
 */
%lex-param  {CTFReaderScanner* scanner}
%parse-param {CTFReaderScanner* scanner}
/* Parser accept pointer to AST builded. */
%parse-param {CTFAST& ast}
%{

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
 *     because of insufficient memory).
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
    std::string* str;
}
%destructor { delete $$;} <str>

%token <str> ID
%token <str> STRING_LITERAL
%token <str> INTEGER_CONSTANT

%token  UNKNOWN

/************************ Grouping ************************************/

%union {
    CTFASTScopeTop* scopeTop;
    CTFASTScopeStruct* scopeStruct;
    CTFASTScopeVariant* scopeVariant;
    CTFASTScopeInt* scopeInt;
    CTFASTScopeEnum* scopeEnum;
}
%destructor {delete $$;} <scopeTop, scopeStruct, scopeVariant, scopeInt, scopeEnum>

%union {
    CTFASTStatement* statement;
    CTFASTTopScopeDecl* topScopeDecl;
    CTFASTStructDecl* structDecl;
    CTFASTVariantDecl* variantDecl;
    CTFASTEnumDecl* enumDecl;
    CTFASTTypedefDecl* typedefDecl;
    CTFASTFieldDecl* fieldDecl;
    CTFASTParameterDef* parameterDef;
    CTFASTTypeAssignment* typeAssignment;
}
%destructor {delete $$;} <statement, topScopeDecl, structDecl, variantDecl, enumDecl>
%destructor {delete $$;} <typedefDecl, fieldDecl, parameterDef, typeAssignment>

%union {
    CTFASTTypeSpec* typeSpec;
    CTFASTStructSpec* structSpec;
    CTFASTVariantSpec* variantSpec;
    CTFASTEnumSpec* enumSpec;
    CTFASTIntSpec* intSpec;
    CTFASTTypeIDSpec* typeIDSpec;
}
%destructor { delete $$;} <typeSpec, structSpec, variantSpec, enumSpec, intSpec, typeIDSpec>

%union {
    CTFASTEnumValueDecl* enumValueDecl;
    CTFASTEnumValueDeclSimple* enumValueDeclSimple;
    CTFASTEnumValueDeclPresize* enumValueDeclPresize;
    CTFASTEnumValueDeclRange* enumValueDeclRange;
}

%destructor { delete $$;} <enumValueDecl, enumValueDeclSimple>
%destructor { delete $$;} <enumValueDeclPresize, enumValueDeclRange>

%union {
    CTFASTTypePostMod* typePostMod;
    CTFASTArrayMod* arrayMod;
    CTFASTSequenceMod* sequenceMod;
}
%destructor {delete $$;} <typePostMod, arrayMod, sequenceMod>

%union {
    CTFASTTypePostMods* typePostMods;
}
%destructor {delete $$;} <typePostMods>

%union {
    CTFASTFieldDecl::TypeInst* typeInstField;
    CTFASTTypedefDecl::TypeInst* typeInstTypedef;
}
%destructor { delete $$;} <typeInstField, typeInstTypedef>

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

%type <scopeTop> top_scope
%type <scopeStruct> struct_scope
%type <scopeInt> int_scope
/* enum_scope_ - enumeration scope which ends on value(not on comma)*/
%type <scopeEnum> enum_scope enum_scope_ enum_scope_empty
%type <scopeVariant> variant_scope

%type <statement> meta_s top_scope_s struct_scope_s variant_scope_s type_decl int_scope_s
%type <topScopeDecl> top_scope_decl
%type <structDecl> struct_decl
%type <variantDecl> variant_decl
%type <enumDecl> enum_decl
/* typedef_decl_ - typedef declaration without terminating ';'*/
%type <typedefDecl> typedef_decl typedef_decl_
/* field_decl_ - field declaration without terminating ';'*/
%type <fieldDecl> field_decl field_decl_
%type <parameterDef> param_def
%type <typeAssignment> type_assign

%type <enumValueDecl> enum_value
%type <enumValueDeclSimple> enum_value_simple
%type <enumValueDeclPresize> enum_value_presize
%type <enumValueDeclRange> enum_value_range

%type <typeSpec> type_spec
/* Specificator for struct, either existing or just created. */
%type <structSpec> struct_spec
/* Specificator for struct, which is just created. */
/* %type <struct_spec> struct_spec_c */
%type <variantSpec> variant_spec
%type <enumSpec> enum_spec
/* Specificator for type which use only type identificator */
%type <typeIDSpec> type_spec_id
%type <intSpec> int_spec
/* Specificator for type which may designate integer type */
%type <typeSpec> type_spec_int

%type <typePostMod> type_post_mod
%type <arrayMod> type_post_mod_array
%type <sequenceMod> type_post_mod_sequence

%type <typePostMods> type_post_mods

%type <typeInstField> type_inst_field
%type <typeInstTypedef> type_inst_typedef

%%
meta                : /* empty */
                    | meta meta_s
                        {ast.rootScope->addStatement(ptr($2));}

meta_s              : type_decl
                    | top_scope_decl
                        { $$ = $1; }
                    
top_scope_decl:     top_scope_name '{' top_scope '}' ';'
                        {$$ = new CTFASTTopScopeDecl(ptr($1), ptr($3));}

type_decl           : struct_decl
                        { $$ = $1;}
                    | variant_decl
                        { $$ = $1;}
                    | enum_decl
                        { $$ = $1;}
                    | typedef_decl
                        { $$ = $1;}

struct_decl         : struct_spec ';'
                        {$$ = new CTFASTStructDecl(ptr($1));}

struct_spec         : STRUCT ID '{' struct_scope '}'
                        {$$ = new CTFASTStructSpec(ptr($2), ptr($4));}
                    | STRUCT '{' struct_scope '}'
                        {$$ = new CTFASTStructSpec(ptr($3));}
                    | STRUCT ID
                        {$$ = new CTFASTStructSpec(ptr($2));}

variant_decl         : variant_spec ';'
                        {$$ = new CTFASTVariantDecl(ptr($1));}

variant_spec         : VARIANT ID '{' variant_scope '}'
                        {
                            $$ = new CTFASTVariantSpec(ptr($2),
                                nullStr(), ptr($4));
                        }
                    | VARIANT ID '<' tag_reference '>' '{' variant_scope '}' 
                        {
                            $$ = new CTFASTVariantSpec(ptr($2),
                                ptr($4), ptr($7));
                        }
                    | VARIANT '{' variant_scope '}'
                        {
                            $$ = new CTFASTVariantSpec(nullStr(),
                                nullStr(), ptr($3));
                        }
                    | VARIANT '<' tag_reference '>' '{' variant_scope '}' 
                        {
                            $$ = new CTFASTVariantSpec(nullStr(),
                                ptr($3), ptr($6));
                        }
                    | VARIANT ID
                        {
                            $$ = new CTFASTVariantSpec(ptr($2),
                                nullStr(), nullScopeVariant());
                        }
                    | VARIANT ID '<' tag_reference '>'
                        {
                            $$ = new CTFASTVariantSpec(ptr($2),
                                ptr($4), nullScopeVariant());
                        }

enum_decl           : enum_spec ';'
                        { $$ = new CTFASTEnumDecl(ptr($1)); }

enum_spec           : ENUM ID ':' type_spec_int '{' enum_scope '}'
                        {
                            $$ = new CTFASTEnumSpec(ptr($2), ptr($6),
                                ptr($4));
                        }
                    | ENUM ':' type_spec_int '{' enum_scope '}'
                        {
                            $$ = new CTFASTEnumSpec(nullStr(), ptr($5),
                                ptr($3));
                        }
                    | ENUM ID
                        {
                            $$ = new CTFASTEnumSpec(ptr($2), nullScopeEnum(),
                                nullTypeSpec());
                        }

type_spec_int       : int_spec {$$ = $1;}
                    | type_spec_id {$$ = $1;}

enum_scope          : enum_scope_empty
                    | enum_scope_
                    | enum_scope_ ','

enum_scope_empty    :   /* empty */
                        {$$ = new CTFASTScopeEnum();}

enum_scope_         : enum_scope_empty enum_value
                        {$$ = $1; $$->addValueDecl(ptr($2));}
                    | enum_scope_ ',' enum_value
                        {$$ = $1; $$->addValueDecl(ptr($3));}
                        
enum_value          : enum_value_simple
                        {$$ = $1;}
                    | enum_value_presize
                        {$$ = $1;}
                    | enum_value_range
                        {$$ = $1;}

enum_value_simple   : ID
                        {$$ = new CTFASTEnumValueDeclSimple(ptr($1)); }

enum_value_presize  : ID '=' INTEGER_CONSTANT
                        {$$ = new CTFASTEnumValueDeclPresize(ptr($1), ptr($3));}
enum_value_range    : ID '=' INTEGER_CONSTANT DOTDOTDOT INTEGER_CONSTANT
                        {$$ = new CTFASTEnumValueDeclRange(ptr($1), ptr($3), ptr($5));}

top_scope_name      : TRACE {$$ = new std::string("trace");}
                    | STREAM {$$ = new std::string("stream");}
                    | EVENT {$$ = new std::string("event");}

top_scope           : /* empty */
                        { $$ = new CTFASTScopeTop();}
                    | top_scope top_scope_s
                        { $$ = $1; $$->addStatement(ptr($2)); }

top_scope_s         : type_decl
                    | param_def {$$ = $1;}
                    | type_assign {$$ = $1;}

struct_scope        : /* empty */
                        { $$ = new CTFASTScopeStruct();}
                    | struct_scope struct_scope_s
                        { $$ = $1; $$->addStatement(ptr($2)); }

struct_scope_s      : type_decl
                    | field_decl {$$ = $1;}

variant_scope        : /* empty */
                        { $$ = new CTFASTScopeVariant();}
                    | variant_scope variant_scope_s
                        { $$ = $1; $$->addStatement(ptr($2)); }

variant_scope_s      : type_decl
                    | field_decl {$$ = $1;}

field_decl          : field_decl_ ';'

field_decl_            : type_spec type_inst_field
                        {$$ = new CTFASTFieldDecl(ptr($1), ptr($2));}
                    | field_decl_ ',' type_inst_field
                        {$$ = $1; $$->addTypeInst(ptr($3));}

type_spec           : struct_spec {$$ = $1;}
                    | variant_spec {$$ = $1;}
                    | type_spec_id {$$ = $1;}
                    | int_spec  {$$ = $1;}
                    | enum_spec  {$$ = $1;}

type_spec_id        : ID { $$ = new CTFASTTypeIDSpec(ptr($1));}

int_spec            : INTEGER '{' int_scope '}'
                        { $$ = new CTFASTIntSpec(ptr($3));}

int_scope           : /* empty */
                        {$$ = new CTFASTScopeInt();}
                    | int_scope int_scope_s
                        {$$ = $1; $$->addStatement(ptr($2)); }

int_scope_s         : param_def
                        {$$ = $1;}

param_def           : ID '=' param_value ';'
                        {$$ = new CTFASTParameterDef(ptr($1), ptr($3));}
param_value         : ID
                    | STRING_LITERAL
                    | INTEGER_CONSTANT

type_assign         : tag_reference TYPE_ASSIGNMENT_OPERATOR type_spec type_post_mods ';'
                        {$$ = new CTFASTTypeAssignment(ptr($1), ptr($3), ptr($4));}


tag_reference       : tag_component
                    | tag_reference '.' tag_component
                        {$$ = $1; $$->append("." + *$3); delete $3;}
                    | tag_reference ARROW tag_component
                        {$$ = $1; $$->append("." + *$3); delete $3;}
                    | tag_reference '[' INTEGER_CONSTANT ']'
                        {$$ = $1; $$->append("[" + *$3 + "]"); delete $3;}

tag_component       : ID
                    | TRACE {$$ = new std::string("trace");}
                    | STREAM {$$ = new std::string("stream");}
                    | EVENT {$$ = new std::string("event");}

typedef_decl        : typedef_decl_ ';'

typedef_decl_       : TYPEDEF type_spec type_inst_typedef 
                        {$$ = new CTFASTTypedefDecl(ptr($2), ptr($3));}
                    | typedef_decl_ ',' type_inst_typedef
                        {$$ = $1; $$->addTypeInst(ptr($3));}
                    

type_inst_field     : ID type_post_mods
                        {$$ = new CTFASTFieldDecl::TypeInst(ptr($1), ptr($2));}

type_inst_typedef   : ID type_post_mods
                        {$$ = new CTFASTTypedefDecl::TypeInst(ptr($1), ptr($2));}


type_post_mods      : /* empty, TODO: may be NULL */
                        {$$ = new CTFASTTypePostMods();}
                    | type_post_mods type_post_mod
                        {$$ = $1; $$->addTypePostMod(ptr($2));}

type_post_mod       : type_post_mod_array {$$ = $1;}
                    | type_post_mod_sequence {$$ = $1;}

type_post_mod_array : '[' INTEGER_CONSTANT ']'
                        { $$ = new CTFASTArrayMod(ptr($2));}

type_post_mod_sequence  : '[' tag_reference ']'
                        { $$ = new CTFASTSequenceMod(ptr($2));}


%%

/* Error method of the parser */
#include <stdexcept>
void yy::parser::error(const yy::location& yyloc,
    const std::string& what)
{
    std::cerr << yyloc << ": " << what << std::endl;
    throw std::runtime_error("Metadata parsing failed");
}

/* Implementation of scanner routine*/
#include "ctf_reader_scanner.h"
int yylex(yy::parser::semantic_type* yylval,
    yy::location* yylloc, CTFReaderScanner* scanner)
{
    return scanner->yylex(yylval, yylloc);
}


/* Implementation of CTFReaderParser methods */
#include "ctf_reader_parser.h"

CTFReaderParser::CTFReaderParser(std::istream& stream, CTFAST& ast)
    : scanner(stream), parserBase(&scanner, ast)
{
}

void CTFReaderParser::parse(void)
{
    parserBase.parse();
}
