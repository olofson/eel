/*(LGPL)
-----------------------------------------------------------------
	zs_path.c - ZeeSpace Path
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

#include <stdlib.h>
#include "zs_path.h"


ZS_Path *zs_NewPath(ZS_State *state)
{
	ZS_Path *p = (ZS_Path *)calloc(1, sizeof(ZS_Path));
	return p;
}


void zs_FreePath(ZS_Path *path)
{
	while(path->elements)
	{
		ZS_Element *e = path->elements;
		path->elements = e->next;
		free(e);
	}
	free(path);
}


static ZS_Element *zs_AddElement(ZS_Path *path, ZS_Elements type)
{
	ZS_Element *e = (ZS_Element *)calloc(1, sizeof(ZS_Element));
	if(!e)
		return NULL;
	if(path->last)
		path->last->next = e;
	else
		path->elements = path->start = path->last = e;
	e->type = type;
	return e;
}


ZS_Errors zs_PathMerge(ZS_Path *to, ZS_Path *from)
{
	while()
	return ZS_OK;
}


ZS_Errors zs_PathLine(ZS_Path *path, float x0, float y0, float x1, float y1);
{
	return ZS_OK;
}


ZS_Errors zs_PathCurve(ZS_Path *path, float x0, float y0, float x1, float y1,
		float x2, float y2, float x3, float y3)
{
	return ZS_OK;
}


ZS_Errors zs_PathClose(ZS_Path *path)
{
	if(!path->start)
		return ZS_ENOPATH;
	return ZS_OK;
}
