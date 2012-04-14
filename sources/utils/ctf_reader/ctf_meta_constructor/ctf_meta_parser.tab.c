
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 1



/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

#include "ctf_meta_parse.h"

/* Line 189 of yacc.c  */
#line 12 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

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


/* Line 189 of yacc.c  */
#line 151 "ctf_meta_parser.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ENUM = 258,
     STRUCT = 259,
     INTEGER = 260,
     VARIANT = 261,
     TYPEDEF = 262,
     TRACE = 263,
     STREAM = 264,
     EVENT = 265,
     TYPE_ASSIGNMENT_OPERATOR = 266,
     ARROW = 267,
     DOTDOTDOT = 268,
     ID = 269,
     STRING_LITERAL = 270,
     INTEGER_CONSTANT = 271,
     UNKNOWN = 272
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 100 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    char* str;


/* Line 214 of yacc.c  */
#line 113 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_scope_top* scope_top;
    struct ctf_parse_scope_struct* scope_struct;
    struct ctf_parse_scope_variant* scope_variant;
    struct ctf_parse_scope_int* scope_int;
    struct ctf_parse_scope_enum* scope_enum;


/* Line 214 of yacc.c  */
#line 126 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_statement* statement;
    struct ctf_parse_scope_top_decl* scope_top_decl;
    struct ctf_parse_struct_decl* struct_decl;
    struct ctf_parse_variant_decl* variant_decl;
    struct ctf_parse_enum_decl* enum_decl;
    struct ctf_parse_typedef_decl* typedef_decl;
    struct ctf_parse_field_decl* field_decl;
    struct ctf_parse_param_def* param_def;
    struct ctf_parse_type_assignment* type_assignment;


/* Line 214 of yacc.c  */
#line 147 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_spec* type_spec;
    struct ctf_parse_struct_spec* struct_spec;
    struct ctf_parse_variant_spec* variant_spec;
    struct ctf_parse_enum_spec* enum_spec;
    struct ctf_parse_type_spec_id* type_spec_id;
    struct ctf_parse_int_spec* int_spec;


/* Line 214 of yacc.c  */
#line 162 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_enum_value* enum_value;
    struct ctf_parse_enum_value_simple* enum_value_simple;
    struct ctf_parse_enum_value_presize* enum_value_presize;
    struct ctf_parse_enum_value_range* enum_value_range;


/* Line 214 of yacc.c  */
#line 174 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_post_mod* type_post_mod;
    struct ctf_parse_type_post_mod_array* type_post_mod_array;
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence;


/* Line 214 of yacc.c  */
#line 183 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_post_mod_list* type_post_mod_list;



/* Line 214 of yacc.c  */
#line 268 "ctf_meta_parser.tab.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 293 "ctf_meta_parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   175

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  29
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  39
/* YYNRULES -- Number of rules.  */
#define YYNRULES  79
/* YYNRULES -- Number of states.  */
#define YYNSTATES  139

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   272

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    24,     2,    26,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    23,    20,
      21,    25,    22,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    27,     2,    28,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    18,     2,    19,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    17,    19,    21,
      23,    25,    28,    34,    39,    42,    45,    51,    60,    65,
      73,    76,    82,    85,    93,   100,   103,   105,   109,   111,
     113,   115,   117,   121,   127,   129,   131,   133,   134,   137,
     139,   141,   142,   145,   147,   149,   150,   153,   155,   157,
     162,   164,   166,   168,   170,   172,   174,   179,   180,   183,
     185,   190,   192,   194,   196,   201,   203,   207,   211,   216,
     218,   220,   222,   224,   230,   231,   234,   236,   238,   242
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      30,     0,    -1,    -1,    30,    31,    -1,    33,    -1,    32,
      -1,    45,    18,    46,    19,    20,    -1,    34,    -1,    36,
      -1,    38,    -1,    63,    -1,    35,    20,    -1,     4,    14,
      18,    48,    19,    -1,     4,    18,    48,    19,    -1,     4,
      14,    -1,    37,    20,    -1,     6,    14,    18,    50,    19,
      -1,     6,    14,    21,    61,    22,    18,    50,    19,    -1,
       6,    18,    50,    19,    -1,     6,    21,    61,    22,    18,
      50,    19,    -1,     6,    14,    -1,     6,    14,    21,    61,
      22,    -1,    39,    20,    -1,     3,    14,    18,    40,    19,
      23,    53,    -1,     3,    18,    40,    19,    23,    53,    -1,
       3,    14,    -1,    41,    -1,    40,    24,    41,    -1,    42,
      -1,    43,    -1,    44,    -1,    14,    -1,    14,    25,    16,
      -1,    14,    25,    16,    13,    16,    -1,     8,    -1,     9,
      -1,    10,    -1,    -1,    46,    47,    -1,    33,    -1,    60,
      -1,    -1,    48,    49,    -1,    33,    -1,    52,    -1,    -1,
      50,    51,    -1,    33,    -1,    52,    -1,    53,    14,    64,
      20,    -1,    35,    -1,    37,    -1,    54,    -1,    55,    -1,
      39,    -1,    14,    -1,     5,    18,    56,    19,    -1,    -1,
      56,    57,    -1,    58,    -1,    14,    25,    59,    20,    -1,
      14,    -1,    15,    -1,    16,    -1,    61,    11,    53,    20,
      -1,    62,    -1,    61,    26,    62,    -1,    61,    12,    62,
      -1,    61,    27,    16,    28,    -1,    14,    -1,     8,    -1,
       9,    -1,    10,    -1,     7,    53,    14,    64,    20,    -1,
      -1,    64,    65,    -1,    66,    -1,    67,    -1,    27,    16,
      28,    -1,    27,    61,    28,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   243,   243,   244,   247,   248,   251,   258,   260,   262,
     264,   267,   273,   280,   287,   294,   300,   307,   315,   322,
     330,   336,   345,   351,   359,   366,   373,   379,   385,   387,
     389,   392,   399,   406,   416,   418,   420,   424,   425,   428,
     429,   433,   434,   437,   438,   441,   442,   445,   446,   449,
     457,   458,   459,   460,   461,   463,   470,   478,   479,   482,
     485,   492,   493,   494,   496,   506,   507,   509,   511,   514,
     515,   517,   519,   522,   532,   536,   542,   544,   547,   554
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ENUM", "STRUCT", "INTEGER", "VARIANT",
  "TYPEDEF", "TRACE", "STREAM", "EVENT", "TYPE_ASSIGNMENT_OPERATOR",
  "ARROW", "DOTDOTDOT", "ID", "STRING_LITERAL", "INTEGER_CONSTANT",
  "UNKNOWN", "'{'", "'}'", "';'", "'<'", "'>'", "':'", "','", "'='", "'.'",
  "'['", "']'", "$accept", "meta", "meta_s", "top_scope_decl", "type_decl",
  "struct_decl", "struct_spec", "variant_decl", "variant_spec",
  "enum_decl", "enum_spec", "enum_scope", "enum_value",
  "enum_value_simple", "enum_value_presize", "enum_value_range",
  "top_scope_name", "top_scope", "top_scope_s", "struct_scope",
  "struct_scope_s", "variant_scope", "variant_scope_s", "field_decl",
  "type_spec", "type_spec_id", "int_spec", "int_scope", "int_scope_s",
  "param_def", "param_value", "type_assign", "tag_reference",
  "tag_component", "typedef_decl", "type_post_mod_list", "type_post_mod",
  "type_post_mod_array", "type_post_mod_sequence", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   123,   125,
      59,    60,    62,    58,    44,    61,    46,    91,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    29,    30,    30,    31,    31,    32,    33,    33,    33,
      33,    34,    35,    35,    35,    36,    37,    37,    37,    37,
      37,    37,    38,    39,    39,    39,    40,    40,    41,    41,
      41,    42,    43,    44,    45,    45,    45,    46,    46,    47,
      47,    48,    48,    49,    49,    50,    50,    51,    51,    52,
      53,    53,    53,    53,    53,    54,    55,    56,    56,    57,
      58,    59,    59,    59,    60,    61,    61,    61,    61,    62,
      62,    62,    62,    63,    64,    64,    65,    65,    66,    67
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     5,     1,     1,     1,
       1,     2,     5,     4,     2,     2,     5,     8,     4,     7,
       2,     5,     2,     7,     6,     2,     1,     3,     1,     1,
       1,     1,     3,     5,     1,     1,     1,     0,     2,     1,
       1,     0,     2,     1,     1,     0,     2,     1,     1,     4,
       1,     1,     1,     1,     1,     1,     4,     0,     2,     1,
       4,     1,     1,     1,     4,     1,     3,     3,     4,     1,
       1,     1,     1,     5,     0,     2,     1,     1,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     1,     0,     0,     0,     0,    34,    35,    36,
       3,     5,     4,     7,     0,     8,     0,     9,     0,     0,
      10,    25,     0,    14,    41,    20,    45,     0,     0,    55,
      50,    51,    54,     0,    52,    53,    11,    15,    22,    37,
       0,    31,     0,    26,    28,    29,    30,    41,     0,    45,
       0,     0,    70,    71,    72,    69,     0,    65,    57,    74,
       0,     0,     0,     0,     0,     0,    13,    43,    50,    51,
      54,    42,    44,     0,     0,     0,    18,    47,    46,    48,
       0,     0,     0,     0,     0,     0,     0,    39,    38,    40,
       0,     0,    32,     0,    27,    12,    74,    16,    21,    67,
      45,    66,     0,     0,    56,    58,    59,    73,     0,    75,
      76,    77,     6,     0,     0,     0,    24,     0,    45,     0,
      68,     0,     0,     0,     0,    23,    33,    49,     0,    19,
      61,    62,    63,     0,    78,    79,    64,    17,    60
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,    10,    11,    77,    13,    68,    15,    69,    17,
      70,    42,    43,    44,    45,    46,    19,    60,    88,    48,
      71,    51,    78,    79,    73,    34,    35,    84,   105,   106,
     133,    89,    56,    57,    20,    85,   109,   110,   111
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -47
static const yytype_int16 yypact[] =
{
     -47,   155,   -47,    93,   132,    85,   133,   -47,   -47,   -47,
     -47,   -47,   -47,   -47,    -9,   -47,    -8,   -47,    12,    15,
     -47,    25,    28,    37,   -47,    50,   -47,     7,    51,   -47,
     -47,   -47,   -47,    64,   -47,   -47,   -47,   -47,   -47,   -47,
      28,    60,    67,   -47,   -47,   -47,   -47,   -47,    33,   -47,
       7,    70,   -47,   -47,   -47,   -47,    19,   -47,   -47,   -47,
      16,    74,    88,    87,    28,    76,   -47,   -47,    -9,    -8,
      12,   -47,   -47,   109,   115,    75,   -47,   -47,   -47,   -47,
       7,   130,     7,   117,    86,    24,   134,   -47,   -47,   -47,
       2,   137,   140,   133,   -47,   -47,   -47,   -47,   148,   -47,
     -47,   -47,   128,   142,   -47,   -47,   -47,   -47,    48,   -47,
     -47,   -47,   -47,   133,   133,   152,   -47,    40,   -47,   121,
     -47,   116,   141,    22,   150,   -47,   -47,   -47,   138,   -47,
     -47,   -47,   -47,   151,   -47,   -47,   -47,   -47,   -47
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -47,   -47,   -47,   -47,     5,   -47,    -1,   -47,     1,   -47,
       3,   135,   108,   -47,   -47,   -47,   -47,   -47,   -47,   126,
     -47,   -46,   -47,   -38,    -5,   -47,   -47,   -47,   -47,   -47,
     -47,   -47,   -42,    69,   -47,    78,   -47,   -47,   -47
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      14,    33,    16,    74,    18,    30,    12,    31,    75,    32,
      72,    36,    37,   113,    80,    52,    53,    54,    90,     3,
       4,    55,     5,     6,    52,    53,    54,    72,    82,    83,
      55,    80,    38,    39,    80,    86,     3,     4,    28,     5,
       6,    81,    41,    40,   107,    82,    83,    29,    82,    83,
     135,   108,    66,    67,   119,    47,    52,    53,    54,    14,
     127,    16,    55,    18,   122,    87,   123,   108,    49,    58,
      67,    50,   128,     3,     4,    28,     5,     6,    59,     3,
       4,    28,     5,     6,    29,    62,    63,    80,   116,    76,
      29,    64,    30,    91,    31,    95,    32,    98,    64,    25,
     103,    82,    83,    26,    92,   104,    27,    21,   124,   125,
      93,    22,    30,    30,    31,    31,    32,    32,     3,     4,
      28,     5,     6,    96,     3,     4,    28,     5,     6,    29,
     130,   131,   132,   102,    97,    29,     3,     4,    28,     5,
     129,     3,     4,    28,     5,     6,    23,    29,   100,    99,
      24,   101,    29,   115,   112,     2,   120,   137,     3,     4,
     114,     5,     6,     7,     8,     9,   118,   121,   126,   134,
     136,   138,    94,    65,   117,    61
};

static const yytype_uint8 yycheck[] =
{
       1,     6,     1,    49,     1,     6,     1,     6,    50,     6,
      48,    20,    20,    11,    12,     8,     9,    10,    60,     3,
       4,    14,     6,     7,     8,     9,    10,    65,    26,    27,
      14,    12,    20,    18,    12,    19,     3,     4,     5,     6,
       7,    22,    14,    18,    20,    26,    27,    14,    26,    27,
      28,    27,    19,    48,   100,    18,     8,     9,    10,    60,
      20,    60,    14,    60,    16,    60,   108,    27,    18,    18,
      65,    21,   118,     3,     4,     5,     6,     7,    14,     3,
       4,     5,     6,     7,    14,    25,    19,    12,    93,    19,
      14,    24,    93,    19,    93,    19,    93,    22,    24,    14,
      14,    26,    27,    18,    16,    19,    21,    14,   113,   114,
      23,    18,   113,   114,   113,   114,   113,   114,     3,     4,
       5,     6,     7,    14,     3,     4,     5,     6,     7,    14,
      14,    15,    16,    16,    19,    14,     3,     4,     5,     6,
      19,     3,     4,     5,     6,     7,    14,    14,    18,    80,
      18,    82,    14,    13,    20,     0,    28,    19,     3,     4,
      23,     6,     7,     8,     9,    10,    18,    25,    16,    28,
      20,    20,    64,    47,    96,    40
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    30,     0,     3,     4,     6,     7,     8,     9,    10,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    45,
      63,    14,    18,    14,    18,    14,    18,    21,     5,    14,
      35,    37,    39,    53,    54,    55,    20,    20,    20,    18,
      18,    14,    40,    41,    42,    43,    44,    18,    48,    18,
      21,    50,     8,     9,    10,    14,    61,    62,    18,    14,
      46,    40,    25,    19,    24,    48,    19,    33,    35,    37,
      39,    49,    52,    53,    50,    61,    19,    33,    51,    52,
      12,    22,    26,    27,    56,    64,    19,    33,    47,    60,
      61,    19,    16,    23,    41,    19,    14,    19,    22,    62,
      18,    62,    16,    14,    19,    57,    58,    20,    27,    65,
      66,    67,    20,    11,    23,    13,    53,    64,    18,    50,
      28,    25,    16,    61,    53,    53,    16,    20,    50,    19,
      14,    15,    16,    59,    28,    28,    20,    19,    20
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (state, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex (scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location, state, scanner); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct ctf_meta_parser_state* state, yyscan_t scanner)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct ctf_meta_parser_state* state;
    yyscan_t scanner;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (state);
  YYUSE (scanner);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct ctf_meta_parser_state* state, yyscan_t scanner)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, state, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct ctf_meta_parser_state* state;
    yyscan_t scanner;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, struct ctf_meta_parser_state* state, yyscan_t scanner)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, state, scanner)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    struct ctf_meta_parser_state* state;
    yyscan_t scanner;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , state, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, state, scanner); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct ctf_meta_parser_state* state, yyscan_t scanner)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, state, scanner)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    struct ctf_meta_parser_state* state;
    yyscan_t scanner;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (state);
  YYUSE (scanner);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {
      case 14: /* "ID" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1343 "ctf_meta_parser.tab.c"
	break;
      case 15: /* "STRING_LITERAL" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1352 "ctf_meta_parser.tab.c"
	break;
      case 16: /* "INTEGER_CONSTANT" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1361 "ctf_meta_parser.tab.c"
	break;
      case 31: /* "meta_s" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1370 "ctf_meta_parser.tab.c"
	break;
      case 32: /* "top_scope_decl" */

/* Line 1000 of yacc.c  */
#line 138 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->scope_top_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1379 "ctf_meta_parser.tab.c"
	break;
      case 33: /* "type_decl" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1388 "ctf_meta_parser.tab.c"
	break;
      case 34: /* "struct_decl" */

/* Line 1000 of yacc.c  */
#line 139 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->struct_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1397 "ctf_meta_parser.tab.c"
	break;
      case 35: /* "struct_spec" */

/* Line 1000 of yacc.c  */
#line 156 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy(&((yyvaluep->struct_spec)->base));};

/* Line 1000 of yacc.c  */
#line 1406 "ctf_meta_parser.tab.c"
	break;
      case 36: /* "variant_decl" */

/* Line 1000 of yacc.c  */
#line 140 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->variant_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1415 "ctf_meta_parser.tab.c"
	break;
      case 37: /* "variant_spec" */

/* Line 1000 of yacc.c  */
#line 157 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy(&((yyvaluep->variant_spec)->base));};

/* Line 1000 of yacc.c  */
#line 1424 "ctf_meta_parser.tab.c"
	break;
      case 38: /* "enum_decl" */

/* Line 1000 of yacc.c  */
#line 141 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->enum_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1433 "ctf_meta_parser.tab.c"
	break;
      case 39: /* "enum_spec" */

/* Line 1000 of yacc.c  */
#line 158 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy(&((yyvaluep->enum_spec)->base));};

/* Line 1000 of yacc.c  */
#line 1442 "ctf_meta_parser.tab.c"
	break;
      case 40: /* "enum_scope" */

/* Line 1000 of yacc.c  */
#line 124 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ctf_parse_scope_destroy(&((yyvaluep->scope_enum)->base));};

/* Line 1000 of yacc.c  */
#line 1451 "ctf_meta_parser.tab.c"
	break;
      case 41: /* "enum_value" */

/* Line 1000 of yacc.c  */
#line 169 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_enum_value_destroy((yyvaluep->enum_value));};

/* Line 1000 of yacc.c  */
#line 1460 "ctf_meta_parser.tab.c"
	break;
      case 42: /* "enum_value_simple" */

/* Line 1000 of yacc.c  */
#line 170 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_enum_value_destroy(&(yyvaluep->enum_value_simple)->base);};

/* Line 1000 of yacc.c  */
#line 1469 "ctf_meta_parser.tab.c"
	break;
      case 43: /* "enum_value_presize" */

/* Line 1000 of yacc.c  */
#line 171 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_enum_value_destroy(&(yyvaluep->enum_value_presize)->base);};

/* Line 1000 of yacc.c  */
#line 1478 "ctf_meta_parser.tab.c"
	break;
      case 44: /* "enum_value_range" */

/* Line 1000 of yacc.c  */
#line 172 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_enum_value_destroy(&(yyvaluep->enum_value_range)->base);};

/* Line 1000 of yacc.c  */
#line 1487 "ctf_meta_parser.tab.c"
	break;
      case 45: /* "top_scope_name" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1496 "ctf_meta_parser.tab.c"
	break;
      case 46: /* "top_scope" */

/* Line 1000 of yacc.c  */
#line 120 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ctf_parse_scope_destroy(&((yyvaluep->scope_top)->base));};

/* Line 1000 of yacc.c  */
#line 1505 "ctf_meta_parser.tab.c"
	break;
      case 47: /* "top_scope_s" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1514 "ctf_meta_parser.tab.c"
	break;
      case 48: /* "struct_scope" */

/* Line 1000 of yacc.c  */
#line 121 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ctf_parse_scope_destroy(&((yyvaluep->scope_struct)->base));};

/* Line 1000 of yacc.c  */
#line 1523 "ctf_meta_parser.tab.c"
	break;
      case 49: /* "struct_scope_s" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1532 "ctf_meta_parser.tab.c"
	break;
      case 50: /* "variant_scope" */

/* Line 1000 of yacc.c  */
#line 122 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ctf_parse_scope_destroy(&((yyvaluep->scope_variant)->base));};

/* Line 1000 of yacc.c  */
#line 1541 "ctf_meta_parser.tab.c"
	break;
      case 51: /* "variant_scope_s" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1550 "ctf_meta_parser.tab.c"
	break;
      case 52: /* "field_decl" */

/* Line 1000 of yacc.c  */
#line 143 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->field_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1559 "ctf_meta_parser.tab.c"
	break;
      case 53: /* "type_spec" */

/* Line 1000 of yacc.c  */
#line 155 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy((yyvaluep->type_spec));};

/* Line 1000 of yacc.c  */
#line 1568 "ctf_meta_parser.tab.c"
	break;
      case 54: /* "type_spec_id" */

/* Line 1000 of yacc.c  */
#line 159 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy(&((yyvaluep->type_spec_id)->base));};

/* Line 1000 of yacc.c  */
#line 1577 "ctf_meta_parser.tab.c"
	break;
      case 55: /* "int_spec" */

/* Line 1000 of yacc.c  */
#line 160 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_spec_destroy(&((yyvaluep->int_spec)->base));};

/* Line 1000 of yacc.c  */
#line 1586 "ctf_meta_parser.tab.c"
	break;
      case 56: /* "int_scope" */

/* Line 1000 of yacc.c  */
#line 123 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ctf_parse_scope_destroy(&((yyvaluep->scope_int)->base));};

/* Line 1000 of yacc.c  */
#line 1595 "ctf_meta_parser.tab.c"
	break;
      case 57: /* "int_scope_s" */

/* Line 1000 of yacc.c  */
#line 137 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy((yyvaluep->statement)); };

/* Line 1000 of yacc.c  */
#line 1604 "ctf_meta_parser.tab.c"
	break;
      case 58: /* "param_def" */

/* Line 1000 of yacc.c  */
#line 144 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->param_def)->base));};

/* Line 1000 of yacc.c  */
#line 1613 "ctf_meta_parser.tab.c"
	break;
      case 59: /* "param_value" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1622 "ctf_meta_parser.tab.c"
	break;
      case 60: /* "type_assign" */

/* Line 1000 of yacc.c  */
#line 145 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->type_assignment)->base));};

/* Line 1000 of yacc.c  */
#line 1631 "ctf_meta_parser.tab.c"
	break;
      case 61: /* "tag_reference" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1640 "ctf_meta_parser.tab.c"
	break;
      case 62: /* "tag_component" */

/* Line 1000 of yacc.c  */
#line 103 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ free((yyvaluep->str));};

/* Line 1000 of yacc.c  */
#line 1649 "ctf_meta_parser.tab.c"
	break;
      case 63: /* "typedef_decl" */

/* Line 1000 of yacc.c  */
#line 142 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_statement_destroy(&((yyvaluep->typedef_decl)->base));};

/* Line 1000 of yacc.c  */
#line 1658 "ctf_meta_parser.tab.c"
	break;
      case 64: /* "type_post_mod_list" */

/* Line 1000 of yacc.c  */
#line 186 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_post_mod_list_destroy((yyvaluep->type_post_mod_list));};

/* Line 1000 of yacc.c  */
#line 1667 "ctf_meta_parser.tab.c"
	break;
      case 65: /* "type_post_mod" */

/* Line 1000 of yacc.c  */
#line 179 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_post_mod_destroy((yyvaluep->type_post_mod));};

/* Line 1000 of yacc.c  */
#line 1676 "ctf_meta_parser.tab.c"
	break;
      case 66: /* "type_post_mod_array" */

/* Line 1000 of yacc.c  */
#line 180 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_post_mod_destroy(&(yyvaluep->type_post_mod_array)->base);};

/* Line 1000 of yacc.c  */
#line 1685 "ctf_meta_parser.tab.c"
	break;
      case 67: /* "type_post_mod_sequence" */

/* Line 1000 of yacc.c  */
#line 181 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
	{ ctf_parse_type_post_mod_destroy(&(yyvaluep->type_post_mod_sequence)->base);};

/* Line 1000 of yacc.c  */
#line 1694 "ctf_meta_parser.tab.c"
	break;

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (struct ctf_meta_parser_state* state, yyscan_t scanner);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc;

/* Number of syntax errors so far.  */
int yynerrs;



/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (struct ctf_meta_parser_state* state, yyscan_t scanner)
#else
int
yyparse (state, scanner)
    struct ctf_meta_parser_state* state;
    yyscan_t scanner;
#endif
#endif
{


    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[2];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

#if YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 1;
#endif

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);

	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
	YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:

/* Line 1455 of yacc.c  */
#line 245 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { ctf_parse_scope_add_statement(&state->ast->scope_root->base, (yyvsp[(2) - (2)].statement)); ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 249 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.statement) = &((yyvsp[(1) - (1)].scope_top_decl)->base); ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 252 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.scope_top_decl) = ctf_parse_scope_top_decl_create();
                            if((yyval.scope_top_decl) == NULL) nomem();
                            (yyval.scope_top_decl)->scope_name = (yyvsp[(1) - (5)].str);
                            ctf_parse_scope_top_connect((yyvsp[(3) - (5)].scope_top), (yyval.scope_top_decl));
                        ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 259 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.statement) = &((yyvsp[(1) - (1)].struct_decl)->base);;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 261 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.statement) = &((yyvsp[(1) - (1)].variant_decl)->base);;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 263 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.statement) = &((yyvsp[(1) - (1)].enum_decl)->base);;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 265 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.statement) = &((yyvsp[(1) - (1)].typedef_decl)->base);;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 268 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.struct_decl) = ctf_parse_struct_decl_create();
                            (yyval.struct_decl)->struct_spec = (yyvsp[(1) - (2)].struct_spec);
                        ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 274 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.struct_spec) = ctf_parse_struct_spec_create();
                            if((yyval.struct_spec) == NULL) nomem();
                            (yyval.struct_spec)->struct_name = (yyvsp[(2) - (5)].str);
                            ctf_parse_scope_struct_connect((yyvsp[(4) - (5)].scope_struct), (yyval.struct_spec));
                        ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 281 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.struct_spec) = ctf_parse_struct_spec_create();
                            if((yyval.struct_spec) == NULL) nomem();
                            (yyval.struct_spec)->struct_name = NULL;
                            ctf_parse_scope_struct_connect((yyvsp[(3) - (4)].scope_struct), (yyval.struct_spec));
                        ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 288 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.struct_spec) = ctf_parse_struct_spec_create();
                            if((yyval.struct_spec) == NULL) nomem();
                            (yyval.struct_spec)->struct_name = (yyvsp[(2) - (2)].str);
                        ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 295 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_decl) = ctf_parse_variant_decl_create();
                            (yyval.variant_decl)->variant_spec = (yyvsp[(1) - (2)].variant_spec);
                        ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 301 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = (yyvsp[(2) - (5)].str);
                            ctf_parse_scope_variant_connect((yyvsp[(4) - (5)].scope_variant), (yyval.variant_spec));
                        ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 308 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = (yyvsp[(2) - (8)].str);
                            ctf_parse_scope_variant_connect((yyvsp[(7) - (8)].scope_variant), (yyval.variant_spec));
                            (yyval.variant_spec)->variant_tag = (yyvsp[(4) - (8)].str);
                        ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 316 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = NULL;
                            ctf_parse_scope_variant_connect((yyvsp[(3) - (4)].scope_variant), (yyval.variant_spec));
                        ;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 323 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = NULL;
                            ctf_parse_scope_variant_connect((yyvsp[(6) - (7)].scope_variant), (yyval.variant_spec));
                            (yyval.variant_spec)->variant_tag = (yyvsp[(3) - (7)].str);
                        ;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 331 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = (yyvsp[(2) - (2)].str);
                        ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 337 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.variant_spec) = ctf_parse_variant_spec_create();
                            if((yyval.variant_spec) == NULL) nomem();
                            (yyval.variant_spec)->variant_name = (yyvsp[(2) - (5)].str);
                            (yyval.variant_spec)->variant_tag = (yyvsp[(4) - (5)].str);
                        ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 346 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.enum_decl) = ctf_parse_enum_decl_create();
                            (yyval.enum_decl)->enum_spec = (yyvsp[(1) - (2)].enum_spec);
                        ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 352 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_spec) = ctf_parse_enum_spec_create();
                            if((yyval.enum_spec) == NULL) nomem();
                            (yyval.enum_spec)->enum_name = (yyvsp[(2) - (7)].str);
                            (yyval.enum_spec)->scope_enum = (yyvsp[(4) - (7)].scope_enum);
                            (yyval.enum_spec)->type_spec_int = (yyvsp[(7) - (7)].type_spec);
                        ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 360 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_spec) = ctf_parse_enum_spec_create();
                            if((yyval.enum_spec) == NULL) nomem();
                            (yyval.enum_spec)->scope_enum = (yyvsp[(3) - (6)].scope_enum);
                            (yyval.enum_spec)->type_spec_int = (yyvsp[(6) - (6)].type_spec);
                        ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 367 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_spec) = ctf_parse_enum_spec_create();
                            if((yyval.enum_spec) == NULL) nomem();
                            (yyval.enum_spec)->enum_name = (yyvsp[(2) - (2)].str);
                        ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 374 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.scope_enum) = ctf_parse_scope_enum_create();
                            if((yyval.scope_enum) == NULL) nomem();
                            ctf_parse_scope_enum_add_value((yyval.scope_enum), (yyvsp[(1) - (1)].enum_value));
                        ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 380 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.scope_enum) = (yyvsp[(1) - (3)].scope_enum);
                            ctf_parse_scope_enum_add_value((yyval.scope_enum), (yyvsp[(3) - (3)].enum_value));
                        ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 386 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.enum_value) = &(yyvsp[(1) - (1)].enum_value_simple)->base;;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 388 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.enum_value) = &(yyvsp[(1) - (1)].enum_value_presize)->base;;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 390 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.enum_value) = &(yyvsp[(1) - (1)].enum_value_range)->base;;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 393 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_value_simple) = ctf_parse_enum_value_simple_create();
                            if((yyval.enum_value_simple) == NULL) nomem();
                            (yyval.enum_value_simple)->val_name = (yyvsp[(1) - (1)].str);
                        ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 400 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_value_presize) = ctf_parse_enum_value_presize_create();
                            if((yyval.enum_value_presize) == NULL) nomem();
                            (yyval.enum_value_presize)->val_name = (yyvsp[(1) - (3)].str);
                            (yyval.enum_value_presize)->int_value = (yyvsp[(3) - (3)].str);
                        ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 407 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.enum_value_range) = ctf_parse_enum_value_range_create();
                            if((yyval.enum_value_range) == NULL) nomem();
                            (yyval.enum_value_range)->val_name = (yyvsp[(1) - (5)].str);
                            (yyval.enum_value_range)->int_value_start = (yyvsp[(3) - (5)].str);
                            (yyval.enum_value_range)->int_value_end = (yyvsp[(5) - (5)].str);
                        ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 417 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("trace"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 419 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("stream"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 421 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("event"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 424 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_top) = ctf_parse_scope_top_create(); if((yyval.scope_top) == NULL) nomem();;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 426 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_top) = (yyvsp[(1) - (2)].scope_top); ctf_parse_scope_add_statement(&(yyval.scope_top)->base, (yyvsp[(2) - (2)].statement)); ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 430 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.statement) = &(yyvsp[(1) - (1)].type_assignment)->base;;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 433 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_struct) = ctf_parse_scope_struct_create(); if((yyval.scope_struct) == NULL) nomem();;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 435 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_struct) = (yyvsp[(1) - (2)].scope_struct); ctf_parse_scope_add_statement(&(yyval.scope_struct)->base, (yyvsp[(2) - (2)].statement)); ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 438 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.statement) = &(yyvsp[(1) - (1)].field_decl)->base;;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 441 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_variant) = ctf_parse_scope_variant_create(); if((yyval.scope_variant) == NULL) nomem();;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 443 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.scope_variant) = (yyvsp[(1) - (2)].scope_variant); ctf_parse_scope_add_statement(&(yyval.scope_variant)->base, (yyvsp[(2) - (2)].statement)); ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 446 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.statement) = &(yyvsp[(1) - (1)].field_decl)->base;;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 450 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.field_decl) = ctf_parse_field_decl_create();
                            if((yyval.field_decl) == NULL) nomem();
                            (yyval.field_decl)->type_spec = (yyvsp[(1) - (4)].type_spec);
                            (yyval.field_decl)->field_name = (yyvsp[(2) - (4)].str);
                            (yyval.field_decl)->type_post_mod_list = (yyvsp[(3) - (4)].type_post_mod_list);
                        ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 457 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.type_spec) = &(yyvsp[(1) - (1)].struct_spec)->base;;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 458 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.type_spec) = &(yyvsp[(1) - (1)].variant_spec)->base;;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 459 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.type_spec) = &(yyvsp[(1) - (1)].type_spec_id)->base;;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 460 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.type_spec) = &(yyvsp[(1) - (1)].int_spec)->base;;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 461 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.type_spec) = &(yyvsp[(1) - (1)].enum_spec)->base;;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 464 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.type_spec_id) = ctf_parse_type_spec_id_create();
                            if((yyval.type_spec_id) == NULL) nomem();
                            (yyval.type_spec_id)->type_name = (yyvsp[(1) - (1)].str);
                        ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 471 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.int_spec) = ctf_parse_int_spec_create();
                            if((yyval.int_spec) == NULL) nomem();
                            (yyval.int_spec)->scope_int = (yyvsp[(3) - (4)].scope_int);
                        ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 478 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.scope_int) = ctf_parse_scope_int_create(); if((yyval.scope_int) == NULL) nomem(); ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 480 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.scope_int) = (yyvsp[(1) - (2)].scope_int); ctf_parse_scope_add_statement(&(yyval.scope_int)->base, (yyvsp[(2) - (2)].statement)); ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 483 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.statement) = &(yyvsp[(1) - (1)].param_def)->base;;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 486 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.param_def) = ctf_parse_param_def_create();
                            if((yyval.param_def) == NULL) nomem();
                            (yyval.param_def)->param_name = (yyvsp[(1) - (4)].str);
                            (yyval.param_def)->param_value = (yyvsp[(3) - (4)].str);
                        ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 497 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { 
                            (yyval.type_assignment) = ctf_parse_type_assignment_create();
                            if((yyval.type_assignment) == NULL) nomem();
                            
                            (yyval.type_assignment)->tag = (yyvsp[(1) - (4)].str);
                            (yyval.type_assignment)->type_spec = (yyvsp[(3) - (4)].type_spec);
                        ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 508 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strappend_format((yyvsp[(1) - (3)].str), ".%s", (yyvsp[(3) - (3)].str)); free((yyvsp[(3) - (3)].str)); if((yyval.str) == NULL) nomem();;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 510 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strappend_format((yyvsp[(1) - (3)].str), ".%s", (yyvsp[(3) - (3)].str)); free((yyvsp[(3) - (3)].str)); if((yyval.str) == NULL) nomem();;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 512 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strappend_format((yyvsp[(1) - (4)].str), "[%s]", (yyvsp[(3) - (4)].str)); free((yyvsp[(3) - (4)].str)); if((yyval.str) == NULL) nomem();;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 516 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("trace"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 518 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("stream"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 520 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {(yyval.str) = strdup("event"); if((yyval.str) == NULL) nomem(); ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 523 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                        (yyval.typedef_decl) = ctf_parse_typedef_decl_create();
                        if((yyval.typedef_decl) == NULL) nomem();
                        (yyval.typedef_decl)->type_spec_base = (yyvsp[(2) - (5)].type_spec);
                        (yyval.typedef_decl)->type_name = (yyvsp[(3) - (5)].str);
                        (yyval.typedef_decl)->type_post_mod_list = (yyvsp[(4) - (5)].type_post_mod_list);
                    ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 532 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.type_post_mod_list) = ctf_parse_type_post_mod_list_create();
                            if((yyval.type_post_mod_list) == NULL) nomem();
                        ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 537 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.type_post_mod_list) = (yyvsp[(1) - (2)].type_post_mod_list);
                            ctf_parse_type_post_mod_list_add_mod((yyval.type_post_mod_list), (yyvsp[(2) - (2)].type_post_mod));
                        ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 543 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.type_post_mod) = &(yyvsp[(1) - (1)].type_post_mod_array)->base;;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 545 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    { (yyval.type_post_mod) = &(yyvsp[(1) - (1)].type_post_mod_sequence)->base;;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 548 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.type_post_mod_array) = ctf_parse_type_post_mod_array_create();
                            if((yyval.type_post_mod_array) == NULL) { free((yyvsp[(2) - (3)].str)); nomem();}
                            (yyval.type_post_mod_array)->array_len = (yyvsp[(2) - (3)].str);
                        ;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 555 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"
    {
                            (yyval.type_post_mod_sequence) = ctf_parse_type_post_mod_sequence_create();
                            if((yyval.type_post_mod_sequence) == NULL) { free((yyvsp[(2) - (3)].str)); nomem();}
                            (yyval.type_post_mod_sequence)->sequence_len = (yyvsp[(2) - (3)].str);
                        ;}
    break;



/* Line 1455 of yacc.c  */
#line 2644 "ctf_meta_parser.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (state, scanner, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (state, scanner, yymsg);
	  }
	else
	  {
	    yyerror (state, scanner, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc, state, scanner);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[0] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp, state, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (state, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc, state, scanner);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, state, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 562 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"


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
