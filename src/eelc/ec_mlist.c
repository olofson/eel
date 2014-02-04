/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_mlist.c - Argument Manipulator List
---------------------------------------------------------------------------
 * Copyright (C) 2004, 2006, 2009, 2011 David Olofson
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
