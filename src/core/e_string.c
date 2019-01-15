/*
---------------------------------------------------------------------------
	e_string.c - EEL String Class + string pool
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2008, 2010-2012, 2014, 2019 David Olofson
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
TODO: The constant C strings used here and there in EEL should probably be
TODO: turned into EEL strings at init time, to avoid hashing every time.
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

/* Size of pool hash table */
#define	PS_TABLESIZE	256
#define	HASH2BUCKET(vmp, hash)	((vmp)->strings + ((hash) % PS_TABLESIZE))

#ifdef EEL_CACHE_STRINGS
/* List operators for string cache (uses the limbo list pointers) */
EEL_DLLIST(ps_cache_, EEL_vm_private, scache, scache_last, EEL_object, lprev, lnext)
#endif

/*-------------------------------------------------------------------------
	String pool engine
-------------------------------------------------------------------------*/

/* Push 'o' as the top object of bucket 's' */
static inline void ps_push(EEL_object **head, EEL_object *o)
{
	EEL_string *ps = o2EEL_string(o);
	ps->snext = *head;
	if(*head)
		o2EEL_string(*head)->sprev = o;
	ps->sprev = NULL;
	*head = o;
}

/* Unlink object 'o' from whatever bucket it's in */
static inline void ps_unlink(EEL_object **head, EEL_object *o)
{
	EEL_string *ps = o2EEL_string(o);
	if(*head == o)
		*head = ps->snext;
	else if(ps->sprev)
		o2EEL_string(ps->sprev)->snext = ps->snext;
	if(ps->snext)
		o2EEL_string(ps->snext)->sprev = ps->sprev;
	ps->sprev = ps->snext = NULL;
}


static inline EEL_object *ps_find(EEL_vm *vm, const char *s,
		int len, unsigned int *hash)
{
	EEL_object *pso;
	EEL_object **list;
	*hash = eel_hashmem((const char *)s, len);
	list = HASH2BUCKET(VMP, *hash);
	for(pso = *list; pso; pso = o2EEL_string(pso)->snext)
	{
		EEL_string *ps = o2EEL_string(pso);
		if(ps->hash != *hash)
			continue;
		if(ps->length != len)
			continue;
		if(strncmp(ps->buffer, s, len) == 0)
			return pso;	/* Found! */
	}
	return NULL;
}


EEL_object *eel_ps_find(EEL_vm *vm, const char *s)
{
	unsigned int hash;
	return ps_find(vm, s, strlen(s), &hash);
}


static inline void print_cache(EEL_vm *vm)
{
#if defined(EEL_CACHE_STRINGS) && (PSDBG3(1)+0 == 1)
	EEL_object *ps = VMP->scache;
	printf("STRING CACHE: -------------------------------\n");
	while(ps)
	{
		printf("  %s\n", eel_o_stringrep(ps));
		ps = ps->lnext;
	}
	printf("---------------------------------------------\n");
	ps = VMP->scache_last;
	while(ps)
	{
		printf("  %s\n", eel_o_stringrep(ps));
		ps = ps->lprev;
	}
	printf("---------------------------------------------\n");
#endif
}


static inline void ps_resurrect(EEL_object *pso)
{
	EEL_vm *vm = pso->vm;
	PSDBG2(printf("FOUND STRING %s\n", eel_o_stringrep(pso));)
#ifdef EEL_CACHE_STRINGS
	if(!pso->refcount)
	{
		/* Bring it back from the (almost) dead. */
		ps_cache_unlink(VMP, pso);
		--VMP->scache_size;
	}
#endif
	eel_o_own(pso);
	print_cache(vm);
}


EEL_object *eel_ps_nnew_grab(EEL_vm *vm, char *s, int len)
{
	unsigned int hash;
	EEL_string *ps;
	EEL_object *pso = ps_find(vm, s, len, &hash);
	if(pso)
	{
		eel_free(vm, s);
		ps_resurrect(pso);
		return pso;
	}
	/*
	 * Nope, we need to add a new string to the pool.
	 */
#ifdef EEL_VM_CHECKING
	if(s[len])
	{
		eel_free(vm, s);
		fprintf(stderr, "INTERNAL ERROR: Tried to grab a string without "
				"a null terminator!\n");
		return NULL;
	}
#endif
	pso = eel_o_alloc(vm, sizeof(EEL_string), EEL_CSTRING);
	if(!pso)
		return NULL;
	ps = o2EEL_string(pso);
	ps->buffer = s;
	ps->length = len;
	ps->hash = hash;
	ps_push(HASH2BUCKET(VMP, hash), pso);
	PSDBG2(printf("CREATED STRING %s\n", eel_o_stringrep(pso));)
	print_cache(vm);
	return pso;
}


EEL_object *eel_ps_new_grab(EEL_vm *vm, char *s)
{
	return eel_ps_nnew_grab(vm, s, strlen(s));
}


EEL_object *eel_ps_nnew(EEL_vm *vm, const char *s, int len)
{
	unsigned int hash;
	EEL_string *ps;
	EEL_object *pso = ps_find(vm, s, len, &hash);
	if(pso)
	{
		ps_resurrect(pso);
		return pso;
	}
	/* Nope, we need to add a new string to the pool. */
	pso = eel_o_alloc(vm, sizeof(EEL_string), EEL_CSTRING);
	if(!pso)
		return NULL;
	ps = o2EEL_string(pso);
	ps->buffer = eel_malloc(vm, len + 1);
	if(!ps->buffer)
	{
		eel_o_disown_nz(pso);
		return NULL;
	}
	memcpy((void *)ps->buffer, s, len);
	((char *)ps->buffer)[len] = 0;
	ps->length = len;
	ps->hash = hash;
	ps_push(HASH2BUCKET(VMP, hash), pso);
	PSDBG2(printf("CREATED STRING %s\n", eel_o_stringrep(pso));)
	print_cache(vm);
	return pso;
}


EEL_object *eel_ps_new(EEL_vm *vm, const char *s)
{
	return eel_ps_nnew(vm, s, strlen(s));
}


static inline void really_destruct(EEL_vm *vm, EEL_object *eo)
{
	EEL_string *ps = o2EEL_string(eo);
	PSDBG2(printf("DESTROYING STRING %s\n", eel_o_stringrep(eo));)
	if(VMP->strings)
		ps_unlink(HASH2BUCKET(VMP, ps->hash), eo);
	eel_free(vm, (void *)(ps->buffer));
	PSDBG2(printf("   STRING DESTROYED.\n");)
}


#ifdef EEL_CACHE_STRINGS
static int ps_cache(EEL_vm *vm, EEL_object *ps)
{
	if(!VMP->strings)
	{
		PSDBG3(printf("STRING CACHE IS CLOSED! ");)
		really_destruct(vm, ps);
		return 0;
	}

	/* Unlink from any limbo list and push onto the cache LIFO */
	PSDBG3(printf("CASHING STRING %s\n", eel_o_stringrep(ps));)
	eel_limbo_unlink(ps);
	ps_cache_push(VMP, ps);
	print_cache(vm);

	/* Spill or count? */
	if(VMP->scache_size > VMP->scache_max)
	{
		/* Cache too large! Drop oldest string. */
		EEL_object *last = VMP->scache_last;
		PSDBG3(printf("SPILLING CACHED STRING %s\n",
				eel_o_stringrep(last));)
		ps_cache_unlink(VMP, last);
		really_destruct(vm, last);
		eel_o_free(last);
	}
	else
	{
		++VMP->scache_size;
		PSDBG3(printf("CACHE GREW TO %d STRINGS.\n", VMP->scache_size);)
	}
	print_cache(vm);
	return EEL_XREFUSE;	/* Refuse to destruct! */
}
#endif /* EEL_CACHE_STRINGS */


#ifdef EEL_CACHE_STRINGS
static void ps_uncache(EEL_vm *vm)
{
	print_cache(vm);
	PSDBG3(printf("CLOSING STRING CACHE...\n");)
	/*
	 * Anything in the cache would normally have been destroyed,
	 * so all strings should ha 0 refcounts. Just destroy'em all!
	 */
	while(VMP->scache)
	{
		EEL_object *ps = VMP->scache;
		PSDBG3(printf("  \"%s\"\n", o2EEL_string(ps)->buffer);)
		ps_cache_unlink(VMP, ps);
		--VMP->scache_size;
		really_destruct(vm, ps);
		eel_o_free(ps);
	}
	PSDBG3(printf("STRING CACHE CLOSED.\n");)
}
#endif


int eel_ps_open(EEL_vm *vm)
{
	VMP->strings = eel_malloc(vm, sizeof(EEL_pstring *) * PS_TABLESIZE);
	if(!VMP->strings)
		return -1;
	memset(VMP->strings, 0, sizeof(EEL_pstring *) * PS_TABLESIZE);
#ifdef EEL_CACHE_STRINGS
	VMP->scache_max = EEL_DEFAULT_STRING_CACHE;
#endif
	return 0;
}


void eel_ps_close(EEL_vm *vm)
{
	int i;
#ifdef EEL_CACHE_STRINGS
	PSDBG(printf("--- Killing string pool cache--------\n");)
	ps_uncache(vm);
	PSDBG(printf("  OK.\n");)
#endif
	PSDBG(printf("--- Closing string pool -------------\n");)
	for(i = 0; i < PS_TABLESIZE; ++i)
	{
		PSDBG(int pb = 0;)
		while(VMP->strings[i])
		{
			EEL_string *ps = o2EEL_string(VMP->strings[i]);
			PSDBG(if(!pb)
			{
				printf("--- BIN %3.1d -------------------------\n", i);
				pb = 1;
			})
			PSDBG(printf("WARNING: Pooled string \"%s\" leaked!\n",
					ps->buffer);)
			ps_unlink(VMP->strings + (ps->hash % PS_TABLESIZE),
					VMP->strings[i]);
		}
	}
	eel_free(vm, VMP->strings);
	VMP->strings = NULL;
	PSDBG(printf("--- String pool closed --------------\n");)
}


/*-------------------------------------------------------------------------
	String class implementation
-------------------------------------------------------------------------*/

static EEL_xno s_construct(EEL_vm *vm, EEL_classes cid,
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
		eo = eel_ps_new_grab(vm, buf);
		if(!eo)
		{
			eel_free(vm, buf);
			return EEL_XCONSTRUCTOR;
		}
		eel_o2v(result, eo);
		return 0;
	}
	else
	{
		eo = eel_ps_nnew(vm, "", 0);
		if(!eo)
			return EEL_XCONSTRUCTOR;
		eel_o2v(result,  eo);
		return 0;
	}
}


static EEL_xno s_destruct(EEL_object *eo)
{
	EEL_vm *vm = eo->vm;
#ifdef EEL_CACHE_STRINGS
	return ps_cache(vm, eo);
#else
	really_destruct(vm, eo);
	return 0;
#endif
}


static EEL_xno s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_string *s = o2EEL_string(eo);
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
	else if(i >= s->length)
		return EEL_XHIGHINDEX;

	/* Read value */
	op2->classid = EEL_CINTEGER;
	op2->integer.v = s->buffer[i] & 0xff;	/* Treat as unsigned! */
	return 0;
}


static EEL_xno s_in(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_string *s = o2EEL_string(eo);
	switch(EEL_CLASS(op1))
	{
	  case EEL_CBOOLEAN:
	  case EEL_CINTEGER:
	  case EEL_CCLASSID:
		return eel_s_char_in(s->buffer, s->length, op1->integer.v,
				op2);
	  case EEL_CREAL:
		return eel_s_char_in(s->buffer, s->length, floor(op1->real.v),
				op2);
	  case EEL_CSTRING:
	  {
		EEL_string *s2;
		if(op1->objref.v == eo)
		{
			/* Same instance! */
			op2->classid = EEL_CINTEGER;
			op2->integer.v = 0;
			return 0;
		}
		s2 = o2EEL_string(op1->objref.v);
		return eel_s_str_in(s->buffer, s->length,
				s2->buffer, s2->length, op2);
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *s2 = o2EEL_dstring(op1->objref.v);
		return eel_s_str_in(s->buffer, s->length,
				s2->buffer, s2->length, op2);
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno s_copy(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_string *s = o2EEL_string(eo);
	int start = eel_v2l(op1);
	int length = eel_v2l(op2);
	if(start < 0)
		return EEL_XLOWINDEX;
	else if(start > s->length)
		return EEL_XHIGHINDEX;
	if(length < 0)
		return EEL_XWRONGINDEX;
	else if(start + length > s->length)
		return EEL_XHIGHINDEX;
	op2->objref.v = eel_ps_nnew(eo->vm, s->buffer + start, length);
	if(!op2->objref.v)
		return EEL_XCONSTRUCTOR;
	op2->classid = EEL_COBJREF;
	return 0;
}


static EEL_xno s_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	op2->classid = EEL_CINTEGER;
	op2->integer.v = o2EEL_string(eo)->length;
	return 0;
}


static EEL_xno s_compare(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_string *s;
	const char *s2buf;
	int s2len;
	if(!EEL_IS_OBJREF(op1->classid))
		return EEL_XWRONGTYPE;
	op2->classid = EEL_CINTEGER;

	switch(op1->objref.v->classid)
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2;
		if(op1->objref.v == eo)
		{
			/* Same instance! */
			op2->integer.v = 0;
			return 0;
		}
		s2 = o2EEL_string(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *s2 = o2EEL_dstring(op1->objref.v);
		s2buf = s2->buffer;
		s2len = s2->length;
		break;
	  }
	  default:
		return EEL_XNOTIMPLEMENTED;
	}

	s = o2EEL_string(eo);
	if(s->length > s2len)
	{
		op2->integer.v = 1;
		return 0;
	}
	else if(s->length < s2len)
	{
		op2->integer.v = -1;
		return 0;
	}
	op2->integer.v = eel_s_cmp((unsigned char *)s->buffer,
			(unsigned const char *)s2buf, s2len);
	return 0;
}


static EEL_xno s_eq(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	switch(op1->classid)
	{
	  case EEL_CNIL:
	  case EEL_CREAL:
	  case EEL_CINTEGER:
	  case EEL_CBOOLEAN:
	  case EEL_CCLASSID:
		op2->classid = EEL_CBOOLEAN;
		op2->integer.v = 0;
		return 0;
	  case EEL_COBJREF:
	  case EEL_CWEAKREF:
	  {
		EEL_object *o = op1->objref.v;
		if(o->classid == EEL_CSTRING)
		{
			op2->classid = EEL_CBOOLEAN;
			op2->integer.v = (eo == o);
			return 0;
		}
		else if(o->classid == EEL_CDSTRING)
		{
			s_compare(eo, op1, op2);
			op2->classid = EEL_CBOOLEAN;
			op2->integer.v = (op2->integer.v == 0);
			return 0;
		}
		else
		{
			op2->classid = EEL_CBOOLEAN;
			op2->integer.v = 0;
			return 0;
		}
	  }
	  default:
		return EEL_XBADTYPE;
	}
}


static EEL_xno s_cast_to_real(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_string *s = o2EEL_string(src->objref.v);
	eel_d2v(dst, atof(s->buffer));
	return 0;
}


static EEL_xno s_cast_to_integer(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_string *s = o2EEL_string(src->objref.v);
	eel_l2v(dst, atol(s->buffer));
	return 0;
}


static EEL_xno s_cast_to_boolean(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_string *s = o2EEL_string(src->objref.v);
	dst->classid = EEL_CBOOLEAN;
	if(!strncmp("true", s->buffer, 4) ||
			!strncmp("yes", s->buffer, 3) ||
			!strncmp("1", s->buffer, 1) ||
			!strncmp("on", s->buffer, 2))
		dst->integer.v = 1;
	else
		dst->integer.v = 0;
	return 0;
}


static EEL_xno s_cast_to_dstring(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_string *s = o2EEL_string(src->objref.v);
	dst->objref.v = eel_ds_nnew(vm, s->buffer, s->length);
	if(!dst->objref.v)
		return EEL_XMEMORY;
	dst->classid = EEL_COBJREF;
	return 0;
}


static EEL_xno s_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	/* NOP, more or less - strings are immutable! */
	eel_o2v(dst, src->objref.v);
	eel_o_own(dst->objref.v);
	return 0;
}


static EEL_xno s_cast_from_nil(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *no = eel_ps_nnew(vm, "nil", 3);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno s_cast_from_integer(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *no;
	char buf[24];
	int len = snprintf(buf, sizeof(buf) - 1, "%d", src->integer.v);
	if(len >= sizeof(buf))
		return EEL_XOVERFLOW;
	buf[sizeof(buf) - 1] = 0;
	no = eel_ps_nnew(vm, buf, len);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno s_cast_from_real(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *no;
	char buf[64];
	int len = snprintf(buf, sizeof(buf) - 1, EEL_REAL_FMT, src->real.v);
	if(len >= sizeof(buf))
		return EEL_XOVERFLOW;
	buf[sizeof(buf) - 1] = 0;
	no = eel_ps_nnew(vm, buf, len);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno s_cast_from_boolean(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *no;
	if(src->integer.v)
		no = eel_ps_nnew(vm, "true", 4);
	else
		no = eel_ps_nnew(vm, "false", 5);
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno s_cast_from_typeid(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *no = eel_ps_new(vm, eel_typename(vm, src->integer.v));
	if(!no)
		return EEL_XCONSTRUCTOR;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno s_add(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_string *s1 = o2EEL_string(eo);
	const char *s2buf;
	int s2len;
	char *buf;

	/* Operand */
	switch(EEL_CLASS(op1))
	{
	  case EEL_CSTRING:
	  {
		EEL_string *s2;
		if(!s1->length)
		{
			/* (self == "") ==> return op1 */
			eel_o2v(op2, op1->objref.v);
			eel_o_own(op1->objref.v);
			return 0;
		}
		s2 = o2EEL_string(op1->objref.v);
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
		return EEL_XNOTIMPLEMENTED;
	}

	/* (operand == "") ==> return self */
	if(!s2len)
	{
		eel_o2v(op2, eo);
		eel_o_own(eo);
		return 0;
	}

	/* Concatenate and return a new string */
	buf = eel_malloc(eo->vm, s1->length + s2len + 1);
	if(!buf)
		return EEL_XMEMORY;
	memcpy(buf, s1->buffer, s1->length);
	memcpy(buf + s1->length, s2buf, s2len);
	buf[s1->length + s2len] = 0;
	op2->objref.v = eel_ps_nnew_grab(eo->vm, buf, s1->length + s2len);
	if(!op2->objref.v)
	{
		eel_free(eo->vm, buf);
		return EEL_XCONSTRUCTOR;
	}
	op2->classid = EEL_COBJREF;
	return 0;
}


void eel_cstring_register(EEL_vm *vm)
{
	EEL_state *es = VMP->state;
	EEL_object *c;
	if(eel_ps_open(vm) < 0)
		eel_serror(es, "Could not initialize string pool!");
	c = eel_register_class(vm, EEL_CSTRING, "string", EEL_COBJECT,
			s_construct, s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, s_getindex);
	eel_set_metamethod(c, EEL_MM_IN, s_in);
	eel_set_metamethod(c, EEL_MM_COPY, s_copy);
	eel_set_metamethod(c, EEL_MM_LENGTH, s_length);
	eel_set_metamethod(c, EEL_MM_COMPARE, s_compare);
	eel_set_metamethod(c, EEL_MM_EQ, s_eq);
	eel_set_metamethod(c, EEL_MM_ADD, s_add);
	eel_set_casts(vm, EEL_CSTRING, EEL_CSTRING, s_clone);
	eel_set_casts(vm, EEL_CSTRING, EEL_CREAL, s_cast_to_real);
	eel_set_casts(vm, EEL_CSTRING, EEL_CINTEGER, s_cast_to_integer);
	eel_set_casts(vm, EEL_CSTRING, EEL_CBOOLEAN, s_cast_to_boolean);
	eel_set_casts(vm, EEL_CSTRING, EEL_CDSTRING, s_cast_to_dstring);
	eel_set_casts(vm, EEL_CNIL, EEL_CSTRING, s_cast_from_nil);
	eel_set_casts(vm, EEL_CREAL, EEL_CSTRING, s_cast_from_real);
	eel_set_casts(vm, EEL_CINTEGER, EEL_CSTRING, s_cast_from_integer);
	eel_set_casts(vm, EEL_CBOOLEAN, EEL_CSTRING, s_cast_from_boolean);
	eel_set_casts(vm, EEL_CCLASSID, EEL_CSTRING, s_cast_from_typeid);
}
