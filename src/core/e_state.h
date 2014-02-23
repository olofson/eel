/*
---------------------------------------------------------------------------
	e_state.h - EEL State (Compiler, VM, symbols etc)
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2010, 2014 David Olofson
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

#ifndef EEL_E_STATE_H
#define	EEL_E_STATE_H

#include <setjmp.h>

#ifdef	BROKEN__LONGJMP
  /* Special case for Cygwin, which won't take _longjmp() for unknown reasons */
#	define	EEL_jmp_buf	jmp_buf
#	define	eel_setjmp(x)	setjmp(x)
#	define	eel_longjmp	longjmp
#else	/* BROKEN__LONGJMP */
/* Try to get the fastest versions available */
#if (defined(HAVE__SETJMP) || defined(HAVE_SETJMP)) &&	\
		(defined(HAVE__LONGJMP) || defined(HAVE_LONGJMP))
#	define	EEL_jmp_buf	jmp_buf
#	ifdef HAVE__SETJMP
#		define	eel_setjmp(x)	_setjmp(x)
#	else
#		define	eel_setjmp(x)	setjmp(x)
#	endif
#	ifdef HAVE__LONGJMP
#		define	eel_longjmp	_longjmp
#	else
#		define	eel_longjmp	longjmp
#	endif
#elif (defined(HAVE_SIGSETJMP) && defined(HAVE_SIGLONGJMP))
#	define	EEL_jmp_buf	sigjmp_buf
#	define	eel_setjmp(x)	sigsetjmp(x, 0)
#	define	eel_longjmp	siglongjmp
#else
#	error Cannot find any setjmp/longjmp implementation!
#	error Cannot build a usable compiler without this.
#endif
#endif	/* BROKEN__LONGJMP */

#include "ec_parser.h"


/*----------------------------------------------------------
	EEL Engine State
----------------------------------------------------------*/

#define	EEL_SBUFFERS	16
#define	EEL_SBUFSIZE	256

struct EEL_state
{
	/* 'environment' table */
	EEL_object	*environment;

	/* Registry */
	EEL_symbol	*root_symtab;
	unsigned	module_id_counter;

	/* Classes */
	int		nclasses;	/* # of classes defined */
	int		maxclasses;	/* Size of table */
	EEL_object	**classes;	/* Table of EEL_classdef objects */

	/* Casting Matrix */
	int		castersdim;	/* Width (and height) of cast matrix */
	EEL_cast_cb	*casters;	/* 2D matrix of cast callbacks */

	/* Lexer */
	EEL_lval	lval;		/* Current lval */
	int		token;		/* Current token */
	EEL_qualifiers	qualifiers;	/* Current qualifiers */
	EEL_lexitem	ls[3];		/* Lexer Stack */
	char		*lexbuf;
	int		lexbuf_length;
	
	/* .ess support */
	EEL_symbol	*tokentab[ESS_TOKENS];

	/* Compiler */
	EEL_context	*context;	/* Linked LIFO stack */
	EEL_vm		*vm;		/* Master/state VM */
/*
FIXME: 'path' probably belongs in the VM...
*/
	EEL_object	*path;		/* EEL_string object */

	/* Compiler error/warning handling */
	unsigned	last_module_id;
	EEL_jumpbuf	*jumpbufs;	/* For eel_try() etc */
	EEL_emessage	*firstmsg, *lastmsg;
	int		include_depth;	/* Circular include detection */
	EEL_object	*modnames;	/* Circular import detection */
	int		werror;		/* Warnings ==> errors (recursive!) */

	/* Misc */
	EEL_sbuffer	*firstfsb, *lastfsb;	/* Free sbuffers */
	EEL_sbuffer	*firstasb, *lastasb;	/* Allocated sbuffers */
	int		unique;		/* For eel_unique() */

	/* Modules */
	EEL_object	*modules;	/* (EEL table w/ weak references!) */
	EEL_object	*deadmods;	/* Modules that are kept around by
					 * external references.
					 */
	EEL_object	*eellib;	/* To keep it around... */
	int		module_lock;	/* >0 ==> module GC disabled */
};


static inline void eel_lock_modules(EEL_state *es)
{
	++es->module_lock;
}


void eel_unlock_modules(EEL_state *es);


/*
 * Exception handling.
 * A call to eel__cthrow() at any point inside an eel_try()
 * block, before the eel_done() call, will abort execution
 * and execute the code in the 'else' block.
 *
 * IMPORTANT:
 *	DO NOT call eel_done() from within an eel_try() block...
 *
 *	...EXCEPT before returning from within an eel_try() block.
 *
 *	DO NOT call eel_done() outside an eel_try() block.
 *
 * ---------------------------------------------------------------
 * !!! WARNING !!! WARNING !!! WARNING !!! WARNING !!! WARNING !!!
 * ---------------------------------------------------------------
 *	DO NOT use local variables to transfer state information
 *	from an eel_try() block into the corresponding exception
 *	handling block! If an exception occurs, such variables
 *	may be restored to whatever values they had before
 *	entering the eel_try() block, and thus, any changes made
 *	to them by the code in the eel_try() block may be lost.
 * ---------------------------------------------------------------
 * !!! WARNING !!! WARNING !!! WARNING !!! WARNING !!! WARNING !!!
 * ---------------------------------------------------------------
 */
/* Jump buffer for compiler exception handling */
struct EEL_jumpbuf
{
	EEL_jumpbuf	*prev;
	EEL_jmp_buf	buf;
	int		contexts;	/* # pushed since eel_try() */
};

void eel__try(EEL_state *es);

#define	eel_try(x)	if(eel__try(x), !eel_setjmp(x->jumpbufs->buf))	\
			{						\
				EEL_state *eel__statesave = x;

#define eel_except		eel_done(eel__statesave);		\
			} else

/*
 * Pop the current exception handling context for 'es'.
 */
void eel_done(EEL_state *es);

/* Throw an exception to be caught by the nearest eel_try() block. */
void eel__cthrow(EEL_state *es);

#endif	/* EEL_E_STATE_H */
