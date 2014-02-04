/*(LGPLv2.1)
---------------------------------------------------------------------------
	eb_image.c - EEL SDL_image Binding
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2008-2010 David Olofson
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

#include "eb_image.h"
#include "eb_sdl.h"
#include "SDL_image.h"
#include "IMG_savepng.h"


static EEL_xno img_Load(EEL_vm *vm)
{
	EEL_xno x;
	SDL_Surface *to;
	const char *fn = eel_v2s(vm->heap + vm->argv);
	if(!fn)
		return EEL_XNEEDSTRING;
	to = IMG_Load(fn);
	if(!to)
		return EEL_XDEVICEERROR;
	x = eel_o_construct(vm, eb_md.surface_cid, NULL, 0, vm->heap + vm->resv);
	if(x)
	{
		SDL_FreeSurface(to);
		return x;
	}
	o2EB_surface(vm->heap[vm->resv].objref.v)->surface = to;
	return 0;
}


static EEL_xno img_SavePNG(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	int res;
	SDL_Surface *from;
	int comp = -1;

	/* Filename */
	const char *fn = eel_v2s(arg);
	if(!fn)
		return EEL_XNEEDSTRING;

	/* Surface */
	if(EEL_TYPE(arg + 1) != eb_md.surface_cid)
			return EEL_XWRONGTYPE;
	from = o2EB_surface(arg[1].objref.v)->surface;

	/* Compression level */
	if(vm->argc >= 3)
	{
		comp = eel_v2l(arg + 2);
#if 0
		if(comp < -1)
			return EEL_XLOWVALUE;
		if(comp > 9)
			return EEL_XHIGHVALUE;
#endif
	}

	res = IMG_SavePNG(fn, from, comp);
	if(res)
		return EEL_XCANTWRITE;
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno img_unload(EEL_object *m, int closing)
{
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp img_constants[] =
{
	/* Compression levels */
	{"COMPRESS_OFF",	IMG_COMPRESS_OFF},
	{"COMPRESS_MAX",	IMG_COMPRESS_MAX},
	{"COMPRESS_DEFAULT",	IMG_COMPRESS_DEFAULT},
	{"UPSIDEDOWN",		IMG_UPSIDEDOWN},

	{NULL, 0}
};


EEL_xno eb_image_init(EEL_vm *vm)
{
	EEL_object *m;
	m = eel_create_module(vm, "SDL_image", img_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	eel_export_cfunction(m, 1, "Load", 1, 0, 0, img_Load);
	eel_export_cfunction(m, 0, "SavePNG", 2, 1, 0, img_SavePNG);

	/* Constants and enums */
	eel_export_lconstants(m, img_constants);

	eel_disown(m);
	return 0;
}
