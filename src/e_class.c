/*
---------------------------------------------------------------------------
	e_class.c - EEL Class Definition
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009, 2012 David Olofson
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
#include "e_class.h"
#include "e_object.h"
#include "e_state.h"
#include "e_register.h"

#include "e_string.h"

/* Trap constructor that throws XNOCONSTRUCTOR */
static EEL_xno no_constructor(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	return EEL_XNOCONSTRUCTOR;
}

/* Dummy destructor that does nothing and says "OK!" */
static EEL_xno no_destructor(EEL_object *eo)
{
	return 0;
}

/* Handler for undefined metamethods */
EEL_xno eel_cclass_no_method(EEL_object *object,
		EEL_value *op1, EEL_value *op2)
{
	return EEL_XNOMETAMETHOD;
}

/* Default handler for EQ */
static EEL_xno eel_cclass_default_eq(EEL_object *object,
		EEL_value *op1, EEL_value *op2)
{
	if((object->type == EEL_TYPE(op1)) &&
			(object == eel_v2o(op1)))
		eel_b2v(op2, 1);
	else
		eel_b2v(op2, 0);
	return 0;
}

static EEL_xno c_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	int mm;
	EEL_classdef *cd;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_classdef), type);
	if(!eo)
		return EEL_XMEMORY;
	eo->refcount = 1;
	cd = o2EEL_classdef(eo);
	memset(cd, 0, sizeof(EEL_classdef));
	cd->construct = no_constructor;
	cd->destruct = no_destructor;
	for(mm = 0; mm < EEL_MM__COUNT; ++mm)
		cd->mmethods[mm] = eel_cclass_no_method;
	cd->mmethods[EEL_MM_EQ] = eel_cclass_default_eq;
	cd->registered = 1;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno c_destruct(EEL_object *eo)
{
	EEL_vm *vm = eo->vm;
	EEL_state *es = VMP->state;
	EEL_classdef *cd = o2EEL_classdef(eo);
	if(cd->unregister)
		cd->unregister(eo, cd->classdata);
	/*
	 * eel_close() will rip the names out
	 * to kill some circular references!
	 */
	if(cd->name)
		eel_o_disown(&cd->name);
	/* Remove ourselves from the class table */
	if(es->classes)
		es->classes[cd->typeid] = NULL;

	if((EEL_classes)cd->typeid == EEL_CCLASS)
	{
		/*
		 * CCLASS is the last one to be destroyed when
		 * closing, and we can't have the normal logic
		 * try to disown it's class - that is, itself.
		 */
		eel_o__dealloc(eo);
		return EEL_XREFUSE;
	}
	return 0;
}


void eel_cclass_register(EEL_vm *vm)
{
	eel_register_class(vm, EEL_CCLASS, "class", EEL_COBJECT,
			c_construct, c_destruct, NULL);
}


EEL_object *eel_cclass_construct(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value v;
	x = c_construct(vm, EEL_CCLASS, NULL, 0, &v);
	if(x)
		return NULL;
	return v.objref.v;
}
