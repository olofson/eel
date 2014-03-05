/*
---------------------------------------------------------------------------
	e_builtin.c - EEL built-in functions
---------------------------------------------------------------------------
 * Copyright 2002-2014 David Olofson
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

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#else
#include <sched.h>
#include <sys/time.h>
#include <sys/wait.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "e_builtin.h"
#include "ec_symtab.h"
#include "e_util.h"
#include "e_string.h"
#include "e_dstring.h"
#include "e_object.h"
#include "e_class.h"
#include "e_state.h"
#include "e_builtin.h"
#include "e_function.h"
#include "e_table.h"

#ifndef WEXITSTATUS
#define WEXITSTATUS(x)	((x) & 0xff)
#endif


typedef struct
{
#ifdef _WIN32
	DWORD start;
#else
	struct timeval start;
#endif
} BI_moduledata;


static EEL_xno bi__version(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	if(arg->type != EEL_TINTEGER)
		return EEL_XNEEDINTEGER;

	vm->heap[vm->resv].type = EEL_TINTEGER;
	switch(arg->integer.v)
	{
	  case 0:
	  	vm->heap[vm->resv].integer.v = EEL_MAJOR_VERSION;
		break;
	  case 1:
	  	vm->heap[vm->resv].integer.v = EEL_MINOR_VERSION;
		break;
	  case 2:
	  	vm->heap[vm->resv].integer.v = EEL_MICRO_VERSION;
		break;
	  case 3:
	  	vm->heap[vm->resv].integer.v = EEL_SNAPSHOT;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	return 0;
}


static EEL_xno bi__xname(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	EEL_object *s;
	const char *n;

	if(arg->type != EEL_TINTEGER)
		return EEL_XNEEDINTEGER;
	if(arg->integer.v >= EEL__XCOUNT)
		return EEL_XHIGHINDEX;
	if(arg->integer.v < 0)
		return EEL_XLOWINDEX;

	n = eel_x_name(vm, arg->integer.v);
	s = eel_ps_new(vm, n);
	if(!s)
		return EEL_XCONSTRUCTOR;
	vm->heap[vm->resv].type = EEL_TOBJREF;
	vm->heap[vm->resv].objref.v = s;
	return 0;
}


static EEL_xno bi__xdesc(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	EEL_object *s;
	const char *n;

	if(arg->type != EEL_TINTEGER)
		return EEL_XNEEDINTEGER;
	if(arg->integer.v >= EEL__XCOUNT)
		return EEL_XHIGHINDEX;
	if(arg->integer.v < 0)
		return EEL_XLOWINDEX;

	n = eel_x_description(vm, arg->integer.v);
	s = eel_ps_new(vm, n);
	if(!s)
		return EEL_XCONSTRUCTOR;
	vm->heap[vm->resv].type = EEL_TOBJREF;
	vm->heap[vm->resv].objref.v = s;
	return 0;
}


static EEL_xno print_object(EEL_vm *vm, EEL_object *o)
{
	switch((EEL_classes)o->type)
	{
	  case EEL_CNIL:
	  case EEL_CREAL:
	  case EEL_CINTEGER:
	  case EEL_CBOOLEAN:
	  case EEL_CTYPEID:
	  case EEL_COBJREF:
	  case EEL_CWEAKREF:
		return EEL_XNEEDOBJECT;
	  case EEL_CVALUE:
	  case EEL_COBJECT:
	  case EEL_CCLASS:
		break;
	  case EEL_CSTRING:
	  {
		EEL_string *s = o2EEL_string(o);
		return printf("%.*s", s->length, s->buffer);
	  }
	  case EEL_CDSTRING:
	  {
		EEL_dstring *ds = o2EEL_dstring(o);
		return printf("%.*s", ds->length, ds->buffer);
	  }
	  case EEL_CFUNCTION:
	  case EEL_CMODULE:
	  case EEL_CARRAY:
	  case EEL_CTABLE:
	  case EEL_CVECTOR:
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
	  case EEL_CVECTOR_D:
	  case EEL__CUSER:
		break;
	}
	{
		int res;
		const char *s = eel_o_stringrep(o);
		res = printf("%s", s);
		eel_sfree(VMP->state, s);
		return res;
	}
}

static EEL_xno bi_print(EEL_vm *vm)
{
	int i, count = 0;
	for(i = 0; i < vm->argc; ++i)
	{
		EEL_value *v = &vm->heap[vm->argv + i];
		switch((EEL_types)v->type)
		{
		  case EEL_TNIL:
			count += printf("<nil>");
			break;
		  case EEL_TREAL:
			count += printf(EEL_REAL_FMT, v->real.v);
			break;
		  case EEL_TINTEGER:
			count += printf("%d", v->integer.v);
			break;
		  case EEL_TBOOLEAN:
			count += printf(v->integer.v ? "true" : "false");
			break;
		  case EEL_TTYPEID:
			count += printf("<typeid %s>",
					eel_o2s(o2EEL_classdef(VMP->state->classes[
					v->integer.v])->name));
			break;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			count += print_object(vm, v->objref.v);
			break;
		}
		if((EEL_nontypes)v->type > EEL_TLASTTYPE)
			count += printf("<illegal value; type %d>", v->type);
	}
	eel_l2v(vm->heap + vm->resv, count);
	return 0;
}


/*
 * NOTE: This takes a flags argument to match the prototype for
 *       $.module_loader entries, but ignores that argument.
 */
static EEL_xno bi__get_loaded_module(EEL_vm *vm)
{
	EEL_object *m;
	EEL_value *args = vm->heap + vm->argv;
	const char *name = eel_v2s(args + 0);
	if(!name)
		return EEL_XNEEDSTRING;
	if(!(m = eel_get_loaded_module(vm, name)))
		return EEL_XWRONGINDEX;
	eel_o2v(vm->heap + vm->resv, m);
	return 0;
}


static EEL_xno bi__load_binary(EEL_vm *vm)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno bi__compile(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	unsigned flags = (unsigned)eel_v2l(args + 1);
	if(EEL_TYPE(args + 0) != (EEL_types)EEL_CMODULE)
		return EEL_XNEEDMODULE;
	eel_try(VMP->state)
		eel_compile(VMP->state, args[0].objref.v, flags);
	eel_except
		return EEL_XCOMPILE;
	return 0;
}


static EEL_xno bi__exports(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	EEL_object *m;
	if(vm->argc >= 1)
	{
		EEL_function *f;
		if((EEL_classes)EEL_TYPE(arg) != EEL_CFUNCTION)
			return EEL_XWRONGTYPE;
		f = o2EEL_function(arg->objref.v);
		m = f->common.module;
	}
	else
	{
		EEL_object *f = eel_calling_function(vm);
		if(!f)
			return EEL_XBADCONTEXT;
		m = o2EEL_function(f)->common.module;
		if(!m)
			return EEL_XBADCONTEXT;
	}
	vm->heap[vm->resv].type = EEL_TOBJREF;
	vm->heap[vm->resv].objref.v = o2EEL_module(m)->exports;
	eel_o_own(vm->heap[vm->resv].objref.v);
	return 0;
}


static unsigned long get_ticks(BI_moduledata *md)
{
#ifdef _WIN32
	DWORD now;
	now = timeGetTime();
	if(now < md->start)
		return ((~(DWORD)0) - md->start) + now;
	else
		return now - md->start;
#else
	struct timeval now;
	gettimeofday(&now, NULL);
	return (now.tv_sec - md->start.tv_sec) * 1000 +
			(now.tv_usec - md->start.tv_usec) / 1000;
#endif
}


static EEL_xno bi_getms(EEL_vm *vm)
{
	BI_moduledata *md = (BI_moduledata *)eel_get_current_moduledata(vm);
	vm->heap[vm->resv].type = EEL_TINTEGER;
	vm->heap[vm->resv].integer.v = get_ticks(md);
	return 0;
}


static EEL_xno bi_getus(EEL_vm *vm)
{
	BI_moduledata *md = (BI_moduledata *)eel_get_current_moduledata(vm);
#ifdef _WIN32
	DWORD now, ticks;
	now = timeGetTime();
	if(now < md->start)
		ticks = (double)(((~(DWORD)0) - md->start) + now) * 1000.0f;
	else
		ticks = (double)(now - md->start) * 1000.0f;
#else
	struct timeval now;
	double ticks;
	gettimeofday(&now, NULL);
	ticks = (now.tv_sec - md->start.tv_sec) * 1000000.0f +
			(now.tv_usec - md->start.tv_usec);
#endif
	vm->heap[vm->resv].type = EEL_TREAL;
	vm->heap[vm->resv].real.v = ticks;
	return 0;
}


static EEL_xno bi_sleep(EEL_vm *vm)
{
#ifdef _WIN32
	DWORD t1 = timeGetTime();
	Sleep(eel_v2l(vm->heap + vm->argv));
	vm->heap[vm->resv].integer.v = timeGetTime() - t1;
#else
	BI_moduledata *md = (BI_moduledata *)eel_get_current_moduledata(vm);
	unsigned long ms = eel_v2l(vm->heap + vm->argv);
	struct timeval tv;
	unsigned long t1, then, now, elapsed;
	t1 = then = get_ticks(md);
	if(!ms)
	{
		sched_yield();
		now = get_ticks(md);
	}
	else
		while(1)
		{
			now = get_ticks(md);
			elapsed = now - then;
			then = now;
			if(elapsed >= ms)
				break;
			ms -= elapsed;
			tv.tv_sec = ms / 1000;
			tv.tv_usec = (ms % 1000) * 1000;
			select(0, NULL, NULL, NULL, &tv);
		}
	vm->heap[vm->resv].integer.v = now - t1;
#endif
	vm->heap[vm->resv].type = EEL_TINTEGER;
	return 0;
}


static EEL_xno bi_getis(EEL_vm *vm)
{
	vm->heap[vm->resv].type = EEL_TINTEGER;
#if DBG6B(1)+0 == 1
	vm->heap[vm->resv].integer.v = VMP->instructions;
#else
	vm->heap[vm->resv].integer.v = 0;
#endif
	return 0;
}


static EEL_xno bi_getmt(EEL_vm *vm)
{
	vm->heap[vm->resv].type = EEL_TOBJREF;
	vm->heap[vm->resv].objref.v = VMP->state->modules;
	eel_o_own(VMP->state->modules);
	return 0;
}


static EEL_xno bi_clean_modules(EEL_vm *vm)
{
	eel_clean_modules(vm);
	return 0;
}


static EEL_xno bi_caller(EEL_vm *vm)
{
	EEL_callframe *cf;
	cf = (EEL_callframe *)(vm->heap + vm->base - EEL_CFREGS);
	cf = (EEL_callframe *)(vm->heap + cf->r_base - EEL_CFREGS);
	if(cf->f)
	{
		cf = (EEL_callframe *)(vm->heap + cf->r_base - EEL_CFREGS);
		if(cf->f)
		{
			vm->heap[vm->resv].type = EEL_TOBJREF;
			vm->heap[vm->resv].objref.v = cf->f;
			eel_o_own(cf->f);
			return 0;
		}
	}
	vm->heap[vm->resv].type = EEL_TNIL;
	return 0;
}


static EEL_xno bi_system(EEL_vm *vm)
{
	int res;
	const char *s = eel_v2s(&vm->heap[vm->argv]);
	if(!s)
		return EEL_XNEEDSTRING;
	res = system(s);
	if(res == -1)
		return EEL_XFILEERROR;
	eel_l2v(vm->heap + vm->resv, WEXITSTATUS(res));
	return 0;
}


static EEL_xno bi_ShellExecute(EEL_vm *vm)
{
#ifdef WIN32
	EEL_value *args = vm->heap + vm->argv;
	HINSTANCE res;
	int showcmd = SW_SHOWNORMAL;
	const char *op = eel_v2s(args);
	const char *file = eel_v2s(args + 1);
	const char *params = NULL;
	const char *dir = NULL;
	if(!file)
		return EEL_XNEEDSTRING;
	if(vm->argc > 2)
		params = eel_v2s(args + 2);
	if(vm->argc > 3)
		dir = eel_v2s(args + 3);
	if(vm->argc > 4)
		showcmd = eel_v2l(args + 4);
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	res = ShellExecute(NULL, op, file, params, dir, showcmd);
	CoUninitialize();
	if((int)res < 32)
	{
		switch((int)res)
		{
		  case 0:
		  case SE_ERR_OOM:
			return EEL_XMEMORY;
		  case ERROR_FILE_NOT_FOUND:
		  case ERROR_PATH_NOT_FOUND:
		  case ERROR_BAD_FORMAT:
		  case SE_ERR_DLLNOTFOUND:
			return EEL_XFILEOPEN;
		  case SE_ERR_ACCESSDENIED:
		  case SE_ERR_ASSOCINCOMPLETE:
		  case SE_ERR_DDEFAIL:
		  case SE_ERR_NOASSOC:
			return EEL_XFILEERROR;
		  case SE_ERR_DDEBUSY:
			return EEL_XFILEOPENED;
		  case SE_ERR_SHARE:
			return EEL_XDEVICEERROR;
		}
		return EEL_XDEVICEERROR;
	}
	eel_l2v(vm->heap + vm->resv, (int)res);
	return 0;
#else
	return EEL_XNOTIMPLEMENTED;
#endif
}


static EEL_xno bi_insert(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	return eel_insert(args->objref.v, args + 1, args + 2);
}


static EEL_xno bi_delete(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	switch(vm->argc)
	{
	  case 3:
		return eel_o__metamethod(args->objref.v, EEL_MM_DELETE,
				args + 1, args + 2);
	  case 2:
		return eel_o__metamethod(args->objref.v, EEL_MM_DELETE,
				args + 1, NULL);
	  case 1:
		return eel_o__metamethod(args->objref.v, EEL_MM_DELETE,
				NULL, NULL);
	}
	return EEL_XINTERNAL;	/* Should never get here! */
}


static EEL_xno bi_copy(EEL_vm *vm)
{
	EEL_xno x;
	EEL_object *o;
	EEL_value start;
	EEL_value len;
	EEL_value *args = vm->heap + vm->argv;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	o = args->objref.v;
	eel_l2v(&start, vm->argc >= 2 ? eel_v2l(args + 1) : 0);
	if(vm->argc >= 3)
		eel_l2v(&len, eel_v2l(args + 2));
	else
	{
		x = eel_o__metamethod(o, EEL_MM_LENGTH, NULL, &len);
		if(x)
			return x;
		len.integer.v -= start.integer.v;
	}
	x = eel_o__metamethod(o, EEL_MM_COPY, &start, &len);
	if(x)
		return x;
	eel_v_move(vm->heap + vm->resv, &len);
	return x;
}


static EEL_xno bi_index(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	switch((EEL_classes)EEL_TYPE(args))
	{
	  case EEL_CTABLE:
	  {
		EEL_tableitem *ti;
		int i = eel_v2l(args + 1);
		if(i < 0)
			return EEL_XLOWINDEX;
		ti = eel_table_get_item(args->objref.v, i);
		if(!ti)
			return EEL_XHIGHINDEX;
		eel_v_copy(res, eel_table_get_value(ti));
		return 0;
	  }
	  default:
		return eel_o__metamethod(args->objref.v, EEL_MM_GETINDEX,
			args + 1, res);
	}
}


static EEL_xno bi_key(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	EEL_tableitem *ti;
	int i;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	if((EEL_classes)EEL_TYPE(args) != EEL_CTABLE)
		return EEL_XNEEDTABLE;
	i = eel_v2l(args + 1);
	if(i < 0)
		return EEL_XLOWINDEX;
	ti = eel_table_get_item(args->objref.v, i);
	if(!ti)
		return EEL_XHIGHINDEX;
	eel_v_copy(res, eel_table_get_key(ti));
	return 0;
}


static EEL_xno bi_tryindex(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	if(eel_o__metamethod(args->objref.v, EEL_MM_GETINDEX, args + 1, res))
	{
		if(vm->argc >= 3)
			eel_v_copy(res, args + 2);
		else
			eel_nil2v(res);
	}
	return 0;
}


static EEL_xno bi_vacall(EEL_vm *vm)
{
/*	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	EEL_tableitem *ti;
	int i;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	if(EEL_TYPE(args) != EEL_CTABLE)
		return EEL_XNEEDTABLE;
	i = eel_v2l(args + 1);
	if(i < 0)
		return EEL_XLOWINDEX;
	ti = eel_table_get_item(args->objref.v, i);
	if(!ti)
		return EEL_XHIGHINDEX;
	eel_v_copy(res, eel_table_get_key(ti));
	return 0;*/
	return EEL_XNOTIMPLEMENTED;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno bi_unload(EEL_object *m, int closing)
{
	if(closing)
	{
#ifdef _WIN32
		timeEndPeriod(1);
#endif
		eel_free(m->vm, eel_get_moduledata(m));
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

#ifdef	EEL_USE_EELBIL
static char builtin_eel[] =
#include "builtin.c"
;
#endif


static const EEL_lconstexp bi_constants[] =
{
	/* showcmd argument for ShellExecute() */
	{ "SW_HIDE",		0	},
	{ "SW_SHOWNORMAL",	1	},
	{ "SW_SHOWMINIMIZED",	2	},
	{ "SW_SHOWMAXIMIZED",	3	},
	{ "SW_SHOWNOACTIVATE",	4	},
	{ "SW_SHOW",		5	},
	{ "SW_MINIMIZE",	6	},
	{ "SW_SHOWMINNOACTIVE",	7	},
	{ "SW_SHOWNA",		8	},
	{ "SW_RESTORE",		9	},
	{ "SW_SHOWDEFAULT",	10	},

	/* Compiler flags */
	{ "SF_NOCOMPILE",	EEL_SF_NOCOMPILE	},
	{ "SF_NOINIT",		EEL_SF_NOINIT		},
	{ "SF_LIST",		EEL_SF_LIST		},
	{ "SF_LISTASM",		EEL_SF_LISTASM		},
	{ "SF_WERROR",		EEL_SF_WERROR		},
	{ "SF_NOPRECEDENCE",	EEL_SF_NOPRECEDENCE	},

	{NULL, 0}
};


EEL_xno eel_builtin_init(EEL_vm *vm)
{
	EEL_object *m;
#ifdef	EEL_USE_EELBIL
	EEL_state *es = VMP->state;
#endif
	BI_moduledata *md = (BI_moduledata *)eel_malloc(vm,
			sizeof(BI_moduledata));
	if(!md)
		return EEL_XMEMORY;
	m = eel_create_module(vm, "builtin_c", bi_unload, md);
	if(!m)
	{
		eel_free(vm, md);
		return EEL_XMODULEINIT;
	}

#ifdef _WIN32
	timeBeginPeriod(1);
	md->start = timeGetTime();
#else
	gettimeofday(&md->start, NULL);
#endif
	/* Version and other info */
	eel_export_cfunction(m, 1, "__version", 1, 0, 0, bi__version);
	eel_export_cfunction(m, 1, "exception_name", 1, 0, 0, bi__xname);
	eel_export_cfunction(m, 1, "exception_description", 1, 0, 0,
			bi__xdesc);

	/* "System" */
	eel_export_cfunction(m, 1, "print", 0, -1, 0, bi_print);
	eel_export_cfunction(m, 1, "getms", 0, 0, 0, bi_getms);
	eel_export_cfunction(m, 1, "getus", 0, 0, 0, bi_getus);
	eel_export_cfunction(m, 1, "sleep", 1, 0, 0, bi_sleep);
	eel_export_cfunction(m, 1, "get_instruction_count", 0, 0, 0, bi_getis);
	eel_export_cfunction(m, 1, "__caller", 0, 0, 0, bi_caller);
	eel_export_cfunction(m, 1, "system", 1, 0, 0, bi_system);

	/* Platform specific: Win32 */
	eel_export_cfunction(m, 1, "ShellExecute", 2, 3, 0, bi_ShellExecute);
	
	/* Operations on indexable objects */
	eel_export_cfunction(m, 0, "insert", 3, 0, 0, bi_insert);
	eel_export_cfunction(m, 0, "delete", 1, 2, 0, bi_delete);
	eel_export_cfunction(m, 1, "copy", 1, 2, 0, bi_copy);
	eel_export_cfunction(m, 1, "index", 2, 0, 0, bi_index);
	eel_export_cfunction(m, 1, "key", 2, 0, 0, bi_key);
	eel_export_cfunction(m, 1, "tryindex", 2, 1, 0, bi_tryindex);

	/* Run-time EEL module management */
	eel_export_cfunction(m, 1, "__get_loaded_module", 2, 0, 0,
			bi__get_loaded_module);
	eel_export_cfunction(m, 1, "__load_binary", 2, 0, 0, bi__load_binary);
	eel_export_cfunction(m, 0, "__compile", 2, 0, 0, bi__compile);
	eel_export_cfunction(m, 1, "__exports", 0, 1, 0, bi__exports);
	eel_export_cfunction(m, 1, "__modules", 0, 0, 0, bi_getmt);
	eel_export_cfunction(m, 0, "__clean_modules", 0, 0, 0,
			bi_clean_modules);

	/* Utilities for variadics */
	eel_export_cfunction(m, 1, "vacall", 2, 2, 0, bi_vacall);

	/* Constants and enums */
	eel_export_lconstants(m, bi_constants);

#ifdef	EEL_USE_EELBIL
	/* Built-in EEL library */
	es->eellib = eel_load_buffer(vm, builtin_eel, strlen(builtin_eel), 0);
	if(!es->eellib)
	{
		eel_o_disown_nz(m);
		return EEL_XMODULEINIT;
	}
	SETNAME(es->eellib, "EEL Built-in Library Module");

	eel_try(es)
		eel_s_get_exports(es->root_symtab, es->eellib);
	eel_except
	{
		eel_o_disown(&es->eellib);
		eel_o_disown_nz(m);
		return EEL_XMODULEINIT;
	}
#endif
	eel_o_disown_nz(m);
	return 0;
}
