/*(LGPL)
-----------------------------------------------------------------
	zs_region.h - ZeeSpace Regions
-----------------------------------------------------------------
 * Copyright (C) 2010-2011 David Olofson
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
