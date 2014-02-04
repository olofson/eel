/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_object.c - EEL Object
---------------------------------------------------------------------------
 * Copyright (C) 2004-2006, 2009-2010 David Olofson
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "EEL.h"
#include "EEL_value.h"
#include "e_object.h"
#include "e_operate.h"
#include "e_class.h"
#include "e_util.h"
#include "e_state.h"

#if DBGM(1) + 0 == 1
static inline void eeld_o_link(EEL_vm *vm, EEL_object *o)
{
	if(VMP->alast)
	{
		o2dbg(VMP->alast)->dnext = o;
		o2dbg(o)->dprev = VMP->alast;
		o2dbg(o)->dnext = NULL;
		VMP->alast = o;
	}
	else
	{
		VMP->afirst = VMP->alast = o;
		o2dbg(o)->dnext = o2dbg(o)->dprev = NULL;
	}
}

static inline void eeld_o_unlink(EEL_vm *vm, EEL_object *o)
{
	if(o2dbg(o)->dprev)
		o2dbg(o2dbg(o)->dprev)->dnext = o2dbg(o)->dnext;
	else
		VMP->afirst = o2dbg(o)->dnext;
	if(o2dbg(o)->dnext)
	{
		o2dbg(o2dbg(o)->dnext)->dprev = o2dbg(o)->dprev;
		o2dbg(o)->dnext = NULL;
	}
	else
		VMP->alast = o2dbg(o)->dprev;
	o2dbg(o)->dprev = NULL;
}
#endif


EEL_object *eel_o_alloc(EEL_vm *vm, int size, EEL_types type)
{
#if DBGM(1) + 0 == 1
	EEL_object *o = (EEL_object *)eel_malloc(vm,
			sizeof(EEL_object_dbg) + sizeof(EEL_object) + size);
#else
	EEL_object *o = (EEL_object *)eel_malloc(vm, sizeof(EEL_object) + size);
#endif
	if(!o)
		return NULL;
#if DBGM(1) + 0 == 1
	o = (EEL_object *)(((EEL_object_dbg *)o) + 1);
#endif
	o->type = type;
	o->lnext = o->lprev = NULL;
	o->vm = vm;
	o->refcount = 1;
	o->weakrefs = NULL;
	DBGM(++VMP->owns);
#if DBGK3(1) + 0 == 1
	printf("*<%s> (refcount = %d)\n", eel_typename(vm, o->type),
			o->refcount);
#endif
	DBGM(++VMP->created;)
#if DBGM(1) + 0 == 1
	eeld_o_link(vm, o);
	DBGM2(o2dbg(o)->dname = NULL;)
#endif
	if(o->type != EEL_CCLASS)
	{
		EEL_object *c = VMP->state->classes[o->type];
		eel_o_own(c);	/* o->type is a reference! */
	}
	return o;
}


static inline void o__dealloc(EEL_object *o)
{
#ifdef EEL_VM_CHECKING
	if(o->lprev || o->lnext)
	{
		fprintf(stderr, "INTERNAL ERROR: Someone forgot to unlink"
				" <%s> from limbo!\n", eel_o_stringrep(o));
		eel_limbo_unlink(o);
	}
#endif
#if DBGM(1) + 0 == 1
	++eel_vm2p(o->vm)->destroyed;
	eeld_o_unlink(o->vm, o);
# if DBGK(1) + 0 == 0
	eel_free(o->vm, o2dbg(o));
# endif
#else
# if DBGK(1) + 0 == 0
	eel_free(o->vm, o);
# endif
#endif
}


void eel_o__dealloc(EEL_object *o)
{
#ifdef EEL_VM_CHECKING
	if(!o)
	{
		fprintf(stderr, "INTERNAL ERROR: Someone tried to "
				"eel_o__dealloc() a NULL pointer!");
		DBGZ2(abort();)
		return;
	}
#endif
	o__dealloc(o);
}


/* Free object 'o' */
void eel_o_free(EEL_object *o)
{
	EEL_vm *vm;
#ifdef EEL_VM_CHECKING
	if(!o)
	{
		fprintf(stderr, "INTERNAL ERROR: Someone tried to "
				"eel_o_free() a NULL pointer!");
		DBGZ2(abort();)
		return;
	}
#endif
	vm = o->vm;
	eel_o_disown_nz(VMP->state->classes[o->type]);
	o__dealloc(o);
}


EEL_xno eel_o_metamethod(EEL_object *object,
		EEL_mmindex mm, EEL_value *op1, EEL_value *op2)
{
	return eel_o__metamethod(object, mm, op1, op2);
}


EEL_xno eel_o_construct(EEL_vm *vm, EEL_types type,
		EEL_value *inits, int initc, EEL_value *result)
{
#ifdef EEL_VM_CHECKING
	EEL_xno x;
	EEL_object *c;
	EEL_classdef *cd;
	if(type >= VMP->state->nclasses)
		return EEL_XBADCLASS;
	c = VMP->state->classes[type];
	if(!c)
		return EEL_XBADCLASS;
	cd = o2EEL_classdef(c);
	if(!cd->construct)
		return EEL_XNOCONSTRUCTOR;
	if(initc && !inits)
	{
		eel_msg(VMP->state, EEL_EM_IERROR,
				"Someone tried to construct a '%s' "
				"with %d initializers with a NULL\n"
				"initializer list pointer!",
				eel_typename(vm, type), initc);
		return EEL_XARGUMENTS;
	}
	x = (cd->construct)(vm, type, inits, initc, result);
	if(x)
	{
		result->type = EEL_TILLEGAL;
		return x;
	}
	if(type == EEL_CVECTOR)	/* Does this all the time... */
		return 0;
	if(EEL_TYPE(result) != type)
		eel_msg(VMP->state, EEL_EM_VMWARNING,
				"'%s' constructor returned an instance "
				"of type\n"
				"'%s' instead of the requested type!\n"
				"This could be intentional - or a bug.",
				eel_typename(vm, type),
				eel_typename(vm, EEL_TYPE(result)));
	return 0;
#else
	return eel_o__construct(vm, type, inits, initc, result);
#endif
}


EEL_xno eel_o__destruct(EEL_object *object)
{
	EEL_xno x;
	EEL_object *c;
	EEL_classdef *cd;
	EEL_vm *vm = object->vm;
#ifdef EEL_VM_CHECKING
	if(object->type >= VMP->state->nclasses)
		return EEL_XBADCLASS;
#endif
	c = VMP->state->classes[object->type];
#ifdef EEL_VM_CHECKING
	if(!c)
		return EEL_XBADCLASS;
#endif
	DBGM2(if(GETNAME(object))
		printf("Destroying object \"%s\"\n", GETNAME(object));)
	cd = o2EEL_classdef(c);
	x = (cd->destruct)(object);
	if(x == EEL_XREFUSE)
	{
		DBGM(++VMP->zombified;)
		return x;	/* Ok, ok, hang around then... */
	}
	eel_kill_weakrefs(object);
	eel_o_free(object);
	return 0;
}


void eel_own(EEL_object *o)
{
	eel_o_own(o);
}


void eel_disown(EEL_object *o)
{
	eel_o_disown_nz(o);
}


void eel_v_disown(EEL_value *v)
{
	switch(v->type)
	{
	  case EEL_TOBJREF:
		eel_disown(v->objref.v);
#ifdef DEBUG
		v->objref.v = NULL;
#endif
		break;
	  case EEL_TWEAKREF:
#ifdef EEL_VM_CHECKING
		if(v->objref.index == EEL_WEAKREF_UNWIRED)
		{
			fprintf(stderr, "ERROR: eel_v_disown() called on a "
					"weakref that was not attached!\n");
			return;
		}
#endif
		eel__weakref_detach(v);
#ifdef DEBUG
		v->objref.v = NULL;
#endif
		break;
	  default:
		break;
	}
}


void eel_copy(EEL_value *value, const EEL_value *from)
{
	eel_v_copy(value, from);
}


/*----------------------------------------------------------
	Handy metamethod wrappers
----------------------------------------------------------*/

long eel_length(EEL_object *io)
{
	EEL_value len;
	EEL_xno x = eel_o__metamethod(io, EEL_MM_LENGTH, NULL, &len);
	if(x)
		return -1;
	return len.integer.v;
}


EEL_xno eel_getindex(EEL_object *io, EEL_value *key, EEL_value *value)
{
	return eel_o__metamethod(io, EEL_MM_GETINDEX, key, value);
}


EEL_xno eel_getlindex(EEL_object *io, long ind, EEL_value *value)
{
	EEL_value keyv;
	eel_l2v(&keyv, ind);
	return eel_o__metamethod(io, EEL_MM_GETINDEX, &keyv, value);
}


EEL_xno eel_getsindex(EEL_object *io, const char *key, EEL_value *value)
{
	EEL_vm *vm = io->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(vm, key));
	if(!keyv.objref.v)
		return EEL_XCONSTRUCTOR;
	x = eel_o__metamethod(io, EEL_MM_GETINDEX, &keyv, value);
	eel_v_disown(&keyv);
	return x;
}


EEL_xno eel_setindex(EEL_object *io, EEL_value *key, EEL_value *value)
{
	return eel_o__metamethod(io, EEL_MM_SETINDEX, key, value);
}


EEL_xno eel_setlindex(EEL_object *io, long ind, EEL_value *value)
{
	EEL_value keyv;
	eel_l2v(&keyv, ind);
	return eel_o__metamethod(io, EEL_MM_SETINDEX, &keyv, value);
}


EEL_xno eel_setsindex(EEL_object *io, const char *key, EEL_value *value)
{
	EEL_vm *vm = io->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(vm, key));
	if(!keyv.objref.v)
		return EEL_XCONSTRUCTOR;
	x = eel_o__metamethod(io, EEL_MM_SETINDEX, &keyv, value);
	eel_v_disown(&keyv);
	return x;
}


EEL_xno eel_delete(EEL_object *io, EEL_value *key)
{
	return eel_o__metamethod(io, EEL_MM_DELETE, key, NULL);
}


EEL_xno eel_idelete(EEL_object *io, long ind, long count)
{
	EEL_value keyv, countv;
	eel_l2v(&keyv, ind);
	eel_l2v(&countv, count);
	return eel_o__metamethod(io, EEL_MM_DELETE, &keyv, &countv);
}


EEL_xno eel_sdelete(EEL_object *io, const char *key)
{
	EEL_vm *vm = io->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(vm, key));
	if(!keyv.objref.v)
		return EEL_XCONSTRUCTOR;
	x = eel_o__metamethod(io, EEL_MM_DELETE, &keyv, NULL);
	eel_v_disown(&keyv);
	return x;
}


EEL_xno eel_insert(EEL_object *io, EEL_value *key, EEL_value *value)
{
	return eel_o__metamethod(io, EEL_MM_INSERT, key, value);
}


EEL_xno eel_o_clone(EEL_vm *vm, EEL_object *orig, EEL_value *result)
{
	EEL_value from;
	eel_o2v(&from, orig);
	return eel_cast(vm, &from, result, EEL_TYPE(&from));
}
