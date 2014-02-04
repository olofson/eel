/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_dstring.c - EEL Dynamic String Class
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2008-2012 David Olofson
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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "e_object.h"
#include "e_operate.h"
#include "e_class.h"
#include "e_string.h"
#include "e_dstring.h"
#include "e_state.h"
#include "e_vm.h"
#include "e_register.h"


static inline int ds_setsize(EEL_object *eo, int newsize)
{
	char *nb;
	EEL_dstring *ds = o2EEL_dstring(eo);
	int n = eel_calcresize(EEL_DSTRING_SIZEBASE, ds->maxlength, newsize);
	if(n == ds->maxlength)
		return 0;
	nb = eel_realloc(eo->vm, ds->buffer, n);
	if(!nb)
		return -1;
	ds->buffer = nb;
	ds->maxlength = n;
	return 0;
}


static inline EEL_object *ds_nnew_grab(EEL_vm *vm, char *s, int len)
{
	EEL_dstring *ds;
	EEL_object *dso = eel_o_alloc(vm, sizeof(EEL_dstring), EEL_CDSTRING);
	if(!dso)
	{
		eel_free(vm, s);
		return NULL;
	}
	ds = o2EEL_dstring(dso);
	ds->buffer = s;
	ds->length = len;
	ds->maxlength = len + 1;
#ifdef EEL_VM_CHECKING
	if(ds->buffer[len])
		fprintf(stderr, "INTERNAL ERROR: Someone gave a non null "
				"terminated string to eel_ds_*new_grab()!");
#endif
	return dso;
}


EEL_object *eel_ds_nnew_grab(EEL_vm *vm, char *s, int len)
{
	return ds_nnew_grab(vm, s, len);
}


EEL_object *eel_ds_new_grab(EEL_vm *vm, char *s)
{
	return ds_nnew_grab(vm, s, strlen(s));
}


static inline EEL_object *ds_nnew(EEL_vm *vm, const char *s, int len)
{
	EEL_dstring *ds;
	EEL_object *dso = eel_o_alloc(vm, sizeof(EEL_dstring), EEL_CDSTRING);
	if(!dso)
		return NULL;
	ds = o2EEL_dstring(dso);
	ds->buffer = eel_malloc(vm, len + 1);
	ds->maxlength = len + 1;
	if(!ds->buffer)
	{
		eel_o_disown_nz(dso);
		return NULL;
	}
	if(s)
	{
		memcpy(ds->buffer, s, len);
		ds->length = len;
		ds->buffer[len] = 0;
	}
	else
	{
		ds->length = 0;
		ds->buffer[0] = 0;
	}
	return dso;
}


EEL_object *eel_ds_nnew(EEL_vm *vm, const char *s, int len)
{
	return ds_nnew(vm, s, len);
}


EEL_object *eel_ds_new(EEL_vm *vm, const char *s)
{
	return ds_nnew(vm, s, strlen(s));
}


/*-------------------------------------------------------------------------
	Dynamic string class implementation
-------------------------------------------------------------------------*/

static EEL_xno ds_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EEL_object *eo;
	if(initc > 0)
	{
		int i;
		char *buf = eel_malloc(vm, initc + 1);
		if(!buf)
			return EEL_XMEMORY;
		for(i = 0; i < initc; ++i)
		{
			int v = eel_get_indexval(vm, initv + i);
			if(v < 0)
			{
				eel_free(vm, buf);
				return EEL_XARGUMENTS;
			}
			buf[i] = v;
		}
		buf[i] = 0;
		eo = ds_nnew_grab(vm, buf, i);
		if(!eo)
		{
			eel_free(vm, buf);
			return EEL_XCONSTRUCTOR;
		}
		eel_o2v(result,  eo);
		return 0;
	}
	else
	{
		eo = ds_nnew(vm, "", 0);
		if(!eo)
			return EEL_XCONSTRUCTOR;
		eel_o2v(result,  eo);
		return 0;
	}
}


static EEL_xno ds_destruct(EEL_object *eo)
{
	EEL_dstring *ds = o2EEL_dstring(eo);
	EEL_vm *vm = eo->vm;
	eel_free(vm, (void *)(ds->buffer));
	return 0;
}


static EEL_xno ds_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds = o2EEL_dstring(eo);
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
	else if(i >= ds->length)
		return EEL_XHIGHINDEX;

	/* Read value */
	op2->type = EEL_TINTEGER;
	op2->integer.v = ds->buffer[i] & 0xff;	/* Treat as unsigned! */
	return 0;
}


static inline EEL_xno ds_set_index(EEL_object *eo, int i, EEL_value *op2)
{
	EEL_dstring *ds = o2EEL_dstring(eo);

	/* Extend and initialize as needed */
	if(i >= ds->length)
	{
		if(ds_setsize(eo, i + 2) < 0)
			return EEL_XMEMORY;
		memset(ds->buffer + ds->length + 1, 0,
				ds->maxlength - ds->length - 1);
		ds->length = i + 1;
	}

	/* Write value */
	ds->buffer[i] = eel_v2l(op2);
	return 0;
}


static EEL_xno ds_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
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
	return ds_set_index(eo, i, op2);
}


EEL_xno eel_ds_write(EEL_object *eo, int pos, const char *s, int len)
{
	EEL_dstring *ds = o2EEL_dstring(eo);

	/* Extend and initialize as needed */
	if(pos + len >= ds->length)
	{
		if(ds_setsize(eo, pos + len + 1) < 0)
			return EEL_XMEMORY;
		memset(ds->buffer + ds->length, 0, ds->maxlength - ds->length);
		ds->length = pos + len;
	}

	/* Write bytes */
	memcpy(ds->buffer + pos, s, len);
	return 0;
}


static EEL_xno ds_insert(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds = o2EEL_dstring(eo);
	int i;
	const char *s2buf;
	int s2len;
	char cb[1];

	/* Index */
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

	/* Operand */
	switch((EEL_classes)EEL_TYPE(op2))
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2 = o2EEL_string(op2->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *ds2 = o2EEL_dstring(op2->objref.v);
		s2buf = ds2->buffer;
		s2len = ds2->length;
		break;
	  }
	  default:
	  {
		cb[0] = eel_v2l(op2);
		s2buf = cb;
		s2len = 1;
		break;
	  }
	}

	/* Resize */
	if(ds_setsize(eo, ds->length + s2len + i + 1) < 0)
		return EEL_XMEMORY;

	/* Move (at least the null terminator) */
	if(i <= ds->length)
		memmove(ds->buffer + i + s2len, ds->buffer + i, ds->length - i + 1);

	/* Write */
	memcpy(ds->buffer + i, s2buf, s2len);

	ds->length += s2len;
	return 0;
}


static EEL_xno ds_delete(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds = o2EEL_dstring(eo);
	int i0, i1;
	EEL_xno x = eel_get_delete_range(&i0, &i1, op1, op2, ds->length);
	if(x)
		return x;
	/* "Missing" '- 1' in the last argument: Move the terminator as well! */
	memmove(ds->buffer + i0, ds->buffer + i1 + 1, ds->length - i1);
	ds->length -= i1 - i0 + 1;
	ds_setsize(eo, ds->length + 1);
	return 0;
}


static EEL_xno ds_copy(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *s = o2EEL_dstring(eo);
	int start = eel_v2l(op1);
	int length = eel_v2l(op2);
	if(start < 0)
		return EEL_XLOWINDEX;
	else if(start >= s->length)
		return EEL_XHIGHINDEX;
	if(length <= 0)
		return EEL_XWRONGINDEX;
	else if(start + length > s->length)
		return EEL_XHIGHINDEX;
	op2->objref.v = ds_nnew(eo->vm, s->buffer + start, length);
	if(!op2->objref.v)
		return EEL_XCONSTRUCTOR;
	op2->type = EEL_TOBJREF;
	return 0;
}


static EEL_xno ds_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	op2->type = EEL_TINTEGER;
	op2->integer.v = o2EEL_dstring(eo)->length;
	return 0;
}


static EEL_xno ds_compare(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds;
	const char *s2buf;
	int s2len;
	if(!EEL_IS_OBJREF(op1->type))
		return EEL_XWRONGTYPE;
	op2->type = EEL_TINTEGER;

	switch((EEL_classes)op1->objref.v->type)
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2 = o2EEL_string(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *s2;
		if(op1->objref.v == eo)
		{
			/* Same instance! */
			op2->integer.v = 0;
			return 0;
		}
		s2 = o2EEL_dstring(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  default:
		return EEL_XNOTIMPLEMENTED;
	}

	ds = o2EEL_dstring(eo);
	if(ds->length > s2len)
	{
		op2->integer.v = 1;
		return 0;
	}
	else if(ds->length < s2len)
	{
		op2->integer.v = -1;
		return 0;
	}
	op2->integer.v = eel_s_cmp((unsigned char *)ds->buffer,
			(unsigned const char *)s2buf, s2len);
	return 0;
}


static EEL_xno ds_eq(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	switch(op1->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		op2->type = EEL_TBOOLEAN;
		op2->integer.v = 0;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
	  {
		EEL_object *o = op1->objref.v;
		if((EEL_classes)o->type == EEL_CDSTRING)
		{
			op2->type = EEL_TBOOLEAN;
			op2->integer.v = (eo == o);
			return 0;
		}
		else if((EEL_classes)o->type == EEL_CSTRING)
		{
			ds_compare(eo, op1, op2);
			op2->type = EEL_TBOOLEAN;
			op2->integer.v = (op2->integer.v == 0);
			return 0;
		}
		else
		{
			op2->type = EEL_TBOOLEAN;
			op2->integer.v = 0;
			return 0;
		}
	  }
	}
	return EEL_XBADTYPE;
}


static EEL_xno ds_cast_to_real(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_dstring *ds = o2EEL_dstring(src->objref.v);
	eel_d2v(dst, atof(ds->buffer));
	return 0;
}


static EEL_xno ds_cast_to_integer(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_dstring *ds = o2EEL_dstring(src->objref.v);
	eel_l2v(dst, atol(ds->buffer));
	return 0;
}


static EEL_xno ds_cast_to_boolean(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_string *ds = o2EEL_string(src->objref.v);
	dst->type = EEL_TBOOLEAN;
	if(!strncmp("true", ds->buffer, 4) ||
			!strncmp("yes", ds->buffer, 3) ||
			!strncmp("1", ds->buffer, 1) ||
			!strncmp("on", ds->buffer, 2))
		dst->integer.v = 1;
	else
		dst->integer.v = 0;
	return 0;
}


static EEL_xno s_cast_to_string(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_dstring *s = o2EEL_dstring(src->objref.v);
	dst->objref.v = eel_ps_nnew(vm, s->buffer, s->length);
	if(!dst->objref.v)
		return EEL_XMEMORY;
	dst->type = EEL_TOBJREF;
	return 0;
}


static EEL_xno ds_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)

{
	EEL_dstring *ds = o2EEL_dstring(src->objref.v);
	EEL_object *no = ds_nnew(vm, ds->buffer, ds->length);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_cast_from_nil(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no = ds_nnew(vm, "nil", 3);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_cast_from_integer(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no;
	char buf[24];
	int len = snprintf(buf, sizeof(buf) - 1, "%d", src->integer.v);
	if(len >= sizeof(buf))
		return EEL_XOVERFLOW;
	buf[sizeof(buf) - 1] = 0;
	no = ds_nnew(vm, buf, len);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_cast_from_real(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no;
	char buf[64];
	int len = snprintf(buf, sizeof(buf) - 1, EEL_REAL_FMT, src->real.v);
	if(len >= sizeof(buf))
		return EEL_XOVERFLOW;
	buf[sizeof(buf) - 1] = 0;
	no = ds_nnew(vm, buf, len);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_cast_from_boolean(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no;
  	if(src->integer.v)
		no = ds_nnew(vm, "true", 4);
	else
		no = ds_nnew(vm, "false", 5);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_cast_from_typeid(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no = eel_ds_new(vm, eel_typename(vm, src->integer.v));
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno ds_add(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds1 = o2EEL_dstring(eo);
	const char *s2buf;
	int s2len;
	char *buf;
	char cb[1];

	/* Operand */
	switch((EEL_classes)EEL_TYPE(op1))
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2 = o2EEL_string(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *ds2 = o2EEL_dstring(op1->objref.v);
		s2buf = ds2->buffer;
		s2len = ds2->length;
		break;
	  }
	  default:
	  {
		cb[0] = eel_v2l(op1);
		s2buf = cb;
		s2len = 1;
		break;
	  }
	}

	/* Concatenate and return a new string */
	buf = eel_malloc(eo->vm, ds1->length + s2len + 1);
	if(!buf)
		return EEL_XMEMORY;
	memcpy(buf, ds1->buffer, ds1->length);
	memcpy(buf + ds1->length, s2buf, s2len);
	buf[ds1->length + s2len] = 0;
	op2->objref.v = ds_nnew_grab(eo->vm, buf, ds1->length + s2len);
	if(!op2->objref.v)
	{
		eel_free(eo->vm, buf);
		return EEL_XCONSTRUCTOR;
	}
	op2->type = EEL_TOBJREF;
	return 0;
}


static EEL_xno ds_ipadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *ds1 = o2EEL_dstring(eo);
	const char *s2buf;
	int s2len;

	/* Operand */
	switch((EEL_classes)EEL_TYPE(op1))
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2 = o2EEL_string(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *ds2 = o2EEL_dstring(op1->objref.v);
		s2buf = ds2->buffer;
		s2len = ds2->length;
		break;
	  }
	  default:
	  {
	  	EEL_xno x;
	  	eel_o2v(op2, eo);
		x = ds_set_index(eo, ds1->length, op1);
		if(x)
			return x;
		eel_o_own(eo);
		return 0;
	  }
	}

	/* Add to buffer */
	if(ds_setsize(eo, ds1->length + s2len + 1) < 0)
		return EEL_XMEMORY;
	memcpy(ds1->buffer + ds1->length, s2buf, s2len);
	ds1->buffer[ds1->length + s2len] = 0;
	ds1->length += s2len;
	eel_o2v(op2, eo);
	eel_o_own(eo);
	return 0;
}


void eel_cdstring_register(EEL_vm *vm)
{
	EEL_object *c = eel_register_class(vm,
			EEL_CDSTRING, "dstring", EEL_COBJECT,
			ds_construct, ds_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, ds_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, ds_setindex);
	eel_set_metamethod(c, EEL_MM_INSERT, ds_insert);
	eel_set_metamethod(c, EEL_MM_DELETE, ds_delete);
	eel_set_metamethod(c, EEL_MM_COPY, ds_copy);
	eel_set_metamethod(c, EEL_MM_LENGTH, ds_length);
	eel_set_metamethod(c, EEL_MM_COMPARE, ds_compare);
	eel_set_metamethod(c, EEL_MM_EQ, ds_eq);
	eel_set_metamethod(c, EEL_MM_ADD, ds_add);
	eel_set_metamethod(c, EEL_MM_IPADD, ds_ipadd);
	eel_set_casts(vm, EEL_CDSTRING, EEL_CDSTRING, ds_clone);
	eel_set_casts(vm, EEL_CDSTRING, EEL_TREAL, ds_cast_to_real);
	eel_set_casts(vm, EEL_CDSTRING, EEL_TINTEGER, ds_cast_to_integer);
	eel_set_casts(vm, EEL_CDSTRING, EEL_TBOOLEAN, ds_cast_to_boolean);
	eel_set_casts(vm, EEL_CDSTRING, EEL_CSTRING, s_cast_to_string);
	eel_set_casts(vm, EEL_TNIL, EEL_CDSTRING, ds_cast_from_nil);
	eel_set_casts(vm, EEL_TREAL, EEL_CDSTRING, ds_cast_from_real);
	eel_set_casts(vm, EEL_TINTEGER, EEL_CDSTRING, ds_cast_from_integer);
	eel_set_casts(vm, EEL_TBOOLEAN, EEL_CDSTRING, ds_cast_from_boolean);
	eel_set_casts(vm, EEL_TTYPEID, EEL_CDSTRING, ds_cast_from_typeid);
}
