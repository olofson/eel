/*
---------------------------------------------------------------------------
	EEL_value.h - EEL Data Element (For VM stack, symbol values etc.)
---------------------------------------------------------------------------
 * Copyright 2000, 2002, 2004-2007, 2009-2012, 2019 David Olofson
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

#ifndef EEL_VALUE_H
#define EEL_VALUE_H

#include <math.h>
#include "EEL_export.h"
#include "EEL_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Built-in types and classes
 */
typedef enum
{
	/* Special (non-)type class IDs */
	EEL_CINDEXABLE = -3,	/* "Any indexable class" wildcard */
	EEL_CANY =	-2,	/* "Any class" wildcard */
	EEL_CILLEGAL =	-1,	/* "Illegal type" marker (debug) */

	/* Value types */
	EEL_CNIL =	0,	/* No object, implicit value */
	EEL_CREAL,		/* Real		real.v = value */
	EEL_CINTEGER,		/* Integer	integer.v = value */
	EEL_CBOOLEAN,		/* Boolean	integer.v = value (0 or 1) */
	EEL_CCLASSID,		/* Class ID	integer.v = object class ID */

	/*
	 * Last value type.
	 *
	 * MAKE SURE TO UPDATE THIS AS NEEDED, IF VALUE TYPES ARE CHANGED!
	 */
	EEL_CLASTVALUE = EEL_CCLASSID,

	/* Object reference types (detect with EEL_IS_OBJREF()) */
	EEL_COBJREF,		/* Object ref.	object.v -> EEL_object */
	EEL_CWEAKREF,		/* Weak ref.	object.v -> EEL_object */
				/*		object.index = backref index */

	/* Built-in classes */
	EEL_CVALUE,		/* Base class of all value types */
	EEL_COBJECT,		/* Base class of all objects */

	EEL_CCLASS,		/* Class definition */
	EEL_CSTRING,		/* Pooled string */
	EEL_CDSTRING,		/* Dynamic string */
	EEL_CFUNCTION,		/* Function */
	EEL_CMODULE,		/* Module */
	EEL_CARRAY,		/* Array of EEL_value */
	EEL_CTABLE,		/* <key, value> table */
	EEL_CVECTOR,		/* Vector base class (virtual) */

	/* Subclasses of CVECTOR, for various value types */
	EEL_CVECTOR_U8,		/* 8 bits unsigned*/
	EEL_CVECTOR_S8,		/* 8 bits signed*/
	EEL_CVECTOR_U16,	/* 16 bits unsigned*/
	EEL_CVECTOR_S16,	/* 16 bits signed*/
	EEL_CVECTOR_U32,	/* 32 bits unsigned*/
	EEL_CVECTOR_S32,	/* 32 bits signed*/
	EEL_CVECTOR_F,		/* float (usually 32 bits) */
	EEL_CVECTOR_D,		/* double (usually 64 bits) */

	EEL__CUSER		/* First user defined class ID */
} EEL_classes;


/* Data element */
union EEL_value
{
	EEL_classes	classid;

	/*
	 * Primitive types
	 */
	struct	/* EEL_CREAL */
	{
		EEL_classes	classid;
		EEL_real	v;
	} real;
	struct	/* EEL_CINTEGER, EEL_CBOOLEAN, EEL_CCLASSID */
	{
		EEL_classes	classid;
		EEL_integer	v;
	} integer;

	/*
	 * Object reference types
	 */
	struct	/* EEL_COBJREF, EEL_CWEAKREF */
	{
		EEL_classes	classid;
		EEL_index	index;	/* Index into weakref vector */
		EEL_object	*v;	/* NULL is illegal! */
	} objref;

	char sizecheck[EEL_VALUE_SIZE];
};
#define EEL_COMPILE_TIME_ASSERT(name, x)               \
       typedef int EEL_dummy_ ## name[(x) * 2 - 1]
EEL_COMPILE_TIME_ASSERT(eel_data, sizeof(EEL_value) == EEL_VALUE_SIZE);
#undef EEl_COMPILE_TIME_ASSERT


/*
 * This is "true" for any class that has an object reference.
 * (Currently, that means EEL_COBJREF and EEL_CWEAKREF.)
 */
#define	EEL_IS_OBJREF(x)	((x) >= EEL_COBJREF)

static inline EEL_object *eel_v2o(EEL_value *v)
{
	return v->objref.v;
}


/*
 * A weakrefs index field is set to this value before the weakref is wired to
 * its target.
 */
#define	EEL_WEAKREF_UNWIRED	(-1)


/*
 * Cast an EEL value to the respective C type.
 *
 * NOTE:
 * 	Don't use the eel__v2*() versions directly!
 *	They may not recognise the built-in simple
 *	types, since those are supposed to be handled
 *	by the eel_v2*() inlines.
 */
EELAPI(long int)eel__v2l(EEL_value *v);
EELAPI(double)eel__v2d(EEL_value *v);
static inline long int eel_v2l(EEL_value *v)
{
	switch(v->classid)
	{
	  case EEL_CNIL:
		return 0;
	  case EEL_CREAL:
#ifdef __cplusplus
		return (long int)floor(v->real.v);
#else
		return floor(v->real.v);
#endif
	  case EEL_CINTEGER:
	  case EEL_CBOOLEAN:
	  case EEL_CCLASSID:
		return v->integer.v;
	  default:
		return eel__v2l(v);
	}
}
static inline double eel_v2d(EEL_value *v)
{
	switch(v->classid)
	{
	  case EEL_CNIL:
		return 0.0f;
	  case EEL_CREAL:
		return v->real.v;
	  case EEL_CINTEGER:
	  case EEL_CBOOLEAN:
	  case EEL_CCLASSID:
		return v->integer.v;
	  default:
		return eel__v2d(v);
	}
}


/*
 * Convenience inlines for generating values
 */

static inline void eel_nil2v(EEL_value *v)
{
	v->classid = EEL_CNIL;
}

static inline void eel_b2v(EEL_value *v, int b)
{
	v->classid = EEL_CBOOLEAN;
	v->integer.v = b ? 1 : 0;
}

static inline void eel_l2v(EEL_value *v, long l)
{
	v->classid = EEL_CINTEGER;
	v->integer.v = l;
}

static inline void eel_d2v(EEL_value *v, double d)
{
	v->classid = EEL_CREAL;
	v->real.v = d;
}

static inline void eel_o2v(EEL_value *v, EEL_object *o)
{
	v->classid = EEL_COBJREF;
	v->objref.v = o;
}

/*
 * Create an "unwired" weak reference. This has to be copied
 * to a "final" location using some other call, somehow invoking
 * eel_weakref_attach().
 */
static inline void eel_o2wr(EEL_value *v, EEL_object *o)
{
	v->classid = EEL_CWEAKREF;
	v->objref.index = EEL_WEAKREF_UNWIRED;
	v->objref.v = o;
}

#ifdef __cplusplus
}
#endif

#endif	/* EEL_VALUE_H */
