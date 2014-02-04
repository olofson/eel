/*(LGPLv2.1)
---------------------------------------------------------------------------
	eb_sdl.h - EEL SDL Binding
---------------------------------------------------------------------------
 * Copyright (C) 2005, 2007, 2009, 2011 David Olofson
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

#ifndef EELBOX_SDL_H
#define EELBOX_SDL_H

#include "EEL.h"
#include "SDL.h"
#include "sfifo.h"

EEL_MAKE_CAST(SDL_Rect)


/* Surface */
typedef struct
{
	SDL_Surface	*surface;
} EB_surface;
EEL_MAKE_CAST(EB_surface)


/* SurfaceLock */
typedef struct
{
	EEL_object	*surface;
} EB_surfacelock;
EEL_MAKE_CAST(EB_surfacelock)


/* Joystick */
typedef struct
{
	int		index;
	EEL_object	*name;
	SDL_Joystick	*joystick;
} EB_joystick;
EEL_MAKE_CAST(EB_joystick)


/* Module instance data */
typedef struct
{
	/* Class Type IDs */
	int		rect_cid;
	int		surface_cid;
	int		surfacelock_cid;
	int		joystick_cid;

	/* Current video surface, if any */
	EEL_object	*video_surface;

	/* Audio interface */
	int		audio_open;
	int		audio_pos;
	sfifo_t		audiofifo;
} EB_moduledata;

extern EB_moduledata eb_md;

EEL_xno eb_sdl_init(EEL_vm *vm);

#endif /* EELBOX_SDL_H */
