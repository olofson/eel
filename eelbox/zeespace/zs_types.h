/*
-----------------------------------------------------------------
	zs_types.h - ZeeSpace Types
-----------------------------------------------------------------
 * Copyright 2005, 2011 David Olofson
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
