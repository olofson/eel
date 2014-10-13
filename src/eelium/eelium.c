/*
---------------------------------------------------------------------------
	eelium.c - EEL binding to SDL, OpenGL, Audiality 2 etc
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
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

#include "eelium.h"
#include "SDL.h"
#include "SDL_net.h"
#include "fastevents.h"
#include "net2.h"
#include "eel_sdl.h"
#include "eel_net.h"
#include "eel_image.h"
#include "eel_opengl.h"
#include "eel_midi.h"
#include "eel_zeespace.h"
#include "eel_audiality2.h"
#include "eel_physics.h"


/*----------------------------------------------------------
	Tiny module for Eelium versioning and stuff
----------------------------------------------------------*/
static EEL_xno e_unload(EEL_object *m, int closing)
{
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


static EEL_xno e_version(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, EEL_MAJOR_VERSION);
	x = eel_setlindex(a, 0, &v);
	if(!x)
	{
		eel_l2v(&v, EEL_MINOR_VERSION);
		x = eel_setlindex(a, 1, &v);
		if(!x)
		{
			eel_l2v(&v, EEL_MICRO_VERSION);
			x = eel_setlindex(a, 2, &v);
		}
	}
	if(x)
	{
		eel_disown(a);
		return x;
	}
	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}


static EEL_xno e_init(EEL_vm *vm)
{
	EEL_object *m = eel_create_module(vm, "eelium", e_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;
	eel_export_cfunction(m, 1, "version", 0, 0, 0, e_version);
	eel_disown(m);
	return 0;
}


/*----------------------------------------------------------
	Table of Eelium modules
----------------------------------------------------------*/

static struct
{
	const char *name;
	EEL_xno (*fn)(EEL_vm *vm);
} bindings[] = {
	{"eelium",	e_init},
	{"SDL",		eel_sdl_init},
	{"NET2",	eel_net_init},
	{"SDL_image",	eel_image_init},
	{"OpenGL",	eel_gl_init},
	{"midi",	eel_midi_init},
	{"ZeeSpace",	eel_zeespace_init},
	{"Audiality2",	eel_audiality2_init},
	{"physicsbase",	eph_init},
	{NULL, NULL}
};


EEL_xno eelium_init_bindings(EEL_vm *vm)
{
	EEL_xno x;
	int i;
	for(i = 0; bindings[i].name; ++i)
	{
		x = bindings[i].fn(vm);
		if(x)
		{
			fprintf(stderr, "Could not initialize module \"%s\"!\n",
					bindings[i].name);
			return x;
		}
	}
	return EEL_XOK;
}


/*----------------------------------------------------------
	SDL subsystems open/close
----------------------------------------------------------*/

int eelium_open_subsystems(void)
{
	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO
#ifndef _WIN32
			| SDL_INIT_EVENTTHREAD
#endif
#ifdef DEBUG
			| SDL_INIT_NOPARACHUTE
#endif
			) == 0)
	{
		if(FE_Init() == 0)
		{
			if(SDLNet_Init() == 0)
			{
				if(NET2_Init() == 0)
					return 0;
				fprintf(stderr, "Eelium: Could not initialize"
						" Net2!\n");
				SDLNet_Quit();
			}
			fprintf(stderr, "Eelium: Could not initialize"
					" SDL_net!\n");
			FE_Quit();
		}
		fprintf(stderr, "Eelium: Could not initialize FastEvents!\n");
		SDL_Quit();
	}
	fprintf(stderr, "Eelium: Could not initialize SDL!\n");
	return -1;
}


void eelium_close_subsystems(void)
{
	NET2_Quit();
	SDLNet_Quit();
	FE_Quit();
	SDL_Quit();
}
