/*(LGPLv2.1)
---------------------------------------------------------------------------
	EEL_module.h - EEL code module management (API)
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2004-2006, 2009 David Olofson
 *
 * This library is free software;  you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation;  either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library  is  distributed  in  the hope that it will be useful,  but
 * WITHOUT   ANY   WARRANTY;   without   even   the   implied  warranty  of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library;  if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

	/*
	 * Serialization flags
	 */
	/* Generate binary file header */
	EEL_SF_HEADER =		0x00000100,
	/* Include full type info */
	EEL_SF_TYPEINFO =	0x00000200
} EEL_sflags;

EELAPI(EEL_object *)eel_load_from_mem(EEL_vm *vm,
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
