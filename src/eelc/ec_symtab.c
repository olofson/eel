/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_symtab.c - Symbol Table/Tree
---------------------------------------------------------------------------
 * Copyright (C) 2000-2006, 2009-2011 David Olofson
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
#include <stdlib.h>
#include <string.h>
#include "ec_symtab.h"
#include "e_state.h"
#include "e_portable.h"
#include "e_util.h"
#include "e_function.h"
#include "e_object.h"
#include "e_table.h"
#include "e_string.h"

EEL_DLLIST(eel_s_, EEL_symbol, symbols, lastsym, EEL_symbol, prev, next);


/*------------------------------------------------
	Symbol
------------------------------------------------*/

void eel_s_free(EEL_state *es, EEL_symbol *s)
{
	/* Unlink */
	if(s->parent)
		eel_s_unlink(s->parent, s);

	/* Cleanup */
	while(s->symbols)
		eel_s_free(es, s->symbols);
	switch(s->type)
	{
	  case EEL_SUNDEFINED:
	  case EEL_SKEYWORD:
	  case EEL_SVARIABLE:
	  case EEL_SUPVALUE:
	  case EEL_SBODY:
	  case EEL_SNAMESPACE:
		break;
	  case EEL_SCONSTANT:
		eel_v_disown(&s->v.value);
		break;
	  case EEL_SCLASS:
	  case EEL_SMODULE:
	  case EEL_SFUNCTION:
		if(s->v.object)
			eel_o_disown_nz(s->v.object);
		break;
	  case EEL_SOPERATOR:
		eel_operator_free(s->v.op);
		break;
	}
	if(s->name)
		eel_o_disown_nz(s->name);

	/* Destroy */
	eel_free(es->vm, s);
}


void eel_s_rename(EEL_state *es, EEL_symbol *s, const char *name)
{
	if(s->name)
		eel_o_disown_nz(s->name);
	s->name = eel_ps_new(es->vm, name);
	if(!s->name)
		eel_serror(es, "Could not rename symbol!"
				" (Failed to create new EEL_string.)");
}


EEL_symbol *eel_s_add(EEL_state *es, EEL_symbol *parent,
		const char *name, EEL_symtypes type)
{
	EEL_symbol *sym = (EEL_symbol *)eel_malloc(es->vm, sizeof(EEL_symbol));
	if(!sym)
		eel_serror(es, "Out of memory when adding symbol!");
	memset(sym, 0, sizeof(EEL_symbol));
	sym->parent = parent;
	if(name)
	{
		sym->name = eel_ps_new(es->vm, name);
		if(!name)
			eel_serror(es, "Could not create symbol name!");
	}
	sym->type = type;
	if(parent)
	{
		eel_s_link(parent, sym);
		sym->uvlevel = parent->uvlevel;
	}
	if(EEL_SFUNCTION == type)
		++sym->uvlevel;
	return sym;
}


EEL_symbol *eel_s_find(EEL_state *es, EEL_symbol *table,
		const char *name, EEL_symtypes type)
{
	EEL_symbol *sym = table->symbols;
	EEL_object *no = eel_ps_new(es->vm, name);
	if(!no)
		eel_serror(es, "Could not get name object!");
	while(sym)
	{
		if((sym->type == type) && (sym->name == no))
			break;
		sym = sym->next;
	}
	eel_o_disown_nz(no);
	return sym;
}


void eel_finder_init(EEL_state *es, EEL_finder *f, EEL_symbol *st, int flags)
{
	f->state = es;
	f->symtab = f->start = st;
	f->symbol = NULL;
	f->name = NULL;		/* Anything */
	f->types = -1;		/* All types */
	f->index = 0;
	f->flags = flags;
}


EEL_symbol *eel_finder_go(EEL_finder *f)
{
	EEL_symbol *hit = NULL;
	if(!f->symtab)
		return NULL;
	DBG3(printf("Searching for '%s' in '%s'...\n",
			f->name ? eel_o2s(f->name) : "<unnamed symbols>",
			f->symtab->name ? eel_o2s(f->symtab->name) :
			"<unnamed symbol table>");)
	while(!hit)
	{
		if(f->symbol)
			f->symbol = f->symbol->next;	/* Next */
		else
			f->symbol = f->symtab->symbols; /* First */

		/* Check for upward recursion or completion */
		if(!f->symbol)
		{
			if(f->flags & ESTF_UP)
			{
				/* Ok, done here; try parent table. */
				if(!f->symtab->parent)
				{
					DBG3(printf("  Not found (1).\n");)
					return NULL;	/* Complete! --> */
				}
				DBG3(printf("  Recursing up.\n");)
				f->symtab = f->symtab->parent;
				f->symbol = NULL;
				continue;	/* Up; search from start */
			}
			else if(f->flags & ESTF_DOWN)
			{
				if(f->symtab == f->start)
				{
					DBG3(printf("  Not found (2).\n");)
					return NULL;	/* Complete! --> */
				}
				/* End of table. Go back up and continue. */
				DBG3(printf("  Continuing in parent table.\n");)
				f->symbol = f->symtab;
				f->symtab = f->symbol->parent;
				continue;	/* Up; continue search */
			}
			else
			{
				DBG3(printf("  Not found (4).\n");)
				return NULL;	/* Complete! --> */
			}
		}

		/* Check for match */
		DBGI(printf("  Testing '%s' (%p).\n",
				f->symbol->name ? EEL_O2S(f->symtab->name) :
				f->symbol->name,
				f->symbol);)
		hit = f->symbol;
		if((f->flags & ESTF_NAME) && f->name)
			if(!hit->name || (hit->name != f->name))
				hit = NULL;
		if(hit && (f->flags & ESTF_TYPES))
			if(!((1 << hit->type) & f->types))
				hit = NULL;

		/* Check for downward recursion */
		if(f->symbol->symbols)
			if(((f->flags & ESTF_DOWN) &&
					  (!(f->flags & ESTF_TYPES) ||
					  ((1 << f->symbol->type) & f->types))))
			{
				DBG3(printf("  Recursing down.\n");)
				f->symtab = f->symbol;
				f->symbol = NULL;
			}
	}

	/* Match! */
	DBG2(printf("  Found '%s' in '%s'!\n",
			hit->name ? eel_o2s(hit->name) : "<unnamed symbol>",
			hit->parent->name ? eel_o2s(hit->parent->name) :
			"<unnamed symbol table>");)
	return hit;
}


EEL_symbol *eel__register(EEL_vm *vm, const char *name, EEL_symtypes stype)
{
	EEL_state *es = VMP->state;
	EEL_symbol *s;
	if(!es->root_symtab)
		return NULL;	/* Class bootstrapping */
	s = eel_s_add(es, es->root_symtab, name, stype);
	if(!s)
		eel_ierror(es, "Could not register symbol '%s'!", name);
	return s;
}


static int get_export(EEL_symbol *st, EEL_object *n, EEL_value *v,
		EEL_object *m)
{
	EEL_vm *vm = n->vm;
	EEL_state *es = VMP->state;
	EEL_symbol *s;
	switch(EEL_TYPE(v))
	{
	  case EEL_CFUNCTION:
		s = eel_s_add(es, st, NULL, EEL_SFUNCTION);
		if(!s)
			return -1;
		s->name = n;
		eel_o_own(s->name);
		s->v.object = v->objref.v;
		eel_o_own(s->v.object);
		break;
	  case EEL_CCLASS:
		s = eel_s_add(es, st, NULL, EEL_SCLASS);
		if(!s)
			return -1;
		s->name = n;
		eel_o_own(s->name);
		s->v.object = v->objref.v;
		eel_o_own(s->v.object);
		break;
	  default:
		s = eel_s_add(es, st, NULL, EEL_SCONSTANT);
		if(!s)
			return -1;
		s->name = n;
		eel_o_own(s->name);
		eel_v_copy(&s->v.value, v);
		break;
	}
	DBGX2(printf("Imported \"%s\" from module \"%s\" (\"%s\").\n",
			eel_o2s(n), eel_module_modname(m),
			eel_module_filename(m));)
	return 0;
}


int eel_s_get_exports(EEL_symbol *st, EEL_object *m)
{
	int i, len;
	EEL_module *mm;
	EEL_vm *vm;
	EEL_state *es;
	EEL_object *im;
	if(m->type != EEL_CMODULE)
		return -1;
	vm = m->vm;
	es = VMP->state;
	im = eel_ps_new(m->vm, "__init_module");
	if(!im)
		eel_serror(es, "Could not create \"__init_module\" string!");
	mm = o2EEL_module(m);
	len = eel_length(mm->exports);
	for(i = 0; i < len; ++i)
	{
		EEL_tableitem *ti = eel_table_get_item(mm->exports, i);
		EEL_finder ef;
		EEL_symbol *s;
		EEL_value *k = eel_table_get_key(ti);
		EEL_value *v = eel_table_get_value(ti);
		DBG11(printf("Importing export '%s' (%s)...\n",
				eel_v2s(k), eel_v_stringrep(vm, v));)

		/* Key must be string! */
		if(EEL_TYPE(k) != EEL_CSTRING)
		{
			eel_o_disown_nz(im);
			eel_ierror(es, "eel_s_add_exports() got a table "
					"item with a non-string name!");
		}

		/* Don't import anything named "__module_init"! */
		if(k->objref.v == im)
			continue;

		/* Name must not exist in current scope! */
		eel_finder_init(es, &ef, st, ESTF_NAME | ESTF_TYPES);
		ef.name = k->objref.v;
		ef.types = EEL_SMFUNCTION;
		s = eel_finder_go(&ef);
		if(s)
		{
			eel_o_disown_nz(im);
			eel_cerror(es, "[ST] Export '%s' from module "
					"\"%s\" (\"%s\") "
					" causes a conflict!",
					eel_o2s(s->name),
					eel_module_modname(m),
					eel_module_filename(m));
		}

		if(get_export(st, k->objref.v, v, m) < 0)
		{
			eel_o_disown_nz(im);
			return -1;
		}
	}
	eel_o_disown_nz(im);
	return 0;
}
