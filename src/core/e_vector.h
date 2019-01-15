/*
---------------------------------------------------------------------------
	e_vector.h - EEL Vector Class implementation
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009-2010, 2019 David Olofson
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

#ifndef	EEL_E_VECTOR_H
#define	EEL_E_VECTOR_H

#include "EEL.h"
#include "EEL_types.h"
#include "e_config.h"

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
 * Create an uninitialized vector of class 'cid' (must be one of the vector
 * types), of 'size' items. The buffer will be allocated and internal fields
 * set accordingly, but the buffer contents is undefined!
 */
EEL_object *eel_cv_new_noinit(EEL_vm *vm, EEL_classes cid, unsigned size);

#endif	/* EEL_E_VECTOR_H */
