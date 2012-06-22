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
#include "ctf_reader_scanner.h"
#include "ctf_reader_parser_base.tab.hh"
}
/* Extra data is really pointer to the parser state */
%option extra-type="CTFReaderScanner::ExtraData*"

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

#include <iostream> /* printf */
#include <string> /* realloc for append string */
#include <stdexcept>
#include <cassert> /* assertions */

typedef yy::parser::token token;

/* Add parameters to yylex() call. */
#undef YY_DECL
#define YY_DECL int yylex(yyscan_t yyscanner, yy::parser::semantic_type* yylval, \
yy::location* yylloc)

/* Redefine YY_INPUT for use 'std::istream' instead of 'FILE*'. */
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) {   \
    std::istream& s = yyextra->s;           \
    if(s.eof() || s.fail()) result = 0;   \
    else if(s.read(buf, max_size).bad())   \
        throw std::runtime_error("Error occures while read file with CTF metadata."); \
    else result = s.gcount(); }

/*
 * Use macro for update token location each time when rule is matched.
 * 
 * These actions are correct for any non-skipping match.
 * 
 * Action which skip characters should additionally perform
 * yylloc->step();
 *
 * NOTE: This macro relies on the fact that all patterns including
 * newline characters doesn't include non-newline characters.
 */
#define YY_USER_ACTION \
    if(*yytext != '\n') yylloc->columns (yyleng);\
    else yylloc->lines(yyleng);
%}

id              [a-zA-Z_][a-zA-Z_0-9]*

constant_dec    [1-9][0-9]*
constant_hex    0[xX][:xdigit:]+
constant_oct    0[1-7][0-7]*
constant_int    0|{constant_dec}|{constant_hex}|{constant_oct}

space           [ \t]
newline         \n

/* 
 * Should corresponds to all keywords.
 *
 * May be used for reject invalid identificators.
 */
keyword        trace|stream|event|struct|integer|variant|enum|typedef

/* Inside comments */
%x COMMENTS

/*
 * Currently inside quotes. Outer state is stored at the top of the stack.
 */
%x STRING

%%
    /* Make step() on location each time when new token is requested. */
%{
    yylloc->step();
%}


<COMMENTS>{
"*/"           yy_pop_state(yyscanner); yylloc->step();
"*"            yylloc->step();
"\n"           yylloc->step();
[^*\n]+        yylloc->step();
<<EOF>>        throw std::logic_error("Unexpected EOF while parse comments.");
}

<STRING>{
"\""            {
    yy_pop_state(yyscanner);
    switch(YY_START)
    {
    case INITIAL:
        return token::STRING_LITERAL;
    break;
    default:
        throw std::logic_error("Unexpected state while parsing string is finished.");
    }
    }

"\\\""  yylval->str->push_back('\"');
"\\\\"  yylval->str->push_back('\\');
"\\n"   yylval->str->push_back('\n');
"\\"    {
            std::cerr << "Unrecognized escape sequence." << std::endl;
            yylval->str->push_back('\\');
        }

"\n"        |
[^\"\\\n]+  yylval->str->append(yytext, yyleng);

<<EOF>>     throw std::logic_error("Unexpected EOF while parse string in qoutes.");
}

    /* 
     * In almost all states space and newlines are ignored, same for
     * comments.
     * 
     * Cases, for which this is not true should be processed above.
     */
{space}+        yylloc->step();
{newline}+      yylloc->step();
"/*"            yy_push_state(COMMENTS, yyscanner); yylloc->step();

struct          return token::STRUCT;
integer         return token::INTEGER;
enum            return token::ENUM;
variant         return token::VARIANT;

trace           return token::TRACE;
stream          return token::STREAM;
event           return token::EVENT;
typedef         return token::TYPEDEF;

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

":="            return token::TYPE_ASSIGNMENT_OPERATOR;
"->"            return token::ARROW;
"..."           return token::DOTDOTDOT;

"\""            {
                    yylval->str = new std::string();
                    yy_push_state(STRING, yyscanner);
                }

{id}            {
                    yylval->str = new std::string(yytext, yyleng);
                    return token::ID;
                }

{constant_int}  {
                    yylval->str = new std::string(yytext, yyleng);
                    return token::INTEGER_CONSTANT;
                }

.               {
                    yylval->str = new std::string(1, *yytext);
                    return token::UNKNOWN;
                }
<<EOF>>         return 0;
%%

CTFReaderScanner::CTFReaderScanner(std::istream& s) : extraData(s)
{
    if(yylex_init_extra(&extraData, (yyscan_t*)&_scanner) != 0)
        throw std::runtime_error("Failed to initialize scanner");
}

CTFReaderScanner::~CTFReaderScanner(void)
{
    yylex_destroy((yyscan_t)_scanner);
}

int CTFReaderScanner::yylex(yy::parser::semantic_type* yylval,
    yy::location* yylloc)
{
    return ::yylex((yyscan_t)_scanner, yylval, yylloc);
}
