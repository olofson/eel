/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_dstring.h - EEL Dynamic String Class
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009, 2011 David Olofson
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

#ifndef	EEL_E_DSTRING_H
#define	EEL_E_DSTRING_H

#include "EEL.h"
#include "EEL_types.h"

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
