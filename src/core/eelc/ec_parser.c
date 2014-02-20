/*
---------------------------------------------------------------------------
	ec_parser.c - The EEL Parser/Compiler
---------------------------------------------------------------------------
 * Copyright 2002-2006, 2009-2012, 2014 David Olofson
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

#include <stdlib.h>
#include <stdio.h>
#include "ec_parser.h"
#include "ec_symtab.h"
#include "ec_mlist.h"
#include "ec_context.h"
#include "e_state.h"
#include "e_class.h"
#include "e_function.h"
#include "e_module.h"
#include "e_string.h"
#include "e_object.h"
#include "e_table.h"

/*
 * True if we're NOT supposed to warn when the old operator precedence would
 * have kicked in.
 */
#define	EEL_NOPRECEDENCE(x)	((x)->context->bio->noprecedence)

#if DBGH2(1)+0 == 1
static inline int eel__tk(const char *tkn, int tkc, int line)
{
	if(TK_WRONG == tkc)
		return tkc;
	printf("Parsed %s; line %d.\n", tkn, line);
	return tkc;
}
#  define TK(x)		eel__tk(#x, TK_##x, __LINE__)
#  define TK2(x, y)	eel__tk(x, y, __LINE__)
#else
#  define TK(x)		TK_##x
#  define TK2(x, y)	y
#endif

static int expression2(EEL_state *es, int limit, EEL_mlist *al, int wantresult);
static int expression(EEL_state *es, EEL_mlist *al, int wantresult);
static int body(EEL_state *es, int flags);
static int argdeflist(EEL_state *es, EEL_varkinds kind);
static int block(EEL_state *es);
static int statement(EEL_state *es, int flags);

static void add_export(EEL_object *mo, EEL_value *xv, const char *name);


/*----------------------------------------------------------
	Parser helper functions
----------------------------------------------------------*/

/* Verify that a class exists */
static void check_class(EEL_state *es, EEL_classes type)
{
	EEL_object *c;
	if(type >= es->nclasses)
		eel_cerror(es, "Undefined class! (Type ID %d)", type);
	if(type < 0)
		eel_cerror(es, "Illegal type ID! (Type ID %d)", type);
	c = es->classes[type];
	if(!c)
		eel_cerror(es, "Undefined class! (Type ID %d)", type);
}


/* Verify that a class can construct instances */
static void check_constructor(EEL_state *es, EEL_classes type)
{
	EEL_classdef *cd;
	check_class(es, type);
	cd = o2EEL_classdef(es->classes[type]);
	if(!cd->construct)
		eel_cerror(es, "Class '%s' does not have a constructor!",
				eel_typename(es->vm, type));
}


/* Check that 'al' has at least 'min' and at most 'max' arguments. */
static void check_argc(EEL_mlist *al, int min, int max)
{
	EEL_state *es = al->coder->state;
	if(min > 0)
		if(al->length < min)
			eel_cerror(es, "Too few arguments! "
					"(Got %d; expected at least %d)",
					al->length, min);
	if(max > 0)
		if(al->length > max)
			eel_cerror(es, "Too many arguments!"
					"(Got %d; expected at most %d)",
					al->length, max);
}


/* Make sure we have 'token' as the current token. */
static void expect(EEL_state *es, int token, const char *errmsg, ...)
{
	if(es->token != token)
	{
		if(errmsg)
		{
			va_list args;
			va_start(args, errmsg);
			eel_vmsg_src(es, EEL_EM_CERROR, errmsg, args);
			eel_cthrow(es);
		}
		else
		{
			if((es->token >= ' ') && (es->token < 127))
				eel_cerror(es, "Expected '%c' rather than '%c'.",
						token, es->token);
			else
				eel_cerror(es, "Expected '%c'.", token);
		}
	}
	eel_lex(es, 0);
}


/*
 * Generate code to prepare to leave specified context,
 * and any contexts below it.
 */
static void code_leave_context(EEL_state *es, EEL_context *ctx)
{
	EEL_coder *cdr = es->context->coder;
	if(!eel_keep_variables(ctx) &&
			(eel_initializations(ctx) >
			eel_initializations(ctx->previous)))
		eel_codeA(cdr, EEL_OCLEAN_A,
				eel_initializations(ctx->previous));
}


/*
 * Find nearest context with specified flag(s) set
 * Returns NULL if there is no such context below the top level.
 */
static EEL_context *find_context_flags(EEL_state *es, int flags)
{
	EEL_context *ctx = es->context;
	while(ctx->previous)	/* Root context is reserved - don't touch! */
	{
		if(ctx->flags & flags)
			return ctx;
		ctx = ctx->previous;
	}
	return NULL;
}


/*
 * Break out of the specified block (NULL = nearest breakable)
 */
static void code_break(EEL_state *es, EEL_context *ctx)
{
	int jump_out;
	EEL_codemark *cm;
	EEL_cestate xs;
	if(!ctx)
		ctx = find_context_flags(es, ECTX_BREAKABLE);
	if(!ctx)
		eel_cerror(es, "'break' outside breakable context!");
	
	xs = EEL_ENOT(eel_test_exit(es));
#if defined(EEL_DEAD_CODE_ELIMINATION) && !defined(EEL_DEAD_CODE_ILLEGAL)
	if(xs == EEL_ENO)
		return;
#endif
	/* Jump to end of context */
	DBGE(printf("Breaking out of context %p\n", ctx);)
	code_leave_context(es, ctx);
	jump_out = eel_codesAx(es->context->coder, EEL_OJUMP_sAx, 0);
	cm = (EEL_codemark *)malloc(sizeof(EEL_codemark));
	if(!cm)
		eel_ierror(es, "Could not create codemark for 'break' jump!");
	cm->pos = jump_out;
	cm->xstate = xs;
	ej_link(ctx, cm);
	eel_e_break(es, ctx);
}


/* Jump to the start of the specified repeatable context. */
static void code_repeat(EEL_state *es, EEL_context *ctx)
{
	int jmp;
	DBGE(printf("Repeating context %p\n", ctx);)
	code_leave_context(es, ctx);
	jmp = eel_codesAx(es->context->coder, EEL_OJUMP_sAx, 0);
	eel_code_setjump(es->context->coder, jmp, ctx->startpos);
	eel_e_break(es, ctx);
}

/* Skip the rest of the 'for' body and go for the next iteration */
static void code_next(EEL_state *es, EEL_context *ctx)
{
	int jump_out;
	EEL_codemark *cm;
	EEL_cestate xs = EEL_ENOT(eel_test_exit(es));
#if defined(EEL_DEAD_CODE_ELIMINATION) && !defined(EEL_DEAD_CODE_ILLEGAL)
	if(xs == EEL_ENO)
		return;
#endif
	DBGE(printf("Continuing 'for' loop %p\n", ctx);)
	code_leave_context(es, ctx);
/*
FIXME: We could make room for LOOP a instruction and an end jump instead,
FIXME: to avoid an extra jump when continuing.
*/
	jump_out = eel_codesAx(es->context->coder, EEL_OJUMP_sAx, 0);
	cm = (EEL_codemark *)malloc(sizeof(EEL_codemark));
	if(!cm)
		eel_ierror(es, "Could not create codemark for 'continue' jump!");
	cm->pos = jump_out;
	cm->xstate = xs;
	cj_link(ctx, cm);
	eel_e_break(es, ctx);
}


/* Fix any jumps ('for' continues) to the loop test */
static void code_fixup_continuations(EEL_state *es)
{
	EEL_context *c = es->context;
	EEL_coder *cdr = c->coder;
	int pos = eel_code_target(cdr);
	DBGD(printf("  Now fixing up continue jumps...\n");)
	while(c->contjumps)
	{
		EEL_codemark *cm = c->contjumps;
		DBGD(if(cm->pos >= 0)
			printf("    %d: JUMP PC + %d\n", cm->pos, pos);
		else
			printf("(DEAD: %d: JUMP PC + %d)\n", -cm->pos, pos);)
		eel_code_setjump(cdr, cm->pos, pos);
		eel_e_target(es, cm->xstate);
		cj_unlink(c, cm);
		free(cm);
	}
	DBGD(printf("  Done!\n");)
}


/* Fix any jumps (breaks) to the end of the context */
static void code_fixup_breaks(EEL_state *es)
{
	EEL_context *c = es->context;
	EEL_coder *cdr = c->coder;
	int pos = eel_code_target(cdr);
	DBGD(printf("  Fixing up end jumps...\n");)
	while(c->endjumps)
	{
		EEL_codemark *cm = c->endjumps;
		DBGD(if(cm->pos >= 0)
			printf("    %d: JUMP PC + %d\n", cm->pos, pos);
		else
			printf("(DEAD: %d: JUMP PC + %d)\n", -cm->pos, pos);)
		eel_code_setjump(cdr, cm->pos, pos);
		eel_e_target(es, cm->xstate);
		ej_unlink(c, cm);
		free(cm);
	}
	DBGD(printf("  Done!\n");)
}


/* Move any end jumps (breaks) to the previous context */
static void code_move_breaks_up(EEL_state *es)
{
	EEL_context *c = es->context;
	DBGD(EEL_coder *cdr = c->coder;)
	DBGD(printf("  Moving up end jumps...\n");)
	while(c->endjumps)
	{
		EEL_codemark *cm = c->endjumps;
		DBGD(printf("    %d: JUMP PC + %d\n", cm->pos, cdr->pos);)
		ej_unlink(c, cm);
		ej_link(c->previous, cm);
	}
	DBGD(printf("  Done!\n");)
}


/* Return from procedure */
static void procreturn(EEL_state *es)
{
	EEL_function *f = o2EEL_function(es->context->coder->f);
	int flags = f->common.flags;
	DBGE(printf("Returning from context %p\n", es->context);)
	if((flags & EEL_FF_RESULTS) && (eel_test_result(es) != EEL_EYES))
			eel_cerror(es, "Control reaches end of function that"
					" should return a value!");
	eel_code0(es->context->coder, EEL_ORETURN_0);
	eel_e_return(es);
}


/* filelist() item handler for include'. */
static void include_handler(EEL_state *es, int flags)
{
	EEL_object *m;
	if(es->include_depth > EEL_MAX_INCLUDE_DEPTH)
	{
		int id = es->include_depth;
		es->include_depth = -1;
		eel_cerror(es, "Circular include detected when trying to"
				" include file \"%s\"! (Depth: %d)",
				es->lval.v.s.buf, id);
	}

	m = eel_load_nc(es->vm, es->lval.v.s.buf, 0);
	if(!m)
		eel_cerror(es, "Couldn't include file \"%s\"! (Didn't load.)",
				es->lval.v.s.buf);
	o2EEL_module(m)->refsum = -1;

	DBGD(printf(">>>>>>>> including file '%s' >>>>>>>>\n", es->lval.v.s.buf);)

	eel_try(es)
		eel_context_push(es, ECTX_BLOCK, es->lval.v.s.buf);
	eel_except
	{
		eel_o_disown_nz(m);
		eel_cerror(es, "Couldn't include file \"%s\"!",
				es->lval.v.s.buf);
	}

	eel_try(es)
	{
		++es->include_depth;
		eel_lexer_start(es, m);
		if(block(es) != TK_EOF)
			eel_cerror(es, "Expected EOF!");
		--es->include_depth;
	}
	eel_except
	{
		eel_o_disown_nz(m);
		if(es->include_depth == -1)
			eel_cthrow(es);
		else
			eel_cerror(es, "Couldn't include file \"%s\"!",
					es->lval.v.s.buf);
	}

	eel_context_pop(es);

	DBGD(printf("<<<<<<<< leaving included file '%s' <<<<<<<<\n",
			eel_module_filename(m));)
	eel_o_disown_nz(m);
	eel_lexer_invalidate(es);
	eel_lex(es, 0);
}


/*
 * Add object 'o' to the 'objects' array of the current module.
 * The module will gain ownership of the object, and the module
 * will disown the object when the module is destroyed.
 */
static void add_object_to_module(EEL_state *es, EEL_object *o)
{
	EEL_module *m = o2EEL_module(es->context->module);
	EEL_object_p_da *a = &m->objects;
	if(eel_objs_setsize(es->vm, a, a->size + 1) < 0)
		eel_ierror(es, "Could not add object to module 'objects' list!");
	a->array[a->size] = o;
	eel_o_own(o);
	++a->size;
}


static void forward_exports(EEL_object *from, EEL_object *to, int symcheck)
{
	EEL_vm *vm = to->vm;
	EEL_state *es = VMP->state;
	int i, len;
	EEL_module *mm;
	EEL_object *im = eel_ps_new(vm, "__init_module");
	EEL_object *mn = eel_ps_new(vm, "__modname");
	EEL_object *fn = eel_ps_new(vm, "__filename");
	if(!im || !mn || !fn)
		eel_serror(es, "Could not create \"__init_module\" string!");
	mm = o2EEL_module(from);
	len = eel_length(mm->exports);
	for(i = 0; i < len; ++i)
	{
		EEL_tableitem *ti = eel_table_get_item(mm->exports, i);
		EEL_finder ef;
		EEL_symbol *s;
		EEL_value *k = eel_table_get_key(ti);
		EEL_value *v = eel_table_get_value(ti);
		DBG11(printf("Forwarding export '%s' (%s)...\n",
				eel_v2s(k), eel_v_stringrep(vm, v));)

		/* Key must be string! */
		if((EEL_classes)EEL_TYPE(k) != EEL_CSTRING)
			eel_ierror(es, "forward_exports() got a table "
					"item with a non-string name!");

		/* Don't forward any special module "private" stuff! */
		if(k->objref.v == im)
			continue;
		if(k->objref.v == mn)
			continue;
		if(k->objref.v == fn)
			continue;

		if(symcheck)
		{
			/* Name must not exist in current scope! */
			eel_finder_init(es, &ef, es->context->symtab,
					ESTF_NAME | ESTF_TYPES);
			ef.name = k->objref.v;
			ef.types = EEL_SMFUNCTION;
			s = eel_finder_go(&ef);
			if(s)
			{
				eel_o_disown_nz(im);
				eel_o_disown_nz(mn);
				eel_o_disown_nz(fn);
				eel_cerror(es, "Export '%s' from module "
						"\"%s\" (\"%s\") "
						" causes a conflict!",
						eel_o2s(s->name),
						eel_module_modname(from),
						eel_module_filename(from));
			}
		}

		add_export(to, v, eel_o2s(k->objref.v));
	}
	eel_o_disown_nz(im);
	eel_o_disown_nz(mn);
	eel_o_disown_nz(fn);
}


/* filelist() item handler for 'import'. */
/*
	import:
		  [KM_EXPORT] KW_IMPORT filelist
		| [KM_EXPORT] KW_IMPORT STRING KW_AS NAME
		;
*/
static void import_handler(EEL_state *es, int forward)
{
	char *modname = strdup(es->lval.v.s.buf);
	EEL_object *m = eel_import(es->vm, modname);
	if(!m)
	{
		char mn[256];
		snprintf(mn, sizeof(mn) - 1, "%s", modname);
		mn[sizeof(mn) - 1] = 0;
		free(modname);
		eel_cerror(es, "Couldn't import module \"%s\"!", mn);
	}
	eel_lex(es, 0);
	if(TK_KW_AS == es->token)
	{
		EEL_symbol *ns;
		eel_lex(es, 0);
		if(es->token != TK_NAME)
			eel_cerror(es, "Expected namespace name!");
		ns = eel_s_find(es, es->context->symtab, es->lval.v.s.buf,
				EEL_SNAMESPACE);
		if(ns)
			eel_cerror(es, "There already is a namespace '%s' "
					"in this context!", es->lval.v.s.buf);
		ns = eel_s_add(es, es->context->symtab, es->lval.v.s.buf,
				EEL_SNAMESPACE);
		eel_s_get_exports(ns, m);
		eel_lex(es, 0);
		if(forward)
			forward_exports(m, es->context->module, 1);
	}
	else
	{
		eel_s_get_exports(es->context->symtab, m);
		if(forward)
			forward_exports(m, es->context->module, 0);
	}
	eel_o_disown_nz(m);	/* Now held via the exports! */
	free(modname);
}


/*
 * Turn the current context into a function.
 * A suitable context must be in place!
 */
static int declare_func(EEL_state *es, const char *name, EEL_mlist *al,
		EEL_symbol *decl, int flags, const char *symname)
{
	EEL_xno x;
	int pflags;
	EEL_coder *nc;
	EEL_object *fo;
	EEL_function *f;
	EEL_symbol *st = es->context->symtab;

	if(es->context->coder && es->context->coder->f)
		pflags = o2EEL_function(es->context->coder->f)->common.flags;
	else
		pflags = 0;

	if(decl)
	{
		/* Fill in the predeclared function! */
		fo = decl->v.object;
		f = o2EEL_function(fo);
		eel_o_own(fo);
	}
	else
	{
		EEL_value fov;
		x = eel_o_construct(es->vm, EEL_CFUNCTION, NULL, 0, &fov);
		if(x)
			return -1;
		if((EEL_classes)EEL_TYPE(&fov) != EEL_CFUNCTION)
			eel_ierror(es, "FUNCTION constructor returned %s "
					"instance!", eel_typename(es->vm,
					fov.type));
		fo = fov.objref.v;
		f = o2EEL_function(fo);
		f->common.module = es->context->module;
		add_object_to_module(es, fo);
	}

	f->common.flags = flags;

	/*
	 * Apply qualifiers
	 */
	if(es->qualifiers & EEL_QEXPORT)
		f->common.flags |= EEL_FF_EXPORT;

	/*
	 * If this is an xblock, mark as XBLOCK and inherit
	 * the parent's RESULTS flag.
	 */
	if(flags & EEL_FF_XBLOCK)
		f->common.flags |= (pflags & EEL_FF_RESULTS);

	/* Move code generation to a local coder for the function */
	nc = eel_coder_open(es, fo);
	if(!nc)
	{
		eel_o_disown_nz(fo);
		return -1;
	}
	es->context->coder = nc;
	es->context->flags |= ECTX_OWNS_CODER;
	es->context->startpos = 0;

	/* Turn the current namespace into the new function */
	st->v.object = es->context->coder->f;
	if(!decl)
	{
		if(!symname)
			symname = name;
		eel_s_rename(es, st, symname);
		if(f->common.name)
			eel_o_disown_nz(f->common.name);
		f->common.name = eel_ps_new(es->vm, name);
		if(!f->common.name)
			eel_serror(es, "Could not rename function!");
	}

	/* Add the function to the arglist */
	if(al)
		eel_m_object(al, fo);

	DBGF(printf("-- declared function '%s' in '%s'; flags = %d\n", name,
			st->parent->name ? eel_o2s(st->parent->name) :
			"<unnamed>", f->common.flags);)
	return 0;
}


/*
 * Scan the local symbol table for results and arguments,
 * and use the information to construct the function call
 * prototype info.
 */
static void declare_func_args(EEL_state *es, int has_result)
{
	EEL_symbol *s;
	EEL_finder ef;
	EEL_function *f = o2EEL_function(es->context->coder->f);
	eel_finder_init(es, &ef, es->context->symtab, ESTF_TYPES);
	ef.types = EEL_SMVARIABLE;
	f->common.results = has_result ? 1 : 0;
	f->common.reqargs = 0;
	f->common.optargs = 0;
	f->common.tupargs = 0;
	while((s = eel_finder_go(&ef)))
		switch((EEL_varkinds)s->v.var.kind)
		{
		  case EVK_STACK:
		  case EVK_STATIC:
			break;
		  case EVK_ARGUMENT:
			s->v.var.location = f->common.reqargs;
			++f->common.reqargs;
			break;
		  case EVK_OPTARG:
			s->v.var.location = f->common.optargs;
			++f->common.optargs;
			break;
		  case EVK_TUPARG:
			s->v.var.location = f->common.tupargs;
			++f->common.tupargs;
			break;
		}
	if(f->common.results)
		f->common.flags |= EEL_FF_RESULTS;
	if(f->common.reqargs || f->common.optargs || f->common.tupargs)
		f->common.flags |= EEL_FF_ARGS;
    DBGF({
	printf("-- function '%s' has ", eel_o2s(es->context->symtab->name));
	printf("%d results, ", f->common.results);
	printf("%d required arguments, ", f->common.reqargs);
	printf("%d optional arguments and ", f->common.optargs);
	printf("%d tuple arguments.\n", f->common.tupargs);
    })
}


/*
FIXME: This implementation might generate register fragmentation!
FIXME: This would happen if there are temporary registers allocated
FIXME: when declaring stack variables. The regs will be recovered
FIXME: sooner or later, so it's no showstopper even if it happens.

TODO: We should treat static variables like stack variables (ie
TODO: track initializations) when compiling into __module_init,
TODO: and make sure that there are no uninitialized statics when
TODO: compiler closes the __module_init function.
TODO: Currently, static variables are initialized to nil as they're
TODO: declared... They shouldn't even be existing physically at
TODO: compile time!
*/
static EEL_symbol *declare_var(EEL_state *es, const char *name, EEL_varkinds kind)
{
	EEL_coder *cdr = es->context->coder;
	EEL_symbol *s = eel_s_add(es, es->context->symtab, name, EEL_SVARIABLE);
	if(!s)
		eel_serror(es, "Could not create symbol for new variable!");
	s->v.var.kind = kind;
	switch(kind)
	{
	  case EVK_STACK:
		s->v.var.location = eel_r_alloc(cdr, 1, EEL_RUVARIABLE);
		DBGF(printf("-- declared stack variable '%s' (storage: R[%d])",
				 name, s->v.var.location);)
		break;
	  case EVK_STATIC:
		s->v.var.location = eel_coder_add_variable(cdr, NULL);
		DBGF(printf("-- declared static variable '%s' (storage: SV[%d])",
				 name, s->v.var.location);)
		break;
	  case EVK_ARGUMENT:
		DBGF(printf("-- declared required argument '%s'", name);)
		break;
	  case EVK_OPTARG:
		DBGF(printf("-- declared optional argument '%s'", name);)
		break;
	  case EVK_TUPARG:
		DBGF(printf("-- declared tuple argument '%s'", name);)
		break;
	}
	DBGF(printf(" in '%s'\n", eel_o2s(es->context->symtab->name));)
	return s;
}


/*
 * Explicitly declare variable 'var' as an upvalue for access without
 * warnings in the current function.
 */
static EEL_symbol *declare_upvalue(EEL_state *es, EEL_symbol *var)
{
	EEL_symbol *s = eel_s_add(es, es->context->symtab,
			o2EEL_string(var->name)->buffer, EEL_SUPVALUE);
	if(!s)
		eel_serror(es, "Could not create symbol for upvalue!");

	/* Make the new symbol a local shadow of the "real" symbol */
	s->v.var.kind = var->v.var.kind;
	s->v.var.location = var->v.var.location;
	s->uvlevel = var->uvlevel;
	return s;
}


/*
 * Get variable 's', adding it to 'al'.
 *
 * If 'al' allows it, the register index of the variable itself
 * is added. If not, a temporary register is allocated via 'al',
 * and code is generated to copy the value.
 */
static void do_getvar(EEL_state *es, EEL_symbol *s, EEL_mlist *al)
{
	int level = es->context->symtab->uvlevel - s->uvlevel;
	int uv = level != 0;
	DBGF(printf("Getting '%s' in '%s'; level = %d; ",
			eel_o2s(s->name),
			eel_o2s(es->context->symtab->name), level));
	switch((EEL_varkinds)s->v.var.kind)
	{
	  case EVK_STACK:
		DBGF(printf("STACK\n"));
		eel_m_variable(al, s, level);
		break;
	  case EVK_STATIC:
		DBGF(printf("STATIC\n"));
		eel_m_statvar(al, s);
		uv = 0;	/* Statics are never upvalues! */
		break;
	  case EVK_ARGUMENT:
		DBGF(printf("ARGUMENT\n"));
		eel_m_argument(al, s, level);
		break;
	  case EVK_OPTARG:
		DBGF(printf("OPTARG\n"));
		eel_m_optarg(al, s, level);
		break;
	  case EVK_TUPARG:
		DBGF(printf("TUPARG\n"));
		eel_m_tuparg(al, s, level);
		break;
	  default:
		eel_cerror(es, "Tried to get variable '%s', which is not"
				" of any legal variable kind!",
				eel_o2s(s->name));
	}
	if(uv)
	{
		EEL_function *f = o2EEL_function(es->context->coder->f);
		f->common.flags |= EEL_FF_UPVALUES;
		if((EEL_SVARIABLE == s->type) &&
				!(f->common.flags & EEL_FF_XBLOCK))
			eel_cerror(es, "Implicit upvalue '%s'.",
					eel_o2s(s->name));
	}
}


/*
 * Try to index 'obj' by 'index', adding the result(s) to 'dest'.
 */
static void do_index(EEL_state *es, EEL_mlist *obj, EEL_mlist *ind,
		EEL_mlist *dest)
{
	int i, count, oi, ii;
	if(!obj->length)
		eel_cerror(es, "Trying to index nothing!");
	if(!ind->length)
		eel_cerror(es, "Trying to index object with nothing!");
	if(obj->length == 1)
	{
		count = ind->length;
		oi = 0;
		ii = 1;
	}
	else if(ind->length == 1)
	{
		count = obj->length;
		oi = 1;
		ii = 0;
	}
	else
	{
		if(obj->length != ind->length)
			eel_cerror(es, "Number of objects does not match "
					"number of indices!");
		count = obj->length;
		oi = ii = 1;
	}
	for(i = 0; i < count; ++i)
		eel_m_index(dest, eel_ml_get(obj, i * oi),
				eel_ml_get(ind, i * ii));
}

/*
 * Create a string object from a C string.
 * The object is returned as a reference through 'v'.
 */
static void v_set_string(EEL_state *es, const char *s, EEL_value *v)
{
	v->type = EEL_TOBJREF;
	v->objref.v = eel_ps_new(es->vm, s);
	if(!v->objref.v)
		eel_serror(es, "Could not create string object!");
}

/*
 * Create a string object from a memory block + length.
 * The object is returned as a reference through 'v'.
 */
static void v_set_string_n(EEL_state *es, const char *s, int len, EEL_value *v)
{
	v->type = EEL_TOBJREF;
	v->objref.v = eel_ps_nnew(es->vm, s, len);
	if(!v->objref.v)
		eel_serror(es, "Could not create string object "
				"for string literal!");
}


/*
 * Parse zero or more qualifier keywords, returning
 * a bit mask with 1's for the qualifiers parsed.
 */
/*
	qualifier:
		  KW_LOCAL
		| KW_STATIC
		| KW_UPVALUE
		| KW_EXPORT
		| KW_SHADOW
		;

	qualifiers:
		  qualifier
		| qualifier qualifiers
		;
*/
static int parse_qualifiers(EEL_state *es)
{
	EEL_qualifiers result = 0;
	while(1)
	{
		EEL_qualifiers q;
		switch(es->token)
		{
		  case TK_KW_LOCAL:	q = EEL_QLOCAL; break;
		  case TK_KW_STATIC:	q = EEL_QSTATIC; break;
		  case TK_KW_UPVALUE:	q = EEL_QUPVALUE; break;
		  case TK_KW_EXPORT:	q = EEL_QEXPORT; break;
		  case TK_KW_SHADOW:	q = EEL_QSHADOW; break;
		  default:
			return result;
		}
		if(q & result)
			eel_cerror(es, "Qualifier '%s' specified"
					" more than once!",
					eel_o2s(es->lval.v.symbol->name));
		result |= q;
		eel_lex(es, 0);
	}
}

/*
 * Check that only the listed qualifiers are present, and throw an error as
 * defined by 'fmt' if others are found. If 'fmt' contains a %s format code,
 * it is replaced with the qualifier keyword in any error message.
 */
static void q_allow_only(EEL_state *es, EEL_qualifiers mask, const char *fmt)
{
	mask = es->qualifiers & ~mask;
	if(mask & EEL_QLOCAL)
		eel_cerror(es, fmt, "local");
	if(mask & EEL_QSTATIC)
		eel_cerror(es, fmt, "static");
	if(mask & EEL_QUPVALUE)
		eel_cerror(es, fmt, "upvalue");
	if(mask & EEL_QEXPORT)
		eel_cerror(es, fmt, "export");
	if(mask & EEL_QSHADOW)
		eel_cerror(es, fmt, "shadow");
}


/* Complain if any qualifiers at all are present. */
#ifdef DEBUG
static void __no_qualifiers(EEL_state *es, int line)
# define no_qualifiers(es) __no_qualifiers(es, __LINE__)
#else
static void no_qualifiers(EEL_state *es)
#endif
{
	if(es->qualifiers)
#ifdef DEBUG
		eel_cerror(es, "No qualifiers allowed in this context! (%d)", line);
#else
		eel_cerror(es, "No qualifiers allowed in this context!");
#endif
}



/*
 * "Eat" the specified qualifiers, and complain if any other qualifiers are
 * present.
 */
static void qualifiers_handled(EEL_state *es, EEL_qualifiers mask)
{
	es->qualifiers &= ~mask;
	q_allow_only(es, -1, "Qualifier '%s' not allowed in this context!");
}


/*----------------------------------------------------------
	Parser
------------------------------------------------------------

Parser functions:
	The parser functions are written pretty much exactly
	according to the grammar. That is, each function
	corresponds to one rule (or sometimes, two rules),
	and calling the function tries to parse the code
	according to it's corresponding rule(s).

	This means that the parser will call around and
	climb the stack a bit just to check whether or not a
	rule is applicable, but it makes the code a lot
	simpler to read and maintain. It is likely that the
	parser will be optimized later on, when the grammar
	is set in stone.

Return values:
	All parser functions return tokens that describe what
	the parsed code evaluated into, or whether or not
	parsing was successful.

	TK_WRONG is returned by a parser function when it
	realizes that it cannot parse. TK_WRONG means that
	the parser state was not changed, or that it was
	restored, so that it is safe to continue parsing.

	Note that a parser function may return other tokens
	than it's respective grammar rule token. This can be
	thought of as multiple grammar rules being
	implemented in a single parser function. The return
	token indicates which of the rules matched.

Parser state:
	When TK_WRONG is returned, the parser state is the
	same as before the call. In all other cases, the EEL
	state holds the first token following the tokens
	parsed so far.
 */


/*----------------------------------------------------------
	Generic "list of" rule
----------------------------------------------------------*/
static int listof(EEL_state *es, EEL_mlist *al,
		int(*what)(EEL_state *es, EEL_mlist *al, int xopt),
		const char *whatn, int lexflags, int xopt)
{
	int first = 1;
	while(1)
	{
		switch(what(es, al, xopt))
		{
		  case TK_WRONG:
			return first ? TK(WRONG) : TK2("LISTOF <?>", TK_EXPLIST);
		  case TK_VOID:
			if(first && (es->token != ','))
				return TK(VOID);
			else
				eel_cerror(es, "Void expression in %s "
						"list!", whatn);
		}
		first = 0;
		if(es->token != ',')
			return TK2("LISTOF <?>", TK_EXPLIST);
		eel_lex(es, lexflags);
	}
}


/*----------------------------------------------------------
	explist rule
----------------------------------------------------------*/
/*
	explist:
		  expression
		| expression ',' explist
		;
*/
static int explist(EEL_state *es, EEL_mlist *al, int wantresult)
{
	int res;
	EEL_mlist *sal = eel_ml_open(es->context->coder);
	res = listof(es, sal, expression, "expression", 0, wantresult);
	eel_ml_transfer(sal, al);
	eel_ml_close(sal);
	return res;
}


/*----------------------------------------------------------
	call rule
----------------------------------------------------------*/

static int call_member(EEL_state *es, EEL_manipulator *fnref,
		EEL_manipulator *self, EEL_mlist *al, int wantresult)
{
	EEL_coder *cdr = es->context->coder;
	EEL_mlist *args;
	int r;

	/* Generate arguments */
	args = eel_ml_open(cdr);
	if(self)
		eel_m_transfer(self, args);
	switch(explist(es, args, 1))
	{
	  case TK_WRONG:
		if(es->token != ')')
			eel_cerror(es, "Expected argument list!");
		break;
	  case TK_VOID:
		eel_cerror(es, "Argument generates no value!");
	}

	if(self)
		expect(es, ')', "Expected ')' after member call arguments!");
	else
		expect(es, ')', "Expected ')' after function call arguments!");

	/*
	 * We need to override an "ignore result" call if it's actually just
	 * the first simplexp of something bigger!
	 */
	switch(es->token)
	{
	  case '[':
	  case '.':
	  case ':':
		wantresult = 1;
		break;
	}

	/*
	 * Make the call!
	 */
	if(wantresult)
		r = eel_m_result(al);
	else
		r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
	/* We borrow the result register for the funcref */
	eel_try(es)
		eel_m_read(fnref, r);
	eel_except
		eel_cerror(es, "Cannot read function reference for call!");
	DBGG(if(wantresult)
		printf("=== result and funcref in R[%d]\n", r);
	else
		printf("=== funcref in R[%d]\n", r);)
	eel_ml_push(args);
	if(wantresult)
		eel_codeAB(cdr, EEL_OCALLR_AB, r, r);
	else
	{
		eel_codeA(cdr, EEL_OCALL_A, r);
		eel_r_free(cdr, r, 1);
	}

	/* Cleanup */
	eel_ml_close(args);
	DBGE(printf("=== end function call ==============================\n");)
	return TK(CALL);
}


static inline int call_indirect(EEL_state *es, EEL_manipulator *fnref,
		EEL_mlist *al, int wantresult)
{
	return call_member(es, fnref, NULL, al, wantresult);
}


/*
	call:
		  FUNCTION
		| FUNCTION '(' ')'
		| FUNCTION '(' explist ')'
		;
 */
static int call(EEL_state *es, EEL_mlist *al)
{
	EEL_coder *cdr = es->context->coder;
	EEL_object *fo;
	EEL_function *f;
	EEL_value fnref;
	EEL_symbol *s;
	EEL_mlist *args;
	int fnconst, result, uvlevel;
	if(TK_SYM_FUNCTION != es->token)
		return TK(WRONG);

	/* Get function */
	s = es->lval.v.symbol;
	fo = s->v.object;
	if((EEL_classes)fo->type != EEL_CFUNCTION)
		eel_ierror(es, "Function symbol has an object that"
				" is not a function!");
	f = o2EEL_function(fo);
    DBGE({
	printf("=== begin function call ============================\n");
	printf("=== function '%s' has ", eel_o2s(s->name));
	printf("%d results, ", f->common.results);
	printf("%d required arguments, ", f->common.reqargs);
	printf("%d optional arguments and ", f->common.optargs);
	printf("%d tuple arguments.\n", f->common.tupargs);
    })
	/* Find/add a constant for that function */
	fnref.type = EEL_TOBJREF;
	fnref.objref.v = fo;
	fnconst = eel_coder_add_constant(cdr, &fnref);
	DBGG(printf("=== func const in C[%d]\n", fnconst);)
	eel_lex(es, 0);

	no_qualifiers(es);

	if(es->token != '(')
	{
		/* Reference to function */
		eel_m_constant(al, &fnref);
		return TK(SIMPLEXP);
	}
	eel_lex(es, 0);

	/* Generate arguments */
	args = eel_ml_open(cdr);
	switch(explist(es, args, 1))
	{
	  case TK_WRONG:
		if(es->token != ')')
			eel_cerror(es, "Expected argument list!");
		break;
	  case TK_VOID:
		eel_cerror(es, "Argument generates no value!");
	}

	/* Check argument count */
	if(args->length < f->common.reqargs)
		eel_cerror(es, "Too few arguments to function '%s'!",
				eel_o2s(s->name));
	if(f->common.tupargs)
	{
		if((args->length - f->common.reqargs) % f->common.tupargs)
			eel_cerror(es, "Incorrect number of arguments"
					" to function '%s'!"
					" (Incomplete tuple.)",
					eel_o2s(s->name));
	}
	else if(((f->common.optargs != 255) &&
			(args->length > f->common.reqargs + f->common.optargs)) ||
			(!f->common.optargs && (args->length > f->common.reqargs)))
		eel_cerror(es, "Too many arguments to function '%s'!",
				eel_o2s(s->name));

	/* Prepare result and arguments */
	if(f->common.results)
		result = eel_m_result(al);
	else
		result = -1;
	eel_ml_push(args);

	/* Make the call! */
	DBGE(printf("=== caller level: %d (%p)\n",
			es->context->symtab->uvlevel, es->context->symtab);)
	DBGE(printf("=== function level: %d (%p)\n", s->uvlevel, s);)
	if(f->common.flags & EEL_FF_UPVALUES)
		uvlevel = es->context->symtab->uvlevel - s->uvlevel + 1;
	else
		uvlevel = 0;
	if(result >= 0)
		eel_codeABCx(cdr, EEL_OCCALLR_ABCx, uvlevel, result, fnconst);
	else
		eel_codeABx(cdr, EEL_OCCALL_ABx, uvlevel, fnconst);

	/* Cleanup */
	eel_ml_close(args);

	if(es->token != ')')
		eel_cerror(es, "Expected ')' after arguments, or ',' "
				"followed by more arguments!");
	eel_lex(es, 0);

	DBGE(printf("=== end function call ==============================\n");)
	if(result)
		return TK(CALL);
	else
		return TK(VOID);
}


/*----------------------------------------------------------
	funcargs rule
----------------------------------------------------------*/

static int getargs(EEL_state *es)
{
	int delim;
	EEL_varkinds kind;
	switch(es->token)
	{
	  /* argument list */
	  case '(':	/* required */
		delim = ')';
		kind = EVK_ARGUMENT;
		break;
	  case '[':	/* optional */
		delim = ']';
		kind = EVK_OPTARG;
		break;
	  case '<':	/* tuple */
		delim = '>';
		kind = EVK_TUPARG;
		break;
	  default:
		return TK(WRONG);
	}
	eel_lex(es, ELF_NO_OPERATORS);
	argdeflist(es, kind);
	if(es->token != delim)
		eel_cerror(es, "Expected closing '%c' after argument list.",
				delim);
	eel_lex(es, ELF_NO_OPERATORS);
	return TK(ARGDEFLIST);
}


static int funcargs_check(EEL_state *es)
{
	/*
	 * At this point, we're expecting a function body
	 * or something, but NOT any more argument lists.
	 * The switch below is only for nice error messages.
	 */
	switch(es->token)
	{
	  case '(':	/* required */
	  case '[':	/* optional */
	  case '<':	/* tuple */
		eel_cerror(es, "Too many argument lists, or illegal "
				"combination of argument lists!");
	}
	return TK(FUNCARGS);
}

/*
	funcargs:
		'(' argdeflist ')'
		| '[' argdeflist ']'
		| '<' argdeflist '>'
		| '(' argdeflist ')' '[' argdeflist ']'
		| '(' argdeflist ')' '<' argdeflist '>'
		;
*/
static int funcargs(EEL_state *es)
{
	eel_unlex(es);
	eel_lex(es, ELF_NO_OPERATORS);

	/* Get first/single list */
	switch(es->token)
	{
	  case '(':	/* required */
		getargs(es);
		break;
	  case '[':	/* optional */
	  case '<':	/* tuple */
		getargs(es);
		switch(es->token)
		{
		  case '(':	/* required */
			eel_cerror(es, "Required arguments must come first!");
		  case '[':	/* optional */
		  case '<':	/* tuple */
			eel_cerror(es, "Cannot have both optional and"
					" tuple arguments!");
		}
		return funcargs_check(es);
	  default:
		return TK(WRONG);
	}

	/*
	 * We have a "required list". If there's a second
	 * argument list, it must be optional or tuple.
	 */
	switch(es->token)
	{
	  case '(':	/* required */
		eel_cerror(es, "Required arguments already specified!");
	  case '[':	/* optional */
	  case '<':	/* tuple */
		getargs(es);
	}
	return funcargs_check(es);
}


/*----------------------------------------------------------
	funcdecl and funcdef rules
----------------------------------------------------------*/
/*
	funcdecl:
		  KW_PROCEDURE NAME funcargs
		| KW_FUNCTION NAME funcargs
		| qualifiers KW_PROCEDURE NAME funcargs
		| qualifiers KW_FUNCTION NAME funcargs
		;

	funcdef:
		  KW_PROCEDURE NAME funcargs body
		| KW_FUNCTION NAME funcargs body
		| qualifiers KW_PROCEDURE NAME funcargs body
		| qualifiers KW_FUNCTION NAME funcargs body
		| KW_PROCEDURE funcargs body
		| KW_FUNCTION funcargs body
		| qualifiers KW_PROCEDURE funcargs body
		| qualifiers KW_FUNCTION funcargs body
		;
 */

static int funcdef2(EEL_state *es, EEL_mlist *al, int is_func, int local)
{
	EEL_function decl;
	EEL_symbol *declsym = NULL;
	int predeclared = 0;
	EEL_function *f;
	const char *fname = NULL;

	/* Get name */
	switch(es->token)
	{
	  case TK_NAME:
		/* New declaration and/or definition */
		fname = es->lval.v.s.buf;
		break;
	  case TK_SYM_FUNCTION:
	  {
		/* Definition of predeclared function */
		EEL_object *fo = es->lval.v.symbol->v.object;
		f = o2EEL_function(fo);
		fname = eel_o2s(f->common.name);
		if(local)
			break;	/* We don't care if the name is "taken"! */
		if((EEL_classes)fo->type != EEL_CFUNCTION)
			eel_ierror(es, "Function symbol has an object that"
					" is not a function!");
		if(f->common.flags & EEL_FF_DECLARATION)
		{
			/* Copy the declaration (empty function) */
			predeclared = 1;
			memcpy(&decl, f, sizeof(EEL_function));
			declsym = es->lval.v.symbol;
			break;
		}
		if(f->common.flags & EEL_FF_CFUNC)
			eel_cerror(es, "There is a C function named '%s'!",
					eel_o2s(es->lval.v.symbol->name));
		else
			eel_cerror(es, "There already is a function named '%s'!",
					eel_o2s(es->lval.v.symbol->name));
	  }
	  case TK_SYM_OPERATOR:
		eel_cerror(es, "There is an operator named '%s'!",
				eel_o2s(es->lval.v.symbol->name));
	  case_all_TK_KW:
		eel_cerror(es, "'%s' is a reserved EEL keyword!",
				eel_o2s(es->lval.v.symbol->name));
	  case '(':
	  case '[':
	  case '<':
	  case '{':
		/* Anonymous function! */
		break;
	  default:
		eel_cerror(es, "Expected function name!");
	}

	if(local)
	{
		const char *symname;
		if(!fname)
			eel_cerror(es, "Member function must be named!");
		symname = eel_unique(es->vm, fname);
		declare_func(es, fname, al, NULL, 0, symname);
		eel_sfree(es, symname);
		eel_lex(es, 0);
	}
	else if(fname)
	{
		declare_func(es, fname, al, declsym, 0, NULL);
		eel_lex(es, 0);
	}
	else
	{
		fname = eel_unique(es->vm, "__anonymous_function");
		declare_func(es, fname, al, NULL, 0, NULL);
		eel_sfree(es, fname);
		/* NOTE: Current token was not a name, so no eel_lex() here! */
	}
	f = o2EEL_function(es->context->coder->f);

	/* Check general declaration/definition qualifiers */
	if((es->qualifiers & (EEL_QLOCAL | EEL_QEXPORT)) ==
			(EEL_QLOCAL | EEL_QEXPORT))
		eel_cerror(es, "Functions cannot be both local"
				" and exported!");
	q_allow_only(es, EEL_QLOCAL | EEL_QEXPORT,
			"Cannot use qualifier '%s' with functions!");
	if(es->qualifiers & EEL_QEXPORT)
		if(f->e.flags & EEL_FF_UPVALUES)
			eel_cerror(es, "Functions that use upvalues"
					" cannot be exported!");

	/* Get argument list */
	funcargs(es);
	declare_func_args(es, is_func);

	/* Predeclared function? */
	if(predeclared)
	{
		/* Check prototype */
		EEL_function *df = o2EEL_function(declsym->v.object);
		if(!eel_function_compare(df, &decl))
			eel_cerror(es, "Function definition does not match "
					"previous declaration!");

		/* Redirect context to declaration symbol */
		eel_s_free(es, es->context->symtab);
		es->context->symtab = declsym;

		/* Verify any qualifiers against the original declaration */
		if((es->qualifiers & EEL_QLOCAL) &&
				(f->common.flags & EEL_FF_EXPORT))
			eel_cerror(es, "Definition of exported function '%s'"
					" tries to make the function local!",
					eel_o2s(f->e.name));
		if((es->qualifiers & EEL_QEXPORT) &&
				!(f->common.flags & EEL_FF_EXPORT))
			eel_cerror(es, "Definition of local function '%s'"
					" tries to export the function!",
					eel_o2s(f->e.name));
	}

	qualifiers_handled(es, EEL_QLOCAL | EEL_QEXPORT);

	/* Check for accidental Pascal inspired semicolons */
	if(es->token == ';')
	{
		eel_lex(es, 0);
		if(es->token == '{')
			eel_cwarning(es, "Likely accidental ';' breaking"
					" function definition!");
		eel_unlex(es);
	}

	/* Compile function body */
	if(TK_WRONG == body(es, ECTX_FUNCTION))
	{
		if(predeclared)
			eel_cerror(es, "Expected function body!");

		/* Just a declaration! */
		f = o2EEL_function(es->context->symtab->v.object);
		f->common.flags |= EEL_FF_DECLARATION;

		DBGH(printf("## funcdecl #####################################\n");)
		return TK(FUNCDECL);
	}

	/* Leave and finalize function */
	procreturn(es);
	DBGH(printf("## funcdef #####################################\n");)

	return TK(FUNCDEF);
}


static int funcdef(EEL_state *es, EEL_mlist *al, int local)
{
	int res, is_func;
	switch(es->token)
	{
	  case TK_SYM_CLASS:
		/*
		 * This "hack" is needed because 'function' is
		 * actually a type name, rather than a keyword.
		 */
		if((EEL_classes)eel_class_typeid(es->lval.v.symbol->v.object) !=
				EEL_CFUNCTION)
			return TK(WRONG);
		is_func = 1;
		break;
	  case TK_KW_PROCEDURE:
		is_func = 0;
		break;
	  default:
		return TK(WRONG);
	}
	eel_lex(es, 0);

	/*
	 * Hack to make "(function)" and "(procedure)"
	 * generate a CFUNCTION type id literal.
	 */
	if(es->token == ')')
	{
		EEL_value v;
		v.type = EEL_TTYPEID;
		v.integer.v = EEL_CFUNCTION;
		eel_m_constant(al, &v);
		return TK(SIMPLEXP);
	}

	eel_context_push(es, ECTX_FUNCTION, NULL);
	res = funcdef2(es, al, is_func, local);
	eel_context_pop(es);

	/* The next token was lexed in the function's context! Re-lex it. */
	eel_relex(es, 0);

	return res;
}


/*----------------------------------------------------------
	xblock rule
----------------------------------------------------------*/
/*
	xblock:
		block
		;
*/
static int xblock(EEL_state *es, const char *basename, EEL_mlist *al, int flags)
{
	char *name = eel_unique(es->vm, basename);
	eel_context_push(es, ECTX_FUNCTION | flags, NULL);
	declare_func(es, name, al, NULL, EEL_FF_XBLOCK, NULL);
	eel_sfree(es, name);

	/*
	 * Catchers use R[0] for 'exception' - and that needs cleaning!
	 * Try blocks don't need R[0] at all.
	 */
	if(flags & ECTX_CATCHER)
		eel_r_alloc_reg(es->context->coder, 0, EEL_RUVARIABLE);

	if(!(flags & ECTX_DUMMY))
	{
		/* Compile function body */
		if(TK_WRONG == statement(es, ECTX_FUNCTION))
			eel_cerror(es, "Expected a statement or body!");
	}

/*
FIXME: Do we need a special "hands off" RETURN here, to avoid messing up the
FIXME: return value of the containing normal function?
 */
/*
	if(flags & EEL_FF_RESULTS)
		eel_codeA(cdr, EEL_ORETXR_A, r);
	else
		eel_code0(es->context->coder, EEL_ORETX_0);
*/
	eel_code0(es->context->coder, EEL_ORETURN_0);

	eel_context_pop(es);

	if(!(flags & ECTX_DUMMY))
	{
		/* The token was lexed in the function's context. Re-lex! */
		eel_unlex(es);
		eel_lex(es, 0);
	}
	return TK(FUNCDEF);
}


/*----------------------------------------------------------
	arginfo rule
----------------------------------------------------------*/
static void check_specified(EEL_state *es, EEL_symbol *s, EEL_mlist *al)
{
	int r;
	EEL_coder *cdr = es->context->coder;
	int level = s->uvlevel - es->context->symtab->uvlevel;
	EEL_mlist *ind;
	eel_lex(es, 0);
	switch((EEL_varkinds)s->v.var.kind)
	{
	  case EVK_ARGUMENT:
		eel_cerror(es, "'specified' used on required argument!");
	  /* | KW_SPECIFIED VARIABLE */
	  case EVK_OPTARG:
		if(level)
			eel_cerror(es, "'specified' cannot test upvalues!");
		eel_codeAB(cdr, EEL_OSPEC_AB, s->v.var.location, eel_m_result(al));
		break;
	  /* | KW_SPECIFIED VARIABLE '[' expression ']' */
	  case EVK_TUPARG:
		if(level)
			eel_cerror(es, "'specified' cannot test upvalues!");
		ind = eel_ml_open(cdr);
		expect(es, '[', NULL);
		switch(expression(es, ind, 1))
		{
		  case TK_WRONG:
			eel_cerror(es, "Index expression does not"
					" generate a result!");
		  case TK_VOID:
			eel_cerror(es, "Expected index expression!");
		}
		expect(es, ']', "Missing ']'!");
		check_argc(ind, 1, 1);
		r = eel_m_result(al);
		eel_m_read(eel_ml_get(ind, 0), r);
		eel_codeAB(cdr, EEL_OTSPEC_AB, r, r);
		eel_ml_close(ind);
		break;
	  default:
		eel_cerror(es, "Expected argument identifier!");
	}
}

/*
	arginfo:
		  KW_ARGUMENTS
		| KW_TUPLES
		| KW_SPECIFIED VARIABLE
		| KW_SPECIFIED VARIABLE '[' expression ']'
		;
*/
static int arginfo(EEL_state *es, EEL_mlist *al)
{
	EEL_coder *cdr = es->context->coder;
	EEL_function *f = o2EEL_function(cdr->f);
	switch(es->token)
	{
	  case TK_KW_ARGUMENTS:
		no_qualifiers(es);
		if(!f->common.optargs && !f->common.tupargs)
			eel_cerror(es, "'arguments' used in a function with no "
					"optional or tuple arguments!");
		eel_codeA(cdr, EEL_OARGC_A, eel_m_result(al));
		eel_lex(es, 0);
		break;
	  case TK_KW_TUPLES:
		no_qualifiers(es);
		if(!f->common.tupargs)
			eel_cerror(es, "'tuples' used in a function with no "
					"tuple arguments!");
		eel_codeA(cdr, EEL_OTUPC_A, eel_m_result(al));
		eel_lex(es, 0);
		break;
	  case TK_KW_SPECIFIED:
		no_qualifiers(es);
		eel_lex(es, 0);
		if(TK_SYM_VARIABLE == es->token)
			check_specified(es, es->lval.v.symbol, al);
		else
			eel_cerror(es, "Expected argument identifier!");
		break;
	  default:
		return TK(WRONG);
	}
	return TK(SIMPLEXP);
}


/*----------------------------------------------------------
	field rule
----------------------------------------------------------*/
/*
	field:
		  CONSTANT
		| VARIABLE
		| FUNCTION
		| NAME
		;
*/
/* NOTE: The caller is responsible for ensuring that symbol lookups for
 *       the current token are done in the right scope.
 */
static int field(EEL_state *es, EEL_mlist *al, int xopt)
{
	switch(es->token)
	{
	  case TK_SYM_CONSTANT:
		eel_m_constant(al, &es->lval.v.symbol->v.value);
		eel_lex(es, 0);
		DBGE2(printf("@@@@@@@@@ CONSTANT field @@@@@@@@@\n");)
		return TK(SIMPLEXP);

	  case TK_SYM_VARIABLE:
		do_getvar(es, es->lval.v.symbol, al);
		eel_lex(es, 0);
		DBGE2(printf("@@@@@@@@@ VARIABLE field @@@@@@@@@\n");)
		return TK(SIMPLEXP);

	  case TK_SYM_FUNCTION:
	  {
		eel_ierror(es, "How did I end up with a field that is a "
				"function reference...!?");
#if 0
		EEL_object *o;
		EEL_symbol *s;
		EEL_function *f;

		/* Get function */
		s = es->lval.v.symbol;
		eel_lex(es, 0);
		o = s->v.object;
		if(o->type != EEL_CFUNCTION)
			eel_ierror(es, "Function symbol has an object that"
					" is not a function!");
		f = o2EEL_function(o);
	    DBGE({
		printf("=== indexing function field =====================\n");
		printf("=== function '%s' has ", eel_o2s(s->name));
		printf("%d results, ", f->common.results);
		printf("%d required arguments, ", f->common.reqargs);
		printf("%d optional arguments and ", f->common.optargs);
		printf("%d tuple arguments.\n", f->common.tupargs);
	    })

///////////// Figure out index of function in the indexed object

		DBGE2(printf("@@@@@@@@@ FUNCTION field @@@@@@@@@\n");)
		return TK(SIMPLEXP);
#endif
	  }
	  case TK_NAME:
	  {
		EEL_value v;
		v_set_string(es, es->lval.v.s.buf, &v);
		eel_m_constant(al, &v);
		eel_v_disown_nz(&v);
		eel_lex(es, 0);
		DBGE2(printf("@@@@@@@@@ NAME field @@@@@@@@@\n");)
	  }
		return TK(SIMPLEXP);
	}

	return TK(WRONG);
}


/*----------------------------------------------------------
	fieldlist rule
----------------------------------------------------------*/
/*
	fieldlist:
		  field
		| field ',' fieldlist
		;
*/
static int fieldlist(EEL_state *es, EEL_mlist *al)
{
	return listof(es, al, field, "field", ELF_LOCALS_ONLY, 0);
}


/*----------------------------------------------------------
	vardecl and typeid rules
----------------------------------------------------------*/
/*
	vardecl:
		qualifiers NAME
TODO:		| qualifiers TYPENAME NAME
		;
*/
static int vardecl(EEL_state *es, EEL_mlist *al)
{
	EEL_varkinds vk;
/*	EEL_types t = EEL_TANYTYPE;*/

	switch(es->token)
	{
	  case TK_NAME:
		break;
	  case TK_SYM_CLASS:
		switch((EEL_classes)eel_class_typeid(es->lval.v.symbol->v.object))
		{
			/*
			 * These have special uses, so we need
			 * an early exit here for now...
			 */
		  case EEL_CFUNCTION:
		  case EEL_CMODULE:
			return TK(WRONG);
		  default:
			break;
		}
		eel_lex(es, 0);
		if(es->token != TK_NAME)
		{
			eel_unlex(es);
			return TK(WRONG);
		}
		eel_cerror(es, "Static typing not yet implemented!");
#if 0
		t = es->lval.v.i;
#endif
		eel_unlex(es);
		break;
	  default:
		return TK(WRONG);
	}

	q_allow_only(es, EEL_QLOCAL | EEL_QUPVALUE | EEL_QSHADOW | EEL_QSTATIC,
			"Cannot use qualifier '%s' with variables!");

	/* Either a 'local' or 'static' qualifier is required! */
	if(!(es->qualifiers & EEL_QLOCAL) &&
			!(es->qualifiers & EEL_QSTATIC))
		eel_cerror(es, "'%s' not declared in the "
				"current scope!",
				es->lval.v.s.buf);
	vk = EVK_STACK;
	if((es->qualifiers & (EEL_QUPVALUE | EEL_QSHADOW)) ==
			(EEL_QUPVALUE | EEL_QSHADOW))
		eel_cerror(es, "Can't both use and shadow an upvalue!");
	if((es->qualifiers & (EEL_QUPVALUE | EEL_QSTATIC)) ==
			(EEL_QUPVALUE | EEL_QSTATIC))
		eel_cerror(es, "Cannot make an upvalue static!");

	/* 'static' qualifier makes variable static regardless of scope */
	if(es->qualifiers & EEL_QSTATIC)
		vk = EVK_STATIC;

	do_getvar(es, declare_var(es, es->lval.v.s.buf, vk), al);
	eel_lex(es, 0);

	qualifiers_handled(es, EEL_QLOCAL | EEL_QUPVALUE |
			EEL_QSHADOW | EEL_QSTATIC);
	return TK(SIMPLEXP);
}


/*----------------------------------------------------------
	tablector rule
----------------------------------------------------------*/
/*
	tabitem:
		  '(' expression ',' expression ')'
		| '.' NAME expression
		| expression expression
		| funcdecl
		;

	tabitemlist:
		  tabitem
		| tabitem ',' tabitemlist
		;

	ctor:
		...
		| KW_TABLE '[' tabitemlist ']'
		...
*/
static int tablector(EEL_state *es, EEL_mlist *al)
{
	int r, lastcount;
	EEL_coder *cdr;
	EEL_mlist *inits;

	/* "header" */
	switch(es->token)
	{
	  case '[':
		eel_lex(es, 0);
		break;
	  default:
		return TK(WRONG);
	}

	no_qualifiers(es);

	/* Initializer tuples */
	r = eel_m_result(al);
	cdr = es->context->coder;
	inits = eel_ml_open(cdr);
	while(1)
	{
		int func_by_name = 0;
		if(']' == es->token)
			break;
		lastcount = inits->length;
		if('(' == es->token)
		{
			/* (key, value) syntax */
			eel_lex(es, 0);
			if(TK_WRONG == explist(es, inits, 1) ||
					(inits->length - lastcount != 2))
				eel_cerror(es, "Expected (key, value) tuple!");
			expect(es, ')', NULL);
		}
		else if(TK_WRONG != funcdef(es, inits, 1))
		{
			EEL_manipulator *fm = eel_ml_get(inits, -1);
			EEL_object *fo = fm->v.constant.v.objref.v;
			EEL_function *f = o2EEL_function(fo);
			eel_m_object(inits, f->common.name);
			eel_m_transfer(fm, inits);	/* Move last! */
			func_by_name = 1;
		}
		else
		{
			/* '.' NAME <value> ? */
			if(es->token == '.')
			{
				EEL_value v;
				EEL_symbol *st = es->context->symtab;
				es->context->symtab = NULL;
				eel_lex(es, ELF_LOCALS_ONLY);
				if(es->token != TK_NAME)
					eel_cerror(es, "Expected name!");
				v_set_string(es, es->lval.v.s.buf, &v);
				eel_m_constant(inits, &v);
				eel_v_disown_nz(&v);
				es->context->symtab = st;
				eel_lex(es, 0);
			}
			else
				/* <key> <value> syntax */
				if(TK_WRONG == expression(es, inits, 1) ||
						(inits->length - lastcount != 1))
					eel_cerror(es, "Expected key expression!");
			if(TK_WRONG == expression(es, inits, 1) ||
					(inits->length - lastcount != 2))
				eel_cerror(es, "Expected value expression!");
		}
		if(func_by_name)
		{
			/* Coma is optional after "function by name"! */
			if(',' == es->token)
				eel_lex(es, 0);
		}
		else
		{
			if(',' != es->token)
				break;
			eel_lex(es, 0);
		}
	}

	/* Closing brace */
	expect(es, ']', NULL);

	if(inits->length & 1)
		eel_ierror(es, "tablector somehow generated an odd"
				" number of initializers!");

	/* Generate actual constructor code */
	eel_ml_push(inits);
	eel_codeAB(cdr, EEL_ONEW_AB, r, EEL_CTABLE);
	eel_ml_close(inits);

	return TK(SIMPLEXP);
}


/*----------------------------------------------------------
	ctor rule
----------------------------------------------------------*/
/*
	ctor:
		  '[' explist ']'
		| KW_TABLE '[' tabitemlist ']'
		| TYPENAME '[' explist ']'
		;
*/
static int ctor(EEL_state *es, EEL_mlist *al)
{
	int r, t;
	EEL_mlist *inits;
	EEL_coder *cdr = es->context->coder;

	/* TYPENAME */
	if(TK_SYM_CLASS == es->token)
	{
		t = eel_class_typeid(es->lval.v.symbol->v.object);
		eel_lex(es, 0);
		if(es->token != '[')
		{
			eel_unlex(es);
			return TK(WRONG);
		}
	}
	else
	{
		if(es->token != '[')
			return TK(WRONG);
		t = EEL_CARRAY;
	}
	check_constructor(es, t);
	switch(t)
	{
	  case EEL_CTABLE:
		return tablector(es, al);
	  default:
		/* gctor */
		break;
	}
	eel_lex(es, 0);

	no_qualifiers(es);

	/* Initializers */
	r = eel_m_result(al);
	inits = eel_ml_open(cdr);
	if(es->token != ']')
		if(TK_WRONG == explist(es, inits, 1))
			eel_cerror(es, "Expected list of initializers!");

	/* Closing bracket */
	expect(es, ']', NULL);

	/* Generate actual constructor code */
	eel_ml_push(inits);
	eel_codeAB(cdr, EEL_ONEW_AB, r, t);
	eel_ml_close(inits);

	return TK(SIMPLEXP);
}


/*----------------------------------------------------------
	simplexp rule
----------------------------------------------------------*/
/*
	simplexp:
		  INUM
		| RNUM
		| '-' INUM
		| '-' RNUM
		| KW_TRUE
		| KW_FALSE
		| KW_NIL
		| STRING
		| CONSTANT
		| VARIABLE
		| KW_EXCEPTION
		| '#' KW_ARGUMENTS
		| '#' KW_TUPLES
		| '(' explist ')'
		| '(' TYPENAME ')' simplexp
		| call
		| vardecl
		| funcdecl
		| funcdef
		| arginfo
		| ctor
		| TYPENAME
		| simplexp '[' expression ']'
		| simplexp '.' field
		| simplexp '.' '(' fieldlist ')'
		| simplexp '(' explist ')'
		| simplexp ':' NAME '(' explist ')'
		;
*/
/* Handles the simplexp rule, except for indexing. */
static int simplexp2(EEL_state *es, EEL_mlist *al, int wantresult)
{
	int res;
	EEL_value v;
	EEL_coder *cdr = es->context->coder;
	switch(es->token)
	{
	  case TK_INUM:
		no_qualifiers(es);
		v.type = EEL_TINTEGER;
		v.integer.v = es->lval.v.i;
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_RNUM:
		no_qualifiers(es);
		v.type = EEL_TREAL;
		v.real.v = es->lval.v.r;
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_SYM_OPERATOR:
	  {
		/* Handle negative number literals right here! */
		EEL_operator *op = es->lval.v.symbol->v.op;
		if(op->un_op != EEL_OP_NEG)
			return TK(WRONG);
		eel_lex(es, 0);
		switch(es->token)
		{
		  case TK_INUM:
			no_qualifiers(es);
			v.type = EEL_TINTEGER;
			v.integer.v = -es->lval.v.i;
			eel_m_constant(al, &v);
			eel_lex(es, 0);
			return TK(SIMPLEXP);
		  case TK_RNUM:
			no_qualifiers(es);
			v.type = EEL_TREAL;
			v.real.v = -es->lval.v.r;
			eel_m_constant(al, &v);
			eel_lex(es, 0);
			return TK(SIMPLEXP);
		}
		eel_unlex(es);
		return TK(WRONG);
	  }

	  case TK_KW_TRUE:
		no_qualifiers(es);
		v.type = EEL_TBOOLEAN;
		v.integer.v = 1;
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_KW_FALSE:
		no_qualifiers(es);
		v.type = EEL_TBOOLEAN;
		v.integer.v = 0;
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_KW_NIL:
		no_qualifiers(es);
		v.type = EEL_TNIL;
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_STRING:
		no_qualifiers(es);
		v_set_string_n(es, es->lval.v.s.buf, es->lval.v.s.len, &v);
		eel_m_constant(al, &v);
		eel_v_disown_nz(&v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_SYM_CONSTANT:
		no_qualifiers(es);
		eel_m_constant(al, &es->lval.v.symbol->v.value);
		eel_lex(es, 0);
		return TK(SIMPLEXP);

	  case TK_SYM_VARIABLE:
		if(es->qualifiers & EEL_QLOCAL)
			eel_cerror(es, "'%s' is already declared in the"
					" current scope!",
					eel_o2s(es->lval.v.symbol->name));
		q_allow_only(es, EEL_QUPVALUE, "Only qualifier allowed "
				"here is 'upvalue', unless 'shadow' is used!");
		do_getvar(es, es->lval.v.symbol, al);
		eel_lex(es, 0);
		qualifiers_handled(es, EEL_QUPVALUE);
		return TK(SIMPLEXP);

	  case TK_KW_EXCEPTION:
	  {
		EEL_context *fctx = eel_find_function_context(es->context);
		if(!fctx)
			eel_ierror(es, "'exception' outside function context!?");
		if(!(fctx->flags & ECTX_CATCHER))
			eel_cerror(es, "'exception' used outside 'except' block!");
		no_qualifiers(es);
		eel_m_register(al, 0);
		eel_lex(es, 0);
		return TK(SIMPLEXP);
	  }

	  /*
	   * Argument expansion expressions
	   */
	  case '#':
	  {
		no_qualifiers(es);
		eel_lex(es, 0);
		switch(es->token)
		{
		  case TK_KW_ARGUMENTS:
			eel_m_args(al);
			eel_lex(es, 0);
			return TK(SIMPLEXP);
		  case TK_KW_TUPLES:
			eel_m_tupargs(al);
			eel_lex(es, 0);
			return TK(SIMPLEXP);
		  default:
			eel_cerror(es, "Invalid argument expansion expression!");
		}
	  }

	  /*
	   *   '(' explist ')'
	   * | '(' typeid ')' simplexp
	   */
	  case '(':
	  {
		EEL_manipulator *lastm;
		no_qualifiers(es);
		eel_lex(es, 0);
		res = explist(es, al, 1);
		if(TK_WRONG == res)
			eel_cerror(es, "Expected expression list or type name!");
		expect(es, ')', "Missing ')' in expression.");
		if(al->length)
			lastm = eel_ml_get(al, -1);
		else
			lastm = NULL;
		if(lastm && (EEL_MCONSTANT == lastm->kind) &&
				(EEL_TTYPEID == lastm->v.constant.v.type) &&
				(lastm->v.constant.v.integer.v != EEL_CFUNCTION))
		{
			int i;
			EEL_types t = lastm->v.constant.v.integer.v;
			EEL_mlist *src = eel_ml_open(cdr);
			/* FIXME: Temporary operator precedence disabler! */
			if(TK_WRONG == expression2(es,
					EEL_CAST_PRIORITY + 1000, src, 1))
				eel_cerror(es, "Expected expression!");
			if(!src->length)
				eel_cerror(es, "Cast operator gets void operand!");
			check_class(es, t);
			for(i = 0; i < src->length; ++i)
				eel_m_cast(al, eel_ml_get(src, i), t);
			eel_ml_close(src);
			eel_m_delete(lastm);	/* Replaced! */
			return TK(SIMPLEXP);
		}
		if(TK_VOID == res)
			return TK(VOID);
		else
			return TK(SIMPLEXP);
	  }
	}

	/* call */
	switch(call(es, al))
	{
	  case TK_WRONG:
		break;
	  case TK_VOID:
		return TK(VOID);
	  default:
		return TK(SIMPLEXP);
	}

	/* vardecl */
	if(TK_WRONG != vardecl(es, al))
		return TK(SIMPLEXP);

	/* funcdef */
	if(TK_WRONG != funcdef(es, al, 0))
		return TK(SIMPLEXP);

	/* arginfo */
	if(TK_WRONG != arginfo(es, al))
		return TK(SIMPLEXP);

	/* ctor */
	if(TK_WRONG != ctor(es, al))
		return TK(SIMPLEXP);

	/* TYPENAME */
	if(TK_SYM_CLASS == es->token)
	{
		v.type = EEL_TTYPEID;
		v.integer.v = eel_class_typeid(es->lval.v.symbol->v.object);
		eel_m_constant(al, &v);
		eel_lex(es, 0);
		return TK(SIMPLEXP);
	}

	/* Not a simplexp! */
	return TK(WRONG);
}

static int simplexp(EEL_state *es, EEL_mlist *al, int wantresult)
{
	int res;
	EEL_coder *cdr = es->context->coder;
	int a = al->length;

	/* Look for qualifiers for declarations and definitions */
	es->qualifiers = parse_qualifiers(es);

	if(es->qualifiers & EEL_QSHADOW)
	{
		/* Don't look for upvalues! */
		int old_token = es->token;
		eel_relex(es, ELF_LOCALS_ONLY);
		switch(es->token)
		{
		  case TK_SYM_VARIABLE:
			eel_cerror(es, "Cannot shadow a local variable!");
			break;
		  case TK_SYM_FUNCTION:
			eel_cerror(es, "Cannot shadow a local function!");
			break;
		  case TK_INUM:
		  case TK_RNUM:
		  case TK_KW_TRUE:
		  case TK_KW_FALSE:
		  case TK_STRING:
		  case TK_SYM_CONSTANT:
		  case '[':
		  case '<':
			eel_cerror(es, "Incorrect use of 'shadow'!");
			break;
		  default:
			if(es->token == old_token)
				eel_cwarning(es, "Use of 'shadow' to no"
						" effect.");
			break;
		}
	}

	if(es->qualifiers & EEL_QUPVALUE)
	{
		int level;
		if(es->token != TK_SYM_VARIABLE)
			eel_cerror(es, "Can't use 'upvalue' on something that"
					" is not a variable!");
		level = es->context->symtab->uvlevel - es->lval.v.symbol->uvlevel;
		if(!level)
			eel_cerror(es, "Tried to use 'upvalue' on local "
					"variable '%s'!",
					eel_o2s(es->lval.v.symbol->name));
		if(EEL_SUPVALUE == es->lval.v.symbol->type)
			eel_cerror(es, "Variable '%s' is already declared "
					"upvalue!",
					eel_o2s(es->lval.v.symbol->name));
		declare_upvalue(es, es->lval.v.symbol);
		eel_relex(es, ELF_LOCALS_ONLY);
	}

	/* Parse the initial simplexp */
	res = simplexp2(es, al, wantresult);
	if(TK_WRONG == res || al->length <= a)
	{
		/* Don't let stray qualifiers leak outside simplexp()! */
		no_qualifiers(es);
		/* Re-lex with normal scope */
		eel_relex(es, 0);
		return res;
	}

	/*
	 * No out-of-scope declarations, so we don't want any qualifiers here.
	 * If any qualifiers leak from simplexp2(), it's a compiler bug!
	 */
	if(es->qualifiers)
		eel_ierror(es, "simplexp2() leaked qualifiers"
				" without complaining!");

	/* Handle field/member/object indexing */
	while(1)
	{
		switch(es->token)
		{
		  case '[':
		  {
			EEL_mlist *ind = eel_ml_open(cdr);
			EEL_mlist *vals = eel_ml_open(cdr);
			DBGE2(printf("--- simplexp '[' ---\n");)
			eel_lex(es, 0);
			if(TK_VOID == explist(es, ind, 1))
				eel_cerror(es, "Index expression evaluates to"
						" nothing!");
			expect(es, ']', "Expected closing ']' after index "
					"expression!");
			do_index(es, al, ind, vals);
			eel_ml_delete(al, 0, al->length);
			eel_ml_transfer(vals, al);
			eel_ml_close(ind);
			eel_ml_close(vals);
			break;
		  }
		  case '.':
		  case ':':
		  {
			EEL_symbol *st;
			EEL_mlist *ind, *vals;
			int is_membercall = (es->token == ':');
			eel_lex(es, 0);
			if(es->token == TK_SYM_OPERATOR)
			{
				/*
				 * Ok, let's stop here.
				 * Probably an inplace operation statement.
				 */
				eel_unlex(es);
				return TK(SIMPLEXP);
			}
			eel_unlex(es);
			st = es->context->symtab;
			ind = eel_ml_open(cdr);
			DBGE2(printf("--- simplexp '.' ---\n");)
/*
TODO: Point symbol table to namespace for parent object, so we can use the
TODO: VARIABLE and FUNCTION subrules of the field rule. Needs typed variables.
*/
			es->context->symtab = NULL;
			eel_lex(es, ELF_LOCALS_ONLY);
			if(es->token == '(')
			{
				if(is_membercall)
					eel_cerror(es, "Expected name of "
							"member function or "
							"procedure!");
				eel_lex(es, ELF_LOCALS_ONLY);
				if(TK_VOID == fieldlist(es, ind))
					eel_cerror(es, "Field list evaluates"
							" to nothing!");
				expect(es, ')', "Expected ')' after field list, "
						"or ',' to continue it!");
			}
			else
				if(TK_VOID == field(es, ind, 0))
					eel_cerror(es, "Field evaluates to nothing!");
			es->context->symtab = st;
			eel_relex(es, 0);
			vals = eel_ml_open(cdr);
			do_index(es, al, ind, vals);
			if(is_membercall)
			{
				EEL_manipulator *fn, *self;
				DBGE2(printf("--- membercall ---\n");)
				if(al->length != 1 || ind->length != 1)
					eel_cerror(es, "Multiple member calls through "
							"expression lists not "
							"yet implemented!");
				expect(es, '(', "Expected member call argument list!");
				self = eel_ml_get(al, 0);
				fn = eel_ml_get(vals, 0);
				call_member(es, fn, self, al, wantresult);
			}
			else
			{
				/* Replace objects with index results */
				eel_ml_delete(al, 0, al->length);
				eel_ml_transfer(vals, al);
			}
			eel_ml_close(ind);
			eel_ml_close(vals);
			break;
		  }
		  case '(':
		  {
			EEL_manipulator *fn;
			DBGE2(printf("--- simplexp '(' ---\n");)
			eel_lex(es, 0);
			if(al->length != 1)
				eel_cerror(es, "Cannot make method calls on "
						"expression lists!");
			fn = eel_ml_get(al, 0);
			call_indirect(es, fn, al, wantresult);
			eel_m_delete(fn); /* Result(s) replace the funcref! */
			break;
		  }
		  default:
			return TK(SIMPLEXP);
		}
	}
}


/*----------------------------------------------------------
	expression rule
----------------------------------------------------------*/
/*
 * Returns the left priority of the next operator;
 * -1 or -3 if the next token is not an operator,
 * or -4 if the expression is correct but generates
 * no result.
 *
 * The -3 return is used IFF limit == -1, to trap
 * "this is not an expression" on the first level.
 *
 * NOTE:
 *	As of EEL 0.3.7, operator precedence has been removed!
 *	For now, we still have the logic in place, but all it
 *	does is handle chains of unary operators, and issuing
 *	warnings when evaluation would have been different
 *	with EEL 0.3.6 and older.
 */
static int expression3(EEL_state *es, int limit, EEL_mlist *left,
		EEL_mlist *al, int wantresult)
{
	int lpri;
	EEL_coder *cdr = es->context->coder;

	int res = simplexp(es, left, wantresult);
	switch(res)
	{
	  case TK_WRONG:
	  case TK_VOID:
		if(es->token == TK_SYM_OPERATOR)
		{
			int i;
			EEL_mlist *right = eel_ml_open(cdr);
			EEL_operator *op = es->lval.v.symbol->v.op;
			const char *opname = eel_o2s(es->lval.v.symbol->name);
			if(!(op->flags & EOPF_UNARY))
				eel_cerror(es, "No unary version of operator '%s' "
						"available!", opname);
			eel_lex(es, 0);
			/* FIXME: Temporary operator precedence disabler! */
			expression2(es, op->un_pri + 1000, right, 1);
			if(!right->length)
				eel_cerror(es, "No operands for unary operator '%s'!",
						opname);
			for(i = 0; i < right->length; ++i)
				eel_m_op(left, NULL, op->un_op, eel_ml_get(right, i));
			eel_ml_close(right);
			if(op->flags & EOPF_NORESULT)
				return -4;
		}
		else if(res == -4)
			return -4;
		else
			return limit == -1 ? -3 : -1;
	}

	/* Loop or recurse until we're done */
	while(1)
	{
		int i, ri, linc, rinc, count = 0;
		EEL_operator *op;
		EEL_mlist *right;
		const char *opname;
		if(es->token != TK_SYM_OPERATOR)
		{
			eel_ml_transfer(left, al);
			return -1;
		}

		lpri = es->lval.v.symbol->v.op->lpri;
		op = es->lval.v.symbol->v.op;
		opname = eel_o2s(es->lval.v.symbol->name);
		if(!(op->flags & EOPF_BINARY))
			eel_cerror(es, "No binary version of operator '%s' "
					"available!", opname);

		/*
		 * Warn whenever operator precedence would have made a
		 * difference!
		 */
		if(!EEL_NOPRECEDENCE(es) && (limit >= 1000) &&
				((lpri <= (limit - 1000)) != (lpri <= limit)))
			eel_cwarning(es, "EEL <= 0.3.6 would evaluate this "
					"expression differently due to "
					"operator precedence!");
		if(lpri <= limit)
		{
			eel_ml_transfer(left, al);
			return lpri;
		}

		eel_lex(es, 0);
		right = eel_ml_open(cdr);
		/* FIXME: Temporary operator precedence disabler! */
		lpri = expression2(es, op->rpri + 1000, right, 1);
		if(lpri == -4)
			eel_cerror(es, "Subexpression generates no result!");

		/*
		 * Perform the operation, replacing
		 * left hand terms with result.
		 */
		linc = rinc = 1;
		ri = 0;
		if(left->length == 1)
		{
			linc = 0;
			count = right->length;
			if(!count)
				eel_cerror(es, "No right hand operands to "
						"binary operator '%s'!",
						opname);
		}
		else if(right->length == 1)
		{
			rinc = 0;
			count = left->length;
			if(!count)
				eel_cerror(es, "No left hand operands to "
						"binary operator '%s'!",
						opname);
		}
		else if(left->length < right->length)
			eel_cerror(es, "Too few left hand operands to"
					" operator '%s'!\n"
					"(Parsed %d/%d operands.)",
					opname, left->length, right->length);
		else if(left->length > right->length)
			eel_cerror(es, "Too few right hand operands to"
					" operator '%s'!"
					"(Parsed %d/%d operands.)",
					opname, left->length, right->length);
		else
			count = left->length;
		for(i = 0; i < count; ++i)
		{
			EEL_manipulator *lm = eel_ml_get(left, 0);
			eel_m_op(left, lm, op->op, eel_ml_get(right, ri));
			if(linc || (count - 1 == i))
				eel_m_delete(lm);
			ri += rinc;
		}
		eel_ml_close(right);
		if(op->flags & EOPF_NORESULT)
			return -4;
	}
}


static int expression2(EEL_state *es, int limit, EEL_mlist *al, int wantresult)
{
	EEL_mlist *left = eel_ml_open(es->context->coder);
	int res = expression3(es, limit, left, al, wantresult);
	eel_ml_close(left);
	return res;
}


/*
	expression:
		  simplexp
		| OPERATOR expression
		| expression OPERATOR expression
		;
 */
static int expression(EEL_state *es, EEL_mlist *al, int wantresult)
{
	switch(expression2(es, -1, al, wantresult))
	{
	  case -3:
		return TK(WRONG);
	  case -4:
		return TK(VOID);
	}
	return TK(EXPRESSION);
}


/*----------------------------------------------------------
	assignstat rule
----------------------------------------------------------*/
/*
	assignstat:
		  explist ';'
		| explist '=' explist ';'
		| explist '(' '=' ')' explist ';'
		| explist SHORTHAND_OPERATOR explist ';'
		| explist '.' OPERATOR explist ';'
		;
 */
static int assignstat(EEL_state *es)
{
	int i, count, si, di, dsi, ddi;
	EEL_mlist *expr, *src;
	EEL_coder *cdr = es->context->coder;
	EEL_operators op = EEL_OP_ASSIGN;
	const char *ops = "assignment";
	int inplace = 0;

	/*
	 * Parse the (left hand) expression.
	 */
	expr = eel_ml_open(cdr);
	if(TK_WRONG == explist(es, expr, 0))
	{
		eel_ml_close(expr);
		return TK(WRONG);
	}

	/*
	 * In some special cases, we end the statement
	 * without a semicolon.
	 */
	eel_unlex(es);
	if('}' == es->token)
	{
		eel_lex(es, 0);
		eel_ml_close(expr);
		return TK(STATEMENT);
	}
	eel_lex(es, 0);

	/* Just an expression or an assignment? */
	switch(es->token)
	{
	  case TK_SYM_SHORTOP:
		op = es->lval.v.symbol->v.op->op;
		eel_lex(es, 0);
		ops = "shorthand operation";
		break;
	  case '.':
		eel_lex(es, 0);
		if(es->token != TK_SYM_OPERATOR)
			eel_cerror(es, "Expected operator for inplace operation!");
		op = es->lval.v.symbol->v.op->op;
		eel_lex(es, 0);
		inplace = 1;
		ops = "inplace operation";
		break;
	  case '=':
		op = EEL_OP_ASSIGN;
		eel_lex(es, 0);
		break;
	  case TK_WEAKASSIGN:
		op = EEL_OP_WKASSN;
		eel_lex(es, 0);
		ops = "weak assignment";
		break;
	  case ';':
		eel_lex(es, 0);
		for(i = 0; i < expr->length; ++i)
			eel_m_evaluate(eel_ml_get(expr, i));
		eel_ml_close(expr);
		return TK(STATEMENT);
	  default:
		if((expr->length >= 1) && eel_m_writable(eel_ml_get(expr, 0)))
			eel_cerror(es, "Expected '=' or ';'!");
		else
			eel_cerror(es, "Expected ';' or '('!");
	}

	/*
	 * Ok, this looks like an assignment or inplace operation.
	 */
	if(expr->length < 1)
		eel_cerror(es, "No target operand for %s!", ops);

	src = eel_ml_open(cdr);
	switch(explist(es, src, 1))
	{
	  case TK_WRONG:
		eel_cerror(es, "Expected an expression!");
	  case TK_VOID:
		eel_cerror(es, "Expression does not generate a value!");
	}
	if(src->length < 1)
		eel_cerror(es, "No source operand for %s!", ops);
	else if((src->length != 1) && (expr->length != 1))
	{
		if(expr->length > src->length)
			eel_cerror(es, "Too few sources in multiple %s!", ops);
		else if(expr->length < src->length)
			eel_cerror(es, "Too many sources in multiple %s!", ops);
	}

	if((op == EEL_OP_ASSIGN || op == EEL_OP_WKASSN) &&
			(src->length > expr->length))
		eel_cerror(es, "Multiple assignment with fewer targets than "
			       "sources!");
	si = di = 0;
	dsi = src->length > 1;
	ddi = expr->length > 1;
	count = src->length > expr->length ? src->length : expr->length;
	if(inplace)
		for(i = 0; i < count; ++i, si += dsi, di += ddi)
			eel_m_ipoperate(eel_ml_get(src, si), op,
					eel_ml_get(expr, di));
	else
		for(i = 0; i < count; ++i, si += dsi, di += ddi)
			eel_m_operate(eel_ml_get(src, si), op,
					eel_ml_get(expr, di));

	eel_ml_close(src);
	eel_ml_close(expr);

	expect(es, ';', "Missing ';' after %s statement!", ops);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	constdeclstat rule
----------------------------------------------------------*/
/*
	constdeclstat:
		qualifiers KW_CONSTANT NAME '=' expression ';'
		{ expression must be constant, compile time evaluated }
		;
FIXME: This should probably go into simplexp and be more unified with
FIXME: variable declarations, assignstat etc.
 */
static int constdeclstat(EEL_state *es)
{
	EEL_coder *cdr = es->context->coder;
	EEL_symbol *cs;
	EEL_mlist *val;
	char *cname;
	int do_export = 0;
	switch(es->token)
	{
	  case TK_KW_CONSTANT:
		eel_lex(es, 0);
		break;
	  case TK_KW_EXPORT:
		do_export = 1;
		eel_lex(es, 0);
		if(es->token != TK_KW_CONSTANT)
		{
			eel_unlex(es);
			return TK(WRONG);
		}
		eel_lex(es, 0);
		break;
	  default:
		return TK(WRONG);
	}
	if(es->token != TK_NAME)
		eel_cerror(es, "Expected constant name!");
	cname = strdup(es->lval.v.s.buf);
	eel_lex(es, 0);
	expect(es, '=', NULL);
	val = eel_ml_open(cdr);
	switch(expression(es, val, 1))
	{
	  case TK_WRONG:
		eel_cerror(es, "Expression does not generate a value!");
	  case TK_VOID:
		eel_cerror(es, "Expected constant expression!");
	}
	check_argc(val, 1, 1);
	if(!eel_m_is_constant(eel_ml_get(val, 0)))
		eel_cerror(es, "Cannot evaluate constant value of expression!");
	cs = eel_s_add(es, es->context->symtab, cname, EEL_SCONSTANT);
	if(!cs)
		eel_serror(es, "Could not create symbol for constant!");
	eel_m_get_constant(eel_ml_get(val, 0), &cs->v.value);
	if(do_export)
		add_export(es->context->module, &cs->v.value, cname);
	free(cname);
	eel_ml_close(val);
	expect(es, ';', "Missing ';' after constant declaration statement!");
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	body rule
----------------------------------------------------------*/
/*
	body:
		'{' block '}'
		| NAME ':' '{' block '}'
		;

 */
static int body(EEL_state *es, int flags)
{
	int res;
	char name[128];
	int is_function = (flags & ECTX_TYPE) == ECTX_FUNCTION;
	flags &= ~ECTX_TYPE;
	name[0] = 0;
	switch(es->token)
	{
	  case '{':
		break;
	  case TK_NAME:
		strncpy(name, es->lval.v.s.buf, sizeof(name) - 1);
		name[sizeof(name) - 1] = 0;
		eel_lex(es, 0);
		if(es->token != ':')
		{
			eel_unlex(es);
			return TK(WRONG);
		}
		if(is_function)
			eel_cerror(es, "Function body must not be named!");
		eel_lex(es, 0);
		if(es->token != '{')
			eel_cerror(es, "Expected '{'.");
		break;
	  default:
		return TK(WRONG);
	}
	eel_lex(es, 0);

	DBGH(printf("## body: '%s' '{'\n", name[0] ? name : "(unnamed)");)
	if(!is_function)
	{
		char *n = name[0] ? name : eel_unique(es->vm, "__body");
		eel_context_push(es, ECTX_BODY | flags, n);
		if(n != name)
			eel_sfree(es, n);
	}
	es->context->flags |= ECTX_WRAPPED;
	switch(block(es))
	{
	  case TK_EOF:
		eel_cerror(es, "Unexpected EOF; unterminated {...} block!");
	  default:
		res = TK_BODY;
		break;
	}
	if(!is_function && !(flags & ECTX_KEEP))
	{
		code_leave_context(es, es->context);
		eel_context_pop(es);
	}
	es->context->flags &= ~ECTX_WRAPPED;

	expect(es, '}', "Expected new statement or closing '}'.");
	DBGH(printf("## body: '}'\n");)

	return res;
}


/*----------------------------------------------------------
	argdef and argdeflist rules
----------------------------------------------------------*/
/*
	argdef:
		NAME
		| TYPENAME NAME
		;

	argdeflist:
		argdef
		| argdef ',' argdeflist
		;
 */
static int argdeflist(EEL_state *es, EEL_varkinds kind)
{
/*
TODO: Check for arguments shadowing upvalues and other
TODO: higher level objects. Maybe that should even be an
TODO: error? (Shadowing arguments definitely should be.)
*/
	int first = 1;
	while(1)
	{
		switch(es->token)
		{
		  case TK_NAME:
		  {
			declare_var(es, es->lval.v.s.buf, kind);
			DBGH(printf("## argdeflist: NAME\n");)
			eel_lex(es, ELF_LOCALS_ONLY | ELF_NO_OPERATORS);
			break;
		  }
		  case TK_SYM_CLASS:
			eel_cerror(es, "Typed arguments not yet implemented!");
		  case TK_SYM_VARIABLE:
			eel_cerror(es, "There already is a result or argument"
					" named '%s'!",
					eel_o2s(es->lval.v.symbol->name));
		  case TK_SYMBOL:
		  case TK_SYM_CONSTANT:
		  case TK_SYM_FUNCTION:
		  case TK_SYM_OPERATOR:
		  case TK_SYM_PARSER:
		  case TK_SYM_BODY:
			eel_cerror(es, "Result/argument name '%s'"
					" is taken by %s.",
					eel_o2s(es->lval.v.symbol->name),
					eel_symbol_is(es->lval.v.symbol));
		  default:
			if(first)
				return TK(WRONG);
			else
				eel_cerror(es, "Incorrect result/argument"
						" declaration.");
		}
		first = 0;
		if(es->token != ',')
			return TK(ARGDEFLIST);
		eel_lex(es, ELF_LOCALS_ONLY | ELF_NO_OPERATORS);
	}
}


/*----------------------------------------------------------
	filelist rule
----------------------------------------------------------*/
/*
	filelist:
		STRING
		| STRING ',' filelist
		;
*/
static int filelist(EEL_state *es, void (*handler)(EEL_state *es, int flags),
		int flags)
{
	while(1)
	{
		switch(es->token)
		{
		  case TK_STRING:
			handler(es, flags);
			break;
		  default:
			eel_cerror(es, "Expected a string literal.");
		}
		if(es->token != ',')
			return TK(FILELIST);
		eel_lex(es, 0);
	}
}


/*----------------------------------------------------------
	ifstat rule
----------------------------------------------------------*/
/*
	ifstat:
		  KW_IF expression statement
		| KW_IF expression statement KW_ELSE statement
		;
*/
static int ifstat(EEL_state *es)
{
	EEL_mlist *expr;
	int jump_false, jump_out, r;
	EEL_coder *cdr = es->context->coder;
	if(TK_KW_IF != es->token)
		return TK(WRONG);
	eel_lex(es, 0);

	/* Expression and conditional jump */
	expr = eel_ml_open(cdr);
	switch(expression(es, expr, 1))
	{
	  case TK_VOID:
		eel_cerror(es, "Test expression does not generate a result!");
	  case TK_WRONG:
		eel_cerror(es, "Expected an expression!");
	}
	check_argc(expr, 1, 1);
	r = eel_m_direct_read(eel_ml_get(expr, 0));
	if(r >= 0)
		jump_false = eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, r, 0);
	else
	{
		r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(eel_ml_get(expr, 0), r);
		jump_false = eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, r, 0);
		eel_r_free(cdr, r, 1);
	}
	eel_ml_close(expr);

	/* Code for "true" */
	if(statement(es, ECTX_CONDITIONAL | ECTX_KEEP) != TK_STATEMENT)
		eel_cerror(es, "Expected 'true' condition statement!");
	code_leave_context(es, es->context);
	if(TK_KW_ELSE == es->token)
	{
		/*
		 * Skip else section if expression is true.
		 * (We want this inside the conditional for
		 * dead code elimination.)
		 */
		jump_out = eel_codesAx(cdr, EEL_OJUMP_sAx, 0);
	}
	else
		jump_out = -1;
	eel_context_pop(es);

	/* else section? */
	eel_code_setjump(cdr, jump_false, eel_code_target(cdr));
	if(TK_KW_ELSE != es->token)
		eel_e_merge(es->context, EEL_EMAYBE);
	else
	{
		/* 'else' block */
		eel_lex(es, 0);
		if(statement(es, ECTX_CONDITIONAL) != TK_STATEMENT)
			eel_cerror(es, "Expected 'false' condition statement!");

		eel_code_setjump(cdr, jump_out, eel_code_target(cdr));
		eel_e_merge(es->context, EEL_EYES);
	}

	/* Check for "maybe initialized" right away... */
	eel_initializations(es->context);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	switchstat rule
----------------------------------------------------------*/
/*
	case:
		KW_CASE explist statement
		;

	caselist:
		  case
		| case caselist
		;
*/
static void caselist(EEL_state *es, EEL_object *jtab)
{
	EEL_coder *cdr = es->context->coder;
	while(es->token == TK_KW_CASE)
	{
		int i;
		EEL_mlist *values = eel_ml_open(cdr);
		EEL_value posv;
		eel_lex(es, 0);
		if(TK_WRONG == explist(es, values, 1) || values->length < 1)
			eel_cerror(es, "Expected case value(s)!");
		eel_l2v(&posv, eel_code_target(cdr));
		for(i = 0; i < values->length; ++i)
		{
			EEL_manipulator *m = eel_ml_get(values, i);
			EEL_value vv;
			EEL_value *kv;
			if(m->kind != EEL_MCONSTANT)
				eel_cerror(es, "Case value must be a constant!");
			kv = &m->v.constant.v;
			if(!eel_table_get(jtab, kv, &vv))
				eel_cerror(es, "Case value '%s' already"
						" handled in switch!",
						eel_v_stringrep(es->vm, kv));
			if(eel_table_set(jtab, kv, &posv))
				eel_ierror(es, "Could not add case value '%s'"
						" to switch jump table!",
						eel_v_stringrep(es->vm, kv));
		}
		eel_ml_close(values);
		if(statement(es, ECTX_CONDITIONAL | ECTX_KEEP) != TK_STATEMENT)
			eel_cerror(es, "Expected 'case' body statement!");
		code_break(es, es->context);
		code_move_breaks_up(es);
		eel_context_pop(es);
	}
}

/*
	switchstat:
		  KW_SWITCH expression caselist
		| KW_SWITCH expression caselist KW_DEFAULT statement
		;
*/
static int switchstat(EEL_state *es)
{
	EEL_mlist *expr;
	int jump_else, r, jtabc;
	char *n;
	EEL_object *jtab;
	EEL_coder *cdr = es->context->coder;
	EEL_value v;
	EEL_xno x;
	if(TK_KW_SWITCH != es->token)
		return TK(WRONG);
	eel_lex(es, 0);

	/* Warning for putting a switch directly inside a case! */
	if((es->context->creator == switchstat) &&
			!(es->context->flags & ECTX_WRAPPED))
		eel_cerror(es, "'switch' directly inside 'case' not allowed! "
				"Please enclose in braces. ('{...}')");

	/*
	 * Push switch wrapper context.
	 * (We need this to avoid mixing our implicit "case breaks" with breaks
	 * that belong to outer contexts!)
	 */
	n = eel_unique(es->vm, "__switchstat");
	eel_context_push(es, ECTX_BLOCK, n);
	es->context->creator = switchstat;
	eel_sfree(es, n);

	/* Create jump table */
	x = eel_o_construct(es->vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		eel_ierror(es, "Could not construct switch() jump table!");
	jtab = v.objref.v;
	jtabc = eel_coder_add_constant(cdr, &v);
	eel_o_disown(&jtab);

	/* Expression and jump via table */
	expr = eel_ml_open(cdr);
	switch(expression(es, expr, 1))
	{
	  case TK_VOID:
		eel_cerror(es, "Switch expression does not generate a result!");
	  case TK_WRONG:
		eel_cerror(es, "Expected an expression!");
	}
	check_argc(expr, 1, 1);
	r = eel_m_direct_read(eel_ml_get(expr, 0));
	if(r >= 0)
		jump_else = eel_codeABxsCx(cdr, EEL_OSWITCH_ABxsCx, r, jtabc, 0);
	else
	{
		r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(eel_ml_get(expr, 0), r);
		jump_else = eel_codeABxsCx(cdr, EEL_OSWITCH_ABxsCx, r, jtabc, 0);
		eel_r_free(cdr, r, 1);
	}
	eel_ml_close(expr);

	caselist(es, jtab);

	/* default section? */
	eel_code_setjump(cdr, jump_else, eel_code_target(cdr));
#if 0
//FIXME: This should be taken out some time soon... Also note that this will
//FIXME: NOT warn about accidental 'else' in an 'if' statement inside a 'switch',
//FIXME: which is where the actual danger is...!
	if(es->token == TK_KW_ELSE)
	{
		eel_cwarning(es, "Deprecated use of 'else' in 'switch' "
				"statement! Please use 'default' instead.");
		es->token = TK_KW_DEFAULT;
	}
#endif
	if(es->token == TK_KW_DEFAULT)
	{
		eel_lex(es, 0);
		if(statement(es, ECTX_CONDITIONAL | ECTX_KEEP) != TK_STATEMENT)
			eel_cerror(es, "Expected 'default' catch statement!");
		code_move_breaks_up(es);
		eel_context_pop(es);
		eel_e_merge(es->context, EEL_EYES);
	}
	else
		eel_e_merge(es->context, EEL_EMAYBE);

	/* Wire all case exit jumps to jump here. */
	code_fixup_breaks(es);

	/* Pop switch wrapper context */
	eel_context_pop(es);
	eel_e_merge(es->context, EEL_EYES);
	eel_relex(es, 0);

	/* Check for misplaced 'case', for less nonsensical error messages
	 * when accidentally placing 'default' anywhere but last!
	 */
	if(es->token == TK_KW_CASE)
		eel_cerror(es, "Misplaced 'case'! ('default' must come last in "
				"a 'switch'.)");

	/* Check for "maybe initialized" right away... */
	eel_initializations(es->context);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	whilestat rule
----------------------------------------------------------*/
/*
	whilestat:
		KW_WHILE expression statement
		;
*/
static int whilestat(EEL_state *es)
{
	EEL_mlist *expr;
	EEL_coder *cdr = es->context->coder;
	int loop_start, jump_out, pos;
	EEL_cestate evstate;
	if(TK_KW_WHILE != es->token)
		return TK(WRONG);
	eel_lex(es, 0);

	/* Grab the loop start point! */
	loop_start = eel_code_target(cdr);

	/* Expression and conditional jump */
	expr = eel_ml_open(cdr);
	switch(expression(es, expr, 1))
	{
	  case TK_VOID:
		eel_cerror(es, "Test expression does not generate a result!");
	  case TK_WRONG:
		eel_cerror(es, "Expected a test expression!");
	}
	check_argc(expr, 1, 1);
	switch(eel_m_direct_bool(eel_ml_get(expr, 0)))
	{
	  case 0:
		jump_out = eel_codesAx(cdr, EEL_OJUMP_sAx, 0);
		evstate = EEL_ENO;
		break;
	  case 1:
		jump_out = -1;
		evstate = EEL_EYES;
		break;
	  default:
	  {
		int r = eel_m_direct_read(eel_ml_get(expr, 0));
		if(r >= 0)
			jump_out = eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, r, 0);
		else
		{
			r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(eel_ml_get(expr, 0), r);
			jump_out = eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, r, 0);
			eel_r_free(cdr, r, 1);
		}
		evstate = EEL_EMAYBE;
		break;
	  }
	}
	eel_ml_close(expr);

	/* Loop body */
	if(statement(es, ECTX_CONDITIONAL | ECTX_BREAKABLE | ECTX_CONTINUABLE |
			ECTX_KEEP)
			!= TK_STATEMENT)
		eel_cerror(es, "Expected loop body statement!");
	code_leave_context(es, es->context);

	/* Continue jumps land here */
	code_fixup_continuations(es);

	/* Loop jump */
	pos = eel_codesAx(cdr, EEL_OJUMP_sAx, 0);
	eel_code_setjump(cdr, pos, loop_start);

	/* Stop jump and breaks land here */
	eel_code_setjump(cdr, jump_out, eel_code_target(cdr));
	code_fixup_breaks(es);

	/* Close the loop body context */
	eel_context_pop(es);

	eel_e_merge(es->context, evstate);

	/* Allow no "maybe initialized" variables! */
	eel_initializations(es->context);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	dostat rule
----------------------------------------------------------*/
/*
	dostat:
		  KW_DO statement KW_WHILE expression
		| KW_DO statement KW_UNTIL expression
		;
*/
static int dostat(EEL_state *es)
{
	EEL_mlist *expr;
	EEL_coder *cdr = es->context->coder;
	int loop_start, r, jumpins, loopjump;
	if(TK_KW_DO != es->token)
		return TK(WRONG);
	eel_lex(es, 0);

	/* Grab the loop start point! */
	loop_start = eel_code_target(cdr);

	/* Loop body */
	if(statement(es, ECTX_CONDITIONAL | ECTX_BREAKABLE | ECTX_CONTINUABLE |
			ECTX_KEEP)
			!= TK_STATEMENT)
		eel_cerror(es, "Expected loop body statement!");
	code_leave_context(es, es->context);

	/* Continue jumps land here */
	code_fixup_continuations(es);

	/* 'while' or 'until'? */
	switch(es->token)
	{
	  case TK_KW_UNTIL:
		jumpins = EEL_OJUMPZ_AsBx;
		break;
	  case TK_KW_WHILE:
		jumpins = EEL_OJUMPNZ_AsBx;
		break;
	  default:
		eel_cerror(es, "Expected 'until' or 'while'!");
	}
	eel_lex(es, 0);

	/* Expression and conditional jump */
	expr = eel_ml_open(cdr);
	switch(expression(es, expr, 1))
	{
	  case TK_VOID:
		eel_cerror(es, "Test expression does not generate a result!");
	  case TK_WRONG:
		eel_cerror(es, "Expected a test expression!");
	}
	check_argc(expr, 1, 1);
	r = eel_m_direct_read(eel_ml_get(expr, 0));
	if(r >= 0)
		loopjump = eel_codeAsBx(cdr, jumpins, r, 0);
	else
	{
		r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(eel_ml_get(expr, 0), r);
		loopjump = eel_codeAsBx(cdr, jumpins, r, 0);
		eel_r_free(cdr, r, 1);
	}
	eel_code_setjump(cdr, loopjump, loop_start);
	eel_ml_close(expr);

	/* Breaks land here */
	code_fixup_breaks(es);

	/* Close the loop body context */
	eel_context_pop(es);

	eel_e_merge(es->context, EEL_EMULTIPLE);

	/* Allow no "maybe initialized" variables! */
	eel_initializations(es->context);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	forstat rule
----------------------------------------------------------*/
/*
	forstat:
TODO:		  KW_FOR expression KW_IN explist statement
		| KW_FOR expression '=' expression ','  expression
				statement
		| KW_FOR expression '=' expression ','  expression
				','  expression statement
		;
*/
static int forstat(EEL_state *es)
{
	char *n;
	EEL_mlist *iter, *params;
	EEL_coder *cdr = es->context->coder;
	int preloop, loopstart, loopjump;
	int i, limit, incr;
	if(TK_KW_FOR != es->token)
		return TK(WRONG);
	eel_lex(es, 0);

	/* Wrap! We want the iterator to be local if it's declared here. */
	n = eel_unique(es->vm, "__forstat");
	eel_context_push(es, ECTX_BODY, n);
	eel_sfree(es, n);

	/* Parse the iterator declaration */
	iter = eel_ml_open(cdr);
	if(TK_WRONG == expression(es, iter, 1))
	{
		eel_ml_close(iter);
		eel_context_pop(es);
		return TK(WRONG);
	}
	if(iter->length < 1)
		eel_cerror(es, "No iterator variable!");

	expect(es, '=', NULL);

	/* Parse the iteration parameters */
	params = eel_ml_open(cdr);
	switch(explist(es, params, 1))
	{
	  case TK_WRONG:
	  case TK_VOID:
		eel_cerror(es, "Expected iteration parameters!");
	}
	if((params->length < 2) || (params->length > 3))
		eel_cerror(es, "'for' needs 2 or 3 parameters!");

	/* Initialize the iteration variable */
	eel_m_copy(eel_ml_get(params, 0), eel_ml_get(iter, 0));
	i = eel_m_direct_read(eel_ml_get(iter, 0));
	if(i < 0)
		eel_cerror(es, "Iterator variable must be a local variable!");

	/* Set up limit and increment registers */
	limit = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
	incr = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
	eel_m_read(eel_ml_get(params, 1), limit);
	if(params->length >= 3)
		eel_m_read(eel_ml_get(params, 2), incr);
	else
		eel_codeAsBx(cdr, EEL_OLDI_AsBx, incr, 1);

	/* Cast values and handle the "end is before start" case */
	preloop = eel_codeABCsDx(cdr, EEL_OPRELOOP_ABCsDx, i, incr, limit, 0);

	/* Grab the loop start point! */
	loopstart = eel_code_target(cdr);

	/* Loop body */
	if(statement(es, ECTX_CONDITIONAL | ECTX_BREAKABLE | ECTX_CONTINUABLE |
			ECTX_REPEATABLE | ECTX_KEEP)
			!= TK_STATEMENT)
		eel_cerror(es, "Expected loop body statement!");
	code_leave_context(es, es->context);

	/* 'continue' lands here */
	code_fixup_continuations(es);

	/* Update the iteration variable, test and (maybe) loop */
	loopjump = eel_codeABCsDx(cdr, EEL_OLOOP_ABCsDx, i, incr, limit, 0);
	eel_code_setjump(cdr, loopjump, loopstart);
	eel_code_setjump(cdr, preloop, eel_code_target(cdr));

	/* Free registers and stuff */
	eel_r_free(cdr, incr, 1);
	eel_r_free(cdr, limit, 1);
	eel_ml_close(params);
	eel_ml_close(iter);

	/* Breaks land here */
	code_fixup_breaks(es);

	/* Close the loop body context */
	eel_context_pop(es);
	eel_e_merge(es->context, EEL_EMULTIPLE);

	code_leave_context(es, es->context);
	eel_context_pop(es);
	eel_relex(es, 0);
	return TK(STATEMENT);
}



/*----------------------------------------------------------
	breakstat rule
----------------------------------------------------------*/
/*
	breakstat:
		  KW_BREAK
		| KW_BREAK BLOCKNAME
		;
*/
static int breakstat(EEL_state *es)
{
	EEL_context *ctx;
	if(TK_KW_BREAK != es->token)
		return TK(WRONG);

	eel_lex(es, 0);

	if(TK_SYM_BODY == es->token)
	{
		DBGH(printf("## statement: BREAK <blockname>\n");)
		ctx = es->lval.v.symbol->v.context;
		if(!ctx)
			eel_ierror(es, "Named body has NULL context!");
		eel_lex(es, 0);
	}
	else
	{
		DBGH(printf("## statement: BREAK\n");)
		ctx = NULL;
	}
	code_break(es, ctx);

	expect(es, ';', "Missing ';' after 'break' statement!");
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	contstat rule
----------------------------------------------------------*/
/*
	contstat:
		  KW_CONTINUE
		| KW_CONTINUE BLOCKNAME
		;
*/
static int contstat(EEL_state *es)
{
	EEL_context *ctx;
	if(TK_KW_CONTINUE != es->token)
		return TK(WRONG);

	eel_lex(es, 0);

	if(TK_SYM_BODY == es->token)
	{
		DBGH(printf("## statement: CONTINUE <blockname>\n");)
		ctx = es->lval.v.symbol->v.context;
		eel_lex(es, 0);
	}
	else
	{
		DBGH(printf("## statement: CONTINUE\n");)
		ctx = find_context_flags(es, ECTX_CONTINUABLE);
		if(!ctx)
			eel_cerror(es, "'continue' outside continuable context!");
	}
	code_next(es, ctx);

	expect(es, ';', "Missing ';' after 'continue' statement!");
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	repeatstat rule
----------------------------------------------------------*/
/*
	repeatstat:
		  KW_REPEAT
		| KW_REPEAT BLOCKNAME
		;
*/
static int repeatstat(EEL_state *es)
{
	EEL_context *ctx;
	if(TK_KW_REPEAT != es->token)
		return TK(WRONG);

	eel_lex(es, 0);

	if(TK_SYM_BODY == es->token)
	{
		DBGH(printf("## statement: REPEAT <blockname>\n");)
		ctx = es->lval.v.symbol->v.context;
		eel_lex(es, 0);
	}
	else
	{
		DBGH(printf("## statement: REPEAT\n");)
		ctx = find_context_flags(es, ECTX_REPEATABLE);
	}
	if(!ctx || !(ctx->flags & ECTX_REPEATABLE))
		eel_cerror(es, "'repeat' outside repeatable context!");
	code_repeat(es, ctx);

	expect(es, ';', "Missing ';' after 'repeate' statement!");
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	returnstat rule
----------------------------------------------------------*/
/*
	returnstat:
		  KW_RETURN ';'
		| KW_RETURN expression ';'
		;
*/
static int returnstat(EEL_state *es)
{
	EEL_mlist *result;
	int rres;
	EEL_coder *cdr = es->context->coder;
	EEL_function *f = o2EEL_function(cdr->f);
	int flags = f->common.flags;
	if(TK_KW_RETURN != es->token)
		return TK(WRONG);

	DBGH(printf("## statement: RETURN\n");)
	eel_lex(es, 0);

	/*
	 * Initialization state MUST be certain at this
	 * point, or the RET* instruction may blow up!
	 */
	eel_initializations(es->context);

	/*
	 * If this function is supposed to return something,
	 * try to evaluate the expression that should be here.
	 */
	result = eel_ml_open(cdr);
	rres = expression(es, result, 1);
	if(flags & EEL_FF_RESULTS)
	{
		EEL_manipulator *m;
		int r;
		if(rres == TK_VOID)
			eel_cerror(es, "Return value evaluates to nothing!");
		if(result->length < 1)
		{
			if(es->token == ';')
				eel_cerror(es, "Function must return a value!");
			eel_cerror(es, "Return value expression syntax error!");
		}
		else if(result->length > 1)
			eel_cerror(es, "Function can only return one value!");
		m = eel_ml_get(result, 0);
		r = eel_m_direct_read(m);
		if(r < 0)
		{
			r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(m, r);
		}
		if(flags & EEL_FF_XBLOCK)
			eel_codeA(cdr, EEL_ORETXR_A, r);
		else
			eel_codeA(cdr, EEL_ORETURNR_A, r);
		eel_e_result(es);
		eel_e_return(es);
		expect(es, ';', "Expected ';' after 'return' statement!");
	}
	else
	{
		if((rres != TK_VOID) && result->length)
			eel_cerror(es, "A procedure cannot return a value!");
		if(flags & EEL_FF_XBLOCK)
			eel_code0(es->context->coder, EEL_ORETX_0);
		else
			eel_code0(es->context->coder, EEL_ORETURN_0);
		eel_e_return(es);
		expect(es, ';', "Expected ';' after procedure return statement.");
	}
	eel_ml_close(result);
	return TK(STATEMENT);
}


/*----------------------------------------------------------
	trystat rule
----------------------------------------------------------*/
/*
	trystat:
		  KW_TRY xblock
		| KW_TRY xblock KW_EXCEPT xblock
		;
*/
static int trystat(EEL_state *es)
{
	int tf, xf;
	EEL_mlist *fl;
	EEL_coder *cdr = es->context->coder;
	if(TK_KW_TRY != es->token)
		return TK(WRONG);

	DBGH(printf("## trystat\n");)
	eel_lex(es, 0);

	fl = eel_ml_open(cdr);
	if(xblock(es, "__try", fl, 0) == TK_WRONG)
		eel_cerror(es, "Expected 'try' block!");
	tf = eel_m_prepare_constant(eel_ml_get(fl, 0));

	if(TK_KW_EXCEPT == es->token)
	{
		eel_lex(es, 0);
		if(xblock(es, "__except", fl, ECTX_CATCHER) == TK_WRONG)
			eel_cerror(es, "Expected 'except' block!");
		xf = eel_m_prepare_constant(eel_ml_get(fl, 1));
	}
	else
	{
		if(xblock(es, "__except", fl, ECTX_CATCHER | ECTX_DUMMY) ==
				TK_WRONG)
			eel_ierror(es, "Could not compile dummy 'except' block!");
		xf = eel_m_prepare_constant(eel_ml_get(fl, 1));
	}

	/*
	 * Must code this before merging the events!
	 * It actually belongs *before* the bodies,
	 * but it's easier getting the operands into
	 * the TRY instruction this way.
	 */
	eel_codeAxBx(cdr, EEL_OTRY_AxBx, xf, tf);

	eel_ml_close(fl);

	/*
	 * Note that since exceptions can abort the 'try'
	 * block at any time (*), most events are useless!
	 * All we care about is the single case where both
	 * blocks definitely end by throwing exceptions.
	 *
	 * (*) An exception can abort anything at any time,
	 *     but it doesn't matter - unless we prevent
	 *     exceptions from aborting the function, which
	 *     is exactly what we're doing here!
	 */
	eel_e_merge(es->context, EEL_EYES);

	return TK(STATEMENT);
}


/*----------------------------------------------------------
	untrystat rule
----------------------------------------------------------*/
/*
	untrystat:
		KW_UNTRY xblock
		;
*/
static int untrystat(EEL_state *es)
{
	int tf;
	EEL_mlist *fl;
	EEL_coder *cdr = es->context->coder;
	if(TK_KW_UNTRY != es->token)
		return TK(WRONG);

	DBGH(printf("## untrystat\n");)
	eel_lex(es, 0);

	fl = eel_ml_open(cdr);
	if(xblock(es, "__untry", fl, 0) == TK_WRONG)
		eel_cerror(es, "Expected 'untry' block!");
	tf = eel_m_prepare_constant(eel_ml_get(fl, 0));

	eel_codeAx(cdr, EEL_OUNTRY_Ax, tf);

	eel_ml_close(fl);

	eel_e_merge(es->context, EEL_EYES);

	return TK(STATEMENT);
}


/*----------------------------------------------------------
	statement rule
----------------------------------------------------------*/
/*
	statement:
		  body
		| EOF
		| KW_END ';'
		| ';'
HACK:		| KW_NOPRECEDENCE ';'
		| KW_INCLUDE filelist ';'
		| importstat
TODO:		| KW_ALWAYS ':'
		| throwstat
		| retrystat
		| ifstat
		| switchstat
		| whilestat
		| dostat
		| forstat
		| breakstat
		| contstat
		| repeatstat
		| returnstat
		| trystat
		| untrystat
		| constdeclstat
		| assignstat
		;
 */
static int statement2(EEL_state *es)
{
	int fwimports = 0;
	switch(es->token)
	{
	  /* EOF */
	  case TK_EOF:
	  case TK_KW_END:
		DBGH(printf("## statement: EOF\n");)
		return TK(EOF);
	  /* ';' */
	  case ';':
		eel_lex(es, 0);
		DBGH(printf("## statement: ';'\n");)
		return TK(STATEMENT);
	  /* KW_NOPRECEDENCE ';' */
	  case TK_KW_NOPRECEDENCE:
		eel_lex(es, 0);
		es->context->bio->noprecedence = 1;
		expect(es, ';', NULL);
		return TK(STATEMENT);
	  /* KW_INCLUDE filelist ';' */
	  case TK_KW_INCLUDE:
		eel_lex(es, 0);
		filelist(es, include_handler, 0);
		expect(es, ';', "Missing ';' after 'include' statement!");
		DBGH(printf("## statement: KW_INCLUDE filelist ';'\n");)
		return TK(STATEMENT);
	  /* [KW_EXPORT] KW_IMPORT filelist ';' */
	  case TK_KW_EXPORT:
		eel_lex(es, 0);
		if(es->token != TK_KW_IMPORT)
		{
			eel_unlex(es);
			break;
		}
		fwimports = 1;
		/* Fall through! */
	  case TK_KW_IMPORT:
		eel_lex(es, 0);
		filelist(es, import_handler, fwimports);
		expect(es, ';', "Missing ';' after 'import' statement!");
		DBGH(printf("## statement: KW_IMPORT filelist ';'\n");)
		return TK(STATEMENT);
	  /* Some nice error checking */
	  case TK_SYM_CLASS:
	  	if((EEL_classes)eel_class_typeid(es->lval.v.symbol->v.object) !=
				EEL_CMODULE)
			break;
		eel_cerror(es, "Cannot declare a module within a module!");
	  /* throwstat */
	  case TK_KW_THROW:
	  {
		int r;
		EEL_manipulator *m;
		EEL_coder *cdr = es->context->coder;
		EEL_mlist *al = eel_ml_open(cdr);
		eel_lex(es, 0);
		if(TK_WRONG == expression(es, al, 1) || (al->length != 1))
			eel_cerror(es, "Expected exception value!");
		m = eel_ml_get(al, 0);
		r = eel_m_direct_read(m);
		if(r >= 0)
			eel_codeA(cdr, EEL_OTHROW_A, r);
		else
		{
			r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(m, r);
			eel_codeA(cdr, EEL_OTHROW_A, r);
			eel_r_free(cdr, r, 1);
		}
		eel_ml_close(al);
		eel_e_return(es); /* Remember, 'try' blocks are functions! */
		eel_e_result(es); /* Not quite true, but equivalent. */
		expect(es, ';', NULL);
		return TK(STATEMENT);
	  }
	  /* retrystat */
	  case TK_KW_RETRY:
		eel_lex(es, 0);
		eel_code0(es->context->coder, EEL_ORETRY_0);
		expect(es, ';', NULL);
		return TK(STATEMENT);
	}

	/* ifstat */
	if(TK_WRONG != ifstat(es))
		return TK(STATEMENT);

	/* switchstat */
	if(TK_WRONG != switchstat(es))
		return TK(STATEMENT);

	/* whilestat */
	if(TK_WRONG != whilestat(es))
		return TK(STATEMENT);

	/* dostat */
	if(TK_WRONG != dostat(es))
		return TK(STATEMENT);

	/* forstat */
	if(TK_WRONG != forstat(es))
		return TK(STATEMENT);

	/* breakstat */
	if(TK_WRONG != breakstat(es))
		return TK(STATEMENT);

	/* contstat */
	if(TK_WRONG != contstat(es))
		return TK(STATEMENT);

	/* repeatstat */
	if(TK_WRONG != repeatstat(es))
		return TK(STATEMENT);

	/* returnstat */
	if(TK_WRONG != returnstat(es))
		return TK(STATEMENT);

	/* trystat */
	if(TK_WRONG != trystat(es))
		return TK(STATEMENT);

	/* untrystat */
	if(TK_WRONG != untrystat(es))
		return TK(STATEMENT);

	/* constdeclstat */
	if(TK_WRONG != constdeclstat(es))
		return TK(STATEMENT);

	/* assignstat */
	if(TK_WRONG != assignstat(es))
		return TK(STATEMENT);

	/* Not a statement! */
	return TK(WRONG);
}


static int statement(EEL_state *es, int flags)
{
	int res;

	/* This wraps itself if needed */
	if(TK_WRONG != body(es, flags))
		return TK(STATEMENT);

	/* Wrap if needed */
	if(flags & ECTX_CONDITIONAL)
	{
		char *n = eel_unique(es->vm, "__condstat");
		eel_context_push(es, flags | ECTX_BLOCK, n);
		eel_sfree(es, n);
	}

	/* Do the actual parsing */
	res = statement2(es);

	/* Unwrap */
	if((flags & ECTX_CONDITIONAL) && !(flags & ECTX_KEEP))
	{
		code_leave_context(es, es->context);
		eel_context_pop(es);
	}
	return res;
}


/*----------------------------------------------------------
	block rule
----------------------------------------------------------*/
/*
	block:
		<empty>
		| statement
		| statement block
		;
 */
static int block(EEL_state *es)
{
	while(1)
		switch(statement(es, 0))
		{
		  case TK_WRONG:
			return TK(BLOCK);
		  case TK_EOF:
			return TK(EOF);
		}
}


/*----------------------------------------------------------
	Interface
----------------------------------------------------------*/

static void add_export(EEL_object *mo, EEL_value *xv, const char *name)
{
	EEL_xno x;
	EEL_vm *vm = mo->vm;
	EEL_state *es = VMP->state;
	EEL_module *m = o2EEL_module(mo);
	x = eel_table_sets(m->exports, name, xv);
	if(x)
		eel_ierror(es, "Could not add '%s' to exports!",
				name);
	DBGX2(printf("| Added '%s' to module '%s' exports.\n",
			name, eel_module_modname(mo));)
}


static void init_exports(EEL_state *es, EEL_object *mo)
{
	EEL_symbol *s;
	EEL_finder ef;
	EEL_value xv;
	eel_finder_init(es, &ef, es->context->symtab, ESTF_TYPES);
	ef.types = EEL_SMFUNCTION;
	DBGX2(printf(".-------------------------------------------------------------\n");)
	xv.type = EEL_TOBJREF;
	xv.objref.v = es->context->coder->f;
	add_export(mo, &xv, "__init_module");
	DBGX2(printf("| Added root function to module '%s' exports.\n",
			eel_module_modname(mo));)
	while((s = eel_finder_go(&ef)))
	{
		EEL_function *f = o2EEL_function(s->v.object);
		if(!(f->common.flags & EEL_FF_EXPORT))
			continue;	/* Only exported functions! */
		if(f->common.module != mo)
			continue;	/* Don't let imports leak through! */
		xv.objref.v = s->v.object;
		add_export(mo, &xv, eel_o2s(s->name));
	}
	DBGX2(printf("'-------------------------------------------------------------\n");)
}


static void check_declarations(EEL_state *es, EEL_object *mo)
{
	EEL_module *m = o2EEL_module(mo);
	EEL_object_p_da *a = &m->objects;
	int i;
	for(i = 0; i < a->size; ++i)
	{
		EEL_object *o = a->array[i];
		EEL_function *f;
		if((EEL_classes)o->type != EEL_CFUNCTION)
			continue;
		f = o2EEL_function(o);
		if(f->common.flags & EEL_FF_DECLARATION)
			eel_cerror(es, "Function '%s' declared but not defined!",
					eel_o2s(f->common.name));
	}
}


static void declare_environment(EEL_state *es)
{
	EEL_value v;
	EEL_symbol *s = eel_s_add(es, es->context->symtab, "$", EEL_SVARIABLE);
	if(!s)
		eel_serror(es, "Could not create 'environment' variable!");
	s->v.var.kind = EVK_STATIC;
	eel_o2v(&v, es->environment);
	s->v.var.location = eel_coder_add_variable(es->context->coder, &v);
}


static void compile2(EEL_state *es, EEL_object *mo, EEL_sflags flags)
{
	int res;
	int shared_module = 0;
	EEL_symbol *mst;
	EEL_module *m = o2EEL_module(mo);

	/*
	 * This tells the module GC knows that there can be no actual
	 * dependencies on this module, in case we have to bail out:
	 */
	m->refsum = -1;

	if(eel_lexer_init(es) < 0)
		eel_ierror(es, "Could not initialize lexer!");

	/* Create module context */
	eel_context_push(es, ECTX_MODULE, NULL);
	es->context->symtab->v.object = mo;
	eel_o_own(es->context->symtab->v.object);	/* For symtab */

	if(eel_lexer_start(es, mo) < 0)
		eel_ierror(es, "Compiler failed to start the lexer!");

	/* Named module? */
	if((es->token == TK_SYM_CLASS) &&
			((EEL_classes)eel_class_typeid(es->lval.v.symbol->v.object) ==
					EEL_CMODULE))
	{
		eel_lex(es, 0);
		if(TK_NAME != es->token)
			eel_cerror(es, "Expected module name!");
		eel_s_rename(es, es->context->symtab, es->lval.v.s.buf);
		eel_table_setss(m->exports, "__modname", es->lval.v.s.buf);
		eel_lex(es, 0);
		expect(es, ';', "Missing ';' after module declaration!");
		shared_module = 1;
	}

	/* Create function context */
	eel_context_push(es, ECTX_FUNCTION, NULL);

	/* Prepare to code top-level function */
	declare_func(es, "__init_module", NULL, NULL, 0, NULL);
	declare_func_args(es, 0);
	o2EEL_function(es->context->coder->f)->common.flags |= EEL_FF_ROOT;

	declare_environment(es);
	eel_relex(es, 0);

	/* Go! */
	res = block(es);
	switch(res)
	{
	  case TK_WRONG:
		eel_cerror(es, "Syntax error! Statement expected.");
	  case TK_EOF:
		break;
	}

	/* Finalize the top-level function */
	check_declarations(es, mo);
	init_exports(es, mo);
	procreturn(es);
	eel_coder_finalize_module(es->context->coder);
	eel_context_pop(es);

	/* Leave module context and remove symbols */
	mst = es->context->symtab;
	eel_context_pop(es);
	if(flags & EEL_SF_LIST)
		eel_s_dump(es->vm, NULL, flags & EEL_SF_LISTASM);
	eel_s_free(es, mst);

	eel_lexer_invalidate(es);

	/* If it's a "real" module, add it to the shared module table */
	if(shared_module)
		switch(eel_share_module(mo))
		{
		  case -1:
			eel_ierror(es, "Where did the module name go!?");
		  case -2:
		  {
			EEL_value mn;
			eel_table_gets(m->exports, "__modname", &mn);
			eel_cwarning(es, "No module table - can't make "
					"module \"%s\" shared!",
					eel_v2s(&mn));
			break;
		  }
		}

	/* Record the initial refcount, for module garbage collection */
	m->refsum = eel_module_countref(mo);
}


void eel_compile(EEL_state *es, EEL_object *mo, EEL_sflags flags)
{
	EEL_xno x;
	EEL_value filename;
	EEL_module *m;
	int include_depth_save = es->include_depth;

	if((EEL_classes)mo->type != EEL_CMODULE)
		eel_cerror(es, "Object is not a module!");
	m = o2EEL_module(mo);

	/* Circular module import detection */
	x = eel_table_gets(m->exports, "__filename", &filename);
	if(!x)
	{
		EEL_value v;
		if(!eel_table_get(es->modnames, &filename, &v))
			eel_cerror(es, "Circular module import detected!\n"
					"Tried to compile module \"%s\", "
					"which is already being compiled.",
					eel_o2s(filename.objref.v));
		v.type = EEL_TNIL;
		eel_table_set(es->modnames, &filename, &v);
	}
	else
		filename.type = EEL_TNIL;

	es->include_depth = 0;
/* FIXME: 'x' might be clobbered by setjmp()/longjmp()... */
	x = 0;
	eel_try(es)
		compile2(es, mo, flags);
	eel_except
		x = 1;

	es->include_depth = include_depth_save;
	if(filename.type != EEL_TNIL)
		eel_table_delete(es->modnames, &filename);

	if(x)
	{
		eel_perror(es->vm, 1);
		eel_cthrow(es);
	}
}
