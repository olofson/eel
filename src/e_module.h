/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_module.h - EEL code module management
---------------------------------------------------------------------------
 * Copyright (C) 2002-2006, 2009, 2011 David Olofson
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
EEL_object *eel_load_from_mem_nc(EEL_vm *vm,
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
