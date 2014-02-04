/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_cast.h - EEL Typecasting Utilities
---------------------------------------------------------------------------
 * Copyright (C) 2006, 2009, 2011 David Olofson
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

#ifndef EEL_E_CAST_H
#define EEL_E_CAST_H

#include "EEL_object.h"
#include "e_state.h"
#include "e_util.h"

/*
 * Cast value 'src' to type 't' and put the result in 'dst'.
 * If 't' is a class, an object of that class is constructed,
 * using 'src' for the initializer operand to the constructor.
 * Returns a VM exception code if the operation fails.
 */
static inline EEL_xno eel_cast(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_state *st = VMP->state;
	EEL_cast_cb *cbs = st->casters;
	EEL_xno x = cbs[EEL_TYPE(src) * st->castersdim + t](vm, src, dst, t);
	if(x && (t == EEL_CSTRING || t == EEL_CDSTRING))
	{
		const char *s = eel_v_stringrep(vm, src);
		if(!s)
			return EEL_XWRONGTYPE;	/* Giving up! */
		if(t == EEL_CSTRING)
			dst->objref.v = eel_ps_new(vm, s);
		else/* if(t == EEL_CDSTRING)*/
			dst->objref.v = eel_ds_new(vm, s);
		dst->type = EEL_TOBJREF;
		return dst->objref.v ? 0 : EEL_XMEMORY;
	}
	else
		return x;
};


int eel_realloc_cast_matrix(EEL_state *es, int newdim);

EEL_xno eel_cast_open(EEL_state *es);
void eel_cast_init(EEL_state *es);
void eel_cast_close(EEL_state *es);

#endif /* EEL_E_CAST_H */
