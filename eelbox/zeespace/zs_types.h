/*(LGPL)
-----------------------------------------------------------------
	zs_types.h - ZeeSpace Types
-----------------------------------------------------------------
 * Copyright (C) 2005, 2011 David Olofson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	ZS_TYPES_H
#define	ZS_TYPES_H

#include <stdint.h>

/* Error codes */
typedef enum ZS_Errors
{
	ZS_EOK = 0,	/* No error! */
	ZS_EINTERNAL,	/* Internal error - probably a ZeeSpace bug! */
	ZS_EMEMORY,	/* Out of memory! */
	ZS_EILLEGAL,	/* Illegal operation */
	ZS_ENOTIMPL	/* Operation not implemented */
} ZS_Errors;


typedef struct ZS_Surface ZS_Surface;
typedef struct ZS_Pipe ZS_Pipe;
typedef struct ZS_Region ZS_Region;
typedef struct ZS_Span ZS_Span;


/* Single RGBAIZ pixel */
typedef struct
{
	uint8_t		r, g, b, a;
	uint16_t	i;		/* fixp 8:8 */
	uint16_t	z;		/* fixp 8:8 */
} ZS_Pixel;


/* Rectangle */
typedef struct
{
	int		x, y;
	unsigned	w, h;
} ZS_Rect;


/* ZeeSpace state */
typedef struct ZS_State
{
	int		bufw;		/* # of pixels in scratch and pipebuf */
	ZS_Pixel	*scratch;	/* Rendering scratch pad (2 pixel rows) */
	ZS_Pixel	*pipebuf;	/* Intermediate pipeline buffer (one pixel row) */
	int		refcount;
	unsigned	rnd1, rnd2, rbal;
} ZS_State;

#endif /* ZS_TYPES_H */
