/*(LGPL)
-----------------------------------------------------------------
	zs_path.h - ZeeSpace Path
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

#ifndef	ZS_PATH_H
#define	ZS_PATH_H

#include "zs_types.h"

typedef struct ZS_Element ZS_Element;

typedef enum ZS_Elements
{
	ZS_CLOSE = 0,	/* Close current subpath */
	ZS_LINE,	/* Line segment */
	ZS_CUBIC	/* Cubic Bezier curve segment */
} ZS_Elements;

struct ZS_Element
{
	ZS_Element	*next;
	ZS_Elements	type;	/* Element type */
	float	x[4], y[4];	/* Control points */
};

typedef struct ZS_Path
{
	ZS_Element	*elements;	/* Linked list of elements */
	ZS_Element	*start;		/* First element in last subpath */
	ZS_Element	*last;		/* Last element */
} ZS_Path;


/*---------------------------------------------------------------
	Path management and operations
---------------------------------------------------------------*/

ZS_Path *zs_NewPath(ZS_State *state);
void zs_FreePath(ZS_Path *path);

/* Merge path 'from' into path 'to'. ('from' is not modified!) */
/*
TODO: Transforms...?
 */
ZS_Errors zs_PathMerge(ZS_Path *to, ZS_Path *from);

/* Add a line segment from (x0, y0) through (x1, y1) */
ZS_Errors zs_PathLine(ZS_Path *path, float x0, float y0, float x1, float y1);

/*
 * Add a Bezier curve segment from (x0, y0) through (x3, y3), with control points
 * (x1, y1) and (x2, y2) for the start and end points respectively.
 */
ZS_Errors zs_PathCurve(ZS_Path *path, float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3);

/*
 * Close current subpath. If the last segment did not end at the starting point
 * of the first segment, a line segment is added to close the subpath.
 */
ZS_Errors zs_PathClose(ZS_Path *path);

/*
TODO: End *without* closing. However, we have no use for open paths in ZeeSpace
TODO: right now (we use them only to construct regions), so we'll just leave
TODO: this out for now.
ZS_Errors zs_PathEnd(ZS_Path *path);
*/

#endif	/* ZS_PATH_H */
