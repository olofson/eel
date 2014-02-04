/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_array.c - EEL Array Class implementation
---------------------------------------------------------------------------
 * Copyright (C) 2004-2006, 2009-2011 David Olofson
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

#include <math.h>
#include <string.h>
#include "e_object.h"
#include "e_class.h"
#include "e_array.h"
#include "e_vm.h"
#include "e_operate.h"
#include "e_register.h"


static inline int a_setsize(EEL_object *eo, int newsize)
{
	EEL_value *nv;
	EEL_array *a = o2EEL_array(eo);
	int n = eel_calcresize(EEL_ARRAY_SIZEBASE, a->maxlength, newsize);
	if(n == a->maxlength)
		return 0;
	nv = eel_realloc(eo->vm, a->values, n * sizeof(EEL_value));
	if(!nv)
	{
		if(newsize)
			return -1;
		a->values = NULL;
		a->maxlength = 0;
		return 0;
	}
	if(nv != a->values)
	{
		/* Block moved! Relocate any weakrefs. */
		int i;
		int min = a->length < n ? a->length : n;
		for(i = 0; i < min; ++i)
			if(nv[i].type == EEL_TWEAKREF)
				eel_weakref_relocate(&nv[i]);
		a->values = nv;
	}
	a->maxlength = n;
	return 0;
}


static EEL_xno a_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	int i;
	EEL_array *a;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_array), type);
	if(!eo)
		return EEL_XMEMORY;
	a = o2EEL_array(eo);
	if(!initc)
	{
		/* Empty array! */
		a->values = eel_malloc(vm, EEL_ARRAY_SIZEBASE * sizeof(EEL_value));
		if(!a->values)
		{
			eel_o_free(eo);
			return EEL_XMEMORY;
		}
		a->maxlength = EEL_ARRAY_SIZEBASE;
		a->length = 0;
		eel_o2v(result, eo);
		return 0;
	}
	a->values = eel_malloc(vm, initc * sizeof(EEL_value));
	if(!a->values)
	{
		eel_o_free(eo);
		return EEL_XMEMORY;
	}
	a->length = a->maxlength = initc;
	for(i = 0; i < initc; ++i)
		eel_v_copy(&a->values[i], initv + i);
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno a_destruct(EEL_object *eo)
{
	EEL_array *a = o2EEL_array(eo);
	int i;
/*
FIXME: If there are "many" items, any objects should be sent off to incremental cleanup!
*/
	for(i = 0; i < a->length; ++i)
		eel_v_disown_nz(&a->values[i]);
	eel_free(eo->vm, a->values);
	return 0;
}


static EEL_xno a_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_array *a = o2EEL_array(eo);
	int i;

	/* Cast index to int */
	switch(op1->type)
	{
	  case EEL_TBOOLEAN:
	  case EEL_TINTEGER:
	  case EEL_TTYPEID:
		i = op1->integer.v;
		break;
	  case EEL_TREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}

	/* Check index */
	if(i < 0)
		return EEL_XLOWINDEX;
	else if(i >= a->length)
		return EEL_XHIGHINDEX;

	/* Read value */
	eel_v_copy(op2, &a->values[i]);
	return 0;
}


static inline EEL_xno a_set_index(EEL_object *eo, int i, EEL_value *op2)
{
	EEL_array *a = o2EEL_array(eo);

	/* Initialize or assign? */
	if(i >= a->length)
	{
		if(a_setsize(eo, i + 1) < 0)
			return EEL_XMEMORY;
		/* Clear any skipped (uninitialized) values */
		if(i > a->length)
/*
FIXME: It might be faster to just set the type fields to EEL_TNIL...
*/
			memset(a->values + a->length, 0,
					(i - a->length) * sizeof(EEL_value));
		a->length = i + 1;
	}
	else
		/* Assign: Only for initialized indices! */
		eel_v_disown_nz(&a->values[i]);

	/* Write value */
	eel_v_copy(&a->values[i], op2);
	return 0;
}


static EEL_xno a_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int i;
	switch(op1->type)
	{
	  case EEL_TBOOLEAN:
	  case EEL_TINTEGER:
	  case EEL_TTYPEID:
		i = op1->integer.v;
		break;
	  case EEL_TREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	if(i < 0)
		return EEL_XLOWINDEX;
	return a_set_index(eo, i, op2);
}


static EEL_xno a_insert(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_array *a = o2EEL_array(eo);
	int i, mv;
	switch(op1->type)
	{
	  case EEL_TBOOLEAN:
	  case EEL_TINTEGER:
	  case EEL_TTYPEID:
		i = op1->integer.v;
		break;
	  case EEL_TREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	if(i < 0)
		return EEL_XLOWINDEX;

	/* Resize */
	if(a_setsize(eo, a->length + 1) < 0)
		return EEL_XMEMORY;
	++a->length;

	/* Move */
	for(mv = a->length - 1; mv > i; --mv)
		eel_v_move(a->values + mv, a->values + mv - 1);

	/* Write value */
	eel_v_copy(&a->values[i], op2);
	return 0;
}


static EEL_xno a_delete(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_array *a = o2EEL_array(eo);
	int i0, i1, i;
	if(op1 && EEL_IS_OBJREF(op1->type))
	{
		i0 = -1;
		for(i = 0; i < a->length; ++i)
			if(a->values[i].objref.v == op1->objref.v)
			{
				i0 = i1 = i;
				break;
			}
		if(i0 < 0)
			return EEL_XWRONGINDEX;
	}
	else
	{
		EEL_xno x = eel_get_delete_range(&i0, &i1, op1, op2, a->length);
		if(x)
			return x;
	}
	for(i = i0; i <= i1; ++i)
		eel_v_disown_nz(&a->values[i]);
	for(i = 0; i < a->length - i1 - 1; ++i)
		eel_v_move(a->values + i0 + i, a->values + i1 + i + 1);
	a->length -= i1 - i0 + 1;
	a_setsize(eo, a->length);
	return 0;
}


static inline EEL_object *a__clone(EEL_object *orig)
{
	EEL_vm *vm = orig->vm;
	int i, len;
	EEL_array *clonea;
	EEL_array *origa = o2EEL_array(orig);
	EEL_object *clone = eel_o_alloc(vm, sizeof(EEL_array), orig->type);
	if(!clone)
		return NULL;
	clonea = o2EEL_array(clone);
	clonea->values = (EEL_value *)eel_malloc(vm,
			origa->length * sizeof(EEL_value));
	if(!clonea->values)
	{
		eel_o_free(clone);
		return NULL;
	}
	len = clonea->length = clonea->maxlength = origa->length;
	for(i = 0; i < len; ++i)
		eel_v_clone(&clonea->values[i], &origa->values[i]);
	return clone;
}


static EEL_xno a_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no = a__clone(src->objref.v);
	if(!no)
		return EEL_XMEMORY;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno a_add(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	eo = a__clone(eo);
	if(!eo)
		return EEL_XMEMORY;
	x = a_set_index(eo, o2EEL_array(eo)->length, op1);
	if(x)
	{
		eel_o_free(eo);
		return x;
	}
	eel_o2v(op2, eo);
	return 0;
}


static EEL_xno a_ipadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	eel_o2v(op2, eo);
	x = a_set_index(eo, o2EEL_array(eo)->length, op1);
	if(x)
		return x;
	eel_o_own(eo);
	return 0;
}


static EEL_xno a_copy(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int i;
	EEL_object *so;
	EEL_array *sa;
	EEL_vm *vm = eo->vm;
	EEL_array *oa = o2EEL_array(eo);
	int start = eel_v2l(op1);
	int length = eel_v2l(op2);
	if(start < 0)
		return EEL_XLOWINDEX;
	else if(start >= oa->length)
		return EEL_XHIGHINDEX;
	if(length <= 0)
		return EEL_XWRONGINDEX;
	else if(start + length > oa->length)
		return EEL_XHIGHINDEX;
	so = eel_o_alloc(vm, sizeof(EEL_array), EEL_CARRAY);
	if(!so)
		return EEL_XCONSTRUCTOR;
	sa = o2EEL_array(so);
	sa->values = eel_malloc(vm, length * sizeof(EEL_value));
	if(!sa->values)
	{
		eel_o_free(so);
		return EEL_XMEMORY;
	}
	sa->maxlength = sa->length = length;
	for(i = 0; i < length; ++i)
		eel_v_clone(&sa->values[i], &oa->values[start + i]);
	eel_o2v(op2, so);
	return 0;
}


static EEL_xno a_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	eel_l2v(op2, o2EEL_array(eo)->length);
	return 0;
}


static EEL_xno a_compare(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return EEL_XNOTIMPLEMENTED;
}


void eel_carray_register(EEL_vm *vm)
{
	EEL_object *c = eel_register_class(vm, EEL_CARRAY, "array", EEL_COBJECT,
			a_construct, a_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, a_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, a_setindex);
	eel_set_metamethod(c, EEL_MM_INSERT, a_insert);
	eel_set_metamethod(c, EEL_MM_DELETE, a_delete);
	eel_set_metamethod(c, EEL_MM_COPY, a_copy);
	eel_set_metamethod(c, EEL_MM_LENGTH, a_length);
	eel_set_metamethod(c, EEL_MM_COMPARE, a_compare);
	eel_set_metamethod(c, EEL_MM_ADD, a_add);
	eel_set_metamethod(c, EEL_MM_IPADD, a_ipadd);
	eel_set_casts(vm, EEL_CARRAY, EEL_CARRAY, a_clone);
}
