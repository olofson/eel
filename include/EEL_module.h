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
	 * Compiler flags
	 */
	/* Compile! */
	EEL_SF_COMPILE =	0x00000001,
	/* Initialize module after compiling */
	EEL_SF_INITIALIZE =	0x00000002,
	/* List symbol tree after compiling */
	EEL_SF_LIST =		0x00000004,
	/* List VM asm code in the symbol tree */
	EEL_SF_LISTASM =	0x00000008,
	/* Turn compiler warnings into errors (give up after current module) */
	EEL_SF_WERROR =		0x00000010,

	/*
	 * Serialization flags
	 */
	/* Generate binary file header */
	EEL_SF_HEADER =		0x00000100,
	/* Include full type info */
	EEL_SF_TYPEINFO =	0x00000200
} EEL_sflags;

EELAPI(EEL_object *)eel_load_buffer(EEL_vm *vm,
		const char *source, unsigned len, EEL_sflags flags);

/*
 * Load and compile or deserialize file 'filename'.
 *
 * If the file begins with a valid EEL binary header,
 * it is deserialized, and the resulting object is returned.
 *
 * If the file does not seem to be a valid EEL binary,
 * it is assumed to be an EEL source file, and is compiled
 * into a module that is returned.
 *
 * SUCCESS: Returns an object.
 * FAILURE: Returns NULL.
 */
EELAPI(EEL_object *)eel_load(EEL_vm *vm, const char *filename,
		EEL_sflags flags);

/*
 * Serialize 'object' to file 'filename'.
 */
EELAPI(EEL_xno)eel_save(EEL_vm *vm, EEL_object *object,
		const char *filename, EEL_sflags flags);

/*
 * Try to find or load module 'modname'.
 *
 * If no module named 'modname' is loaded, EEL will try
 * to load "<modname>.eec", and if that fails, it will try
 * to load and compile "<modname>.eel".
 *
 * If a module named 'modname' is already loaded, it will
 * be returned, with an incremented refcount.
 *
 * NOTE:
 *	Modules should declare proper module names using
 *	the 'module' directive! eel_import() will not find
 *	modules that use the file name (default) for module
 *	name, and will load and compile one instance for
 *	each request!
 *
 * SUCCESS: Returns a module object.
 * FAILURE: Returns NULL.
 */
EELAPI(EEL_object *)eel_import(EEL_vm *vm, const char *modname);


/* Get moduledata field for module instance 'm' */
EELAPI(void *)eel_get_moduledata(EEL_object *mo);

/* Get moduledata for the module of the currently executing function */
EELAPI(void *)eel_get_current_moduledata(EEL_vm *vm);

#ifdef __cplusplus
}
#endif

#endif /*EEL_MODULE_H*/
