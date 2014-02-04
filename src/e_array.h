/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_array.h - EEL Array Class
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009-2011 David Olofson
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

#ifndef	EEL_E_ARRAY_H
#define	EEL_E_ARRAY_H

#include "EEL.h"
#include "EEL_types.h"

typedef struct
{
	int		length;		/* # of items */
	int		maxlength;	/* Buffer size */
	EEL_value	*values;
} EEL_array;
EEL_MAKE_CAST(EEL_array)
void eel_carray_register(EEL_vm *vm);
void eel_carray_unregister(EEL_vm *vm);

#endif	/* EEL_E_ARRAY_H */
