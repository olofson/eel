/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_util.h - EEL engine utilities
---------------------------------------------------------------------------
 * Copyright (C) 2002-2006, 2009, 2011 David Olofson
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

#ifndef EEL_E_UTIL_H
#define EEL_E_UTIL_H

#include <stdlib.h>
#include <string.h>
#include "EEL.h"
#include "e_vm.h"
#include "e_string.h"


/*----------------------------------------------------------
	"Alloc and forget" strings
----------------------------------------------------------*/

/*
 * Be careful about these, as they can be silently
 * reclaimed as new ones are allocated!
 *
 * In order to guarantee the "N call lifetime"
 * of strings returned from API calls, internal
 * EEL code should eel_sfree() strings that are
 * no longer needed, so that those strings can be
 * reused, instead of stealing strings that may
 * be needed by the application.
 */
int eel_sbuffer_open(EEL_state *es);
void eel_sbuffer_close(EEL_state *es);

/*
 * Allocate an "alloc and forget" string.
 */
char *eel_salloc(EEL_state *es);

/*
 * Free an "alloc and forget" string. Use this
 * internally to maximize the lifetime of
 * strings passed to applications. (Those won't
 * be freed explicitly.)
 */
void eel_sfree(EEL_state *es, const char *s);


/*----------------------------------------------------------
	Templates for common data structures
----------------------------------------------------------*/

/*
 * "Template" for doubly linked list with end pointer.
 */
#define	EEL_DLLIST(pf, ptype, pfirst, plast, ctype, cprev, cnext)\
							\
static inline void pf##push(ptype *p, ctype *c)		\
{							\
	if(p->pfirst)					\
	{						\
		c->cnext = p->pfirst;			\
		c->cprev = NULL;			\
		p->pfirst->cprev = c;			\
		p->pfirst = c;				\
	}						\
	else						\
	{						\
		p->pfirst = p->plast = c;		\
		c->cnext = c->cprev = NULL;		\
	}						\
}							\
							\
static inline void pf##link(ptype *p, ctype *c)		\
{							\
	if(p->plast)					\
	{						\
		p->plast->cnext = c;			\
		c->cprev = p->plast;			\
		c->cnext = NULL;			\
		p->plast = c;				\
	}						\
	else						\
	{						\
		p->pfirst = p->plast = c;		\
		c->cnext = c->cprev = NULL;		\
	}						\
}							\
							\
static inline void pf##unlink(ptype *p, ctype *c)	\
{							\
	if(c->cprev)					\
		c->cprev->cnext = c->cnext;		\
	else						\
		p->pfirst = c->cnext;			\
	if(c->cnext)					\
	{						\
		c->cnext->cprev = c->cprev;		\
		c->cnext = NULL;			\
	}						\
	else						\
		p->plast = c->cprev;			\
	c->cprev = NULL;				\
}


/*
 * "Template" for doubly linked LIFO stack.
 *
 * IMPORTANT:
 *	The 'onext' field MUST be the first field of
 *	the 'otype' struct! This is because the header
 *	is actually treated as a node in the list.
 *	This is possible because only the 'onext' field
 *	of the header node is ever used.
 */
#define	EEL_DLSTACK(pf, otype, onext, oprev)		\
/* Push 'o' as the top object of stack 's' */		\
static inline void pf##push(otype **s, otype *o)	\
{							\
	EEL_VMCHECK(if(o->oprev || o->onext)		\
	{						\
		eel_vmdump(o->vm, "Limbo object %s "	\
				"re-added to limbo!\n",	\
				eel_o_stringrep(o));	\
		DBGZ2(abort());				\
		return;					\
	})						\
	o->onext = *s;					\
	if(*s)						\
		(*s)->oprev = o;			\
	o->oprev = (otype *)s;				\
	*s = o;						\
}							\
/* Unlink object 'o' from whatever stack it's in */	\
static inline void pf##unlink(otype *o)			\
{							\
	if(o->oprev)					\
		o->oprev->onext = o->onext;		\
	if(o->onext)					\
	{						\
		o->onext->oprev = o->oprev;		\
		o->onext = NULL;			\
	}						\
	o->oprev = NULL;				\
}


/*
 * "Template" for dynamic array.
 *
 * New elements are initialized by zeroing the memory.
 * Setting the size to 0 frees the array.
 * Shrinking the array limits 'size' as needed to stay valid.
 *
 * Returns a negative value in case of failure.
 */
#define	EEL_DARRAY(pf, it)			\
typedef struct it##_da				\
{						\
	int	size;				\
	int	maxsize;			\
	it	*array;				\
} it##_da;					\
/* Set array size to 'n' elements */				\
static inline int pf##setsize(EEL_vm *vm, it##_da *a, int n)	\
{								\
	it *na;							\
	if(n == a->maxsize)					\
		return 0;					\
	na = vm->realloc(vm, a->array, sizeof(it) * n);		\
	if(!na)							\
		return -1;					\
	if(n > a->maxsize)					\
		memset(&na[a->maxsize], 0,			\
				sizeof(it) * (n - a->maxsize));	\
	a->array = na;						\
	a->maxsize = n;						\
	if(a->size > a->maxsize)				\
		a->size = a->maxsize;				\
	return 0;						\
}


/*----------------------------------------------------------
	Standard C stuff for the EEL memory manager
----------------------------------------------------------*/

static inline char *eel_strdup(EEL_vm *vm, const char *s)
{
	int len = strlen(s);
	char *buf = vm->malloc(vm, len + 1);
	if(!buf)
		return NULL;
	memcpy(buf, s, len + 1);
	return buf;
}

/*
 * Clear memory from and including 'start' through but not
 * including 'end'.
 */
static inline void eel_rangeclear(void *start, void *end)
{
	char *s = (char *)start;
	char *e = (char *)end;
	memset(s, 0, e - s);
}


/*----------------------------------------------------------
	Memory/speed balanced resize calculator
----------------------------------------------------------*/
static inline int eel_calcresize(int base, int current, int requested)
{
	if(requested > current)
	{
		/* Grow */
		int n;
		if(current)
			n = current;
		else
			n = base;
		while(n < requested)
			n = (n * 3 >> 1) + base;
		return n;
	}
	else
	{
		/* Shrink */
		int n = current / 2;
		if(requested > n)
#ifdef EEL_DEFENSIVE_REALLOC
			return current;
#else
			return requested;
#endif
		else if(n < base)
			return base;
		else
			return n;
	}
}


/*----------------------------------------------------------
	Hash codes
----------------------------------------------------------*/

static inline EEL_hash eel_hashmem(const void *dat, unsigned len)
{
	int i;
	const char *str = (const char *)dat;
	EEL_hash hash = 1315423911;
	if(len > 32)
		len = 32;
	for(i = 0; i < len; str++, i++)
		hash ^= ((hash << 5) + (*str) + (hash >> 2));
	return hash;
}


static inline EEL_hash eel_v2hash(EEL_value *v)
{
	switch(v->type)
	{
	  case EEL_TNIL:
		return 1315423911;
	  case EEL_TREAL:
	  {
		unsigned *i = (unsigned *)&v->real;
		return 1315423911 ^ (42422421131 + (i[0] ^ i[1]));
	  }
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
		return (1315423911 << v->type) ^ v->integer.v;
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		if(v->objref.v->type == EEL_CSTRING)
			return o2EEL_string(v->objref.v)->hash;
		else
		{
			unsigned *i = (unsigned *)&v->objref.v;
			EEL_hash hash = 1315423911;
			int j;
			for(j = 0; j < sizeof(v->objref.v) / sizeof(unsigned); ++j)
				hash ^= ((hash << 5) + i[j] + (hash >> 2));
			return hash;
	 	}
	}
	return 0;
}


/*----------------------------------------------------------
	Handy string representation generators
----------------------------------------------------------*/
/*
 * Generate an ASCII representation of 'value'.
 *
 * NOTE: The result string has limited life time!
 */
const char *eel_v_stringrep(EEL_vm *vm, EEL_value *value);

/*
 * As eel_symbol_is(), but for data containers.
 */
const char *eel_data_is(EEL_value *d);

/* Returns a string with the name of type 'type'. */
const char *eel_typename(EEL_vm *vm, EEL_types type);

#endif /*EEL_E_UTIL_H*/
