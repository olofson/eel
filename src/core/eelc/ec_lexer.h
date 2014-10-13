/*
---------------------------------------------------------------------------
	ec_lexer.h - EEL engine lexer
---------------------------------------------------------------------------
 * Copyright 2002, 2004-2006, 2009-2011, 2014 David Olofson
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef EEL_EC_LEXER_H
#define EEL_EC_LEXER_H

#include "e_eel.h"


/*----------------------------------------------------------
	Tokens and Unique User Token management
----------------------------------------------------------*/

typedef enum
{
	TK_VOID =	-4,	/* No result generated! */
	TK_WRONG =	-2,	/* Wrong rule! */
	TK_EOF =	0,	/* End of file or script */

	/* (All ASCII characters + the 128 8 bit codes go here.) */
	TK_LAST8BIT =	255,

	/* Values */
	TK_RNUM,	/* Real number */
	TK_INUM,	/* Integer number */
	TK_STRING,	/* String literal */
	TK_NAME,	/* New name (not found in symbol table) */

	/* Specials */
	TK_WEAKASSIGN,	/* Weak assignment operator */

	/* Keyword tokens */
	TK_KW_INCLUDE,
	TK_KW_IMPORT,
	TK_KW_AS,
	TK_KW_END,
	TK_KW_EELVERSION,

	TK_KW_RETURN,
	TK_KW_IF,
	TK_KW_ELSE,
	TK_KW_SWITCH,
	TK_KW_CASE,
	TK_KW_DEFAULT,
	TK_KW_FOR,
	TK_KW_DO,
	TK_KW_WHILE,
	TK_KW_UNTIL,
	TK_KW_BREAK,
	TK_KW_CONTINUE,
	TK_KW_REPEAT,

	TK_KW_TRY,
	TK_KW_UNTRY,
	TK_KW_EXCEPT,
	TK_KW_THROW,
	TK_KW_RETRY,
	TK_KW_EXCEPTION,

	TK_KW_LOCAL,
	TK_KW_STATIC,
	TK_KW_UPVALUE,
	TK_KW_EXPORT,
	TK_KW_SHADOW,
	TK_KW_CONSTANT,

	TK_KW_PROCEDURE,

	TK_KW_TRUE,
	TK_KW_FALSE,
	TK_KW_NIL,

	TK_KW_ARGUMENTS,
	TK_KW_TUPLES,
	TK_KW_SPECIFIED,

#define	case_all_TK_KW		\
	case TK_KW_INCLUDE:	\
	case TK_KW_IMPORT:	\
	case TK_KW_AS:		\
	case TK_KW_END:		\
	\
	case TK_KW_RETURN:	\
	case TK_KW_IF:		\
	case TK_KW_ELSE:	\
	case TK_KW_SWITCH:	\
	case TK_KW_CASE:	\
	case TK_KW_DEFAULT:	\
	case TK_KW_FOR:		\
	case TK_KW_DO:		\
	case TK_KW_WHILE:	\
	case TK_KW_UNTIL:	\
	case TK_KW_BREAK:	\
	case TK_KW_CONTINUE:	\
	case TK_KW_REPEAT:	\
	\
	case TK_KW_TRY:		\
	case TK_KW_UNTRY:	\
	case TK_KW_EXCEPT:	\
	case TK_KW_THROW:	\
	case TK_KW_RETRY:	\
	case TK_KW_EXCEPTION:	\
	\
	case TK_KW_LOCAL:	\
	case TK_KW_STATIC:	\
	case TK_KW_UPVALUE:	\
	case TK_KW_EXPORT:	\
	case TK_KW_SHADOW:	\
	case TK_KW_CONSTANT:	\
	\
	case TK_KW_PROCEDURE:	\
	\
	case TK_KW_TRUE:	\
	case TK_KW_FALSE:	\
	case TK_KW_NIL:		\
	\
	case TK_KW_ARGUMENTS:	\
	case TK_KW_TUPLES:	\
	case TK_KW_SPECIFIED

	/* Symbol types */
	TK_SYMBOL,	/* Symbol of type w/o token */
	TK_SYM_CONSTANT,
	TK_SYM_CLASS,
	TK_SYM_VARIABLE,
	TK_SYM_FUNCTION,
	TK_SYM_OPERATOR,
	TK_SYM_SHORTOP,
	TK_SYM_PARSER,
	TK_SYM_BODY,

	/* Grammar rule tokens */
	TK_EXPRESSION,
	TK_SIMPLEXP,
	TK_EXPLIST,
	TK_BODY,
	TK_BLOCK,
	TK_STATEMENT
} EEL_token;

/* Definitions for .ess stripped, tokenized source files */
#define	ESS_FIRST_TK	TK_KW_INCLUDE
#define	ESS_LAST_TK	TK_KW_SPECIFIED
typedef enum
{
	/* Operators */
	ESSX_TYPEOF = (ESS_LAST_TK - ESS_FIRST_TK + 1),
	ESSX_SIZEOF,
	ESSX_CLONE,
	ESSX_NOT,
	ESSX_AND,
	ESSX_OR,
	ESSX_XOR,
	ESSX_IN,

	/* Types */
	ESSX_REAL,
	ESSX_INTEGER,
	ESSX_BOOLEAN,
	ESSX_TYPEID,

	/* Classes */
	ESSX_STRING,
	ESSX_FUNCTION,
	ESSX_MODULE,
	ESSX_ARRAY,
	ESSX_TABLE,
	ESSX_VECTOR,
	ESSX_VECTOR_D,
	ESSX_VECTOR_F,
	ESSX_DSTRING,

	ESSX__END,
	ESSX_COUNT = (ESSX__END - ESSX_TYPEOF)
} ESS_xtokens;
#define	ESS_TOKEN_BASE	128
#define	ESS_TOKENS	(ESS_LAST_TK - ESS_FIRST_TK + 1 + ESSX_COUNT)


/*----------------------------------------------------------
	"Lexer Value"
----------------------------------------------------------*/

typedef enum
{
	ELVT_NONE = 0,
	ELVT_REAL,
	ELVT_INTEGER,
	ELVT_STRING,
	ELVT_SYMREF
} EEL_lvaltypes;

typedef struct EEL_lval
{
	EEL_lvaltypes	type;
	union
	{
		double		r;
		int		i;
		struct
		{
			int		len;
			char		*buf;
		} s;
		EEL_symbol	*symbol;
	} v;
} EEL_lval;

void eel_lval_copy_string(EEL_lval *lval, const char *s, int len);


/*----------------------------------------------------------
	The Lexer
----------------------------------------------------------*/

/* Lexer stack item, for unlexing */
typedef struct
{
	EEL_lval	lval;
	int		token;
	int		pos;
} EEL_lexitem;

/* Qualifiers for eel_lex() */
typedef enum
{
	ELF_REPORT_EOLN =	0x00000001,
	ELF_LOCALS_ONLY =	0x00000002,
	ELF_NO_OPERATORS =	0x00000004,
	ELF_CHARACTERS =	0x00000008,
	ELF_NO_SKIPWHITE =	0x00000010,
	ELF_DOTTED_NAME =	0x00000020
} EEL_lexflags;

int eel_lexer_init(EEL_state *es);
void eel_lexer_cleanup(EEL_state *es);
int eel_lexer_start(EEL_state *es, EEL_object *mo);
void eel_lexer_invalidate(EEL_state *es);

/*
 * Get next "token".
 * Flags can be used to temporarily change the lexical rules.
 */
int eel_lex(EEL_state *es, int flags);

/*
 * Push back last token + lval.
 * Works only for *one* token per context!
 */
void eel_unlex(EEL_state *es);

/*
 * Re-lex last token. (Use when 'flags' change between tokens.)
FIXME: It would probably be cleaner and safer to use a separate call
FIXME: to set the flags. That call would re-lex automatically.
FIXME: The "set flags" call should probably push the old flags onto
FIXME: a stack, and there should be a "pop" call to restore flags.
 */
int eel_relex(EEL_state *es, int flags);


/*
 * Get the bio read start position for 'item', where
 * 0 is the current item and positive numbers move
 * back up the stack to older items.
 *
 * Returns the bio position of the requested lexer
 * item, or -1 if the item is uninitialized, or -2
 * if 'item' has fallen out the bottom of the stack.
 */
int eel_lex_getpos(EEL_state *es, int item);

#endif /*EEL_EC_LEXER_H*/
