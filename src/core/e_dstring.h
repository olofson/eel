/*
---------------------------------------------------------------------------
	e_dstring.h - EEL Dynamic String Class
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009, 2011 David Olofson
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

#ifndef	EEL_E_DSTRING_H
#define	EEL_E_DSTRING_H

#include "EEL.h"
#include "EEL_types.h"
#include "e_config.h"

typedef struct
{
	char	*buffer;	/* The string buffer */
	int	length;		/* # of characters */
	int	maxlength;	/* Buffer size */
} EEL_dstring;
EEL_MAKE_CAST(EEL_dstring)

void eel_cdstring_register(EEL_vm *vm);

EEL_object *eel_ds_new(EEL_vm *vm, const char *s);
EEL_object *eel_ds_nnew(EEL_vm *vm, const char *s, int len);

/*
 * Like eel_ds_new(), but takes over 's'. (It's thrown
 * away if it's not needed, and in case of failure.)
 */
EEL_object *eel_ds_new_grab(EEL_vm *vm, char *s);
EEL_object *eel_ds_nnew_grab(EEL_vm *vm, char *s, int len);

/* Shortcut API for using dstrings as memfile buffers */
EEL_xno eel_ds_write(EEL_object *eo, int pos, const char *s, int len);

#endif	/* EEL_E_DSTRING_H */
