/*
---------------------------------------------------------------------------
	e_operate.h - Operations on values and objects
---------------------------------------------------------------------------
 * Copyright 2005-2007, 2009-2012, 2014 David Olofson
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

#ifndef	EEL_E_OPERATE_H
#define	EEL_E_OPERATE_H

#include <math.h>
#include "EEL.h"
#include "e_error.h"
#include "e_class.h"
#include "e_object.h"
#include "e_state.h"
#include "e_cast.h"
#include "e_config.h"

#define	EEL_NTYPES		(EEL_TLASTTYPE + 1)
#define	EEL_MK2TYPES(l, r)	((EEL_NTYPES * EEL_NTYPES) + \
						(l) * EEL_NTYPES + (r))


/*----------------------------------------------------------
	Operators
----------------------------------------------------------*/
typedef enum
{
	EEL_OP_ASSIGN = 0,
	EEL_OP_WKASSN,

	/* Arithmetics (corresponds to the operator metamethods) */
	EEL_OP_POWER,
	EEL_OP_MOD,
	EEL_OP_DIV,
	EEL_OP_MUL,
	EEL_OP_SUB,
	EEL_OP_ADD,

	/* Vector arithmetics (corresponds to the vector metamethods) */
	EEL_OP_VPOWER,
	EEL_OP_VMOD,
	EEL_OP_VDIV,
	EEL_OP_VMUL,
	EEL_OP_VSUB,
	EEL_OP_VADD,

	/* Bitwise logic */
	EEL_OP_BAND,
	EEL_OP_BOR,
	EEL_OP_BXOR,
	EEL_OP_SHL,
	EEL_OP_SHR,
	EEL_OP_ROL,
	EEL_OP_ROR,
	EEL_OP_BREV,

	/* Binary, boolean result */
	EEL_OP_AND,
	EEL_OP_OR,
	EEL_OP_XOR,
	EEL_OP_EQ,	/* !EEL_MM_NE */
	EEL_OP_NE,
	EEL_OP_GT,	/* EEL_MM_COMPARE */
	EEL_OP_GE,	/* EEL_MM_COMPARE */
	EEL_OP_LT,	/* EEL_MM_COMPARE */
	EEL_OP_LE,	/* EEL_MM_COMPARE */
	EEL_OP_IN,

	/* Selectors */
	EEL_OP_MAX,	/* EEL_MM_COMPARE */
	EEL_OP_MIN,	/* EEL_MM_COMPARE */

	/* Unary */
	EEL_OP_NEG,
	EEL_OP_CASTR,
	EEL_OP_CASTI,
	EEL_OP_CASTB,
	EEL_OP_TYPEOF,
	EEL_OP_SIZEOF,
	EEL_OP_CLONE,

	/* Unary, boolean result */
	EEL_OP_NOT,

	/* Unary, bitwise */
	EEL_OP_BNOT

} EEL_operators;

/* Returns a string with the name of operator 'op'. */
const char *eel_opname(EEL_operators op);


/*----------------------------------------------------------
	Normal and reverse ops for objects only
----------------------------------------------------------*/
EEL_xno eel_object_op(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result, int inplace);
EEL_xno eel_object_rop(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result, int inplace);


/*----------------------------------------------------------
	Metamethod calling
----------------------------------------------------------*/

static inline EEL_xno eel_o__metamethod(EEL_object *object,
		EEL_mmindex mm, EEL_value *op1, EEL_value *op2)
{
#ifdef EEL_VM_CHECKING
	EEL_vm *vm = object->vm;
	EEL_xno x;
	EEL_object *c;
	EEL_classdef *cd;
	DBGM(int old_owns = VMP->owns;)
	if(object->type >= VMP->state->nclasses)
		return EEL_XBADCLASS;
	c = VMP->state->classes[object->type];
	cd = o2EEL_classdef(c);
	x = cd->mmethods[mm](object, op1, op2);
	DBGM(if(!x && (mm != EEL_MM_DELETE) && (op2->type == EEL_TOBJREF) &&
			(old_owns == VMP->owns))
		eel_ierror(VMP->state, "Metamethod %s::%s received or "
				"returned an object, but no one inc'ed any "
				"refcount!\n", eel_typename(vm, object->type),
				eel_mm_name(mm));)
	return x;
#else
	EEL_vm *vm = object->vm;
	EEL_object *c = VMP->state->classes[object->type];
	EEL_classdef *cd = o2EEL_classdef(c);
	return cd->mmethods[mm](object, op1, op2);
#endif
}


/*
 * Get an integer value in order to index something.
 *
 * NOTE:
 *	This is for low level VM stuff, not generic object[value] indexing.
 */
static inline int eel_get_indexval(EEL_vm *vm, EEL_value *v)
{
	switch(v->type)
	{
	  case EEL_TNIL:
		return -1;
	  case EEL_TREAL:
		return floor(v->real.v);
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		return v->integer.v;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return -1;
	}
	return -1;
}


/*
 * Extract a real numbered value from 'v', if possible.
 * Returns a VM exception code if the operation fails.
 */
static inline double eel_get_realval(EEL_vm *vm, EEL_value *v, double *dv)
{
	switch(v->type)
	{
	  case EEL_TNIL:
		*dv = 0.0;
		return 0;
	  case EEL_TREAL:
		*dv = v->real.v;
		return 0;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		*dv = v->integer.v;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
	  {
		EEL_xno x;
		EEL_value result;
		x = eel_cast(vm, v, &result, EEL_TREAL);
		if(x)
			return x;
		*dv = result.integer.v;
		return 0;
	  }
	}
	return EEL_XBADTYPE;
}


/*----------------------------------------------------------
	(Utils for the below)
----------------------------------------------------------*/

static inline EEL_xno eel__op_fallback(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result)
{
	if(EEL_IS_OBJREF(left->type))
		return eel_object_op(left, binop, right, result, 0);
	else if(EEL_IS_OBJREF(right->type))
		return eel_object_rop(right, binop, left, result, 0);
	else
		return EEL_XNOTIMPLEMENTED;
}


/*----------------------------------------------------------
	Boolean Operations
----------------------------------------------------------*/

static inline int eel_test_nz(EEL_vm *vm, EEL_value *opr)
{
	DBG6(printf("test_nz heap[%ld] ", opr - vm->heap);)
	switch(opr->type)
	{
	  case EEL_TNIL:
		DBG6(printf(" NIL\n");)
		return 0;
	  case EEL_TREAL:
		DBG6(printf(" REAL %g\n", opr->real.v);)
		return opr->real.v != 0.0;
	  case EEL_TINTEGER:
		DBG6(printf(" INTEGER %d\n", opr->integer.v);)
		return opr->integer.v != 0;
	  case EEL_TBOOLEAN:
		DBG6(printf(" BOOLEAN %s\n", opr->integer.v ? "true" : "false");)
		return opr->integer.v;
	  case EEL_TTYPEID:
		DBG6(printf(" TYPEID %s\n", eel_typename(vm, opr->integer.v));)
		return 1;
	  case EEL_TOBJREF:	/* An object is "something". (nil is nothing.) */
		DBG6(printf(" OBJREF %s\n", eel_v_stringrep(vm, opr));)
		return 1;
	  case EEL_TWEAKREF:
		DBG6(printf(" WEAKREF %s\n", eel_v_stringrep(vm, opr));)
		return 1;
	}
	DBG6(printf(" ILLEGAL TYPE!\n");)
	return 0;	/* Warning eliminator */
}

static inline EEL_xno eel_op_and(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v && right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v && right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v && right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v && right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_AND, right, result);
	}
}

static inline EEL_xno eel_op_or(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = right->real.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = right->integer.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v || right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v || right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v || right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v || right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_OR, right, result);
	}
}

static inline EEL_xno eel_op_xor(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = right->real.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = right->integer.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v != 0.0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (left->real.v != 0.0) ^ (right->real.v != 0.0);
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (left->real.v != 0.0) ^ (right->integer.v != 0);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (left->integer.v != 0) ^ (right->real.v != 0.0);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (left->integer.v != 0) ^ (right->integer.v != 0);
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_XOR, right, result);
	}
}

static inline EEL_xno eel_op_eq(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v == right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v == (EEL_real)right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (EEL_real)left->integer.v == right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v == right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_EQ, right, result);
	}
}

static inline EEL_xno eel_op_ne(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	EEL_xno res = eel_op_eq(left, right, result);
	if(res)
		return res;
	result->integer.v = !result->integer.v;
	return 0;
}

static inline EEL_xno eel_op_gt(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v > right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v > (EEL_real)right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (EEL_real)left->integer.v > right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v > right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_GT, right, result);
	}
}

static inline EEL_xno eel_op_ge(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v >= right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->real.v >= (EEL_real)right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TBOOLEAN;
		result->integer.v = (EEL_real)left->integer.v >= right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v >= right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_GE, right, result);
	}
}

static inline EEL_xno eel_op_lt(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	EEL_xno res = eel_op_ge(left, right, result);
	if(res)
		return res;
	result->integer.v = !result->integer.v;
	return 0;
}

static inline EEL_xno eel_op_le(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	EEL_xno res = eel_op_gt(left, right, result);
	if(res)
		return res;
	result->integer.v = !result->integer.v;
	return 0;
}


/*----------------------------------------------------------
	Arithmetic Operations
----------------------------------------------------------*/

static inline EEL_xno eel_op_power(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TINTEGER;
		result->integer.v = 1;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = pow(left->real.v, right->real.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		result->real.v = pow(left->real.v, right->integer.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = pow(left->integer.v, right->real.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = floor(pow(left->integer.v,
				right->integer.v));
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_POWER, right, result);
	}
}

static inline EEL_xno eel_op_mod(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		return EEL_XDIVBYZERO;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = fmod(left->real.v, right->real.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = fmod(left->real.v, right->integer.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = fmod(left->integer.v, right->real.v);
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->integer.v = left->integer.v % right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_MOD, right, result);
	}
}

static inline EEL_xno eel_op_div(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		return EEL_XDIVBYZERO;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = left->real.v / right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = left->real.v / right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		if(!right->real.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = left->integer.v / right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
#ifdef EEL_PASCAL_IDIV
		result->type = EEL_TREAL;
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->real.v = (EEL_real)left->integer.v /
					right->integer.v;
		return 0;
#endif
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
		result->type = EEL_TINTEGER;
		if(!right->integer.v)
			return EEL_XDIVBYZERO;
		else
			result->integer.v = left->integer.v / right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_DIV, right, result);
	}
}

static inline EEL_xno eel_op_mul(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->real.v * right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		result->real.v = left->real.v * right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->integer.v * right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v * right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_MUL, right, result);
	}
}

static inline EEL_xno eel_op_sub(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
		result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = - right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = - right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
		result->type = EEL_TREAL;
		result->real.v = left->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->real.v - right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		result->real.v = left->real.v - right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->integer.v - right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v - right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_SUB, right, result);
	}
}

static inline EEL_xno eel_op_add(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TNIL):
		result->type = EEL_TNIL;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TNIL, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = - right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TNIL):
		result->type = EEL_TREAL;
		result->real.v = left->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TNIL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TNIL):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->real.v + right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TTYPEID):
		result->type = EEL_TREAL;
		result->real.v = left->real.v + right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TREAL):
		result->type = EEL_TREAL;
		result->real.v = left->integer.v + right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TTYPEID):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TTYPEID, EEL_TTYPEID):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v + right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_ADD, right, result);
	}
}


/*----------------------------------------------------------
	Bit Operations
----------------------------------------------------------*/

static inline EEL_xno eel_op_band(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v & right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v & -right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = -left->integer.v & right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v && right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_BAND, right, result);
	}
}

static inline EEL_xno eel_op_bor(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v | right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v | -right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = -left->integer.v | right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v || right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_BOR, right, result);
	}
}

static inline EEL_xno eel_op_bxor(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v ^ right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v ^ -right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = -left->integer.v ^ right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TBOOLEAN):
		result->type = EEL_TBOOLEAN;
		result->integer.v = left->integer.v ^ right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_BXOR, right, result);
	}
}

static inline EEL_xno eel_op_shl(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
		result->integer.v = (int)left->real.v << right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v << (int)right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TINTEGER;
		result->integer.v = (int)left->real.v << (int)right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v << right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_SHL, right, result);
	}
}

static inline EEL_xno eel_op_shr(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TBOOLEAN):
		result->integer.v = (int)left->real.v >> right->integer.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TREAL):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TREAL):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v >> (int)right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TREAL, EEL_TREAL):
		result->type = EEL_TINTEGER;
		result->integer.v = (int)left->real.v >> (int)right->real.v;
		return 0;
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
		result->type = EEL_TINTEGER;
		result->integer.v = left->integer.v >> right->integer.v;
		return 0;
	  default:
		return eel__op_fallback(left, EEL_OP_SHR, right, result);
	}
}

static inline EEL_xno eel_op_rol(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  {
		int sh = right->integer.v & 31;
		unsigned int v = left->integer.v;
		result->type = EEL_TINTEGER;
		result->integer.v = (v << sh) | (v >> (32 - sh));
		return 0;
	  }
	  default:
		return eel__op_fallback(left, EEL_OP_ROL, right, result);
	}
}

static inline EEL_xno eel_op_ror(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TBOOLEAN):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  {
	  	int sh = right->integer.v & 31;
		unsigned int v = left->integer.v;
		result->type = EEL_TINTEGER;
		result->integer.v = (v >> sh) | (v << (32 - sh));
		return 0;
	  }
	  default:
		return eel__op_fallback(left, EEL_OP_ROR, right, result);
	}
}

static inline EEL_xno eel_op_brev(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	switch(EEL_MK2TYPES(left->type, right->type))
	{
	  case EEL_MK2TYPES(EEL_TINTEGER, EEL_TINTEGER):
	  case EEL_MK2TYPES(EEL_TBOOLEAN, EEL_TINTEGER):
	  {
/*FIXME: Add support for non-32-bit EEL_integer! */
		EEL_uint32 tmp;
		if(right->integer.v > 32)
			return EEL_XHIGHVALUE;
		else if(right->integer.v < 0)
			return EEL_XLOWVALUE;
		tmp = left->integer.v;
		tmp = (tmp << 16) | (tmp >> 16);
		tmp = ((tmp & 0x00ff00ff) << 8) | ((tmp & 0xff00ff00) >> 8);
		tmp = ((tmp & 0x0f0f0f0f) << 4) | ((tmp & 0xf0f0f0f0) >> 4);
		tmp = ((tmp & 0x33333333) << 2) | ((tmp & 0xcccccccc) >> 2);
		tmp = ((tmp & 0x55555555) << 1) | ((tmp & 0xaaaaaaaa) >> 1);
		tmp >>= 32 - right->integer.v;
		result->type = EEL_TINTEGER;
		result->integer.v = tmp;
		return 0;
	  }
	  default:
		return eel__op_fallback(left, EEL_OP_BREV, right, result);
	}
}


/*----------------------------------------------------------
	Selector Operations
----------------------------------------------------------*/

static inline EEL_xno eel_op_max(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	EEL_value r;
	EEL_xno res = eel_op_ge(left, right, &r);
	if(res)
		return res;
	if(r.integer.v)
		eel_v_copy(result, left);
	else
		eel_v_copy(result, right);
	return 0;
}

static inline EEL_xno eel_op_min(EEL_value *left, EEL_value *right,
		EEL_value *result)
{
	EEL_value r;
	EEL_xno res = eel_op_ge(left, right, &r);
	if(res)
		return res;
	if(r.integer.v)
		eel_v_copy(result, right);
	else
		eel_v_copy(result, left);
	return 0;
}


/*----------------------------------------------------------
	Unary Operations
----------------------------------------------------------*/

static inline EEL_xno eel_op_neg(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TREAL:
		result->type = EEL_TREAL;
		result->real.v = -right->real.v;
		return 0;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
		result->type = EEL_TINTEGER;
		result->integer.v = -right->integer.v;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return EEL_XNOTIMPLEMENTED;
	  case EEL_TNIL:
	  case EEL_TTYPEID:
		return EEL_XWRONGTYPE;
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_castr(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
		result->type = EEL_TREAL;
		result->real.v = 0.0;
		return 0;
	  case EEL_TREAL:
		result->type = EEL_TREAL;
		result->real.v = right->real.v;
		return 0;
	  case EEL_TINTEGER:
	  case EEL_TTYPEID:
		result->type = EEL_TREAL;
		result->real.v = right->integer.v;
		return 0;
	  case EEL_TBOOLEAN:
		result->type = EEL_TREAL;
		result->real.v = right->integer.v ? 1.0 : 0.0;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return eel_cast(right->objref.v->vm, right, result, EEL_TREAL);
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_casti(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
		result->type = EEL_TINTEGER;
		result->integer.v = 0;
		return 0;
	  case EEL_TREAL:
		result->type = EEL_TINTEGER;
		result->integer.v = floor(right->real.v);
		return 0;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		result->type = EEL_TINTEGER;
		result->integer.v = right->integer.v;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return eel_cast(right->objref.v->vm,
				right, result, EEL_TINTEGER);
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_castb(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	  case EEL_TREAL:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0.0 != right->real.v;
		return 0;
	  case EEL_TINTEGER:
	  case EEL_TTYPEID:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0 != right->integer.v;
		return 0;
	  case EEL_TBOOLEAN:
		result->type = EEL_TBOOLEAN;
		result->integer.v = right->integer.v;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return eel_cast(right->objref.v->vm,
				right, result, EEL_TBOOLEAN);
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_typeof(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
		result->type = EEL_TNIL;
		return 0;
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		result->integer.v = right->type;
		result->type = EEL_TTYPEID;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		result->integer.v = right->objref.v->type;
		result->type = EEL_TTYPEID;
		return 0;
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_sizeof(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		result->type = EEL_TINTEGER;
		result->integer.v = 1;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return eel_o__metamethod(right->objref.v,
				EEL_MM_LENGTH, NULL, result);
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_clone(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		eel_v_qcopy(result, right);
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		return eel_cast(right->objref.v->vm,
				right, result, EEL_TYPE(right));
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_not(EEL_value *right, EEL_value *result)
{
	switch(right->type)
	{
	  case EEL_TNIL:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 1;
		return 0;
	  case EEL_TREAL:
		result->type = EEL_TREAL;
		result->real.v = 0 == right->real.v;
		return 0;
	  case EEL_TINTEGER:
		result->type = EEL_TINTEGER;
		result->integer.v = !right->integer.v;
		return 0;
	  case EEL_TBOOLEAN:
		result->type = EEL_TBOOLEAN;
		result->integer.v = !right->integer.v;
		return 0;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
	  case EEL_TTYPEID:
		result->type = EEL_TBOOLEAN;
		result->integer.v = 0;
		return 0;
	}
	return EEL_XWRONGTYPE;
}

static inline EEL_xno eel_op_bnot(EEL_value *right, EEL_value *result)
{
	if(right->type != EEL_TINTEGER)
		return EEL_XWRONGTYPE;
	result->type = EEL_TINTEGER;
	result->integer.v = ~right->integer.v;
	return 0;
}


/*----------------------------------------------------------
	Catch-all for unary operators
------------------------------------------------------------
 * NOTE:
 *	For constant the folding in ec_manip.c. The VM uses the specific
 *	operator calls directly.
 */

static inline EEL_xno eel_unop(int unop, EEL_value *right, EEL_value *result)
{
	switch(unop)
	{
	  case EEL_OP_NEG:	return eel_op_neg(right, result);
	  case EEL_OP_CASTR:	return eel_op_castr(right, result);
	  case EEL_OP_CASTI:	return eel_op_casti(right, result);
	  case EEL_OP_CASTB:	return eel_op_castb(right, result);
	  case EEL_OP_TYPEOF:	return eel_op_typeof(right, result);
	  case EEL_OP_SIZEOF:	return eel_op_sizeof(right, result);
	  case EEL_OP_CLONE:	return eel_op_clone(right, result);
	  case EEL_OP_NOT:	return eel_op_not(right, result);
	  case EEL_OP_BNOT:	return eel_op_bnot(right, result);
	  default:		return EEL_XNOTIMPLEMENTED;
	}
}


/*----------------------------------------------------------
	Catch-(almost)-all binary operator implementation
------------------------------------------------------------
 * NOTE:
 *	This is really for the VM implementation, and it generally does not
 *	implement operations that have dedicated VM instructions!
 */

static inline EEL_xno eel__operate(EEL_value *left, int binop,
		EEL_value *right, EEL_value *result)
{
	switch(binop)
	{
	  case EEL_OP_POWER:	return eel_op_power(left, right, result);
	  case EEL_OP_MOD:	return eel_op_mod(left, right, result);
	  case EEL_OP_DIV:	return eel_op_div(left, right, result);
	  case EEL_OP_MUL:	return eel_op_mul(left, right, result);
	  case EEL_OP_SUB:	return eel_op_sub(left, right, result);
	  case EEL_OP_ADD:	return eel_op_add(left, right, result);

	  case EEL_OP_BAND:	return eel_op_band(left, right, result);
	  case EEL_OP_BOR:	return eel_op_bor(left, right, result);
	  case EEL_OP_BXOR:	return eel_op_bxor(left, right, result);
	  case EEL_OP_SHL:	return eel_op_shl(left, right, result);
	  case EEL_OP_SHR:	return eel_op_shr(left, right, result);
	  case EEL_OP_ROL:	return eel_op_rol(left, right, result);
	  case EEL_OP_ROR:	return eel_op_ror(left, right, result);
	  case EEL_OP_BREV:	return eel_op_brev(left, right, result);

	  case EEL_OP_AND:	return eel_op_and(left, right, result);
	  case EEL_OP_OR:	return eel_op_or(left, right, result);
	  case EEL_OP_XOR:	return eel_op_xor(left, right, result);
	  case EEL_OP_EQ:	return eel_op_eq(left, right, result);
	  case EEL_OP_NE:	return eel_op_ne(left, right, result);
	  case EEL_OP_GT:	return eel_op_gt(left, right, result);
	  case EEL_OP_GE:	return eel_op_ge(left, right, result);
	  case EEL_OP_LT:	return eel_op_lt(left, right, result);
	  case EEL_OP_LE:	return eel_op_le(left, right, result);

	  case EEL_OP_MIN:	return eel_op_min(left, right, result);
	  case EEL_OP_MAX:	return eel_op_max(left, right, result);

	  default:
		return eel__op_fallback(left, binop, right, result);
	}
}

static inline EEL_xno eel__ipoperate(EEL_value *left, int binop,
		EEL_value *right, EEL_value *result)
{
	if(EEL_IS_OBJREF(left->type))
		return eel_object_op(left, binop, right, result, 1);
	if((left->type >= EEL_NTYPES) || (right->type >= EEL_NTYPES))
		return EEL_XNOTIMPLEMENTED;
	return EEL_XCANTINPLACE;
}

#ifdef EEL_VM_CHECKING
static inline EEL_xno eel_operate(EEL_value *left, int binop, EEL_value *right,
		EEL_value *result)
{
		EEL_xno x;
		EEL_value rv;
		rv.type = EEL_TILLEGAL;
		EEL_VMCHECK(rv.integer.v = 1004;)
		x = eel__operate(left, binop, right, &rv);
		if(x)
			return x;
		if((EEL_nontypes)rv.type == EEL_TILLEGAL)
		{
			eel_vmdump(NULL, "Operator generated no result! "
					"Metamethod bug?");
			return EEL_XINTERNAL;
		}
		eel_v_move(result, &rv);
		return 0;
}
static inline EEL_xno eel_ipoperate(EEL_value *left, int binop,
		EEL_value *right, EEL_value *result)
{
		EEL_xno x;
		EEL_value rv;
		rv.type = EEL_TILLEGAL;
		EEL_VMCHECK(rv.integer.v = 1005;)
		x = eel__ipoperate(left, binop, right, &rv);
		if(x)
			return x;
		if((EEL_nontypes)rv.type == EEL_TILLEGAL)
		{
			eel_vmdump(NULL, "Operator generated no result! "
					"Metamethod bug?");
			return EEL_XINTERNAL;
		}
		eel_v_move(result, &rv);
		return 0;
}
#else
#  define	eel_operate	eel__operate
#  define	eel_ipoperate	eel__ipoperate
#endif

#endif /* EEL_E_OPERATE_H */
