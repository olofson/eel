/*
---------------------------------------------------------------------------
	ec_event.h - EEL Compiler Events
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009 David Olofson
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

#ifndef	EEL_EC_EVENT_H
#define	EEL_EC_EVENT_H

#include "e_vm.h"

/*
 * Compiler Events are used for keeping track of things like
 * variable initializations and leaving code blocks.
 *    Event lists deal with conditional code by combining
 * the events from branches after they have been compiled.
 * As a result, one does not have to worry about code being
 * conditional while actually compiling it, since the
 * combined set of events of the current context and all
 * parent contexts (which is what the ce_test_*() calls
 * analyze) always reflect the current state of things.
 *
 * Event lists and the context stack
 * ---------------------------------
 * The context struct has a list of event lists. Normally,
 * this will only contain one event list.
 *    The eel_e_(exit|return|init|result|target)() calls
 * add events to the current context.
 * The eel_test_*() calls check for events in the current
 * context, and when appropriate, in parent contexts.
 *    When a normal context is popped, it's events are
 * analyzed, and the appropriate ones are added to the
 * parent context's event list;
 *	* EXIT and TARGET events are dropped.
 *	* INIT events never escape the context their
 *	  respective variables were declared in.
 *	* No events escape FUNCTION contexts, except
 *	  RESULT and RETURN events that can escape
 *	  XBLOCK functions.
 *
 *    When a CONDITIONAL context is popped, it's event list
 * is moved to the parent context's list of event lists. The
 * compiler can then use eel_e_merge() to combine the extra
 * event lists and adjust the current context's event list
 * as required. This is used to deal with multiple paths that
 * combined guarantee initializations, to detect constructs
 * with multiple function exits leaving following code dead,
 * and similar situations.
 */
typedef enum
{
	EEL_EEXIT = EEL_MAXREG + 1,	/* Control has left current block */
	EEL_ERETURN,			/* Control has left function */
	EEL_ERESULT,			/* Result was initialized */
	EEL_ETARGET,			/* Someone jumped back in! */
	EEL__EVENTS			/* Count */
} EEL_cevents;

typedef enum
{
	EEL_ENO = 0,	/* Event did not occur */
	EEL_EMAYBE = 1,	/* Event may have occurred */
	EEL_EYES = 2	/* Event did occur */
} EEL_cestate;

/*
 * This means "once or maybe more", and is meant for do..* loops.
 * Might make this a real event state some time, but for now, this
 * hack only results in slightly confusing error messages in some
 * unusual cases. I think... :-)
 */
#define	EEL_EMULTIPLE	EEL_EMAYBE

#define	EEL_ENOT(x)	(EEL_EYES - (x))

typedef struct EEL_cevlist EEL_cevlist;
struct EEL_cevlist
{
	EEL_context	*parent;
	EEL_cevlist	*prev, *next;
	EEL_cestate	events[EEL__EVENTS];
	int		breakto;
	int		maybe_breakto;
};

/* Event list management */
EEL_cevlist *eel_e_open(EEL_context *c);
void eel_e_close(EEL_cevlist *el);

/*
 * Grab relevant events from context 'c' and put them in a
 * new event list in the context previous to 'c'.
 */
void eel_e_move_up(EEL_context *c);
void eel_e_move_up_xblock(EEL_context *c);

/* Add event to the current context */
void eel_e_exit(EEL_state *es);
void eel_e_return(EEL_state *es);
void eel_e_init(EEL_state *es, EEL_symbol *s);
void eel_e_result(EEL_state *es);
void eel_e_target(EEL_state *es, EEL_cestate st);

/* Add BREAK event to the specified context */
void eel_e_break(EEL_state *es, EEL_context *ctx);

/* Check if event has occurred in the relevant context */
EEL_cestate eel_test_exit(EEL_state *es);
EEL_cestate eel_test_init(EEL_state *es, EEL_symbol *s);
EEL_cestate eel_test_result(EEL_state *es);

/*
 * Process event lists propagated from conditional sub-contexts.
 *
 * This yells about variables that are initialized in some, but
 * not all sub-contexts.
 *
 * 'modulator' specifies the state of all sub-contexts as a whole.
 * (It essentially modulates the state of the sub-contexts.) This
 * is used for 'if' without 'else', loops and other constructs
 * where there is not one event list per possible execution path.
 */
void eel_e_merge(EEL_context *c, EEL_cestate modulator);

/*
 * Propagate events from context 'c' to the previous context.
 */
void eel_e_merge_up(EEL_context *c);

/*
 * Count initializations done in the function that context 'c'
 * belongs to.
 */
int eel_initializations(EEL_context *c);

/*
 * Returns 1 if INIT events escape context 'c', that is, when
 * you should NOT clean variables upon leaving the context.
 */
int eel_keep_variables(EEL_context *c);

/* Print event state of EXIT, RESULT and the first 20 registers. */
DBGB2(void eel_e_print(EEL_state *es);)

#endif	/* EEL_EC_EVENT_H */
