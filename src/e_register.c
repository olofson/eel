/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_register.c - EEL operator registry
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2005-2006, 2009-2012 David Olofson
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

#include <string.h>
#include "ec_symtab.h"
#include "ec_lexer.h"
#include "e_state.h"
#include "e_register.h"
#include "e_function.h"
#include "e_object.h"
#include "e_class.h"
#include "e_string.h"
#include "e_table.h"

void eel_register_essx(EEL_vm *vm, EEL_types t, EEL_symbol *s)
{
	switch((EEL_classes)t)
	{
	  case EEL_TREAL:	VMP->state->tokentab[ESSX_REAL] = s; break;
	  case EEL_TINTEGER:	VMP->state->tokentab[ESSX_INTEGER] = s; break;
	  case EEL_TBOOLEAN:	VMP->state->tokentab[ESSX_BOOLEAN] = s; break;
	  case EEL_TTYPEID:	VMP->state->tokentab[ESSX_TYPEID] = s; break;
	  case EEL_CSTRING:	VMP->state->tokentab[ESSX_STRING] = s; break;
	  case EEL_CFUNCTION:	VMP->state->tokentab[ESSX_FUNCTION] = s; break;
	  case EEL_CMODULE:	VMP->state->tokentab[ESSX_MODULE] = s; break;
	  case EEL_CARRAY:	VMP->state->tokentab[ESSX_ARRAY] = s; break;
	  case EEL_CTABLE:	VMP->state->tokentab[ESSX_TABLE] = s; break;
	  case EEL_CVECTOR:	VMP->state->tokentab[ESSX_VECTOR] = s; break;
	  case EEL_CVECTOR_D:	VMP->state->tokentab[ESSX_VECTOR_D] = s; break;
	  case EEL_CVECTOR_F:	VMP->state->tokentab[ESSX_VECTOR_F] = s; break;
	  case EEL_CDSTRING:	VMP->state->tokentab[ESSX_DSTRING] = s; break;
	  default:
		break;
	}
}

void eel_register_essxop(EEL_vm *vm, int op, EEL_symbol *s)
{
	switch(op)
	{
	  case EEL_OP_AND:	VMP->state->tokentab[ESSX_AND] = s; break;
	  case EEL_OP_OR:	VMP->state->tokentab[ESSX_OR] = s; break;
	  case EEL_OP_XOR:	VMP->state->tokentab[ESSX_XOR] = s; break;
	  case EEL_OP_IN:	VMP->state->tokentab[ESSX_IN] = s; break;
	  case EEL_OP_TYPEOF:	VMP->state->tokentab[ESSX_TYPEOF] = s; break;
	  case EEL_OP_SIZEOF:	VMP->state->tokentab[ESSX_SIZEOF] = s; break;
	  case EEL_OP_CLONE:	VMP->state->tokentab[ESSX_CLONE] = s; break;
	  case EEL_OP_NOT:	VMP->state->tokentab[ESSX_NOT] = s; break;
	}
}

static inline EEL_object *eel_cid2c(EEL_vm *vm, int cid)
{
	EEL_state *es = VMP->state;
	return es->classes[cid];
}

static inline int eel_c2cid(EEL_object *c)
{
	EEL_classdef *cd;
	if((EEL_classes)c->type != EEL_CCLASS)
		return -1;
	cd = o2EEL_classdef(c);
	return cd->typeid;
}


int eel_register_keyword(EEL_vm *vm, const char *name, int token)
{
	EEL_symbol *s = eel__register(vm, name, EEL_SKEYWORD);
	s->v.token = token;
	if((token >= ESS_FIRST_TK) && (token <= ESS_LAST_TK))
		VMP->state->tokentab[token - ESS_FIRST_TK] = s;
	return 0;
}


int eel_register_binop(EEL_vm *vm, const char *name, int op, int pri, int flags)
{
	EEL_state *es = VMP->state;
	EEL_finder f;
	EEL_symbol *s;
	eel_finder_init(es, &f, es->root_symtab, ESTF_NAME | ESTF_TYPES);
	f.name = eel_ps_new(es->vm, name);
	if(!f.name)
		eel_serror(es, "Couldn't create search string!");
	f.types = EEL_SMOPERATOR;
	s = eel_finder_go(&f);
	eel_o_disown_nz(f.name);
	if(s)
	{
		if(s->v.op->flags & EOPF_BINARY)
			eel_ierror(es, "Binary operator '%s' already"
					" registered!", name);
	}
	else
	{
		s = eel__register(vm, name, EEL_SOPERATOR);
		if(!s)
			return -1;
		s->v.op = eel_operator_new();
		if(!s->v.op)
		{
			eel_s_free(es, s);
			return -1;
		}
	}
	s->v.op->op = op;
	s->v.op->lpri = pri;
	if(flags & EOPF_RIGHT)
		s->v.op->rpri = pri - 1;
	else
		s->v.op->rpri = pri;
	s->v.op->flags |= (flags & 0xff) | EOPF_BINARY;
	eel_register_essxop(vm, op, s);
	return 0;
}


int eel_register_unop(EEL_vm *vm, const char *name, int op, int pri, int flags)
{
	EEL_state *es = VMP->state;
	EEL_finder f;
	EEL_symbol *s;
	eel_finder_init(es, &f, es->root_symtab, ESTF_NAME | ESTF_TYPES);
	f.name = eel_ps_new(es->vm, name);
	if(!f.name)
		eel_serror(es, "Couldn't create search string!");
	f.types = EEL_SMOPERATOR;
	s = eel_finder_go(&f);
	eel_o_disown_nz(f.name);
	if(s)
	{
		if(s->v.op->flags & EOPF_UNARY)
			eel_ierror(es, "Unary operator '%s' already"
					" registered!", name);
	}
	else
	{
		s = eel__register(vm, name, EEL_SOPERATOR);
		if(!s)
			return -1;
		s->v.op = eel_operator_new();
		if(!s->v.op)
		{
			eel_s_free(es, s);
			return -1;
		}
	}
	s->v.op->un_op = op;
	s->v.op->un_pri = pri;
	s->v.op->flags |= ((flags & 0xff) << 8) | EOPF_UNARY;
	eel_register_essxop(vm, op, s);
	return 0;
}


static EEL_object *eel__register_cfunction(EEL_vm *vm,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_state *es = VMP->state;
	EEL_xno x;
	EEL_value fv;
	EEL_function *f;
	eel_info(es, "Registering C function '%s':", name);
	if((results < 0) || (results > 1))
		eel_cerror(es, "Out of range value for 'results'!");
	if((reqargs < 0) || (reqargs > 255))
		eel_cerror(es, "Out of range value for 'reqargs'!");
	if(optargs && tupargs)
		eel_cerror(es, "Cannot have both optional"
					" and tuple arguments!");
	if((tupargs < 0) || (tupargs > 255))
	        eel_cerror(es, "Out of range value for 'tupargs'!");
	if(optargs > 255)
		eel_cerror(es, "Out of range value for 'optargs'!");
	else if(optargs < 0)
		optargs = 255;
	x = eel_o_construct(vm, EEL_CFUNCTION, NULL, 0, &fv);
	if(x)
		eel_serror(es, "Could not create FUNCTION object!");
	f = o2EEL_function(fv.objref.v);
	f->common.name = eel_ps_new(vm, name);
	if(!f->common.name)
	{
		eel_o_disown_nz(fv.objref.v);
		eel_serror(es, "Could not duplicate function name!");
	}
	f->common.flags = EEL_FF_CFUNC;
	f->common.results = results;
	f->common.reqargs = reqargs;
	f->common.optargs = optargs;
	f->common.tupargs = tupargs;
	if(f->common.results)
		f->common.flags |= EEL_FF_RESULTS;
	if(f->common.reqargs || f->common.optargs || f->common.tupargs)
		f->common.flags |= EEL_FF_ARGS;
	f->c.cb = func;
	eel_clear_info(es);
	return fv.objref.v;
}


EEL_object *eel_register_cfunction(EEL_vm *vm,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_symbol *s;
	EEL_object *fo = eel__register_cfunction(vm, results, name, reqargs,
			optargs, tupargs, func);
	if(!fo)
		return NULL;
	s = eel__register(vm, name, EEL_SFUNCTION);
	s->uvlevel = 0;		/* C functions don't use upvalues! */
	s->v.object = fo;	/* Ownership is taken over by the symbol! */
	return fo;
}


/*----------------------------------------------------------
	Metamethod
----------------------------------------------------------*/

static void eel__add_class_name(EEL_state *es, EEL_classes type,
		const char *name, int notype)
{
	EEL_vm *vm = es->vm;
	EEL_classdef *cd;
	EEL_symbol *s;
	/* Add name only if EEL_CSTRING is registered! */
	if((es->nclasses <= EEL_CSTRING) || !es->classes[EEL_CSTRING])
		return;
	cd = o2EEL_classdef(es->classes[type]);
	DBGO(printf("-----------: Adding name for class '%s'.\n", name);)
	cd->name = eel_ps_new(es->vm, name);
	if(!cd->name)
		eel_serror(es, "Could not duplicate class name!");
	if(notype)
		return;
	s = eel__register(vm, name, EEL_SCLASS);
	if(!s)
		return;
	s->v.object = es->classes[type];
	eel_o_own(s->v.object);
	eel_register_essx(vm, type, s);
}

/*
 * Bootstrap logic in eel_register_class():
 *	EEL_CCLASS has a shortcut constructor that bypasses
 *	eel_o_construct(), in order to be operational even
 *	if EEL_CCLASS itself is not yet registered.
 *	   As long as EEL_CSTRING is not yet registered,
 *	classes are registered without names. Trying to
 *	register a class that is already registered is
 *	normally an error, but if the existing class has
 *	no name, it just adds the name and returns.
 */
static EEL_object *eel__register_class(EEL_vm *vm,
		EEL_classes type, const char *name, EEL_classes ancestor,
		EEL_ctor_cb construct, EEL_dtor_cb destruct,
		EEL_rector_cb reconstruct, int notype)
{
	EEL_state *es = VMP->state;
	EEL_classdef *cd;
	if((int)type < 0)
		type = es->nclasses;
	eel_info(es, "Registering class '%s' (ID: %d):", name, type);
//printf("Registering class '%s' (ID: %d)\n", name, type);
	if(type >= es->maxclasses)
	{
		EEL_object **nt;
		int nmax = es->maxclasses ? es->maxclasses * 2 : 32;
		while(nmax <= type)
			nmax *= 2;
		nt = (EEL_object **)eel_realloc(es->vm, es->classes,
				sizeof(EEL_object *) * nmax);
		if(!nt)
			eel_serror(es, "Could not reallocate class table!");
		memset(nt + es->maxclasses, 0,
				sizeof(EEL_object *) * (nmax - es->maxclasses));
		es->classes = nt;
		es->maxclasses = nmax;
	}
	if(type >= es->nclasses)
	{
		es->nclasses = type + 1;
		if(eel_realloc_cast_matrix(es, es->nclasses) < 0)
			eel_serror(es, "Could not reallocate cast matrix!");
	}
	if(es->classes[type])
	{
		/*
		 * Bootstrap mode:
		 *	We're allowed to "reregister" to add a name
		 *	if ->name is NULL, but anything else will fail.
		 */
		if(o2EEL_classdef(es->classes[type])->name)
			eel_ierror(es, "Class already registered!");
		DBGO(printf("BOOTSTRAP MODE: Class '%s' exists.\n", name);)
	}
	else
	{
		DBGO(printf("-----------: Adding class '%s'.\n", name);)
		es->classes[type] = eel_cclass_construct(es->vm);
		if(!es->classes[type])
			eel_serror(es, "Class constructor failed!");

		/* An instance is a reference to it's class! */
		if(es->classes[EEL_CCLASS])	/* Bootstrapping... */
			eel_o_own(es->classes[EEL_CCLASS]);

		cd = o2EEL_classdef(es->classes[type]);

		/* Set this and ancestor class */
		cd->typeid = type;
		cd->ancestor = ancestor;

		/* Register constructor and destructor */
		if(construct)
			cd->construct = construct;
		if(destruct)
			cd->destruct = destruct;
		if(reconstruct)
			cd->reconstruct = reconstruct;
	}
	eel__add_class_name(es, type, name, notype);
	eel_clear_info(es);
	return es->classes[type];
}


EEL_object *eel_register_class(EEL_vm *vm,
		EEL_classes type, const char *name, EEL_classes ancestor,
		EEL_ctor_cb construct, EEL_dtor_cb destruct,
		EEL_rector_cb reconstruct)
{
	int notype;
	if('\001' == name[0])
	{
		notype = 1;
		++name;
	}
	else
		notype = 0;
	return eel__register_class(vm, type, name, ancestor,
			construct, destruct, reconstruct, notype);
}


void eel_unregister_class(EEL_object *c)
{
	EEL_classdef *cd = o2EEL_classdef(c);
	/*
	 * We can't just destroy the class, because there may still
	 * be instances hanging around. However, when we remove the
	 * cd->registered mark and have the class table disown the
	 * class, it will go away with the last instance, clearing
	 * the class table entry in the process.
	 */
	if(cd->registered)
	{
		cd->registered = 0;
		eel_o_disown_nz(c);
	}
}


static void _set_metamethod(EEL_object *c, EEL_mmindex mm, EEL_mm_cb cb)
{
	EEL_vm *vm = c->vm;
	EEL_state *es = VMP->state;
	if((mm < 0) || (mm >= EEL_MM__COUNT))
		eel_ierror(es, "Metamethod index out of range!");
	if(!cb)
		cb = eel_cclass_no_method;
	o2EEL_classdef(es->classes[eel_c2cid(c)])->mmethods[mm] = cb;
}


void eel_set_unregister(EEL_object *classdef, EEL_unregister_cb ur)
{
	EEL_vm *vm = classdef->vm;
	EEL_state *es = VMP->state;
	if((EEL_classes)classdef->type != EEL_CCLASS)
		eel_ierror(es, "Object %s passed to eel_set_unregister() "
				"is not a classdef!", eel_o_stringrep(classdef));
	o2EEL_classdef(classdef)->unregister = ur;
}


/* Set classdata field for class 'cid' */
void eel_set_classdata(EEL_object *classdef, void *classdata)
{
	EEL_vm *vm = classdef->vm;
	EEL_state *es = VMP->state;
	if((EEL_classes)classdef->type != EEL_CCLASS)
		eel_ierror(es, "Object %s passed to eel_set_unregister() "
				"is not a classdef!", eel_o_stringrep(classdef));
	o2EEL_classdef(classdef)->classdata = classdata;
}


void *eel_get_classdata(EEL_object *object)
{
	EEL_vm *vm = object->vm;
	EEL_state *es = VMP->state;
	EEL_classdef *cd = o2EEL_classdef(es->classes[object->type]);
	return cd->classdata;
}


/*----------------------------------------------------------
	Module Injection API
----------------------------------------------------------*/

static int eel__export_object(EEL_object *module, const char *name, EEL_object *o)
{
	EEL_module *m = o2EEL_module(module);
	EEL_value or;
	eel_o2v(&or, o);
	if(eel_table_sets(m->exports, name, &or))
		return -1;
	return 0;
}


int eel_share_module(EEL_object *module)
{
	EEL_vm *vm = module->vm;
	EEL_module *m = o2EEL_module(module);
	EEL_value mn, mref;
	EEL_state *es = VMP->state;
	EEL_xno x = eel_table_gets(m->exports, "__modname", &mn);
	if(x)
		return -1;
	if(es->modules)
	{
		/* Add to module list! */
		eel_o2wr(&mref, module);
		eel_table_set(es->modules, &mn, &mref);
	}
	else
		return -2;
	return 0;
}


EEL_object *eel_create_module(EEL_vm *vm, const char *name,
		EEL_unload_cb unload, void *moduledata)
{
	EEL_value v;
	EEL_module *m;
	EEL_xno x;
	x = eel_o_construct(vm, EEL_CMODULE, NULL, 0, &v);
	if(x)
		return NULL;
	m = o2EEL_module(v.objref.v);
	eel_table_setss(m->exports, "__modname", name);
	eel_table_setss(m->exports, "__filename", name);
	m->unload = unload;
	m->moduledata = moduledata;
	eel_share_module(v.objref.v);
	return v.objref.v;
}


EEL_object *eel_export_class(EEL_object *module,
		const char *name, EEL_classes ancestor,
		EEL_ctor_cb construct, EEL_dtor_cb destruct,
		EEL_rector_cb reconstruct)
{
	EEL_vm *vm = module->vm;
	EEL_state *es = VMP->state;
	eel_try(es)
	{
		EEL_object *c = eel__register_class(vm,
				-1, name, ancestor,
				construct, destruct, reconstruct, 0);
		if(eel__export_object(module, name, c) < 0)
		{
			eel_unregister_class(c);
			eel_done(es);
			return NULL;
		}
		eel_o_disown_nz(c);	/* Module owns it now */
		eel_done(es);
		return c;
	}
	eel_except
		return NULL;
}


EEL_types eel_class_typeid(EEL_object *c)
{
	return eel_c2cid(c);
}


EEL_xno eel_set_metamethod(EEL_object *c, EEL_mmindex mm, EEL_mm_cb cb)
{
	EEL_vm *vm = c->vm;
	EEL_state *es = VMP->state;
	eel_try(es)
	{
		_set_metamethod(c, mm, cb);
		eel_done(es);
		return 0;
	}
	eel_except
		return EEL_XCANTSETMETHOD;
}


#if 0
/*
FIXME: The objects array is the right place, but this
FIXME: code refuses to work for some reason!
*/
EEL_object *eel_add_cfunction(EEL_object *module,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_module *m = o2EEL_module(module);
	EEL_object_p_da *a;
	EEL_object *fo = eel__register_cfunction(module->vm,
			results, name, reqargs, optargs, tupargs, func);
	if(!fo)
		return NULL;
	a = &m->objects;
	if(eel_objs_setsize(module->vm, a, a->size + 1) < 0)
	{
		eel_o_disown_nz(fo);
		return NULL;
	}
	a->array[a->size] = fo;		/* Module takes ownership! */
	++a->size;
	return fo;
}
#else
EEL_object *eel_add_cfunction(EEL_object *module,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_vm *vm = module->vm;
	EEL_state *es = VMP->state;
	char *nname = eel_unique(vm, name);
	EEL_object *o = eel_export_cfunction(module, results, nname,
			reqargs, optargs, tupargs, func);
	eel_sfree(es, nname);
	return o;
}
#endif

EEL_object *eel_export_cfunction(EEL_object *module,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_xno x;
	EEL_value fv, fn;
	EEL_module *m = o2EEL_module(module);
	EEL_object *fo = eel__register_cfunction(module->vm,
			results, name, reqargs, optargs, tupargs, func);
	if(!fo)
		return NULL;
	o2EEL_function(fo)->common.module = module;
	eel_o2v(&fn, o2EEL_function(fo)->common.name);
	eel_o2v(&fv, fo);
	x = eel_table_set(m->exports, &fn, &fv); /* Table takes ownership! */
	eel_o_disown_nz(fo);	/* Failure to insert means function dies here. */
	return x ? NULL : fo;
}


EEL_xno eel_export_constant(EEL_object *module,
		const char *name, EEL_value *value)
{
	EEL_module *m = o2EEL_module(module);
	return eel_table_sets(m->exports, name, value);
}


EEL_xno eel_export_lconstant(EEL_object *module,
		const char *name, long value)
{
	EEL_value v;
	EEL_module *m = o2EEL_module(module);
	eel_l2v(&v, value);
	return eel_table_sets(m->exports, name, &v);
}


EEL_xno eel_export_dconstant(EEL_object *module,
		const char *name, double value)
{
	EEL_value v;
	EEL_module *m = o2EEL_module(module);
	eel_d2v(&v, value);
	return eel_table_sets(m->exports, name, &v);
}


EEL_xno eel_export_sconstant(EEL_object *module,
		const char *name, const char *value)
{
	EEL_module *m = o2EEL_module(module);
	return eel_table_setss(m->exports, name, value);
}


EEL_xno eel_export_lconstants(EEL_object *module, const EEL_lconstexp *data)
{
	while(data->name)
	{
		EEL_xno x = eel_export_lconstant(module, data->name, data->value);
		if(x)
			return x;
		++data;
	}
	return 0;
}
