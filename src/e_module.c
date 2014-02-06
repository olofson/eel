/*
---------------------------------------------------------------------------
	e_module.c - EEL code module management
---------------------------------------------------------------------------
 * Copyright 2002, 2004-2006, 2009-2013 David Olofson
 * Copyright 2002 Florian Schulze <fs@crowproductions.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "e_function.h"
#include "e_operate.h"
#include "e_table.h"
#include "e_vector.h"
#include "e_register.h"

#ifdef	EEL_PROFILING
#  include "e_string.h"
#endif


/*----------------------------------------------------------
	Code module management
----------------------------------------------------------*/

static void read_file(EEL_state *es, EEL_object *mo, FILE *f)
{
	EEL_module *m = o2EEL_module(mo);
	if(fseek(f, 0, SEEK_END) < 0)
		eel_serror(es, "Could not seek to EOF!");

	m->len = ftell(f);
	if(fseek(f, 0, SEEK_SET) < 0)
		eel_serror(es, "Rewind failed!");

	m->source = (unsigned char *)malloc(m->len + 1);
	if(!m->source)
		eel_serror(es, "Out of memory!");

	if(fread(m->source, m->len, 1, f) != 1)
		eel_serror(es, "Read error!");

	DBG1(printf("Loaded module \"%s\". (ID: %d)\n",
			eel_table_getss(m->exports, "__modname"), m->id);)
}


static void load_from_file(EEL_state *es, EEL_object *mo,
		const char *filename, const char *extension)
{
	EEL_module *m = o2EEL_module(mo);
	FILE *f;
	char *name;
	const char *fn;
	int len = strlen(filename) + strlen(eel_path(es->vm));
	if(extension)
		len += strlen(extension) + 1;

	/* Construct full name with path */
	name = malloc(len + 1);
	if(!name)
		eel_serror(es, "Out of memory when constructing path!");
	strcpy(name, eel_path(es->vm));
	if(strlen(name))
	{
#ifdef WIN32
		if(name[strlen(name)-1] != '\\')
			strcat(name, "\\");
#elif defined MACOS
		if(name[strlen(name)-1] != ':')
			strcat(name, ":");
#else
		if(name[strlen(name)-1] != '/')
			strcat(name, "/");
#endif
	}
	strcat(name, filename);
	if(extension)
	{
		strcat(name, ".");
		strcat(name, extension);
	}
	eel_table_setss(m->exports, "__filename", name);
	eel_table_setss(m->exports, "__modname", name);
	free(name);

	fn = eel_table_getss(m->exports, "__filename");
	if(!fn)
		eel_cerror(es, "File name went missing in action!");

	f = fopen(fn, "rb");
	if(!f)
		eel_cerror(es, "Could not open file!");

	eel_try(es)
	{
		read_file(es, mo, f);
		fclose(f);
	}
	eel_except
	{
		fclose(f);
		eel_cerror(es, "Could not load file \"%s\"!", fn);
	}
}


static EEL_object *try_load(EEL_state *es, const char *filename,
		const char *extension, EEL_sflags flags)
{
	EEL_value v;
	EEL_object *m;
	eel_info(es, "Trying to load \"%s%s%s\"", filename,
			extension ? "." : "",
			extension ?  extension: "");
	es->vm->lasterror = eel_o_construct(es->vm, EEL_CMODULE, NULL, 0, &v);
	if(es->vm->lasterror)
		return NULL;
	m = v.objref.v;

	eel_try(es)
	{
		es->vm->lasterror = EEL_XFILELOAD;
		load_from_file(es, m, filename, extension);
		if(flags & EEL_SF_COMPILE)
		{
			eel_info(es, "Trying to compile \"%s%s%s\"", filename,
					extension ? "." : "",
					extension ?  extension: "");
			es->vm->lasterror = EEL_XCOMPILE;
			eel_compile(es, m, flags);
			if(flags & EEL_SF_INITIALIZE)
			{
				eel_info(es, "Trying to initialize \"%s%s%s\"",
						filename,
						extension ? "." : "",
						extension ?  extension: "");
				es->vm->lasterror = eel_callnf(es->vm, m,
						"__init_module", NULL);
				if(es->vm->lasterror)
					eel_cerror(es, "Could not initialize"
							" module '%s'! (%s)",
							eel_table_getss(
							o2EEL_module(m)->exports,
							"__modname"),
							eel_x_name(es->vm, es->vm->lasterror));
			}
		}
	}
	eel_except
	{
		eel_lexer_invalidate(es);
		eel_o_disown_nz(m);
		return NULL;
	}
	es->vm->lasterror = 0;
	return m;
}


static EEL_object *try_load_names(EEL_state *es, const char *filename,
		EEL_sflags flags)
{
	EEL_object *m;
	char *dot;
	dot = strrchr(filename, '.');
	if(dot)
	{
		/* Find last path separator */
		char *separator, *s2, *s3;
		separator = strrchr(filename, '/');
		s2 = strrchr(filename, '\\');
		s3 = strrchr(filename, ':');
		if(s2 > separator)
			separator = s2;
		if(s3 > separator)
			separator = s3;
		/* Dot part of the actual file name? */
		if(dot > separator)
		{
			m = try_load(es, filename, NULL, flags);
			if(m)
			{
				eel_clear_errors(es);
				return m;
			}
		}
	}
#if 0
	m = try_load(es, filename, "eec", flags);
	if(m)
	{
		eel_clear_errors(es);
		return m;
	}
#endif
	m = try_load(es, filename, "ess", flags);
	if(m)
	{
		eel_clear_errors(es);
		return m;
	}
	m = try_load(es, filename, "eel", flags);
	if(m)
	{
		eel_clear_errors(es);
		return m;
	}
	return NULL;
}


EEL_object *eel_load(EEL_vm *vm, const char *filename, EEL_sflags flags)
{
	eel_clear_errors(VMP->state);
	eel_clear_warnings(VMP->state);
	return try_load_names(VMP->state, filename,
			flags | EEL_SF_COMPILE | EEL_SF_INITIALIZE);
}


EEL_object *eel_load_nc(EEL_vm *vm, const char *filename, EEL_sflags flags)
{
	return try_load_names(VMP->state, filename, flags);
}


EEL_object *eel_import(EEL_vm *vm, const char *modname)
{
	EEL_state *es = VMP->state;
	EEL_value v;
	EEL_object *m;
	eel_clear_errors(VMP->state);
	eel_clear_warnings(VMP->state);

	if(es->modules)
	{
		/* Already loaded? */
		if(!eel_table_gets(es->modules, modname, &v) &&
				EEL_IS_OBJREF(v.type))
		{
			m = v.objref.v;
			if(!m->refcount)
				eel_limbo_unlink(m);	/* es->deadmods! */
			eel_o_own(m);
			return m;
		}
	}

	/* Nope... Try to load it. */
	m = try_load_names(VMP->state,
			modname, EEL_SF_COMPILE | EEL_SF_INITIALIZE);
	if(!m)
		return NULL;
	return m;
}


static void load_from_mem(EEL_vm *vm, EEL_module *m,
		const char *source, unsigned len)
{
	EEL_state *es = VMP->state;
	char buf[256];
	const char *sn;

	/* Construct name */
	snprintf(buf, sizeof(buf) - 1, "memory script, %u bytes from %p",
			len, source);
	buf[sizeof(buf) - 1] = '\0';

	eel_table_setss(m->exports, "__filename", buf);
	eel_table_setss(m->exports, "__modname", buf);

	sn = eel_table_getss(m->exports, "__filename");
	if(!sn)
		eel_cerror(es, "Script name went missing in action!");

	m->len = len;
	m->source = (unsigned char *)malloc(m->len + 1);
	if(!m->source)
		eel_serror(es, "Could not allocate space for source!");

	memcpy(m->source, source, len);
	m->source[len] = 0;
	DBG1(printf("Loaded module \"%s\" (ID: %d)\n", sn, m->id);)
}


EEL_object *eel_load_from_mem_nc(EEL_vm *vm,
		const char *source, unsigned len, EEL_sflags flags)
{
	EEL_state *es = VMP->state;
	EEL_value v;
	EEL_object *m;
	EEL_xno x = eel_o_construct(es->vm, EEL_CMODULE, NULL, 0, &v);
	if(x)
		return NULL;
	m = v.objref.v;
	eel_try(es)
		load_from_mem(vm, o2EEL_module(m), source, len);
	eel_except
	{
		eel_o_disown_nz(m);
		return NULL;
	}
	return m;
}


EEL_object *eel_load_from_mem(EEL_vm *vm,
		const char *source, unsigned len, EEL_sflags flags)
{
	EEL_state *es = VMP->state;
	EEL_object *m = eel_load_from_mem_nc(vm, source, len, flags);
	if(!m)
		return NULL;
	eel_clear_errors(es);
	eel_clear_warnings(es);
	eel_try(es)
	{
		EEL_xno x;
		eel_compile(es, m, flags);
		x = eel_callnf(es->vm, m, "__init_module", NULL);
		if(x)
			eel_cerror(es, "Could not initialize module '%s'! (%s)",
					eel_table_getss(o2EEL_module(m)->exports,
					"__modname"), eel_x_name(vm, x));
	}
	eel_except
	{
		eel_lexer_invalidate(es);
		eel_o_disown_nz(m);
		return NULL;
	}
	return m;
}


EEL_xno eel_save(EEL_vm *vm, EEL_object *object,
		const char *filename, EEL_sflags flags)
{
	eel_clear_errors(VMP->state);
	eel_clear_warnings(VMP->state);
	return EEL_XNOTIMPLEMENTED;
}


/*----------------------------------------------------------
	EEL module class
----------------------------------------------------------*/



static EEL_xno m_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EEL_xno x;
	EEL_value v;
	EEL_module *m;
	EEL_object *eo;

	eel_clean_modules(vm);

	eo = eel_o_alloc(vm, sizeof(EEL_module), type);
	if(!eo)
		return EEL_XMEMORY;
	m = o2EEL_module(eo);
	memset(m, 0, sizeof(EEL_module));
	m->id = VMP->state->module_id_counter++;
	x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
	{
		eel_o_free(eo);
		return x;
	}
	m->exports = v.objref.v;
	eel_o2v(result,  eo);
	return 0;
}


/*
 * The real destructor!
 * We can't install this as a normal destructor, since we abuse
 * the module 'destruct' metamethod for module GC triggering.
 */
static EEL_xno m_real_destruct(EEL_object *eo)
{
	EEL_vm *vm = eo->vm;
	int i;
	EEL_xno x;
	EEL_module *m = o2EEL_module(eo);
	EEL_object_p_da *o = &m->objects;
#ifdef	EEL_PROFILING
	const char *modname = eel_table_getss(m->exports, "__modname");
#endif
	if(m->unload)
	{
		x = (m->unload)(eo, VMP->is_closing);
		if(x && !VMP->is_closing)
			return x;
	}

	if(m->refsum != -1)
	{
		int refs = eel_module_countref(eo);
		if(refs != m->refsum)
		{
#if DBG12(1)+0 == 1
			if(m->exports)
				fprintf(stderr, "Module %s refused to destruct! "
						"Refsum: %d  Recorded: %d\n",
						eel_table_getss(m->exports, "__modname"),
						refs, m->refsum);
			else
				fprintf(stderr, "Module %p refused to destruct! "
						"Refsum: %d  Recorded: %d\n",
						eo, refs, m->refsum);
#endif
			return EEL_XREFUSE;
		}
	}

#ifdef	EEL_PROFILING
	for(i = 0; i < o->size; ++i)
	{
		EEL_function *f;
		if(o->array[i]->type != EEL_CFUNCTION)
			continue;
		f = o2EEL_function(o->array[i]);
		if(!f->common.ccount)
			continue;
		printf("%s\t%s\t%ld\t%d\t%f\n",
		       		modname,
				eel_o2s(f->common.name),
				f->common.rtime,
				f->common.ccount,
				(double)f->common.rtime / f->common.ccount);
	}
#endif
	DBG1(if(m->exports)
		printf("Unloading module \"%s\". (ID %d)\n",
				eel_table_getss(m->exports, "__modname"), m->id);)

	/* Release static variables */
	DBG1(printf("  Static variables... (%d)\n", m->nvariables);)
	while(m->nvariables)
	{
		--m->nvariables;
		DBG1(printf("    <%s", eel_typename(vm,
				m->variables[m->nvariables].type));)
		DBG1(if(EEL_IS_OBJREF(m->variables[m->nvariables].type))
			printf(", %s", eel_typename(vm,
				m->variables[m->nvariables].objref.v->type));)
		DBG1(printf(">\n");)
		eel_v_disown(&m->variables[m->nvariables]);
	}
	eel_free(vm, m->variables);

	/* Release exports */
	if(m->exports)
		eel_o_disown(&m->exports);

	/* Release functions and other objects */
	DBG1(printf("  Objects... (%d)\n", o->size);)
	for(i = 0; i < o->size; ++i)
		if((EEL_classes)o->array[i]->type == EEL_CFUNCTION)
			eel_function_detach(o2EEL_function(o->array[i]));
	while(o->size)
	{
		--o->size;
		eel_o_disown_nz(o->array[o->size]);
	}
	eel_free(vm, o->array);

	DBG1(printf("  Destruction...\n");)
	eel_free(vm, m->source);
	DBG1(printf("Done.\n");)
	return 0;
}


/*
 * This is a fake destructor! All it does is add the module to
 * the list of potentially unused modules, and then it fires up
 * the module garbage collector. (Which is actually equivalent
 * to just destroying the module, unless there are modules that
 * are kept around because they've handed out callbacks and stuff.)
 */
static EEL_xno m_destruct(EEL_object *eo)
{
	EEL_vm *vm = eo->vm;
	DBG12(EEL_module *m = o2EEL_module(eo);)
	EEL_state *es = VMP->state;
	eel_limbo_push(&es->deadmods, eo);
#if DBG12(1)+0 == 1
	if(m->exports)
		fprintf(stderr, "Module \"%s\" possibly dead!\n",
				eel_table_getss(m->exports, "__modname"));
	else
		fprintf(stderr, "Module %p possibly dead!\n", eo);
#endif
	eel_clean_modules(vm);
	/*
	 * NOTE:
	 *	It may be important to consider the fact that
	 *	the module COULD be destroyed anyway at this
	 *	point, despite this EEL_XREFUSE return code.
	 *	   That is, anyone getting an EEL_XREFUSE when
	 *	trying to destroy an object should strictly
	 *	consider it a "hands off - state unknown!"
	 *	warning, and never assume that the object
	 *	really was not destroyed!
	 */
	return EEL_XREFUSE;
}


static EEL_xno m_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_module *m = o2EEL_module(eo);
#ifdef EEL_VM_CHECKING
	if(!m->exports)
		return EEL_XINTERNAL;
#endif /* EEL_VM_CHECKING */
	return eel_o__metamethod(m->exports, EEL_MM_GETINDEX, op1, op2);
}


static EEL_xno m_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return EEL_XCANTWRITE;
}


void eel_cmodule_register(EEL_vm *vm)
{
	EEL_object *c = eel_register_class(vm,
			EEL_CMODULE, "module", EEL_COBJECT,
			m_construct, m_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, m_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, m_setindex);
}


/*----------------------------------------------------------
	Indirect module reference management
----------------------------------------------------------*/

int eel_module_countref(EEL_object *mo)
{
	EEL_module *m = o2EEL_module(mo);
	int i;
	int rc = 0;
	DBG12(fprintf(stderr, "Counting refs in \"%s\"...\n",
			m->exports ? eel_table_getss(m->exports, "__modname")
			: "<some module without exports>");)
	for(i = 0; i < m->objects.size; ++i)
	{
		DBG12(fprintf(stderr, "  %s\n", eel_o_stringrep(
				m->objects.array[i]));)
		rc += m->objects.array[i]->refcount;
	}
	DBG12(fprintf(stderr, "Refs counted; result: %d.\n", rc);)
	return rc;
}


int eel_clean_modules(EEL_vm *vm)
{
	EEL_object *mo;
	EEL_state *es = VMP->state;
	int hits = 0;
	int i;
	if(es->module_lock)
	{
		DBG12(fprintf(stderr, "Cannot clean modules! "
				"module_lock = %d\n", es->module_lock);)
		return 0;
	}
	++es->module_lock;

	/*
	 * See if we can get rid of any of the dead modules...
	 */
	DBG12(fprintf(stderr, "Cleaning deadmods...\n");)
	mo = es->deadmods;
	while(mo)
	{
		EEL_object *nmo = mo->lnext;
#if DBG12(1)+0 == 1
		EEL_module *m = o2EEL_module(mo);
		fprintf(stderr, "  %p (next: %p)\n", mo, nmo);
		if(m->exports)
			fprintf(stderr, "  Trying %s\n",
					eel_table_getss(m->exports, "__modname"));
		else
			fprintf(stderr, "  Trying %p\n", mo);
#endif
		if(m_real_destruct(mo) != EEL_XREFUSE)
		{
			eel_limbo_unlink(mo);
			eel_kill_weakrefs(mo);
			eel_o_free(mo);
			++hits;
			DBG12(fprintf(stderr, "    Deleted!\n");)
		}
		DBG12(else
			fprintf(stderr, "    Refused!\n");)
		mo = nmo;
	}

	/*
	 * Delete any entries with nil references. (Dead weakrefs, that is.)
	 */
	DBG12(fprintf(stderr, "Cleaning modules...\n");)
	for(i = 0; i < eel_length(es->modules); )
	{
		EEL_tableitem *ti;
		EEL_value *v;
		ti = eel_table_get_item(es->modules, i);
		v = eel_table_get_value(ti);
		if(v->type == EEL_TNIL)
			eel_table_delete(es->modules, eel_table_get_key(ti));
		else
			++i;
	}
	DBG12(fprintf(stderr, "Cleaning done!\n");)
	--es->module_lock;

	return hits;
}


void eel_unlock_modules(EEL_state *es)
{
	--es->module_lock;
	if(!es->module_lock)
		eel_clean_modules(es->vm);
}


/*----------------------------------------------------------
	Convenience functions
----------------------------------------------------------*/

const char *eel_module_modname(EEL_object *m)
{
	if(!m)
		return "<nil module pointer>";
	if((EEL_classes)m->type != EEL_CMODULE)
		return "<not a module>";
	return eel_table_getss(o2EEL_module(m)->exports, "__modname");
}


const char *eel_module_filename(EEL_object *m)
{
	if(!m)
		return "<nil module pointer>";
	if((EEL_classes)m->type != EEL_CMODULE)
		return "<not a module>";
	return eel_table_getss(o2EEL_module(m)->exports, "__filename");
}


/*----------------------------------------------------------
	Module instance data management
----------------------------------------------------------*/

void *eel_get_moduledata(EEL_object *mo)
{
	EEL_module *m;
	if(!mo)
		return NULL;
	if((EEL_classes)mo->type != EEL_CMODULE)
		return NULL;
	m = o2EEL_module(mo);
	return m->moduledata;
}

void *eel_get_current_moduledata(EEL_vm *vm)
{
/*
FIXME: Do some benchmarking on this! The problem is that passing moduledata
FIXME: directly as an argument to all C functions means the VM wastes cycles
FIXME: preparing this pointer for lots of C functions that don't need it.
FIXME: OTOH, the current method penalizes C functions that do need moduledata,
FIXME: and the function call overhead is hard to avoid without exposing VM
FIXME: internals in the API.
*/
	EEL_object *o = eel__current_function(vm);
	o = o2EEL_function(o)->common.module;
	return o2EEL_module(o)->moduledata;
}
