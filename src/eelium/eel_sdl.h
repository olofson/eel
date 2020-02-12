/*
---------------------------------------------------------------------------
	eel_sdl.h - EEL SDL Binding
---------------------------------------------------------------------------
 * Copyright 2005, 2007, 2009, 2011, 2013-2014, 2017, 2019 David Olofson
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

#ifndef EELIUM_SDL_H
#define EELIUM_SDL_H

#include "EEL.h"
#include "SDL.h"
#include "sfifo.h"

/* Argument handling macros */
/* TODO: Generalize this, and move it into EEL. */
#define	ESDL_ARG(i)	(vm->heap + vm->argv + (i))
#define	ESDL_ARG_DEF(i)	(vm->argc > (i))
#define	ESDL_ARG_NIL(i)	(EEL_CLASS(ESDL_ARG(i)) == EEL_CNIL)

#define ESDL_OPTIONAL(t, i, n, d)					\
{									\
	if(ESDL_ARG_DEF(i))						\
		ESDL_ARG_##t(i, n)					\
	else								\
		n = d;							\
}

#define ESDL_OPTIONAL_NIL(t, i, n, d)					\
{									\
	if(ESDL_ARG_DEF(i) && !ESDL_ARG_NIL(i))				\
		ESDL_ARG_##t(i, n)					\
	else								\
		n = d;							\
}

/* Integer arguments */
#define	ESDL_ARG_INTEGER(i, n)						\
{									\
	n = eel_v2l(ESDL_ARG(i));					\
}

#define	ESDL_OPTARG_INTEGER(i, n, d)	ESDL_OPTIONAL(INTEGER, i, n, d)

#define	ESDL_OPTARGNIL_INTEGER(i, n, d)	ESDL_OPTIONAL_NIL(INTEGER, i, n, d)

/* Real arguments */
#define	ESDL_ARG_REAL(i, n)						\
{									\
	n = eel_v2d(ESDL_ARG(i));					\
}

#define	ESDL_OPTARG_REAL(i, n, d)	ESDL_OPTIONAL(REAL, i, n, d)

/* String arguments */
#define	ESDL_ARG_STRING(i, n)						\
{									\
	if(!(n = eel_v2s(ESDL_ARG(i))))					\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_STRING(i, n, d)	ESDL_OPTIONAL(STRING, i, n, d)

#define	ESDL_OPTARGNIL_STRING(i, n, d)	ESDL_OPTIONAL_NIL(STRING, i, n, d)


/* Rect */
EEL_MAKE_CAST(SDL_Rect)

#define	ESDL_ARG_RECT(i, n)						\
{									\
	if(EEL_CLASS(ESDL_ARG(i)) == esdl_md.rect_cid)			\
		n = o2SDL_Rect(ESDL_ARG(i)->objref.v);			\
	else								\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_RECT(i, n, d)	ESDL_OPTIONAL(RECT, i, n, d)

#define	ESDL_OPTARGNIL_RECT(i, n, d)	ESDL_OPTIONAL_NIL(RECT, i, n, d)


/* Window */
typedef struct
{
	SDL_Window	*window;
} ESDL_window;

EEL_MAKE_CAST(ESDL_window)

#define	ESDL_ARG_WINDOW(i, n)						\
{									\
	if(EEL_CLASS(ESDL_ARG(i)) == esdl_md.window_cid)			\
		n = o2ESDL_window(ESDL_ARG(i)->objref.v)->window;	\
	else								\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_WINDOW(i, n, d)	ESDL_OPTIONAL(WINDOW, i, n, d)

#define	ESDL_OPTARGNIL_WINDOW(i, n, d)	ESDL_OPTIONAL_NIL(WINDOW, i, n, d)


/* Renderer */
typedef struct
{
	SDL_Renderer	*renderer;
} ESDL_renderer;

EEL_MAKE_CAST(ESDL_renderer)

#define	ESDL_ARG_RENDERER(i, n)						\
{									\
	if(EEL_CLASS(ESDL_ARG(i)) == esdl_md.renderer_cid)		\
		n = o2ESDL_renderer(ESDL_ARG(i)->objref.v)->renderer;	\
	else								\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_RENDERER(i, n, d)	ESDL_OPTIONAL(RENDERER, i, n, d)

#define	ESDL_OPTARGNIL_RENDERER(i, n, d) ESDL_OPTIONAL_NIL(RENDERER, i, n, d)


/* OpenGL context */
typedef struct
{
	SDL_GLContext	*context;
} ESDL_glcontext;

EEL_MAKE_CAST(ESDL_glcontext)


/* Texture */
typedef struct
{
	SDL_Texture	*texture;
} ESDL_texture;

EEL_MAKE_CAST(ESDL_texture)

#define	ESDL_ARG_TEXTURE(i, n)						\
{									\
	if(EEL_CLASS(ESDL_ARG(i)) == esdl_md.texture_cid)		\
		n = o2ESDL_texture(ESDL_ARG(i)->objref.v)->texture;	\
	else								\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_TEXTURE(i, n, d)	ESDL_OPTIONAL(TEXTURE, i, n, d)

#define	ESDL_OPTARGNIL_TEXTURE(i, n, d)	ESDL_OPTIONAL_NIL(TEXTURE, i, n, d)


/* Surface */
typedef struct
{
	SDL_Surface	*surface;
	int		is_window_surface;
} ESDL_surface;

EEL_MAKE_CAST(ESDL_surface)

#define	ESDL_ARG_SURFACE(i, n)						\
{									\
	if(EEL_CLASS(ESDL_ARG(i)) == esdl_md.surface_cid)		\
		n = o2ESDL_surface(ESDL_ARG(i)->objref.v)->surface;	\
	else								\
		return EEL_XWRONGTYPE;					\
}

#define	ESDL_OPTARG_SURFACE(i, n, d)	ESDL_OPTIONAL(SURFACE, i, n, d)

#define	ESDL_OPTARGNIL_SURFACE(i, n, d)	ESDL_OPTIONAL_NIL(SURFACE, i, n, d)


/* SurfaceLock */
typedef struct
{
	EEL_object	*surface;
} ESDL_surfacelock;

EEL_MAKE_CAST(ESDL_surfacelock)


/* Joystick */
typedef struct ESDL_joystick ESDL_joystick;

struct ESDL_joystick
{
	ESDL_joystick	*next;
	int		index;
	EEL_object	*name;
	SDL_Joystick	*joystick;
};

EEL_MAKE_CAST(ESDL_joystick)


/* Module instance data */
typedef struct
{
	/* Class Type IDs */
	int		rect_cid;
	int		window_cid;
	int		renderer_cid;
	int		glcontext_cid;
	int		texture_cid;
	int		surface_cid;
	int		surfacelock_cid;
	int		joystick_cid;

	/* Linked list of joysticks */
	ESDL_joystick	*joysticks;

	/* Audio interface */
	int		audio_open;
	int		audio_pos;
	sfifo_t		audiofifo;
} ESDL_moduledata;

extern ESDL_moduledata esdl_md;

EEL_xno eel_sdl_init(EEL_vm *vm);

#endif /* EELIUM_SDL_H */
