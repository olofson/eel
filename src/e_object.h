/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_object.h - EEL Object (Internal)
---------------------------------------------------------------------------
 * Copyright (C) 2004-2007, 2009-2011 David Olofson
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

/*
   VM coding and weakrefs
   ----------------------
	Weakrefs are implemented as a special value type EEL_TWEAKREF.
	It is similar to EEL_TOBJREF, but adds an index field in the
	value struct, and an array of back-pointers in the target
	object. (The index field is a performance hack, and holds the
	index of the particular back-pointer to the value at hand.)
	   The VM has a special instruction WEAKREF, that creates an
	"unwired" weakref. As the unwired weakref is copied to its final
	location, it is wired to its target object, and becomes an
	actual weakref.
	   Currently, only assigments to static variables and SETINDEX
	to container objects (tables and arrays) will accept weakrefs.
	Any other assignments - including reading of static variables
	and GETINDEX - will turn a weakref into an objref! The only
	exception is cloning container objects, where clones will get
	their own, new weakrefs.
	   Weakrefs (obviously!) are not reflected by the refcounts of
	their target objects, and thus, objects may be destroyed while
	there are "live" weakrefs around. When that happens, the
	weakrefs are turned into 'nil' values.

	Coding rules:
		* Unwired weakrefs MUST be wired before doing anything
		  that may cause objects to be destroyed! They're
		  really just a shortcut to avoid explicit "weak
		  assignment" operators, metamethods etc. Basically,
		  always issue the WEAKREF instruction right before
		  the instruction(s) that write(s) the weakref to its
		  final destination.
 */


#ifndef EEL_E_OBJECT_H
#define EEL_E_OBJECT_H

#if (DBGK(1)+0 == 1) || (DBGL(1)+0 == 1) || defined (EEL_CLEAN_COPY) ||	\
		defined(EEL_VM_CHECKING)
# include <stdio.h>
#endif
#include "EEL_object.h"
#include "e_module.h"
#include "e_error.h"

EEL_DLSTACK(eel_limbo_, EEL_object, lnext, lprev)

#if DBGM(1) + 0 == 1
typedef struct
{
	EEL_object	*dnext;
	EEL_object	*dprev;
	DBGM2(char	*dname;)
} EEL_object_dbg;

static inline EEL_object_dbg *o2dbg(EEL_object *o)
{
	return ((EEL_object_dbg *)o) - 1;
}
#endif

#if DBGM2(1)+0 == 1
#  define SETNAME(o, x)	o2dbg(o)->dname = strdup(x)
#  define GETNAME(o)	(o2dbg(o)->dname)
#else
#  define SETNAME(o, x)
#  define GETNAME(o)	NULL
#endif


#ifdef EEL_VM_CHECKING
#define	eel_o__construct eel_o_construct
#else
#define eel_o__construct(vm, type, inits, initc, result)		\
	(o2EEL_classdef(VMP->state->classes[type])->construct)		\
			(vm, type, inits, initc, result)
#endif


/*
 * Weak reference management
 */
typedef struct
{
/*FIXME: Why are these signed? */
	int		nrefs;		/* Number of weak references */
	int		size;		/* Number of refs we have room for */
	EEL_value	*refs[1];	/* Vector of weak references */
} EEL_weakrefs;

#define	EEL_WR_MINSIZE	4


static inline void eel_weakref_attach(EEL_value *v)
{
	EEL_weakrefs *wr = v->objref.v->weakrefs;
	EEL_vm *vm = v->objref.v->vm;
	int nrefs, size;
#ifdef EEL_VM_CHECKING
	switch(v->type)
	{
	  case EEL_TOBJREF:
		eel_vmdump(vm, "INTERNAL ERROR: Tried to attach an objref "
				"value! (Needs weakref.)\n");
		eel_perror(vm, 1);
		return;
	  case EEL_TWEAKREF:
		if(EEL_IN_HEAP(vm, v))
		{
			VMP->weakrefs++;
			DBG7W(fprintf(stderr, "WARNING: %d weakrefs in heap!\n",
					VMP->weakrefs);)
		}
		break;
	  default:
		eel_vmdump(vm, "INTERNAL ERROR: Tried to attach a non-objref "
				"value!\n");
		eel_perror(vm, 1);
		return;
	}
#endif
	if(wr)
	{
		nrefs = wr->nrefs;
		size = wr->size;
	}
	else
		nrefs = size = 0;
	if(size < nrefs + 1)
	{
		int newsize = eel_calcresize(EEL_WR_MINSIZE, size, nrefs + 1);
		int ms = newsize * sizeof(EEL_value *) +
				((char *)&wr->refs[0] - (char *)wr);
		wr = (EEL_weakrefs *)vm->realloc(vm, wr, ms);
		if(!wr)
		{
			eel_own(v->objref.v);
			eel_vmdump(vm, "INTERNAL ERROR: Out of memory while "
					"adding weak reference! Refcount "
					"increased.\n");
			eel_perror(vm, 1);
			return;
		}
		v->objref.v->weakrefs = wr;
		wr->nrefs = nrefs;
		wr->size = newsize;
	}
	DBG7W(fprintf(stderr, "attach(%p): index = %d\n", v, wr->nrefs);)
	wr->refs[wr->nrefs] = v;
	v->objref.index = wr->nrefs;
	++wr->nrefs;
}


static inline void eel__weakref_detach(EEL_value *v)
{
	EEL_vm *vm;
	EEL_weakrefs *wr = v->objref.v->weakrefs;
	DBG7W(fprintf(stderr, "eel__weakref_detach(%p)\n", v);)
#ifdef EEL_VM_CHECKING
	switch(v->type)
	{
	  case EEL_TOBJREF:
		fprintf(stderr, "INTERNAL ERROR: Tried to detach non-weakref "
				"value!\n");
		return;
	  case EEL_TWEAKREF:
		break;
	  default:
		fprintf(stderr, "INTERNAL ERROR: Tried to eel_weak_detach() "
				"a non-objref value!\n");
		return;
	}
#endif
	vm = v->objref.v->vm;
#ifdef EEL_VM_CHECKING
	if(!wr)
	{
		eel_vmdump(vm, "INTERNAL ERROR: Tried to detach weakref, but "
				"object has no weakref vector!\n");
		eel_perror(vm, 1);
		return;
	}
	if(EEL_IN_HEAP(vm, v))
		VMP->weakrefs--;
#endif
	if(wr->nrefs > v->objref.index)
	{
		// Replace this ref with the last in the vector
		wr->refs[v->objref.index] = wr->refs[wr->nrefs - 1];
		// Update the index field of the moved weakref
		wr->refs[v->objref.index]->objref.index = v->objref.index;
	}
	--wr->nrefs;

	// Shrink the vector if it's more than twice as large as it needs to be
	if((wr->nrefs < wr->size / 2) && (wr->size > EEL_WR_MINSIZE))
	{
		int newsize = wr->size /= 2;
		int ms = newsize * sizeof(EEL_value *) +
				((char *)&wr->refs[0] - (char *)wr);
		wr = (EEL_weakrefs *)vm->realloc(vm, wr, ms);
		if(wr)
		{
			wr->size = newsize;
			v->objref.v->weakrefs = wr;
		}
	}
	else if(!wr->nrefs)
	{
		vm->free(vm, wr);
		v->objref.v->weakrefs = NULL;
	}

	// Clear the ref
	v->type = EEL_TNIL;
}


static inline void eel_weakref_relocate(EEL_value *to)
{
	EEL_weakrefs *wr;
#ifdef EEL_VM_CHECKING
	EEL_vm *vm;
	if(to->type != EEL_TWEAKREF)
	{
		fprintf(stderr, "INTERNAL ERROR: Tried to relocate non-weakref "
				"value!\n");
		return;
	}
#endif
	wr = to->objref.v->weakrefs;
	DBG7W(fprintf(stderr, "Relocated weakref %p to %p!\n",
			wr->refs[to->objref.index], to);)
#ifdef EEL_VM_CHECKING
	vm = to->objref.v->vm;
	if(!wr)
	{
		eel_vmdump(vm, "INTERNAL ERROR: Tried to relocate weakref, but "
				"object has no weakref vector!\n");
		eel_perror(vm, 1);
		return;
	}
	if(to->objref.index == EEL_WEAKREF_UNWIRED)
	{
		eel_vmdump(vm, "INTERNAL ERROR: Tried to relocate an unwired "
				"weakref!\n");
		eel_perror(vm, 1);
		return;
	}
#endif
	wr->refs[to->objref.index] = to;
}


/*
 * Set any weakrefs attached to 'o' to nil, to tell weakref owners that 'o' is
 * off to the Happy Hunting Grounds.
 */
static inline void eel_kill_weakrefs(EEL_object *o)
{
	int i;
	EEL_weakrefs *wr = o->weakrefs;
	if(!wr)
		return;
	DBG7W(fprintf(stderr, "  Clearing %d weakrefs...\n", wr->nrefs);)
#ifdef EEL_VM_CHECKING
	if(wr->nrefs < 0)
	{
		eel_vmdump(o->vm, "INTERNAL ERROR: Weakref with negativ nrefs!\n");
		eel_perror(o->vm, 1);
	}
#endif
	for(i = 0; i < wr->nrefs; i++)
		wr->refs[i]->type = EEL_TNIL;
	o->vm->free(o->vm, wr);
	o->weakrefs = NULL;
	DBG7W(fprintf(stderr, "    Done\n");)
}


/*
 ************************************************************
 *     DO NOT USE THE DOUBLE UNDERSCORE CALLS DIRECTLY!
 *                   Use eel_o_disown().
 ************************************************************
 */

/*
 * Free struct of object 'o'. Raw, brutal version.
 * No destructors or anything involved.
 */
void eel_o__dealloc(EEL_object *o);


/*
 * Destroy 'object' instantly. Note that any objects owned
 * by 'object' may be thrown in the garbage list to be
 * destroyed later, rather than being destoyed instantly.
 */
EEL_xno eel_o__destruct(EEL_object *object);


/*
 * Get rid of an object - instantly or via the garbage list
 */
static inline void eel_o__dispose(EEL_object *object)
{
	eel_limbo_unlink(object);
#ifdef EEL_HYPER_AGGRESSIVE_MM
	eel_o__destruct(object);
#else /* EEL_HYPER_AGGRESSIVE_MM */
#  ifdef EEL_CACHE_STRINGS
	/* The string class has it's own "garbage bin"... */
	if(EEL_CSTRING == object->type)
		eel_o__destruct(object);
	else
#  else /* EEL_CACHE_STRINGS */
		eel_push(&object->vm->garbage, object);
#  endif /* EEL_CACHE_STRINGS */
#endif /* EEL_HYPER_AGGRESSIVE_MM */
}


/*
 * Add reference ownership to object.
 */
static inline void eel_o_own(EEL_object *o)
{
#if DBGM(1) + DBGK(1) + DBGK3(1) + 0 > 0
	EEL_vm *vm = o->vm;
#endif
	DBGK3(const char *s;)
	DBGM(if(o->refcount <= 0)
		++VMP->resurrected;)
	++o->refcount;
	DBGM(++VMP->owns);
	DBGK3(
		s = eel_o_stringrep(o);
		printf("++%s.refcount (result: %d)\n", s, o->refcount);
		eel_sfree(VMP->state, s);
	)
}


/*
 * Safe, nice, pointer clearing version - the one you should use.
 *
 * This clears the pointer BEFORE destroying the object, to ensure that side
 * effects of the destructor don't run into the pointer after it has become
 * invalid.
 */
static inline void eel_o_disown(EEL_object **po)
{
	DBGM(EEL_vm *vm = (*po)->vm;)
	EEL_object *o = *po;
#if DBGK(1) + DBGK3(1) + 0 > 0
	EEL_state *es = eel_vm2p(o->vm)->state;
	const char *s;
#endif
	DBGK(if(o->refcount <= 0)
	{
		s = eel_o_stringrep(o);
		eel_vmdump(o->vm, "eel_o_disown(): Tried to disown zombified "
				"object %s!\n", s);
		eel_perror(o->vm, 1);
		eel_sfree(es, s);
		DBGZ2(abort();)
		return;
	})
	--o->refcount;
	DBGM(++VMP->disowns);
	DBGK3(
		s = eel_o_stringrep(o);
		printf("--%s.refcount (result: %d)\n", s, o->refcount);
		eel_sfree(es, s);
	)
	if(!o->refcount)
	{
		*po = NULL;
		eel_o__dispose(o);
 	}
}


/*
 * "Ignore the pointer" version. Use only when you're about to destroy or
 * overwrite the pointer container anyway.
 */
static inline void eel_o_disown_nz(EEL_object *o)
{
	DBGM(EEL_vm *vm = o->vm;)
#if DBGK(1) + DBGK3(1) + 0 > 0
	EEL_state *es = eel_vm2p(o->vm)->state;
	const char *s;
#endif
	DBGK(if(o->refcount <= 0)
	{
		s = eel_o_stringrep(o);
		eel_vmdump(o->vm, "eel_o_disown_nz(): Tried to disown destroyed "
				"object %s!\n", s);
		eel_perror(o->vm, 1);
		eel_sfree(es, s);
		DBGZ2(abort();)
		return;
	})
	--o->refcount;
	DBGM(++VMP->disowns);
	DBGK3(
		s = eel_o_stringrep(o);
		printf("--%s.refcount (result: %d)\n", s, o->refcount);
		eel_sfree(es, s);
	)
	if(!o->refcount)
		eel_o__dispose(o);
}


/*
 * If 'value' is a (strong) reference to an object, decrement that objects
 * refcount. If it reaches zero, have the object collected. A weak reference
 * is detached from its target.
 *
 *	WARNING: This may leave behind an invalid value!
 */
static inline void eel_v_disown_nz(EEL_value *value)
{
	if(value->type == EEL_TOBJREF)
		eel_o_disown_nz(value->objref.v);
	else if(value->type == EEL_TWEAKREF)
		eel__weakref_detach(value);
#ifdef EEL_VM_CHECKING
	else if(value->type == EEL_TILLEGAL)
	{
		fprintf(stderr, "INTERNAL ERROR: eel_v_disown_nz(): ILLEGAL value!\n");
		DBGZ2(abort();)
	}
#endif
}


/* returns a non-zero value if 'o' is in limbo. */
static inline int eel_in_limbo(EEL_object *object)
{
	return (object->lprev != NULL);
}


/*
 * Add object 'o' to the current function's limbo list.
 * This will ensure that newly created objects are not
 * leaked if they're left around unused. (The VM/compiler
 * doesn't do refcounting for low level "shuffling" code.)
 */
static inline void eel_o_limbo(EEL_object *o)
{
	EEL_callframe *cf = (EEL_callframe *)
			(o->vm->heap + o->vm->base - EEL_CFREGS);
#ifdef EEL_VM_CHECKING
	if(eel_in_limbo(o))
	{
		eel_vmdump(o->vm, "INTERNAL ERROR: Tried to add object %s "
				"to limbo list more than once!\n",
				eel_o_stringrep(o));
		eel_perror(o->vm, 1);
		DBGZ2(abort();)
		return;
	}
#endif
	DBG7(printf("Added %s to limbo.\n", eel_o_stringrep(o), o);)
	eel_limbo_push(&cf->limbo, o);
}

/*
 * If 'v' is a reference to an object, add that object to the limbo list of the
 * current function.
 */
static inline void eel_v_limbo(EEL_value *v)
{
	if(EEL_IS_OBJREF(v->type))
	{
		EEL_vm *vm = v->objref.v->vm;
		EEL_callframe *cf = (EEL_callframe *)
				(vm->heap + vm->base - EEL_CFREGS);
#ifdef EEL_VM_CHECKING
		if(eel_in_limbo(v->objref.v))
		{
			eel_vmdump(vm, "INTERNAL ERROR: Tried to add object"
					" to limbo list more than"
					" once!\n");
			eel_perror(vm, 1);
			DBGZ2(abort();)
			return;
		}
#endif
		eel_limbo_push(&cf->limbo, v->objref.v);
	}
#ifdef EEL_VM_CHECKING
	if(v->type == EEL_TILLEGAL)
	{
		fprintf(stderr, "INTERNAL ERROR: eel_v_limbo(): ILLEGAL value!\n");
		DBGZ2(abort();)
	}
#endif
}


/*
 * Receive a result from an operator, cast method or function. Use this whenever
 * you don't know whether the return is in limbo, newly created or something
 * else.
 *
 * Any returned object should have had it's refcount increased, so what we do
 * here is deal with that accordingly;
 *
 *	* If the object is in limbo, we just restore
 *	  the refcount by subtracting one.
 *
 *	* If the object is not in limbo, add it! We
 *	  keep the refcount as the ownership for the
 *	  limbo list.
 */
static inline void eel_v_receive(EEL_value *v)
{
	if(EEL_IS_OBJREF(v->type))
	{
		EEL_object *o = v->objref.v;
#if DBGK4(1) + DBGM(1) + 0 >= 1
		EEL_vm *vm = o->vm;
#endif
#if DBGK4(1) + 0 == 1
		EEL_state *es = VMP->state;
		const char *s = eel_o_stringrep(o);
#endif
		if(eel_in_limbo(o))
		{
#ifdef EEL_VM_CHECKING
			if(!o->refcount)
			{
				eel_vmdump(o->vm, "INTERNAL ERROR: Received"
						" an object with zero"
						" refcount!\n");
				eel_perror(o->vm, 1);
				DBGZ2(abort();)
				return;
			}
#endif
			--o->refcount;
			DBGM(++VMP->disowns);
			DBGK4(printf("Received limbo object; "
					"--%s.refcount (result: %d)\n",
					s, o->refcount);)
		}
		else
		{
			eel_o_limbo(o);
			DBGK4(printf("Received non-limbo object "
					" %s (refcount: %d)\n",
					s, o->refcount);)
		}
		DBGK4(eel_sfree(es, s);)
	}
#ifdef EEL_VM_CHECKING
	if(v->type == EEL_TILLEGAL)
	{
		fprintf(stderr, "INTERNAL ERROR: eel_v_receive(): ILLEGAL value!\n");
		DBGZ2(abort();)
	}
#endif
}


/*
 * Like receive, but for grabbing objects directly,
 * rather than having them passed to us via some
 * method that inc's the refcount for us. Used by the
 * VM for grabbing upvalues, static variables etc.
 *
 * NOTE:
 *	Using this on a weakref still increases the refcount
 *	if the object is added to the limbo list!
 */
static inline void eel_v_grab(EEL_value *v)
{
	if(EEL_IS_OBJREF(v->type))
		if(!eel_in_limbo(v->objref.v))
		{
			eel_o_own(v->objref.v);
			eel_o_limbo(v->objref.v);
		}
#ifdef EEL_VM_CHECKING
	if(v->type == EEL_TILLEGAL)
	{
		fprintf(stderr, "INTERNAL ERROR: eel_v_grab(): ILLEGAL value!\n");
		DBGZ2(abort();)
	}
#endif
}


/*
 * Make 'value' identical to 'from'.
 *
 *	The previous contents of 'value' is brutally ignored!
 *
 *	Unwired weakrefs become real weakrefs.
 *
 *	Wired weakrefs become new weakrefs.
 *
 *	Objrefs inc the refcount of the target objects.
 */
static inline void eel_v_clone(EEL_value *value, const EEL_value *from)
{
#ifdef EEL_CLEAN_COPY
	value->type = from->type;
	switch(from->type)
	{
	  case EEL_TNIL:
		return;
	  case EEL_TREAL:
		value->real.v = from->real.v;
		return;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		value->integer.v = from->integer.v;
		return;
	  case EEL_TOBJREF:
		value->objref.v = from->objref.v;
		eel_o_own(value->objref.v);
		return;
	  case EEL_TWEAKREF:
		DBG7W(fprintf(stderr, "eel_v_clone(): WEAKREF %p -> %p!\n",
				from, value);)
		value->objref.v = from->objref.v;
		DBG7W(fprintf(stderr, "      ptr = %p\n", value->objref.v);)
		DBG7W(fprintf(stderr, " refcount = %d\n",
				value->objref.v->refcount);)
		eel_weakref_attach(value);
		DBG7W(fprintf(stderr, "eel_v_clone(): Done!\n");)
		return;
	}
	fprintf(stderr, "INTERNAL ERROR: Tried to clone "
			"a value of illegal type %d!\n",
			value->type);
	DBGZ2(abort();)
#else
	*value = *from;
	if(value->type == EEL_TOBJREF)
		eel_o_own(value->objref.v);
	else if(value->type == EEL_TWEAKREF)
		eel_weakref_attach(value);
#endif
}


/*
 * Like eel_v_clone(), but turns weakrefs into objrefs, incrementing the
 * refcount of the target.
 */
static inline void eel_v_copy(EEL_value *value, const EEL_value *from)
{
#ifdef EEL_CLEAN_COPY
	value->type = from->type;
	switch(from->type)
	{
	  case EEL_TNIL:
		return;
	  case EEL_TREAL:
		value->real.v = from->real.v;
		return;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		value->integer.v = from->integer.v;
		return;
	  case EEL_TOBJREF:
		value->objref.v = from->objref.v;
		eel_o_own(value->objref.v);
		return;
	  case EEL_TWEAKREF:
		DBG7W(fprintf(stderr, "eel_v_copy(): WEAKREF %p -> %p!\n",
				from, value);)
		value->objref.v = from->objref.v;
		DBG7W(fprintf(stderr, "      ptr = %p\n", value->objref.v);)
		DBG7W(fprintf(stderr, " refcount = %d\n",
				value->objref.v->refcount);)
		if(from->objref.index == EEL_WEAKREF_UNWIRED)
		{
			DBG7W(fprintf(stderr, "     UNWIRED! Attached!\n");)
			eel_weakref_attach(value);
			DBG7W(fprintf(stderr, "    index = %d\n",
					value->objref.index);)
		}
		else
		{
			DBG7W(fprintf(stderr, "     Converted to OBJREF!\n");)
			value->type = EEL_TOBJREF;
			eel_o_own(value->objref.v);
		}
		DBG7W(fprintf(stderr, "eel_v_copy(): Done!\n");)
		return;
	}
	fprintf(stderr, "INTERNAL ERROR: Tried to copy "
			"a value of illegal type %d!\n",
			value->type);
	DBGZ2(abort();)
#else
	*value = *from;
	if(value->type == EEL_TOBJREF)
		eel_o_own(value->objref.v);
	else if(value->type == EEL_TWEAKREF)
	{
		if(from->objref.index == EEL_WEAKREF_UNWIRED)
			eel_weakref_attach(value);
		else
		{
			value->type = EEL_TOBJREF;
			eel_o_own(value->objref.v);
		}
	}
#endif
}


/*
 * Move value 'from' to the new location 'value'.
 *
 *	The original value should be considered INVALID after this call!
 */
static inline void eel_v_move(EEL_value *value, const EEL_value *from)
{
#ifdef EEL_CLEAN_COPY
	value->type = from->type;
	switch(from->type)
	{
	  case EEL_TNIL:
		return;
	  case EEL_TREAL:
		value->real.v = from->real.v;
		return;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		value->integer.v = from->integer.v;
		return;
	  case EEL_TOBJREF:
		value->objref.v = from->objref.v;
		return;
	  case EEL_TWEAKREF:
		DBG7W(fprintf(stderr, "eel_v_move(): WEAKREF!\n");)
		value->objref.v = from->objref.v;
		value->objref.index = from->objref.index;
		eel_weakref_relocate(value);
		return;
	}
	fprintf(stderr, "INTERNAL ERROR: Tried to move "
			"a value of illegal type %d!\n",
			value->type);
	DBGZ2(abort();)
#else
	*value = *from;
	if(value->type == EEL_TWEAKREF)
		eel_weakref_relocate(value);
#endif
}


/*
 * Make a temporary quick copy of value 'from'.
 *
 *	The original value is still valid after this call, whereas the new
 *	value does not get ownership! Weakrefs are turned into "weak" objrefs.
 */
static inline void eel_v_qcopy(EEL_value *value, const EEL_value *from)
{
#ifdef EEL_CLEAN_COPY
	value->type = from->type;
	switch(from->type)
	{
	  case EEL_TNIL:
		return;
	  case EEL_TREAL:
		value->real.v = from->real.v;
		return;
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		value->integer.v = from->integer.v;
		return;
	  case EEL_TOBJREF:
		value->objref.v = from->objref.v;
		return;
	  case EEL_TWEAKREF:
		DBG7W(fprintf(stderr, "eel_v_qcopy(): WEAKREF!\n");)
		value->objref.v = from->objref.v;
		value->type = EEL_TOBJREF;
		return;
	}
	fprintf(stderr, "INTERNAL ERROR: Tried to qcopy "
			"a value of illegal type %d!\n",
			value->type);
	DBGZ2(abort();)
#else
	*value = *from;
	if(value->type == EEL_TWEAKREF)
		value->type = EEL_TOBJREF;
#endif
}


/*
 * Extract and check range operands for the DELETE metamethod.
 * If the specified range is outside [0..(length-1)], an appropriate
 * exception code is returned.
 *
 * In:	i0 -> place for start index
 *	i1 -> place for end index
 *	op1 -> start index operand, or NULL for "all"
 *	op2 -> item count, or NULL for "single" or "all"
 *	length = number of items
 *
 * Note:
 *	When trying to "delete all" on an object with no items,
 *	eel_get_delete_range() sets i0 to 0 and i1 to -1. Make
 *	sure this is handled correctly!
 *
 * SUCCESS: i0 and i1 are filled in with the first and last indices
 *          to delete, and the function returns 0.
 *
 * FAILURE: Returns an exception code. The contents of *i0 and *i1
 *          are undefined!
 */
static inline EEL_xno eel_get_delete_range(int *i0, int *i1,
		EEL_value *op1, EEL_value *op2, int length)
{
	if(op1)
	{
		/* Get start index */
		switch(op1->type)
		{
		  case EEL_TBOOLEAN:
		  case EEL_TINTEGER:
		  case EEL_TTYPEID:
			*i0 = op1->integer.v;
			break;
		  case EEL_TREAL:
			*i0 = floor(op1->real.v);
			break;
		  default:
			return EEL_XWRONGTYPE;
		}
		if(op2)
			/* Get item count */
			switch(op2->type)
			{
			  case EEL_TBOOLEAN:
			  case EEL_TINTEGER:
			  case EEL_TTYPEID:
				*i1 = *i0 + op2->integer.v - 1;
				break;
			  case EEL_TREAL:
				*i1 = *i0 + floor(op2->real.v) - 1;
				break;
			  default:
				return EEL_XWRONGTYPE;
			}
		else
			/* Delete one item only */
			*i1 = *i0;
	}
	else
	{
		/* Delete all! */
		*i0 = 0;
		*i1 = length - 1;
		return 0;
	}
	if(*i0 < 0)
		return EEL_XLOWINDEX;
	if(*i1 < *i0)
		return EEL_XWRONGINDEX;
	if(*i1 > length)
		return EEL_XHIGHINDEX;
	return 0;
}

#endif /* EEL_E_OBJECT_H */
