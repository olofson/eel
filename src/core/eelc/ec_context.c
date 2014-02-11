/*
---------------------------------------------------------------------------
	ec_context.c - Compiler Context
---------------------------------------------------------------------------
 * Copyright 2002-2006 David Olofson
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

#include "ec_context.h"
#include "ec_coder.h"
#include "ec_symtab.h"
#include "e_state.h"
#include "e_string.h"
#include "e_function.h"

/*----------------------------------------------------------
	Compiler Context
----------------------------------------------------------*/

static void context_init(EEL_context *c)
{
	c->firstel = c->lastel = NULL;
	c->endjumps = c->lastej = c->contjumps = c->lastcj = NULL;
	eel_e_open(c);
}


static void context_cleanup(EEL_state *es, EEL_context *c)
{
	switch((EEL_ctxtypes)c->type)
	{
	  case ECTX_MODULE:
		eel_unlock_modules(es);
		break;
	  case ECTX_BLOCK:
	  case ECTX_BODY:
	  case ECTX_FUNCTION:
		break;
	}
	while(c->endjumps)
	{
		EEL_codemark *cm = c->endjumps;
		ej_unlink(c, cm);
		eel_free(es->vm, cm);
	}
	while(c->contjumps)
	{
		EEL_codemark *cm = c->contjumps;
		cj_unlink(c, cm);
		eel_free(es->vm, cm);
	}
	while(c->firstel)
		eel_e_close(c->firstel);
	if((c->flags & ECTX_OWNS_BIO) && c->bio)
		eel_bio_close(c->bio);
	if((c->flags & ECTX_OWNS_CODER) && c->coder)
		eel_coder_close(c->coder);
	DBGD(printf("Killed context '%s'\n", c->name);)
	DBGD(free((char *)c->name);)
}


void context_open(EEL_state *es)
{
	EEL_context *c = (EEL_context *)eel_malloc(es->vm, sizeof(EEL_context));
	if(!c)
		eel_serror(es, "Failed to create context!");
	memset(c, 0, sizeof(EEL_context));
	context_init(c);
	c->symtab = es->root_symtab;
	es->context = c;
}


void context_close(EEL_state *es)
{
	while(es->context)
	{
		EEL_context *c = es->context;
		context_cleanup(es, c);
		es->context = c->previous;
		eel_free(es->vm, c);
	}
}


void eel_context_push(EEL_state *es, int flags, const char *name)
{
	EEL_symbol *s;
	EEL_context *c = (EEL_context *)eel_malloc(es->vm, sizeof(EEL_context));
	if(!c)
		eel_ierror(es, "Failed to push compiler context!");
	memcpy(c, es->context, sizeof(EEL_context));

	DBGD(c->name = name ? strdup(name) : strdup("<unnamed>");)
	DBGD(printf("Pushing context '%s'...\n", c->name);)

	context_init(c);

	/* Push */
	c->previous = es->context;
	es->context = c;
	if(es->jumpbufs)
		++es->jumpbufs->contexts;
	c->level = c->previous->level + 1;

	/* Extract and remove context type */
	c->type = flags & ECTX_TYPE;
	flags &= ~ECTX_TYPE;

	/* Breakable, conditional etc are not inherited! */
	c->flags &= ~(ECTX_CONDITIONAL | ECTX_BREAKABLE |
			ECTX_CONTINUABLE | ECTX_REPEATABLE);

	/* Drop ownership by default */
	c->flags &= ~(ECTX_OWNS_BIO | ECTX_OWNS_CODER);

	/* Replace the creation flags with those for the new context */
	c->flags &= 0xffff0000;
	c->flags |= flags & 0x0000ffff;

	switch((EEL_ctxtypes)c->type)
	{
	  case ECTX_BLOCK:
		break;
	  case ECTX_BODY:
		if(name)
			s = eel_s_add(es, c->symtab, name, EEL_SBODY);
		else
		{
			const char *nn = eel_unique(es->vm, "__body");
			s = eel_s_add(es, c->symtab, nn, EEL_SBODY);
			eel_sfree(es, nn);
			DBGD(free(c->name); c->name = strdup(eel_o2s(s->name));)
		}
		if(!s)
			eel_ierror(es, "Failed to create block symtab!");
		c->symtab = s;
		s->v.context = c;
		break;
	  case ECTX_FUNCTION:
		if(name)
			s = eel_s_add(es, c->symtab, name, EEL_SFUNCTION);
		else
		{
			const char *nn = eel_unique(es->vm, "__function");
			s = eel_s_add(es, c->symtab, nn, EEL_SFUNCTION);
			eel_sfree(es, nn);
			DBGD(free(c->name); c->name = strdup(eel_o2s(s->name));)
		}
		if(!s)
			eel_ierror(es, "Failed to create function symtab!");
		c->symtab = s;
		break;
	  case ECTX_MODULE:
		/* Unnamed modules should *really* be unnamed! */
		s = eel_s_add(es, es->root_symtab, name, EEL_SMODULE);
		if(!s)
			eel_ierror(es, "Failed to create module symtab!");
		c->symtab = s;
		eel_lock_modules(es);	/* Lock imports in place! */
		break;
	  default:
		eel_ierror(es, "Tried to push context of undefined type %d!",
				c->type);
		break;
	}

	if((flags & ECTX_CLONEBIO) && c->bio)
	{
		c->bio = eel_bio_clone(c->previous->bio);
		if(!c->bio)
			eel_ierror(es, "Couldn't create local bio!");
		c->flags |= ECTX_OWNS_BIO;
	}

	/*
	 * Start new code optimization fragment if this is a
	 * context where the start serves as a jump target.
	 */
	if(flags & ECTX_CONTINUABLE)
	{
		if(c->coder)
			c->startpos = eel_code_target(c->coder);
		DBGD(printf("  startpos = %d\n", c->startpos);)
	}

#if DBGD(1) + 0 == 1
	printf("\033[31mPushed context '%s' (%p);\033[0m type: ", c->name, c);
	switch(c->type)
	{
	  case ECTX_BLOCK:	printf("BLOCK"); break;
	  case ECTX_BODY:	printf("BODY"); break;
	  case ECTX_MODULE:	printf("MODULE"); break;
	  case ECTX_FUNCTION:	printf("FUNCTION"); break;
	}
	printf("; flags: ");
	if(c->flags & ECTX_CLONEBIO)	printf("CLONEBIO ");
	if(c->flags & ECTX_BREAKABLE)	printf("BREAKABLE ");
	if(c->flags & ECTX_CONTINUABLE)	printf("CONTINUABLE ");
	if(c->flags & ECTX_CONDITIONAL)	printf("CONDITIONAL ");
	if(c->flags & ECTX_REPEATABLE)	printf("REPEATABLE ");
	if(c->flags & ECTX_ROOT)	printf("ROOT ");
	if(c->flags & ECTX_CATCHER)	printf("CATCHER ");
	if(c->flags & ECTX_WRAPPED)	printf("WRAPPED ");
	if(c->flags & ECTX_DUMMY)	printf("DUMMY ");
	if(c->flags & ECTX_KEEP)	printf("KEEP ");
	if(c->flags & ECTX_OWNS_BIO)	printf("OWNS_BIO ");
	if(c->flags & ECTX_OWNS_CODER)	printf("OWNS_CODER ");
	printf("\n", c->name);
#endif
}


void eel_context_pop(EEL_state *es)
{
	EEL_context *c = es->context;
	if(!c->previous)
		eel_ierror(es, "Tried to pop top level context!");
	DBGD(printf("\033[32mPopping context '%s' (%p)\033[0m\n", c->name, c);)

	/*
	 * Propagate events to the parent context
	 */
	switch((EEL_ctxtypes)c->type)
	{
	  case ECTX_MODULE:
	  case ECTX_BLOCK:
	  case ECTX_BODY:
		if(c->flags & ECTX_CONDITIONAL)
			eel_e_move_up(c);
		else
			eel_e_merge_up(c);
		break;
	  case ECTX_FUNCTION:
		if(c->coder && c->coder->f)
		{
			EEL_function *f = o2EEL_function(c->coder->f);
			if(f->common.flags & EEL_FF_XBLOCK)
				eel_e_move_up_xblock(c);
		}
		break;
	}

	/*
	 * Kill this context
	 */
	context_cleanup(es, c);

	/*
	 * Restore the previous context
	 */
	es->context = c->previous;
	free(c);
	if(es->jumpbufs)
		--es->jumpbufs->contexts;
}


EEL_context *eel_find_function_context(EEL_context *ctx)
{
	while(ctx)
	{
		if(ctx->type == ECTX_FUNCTION)
			return ctx;
		ctx = ctx->previous;
	}
	return NULL;
}
