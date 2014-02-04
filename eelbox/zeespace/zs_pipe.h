/*(LGPL)
-----------------------------------------------------------------
	zs_pipe.h - ZeeSpace Pixel Processing Pipeline
-----------------------------------------------------------------
 * Copyright (C) 2005, 2010-2011 David Olofson
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

#ifndef	ZS_PIPE_H
#define	ZS_PIPE_H

#include "zs_types.h"


/* Pipeline modes */
typedef enum
{
	/*
	 * Stage 1: Grab, transform and analyze the Z channel.
	 *
	 * This may abort the rest of the pipeline, so it's best done first, to
	 * avoid generating pixels that will just be thrown away.
	 */
	/* Z transforms */
	ZS_Z_ =		0x00000003,
	ZS_Z_NORMAL = 	0x00000000,	/* p.z = clam(s.z + pipe.z) */
	ZS_Z_ADD = 	0x00000001,	/* p.z = clamp(d.z + s.z + pipe.z) */
	ZS_Z_SUBTRACT =	0x00000002,	/* p.z = clamp(d.z - s.z - pipe.z) */
	ZS_Z_MULTIPLY =	0x00000003,	/* p.z = clamp(d.z * s.z * 16) */

	/* Z-buffering modes */
	ZS_ZB_ =	0x0000000c,
	ZS_ZB_ALL = 	0x00000000,	/* No Z-buffering - all pixels are drawn. */
	ZS_ZB_ABOVE = 	0x00000004,	/* if(p.z >= d.z) then render; ("build") */
	ZS_ZB_BELOW = 	0x00000008,	/* if(p.z <= d.z) then render; ("carve") */

	/*
	 * Stage 2: Grab and transform the R, G, B, A and I channels.
	 *
	 * This is where source and destination colors are combined, and alpha
	 * and intensity values are calculated.
	 */
	/* Color transforms */
	ZS_C_ =		0x000000f0,
	ZS_C_NORMAL =	0x00000000,	/* p.rgb = s.rgb */
	ZS_C_ADD =	0x00000010,	/* p.rgb = clamp(d.rgb + s.rgb) */
	ZS_C_SUBTRACT =	0x00000020,	/* p.rgb = clamp(d.rgb - s.rgb) */
	ZS_C_MULTIPLY =	0x00000030,	/* p.rgb = d.rgb * s.rgb */
	ZS_C_DIVIDE =	0x00000040,	/* p.rgb = d.rgb / (1 + s.rgb * 256) */
	ZS_C_LIGHTEN =	0x00000050,	/* p.rgb = max(d.rgb, s.rgb) */
	ZS_C_DARKEN =	0x00000060,	/* p.rgb = min(d.rgb, s.rgb) */
	ZS_C_CONTRAST =	0x00000070,	/* p.rgb = clamp((d.rgb - .5)*(s.rgb * 2) + .5) */
	ZS_C_INVERT =	0x00000080,	/* p.rgb = d.rgb * s.rgb +
							(1 - d.rgb)*(1 - s.rgb) */
	ZS_C_HUE =	0x00000090,	/* p.rgb = hue(d.rgb, get_hue(s.rgb)) */
	ZS_C_SATURATE =	0x000000a0,	/* p.rgb = saturate(d.rgb,
							get_saturation(s.rgb))*/
	ZS_C_COLOR =	0x000000b0,	/* p.rgb = colorize(d.rgb, get_color(s.rgb)) */
	ZS_C_VALUE =	0x000000c0,	/* p.rgb = value(d.rgb, get_value(s.rgb)) */

	/* Alpha transforms */
	ZS_A_ =		0x00000300,
	ZS_A_FIXED =	0x00000000,	/* p.a = pipe.alpha */
	ZS_A_SOURCE =	0x00000100,	/* p.a = s.a * pipe.alpha */
	ZS_A_DEST =	0x00000200,	/* p.a = d.a * pipe.alpha */

	/* Intensity transforms */
	ZS_I_ =		0x00000c00,
	ZS_I_FIXED =	0x00000000,	/* p.i = pipe.intensity */
	ZS_I_SOURCE =	0x00000400,	/* p.i = s.i * pipe.intensity */
	ZS_I_DEST =	0x00000800,	/* p.i = d.i * pipe.intensity */

	/*
	 * Stage 3: Combine and write enabled channels.
	 *
	 * This is where the alpha and intensity channels modulate the RGB
	 * channels, and the resulting output is written.
	 */
	/* Color channel write mask (0 bit ==> d.r/g/b left untouched) */
	ZS_CW_ =	0x00007000,
	ZS_CW_R =	0x00001000,	/* d.r = p.r * p.i * p.a + d.r * (1 - p.a) */
	ZS_CW_G =	0x00002000,	/* d.g = p.g * p.i * p.a + d.g * (1 - p.a) */
	ZS_CW_B =	0x00004000,	/* d.b = p.b * p.i * p.a + d.b * (1 - p.a) */
	ZS_CW_RGB =	ZS_CW_R | ZS_CW_G | ZS_CW_B,

	/* Alpha write modes */
	ZS_AW_ =	0x000f0000,
	ZS_AW_OFF =	0x00000000,	/* NOP */
	ZS_AW_COPY =	0x00010000,	/* d.a = p.a */
	ZS_AW_ADD =	0x00020000,	/* d.a = clamp(d.a + p.a) */
	ZS_AW_SUBTRACT=	0x00030000,	/* d.a = clamp(d.a - p.a) */
	ZS_AW_MULTIPLY=	0x00040000,	/* d.a = clamp(d.a * p.a) */

	/* Intensity write modes */
	ZS_IW_ =	0x00f00000,
	ZS_IW_OFF =	0x00000000,	/* NOP */
	ZS_IW_COPY =	0x00100000,	/* d.i = p.i */
	ZS_IW_ADD =	0x00200000,	/* d.i = clamp(d.i + p.i) */
	ZS_IW_SUBTRACT=	0x00300000,	/* d.i = clamp(d.i - p.i) */
	ZS_IW_MULTIPLY=	0x00400000,	/* d.i = clamp(d.i * p.i) */
	ZS_IW_BLEND =	0x00500000,	/* d.i = p.i * p.a + d.i * (1 - p.a) */

	/* Z write modes */
	ZS_ZW_ = 	0x01000000,
	ZS_ZW_OFF = 	0x00000000,	/* NOP */
	ZS_ZW_ON = 	0x01000000	/* d.z = p.z */
} ZS_pipemode;


/* Max number of pipeline stages */
#define	ZS_MAX_STAGES	10


/*
 * Abstract pixel combinator object.
 * These are used to cache optimized pipelines constructed
 * from ZS_pipemode values + pipe parameters.
 */
typedef void (*ZS_StageCb)(ZS_Pipe *pipe, int current,
		ZS_Pixel *s, ZS_Pixel *d, int w);

struct ZS_Pipe
{
	ZS_State	*state;
	ZS_pipemode	mode;
	int		z;		/* Z value or offset */
	int		alpha;		/* Alpha value or scale */
	int		intensity;	/* Intensity value or scale */
	ZS_Pixel	*pipebuf;	/* Intermediate buffer */
	int		bufsize;	/* # of pixels in pipebuf */
	int		nstages;	/* # of stages used */
	ZS_StageCb	stages[ZS_MAX_STAGES];
};


ZS_Pipe *zs_NewPipe(ZS_State *state);
void zs_FreePipe(ZS_Pipe *pipe);

int zs_PipeZ(ZS_Pipe *pipe, ZS_pipemode zmode, float z);
int zs_PipeColor(ZS_Pipe *pipe, ZS_pipemode cmode);
int zs_PipeAlpha(ZS_Pipe *pipe, ZS_pipemode amode, float alpha);
int zs_PipeIntensity(ZS_Pipe *pipe, ZS_pipemode imode, float intensity);
int zs_PipeWrite(ZS_Pipe *pipe, ZS_pipemode wmode);


static inline void zs_PipeRun(ZS_Pipe *pipe, int stage,
		ZS_Pixel *s, ZS_Pixel *d, int w)
{
	/*
	 * Stage callbacks are responsible for calling down the chain.
	 * Z buffering stages may split spans to eliminate further
	 * processing of invisible pixels. The last stage should
	 * obviously not call anything!
	 */
	(pipe->stages[stage])(pipe, stage, s, d, w);
}

/*
 * Prepare pipe for work.
 */
int zs_PipePrepare(ZS_Pipe *pipe);

#endif /* ZS_PIPE_H */
