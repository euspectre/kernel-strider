
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
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

/* Line 1676 of yacc.c  */
#line 100 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    char* str;


/* Line 1676 of yacc.c  */
#line 113 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_scope_top* scope_top;
    struct ctf_parse_scope_struct* scope_struct;
    struct ctf_parse_scope_variant* scope_variant;
    struct ctf_parse_scope_int* scope_int;
    struct ctf_parse_scope_enum* scope_enum;


/* Line 1676 of yacc.c  */
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


/* Line 1676 of yacc.c  */
#line 147 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_spec* type_spec;
    struct ctf_parse_struct_spec* struct_spec;
    struct ctf_parse_variant_spec* variant_spec;
    struct ctf_parse_enum_spec* enum_spec;
    struct ctf_parse_type_spec_id* type_spec_id;
    struct ctf_parse_int_spec* int_spec;


/* Line 1676 of yacc.c  */
#line 162 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_enum_value* enum_value;
    struct ctf_parse_enum_value_simple* enum_value_simple;
    struct ctf_parse_enum_value_presize* enum_value_presize;
    struct ctf_parse_enum_value_range* enum_value_range;


/* Line 1676 of yacc.c  */
#line 174 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_post_mod* type_post_mod;
    struct ctf_parse_type_post_mod_array* type_post_mod_array;
    struct ctf_parse_type_post_mod_sequence* type_post_mod_sequence;


/* Line 1676 of yacc.c  */
#line 183 "/home/andrew/kernel-strider/sources/utils/ctf_reader/ctf_meta_constructor/ctf_meta_parser.y"

    struct ctf_parse_type_post_mod_list* type_post_mod_list;



/* Line 1676 of yacc.c  */
#line 133 "ctf_meta_parser.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;

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

extern YYLTYPE yylloc;

