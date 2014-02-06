/*
---------------------------------------------------------------------------
	eel_zeespace.h - EEL ZeeSpace binding
---------------------------------------------------------------------------
 * Copyright 2010-2011 David Olofson
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

#ifndef EEL_ZEESPACE_H
#define EEL_ZEESPACE_H

#include "EEL.h"
#include "zeespace.h"

EEL_MAKE_CAST(ZS_Pixel)
EEL_MAKE_CAST(ZS_Rect)

/* Pipe */
typedef struct
{
	ZS_Pipe		*pipe;
} EZS_pipe;
EEL_MAKE_CAST(EZS_pipe)

/* Region */
typedef struct
{
	ZS_Region	*region;
} EZS_region;
EEL_MAKE_CAST(EZS_region)

/* Surface */
typedef struct
{
	ZS_Surface	*surface;
} EZS_surface;
EEL_MAKE_CAST(EZS_surface)

EEL_xno eel_zeespace_init(EEL_vm *vm);

#endif /* EEL_ZEESPACE_H */
