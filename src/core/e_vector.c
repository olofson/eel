/*
---------------------------------------------------------------------------
	e_vector.c - EEL Vector Class implementation
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2010, 2012, 2014, 2019 David Olofson
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

#include <math.h>
#include <string.h>
#include "e_class.h"
#include "e_vector.h"
#include "e_vm.h"
#include "e_string.h"
#include "e_register.h"


static inline EEL_xno v_setsize(EEL_object *eo, int newsize)
{
	char *nb;
	EEL_vector *v = o2EEL_vector(eo);
	int n = eel_calcresize(EEL_VECTOR_SIZEBASE, v->maxlength, newsize);
	if(n == v->maxlength)
		return 0;
	nb = eel_realloc(eo->vm, v->buffer.u8, n * v->isize);
	if(!nb)
		return -1;
	v->buffer.u8 = (unsigned char *)nb;
	v->maxlength = n;
	return 0;
}


/* Fast cast and write - no index checking! */
static inline EEL_xno write_index(EEL_object *eo, int i, EEL_value *v)
{
	EEL_vector *vec = o2EEL_vector(eo);
	switch(v->classid)
	{
	  case EEL_CNIL:
		switch(eo->classid)
		{
		  /*
		   * No sign extension needed as long as all vector types
		   * have size <= that of EEL_value.integer.v, so we can
		   * group integers of the same size!
		   */
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			vec->buffer.u8[i] = 0;
			break;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			vec->buffer.u16[i] = 0;
			break;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			vec->buffer.u32[i] = 0;
			break;
		  case EEL_CVECTOR_F:
			vec->buffer.f[i] = 0;
			break;
		  case EEL_CVECTOR_D:
			vec->buffer.d[i] = 0;
			break;
		  default:
			return EEL_XINTERNAL;
		}
		break;
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		switch(eo->classid)
		{
		  /*
		   * No sign extension needed as long as all vector types
		   * have size <= that of EEL_value.integer.v, so we can
		   * group integers of the same size!
		   */
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			vec->buffer.u8[i] = v->integer.v;
			break;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			vec->buffer.u16[i] = v->integer.v;
			break;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			vec->buffer.u32[i] = v->integer.v;
			break;
		  case EEL_CVECTOR_F:
			vec->buffer.f[i] = v->integer.v;
			break;
		  case EEL_CVECTOR_D:
			vec->buffer.d[i] = v->integer.v;
			break;
		  default:
			return EEL_XINTERNAL;
		}
		break;
	  case EEL_CREAL:
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			vec->buffer.u8[i] = floor(v->real.v);
			break;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			vec->buffer.u16[i] = floor(v->real.v);
			break;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			vec->buffer.u32[i] = floor(v->real.v);
			break;
		  case EEL_CVECTOR_F:
			vec->buffer.f[i] = v->real.v;
			break;
		  case EEL_CVECTOR_D:
			vec->buffer.d[i] = v->real.v;
			break;
		  default:
			return EEL_XINTERNAL;
		}
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	return 0;
}


static inline EEL_integer get_ivalue(EEL_object *o, int i)
{
	EEL_vector *vec = o2EEL_vector(o);
	if(i >= vec->length)
		return 0;
	switch(o->classid)
	{
	  case EEL_CVECTOR_U8:
		return vec->buffer.u8[i];
	  case EEL_CVECTOR_S8:
		return vec->buffer.s8[i];
	  case EEL_CVECTOR_U16:
		return vec->buffer.u16[i];
	  case EEL_CVECTOR_S16:
		return vec->buffer.s16[i];
	  case EEL_CVECTOR_U32:
		return vec->buffer.u32[i];
	  case EEL_CVECTOR_S32:
		return vec->buffer.s32[i];
	  case EEL_CVECTOR_F:
		return floor(vec->buffer.f[i]);
	  case EEL_CVECTOR_D:
		return floor(vec->buffer.d[i]);
	  default:
		return 0;
	}
}


static inline EEL_real get_rvalue(EEL_object *o, int i)
{
	EEL_vector *vec = o2EEL_vector(o);
	if(i >= vec->length)
		return 0.0;
	switch(o->classid)
	{
	  case EEL_CVECTOR_U8:
		return vec->buffer.u8[i];
	  case EEL_CVECTOR_S8:
		return vec->buffer.s8[i];
	  case EEL_CVECTOR_U16:
		return vec->buffer.u16[i];
	  case EEL_CVECTOR_S16:
		return vec->buffer.s16[i];
	  case EEL_CVECTOR_U32:
		return vec->buffer.u32[i];
	  case EEL_CVECTOR_S32:
		return vec->buffer.s32[i];
	  case EEL_CVECTOR_F:
		return vec->buffer.f[i];
	  case EEL_CVECTOR_D:
		return vec->buffer.d[i];
	  default:
		return 0.0;
	}
}


static inline int item_size(EEL_classes vt)
{
	switch(vt)
	{
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
		return sizeof(EEL_uint8);
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
		return sizeof(EEL_uint16);
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
		return sizeof(EEL_uint32);
	  case EEL_CVECTOR_F:
		return sizeof(float);
	  case EEL_CVECTOR_D:
		return sizeof(double);
	  default:
		return 0;
	}
}


static EEL_xno v_delete(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_vector *v = o2EEL_vector(eo);
	int i0, i1, is;
	EEL_xno x = eel_get_delete_range(&i0, &i1, op1, op2, v->length);
	if(x)
		return x;
	is = item_size(eo->classid);
	memmove(v->buffer.u8 + i0 * is, v->buffer.u8 + (i1 + 1) * is,
			(v->length - i1 - 1) * is);
	v->length -= i1 - i0 + 1;
	v_setsize(eo, v->length);
	return 0;
}


/*
 * Allocate data and set members to make 'clone' a clone of 'orig'.
 * 'clone' and 'orig' must be objects of the same type!
 * NOTE:
 *	The vector values are NOT copied! You get a "garbage" memory
 *	block that you're meant to overwrite without reading.
 * Return 0 upon success, or -1 if in case of failure.
 */
static inline int make_clone_nc(EEL_vm *vm, EEL_object *clone,
		EEL_object *orig)
{
	EEL_vector *origv = o2EEL_vector(orig);
	EEL_vector *clonev = o2EEL_vector(clone);
	clonev->buffer.u8 = eel_malloc(vm, origv->length * origv->isize);
	if(!clonev->buffer.u8)
		return -1;
	clonev->isize = origv->isize;
	clonev->maxlength = origv->length;
	clonev->length = origv->length;
	return 0;
}


/* Create a "clone" with an uninitialized buffer */
static inline EEL_object *empty_clone(EEL_object *orig)
{
	EEL_vm *vm = orig->vm;
	EEL_object *clone = eel_o_alloc(vm, sizeof(EEL_vector), orig->classid);
	if(!clone)
		return NULL;
	if(make_clone_nc(vm, clone, orig) < 0)
	{
		eel_o_free(clone);
		return NULL;
	}
	return clone;
}


/* Create a full clone, values and all */
static inline EEL_object *full_clone(EEL_object *orig)
{
	EEL_vector *vec;
	EEL_object *clone = empty_clone(orig);
	if(!clone)
		return NULL;
	vec = o2EEL_vector(clone);
	memcpy(vec->buffer.u8, o2EEL_vector(orig)->buffer.u8,
			vec->length * vec->isize);
	return clone;
}


static EEL_xno v_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	int i;
	EEL_vector *vec;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_vector), cid);
	if(!eo)
		return EEL_XMEMORY;
	vec = o2EEL_vector(eo);

	/* Initialize as empty vector */
	vec->isize = item_size(eo->classid);
	vec->length = vec->maxlength = 0;
	vec->buffer.u8 = NULL;
	if(!initc)
	{
		eel_o2v(result, eo);
		return 0;	/* That's it; they want an empty vector! */
	}
	vec->buffer.u8 = eel_malloc(vm, initc * vec->isize);
	if(!vec->buffer.u8)
	{
		eel_o_free(eo);
		return EEL_XMEMORY;
	}
	vec->maxlength = initc;
	for(i = 0; i < initc; ++i)
	{
		int x = write_index(eo, i, initv + i);
		if(x)
		{
			eel_free(vm, vec->buffer.u8);
			eel_o_free(eo);
			return x;	/* Something went wrong... */
		}
	}
	vec->length = initc;
	eel_o2v(result, eo);
	return 0;
}


EEL_object *eel_cv_new_noinit(EEL_vm *vm, EEL_classes cid, unsigned size)
{
	EEL_vector *vec;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_vector), cid);
	if(!eo)
		return NULL;
	vec = o2EEL_vector(eo);
	vec->isize = item_size(eo->classid);
	vec->length = vec->maxlength = size;
	vec->buffer.u8 = eel_malloc(vm, vec->length * vec->isize);
	if(!vec->buffer.u8)
	{
		eel_o_free(eo);
		return NULL;
	}
	return eo;
}


static EEL_xno v_destruct(EEL_object *eo)
{
	eel_free(eo->vm, o2EEL_vector(eo)->buffer.u8);
	return 0;
}


static EEL_xno v_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_vector *vec = o2EEL_vector(eo);
	int i;

	/* Cast index to int */
	switch(op1->classid)
	{
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		i = op1->integer.v;
		break;
	  case EEL_CREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}

	/* Check index */
	if(i < 0)
		return EEL_XLOWINDEX;
	else if(i >= vec->length)
		return EEL_XHIGHINDEX;

	/* Read value */
	switch(eo->classid)
	{
	  case EEL_CVECTOR_U8:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.u8[i];
		break;
	  case EEL_CVECTOR_S8:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.s8[i];
		break;
	  case EEL_CVECTOR_U16:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.u16[i];
		break;
	  case EEL_CVECTOR_S16:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.s16[i];
		break;
	  case EEL_CVECTOR_U32:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.u32[i];
		break;
	  case EEL_CVECTOR_S32:
		op2->classid = EEL_CINTEGER;
		op2->integer.v = vec->buffer.s32[i];
		break;
	  case EEL_CVECTOR_F:
		op2->classid = EEL_CREAL;
		op2->real.v = vec->buffer.f[i];
		break;
	  case EEL_CVECTOR_D:
		op2->classid = EEL_CREAL;
		op2->real.v = vec->buffer.d[i];
		break;
	  default:
		return EEL_XINTERNAL;
	}
	return 0;
}


static EEL_xno v_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_vector *vec = o2EEL_vector(eo);
	EEL_xno x;
	int i;

	/* Cast index to int */
	switch(op1->classid)
	{
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		i = op1->integer.v;
		break;
	  case EEL_CREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}

	/* Check index */
	if(i < 0)
		return EEL_XLOWINDEX;

	if(i >= vec->length)
	{
		if(v_setsize(eo, i + 1) < 0)
			return EEL_XMEMORY;
		if(i > vec->length)
			memset(vec->buffer.u8 + vec->length * vec->isize, 0,
					(i - vec->length) * vec->isize);
		vec->length = i + 1;
	}
	/* Cast and write value */
/*
TODO:	  case EEL_TLIST:
TODO:	Write values from 'i' and up from the LIST, so
TODO:		v[5] = 1, 2, 3;
TODO:	becomes shorthand for
TODO:		v[5, 6, 7] = 1, 2, 3;
TODO:	(Both should of course be optimized using SETINDEX with LISTs.)
*/
	x = write_index(eo, i, op2);
	if(x)
		return x;
	return 0;
}


static EEL_xno v_insert(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int x, i, is;
	EEL_vector *v = o2EEL_vector(eo);
	switch(op1->classid)
	{
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		i = op1->integer.v;
		break;
	  case EEL_CREAL:
		i = floor(op1->real.v);
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	if(i < 0)
		return EEL_XLOWINDEX;

	/* Extend buffer if needed */
	x = v_setsize(eo, v->length + 1);
	if(x)
		return x;
	++v->length;

	/* Move */
	is = item_size(eo->classid);
	memmove(v->buffer.u8 + (i + 1) * is, v->buffer.u8 + i * is,
			(v->length - i - 1) * is);

	/* Write value */
	return write_index(eo, i, op2);
}


static EEL_xno v_copy(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_vm *vm = eo->vm;
	EEL_vector *ov = o2EEL_vector(eo);
	EEL_object *so;
	EEL_vector *sv;
	int start = eel_v2l(op1);
	int length = eel_v2l(op2);
	if(start < 0)
		return EEL_XLOWINDEX;
	else if(start > ov->length)
		return EEL_XHIGHINDEX;
	if(length < 0)
		return EEL_XWRONGINDEX;
	else if(start + length > ov->length)
		return EEL_XHIGHINDEX;
	so = eel_o_alloc(vm, sizeof(EEL_vector), eo->classid);
	if(!so)
		return EEL_XCONSTRUCTOR;
	sv = o2EEL_vector(so);
	sv->buffer.u8 = eel_malloc(vm, length * ov->isize);
	if(!sv->buffer.u8)
	{
		eel_o_free(so);
		return EEL_XMEMORY;
	}
	sv->isize = ov->isize;
	sv->maxlength = length;
	sv->length = length;
	memcpy(sv->buffer.u8, ov->buffer.u8 + start * ov->isize,
			length * ov->isize);
	eel_o2v(op2, so);
	return 0;
}


static EEL_xno v_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	op2->classid = EEL_CINTEGER;
	op2->integer.v = o2EEL_vector(eo)->length;
	return 0;
}


static inline int vcmp_u8_u8(EEL_uint8 *a, EEL_uint8 *b, int length)
{
	int i;
	for(i = 0; i < length; ++i)
		if(a[i] != b[i])
		{
			if(a[i] > b[i])
				return 1;
			else/* if(a[i] < b[i])*/
				return -1;
		}
	return 0;
}


static EEL_xno v_compare(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_vector *v, *ov;
	if(!EEL_IS_OBJREF(op1->classid))
		return EEL_XWRONGTYPE;

	if(op1->objref.v->classid != eo->classid)
		return EEL_XNOTIMPLEMENTED;

	v = o2EEL_vector(eo);
	ov = o2EEL_vector(op1->objref.v);
	op2->classid = EEL_CINTEGER;
	if(v->length > ov->length)
	{
		op2->integer.v = 1;
		return 0;
	}
	else if(v->length < ov->length)
	{
		op2->integer.v = -1;
		return 0;
	}
	switch(eo->classid)
	{
	  case EEL_CVECTOR_U8:
		op2->integer.v = vcmp_u8_u8(v->buffer.u8, ov->buffer.u8, v->length);
		return 0;
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
	  case EEL_CVECTOR_D:
		return EEL_XNOTIMPLEMENTED;
	  default:
		return EEL_XINTERNAL;
	}
}


static EEL_xno v_cast_to_string(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)

{
	int i;
	EEL_object *orig = src->objref.v;
	EEL_vector *vec = o2EEL_vector(orig);
	char *buf = eel_malloc(vm, vec->length + 1);
	if(!buf)
		return EEL_XMEMORY;
	for(i = 0; i < vec->length; ++i)
		buf[i] = get_ivalue(orig, i);
	buf[i] = 0;
	dst->objref.v = eel_ps_nnew_grab(vm, buf, vec->length);
	if(!dst->objref.v)
	{
		eel_free(vm, buf);
		return EEL_XCONSTRUCTOR;
	}
	dst->classid = EEL_COBJREF;
	return 0;
}


static EEL_xno v_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *orig = src->objref.v;
	EEL_object *no = full_clone(orig);
	if(!no)
		return EEL_XMEMORY;
	eel_o2v(dst, no);
	return 0;
}


static inline EEL_xno do_vadd(EEL_object *eo, EEL_value *op1, EEL_object *to)
{
	int i;
	EEL_integer iv;
	EEL_real rv;
	EEL_vector *source = o2EEL_vector(eo);
	EEL_vector *target = o2EEL_vector(to);
	switch(op1->classid)
	{
	  case EEL_CNIL:
		if(target == source)
			return 0;
		memcpy(target->buffer.u8, source->buffer.u8,
				source->length * source->isize);
		return 0;
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		switch(eo->classid)
		{
		  /*
		   * Sign is irrelevant to addition, so we group u* and s*.
		   */
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] +
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] +
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] +
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_F:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] + rv;
			return 0;
		  case EEL_CVECTOR_D:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] + rv;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_CREAL:
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] + iv;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] + iv;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] + iv;
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] +
						op1->real.v;
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] +
						op1->real.v;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_COBJREF:
	  case EEL_CWEAKREF:
	  {
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] +
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] +
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] +
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] +
						get_rvalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] +
						get_rvalue(op1->objref.v, i);
			return 0;
		  default:
			return EEL_XWRONGTYPE;
		}
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno v_vadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	EEL_object *to = empty_clone(eo);
	if(!to)
		return EEL_XMEMORY;
	x = do_vadd(eo, op1, to);
	if(x)
	{
		eel_o_free(to);
		return x;
	}
	eel_o2v(op2, to);
	return 0;
}


static EEL_xno v_ipvadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x = do_vadd(eo, op1, eo);
	if(x)
		return x;
	eel_o_own(eo);
	eel_o2v(op2, eo);
	return 0;
}


static inline EEL_xno do_vsub(EEL_object *eo, EEL_value *op1, EEL_object *to)
{
	int i;
	EEL_integer iv;
	EEL_real rv;
	EEL_vector *source = o2EEL_vector(eo);
	EEL_vector *target = o2EEL_vector(to);
	switch(op1->classid)
	{
	  case EEL_CNIL:
		if(target == source)
			return 0;
		memcpy(target->buffer.u8, source->buffer.u8,
				source->length * source->isize);
		return 0;
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		switch(eo->classid)
		{
		  /*
		   * Sign is irrelevant to addition, so we group u* and s*.
		   */
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] -
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] -
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] -
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_F:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] - rv;
			return 0;
		  case EEL_CVECTOR_D:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] - rv;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_CREAL:
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] - iv;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] - iv;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] - iv;
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] -
						op1->real.v;
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] -
						op1->real.v;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_COBJREF:
	  case EEL_CWEAKREF:
	  {
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] -
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] -
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] -
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] -
						get_rvalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] -
						get_rvalue(op1->objref.v, i);
			return 0;
		  default:
			return EEL_XWRONGTYPE;
		}
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno v_vsub(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	EEL_object *to = empty_clone(eo);
	if(!to)
		return EEL_XMEMORY;
	x = do_vsub(eo, op1, to);
	if(x)
	{
		eel_o_free(to);
		return x;
	}
	eel_o2v(op2, to);
	return 0;
}


static EEL_xno v_ipvsub(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x = do_vsub(eo, op1, eo);
	if(x)
		return x;
	eel_o_own(eo);
	eel_o2v(op2, eo);
	return 0;
}


static inline EEL_xno do_vmul(EEL_object *eo, EEL_value *op1, EEL_object *to)
{
	int i;
	EEL_integer iv;
	EEL_real rv;
	EEL_vector *source = o2EEL_vector(eo);
	EEL_vector *target = o2EEL_vector(to);
	switch(op1->classid)
	{
	  case EEL_CNIL:
		if(target == source)
			return 0;
		memset(target->buffer.u8, 0, source->length * source->isize);
		return 0;
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		switch(eo->classid)
		{
		  /*
		   * Sign is irrelevant to addition, so we group u* and s*.
		   */
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] *
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] *
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] *
						op1->integer.v;
			return 0;
		  case EEL_CVECTOR_F:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] * rv;
			return 0;
		  case EEL_CVECTOR_D:
			rv = op1->integer.v;
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] * rv;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_CREAL:
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] * iv;
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] * iv;
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			iv = floor(op1->real.v);
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] * iv;
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] *
						op1->real.v;
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] *
						op1->real.v;
			return 0;
		  default:
			return EEL_XINTERNAL;
		}
	  case EEL_COBJREF:
	  case EEL_CWEAKREF:
	  {
		switch(eo->classid)
		{
		  case EEL_CVECTOR_U8:
		  case EEL_CVECTOR_S8:
			for(i = 0; i < source->length; ++i)
				target->buffer.u8[i] = source->buffer.u8[i] *
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U16:
		  case EEL_CVECTOR_S16:
			for(i = 0; i < source->length; ++i)
				target->buffer.u16[i] = source->buffer.u16[i] *
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_U32:
		  case EEL_CVECTOR_S32:
			for(i = 0; i < source->length; ++i)
				target->buffer.u32[i] = source->buffer.u32[i] *
						get_ivalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_F:
			for(i = 0; i < source->length; ++i)
				target->buffer.f[i] = source->buffer.f[i] *
						get_rvalue(op1->objref.v, i);
			return 0;
		  case EEL_CVECTOR_D:
			for(i = 0; i < source->length; ++i)
				target->buffer.d[i] = source->buffer.d[i] *
						get_rvalue(op1->objref.v, i);
			return 0;
		  default:
			return EEL_XWRONGTYPE;
		}
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno v_vmul(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	EEL_object *to = empty_clone(eo);
	if(!to)
		return EEL_XMEMORY;
	x = do_vmul(eo, op1, to);
	if(x)
	{
		eel_o_free(to);
		return x;
	}
	eel_o2v(op2, to);
	return 0;
}


static EEL_xno v_ipvmul(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x = do_vmul(eo, op1, eo);
	if(x)
		return x;
	eel_o_own(eo);
	eel_o2v(op2, eo);
	return 0;
}


/* Append value or array/vector of values to vector */
static inline EEL_xno v__append(EEL_object *eo, EEL_value *op1, EEL_object *to)
{
	EEL_xno x;
	EEL_vector *vec = o2EEL_vector(eo);
	int start = vec->length;
	int len = EEL_IS_OBJREF(op1->classid) ? eel_length(op1->objref.v) : -1;
	if(!len)
		return 0;	/* Nothing to do! */
	if(len > 0)
	{
		int i;
		/* Append items from indexable type. */
		if(v_setsize(eo, start + len) < 0)
			return EEL_XMEMORY;
		if(EEL_CLASS(op1) == EEL_CTABLE)
			return EEL_XWRONGTYPE;
		for(i = 0; i < len; ++i)
		{
			EEL_value v;
			eel_l2v(&v, i);
			x = eel_o__metamethod(op1->objref.v, EEL_MM_GETINDEX, &v, &v);
			if(x)
				return x;
			x = write_index(eo, start + i, &v);
			eel_v_disown(&v);
			if(x)
				return x;
		}
		vec->length = start + len;
		return 0;
	}

	/* Append single value. */
	if(v_setsize(eo, start + 1) < 0)
		return EEL_XMEMORY;
	x =  write_index(eo, start, op1);
	if(x)
		return x;
	vec->length = start + 1;
	return 0;
}


static EEL_xno v_add(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	EEL_object *to = full_clone(eo);
	if(!to)
		return EEL_XMEMORY;
	x = v__append(eo, op1, to);
	if(x)
	{
		eel_o_free(to);
		return x;
	}
	eel_o2v(op2, to);
	return 0;
}


static EEL_xno v_ipadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	x = v__append(eo, op1, eo);
	if(x)
		return x;
	eel_o_own(eo);
	eel_o2v(op2, eo);
	return 0;
}


static EEL_xno default_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	return v_construct(vm, EEL_DEFAULT_VECTOR_SUBCLASS,
			initv, initc, result);
}


static EEL_xno v_serialize(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno v_reconstruct(EEL_vm *vm, EEL_classes cid,
		EEL_object *stream, EEL_value *result)
{
	return EEL_XNOTIMPLEMENTED;
}


void eel_cvector_register(EEL_vm *vm)
{
	EEL_classes i;
	const char *names[] =
	{
		"vector_u8",	"vector_s8",
		"vector_u16",	"vector_s16",
		"vector_u32",	"vector_s32",
		"vector_f",	"vector_d"
	};

	/* Register virtual base class */
	eel_register_class(vm, EEL_CVECTOR, "vector", EEL_COBJECT,
			default_construct, NULL, NULL);

	/* Install subclasses for the respective value types */
	for(i = EEL_CVECTOR_U8; i <= EEL_CVECTOR_D ; ++i)
	{
		EEL_object *c = eel_register_class(vm, i,
				names[i - EEL_CVECTOR_U8], EEL_CVECTOR,
				v_construct, v_destruct, v_reconstruct);
		eel_set_metamethod(c, EEL_MM_GETINDEX, v_getindex);
		eel_set_metamethod(c, EEL_MM_SETINDEX, v_setindex);
		eel_set_metamethod(c, EEL_MM_COPY, v_copy);
		eel_set_metamethod(c, EEL_MM_LENGTH, v_length);
		eel_set_metamethod(c, EEL_MM_COMPARE, v_compare);
		eel_set_metamethod(c, EEL_MM_SERIALIZE, v_serialize);
		eel_set_metamethod(c, EEL_MM_ADD, v_add);
		eel_set_metamethod(c, EEL_MM_IPADD, v_ipadd);
		eel_set_metamethod(c, EEL_MM_VADD, v_vadd);
		eel_set_metamethod(c, EEL_MM_IPVADD, v_ipvadd);
		eel_set_metamethod(c, EEL_MM_VSUB, v_vsub);
		eel_set_metamethod(c, EEL_MM_IPVSUB, v_ipvsub);
		eel_set_metamethod(c, EEL_MM_VMUL, v_vmul);
		eel_set_metamethod(c, EEL_MM_IPVMUL, v_ipvmul);
		eel_set_metamethod(c, EEL_MM_INSERT, v_insert);
		eel_set_metamethod(c, EEL_MM_DELETE, v_delete);
		eel_set_casts(vm, i, i, v_clone);
		eel_set_casts(vm, i, EEL_CSTRING, v_cast_to_string);
	}
}


#if 0
/*----------------------------------------------------------
	EEL Vector API
----------------------------------------------------------*/

EEL_xno eel_vector_read_i(EEL_object *v, int start, int vstride,
		int *buf, int count, int bstride)
{
	return EEL_XNOTIMPLEMENTED;
}


EEL_xno eel_vector_read_f(EEL_object *v, int start, int vstride,
		float *buf, int count, int bstride)
{
	return EEL_XNOTIMPLEMENTED;
}


EEL_xno eel_vector_read_d(EEL_object *v, int start, int vstride,
		double *buf, int count, int bstride)
{
	EEL_vector *vec = o2EEL_vector(eo);
	switch(v->type)
	{
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
		return EEL_XNOTIMPLEMENTED;
	  case EEL_CVECTOR_D:
	  {
		vec->buffer.d[i];
		return EEL_XNONE;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
	return EEL_XNOTIMPLEMENTED;
}


EEL_xno eel_vector_write_i(EEL_object *v, int start, int vstride,
		const int *buf, int count, int bstride)
{
	return EEL_XNOTIMPLEMENTED;
}


EEL_xno eel_vector_write_f(EEL_object *v, int start, int vstride,
		const float *buf, int count, int bstride)
{
	return EEL_XNOTIMPLEMENTED;
}


EEL_xno eel_vector_write_d(EEL_object *v, int start, int vstride,
		const double *buf, int count, int bstride)
{
	return EEL_XNOTIMPLEMENTED;
}
#endif
