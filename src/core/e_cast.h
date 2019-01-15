/*
---------------------------------------------------------------------------
	e_cast.h - EEL Typecasting Utilities
---------------------------------------------------------------------------
 * Copyright 2006, 2009, 2011-2012, 2019 David Olofson
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

#ifndef EEL_E_CAST_H
#define EEL_E_CAST_H

#include "EEL_object.h"
#include "e_state.h"
#include "e_util.h"
#include "e_config.h"

/*
 * Cast value 'src' to class 'cid' and put the result in 'dst'. If 't' is an
 * object class, an object of that class is constructed, using 'src' for the
 * initializer operand to the constructor.
 *
 * Returns a VM exception code if the operation fails.
 */
static inline EEL_xno eel_cast(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_state *st = VMP->state;
	EEL_cast_cb *cbs = st->casters;
	EEL_xno x = cbs[EEL_CLASS(src) * st->castersdim + cid](vm, src, dst,
			cid);
	if(x && (cid == EEL_CSTRING || cid == EEL_CDSTRING))
	{
		const char *s = eel_v_stringrep(vm, src);
		if(!s)
			return EEL_XWRONGTYPE;	/* Giving up! */
		if(cid == EEL_CSTRING)
			dst->objref.v = eel_ps_new(vm, s);
		else/* if(cid == EEL_CDSTRING)*/
			dst->objref.v = eel_ds_new(vm, s);
		dst->classid = EEL_COBJREF;
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
