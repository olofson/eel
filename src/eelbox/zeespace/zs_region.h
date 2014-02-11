/*
-----------------------------------------------------------------
	zs_region.h - ZeeSpace Regions
-----------------------------------------------------------------
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

#ifndef	ZS_REGION_H
#define	ZS_REGION_H

#include "zs_types.h"

/* Boolean operations */
typedef enum ZS_Operations
{
	ZS_NOP = 0,
	ZS_UNION,
	ZS_DIFFERENCE,
	ZS_INTERSECTION
} ZS_Operations;

struct ZS_Span
{
/*
TODO: Potential performance hack: Have an extra pair of coverage values for
TODO: single pixel edge antialiasing for mostly vertical contours. That could
TODO: easily cut span count and pipeline call overhead in half in many cases!
*/
	uint16_t	x, y, w;	/* Start position and length */
	uint16_t	c0, c1;		/* Start/end coverage */
};

struct ZS_Region
{
	unsigned	maxspans;	/* Buffer size */
	unsigned	nspans;		/* Spans used */
	ZS_Span		*spans;		/* Array of span descriptors */
};


/*---------------------------------------------------------------
	Region management and operations
---------------------------------------------------------------*/

ZS_Region *zs_NewRegion(unsigned initspans);
void zs_FreeRegion(ZS_Region *region);

/* Create a clone of region 'r' */
ZS_Region *zs_RegionClone(ZS_Region *r);

/* Apply region 'b' to region 'a' using operation 'op'. */
ZS_Errors zs_RegionApply(ZS_Region *a, ZS_Region *b, ZS_Operations op);

/* Create a new region containing the result of ('a' 'op' 'b') */
ZS_Region *zs_RegionCombine(ZS_Region *a, ZS_Region *b, ZS_Operations op);

/* Apply the shape described by 'path' to 'region' using operation 'op' */
//ZS_Errors zs_RegionPath(ZS_Region *region, ZS_Path *path, ZS_Operations op);

/* Apply a rectangular shape to 'region' using operation 'op' */
ZS_Errors zs_RegionRect(ZS_Region *region, float x, float y, float w, float h,
		ZS_Operations op);

/* Apply a circular shape to 'region' using operation 'op' */
ZS_Errors zs_RegionCircle(ZS_Region *region, float x, float y, float r,
		ZS_Operations op);

#endif	/* ZS_REGION_H */
