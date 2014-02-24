/*
---------------------------------------------------------------------------
	e_function.c - EEL Function Class implementation
---------------------------------------------------------------------------
 * Copyright 2004-2005, 2009, 2011-2012 David Olofson
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

#include <stdlib.h>
#include <string.h>
#include "e_function.h"
#include "e_object.h"
#include "e_string.h"
#include "e_register.h"
#if DBGN(1)+0 == 1
# include <stdio.h>
#endif

/*----------------------------------------------------------
	Function Class
----------------------------------------------------------*/

static EEL_xno f_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EEL_function *f;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_function), type);
	if(!eo)
		return EEL_XMEMORY;
	f = o2EEL_function(eo);
	memset(f, 0, sizeof(EEL_function));
	eel_o2v(result,  eo);
	return 0;
}


static EEL_xno f_destruct(EEL_object *eo)
{
	EEL_vm *vm = eo->vm;
	EEL_function *f = o2EEL_function(eo);
	if(!(f->common.flags & EEL_FF_CFUNC))
	{
		int i;
		eel_free(vm, f->e.lines);
		eel_free(vm, f->e.code);
		DBGN(printf("--- Freeing constants of '%s' ---\n",
				eel_o2s(f->common.name));)
		for(i = 0; i < f->e.nconstants; ++i)
			eel_v_disown(&f->e.constants[i]);
		eel_free(vm, f->e.constants);
		DBGN(printf("--- Freed constants of '%s' ---\n",
				eel_o2s(f->common.name));)
	}
	eel_o_disown_nz(f->common.name);
	return 0;
}


static EEL_xno f_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_function_cd *cd = (EEL_function_cd *)eel_get_classdata(eo);
	if(!EEL_IS_OBJREF(op1->type))
		return EEL_XWRONGINDEX;
	if(op1->objref.v == cd->i_name)
	{
		eel_o2v(op2, o2EEL_function(eo)->common.name);
		eel_o_own(op2->objref.v);
		return 0;
	}
	else if(op1->objref.v == cd->i_module)
	{
		eel_o2v(op2, o2EEL_function(eo)->common.module);
		eel_o_own(op2->objref.v);
		return 0;
	}
	else if(op1->objref.v == cd->i_results)
	{
		eel_l2v(op2, o2EEL_function(eo)->common.results);
		return 0;
	}
	else if(op1->objref.v == cd->i_reqargs)
	{
		eel_l2v(op2, o2EEL_function(eo)->common.reqargs);
		return 0;
	}
	else if(op1->objref.v == cd->i_optargs)
	{
		eel_l2v(op2, o2EEL_function(eo)->common.optargs);
		return 0;
	}
	else if(op1->objref.v == cd->i_tupargs)
	{
		eel_l2v(op2, o2EEL_function(eo)->common.tupargs);
		return 0;
	}
	return EEL_XWRONGINDEX;
}


static void freenames(EEL_function_cd *cd)
{
	eel_o_disown_nz(cd->i_name);
	eel_o_disown_nz(cd->i_module);
	eel_o_disown_nz(cd->i_results);
	eel_o_disown_nz(cd->i_reqargs);
	eel_o_disown_nz(cd->i_optargs);
	eel_o_disown_nz(cd->i_tupargs);
}


static void f_unregister(EEL_object *classdef, void *classdata)
{
	EEL_vm *vm = classdef->vm;
	EEL_function_cd *cd = (EEL_function_cd *)classdata;
	freenames(cd);
	eel_free(vm, classdata);
}


void eel_cfunction_register(EEL_vm *vm)
{
	EEL_state *es = VMP->state;
	EEL_function_cd *cd;
	EEL_object *c = eel_register_class(vm,
			EEL_CFUNCTION, "function", EEL_COBJECT,
			f_construct, f_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, f_getindex);

	/* Set up classdata */
	cd = eel_malloc(vm, sizeof(EEL_function_cd));
	if(!cd)
		eel_serror(es, "Could not allocate classdata for EEL_function!\n");
	memset(cd, 0, sizeof(EEL_function_cd));
	if(!(		(cd->i_name = eel_ps_new(vm, "name")) &&
			(cd->i_module = eel_ps_new(vm, "module")) &&
			(cd->i_results = eel_ps_new(vm, "results")) &&
			(cd->i_reqargs = eel_ps_new(vm, "reqargs")) &&
			(cd->i_optargs = eel_ps_new(vm, "optargs")) &&
			(cd->i_tupargs = eel_ps_new(vm, "tupargs"))
		))
	{
		freenames(cd);
		eel_free(vm, cd);
		eel_serror(es, "Could not initialize EEL_function classdata!\n");
	}
	eel_set_unregister(c, f_unregister);
	eel_set_classdata(c, cd);
}


#define	FUNCTION_COMPARE_MASK	(EEL_FF_ARGS | EEL_FF_RESULTS)
int eel_function_compare(EEL_function *f1, EEL_function *f2)
{
	unsigned flags1, flags2;
	flags1 = f1->common.flags & FUNCTION_COMPARE_MASK;
	flags2 = f2->common.flags & FUNCTION_COMPARE_MASK;
	if(flags1 != flags2)
		return 0;
	if(f1->common.results != f2->common.results)
		return 0;
	if(f1->common.reqargs != f2->common.reqargs)
		return 0;
	if(f1->common.optargs != f2->common.optargs)
		return 0;
	if(f1->common.tupargs != f2->common.tupargs)
		return 0;
	if(f1->common.name != f1->common.name)
		return 0;
	return 1;
}


void eel_function_detach(EEL_function *f)
{
	/*
	 * Just kill all constants that refer to other
	 * functions in this module. When destroying a
	 * module, we have to do this before destroying
	 * any functions, or we'll get segfaults when
	 * checking whether objects are functions.
	 */
	int i;
	for(i = 0; i < f->e.nconstants; ++i)
	{
		EEL_object *o;
		if(!EEL_IS_OBJREF(f->e.constants[i].type))
			continue;
		o = f->e.constants[i].objref.v;
		if(((EEL_classes)o->type == EEL_CFUNCTION) &&
				(o2EEL_function(o)->common.module ==
						f->common.module))
			f->e.constants[i].type = EEL_TNIL;
	}
}
