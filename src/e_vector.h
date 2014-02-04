/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_vector.h - EEL Vector Class implementation
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009-2010 David Olofson
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

#ifndef	EEL_E_VECTOR_H
#define	EEL_E_VECTOR_H

#include "EEL.h"
#include "EEL_types.h"

/* What you get if you try to instantiate the base class */
#define	EEL_DEFAULT_VECTOR_SUBCLASS	EEL_CVECTOR_D

/*
 * The actual vector class
 */
typedef struct
{
	int		length;		/* # of items */
	int		maxlength;	/* Buffer size (items) */
	int		isize;		/* Size of one item (bytes) */
	union
	{
		EEL_uint8	*u8;
		EEL_int8	*s8;
		EEL_uint16	*u16;
		EEL_int16	*s16;
		EEL_uint32	*u32;
		EEL_int32	*s32;
		float		*f;
		double		*d;
	} buffer;
} EEL_vector;
EEL_MAKE_CAST(EEL_vector)
void eel_cvector_register(EEL_vm *vm);

/*
 * Extended API
 */

/*
 * Create an uninitialized vector of 'type' (must be one of the vector types),
 * of 'size' items. The buffer will be allocated and internal fields set
 * accordingly, but the buffer contents is undefined!
 */
EEL_object *eel_cv_new_noinit(EEL_vm *vm, EEL_types type, unsigned size);

#endif	/* EEL_E_VECTOR_H */
