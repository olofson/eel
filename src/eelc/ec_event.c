/*
---------------------------------------------------------------------------
	ec_event.c - EEL Compiler Events
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009 David Olofson
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

#include <limits.h>
#include "ec_event.h"
#include "ec_symtab.h"
#include "ec_context.h"
#include "ec_coder.h"
#include "e_string.h"
#include "e_state.h"

EEL_DLLIST(e_, EEL_context, firstel, lastel, EEL_cevlist, prev, next)

/*
 * API
 */
EEL_cevlist *eel_e_open(EEL_context *c)
{
	EEL_cevlist *e = (EEL_cevlist *)calloc(1, sizeof(EEL_cevlist));
	if(!e)
		eel_ierror(c->coder->state, "Could not create EEL_cevlist!");
	e->parent = c;
	e_link(c, e);
	DBGB(printf("CREATED EVENT LIST %p FOR CONTEXT %p.\n", e, c);)
	return e;
}

void eel_e_close(EEL_cevlist *e)
{
	DBGB(printf("DELETING EVENT LIST %p FOR CONTEXT %p.\n", e, e->parent);)
	e_unlink(e->parent, e);
	free(e);
}


/* Used when popping CONDITIONAL contexts */
void eel_e_move_up(EEL_context *c)
{
	int i;
	EEL_cevlist *e = c->firstel;
	DBGB(printf("MOVING EVENT LIST %p FROM CONTEXT %p TO CONTEXT %p.\n",
			e, c, c->previous);)
	/* Delete events that are not to leave their context */
	for(i = 0; i < EEL__EVENTS; ++i)
	{
		if(e->events[i] == EEL_ENO)
			continue;
		switch(i)
		{
		  case EEL_EEXIT:
		  case EEL_ETARGET:
			e->events[i] = EEL_ENO;
			break;
		  case EEL_ERETURN:	break;
		  case EEL_ERESULT:	break;
		  case EEL__EVENTS:
		  	break;
		}
		/* Drop any INIT events for body local variables! */
		if((i <= EEL_MAXREG) && (ECTX_BLOCK != c->type))
		{
			DBGB(printf("  Dropping INIT event for R[%d]\n", i);)
			e->events[i] = EEL_ENO;
		}
	}
	/* Peel one level off of any break events */
	if(e->breakto)
		--e->breakto;
	if(e->maybe_breakto)
		--e->maybe_breakto;
	/* Add the list as the last list of the previous context */
	e_unlink(e->parent, e);
	e->parent = c->previous;
	e_link(c->previous, e);
}


/* Used when popping XBLOCK function contexts */
void eel_e_move_up_xblock(EEL_context *c)
{
	int i;
	EEL_cevlist *e = c->firstel;
	DBGB(printf("MOVING EVENT LIST %p FROM CONTEXT %p TO CONTEXT %p.\n",
			e, c, c->previous);)
	/* Delete everything but RETURN events... */
	for(i = 0; i < EEL__EVENTS; ++i)
		switch(i)
		{
		  case EEL_ERETURN:
			DBGB(printf("  RETURN event escaped xblock.\n");)
			break;
		  case EEL_ERESULT:
			DBGB(printf("  RESULT event escaped xblock.\n");)
			break;
		  default:
			e->events[i] = EEL_ENO;
			break;
		}
	/* Add the list as the last list of the previous context */
	e_unlink(e->parent, e);
	e->parent = c->previous;
	e_link(c->previous, e);
}


/* Used when popping non-CONDITIONAL contexts */
void eel_e_merge_up(EEL_context *c)
{
	int i;
	EEL_cevlist *e = c->firstel;
	EEL_cevlist *pe = c->previous->firstel;
	DBGB(printf("MERGING UP EVENT LIST %p UP FROM CONTEXT %p TO CONTEXT %p.\n",
			e, c, c->previous);)
	for(i = 0; i < EEL__EVENTS; ++i)
	{
		if(e->events[i] == EEL_ENO)
			continue;
		if(i == EEL_ETARGET)
			continue;
		if((i <= EEL_MAXREG) && (c->type != ECTX_BLOCK))
		{
			DBGB(printf("  Dropped INIT event for R[%d].\n", i);)
			continue;
		}
		if(e->events[i] > pe->events[i])
			pe->events[i] = e->events[i];
	}
}


void eel_e_merge(EEL_context *c, EEL_cestate modulator)
{
	EEL_cevlist *list;
	int i, j;
	int branches;		/* # of branches to combine */
	int sum;		/* Sum of event states for current event */
	int shallowest;		/* Shallowest breakto in any branch */
	if(!c->firstel->next)
		return;		/* No extra lists! Nothing to do here. */

	DBGB(printf("MERGING MULTIPLE EVENT LISTS FOR CONTEXT %p.\n", c);)

	/* Count branches */
	list = c->firstel;
	branches = 0;
	while(list->next)
	{
		++branches;
		list = list->next;
	}
	DBGB(printf("###### modulator: %d\n", modulator);)
	DBGB(printf("###### branches: %d\n", branches);)

	/*
	 * Sum the states of the occurrences of each "unique" event.
	 *
	 * If a sum matches (branches * EEL_EYES), the result is an
	 * event with state "YES". If the sum is 0, the result is
	 * nothing. Any other sum results a "MAYBE" event.
	 *
	 * The resulting events are combined with the master (first)
	 * event list as they are generated.
	 */
	for(i = 0; i < EEL__EVENTS; ++i)
	{
		/* Drop events that should never escape a context */
		switch(i)
		{
		  case EEL_EEXIT:
		  case EEL_ETARGET:
			continue;
		  case EEL_ERETURN:
		  case EEL_ERESULT:
		  case EEL__EVENTS:
			break;
		}

		/* Sum */
		sum = 0;
		list = c->firstel->next;
		for(j = 0; j < branches; ++j)
		{
			sum += list->events[i];
			list = list->next;
		}

		/* Process the result */
		if(0 == sum)
			sum = EEL_ENO;
		else if(branches * EEL_EYES == sum)
			sum = EEL_EYES;
		else
			sum = EEL_EMAYBE;
		if(sum > modulator)
			sum = modulator;
		if(!sum)
			continue;

		if(c->firstel->events[i] < sum)
			c->firstel->events[i] = sum;
	}

	/*
	 * Sum break events that reach outside their contexts.
	 *
	 * Tricky? Nah! First, just have all (breakto - 1) and
	 * (maybe_breakto - 1) bump our maybe break. Then figure
	 * out the deepest breakto level that is reached by *all*
	 * branches - that is, find the shallowest breakto - and
	 * bump our breakto to that level - 1.
	 */
	shallowest = INT_MAX;
	list = c->firstel->next;
	for(j = 0; j < branches; ++j)
	{
		int breakto = list->breakto - 1;
		int maybe_breakto = list->maybe_breakto - 1;
		if(breakto > c->firstel->maybe_breakto)
			c->firstel->maybe_breakto = breakto;
		if(maybe_breakto > c->firstel->maybe_breakto)
			c->firstel->maybe_breakto = maybe_breakto;
		if(breakto < shallowest)
			shallowest = breakto;
		list = list->next;
	}
	if(modulator > EEL_EMAYBE)
		if(shallowest > c->firstel->breakto)
			c->firstel->breakto = shallowest;

	/* Remove the extra event lists */
	while(c->firstel->next)
		eel_e_close(c->firstel->next);

DBGB(printf("###### done!\n");)
DBGB(printf("###### result: "); eel_e_print(c->coder->state);)
DBGB(printf("\n####################################################\n");)
}


/* Check whether event 'type' has occurred in the current function. */
static EEL_cestate test_ev(EEL_context *c, EEL_cevents type)
{
	int result = EEL_ENO;
	DBGC(printf("---- test_ev(%p, %d):", c, type);)
	while(c)
	{
		EEL_cevlist *e = c->firstel;
		DBGC2(printf(" (%s)", c->name);)
		if(e->events[type] > result)
		{
			result = e->events[type];
			if(result == EEL_EYES)
				break;
		}
		if(c->type == ECTX_FUNCTION)
		{
			DBGC2(printf(" FUNTION BOUNDARY!");)
			break;	/* Don't leave the current function! */
		}
		c = c->previous;
	}
	DBGC(switch(result)
	{
	  case EEL_ENO:
		printf(" -- NO\n");
		break;
	  case EEL_EMAYBE:
		printf(" -- MAYBE\n");
		break;
	  case EEL_EYES:
		printf(" -- YES\n");
		break;
	})
	return result;
}


static EEL_cestate test_break(EEL_context *c)
{
	if(c->firstel->breakto)
		return EEL_EYES;
	if(c->firstel->maybe_breakto)
		return EEL_EMAYBE;
	return EEL_ENO;
}


EEL_cestate eel_test_exit(EEL_state *es)
{
	EEL_cestate ex, ret, brk, tgt;
	tgt = es->context->firstel->events[EEL_ETARGET];
	if(tgt == EEL_EYES)
		return EEL_ENO;
	ex = test_ev(es->context, EEL_EEXIT);
	if(ex == EEL_EYES)
		return EEL_EYES;
	ret = test_ev(es->context, EEL_ERETURN);
	if(ret == EEL_EYES)
		return EEL_EYES;
	brk = test_break(es->context);
	if(brk == EEL_EYES)
		return EEL_EYES;
	if((ex == EEL_ENO) && (ret == EEL_ENO) && (brk == EEL_ENO))
		return EEL_ENO;
	return EEL_EMAYBE;
}

EEL_cestate eel_test_init(EEL_state *es, EEL_symbol *s)
{
	return test_ev(es->context, s->v.var.location);
}

EEL_cestate eel_test_result(EEL_state *es)
{
	return test_ev(es->context, EEL_ERESULT);
}


int eel_initializations(EEL_context *c)
{
	int count = 0;
	while(c)
	{
		EEL_cevlist *e = c->firstel;
		int i;
		for(i = 0; i <= EEL_MAXREG; ++i)
		{
			if(e->events[i] == EEL_EYES)
				++count;
			else if(e->events[i] == EEL_EMAYBE)
				eel_cerror(c->coder->state, "Some variables "
						"may or may not be initialized"
						" at this point!");
		}
		if(c->type == ECTX_FUNCTION)
			break;	/* Don't leave the current function! */
		c = c->previous;
	}
	return count;
}


int eel_keep_variables(EEL_context *c)
{
	return (c->type == ECTX_BLOCK)/* && (c->flags & ECTX_CONDITIONAL)*/;
}


void eel_e_exit(EEL_state *es)
{
	EEL_cevlist *e = es->context->firstel;
	DBGB(printf("EVENT: EXIT\n");)
	e->events[EEL_EEXIT] = EEL_EYES;
}

void eel_e_return(EEL_state *es)
{
	EEL_cevlist *e = es->context->firstel;
	DBGB(printf("EVENT: RETURN\n");)
	e->events[EEL_ERETURN] = EEL_EYES;
}

void eel_e_break(EEL_state *es, EEL_context *ctx)
{
	EEL_cevlist *e = es->context->firstel;
	int depth = es->context->level - ctx->level + 1;
	DBGB(printf("EVENT: BREAK\n");)
	if(depth > e->breakto)
		e->breakto = depth;
}

void eel_e_target(EEL_state *es, EEL_cestate st)
{
	EEL_cevlist *e = es->context->firstel;
	DBGB(printf("EVENT: TARGET\n");)
	if(st > e->events[EEL_ETARGET])
		e->events[EEL_ETARGET] = st;
}

void eel_e_init(EEL_state *es, EEL_symbol *s)
{
	EEL_cevlist *e = es->context->firstel;
	DBGB(printf("EVENT: INIT R[%d] ('%s')\n",
			s->v.var.location, s->name);)
	if(eel_test_init(es, s))
		eel_ierror(es, "Variable '%s' has already been initialized!",
				eel_o2s(s->name));
	if((s->v.var.location < 0) || (s->v.var.location > EEL_MAXREG))
		eel_ierror(es, "Variable '%s' is located in an out of range "
				"register!", eel_o2s(s->name));
	e->events[s->v.var.location] = EEL_EYES;
}

void eel_e_result(EEL_state *es)
{
	EEL_cevlist *e = es->context->firstel;
	DBGB(printf("EVENT: RESULT\n");)
	e->events[EEL_ERESULT] = EEL_EYES;
}

#if DBGB(1) + 0 == 1
void eel_e_print(EEL_state *es)
{
	int r;
	printf("{ ");
	switch(test_ev(es->context, EEL_EEXIT))
	{
	  case EEL_ENO:		break;
	  case EEL_EMAYBE:	printf("EXIT? "); break;
	  case EEL_EYES:	printf("EXIT! "); break;
	}
	switch(test_ev(es->context, EEL_ERETURN))
	{
	  case EEL_ENO:		break;
	  case EEL_EMAYBE:	printf("RETURN? "); break;
	  case EEL_EYES:	printf("RETURN! "); break;
	}
	switch(test_break(es->context))
	{
	  case EEL_ENO:
		break;
	  case EEL_EMAYBE:
		printf("BREAK?(%d) ", es->context->firstel->maybe_breakto);
		break;
	  case EEL_EYES:
		printf("BREAK!(%d) ", es->context->firstel->breakto);
		break;
	}

	switch(eel_test_exit(es))
	{
	  case EEL_ENO:		break;
	  case EEL_EMAYBE:	printf("(EXIT?) "); break;
	  case EEL_EYES:	printf("(EXIT!) "); break;
	}
	switch(eel_test_result(es))
	{
	  case EEL_ENO:		break;
	  case EEL_EMAYBE:	printf("(RESULT?) "); break;
	  case EEL_EYES:	printf("(RESULT!) "); break;
	}

	for(r = 0; r < 20; ++r)
		switch(test_ev(es->context, r))
		{
		  case EEL_ENO:		break;
		  case EEL_EMAYBE:	printf("R[%d]? ", r); break;
		  case EEL_EYES:	printf("R[%d]! ", r); break;
		}
	printf("}");
}
#endif
