/*
---------------------------------------------------------------------------
	EEL_module.h - EEL code module management (API)
---------------------------------------------------------------------------
 * Copyright 2002, 2004-2006, 2009, 2014 David Olofson
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

#ifndef EEL_MODULE_H
#define EEL_MODULE_H

#include "EEL_export.h"
#include "EEL_types.h"
#include "EEL_xno.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	/*
	 * Module loading flags
	 */
	/* Do not compile! */
	EEL_SF_NOCOMPILE =	0x00000001,
	/* Do not initialize module after compiling */
	EEL_SF_NOINIT =		0x00000002,

	/*
	 * Compiler flags
	 */
	/* List symbol tree after compiling */
	EEL_SF_LIST =		0x00000004,
	/* List VM asm code in the symbol tree */
	EEL_SF_LISTASM =	0x00000008,
	/* Turn compiler warnings into errors (give up after current module) */
	EEL_SF_WERROR =		0x00000010,
	/* Disable the operator precedence warning introduced in 0.3.7 */
	EEL_SF_NOPRECEDENCE =	0x00000020,
} EEL_sflags;

EELAPI(EEL_object *)eel_load_buffer(EEL_vm *vm,
		const char *source, unsigned len, EEL_sflags flags);

/*
 * Try to find or load module 'modname'.
 *
 * This call only wraps load() of builtin.eel, and the exact behavior is
 * determined by $.module_loaders, $.path_modules and $.injected_modules.
 *
 * SUCCESS: Returns a module EEL object.
 * FAILURE: Returns NULL.
 */
EELAPI(EEL_object *)eel_load(EEL_vm *vm, const char *modname,
		EEL_sflags flags);

/* Try to find loaded module 'modname'. */
EEL_object *eel_get_loaded_module(EEL_vm *vm, const char *modname);

/* Get moduledata field for module instance 'm' */
EELAPI(void *)eel_get_moduledata(EEL_object *mo);

/* Get moduledata for the module of the currently executing function */
EELAPI(void *)eel_get_current_moduledata(EEL_vm *vm);

#ifdef __cplusplus
}
#endif

#endif /*EEL_MODULE_H*/
