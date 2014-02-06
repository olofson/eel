/*
-----------------------------------------------------------------
	zs_pipe.c - ZeeSpace Pixel Processing Pipeline
-----------------------------------------------------------------
 * Copyright 2005, 2010-2011 David Olofson
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "zeespace.h"


/*------------------------------------------------------------------
	Math tools
------------------------------------------------------------------*/

static inline int addclamp(int a, int b, int max)
{
	int r = a + b;
	return r > max ? max : r;
}

static inline int subclamp(int a, int b, int min)
{
	int r = a - b;
	return r < min ? min : r;
}

static inline int addclamp2(int a, int b, int c, int min, int max)
{
	int r = a + b + c;
	if(r < min)
		return min;
	else if(r > max)
		return max;
	else
		return r;
}

static inline int subclamp2(int a, int b, int c, int min, int max)
{
	int r = a - b - c;
	if(r < min)
		return min;
	else if(r > max)
		return max;
	else
		return r;
}

static inline int fixmul(int a, int b, int bits)
{
	return (int64_t)a * b >> bits;
}


/*------------------------------------------------------------------
	Pipe implementation
------------------------------------------------------------------*/

/* Z transforms */

/*
 * IMPORTANT:
 *	These "first slot" stages all initialize 'pipebuf' from the state, as
 *	the buffer may be reallocated whenever a new surface is created! Without
 *	this hack, it would be necessary to call zs_PipePrepare() before every
 *	rendering operation, to be safe.
 */

static void stage_z_normal(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf = pipe->state->pipebuf;
	int x;
	if(pipe->z)
		for(x = 0; x < w; ++x)
			p[x].z = addclamp2(s[x].z, 0, pipe->z, 0, 65535);
	else
		for(x = 0; x < w; ++x)
			p[x].z = s[x].z;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_z_add(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf = pipe->state->pipebuf;
	int x;
	if(pipe->z)
		for(x = 0; x < w; ++x)
			p[x].z = addclamp2(d[x].z, s[x].z, pipe->z, 0, 65535);
	else
		for(x = 0; x < w; ++x)
			p[x].z = addclamp(d[x].z, s[x].z, 65535);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_z_subtract(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf = pipe->state->pipebuf;
	int x;
	if(pipe->z)
		for(x = 0; x < w; ++x)
			p[x].z = subclamp2(d[x].z, s[x].z, pipe->z, 0, 0);
	else
		for(x = 0; x < w; ++x)
			p[x].z = subclamp(d[x].z, s[x].z, 0);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_z_multiply(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf = pipe->state->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		int z = fixmul(d[x].z, s[x].z, 12);
		p[x].z = z > 65535 ? 65535 : z;
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}


/* Z-buffering modes */

static inline unsigned lerp8(unsigned a, unsigned b, unsigned x)
{
	return (a * (256 - x) + b * x) >> 8;
}

static void stage_zb_above(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x, sx;
	x = sx = 0;
	while(x < w)
	{
#if 1
		/* Find start of visible span */
		while((x < w) && (p[x].z < d[x].z))
			++x;
		sx = x;

		/* Find end of visible span */
		while((x < w) && (p[x].z >= d[x].z))
			++x;

		/* Render span */
		if(x != sx)
		{
			pipe->pipebuf = p + sx;
			zs_PipeRun(pipe, current + 1, s + sx, d + sx, x - sx);
		}
#else
/* TODO: This is at least better than nothing. How to do it correctly? */
		if(p[x].z > d[x].z + 128)
		{
			pipe->pipebuf = p + x;
			zs_PipeRun(pipe, current + 1, s + x, d + x, 1);
		}
		else if(p[x].z + 128 > d[x].z)
		{
			ZS_Pixel buf = d[x];
			int cov = p[x].z - d[x].z + 128;
			pipe->pipebuf = p + x;
			zs_PipeRun(pipe, current + 1, s + x, &buf, 1);
			d[x].r = lerp8(d[x].r, buf.r, cov);
			d[x].g = lerp8(d[x].g, buf.g, cov);
			d[x].b = lerp8(d[x].b, buf.b, cov);
			d[x].a = buf.a;
			d[x].i = lerp8(d[x].i, buf.i, cov);
			d[x].z = lerp8(d[x].z, buf.z, cov);
		}
		++x;
#endif
	}
	pipe->pipebuf = p;
}

static void stage_zb_below(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x, sx;
	x = sx = 0;
	while(x < w)
	{
		/* Find start of visible span */
		while((x < w) && (p[x].z > d[x].z))
			++x;
		sx = x;

		/* Find end of visible span */
		while((x < w) && (p[x].z <= d[x].z))
			++x;

		/* Render span */
		if(x != sx)
		{
			pipe->pipebuf = p + sx;
			zs_PipeRun(pipe, current + 1, s + sx, d + sx, x - sx);
		}
	}
	pipe->pipebuf = p;
}


/* Color transforms */

static void stage_c_normal(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		p[x].r = s[x].r;
		p[x].g = s[x].g;
		p[x].b = s[x].b;
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_c_add(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		p[x].r = addclamp(s[x].r, d[x].r, 255);
		p[x].g = addclamp(s[x].g, d[x].g, 255);
		p[x].b = addclamp(s[x].b, d[x].b, 255);
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_c_subtract(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		p[x].r = subclamp(s[x].r, d[x].r, 0);
		p[x].g = subclamp(s[x].g, d[x].g, 0);
		p[x].b = subclamp(s[x].b, d[x].b, 0);
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_c_multiply(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		p[x].r = fixmul(d[x].r, s[x].r, 8);
		p[x].g = fixmul(d[x].g, s[x].g, 8);
		p[x].b = fixmul(d[x].b, s[x].b, 8);
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

/* Alpha transforms */

static void stage_a_fixed(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].a = pipe->alpha;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_a_source(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].a = s[x].a * pipe->alpha >> 8;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_a_dest(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].a = d[x].a * pipe->alpha >> 8;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

	
/* Intensity transforms */

static void stage_i_fixed(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].i = pipe->intensity;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_i_source(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].i = s[x].i * pipe->intensity >> 8;
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_i_dest(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		p[x].i = d[x].i * pipe->intensity >> 8;
	zs_PipeRun(pipe, current + 1, s, d, w);
}


/* Color channel write mask */

static void stage_cw_(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	if(pipe->mode & ZS_CW_R)
		for(x = 0; x < w; ++x)
		{
			int v = (int)p[x].r * p[x].i * p[x].a >> 8;
			v += (int)d[x].r * (255 - p[x].a);
			if(v > 65535)
				v = 65535;
			d[x].r = v >> 8;
		}
	if(pipe->mode & ZS_CW_G)
		for(x = 0; x < w; ++x)
		{
			int v = (int)p[x].g * p[x].i * p[x].a >> 8;
			v += (int)d[x].g * (255 - p[x].a);
			if(v > 65535)
				v = 65535;
			d[x].g = v >> 8;
		}
	if(pipe->mode & ZS_CW_B)
		for(x = 0; x < w; ++x)
		{
			int v = (int)p[x].b * p[x].i * p[x].a >> 8;
			v += (int)d[x].b * (255 - p[x].a);
			if(v > 65535)
				v = 65535;
			d[x].b = v >> 8;
		}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_cw_rgb(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		int v = (int)p[x].r * p[x].i * p[x].a >> 8;
		v += (int)d[x].r * (255 - p[x].a);
		if(v > 65535)
			v = 65535;
		d[x].r = v >> 8;
		v = (int)p[x].g * p[x].i * p[x].a >> 8;
		v += (int)d[x].g * (255 - p[x].a);
		if(v > 65535)
			v = 65535;
		d[x].g = v >> 8;
		v = (int)p[x].b * p[x].i * p[x].a >> 8;
		v += (int)d[x].b * (255 - p[x].a);
		if(v > 65535)
			v = 65535;
		d[x].b = v >> 8;
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

	
/* Alpha write modes */
	
static void stage_aw_copy(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].a = p[x].a;
	zs_PipeRun(pipe, current + 1, s, d, w);
}
	
static void stage_aw_add(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].a = addclamp(p[x].a, d[x].a, 255);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_aw_subtract(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].a = subclamp(p[x].a, d[x].a, 0);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_aw_multiply(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].a = fixmul(d[x].a, p[x].a, 8);
	zs_PipeRun(pipe, current + 1, s, d, w);
}


/* Intensity write modes */

static void stage_iw_copy(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].i = p[x].i;
	zs_PipeRun(pipe, current + 1, s, d, w);
}
	
static void stage_iw_add(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].i = addclamp(p[x].i, d[x].i, 65535);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_iw_subtract(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].i = subclamp(p[x].i, d[x].i, 0);
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_iw_multiply(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		int i = fixmul(p[x].i, d[x].i, 8);
		d[x].i = i > 65535 ? 65535 : i;
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}

static void stage_iw_blend(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
	{
		int i = (int)p[x].i * p[x].a;
		i += (int)d[x].i * (255 - p[x].a);
		d[x].i = i >> 8;
	}
	zs_PipeRun(pipe, current + 1, s, d, w);
}


/* Z write modes */

static void stage_zw_off(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	/* NOP */
	/* End of pipe! */
}

static void stage_zw_on(ZS_Pipe *pipe, int current, ZS_Pixel *s, ZS_Pixel *d, int w)
{
	ZS_Pixel *p = pipe->pipebuf;
	int x;
	for(x = 0; x < w; ++x)
		d[x].z = p[x].z;
	/* End of pipe! */
}


int zs_PipePrepare(ZS_Pipe *pipe)
{
	pipe->nstages = 0;
/*	pipe->pipebuf = pipe->state->pipebuf;*/

	/* Z transforms */
	switch(pipe->mode & ZS_Z_)
	{
	  case ZS_Z_NORMAL:
		pipe->stages[pipe->nstages++] = stage_z_normal;
		break;
	  case ZS_Z_ADD:
		pipe->stages[pipe->nstages++] = stage_z_add;
		break;
	  case ZS_Z_SUBTRACT:
		pipe->stages[pipe->nstages++] = stage_z_subtract;
		break;
	  case ZS_Z_MULTIPLY:
		pipe->stages[pipe->nstages++] = stage_z_multiply;
		break;
	  default:
		return -1;
	}

	/* Z-buffering modes */
	switch(pipe->mode & ZS_ZB_)
	{
	  case ZS_ZB_ALL:
		/* Fall through directly to the color transform stage */
		break;
	  case ZS_ZB_ABOVE:
		pipe->stages[pipe->nstages++] = stage_zb_above;
		break;
	  case ZS_ZB_BELOW:
		pipe->stages[pipe->nstages++] = stage_zb_below;
		break;
	  default:
		return -1;
	}

	/* Color transforms */
	switch(pipe->mode & ZS_C_)
	{
	  case ZS_C_NORMAL:
		pipe->stages[pipe->nstages++] = stage_c_normal;
		break;
	  case ZS_C_ADD:
		pipe->stages[pipe->nstages++] = stage_c_add;
		break;
	  case ZS_C_SUBTRACT:
		pipe->stages[pipe->nstages++] = stage_c_subtract;
		break;
	  case ZS_C_MULTIPLY:
		pipe->stages[pipe->nstages++] = stage_c_multiply;
		break;
	  default:
		return -1;
	}

	/* Alpha transforms */
	switch(pipe->mode & ZS_A_)
	{
	  case ZS_A_FIXED:
		pipe->stages[pipe->nstages++] = stage_a_fixed;
		break;
	  case ZS_A_SOURCE:
		pipe->stages[pipe->nstages++] = stage_a_source;
		break;
	  case ZS_A_DEST:
		pipe->stages[pipe->nstages++] = stage_a_dest;
		break;
	  default:
		return -1;
	}

	/* Intensity transforms */
	switch(pipe->mode & ZS_I_)
	{
	  case ZS_I_FIXED:
		pipe->stages[pipe->nstages++] = stage_i_fixed;
		break;
	  case ZS_I_SOURCE:
		pipe->stages[pipe->nstages++] = stage_i_source;
		break;
	  case ZS_I_DEST:
		pipe->stages[pipe->nstages++] = stage_i_dest;
		break;
	  default:
		return -1;
	}

	/* Color channel write mask */
	switch(pipe->mode & ZS_CW_)
	{
	  case 0:
		break;
	  case ZS_CW_RGB:
		pipe->stages[pipe->nstages++] = stage_cw_rgb;
		break;
	  default:
		pipe->stages[pipe->nstages++] = stage_cw_;
		break;
	}

	/* Alpha write modes */
	switch(pipe->mode & ZS_AW_)
	{
	  case ZS_AW_OFF:
		break;
	  case ZS_AW_COPY:
		pipe->stages[pipe->nstages++] = stage_aw_copy;
		break;
	  case ZS_AW_ADD:
		pipe->stages[pipe->nstages++] = stage_aw_add;
		break;
	  case ZS_AW_SUBTRACT:
		pipe->stages[pipe->nstages++] = stage_aw_subtract;
		break;
	  case ZS_AW_MULTIPLY:
		pipe->stages[pipe->nstages++] = stage_aw_multiply;
		break;
	  default:
		return -1;
	}

	/* Intensity write modes */
	switch(pipe->mode & ZS_IW_)
	{
	  case ZS_IW_OFF:
		break;
	  case ZS_IW_COPY:
		pipe->stages[pipe->nstages++] = stage_iw_copy;
		break;
	  case ZS_IW_ADD:
		pipe->stages[pipe->nstages++] = stage_iw_add;
		break;
	  case ZS_IW_SUBTRACT:
		pipe->stages[pipe->nstages++] = stage_iw_subtract;
		break;
	  case ZS_IW_MULTIPLY:
		pipe->stages[pipe->nstages++] = stage_iw_multiply;
		break;
	  case ZS_IW_BLEND:
		pipe->stages[pipe->nstages++] = stage_iw_blend;
		break;
	  default:
		return -1;
	}

	/* Z write modes */
	switch(pipe->mode & ZS_ZW_)
	{
	  case ZS_ZW_OFF:
		pipe->stages[pipe->nstages++] = stage_zw_off;
		break;
	  case ZS_ZW_ON:
		pipe->stages[pipe->nstages++] = stage_zw_on;
		break;
	  default:
		return -1;
	}
	return 0;
}


/*------------------------------------------------------------------
	Pipe API
------------------------------------------------------------------*/

ZS_Pipe *zs_NewPipe(ZS_State *state)
{
	ZS_Pipe *p = (ZS_Pipe *)calloc(1, sizeof(ZS_Pipe));
	if(!p)
		return NULL;
	p->state = state;
	++state->refcount;
	/* Sensible defaults (?) */
	zs_PipeZ(p, ZS_Z_NORMAL | ZS_ZB_ABOVE, 0.0f);
	zs_PipeColor(p, ZS_C_NORMAL);
	zs_PipeAlpha(p, ZS_A_FIXED, 1.0f);
	zs_PipeIntensity(p, ZS_I_FIXED, 1.0f);
	zs_PipeWrite(p, ZS_CW_RGB | ZS_AW_OFF | ZS_IW_OFF | ZS_ZW_ON);
	return p;
}


int zs_PipeZ(ZS_Pipe *pipe, ZS_pipemode zmode, float z)
{
	pipe->mode &= ~(ZS_Z_ | ZS_ZB_);
	pipe->z = z * 256.0f;
	pipe->mode |= zmode & (ZS_Z_ | ZS_ZB_);
	return zs_PipePrepare(pipe);
}


int zs_PipeColor(ZS_Pipe *pipe, ZS_pipemode cmode)
{
	pipe->mode &= ~ZS_C_;
	pipe->mode |= cmode & ZS_C_;
	return zs_PipePrepare(pipe);
}


int zs_PipeAlpha(ZS_Pipe *pipe, ZS_pipemode amode, float alpha)
{
	pipe->mode &= ~ZS_A_;
	pipe->mode |= amode & ZS_A_;
	pipe->alpha = alpha * 255.0f;
	return zs_PipePrepare(pipe);
}


int zs_PipeIntensity(ZS_Pipe *pipe, ZS_pipemode imode, float intensity)
{
	pipe->mode &= ~ZS_I_;
	pipe->mode |= imode & ZS_I_;
	pipe->intensity = intensity * 256.0f;
	return zs_PipePrepare(pipe);
}


int zs_PipeWrite(ZS_Pipe *pipe, ZS_pipemode wmode)
{
	pipe->mode &= ~(ZS_CW_ |ZS_AW_ |ZS_IW_ | ZS_ZW_);
	pipe->mode |= wmode & (ZS_CW_ |ZS_AW_ |ZS_IW_ | ZS_ZW_);
	return zs_PipePrepare(pipe);
}


void zs_FreePipe(ZS_Pipe *pipe)
{
	zs_Close(pipe->state);
	free(pipe);
}
