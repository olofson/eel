/*
---------------------------------------------------------------------------
	ec_mlist.h - Argument Manipulator List
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
