/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_cast.c - EEL Typecasting Utilities
---------------------------------------------------------------------------
 * Copyright (C) 2004-2006, 2009-2010, 2012 David Olofson
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
#include "e_cast.h"
#include "e_class.h"
#include "e_operate.h"


static EEL_xno cast_not_implemented(EEL_vm *vm,
		const EEL_value *op1, EEL_value *op2, EEL_types t)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno generic_indexable_cast(EEL_vm *vm,
		const EEL_value *op1, EEL_value *op2, EEL_types t)
{
	EEL_xno x;
	EEL_value iv;
	int maxi;
	EEL_object *o1 = op1->objref.v;
	EEL_object *o2;
	x = eel_o__metamethod(o1, EEL_MM_LENGTH, NULL, &iv);
	if(x)
		return x;
	x = eel_o__construct(vm, t, NULL, 0, op2);
	if(x)
		return x;
	o2 = op2->objref.v;
	maxi = iv.integer.v;
	for(iv.integer.v = 0; iv.integer.v < maxi; ++iv.integer.v)
	{
		EEL_value tmp;
		x = eel_o__metamethod(o1, EEL_MM_GETINDEX, &iv, &tmp);
		if(x)
		{
			eel_o_disown_nz(o2);
			return x;
		}
		x = eel_o__metamethod(o2, EEL_MM_SETINDEX, &iv, &tmp);
		if(x)
		{
			eel_o_disown_nz(o2);
			return x;
		}
		eel_v_disown_nz(&tmp);
	}
	return 0;
}


int eel_realloc_cast_matrix(EEL_state *es, int newdim)
{
	int x, y;
	EEL_cast_cb *nc;
	newdim = (newdim + 3) & 0xfffffffc;
	if(newdim <= es->castersdim)
		return 0;
	nc = eel_malloc(es->vm, newdim * newdim * sizeof(EEL_cast_cb));
	if(!nc)
		return -1;
	for(y = 0; y < newdim; ++y)
		for(x = es->castersdim; x < newdim; ++x)
			nc[y * newdim + x] = cast_not_implemented;
	for(y = es->castersdim; y < newdim; ++y)
		for(x = 0; x < es->castersdim; ++x)
			nc[y * newdim + x] = cast_not_implemented;
	if(es->casters)
	{
		for(y = 0; y < es->castersdim; ++y)
			for(x = 0; x < es->castersdim; ++x)
				nc[y * newdim + x] = es->casters[
						y * es->castersdim + x];
		eel_free(es->vm, es->casters);
	}
	es->casters = nc;
	es->castersdim = newdim;
	return 0;
}


static inline int has_mm(EEL_state *es, EEL_types t, EEL_mmindex mm)
{
	EEL_classdef *cd;
	EEL_object *co = es->classes[t];
	if(!co)
		return 0;
	cd = o2EEL_classdef(co);
	if(!cd)
		return 0;
	if(!cd->mmethods)
		return 0;
	return cd->mmethods[mm] != NULL;
}


static EEL_xno set_multiple_casts(EEL_vm *vm,
		EEL_types from, EEL_types to, EEL_cast_cb cb)
{
	EEL_state *es = VMP->state;
	int x, y;
	for(y = 0; y < es->castersdim; ++y)
	{
		switch((EEL_nontypes)from)
		{
		  case EEL_TANYTYPE:
			break;
		  case EEL_TANYINDEXABLE:
			if(!has_mm(es, y, EEL_MM_GETINDEX))
				continue;
			break;
		  default:
			continue;
		}
		for(x = 0; x < es->castersdim; ++x)
		{
			switch((EEL_nontypes)to)
			{
			  case EEL_TANYTYPE:
				break;
			  case EEL_TANYINDEXABLE:
				if(!has_mm(es, x, EEL_MM_SETINDEX))
					continue;
				break;
			  default:
				continue;
			}
			if(es->casters[y * es->castersdim + x] !=
					cast_not_implemented)
				continue;
			es->casters[y * es->castersdim + x] = cb;
		}
	}
	return 0;
}


EEL_xno eel_set_casts(EEL_vm *vm, int from, int to, EEL_cast_cb cb)
{
	EEL_state *es = VMP->state;
	if((from < 0) || (to < 0))
		return set_multiple_casts(vm, from, to, cb);
	es->casters[from * es->castersdim + to] = cb;
	return 0;
}


EEL_xno eel_cast_open(EEL_state *es)
{
	if(eel_realloc_cast_matrix(es, EEL__CUSER) < 0)
		return EEL_XMEMORY;
	return 0;
}


void eel_cast_init(EEL_state *es)
{
	EEL_vm *vm = es->vm;
	eel_set_casts(vm, EEL_TANYINDEXABLE, EEL_TANYINDEXABLE,
			generic_indexable_cast);
}


void eel_cast_close(EEL_state *es)
{
	eel_free(es->vm, es->casters);
	es->casters = NULL;
	es->castersdim = 0;
}
