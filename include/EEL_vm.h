/*
---------------------------------------------------------------------------
	EEL_vm.h - EEL Virtual Machine (API)
---------------------------------------------------------------------------
 * Copyright 2005, 2006, 2009, 2011, 2014 David Olofson
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

#ifndef	EEL_VM_H
#define	EEL_VM_H

#include <stdarg.h>
#include "EEL_xno.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------
	VM API
------------------------------------------------------------
 *
 *	------ IMPORTANT NOTE ON EEL VM OBJECTS ------
 *
 *	Objects returned through  API calls have their
 *	reference  count incremented  to indicate that
 *	the application is holding references to them.
 *	(Just like  objects returned  to the VM, BTW.)
 *	The  application  must   use  eel_disown()  to
 *	release these objects  when done with them, or
 *	they will be locked in place "forever".
 */

/*
 * The VM struct
 *
 * NOTE:
 *	There's a private part following this public
 *	interface, so don't try any dirty tricks, ok? :-)
 */
struct EEL_vm
{
	/* Heap */
	int		heapsize;	/* # of elements total */
	EEL_value	*heap;		/* Array of EEL_value */

	/* Special registers */
	int		pc;		/* PC (index in code[]) */
	int		base;		/* Register base (heap index) */
	int		sp;		/* Stack pointer (heap index) */
	int		sbase;		/* Stack frame base (heap index) */

	/* Memory management */
	void *(*malloc)(EEL_vm *vm, int size);
	void *(*realloc)(EEL_vm *vm, void *block, int size);
	void (*free)(EEL_vm *vm, void *block);

/*FIXME:*/
/* For the C function API only! EEL functions use the callframe fields. */
int	resv;		/* Result */
int	argv;		/* First argument */
int	argc;		/* Argument count */
/*FIXME:*/
};

/* Get currently executing function object. */
EELAPI(EEL_object *)eel_current_function(EEL_vm *vm);

/* Get caller function object. */
EELAPI(EEL_object *)eel_calling_function(EEL_vm *vm);

/*
 * Push values onto the argument stack of 'vm'. Generally used for passing
 * arguments to functions.
 *
 * IMPORTANT:
 *	As of 0.3.0, objects pushed onto the argument stack have their
 *	refcounts incremented normally, and the refcounts are then decremented
 *	as the stack is cleared.
 *
 * Allowed control characters for 'fmt':
 *
 *	     EEL type	C call argument
 *		Description
 *	-----------------------------------------------
 *	*    <n/a>	<n/a>
 * 		Clear the argument stack.
 *
 *	R    <n/a>	int *
 *		Allocate a value for a result and set it up for use by the next
 *		call.
 *
 *	n    nil	<n/a>
 *		Push a 'nil' value.
 *
 *	i    integer	int
 *	r    real	double
 *	b    boolean	int
 *	s    string	const char *
 *	o    object	EEL_object *
 *	v    <any>	EEL_value *
 *		Push an integer, real, boolean, string, object or EEL value,
 *		respectively. A string value will be copied into a native EEL
 *		string as needed to function properly within the VM 
 *		environment.
 *
 * Returns 0 upon success, or a VM exception code if there was an error.
 */
EELAPI(EEL_xno)eel_argf(EEL_vm *vm, const char *fmt, ...);
EELAPI(EEL_xno)eel_vargf(EEL_vm *vm, const char *fmt, va_list args);

/*
 * Call 'f' with the current argument list, as set up by previous calls to
 * eel_argf().
 *
 * Returns 0 upon success, or a VM exception code if there was an error.
 */
EELAPI(EEL_xno)eel_call(EEL_vm *vm, EEL_object *f);

/*
 * Like eel_call(), but takes a module object and the name of the function to
 * call instead.
 */
EELAPI(EEL_xno)eel_calln(EEL_vm *vm, EEL_object *m, const char *fn);

/*
 * Call a function with arguments and result as specified by 'fmt'. If 'fmt' is
 * NULL, an empty argument list is passed to the function, and no space is
 * allocated for a result.
 *
 * eel_callf() takes the function object to call.
 *
 * eel_callnf() takes a module object and the name of the function in the
 * module to call.
 *
 * NOTE:
 *	These calls reset the argument passing mechanism before parsing 'fmt',
 *	as if the first control character was '*' - which consequently, is not
 *	needed with this call. This means that you cannot use them together
 *	with eel_argf(), since they'll throw away whatever values you have
 *	pushed.
 *
 * Returns 0 upon success, or a VM exception code if there was an error.
 */
EELAPI(EEL_xno)eel_callf(EEL_vm *vm, EEL_object *f, const char *fmt, ...);
EELAPI(EEL_xno)eel_callnf(EEL_vm *vm, EEL_object *m, const char *fn,
		const char *fmt, ...);

/*
 * Run 'vm', continuing from whatever state it is in.
 *
 * This can be used for step-time or "burst" execution of EEL scripts.
 *
 * Returns the current exception code (EEL_XOTHER if it's not an integer), or 0
 * (EEL_XNONE) if there is no exception (vm->exception is nil).
 */
EELAPI(EEL_xno)eel_run(EEL_vm *vm);


/*----------------------------------------------------------
	Memory management
----------------------------------------------------------*/
static inline void *eel_malloc(EEL_vm *vm, int size)
{
	return vm->malloc(vm, size);
}

static inline void *eel_realloc(EEL_vm *vm, void *block, int size)
{
	return vm->realloc(vm, block, size);
}

static inline void eel_free(EEL_vm *vm, void *block)
{
	return vm->free(vm, block);
}


/*----------------------------------------------------------
	Compiler API
----------------------------------------------------------*/

/* Open/Close an EEL VM. */
EELAPI(EEL_vm *)eel_open(int argc, const char *argv[]);
EELAPI(void)eel_close(EEL_vm *vm);

/*
 * Get an version of 'base' with a decimal number
 * appended, guaranteeing that the result is a
 * state wide unique string.
 */
EELAPI(char *)eel_unique(EEL_vm *vm, const char *base);

/*
 * Print the current error status of 'vm'.
 * If 'verbose' > 0 extra info, such as
 * where the error occurred, is printed.
 */
EELAPI(void)eel_perror(EEL_vm *vm, int verbose);

#ifdef __cplusplus
};
#endif

#endif /*  EEL_VM_H */
