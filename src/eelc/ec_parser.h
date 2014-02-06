/*
---------------------------------------------------------------------------
	ec_parser.h - The EEL Parser/Compiler
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2010 David Olofson
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

#ifndef EEL_EC_PARSER_H
#define EEL_EC_PARSER_H

#include "e_eel.h"
#include "ec_bio.h"
#include "ec_lexer.h"
#include "ec_event.h"

/* Declaration qualifiers */
typedef enum
{
	EEL_QLOCAL =	0x0001,
	EEL_QSTATIC =	0x0002,
	EEL_QUPVALUE =	0x0004,
	EEL_QEXPORT =	0x0008,
	EEL_QSHADOW =	0x0010
} EEL_qualifiers;


/*----------------------------------------------------------
	Compiler Context
----------------------------------------------------------*/

/* Code bookmark, for forward jumps and stuff */
typedef struct EEL_codemark EEL_codemark;
struct EEL_codemark
{
	EEL_codemark	*next, *prev;
	int		pos;
	EEL_cestate	xstate;	/* eel_test_exit() result for the jump */
};

/* Flags for eel_context_push(): */

/*
 * Symbol table handling (pick at most ONE of these!):
 *
 *   BLOCK:
 *	Block. No symbol table; just a piece of code
 *	to be treated as a block in some way. (A block
 *	may be conditional, for example.)
 *
 *   BODY:
 *	Create a local symbol table for a body.
 *
 *   FUNCTION:
 *	Create a local symbol table for a function scope.
 *
 *   MODULE:
 *	Create a new MODULE symbol table.
 */
#define	ECTX_TYPE		0x00000000f
typedef enum
{
	ECTX_BLOCK = 1,
	ECTX_BODY,
	ECTX_MODULE,
	ECTX_FUNCTION
} EEL_ctxtypes;

/*
 * Create a local EEL_bio. (Clone of current bio.)
 */
#define	ECTX_CLONEBIO		0x00000010

/*
 * Context is breakable, ie one can jump
 * to it's end using break statements.
 */
#define	ECTX_BREAKABLE		0x00000020

/*
 * Context is continuable, ie one can jump to
 * it's start using 'continue'. A 'continue' statement
 * should allow the loop condition check, if any, to
 * determine whether or not the loop actually continues
 * running.
 */
#define	ECTX_CONTINUABLE	0x00000040

/*
 * Has a local event list, for managing conditional
 * code.
 */
#define	ECTX_CONDITIONAL	0x00000080

/*
 * Context is repeatable, which means 'repeat' is
 * allowed, and should restart the loop *bypassing*
 * any loop condition check.
 */
#define	ECTX_REPEATABLE		0x00000100

/*
 * Root function context. The top level symbols of a
 * root function may be exported from the module using
 * the 'export' qualifier, and the top level variables
 * become static variables by default.
 *
 * NOTE:
 *	The latter has the side effect of guaranteeing
 *	that there are no stack variables in the root
 *	function. Thus, there can be no upvalue access
 *	in exported functions (children of the root
 *	function) - so we don't need to check this
 *	explicitly!
 */
#define	ECTX_ROOT		0x00000200

/* Context is an exception catcher function; ie an 'except' block. */
#define	ECTX_CATCHER		0x00000400

/* No explicit code is to be parsed in this context. */
#define	ECTX_DUMMY		0x00000800

/* Context is wrapped in {} or equivalent; ie is a 'body'. */
#define	ECTX_WRAPPED		0x00001000

/*
 * Not really a context flag. This one tells some parser
 * rules to leave the context around when finishing, so
 * that aditional code can be added within the context
 * of the rule.
 *
 * NOTE: The "leave context" code is left out as well!
 */
#define	ECTX_KEEP		0x00008000

/* "Private" flags for EEL_context.flags only: */
#define	ECTX_OWNS_BIO		0x00020000

/*
 * Set this flag after adding a local coder, to have
 * eel_context_pop() destroy it automatically.
 *
 * NOTE:
 *	eel_context_pop() will NOT destroy the target
 *	pfunc of the coder. That is assumed to belong
 *	to some symbol somewhere.
 */
#define	ECTX_OWNS_CODER		0x00040000


struct EEL_context
{
	/* LIFO stack of contexts */
	EEL_context	*previous;

	/* Scope */
	EEL_symbol	*symtab;

	/* Source code */
	EEL_bio		*bio;
	EEL_object	*module;

	/* VM code generation */
	EEL_coder	*coder;
	int		startpos;		/* Start PC of this context */
	EEL_codemark	*endjumps, *lastej;	/* Jumps to end of context */
	EEL_codemark	*contjumps, *lastcj;	/* Jumps to loop test */

	/* Misc. */
	EEL_ctxtypes	type;
	int		flags;
	int		level;
	void		*creator;

	/*
	 * Compiler Events
	 */
	EEL_cevlist	*firstel, *lastel;

	DBGD(char *name;)
};

/* Context management */
void context_open(EEL_state *es);
void context_close(EEL_state *es);
void eel_context_push(EEL_state *es, int flags, const char *name);
void eel_context_pop(EEL_state *es);
EEL_context *eel_find_function_context(EEL_context *ctx);


/*
 * Compile module 'm'.
 */
void eel_compile(EEL_state *es, EEL_object *mo, EEL_sflags flags);

#endif	/* EEL_EC_PARSER_H */
