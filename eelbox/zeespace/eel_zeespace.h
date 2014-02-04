/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_zeespace.h - EEL ZeeSpace binding
---------------------------------------------------------------------------
 * Copyright (C) 2010-2011 David Olofson
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
