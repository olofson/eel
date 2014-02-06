/*
---------------------------------------------------------------------------
	EELBox - A Quick Hack Binding to SDL And Some Helper Libs
---------------------------------------------------------------------------
 * Copyright 2005-2007, 2009-2012, 2014 David Olofson
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

#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"
#include "SDL_net.h"
#include "fastevents.h"
#include "net2.h"

#include "EEL.h"
#include "eb_version.h"
#include "eb_sdl.h"
#include "eb_net.h"
#include "eb_image.h"
#include "eb_opengl.h"
#include "eel_midi.h"
#include "eel_zeespace.h"
#include "eel_audiality2.h"
#include "eel_physics.h"


/*----------------------------------------------------------
	Tiny module for EELBox versioning and stuff
----------------------------------------------------------*/
static EEL_xno eb_unload(EEL_object *m, int closing)
{
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


static EEL_xno eb_version(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, EELBOX_MAJOR_VERSION);
	x = eel_setlindex(a, 0, &v);
	if(!x)
	{
		eel_l2v(&v, EELBOX_MINOR_VERSION);
		x = eel_setlindex(a, 1, &v);
		if(!x)
		{
			eel_l2v(&v, EELBOX_MICRO_VERSION);
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


static EEL_xno eb_init(EEL_vm *vm)
{
	EEL_object *m = eel_create_module(vm, "eelbox", eb_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;
	eel_export_cfunction(m, 1, "version", 0, 0, 0, eb_version);
	eel_disown(m);
	return 0;
}


/*----------------------------------------------------------
	main() with helpers
----------------------------------------------------------*/
static int args2args(EEL_vm *vm, const char *sname,
		int argc, const char *argv[], int *resv)
{
	EEL_xno x;
	int i;
	x = eel_argf(vm, "*Rs", resv, sname);
	if(x)
		return x;
	for(i = 0; i < argc; ++i)
		if( (x = eel_argf(vm, "s", argv[i])) )
			return x;
	return 0;
}


static void usage(const char *exename)
{
	EEL_uint32 v = eel_lib_version();
	fprintf(stderr, ".------------------------------------------------\n");
	fprintf(stderr, "| EELBox v%d.%d.%d - An SDL binding for EEL\n",
			EELBOX_MAJOR_VERSION,
			EELBOX_MINOR_VERSION,
			EELBOX_MICRO_VERSION);
	fprintf(stderr, "| Copyright 2005-2014 David Olofson\n");
	fprintf(stderr, "| Using EEL v%d.%d.%d\n"
			"|------------------------------------------------\n",
			EEL_GET_MAJOR(v),
			EEL_GET_MINOR(v),
			EEL_GET_MICRO(v));
	fprintf(stderr, "| Usage: %s [switches] <file> [arguments]\n", exename);
	fprintf(stderr, "| Switches:  -c  Compile only; don't run\n");
	fprintf(stderr, "|-           -l  List symbol tree\n");
	fprintf(stderr, "|--          -a  List VM assembly code\n");
	fprintf(stderr, "|---         -h  Help\n");
	fprintf(stderr, "'------------------------------------------------\n");
	exit(100);
}


static int open_subsystems(void)
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
				fprintf(stderr, "EELBox: Could not initialize"
						" Net2!\n");
				SDLNet_Quit();
			}
			fprintf(stderr, "EELBox: Could not initialize"
					" SDL_net!\n");
			FE_Quit();
		}
		fprintf(stderr, "EELBox: Could not initialize FastEvents!\n");
		SDL_Quit();
	}
	fprintf(stderr, "EELBox: Could not initialize SDL!\n");
	return -1;
}


static void close_subsystems(void)
{
	NET2_Quit();
	SDLNet_Quit();
	FE_Quit();
	SDL_Quit();
}


static struct
{
	const char *name;
	EEL_xno (*fn)(EEL_vm *vm);
} bindings[] = {
	{"eelbox",	eb_init},
	{"SDL",		eb_sdl_init},
	{"NET2",	eb_net_init},
	{"SDL_image",	eb_image_init},
	{"OpenGL",	ebgl_init},
	{"midi",	eel_midi_init},
	{"ZeeSpace",	eel_zeespace_init},
	{"Audiality2",	eel_audiality2_init},
	{"physicsbase",	eph_init},
	{NULL, NULL}
};


int main(int argc, const char *argv[])
{
	EEL_xno x;
	int result, i;
	EEL_object *m;
	EEL_vm *vm;
	int resv = -1;
	int flags = 0;
	int run = 1;
#ifdef MAIN_AUTOSTART
	const char *defname = "main";
	const char *name = defname;
#else
	const char *name = NULL;
#endif
	int eelargc;
	const char **eelargv;

	for(i = 1; i < argc; ++i)
	{
		int j;
		if('-' != argv[i][0])
		{
			name = argv[i];
			++i;
			break;	/* Pass the rest to the EEL script! */
		}
		for(j = 1; argv[i][j]; ++j)
			switch(argv[i][j])
			{
			  case 'c':
				run = 0;
				break;
			  case 'l':
				flags |= EEL_SF_LIST;
				break;
			  case 'a':
				flags |= EEL_SF_LISTASM;
				break;
			  case 'h':
			  default:
				usage(argv[0]);
			}
	}
	eelargv = argv + i;
	eelargc = argc - i;
	if(!name)
	{
		fprintf(stderr, "No source file name!\n");
		usage(argv[0]);
	}

	/* Init the EEL engine */
	vm = eel_open(argc, argv);
	if(!vm)
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not initialize EEL!\n");
		return 2;
	}

	/* Init the bindings */
	for(i = 0; bindings[i].name; ++i)
	{
		x = bindings[i].fn(vm);
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not initialize module \"%s\"!\n",
					bindings[i].name);
			eel_close(vm);
			return 3;
		}
	}

	/* Load the script */
	m = eel_load(vm, name, flags);
	if(!m)
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not load script!\n");
		eel_close(vm);
		return 4;
	}

	if(run)
	{
		/* Pass arguments and call the script's 'main' function */
		x = args2args(vm, name, eelargc, eelargv, &resv);
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not pass arguments! (%s)\n",
					eel_x_name(vm, x));
			eel_disown(m);
			eel_close(vm);
			return 5;
		}

		if(open_subsystems() < 0)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not open subsystems!\n");
			eel_disown(m);
			eel_close(vm);
			return 6;
		}

		/* Run the script! */
		x = eel_calln(vm, m, "main");
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Failure in main function! (%s)\n",
					eel_x_name(vm, x));
			eel_disown(m);
			eel_close(vm);
			close_subsystems();
			return 7;
		}

		/* Get the return value */
		result = eel_v2l(vm->heap + resv);
#ifdef DEBUG
		printf("Return value: %d (%s @ heap[%d])\n", result,
				eel_v_stringrep(vm, vm->heap + resv), resv);
#endif
	}
	else
		result = 0;

	eel_perror(vm, 0);

#ifdef MAIN_AUTOSTART
	if(result && name == defname)
	{
		fprintf(stderr, "Could not run default main program!\n");
		usage(argv[0]);
	}
#endif

	/* Clean up */
	if(m)
		eel_disown(m);
	eel_close(vm);
	close_subsystems();
	return result;
}
