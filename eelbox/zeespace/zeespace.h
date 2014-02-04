/*(LGPL)
-----------------------------------------------------------------
	ZeeSpace - 2D + Height Field Rendering Engine
-----------------------------------------------------------------
 * Copyright (C) 2005, 2011 David Olofson
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

#ifndef	ZS_ZEESPACE_H
#define	ZS_ZEESPACE_H

#include "SDL.h"
#include "zs_pipe.h"
#include "zs_region.h"

/*
 * ZeeSpace and multithreading
 * ---------------------------
 * There are three rules you need to keep in mind:
 *	1. Use one State for each thread that wants to use ZeeSpace.
 *	2. Only Surfaces can be shared - no other ZeeSpace objects.
 *	3. Only the thread that created a Surface may destroy it.
 *
 * Also note that surfaces are not locked while operated on, so
 * multiple threads may be working in the same surface at the same
 * time. (Quite literally so on SMP and multicore systems!) Just make
 * sure they're not doing it in the same area of the surface...!
 */

/*
TODO: "Path + brush" or similar rendering tool.

TODO: The terrain generator should deal only with Z, and
TODO: it should have some kind of feature that allows
TODO: large, seamless terrain areas to be rendered tile by
TODO: tile, without rendering level tile/tile dependencies.

TODO: Cool looking ZeeSpace logo animation. :-)
*/

/* Masks used for selecting channels (used by zs_Raw*() calls) */
typedef enum
{
	ZS_CH_R =	0x00000001,
	ZS_CH_G =	0x00000002,
	ZS_CH_B =	0x00000004,
	ZS_CH_A =	0x00000008,
	ZS_CH_I =	0x00000010,
	ZS_CH_Z =	0x00000020,

	/* Shorthand definitions */
	ZS_CH_RGB =	ZS_CH_R | ZS_CH_G | ZS_CH_B,
	ZS_CH_RGBA =	ZS_CH_RGB | ZS_CH_A,
	ZS_CH_IZ =	ZS_CH_I | ZS_CH_Z,

	/* Everything (doubles as "relevant bits" mask!) */
	ZS_CH_ALL =	ZS_CH_RGBA | ZS_CH_IZ
} ZS_Channels;


/* Surface flags */
typedef enum
{
	ZS_SF_OWNSPIXELS = 0x00000001
} ZS_SurfaceFlags;


struct ZS_Surface
{
	ZS_State	*state;		/* State this surface belongs to */
	ZS_Surface	*parent;	/* Owner of 'pixel'; NULL if this Surface */
	ZS_Pixel	*pixels;	/* Pixel data 2D array */
	int		refcount;
	int		w, h;		/* Size */
	int		xo, yo;		/* Coordinate of top-left corner */
	int		pitch;		/* Pixels per physical row */
	int		flags;
};


/*---------------------------------------------------------------
	State management
---------------------------------------------------------------*/

ZS_State *zs_Open(void);
void zs_Close(ZS_State *state);


/*---------------------------------------------------------------
	Low level tools
---------------------------------------------------------------*/

/* Fast, non-clipping way of getting a pixel in a surface */
static inline ZS_Pixel *zs_Pixel(ZS_Surface *s, int x, int y)
{
	return &s->pixels[y * s->pitch + x];
}


/* Clipping version (returns NULL if outside) */
static inline ZS_Pixel *zs_PixelClip(ZS_Surface *s, int x, int y)
{
	if((x < 0) || (x > s->w - 1) || (y < 0) || (y > s->h - 1))
		return NULL;
	return zs_Pixel(s, x, y);
}


/* Edge clamping version */
static inline ZS_Pixel *zs_PixelClamp(ZS_Surface *s, int x, int y)
{
	if(x < 0)
		x = 0;
	else if(x > s->w - 1)
		x = s->w - 1;
	if(y < 0)
		y = 0;
	else if(y > s->h - 1)
		y = s->h - 1;
	return zs_Pixel(s, x, y);
}


/*---------------------------------------------------------------
	Pseudo-random number generator
---------------------------------------------------------------*/
/* Reset random number generator and set set1/set2 'balance'. */
void zs_RndReset(ZS_State *state, float balance);

/* Get value in the range [0, range] */
int zs_RndGet(ZS_State *state, unsigned range);


/*---------------------------------------------------------------
	Surface/Window/Region management
---------------------------------------------------------------*/
/* Create new ZeeSpace surface */
ZS_Surface *zs_NewSurface(ZS_State *state, int w, int h);

/* Increment the refcount of surface 's' */
void zs_SurfaceAddRef(ZS_Surface *s);

/* Create a window in 'from' with local coordinate system */
ZS_Surface *zs_Window(ZS_Surface *from, ZS_Rect *area);

/* Create a clipped view of 'area' in 'from' */
ZS_Surface *zs_View(ZS_Surface *from, ZS_Rect *area);

/* Release surface created by zs_NewSurface(), zs_Window() or zs_Region() */
void zs_FreeSurface(ZS_Surface *s);


/*---------------------------------------------------------------
	Raw rendering primitives (No pipelines!)
---------------------------------------------------------------*/
/*
 * Set 'channels' of surface 's' in area 'r' from brush pixel 'b'.
 * The enabled channels are just overwritten, regardless of the
 * values of any channels in 's'. (That is, Z is ignored!)
 */
void zs_RawFill(ZS_Channels channels, ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p);

/*
 * Copy 'channels' from area 'srcr' in surface 'src' to surface 'dst',
 * mapping the top-left corner of the area to (x, y) in 'dst'.
 * Data is just copied as is, regardless of source and destination
 * channel data.
 */
void zs_RawCopy(ZS_Channels channels, ZS_Surface *src, ZS_Rect *srcr,
		ZS_Surface *dst, int x, int y);


/*---------------------------------------------------------------
	2.5D rendering primitives (Using pipelines)
---------------------------------------------------------------*/
/*
 * Render a rectangular block, taking the source data from brush
 * pixel 'b', and rendering into rectangle ('x', 'y') x ('w', 'h')
 * in surface 's'.
 */
void zs_Block(ZS_Pipe *p, ZS_Surface *s,
		float x, float y, float w, float h, ZS_Pixel *px);

/*
 * Render rectangle 'srcr' from surface 'src' into surface 'dst',
 * mapping the top-left corner to ('x', 'y') and offsetting the
 * Z axis by 'z'.
 */
void zs_Blit(ZS_Pipe *p, ZS_Surface *src, ZS_Rect *srcr,
		ZS_Surface *dst, float x, float y);

/*
 * Render a cylinder, cone, beehive (cubic dome "approximation"),
 * or dome shape, respectively, into surface 's', taking colors,
 * alpha and I from 'px'. The shape will be centered at ('cx',
 * 'cy'), 'r' units in radius, based at Z from 'px', and building
 * 'h' units upwards from there.
 *    A dome will be compressed or stretched as needed, if 'h'
 * differs from 'r'.
 *    A cylinder does not have a 'h' argument, as it has only
 * one Z level: the top surface, which is specified through 'z' in
 * 'px'.
 */
void zs_Cylinder(ZS_Pipe *p, ZS_Surface *s,
		float cx, float cy, float r, ZS_Pixel *px);
void zs_Cone(ZS_Pipe *p, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px);
void zs_Beehive(ZS_Pipe *p, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px);
void zs_Dome(ZS_Pipe *p, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px);

/*FIXME:
 *	Temporary hack for the K2 tech demo. Applies to the quadratic area
 *	surrounding the dome as well.
 */
void zs_DomeRect(ZS_Pipe *p, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px);
/*FIXME:*/


/*---------------------------------------------------------------
	Region based rendering
---------------------------------------------------------------*/
/*
 * Fill the area covered by region 'r' of surface 's' with the
 * values from 'px', using pipe 'p'.
 */
void zs_Fill(ZS_Pipe *p, ZS_Region *r, ZS_Surface *s, ZS_Pixel *px);


/*---------------------------------------------------------------
	SDL rendering utilities
---------------------------------------------------------------*/
void zs_Blit2SDL(ZS_Surface *from, SDL_Surface *to, int tx, int ty, int blend);
void zs_3D2SDL(ZS_Surface *from, SDL_Surface *to, float pitch);
void zs_Graph2SDL(ZS_Surface *from, SDL_Surface *to, int scale);
void zs_Z2SDL(ZS_Surface *from, SDL_Surface *to, int scale);
void zs_I2SDL(ZS_Surface *from, SDL_Surface *to);


/*---------------------------------------------------------------
	Lighting tools
---------------------------------------------------------------*/
/*
 * Apply simple shadows to 's'. 'altitude' is the angle from horizon to light
 * source in degrees, and 'scale' is the x,y : z ratio'. 'depth' is the maximum
 * amount of light removed and 'hardness' controls the hardness of the edes of
 * the shadows.
 *
 * NOTE:
 *	This tool *removes* light from the I channel, so the surface must be
 *	initialized with a desired level of ambient light for any shadows to be
 *	visible. Lowering 'depth' gives the impression of increasing the amount
 *	of ambient light in the scene.
 */
void zs_SimpleShadow(ZS_Surface *s, float altitude, float scale, float depth,
		float hardness);

/*
 * Apply bumpmapping to 's'. 'altitude' is the angle from horizon to light
 * source in degrees, and 'scale' is the x,y : z ratio'.
 *
 * NOTE:
 *	zs_BumpMap() is a multiplicative operation, preferably to be applied
 *	after shadowcasting in the general case, so that it uses whatever light
 *	is available.
 */
void zs_BumpMap(ZS_Surface *s, float altitude, float scale, float depth);


/*---------------------------------------------------------------
	Inter-channel operations
---------------------------------------------------------------*/
/*
 * Apply intensity channel to the RGB channels. 'rm', 'gm' and 'bm' are effect
 * magnitudes; 0.0 means "no effect" (color channel is left as is), whereas
 * 1.0 means "full effect", where Intensity 0.0 ==> 0 and Intensity 1.0 leaves
 * the color channel as is.
 */
void zs_ApplyIntensity(ZS_Surface *s, float rm, float gm, float bm);


/*---------------------------------------------------------------
	Terrain tools
---------------------------------------------------------------*/
/*
 *	'base'
 *		Base ground height; that is, the Z value
 *		generated for a point where the Perlin generator
 *		delivers 0.
 *
 *	'u', 'v', 'du', 'dv'
 *		Perlin space coordinates and per-output-pixel
 *		traverse deltas. These allow scaling and rotation
 *		of the view!
 *
 *	'namplitudes'
 *		Number of explicitly defined octaves to generate.
 *		Unless the last specified amplitude is 0, the
 *		generator will continue adding components, halving
 *		the amplitude for each octave, until the full
 *		output bandwidth is covered.
 *
 *	'amplitudes'
 *		An array holding the maximum amplitudes for each
 *		octave component specified.
 *
 *	'flags'
 *		Flags ORed together;
 *		
 *		ZS_PT_NEAREST, ZS_PT_BILINEAR, ZS_PT_BICUBIC
 *			Specifies what type of interpolation to
 *			use when resampling the Perlin noise.
 *			Only one of these can be used at a time!
 *
 *		ZS_PT_STOP, ZS_PT_EXTEND, ZS_PT_DIV2, ZS_PT_DIV4
 *			Determines how to handle octaves that do
 *			not have their amplitudes explicitly
 *			specified. ZS_PT_STOP stops after
 *			'namplitudes' octaves, ZS_PT_EXTEND
 *			repeats the last value indefinitely, and
 *			ZS_PT_DIVn recursively divides by n to
 *			generate new octave amplitudes.
 *
 *		ZS_PT_T_LINEAR
 *			output = z + base
 *		ZS_PT_T_CUBIC
 *			output = z^3 + base
 *		ZS_PT_T_PCUBIC
 *			output = (0.5 * z + 0.5)^3 * 2 - 1 + base
 *		ZS_PT_T_NCUBIC
 *			output = (0.5 * z - 0.5)^3 * 2 + 1 + base
 *
 *		ZS_PT_O_REPLACE
 *			Output overwrites the Z channel.
 *		ZS_PT_O_ADD
 *			Output is added to the Z channel.
 *		ZS_PT_O_SUB
 *			Output is subtracted from the Z channel.
 *		ZS_PT_O_MUL
 *			Output is multiplied with the Z channel.
 */

typedef enum
{
	ZS_PT__INTERPOLATION =	0x0000000f,
	ZS_PT_NEAREST =		0x00000000,
	ZS_PT_BILINEAR = 	0x00000001,
	ZS_PT_BICUBIC =		0x00000002,
	ZS_PT_ADAPTIVE =	0x00000003,

	ZS_PT__EXTENDMODE =	0x000000f0,
	ZS_PT_STOP =		0x00000000,
	ZS_PT_EXTEND =		0x00000010,
	ZS_PT_DIV1P5 =		0x00000020,
	ZS_PT_DIV2 =		0x00000030,
	ZS_PT_DIV3 =		0x00000040,
	ZS_PT_DIV4 =		0x00000050,

	ZS_PT__TRANSFORM =	0x00000f00,
	ZS_PT_T_LINEAR =	0x00000000,
	ZS_PT_T_CUBIC =		0x00000100,
	ZS_PT_T_PCUBIC =	0x00000200,
	ZS_PT_T_NCUBIC =	0x00000300,

	ZS_PT__OUTPUT =		0x0000f000,
	ZS_PT_O_REPLACE =	0x00000000,
	ZS_PT_O_ADD =		0x00001000,
	ZS_PT_O_SUB =		0x00002000,
	ZS_PT_O_MUL =		0x00003000
} ZS_PTFlags;

void zs_PerlinTerrainZ(ZS_Surface *s, float base,
		float u, float v, float du, float dv,
		int namplitudes, float *amplitudes, int flags);

/*
TODO: Turn this into a more generic zs_ScaleZ() function, that scales all Z
TODO: values within or outside a specified range. Having it operating through a
TODO: complex region with a coverage channel would be even more useful!
 * Simulate air/water transition by compressing Z below the surface.
 * Apply *before* lighting, then apply zs_Fog() *after* lighting.
 */
void zs_WaterZ(ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p, int factor);

/* Apply distance fog - preferably applied after lighting */
void zs_Fog(ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p, int visibility);

#endif /* ZS_ZEESPACE_H */
