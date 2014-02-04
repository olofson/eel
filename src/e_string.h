/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_string.h - EEL String Class + string pool
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2008, 2009, 2011-2012 David Olofson
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

#ifndef	EEL_E_STRING_H
#define	EEL_E_STRING_H

#include "EEL.h"
#include "EEL_types.h"

typedef struct
{
	EEL_object	*snext, *sprev;	/* Bucket list */
	char		*buffer;	/* The string */
	int		length;		/* # of characters */
	EEL_hash	hash;		/* Full hash code */
} EEL_string;
EEL_MAKE_CAST(EEL_string)
void eel_cstring_register(EEL_vm *vm);

/*
 * Like eel_ps_new(), but takes over 's'. (It's thrown
 * away if it's not needed, and in case of failure.)
 */
EEL_object *eel_ps_new_grab(EEL_vm *vm, char *s);
EEL_object *eel_ps_nnew_grab(EEL_vm *vm, char *s, int len);

int eel_ps_open(EEL_vm *vm);
void eel_ps_close(EEL_vm *vm);


static inline const char *eel_o2s(EEL_object *o)
{
#ifdef EEL_VM_CHECKING
	if((EEL_classes)o->type != EEL_CSTRING)
		o = NULL;
#endif
	return o2EEL_string(o)->buffer;
}


/* Compare strings/memory blocks of the same length */
static inline int eel_s_cmp(unsigned const char *a,
		unsigned const char *b, int length)
{
	int i;
	for(i = 0; i < length; ++i)
		if(a[i] != b[i])
		{
			if(a[i] > b[i])
				return 1;
			else/* if(a[i] < b[i])*/
				return -1;
		}
	return 0;
}

#endif	/* EEL_E_STRING_H */
