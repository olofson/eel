/*
-----------------------------------------------------------------
	zs_path.h - ZeeSpace Path
-----------------------------------------------------------------
 * Copyright 2011 David Olofson
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
