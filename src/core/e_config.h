/*
---------------------------------------------------------------------------
	e_config.h - EEL Compile Time Configuration
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2011, 2014 David Olofson
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

#ifndef EEL_E_CONFIG_H
#define EEL_E_CONFIG_H

/* Pull in the platform check results */
#include "config.h"

/*
 * Assumed tab size for error column calculation.
 * (Of course, tabs ARE 8 chars, but anyway... ;-D)
 */
#define	EEL_TAB_SIZE	8

/*
 * Undef to compile without the built-in library.
 * Leave this defined unless you know what you're doing!
 */
#define	EEL_USE_EELBIL

/*
 * Default printf() format string for converting TREAL to string.
FIXME: We need a proper "format engine" of some sort for this! Simply piggy-back
FIXME: on C's *printf()? We could just have EEL print() "eat" arguments as
FIXME: required when C printf() format strings are encountered.
FIXME: BTW, as opposed to C, EEL can actually verify that arguments are there
FIXME: and of suitable types!
 */
#define	EEL_REAL_FMT	"%.12g"


/*---------------------------------------------------------
	VM Configuration
---------------------------------------------------------*/

/*
 * Initial VM heap size. (Number of EEL values)
 */
#define	EEL_INITHEAP	256

/*
 * Minimum number of values to allocate for the argument
 * stack when setting up a new function register frame.
 * (Just to avoid another reallocation right after entering
 * a function that uses the argument stack. This memory is
 * not initialized or anything, unless it's actually used.)
 */
#define	EEL_MINSTACK	32

/*
 * Memory allocation parameters for dynamic sized types.
 */
#define	EEL_DSTRING_SIZEBASE	32
#define	EEL_TABLE_SIZEBASE	4
#define	EEL_ARRAY_SIZEBASE	8
#define	EEL_VECTOR_SIZEBASE	8

/*
 * Define to have the '/' operator always generate real
 * type results, Pascal style.
 */
#define	EEL_PASCAL_IDIV

/*
 * Destroy garbage objects instantly and in-line.
 * (Very close to pure refcounting behavior.)
 *
 * NOTE:
 *	The string pool cache is not affected
 *	by this option. See EEL_STRING_CACHE
 */
#define EEL_HYPER_AGGRESSIVE_MM

/* Enable caching of "dead" strings. */
#define	EEL_CACHE_STRINGS

/* Default size of string cache. (Number of string objects.) */
#define	EEL_DEFAULT_STRING_CACHE 100

/*
 * Define this to have eel_calcresize() (used for reallocating
 * tables, arrays, vectors etc) back off a little on the shrinking.
 * Use this if realloc() is too aggressive about moving memory
 * blocks when shrinking them!
 */
#define	EEL_DEFENSIVE_REALLOC

/*
 * Use this with Valgrind and similar tools, or there will be bogus complaints
 * about accessing uninitialized memory! (And it's faster most of the time
 * anyway, so as of 0.1.15, it's no longer DEBUG-only by default.)
 */
#undef	EEL_CLEAN_COPY

/*
 * Check bytecode for errors caused by compiler bugs.
 * This slows down the VM and adds some error messages
 * to the footprint!
 */
#ifdef DEBUG
#  define	EEL_VM_CHECKING
#else
#  undef	EEL_VM_CHECKING
#endif

/*
 * Use threaded VM instruction dispatcher; "computed goto"
 * (Significantly faster than the switch() dispatcher!)
 *
 * WARNING:
 *	Combined with EEL_VMCHECK and/or DBG4, this can
 *	generate a large (and slow) dispatcher!
 */
#ifdef __GNUC__
#define EEL_VM_THREADED
#endif

/*
 * Enable call profiling. This causes the VM to build
 * statistics on all C and EEL function calls, including
 * average and maximum time spent in each function.
 */
#undef	EEL_PROFILING

/*
 * Enable VM instruction profiling. This has the VM sum
 * executions and execution times per VM opcode, printing
 * statistics to stdout when the VM is closed.
 */
#undef	EEL_VM_PROFILING

/*
 * Keep global count of objects and refcounts.
 */
#ifdef DEBUG
#  define	EEL_OBJECT_ACCOUNTING
#else
#  undef	EEL_OBJECT_ACCOUNTING
#endif


/*---------------------------------------------------------
	Compiler Features and Optimizations
---------------------------------------------------------*/

/*
 * Eliminate dead code in the code generator,
 * based on compiler event state.
 */
#define	EEL_DEAD_CODE_ELIMINATION
/* Fill dead code areas with ILLEGAL, NOP, NOP, ... */
#ifdef DEBUG
#  define	EEL_DEAD_CODE_ILLEGAL
#else
#  undef	EEL_DEAD_CODE_ILLEGAL
#endif

/* Substitute recognized instruction sequences with faster equivalents */
#define	EEL_PEEPHOLE_OPTIMIZER

/* Maximum number of recursive include levels */
#define	EEL_MAX_INCLUDE_DEPTH	1000


/*---------------------------------------------------------
	Debug output and checking options
---------------------------------------------------------*/
#ifdef DEBUG

#include <stdio.h>

/*--- BEGIN ---------------------------------------------*/

/* Error logging */
#define	DBGZ(x)	x	/* Print all messages instantly */
#define	DBGZ2(x) x	/* Internal errors -> abort() */

/* Symbol table */
#define	DBG2(x)		/* Hit messages */
#define	DBG3(x)		/* Heavy search debug output */
#define	DBGI(x)		/* Print every tested symbol */
#define	DBGJ(x)		/* eel_find_symbol() */

/* Lexer */
#define	DBGT(x)		/* Print every lexed token */

/* Parser */
#define	DBGD(x)		/* Context management */
#define	DBGE(x)		/* Control flow */
#define	DBGE2(x) 	/* Expressions */
#define	DBGF(x)		/* Declarations + getting variables */
#define	DBGG(x)		/* Register allocation */
#define	DBGH(x)		/* Grammar rules */
#define	DBGH2(x) 	/* Print every returned token */

/* Compiler Tools */
#define	DBGA(x)		/* Extra checking when adding events */
#define	DBGB(x)		/* Event list management */
#define	DBGB2(x) 	/* Event status dump per VM instruction */
#define	DBGC(x)		/* get_var_name() and test_ev() */
#define	DBGC2(x) 	/* test_ev() context scanning */
#define	DBGX(x)		/* Compiler exception handling messages */
#define	DBGY(x)		/* Dump symbol table + code after compile */
#define	DBGY2(x) 	/* Dump symbols + code on warnings and errors */
#define	DBGX2(x) 	/* Module export debug messages */

/* Coder */
#define	DBG8(x)		/* Dump generated code */
#define	DBG9(x)		/* Add some extra comments to the code */
#define	DBG9B(x) 	/* Add conditional/dead status info */
#define	DBG9C(x) 	/* Print register allocation info */
#define	DBG9D(x) 	/* Peephole optimizer debug output */

/* VM debugging (minor performance impact) */
#define	DBG6B(x) 	/* VM instruction counter */

/* VM debugging (insane bandwidth!) */
#define	DBG4(x)		/* Run-time code dumping */
#define	DBG4B(x) 	/* Function entrance info */
#define	DBG4C(x) 	/* eel_argf() and call frame info */
#define	DBG4D(x) 	/* Dump names of called functions */
#define	DBG4E(x) 	/* Callframe debugging */
#define	DBG5(x)		/* Scheduler */
#define	DBG5B(x) 	/* Dump on every exception that hits the dispatcher */
#define	DBG6(x)		/* Operations */
#define	DBG7(x)		/* Memory management */
#define	DBG7S(x) 	/* Argument stack */
#define	DBG7W(x) 	/* Weakref management */

/* Resource management - high bandwidth */
#define	SDBG(x)		/* "sbuffer" string allocator */
#define	PSDBG2(x) 	/* Pooled string find/add/destroy messages */
#define	PSDBG3(x) 	/* Pooled string cache messages */
#define	DBGK3(x) 	/* Refcount + limbo messages */
#define	DBGK4(x) 	/* eel_v_receive() and result passing */
#define	DBGL(x)		/* Print size of allocated objects */

/* Resource management */
#define	PSDBG(x) 	/* String pool analysis when closing */
#define	DBG1(x)		/* Module loading and unloading */
#define	DBG11(x) 	/* eel_s_add_exports() messages */
#define	DBG12(x) 	/* Module garbage collection */
#define	DBGK(x) 	/* Refcount checking (Memory not freed!) */
#define	DBGK2(x) 	/* Destruction messages in eel_close() */
#define	DBGM2(x) 	/* Name interesting objects */
#define	DBGN(x)		/* Constant table destruction messages */
#define	DBGN2(x) 	/* Static variable management messages */
#define	DBGO(x)		/* Class registry debug messages */
/*--- END -----------------------------------------------*/

#else
#define	DBGZ(x)
#define	DBGZ2(x)

#define	DBG2(x)
#define	DBG3(x)
#define	DBGI(x)
#define	DBGJ(x)

#define	DBGT(x)

#define	DBGD(x)
#define	DBGE(x)
#define	DBGE2(x)
#define	DBGF(x)
#define	DBGG(x)
#define	DBGH(x)
#define	DBGH2(x)

#define	DBGA(x)
#define	DBGB(x)
#define	DBGB2(x)
#define	DBGC(x)
#define	DBGC2(x)
#define	DBGX(x)
#define	DBGY(x)
#define	DBGY2(x)
#define	DBGX2(x)

#define	DBG8(x)
#define	DBG9(x)
#define	DBG9B(x)
#define	DBG9C(x)
#define	DBG9D(x)

#define	DBG6B(x)

#define	DBG4(x)
#define	DBG4B(x)
#define	DBG4C(x)
#define	DBG4D(x)
#define	DBG4E(x)
#define	DBG5(x)
#define	DBG5B(x)
#define	DBG6(x)
#define	DBG7(x)
#define	DBG7S(x)
#define	DBG7W(x)

#define	SDBG(x)
#define	PSDBG2(x)
#define	PSDBG3(x)
#define	DBGK3(x)
#define	DBGK4(x)
#define	DBGL(x)

#define	PSDBG(x)
#define	DBG1(x)
#define	DBG11(x)
#define	DBG12(x)
#define	DBGK(x)
#define	DBGK2(x)
#define	DBGM2(x)
#define	DBGN(x)
#define	DBGN2(x)
#define	DBGO(x)
#endif


/*---------------------------------------------------------
	Various configuration dependent macros
---------------------------------------------------------*/

#if defined(EEL_OBJECT_ACCOUNTING) || (DBGM2(1)+0 == 1)
#  define	DBGM(x)	x
#else
#  define	DBGM(x)
#endif

#ifdef	EEL_VM_CHECKING
#  define	EEL_VMCHECK(x)	x
#else
#  define	EEL_VMCHECK(x)
#endif


/*---------------------------------------------------------
	Global debugging variables
---------------------------------------------------------*/
#if DBGM(1)+0 == 1
typedef struct EEL_all_objects EEL_all_objects;
extern EEL_all_objects all_objects;
#endif

#endif /* EEL_E_CONFIG_H */
