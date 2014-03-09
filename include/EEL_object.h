/*
---------------------------------------------------------------------------
	EEL_object.h - EEL Object
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2012, 2014 David Olofson
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

 	/*
	 * IMPORTANT:
	 *	DO NOT allocate, reallocate or free EEL objects on
	 *	your own! The ONLY thing that will work is to allocate
	 *	objects using eel_o_alloc(), and free them using
	 *	eel_o_free().
	 *	   EEL will free the object struct automatically after
	 *	a destructor is finished, or a constructor fails, so
	 *	eel_o_free() should only be used under special
	 *	circumstances. (See e_string.h for an example.)
	 */

#ifndef EEL_OBJECT_H
#define EEL_OBJECT_H

#include "EEL_export.h"
#include "EEL_types.h"
#include "EEL_value.h"
#include "EEL_vm.h"

#ifdef DEBUG
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Metamethods
 */
typedef enum
{
	EEL_MM_GETINDEX = 0,	/* Get element by index.
				 * In:	op1 -> index (any EEL type)
				 * Out:	*op2 = value (any EEL type)
				 */
	EEL_MM_SETINDEX,	/* Set element by index.
				 * In:	op1 -> index (any EEL type)
				 *	op2 -> value (any EEL type)
				 * Out:	Nothing.
				 */
	EEL_MM_COPY,		/* Get a range of elements.
				 * In:	op1 -> start index (any EEL type)
				 *	op2 = number of elements (integer)
				 * Out:	*op2 = value (any EEL type)
				 */
	EEL_MM_INSERT,		/* Insert element before index.
				 * In:	op1 -> index (any EEL type)
				 *	op2 -> value (any EEL type)
				 * Out:	Nothing.
				 */
	EEL_MM_DELETE,		/* Delete element by index.
				 * In:	op1 -> index (any EEL type)
				 *	op2 = number of elements (integer)
				 * Out:	Nothing.
				 * There are three legal ways to use this:
				 *	1. op1 = index; op2 = count, to delete
				 *	   [index..(index + count - 1)].
				 *	2. op1 = index, op2 = NULL, to delete
				 *	   [index], ie one single element.
				 *	3. op1 = op2 = NULL, to delete all
				 *	   elements.
				 * Objects are *not* required to implement all
				 * three variants, but *must* be aware of them,
				 * and throw exceptions when appropriate!
				 */
#if 0
	EEL_MM_SLICE,		/* Create an object that represents the
				 * specified range of elements, without
				 * copying the data.
				 * In:	op1 -> start index (any EEL type)
				 *	op2 = number of elements (integer)
				 * Out:	*op2 = value (any EEL type)
				 */
#endif
	EEL_MM_LENGTH,		/* Get current number of elements.
				 * In:	Nothing
				 * Out:	*op2 = length (integer)
				 */
	EEL_MM_COMPARE,		/* Compare object to 'op1'
				 **************************************
				 * USE EEL_MM_EQ INSTEAD WHEN POSSIBLE!
				 **************************************
				 * In:	op1 -> value (any EEL type)
				 * Out:	*op2 = result (integer);
				 *	0 if the objects are identical,
				 *	1 if object > 'op1' or
				 *	-1 if object < 'op1'.
				 */
	EEL_MM_EQ,		/* Test object against 'op1' for
				 * equality.
				 * In:	op1 -> value (any EEL type)
				 * Out:	*op2 = result (boolean);
				 *	true if the objects are identical,
				 *	otherwise false.
				 */
	EEL_MM_CAST,		/* 'object' cast to type 'op1'
				 * In:	op1 -> value (EEL_TTYPEID)
				 * Out:	*op2 = new value or object
				 */
	EEL_MM_SERIALIZE,	/* Serialize all contained data and write it
				 * to the provided stream. The generated data
				 * must contain all information required to
				 * reconstruct the instance.
				 * In:	op1 -> target stream
				 * Out:	op2 -> number of bytes written
				 *	       (EEL integer)
				 */

	/*
	 * Operator metamethods:
	 *	In:	op1 -> value (any EEL type)
	 *		op2 -> value for result (any EEL type)
	 *	Out:	*op2 = result
	 *
	 * The EEL_MM_* metamethods are copying versions, and should
	 * NEVER modify the original object. That is, unless the mm
	 * is a NOP, op2 must point to a new instances if the operation
	 * succeeds.
	 *
	 * The EEL_MM_IP* metamethods are inplace versions of their
	 * respective copying metamethods. They MUST perform the
	 * operation on the original object. Consequently, op2 must
	 * always point at the original instance if the operation
	 * succeeds.
	 *
	 * The EEL_MM_*R* metamethods are reverse versions; ie for
	 * <object> OPERATOR <self> operations. These are used when
	 * the left hand object is a simple value. For commutative
	 * operations, these can be wired to the corresponding
	 * forward (normal) callbacks.
	 *
	 * NOTE:
	 *	Write-only objects should normally not implement
	 *	inplace metamethods, as these would have to fail
	 *	at all times, except when the operation does not
	 *	modify the object.
	 */
	/* Straight operators; copying and in-place */
	EEL_MM_POWER,		/* 'object' ** 'op1' */
	EEL_MM_IPPOWER,
	EEL_MM_MOD,		/* 'object' % 'op1' */
	EEL_MM_IPMOD,
	EEL_MM_DIV,		/* 'object' / 'op1' */
	EEL_MM_IPDIV,
	EEL_MM_MUL,		/* 'object' * 'op1' */
	EEL_MM_IPMUL,
	EEL_MM_SUB,		/* 'object' - 'op1' */
	EEL_MM_IPSUB,
	EEL_MM_ADD,		/* 'object' + 'op1' */
	EEL_MM_IPADD,

	/* Reverse operators; copying and in-place */
	EEL_MM_RPOWER,		/* 'op1' ** 'object' */
	EEL_MM_IPRPOWER,
	EEL_MM_RMOD,		/* 'op1' % 'object' */
	EEL_MM_IPRMOD,
	EEL_MM_RDIV,		/* 'op1' / 'object' */
	EEL_MM_IPRDIV,
	EEL_MM_RMUL,		/* 'op1' * 'object' */
	EEL_MM_IPRMUL,
	EEL_MM_RSUB,		/* 'op1' - 'object' */
	EEL_MM_IPRSUB,
	EEL_MM_RADD,		/* 'op1' + 'object' */
	EEL_MM_IPRADD,

	/* Straight vector operators; copying and in-place */
	EEL_MM_VPOWER,		/* 'object' #** 'op1' */
	EEL_MM_IPVPOWER,
	EEL_MM_VMOD,		/* 'object' #% 'op1' */
	EEL_MM_IPVMOD,
	EEL_MM_VDIV,		/* 'object' #/ 'op1' */
	EEL_MM_IPVDIV,
	EEL_MM_VMUL,		/* 'object' #* 'op1' */
	EEL_MM_IPVMUL,
	EEL_MM_VSUB,		/* 'object' #- 'op1' */
	EEL_MM_IPVSUB,
	EEL_MM_VADD,		/* 'object' #+ 'op1' */
	EEL_MM_IPVADD,

	/* Reverse vector operators; copying and in-place */
	EEL_MM_VRPOWER,		/* 'op1' #** 'object' */
	EEL_MM_IPVRPOWER,
	EEL_MM_VRMOD,		/* 'op1' #% 'object' */
	EEL_MM_IPVRMOD,
	EEL_MM_VRDIV,		/* 'op1' #/ 'object' */
	EEL_MM_IPVRDIV,
	EEL_MM_VRMUL,		/* 'op1' #* 'object' */
	EEL_MM_IPVRMUL,
	EEL_MM_VRSUB,		/* 'op1' #- 'object' */
	EEL_MM_IPVRSUB,
	EEL_MM_VRADD,		/* 'op1' #+ 'object' */
	EEL_MM_IPVRADD,

	EEL_MM__COUNT
} EEL_mmindex;


/*
 * EEL Object Header
 *
 * NOTE:
 *	The 'type' field counts as a reference to the
 *	EEL_CCLASS that implements the class. This
 *	ensures that classes are not removed as long as
 *	instances remain.
 */
struct EEL_object
{
	EEL_object	*lnext, *lprev;	/* Limbo object list */
	EEL_vm		*vm;		/* Owner VM */
	void		*weakrefs;	/* Weak reference management */
	int		refcount;	/* Reference count */
	EEL_types	type;		/* Class type ID */
	/*
	 * (Implementation struct follows)
	 */
};


/*
 * Constructor callback
 *
 * IMPORTANT:
 *	* If 'initc' > 0, there are 'initc' initializers.
 *	* Initializers are strictly read only! (Unlike function
 *	  arguments, that is.)
 *	* If 'initc' == 0, there are no initializers, and 'inits'
 *	  may be NULL and should be ignored.
 *	* Negative values of 'initc' may be used internally within
 *	  EEL or external modules, but code should NEVER call
 *	  foreign constructors like that!
 *	* A constructor is responsible for freeing anything it's
 *	  allocated in case of failure - including the object!
 *	  (Must be freed with eel_o_free().)
 *
 * In:	type = object type (for constructors for multiple types)
 *	inits = initializers
 *	initc = number of initializers
 *	result -> space for result EEL_value
 *
 * SUCCESS: Returns 0. 'result' -> OBJREF to new object.
 * FAILURE: Returns exception number. 'result' is undefined.
 */
typedef EEL_xno (*EEL_ctor_cb)(EEL_vm *vm, EEL_types type,
		EEL_value *inits, int initc, EEL_value *result);

/*
 * Reconstructor callback
 *
 * This is a special constructor, designed for the sole purpose
 * of reconstructing objects from streams, parsing data generated
 * by the SERIALIZE metamethod of the same class.
 *
 * In:	type = object type (for constructors for multiple types)
 *	stream = source stream
 *	result -> space for result EEL_value
 *
 * SUCCESS: Returns 0. 'result' -> OBJREF to new object.
 * FAILURE: Returns exception number. 'result' is undefined.
 */
typedef EEL_xno (*EEL_rector_cb)(EEL_vm *vm, EEL_types type,
		EEL_object *stream, EEL_value *result);


/*
 * Destructor callback
 *
 * A destructor is responsible for deallocating or disowning
 * any resources and objects it may be holding, including the
 * object struct.
 *
 * IMPORTANT:
 *	Use eel_o_free() to free the object struct!
 *
 * In:	eo -> object to destroy
 * Out:	EEL_XREFUSE to refuse to die, 0 if destroyed.
 */
typedef EEL_xno (*EEL_dtor_cb)(EEL_object *eo);

/*
 * Metamethod callback
 *
 * SUCCESS: Returns 0. Result operands are defined.
 * FAILURE: Returns exception number. Results undefined.
 */
typedef EEL_xno (*EEL_mm_cb)(EEL_object *object,
		EEL_value *op1, EEL_value *op2);

/*
 * Cast method callback
 *
 * SUCCESS: Returns 0. Result operands are defined.
 * FAILURE: Returns exception number. Results undefined.
 */
typedef EEL_xno (*EEL_cast_cb)(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t);

/*
 * Perform metamethod 'mm' on 'object', with arguments
 * 'op1' and 'op2'. (Wrapper for metamethod callbacks.)
 *
 * SUCCESS: Returns 0. Result operands are defined.
 * FAILURE: Returns exception number. Results undefined.
 */
EELAPI(EEL_xno)eel_o_metamethod(EEL_object *object,
		EEL_mmindex mm, EEL_value *op1, EEL_value *op2);

/*
 * Create an instance of class 'type'.
 *
 * SUCCESS: Points 'result' at instance with refcount 1 and returns 0.
 * FAILURE: Leaves 'result' undefined and returns a VM exception code.
 */
EELAPI(EEL_xno)eel_o_construct(EEL_vm *vm, EEL_types type,
		EEL_value *inits, int initc, EEL_value *result);

/*
 * Create a shallow clone of 'orig'.
 *
 * SUCCESS: Points 'result' at instance with refcount 1 and returns 0.
 * FAILURE: Leaves 'result' undefined and returns a VM exception code.
 */
EELAPI(EEL_xno)eel_o_clone(EEL_vm *vm, EEL_object *orig, EEL_value *result);

/*
 * Allocate an object with 'size' bytes reserved for instance data.
 * The object will be initialized with 'vm' as owner and 'type' in
 * the type field. The instance data area is NOT cleared.
 *
 * SUCCESS:	Returns the address of the object.
 * FAILURE:	Returns NULL.
 *
 * WARNING:
 *	DO NOT free() OR eel_free() POINTERS RETURNED BY THIS CALL!
 */
EELAPI(EEL_object *)eel_o_alloc(EEL_vm *vm, int size, EEL_types type);

/* Untangle and free 'object'. */
EELAPI(void)eel_o_free(EEL_object *object);

/*
 * Defines inlines that extract the class specific instance data from an object,
 * and vice versa. These take and return typed pointers, but perform NO RUN TIME
 * TYPE CHECKING!
 */
#define	EEL_MAKE_CAST(t)		\
static inline t *o2##t(EEL_object *o)	\
{					\
	return (t *)(o + 1);		\
}					\
static inline EEL_object *t##2o(t *o)	\
{					\
	return (EEL_object *)(o) - 1;	\
}


/*
 * Get type of value, whether it's a simple value or an objref.
 */
static inline EEL_types EEL_TYPE(const EEL_value *v)
{
	if(EEL_IS_OBJREF(v->type))
		return v->objref.v->type;
	else
		return v->type;
}


/*
 * Assume shared ownership of object 'o'.
 */
EELAPI(void)eel_own(EEL_object *o);


/*
 * Give up shared ownership of object 'o'.
 *
 * If there are no remaining references to the object, the
 * object will be destroyed - instantly or eventually.
 */
EELAPI(void)eel_disown(EEL_object *o);


/*
 * If 'value' is a (strong) reference to an object, increment that objects
 * refcount. Weak references are ignored!
 */
static inline void eel_v_own(EEL_value *value)
{
	if(value->type == EEL_TOBJREF)
		eel_own(value->objref.v);
#ifdef DEBUG
	else if((EEL_nontypes)value->type == EEL_TILLEGAL)
		fprintf(stderr, "INTERNAL ERROR: eel_v_own(): ILLEGAL value! "
				"(Source: %d)", value->integer.v);
#endif
}

/*
 * Disown any object referred by 'v'. If 'v' is an object reference, the
 * pointer field will be set to NULL. If 'v' is a weak reference, it will
 * be detached from the target object.
 */
EELAPI(void)eel_v_disown(EEL_value *v);


/*
 * Copy value, turning weakrefs into objrefs, incrementing refcounts as needed.
 */
EELAPI(void)eel_copy(EEL_value *value, const EEL_value *from);


/*---------------------------------------------------------
	Convenience inlines for metamethod calling
---------------------------------------------------------*/
EELAPI(long)eel_length(EEL_object *io);
EELAPI(EEL_xno)eel_getindex(EEL_object *io, EEL_value *key, EEL_value *value);
EELAPI(EEL_xno)eel_getlindex(EEL_object *io, long ind, EEL_value *value);
EELAPI(EEL_xno)eel_getsindex(EEL_object *io, const char *key, EEL_value *value);
EELAPI(EEL_xno)eel_setindex(EEL_object *io, EEL_value *key, EEL_value *value);
EELAPI(EEL_xno)eel_setlindex(EEL_object *io, long ind, EEL_value *value);
EELAPI(EEL_xno)eel_setsindex(EEL_object *io, const char *key, EEL_value *value);

EELAPI(EEL_xno)eel_delete(EEL_object *io, EEL_value *key);
EELAPI(EEL_xno)eel_idelete(EEL_object *io, long ind, long count);
EELAPI(EEL_xno)eel_sdelete(EEL_object *io, const char *key);

EELAPI(EEL_xno)eel_insert(EEL_object *io, EEL_value *key, EEL_value *value);

/*
 * Return a string describing object 'o'.
 *
 * NOTE: Uses eel_salloc()!
 */
EELAPI(const char *)eel_o_stringrep(EEL_object *o);

/*
 * Generate an ASCII representation of 'value'.
 *
 * NOTE: Uses eel_salloc()!
 */
EELAPI(const char *)eel_v_stringrep(EEL_vm *vm, const EEL_value *value);


/*---------------------------------------------------------
 * Fast table tools
-----------------------------------------------------------
 *	NOTE:
 *		No polymorphism!
 *		No checking!
 *		Use only on actual tables!
 */
EELAPI(EEL_xno)eel_table_get(EEL_object *to, EEL_value *key, EEL_value *value);
EELAPI(EEL_xno)eel_table_gets(EEL_object *to, const char *key, EEL_value *value);
EELAPI(const char *)eel_table_getss(EEL_object *to, const char *key);

EELAPI(EEL_xno)eel_table_set(EEL_object *to, EEL_value *key, EEL_value *value);
EELAPI(EEL_xno)eel_table_sets(EEL_object *to, const char *key, EEL_value *value);
EELAPI(EEL_xno)eel_table_setss(EEL_object *to, const char *key, const char *value);

EELAPI(EEL_xno)eel_table_delete(EEL_object *to, EEL_value *key);
EELAPI(EEL_xno)eel_table_deletes(EEL_object *to, const char *key);

EELAPI(EEL_xno)eel_insert_lconstants(EEL_object *to, const EEL_lconstexp *data);


/*---------------------------------------------------------
	Low level interfaces - AVOID THESE IF POSSIBLE!
---------------------------------------------------------*/

/*
 * Get raw data of object 'o'. This typically returns a direct pointer to the
 * internal buffer/table/array of the object - and obviously, the layout of that
 * depends entirely on the implementation of the object.
 *
 * WARNING
 *	Never use this unless you REALLY know what you're doing! It's really
 *	just a hack to cover missing low level APIs for the EEL core types.
 */
EELAPI(void *)eel_rawdata(EEL_object *o);


#ifdef __cplusplus
}
#endif

#endif /* EEL_OBJECT_H */
