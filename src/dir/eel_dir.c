/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_dir.c - EEL Directory Handling
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009-2010 David Olofson
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "eel_dir.h"
#include "EEL_object.h"

typedef struct
{
	/* Class Type IDs */
	int dir_cid;
} D_moduledata;


/*----------------------------------------------------------
	directory class
----------------------------------------------------------*/
typedef struct
{
	EEL_object	*i_read;	// (string)
	EEL_object	*i_close;	// (string)
	EEL_object	*read;		// (cfunction)
	EEL_object	*close;		// (cfunction)
} EEL_directory_cd;


static EEL_xno d_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	const char *dn;
	EEL_directory *d;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_directory), type);
	if(!eo)
		return EEL_XMEMORY;
	d = o2EEL_directory(eo);
	if(initc != 1)
	{
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	dn = eel_v2s(initv);
	if(!dn)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
	d->handle = opendir(dn);
	if(!d->handle)
	{
		eel_o_free(eo);
		return EEL_XFILEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno d_destruct(EEL_object *eo)
{
	EEL_directory *d = o2EEL_directory(eo);
	if(d->handle)
		closedir(d->handle);
	return 0;
}


static EEL_xno d_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_directory_cd *cd = (EEL_directory_cd *)eel_get_classdata(eo);
	if(!EEL_IS_OBJREF(op1->type))
		return EEL_XWRONGINDEX;
	if(op1->objref.v == cd->i_read)
	{
		eel_own(cd->read);
		eel_o2v(op2, cd->read);
		return 0;
	}
	if(op1->objref.v == cd->i_close)
	{
		eel_own(cd->close);
		eel_o2v(op2, cd->close);
		return 0;
	}
	return EEL_XWRONGINDEX;
}


static EEL_xno d_read(EEL_vm *vm)
{
	D_moduledata *md = (D_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *arg = vm->heap + vm->argv;
	EEL_directory *d;
	EEL_object *s;
	struct dirent *de;
	if(EEL_TYPE(arg) != md->dir_cid)
		return EEL_XWRONGTYPE;
	d = o2EEL_directory(arg->objref.v);
	if(!d->handle)
		return EEL_XFILECLOSED;
	errno = 0;
	de = readdir(d->handle);
	if(!de)
	{
		if(errno)
			return EEL_XFILEREAD;
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	s = eel_ps_new(vm, de->d_name);
	if(!s)
		return EEL_XMEMORY;
	eel_o2v(vm->heap + vm->resv, s);
	return 0;
}


static EEL_xno d_close(EEL_vm *vm)
{
	D_moduledata *md = (D_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *arg = vm->heap + vm->argv;
	EEL_directory *d;
	if(EEL_TYPE(arg) != md->dir_cid)
		return EEL_XWRONGTYPE;
	d = o2EEL_directory(arg->objref.v);
	closedir(d->handle);
	d->handle = NULL;
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/
static EEL_xno d_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		eel_free(m->vm, eel_get_moduledata(m));
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/* unregister callback for the directory class */
static void d_unregister(EEL_object *classdef, void *classdata)
{
	EEL_vm *vm = classdef->vm;
	EEL_directory_cd *cd = (EEL_directory_cd *)classdata;
	eel_disown(cd->i_read);
	eel_disown(cd->i_close);
	eel_free(vm, classdata);
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

EEL_xno eel_dir_init(EEL_vm *vm)
{
	EEL_object *m;
	EEL_object *c;
	EEL_directory_cd *cd;
	D_moduledata *md = (D_moduledata *)eel_malloc(vm,
			sizeof(D_moduledata));
	if(!md)
		return EEL_XMEMORY;

	m = eel_create_module(vm, "dir", d_unload, md);
	if(!m)
	{
		eel_free(vm, md);
		return EEL_XMODULEINIT;
	}

	/* Register class 'directory' */
	c = eel_export_class(m, "directory", -1, d_construct, d_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, d_getindex);
	md->dir_cid = eel_class_typeid(c);

	/* Set up classdata */
	cd = eel_malloc(vm, sizeof(EEL_directory_cd));
	if(!cd)
	{
		eel_free(vm, md);
		return EEL_XMODULEINIT;
	}
	memset(cd, 0, sizeof(EEL_directory_cd));
	cd->read = eel_add_cfunction(m, 1, "read", 1, 1, 0, d_read);
	cd->close = eel_add_cfunction(m, 0, "close", 1, 0, 0, d_close);
	cd->i_read = eel_ps_new(vm, "read");
	cd->i_close = eel_ps_new(vm, "close");
	if(!(cd->read && cd->close && cd->i_read && cd->i_close))
	{
		eel_free(vm, md);
		eel_free(vm, cd);
		return EEL_XMODULEINIT;
	}
	eel_set_unregister(c, d_unregister);
	eel_set_classdata(c, cd);

	eel_disown(m);
	return 0;
}
