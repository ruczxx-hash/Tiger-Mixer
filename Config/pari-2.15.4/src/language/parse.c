/* A Bison parser, made by GNU Bison 3.7.5.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30705

/* Bison version string.  */
#define YYBISON_VERSION "3.7.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         PARI_STYPE
#define YYLTYPE         PARI_LTYPE
/* Substitute the variable and function names.  */
#define yyparse         pari_parse
#define yylex           pari_lex
#define yyerror         pari_error
#define yydebug         pari_debug
#define yynerrs         pari_nerrs

/* First part of user prologue.  */
#line 1 "../src/language/parse.y"

/* Copyright (C) 2006  The PARI group.

This file is part of the PARI package.

PARI/GP is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version. It is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY WHATSOEVER.

Check the License for details. You should have received a copy of it, along
with the package; see the file 'COPYING'. If not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#define PARI_STYPE union token_value
#define PARI_LTYPE struct node_loc
#define YYPTRDIFF_T long
#define YYPTRDIFF_MAXIMUM LONG_MAX
#define YYSIZE_T size_t
#define YYLLOC_DEFAULT(Current, Rhs, N)     \
        ((Current).start  = ((N)?(Rhs)[1].start:(Rhs)[0].end),  \
         (Current).end    = (Rhs)[N].end)
#include "parsec.h"
#define NOARG(x) newnode(Fnoarg,-1,-1,&(x))
#define NORANGE(x) newnode(Fnorange,-1,-1,&(x))

#line 106 "../src/language/parse.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parse.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_KPARROW = 3,                    /* ")->"  */
  YYSYMBOL_KARROW = 4,                     /* "->"  */
  YYSYMBOL_KDOTDOT = 5,                    /* ".."  */
  YYSYMBOL_KPE = 6,                        /* "+="  */
  YYSYMBOL_KSE = 7,                        /* "-="  */
  YYSYMBOL_KME = 8,                        /* "*="  */
  YYSYMBOL_KDE = 9,                        /* "/="  */
  YYSYMBOL_KDRE = 10,                      /* "\\/="  */
  YYSYMBOL_KEUCE = 11,                     /* "\\="  */
  YYSYMBOL_KMODE = 12,                     /* "%="  */
  YYSYMBOL_KAND = 13,                      /* "&&"  */
  YYSYMBOL_KOR = 14,                       /* "||"  */
  YYSYMBOL_KID = 15,                       /* "==="  */
  YYSYMBOL_KEQ = 16,                       /* "=="  */
  YYSYMBOL_KNE = 17,                       /* "!="  */
  YYSYMBOL_KGE = 18,                       /* ">="  */
  YYSYMBOL_KLE = 19,                       /* "<="  */
  YYSYMBOL_KSRE = 20,                      /* ">>="  */
  YYSYMBOL_KSLE = 21,                      /* "<<="  */
  YYSYMBOL_KSR = 22,                       /* ">>"  */
  YYSYMBOL_KSL = 23,                       /* "<<"  */
  YYSYMBOL_KDR = 24,                       /* "\\/"  */
  YYSYMBOL_KPP = 25,                       /* "++"  */
  YYSYMBOL_KSS = 26,                       /* "--"  */
  YYSYMBOL_KINTEGER = 27,                  /* "integer"  */
  YYSYMBOL_KREAL = 28,                     /* "real number"  */
  YYSYMBOL_KENTRY = 29,                    /* "variable name"  */
  YYSYMBOL_KSTRING = 30,                   /* "character string"  */
  YYSYMBOL_SEQ = 31,                       /* SEQ  */
  YYSYMBOL_DEFFUNC = 32,                   /* DEFFUNC  */
  YYSYMBOL_INT = 33,                       /* INT  */
  YYSYMBOL_LVAL = 34,                      /* LVAL  */
  YYSYMBOL_35_ = 35,                       /* ';'  */
  YYSYMBOL_36_ = 36,                       /* ','  */
  YYSYMBOL_37_ = 37,                       /* '='  */
  YYSYMBOL_38_ = 38,                       /* '&'  */
  YYSYMBOL_39_ = 39,                       /* '>'  */
  YYSYMBOL_40_ = 40,                       /* '<'  */
  YYSYMBOL_41_ = 41,                       /* '+'  */
  YYSYMBOL_42_ = 42,                       /* '-'  */
  YYSYMBOL_43_ = 43,                       /* '%'  */
  YYSYMBOL_44_ = 44,                       /* '\\'  */
  YYSYMBOL_45_ = 45,                       /* '/'  */
  YYSYMBOL_46_ = 46,                       /* '*'  */
  YYSYMBOL_SIGN = 47,                      /* SIGN  */
  YYSYMBOL_48_ = 48,                       /* '^'  */
  YYSYMBOL_49_ = 49,                       /* '#'  */
  YYSYMBOL_50_ = 50,                       /* '!'  */
  YYSYMBOL_51_ = 51,                       /* '~'  */
  YYSYMBOL_52_ = 52,                       /* '['  */
  YYSYMBOL_DERIV = 53,                     /* DERIV  */
  YYSYMBOL_54_ = 54,                       /* '\''  */
  YYSYMBOL_55_ = 55,                       /* '.'  */
  YYSYMBOL_56_ = 56,                       /* '('  */
  YYSYMBOL_57_ = 57,                       /* ':'  */
  YYSYMBOL_58_ = 58,                       /* ']'  */
  YYSYMBOL_59_ = 59,                       /* '`'  */
  YYSYMBOL_60_ = 60,                       /* ')'  */
  YYSYMBOL_61_ = 61,                       /* '|'  */
  YYSYMBOL_YYACCEPT = 62,                  /* $accept  */
  YYSYMBOL_sequence = 63,                  /* sequence  */
  YYSYMBOL_seq = 64,                       /* seq  */
  YYSYMBOL_range = 65,                     /* range  */
  YYSYMBOL_matrix_index = 66,              /* matrix_index  */
  YYSYMBOL_backticks = 67,                 /* backticks  */
  YYSYMBOL_history = 68,                   /* history  */
  YYSYMBOL_deriv = 69,                     /* deriv  */
  YYSYMBOL_expr = 70,                      /* expr  */
  YYSYMBOL_lvalue = 71,                    /* lvalue  */
  YYSYMBOL_exprno = 72,                    /* exprno  */
  YYSYMBOL_matrixeltsno = 73,              /* matrixeltsno  */
  YYSYMBOL_matrixelts = 74,                /* matrixelts  */
  YYSYMBOL_matrixlines = 75,               /* matrixlines  */
  YYSYMBOL_matrix = 76,                    /* matrix  */
  YYSYMBOL_in = 77,                        /* in  */
  YYSYMBOL_inseq = 78,                     /* inseq  */
  YYSYMBOL_compr = 79,                     /* compr  */
  YYSYMBOL_arg = 80,                       /* arg  */
  YYSYMBOL_81_1 = 81,                      /* $@1  */
  YYSYMBOL_listarg = 82,                   /* listarg  */
  YYSYMBOL_funcid = 83,                    /* funcid  */
  YYSYMBOL_memberid = 84,                  /* memberid  */
  YYSYMBOL_definition = 85                 /* definition  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined PARI_LTYPE_IS_TRIVIAL && PARI_LTYPE_IS_TRIVIAL \
             && defined PARI_STYPE_IS_TRIVIAL && PARI_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  49
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   711

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  62
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  24
/* YYNRULES -- Number of rules.  */
#define YYNRULES  116
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  198

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   291


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    50,     2,    49,     2,    43,    38,    54,
      56,    60,    46,    41,    36,    42,    55,    45,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    57,    35,
      40,    37,    39,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    52,    44,    58,    48,     2,    59,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    61,     2,    51,     2,     2,     2,
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
      47,    53
};

#if PARI_DEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,    90,    90,    93,    94,    95,    96,    99,   100,   101,
     102,   105,   106,   109,   110,   113,   114,   115,   116,   117,
     118,   121,   122,   125,   126,   127,   128,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   181,   182,
     185,   186,   187,   190,   191,   193,   194,   197,   198,   201,
     202,   205,   206,   207,   208,   209,   210,   213,   216,   217,
     218,   219,   222,   225,   226,   227,   228,   229,   229,   233,
     234,   237,   240,   243,   245,   247,   248
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "\")->\"", "\"->\"",
  "\"..\"", "\"+=\"", "\"-=\"", "\"*=\"", "\"/=\"", "\"\\\\/=\"",
  "\"\\\\=\"", "\"%=\"", "\"&&\"", "\"||\"", "\"===\"", "\"==\"", "\"!=\"",
  "\">=\"", "\"<=\"", "\">>=\"", "\"<<=\"", "\">>\"", "\"<<\"",
  "\"\\\\/\"", "\"++\"", "\"--\"", "\"integer\"", "\"real number\"",
  "\"variable name\"", "\"character string\"", "SEQ", "DEFFUNC", "INT",
  "LVAL", "';'", "','", "'='", "'&'", "'>'", "'<'", "'+'", "'-'", "'%'",
  "'\\\\'", "'/'", "'*'", "SIGN", "'^'", "'#'", "'!'", "'~'", "'['",
  "DERIV", "'\\''", "'.'", "'('", "':'", "']'", "'`'", "')'", "'|'",
  "$accept", "sequence", "seq", "range", "matrix_index", "backticks",
  "history", "deriv", "expr", "lvalue", "exprno", "matrixeltsno",
  "matrixelts", "matrixlines", "matrix", "in", "inseq", "compr", "arg",
  "$@1", "listarg", "funcid", "memberid", "definition", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,    59,    44,    61,    38,    62,
      60,    43,    45,    37,    92,    47,    42,   290,    94,    35,
      33,   126,    91,   291,    39,    46,    40,    58,    93,    96,
      41,   124
};
#endif

#define YYPACT_NINF (-109)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-111)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     655,   -26,  -109,    17,  -109,   655,   655,   -15,   655,   655,
     112,     2,  -109,   621,    33,    40,  -109,   496,   234,    49,
    -109,  -109,  -109,  -109,    60,   621,    64,    64,  -109,    12,
    -109,    35,    81,   -33,    41,    44,  -109,   178,    68,   -18,
      50,  -109,    77,    77,    40,   357,   257,     6,     7,  -109,
     655,   655,   655,   655,   655,   655,   655,   655,   655,   655,
     655,   655,   655,   655,   655,   655,   655,   655,   655,   655,
     655,  -109,  -109,   638,  -109,    78,   621,    82,  -109,    63,
     655,   655,   655,   655,   655,   655,   655,   655,   655,   655,
    -109,  -109,   655,    93,  -109,   655,  -109,   -23,  -109,    35,
    -109,  -109,  -109,   655,    77,   655,   655,  -109,   655,  -109,
    -109,   -32,   -32,  -109,   313,  -109,   655,   621,   496,   539,
     539,   575,   575,   575,   575,   575,    64,    64,    64,   539,
     575,   575,   590,   590,    64,    64,    64,    64,    64,   655,
     -17,   282,    86,   -22,  -109,  -109,    40,   496,   496,   496,
     496,   496,   496,   496,   496,   496,   496,  -109,   496,    87,
     405,   -25,    57,    67,   496,  -109,   496,    90,    90,    70,
     655,    40,    69,   496,   638,  -109,   655,   655,  -109,   655,
    -109,    88,    77,   655,  -109,  -109,   496,    85,   496,    40,
      40,   655,  -109,   451,  -109,   496,    77,  -109
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       3,    23,    24,    80,    27,     0,     0,    15,     0,     0,
       0,     0,    25,     3,     0,     2,    29,     4,    32,    33,
      34,    31,    77,    35,     0,     3,    70,    71,    16,    18,
      13,    17,    50,    49,     0,     0,    91,    87,     0,    85,
       0,    28,     0,     0,   103,     4,    32,     0,     0,     1,
       5,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    75,    73,     7,    21,     0,     3,     0,    76,    74,
       3,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      38,    39,     0,     0,    81,     0,    26,     0,    19,    20,
      14,    96,    93,     0,     0,    84,    86,    94,    86,    95,
      80,   105,   106,    79,     7,   107,     3,     3,     6,    52,
      51,    54,    55,    56,    57,    59,    64,    63,    66,    53,
      58,    60,    62,    61,    65,    67,    68,    69,    72,     0,
       0,     8,   112,     0,    78,    22,   115,    47,    48,    40,
      41,    42,    43,    44,    46,    45,    37,    82,    36,   111,
       0,     0,    98,     0,    83,    88,    87,    89,    90,     0,
       0,   116,     0,    10,     7,    12,     0,     3,    30,     3,
      92,     0,     0,     0,   102,   104,   108,     0,     9,   114,
     113,     0,   100,    99,    11,    97,     0,   101
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -109,  -109,    11,   -30,   -16,   117,  -109,  -109,    -5,    -7,
    -109,  -109,   -80,  -109,  -109,  -109,  -108,  -109,    32,  -109,
      -9,  -109,  -109,  -109
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    14,    44,   140,    78,    31,    16,    79,    17,    18,
     165,    38,    39,    40,    19,   162,   163,    20,    47,   170,
      48,    21,    22,    23
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      26,    27,    94,    32,    33,    37,    46,   115,    45,  -109,
     116,    15,    28,   117,   117,   181,    97,   106,    46,   174,
      73,    74,    75,    76,    77,    93,   167,    73,   168,    24,
      94,    41,    93,    49,    29,   111,   112,   159,   178,    98,
     107,   175,  -109,   117,    30,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,  -109,   143,   141,    46,
     115,    30,  -110,    25,   192,    50,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   108,    95,   156,   197,    96,
     158,   146,   182,   183,   100,    94,    94,   161,   160,   101,
     164,   166,   102,   166,   105,  -110,   110,   142,   109,   141,
      46,   144,    70,    34,    71,    72,    73,   145,    74,    75,
      76,    77,   157,   177,   179,   184,   -85,   171,   185,  -110,
     191,    71,    72,    73,   173,    74,    75,    76,    77,     1,
       2,     3,     4,   194,   187,    94,    99,    35,   -86,   172,
       0,     0,     0,     5,     6,     7,     0,     0,     0,     0,
       0,     8,     9,     0,    10,   186,    11,    12,    13,   141,
      36,   188,     0,     0,     0,   161,     0,     0,   193,     0,
       0,     0,     0,   103,     0,     0,   195,     0,   189,   161,
     190,    51,    52,    53,    54,    55,    56,    57,     0,     0,
      58,    59,    60,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    61,    62,    63,    64,
      65,    66,    67,    68,    69,     0,    70,     0,    71,    72,
      73,     0,    74,    75,    76,    77,     0,     0,    80,   104,
      81,    82,    83,    84,    85,    86,    87,     0,     0,     0,
       0,     0,     0,     0,    88,    89,     0,     0,     0,    90,
      91,    80,     0,    81,    82,    83,    84,    85,    86,    87,
       0,    92,     0,     0,     0,     0,     0,    88,    89,     0,
       0,     0,    90,    91,     0,     0,    73,   176,     0,     0,
       0,    93,     0,     0,    92,    51,    52,    53,    54,    55,
      56,    57,     0,     0,    58,    59,    60,     0,     0,   114,
       0,     0,     0,     0,    93,     0,     0,     0,   169,     0,
      61,    62,    63,    64,    65,    66,    67,    68,    69,     0,
      70,     0,    71,    72,    73,     0,    74,    75,    76,    77,
       1,     2,     3,     4,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     0,     0,
       0,   139,     8,     9,     0,    10,     0,    11,    12,    13,
      51,    52,    53,    54,    55,    56,    57,     0,     0,    58,
      59,    60,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    61,    62,    63,    64,    65,
      66,    67,    68,    69,     0,    70,     0,    71,    72,    73,
       0,    74,    75,    76,    77,     0,     0,   113,    51,    52,
      53,    54,    55,    56,    57,     0,     0,    58,    59,    60,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    61,    62,    63,    64,    65,    66,    67,
      68,    69,     0,    70,     0,    71,    72,    73,     0,    74,
      75,    76,    77,   180,    51,    52,    53,    54,    55,    56,
      57,     0,     0,    58,    59,    60,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   196,     0,     0,    61,
      62,    63,    64,    65,    66,    67,    68,    69,     0,    70,
       0,    71,    72,    73,     0,    74,    75,    76,    77,    51,
      52,    53,    54,    55,    56,    57,     0,     0,    58,    59,
      60,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    61,    62,    63,    64,    65,    66,
      67,    68,    69,     0,    70,     0,    71,    72,    73,     0,
      74,    75,    76,    77,    53,    54,    55,    56,    57,     0,
       0,    58,    59,    60,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    62,    63,
      64,    65,    66,    67,    68,    69,     0,    70,     0,    71,
      72,    73,     0,    74,    75,    76,    77,    58,    59,    60,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    58,    59,    60,     0,    64,    65,    66,    67,
      68,    69,     0,    70,     0,    71,    72,    73,     0,    74,
      75,    76,    77,    66,    67,    68,    69,     0,    70,     0,
      71,    72,    73,     0,    74,    75,    76,    77,     1,     2,
       3,     4,     0,     0,     0,     0,     0,     0,     0,    42,
       0,     0,     5,     6,     7,     1,     2,     3,     4,     0,
       8,     9,    43,    10,     0,    11,    12,    13,     0,     5,
       6,     7,     1,     2,     3,     4,   139,     8,     9,     0,
      10,     0,    11,    12,    13,     0,     5,     6,     7,     0,
       0,     0,     0,     0,     8,     9,     0,    10,     0,    11,
      12,    13
};

static const yytype_int16 yycheck[] =
{
       5,     6,    18,     8,     9,    10,    13,     1,    13,     3,
       3,     0,    27,    36,    36,    40,    25,    35,    25,    36,
      52,    54,    55,    56,    57,    57,   106,    52,   108,    55,
      46,    29,    57,     0,    49,    42,    43,    60,    60,    27,
      58,    58,    36,    36,    59,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    60,    76,    73,    76,
       1,    59,     3,    56,   182,    35,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    35,    37,    92,   196,    29,
      95,    80,    35,    36,    59,   111,   112,   104,   103,    58,
     105,   106,    58,   108,    36,    36,    29,    29,    58,   114,
     117,    29,    48,     1,    50,    51,    52,    54,    54,    55,
      56,    57,    29,    37,    37,    58,    36,   116,    58,    60,
      42,    50,    51,    52,   139,    54,    55,    56,    57,    27,
      28,    29,    30,    58,   174,   161,    29,    35,    36,   117,
      -1,    -1,    -1,    41,    42,    43,    -1,    -1,    -1,    -1,
      -1,    49,    50,    -1,    52,   170,    54,    55,    56,   174,
      58,   176,    -1,    -1,    -1,   182,    -1,    -1,   183,    -1,
      -1,    -1,    -1,     5,    -1,    -1,   191,    -1,   177,   196,
     179,    13,    14,    15,    16,    17,    18,    19,    -1,    -1,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    -1,    48,    -1,    50,    51,
      52,    -1,    54,    55,    56,    57,    -1,    -1,     4,    61,
       6,     7,     8,     9,    10,    11,    12,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    20,    21,    -1,    -1,    -1,    25,
      26,     4,    -1,     6,     7,     8,     9,    10,    11,    12,
      -1,    37,    -1,    -1,    -1,    -1,    -1,    20,    21,    -1,
      -1,    -1,    25,    26,    -1,    -1,    52,     5,    -1,    -1,
      -1,    57,    -1,    -1,    37,    13,    14,    15,    16,    17,
      18,    19,    -1,    -1,    22,    23,    24,    -1,    -1,    52,
      -1,    -1,    -1,    -1,    57,    -1,    -1,    -1,     5,    -1,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    -1,
      48,    -1,    50,    51,    52,    -1,    54,    55,    56,    57,
      27,    28,    29,    30,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    41,    42,    43,    -1,    -1,    -1,
      -1,    48,    49,    50,    -1,    52,    -1,    54,    55,    56,
      13,    14,    15,    16,    17,    18,    19,    -1,    -1,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    -1,    48,    -1,    50,    51,    52,
      -1,    54,    55,    56,    57,    -1,    -1,    60,    13,    14,
      15,    16,    17,    18,    19,    -1,    -1,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    -1,    48,    -1,    50,    51,    52,    -1,    54,
      55,    56,    57,    58,    13,    14,    15,    16,    17,    18,
      19,    -1,    -1,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    35,    -1,    -1,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    -1,    48,
      -1,    50,    51,    52,    -1,    54,    55,    56,    57,    13,
      14,    15,    16,    17,    18,    19,    -1,    -1,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    -1,    48,    -1,    50,    51,    52,    -1,
      54,    55,    56,    57,    15,    16,    17,    18,    19,    -1,
      -1,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    39,    40,
      41,    42,    43,    44,    45,    46,    -1,    48,    -1,    50,
      51,    52,    -1,    54,    55,    56,    57,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    22,    23,    24,    -1,    41,    42,    43,    44,
      45,    46,    -1,    48,    -1,    50,    51,    52,    -1,    54,
      55,    56,    57,    43,    44,    45,    46,    -1,    48,    -1,
      50,    51,    52,    -1,    54,    55,    56,    57,    27,    28,
      29,    30,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,
      -1,    -1,    41,    42,    43,    27,    28,    29,    30,    -1,
      49,    50,    51,    52,    -1,    54,    55,    56,    -1,    41,
      42,    43,    27,    28,    29,    30,    48,    49,    50,    -1,
      52,    -1,    54,    55,    56,    -1,    41,    42,    43,    -1,
      -1,    -1,    -1,    -1,    49,    50,    -1,    52,    -1,    54,
      55,    56
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    27,    28,    29,    30,    41,    42,    43,    49,    50,
      52,    54,    55,    56,    63,    64,    68,    70,    71,    76,
      79,    83,    84,    85,    55,    56,    70,    70,    27,    49,
      59,    67,    70,    70,     1,    35,    58,    70,    73,    74,
      75,    29,    38,    51,    64,    70,    71,    80,    82,     0,
      35,    13,    14,    15,    16,    17,    18,    19,    22,    23,
      24,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      48,    50,    51,    52,    54,    55,    56,    57,    66,    69,
       4,     6,     7,     8,     9,    10,    11,    12,    20,    21,
      25,    26,    37,    57,    66,    37,    29,    82,    27,    67,
      59,    58,    58,     5,    61,    36,    35,    58,    35,    58,
      29,    71,    71,    60,    52,     1,     3,    36,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    48,
      65,    70,    29,    82,    29,    54,    64,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    29,    70,    60,
      70,    71,    77,    78,    70,    72,    70,    74,    74,     5,
      81,    64,    80,    70,    36,    58,     5,    37,    60,    37,
      58,    40,    35,    36,    58,    58,    70,    65,    70,    64,
      64,    42,    78,    70,    58,    70,    35,    78
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    62,    63,    64,    64,    64,    64,    65,    65,    65,
      65,    66,    66,    67,    67,    68,    68,    68,    68,    68,
      68,    69,    69,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      70,    70,    70,    70,    70,    70,    70,    70,    70,    70,
      71,    71,    71,    72,    72,    73,    73,    74,    74,    75,
      75,    76,    76,    76,    76,    76,    76,    77,    78,    78,
      78,    78,    79,    80,    80,    80,    80,    81,    80,    82,
      82,    83,    84,    85,    85,    85,    85
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     1,     2,     3,     0,     1,     3,
       2,     5,     3,     1,     2,     1,     2,     2,     2,     3,
       3,     1,     2,     1,     1,     1,     3,     1,     2,     1,
       4,     1,     1,     1,     1,     1,     3,     3,     2,     2,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       2,     2,     3,     2,     2,     2,     2,     1,     3,     3,
       1,     2,     3,     1,     0,     1,     0,     1,     3,     3,
       3,     2,     5,     3,     3,     3,     3,     4,     1,     3,
       3,     5,     5,     1,     4,     2,     2,     0,     4,     1,
       3,     4,     3,     6,     5,     3,     4
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = PARI_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == PARI_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, lex, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use PARI_error or PARI_UNDEF. */
#define YYERRCODE PARI_UNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if PARI_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YY_LOCATION_PRINT
#  if defined PARI_LTYPE_IS_TRIVIAL && PARI_LTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#   define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

#  else
#   define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#  endif
# endif /* !defined YY_LOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, lex); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, char **lex)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (lex);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, char **lex)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, lex);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, char **lex)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), lex);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, lex); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !PARI_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !PARI_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
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


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
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
            else
              goto append;

          append:
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

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, char **lex)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (lex);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_seq: /* seq  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1513 "../src/language/parse.c"
        break;

    case YYSYMBOL_range: /* range  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1519 "../src/language/parse.c"
        break;

    case YYSYMBOL_matrix_index: /* matrix_index  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1525 "../src/language/parse.c"
        break;

    case YYSYMBOL_backticks: /* backticks  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1531 "../src/language/parse.c"
        break;

    case YYSYMBOL_history: /* history  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1537 "../src/language/parse.c"
        break;

    case YYSYMBOL_deriv: /* deriv  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1543 "../src/language/parse.c"
        break;

    case YYSYMBOL_expr: /* expr  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1549 "../src/language/parse.c"
        break;

    case YYSYMBOL_lvalue: /* lvalue  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1555 "../src/language/parse.c"
        break;

    case YYSYMBOL_exprno: /* exprno  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1561 "../src/language/parse.c"
        break;

    case YYSYMBOL_matrixeltsno: /* matrixeltsno  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1567 "../src/language/parse.c"
        break;

    case YYSYMBOL_matrixelts: /* matrixelts  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1573 "../src/language/parse.c"
        break;

    case YYSYMBOL_matrixlines: /* matrixlines  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1579 "../src/language/parse.c"
        break;

    case YYSYMBOL_matrix: /* matrix  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1585 "../src/language/parse.c"
        break;

    case YYSYMBOL_in: /* in  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1591 "../src/language/parse.c"
        break;

    case YYSYMBOL_inseq: /* inseq  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1597 "../src/language/parse.c"
        break;

    case YYSYMBOL_compr: /* compr  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1603 "../src/language/parse.c"
        break;

    case YYSYMBOL_arg: /* arg  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1609 "../src/language/parse.c"
        break;

    case YYSYMBOL_listarg: /* listarg  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1615 "../src/language/parse.c"
        break;

    case YYSYMBOL_funcid: /* funcid  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1621 "../src/language/parse.c"
        break;

    case YYSYMBOL_memberid: /* memberid  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1627 "../src/language/parse.c"
        break;

    case YYSYMBOL_definition: /* definition  */
#line 87 "../src/language/parse.y"
            { pari_discarded++; }
#line 1633 "../src/language/parse.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (char **lex)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined PARI_LTYPE_IS_TRIVIAL && PARI_LTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = PARI_EMPTY; /* Cause a token to be read.  */

/* User initialization code.  */
#line 33 "../src/language/parse.y"
{ yylloc.start=yylloc.end=*lex; }

#line 1732 "../src/language/parse.c"

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
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

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

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
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == PARI_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc, lex);
    }

  if (yychar <= PARI_EOF)
    {
      yychar = PARI_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == PARI_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = PARI_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
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
      if (yytable_value_is_error (yyn))
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
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = PARI_EMPTY;
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
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* sequence: seq  */
#line 90 "../src/language/parse.y"
                     {(yyval.val)=(yyvsp[0].val); (void) pari_nerrs;}
#line 1944 "../src/language/parse.c"
    break;

  case 3: /* seq: %empty  */
#line 93 "../src/language/parse.y"
                     {(yyval.val)=NOARG((yyloc));}
#line 1950 "../src/language/parse.c"
    break;

  case 4: /* seq: expr  */
#line 94 "../src/language/parse.y"
                     {(yyval.val)=(yyvsp[0].val);}
#line 1956 "../src/language/parse.c"
    break;

  case 5: /* seq: seq ';'  */
#line 95 "../src/language/parse.y"
                     {(yyval.val)=(yyvsp[-1].val); (yyloc)=(yylsp[-1]);}
#line 1962 "../src/language/parse.c"
    break;

  case 6: /* seq: seq ';' expr  */
#line 96 "../src/language/parse.y"
                     {(yyval.val)=newnode(Fseq,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 1968 "../src/language/parse.c"
    break;

  case 7: /* range: %empty  */
#line 99 "../src/language/parse.y"
                      { (yyval.val)=newnode(Frange,NORANGE((yyloc)),NORANGE((yyloc)),&(yyloc)); }
#line 1974 "../src/language/parse.c"
    break;

  case 8: /* range: expr  */
#line 100 "../src/language/parse.y"
                      { (yyval.val)=newnode(Frange,(yyvsp[0].val),NORANGE((yyloc)),&(yyloc)); }
#line 1980 "../src/language/parse.c"
    break;

  case 9: /* range: expr ".." expr  */
#line 101 "../src/language/parse.y"
                      { (yyval.val)=newnode(Frange,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc)); }
#line 1986 "../src/language/parse.c"
    break;

  case 10: /* range: '^' expr  */
#line 102 "../src/language/parse.y"
                      { (yyval.val)=newnode(Frange,NORANGE((yyloc)),(yyvsp[0].val),&(yyloc)); }
#line 1992 "../src/language/parse.c"
    break;

  case 11: /* matrix_index: '[' range ',' range ']'  */
#line 105 "../src/language/parse.y"
                                      {(yyval.val)=newnode(Fmatrix,(yyvsp[-3].val),(yyvsp[-1].val),&(yyloc));}
#line 1998 "../src/language/parse.c"
    break;

  case 12: /* matrix_index: '[' range ']'  */
#line 106 "../src/language/parse.y"
                                      {(yyval.val)=newnode(Fmatrix,(yyvsp[-1].val),-1,&(yyloc));}
#line 2004 "../src/language/parse.c"
    break;

  case 13: /* backticks: '`'  */
#line 109 "../src/language/parse.y"
               {(yyval.val)=1;}
#line 2010 "../src/language/parse.c"
    break;

  case 14: /* backticks: backticks '`'  */
#line 110 "../src/language/parse.y"
                         {(yyval.val)=(yyvsp[-1].val)+1;}
#line 2016 "../src/language/parse.c"
    break;

  case 15: /* history: '%'  */
#line 113 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPhist,-1,-1,&(yyloc));}
#line 2022 "../src/language/parse.c"
    break;

  case 16: /* history: '%' "integer"  */
#line 114 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPhist,newintnode(&(yylsp[0])),-1,&(yyloc));}
#line 2028 "../src/language/parse.c"
    break;

  case 17: /* history: '%' backticks  */
#line 115 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPhist,newnode(Fsmall,-(yyvsp[0].val),-1,&(yyloc)),-1,&(yyloc));}
#line 2034 "../src/language/parse.c"
    break;

  case 18: /* history: '%' '#'  */
#line 116 "../src/language/parse.y"
                          {(yyval.val)=newopcall(OPhisttime,-1,-1,&(yyloc));}
#line 2040 "../src/language/parse.c"
    break;

  case 19: /* history: '%' '#' "integer"  */
#line 117 "../src/language/parse.y"
                          {(yyval.val)=newopcall(OPhisttime,newintnode(&(yylsp[0])),-1,&(yyloc));}
#line 2046 "../src/language/parse.c"
    break;

  case 20: /* history: '%' '#' backticks  */
#line 118 "../src/language/parse.y"
                          {(yyval.val)=newopcall(OPhisttime,newnode(Fsmall,-(yyvsp[0].val),-1,&(yyloc)),-1,&(yyloc));}
#line 2052 "../src/language/parse.c"
    break;

  case 21: /* deriv: '\''  */
#line 121 "../src/language/parse.y"
            {(yyval.val) = 1;}
#line 2058 "../src/language/parse.c"
    break;

  case 22: /* deriv: deriv '\''  */
#line 122 "../src/language/parse.y"
                  {(yyval.val) = (yyvsp[-1].val)+1;}
#line 2064 "../src/language/parse.c"
    break;

  case 23: /* expr: "integer"  */
#line 125 "../src/language/parse.y"
                          {(yyval.val)=newintnode(&(yylsp[0]));}
#line 2070 "../src/language/parse.c"
    break;

  case 24: /* expr: "real number"  */
#line 126 "../src/language/parse.y"
                          {(yyval.val)=newconst(CSTreal,&(yyloc));}
#line 2076 "../src/language/parse.c"
    break;

  case 25: /* expr: '.'  */
#line 127 "../src/language/parse.y"
                          {(yyval.val)=newconst(CSTreal,&(yyloc));}
#line 2082 "../src/language/parse.c"
    break;

  case 26: /* expr: "integer" '.' "variable name"  */
#line 128 "../src/language/parse.y"
                          {(yyval.val)=newnode(Ffunction,newconst(CSTmember,&(yylsp[0])),
                                                newintnode(&(yylsp[-2])),&(yyloc));}
#line 2089 "../src/language/parse.c"
    break;

  case 27: /* expr: "character string"  */
#line 130 "../src/language/parse.y"
                    {(yyval.val)=newconst(CSTstr,&(yyloc));}
#line 2095 "../src/language/parse.c"
    break;

  case 28: /* expr: '\'' "variable name"  */
#line 131 "../src/language/parse.y"
                    {(yyval.val)=newconst(CSTquote,&(yyloc));}
#line 2101 "../src/language/parse.c"
    break;

  case 29: /* expr: history  */
#line 132 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2107 "../src/language/parse.c"
    break;

  case 30: /* expr: expr '(' listarg ')'  */
#line 133 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fcall,(yyvsp[-3].val),(yyvsp[-1].val),&(yyloc));}
#line 2113 "../src/language/parse.c"
    break;

  case 31: /* expr: funcid  */
#line 134 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2119 "../src/language/parse.c"
    break;

  case 32: /* expr: lvalue  */
#line 135 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2125 "../src/language/parse.c"
    break;

  case 33: /* expr: matrix  */
#line 136 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2131 "../src/language/parse.c"
    break;

  case 34: /* expr: compr  */
#line 137 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2137 "../src/language/parse.c"
    break;

  case 35: /* expr: definition  */
#line 138 "../src/language/parse.y"
                        {(yyval.val)=(yyvsp[0].val);}
#line 2143 "../src/language/parse.c"
    break;

  case 36: /* expr: matrix '=' expr  */
#line 139 "../src/language/parse.y"
                      {(yyval.val)=newnode(Fassign,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2149 "../src/language/parse.c"
    break;

  case 37: /* expr: lvalue '=' expr  */
#line 140 "../src/language/parse.y"
                      {(yyval.val)=newnode(Fassign,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2155 "../src/language/parse.c"
    break;

  case 38: /* expr: lvalue "++"  */
#line 141 "../src/language/parse.y"
                      {(yyval.val)=newopcall(OPpp,(yyvsp[-1].val),-1,&(yyloc));}
#line 2161 "../src/language/parse.c"
    break;

  case 39: /* expr: lvalue "--"  */
#line 142 "../src/language/parse.y"
                      {(yyval.val)=newopcall(OPss,(yyvsp[-1].val),-1,&(yyloc));}
#line 2167 "../src/language/parse.c"
    break;

  case 40: /* expr: lvalue "*=" expr  */
#line 143 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPme,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2173 "../src/language/parse.c"
    break;

  case 41: /* expr: lvalue "/=" expr  */
#line 144 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPde,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2179 "../src/language/parse.c"
    break;

  case 42: /* expr: lvalue "\\/=" expr  */
#line 145 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPdre,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2185 "../src/language/parse.c"
    break;

  case 43: /* expr: lvalue "\\=" expr  */
#line 146 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPeuce,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2191 "../src/language/parse.c"
    break;

  case 44: /* expr: lvalue "%=" expr  */
#line 147 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPmode,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2197 "../src/language/parse.c"
    break;

  case 45: /* expr: lvalue "<<=" expr  */
#line 148 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPsle,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2203 "../src/language/parse.c"
    break;

  case 46: /* expr: lvalue ">>=" expr  */
#line 149 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPsre,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2209 "../src/language/parse.c"
    break;

  case 47: /* expr: lvalue "+=" expr  */
#line 150 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPpe,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2215 "../src/language/parse.c"
    break;

  case 48: /* expr: lvalue "-=" expr  */
#line 151 "../src/language/parse.y"
                         {(yyval.val)=newopcall(OPse,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2221 "../src/language/parse.c"
    break;

  case 49: /* expr: '!' expr  */
#line 152 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPnb,(yyvsp[0].val),-1,&(yyloc));}
#line 2227 "../src/language/parse.c"
    break;

  case 50: /* expr: '#' expr  */
#line 153 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPlength,(yyvsp[0].val),-1,&(yyloc));}
#line 2233 "../src/language/parse.c"
    break;

  case 51: /* expr: expr "||" expr  */
#line 154 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPor,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2239 "../src/language/parse.c"
    break;

  case 52: /* expr: expr "&&" expr  */
#line 155 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPand,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2245 "../src/language/parse.c"
    break;

  case 53: /* expr: expr '&' expr  */
#line 156 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPand,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2251 "../src/language/parse.c"
    break;

  case 54: /* expr: expr "===" expr  */
#line 157 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPid,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2257 "../src/language/parse.c"
    break;

  case 55: /* expr: expr "==" expr  */
#line 158 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPeq,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2263 "../src/language/parse.c"
    break;

  case 56: /* expr: expr "!=" expr  */
#line 159 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPne,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2269 "../src/language/parse.c"
    break;

  case 57: /* expr: expr ">=" expr  */
#line 160 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPge,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2275 "../src/language/parse.c"
    break;

  case 58: /* expr: expr '>' expr  */
#line 161 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPg,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2281 "../src/language/parse.c"
    break;

  case 59: /* expr: expr "<=" expr  */
#line 162 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPle,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2287 "../src/language/parse.c"
    break;

  case 60: /* expr: expr '<' expr  */
#line 163 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPl,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2293 "../src/language/parse.c"
    break;

  case 61: /* expr: expr '-' expr  */
#line 164 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPs,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2299 "../src/language/parse.c"
    break;

  case 62: /* expr: expr '+' expr  */
#line 165 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPp,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2305 "../src/language/parse.c"
    break;

  case 63: /* expr: expr "<<" expr  */
#line 166 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPsl,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2311 "../src/language/parse.c"
    break;

  case 64: /* expr: expr ">>" expr  */
#line 167 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPsr,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2317 "../src/language/parse.c"
    break;

  case 65: /* expr: expr '%' expr  */
#line 168 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPmod,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2323 "../src/language/parse.c"
    break;

  case 66: /* expr: expr "\\/" expr  */
#line 169 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPdr,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2329 "../src/language/parse.c"
    break;

  case 67: /* expr: expr '\\' expr  */
#line 170 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPeuc,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2335 "../src/language/parse.c"
    break;

  case 68: /* expr: expr '/' expr  */
#line 171 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPd,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2341 "../src/language/parse.c"
    break;

  case 69: /* expr: expr '*' expr  */
#line 172 "../src/language/parse.y"
                       {(yyval.val)=newopcall(OPm,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2347 "../src/language/parse.c"
    break;

  case 70: /* expr: '+' expr  */
#line 173 "../src/language/parse.y"
                          {(yyval.val)=(yyvsp[0].val);}
#line 2353 "../src/language/parse.c"
    break;

  case 71: /* expr: '-' expr  */
#line 174 "../src/language/parse.y"
                          {(yyval.val)=newopcall(OPn,(yyvsp[0].val),-1,&(yyloc));}
#line 2359 "../src/language/parse.c"
    break;

  case 72: /* expr: expr '^' expr  */
#line 175 "../src/language/parse.y"
                    {(yyval.val)=newopcall(OPpow,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2365 "../src/language/parse.c"
    break;

  case 73: /* expr: expr '~'  */
#line 176 "../src/language/parse.y"
               {(yyval.val)=newopcall(OPtrans,(yyvsp[-1].val),-1,&(yyloc));}
#line 2371 "../src/language/parse.c"
    break;

  case 74: /* expr: expr deriv  */
#line 177 "../src/language/parse.y"
                             {(yyval.val)=newopcall(OPderivn,(yyvsp[-1].val), newnode(Fsmall,(yyvsp[0].val),-1,&(yyloc)),&(yyloc));}
#line 2377 "../src/language/parse.c"
    break;

  case 75: /* expr: expr '!'  */
#line 178 "../src/language/parse.y"
                {(yyval.val)=newopcall(OPfact,(yyvsp[-1].val),-1,&(yyloc));}
#line 2383 "../src/language/parse.c"
    break;

  case 76: /* expr: expr matrix_index  */
#line 179 "../src/language/parse.y"
                        {(yyval.val)=newnode(Fmatcoeff,(yyvsp[-1].val),(yyvsp[0].val),&(yyloc));}
#line 2389 "../src/language/parse.c"
    break;

  case 77: /* expr: memberid  */
#line 180 "../src/language/parse.y"
               {(yyval.val)=(yyvsp[0].val);}
#line 2395 "../src/language/parse.c"
    break;

  case 78: /* expr: expr ':' "variable name"  */
#line 181 "../src/language/parse.y"
                        {(yyval.val)=newnode(Ftag,(yyvsp[-2].val),0,&(yyloc));}
#line 2401 "../src/language/parse.c"
    break;

  case 79: /* expr: '(' expr ')'  */
#line 182 "../src/language/parse.y"
                   {(yyval.val)=(yyvsp[-1].val);}
#line 2407 "../src/language/parse.c"
    break;

  case 80: /* lvalue: "variable name"  */
#line 185 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fentry,newconst(CSTentry,&(yylsp[0])),-1,&(yyloc));}
#line 2413 "../src/language/parse.c"
    break;

  case 81: /* lvalue: lvalue matrix_index  */
#line 186 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fmatcoeff,(yyvsp[-1].val),(yyvsp[0].val),&(yyloc));}
#line 2419 "../src/language/parse.c"
    break;

  case 82: /* lvalue: lvalue ':' "variable name"  */
#line 187 "../src/language/parse.y"
                            {(yyval.val)=newnode(Ftag,(yyvsp[-2].val),newconst(CSTentry,&(yylsp[-1])),&(yyloc));}
#line 2425 "../src/language/parse.c"
    break;

  case 83: /* exprno: expr  */
#line 190 "../src/language/parse.y"
             {(yyval.val)=(yyvsp[0].val);}
#line 2431 "../src/language/parse.c"
    break;

  case 84: /* exprno: %empty  */
#line 191 "../src/language/parse.y"
             {(yyval.val)=NOARG((yyloc));}
#line 2437 "../src/language/parse.c"
    break;

  case 85: /* matrixeltsno: matrixelts  */
#line 193 "../src/language/parse.y"
                         {(yyval.val)=(yyvsp[0].val);}
#line 2443 "../src/language/parse.c"
    break;

  case 86: /* matrixeltsno: %empty  */
#line 194 "../src/language/parse.y"
                   {(yyval.val)=NOARG((yyloc));}
#line 2449 "../src/language/parse.c"
    break;

  case 87: /* matrixelts: expr  */
#line 197 "../src/language/parse.y"
                 {(yyval.val)=(yyvsp[0].val);}
#line 2455 "../src/language/parse.c"
    break;

  case 88: /* matrixelts: matrixeltsno ',' exprno  */
#line 198 "../src/language/parse.y"
                                    {(yyval.val)=newnode(Fmatrixelts,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2461 "../src/language/parse.c"
    break;

  case 89: /* matrixlines: matrixelts ';' matrixelts  */
#line 201 "../src/language/parse.y"
                                        {(yyval.val)=newnode(Fmatrixlines,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2467 "../src/language/parse.c"
    break;

  case 90: /* matrixlines: matrixlines ';' matrixelts  */
#line 202 "../src/language/parse.y"
                                        {(yyval.val)=newnode(Fmatrixlines,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2473 "../src/language/parse.c"
    break;

  case 91: /* matrix: '[' ']'  */
#line 205 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fvec,-1,-1,&(yyloc));}
#line 2479 "../src/language/parse.c"
    break;

  case 92: /* matrix: '[' expr ".." expr ']'  */
#line 206 "../src/language/parse.y"
                               {(yyval.val)=newopcall(OPrange,(yyvsp[-3].val),(yyvsp[-1].val),&(yyloc));}
#line 2485 "../src/language/parse.c"
    break;

  case 93: /* matrix: '[' ';' ']'  */
#line 207 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fmat,-1,-1,&(yyloc));}
#line 2491 "../src/language/parse.c"
    break;

  case 94: /* matrix: '[' matrixelts ']'  */
#line 208 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fvec,(yyvsp[-1].val),-1,&(yyloc));}
#line 2497 "../src/language/parse.c"
    break;

  case 95: /* matrix: '[' matrixlines ']'  */
#line 209 "../src/language/parse.y"
                            {(yyval.val)=newnode(Fmat,(yyvsp[-1].val),-1,&(yyloc));}
#line 2503 "../src/language/parse.c"
    break;

  case 96: /* matrix: '[' error ']'  */
#line 210 "../src/language/parse.y"
                            {(yyval.val)=-1; YYABORT;}
#line 2509 "../src/language/parse.c"
    break;

  case 97: /* in: lvalue '<' '-' expr  */
#line 213 "../src/language/parse.y"
                        {(yyval.val)=newnode(Flistarg,(yyvsp[0].val),(yyvsp[-3].val),&(yyloc));}
#line 2515 "../src/language/parse.c"
    break;

  case 98: /* inseq: in  */
#line 216 "../src/language/parse.y"
                             {(yyval.val)=newopcall(OPcompr,(yyvsp[0].val),-2,&(yyloc));}
#line 2521 "../src/language/parse.c"
    break;

  case 99: /* inseq: in ',' expr  */
#line 217 "../src/language/parse.y"
                             {(yyval.val)=newopcall3(OPcompr,(yyvsp[-2].val),-2,(yyvsp[0].val),&(yyloc));}
#line 2527 "../src/language/parse.c"
    break;

  case 100: /* inseq: in ';' inseq  */
#line 218 "../src/language/parse.y"
                             {(yyval.val)=newopcall(OPcomprc,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2533 "../src/language/parse.c"
    break;

  case 101: /* inseq: in ',' expr ';' inseq  */
#line 219 "../src/language/parse.y"
                             {(yyval.val)=newopcall3(OPcomprc,(yyvsp[-4].val),(yyvsp[0].val),(yyvsp[-2].val),&(yyloc));}
#line 2539 "../src/language/parse.c"
    break;

  case 102: /* compr: '[' expr '|' inseq ']'  */
#line 222 "../src/language/parse.y"
                              {(yyval.val)=addcurrexpr((yyvsp[-1].val),(yyvsp[-3].val),&(yyloc));}
#line 2545 "../src/language/parse.c"
    break;

  case 103: /* arg: seq  */
#line 225 "../src/language/parse.y"
                {(yyval.val)=(yyvsp[0].val);}
#line 2551 "../src/language/parse.c"
    break;

  case 104: /* arg: lvalue '[' ".." ']'  */
#line 226 "../src/language/parse.y"
                         {(yyval.val)=newnode(Fvararg,(yyvsp[-3].val),-1,&(yyloc));}
#line 2557 "../src/language/parse.c"
    break;

  case 105: /* arg: '&' lvalue  */
#line 227 "../src/language/parse.y"
                {(yyval.val)=newnode(Frefarg,(yyvsp[0].val),-1,&(yyloc));}
#line 2563 "../src/language/parse.c"
    break;

  case 106: /* arg: '~' lvalue  */
#line 228 "../src/language/parse.y"
                {(yyval.val)=newnode(Findarg,(yyvsp[0].val),-1,&(yyloc));}
#line 2569 "../src/language/parse.c"
    break;

  case 107: /* $@1: %empty  */
#line 229 "../src/language/parse.y"
                {if (!pari_once) { yyerrok; } pari_once=1;}
#line 2575 "../src/language/parse.c"
    break;

  case 108: /* arg: arg error $@1 expr  */
#line 230 "../src/language/parse.y"
                     {pari_once=0; (yyval.val)=newopcall(OPcat,(yyvsp[-3].val),(yyvsp[0].val),&(yyloc));}
#line 2581 "../src/language/parse.c"
    break;

  case 109: /* listarg: arg  */
#line 233 "../src/language/parse.y"
             {(yyval.val)=(yyvsp[0].val);}
#line 2587 "../src/language/parse.c"
    break;

  case 110: /* listarg: listarg ',' arg  */
#line 234 "../src/language/parse.y"
                         {(yyval.val)=newnode(Flistarg,(yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2593 "../src/language/parse.c"
    break;

  case 111: /* funcid: "variable name" '(' listarg ')'  */
#line 237 "../src/language/parse.y"
                               {(yyval.val)=newnode(Ffunction,newconst(CSTentry,&(yylsp[-3])),(yyvsp[-1].val),&(yyloc));}
#line 2599 "../src/language/parse.c"
    break;

  case 112: /* memberid: expr '.' "variable name"  */
#line 240 "../src/language/parse.y"
                          {(yyval.val)=newnode(Ffunction,newconst(CSTmember,&(yylsp[0])),(yyvsp[-2].val),&(yyloc));}
#line 2605 "../src/language/parse.c"
    break;

  case 113: /* definition: "variable name" '(' listarg ')' '=' seq  */
#line 244 "../src/language/parse.y"
                                   {(yyval.val)=newfunc(CSTentry,&(yylsp[-5]),(yyvsp[-3].val),(yyvsp[0].val),&(yyloc));}
#line 2611 "../src/language/parse.c"
    break;

  case 114: /* definition: expr '.' "variable name" '=' seq  */
#line 246 "../src/language/parse.y"
                                   {(yyval.val)=newfunc(CSTmember,&(yylsp[-2]),newnode(Findarg,(yyvsp[-4].val),-1,&(yylsp[-4])),(yyvsp[0].val),&(yyloc));}
#line 2617 "../src/language/parse.c"
    break;

  case 115: /* definition: lvalue "->" seq  */
#line 247 "../src/language/parse.y"
                                         {(yyval.val)=newnode(Flambda, (yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2623 "../src/language/parse.c"
    break;

  case 116: /* definition: '(' listarg ")->" seq  */
#line 248 "../src/language/parse.y"
                                         {(yyval.val)=newnode(Flambda, (yyvsp[-2].val),(yyvsp[0].val),&(yyloc));}
#line 2629 "../src/language/parse.c"
    break;


#line 2633 "../src/language/parse.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == PARI_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, lex, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          goto yyexhaustedlab;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= PARI_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == PARI_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, lex);
          yychar = PARI_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, lex);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

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


#if 1
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, lex, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != PARI_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, lex);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, lex);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 251 "../src/language/parse.y"

