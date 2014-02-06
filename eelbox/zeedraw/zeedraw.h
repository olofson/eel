/*
-----------------------------------------------------------------
	ZeeDraw - Scene Graph Animation Engine
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
