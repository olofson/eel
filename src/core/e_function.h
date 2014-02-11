/*
---------------------------------------------------------------------------
	e_function.h - EEL Function Class
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009, 2011 David Olofson
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

#ifndef	EEL_E_FUNCTION_H
#define	EEL_E_FUNCTION_H

#include "EEL.h"
#include "EEL_register.h"
#include "e_vm.h"


/*----------------------------------------------------------
	Function (C or EEL)
------------------------------------------------------------
 * Note that this is also used for the implicit top level
 * function of any EEL source file that contains code
 * that is not inside a function body! That is, *any* EEL
 * code compiles into one or more CFUNCTION objects.
 */

/* Flags */
typedef enum
{
	EEL_FF_CFUNC =		0x0001,
	EEL_FF_ARGS =		0x0002,
	EEL_FF_RESULTS =	0x0004,
	EEL_FF_DECLARATION =	0x0008,
	EEL_FF_ROOT =		0x0010,
	EEL_FF_EXPORT =		0x0020,
	EEL_FF_UPVALUES =	0x0040,
	EEL_FF_XBLOCK =		0x0080
} EEL_funcflags;

/* Common fields */
#define	EEL_FUNC_COMMON							\
	EEL_object	*module;	/* Parent module */		\
	EEL_object	*name;						\
	unsigned short	flags;						\
	unsigned char	results;	/* # of results */		\
	unsigned char	reqargs;	/* # of required args */	\
	unsigned char	optargs;	/* # of optional args */	\
	unsigned char	tupargs;	/* # of args in tuple */

#ifdef	EEL_PROFILING
#define	EEL_FUNC_DEBUG							\
	long long	rtime;	/* Time spent in this function */	\
	long		ccount;	/* Number of times called */
#else
#define	EEL_FUNC_DEBUG
#endif

typedef union
{
	struct
	{
		EEL_FUNC_COMMON
		EEL_FUNC_DEBUG
	} common;
	struct
	{
		EEL_FUNC_COMMON
		EEL_FUNC_DEBUG
		unsigned char	framesize;	/* Max # of regs used */
		unsigned char	cleansize;	/* Max size of cleaning table */

		/* Constants */
/* FIXME: Use an EEL_array instead! (Or maybe an EEL_table...?) */
		unsigned short	nconstants;
		EEL_value	*constants;

		/* Code */
/* FIXME: Use an EEL_dstring or EEL_vector! */
		int		codesize;	/* # of bytes */
		unsigned char	*code;		/* Code */

		/* Debug info */
		int		nlines;
		EEL_int32	*lines;		/* Source line numbers */
	} e;
	struct
	{
		EEL_FUNC_COMMON
		EEL_FUNC_DEBUG
		EEL_cfunc_cb	cb;
	} c;
} EEL_function;


/* Shared class data for EEL_function objects */
typedef struct
{
	EEL_object	*i_name;	/* string "name" */
	EEL_object	*i_module;	/* string "module" */
	EEL_object	*i_results;	/* string "results" */
	EEL_object	*i_reqargs;	/* string "reqargs" */
	EEL_object	*i_optargs;	/* string "optargs" */
	EEL_object	*i_tupargs;	/* string "tupargs" */
} EEL_function_cd;

/* Access and registration stuff */
EEL_MAKE_CAST(EEL_function)
void eel_cfunction_register(EEL_vm *vm);

/*
 * Compare two EEL_function structs to verify that they
 * have compatible call prototypes.
 *
 * Returns 1 if the structs are compatible, 0 otherwise.
 *
 * NOTE: This function makes no difference between C and
 *       EEL functions, nor does it distinguish between
 *       real functions and ones marked as declarations.
 *       It DOES check all argument counts, results, name
 *       and parent function.
 */
int eel_function_compare(EEL_function *f1, EEL_function *f2);

/* Prepare a function for having it's module destroyed. */
void eel_function_detach(EEL_function *f);

#endif /* EEL_E_FUNCTION_H */
