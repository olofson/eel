/*(LGPL)
-----------------------------------------------------------------
	ZeeDraw - Scene Graph Animation Engine
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

/*
TODO:
	* Direct and retained modes! Same API calls...?
	* How to deal with properties and inheritance/overriding?
 */

#ifndef	ZS_ZEEDRAW_H
#define	ZS_ZEEDRAW_H

/* Error codes */
typedef enum
{
	ZD_EOK = 0,
	ZD_EMEMORY,
	ZD_ENOTIMPLEMENTED
} ZD_errors;

/* Texture pixel formats */
typedef enum
{
	ZD_OFF = 0,	/* No texture! (That is, all white.) */
//	ZD_I,		/* I 8 (grayscale Intensity channel) */
	ZD_RGB,		/* RGB 8:8:8 */
	ZD_RGBA		/* RGBA 8:8:8:8 */
} ZD_pixelformats;

/* ZeeDraw "base classes" */
typedef struct ZD_entity ZD_entity;
typedef struct ZD_texture ZD_texture;


struct ZD_entity
{
	ZD_entity	*parent;
	ZD_entity	*next;
	ZD_entity	*children;
//	ZD_matrix	transform;
//	ZD_vector	p, v, a;
};

struct ZD_texture
{
//TODO: Filtering, wrapping, mipmapping etc
	unsigned	w, h;
	ZD_pixelformats	format;
	unsigned char	*pixels;
	long		rid;	/* Texture name/id/whatever for renderer */
	void		*rdata;	/* Private renderer data, if any */
};


/*
 * Open/Close
 */
ZD_entity *zd_Open(const char *renderer);
void zd_Close(ZD_entity *state);

/*
 * Textures
 */
ZD_texture *zd_NewTexture(ZD_entity *state, unsigned w, unsigned h,
		ZD_pixelformats format, void *pixels, unsigned stride);

/*
 * Rendering
 */
ZD_errors zd_Render(ZD_entity *entity);

#endif /*ZS_ZEEDRAW_H*/
