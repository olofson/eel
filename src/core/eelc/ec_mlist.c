/*
---------------------------------------------------------------------------
	ec_mlist.c - Argument Manipulator List
---------------------------------------------------------------------------
 * Copyright 2004, 2006, 2009, 2011 David Olofson
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
#include "ec_mlist.h"
#include "e_util.h"

/*----------------------------------------------------------
	Linked list operators
----------------------------------------------------------*/
EEL_DLLIST(ml_, EEL_coder, firstml, lastml, EEL_mlist, prev, next)


EEL_mlist *eel_ml_open(EEL_coder *cdr)
{
	EEL_mlist *ml = (EEL_mlist *)calloc(1, sizeof(EEL_mlist));
	if(!ml)
		eel_ierror(cdr->state, "Could not allocate arglist!");
	ml->coder = cdr;
	ml_link(cdr, ml);
	return ml;
}


void eel_ml_close(EEL_mlist *ml)
{
	ml_unlink(ml->coder, ml);
	while(ml->args)
		eel_m_delete(ml->args);
	free(ml);
}


EEL_manipulator *eel_ml_get(EEL_mlist *ml, int a)
{
	EEL_manipulator *m;
	m = ml->args;
	if(a < 0)
		a += ml->length;
	if((a < 0) || (a >= ml->length))
		eel_ierror(ml->coder->state, "Argument list index out of range!");
	while(a--)
		m = m->next;
	return m;
}

int eel_ml_push(EEL_mlist *ml)
{
	int i;
	EEL_manipulator *m = ml->args;
	for(i = 0; i < ml->length; ++i)
	{
		DBG9(printf("; PUSH element[%d]:\n", i);)
		eel_m_push(m);
		m = m->next;
	}
	return ml->length;
}


void eel_ml_transfer(EEL_mlist *from, EEL_mlist *to)
{
	while(from->length)
		eel_m_transfer(eel_ml_get(from, 0), to);
}


void eel_ml_delete(EEL_mlist *ml, int first, int count)
{
	while(count--)
	{
		EEL_manipulator *m = eel_ml_get(ml, first);
		if(!m)
			break;
		eel_m_delete(m);
	}
}
