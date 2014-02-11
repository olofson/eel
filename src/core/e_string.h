/*
---------------------------------------------------------------------------
	e_string.h - EEL String Class + string pool
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2008, 2009, 2011-2012 David Olofson
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
