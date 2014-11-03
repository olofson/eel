/*
---------------------------------------------------------------------------
	e_state.c - EEL State (Compiler, VM, symbols etc)
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

#include <string.h>
#include "ec_symtab.h"
#include "e_state.h"
#include "e_builtin.h"
#include "e_object.h"
#include "e_class.h"
#include "e_string.h"
#include "e_function.h"
#include "e_array.h"
#include "e_table.h"
#include "e_vector.h"
#include "e_dstring.h"
#include "eel_system.h"
#include "eel_io.h"
#include "eel_dir.h"
#include "eel_math.h"
#include "eel_dsp.h"
#include "e_sharedstate.h"


/*----------------------------------------------------------
	EEL Engine State
----------------------------------------------------------*/

static void es_close(EEL_state *es);

#define XCHECK(xx)	x = (xx);		\
			if(x)			\
				return x;

static EEL_xno array_sadd(EEL_object *a, const char *s)
{
	EEL_xno x;
	EEL_value v;
	eel_s2v(a->vm, &v, s);
	if(v.type == EEL_TNIL)
		return EEL_XMEMORY;
	x = eel_setlindex(a, eel_length(a), &v);
	if(x)
		return x;
	eel_v_disown(&v);
	return 0;
}

static EEL_xno init_env_table(EEL_state *es)
{
	EEL_value a, v;
	EEL_xno x = eel_o_construct(es->vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
	{
		eel_msg(es, EEL_EM_IERROR, "Could not create"
				" 'environment' table!\n");
		return x;
	}
	es->environment = v.objref.v;
	SETNAME(es->environment, "'environment' Table");

	/* Module paths */
	XCHECK(eel_o_construct(es->vm, EEL_CARRAY, NULL, 0, &a));
	XCHECK(array_sadd(a.objref.v, "."));
	XCHECK(array_sadd(a.objref.v, "./modules"));
	XCHECK(array_sadd(a.objref.v, EEL_MODULE_DIR));
	XCHECK(array_sadd(a.objref.v, ""));
	XCHECK(eel_setsindex(es->environment, "path_modules", &a));
	eel_disown(a.objref.v);
	return 0;
}


EEL_vm *eel_open(int argc, const char *argv[])
{
#ifdef DEBUG
	int i;
#endif
	EEL_xno x;
	EEL_value v;
	EEL_vm *vm;
	EEL_state *es = (EEL_state *)calloc(1, sizeof(EEL_state));
	if(!es)
		return NULL;

	if(eel_add_api_user() != EEL_XOK)
		return NULL;

	if(eel_sbuffer_open(es))
	{
		eel_msg(es, EEL_EM_IERROR, "Could not allocate"
				" compiler API string buffers!\n");
		es_close(es);
		return NULL;
	}

	es->last_module_id = -1;

	vm = es->vm = eel_vm_open(es, EEL_INITHEAP);
	if(!vm)
	{
		eel_msg(es, EEL_EM_IERROR, "Could not initialize"
				" virtual machine!\n");
		es_close(es);
		return NULL;
	}

	x = eel_cast_open(es);
	if(x)
	{
		eel_msg(es, EEL_EM_IERROR, "Could not initialize"
				" type casting subsystem!\n");
		es_close(es);
		return NULL;
	}

	/* Built-in classes */
	eel_try(es)
	{
		EEL_symbol *s;

		/* Bootstrap the class system */
		eel_register_class(vm, EEL_COBJECT, "object", -1,
				NULL, NULL, NULL);
		eel_cclass_register(vm);
		eel_cstring_register(vm);
		eel_o_own(es->classes[EEL_CCLASS]);	/* object is a class */

		/* Set up symbol table. (Needs CSTRING!) */
		es->root_symtab = eel_s_add(es, NULL, "root_namespace",
				EEL_SBODY);
		if(!es->root_symtab)
		{
			eel_msg(es, EEL_EM_IERROR, "Could not add"
					" root_namespace symbol!\n");
			es_close(es);
			eel_done(es);
			return NULL;
		}

		/* Name the classes that were installed before CSTRING */
		eel_register_class(vm, EEL_COBJECT, "object", -1,
				NULL, NULL, NULL);
		eel_cclass_register(vm);

		/*
		 * Add 'string' to the compiler symbol table,
		 * as that couldn't be done before. (No symbol table! :-)
		 */
		s = eel__register(vm, "string", EEL_SCLASS);
		if(s)
		{
			s->v.object = es->classes[EEL_CSTRING];
			eel_o_own(s->v.object);
			eel_register_essx(vm, EEL_CSTRING, s);
		}

		/* Register (other) base classes */
		eel_register_class(vm, EEL_CVALUE, "value", -1,
				NULL, NULL, NULL);

		/* Register value "classes" */
		eel_register_class(vm, EEL_TNIL, "\001nil", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TREAL, "real", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TINTEGER, "integer", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TBOOLEAN, "boolean", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TTYPEID, "typeid", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TOBJREF, "\001objref", EEL_CVALUE,
				NULL, NULL, NULL);
		eel_register_class(vm, EEL_TWEAKREF, "\001weakref", EEL_CVALUE,
				NULL, NULL, NULL);

		/* Register the real classes */
		eel_cfunction_register(vm);
		eel_cmodule_register(vm);
		eel_carray_register(vm);
		eel_ctable_register(vm);
		eel_cvector_register(vm);
		eel_cdstring_register(vm);
	}
	eel_except
	{
		eel_msg(es, EEL_EM_IERROR, "Could not register"
				" built-in classes!\n");
		es_close(es);
		return NULL;
	}

	eel_cast_init(es);

	/* Module name table for circular import detection */
	x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
	{
		eel_msg(es, EEL_EM_IERROR, "Could not create"
				" module name table!\n");
		es_close(es);
		return NULL;
	}
	es->modnames = v.objref.v;
	SETNAME(es->modnames, "Module Name Table");

	/* Module table */
	x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
	{
		eel_msg(es, EEL_EM_IERROR, "Could not create module table!\n");
		es_close(es);
		return NULL;
	}
	es->modules = v.objref.v;
	SETNAME(es->modules, "Shared Module Table");

	/* 'environment' table */
	if(init_env_table(es))
	{
		es_close(es);
		return NULL;
	}

	/* Directives */
	eel_register_keyword(vm, "include", TK_KW_INCLUDE);
	eel_register_keyword(vm, "import", TK_KW_IMPORT);
	eel_register_keyword(vm, "as", TK_KW_AS);
	eel_register_keyword(vm, "end", TK_KW_END);
	eel_register_keyword(vm, "eelversion", TK_KW_EELVERSION);

	/* Flow control keywords */
	eel_register_keyword(vm, "return", TK_KW_RETURN);
	eel_register_keyword(vm, "if", TK_KW_IF);
	eel_register_keyword(vm, "else", TK_KW_ELSE);
	eel_register_keyword(vm, "switch", TK_KW_SWITCH);
	eel_register_keyword(vm, "case", TK_KW_CASE);
	eel_register_keyword(vm, "default", TK_KW_DEFAULT);
	eel_register_keyword(vm, "for", TK_KW_FOR);
	eel_register_keyword(vm, "do", TK_KW_DO);
	eel_register_keyword(vm, "while", TK_KW_WHILE);
	eel_register_keyword(vm, "until", TK_KW_UNTIL);
	eel_register_keyword(vm, "break", TK_KW_BREAK);
	eel_register_keyword(vm, "continue", TK_KW_CONTINUE);
	eel_register_keyword(vm, "repeat", TK_KW_REPEAT);

	eel_register_keyword(vm, "try", TK_KW_TRY);
	eel_register_keyword(vm, "untry", TK_KW_UNTRY);
	eel_register_keyword(vm, "except", TK_KW_EXCEPT);
	eel_register_keyword(vm, "throw", TK_KW_THROW);
	eel_register_keyword(vm, "retry", TK_KW_RETRY);

	/* Qualifier keywords */
	eel_register_keyword(vm, "local", TK_KW_LOCAL);
	eel_register_keyword(vm, "static", TK_KW_STATIC);
	eel_register_keyword(vm, "upvalue", TK_KW_UPVALUE);
	eel_register_keyword(vm, "export", TK_KW_EXPORT);
	eel_register_keyword(vm, "shadow", TK_KW_SHADOW);
	eel_register_keyword(vm, "constant", TK_KW_CONSTANT);

	/* Declaration keywords */
	eel_register_keyword(vm, "procedure", TK_KW_PROCEDURE);

	/* Special values */
	eel_register_keyword(vm, "true", TK_KW_TRUE);
	eel_register_keyword(vm, "false", TK_KW_FALSE);
	eel_register_keyword(vm, "nil", TK_KW_NIL);

	/* Special variables and operators */
	eel_register_keyword(vm, "arguments", TK_KW_ARGUMENTS);
	eel_register_keyword(vm, "tuples", TK_KW_TUPLES);
	eel_register_keyword(vm, "specified", TK_KW_SPECIFIED);
	eel_register_keyword(vm, "exception", TK_KW_EXCEPTION);

	/* Unary operators */
	eel_register_unop(vm, "-", EEL_OP_NEG, 100, 0);
	eel_register_unop(vm, "typeof", EEL_OP_TYPEOF, 100, 0);
	eel_register_unop(vm, "sizeof", EEL_OP_SIZEOF, 100, 0);
	eel_register_unop(vm, "clone", EEL_OP_CLONE, 100, 0);

	/* Unary bitwise operations */
	eel_register_unop(vm, "~", EEL_OP_BNOT, 100, 0);

	/* Unary boolean operators */
	eel_register_unop(vm, "not", EEL_OP_NOT, 25, 0);

	/* Arithmetics */
	eel_register_binop(vm, "**", EEL_OP_POWER, 90, EOPF_RIGHT);
	eel_register_binop(vm, "%", EEL_OP_MOD, 70, 0);
	eel_register_binop(vm, "/", EEL_OP_DIV, 70, 0);
	eel_register_binop(vm, "*", EEL_OP_MUL, 70, 0);
	eel_register_binop(vm, "-", EEL_OP_SUB, 60, 0);
	eel_register_binop(vm, "+", EEL_OP_ADD, 60, 0);

	/* Vector arithmetics */
	eel_register_binop(vm, "#**", EEL_OP_VPOWER, 90, EOPF_RIGHT);
	eel_register_binop(vm, "#%", EEL_OP_VMOD, 70, 0);
	eel_register_binop(vm, "#/", EEL_OP_VDIV, 70, 0);
	eel_register_binop(vm, "#*", EEL_OP_VMUL, 70, 0);
	eel_register_binop(vm, "#-", EEL_OP_VSUB, 60, 0);
	eel_register_binop(vm, "#+", EEL_OP_VADD, 60, 0);

	/* Bitwise operators */
	eel_register_binop(vm, "&", EEL_OP_BAND, 70, 0);
	eel_register_binop(vm, "|", EEL_OP_BOR, 60, 0);
	eel_register_binop(vm, "^", EEL_OP_BXOR, 60, 0);
	eel_register_binop(vm, "<<", EEL_OP_SHL, 55, 0);
	eel_register_binop(vm, ">>", EEL_OP_SHR, 55, 0);
	eel_register_binop(vm, "rol", EEL_OP_ROL, 55, 0);
	eel_register_binop(vm, "ror", EEL_OP_ROR, 55, 0);
	eel_register_binop(vm, "><", EEL_OP_BREV, 55, 0);

	/* Selectors */
	eel_register_binop(vm, "|<", EEL_OP_MIN, 50, 0);
	eel_register_binop(vm, ">|", EEL_OP_MAX, 50, 0);

	/* Comparisons */
	eel_register_binop(vm, "<", EEL_OP_LT, 40, EOPF_NOSHORT);
	eel_register_binop(vm, "<=", EEL_OP_LE, 40, EOPF_NOSHORT);
	eel_register_binop(vm, ">", EEL_OP_GT, 40, EOPF_NOSHORT);
	eel_register_binop(vm, ">=", EEL_OP_GE, 40, EOPF_NOSHORT);
	eel_register_binop(vm, "==", EEL_OP_EQ, 30, EOPF_NOSHORT);
	eel_register_binop(vm, "!=", EEL_OP_NE, 30, EOPF_NOSHORT);

	/* Boolean operators */
	eel_register_binop(vm, "and", EEL_OP_AND, 20, 0);
	eel_register_binop(vm, "or", EEL_OP_OR, 10, 0);
	eel_register_binop(vm, "xor", EEL_OP_XOR, 10, 0);

	/* Weak reference assignment */
	eel_register_binop(vm, "(=)", EEL_OP_WKASSN, 0, 0);

	/* Other operators */
	eel_register_binop(vm, "in", EEL_OP_IN, 110, 0);
#ifdef DEBUG
	for(i = 0; i < ESS_TOKENS; ++i)
		if(!es->tokentab[i])
		{
			eel_msg(es, EEL_EM_IERROR, "ESS token %d not defined!\n"
					"(ESS_FIRST_TK: %d, ESS_LAST_TK: %d, "
					"ESS_TOKENS: %d)\n", i, ESS_FIRST_TK,
					ESS_LAST_TK, ESS_TOKENS);
			es_close(es);
			return NULL;
		}
#endif
	eel_try(es)
		context_open(es);
	eel_except
	{
		eel_msg(es, EEL_EM_IERROR,
				"Could not open compiler start context!\n");
		es_close(es);
		return NULL;
	}

	/* Install system module */
	if(eel_system_init(vm, argc, argv))
	{
		eel_msg(es, EEL_EM_IERROR,
				"Could not initialize built-in system module!\n");
		es_close(es);
		return NULL;
	}

	/* Install io module */
	if(eel_io_init(vm))
	{
		eel_msg(es, EEL_EM_IERROR,
				"Could not initialize built-in io module!\n");
		es_close(es);
		return NULL;
	}


	/*
	 * Compile EEL built-in library
	 *
	 * NOTE: We need to inject 'io' and 'system' before 'eelbil' now, as
	 *       'eelbil' needs them for module loading!
	 */
	if(eel_builtin_init(vm))
	{
		eel_perror(vm, 1);
		eel_msg(es, EEL_EM_IERROR,
				"Could not initialize built-in library!\n");
		es_close(es);
		return NULL;
	}

	/* Install math module */
	if(eel_math_init(vm))
	{
		eel_msg(es, EEL_EM_IERROR,
				"Could not initialize built-in math module!\n");
		es_close(es);
		return NULL;
	}

	/* Install directory module */
	if(eel_dir_init(vm))
	{
		eel_msg(es, EEL_EM_IERROR, "Could not initialize built-in"
				" directory module!\n");
		es_close(es);
		return NULL;
	}

	/* Install DSP module */
	if(eel_dsp_init(vm))
	{
		eel_msg(es, EEL_EM_IERROR,
				"Could not initialize built-in DSP module!\n");
		es_close(es);
		return NULL;
	}

	return es->vm;
}


void eel_close(EEL_vm *vm)
{
	EEL_state *es;
	if(!vm)
		return;
	es = VMP->state;
	if(es)
		es_close(es);
}


static void eel__unstrap(EEL_state *es)
{
	EEL_classdef *cd;
	int crc = es->classes[EEL_CCLASS]->refcount;
	int src = es->classes[EEL_CSTRING]->refcount;
	if((crc != 2) || (src != 2))
	{
		eel_msg(es, EEL_EM_VMWARNING,
				"Classes CCLASS and CSTRING refcounts "
				"are %d and %d respectively,\n"
				"while they should both be 2 at this "
				"point. This means there is\n"
				"a memory leak somewhere (EEL or script "
				"bug), and as a result,\n"
				"EEL cannot safely clean up properly.",
				crc, src);
		eel_perror(es->vm, 1);
		return;
	}

	/* Kill CCLASS name -> CSTRING -> CCLASS cycle */
	cd = o2EEL_classdef(es->classes[EEL_CCLASS]);
	eel_o_disown(&cd->name);

	/*
	 * Now, we destroy CSTRING's name, and as a result,
	 * CSTRING no longer needs itself, thus commits suicide,
	 * and as a side effect, no longer needs CCLASS.
	 */
	cd = o2EEL_classdef(es->classes[EEL_CSTRING]);
	eel_o_disown(&cd->name);

	/*
	 * Finally, terminate CCLASS' ownership of itself.
	 * (*_nz() because debug code will prevent deletion
	 * of classes not in the class table.)
	 */
	eel_o_disown_nz(es->classes[EEL_CCLASS]);
	es->classes[EEL_CCLASS] = NULL;
}


#if DBGM(1)+0 == 1
static void dump_owners(EEL_state *es, EEL_object *o, const char *pre)
{
/*
TODO: Find all objects, analyze references and present as a tree or something.
*/
}

static void dump_elements(EEL_state *es, EEL_object *o)
{
#if 0
/*
FIXME: We need to implement this on a lower level, avoiding the 'classes'
FIXME: array and things like that, or it won't actually work when we want to
FIXME: use it: AFTER the VM has tried to clean up!
*/
	int i;
	int len = eel_length(o);
	if(len <= 0)
		return;
	switch(o->type)
	{
	  case EEL_CSTRING:
	  case EEL_CDSTRING:
		return;
	  default:
		break;
	}
	for(i = 0; i < len; ++i)
	{
		const char *s;
		switch(o->type)
		{
		  case EEL_CTABLE:
		  {
			EEL_tableitem *ti;
			ti = eel_table_get_item(o, i);
			if(ti)
			{
				const char *is = eel_v_stringrep(es->vm,
						eel_table_get_key(ti));
				s = eel_v_stringrep(es->vm,
						eel_table_get_value(ti));
				printf("           %s: %s\n", is, s);
				eel_sfree(es, s);
				eel_sfree(es, is);
			}
			continue;
		  }
		  default:
		  {
			EEL_value v;
			if(eel_getlindex(o, i, &v) == EEL_XNONE)
			{
				s = eel_v_stringrep(es->vm, &v);
				printf("           %d: %s\n", i, s);
				eel_sfree(es, s);
			}
			continue;
		  }
		}
	}
#endif
}
#endif

static void es_close(EEL_state *es)
{
	DBGM(EEL_object *o;)
	EEL_vm *vm = es->vm;
	int i;
	if(!vm)
		return;
	VMP->is_closing = 1;
	DBGK2(printf("eel_close(): Running $.cleanup and deleting environment.\n");)
	if(es->eellib)
		eel_callnf(vm, es->eellib, "__cleanup", NULL);
	DBGK2(printf("eel_close(): Garbage collecting remaining modules.\n");)
	while(eel_clean_modules(vm))
		;
	/*
	 * The compiler must be closed before trying to unload the
	 * built-in library, because some exports are "hardwired"
	 * into the root symbol table!
	 */
	DBGK2(printf("eel_close(): Closing compiler contexts.\n");)
	context_close(es);
	DBGK2(printf("eel_close(): Destroying symbol table.\n");)
	if(es->root_symtab)
	{
		eel_s_free(es, es->root_symtab);
		es->root_symtab = NULL;
	}
	DBGK2(printf("eel_close(): Unloading EEL built-in library.\n");)
	if(es->eellib)
		eel_o_disown(&es->eellib);
	DBGK2(printf("eel_close(): Destroying 'environment' table.\n");)
	if(es->environment)
		eel_o_disown(&es->environment);
	DBGK2(printf("eel_close(): Destroying shared module table.\n");)
	if(es->modules)
		eel_o_disown(&es->modules);
	DBGK2(printf("eel_close(): Destroying module name table.\n");)
	if(es->modnames)
		eel_o_disown(&es->modnames);
	DBGK2(printf("eel_close(): Clearing message log.\n");)
	eel_clear_errors(es);
	eel_clear_warnings(es);
	DBGK2(printf("eel_close(): Lexer cleanup.\n");)
	eel_lexer_cleanup(es);
#if 0	/* This sometimes segfaults. Bug or expected? */
# if DBGM(1)+0 == 1
	DBGK2(printf("eel_close(): Eliminating circular references.\n");)
	o = VMP->afirst;
	while(o)
	{
		int dest = VMP->destroyed;
		eel_o__metamethod(o, EEL_MM_DELETE, NULL, NULL);
		if(dest == VMP->destroyed)
			o = o2dbg(o)->dnext;
		else
			o = VMP->afirst;
	}
# endif
#endif
	DBGK2(printf("eel_close(): VM Cleanup.\n");)
	eel_vm_cleanup(vm);
	DBGK2(printf("eel_close(): Destroying built-in classes.\n");)
	if(es->classes)
	{
		/*
		 * Destroy all classes.
		 * (Though CCLASS and CSTRING will stick around...)
		 */
		DBGK2(printf("eel_close(): Unregistering classes.\n");)
		for(i = es->nclasses - 1; i >= 0; --i)
			if(es->classes[i])
				eel_unregister_class(es->classes[i]);

		/*
		 * Unstrap CSTRING and CCLASS.
		 */
		DBGK2(printf("eel_close(): Unstrapping CSTRING and CCLASS.\n");)
		eel__unstrap(es);

		/*
		 * Get rid of the class array.
		 */
		DBGK2(printf("eel_close(): Destroying class array.\n");)
		eel_free(vm, es->classes);
		es->classes = NULL;
	}
#if DBG6B(1)+0 == 1
	printf("    VM instructions: %d\n", VMP->instructions);
#endif
#if DBGM(1)+0 == 1
	printf("      Refcount incs: %d\n", VMP->owns);
	printf("      Refcount decs: %d\n", VMP->disowns);
	printf("    Objects created: %d\n", VMP->created);
	printf("  Objects destroyed: %d\n", VMP->destroyed);
	printf("  Objects zombified: %d\n", VMP->zombified);
	printf("Objects resurrected: %d\n", VMP->resurrected);
	printf("     Objects leaked: %d", VMP->created - VMP->destroyed);
	if(VMP->afirst)
	{
		o = VMP->afirst;
		printf(":\n\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/"
			"\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\n");
		while(o)
		{
			const char *s = eel_o_stringrep(o);
			printf("        %s", s);
			if(o == o->lprev)
				printf("  Self-circular through 'lprev'!");
			if(o == o->lnext)
				printf("  Self-circular through 'lnext'!");
			printf("\n");
			eel_sfree(es, s);
			dump_owners(es, o, "          ");
			dump_elements(es, o);
			o = o2dbg(o)->dnext;
		}
		printf("\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/"
			"\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\n");
	}
	else
		printf("\n");
#endif
	DBGK2(printf("eel_close(): Closing type casting subsystem.\n");)
	eel_cast_close(es);
	DBGK2(printf("eel_close(): Closing VM.\n");)
	eel_vm_close(vm);
	es->vm = NULL;
	DBGK2(printf("eel_close(): Freeing sbuffers.\n");)
	eel_sbuffer_close(es);
	DBGK2(printf("eel_close(): Freeing state struct.\n");)
	free(es);
	eel_remove_api_user();
}


char *eel_unique(EEL_vm *vm, const char *base)
{
	char *buf = eel_salloc(VMP->state);
	if(!base)
		base = "__sym";
	snprintf(buf, EEL_SBUFSIZE, "%s%.4d", base, VMP->state->unique++);
	return buf;
}


/*----------------------------------------------------------
	Compiler exception handling
----------------------------------------------------------*/

#if DBGX(1)+0 == 1
static int xlevel(EEL_state *es)
{
	EEL_jumpbuf *jb;
	int x = 0;
	if(!es)
		return 0;
	jb = es->jumpbufs;
	while(jb)
		++x, jb = jb->prev;
	return x;
}
#endif

void eel__try(EEL_state *es)
{
	EEL_jumpbuf *jb = (EEL_jumpbuf *)malloc(sizeof(EEL_jumpbuf));
	if(!jb)
	{
		fprintf(stderr, "INTERNAL ERROR: Could not set up "
				"error exception handling!\n");
		eel_cthrow(es);
	}
	jb->contexts = 0;
	jb->prev = es->jumpbufs;
	es->jumpbufs = jb;
	DBGX(printf("----- try (%d) -----\n", xlevel(es));)
}


void eel_done(EEL_state *es)
{
	EEL_jumpbuf *jb = es->jumpbufs;
	DBGX(printf("----- done (%d) -----\n", xlevel(es));)
	if(!jb)
	{
		fprintf(stderr, "INTERNAL ERROR: Too many eel_done() calls!\n");
		eel_cthrow(es);
	}
	es->jumpbufs = jb->prev;
	free(jb);
}


void eel__cthrow(EEL_state *es)
{
	DBGX(printf("----- throw (%d) -----\n", xlevel(es));)
	if(es && es->jumpbufs)
	{
		jmp_buf buf;
		while(es->jumpbufs->contexts)
			eel_context_pop(es);
		memcpy(buf, es->jumpbufs->buf, sizeof(jmp_buf));
		eel_done(es);
		eel_longjmp(buf, 1);
	}
	else
	{
		fprintf(stderr, "INTERNAL ERROR: Unhandled compiler"
				" exception!\n");
		eel_perror(es->vm, 1);
		abort();
	}
}
