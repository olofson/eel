/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_mlist.h - Argument Manipulator List
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

#ifndef	EEL_EC_MLIST_H
#define	EEL_EC_MLIST_H

#include "ec_manip.h"

/*----------------------------------------------------------
	Argument Manipulator List
----------------------------------------------------------*/
struct EEL_mlist
{
	EEL_coder	*coder;		/* Parent/owner */
	EEL_mlist	*next, *prev;
	EEL_manipulator	*args, *lastarg;
	int		length;		/* # of arguments */
};

/* Create an argument list. */
EEL_mlist *eel_ml_open(EEL_coder *cdr);

/* Close argument manipulator list 'ml'. */
void eel_ml_close(EEL_mlist *ml);

/*
 * Get the manipulator for argument 'a' in 'ml'.
 * If a < 0, abs(a) is the 1 based index from the end of
 * the list. (-1 ==> last arg, -2 ==> second last arg etc.)
 */
EEL_manipulator *eel_ml_get(EEL_mlist *ml, int a);

/*
 * Push all arguments in 'ml' onto the argument stack.
 * Returns the number of elements pushed.
 */
int eel_ml_push(EEL_mlist *ml);

/* Transfer all manipulators from 'from' to 'to'. */
void eel_ml_transfer(EEL_mlist *from, EEL_mlist *to);

/* Delete manipulators [first, first + count - 1] from 'ml'. */
void eel_ml_delete(EEL_mlist *ml, int first, int count);

#endif /* EEL_EC_MLIST_H */
