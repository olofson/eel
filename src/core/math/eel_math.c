/*
---------------------------------------------------------------------------
	eel_math.c - EEL standard math module
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009-2010, 2012, 2019 David Olofson
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "eel_math.h"
#include "EEL_register.h"
#include "e_vector.h"

#define	MATHWRAP(x)						\
static EEL_xno m_##x(EEL_vm *vm)				\
{								\
	EEL_value *arg = vm->heap + vm->argv;			\
	eel_d2v(vm->heap + vm->resv, x(eel_v2d(arg)));		\
	return 0;						\
}

static EEL_xno errno2x(int e)
{
	switch(e)
	{
	  case EDOM:
		return EEL_XDOMAIN;
	}
	return EEL_XMATHERROR;
}

#define	MATHWRAP_CHECK(x)					\
static EEL_xno m_##x(EEL_vm *vm)				\
{								\
	EEL_value *arg = vm->heap + vm->argv;			\
	errno = 0;						\
	eel_d2v(vm->heap + vm->resv, x(eel_v2d(arg)));		\
	if(errno)						\
		return errno2x(errno);				\
	return 0;						\
}

static EEL_xno m_abs(EEL_vm *vm)
{
	EEL_vector *v, *r;
	EEL_value *arg = vm->heap + vm->argv;
	int i, size;
	switch(EEL_CLASS(arg))
	{
	  case EEL_CNIL:
	  case EEL_CBOOLEAN:
		vm->heap[vm->resv] = *arg;
		return 0;
	  case EEL_CINTEGER:
		eel_l2v(vm->heap + vm->resv, labs(arg->integer.v));
		return 0;
	  case EEL_CREAL:
		eel_d2v(vm->heap + vm->resv, fabs(arg->real.v));
		return 0;
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
	  case EEL_CVECTOR_D:
	  {
		EEL_object *o;
		size = eel_length(arg->objref.v);
		o = eel_cv_new_noinit(vm, EEL_CLASS(arg), size);
		if(!o)
			return EEL_XMEMORY;
		eel_o2v(vm->heap + vm->resv, o);
		v = o2EEL_vector(arg->objref.v);
		r = o2EEL_vector(o);
		break;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
	switch(EEL_CLASS(arg))
	{
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_U32:
		memcpy(r->buffer.u8, v->buffer.u8, size * v->isize);
		break;
	  case EEL_CVECTOR_S8:
		for(i = 0; i < size; ++i)
			r->buffer.s8[i] = labs(v->buffer.s8[i]);
		break;
	  case EEL_CVECTOR_S16:
		for(i = 0; i < size; ++i)
			r->buffer.s16[i] = labs(v->buffer.s16[i]);
		break;
	  case EEL_CVECTOR_S32:
		for(i = 0; i < size; ++i)
			r->buffer.s32[i] = labs(v->buffer.s32[i]);
		break;
	  case EEL_CVECTOR_F:
		for(i = 0; i < size; ++i)
			r->buffer.f[i] = fabsf(v->buffer.f[i]);
		break;
	  case EEL_CVECTOR_D:
		for(i = 0; i < size; ++i)
			r->buffer.d[i] = fabs(v->buffer.d[i]);
		break;
	  default:
		return EEL_XWRONGTYPE;	/* Warning eliminator... */
	}
	return 0;
}
MATHWRAP(ceil)
MATHWRAP(floor)
MATHWRAP(sqrt)
MATHWRAP(log)
MATHWRAP(log10)
MATHWRAP(exp)
static EEL_xno m_ldexp(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, ldexp(eel_v2d(arg), eel_v2d(arg + 1)));
	return 0;
}
MATHWRAP(sin)
MATHWRAP(cos)
MATHWRAP(tan)
MATHWRAP_CHECK(asin)
MATHWRAP_CHECK(acos)
MATHWRAP(atan)
static EEL_xno m_atan2(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, atan2(eel_v2d(arg), eel_v2d(arg + 1)));
	return 0;
}


static EEL_xno m_unload(EEL_object *m, int closing)
{
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


EEL_xno eel_math_init(EEL_vm *vm)
{
	EEL_object *m = eel_create_module(vm, "math", m_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	eel_export_cfunction(m, 1, "abs", 1, 0, 0, m_abs);
	eel_export_cfunction(m, 1, "ceil", 1, 0, 0, m_ceil);
	eel_export_cfunction(m, 1, "floor", 1, 0, 0, m_floor);
	eel_export_cfunction(m, 1, "sqrt", 1, 0, 0, m_sqrt);
	eel_export_cfunction(m, 1, "log", 1, 0, 0, m_log);
	eel_export_cfunction(m, 1, "log10", 1, 0, 0, m_log10);
	eel_export_cfunction(m, 1, "exp", 1, 0, 0, m_exp);
	eel_export_cfunction(m, 1, "ldexp", 2, 0, 0, m_ldexp);
	eel_export_cfunction(m, 1, "sin", 1, 0, 0, m_sin);
	eel_export_cfunction(m, 1, "cos", 1, 0, 0, m_cos);
	eel_export_cfunction(m, 1, "tan", 1, 0, 0, m_tan);
	eel_export_cfunction(m, 1, "asin", 1, 0, 0, m_asin);
	eel_export_cfunction(m, 1, "acos", 1, 0, 0, m_acos);
	eel_export_cfunction(m, 1, "atan", 1, 0, 0, m_atan);
	eel_export_cfunction(m, 1, "atan2", 2, 0, 0, m_atan2);

#ifdef M_PIl
	eel_export_dconstant(m, "PI", M_PIl);
#else
	eel_export_dconstant(m, "PI", M_PI);
#endif
#ifdef M_El
	eel_export_dconstant(m, "E", M_El);
#else
	eel_export_dconstant(m, "E", M_E);
#endif

	eel_disown(m);
	return 0;
}
