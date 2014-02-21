/*
---------------------------------------------------------------------------
	e_module.h - EEL code module management
---------------------------------------------------------------------------
 * Copyright 2002-2006, 2009, 2011, 2014 David Olofson
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

#ifndef EEL_E_MODULE_H
#define EEL_E_MODULE_H

#include "EEL_register.h"
#include "EEL_module.h"
#include "e_util.h"

typedef EEL_object *EEL_object_p;
EEL_DARRAY(eel_objs_, EEL_object_p)

typedef struct
{
	unsigned	id;		/* Long time "unique" ID. */

	EEL_object	*exports;	/* Exports (EEL_table) */

	/* Source code */
	unsigned	len;
	unsigned char	*source;

	/* All functions and any other permanently owned objects */
	EEL_object_p_da	objects;
	/*
	 * Initial refcount sum for permanent objects.
	 * When the actual refcount sum returns to this
	 * value, it should be safe to unload the module.
	 */
	unsigned	refsum;

	/* Static variables */
	unsigned	nvariables;
	unsigned	maxvariables;	/* FIXME: This is compiler stuff! */
	EEL_value	*variables;
#if 0
TODO:	/* Constants */
TODO: These are intended to replace the per-function constants,
TODO: to minimize duplication of data.
	unsigned	nconstants;
	EEL_value	*constants;
#endif
	EEL_unload_cb	unload;

	/* Module instance data */
	void		*moduledata;
} EEL_module;

/* Access and registration stuff */
EEL_MAKE_CAST(EEL_module);
void eel_cmodule_register(EEL_vm *vm);

/* Load a file as a module, but don't compile or initialize it. */
EEL_object *eel_load_nc(EEL_vm *vm, const char *filename, EEL_sflags flags);

/* Load a string as a module, but don't compile or initialize it. */
EEL_object *eel_load_buffer_nc(EEL_vm *vm,
		const char *source, unsigned len, EEL_sflags flags);

/* Count references to compiler generated objects */
int eel_module_countref(EEL_object *mo);

/*
 * Garbage collect the "potentially dead modules" list.
 * Returns the number of modules actually unloaded.
 */
int eel_clean_modules(EEL_vm *vm);

const char *eel_module_modname(EEL_object *m);
const char *eel_module_filename(EEL_object *m);

#endif /*EEL_E_MODULE_H*/
