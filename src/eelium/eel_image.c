/*
---------------------------------------------------------------------------
	eel_image.c - EEL SDL_image Binding
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2008-2010, 2014, 2019 David Olofson
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

#include "eel_image.h"
#include "eel_sdl.h"
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
	x = eel_o_construct(vm, esdl_md.surface_cid, NULL, 0, vm->heap + vm->resv);
	if(x)
	{
		SDL_FreeSurface(to);
		return x;
	}
	o2ESDL_surface(vm->heap[vm->resv].objref.v)->surface = to;
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
	if(EEL_CLASS(arg + 1) != esdl_md.surface_cid)
			return EEL_XWRONGTYPE;
	from = o2ESDL_surface(arg[1].objref.v)->surface;

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


EEL_xno eel_image_init(EEL_vm *vm)
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
