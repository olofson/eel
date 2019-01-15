/*
---------------------------------------------------------------------------
	EEL_register.h - EEL extension registry
---------------------------------------------------------------------------
 * Copyright 2002-2006, 2009-2010, 2014, 2019 David Olofson
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

#ifndef EEL_REGISTER_H
#define EEL_REGISTER_H

#include "EEL_export.h"
#include "EEL_types.h"
#include "EEL_object.h"
#include "EEL_xno.h"

#ifdef __cplusplus
extern "C" {
#endif


/*----------------------------------------------------------
	Binary or unary operator
----------------------------------------------------------*/
/* Operator is right associative */
#define	EOPF_RIGHT	0x00000001

/* Operator generates no result */
#define	EOPF_NORESULT	0x00000002

/* Operator cannot be used in shorthand assignment/operation statements */
#define	EOPF_NOSHORT	0x00000004

/* Cast operator priority */
#define	EEL_CAST_PRIORITY	100

EELAPI(int)eel_register_unop(EEL_vm *vm,
		const char *name, int op, int pri, int flags);
EELAPI(int)eel_register_binop(EEL_vm *vm,
		const char *name, int op, int pri, int flags);

/*
 * Register 'name' so that it generates 'token' when found in the source code
 * by the lexer.
 *
 * SUCCESS: Returns 0.
 * FAILURE: Returns a negative value.
 */
EELAPI(int)eel_register_keyword(EEL_vm *vm, const char *name, int token);

/* Set up callback 'ur' to be called when class 'c' is unregistered */
typedef void(*EEL_unregister_cb)(EEL_object *classdef, void *classdata);
EELAPI(void)eel_set_unregister(EEL_object *classdef, EEL_unregister_cb ur);

/* Set classdata for class 'cid' */
EELAPI(void)eel_set_classdata(EEL_vm *vm, unsigned cid, void *classdata);

/* Get classdata for class 'cid' */
EELAPI(void *)eel_get_classdata(EEL_vm *vm, unsigned cid);


/*----------------------------------------------------------
	Module Injection API
----------------------------------------------------------*/

/*
 * C callback for module unloading.
 *
 * NOTE:
 *	A host injected module should provide an 'unload' callback that returns
 *	EEL_XREFUSE when called with a 'closing' argument of 0! Otherwise the
 *	module will unload instantly when the application disowns it's
 *	reference to the module, as one normally does when done adding exports.
 *	   Alternatively, the host can own a reference to the module, and just
 *      hold on to that until the module is no longer needed.
 */
typedef EEL_xno (*EEL_unload_cb)(EEL_object *module, int closing);

/*
 * Create a new module object named 'name'. If 'unload' != NULL, 'unload' will
 * be called before the module object is destroyed. 'moduledata' is passed to C
 * functions exported from the module. The created module is shared by default!
 *
 * SUCCESS: Returns the new module object.
 * FAILURE: Returns NULL.
 */
EELAPI(EEL_object *)eel_create_module(EEL_vm *vm, const char *name,
		EEL_unload_cb unload, void *moduledata);

/*
 * Make module instance 'module' shared, allowing eel_import() and the 'import'
 * directive to retrieve this instance, instead of trying to load new
 * instances.
 */
EELAPI(int)eel_share_module(EEL_object *module);

/*
 * Register a new class and export it from 'module'. (Arguments 'name' through
 * 'destruct' are the same as for eel_register_class().)
 *
 * NOTE:
 *	The returned classdef reference borrows ownership from 'module'.
 */
EELAPI(EEL_object *)eel_export_class(EEL_object *module,
		const char *name, EEL_classes ancestor,
		EEL_ctor_cb construct, EEL_dtor_cb destruct,
		EEL_rector_cb reconstruct);

/*
 * Get the EEL class ID of class 'c'.
 *
 * Returns -1 if 'c' is not an EEL_classdef object, or if there's some other
 * problem.
 */
EELAPI(EEL_classes)eel_class_cid(EEL_object *c);

/*
 * Set callback for metamethod 'mm' of class 'cid' to 'cb'. 'cid' must refer to
 * a previously registered class.
 *
 * SUCCESS: Returns 0.
 * FAILURE: Returns an EEL exception number.
 */
EELAPI(EEL_xno)eel_set_metamethod(EEL_object *c, EEL_mmindex mm, EEL_mm_cb cb);

/*
 * Register cast method 'cb' for casting type 'from' into type 'to'. 'from' and
 * 'to' most be type or class IDs, or one of the wildcards EEL_CANY or
 * EEL_CINDEXABLE.
 *    Note that EEL_CINDEXABLE does *not* match permutations that are already
 * defined.
 *
 * SUCCESS: Returns 0.
 * FAILURE: Returns an EEL exception number.
 */
EELAPI(EEL_xno)eel_set_casts(EEL_vm *vm, int from, int to, EEL_cast_cb cb);

/*
 * C callback for calls from the EEL VM. The callback should return 0 if all is
 * well, or an exception number in case of error, to make the call throw a VM
 * exception.
 *
 * Arguments and results are passed via the register frame that the VM sets up
 * for the C function. (Same as for EEL functions.)
 */
typedef EEL_xno (*EEL_cfunc_cb)(EEL_vm *vm);

/*
 * Export C function from 'module'. C function 'func' is named 'name', and must
 * take 'args' fixed arguments, and up to 'optargs' optional arguments,
 * followed by any number of groups of 'tupargs' arguments, if 'tupargs' != 0.
 * It must return 'results' results.
 *
 * 'results', 'args', 'optargs' and 'tupargs' must be 0 or higher.
 *
 * The function object created is owned and managed by the module, and the
 * function is added to the module's export table.
 *
 * SUCCESS: Returns the EEL_function object.
 * FAILURE: Returns NULL.
 */
EELAPI(EEL_object *)eel_export_cfunction(EEL_object *module,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func);

/*
 * Same as eel_export_cfunction(), except the function is NOT added to the
 * module export table, so it will only be directly visible to script code
 * compiled in the scope of the module.
 */
EELAPI(EEL_object *)eel_add_cfunction(EEL_object *module,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func);

/*
 * Export constant value 'value' from 'module', named 'name'.
 *
 * SUCCESS: Returns 0.
 * FAILURE: Returns an EEL exception number.
 *
 * WARNING:
 *	Though the *value* is treated as a constant by the EEL compiler (which
 *	means the only way EEL code can change it is by abusing "internal" low
 *	level features), objects referred to by exported values do not enjoy
 *	any special protection! If they're not read-only by nature (like the
 *	normal immutable strings), they can be modified without restriction.
 *	However, they cannot be destroyed, as the export adds a reference.
 */
EELAPI(EEL_xno)eel_export_constant(EEL_object *module,
		const char *name, EEL_value *value);
EELAPI(EEL_xno)eel_export_lconstant(EEL_object *module,
		const char *name, long value);
EELAPI(EEL_xno)eel_export_dconstant(EEL_object *module,
		const char *name, double value);
EELAPI(EEL_xno)eel_export_sconstant(EEL_object *module,
		const char *name, const char *value);

EELAPI(EEL_xno)eel_export_lconstants(EEL_object *module,
		const EEL_lconstexp *data);

#ifdef __cplusplus
};
#endif

#endif /*EEL_REGISTER_H*/
