
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison LALR(1) parsers in C++
   
      Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008 Free Software
   Foundation, Inc.
   
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


/* First part of user declarations.  */

/* Line 311 of lalr1.cc  */
#line 42 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"


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


/* Line 311 of lalr1.cc  */
#line 73 "ctf_reader_parser_base.tab.cc"


#include "ctf_reader_parser_base.tab.hh"

/* User implementation prologue.  */


/* Line 317 of lalr1.cc  */
#line 82 "ctf_reader_parser_base.tab.cc"
/* Unqualified %code blocks.  */

/* Line 318 of lalr1.cc  */
#line 14 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"

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



/* Line 318 of lalr1.cc  */
#line 111 "ctf_reader_parser_base.tab.cc"

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* FIXME: INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#define YYUSE(e) ((void) (e))

/* Enable debugging if requested.  */
#if YYDEBUG

/* A pseudo ostream that takes yydebug_ into account.  */
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)	\
do {							\
  if (yydebug_)						\
    {							\
      *yycdebug_ << Title << ' ';			\
      yy_symbol_print_ ((Type), (Value), (Location));	\
      *yycdebug_ << std::endl;				\
    }							\
} while (false)

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug_)				\
    yy_reduce_print_ (Rule);		\
} while (false)

# define YY_STACK_PRINT()		\
do {					\
  if (yydebug_)				\
    yystack_print_ ();			\
} while (false)

#else /* !YYDEBUG */

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_REDUCE_PRINT(Rule)
# define YY_STACK_PRINT()

#endif /* !YYDEBUG */

#define yyerrok		(yyerrstatus_ = 0)
#define yyclearin	(yychar = yyempty_)

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


/* Line 380 of lalr1.cc  */
#line 1 "[Bison:b4_percent_define_default]"

namespace yy {

/* Line 380 of lalr1.cc  */
#line 180 "ctf_reader_parser_base.tab.cc"
#if YYERROR_VERBOSE

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  parser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
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
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }

#endif

  /// Build a parser object.
  parser::parser (CTFReaderScanner* scanner_yyarg, CTFAST& ast_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      scanner (scanner_yyarg),
      ast (ast_yyarg)
  {
  }

  parser::~parser ()
  {
  }

#if YYDEBUG
  /*--------------------------------.
  | Print this symbol on YYOUTPUT.  |
  `--------------------------------*/

  inline void
  parser::yy_symbol_value_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yyvaluep);
    switch (yytype)
      {
         default:
	  break;
      }
  }


  void
  parser::yy_symbol_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    *yycdebug_ << (yytype < yyntokens_ ? "token" : "nterm")
	       << ' ' << yytname_[yytype] << " ("
	       << *yylocationp << ": ";
    yy_symbol_value_print_ (yytype, yyvaluep, yylocationp);
    *yycdebug_ << ')';
  }
#endif

  void
  parser::yydestruct_ (const char* yymsg,
			   int yytype, semantic_type* yyvaluep, location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yymsg);
    YYUSE (yyvaluep);

    YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

    switch (yytype)
      {
        case 14: /* "ID" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 288 "ctf_reader_parser_base.tab.cc"
	break;
      case 15: /* "STRING_LITERAL" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 297 "ctf_reader_parser_base.tab.cc"
	break;
      case 16: /* "INTEGER_CONSTANT" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 306 "ctf_reader_parser_base.tab.cc"
	break;
      case 48: /* "top_scope_name" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 315 "ctf_reader_parser_base.tab.cc"
	break;
      case 63: /* "param_value" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 324 "ctf_reader_parser_base.tab.cc"
	break;
      case 65: /* "tag_reference" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 333 "ctf_reader_parser_base.tab.cc"
	break;
      case 66: /* "tag_component" */

/* Line 480 of lalr1.cc  */
#line 92 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{ delete (yyvaluep->str);};

/* Line 480 of lalr1.cc  */
#line 342 "ctf_reader_parser_base.tab.cc"
	break;
      case 71: /* "type_post_mods" */

/* Line 480 of lalr1.cc  */
#line 155 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
	{delete (yyvaluep->typePostMods);};

/* Line 480 of lalr1.cc  */
#line 351 "ctf_reader_parser_base.tab.cc"
	break;

	default:
	  break;
      }
  }

  void
  parser::yypop_ (unsigned int n)
  {
    yystate_stack_.pop (n);
    yysemantic_stack_.pop (n);
    yylocation_stack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  parser::debug_level_type
  parser::debug_level () const
  {
    return yydebug_;
  }

  void
  parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif

  int
  parser::parse ()
  {
    /// Lookahead and lookahead in internal form.
    int yychar = yyempty_;
    int yytoken = 0;

    /* State.  */
    int yyn;
    int yylen = 0;
    int yystate = 0;

    /* Error handling.  */
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// Semantic value of the lookahead.
    semantic_type yylval;
    /// Location of the lookahead.
    location_type yylloc;
    /// The locations where the error started and ended.
    location_type yyerror_range[2];

    /// $$.
    semantic_type yyval;
    /// @$.
    location_type yyloc;

    int yyresult;

    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stacks.  The initial state will be pushed in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystate_stack_ = state_stack_type (0);
    yysemantic_stack_ = semantic_stack_type (0);
    yylocation_stack_ = location_stack_type (0);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* New state.  */
  yynewstate:
    yystate_stack_.push (yystate);
    YYCDEBUG << "Entering state " << yystate << std::endl;

    /* Accept?  */
    if (yystate == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    /* Backup.  */
  yybackup:

    /* Try to take a decision without lookahead.  */
    yyn = yypact_[yystate];
    if (yyn == yypact_ninf_)
      goto yydefault;

    /* Read a lookahead token.  */
    if (yychar == yyempty_)
      {
	YYCDEBUG << "Reading a token: ";
	yychar = yylex (&yylval, &yylloc, scanner);
      }


    /* Convert token to internal form.  */
    if (yychar <= yyeof_)
      {
	yychar = yytoken = yyeof_;
	YYCDEBUG << "Now at end of input." << std::endl;
      }
    else
      {
	yytoken = yytranslate_ (yychar);
	YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
      }

    /* If the proper action on seeing token YYTOKEN is to reduce or to
       detect an error, take that action.  */
    yyn += yytoken;
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yytoken)
      goto yydefault;

    /* Reduce or error.  */
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
	if (yyn == 0 || yyn == yytable_ninf_)
	goto yyerrlab;
	yyn = -yyn;
	goto yyreduce;
      }

    /* Shift the lookahead token.  */
    YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

    /* Discard the token being shifted.  */
    yychar = yyempty_;

    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* Count tokens shifted since error; after three, turn off error
       status.  */
    if (yyerrstatus_)
      --yyerrstatus_;

    yystate = yyn;
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystate];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    /* If YYLEN is nonzero, implement the default value of the action:
       `$$ = $1'.  Otherwise, use the top of the stack.

       Otherwise, the following line sets YYVAL to garbage.
       This behavior is undocumented and Bison
       users should not rely upon it.  */
    if (yylen)
      yyval = yysemantic_stack_[yylen - 1];
    else
      yyval = yysemantic_stack_[0];

    {
      slice<location_type, location_stack_type> slice (yylocation_stack_, yylen);
      YYLLOC_DEFAULT (yyloc, slice, yylen);
    }
    YY_REDUCE_PRINT (yyn);
    switch (yyn)
      {
	  case 3:

/* Line 678 of lalr1.cc  */
#line 228 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {ast.rootScope->addStatement(ptr((yysemantic_stack_[(2) - (2)].statement)));}
    break;

  case 5:

/* Line 678 of lalr1.cc  */
#line 232 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.statement) = (yysemantic_stack_[(1) - (1)].topScopeDecl); }
    break;

  case 6:

/* Line 678 of lalr1.cc  */
#line 235 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.topScopeDecl) = new CTFASTTopScopeDecl(ptr((yysemantic_stack_[(5) - (1)].str)), ptr((yysemantic_stack_[(5) - (3)].scopeTop)));}
    break;

  case 7:

/* Line 678 of lalr1.cc  */
#line 238 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.statement) = (yysemantic_stack_[(1) - (1)].structDecl);}
    break;

  case 8:

/* Line 678 of lalr1.cc  */
#line 240 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.statement) = (yysemantic_stack_[(1) - (1)].variantDecl);}
    break;

  case 9:

/* Line 678 of lalr1.cc  */
#line 242 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.statement) = (yysemantic_stack_[(1) - (1)].enumDecl);}
    break;

  case 10:

/* Line 678 of lalr1.cc  */
#line 244 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.statement) = (yysemantic_stack_[(1) - (1)].typedefDecl);}
    break;

  case 11:

/* Line 678 of lalr1.cc  */
#line 247 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.structDecl) = new CTFASTStructDecl(ptr((yysemantic_stack_[(2) - (1)].structSpec)));}
    break;

  case 12:

/* Line 678 of lalr1.cc  */
#line 250 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.structSpec) = new CTFASTStructSpec(ptr((yysemantic_stack_[(5) - (2)].str)), ptr((yysemantic_stack_[(5) - (4)].scopeStruct)));}
    break;

  case 13:

/* Line 678 of lalr1.cc  */
#line 252 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.structSpec) = new CTFASTStructSpec(ptr((yysemantic_stack_[(4) - (3)].scopeStruct)));}
    break;

  case 14:

/* Line 678 of lalr1.cc  */
#line 254 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.structSpec) = new CTFASTStructSpec(ptr((yysemantic_stack_[(2) - (2)].str)));}
    break;

  case 15:

/* Line 678 of lalr1.cc  */
#line 257 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.variantDecl) = new CTFASTVariantDecl(ptr((yysemantic_stack_[(2) - (1)].variantSpec)));}
    break;

  case 16:

/* Line 678 of lalr1.cc  */
#line 260 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(ptr((yysemantic_stack_[(5) - (2)].str)),
                                nullStr(), ptr((yysemantic_stack_[(5) - (4)].scopeVariant)));
                        }
    break;

  case 17:

/* Line 678 of lalr1.cc  */
#line 265 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(ptr((yysemantic_stack_[(8) - (2)].str)),
                                ptr((yysemantic_stack_[(8) - (4)].str)), ptr((yysemantic_stack_[(8) - (7)].scopeVariant)));
                        }
    break;

  case 18:

/* Line 678 of lalr1.cc  */
#line 270 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(nullStr(),
                                nullStr(), ptr((yysemantic_stack_[(4) - (3)].scopeVariant)));
                        }
    break;

  case 19:

/* Line 678 of lalr1.cc  */
#line 275 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(nullStr(),
                                ptr((yysemantic_stack_[(7) - (3)].str)), ptr((yysemantic_stack_[(7) - (6)].scopeVariant)));
                        }
    break;

  case 20:

/* Line 678 of lalr1.cc  */
#line 280 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(ptr((yysemantic_stack_[(2) - (2)].str)),
                                nullStr(), nullScopeVariant());
                        }
    break;

  case 21:

/* Line 678 of lalr1.cc  */
#line 285 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.variantSpec) = new CTFASTVariantSpec(ptr((yysemantic_stack_[(5) - (2)].str)),
                                ptr((yysemantic_stack_[(5) - (4)].str)), nullScopeVariant());
                        }
    break;

  case 22:

/* Line 678 of lalr1.cc  */
#line 291 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.enumDecl) = new CTFASTEnumDecl(ptr((yysemantic_stack_[(2) - (1)].enumSpec))); }
    break;

  case 23:

/* Line 678 of lalr1.cc  */
#line 294 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.enumSpec) = new CTFASTEnumSpec(ptr((yysemantic_stack_[(7) - (2)].str)), ptr((yysemantic_stack_[(7) - (6)].scopeEnum)),
                                ptr((yysemantic_stack_[(7) - (4)].typeSpec)));
                        }
    break;

  case 24:

/* Line 678 of lalr1.cc  */
#line 299 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.enumSpec) = new CTFASTEnumSpec(nullStr(), ptr((yysemantic_stack_[(6) - (5)].scopeEnum)),
                                ptr((yysemantic_stack_[(6) - (3)].typeSpec)));
                        }
    break;

  case 25:

/* Line 678 of lalr1.cc  */
#line 304 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {
                            (yyval.enumSpec) = new CTFASTEnumSpec(ptr((yysemantic_stack_[(2) - (2)].str)), nullScopeEnum(),
                                nullTypeSpec());
                        }
    break;

  case 26:

/* Line 678 of lalr1.cc  */
#line 309 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].intSpec);}
    break;

  case 27:

/* Line 678 of lalr1.cc  */
#line 310 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].typeIDSpec);}
    break;

  case 31:

/* Line 678 of lalr1.cc  */
#line 317 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.scopeEnum) = new CTFASTScopeEnum();}
    break;

  case 32:

/* Line 678 of lalr1.cc  */
#line 320 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.scopeEnum) = (yysemantic_stack_[(2) - (1)].scopeEnum); (yyval.scopeEnum)->addValueDecl(ptr((yysemantic_stack_[(2) - (2)].enumValueDecl)));}
    break;

  case 33:

/* Line 678 of lalr1.cc  */
#line 322 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.scopeEnum) = (yysemantic_stack_[(3) - (1)].scopeEnum); (yyval.scopeEnum)->addValueDecl(ptr((yysemantic_stack_[(3) - (3)].enumValueDecl)));}
    break;

  case 34:

/* Line 678 of lalr1.cc  */
#line 325 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDecl) = (yysemantic_stack_[(1) - (1)].enumValueDeclSimple);}
    break;

  case 35:

/* Line 678 of lalr1.cc  */
#line 327 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDecl) = (yysemantic_stack_[(1) - (1)].enumValueDeclPresize);}
    break;

  case 36:

/* Line 678 of lalr1.cc  */
#line 329 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDecl) = (yysemantic_stack_[(1) - (1)].enumValueDeclRange);}
    break;

  case 37:

/* Line 678 of lalr1.cc  */
#line 332 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDeclSimple) = new CTFASTEnumValueDeclSimple(ptr((yysemantic_stack_[(1) - (1)].str))); }
    break;

  case 38:

/* Line 678 of lalr1.cc  */
#line 335 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDeclPresize) = new CTFASTEnumValueDeclPresize(ptr((yysemantic_stack_[(3) - (1)].str)), ptr((yysemantic_stack_[(3) - (3)].str)));}
    break;

  case 39:

/* Line 678 of lalr1.cc  */
#line 337 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.enumValueDeclRange) = new CTFASTEnumValueDeclRange(ptr((yysemantic_stack_[(5) - (1)].str)), ptr((yysemantic_stack_[(5) - (3)].str)), ptr((yysemantic_stack_[(5) - (5)].str)));}
    break;

  case 40:

/* Line 678 of lalr1.cc  */
#line 339 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("trace");}
    break;

  case 41:

/* Line 678 of lalr1.cc  */
#line 340 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("stream");}
    break;

  case 42:

/* Line 678 of lalr1.cc  */
#line 341 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("event");}
    break;

  case 43:

/* Line 678 of lalr1.cc  */
#line 344 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeTop) = new CTFASTScopeTop();}
    break;

  case 44:

/* Line 678 of lalr1.cc  */
#line 346 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeTop) = (yysemantic_stack_[(2) - (1)].scopeTop); (yyval.scopeTop)->addStatement(ptr((yysemantic_stack_[(2) - (2)].statement))); }
    break;

  case 46:

/* Line 678 of lalr1.cc  */
#line 349 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.statement) = (yysemantic_stack_[(1) - (1)].parameterDef);}
    break;

  case 47:

/* Line 678 of lalr1.cc  */
#line 350 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.statement) = (yysemantic_stack_[(1) - (1)].typeAssignment);}
    break;

  case 48:

/* Line 678 of lalr1.cc  */
#line 353 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeStruct) = new CTFASTScopeStruct();}
    break;

  case 49:

/* Line 678 of lalr1.cc  */
#line 355 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeStruct) = (yysemantic_stack_[(2) - (1)].scopeStruct); (yyval.scopeStruct)->addStatement(ptr((yysemantic_stack_[(2) - (2)].statement))); }
    break;

  case 51:

/* Line 678 of lalr1.cc  */
#line 358 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.statement) = (yysemantic_stack_[(1) - (1)].fieldDecl);}
    break;

  case 52:

/* Line 678 of lalr1.cc  */
#line 361 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeVariant) = new CTFASTScopeVariant();}
    break;

  case 53:

/* Line 678 of lalr1.cc  */
#line 363 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.scopeVariant) = (yysemantic_stack_[(2) - (1)].scopeVariant); (yyval.scopeVariant)->addStatement(ptr((yysemantic_stack_[(2) - (2)].statement))); }
    break;

  case 55:

/* Line 678 of lalr1.cc  */
#line 366 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.statement) = (yysemantic_stack_[(1) - (1)].fieldDecl);}
    break;

  case 57:

/* Line 678 of lalr1.cc  */
#line 371 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.fieldDecl) = new CTFASTFieldDecl(ptr((yysemantic_stack_[(2) - (1)].typeSpec)), ptr((yysemantic_stack_[(2) - (2)].typeInstField)));}
    break;

  case 58:

/* Line 678 of lalr1.cc  */
#line 373 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.fieldDecl) = (yysemantic_stack_[(3) - (1)].fieldDecl); (yyval.fieldDecl)->addTypeInst(ptr((yysemantic_stack_[(3) - (3)].typeInstField)));}
    break;

  case 59:

/* Line 678 of lalr1.cc  */
#line 375 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].structSpec);}
    break;

  case 60:

/* Line 678 of lalr1.cc  */
#line 376 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].variantSpec);}
    break;

  case 61:

/* Line 678 of lalr1.cc  */
#line 377 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].typeIDSpec);}
    break;

  case 62:

/* Line 678 of lalr1.cc  */
#line 378 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].intSpec);}
    break;

  case 63:

/* Line 678 of lalr1.cc  */
#line 379 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeSpec) = (yysemantic_stack_[(1) - (1)].enumSpec);}
    break;

  case 64:

/* Line 678 of lalr1.cc  */
#line 381 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.typeIDSpec) = new CTFASTTypeIDSpec(ptr((yysemantic_stack_[(1) - (1)].str)));}
    break;

  case 65:

/* Line 678 of lalr1.cc  */
#line 384 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.intSpec) = new CTFASTIntSpec(ptr((yysemantic_stack_[(4) - (3)].scopeInt)));}
    break;

  case 66:

/* Line 678 of lalr1.cc  */
#line 387 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.scopeInt) = new CTFASTScopeInt();}
    break;

  case 67:

/* Line 678 of lalr1.cc  */
#line 389 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.scopeInt) = (yysemantic_stack_[(2) - (1)].scopeInt); (yyval.scopeInt)->addStatement(ptr((yysemantic_stack_[(2) - (2)].statement))); }
    break;

  case 68:

/* Line 678 of lalr1.cc  */
#line 392 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.statement) = (yysemantic_stack_[(1) - (1)].parameterDef);}
    break;

  case 69:

/* Line 678 of lalr1.cc  */
#line 395 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.parameterDef) = new CTFASTParameterDef(ptr((yysemantic_stack_[(4) - (1)].str)), ptr((yysemantic_stack_[(4) - (3)].str)));}
    break;

  case 73:

/* Line 678 of lalr1.cc  */
#line 401 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeAssignment) = new CTFASTTypeAssignment(ptr((yysemantic_stack_[(5) - (1)].str)), ptr((yysemantic_stack_[(5) - (3)].typeSpec)), ptr((yysemantic_stack_[(5) - (4)].typePostMods)));}
    break;

  case 75:

/* Line 678 of lalr1.cc  */
#line 406 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = (yysemantic_stack_[(3) - (1)].str); (yyval.str)->append("." + *(yysemantic_stack_[(3) - (3)].str)); delete (yysemantic_stack_[(3) - (3)].str);}
    break;

  case 76:

/* Line 678 of lalr1.cc  */
#line 408 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = (yysemantic_stack_[(3) - (1)].str); (yyval.str)->append("." + *(yysemantic_stack_[(3) - (3)].str)); delete (yysemantic_stack_[(3) - (3)].str);}
    break;

  case 77:

/* Line 678 of lalr1.cc  */
#line 410 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = (yysemantic_stack_[(4) - (1)].str); (yyval.str)->append("[" + *(yysemantic_stack_[(4) - (3)].str) + "]"); delete (yysemantic_stack_[(4) - (3)].str);}
    break;

  case 79:

/* Line 678 of lalr1.cc  */
#line 413 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("trace");}
    break;

  case 80:

/* Line 678 of lalr1.cc  */
#line 414 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("stream");}
    break;

  case 81:

/* Line 678 of lalr1.cc  */
#line 415 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.str) = new std::string("event");}
    break;

  case 83:

/* Line 678 of lalr1.cc  */
#line 420 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typedefDecl) = new CTFASTTypedefDecl(ptr((yysemantic_stack_[(3) - (2)].typeSpec)), ptr((yysemantic_stack_[(3) - (3)].typeInstTypedef)));}
    break;

  case 84:

/* Line 678 of lalr1.cc  */
#line 422 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typedefDecl) = (yysemantic_stack_[(3) - (1)].typedefDecl); (yyval.typedefDecl)->addTypeInst(ptr((yysemantic_stack_[(3) - (3)].typeInstTypedef)));}
    break;

  case 85:

/* Line 678 of lalr1.cc  */
#line 426 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeInstField) = new CTFASTFieldDecl::TypeInst(ptr((yysemantic_stack_[(2) - (1)].str)), ptr((yysemantic_stack_[(2) - (2)].typePostMods)));}
    break;

  case 86:

/* Line 678 of lalr1.cc  */
#line 429 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typeInstTypedef) = new CTFASTTypedefDecl::TypeInst(ptr((yysemantic_stack_[(2) - (1)].str)), ptr((yysemantic_stack_[(2) - (2)].typePostMods)));}
    break;

  case 87:

/* Line 678 of lalr1.cc  */
#line 433 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typePostMods) = new CTFASTTypePostMods();}
    break;

  case 88:

/* Line 678 of lalr1.cc  */
#line 435 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typePostMods) = (yysemantic_stack_[(2) - (1)].typePostMods); (yyval.typePostMods)->addTypePostMod(ptr((yysemantic_stack_[(2) - (2)].typePostMod)));}
    break;

  case 89:

/* Line 678 of lalr1.cc  */
#line 437 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typePostMod) = (yysemantic_stack_[(1) - (1)].arrayMod);}
    break;

  case 90:

/* Line 678 of lalr1.cc  */
#line 438 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    {(yyval.typePostMod) = (yysemantic_stack_[(1) - (1)].sequenceMod);}
    break;

  case 91:

/* Line 678 of lalr1.cc  */
#line 441 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.arrayMod) = new CTFASTArrayMod(ptr((yysemantic_stack_[(3) - (2)].str)));}
    break;

  case 92:

/* Line 678 of lalr1.cc  */
#line 444 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"
    { (yyval.sequenceMod) = new CTFASTSequenceMod(ptr((yysemantic_stack_[(3) - (2)].str)));}
    break;



/* Line 678 of lalr1.cc  */
#line 1103 "ctf_reader_parser_base.tab.cc"
	default:
          break;
      }
    YY_SYMBOL_PRINT ("-> $$ =", yyr1_[yyn], &yyval, &yyloc);

    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();

    yysemantic_stack_.push (yyval);
    yylocation_stack_.push (yyloc);

    /* Shift the result of the reduction.  */
    yyn = yyr1_[yyn];
    yystate = yypgoto_[yyn - yyntokens_] + yystate_stack_[0];
    if (0 <= yystate && yystate <= yylast_
	&& yycheck_[yystate] == yystate_stack_[0])
      yystate = yytable_[yystate];
    else
      yystate = yydefgoto_[yyn - yyntokens_];
    goto yynewstate;

  /*------------------------------------.
  | yyerrlab -- here on detecting error |
  `------------------------------------*/
  yyerrlab:
    /* If not already recovering from an error, report this error.  */
    if (!yyerrstatus_)
      {
	++yynerrs_;
	error (yylloc, yysyntax_error_ (yystate));
      }

    yyerror_range[0] = yylloc;
    if (yyerrstatus_ == 3)
      {
	/* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

	if (yychar <= yyeof_)
	  {
	  /* Return failure if at end of input.  */
	  if (yychar == yyeof_)
	    YYABORT;
	  }
	else
	  {
	    yydestruct_ ("Error: discarding", yytoken, &yylval, &yylloc);
	    yychar = yyempty_;
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
    if (false)
      goto yyerrorlab;

    yyerror_range[0] = yylocation_stack_[yylen - 1];
    /* Do not reclaim the symbols of the rule which action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    yystate = yystate_stack_[0];
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;	/* Each real token shifted decrements this.  */

    for (;;)
      {
	yyn = yypact_[yystate];
	if (yyn != yypact_ninf_)
	{
	  yyn += yyterror_;
	  if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
	    {
	      yyn = yytable_[yyn];
	      if (0 < yyn)
		break;
	    }
	}

	/* Pop the current state because it cannot handle the error token.  */
	if (yystate_stack_.height () == 1)
	YYABORT;

	yyerror_range[0] = yylocation_stack_[0];
	yydestruct_ ("Error: popping",
		     yystos_[yystate],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);
	yypop_ ();
	yystate = yystate_stack_[0];
	YY_STACK_PRINT ();
      }

    yyerror_range[1] = yylloc;
    // Using YYLLOC is tempting, but would change the location of
    // the lookahead.  YYLOC is available though.
    YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yyloc);

    /* Shift the error token.  */
    YY_SYMBOL_PRINT ("Shifting", yystos_[yyn],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);

    yystate = yyn;
    goto yynewstate;

    /* Accept.  */
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    /* Abort.  */
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (yychar != yyempty_)
      yydestruct_ ("Cleanup: discarding lookahead", yytoken, &yylval, &yylloc);

    /* Do not reclaim the symbols of the rule which action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (yystate_stack_.height () != 1)
      {
	yydestruct_ ("Cleanup: popping",
		   yystos_[yystate_stack_[0]],
		   &yysemantic_stack_[0],
		   &yylocation_stack_[0]);
	yypop_ ();
      }

    return yyresult;
  }

  // Generate an error message.
  std::string
  parser::yysyntax_error_ (int yystate)
  {
    std::string res;
    YYUSE (yystate);
#if YYERROR_VERBOSE
    int yyn = yypact_[yystate];
    if (yypact_ninf_ < yyn && yyn <= yylast_)
      {
	/* Start YYX at -YYN if negative to avoid negative indexes in
	   YYCHECK.  */
	int yyxbegin = yyn < 0 ? -yyn : 0;

	/* Stay within bounds of both yycheck and yytname.  */
	int yychecklim = yylast_ - yyn + 1;
	int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
	int count = 0;
	for (int x = yyxbegin; x < yyxend; ++x)
	  if (yycheck_[x + yyn] == x && x != yyterror_)
	    ++count;

	// FIXME: This method of building the message is not compatible
	// with internationalization.  It should work like yacc.c does it.
	// That is, first build a string that looks like this:
	// "syntax error, unexpected %s or %s or %s"
	// Then, invoke YY_ on this string.
	// Finally, use the string as a format to output
	// yytname_[tok], etc.
	// Until this gets fixed, this message appears in English only.
	res = "syntax error, unexpected ";
	res += yytnamerr_ (yytname_[tok]);
	if (count < 5)
	  {
	    count = 0;
	    for (int x = yyxbegin; x < yyxend; ++x)
	      if (yycheck_[x + yyn] == x && x != yyterror_)
		{
		  res += (!count++) ? ", expecting " : " or ";
		  res += yytnamerr_ (yytname_[x]);
		}
	  }
      }
    else
#endif
      res = YY_("syntax error");
    return res;
  }


  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
  const signed char parser::yypact_ninf_ = -93;
  const short int
  parser::yypact_[] =
  {
       -93,   142,   -93,    22,    49,    96,   130,   -93,   -93,   -93,
     -93,   -93,   -93,   -93,    -4,   -93,    17,   -93,    28,    31,
     -93,    -6,    56,    10,    76,   -93,    48,   -93,   145,    97,
     -93,   -93,   -93,   -93,    98,   -93,   -93,   -93,   -93,   -93,
     -93,   -93,    98,    10,   108,   -93,   -93,   -93,    36,   -93,
     145,    71,   -93,   -93,   -93,   -93,    32,   -93,   -93,   -93,
     -93,    19,   -93,   121,   -93,    77,   -93,   -93,    -4,    17,
      28,   -93,   -93,   136,   126,    94,    80,   -93,   -93,   -93,
     -93,   145,   123,   145,   131,    38,   -10,   132,   138,   -93,
     -93,   -93,   -93,     8,   -93,   143,   147,   139,   -93,   -93,
     126,   -93,   -93,   -93,   146,   -93,   -93,   -93,   137,   132,
     -93,   -93,   -93,    95,   -93,   -93,   -93,    72,   -93,   130,
     148,   -93,   141,   -93,   -93,   -93,   -93,   147,   -93,   -10,
     -93,   118,   -93,   140,    44,   -93,   -93,   -93,   149,   -93,
     -93,   154,   -93,   124,   -93,   -93,   -93,   -93,    26,   158,
     -93,   -93,   156,   -93
  };

  /* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
     doesn't specify something else to do.  Zero means the default is an
     error.  */
  const unsigned char
  parser::yydefact_[] =
  {
         2,     0,     1,     0,     0,     0,     0,    40,    41,    42,
       3,     5,     4,     7,     0,     8,     0,     9,     0,     0,
      10,     0,    25,     0,    14,    48,    20,    52,     0,     0,
      64,    59,    60,    63,     0,    61,    62,    11,    15,    22,
      43,    82,     0,     0,     0,    27,    26,    48,     0,    52,
       0,     0,    79,    80,    81,    78,     0,    74,    66,    87,
      83,     0,    84,     0,    31,     0,    13,    50,    59,    60,
      63,    49,    51,     0,     0,     0,     0,    18,    54,    53,
      55,     0,     0,     0,     0,     0,    86,    78,     0,    45,
      44,    46,    47,     0,    31,     0,    28,    29,    12,    56,
       0,    87,    57,    16,    21,    76,    52,    75,     0,     0,
      65,    67,    68,     0,    88,    89,    90,     0,     6,     0,
       0,    24,    37,    32,    34,    35,    36,    30,    58,    85,
      52,     0,    77,     0,     0,    70,    71,    72,     0,    87,
      23,     0,    33,     0,    19,    91,    92,    69,     0,    38,
      17,    73,     0,    39
  };

  /* YYPGOTO[NTERM-NUM].  */
  const short int
  parser::yypgoto_[] =
  {
       -93,   -93,   -93,   -93,     3,   -93,    -1,   -93,     0,   -93,
       1,   133,    79,   -93,   -93,    47,   -93,   -93,   -93,   -93,
     -93,   -93,   128,   -93,   -41,   -93,   -35,   -93,    -3,   -12,
     -11,   -93,   -93,    92,   -93,   -93,   -40,    12,   -93,   -93,
      78,   144,   -92,   -93,   -93,   -93
  };

  /* YYDEFGOTO[NTERM-NUM].  */
  const short int
  parser::yydefgoto_[] =
  {
        -1,     1,    10,    11,    78,    13,    68,    15,    69,    17,
      70,    44,    95,    96,    97,   123,   124,   125,   126,    19,
      61,    90,    48,    71,    51,    79,    80,    73,    74,    35,
      36,    85,   111,    91,   138,    92,    56,    57,    20,    21,
     102,    60,    86,   114,   115,   116
  };

  /* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule which
     number is the opposite.  If zero, do what YYDEFACT says.  */
  const signed char parser::yytable_ninf_ = -1;
  const unsigned char
  parser::yytable_[] =
  {
        14,    16,    18,    34,    12,    31,    32,    33,    75,   129,
      76,    45,    46,    72,    41,    29,    37,   113,    42,   119,
      81,    93,     3,     4,    30,     5,     6,    52,    53,    54,
      72,    45,    46,    87,    83,    84,    22,    38,    88,     3,
       4,    29,     5,     6,    81,    23,   151,   148,    39,    40,
      30,    67,   109,   113,    82,    66,    81,   110,    83,    84,
      14,    16,    18,    24,    89,   131,    49,    25,    67,    50,
      83,    84,   146,   134,     3,     4,    29,     5,     6,    43,
       3,     4,    29,     5,     6,    30,   135,   136,   137,   143,
      77,    30,    81,   105,    47,   107,    98,     3,     4,    29,
       5,     6,   104,    52,    53,    54,    83,    84,    30,    55,
      26,   133,    59,   103,    27,    58,   139,    28,    31,    32,
      33,     3,     4,    29,     5,     6,    64,     3,     4,    29,
       5,     6,    30,     3,     4,    29,     5,   144,    30,    94,
     101,   106,     2,   150,    30,     3,     4,   108,     5,     6,
       7,     8,     9,    52,    53,    54,    99,   117,   118,    55,
     100,   122,   121,   127,   130,   132,   141,   140,   145,   147,
     149,   152,   153,   120,   142,    65,    63,   112,   128,     0,
       0,     0,     0,     0,     0,     0,    62
  };

  /* YYCHECK.  */
  const short int
  parser::yycheck_[] =
  {
         1,     1,     1,     6,     1,     6,     6,     6,    49,   101,
      50,    23,    23,    48,    20,     5,    20,    27,    24,    11,
      12,    61,     3,     4,    14,     6,     7,     8,     9,    10,
      65,    43,    43,    14,    26,    27,    14,    20,    19,     3,
       4,     5,     6,     7,    12,    23,    20,   139,    20,    18,
      14,    48,    14,    27,    22,    19,    12,    19,    26,    27,
      61,    61,    61,    14,    61,   106,    18,    18,    65,    21,
      26,    27,    28,   113,     3,     4,     5,     6,     7,    23,
       3,     4,     5,     6,     7,    14,    14,    15,    16,   130,
      19,    14,    12,    81,    18,    83,    19,     3,     4,     5,
       6,     7,    22,     8,     9,    10,    26,    27,    14,    14,
      14,    16,    14,    19,    18,    18,   119,    21,   119,   119,
     119,     3,     4,     5,     6,     7,    18,     3,     4,     5,
       6,     7,    14,     3,     4,     5,     6,    19,    14,    18,
      14,    18,     0,    19,    14,     3,     4,    16,     6,     7,
       8,     9,    10,     8,     9,    10,    20,    25,    20,    14,
      24,    14,    19,    24,    18,    28,    25,    19,    28,    20,
      16,    13,    16,    94,   127,    47,    43,    85,   100,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    42
  };

  /* STOS_[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
  const unsigned char
  parser::yystos_[] =
  {
         0,    30,     0,     3,     4,     6,     7,     8,     9,    10,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    48,
      67,    68,    14,    23,    14,    18,    14,    18,    21,     5,
      14,    35,    37,    39,    57,    58,    59,    20,    20,    20,
      18,    20,    24,    23,    40,    58,    59,    18,    51,    18,
      21,    53,     8,     9,    10,    14,    65,    66,    18,    14,
      70,    49,    70,    40,    18,    51,    19,    33,    35,    37,
      39,    52,    55,    56,    57,    53,    65,    19,    33,    54,
      55,    12,    22,    26,    27,    60,    71,    14,    19,    33,
      50,    62,    64,    65,    18,    41,    42,    43,    19,    20,
      24,    14,    69,    19,    22,    66,    18,    66,    16,    14,
      19,    61,    62,    27,    72,    73,    74,    25,    20,    11,
      41,    19,    14,    44,    45,    46,    47,    24,    69,    71,
      18,    53,    28,    16,    65,    14,    15,    16,    63,    57,
      19,    25,    44,    53,    19,    28,    28,    20,    71,    16,
      19,    20,    13,    16
  };

#if YYDEBUG
  /* TOKEN_NUMBER_[YYLEX-NUM] -- Internal symbol number corresponding
     to YYLEX-NUM.  */
  const unsigned short int
  parser::yytoken_number_[] =
  {
         0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   123,   125,
      59,    60,    62,    58,    44,    61,    46,    91,    93
  };
#endif

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
  const unsigned char
  parser::yyr1_[] =
  {
         0,    29,    30,    30,    31,    31,    32,    33,    33,    33,
      33,    34,    35,    35,    35,    36,    37,    37,    37,    37,
      37,    37,    38,    39,    39,    39,    40,    40,    41,    41,
      41,    42,    43,    43,    44,    44,    44,    45,    46,    47,
      48,    48,    48,    49,    49,    50,    50,    50,    51,    51,
      52,    52,    53,    53,    54,    54,    55,    56,    56,    57,
      57,    57,    57,    57,    58,    59,    60,    60,    61,    62,
      63,    63,    63,    64,    65,    65,    65,    65,    66,    66,
      66,    66,    67,    68,    68,    69,    70,    71,    71,    72,
      72,    73,    74
  };

  /* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
  const unsigned char
  parser::yyr2_[] =
  {
         0,     2,     0,     2,     1,     1,     5,     1,     1,     1,
       1,     2,     5,     4,     2,     2,     5,     8,     4,     7,
       2,     5,     2,     7,     6,     2,     1,     1,     1,     1,
       2,     0,     2,     3,     1,     1,     1,     1,     3,     5,
       1,     1,     1,     0,     2,     1,     1,     1,     0,     2,
       1,     1,     0,     2,     1,     1,     2,     2,     3,     1,
       1,     1,     1,     1,     1,     4,     0,     2,     1,     4,
       1,     1,     1,     5,     1,     3,     3,     4,     1,     1,
       1,     1,     2,     3,     3,     2,     2,     0,     2,     1,
       1,     3,     3
  };

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
  /* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
     First, the terminals, then, starting at \a yyntokens_, nonterminals.  */
  const char*
  const parser::yytname_[] =
  {
    "$end", "error", "$undefined", "ENUM", "STRUCT", "INTEGER", "VARIANT",
  "TYPEDEF", "TRACE", "STREAM", "EVENT", "TYPE_ASSIGNMENT_OPERATOR",
  "ARROW", "DOTDOTDOT", "ID", "STRING_LITERAL", "INTEGER_CONSTANT",
  "UNKNOWN", "'{'", "'}'", "';'", "'<'", "'>'", "':'", "','", "'='", "'.'",
  "'['", "']'", "$accept", "meta", "meta_s", "top_scope_decl", "type_decl",
  "struct_decl", "struct_spec", "variant_decl", "variant_spec",
  "enum_decl", "enum_spec", "type_spec_int", "enum_scope",
  "enum_scope_empty", "enum_scope_", "enum_value", "enum_value_simple",
  "enum_value_presize", "enum_value_range", "top_scope_name", "top_scope",
  "top_scope_s", "struct_scope", "struct_scope_s", "variant_scope",
  "variant_scope_s", "field_decl", "field_decl_", "type_spec",
  "type_spec_id", "int_spec", "int_scope", "int_scope_s", "param_def",
  "param_value", "type_assign", "tag_reference", "tag_component",
  "typedef_decl", "typedef_decl_", "type_inst_field", "type_inst_typedef",
  "type_post_mods", "type_post_mod", "type_post_mod_array",
  "type_post_mod_sequence", 0
  };
#endif

#if YYDEBUG
  /* YYRHS -- A `-1'-separated list of the rules' RHS.  */
  const parser::rhs_number_type
  parser::yyrhs_[] =
  {
        30,     0,    -1,    -1,    30,    31,    -1,    33,    -1,    32,
      -1,    48,    18,    49,    19,    20,    -1,    34,    -1,    36,
      -1,    38,    -1,    67,    -1,    35,    20,    -1,     4,    14,
      18,    51,    19,    -1,     4,    18,    51,    19,    -1,     4,
      14,    -1,    37,    20,    -1,     6,    14,    18,    53,    19,
      -1,     6,    14,    21,    65,    22,    18,    53,    19,    -1,
       6,    18,    53,    19,    -1,     6,    21,    65,    22,    18,
      53,    19,    -1,     6,    14,    -1,     6,    14,    21,    65,
      22,    -1,    39,    20,    -1,     3,    14,    23,    40,    18,
      41,    19,    -1,     3,    23,    40,    18,    41,    19,    -1,
       3,    14,    -1,    59,    -1,    58,    -1,    42,    -1,    43,
      -1,    43,    24,    -1,    -1,    42,    44,    -1,    43,    24,
      44,    -1,    45,    -1,    46,    -1,    47,    -1,    14,    -1,
      14,    25,    16,    -1,    14,    25,    16,    13,    16,    -1,
       8,    -1,     9,    -1,    10,    -1,    -1,    49,    50,    -1,
      33,    -1,    62,    -1,    64,    -1,    -1,    51,    52,    -1,
      33,    -1,    55,    -1,    -1,    53,    54,    -1,    33,    -1,
      55,    -1,    56,    20,    -1,    57,    69,    -1,    56,    24,
      69,    -1,    35,    -1,    37,    -1,    58,    -1,    59,    -1,
      39,    -1,    14,    -1,     5,    18,    60,    19,    -1,    -1,
      60,    61,    -1,    62,    -1,    14,    25,    63,    20,    -1,
      14,    -1,    15,    -1,    16,    -1,    65,    11,    57,    71,
      20,    -1,    66,    -1,    65,    26,    66,    -1,    65,    12,
      66,    -1,    65,    27,    16,    28,    -1,    14,    -1,     8,
      -1,     9,    -1,    10,    -1,    68,    20,    -1,     7,    57,
      70,    -1,    68,    24,    70,    -1,    14,    71,    -1,    14,
      71,    -1,    -1,    71,    72,    -1,    73,    -1,    74,    -1,
      27,    16,    28,    -1,    27,    65,    28,    -1
  };

  /* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
     YYRHS.  */
  const unsigned short int
  parser::yyprhs_[] =
  {
         0,     0,     3,     4,     7,     9,    11,    17,    19,    21,
      23,    25,    28,    34,    39,    42,    45,    51,    60,    65,
      73,    76,    82,    85,    93,   100,   103,   105,   107,   109,
     111,   114,   115,   118,   122,   124,   126,   128,   130,   134,
     140,   142,   144,   146,   147,   150,   152,   154,   156,   157,
     160,   162,   164,   165,   168,   170,   172,   175,   178,   182,
     184,   186,   188,   190,   192,   194,   199,   200,   203,   205,
     210,   212,   214,   216,   222,   224,   228,   232,   237,   239,
     241,   243,   245,   248,   252,   256,   259,   262,   263,   266,
     268,   270,   274
  };

  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
  const unsigned short int
  parser::yyrline_[] =
  {
         0,   226,   226,   227,   230,   231,   234,   237,   239,   241,
     243,   246,   249,   251,   253,   256,   259,   264,   269,   274,
     279,   284,   290,   293,   298,   303,   309,   310,   312,   313,
     314,   317,   319,   321,   324,   326,   328,   331,   334,   336,
     339,   340,   341,   344,   345,   348,   349,   350,   353,   354,
     357,   358,   361,   362,   365,   366,   368,   370,   372,   375,
     376,   377,   378,   379,   381,   383,   387,   388,   391,   394,
     396,   397,   398,   400,   404,   405,   407,   409,   412,   413,
     414,   415,   417,   419,   421,   425,   428,   433,   434,   437,
     438,   440,   443
  };

  // Print the state stack on the debug stream.
  void
  parser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (state_stack_type::const_iterator i = yystate_stack_.begin ();
	 i != yystate_stack_.end (); ++i)
      *yycdebug_ << ' ' << *i;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  parser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    /* Print the symbols being reduced, and their result.  */
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
	       << " (line " << yylno << "):" << std::endl;
    /* The symbols being reduced.  */
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
		       yyrhs_[yyprhs_[yyrule] + yyi],
		       &(yysemantic_stack_[(yynrhs) - (yyi + 1)]),
		       &(yylocation_stack_[(yynrhs) - (yyi + 1)]));
  }
#endif // YYDEBUG

  /* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
  parser::token_number_type
  parser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
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
    if ((unsigned int) t <= yyuser_token_number_max_)
      return translate_table[t];
    else
      return yyundef_token_;
  }

  const int parser::yyeof_ = 0;
  const int parser::yylast_ = 186;
  const int parser::yynnts_ = 46;
  const int parser::yyempty_ = -2;
  const int parser::yyfinal_ = 2;
  const int parser::yyterror_ = 1;
  const int parser::yyerrcode_ = 256;
  const int parser::yyntokens_ = 29;

  const unsigned int parser::yyuser_token_number_max_ = 272;
  const parser::token_number_type parser::yyundef_token_ = 2;


/* Line 1054 of lalr1.cc  */
#line 1 "[Bison:b4_percent_define_default]"

} // yy

/* Line 1054 of lalr1.cc  */
#line 1680 "ctf_reader_parser_base.tab.cc"


/* Line 1056 of lalr1.cc  */
#line 447 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_reader_parser_base.yy"


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

