
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

/* Substitute the variable and function names.  */
#define yyparse         plpgsql_yyparse
#define yylex           plpgsql_yylex
#define yyerror         plpgsql_yyerror
#define yylval          plpgsql_yylval
#define yychar          plpgsql_yychar
#define yydebug         plpgsql_yydebug
#define yynerrs         plpgsql_yynerrs
#define yylloc          plpgsql_yylloc

/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "gram.y"

/*-------------------------------------------------------------------------
 *
 * gram.y				- Parser for the PL/pgSQL procedural language
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/pl/plpgsql/src/gram.y,v 1.143.2.1 2010/08/19 18:58:04 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "plpgsql.h"

#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "parser/parser.h"
#include "parser/parse_type.h"
#include "parser/scanner.h"
#include "parser/scansup.h"


/* Location tracking support --- simpler than bison's default */
#define YYLLOC_DEFAULT(Current, Rhs, N) \
	do { \
		if (N) \
			(Current) = (Rhs)[1]; \
		else \
			(Current) = (Rhs)[0]; \
	} while (0)

/*
 * Bison doesn't allocate anything that needs to live across parser calls,
 * so we can easily have it use palloc instead of malloc.  This prevents
 * memory leaks if we error out during parsing.  Note this only works with
 * bison >= 2.0.  However, in bison 1.875 the default is to use alloca()
 * if possible, so there's not really much problem anyhow, at least if
 * you're building with gcc.
 */
#define YYMALLOC palloc
#define YYFREE   pfree


typedef struct
{
	int			location;
	int			leaderlen;
} sql_error_callback_arg;

#define parser_errposition(pos)  plpgsql_scanner_errposition(pos)

union YYSTYPE;					/* need forward reference for tok_is_keyword */

static	bool			tok_is_keyword(int token, union YYSTYPE *lval,
									   int kw_token, const char *kw_str);
static	void			word_is_not_variable(PLword *word, int location);
static	void			cword_is_not_variable(PLcword *cword, int location);
static	void			current_token_is_not_variable(int tok);
static	PLpgSQL_expr	*read_sql_construct(int until,
											int until2,
											int until3,
											const char *expected,
											const char *sqlstart,
											bool isexpression,
											bool valid_sql,
											int *startloc,
											int *endtoken);
static	PLpgSQL_expr	*read_sql_expression(int until,
											 const char *expected);
static	PLpgSQL_expr	*read_sql_expression2(int until, int until2,
											  const char *expected,
											  int *endtoken);
static	PLpgSQL_expr	*read_sql_stmt(const char *sqlstart);
static	PLpgSQL_type	*read_datatype(int tok);
static	PLpgSQL_stmt	*make_execsql_stmt(int firsttoken, int location);
static	PLpgSQL_stmt_fetch *read_fetch_direction(void);
static	void			 complete_direction(PLpgSQL_stmt_fetch *fetch,
											bool *check_FROM);
static	PLpgSQL_stmt	*make_return_stmt(int location);
static	PLpgSQL_stmt	*make_return_next_stmt(int location);
static	PLpgSQL_stmt	*make_return_query_stmt(int location);
static  PLpgSQL_stmt 	*make_case(int location, PLpgSQL_expr *t_expr,
								   List *case_when_list, List *else_stmts);
static	char			*NameOfDatum(PLwdatum *wdatum);
static	void			 check_assignable(PLpgSQL_datum *datum, int location);
static	void			 read_into_target(PLpgSQL_rec **rec, PLpgSQL_row **row,
										  bool *strict);
static	PLpgSQL_row		*read_into_scalar_list(char *initial_name,
											   PLpgSQL_datum *initial_datum,
											   int initial_location);
static	PLpgSQL_row		*make_scalar_list1(char *initial_name,
										   PLpgSQL_datum *initial_datum,
										   int lineno, int location);
static	void			 check_sql_expr(const char *stmt, int location,
										int leaderlen);
static	void			 plpgsql_sql_error_callback(void *arg);
static	PLpgSQL_type	*parse_datatype(const char *string, int location);
static	void			 check_labels(const char *start_label,
									  const char *end_label,
									  int end_location);
static	PLpgSQL_expr 	*read_cursor_args(PLpgSQL_var *cursor,
										  int until, const char *expected);
static	List			*read_raise_options(void);



/* Line 189 of yacc.c  */
#line 191 "pl_gram.c"

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
     IDENT = 258,
     FCONST = 259,
     SCONST = 260,
     BCONST = 261,
     XCONST = 262,
     Op = 263,
     ICONST = 264,
     PARAM = 265,
     TYPECAST = 266,
     DOT_DOT = 267,
     COLON_EQUALS = 268,
     T_WORD = 269,
     T_CWORD = 270,
     T_DATUM = 271,
     LESS_LESS = 272,
     GREATER_GREATER = 273,
     K_ABSOLUTE = 274,
     K_ALIAS = 275,
     K_ALL = 276,
     K_BACKWARD = 277,
     K_BEGIN = 278,
     K_BY = 279,
     K_CASE = 280,
     K_CLOSE = 281,
     K_CONSTANT = 282,
     K_CONTINUE = 283,
     K_CURSOR = 284,
     K_DEBUG = 285,
     K_DECLARE = 286,
     K_DEFAULT = 287,
     K_DETAIL = 288,
     K_DIAGNOSTICS = 289,
     K_DUMP = 290,
     K_ELSE = 291,
     K_ELSIF = 292,
     K_END = 293,
     K_ERRCODE = 294,
     K_ERROR = 295,
     K_EXCEPTION = 296,
     K_EXECUTE = 297,
     K_EXIT = 298,
     K_FETCH = 299,
     K_FIRST = 300,
     K_FOR = 301,
     K_FORWARD = 302,
     K_FROM = 303,
     K_GET = 304,
     K_HINT = 305,
     K_IF = 306,
     K_IN = 307,
     K_INFO = 308,
     K_INSERT = 309,
     K_INTO = 310,
     K_IS = 311,
     K_LAST = 312,
     K_LOG = 313,
     K_LOOP = 314,
     K_MESSAGE = 315,
     K_MOVE = 316,
     K_NEXT = 317,
     K_NO = 318,
     K_NOT = 319,
     K_NOTICE = 320,
     K_NULL = 321,
     K_OPEN = 322,
     K_OPTION = 323,
     K_OR = 324,
     K_PERFORM = 325,
     K_PRIOR = 326,
     K_QUERY = 327,
     K_RAISE = 328,
     K_RELATIVE = 329,
     K_RESULT_OID = 330,
     K_RETURN = 331,
     K_REVERSE = 332,
     K_ROWTYPE = 333,
     K_ROW_COUNT = 334,
     K_SCROLL = 335,
     K_SQLSTATE = 336,
     K_STRICT = 337,
     K_THEN = 338,
     K_TO = 339,
     K_TYPE = 340,
     K_USE_COLUMN = 341,
     K_USE_VARIABLE = 342,
     K_USING = 343,
     K_VARIABLE_CONFLICT = 344,
     K_WARNING = 345,
     K_WHEN = 346,
     K_WHILE = 347
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 214 of yacc.c  */
#line 114 "gram.y"

		core_YYSTYPE			core_yystype;
		/* these fields must match core_YYSTYPE: */
		int						ival;
		char					*str;
		const char				*keyword;

		PLword					word;
		PLcword					cword;
		PLwdatum				wdatum;
		bool					boolean;
		struct
		{
			char *name;
			int  lineno;
		}						varname;
		struct
		{
			char *name;
			int  lineno;
			PLpgSQL_datum   *scalar;
			PLpgSQL_rec     *rec;
			PLpgSQL_row     *row;
		}						forvariable;
		struct
		{
			char *label;
			int  n_initvars;
			int  *initvarnos;
		}						declhdr;
		struct
		{
			List *stmts;
			char *end_label;
			int   end_label_location;
		}						loop_body;
		List					*list;
		PLpgSQL_type			*dtype;
		PLpgSQL_datum			*datum;
		PLpgSQL_var				*var;
		PLpgSQL_expr			*expr;
		PLpgSQL_stmt			*stmt;
		PLpgSQL_condition		*condition;
		PLpgSQL_exception		*exception;
		PLpgSQL_exception_block	*exception_block;
		PLpgSQL_nsitem			*nsitem;
		PLpgSQL_diag_item		*diagitem;
		PLpgSQL_stmt_fetch		*fetch;
		PLpgSQL_case_when		*casewhen;



/* Line 214 of yacc.c  */
#line 372 "pl_gram.c"
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
#line 397 "pl_gram.c"

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   479

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  100
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  78
/* YYNRULES -- Number of rules.  */
#define YYNRULES  179
/* YYNRULES -- Number of states.  */
#define YYNSTATES  250

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   347

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    93,     2,     2,     2,     2,
      95,    96,     2,     2,    97,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    94,
       2,    98,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    99,     2,     2,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     7,     8,    11,    15,    19,    23,    27,
      28,    30,    37,    39,    42,    46,    48,    51,    53,    55,
      57,    61,    67,    73,    74,    82,    83,    86,    88,    89,
      90,    94,    96,   100,   103,   105,   107,   109,   111,   113,
     115,   116,   118,   119,   120,   123,   125,   127,   129,   131,
     133,   135,   136,   138,   141,   143,   146,   148,   150,   152,
     154,   156,   158,   160,   162,   164,   166,   168,   170,   172,
     174,   176,   178,   180,   182,   185,   189,   194,   198,   200,
     204,   205,   207,   209,   211,   213,   217,   225,   226,   231,
     234,   242,   243,   246,   248,   252,   253,   256,   260,   265,
     270,   273,   275,   277,   279,   283,   285,   287,   289,   291,
     297,   299,   301,   303,   305,   308,   313,   318,   319,   323,
     326,   328,   330,   332,   333,   334,   338,   341,   343,   348,
     352,   354,   356,   357,   358,   359,   360,   361,   365,   366,
     368,   370,   373,   375,   377,   379,   381,   383,   385,   387,
     389,   391,   393,   395,   397,   399,   401,   403,   405,   407,
     409,   411,   413,   415,   417,   419,   421,   423,   425,   427,
     429,   431,   433,   435,   437,   439,   441,   443,   445,   447
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     101,     0,    -1,   102,   105,   104,    -1,    -1,   102,   103,
      -1,    93,    68,    35,    -1,    93,    89,    40,    -1,    93,
      89,    87,    -1,    93,    89,    86,    -1,    -1,    94,    -1,
     106,    23,   126,   163,    38,   174,    -1,   173,    -1,   173,
     107,    -1,   173,   107,   108,    -1,    31,    -1,   108,   109,
      -1,   109,    -1,   110,    -1,    31,    -1,    17,   176,    18,
      -1,   119,   120,   121,   122,   123,    -1,   119,    20,    46,
     118,    94,    -1,    -1,   119,   112,    29,   111,   114,   117,
     113,    -1,    -1,    63,    80,    -1,    80,    -1,    -1,    -1,
      95,   115,    96,    -1,   116,    -1,   115,    97,   116,    -1,
     119,   121,    -1,    56,    -1,    46,    -1,    14,    -1,    15,
      -1,    14,    -1,   177,    -1,    -1,    27,    -1,    -1,    -1,
      64,    66,    -1,    94,    -1,   124,    -1,   125,    -1,    32,
      -1,    98,    -1,    13,    -1,    -1,   127,    -1,   127,   128,
      -1,   128,    -1,   105,    94,    -1,   130,    -1,   137,    -1,
     139,    -1,   144,    -1,   145,    -1,   146,    -1,   149,    -1,
     151,    -1,   152,    -1,   154,    -1,   155,    -1,   129,    -1,
     131,    -1,   156,    -1,   157,    -1,   158,    -1,   160,    -1,
     161,    -1,    70,   169,    -1,   136,   125,   169,    -1,    49,
      34,   132,    94,    -1,   132,    97,   133,    -1,   133,    -1,
     135,   125,   134,    -1,    -1,    16,    -1,    14,    -1,    15,
      -1,    16,    -1,   136,    99,   170,    -1,    51,   171,   126,
     138,    38,    51,    94,    -1,    -1,    37,   171,   126,   138,
      -1,    36,   126,    -1,    25,   140,   141,   143,    38,    25,
      94,    -1,    -1,   141,   142,    -1,   142,    -1,    91,   171,
     126,    -1,    -1,    36,   126,    -1,   173,    59,   153,    -1,
     173,    92,   172,   153,    -1,   173,    46,   147,   153,    -1,
     148,    52,    -1,    16,    -1,    14,    -1,    15,    -1,   150,
     174,   175,    -1,    43,    -1,    28,    -1,    76,    -1,    73,
      -1,   126,    38,    59,   174,    94,    -1,    54,    -1,    14,
      -1,    15,    -1,    42,    -1,    67,   162,    -1,    44,   159,
     162,    55,    -1,    61,   159,   162,    94,    -1,    -1,    26,
     162,    94,    -1,    66,    94,    -1,    16,    -1,    14,    -1,
      15,    -1,    -1,    -1,    41,   164,   165,    -1,   165,   166,
      -1,   166,    -1,    91,   167,    83,   126,    -1,   167,    69,
     168,    -1,   168,    -1,   176,    -1,    -1,    -1,    -1,    -1,
      -1,    17,   176,    18,    -1,    -1,   176,    -1,    94,    -1,
      91,   169,    -1,    14,    -1,    16,    -1,    19,    -1,    20,
      -1,    22,    -1,    27,    -1,    29,    -1,    30,    -1,    33,
      -1,    35,    -1,    39,    -1,    40,    -1,    45,    -1,    47,
      -1,    50,    -1,    53,    -1,    56,    -1,    57,    -1,    58,
      -1,    60,    -1,    62,    -1,    63,    -1,    65,    -1,    68,
      -1,    71,    -1,    72,    -1,    74,    -1,    75,    -1,    77,
      -1,    79,    -1,    78,    -1,    80,    -1,    81,    -1,    85,
      -1,    86,    -1,    87,    -1,    89,    -1,    90,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   316,   316,   322,   323,   326,   330,   334,   338,   344,
     345,   348,   370,   378,   385,   394,   406,   407,   410,   411,
     415,   428,   466,   472,   471,   523,   526,   530,   537,   543,
     546,   575,   579,   585,   593,   594,   596,   611,   639,   652,
     668,   669,   674,   685,   686,   690,   692,   698,   699,   702,
     703,   707,   708,   712,   719,   728,   730,   732,   734,   736,
     738,   740,   742,   744,   746,   748,   750,   752,   754,   756,
     758,   760,   762,   764,   768,   781,   795,   808,   812,   818,
     831,   845,   857,   862,   870,   875,   890,   906,   909,   939,
     945,   952,   966,   970,   976,   988,   991,  1006,  1023,  1041,
    1075,  1335,  1367,  1382,  1389,  1404,  1408,  1414,  1440,  1579,
    1597,  1601,  1611,  1623,  1687,  1764,  1796,  1809,  1814,  1827,
    1834,  1850,  1855,  1863,  1865,  1864,  1900,  1904,  1910,  1923,
    1932,  1938,  1975,  1979,  1983,  1987,  1991,  1995,  2003,  2006,
    2014,  2016,  2023,  2027,  2036,  2037,  2038,  2039,  2040,  2041,
    2042,  2043,  2044,  2045,  2046,  2047,  2048,  2049,  2050,  2051,
    2052,  2053,  2054,  2055,  2056,  2057,  2058,  2059,  2060,  2061,
    2062,  2063,  2064,  2065,  2066,  2067,  2068,  2069,  2070,  2071
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "IDENT", "FCONST", "SCONST", "BCONST",
  "XCONST", "Op", "ICONST", "PARAM", "TYPECAST", "DOT_DOT", "COLON_EQUALS",
  "T_WORD", "T_CWORD", "T_DATUM", "LESS_LESS", "GREATER_GREATER",
  "K_ABSOLUTE", "K_ALIAS", "K_ALL", "K_BACKWARD", "K_BEGIN", "K_BY",
  "K_CASE", "K_CLOSE", "K_CONSTANT", "K_CONTINUE", "K_CURSOR", "K_DEBUG",
  "K_DECLARE", "K_DEFAULT", "K_DETAIL", "K_DIAGNOSTICS", "K_DUMP",
  "K_ELSE", "K_ELSIF", "K_END", "K_ERRCODE", "K_ERROR", "K_EXCEPTION",
  "K_EXECUTE", "K_EXIT", "K_FETCH", "K_FIRST", "K_FOR", "K_FORWARD",
  "K_FROM", "K_GET", "K_HINT", "K_IF", "K_IN", "K_INFO", "K_INSERT",
  "K_INTO", "K_IS", "K_LAST", "K_LOG", "K_LOOP", "K_MESSAGE", "K_MOVE",
  "K_NEXT", "K_NO", "K_NOT", "K_NOTICE", "K_NULL", "K_OPEN", "K_OPTION",
  "K_OR", "K_PERFORM", "K_PRIOR", "K_QUERY", "K_RAISE", "K_RELATIVE",
  "K_RESULT_OID", "K_RETURN", "K_REVERSE", "K_ROWTYPE", "K_ROW_COUNT",
  "K_SCROLL", "K_SQLSTATE", "K_STRICT", "K_THEN", "K_TO", "K_TYPE",
  "K_USE_COLUMN", "K_USE_VARIABLE", "K_USING", "K_VARIABLE_CONFLICT",
  "K_WARNING", "K_WHEN", "K_WHILE", "'#'", "';'", "'('", "')'", "','",
  "'='", "'['", "$accept", "pl_function", "comp_options", "comp_option",
  "opt_semi", "pl_block", "decl_sect", "decl_start", "decl_stmts",
  "decl_stmt", "decl_statement", "$@1", "opt_scrollable",
  "decl_cursor_query", "decl_cursor_args", "decl_cursor_arglist",
  "decl_cursor_arg", "decl_is_for", "decl_aliasitem", "decl_varname",
  "decl_const", "decl_datatype", "decl_notnull", "decl_defval",
  "decl_defkey", "assign_operator", "proc_sect", "proc_stmts", "proc_stmt",
  "stmt_perform", "stmt_assign", "stmt_getdiag", "getdiag_list",
  "getdiag_list_item", "getdiag_item", "getdiag_target", "assign_var",
  "stmt_if", "stmt_else", "stmt_case", "opt_expr_until_when",
  "case_when_list", "case_when", "opt_case_else", "stmt_loop",
  "stmt_while", "stmt_for", "for_control", "for_variable", "stmt_exit",
  "exit_type", "stmt_return", "stmt_raise", "loop_body", "stmt_execsql",
  "stmt_dynexecute", "stmt_open", "stmt_fetch", "stmt_move",
  "opt_fetch_direction", "stmt_close", "stmt_null", "cursor_variable",
  "exception_sect", "@2", "proc_exceptions", "proc_exception",
  "proc_conditions", "proc_condition", "expr_until_semi",
  "expr_until_rightbracket", "expr_until_then", "expr_until_loop",
  "opt_block_label", "opt_label", "opt_exitcond", "any_identifier",
  "unreserved_keyword", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,    35,    59,    40,    41,    44,    61,    91
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   100,   101,   102,   102,   103,   103,   103,   103,   104,
     104,   105,   106,   106,   106,   107,   108,   108,   109,   109,
     109,   110,   110,   111,   110,   112,   112,   112,   113,   114,
     114,   115,   115,   116,   117,   117,   118,   118,   119,   119,
     120,   120,   121,   122,   122,   123,   123,   124,   124,   125,
     125,   126,   126,   127,   127,   128,   128,   128,   128,   128,
     128,   128,   128,   128,   128,   128,   128,   128,   128,   128,
     128,   128,   128,   128,   129,   130,   131,   132,   132,   133,
     134,   135,   135,   135,   136,   136,   137,   138,   138,   138,
     139,   140,   141,   141,   142,   143,   143,   144,   145,   146,
     147,   148,   148,   148,   149,   150,   150,   151,   152,   153,
     154,   154,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   162,   162,   163,   164,   163,   165,   165,   166,   167,
     167,   168,   169,   170,   171,   172,   173,   173,   174,   174,
     175,   175,   176,   176,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     3,     0,     2,     3,     3,     3,     3,     0,
       1,     6,     1,     2,     3,     1,     2,     1,     1,     1,
       3,     5,     5,     0,     7,     0,     2,     1,     0,     0,
       3,     1,     3,     2,     1,     1,     1,     1,     1,     1,
       0,     1,     0,     0,     2,     1,     1,     1,     1,     1,
       1,     0,     1,     2,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     4,     3,     1,     3,
       0,     1,     1,     1,     1,     3,     7,     0,     4,     2,
       7,     0,     2,     1,     3,     0,     2,     3,     4,     4,
       2,     1,     1,     1,     3,     1,     1,     1,     1,     5,
       1,     1,     1,     1,     2,     4,     4,     0,     3,     2,
       1,     1,     1,     0,     0,     3,     2,     1,     4,     3,
       1,     1,     0,     0,     0,     0,     0,     3,     0,     1,
       1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,   136,     1,     0,     0,     4,     9,     0,    12,
     142,   143,     0,     0,     0,    10,     2,   136,    15,    13,
     137,     5,     6,     8,     7,   111,   112,    84,    91,     0,
     106,   113,   105,   117,     0,   134,   110,   117,     0,     0,
     132,   108,   107,     0,   123,    52,    54,    67,    56,    68,
       0,    57,    58,    59,    60,    61,    62,   138,    63,    64,
      65,    66,    69,    70,    71,    72,    73,    12,    38,     0,
     144,   145,   146,   147,   148,   149,    19,   150,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   172,   171,
     173,   174,   175,   176,   177,   178,   179,    14,    17,    18,
      40,    39,     0,   121,   122,   120,     0,     0,     0,   136,
       0,   119,   114,    74,    55,   124,     0,    53,    50,    49,
     133,   132,     0,   139,     0,   136,   135,     0,    16,     0,
      41,     0,    27,     0,    42,   134,    95,    93,   118,     0,
      82,    83,    81,     0,    78,     0,    87,     0,     0,   138,
      85,    75,   132,   140,   104,   102,   103,   101,   136,     0,
       0,    97,   136,    20,     0,    26,    23,    43,   136,   136,
      92,     0,   115,    76,     0,    80,   136,   134,     0,   116,
       0,   125,   127,    11,   141,    99,   100,     0,    98,    36,
      37,     0,    29,     0,     0,    94,    96,     0,    77,    79,
      89,   136,     0,     0,   130,   131,   126,   138,    22,     0,
       0,    44,    48,    45,    21,    46,    47,     0,    87,     0,
       0,   136,     0,     0,    31,    42,    35,    34,    28,    90,
      88,    86,   129,   128,   109,    30,     0,    33,    24,    32
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     6,    16,    43,     8,    19,   107,   108,
     109,   202,   143,   248,   220,   233,   234,   238,   201,   110,
     144,   177,   204,   224,   225,   131,   170,    45,    46,    47,
      48,    49,   153,   154,   209,   155,    50,    51,   188,    52,
     112,   146,   147,   181,    53,    54,    55,   168,   169,    56,
      57,    58,    59,   171,    60,    61,    62,    63,    64,   117,
      65,    66,   116,   126,   158,   191,   192,   213,   214,   123,
     160,   119,   172,    67,   132,   164,   133,   111
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -190
static const yytype_int16 yypact[] =
{
    -190,    44,   -10,  -190,    36,   -62,  -190,   -35,    43,    47,
    -190,  -190,    64,    56,    -7,  -190,  -190,   383,  -190,   189,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,    61,
    -190,  -190,  -190,  -190,    59,  -190,  -190,  -190,     7,    61,
    -190,  -190,  -190,     8,    63,    -6,  -190,  -190,  -190,  -190,
     -11,  -190,  -190,  -190,  -190,  -190,  -190,    36,  -190,  -190,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,   -18,  -190,    36,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,   189,  -190,  -190,
      29,  -190,    16,  -190,  -190,  -190,    19,    61,    84,   340,
      61,  -190,  -190,  -190,  -190,  -190,    76,  -190,  -190,  -190,
    -190,  -190,   -65,  -190,    96,   403,  -190,    97,  -190,    71,
    -190,    39,  -190,    91,  -190,  -190,   -20,  -190,  -190,    66,
    -190,  -190,  -190,   -55,  -190,    -8,    -2,    31,    35,    36,
    -190,  -190,  -190,  -190,  -190,  -190,  -190,  -190,   403,    75,
      90,  -190,   403,  -190,    48,  -190,  -190,    68,    80,   403,
    -190,    92,  -190,  -190,    84,  -190,   403,  -190,    95,  -190,
      36,    35,  -190,  -190,  -190,  -190,  -190,    77,  -190,  -190,
    -190,    41,    49,    79,    -9,  -190,  -190,   117,  -190,  -190,
    -190,   340,   101,   -51,  -190,  -190,  -190,    36,  -190,   263,
     -25,  -190,  -190,  -190,  -190,  -190,  -190,    60,    -2,    65,
      36,   123,    70,   -28,  -190,  -190,  -190,  -190,  -190,  -190,
    -190,  -190,  -190,  -190,  -190,  -190,   263,  -190,  -190,  -190
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -190,  -190,  -190,  -190,  -190,   141,  -190,  -190,  -190,    50,
    -190,  -190,  -190,  -190,  -190,  -190,   -91,  -190,  -190,  -189,
    -190,   -77,  -190,  -190,  -190,  -131,   -16,  -190,   115,  -190,
    -190,  -190,  -190,   -15,  -190,  -190,  -190,  -190,   -60,  -190,
    -190,  -190,    27,  -190,  -190,  -190,  -190,  -190,  -190,  -190,
    -190,  -190,  -190,  -121,  -190,  -190,  -190,  -190,  -190,   138,
    -190,  -190,   -36,  -190,  -190,  -190,   -13,  -190,   -54,  -116,
    -190,  -133,  -190,   177,  -145,  -190,    -4,  -190
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -137
static const yytype_int16 yytable[] =
{
      12,    44,   128,   122,   128,   128,    13,     4,    25,    26,
      27,     4,   178,    18,   193,   161,   179,  -136,   230,    28,
      29,   236,    30,   222,   185,  -136,   162,    14,   134,   163,
     235,   237,   231,    22,   186,   187,    31,    32,    33,   183,
    -136,   135,   184,    34,     3,    35,   194,   195,    36,   139,
      10,   198,    11,  -136,   211,    37,   140,   235,   -25,    15,
      38,    39,   199,   200,    40,   137,    17,    41,   245,   246,
      42,   145,   232,   226,   136,   113,   114,   115,    18,    23,
      24,   149,    20,     5,   157,   223,  -136,   129,   130,   129,
     129,    21,   141,   118,    25,    26,    27,     4,   150,   151,
     152,   121,   124,   156,   125,    28,    29,   145,    30,   142,
     165,   166,   167,   148,   159,   173,   -51,   174,   -51,   175,
     176,   182,    31,    32,    33,   189,   190,   196,   197,    34,
     207,    35,   203,   212,    36,   218,   217,    25,    26,    27,
       4,    37,   227,     7,   219,   221,    38,    39,    28,    29,
      40,    30,   229,    41,   239,   249,    42,   138,   247,   241,
     127,   -51,   205,   206,   244,    31,    32,    33,   240,   208,
     210,   -51,    34,   180,    35,   120,   242,    36,   216,     9,
       0,     0,     0,     0,    37,     0,   215,     0,     0,    38,
      39,     0,     0,    40,     0,   228,    41,     0,     0,    42,
       0,     0,     0,    68,     0,     0,    69,     0,    70,    71,
       0,    72,     0,     0,   -51,   243,    73,     0,    74,    75,
      76,     0,    77,     0,    78,     0,   215,     0,    79,    80,
       0,     0,     0,     0,    81,     0,    82,     0,     0,    83,
       0,     0,    84,     0,     0,    85,    86,    87,     0,    88,
       0,    89,    90,     0,    91,     0,     0,    92,     0,     0,
      93,    94,     0,    95,    96,     0,    97,    98,    99,   100,
     101,     0,     0,     0,   102,   103,   104,    68,   105,   106,
       0,     0,    70,    71,     0,    72,     0,     0,     0,     0,
      73,     0,    74,    75,     0,     0,    77,     0,    78,     0,
       0,     0,    79,    80,     0,     0,     0,     0,    81,     0,
      82,     0,     0,    83,     0,     0,    84,     0,     0,    85,
      86,    87,     0,    88,     0,    89,    90,     0,    91,     0,
       0,    92,     0,     0,    93,    94,     0,    95,    96,     0,
      97,    98,    99,   100,   101,     0,     0,     0,   102,   103,
     104,     0,   105,   106,    25,    26,    27,     4,     0,     0,
       0,     0,     0,     0,     0,    28,    29,     0,    30,     0,
       0,     0,     0,     0,     0,     0,   -51,   -51,   -51,     0,
       0,     0,    31,    32,    33,     0,     0,     0,     0,    34,
       0,    35,     0,     0,    36,     0,     0,    25,    26,    27,
       4,    37,     0,     0,     0,     0,    38,    39,    28,    29,
      40,    30,     0,    41,     0,     0,    42,    25,    26,    27,
       4,   -51,     0,     0,   -51,    31,    32,    33,    28,    29,
       0,    30,    34,     0,    35,     0,     0,    36,     0,     0,
       0,   -51,     0,     0,    37,    31,    32,    33,     0,    38,
      39,     0,    34,    40,    35,     0,    41,    36,     0,    42,
       0,     0,     0,     0,    37,     0,     0,     0,     0,    38,
      39,     0,     0,    40,     0,     0,    41,     0,     0,    42
};

static const yytype_int16 yycheck[] =
{
       4,    17,    13,    39,    13,    13,    68,    17,    14,    15,
      16,    17,   145,    31,   159,   131,    36,    23,    69,    25,
      26,    46,    28,    32,   155,    31,    91,    89,    46,    94,
     219,    56,    83,    40,    36,    37,    42,    43,    44,    94,
      46,    59,    97,    49,     0,    51,   162,   168,    54,    20,
      14,   172,    16,    59,   187,    61,    27,   246,    29,    94,
      66,    67,    14,    15,    70,    69,    23,    73,    96,    97,
      76,    91,   217,   204,    92,    14,    15,    16,    31,    86,
      87,   117,    18,    93,   120,    94,    92,    98,    99,    98,
      98,    35,    63,    34,    14,    15,    16,    17,    14,    15,
      16,    94,    94,   119,    41,    25,    26,    91,    28,    80,
      14,    15,    16,    94,    38,    18,    36,    46,    38,    80,
      29,    55,    42,    43,    44,    94,    91,    52,    38,    49,
      38,    51,    64,    38,    54,    94,    59,    14,    15,    16,
      17,    61,    25,     2,    95,    66,    66,    67,    25,    26,
      70,    28,    51,    73,    94,   246,    76,   107,   235,    94,
      45,    38,   178,   179,    94,    42,    43,    44,   228,   184,
     186,    91,    49,   146,    51,    37,   230,    54,   191,     2,
      -1,    -1,    -1,    -1,    61,    -1,   190,    -1,    -1,    66,
      67,    -1,    -1,    70,    -1,   211,    73,    -1,    -1,    76,
      -1,    -1,    -1,    14,    -1,    -1,    17,    -1,    19,    20,
      -1,    22,    -1,    -1,    91,   231,    27,    -1,    29,    30,
      31,    -1,    33,    -1,    35,    -1,   230,    -1,    39,    40,
      -1,    -1,    -1,    -1,    45,    -1,    47,    -1,    -1,    50,
      -1,    -1,    53,    -1,    -1,    56,    57,    58,    -1,    60,
      -1,    62,    63,    -1,    65,    -1,    -1,    68,    -1,    -1,
      71,    72,    -1,    74,    75,    -1,    77,    78,    79,    80,
      81,    -1,    -1,    -1,    85,    86,    87,    14,    89,    90,
      -1,    -1,    19,    20,    -1,    22,    -1,    -1,    -1,    -1,
      27,    -1,    29,    30,    -1,    -1,    33,    -1,    35,    -1,
      -1,    -1,    39,    40,    -1,    -1,    -1,    -1,    45,    -1,
      47,    -1,    -1,    50,    -1,    -1,    53,    -1,    -1,    56,
      57,    58,    -1,    60,    -1,    62,    63,    -1,    65,    -1,
      -1,    68,    -1,    -1,    71,    72,    -1,    74,    75,    -1,
      77,    78,    79,    80,    81,    -1,    -1,    -1,    85,    86,
      87,    -1,    89,    90,    14,    15,    16,    17,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,    26,    -1,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    38,    -1,
      -1,    -1,    42,    43,    44,    -1,    -1,    -1,    -1,    49,
      -1,    51,    -1,    -1,    54,    -1,    -1,    14,    15,    16,
      17,    61,    -1,    -1,    -1,    -1,    66,    67,    25,    26,
      70,    28,    -1,    73,    -1,    -1,    76,    14,    15,    16,
      17,    38,    -1,    -1,    41,    42,    43,    44,    25,    26,
      -1,    28,    49,    -1,    51,    -1,    -1,    54,    -1,    -1,
      -1,    38,    -1,    -1,    61,    42,    43,    44,    -1,    66,
      67,    -1,    49,    70,    51,    -1,    73,    54,    -1,    76,
      -1,    -1,    -1,    -1,    61,    -1,    -1,    -1,    -1,    66,
      67,    -1,    -1,    70,    -1,    -1,    73,    -1,    -1,    76
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   101,   102,     0,    17,    93,   103,   105,   106,   173,
      14,    16,   176,    68,    89,    94,   104,    23,    31,   107,
      18,    35,    40,    86,    87,    14,    15,    16,    25,    26,
      28,    42,    43,    44,    49,    51,    54,    61,    66,    67,
      70,    73,    76,   105,   126,   127,   128,   129,   130,   131,
     136,   137,   139,   144,   145,   146,   149,   150,   151,   152,
     154,   155,   156,   157,   158,   160,   161,   173,    14,    17,
      19,    20,    22,    27,    29,    30,    31,    33,    35,    39,
      40,    45,    47,    50,    53,    56,    57,    58,    60,    62,
      63,    65,    68,    71,    72,    74,    75,    77,    78,    79,
      80,    81,    85,    86,    87,    89,    90,   108,   109,   110,
     119,   177,   140,    14,    15,    16,   162,   159,    34,   171,
     159,    94,   162,   169,    94,    41,   163,   128,    13,    98,
      99,   125,   174,   176,    46,    59,    92,   176,   109,    20,
      27,    63,    80,   112,   120,    91,   141,   142,    94,   162,
      14,    15,    16,   132,   133,   135,   126,   162,   164,    38,
     170,   169,    91,    94,   175,    14,    15,    16,   147,   148,
     126,   153,   172,    18,    46,    80,    29,   121,   171,    36,
     142,   143,    55,    94,    97,   125,    36,    37,   138,    94,
      91,   165,   166,   174,   169,   153,    52,    38,   153,    14,
      15,   118,   111,    64,   122,   126,   126,    38,   133,   134,
     126,   171,    38,   167,   168,   176,   166,    59,    94,    95,
     114,    66,    32,    94,   123,   124,   125,    25,   126,    51,
      69,    83,   174,   115,   116,   119,    46,    56,   117,    94,
     138,    94,   168,   126,    94,    96,    97,   121,   113,   116
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
      yyerror (YY_("syntax error: cannot back up")); \
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
# define YYLEX yylex ()
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
		  Type, Value, Location); \
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp);
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
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
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
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule); \
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

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
int yyparse (void);
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
yyparse (void)
#else
int
yyparse ()

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
        case 2:

/* Line 1455 of yacc.c  */
#line 317 "gram.y"
    {
						plpgsql_parse_result = (PLpgSQL_stmt_block *) (yyvsp[(2) - (3)].stmt);
					;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 327 "gram.y"
    {
						plpgsql_DumpExecTree = true;
					;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 331 "gram.y"
    {
						plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_ERROR;
					;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 335 "gram.y"
    {
						plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_VARIABLE;
					;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 339 "gram.y"
    {
						plpgsql_curr_compile->resolve_option = PLPGSQL_RESOLVE_COLUMN;
					;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 349 "gram.y"
    {
						PLpgSQL_stmt_block *new;

						new = palloc0(sizeof(PLpgSQL_stmt_block));

						new->cmd_type	= PLPGSQL_STMT_BLOCK;
						new->lineno		= plpgsql_location_to_lineno((yylsp[(2) - (6)]));
						new->label		= (yyvsp[(1) - (6)].declhdr).label;
						new->n_initvars = (yyvsp[(1) - (6)].declhdr).n_initvars;
						new->initvarnos = (yyvsp[(1) - (6)].declhdr).initvarnos;
						new->body		= (yyvsp[(3) - (6)].list);
						new->exceptions	= (yyvsp[(4) - (6)].exception_block);

						check_labels((yyvsp[(1) - (6)].declhdr).label, (yyvsp[(6) - (6)].str), (yylsp[(6) - (6)]));
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 371 "gram.y"
    {
						/* done with decls, so resume identifier lookup */
						plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
						(yyval.declhdr).label	  = (yyvsp[(1) - (1)].str);
						(yyval.declhdr).n_initvars = 0;
						(yyval.declhdr).initvarnos = NULL;
					;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 379 "gram.y"
    {
						plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
						(yyval.declhdr).label	  = (yyvsp[(1) - (2)].str);
						(yyval.declhdr).n_initvars = 0;
						(yyval.declhdr).initvarnos = NULL;
					;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 386 "gram.y"
    {
						plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
						(yyval.declhdr).label	  = (yyvsp[(1) - (3)].str);
						/* Remember variables declared in decl_stmts */
						(yyval.declhdr).n_initvars = plpgsql_add_initdatums(&((yyval.declhdr).initvarnos));
					;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 395 "gram.y"
    {
						/* Forget any variables created before block */
						plpgsql_add_initdatums(NULL);
						/*
						 * Disable scanner lookup of identifiers while
						 * we process the decl_stmts
						 */
						plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_DECLARE;
					;}
    break;

  case 19:

/* Line 1455 of yacc.c  */
#line 412 "gram.y"
    {
						/* We allow useless extra DECLAREs */
					;}
    break;

  case 20:

/* Line 1455 of yacc.c  */
#line 416 "gram.y"
    {
						/*
						 * Throw a helpful error if user tries to put block
						 * label just before BEGIN, instead of before DECLARE.
						 */
						ereport(ERROR,
								(errcode(ERRCODE_SYNTAX_ERROR),
								 errmsg("block label must be placed before DECLARE, not after"),
								 parser_errposition((yylsp[(1) - (3)]))));
					;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 429 "gram.y"
    {
						PLpgSQL_variable	*var;

						var = plpgsql_build_variable((yyvsp[(1) - (5)].varname).name, (yyvsp[(1) - (5)].varname).lineno,
													 (yyvsp[(3) - (5)].dtype), true);
						if ((yyvsp[(2) - (5)].boolean))
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->isconst = (yyvsp[(2) - (5)].boolean);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("row or record variable cannot be CONSTANT"),
										 parser_errposition((yylsp[(2) - (5)]))));
						}
						if ((yyvsp[(4) - (5)].boolean))
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->notnull = (yyvsp[(4) - (5)].boolean);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("row or record variable cannot be NOT NULL"),
										 parser_errposition((yylsp[(4) - (5)]))));

						}
						if ((yyvsp[(5) - (5)].expr) != NULL)
						{
							if (var->dtype == PLPGSQL_DTYPE_VAR)
								((PLpgSQL_var *) var)->default_val = (yyvsp[(5) - (5)].expr);
							else
								ereport(ERROR,
										(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
										 errmsg("default value for row or record variable is not supported"),
										 parser_errposition((yylsp[(5) - (5)]))));
						}
					;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 467 "gram.y"
    {
						plpgsql_ns_additem((yyvsp[(4) - (5)].nsitem)->itemtype,
										   (yyvsp[(4) - (5)].nsitem)->itemno, (yyvsp[(1) - (5)].varname).name);
					;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 472 "gram.y"
    { plpgsql_ns_push((yyvsp[(1) - (3)].varname).name); ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 474 "gram.y"
    {
						PLpgSQL_var *new;
						PLpgSQL_expr *curname_def;
						char		buf[1024];
						char		*cp1;
						char		*cp2;

						/* pop local namespace for cursor args */
						plpgsql_ns_pop();

						new = (PLpgSQL_var *)
							plpgsql_build_variable((yyvsp[(1) - (7)].varname).name, (yyvsp[(1) - (7)].varname).lineno,
												   plpgsql_build_datatype(REFCURSOROID,
																		  -1),
												   true);

						curname_def = palloc0(sizeof(PLpgSQL_expr));

						curname_def->dtype = PLPGSQL_DTYPE_EXPR;
						strcpy(buf, "SELECT ");
						cp1 = new->refname;
						cp2 = buf + strlen(buf);
						/*
						 * Don't trust standard_conforming_strings here;
						 * it might change before we use the string.
						 */
						if (strchr(cp1, '\\') != NULL)
							*cp2++ = ESCAPE_STRING_SYNTAX;
						*cp2++ = '\'';
						while (*cp1)
						{
							if (SQL_STR_DOUBLE(*cp1, true))
								*cp2++ = *cp1;
							*cp2++ = *cp1++;
						}
						strcpy(cp2, "'::pg_catalog.refcursor");
						curname_def->query = pstrdup(buf);
						new->default_val = curname_def;

						new->cursor_explicit_expr = (yyvsp[(7) - (7)].expr);
						if ((yyvsp[(5) - (7)].datum) == NULL)
							new->cursor_explicit_argrow = -1;
						else
							new->cursor_explicit_argrow = (yyvsp[(5) - (7)].datum)->dno;
						new->cursor_options = CURSOR_OPT_FAST_PLAN | (yyvsp[(2) - (7)].ival);
					;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 523 "gram.y"
    {
						(yyval.ival) = 0;
					;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 527 "gram.y"
    {
						(yyval.ival) = CURSOR_OPT_NO_SCROLL;
					;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 531 "gram.y"
    {
						(yyval.ival) = CURSOR_OPT_SCROLL;
					;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 537 "gram.y"
    {
						(yyval.expr) = read_sql_stmt("");
					;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 543 "gram.y"
    {
						(yyval.datum) = NULL;
					;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 547 "gram.y"
    {
						PLpgSQL_row *new;
						int i;
						ListCell *l;

						new = palloc0(sizeof(PLpgSQL_row));
						new->dtype = PLPGSQL_DTYPE_ROW;
						new->lineno = plpgsql_location_to_lineno((yylsp[(1) - (3)]));
						new->rowtupdesc = NULL;
						new->nfields = list_length((yyvsp[(2) - (3)].list));
						new->fieldnames = palloc(new->nfields * sizeof(char *));
						new->varnos = palloc(new->nfields * sizeof(int));

						i = 0;
						foreach (l, (yyvsp[(2) - (3)].list))
						{
							PLpgSQL_variable *arg = (PLpgSQL_variable *) lfirst(l);
							new->fieldnames[i] = arg->refname;
							new->varnos[i] = arg->dno;
							i++;
						}
						list_free((yyvsp[(2) - (3)].list));

						plpgsql_adddatum((PLpgSQL_datum *) new);
						(yyval.datum) = (PLpgSQL_datum *) new;
					;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 576 "gram.y"
    {
						(yyval.list) = list_make1((yyvsp[(1) - (1)].datum));
					;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 580 "gram.y"
    {
						(yyval.list) = lappend((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].datum));
					;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 586 "gram.y"
    {
						(yyval.datum) = (PLpgSQL_datum *)
							plpgsql_build_variable((yyvsp[(1) - (2)].varname).name, (yyvsp[(1) - (2)].varname).lineno,
												   (yyvsp[(2) - (2)].dtype), true);
					;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 597 "gram.y"
    {
						PLpgSQL_nsitem *nsi;

						nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false,
												(yyvsp[(1) - (1)].word).ident, NULL, NULL,
												NULL);
						if (nsi == NULL)
							ereport(ERROR,
									(errcode(ERRCODE_UNDEFINED_OBJECT),
									 errmsg("variable \"%s\" does not exist",
											(yyvsp[(1) - (1)].word).ident),
									 parser_errposition((yylsp[(1) - (1)]))));
						(yyval.nsitem) = nsi;
					;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 612 "gram.y"
    {
						PLpgSQL_nsitem *nsi;

						if (list_length((yyvsp[(1) - (1)].cword).idents) == 2)
							nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false,
													strVal(linitial((yyvsp[(1) - (1)].cword).idents)),
													strVal(lsecond((yyvsp[(1) - (1)].cword).idents)),
													NULL,
													NULL);
						else if (list_length((yyvsp[(1) - (1)].cword).idents) == 3)
							nsi = plpgsql_ns_lookup(plpgsql_ns_top(), false,
													strVal(linitial((yyvsp[(1) - (1)].cword).idents)),
													strVal(lsecond((yyvsp[(1) - (1)].cword).idents)),
													strVal(lthird((yyvsp[(1) - (1)].cword).idents)),
													NULL);
						else
							nsi = NULL;
						if (nsi == NULL)
							ereport(ERROR,
									(errcode(ERRCODE_UNDEFINED_OBJECT),
									 errmsg("variable \"%s\" does not exist",
											NameListToString((yyvsp[(1) - (1)].cword).idents)),
									 parser_errposition((yylsp[(1) - (1)]))));
						(yyval.nsitem) = nsi;
					;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 640 "gram.y"
    {
						(yyval.varname).name = (yyvsp[(1) - (1)].word).ident;
						(yyval.varname).lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						/*
						 * Check to make sure name isn't already declared
						 * in the current block.
						 */
						if (plpgsql_ns_lookup(plpgsql_ns_top(), true,
											  (yyvsp[(1) - (1)].word).ident, NULL, NULL,
											  NULL) != NULL)
							yyerror("duplicate declaration");
					;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 653 "gram.y"
    {
						(yyval.varname).name = pstrdup((yyvsp[(1) - (1)].keyword));
						(yyval.varname).lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						/*
						 * Check to make sure name isn't already declared
						 * in the current block.
						 */
						if (plpgsql_ns_lookup(plpgsql_ns_top(), true,
											  (yyvsp[(1) - (1)].keyword), NULL, NULL,
											  NULL) != NULL)
							yyerror("duplicate declaration");
					;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 668 "gram.y"
    { (yyval.boolean) = false; ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 670 "gram.y"
    { (yyval.boolean) = true; ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 674 "gram.y"
    {
						/*
						 * If there's a lookahead token, read_datatype
						 * should consume it.
						 */
						(yyval.dtype) = read_datatype(yychar);
						yyclearin;
					;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 685 "gram.y"
    { (yyval.boolean) = false; ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 687 "gram.y"
    { (yyval.boolean) = true; ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 691 "gram.y"
    { (yyval.expr) = NULL; ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 693 "gram.y"
    {
						(yyval.expr) = read_sql_expression(';', ";");
					;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 707 "gram.y"
    { (yyval.list) = NIL; ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 709 "gram.y"
    { (yyval.list) = (yyvsp[(1) - (1)].list); ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 713 "gram.y"
    {
							if ((yyvsp[(2) - (2)].stmt) == NULL)
								(yyval.list) = (yyvsp[(1) - (2)].list);
							else
								(yyval.list) = lappend((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].stmt));
						;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 720 "gram.y"
    {
							if ((yyvsp[(1) - (1)].stmt) == NULL)
								(yyval.list) = NIL;
							else
								(yyval.list) = list_make1((yyvsp[(1) - (1)].stmt));
						;}
    break;

  case 55:

/* Line 1455 of yacc.c  */
#line 729 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (2)].stmt); ;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 731 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 57:

/* Line 1455 of yacc.c  */
#line 733 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 735 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 59:

/* Line 1455 of yacc.c  */
#line 737 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 739 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 61:

/* Line 1455 of yacc.c  */
#line 741 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 743 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 63:

/* Line 1455 of yacc.c  */
#line 745 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 747 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 65:

/* Line 1455 of yacc.c  */
#line 749 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 751 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 753 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 68:

/* Line 1455 of yacc.c  */
#line 755 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 757 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 759 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 761 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 763 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 765 "gram.y"
    { (yyval.stmt) = (yyvsp[(1) - (1)].stmt); ;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 769 "gram.y"
    {
						PLpgSQL_stmt_perform *new;

						new = palloc0(sizeof(PLpgSQL_stmt_perform));
						new->cmd_type = PLPGSQL_STMT_PERFORM;
						new->lineno   = plpgsql_location_to_lineno((yylsp[(1) - (2)]));
						new->expr  = (yyvsp[(2) - (2)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 782 "gram.y"
    {
						PLpgSQL_stmt_assign *new;

						new = palloc0(sizeof(PLpgSQL_stmt_assign));
						new->cmd_type = PLPGSQL_STMT_ASSIGN;
						new->lineno   = plpgsql_location_to_lineno((yylsp[(1) - (3)]));
						new->varno = (yyvsp[(1) - (3)].ival);
						new->expr  = (yyvsp[(3) - (3)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 796 "gram.y"
    {
						PLpgSQL_stmt_getdiag	 *new;

						new = palloc0(sizeof(PLpgSQL_stmt_getdiag));
						new->cmd_type = PLPGSQL_STMT_GETDIAG;
						new->lineno   = plpgsql_location_to_lineno((yylsp[(1) - (4)]));
						new->diag_items  = (yyvsp[(3) - (4)].list);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 809 "gram.y"
    {
						(yyval.list) = lappend((yyvsp[(1) - (3)].list), (yyvsp[(3) - (3)].diagitem));
					;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 813 "gram.y"
    {
						(yyval.list) = list_make1((yyvsp[(1) - (1)].diagitem));
					;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 819 "gram.y"
    {
						PLpgSQL_diag_item *new;

						new = palloc(sizeof(PLpgSQL_diag_item));
						new->target = (yyvsp[(1) - (3)].ival);
						new->kind = (yyvsp[(3) - (3)].ival);

						(yyval.diagitem) = new;
					;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 831 "gram.y"
    {
						int	tok = yylex();

						if (tok_is_keyword(tok, &yylval,
										   K_ROW_COUNT, "row_count"))
							(yyval.ival) = PLPGSQL_GETDIAG_ROW_COUNT;
						else if (tok_is_keyword(tok, &yylval,
												K_RESULT_OID, "result_oid"))
							(yyval.ival) = PLPGSQL_GETDIAG_RESULT_OID;
						else
							yyerror("unrecognized GET DIAGNOSTICS item");
					;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 846 "gram.y"
    {
						check_assignable((yyvsp[(1) - (1)].wdatum).datum, (yylsp[(1) - (1)]));
						if ((yyvsp[(1) - (1)].wdatum).datum->dtype == PLPGSQL_DTYPE_ROW ||
							(yyvsp[(1) - (1)].wdatum).datum->dtype == PLPGSQL_DTYPE_REC)
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("\"%s\" is not a scalar variable",
											NameOfDatum(&((yyvsp[(1) - (1)].wdatum)))),
									 parser_errposition((yylsp[(1) - (1)]))));
						(yyval.ival) = (yyvsp[(1) - (1)].wdatum).datum->dno;
					;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 858 "gram.y"
    {
						/* just to give a better message than "syntax error" */
						word_is_not_variable(&((yyvsp[(1) - (1)].word)), (yylsp[(1) - (1)]));
					;}
    break;

  case 83:

/* Line 1455 of yacc.c  */
#line 863 "gram.y"
    {
						/* just to give a better message than "syntax error" */
						cword_is_not_variable(&((yyvsp[(1) - (1)].cword)), (yylsp[(1) - (1)]));
					;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 871 "gram.y"
    {
						check_assignable((yyvsp[(1) - (1)].wdatum).datum, (yylsp[(1) - (1)]));
						(yyval.ival) = (yyvsp[(1) - (1)].wdatum).datum->dno;
					;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 876 "gram.y"
    {
						PLpgSQL_arrayelem	*new;

						new = palloc0(sizeof(PLpgSQL_arrayelem));
						new->dtype		= PLPGSQL_DTYPE_ARRAYELEM;
						new->subscript	= (yyvsp[(3) - (3)].expr);
						new->arrayparentno = (yyvsp[(1) - (3)].ival);

						plpgsql_adddatum((PLpgSQL_datum *) new);

						(yyval.ival) = new->dno;
					;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 891 "gram.y"
    {
						PLpgSQL_stmt_if *new;

						new = palloc0(sizeof(PLpgSQL_stmt_if));
						new->cmd_type	= PLPGSQL_STMT_IF;
						new->lineno		= plpgsql_location_to_lineno((yylsp[(1) - (7)]));
						new->cond		= (yyvsp[(2) - (7)].expr);
						new->true_body	= (yyvsp[(3) - (7)].list);
						new->false_body = (yyvsp[(4) - (7)].list);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 87:

/* Line 1455 of yacc.c  */
#line 906 "gram.y"
    {
						(yyval.list) = NIL;
					;}
    break;

  case 88:

/* Line 1455 of yacc.c  */
#line 910 "gram.y"
    {
						/*----------
						 * Translate the structure:	   into:
						 *
						 * IF c1 THEN				   IF c1 THEN
						 *	 ...						   ...
						 * ELSIF c2 THEN			   ELSE
						 *								   IF c2 THEN
						 *	 ...							   ...
						 * ELSE							   ELSE
						 *	 ...							   ...
						 * END IF						   END IF
						 *							   END IF
						 *----------
						 */
						PLpgSQL_stmt_if *new_if;

						/* first create a new if-statement */
						new_if = palloc0(sizeof(PLpgSQL_stmt_if));
						new_if->cmd_type	= PLPGSQL_STMT_IF;
						new_if->lineno		= plpgsql_location_to_lineno((yylsp[(1) - (4)]));
						new_if->cond		= (yyvsp[(2) - (4)].expr);
						new_if->true_body	= (yyvsp[(3) - (4)].list);
						new_if->false_body	= (yyvsp[(4) - (4)].list);

						/* wrap the if-statement in a "container" list */
						(yyval.list) = list_make1(new_if);
					;}
    break;

  case 89:

/* Line 1455 of yacc.c  */
#line 940 "gram.y"
    {
						(yyval.list) = (yyvsp[(2) - (2)].list);
					;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 946 "gram.y"
    {
						(yyval.stmt) = make_case((yylsp[(1) - (7)]), (yyvsp[(2) - (7)].expr), (yyvsp[(3) - (7)].list), (yyvsp[(4) - (7)].list));
					;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 952 "gram.y"
    {
						PLpgSQL_expr *expr = NULL;
						int	tok = yylex();

						if (tok != K_WHEN)
						{
							plpgsql_push_back_token(tok);
							expr = read_sql_expression(K_WHEN, "WHEN");
						}
						plpgsql_push_back_token(K_WHEN);
						(yyval.expr) = expr;
					;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 967 "gram.y"
    {
						(yyval.list) = lappend((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].casewhen));
					;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 971 "gram.y"
    {
						(yyval.list) = list_make1((yyvsp[(1) - (1)].casewhen));
					;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 977 "gram.y"
    {
						PLpgSQL_case_when *new = palloc(sizeof(PLpgSQL_case_when));

						new->lineno	= plpgsql_location_to_lineno((yylsp[(1) - (3)]));
						new->expr	= (yyvsp[(2) - (3)].expr);
						new->stmts	= (yyvsp[(3) - (3)].list);
						(yyval.casewhen) = new;
					;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 988 "gram.y"
    {
						(yyval.list) = NIL;
					;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 992 "gram.y"
    {
						/*
						 * proc_sect could return an empty list, but we
						 * must distinguish that from not having ELSE at all.
						 * Simplest fix is to return a list with one NULL
						 * pointer, which make_case() must take care of.
						 */
						if ((yyvsp[(2) - (2)].list) != NIL)
							(yyval.list) = (yyvsp[(2) - (2)].list);
						else
							(yyval.list) = list_make1(NULL);
					;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 1007 "gram.y"
    {
						PLpgSQL_stmt_loop *new;

						new = palloc0(sizeof(PLpgSQL_stmt_loop));
						new->cmd_type = PLPGSQL_STMT_LOOP;
						new->lineno   = plpgsql_location_to_lineno((yylsp[(2) - (3)]));
						new->label	  = (yyvsp[(1) - (3)].str);
						new->body	  = (yyvsp[(3) - (3)].loop_body).stmts;

						check_labels((yyvsp[(1) - (3)].str), (yyvsp[(3) - (3)].loop_body).end_label, (yyvsp[(3) - (3)].loop_body).end_label_location);
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 1024 "gram.y"
    {
						PLpgSQL_stmt_while *new;

						new = palloc0(sizeof(PLpgSQL_stmt_while));
						new->cmd_type = PLPGSQL_STMT_WHILE;
						new->lineno   = plpgsql_location_to_lineno((yylsp[(2) - (4)]));
						new->label	  = (yyvsp[(1) - (4)].str);
						new->cond	  = (yyvsp[(3) - (4)].expr);
						new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;

						check_labels((yyvsp[(1) - (4)].str), (yyvsp[(4) - (4)].loop_body).end_label, (yyvsp[(4) - (4)].loop_body).end_label_location);
						plpgsql_ns_pop();

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 1042 "gram.y"
    {
						/* This runs after we've scanned the loop body */
						if ((yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_FORI)
						{
							PLpgSQL_stmt_fori		*new;

							new = (PLpgSQL_stmt_fori *) (yyvsp[(3) - (4)].stmt);
							new->lineno   = plpgsql_location_to_lineno((yylsp[(2) - (4)]));
							new->label	  = (yyvsp[(1) - (4)].str);
							new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;
							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else
						{
							PLpgSQL_stmt_forq		*new;

							Assert((yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_FORS ||
								   (yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_FORC ||
								   (yyvsp[(3) - (4)].stmt)->cmd_type == PLPGSQL_STMT_DYNFORS);
							/* forq is the common supertype of all three */
							new = (PLpgSQL_stmt_forq *) (yyvsp[(3) - (4)].stmt);
							new->lineno   = plpgsql_location_to_lineno((yylsp[(2) - (4)]));
							new->label	  = (yyvsp[(1) - (4)].str);
							new->body	  = (yyvsp[(4) - (4)].loop_body).stmts;
							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}

						check_labels((yyvsp[(1) - (4)].str), (yyvsp[(4) - (4)].loop_body).end_label, (yyvsp[(4) - (4)].loop_body).end_label_location);
						/* close namespace started in opt_block_label */
						plpgsql_ns_pop();
					;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 1076 "gram.y"
    {
						int			tok = yylex();
						int			tokloc = yylloc;

						if (tok == K_EXECUTE)
						{
							/* EXECUTE means it's a dynamic FOR loop */
							PLpgSQL_stmt_dynfors	*new;
							PLpgSQL_expr			*expr;
							int						term;

							expr = read_sql_expression2(K_LOOP, K_USING,
														"LOOP or USING",
														&term);

							new = palloc0(sizeof(PLpgSQL_stmt_dynfors));
							new->cmd_type = PLPGSQL_STMT_DYNFORS;
							if ((yyvsp[(1) - (2)].forvariable).rec)
							{
								new->rec = (yyvsp[(1) - (2)].forvariable).rec;
								check_assignable((PLpgSQL_datum *) new->rec, (yylsp[(1) - (2)]));
							}
							else if ((yyvsp[(1) - (2)].forvariable).row)
							{
								new->row = (yyvsp[(1) - (2)].forvariable).row;
								check_assignable((PLpgSQL_datum *) new->row, (yylsp[(1) - (2)]));
							}
							else if ((yyvsp[(1) - (2)].forvariable).scalar)
							{
								/* convert single scalar to list */
								new->row = make_scalar_list1((yyvsp[(1) - (2)].forvariable).name, (yyvsp[(1) - (2)].forvariable).scalar,
															 (yyvsp[(1) - (2)].forvariable).lineno, (yylsp[(1) - (2)]));
								/* no need for check_assignable */
							}
							else
							{
								ereport(ERROR,
										(errcode(ERRCODE_DATATYPE_MISMATCH),
										 errmsg("loop variable of loop over rows must be a record or row variable or list of scalar variables"),
										 parser_errposition((yylsp[(1) - (2)]))));
							}
							new->query = expr;

							if (term == K_USING)
							{
								do
								{
									expr = read_sql_expression2(',', K_LOOP,
																", or LOOP",
																&term);
									new->params = lappend(new->params, expr);
								} while (term == ',');
							}

							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else if (tok == T_DATUM &&
								 yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_VAR &&
								 ((PLpgSQL_var *) yylval.wdatum.datum)->datatype->typoid == REFCURSOROID)
						{
							/* It's FOR var IN cursor */
							PLpgSQL_stmt_forc	*new;
							PLpgSQL_var			*cursor = (PLpgSQL_var *) yylval.wdatum.datum;

							new = (PLpgSQL_stmt_forc *) palloc0(sizeof(PLpgSQL_stmt_forc));
							new->cmd_type = PLPGSQL_STMT_FORC;
							new->curvar = cursor->dno;

							/* Should have had a single variable name */
							if ((yyvsp[(1) - (2)].forvariable).scalar && (yyvsp[(1) - (2)].forvariable).row)
								ereport(ERROR,
										(errcode(ERRCODE_SYNTAX_ERROR),
										 errmsg("cursor FOR loop must have only one target variable"),
										 parser_errposition((yylsp[(1) - (2)]))));

							/* can't use an unbound cursor this way */
							if (cursor->cursor_explicit_expr == NULL)
								ereport(ERROR,
										(errcode(ERRCODE_SYNTAX_ERROR),
										 errmsg("cursor FOR loop must use a bound cursor variable"),
										 parser_errposition(tokloc)));

							/* collect cursor's parameters if any */
							new->argquery = read_cursor_args(cursor,
															 K_LOOP,
															 "LOOP");

							/* create loop's private RECORD variable */
							new->rec = plpgsql_build_record((yyvsp[(1) - (2)].forvariable).name,
															(yyvsp[(1) - (2)].forvariable).lineno,
															true);

							(yyval.stmt) = (PLpgSQL_stmt *) new;
						}
						else
						{
							PLpgSQL_expr	*expr1;
							int				expr1loc;
							bool			reverse = false;

							/*
							 * We have to distinguish between two
							 * alternatives: FOR var IN a .. b and FOR
							 * var IN query. Unfortunately this is
							 * tricky, since the query in the second
							 * form needn't start with a SELECT
							 * keyword.  We use the ugly hack of
							 * looking for two periods after the first
							 * token. We also check for the REVERSE
							 * keyword, which means it must be an
							 * integer loop.
							 */
							if (tok_is_keyword(tok, &yylval,
											   K_REVERSE, "reverse"))
								reverse = true;
							else
								plpgsql_push_back_token(tok);

							/*
							 * Read tokens until we see either a ".."
							 * or a LOOP. The text we read may not
							 * necessarily be a well-formed SQL
							 * statement, so we need to invoke
							 * read_sql_construct directly.
							 */
							expr1 = read_sql_construct(DOT_DOT,
													   K_LOOP,
													   0,
													   "LOOP",
													   "SELECT ",
													   true,
													   false,
													   &expr1loc,
													   &tok);

							if (tok == DOT_DOT)
							{
								/* Saw "..", so it must be an integer loop */
								PLpgSQL_expr		*expr2;
								PLpgSQL_expr		*expr_by;
								PLpgSQL_var			*fvar;
								PLpgSQL_stmt_fori	*new;

								/* Check first expression is well-formed */
								check_sql_expr(expr1->query, expr1loc, 7);

								/* Read and check the second one */
								expr2 = read_sql_expression2(K_LOOP, K_BY,
															 "LOOP",
															 &tok);

								/* Get the BY clause if any */
								if (tok == K_BY)
									expr_by = read_sql_expression(K_LOOP,
																  "LOOP");
								else
									expr_by = NULL;

								/* Should have had a single variable name */
								if ((yyvsp[(1) - (2)].forvariable).scalar && (yyvsp[(1) - (2)].forvariable).row)
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("integer FOR loop must have only one target variable"),
											 parser_errposition((yylsp[(1) - (2)]))));

								/* create loop's private variable */
								fvar = (PLpgSQL_var *)
									plpgsql_build_variable((yyvsp[(1) - (2)].forvariable).name,
														   (yyvsp[(1) - (2)].forvariable).lineno,
														   plpgsql_build_datatype(INT4OID,
																				  -1),
														   true);

								new = palloc0(sizeof(PLpgSQL_stmt_fori));
								new->cmd_type = PLPGSQL_STMT_FORI;
								new->var	  = fvar;
								new->reverse  = reverse;
								new->lower	  = expr1;
								new->upper	  = expr2;
								new->step	  = expr_by;

								(yyval.stmt) = (PLpgSQL_stmt *) new;
							}
							else
							{
								/*
								 * No "..", so it must be a query loop. We've
								 * prefixed an extra SELECT to the query text,
								 * so we need to remove that before performing
								 * syntax checking.
								 */
								char				*tmp_query;
								PLpgSQL_stmt_fors	*new;

								if (reverse)
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("cannot specify REVERSE in query FOR loop"),
											 parser_errposition(tokloc)));

								Assert(strncmp(expr1->query, "SELECT ", 7) == 0);
								tmp_query = pstrdup(expr1->query + 7);
								pfree(expr1->query);
								expr1->query = tmp_query;

								check_sql_expr(expr1->query, expr1loc, 0);

								new = palloc0(sizeof(PLpgSQL_stmt_fors));
								new->cmd_type = PLPGSQL_STMT_FORS;
								if ((yyvsp[(1) - (2)].forvariable).rec)
								{
									new->rec = (yyvsp[(1) - (2)].forvariable).rec;
									check_assignable((PLpgSQL_datum *) new->rec, (yylsp[(1) - (2)]));
								}
								else if ((yyvsp[(1) - (2)].forvariable).row)
								{
									new->row = (yyvsp[(1) - (2)].forvariable).row;
									check_assignable((PLpgSQL_datum *) new->row, (yylsp[(1) - (2)]));
								}
								else if ((yyvsp[(1) - (2)].forvariable).scalar)
								{
									/* convert single scalar to list */
									new->row = make_scalar_list1((yyvsp[(1) - (2)].forvariable).name, (yyvsp[(1) - (2)].forvariable).scalar,
																 (yyvsp[(1) - (2)].forvariable).lineno, (yylsp[(1) - (2)]));
									/* no need for check_assignable */
								}
								else
								{
									ereport(ERROR,
											(errcode(ERRCODE_SYNTAX_ERROR),
											 errmsg("loop variable of loop over rows must be a record or row variable or list of scalar variables"),
											 parser_errposition((yylsp[(1) - (2)]))));
								}

								new->query = expr1;
								(yyval.stmt) = (PLpgSQL_stmt *) new;
							}
						}
					;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 1336 "gram.y"
    {
						(yyval.forvariable).name = NameOfDatum(&((yyvsp[(1) - (1)].wdatum)));
						(yyval.forvariable).lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						if ((yyvsp[(1) - (1)].wdatum).datum->dtype == PLPGSQL_DTYPE_ROW)
						{
							(yyval.forvariable).scalar = NULL;
							(yyval.forvariable).rec = NULL;
							(yyval.forvariable).row = (PLpgSQL_row *) (yyvsp[(1) - (1)].wdatum).datum;
						}
						else if ((yyvsp[(1) - (1)].wdatum).datum->dtype == PLPGSQL_DTYPE_REC)
						{
							(yyval.forvariable).scalar = NULL;
							(yyval.forvariable).rec = (PLpgSQL_rec *) (yyvsp[(1) - (1)].wdatum).datum;
							(yyval.forvariable).row = NULL;
						}
						else
						{
							int			tok;

							(yyval.forvariable).scalar = (yyvsp[(1) - (1)].wdatum).datum;
							(yyval.forvariable).rec = NULL;
							(yyval.forvariable).row = NULL;
							/* check for comma-separated list */
							tok = yylex();
							plpgsql_push_back_token(tok);
							if (tok == ',')
								(yyval.forvariable).row = read_into_scalar_list((yyval.forvariable).name,
															   (yyval.forvariable).scalar,
															   (yylsp[(1) - (1)]));
						}
					;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 1368 "gram.y"
    {
						int			tok;

						(yyval.forvariable).name = (yyvsp[(1) - (1)].word).ident;
						(yyval.forvariable).lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						(yyval.forvariable).scalar = NULL;
						(yyval.forvariable).rec = NULL;
						(yyval.forvariable).row = NULL;
						/* check for comma-separated list */
						tok = yylex();
						plpgsql_push_back_token(tok);
						if (tok == ',')
							word_is_not_variable(&((yyvsp[(1) - (1)].word)), (yylsp[(1) - (1)]));
					;}
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 1383 "gram.y"
    {
						/* just to give a better message than "syntax error" */
						cword_is_not_variable(&((yyvsp[(1) - (1)].cword)), (yylsp[(1) - (1)]));
					;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 1390 "gram.y"
    {
						PLpgSQL_stmt_exit *new;

						new = palloc0(sizeof(PLpgSQL_stmt_exit));
						new->cmd_type = PLPGSQL_STMT_EXIT;
						new->is_exit  = (yyvsp[(1) - (3)].boolean);
						new->lineno	  = plpgsql_location_to_lineno((yylsp[(1) - (3)]));
						new->label	  = (yyvsp[(2) - (3)].str);
						new->cond	  = (yyvsp[(3) - (3)].expr);

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 1405 "gram.y"
    {
						(yyval.boolean) = true;
					;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 1409 "gram.y"
    {
						(yyval.boolean) = false;
					;}
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 1415 "gram.y"
    {
						int	tok;

						tok = yylex();
						if (tok == 0)
							yyerror("unexpected end of function definition");

						if (tok_is_keyword(tok, &yylval,
										   K_NEXT, "next"))
						{
							(yyval.stmt) = make_return_next_stmt((yylsp[(1) - (1)]));
						}
						else if (tok_is_keyword(tok, &yylval,
												K_QUERY, "query"))
						{
							(yyval.stmt) = make_return_query_stmt((yylsp[(1) - (1)]));
						}
						else
						{
							plpgsql_push_back_token(tok);
							(yyval.stmt) = make_return_stmt((yylsp[(1) - (1)]));
						}
					;}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 1441 "gram.y"
    {
						PLpgSQL_stmt_raise		*new;
						int	tok;

						new = palloc(sizeof(PLpgSQL_stmt_raise));

						new->cmd_type	= PLPGSQL_STMT_RAISE;
						new->lineno		= plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						new->elog_level = ERROR;	/* default */
						new->condname	= NULL;
						new->message	= NULL;
						new->params		= NIL;
						new->options	= NIL;

						tok = yylex();
						if (tok == 0)
							yyerror("unexpected end of function definition");

						/*
						 * We could have just RAISE, meaning to re-throw
						 * the current error.
						 */
						if (tok != ';')
						{
							/*
							 * First is an optional elog severity level.
							 */
							if (tok_is_keyword(tok, &yylval,
											   K_EXCEPTION, "exception"))
							{
								new->elog_level = ERROR;
								tok = yylex();
							}
							else if (tok_is_keyword(tok, &yylval,
													K_WARNING, "warning"))
							{
								new->elog_level = WARNING;
								tok = yylex();
							}
							else if (tok_is_keyword(tok, &yylval,
													K_NOTICE, "notice"))
							{
								new->elog_level = NOTICE;
								tok = yylex();
							}
							else if (tok_is_keyword(tok, &yylval,
													K_INFO, "info"))
							{
								new->elog_level = INFO;
								tok = yylex();
							}
							else if (tok_is_keyword(tok, &yylval,
													K_LOG, "log"))
							{
								new->elog_level = LOG;
								tok = yylex();
							}
							else if (tok_is_keyword(tok, &yylval,
													K_DEBUG, "debug"))
							{
								new->elog_level = DEBUG1;
								tok = yylex();
							}
							if (tok == 0)
								yyerror("unexpected end of function definition");

							/*
							 * Next we can have a condition name, or
							 * equivalently SQLSTATE 'xxxxx', or a string
							 * literal that is the old-style message format,
							 * or USING to start the option list immediately.
							 */
							if (tok == SCONST)
							{
								/* old style message and parameters */
								new->message = yylval.str;
								/*
								 * We expect either a semi-colon, which
								 * indicates no parameters, or a comma that
								 * begins the list of parameter expressions,
								 * or USING to begin the options list.
								 */
								tok = yylex();
								if (tok != ',' && tok != ';' && tok != K_USING)
									yyerror("syntax error");

								while (tok == ',')
								{
									PLpgSQL_expr *expr;

									expr = read_sql_construct(',', ';', K_USING,
															  ", or ; or USING",
															  "SELECT ",
															  true, true,
															  NULL, &tok);
									new->params = lappend(new->params, expr);
								}
							}
							else if (tok != K_USING)
							{
								/* must be condition name or SQLSTATE */
								if (tok_is_keyword(tok, &yylval,
												   K_SQLSTATE, "sqlstate"))
								{
									/* next token should be a string literal */
									char   *sqlstatestr;

									if (yylex() != SCONST)
										yyerror("syntax error");
									sqlstatestr = yylval.str;

									if (strlen(sqlstatestr) != 5)
										yyerror("invalid SQLSTATE code");
									if (strspn(sqlstatestr, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 5)
										yyerror("invalid SQLSTATE code");
									new->condname = sqlstatestr;
								}
								else
								{
									if (tok != T_WORD)
										yyerror("syntax error");
									new->condname = yylval.word.ident;
									plpgsql_recognize_err_condition(new->condname,
																	false);
								}
								tok = yylex();
								if (tok != ';' && tok != K_USING)
									yyerror("syntax error");
							}

							if (tok == K_USING)
								new->options = read_raise_options();
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 1580 "gram.y"
    {
						(yyval.loop_body).stmts = (yyvsp[(1) - (5)].list);
						(yyval.loop_body).end_label = (yyvsp[(4) - (5)].str);
						(yyval.loop_body).end_label_location = (yylsp[(4) - (5)]);
					;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1598 "gram.y"
    {
						(yyval.stmt) = make_execsql_stmt(K_INSERT, (yylsp[(1) - (1)]));
					;}
    break;

  case 111:

/* Line 1455 of yacc.c  */
#line 1602 "gram.y"
    {
						int			tok;

						tok = yylex();
						plpgsql_push_back_token(tok);
						if (tok == '=' || tok == COLON_EQUALS || tok == '[')
							word_is_not_variable(&((yyvsp[(1) - (1)].word)), (yylsp[(1) - (1)]));
						(yyval.stmt) = make_execsql_stmt(T_WORD, (yylsp[(1) - (1)]));
					;}
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 1612 "gram.y"
    {
						int			tok;

						tok = yylex();
						plpgsql_push_back_token(tok);
						if (tok == '=' || tok == COLON_EQUALS || tok == '[')
							cword_is_not_variable(&((yyvsp[(1) - (1)].cword)), (yylsp[(1) - (1)]));
						(yyval.stmt) = make_execsql_stmt(T_CWORD, (yylsp[(1) - (1)]));
					;}
    break;

  case 113:

/* Line 1455 of yacc.c  */
#line 1624 "gram.y"
    {
						PLpgSQL_stmt_dynexecute *new;
						PLpgSQL_expr *expr;
						int endtoken;

						expr = read_sql_construct(K_INTO, K_USING, ';',
												  "INTO or USING or ;",
												  "SELECT ",
												  true, true,
												  NULL, &endtoken);

						new = palloc(sizeof(PLpgSQL_stmt_dynexecute));
						new->cmd_type = PLPGSQL_STMT_DYNEXECUTE;
						new->lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						new->query = expr;
						new->into = false;
						new->strict = false;
						new->rec = NULL;
						new->row = NULL;
						new->params = NIL;

						/*
						 * We loop to allow the INTO and USING clauses to
						 * appear in either order, since people easily get
						 * that wrong.  This coding also prevents "INTO foo"
						 * from getting absorbed into a USING expression,
						 * which is *really* confusing.
						 */
						for (;;)
						{
							if (endtoken == K_INTO)
							{
								if (new->into)			/* multiple INTO */
									yyerror("syntax error");
								new->into = true;
								read_into_target(&new->rec, &new->row, &new->strict);
								endtoken = yylex();
							}
							else if (endtoken == K_USING)
							{
								if (new->params)		/* multiple USING */
									yyerror("syntax error");
								do
								{
									expr = read_sql_construct(',', ';', K_INTO,
															  ", or ; or INTO",
															  "SELECT ",
															  true, true,
															  NULL, &endtoken);
									new->params = lappend(new->params, expr);
								} while (endtoken == ',');
							}
							else if (endtoken == ';')
								break;
							else
								yyerror("syntax error");
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1688 "gram.y"
    {
						PLpgSQL_stmt_open *new;
						int				  tok;

						new = palloc0(sizeof(PLpgSQL_stmt_open));
						new->cmd_type = PLPGSQL_STMT_OPEN;
						new->lineno = plpgsql_location_to_lineno((yylsp[(1) - (2)]));
						new->curvar = (yyvsp[(2) - (2)].var)->dno;
						new->cursor_options = CURSOR_OPT_FAST_PLAN;

						if ((yyvsp[(2) - (2)].var)->cursor_explicit_expr == NULL)
						{
							/* be nice if we could use opt_scrollable here */
						    tok = yylex();
							if (tok_is_keyword(tok, &yylval,
											   K_NO, "no"))
							{
								tok = yylex();
								if (tok_is_keyword(tok, &yylval,
												   K_SCROLL, "scroll"))
								{
									new->cursor_options |= CURSOR_OPT_NO_SCROLL;
									tok = yylex();
								}
							}
							else if (tok_is_keyword(tok, &yylval,
													K_SCROLL, "scroll"))
							{
								new->cursor_options |= CURSOR_OPT_SCROLL;
								tok = yylex();
							}

							if (tok != K_FOR)
								yyerror("syntax error, expected \"FOR\"");

							tok = yylex();
							if (tok == K_EXECUTE)
							{
								int		endtoken;

								new->dynquery =
									read_sql_expression2(K_USING, ';',
														 "USING or ;",
														 &endtoken);

								/* If we found "USING", collect argument(s) */
								if (endtoken == K_USING)
								{
									PLpgSQL_expr *expr;
									
									do
									{
										expr = read_sql_expression2(',', ';',
																	", or ;",
																	&endtoken);
										new->params = lappend(new->params,
															  expr);
									} while (endtoken == ',');
								}
							}
							else
							{
								plpgsql_push_back_token(tok);
								new->query = read_sql_stmt("");
							}
						}
						else
						{
							/* predefined cursor query, so read args */
							new->argquery = read_cursor_args((yyvsp[(2) - (2)].var), ';', ";");
						}

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1765 "gram.y"
    {
						PLpgSQL_stmt_fetch *fetch = (yyvsp[(2) - (4)].fetch);
						PLpgSQL_rec	   *rec;
						PLpgSQL_row	   *row;

						/* We have already parsed everything through the INTO keyword */
						read_into_target(&rec, &row, NULL);

						if (yylex() != ';')
							yyerror("syntax error");

						/*
						 * We don't allow multiple rows in PL/pgSQL's FETCH
						 * statement, only in MOVE.
						 */
						if (fetch->returns_multiple_rows)
							ereport(ERROR,
									(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
									 errmsg("FETCH statement cannot return multiple rows"),
									 parser_errposition((yylsp[(1) - (4)]))));

						fetch->lineno = plpgsql_location_to_lineno((yylsp[(1) - (4)]));
						fetch->rec		= rec;
						fetch->row		= row;
						fetch->curvar	= (yyvsp[(3) - (4)].var)->dno;
						fetch->is_move	= false;

						(yyval.stmt) = (PLpgSQL_stmt *)fetch;
					;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1797 "gram.y"
    {
						PLpgSQL_stmt_fetch *fetch = (yyvsp[(2) - (4)].fetch);

						fetch->lineno = plpgsql_location_to_lineno((yylsp[(1) - (4)]));
						fetch->curvar	= (yyvsp[(3) - (4)].var)->dno;
						fetch->is_move	= true;

						(yyval.stmt) = (PLpgSQL_stmt *)fetch;
					;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1809 "gram.y"
    {
						(yyval.fetch) = read_fetch_direction();
					;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1815 "gram.y"
    {
						PLpgSQL_stmt_close *new;

						new = palloc(sizeof(PLpgSQL_stmt_close));
						new->cmd_type = PLPGSQL_STMT_CLOSE;
						new->lineno = plpgsql_location_to_lineno((yylsp[(1) - (3)]));
						new->curvar = (yyvsp[(2) - (3)].var)->dno;

						(yyval.stmt) = (PLpgSQL_stmt *)new;
					;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1828 "gram.y"
    {
						/* We do not bother building a node for NULL */
						(yyval.stmt) = NULL;
					;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1835 "gram.y"
    {
						if ((yyvsp[(1) - (1)].wdatum).datum->dtype != PLPGSQL_DTYPE_VAR)
							ereport(ERROR,
									(errcode(ERRCODE_DATATYPE_MISMATCH),
									 errmsg("cursor variable must be a simple variable"),
									 parser_errposition((yylsp[(1) - (1)]))));

						if (((PLpgSQL_var *) (yyvsp[(1) - (1)].wdatum).datum)->datatype->typoid != REFCURSOROID)
							ereport(ERROR,
									(errcode(ERRCODE_DATATYPE_MISMATCH),
									 errmsg("variable \"%s\" must be of type cursor or refcursor",
											((PLpgSQL_var *) (yyvsp[(1) - (1)].wdatum).datum)->refname),
									 parser_errposition((yylsp[(1) - (1)]))));
						(yyval.var) = (PLpgSQL_var *) (yyvsp[(1) - (1)].wdatum).datum;
					;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1851 "gram.y"
    {
						/* just to give a better message than "syntax error" */
						word_is_not_variable(&((yyvsp[(1) - (1)].word)), (yylsp[(1) - (1)]));
					;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1856 "gram.y"
    {
						/* just to give a better message than "syntax error" */
						cword_is_not_variable(&((yyvsp[(1) - (1)].cword)), (yylsp[(1) - (1)]));
					;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 1863 "gram.y"
    { (yyval.exception_block) = NULL; ;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 1865 "gram.y"
    {
						/*
						 * We use a mid-rule action to add these
						 * special variables to the namespace before
						 * parsing the WHEN clauses themselves.  The
						 * scope of the names extends to the end of the
						 * current block.
						 */
						int			lineno = plpgsql_location_to_lineno((yylsp[(1) - (1)]));
						PLpgSQL_exception_block *new = palloc(sizeof(PLpgSQL_exception_block));
						PLpgSQL_variable *var;

						var = plpgsql_build_variable("sqlstate", lineno,
													 plpgsql_build_datatype(TEXTOID, -1),
													 true);
						((PLpgSQL_var *) var)->isconst = true;
						new->sqlstate_varno = var->dno;

						var = plpgsql_build_variable("sqlerrm", lineno,
													 plpgsql_build_datatype(TEXTOID, -1),
													 true);
						((PLpgSQL_var *) var)->isconst = true;
						new->sqlerrm_varno = var->dno;

						(yyval.exception_block) = new;
					;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 1892 "gram.y"
    {
						PLpgSQL_exception_block *new = (yyvsp[(2) - (3)].exception_block);
						new->exc_list = (yyvsp[(3) - (3)].list);

						(yyval.exception_block) = new;
					;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 1901 "gram.y"
    {
							(yyval.list) = lappend((yyvsp[(1) - (2)].list), (yyvsp[(2) - (2)].exception));
						;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 1905 "gram.y"
    {
							(yyval.list) = list_make1((yyvsp[(1) - (1)].exception));
						;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 1911 "gram.y"
    {
						PLpgSQL_exception *new;

						new = palloc0(sizeof(PLpgSQL_exception));
						new->lineno     = plpgsql_location_to_lineno((yylsp[(1) - (4)]));
						new->conditions = (yyvsp[(2) - (4)].condition);
						new->action	    = (yyvsp[(4) - (4)].list);

						(yyval.exception) = new;
					;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 1924 "gram.y"
    {
							PLpgSQL_condition	*old;

							for (old = (yyvsp[(1) - (3)].condition); old->next != NULL; old = old->next)
								/* skip */ ;
							old->next = (yyvsp[(3) - (3)].condition);
							(yyval.condition) = (yyvsp[(1) - (3)].condition);
						;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 1933 "gram.y"
    {
							(yyval.condition) = (yyvsp[(1) - (1)].condition);
						;}
    break;

  case 131:

/* Line 1455 of yacc.c  */
#line 1939 "gram.y"
    {
							if (strcmp((yyvsp[(1) - (1)].str), "sqlstate") != 0)
							{
								(yyval.condition) = plpgsql_parse_err_condition((yyvsp[(1) - (1)].str));
							}
							else
							{
								PLpgSQL_condition *new;
								char   *sqlstatestr;

								/* next token should be a string literal */
								if (yylex() != SCONST)
									yyerror("syntax error");
								sqlstatestr = yylval.str;

								if (strlen(sqlstatestr) != 5)
									yyerror("invalid SQLSTATE code");
								if (strspn(sqlstatestr, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") != 5)
									yyerror("invalid SQLSTATE code");

								new = palloc(sizeof(PLpgSQL_condition));
								new->sqlerrstate =
									MAKE_SQLSTATE(sqlstatestr[0],
												  sqlstatestr[1],
												  sqlstatestr[2],
												  sqlstatestr[3],
												  sqlstatestr[4]);
								new->condname = sqlstatestr;
								new->next = NULL;

								(yyval.condition) = new;
							}
						;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 1975 "gram.y"
    { (yyval.expr) = read_sql_expression(';', ";"); ;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 1979 "gram.y"
    { (yyval.expr) = read_sql_expression(']', "]"); ;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 1983 "gram.y"
    { (yyval.expr) = read_sql_expression(K_THEN, "THEN"); ;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 1987 "gram.y"
    { (yyval.expr) = read_sql_expression(K_LOOP, "LOOP"); ;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 1991 "gram.y"
    {
						plpgsql_ns_push(NULL);
						(yyval.str) = NULL;
					;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 1996 "gram.y"
    {
						plpgsql_ns_push((yyvsp[(2) - (3)].str));
						(yyval.str) = (yyvsp[(2) - (3)].str);
					;}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 2003 "gram.y"
    {
						(yyval.str) = NULL;
					;}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 2007 "gram.y"
    {
						if (plpgsql_ns_lookup_label(plpgsql_ns_top(), (yyvsp[(1) - (1)].str)) == NULL)
							yyerror("label does not exist");
						(yyval.str) = (yyvsp[(1) - (1)].str);
					;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 2015 "gram.y"
    { (yyval.expr) = NULL; ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 2017 "gram.y"
    { (yyval.expr) = (yyvsp[(2) - (2)].expr); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 2024 "gram.y"
    {
						(yyval.str) = (yyvsp[(1) - (1)].word).ident;
					;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 2028 "gram.y"
    {
						if ((yyvsp[(1) - (1)].wdatum).ident == NULL) /* composite name not OK */
							yyerror("syntax error");
						(yyval.str) = (yyvsp[(1) - (1)].wdatum).ident;
					;}
    break;



/* Line 1455 of yacc.c  */
#line 4124 "pl_gram.c"
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
      yyerror (YY_("syntax error"));
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
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
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
		      yytoken, &yylval, &yylloc);
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
		  yystos[yystate], yyvsp, yylsp);
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
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp);
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
#line 2074 "gram.y"


/*
 * Check whether a token represents an "unreserved keyword".
 * We have various places where we want to recognize a keyword in preference
 * to a variable name, but not reserve that keyword in other contexts.
 * Hence, this kluge.
 */
static bool
tok_is_keyword(int token, union YYSTYPE *lval,
			   int kw_token, const char *kw_str)
{
	if (token == kw_token)
	{
		/* Normal case, was recognized by scanner (no conflicting variable) */
		return true;
	}
	else if (token == T_DATUM)
	{
		/*
		 * It's a variable, so recheck the string name.  Note we will not
		 * match composite names (hence an unreserved word followed by "."
		 * will not be recognized).
		 */
		if (!lval->wdatum.quoted && lval->wdatum.ident != NULL &&
			strcmp(lval->wdatum.ident, kw_str) == 0)
			return true;
	}
	return false;				/* not the keyword */
}

/*
 * Convenience routine to complain when we expected T_DATUM and got T_WORD,
 * ie, unrecognized variable.
 */
static void
word_is_not_variable(PLword *word, int location)
{
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("\"%s\" is not a known variable",
					word->ident),
			 parser_errposition(location)));
}

/* Same, for a CWORD */
static void
cword_is_not_variable(PLcword *cword, int location)
{
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("\"%s\" is not a known variable",
					NameListToString(cword->idents)),
			 parser_errposition(location)));
}

/*
 * Convenience routine to complain when we expected T_DATUM and got
 * something else.  "tok" must be the current token, since we also
 * look at yylval and yylloc.
 */
static void
current_token_is_not_variable(int tok)
{
	if (tok == T_WORD)
		word_is_not_variable(&(yylval.word), yylloc);
	else if (tok == T_CWORD)
		cword_is_not_variable(&(yylval.cword), yylloc);
	else
		yyerror("syntax error");
}

/* Convenience routine to read an expression with one possible terminator */
static PLpgSQL_expr *
read_sql_expression(int until, const char *expected)
{
	return read_sql_construct(until, 0, 0, expected,
							  "SELECT ", true, true, NULL, NULL);
}

/* Convenience routine to read an expression with two possible terminators */
static PLpgSQL_expr *
read_sql_expression2(int until, int until2, const char *expected,
					 int *endtoken)
{
	return read_sql_construct(until, until2, 0, expected,
							  "SELECT ", true, true, NULL, endtoken);
}

/* Convenience routine to read a SQL statement that must end with ';' */
static PLpgSQL_expr *
read_sql_stmt(const char *sqlstart)
{
	return read_sql_construct(';', 0, 0, ";",
							  sqlstart, false, true, NULL, NULL);
}

/*
 * Read a SQL construct and build a PLpgSQL_expr for it.
 *
 * until:		token code for expected terminator
 * until2:		token code for alternate terminator (pass 0 if none)
 * until3:		token code for another alternate terminator (pass 0 if none)
 * expected:	text to use in complaining that terminator was not found
 * sqlstart:	text to prefix to the accumulated SQL text
 * isexpression: whether to say we're reading an "expression" or a "statement"
 * valid_sql:   whether to check the syntax of the expr (prefixed with sqlstart)
 * startloc:	if not NULL, location of first token is stored at *startloc
 * endtoken:	if not NULL, ending token is stored at *endtoken
 *				(this is only interesting if until2 or until3 isn't zero)
 */
static PLpgSQL_expr *
read_sql_construct(int until,
				   int until2,
				   int until3,
				   const char *expected,
				   const char *sqlstart,
				   bool isexpression,
				   bool valid_sql,
				   int *startloc,
				   int *endtoken)
{
	int					tok;
	StringInfoData		ds;
	IdentifierLookup	save_IdentifierLookup;
	int					startlocation = -1;
	int					parenlevel = 0;
	PLpgSQL_expr		*expr;

	initStringInfo(&ds);
	appendStringInfoString(&ds, sqlstart);

	/* special lookup mode for identifiers within the SQL text */
	save_IdentifierLookup = plpgsql_IdentifierLookup;
	plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;

	for (;;)
	{
		tok = yylex();
		if (startlocation < 0)			/* remember loc of first token */
			startlocation = yylloc;
		if (tok == until && parenlevel == 0)
			break;
		if (tok == until2 && parenlevel == 0)
			break;
		if (tok == until3 && parenlevel == 0)
			break;
		if (tok == '(' || tok == '[')
			parenlevel++;
		else if (tok == ')' || tok == ']')
		{
			parenlevel--;
			if (parenlevel < 0)
				yyerror("mismatched parentheses");
		}
		/*
		 * End of function definition is an error, and we don't expect to
		 * hit a semicolon either (unless it's the until symbol, in which
		 * case we should have fallen out above).
		 */
		if (tok == 0 || tok == ';')
		{
			if (parenlevel != 0)
				yyerror("mismatched parentheses");
			if (isexpression)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing \"%s\" at end of SQL expression",
								expected),
						 parser_errposition(yylloc)));
			else
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("missing \"%s\" at end of SQL statement",
								expected),
						 parser_errposition(yylloc)));
		}
	}

	plpgsql_IdentifierLookup = save_IdentifierLookup;

	if (startloc)
		*startloc = startlocation;
	if (endtoken)
		*endtoken = tok;

	/* give helpful complaint about empty input */
	if (startlocation >= yylloc)
	{
		if (isexpression)
			yyerror("missing expression");
		else
			yyerror("missing SQL statement");
	}

	plpgsql_append_source_text(&ds, startlocation, yylloc);

	/* trim any trailing whitespace, for neatness */
	while (ds.len > 0 && scanner_isspace(ds.data[ds.len - 1]))
		ds.data[--ds.len] = '\0';

	expr = palloc0(sizeof(PLpgSQL_expr));
	expr->dtype			= PLPGSQL_DTYPE_EXPR;
	expr->query			= pstrdup(ds.data);
	expr->plan			= NULL;
	expr->paramnos		= NULL;
	expr->ns            = plpgsql_ns_top();
	pfree(ds.data);

	if (valid_sql)
		check_sql_expr(expr->query, startlocation, strlen(sqlstart));

	return expr;
}

static PLpgSQL_type *
read_datatype(int tok)
{
	StringInfoData		ds;
	char			   *type_name;
	int					startlocation;
	PLpgSQL_type		*result;
	int					parenlevel = 0;

	/* Should only be called while parsing DECLARE sections */
	Assert(plpgsql_IdentifierLookup == IDENTIFIER_LOOKUP_DECLARE);

	/* Often there will be a lookahead token, but if not, get one */
	if (tok == YYEMPTY)
		tok = yylex();

	startlocation = yylloc;

	/*
	 * If we have a simple or composite identifier, check for %TYPE
	 * and %ROWTYPE constructs.
	 */
	if (tok == T_WORD)
	{
		char   *dtname = yylval.word.ident;

		tok = yylex();
		if (tok == '%')
		{
			tok = yylex();
			if (tok_is_keyword(tok, &yylval,
							   K_TYPE, "type"))
			{
				result = plpgsql_parse_wordtype(dtname);
				if (result)
					return result;
			}
			else if (tok_is_keyword(tok, &yylval,
									K_ROWTYPE, "rowtype"))
			{
				result = plpgsql_parse_wordrowtype(dtname);
				if (result)
					return result;
			}
		}
	}
	else if (tok == T_CWORD)
	{
		List   *dtnames = yylval.cword.idents;

		tok = yylex();
		if (tok == '%')
		{
			tok = yylex();
			if (tok_is_keyword(tok, &yylval,
							   K_TYPE, "type"))
			{
				result = plpgsql_parse_cwordtype(dtnames);
				if (result)
					return result;
			}
			else if (tok_is_keyword(tok, &yylval,
									K_ROWTYPE, "rowtype"))
			{
				result = plpgsql_parse_cwordrowtype(dtnames);
				if (result)
					return result;
			}
		}
	}

	while (tok != ';')
	{
		if (tok == 0)
		{
			if (parenlevel != 0)
				yyerror("mismatched parentheses");
			else
				yyerror("incomplete data type declaration");
		}
		/* Possible followers for datatype in a declaration */
		if (tok == K_NOT || tok == '=' || tok == COLON_EQUALS || tok == K_DEFAULT)
			break;
		/* Possible followers for datatype in a cursor_arg list */
		if ((tok == ',' || tok == ')') && parenlevel == 0)
			break;
		if (tok == '(')
			parenlevel++;
		else if (tok == ')')
			parenlevel--;

		tok = yylex();
	}

	/* set up ds to contain complete typename text */
	initStringInfo(&ds);
	plpgsql_append_source_text(&ds, startlocation, yylloc);
	type_name = ds.data;

	if (type_name[0] == '\0')
		yyerror("missing data type declaration");

	result = parse_datatype(type_name, startlocation);

	pfree(ds.data);

	plpgsql_push_back_token(tok);

	return result;
}

static PLpgSQL_stmt *
make_execsql_stmt(int firsttoken, int location)
{
	StringInfoData		ds;
	IdentifierLookup	save_IdentifierLookup;
	PLpgSQL_stmt_execsql *execsql;
	PLpgSQL_expr		*expr;
	PLpgSQL_row			*row = NULL;
	PLpgSQL_rec			*rec = NULL;
	int					tok;
	int					prev_tok;
	bool				have_into = false;
	bool				have_strict = false;
	int					into_start_loc = -1;
	int					into_end_loc = -1;

	initStringInfo(&ds);

	/* special lookup mode for identifiers within the SQL text */
	save_IdentifierLookup = plpgsql_IdentifierLookup;
	plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;

	/*
	 * We have to special-case the sequence INSERT INTO, because we don't want
	 * that to be taken as an INTO-variables clause.  Fortunately, this is the
	 * only valid use of INTO in a pl/pgsql SQL command, and INTO is already a
	 * fully reserved word in the main grammar.  We have to treat it that way
	 * anywhere in the string, not only at the start; consider CREATE RULE
	 * containing an INSERT statement.
	 */
	tok = firsttoken;
	for (;;)
	{
		prev_tok = tok;
		tok = yylex();
		if (have_into && into_end_loc < 0)
			into_end_loc = yylloc;		/* token after the INTO part */
		if (tok == ';')
			break;
		if (tok == 0)
			yyerror("unexpected end of function definition");

		if (tok == K_INTO && prev_tok != K_INSERT)
		{
			if (have_into)
				yyerror("INTO specified more than once");
			have_into = true;
			into_start_loc = yylloc;
			plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_NORMAL;
			read_into_target(&rec, &row, &have_strict);
			plpgsql_IdentifierLookup = IDENTIFIER_LOOKUP_EXPR;
		}
	}

	plpgsql_IdentifierLookup = save_IdentifierLookup;

	if (have_into)
	{
		/*
		 * Insert an appropriate number of spaces corresponding to the
		 * INTO text, so that locations within the redacted SQL statement
		 * still line up with those in the original source text.
		 */
		plpgsql_append_source_text(&ds, location, into_start_loc);
		appendStringInfoSpaces(&ds, into_end_loc - into_start_loc);
		plpgsql_append_source_text(&ds, into_end_loc, yylloc);
	}
	else
		plpgsql_append_source_text(&ds, location, yylloc);

	/* trim any trailing whitespace, for neatness */
	while (ds.len > 0 && scanner_isspace(ds.data[ds.len - 1]))
		ds.data[--ds.len] = '\0';

	expr = palloc0(sizeof(PLpgSQL_expr));
	expr->dtype			= PLPGSQL_DTYPE_EXPR;
	expr->query			= pstrdup(ds.data);
	expr->plan			= NULL;
	expr->paramnos		= NULL;
	expr->ns            = plpgsql_ns_top();
	pfree(ds.data);

	check_sql_expr(expr->query, location, 0);

	execsql = palloc(sizeof(PLpgSQL_stmt_execsql));
	execsql->cmd_type = PLPGSQL_STMT_EXECSQL;
	execsql->lineno  = plpgsql_location_to_lineno(location);
	execsql->sqlstmt = expr;
	execsql->into	 = have_into;
	execsql->strict	 = have_strict;
	execsql->rec	 = rec;
	execsql->row	 = row;

	return (PLpgSQL_stmt *) execsql;
}


/*
 * Read FETCH or MOVE direction clause (everything through FROM/IN).
 */
static PLpgSQL_stmt_fetch *
read_fetch_direction(void)
{
	PLpgSQL_stmt_fetch *fetch;
	int			tok;
	bool		check_FROM = true;

	/*
	 * We create the PLpgSQL_stmt_fetch struct here, but only fill in
	 * the fields arising from the optional direction clause
	 */
	fetch = (PLpgSQL_stmt_fetch *) palloc0(sizeof(PLpgSQL_stmt_fetch));
	fetch->cmd_type = PLPGSQL_STMT_FETCH;
	/* set direction defaults: */
	fetch->direction = FETCH_FORWARD;
	fetch->how_many  = 1;
	fetch->expr      = NULL;
	fetch->returns_multiple_rows = false;

	tok = yylex();
	if (tok == 0)
		yyerror("unexpected end of function definition");

	if (tok_is_keyword(tok, &yylval,
					   K_NEXT, "next"))
	{
		/* use defaults */
	}
	else if (tok_is_keyword(tok, &yylval,
							K_PRIOR, "prior"))
	{
		fetch->direction = FETCH_BACKWARD;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_FIRST, "first"))
	{
		fetch->direction = FETCH_ABSOLUTE;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_LAST, "last"))
	{
		fetch->direction = FETCH_ABSOLUTE;
		fetch->how_many  = -1;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_ABSOLUTE, "absolute"))
	{
		fetch->direction = FETCH_ABSOLUTE;
		fetch->expr = read_sql_expression2(K_FROM, K_IN,
										   "FROM or IN",
										   NULL);
		check_FROM = false;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_RELATIVE, "relative"))
	{
		fetch->direction = FETCH_RELATIVE;
		fetch->expr = read_sql_expression2(K_FROM, K_IN,
										   "FROM or IN",
										   NULL);
		check_FROM = false;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_ALL, "all"))
	{
		fetch->how_many = FETCH_ALL;
		fetch->returns_multiple_rows = true;
	}
	else if (tok_is_keyword(tok, &yylval,
							K_FORWARD, "forward"))
	{
		complete_direction(fetch, &check_FROM);
	}
	else if (tok_is_keyword(tok, &yylval,
							K_BACKWARD, "backward"))
	{
		fetch->direction = FETCH_BACKWARD;
		complete_direction(fetch, &check_FROM);
	}
	else if (tok == K_FROM || tok == K_IN)
	{
		/* empty direction */
		check_FROM = false;
	}
	else if (tok == T_DATUM)
	{
		/* Assume there's no direction clause and tok is a cursor name */
		plpgsql_push_back_token(tok);
		check_FROM = false;
	}
	else
	{
		/*
		 * Assume it's a count expression with no preceding keyword.
		 * Note: we allow this syntax because core SQL does, but we don't
		 * document it because of the ambiguity with the omitted-direction
		 * case.  For instance, "MOVE n IN c" will fail if n is a variable.
		 * Perhaps this can be improved someday, but it's hardly worth a
		 * lot of work.
		 */
		plpgsql_push_back_token(tok);
		fetch->expr = read_sql_expression2(K_FROM, K_IN,
										   "FROM or IN",
										   NULL);
		fetch->returns_multiple_rows = true;
		check_FROM = false;
	}

	/* check FROM or IN keyword after direction's specification */
	if (check_FROM)
	{
		tok = yylex();
		if (tok != K_FROM && tok != K_IN)
			yyerror("expected FROM or IN");
	}

	return fetch;
}

/*
 * Process remainder of FETCH/MOVE direction after FORWARD or BACKWARD.
 * Allows these cases:
 *   FORWARD expr,  FORWARD ALL,  FORWARD
 *   BACKWARD expr, BACKWARD ALL, BACKWARD
 */
static void
complete_direction(PLpgSQL_stmt_fetch *fetch,  bool *check_FROM)
{
	int			tok;

	tok = yylex();
	if (tok == 0)
		yyerror("unexpected end of function definition");

	if (tok == K_FROM || tok == K_IN)
	{
		*check_FROM = false;
		return;
	}

	if (tok == K_ALL)
	{
		fetch->how_many = FETCH_ALL;
		fetch->returns_multiple_rows = true;
		*check_FROM = true;
		return;
	}

	plpgsql_push_back_token(tok);
	fetch->expr = read_sql_expression2(K_FROM, K_IN,
									   "FROM or IN",
									   NULL);
	fetch->returns_multiple_rows = true;
	*check_FROM = false;
}


static PLpgSQL_stmt *
make_return_stmt(int location)
{
	PLpgSQL_stmt_return *new;

	new = palloc0(sizeof(PLpgSQL_stmt_return));
	new->cmd_type = PLPGSQL_STMT_RETURN;
	new->lineno   = plpgsql_location_to_lineno(location);
	new->expr	  = NULL;
	new->retvarno = -1;

	if (plpgsql_curr_compile->fn_retset)
	{
		if (yylex() != ';')
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("RETURN cannot have a parameter in function returning set"),
					 errhint("Use RETURN NEXT or RETURN QUERY."),
					 parser_errposition(yylloc)));
	}
	else if (plpgsql_curr_compile->out_param_varno >= 0)
	{
		if (yylex() != ';')
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("RETURN cannot have a parameter in function with OUT parameters"),
					 parser_errposition(yylloc)));
		new->retvarno = plpgsql_curr_compile->out_param_varno;
	}
	else if (plpgsql_curr_compile->fn_rettype == VOIDOID)
	{
		if (yylex() != ';')
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("RETURN cannot have a parameter in function returning void"),
					 parser_errposition(yylloc)));
	}
	else if (plpgsql_curr_compile->fn_retistuple)
	{
		switch (yylex())
		{
			case K_NULL:
				/* we allow this to support RETURN NULL in triggers */
				break;

			case T_DATUM:
				if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW ||
					yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
					new->retvarno = yylval.wdatum.datum->dno;
				else
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("RETURN must specify a record or row variable in function returning row"),
							 parser_errposition(yylloc)));
				break;

			default:
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("RETURN must specify a record or row variable in function returning row"),
						 parser_errposition(yylloc)));
				break;
		}
		if (yylex() != ';')
			yyerror("syntax error");
	}
	else
	{
		/*
		 * Note that a well-formed expression is _required_ here;
		 * anything else is a compile-time error.
		 */
		new->expr = read_sql_expression(';', ";");
	}

	return (PLpgSQL_stmt *) new;
}


static PLpgSQL_stmt *
make_return_next_stmt(int location)
{
	PLpgSQL_stmt_return_next *new;

	if (!plpgsql_curr_compile->fn_retset)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot use RETURN NEXT in a non-SETOF function"),
				 parser_errposition(location)));

	new = palloc0(sizeof(PLpgSQL_stmt_return_next));
	new->cmd_type	= PLPGSQL_STMT_RETURN_NEXT;
	new->lineno		= plpgsql_location_to_lineno(location);
	new->expr		= NULL;
	new->retvarno	= -1;

	if (plpgsql_curr_compile->out_param_varno >= 0)
	{
		if (yylex() != ';')
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("RETURN NEXT cannot have a parameter in function with OUT parameters"),
					 parser_errposition(yylloc)));
		new->retvarno = plpgsql_curr_compile->out_param_varno;
	}
	else if (plpgsql_curr_compile->fn_retistuple)
	{
		switch (yylex())
		{
			case T_DATUM:
				if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW ||
					yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
					new->retvarno = yylval.wdatum.datum->dno;
				else
					ereport(ERROR,
							(errcode(ERRCODE_DATATYPE_MISMATCH),
							 errmsg("RETURN NEXT must specify a record or row variable in function returning row"),
							 parser_errposition(yylloc)));
				break;

			default:
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("RETURN NEXT must specify a record or row variable in function returning row"),
						 parser_errposition(yylloc)));
				break;
		}
		if (yylex() != ';')
			yyerror("syntax error");
	}
	else
		new->expr = read_sql_expression(';', ";");

	return (PLpgSQL_stmt *) new;
}


static PLpgSQL_stmt *
make_return_query_stmt(int location)
{
	PLpgSQL_stmt_return_query *new;
	int			tok;

	if (!plpgsql_curr_compile->fn_retset)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot use RETURN QUERY in a non-SETOF function"),
				 parser_errposition(location)));

	new = palloc0(sizeof(PLpgSQL_stmt_return_query));
	new->cmd_type = PLPGSQL_STMT_RETURN_QUERY;
	new->lineno = plpgsql_location_to_lineno(location);

	/* check for RETURN QUERY EXECUTE */
	if ((tok = yylex()) != K_EXECUTE)
	{
		/* ordinary static query */
		plpgsql_push_back_token(tok);
		new->query = read_sql_stmt("");
	}
	else
	{
		/* dynamic SQL */
		int		term;

		new->dynquery = read_sql_expression2(';', K_USING, "; or USING",
											 &term);
		if (term == K_USING)
		{
			do
			{
				PLpgSQL_expr *expr;

				expr = read_sql_expression2(',', ';', ", or ;", &term);
				new->params = lappend(new->params, expr);
			} while (term == ',');
		}
	}

	return (PLpgSQL_stmt *) new;
}


/* convenience routine to fetch the name of a T_DATUM */
static char *
NameOfDatum(PLwdatum *wdatum)
{
	if (wdatum->ident)
		return wdatum->ident;
	Assert(wdatum->idents != NIL);
	return NameListToString(wdatum->idents);
}

static void
check_assignable(PLpgSQL_datum *datum, int location)
{
	switch (datum->dtype)
	{
		case PLPGSQL_DTYPE_VAR:
			if (((PLpgSQL_var *) datum)->isconst)
				ereport(ERROR,
						(errcode(ERRCODE_ERROR_IN_ASSIGNMENT),
						 errmsg("\"%s\" is declared CONSTANT",
								((PLpgSQL_var *) datum)->refname),
						 parser_errposition(location)));
			break;
		case PLPGSQL_DTYPE_ROW:
			/* always assignable? */
			break;
		case PLPGSQL_DTYPE_REC:
			/* always assignable?  What about NEW/OLD? */
			break;
		case PLPGSQL_DTYPE_RECFIELD:
			/* always assignable? */
			break;
		case PLPGSQL_DTYPE_ARRAYELEM:
			/* always assignable? */
			break;
		default:
			elog(ERROR, "unrecognized dtype: %d", datum->dtype);
			break;
	}
}

/*
 * Read the argument of an INTO clause.  On entry, we have just read the
 * INTO keyword.
 */
static void
read_into_target(PLpgSQL_rec **rec, PLpgSQL_row **row, bool *strict)
{
	int			tok;

	/* Set default results */
	*rec = NULL;
	*row = NULL;
	if (strict)
		*strict = false;

	tok = yylex();
	if (strict && tok == K_STRICT)
	{
		*strict = true;
		tok = yylex();
	}

	/*
	 * Currently, a row or record variable can be the single INTO target,
	 * but not a member of a multi-target list.  So we throw error if there
	 * is a comma after it, because that probably means the user tried to
	 * write a multi-target list.  If this ever gets generalized, we should
	 * probably refactor read_into_scalar_list so it handles all cases.
	 */
	switch (tok)
	{
		case T_DATUM:
			if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW)
			{
				check_assignable(yylval.wdatum.datum, yylloc);
				*row = (PLpgSQL_row *) yylval.wdatum.datum;

				if ((tok = yylex()) == ',')
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("record or row variable cannot be part of multiple-item INTO list"),
							 parser_errposition(yylloc)));
				plpgsql_push_back_token(tok);
			}
			else if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
			{
				check_assignable(yylval.wdatum.datum, yylloc);
				*rec = (PLpgSQL_rec *) yylval.wdatum.datum;

				if ((tok = yylex()) == ',')
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("record or row variable cannot be part of multiple-item INTO list"),
							 parser_errposition(yylloc)));
				plpgsql_push_back_token(tok);
			}
			else
			{
				*row = read_into_scalar_list(NameOfDatum(&(yylval.wdatum)),
											 yylval.wdatum.datum, yylloc);
			}
			break;

		default:
			/* just to give a better message than "syntax error" */
			current_token_is_not_variable(tok);
	}
}

/*
 * Given the first datum and name in the INTO list, continue to read
 * comma-separated scalar variables until we run out. Then construct
 * and return a fake "row" variable that represents the list of
 * scalars.
 */
static PLpgSQL_row *
read_into_scalar_list(char *initial_name,
					  PLpgSQL_datum *initial_datum,
					  int initial_location)
{
	int				 nfields;
	char			*fieldnames[1024];
	int				 varnos[1024];
	PLpgSQL_row		*row;
	int				 tok;

	check_assignable(initial_datum, initial_location);
	fieldnames[0] = initial_name;
	varnos[0]	  = initial_datum->dno;
	nfields		  = 1;

	while ((tok = yylex()) == ',')
	{
		/* Check for array overflow */
		if (nfields >= 1024)
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("too many INTO variables specified"),
					 parser_errposition(yylloc)));

		tok = yylex();
		switch (tok)
		{
			case T_DATUM:
				check_assignable(yylval.wdatum.datum, yylloc);
				if (yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_ROW ||
					yylval.wdatum.datum->dtype == PLPGSQL_DTYPE_REC)
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("\"%s\" is not a scalar variable",
									NameOfDatum(&(yylval.wdatum))),
							 parser_errposition(yylloc)));
				fieldnames[nfields] = NameOfDatum(&(yylval.wdatum));
				varnos[nfields++]	= yylval.wdatum.datum->dno;
				break;

			default:
				/* just to give a better message than "syntax error" */
				current_token_is_not_variable(tok);
		}
	}

	/*
	 * We read an extra, non-comma token from yylex(), so push it
	 * back onto the input stream
	 */
	plpgsql_push_back_token(tok);

	row = palloc(sizeof(PLpgSQL_row));
	row->dtype = PLPGSQL_DTYPE_ROW;
	row->refname = pstrdup("*internal*");
	row->lineno = plpgsql_location_to_lineno(initial_location);
	row->rowtupdesc = NULL;
	row->nfields = nfields;
	row->fieldnames = palloc(sizeof(char *) * nfields);
	row->varnos = palloc(sizeof(int) * nfields);
	while (--nfields >= 0)
	{
		row->fieldnames[nfields] = fieldnames[nfields];
		row->varnos[nfields] = varnos[nfields];
	}

	plpgsql_adddatum((PLpgSQL_datum *)row);

	return row;
}

/*
 * Convert a single scalar into a "row" list.  This is exactly
 * like read_into_scalar_list except we never consume any input.
 *
 * Note: lineno could be computed from location, but since callers
 * have it at hand already, we may as well pass it in.
 */
static PLpgSQL_row *
make_scalar_list1(char *initial_name,
				  PLpgSQL_datum *initial_datum,
				  int lineno, int location)
{
	PLpgSQL_row		*row;

	check_assignable(initial_datum, location);

	row = palloc(sizeof(PLpgSQL_row));
	row->dtype = PLPGSQL_DTYPE_ROW;
	row->refname = pstrdup("*internal*");
	row->lineno = lineno;
	row->rowtupdesc = NULL;
	row->nfields = 1;
	row->fieldnames = palloc(sizeof(char *));
	row->varnos = palloc(sizeof(int));
	row->fieldnames[0] = initial_name;
	row->varnos[0] = initial_datum->dno;

	plpgsql_adddatum((PLpgSQL_datum *)row);

	return row;
}

/*
 * When the PL/PgSQL parser expects to see a SQL statement, it is very
 * liberal in what it accepts; for example, we often assume an
 * unrecognized keyword is the beginning of a SQL statement. This
 * avoids the need to duplicate parts of the SQL grammar in the
 * PL/PgSQL grammar, but it means we can accept wildly malformed
 * input. To try and catch some of the more obviously invalid input,
 * we run the strings we expect to be SQL statements through the main
 * SQL parser.
 *
 * We only invoke the raw parser (not the analyzer); this doesn't do
 * any database access and does not check any semantic rules, it just
 * checks for basic syntactic correctness. We do this here, rather
 * than after parsing has finished, because a malformed SQL statement
 * may cause the PL/PgSQL parser to become confused about statement
 * borders. So it is best to bail out as early as we can.
 *
 * It is assumed that "stmt" represents a copy of the function source text
 * beginning at offset "location", with leader text of length "leaderlen"
 * (typically "SELECT ") prefixed to the source text.  We use this assumption
 * to transpose any error cursor position back to the function source text.
 * If no error cursor is provided, we'll just point at "location".
 */
static void
check_sql_expr(const char *stmt, int location, int leaderlen)
{
	sql_error_callback_arg cbarg;
	ErrorContextCallback  syntax_errcontext;
	MemoryContext oldCxt;

	if (!plpgsql_check_syntax)
		return;

	cbarg.location = location;
	cbarg.leaderlen = leaderlen;

	syntax_errcontext.callback = plpgsql_sql_error_callback;
	syntax_errcontext.arg = &cbarg;
	syntax_errcontext.previous = error_context_stack;
	error_context_stack = &syntax_errcontext;

	oldCxt = MemoryContextSwitchTo(compile_tmp_cxt);
	(void) raw_parser(stmt);
	MemoryContextSwitchTo(oldCxt);

	/* Restore former ereport callback */
	error_context_stack = syntax_errcontext.previous;
}

static void
plpgsql_sql_error_callback(void *arg)
{
	sql_error_callback_arg *cbarg = (sql_error_callback_arg *) arg;
	int			errpos;

	/*
	 * First, set up internalerrposition to point to the start of the
	 * statement text within the function text.  Note this converts
	 * location (a byte offset) to a character number.
	 */
	parser_errposition(cbarg->location);

	/*
	 * If the core parser provided an error position, transpose it.
	 * Note we are dealing with 1-based character numbers at this point.
	 */
	errpos = geterrposition();
	if (errpos > cbarg->leaderlen)
	{
		int		myerrpos = getinternalerrposition();

		if (myerrpos > 0)		/* safety check */
			internalerrposition(myerrpos + errpos - cbarg->leaderlen - 1);
	}

	/* In any case, flush errposition --- we want internalerrpos only */
	errposition(0);
}

/*
 * Parse a SQL datatype name and produce a PLpgSQL_type structure.
 *
 * The heavy lifting is done elsewhere.  Here we are only concerned
 * with setting up an errcontext link that will let us give an error
 * cursor pointing into the plpgsql function source, if necessary.
 * This is handled the same as in check_sql_expr(), and we likewise
 * expect that the given string is a copy from the source text.
 */
static PLpgSQL_type *
parse_datatype(const char *string, int location)
{
	Oid			type_id;
	int32		typmod;
	sql_error_callback_arg cbarg;
	ErrorContextCallback  syntax_errcontext;

	cbarg.location = location;
	cbarg.leaderlen = 0;

	syntax_errcontext.callback = plpgsql_sql_error_callback;
	syntax_errcontext.arg = &cbarg;
	syntax_errcontext.previous = error_context_stack;
	error_context_stack = &syntax_errcontext;

	/* Let the main parser try to parse it under standard SQL rules */
	parseTypeString(string, &type_id, &typmod);

	/* Restore former ereport callback */
	error_context_stack = syntax_errcontext.previous;

	/* Okay, build a PLpgSQL_type data structure for it */
	return plpgsql_build_datatype(type_id, typmod);
}

/*
 * Check block starting and ending labels match.
 */
static void
check_labels(const char *start_label, const char *end_label, int end_location)
{
	if (end_label)
	{
		if (!start_label)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("end label \"%s\" specified for unlabelled block",
							end_label),
					 parser_errposition(end_location)));

		if (strcmp(start_label, end_label) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("end label \"%s\" differs from block's label \"%s\"",
							end_label, start_label),
					 parser_errposition(end_location)));
	}
}

/*
 * Read the arguments (if any) for a cursor, followed by the until token
 *
 * If cursor has no args, just swallow the until token and return NULL.
 * If it does have args, we expect to see "( expr [, expr ...] )" followed
 * by the until token.  Consume all that and return a SELECT query that
 * evaluates the expression(s) (without the outer parens).
 */
static PLpgSQL_expr *
read_cursor_args(PLpgSQL_var *cursor, int until, const char *expected)
{
	PLpgSQL_expr *expr;
	int			tok;

	tok = yylex();
	if (cursor->cursor_explicit_argrow < 0)
	{
		/* No arguments expected */
		if (tok == '(')
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("cursor \"%s\" has no arguments",
							cursor->refname),
					 parser_errposition(yylloc)));

		if (tok != until)
			yyerror("syntax error");

		return NULL;
	}

	/* Else better provide arguments */
	if (tok != '(')
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("cursor \"%s\" has arguments",
						cursor->refname),
				 parser_errposition(yylloc)));

	/*
	 * Read expressions until the matching ')'.
	 */
	expr = read_sql_expression(')', ")");

	/* Next we'd better find the until token */
	tok = yylex();
	if (tok != until)
		yyerror("syntax error");

	return expr;
}

/*
 * Parse RAISE ... USING options
 */
static List *
read_raise_options(void)
{
	List	   *result = NIL;

	for (;;)
	{
		PLpgSQL_raise_option *opt;
		int		tok;

		if ((tok = yylex()) == 0)
			yyerror("unexpected end of function definition");

		opt = (PLpgSQL_raise_option *) palloc(sizeof(PLpgSQL_raise_option));

		if (tok_is_keyword(tok, &yylval,
						   K_ERRCODE, "errcode"))
			opt->opt_type = PLPGSQL_RAISEOPTION_ERRCODE;
		else if (tok_is_keyword(tok, &yylval,
								K_MESSAGE, "message"))
			opt->opt_type = PLPGSQL_RAISEOPTION_MESSAGE;
		else if (tok_is_keyword(tok, &yylval,
								K_DETAIL, "detail"))
			opt->opt_type = PLPGSQL_RAISEOPTION_DETAIL;
		else if (tok_is_keyword(tok, &yylval,
								K_HINT, "hint"))
			opt->opt_type = PLPGSQL_RAISEOPTION_HINT;
		else
			yyerror("unrecognized RAISE statement option");

		tok = yylex();
		if (tok != '=' && tok != COLON_EQUALS)
			yyerror("syntax error, expected \"=\"");

		opt->expr = read_sql_expression2(',', ';', ", or ;", &tok);

		result = lappend(result, opt);

		if (tok == ';')
			break;
	}

	return result;
}

/*
 * Fix up CASE statement
 */
static PLpgSQL_stmt *
make_case(int location, PLpgSQL_expr *t_expr,
		  List *case_when_list, List *else_stmts)
{
	PLpgSQL_stmt_case 	*new;

	new = palloc(sizeof(PLpgSQL_stmt_case));
	new->cmd_type = PLPGSQL_STMT_CASE;
	new->lineno = plpgsql_location_to_lineno(location);
	new->t_expr = t_expr;
	new->t_varno = 0;
	new->case_when_list = case_when_list;
	new->have_else = (else_stmts != NIL);
	/* Get rid of list-with-NULL hack */
	if (list_length(else_stmts) == 1 && linitial(else_stmts) == NULL)
		new->else_stmts = NIL;
	else
		new->else_stmts = else_stmts;

	/*
	 * When test expression is present, we create a var for it and then
	 * convert all the WHEN expressions to "VAR IN (original_expression)".
	 * This is a bit klugy, but okay since we haven't yet done more than
	 * read the expressions as text.  (Note that previous parsing won't
	 * have complained if the WHEN ... THEN expression contained multiple
	 * comma-separated values.)
	 */
	if (t_expr)
	{
		char	varname[32];
		PLpgSQL_var *t_var;
		ListCell *l;

		/* use a name unlikely to collide with any user names */
		snprintf(varname, sizeof(varname), "__Case__Variable_%d__",
				 plpgsql_nDatums);

		/*
		 * We don't yet know the result datatype of t_expr.  Build the
		 * variable as if it were INT4; we'll fix this at runtime if needed.
		 */
		t_var = (PLpgSQL_var *)
			plpgsql_build_variable(varname, new->lineno,
								   plpgsql_build_datatype(INT4OID, -1),
								   true);
		new->t_varno = t_var->dno;

		foreach(l, case_when_list)
		{
			PLpgSQL_case_when *cwt = (PLpgSQL_case_when *) lfirst(l);
			PLpgSQL_expr *expr = cwt->expr;
			StringInfoData	ds;

			/* copy expression query without SELECT keyword (expr->query + 7) */
			Assert(strncmp(expr->query, "SELECT ", 7) == 0);

			/* And do the string hacking */
			initStringInfo(&ds);

			appendStringInfo(&ds, "SELECT \"%s\" IN (%s)",
							 varname, expr->query + 7);

			pfree(expr->query);
			expr->query = pstrdup(ds.data);
			/* Adjust expr's namespace to include the case variable */
			expr->ns = plpgsql_ns_top();

			pfree(ds.data);
		}
	}

	return (PLpgSQL_stmt *) new;
}

