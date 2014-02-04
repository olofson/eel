/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_table.c - EEL Table Class implementation
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009-2012 David Olofson
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

#include <math.h>
#include <string.h>
#include "e_object.h"
#include "e_table.h"
#include "e_string.h"
#include "e_vm.h"
#include "e_operate.h"
#include "e_register.h"


struct EEL_tableitem
{
	EEL_hash	hash;
	EEL_value	key;
	EEL_value	value;
};


static inline int t_setsize(EEL_object *eo, int newlength)
{
	EEL_table *t = o2EEL_table(eo);
	EEL_tableitem *ni;
	int n = eel_calcresize(EEL_TABLE_SIZEBASE, t->asize, newlength);
	if(n == t->asize)
	{
		t->length = newlength;
		return 0;
	}
	ni = eel_realloc(eo->vm, t->items, sizeof(EEL_tableitem) * n);
	if(!ni)
	{
		if(newlength)
			return -1;	/* OOM!!! --> */
		t->length = t->asize = 0;
		t->items = NULL;
		return 0;
	}
	if(ni != t->items)
	{
		/* Block moved! Relocate any weakrefs. */
		int i;
		int min = t->length < newlength ? t->length : newlength;
		for(i = 0; i < min; ++i)
		{
			if(ni[i].key.type == EEL_TWEAKREF)
				eel_weakref_relocate(&ni[i].key);
			if(ni[i].value.type == EEL_TWEAKREF)
				eel_weakref_relocate(&ni[i].value);
		}
		t->items = ni;
	}
	t->asize = n;
	t->length = newlength;
	return 0;
}


/*
 * WARNING:
 *	This REQUIRES that 'pos' is inside or just after the end of the array!
 */
static inline EEL_tableitem *insert_item(EEL_object *eo, int pos,
		EEL_value *key, EEL_value *value, EEL_hash h)
{
	int i;
	EEL_tableitem *ti;
	EEL_table *t = o2EEL_table(eo);
#ifdef EEL_VM_CHECKING
	if(pos > t->length)
	{
		fprintf(stderr, "INTERNAL ERROR: e_table.c, insert_item(): "
				"Insert position %d, but there are only %d "
				"items!\n", pos, t->length);
		return NULL;
	}
#endif

	/* Resize */
	if(t_setsize(eo, t->length + 1) < 0)
		return NULL;
	ti = t->items;

	/* Move */
	for(i = t->length - 1; i > pos; --i)
	{
		ti[i].hash = ti[i - 1].hash;
		eel_v_move(&ti[i].key, &ti[i - 1].key);
		eel_v_move(&ti[i].value, &ti[i - 1].value);
	}

	/* Write */
	ti[pos].hash = h;
	eel_v_copy(&ti[pos].key, key);
	eel_v_copy(&ti[pos].value, value);
#if 0
	printf("....................\n");
	for(i = 0; i < t->length; ++i)
		printf("   %d:\t%x\t%s\t%s\n", i, ti[i].hash,
				eel_v_stringrep(eo->vm, &ti[i].key),
				eel_v_stringrep(eo->vm, &ti[i].value));
	printf("''''''''''''''''''''\n");
#endif
#ifdef EEL_VM_CHECKING
	for(i = 0; i < t->length - 1; ++i)
		if(ti[i].hash > ti[i + 1].hash)
			fprintf(stderr, "INTERNAL ERROR: Items %d and %d of %s "
					"are stored in the wrong order!\n",
					i, i + 1, eel_o_stringrep(eo));
#endif
	return ti + pos;
}


/*
 * Find an item with hash code 'key' in array 'list' of 'size' elements.
 *
 * Returns the index of the hit, or if there is no hit, an inverted (that is,
 * *not* two's complement!) index indicating where a new item with hash code
 * 'key' should go if inserted.
 *
 * NOTE:
 *	Hash codes are not guaranteed to make unique keys! As a result, all we
 *	know when getting a "positive" from this is that we found one of
 *	potentially several items with the right hash code. If there are
 *	several, they will all be grouped together, but we may land anywhere in
 *	the group, so we have to look in both directions.
 */
static inline int t__binary_find(EEL_tableitem *list, int size, EEL_hash h)
{
	int low = 0;
	int high = size - 1;
	while(high >= low)
	{
		int mid = (low + high + 1) / 2;
		if(h < list[mid].hash)
			high = mid - 1;
		else if(h > list[mid].hash)
			low = mid + 1;
		else
			return mid;
	}
	return ~((low + high + 1) / 2);
}


/*
 * Find item by 'key' in table 't', starting at item 'first'.
 *
 * Returns the index of the hit, or if there is no hit, an inverted (that is,
 * *not* two's complement!) index indicating where a new item with hash code
 * 'key' should go if inserted.
 */
static inline int t__find(EEL_object *eo, EEL_value *key, EEL_hash h)
{
	int i, first;
	EEL_table *t = o2EEL_table(eo);
	EEL_tableitem *ti = t->items;
	if(t->length < 10)
	{
		for(first = 0; ; ++first)
		{
			if(first == t->length)
				return ~first;
			if(ti[first].hash < h)
				continue;
			if(ti[first].hash > h)
				return ~first;
			break;		/* Found! */
		}
	}
	else
	{
		/* Find group of items with the right hash, if any. */
		first = t__binary_find(ti, t->length, h);
		if(first < 0)
			return first;

		/* Back up to the first item with this hash code. */
		while((first > 0) && (ti[first - 1].hash == h))
			--first;
	}
	
	/* Fast-path for string lookups */
	if(EEL_IS_OBJREF(key->type) && ((EEL_classes)key->objref.v->type == EEL_CSTRING))
	{
		for(i = first; (i < t->length) && (ti[i].hash == h); ++i)
			/* NOTE: Nasty backwards testing here...! */
			if(ti[i].key.objref.v == key->objref.v)
				if(EEL_IS_OBJREF(ti[i].key.type))
					return i;	/* Found! */
		return ~first;	/* Not found! */
	}

	/* Generic linear scan - NO shortcuts for strings! */
	for(i = first; (i < t->length) && (ti[i].hash == h); ++i)
		switch(key->type)
		{
		  case EEL_TNIL:
			if(ti[i].key.type == EEL_TNIL)
				return i;
			continue;
		  case EEL_TREAL:
			if(ti[i].key.type != EEL_TREAL)
				continue;
			if(ti[i].key.real.v == key->real.v)
				return i;
			continue;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			if(ti[i].key.type != key->type)
				continue;
			if(ti[i].key.integer.v == key->integer.v)
				return i;
			continue;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
		  {
			EEL_value v;
			if(!EEL_IS_OBJREF(ti[i].key.type))
				continue;
			if(ti[i].key.objref.v == key->objref.v)
				return i;	/* Same instance! ==> */
			if(eel_o__metamethod(ti[i].key.objref.v,
					EEL_MM_COMPARE, key, &v))
				continue;	/* Cannot even compare! */
			if(v.integer.v)
				continue;	/* Not equal! */
			return i;
		  }
#ifdef EEL_VM_CHECKING
		  default:
			fprintf(stderr, "e_table.c: INTERNAL ERROR: Illegal "
					"key type for t__find()!\n");
			return -1;
#endif
		}
	return ~first;	/* Not found! */
}


static inline EEL_xno t__setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int pos;
	EEL_table *t = o2EEL_table(eo);
	EEL_hash h = eel_v2hash(op1);
#ifdef EEL_VM_CHECKING
	switch(op1->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	  default:
		return EEL_XBADTYPE;
	}
#endif
	pos = t__find(eo, op1, h);
	if(pos >= 0)
	{
		/* Replace value */
		eel_v_disown_nz(&t->items[pos].value);
		eel_v_copy(&t->items[pos].value, op2);
		return 0;
	}
	else
	{
		/* Add new item */
		if(!insert_item(eo, ~pos, op1, op2, h))
			return EEL_XMEMORY;
		return 0;
	}
}


static EEL_xno t_destruct(EEL_object *eo)
{
	EEL_table *t = o2EEL_table(eo);
	int i;
	for(i = 0; i < t->length; ++i)
	{
		EEL_tableitem *ti = t->items + i;
		eel_v_disown_nz(&ti->key);
		eel_v_disown_nz(&ti->value);
	}
	eel_free(eo->vm, t->items);
	return 0;
}


static EEL_xno insert_items(EEL_object *eo, EEL_object *from)
{
	int i, c;
	EEL_table *src;
	src = o2EEL_table(from);
	c = src->length;
	for(i = 0; i < c; ++i)
	{
		EEL_tableitem *sti = src->items + i;
		EEL_xno x = t__setindex(eo, &sti->key, &sti->value);
		if(x)
			return x;
	}
	return 0;
}


static EEL_xno t_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	int i;
	EEL_table *t;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_table), type);
	if(!eo)
		return EEL_XMEMORY;
	t = o2EEL_table(eo);
	t->items = NULL;
	t->asize = 0;
	t->length = 0;
	if(!initc)
	{
		eel_o2v(result, eo);
		return 0;
	}
	if(initc & 1)
	{
		eel_o_free(eo);
		return EEL_XNEEDEVEN;
	}
	for(i = 0; i < initc; i += 2)
	{
		EEL_xno x = t__setindex(eo, initv + i, initv + i + 1);
		if(x)
		{
			t_destruct(eo);
			eel_o_free(eo);
			return x;
		}
	}
	eel_o2v(result, eo);
	return 0;
}


static inline EEL_object *t__clone(EEL_object *orig)
{
	int i, len;
	EEL_table *clonet;
	EEL_table *origt = o2EEL_table(orig);
	EEL_object *clone = eel_o_alloc(orig->vm, sizeof(EEL_table), EEL_CTABLE);
	if(!clone)
		return NULL;
	clonet = o2EEL_table(clone);
	len = origt->length;
	clonet->items = (EEL_tableitem *)eel_malloc(orig->vm,
			sizeof(EEL_tableitem) * len);
	if(!clonet->items)
	{
		eel_o_free(clone);
		return NULL;
	}
	for(i = 0; i < len; ++i)
	{
		clonet->items[i].hash = origt->items[i].hash;
		eel_v_clone(&clonet->items[i].key, &origt->items[i].key);
		eel_v_clone(&clonet->items[i].value, &origt->items[i].value);
	}
	clonet->asize = clonet->length = len;
	return clone;
}


static EEL_xno t_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *no = t__clone(src->objref.v);
	if(!no)
		return EEL_XMEMORY;
	eel_o2v(dst, no);
	return 0;
}


static EEL_xno t_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int pos;
	EEL_table *t = o2EEL_table(eo);
	EEL_hash h = eel_v2hash(op1);
#ifdef EEL_VM_CHECKING
	switch(op1->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	  default:
		return EEL_XBADTYPE;
	}
#endif
	pos = t__find(eo, op1, h);
	if(pos >= 0)
	{
		eel_v_copy(op2, &t->items[pos].value);
		return 0;
	}
	else
		return EEL_XWRONGINDEX;		/* Not found. */
}


static EEL_xno t_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return t__setindex(eo, op1, op2);
}


static EEL_xno t_insert(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int pos;
	EEL_hash h = eel_v2hash(op1);
#ifdef EEL_VM_CHECKING
	switch(op1->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	  default:
		return EEL_XBADTYPE;
	}
#endif
	pos = t__find(eo, op1, h);
//printf("t_insert: %d\n", pos);
/*FIXME: Do we really want this overwrite protection? */
	if(pos >= 0)
		return EEL_XWRONGINDEX;

	/* Add new item */
	if(!insert_item(eo, ~pos, op1, op2, h))
		return EEL_XMEMORY;
	return 0;
}


static EEL_xno t_delete(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int pos, i;
	EEL_table *t = o2EEL_table(eo);
	EEL_tableitem *ti = t->items;
	EEL_hash h;
	if(op2)
		return EEL_XWRONGINDEX;	/* Can't do ranges with tables! */
	if(!op1)
	{
		for(i = 0; i < t->length; ++i)
		{
			eel_v_disown_nz(&ti[i].key);
			eel_v_disown_nz(&ti[i].value);
		}
		t_setsize(eo, 0);
		return 0;
	}
#ifdef EEL_VM_CHECKING
	switch(op1->type)
	{
	  case EEL_TNIL:
	  case EEL_TREAL:
	  case EEL_TINTEGER:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	  default:
		return EEL_XBADTYPE;
	}
#endif
	h = eel_v2hash(op1);
	pos = t__find(eo, op1, h);
	if(pos < 0)
		return EEL_XWRONGINDEX;

	/* Clean out item */
	eel_v_disown_nz(&ti[pos].key);
	eel_v_disown_nz(&ti[pos].value);

	/* Shift subsequent items */
	for(i = pos; i < t->length - 1; ++i)
	{
		ti[i].hash = ti[i + 1].hash;
		eel_v_move(&ti[i].key, &ti[i + 1].key);
		eel_v_move(&ti[i].value, &ti[i + 1].value);
	}

	/* Truncate */
	t_setsize(eo, t->length - 1);
	return 0;
}


static EEL_xno t_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	eel_l2v(op2, o2EEL_table(eo)->length);
	return 0;
}


static EEL_xno t_add(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	if((EEL_classes)EEL_TYPE(op1) != EEL_CTABLE)
		return EEL_XWRONGTYPE;
	eo = t__clone(eo);
	if(!eo)
		return EEL_XMEMORY;
	x = insert_items(eo, op1->objref.v);
	if(x)
	{
		eel_o_free(eo);
		return x;
	}
	eel_o2v(op2, eo);
	return 0;
}


static EEL_xno t_ipadd(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_xno x;
	if((EEL_classes)EEL_TYPE(op1) != EEL_CTABLE)
		return EEL_XWRONGTYPE;
	x = insert_items(eo, op1->objref.v);
	if(x)
		return x;
	eel_o_own(eo);
	eel_o2v(op2, eo);
	return 0;
}


void eel_ctable_register(EEL_vm *vm)
{
	EEL_object *c = eel_register_class(vm,
			EEL_CTABLE, "table", EEL_COBJECT,
			t_construct, t_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, t_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, t_setindex);
	eel_set_metamethod(c, EEL_MM_DELETE, t_delete);
	eel_set_metamethod(c, EEL_MM_INSERT, t_insert);
	eel_set_metamethod(c, EEL_MM_LENGTH, t_length);
	eel_set_metamethod(c, EEL_MM_ADD, t_add);
	eel_set_metamethod(c, EEL_MM_IPADD, t_ipadd);
	eel_set_casts(vm, EEL_CTABLE, EEL_CTABLE, t_clone);
}


EEL_tableitem *eel_table_get_item(EEL_object *to, int i)
{
#ifdef EEL_VM_CHECKING
	if((EEL_classes)to->type != EEL_CTABLE)
		eel_ierror(eel_vm2p(to->vm)->state,
				"eel_table_get_item() used on "
				"non-table object %s!\n",
				eel_o_stringrep(to));
#endif
	if(i < o2EEL_table(to)->length)
		return o2EEL_table(to)->items + i;
	else
		return NULL;
}


EEL_xno eel_table_get(EEL_object *to, EEL_value *key, EEL_value *value)
{
	int pos;
	EEL_table *t = o2EEL_table(to);
	EEL_hash h = eel_v2hash(key);
#ifdef EEL_VM_CHECKING
	if((EEL_classes)to->type != EEL_CTABLE)
		eel_ierror(eel_vm2p(to->vm)->state,
				"eel_table_get() used on "
				"non-table object %s!\n",
				eel_o_stringrep(to));
#endif
	pos = t__find(to, key, h);
	if(pos < 0)
		return EEL_XWRONGINDEX;		/* Not found. */
	eel_v_qcopy(value, &t->items[pos].value);
	return 0;
}


EEL_xno eel_table_gets(EEL_object *to, const char *key, EEL_value *value)
{
	EEL_vm *vm = to->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(vm, key));
	if(!keyv.objref.v)
		eel_serror(VMP->state, "Could not create string \"%s\"!", key);
	x = eel_table_get(to, &keyv, value);
	eel_v_disown_nz(&keyv);
	return x;
}


const char *eel_table_getss(EEL_object *to, const char *key)
{
	EEL_value v;
	EEL_xno x = eel_table_gets(to, key, &v);
	if(x)
		return NULL;
	else
		return eel_v2s(&v);
}


EEL_xno eel_table_set(EEL_object *to, EEL_value *key, EEL_value *value)
{
#ifdef EEL_VM_CHECKING
	if((EEL_classes)to->type != EEL_CTABLE)
		eel_ierror(eel_vm2p(to->vm)->state,
				"eel_table_set() used on "
				"non-table object %s!\n",
				eel_o_stringrep(to));
#endif
	return eel_o__metamethod(to, EEL_MM_SETINDEX, key, value);
}


EEL_xno eel_table_sets(EEL_object *to, const char *key, EEL_value *value)
{
	EEL_vm *vm = to->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(to->vm, key));
	if(!keyv.objref.v)
		eel_serror(VMP->state, "Could not create key string \"%s\"!", key);
	x = eel_table_set(to, &keyv, value);
	eel_v_disown_nz(&keyv);
	return x;
}


EEL_xno eel_table_setss(EEL_object *to, const char *key, const char *value)
{
	EEL_vm *vm = to->vm;
	EEL_xno x;
	EEL_value vv;
	eel_o2v(&vv, eel_ps_new(to->vm, value));
	if(!vv.objref.v)
		eel_serror(VMP->state, "Could not create value string \"%s\"!", value);
	x = eel_table_sets(to, key, &vv);
	eel_v_disown_nz(&vv);
	return x;
}


EEL_value *eel_table_get_key(EEL_tableitem *ti)
{
	return &ti->key;
}


EEL_value *eel_table_get_value(EEL_tableitem *ti)
{
	return &ti->value;
}


EEL_xno eel_table_delete(EEL_object *to, EEL_value *key)
{
#ifdef EEL_VM_CHECKING
	if((EEL_classes)to->type != EEL_CTABLE)
		eel_ierror(eel_vm2p(to->vm)->state,
				"eel_table_delete() used on "
				"non-table object %s!\n",
				eel_o_stringrep(to));
#endif
	return eel_o__metamethod(to, EEL_MM_DELETE, key, NULL);
}


EEL_xno eel_table_deletes(EEL_object *to, const char *key)
{
	EEL_vm *vm = to->vm;
	EEL_xno x;
	EEL_value keyv;
	eel_o2v(&keyv, eel_ps_new(to->vm, key));
	if(!keyv.objref.v)
		eel_serror(VMP->state, "Could not create key string \"%s\"!", key);
	x = eel_table_delete(to, &keyv);
	eel_v_disown_nz(&keyv);
	return x;
}


EEL_xno eel_insert_lconstants(EEL_object *to, const EEL_lconstexp *data)
{
	EEL_vm *vm = to->vm;
	while(data->name)
	{
		EEL_xno x;
		EEL_value keyv, v;
		eel_l2v(&v, data->value);
		eel_o2v(&keyv, eel_ps_new(vm, data->name));
		if(!keyv.objref.v)
			eel_serror(VMP->state, "Could not create key "
					"string \"%s\"!", data->name);
		x = eel_table_set(to, &keyv, &v);
		eel_v_disown_nz(&keyv);
		if(x)
			return x;
		++data;
	}
	return 0;
}
