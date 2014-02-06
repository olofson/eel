/*
-----------------------------------------------------------------
	zs_path.c - ZeeSpace Path
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
