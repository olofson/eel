/*(LGPL)
-----------------------------------------------------------------
	zs_region.c - ZeeSpace Regions
-----------------------------------------------------------------
 * Copyright (C) 2011 David Olofson
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "zs_region.h"

#define	ZS_INITREGIONSIZE	512
#define	ZS_REGIONBUMPSIZE	128


static inline unsigned calcresize(unsigned base, unsigned current, unsigned requested)
{
	if(requested > current)
	{
		/* Grow */
		unsigned n;
		if(current)
			n = current;
		else
			n = base;
		while(n < requested)
			n = (n * 3 >> 1) + base;
		return n;
	}
	else
	{
		/* Shrink */
		unsigned n = current / 2;
		if(requested > n)
			return requested;
		else if(n < base)
			return base;
		else
			return n;
	}
}


/*---------------------------------------------------------------
	Low level operations
---------------------------------------------------------------*/

/* Ensure that region 'r' has room for at least 'n' additional spans. */
static inline ZS_Errors zs_RegionRealloc(ZS_Region *r, unsigned n)
{
	ZS_Span *newbuf;
	unsigned newsize = calcresize(ZS_INITREGIONSIZE, r->maxspans,
			r->nspans + n);
	if(newsize == r->maxspans)
		return ZS_EOK;
	newbuf = (ZS_Span *)realloc(r->spans, newsize * sizeof(ZS_Span));
	if(!newbuf)
		return ZS_EMEMORY;
	r->spans = newbuf;
	r->maxspans = newsize;
	return ZS_EOK;
}

static inline ZS_Errors zs_RegionAddSpanC(ZS_Region *r, int x, int y, int w,
		uint16_t c0, uint16_t c1)
{
	ZS_Span *s;
	if(w <= 0)
		return ZS_EOK;
	if(!c0 || !c1)
		return ZS_EOK;
	if(r->nspans + 1 > r->maxspans)
		if(zs_RegionRealloc(r, ZS_REGIONBUMPSIZE))
			return ZS_EMEMORY;
	s = r->spans + r->nspans;
/*TODO: Clipping here...? */
	s->x = x;
	s->y = y;
	s->w = w;
	s->c0 = c0;
	s->c1 = c1;
	r->nspans++;
	return ZS_EOK;
}

static inline int zs_RegionAddSpan(ZS_Region *r, int x, int y, int w)
{
	return zs_RegionAddSpanC(r, x, y, w, 256, 256);
}


/*---------------------------------------------------------------
	Region management and operations
---------------------------------------------------------------*/

ZS_Region *zs_NewRegion(unsigned initspans)
{
	ZS_Region *r = (ZS_Region *)calloc(1, sizeof(ZS_Region));
	if(!r)
		return NULL;
	if(!initspans)
		initspans = ZS_INITREGIONSIZE;
	if(zs_RegionRealloc(r, initspans))
	{
		free(r);
		return NULL;
	}
	return r;
}

void zs_FreeRegion(ZS_Region *region)
{
	free(region->spans);
	free(region);
}

ZS_Region *zs_RegionClone(ZS_Region *r)
{
	ZS_Region *cr = zs_NewRegion(r->nspans);
	if(!cr)
		return NULL;
	memcpy(cr->spans, r->spans, r->nspans * sizeof(ZS_Span));
	cr->nspans = r->nspans;
	return cr;
}

ZS_Errors zs_RegionApply(ZS_Region *a, ZS_Region *b, ZS_Operations op)
{
/*TODO: Dummy! 'op' is ignored, and spans are just added; no overlap handling! */
	if(zs_RegionRealloc(a, b->nspans))
		return ZS_EMEMORY;
	memcpy(a->spans + a->nspans, b->spans, b->nspans * sizeof(ZS_Span));
	a->nspans += b->nspans;
	return ZS_EOK;
}

ZS_Region *zs_RegionCombine(ZS_Region *a, ZS_Region *b, ZS_Operations op)
{
	ZS_Region *r2 = zs_RegionClone(a);
	if(!r2)
		return NULL;
	if(zs_RegionApply(r2, b, op))
	{
		zs_FreeRegion(r2);
		return NULL;
	}
	return NULL;
}

#if 0
ZS_Errors zs_RegionPath(ZS_Region *r, ZS_Path *path, ZS_Operations op)
{
	return ZS_ENOTIMPL;
}
#endif

ZS_Errors zs_RegionRect(ZS_Region *r, float x, float y, float w, float h,
		ZS_Operations op)
{
	ZS_Errors res;
	int cl, c, cr;
	int ix, iy, ix2, iy2, yy;
	ZS_Region *r2 = zs_NewRegion(ceil(h + 1) * 3);
	if(!r2)
		return ZS_EMEMORY;

	ix = ceil(x);
	ix2 = floor(x + w);
	cl = 255 * (1.0f - (ix - x));
	cr = 255 * (1.0f - (ix - x));

	iy = ceil(y);
	iy2 = floor(y + h);

	/* Top row */
	c = 256 * (1.0f - (iy - y));
	zs_RegionAddSpanC(r2, ix - 1, iy - 1, 1, c * cl / 255, c * cl / 255);
	zs_RegionAddSpanC(r2, ix, iy - 1, (ix2 - ix), c, c);
	zs_RegionAddSpanC(r2, ix2, iy - 1, 1, c * cr / 255, c * cr / 255);

	/* Body */
	for(yy = iy; yy <= iy2; ++yy)
	{
		zs_RegionAddSpanC(r2, ix - 1, yy, 1, cl, cl);
		zs_RegionAddSpan(r2, ix, yy, (ix2 - ix));
		zs_RegionAddSpanC(r2, ix2, yy, 1, cr, cr);
	}

	/* Bottom row */
	c = 256 * (1.0f - (y + h - iy2));
	zs_RegionAddSpanC(r2, ix - 1, iy2, 1, c * cl / 255, c * cl / 255);
	zs_RegionAddSpanC(r2, ix, iy2, (ix2 - ix), c, c);
	zs_RegionAddSpanC(r2, ix2, iy2, 1, c * cr / 255, c * cr / 255);

	res = zs_RegionApply(r, r2, op);
	zs_FreeRegion(r2);
	return res;
}

ZS_Errors zs_RegionCircle(ZS_Region *region, float x, float y, float r,
		ZS_Operations op)
{
	return ZS_ENOTIMPL;
}
