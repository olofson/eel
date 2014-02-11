/*
-----------------------------------------------------------------
	ZeeSpace - 2D + Height Field Rendering Engine
-----------------------------------------------------------------
 * Copyright 2005-2006, 2010-2011 David Olofson
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
#include <string.h>
#include <math.h>
#include "zeespace.h"


/*---------------------------------------------------------------
	Pseudo-random number generator with cross-fade
---------------------------------------------------------------*/

void zs_RndReset(ZS_State *state, float balance)
{
	state->rnd1 = state->rnd2 = 16576;
	if(balance < 0.0f)
		balance = 0.0f;
	else if(balance > 1.0f)
		balance = 1.0f;
	state->rbal = balance * 16384;
}

/* Returns a 32 bit pseudo random number; generator 1 */
static inline int rnd1_get(ZS_State *state)
{
	state->rnd1 *= 1566083941UL;
	++state->rnd1;
	state->rnd1 &= 0xffffffffUL;    /* NOP on 32 bit machines */
	return state->rnd1 * (state->rnd1 >> 16);
}


/* Returns a 32 bit pseudo random number; generator 2 */
static inline int rnd2_get(ZS_State *state)
{
	state->rnd2 = 16807 * state->rnd2;
	state->rnd2 %= 4294967294UL;
	--state->rnd2;
	return state->rnd2;
}


/*
 * Grab a "random" value that is a cross-fade between the
 * N'th value from rnd1_get() and rnd2_get().
 */
static inline int rnd_get(ZS_State *state, unsigned range)
{
	unsigned d = 0xffffffff / range;
	unsigned r1 = rnd1_get(state) / d;
	unsigned r2 = rnd2_get(state) / d;
	return (r1 * state->rbal + r2 * (16383 - state->rbal)) >> 14;
}


int zs_RndGet(ZS_State *state, unsigned range)
{
	return rnd_get(state, range);
}


/*---------------------------------------------------------------
	Integer math
---------------------------------------------------------------*/

/* Round 'value' upwards by 'granularity' */
static inline int granup(int value, int granularity)
{
	return (value + granularity - 1) / granularity * granularity;
}

/* Clamp 'val' to the range of unsigned 8:8 fixed point */
static inline unsigned clamp88(int value)
{
	if(value < 0)
		return 0;
	else if(value > 65535)
		return 65535;
	else
		return value;
}


/* Square root for 24:8 fixed point */
static inline unsigned long sqrt248(unsigned long val)
{
	return (unsigned long)(sqrtf(val) + 128) >> 8;
}


/*---------------------------------------------------------------
	State management
---------------------------------------------------------------*/

/* Prepare internal buffers for a maximum span width of 'w' pixels */
static int zs_StatePrepare(ZS_State *st, int w)
{
	if(w <= st->bufw)
		return 0;
	free(st->scratch);
	free(st->pipebuf);
	st->scratch = (ZS_Pixel *)malloc(sizeof(ZS_Pixel) * w * 2);
	st->pipebuf = (ZS_Pixel *)malloc(sizeof(ZS_Pixel) * w);
	if(!st->scratch || !st->pipebuf)
		return -1;
	st->bufw = w;
	return 0;
}


ZS_State *zs_Open(void)
{
	ZS_State *st = (ZS_State *)calloc(1, sizeof(ZS_State));
	if(!st)
		return NULL;
	st->refcount = 1;
	if(zs_StatePrepare(st, 100) < 0)
	{
		free(st);
		return NULL;
	}
	return st;
}


void zs_Close(ZS_State *state)
{
	if(--state->refcount <= 0)
	{
		free(state->scratch);
		free(state->pipebuf);
		free(state);
	}
}


/*---------------------------------------------------------------
	Surface/Window/Region management
---------------------------------------------------------------*/

static inline int zs_Clip(ZS_Rect *r, int w, int h)
{
	int nw, nh;
	int x2 = r->x + r->w;
	int y2 = r->y + r->h;
	if(r->x < 0)
		r->x = 0;
	if(x2 > w)
		x2 = w;
	if(r->y < 0)
		r->y = 0;
	if(y2 > h)
		y2 = h;
	r->w = nw = x2 - r->x;
	if(r->w <= 0)
		return 0;
	r->h = nh = y2 - r->y;
	if(r->h <= 0)
		return 0;
	return 1;
}


ZS_Surface *zs_NewSurface(ZS_State *state, int w, int h)
{
	ZS_Surface *s = calloc(1, sizeof(ZS_Surface));
	if(!s)
		return NULL;
	if(zs_StatePrepare(state, w) < 0)
	{
		free(s);
		return NULL;
	}
	s->refcount = 1;
	s->state = state;
	s->pixels = calloc(w * h, sizeof(ZS_Pixel));
	if(!s->pixels)
	{
		free(s);
		return NULL;
	}
	s->w = s->pitch = w;
	s->h = h;
	s->flags = ZS_SF_OWNSPIXELS;
	++state->refcount;
	return s;
}


ZS_Surface *zs_Window(ZS_Surface *from, ZS_Rect *area)
{
	ZS_Surface *s = calloc(1, sizeof(ZS_Surface));
	if(!s)
		return NULL;
	s->state = from->state;
	s->parent = from;
	s->pitch = from->pitch;
	zs_Clip(area, from->w, from->h);
	s->w = area->w;
	s->h = area->h;
	s->pixels = zs_Pixel(from, area->x, area->y);
	s->flags = 0;
	++from->refcount;
	++s->state->refcount;
	return s;
}


ZS_Surface *zs_View(ZS_Surface *from, ZS_Rect *area)
{
	ZS_Surface *s = zs_Window(from, area);
	if(!s)
		return NULL;
	s->xo = area->x;
	s->yo = area->y;
	return s;
}


void zs_FreeSurface(ZS_Surface *s)
{
	if(--s->refcount > 0)
		return;
	if(s->flags & ZS_SF_OWNSPIXELS)
		free(s->pixels);
	else
		zs_FreeSurface(s->parent);
	zs_Close(s->state);
	free(s);
}


void zs_RawFill(ZS_Channels channels, ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p)
{
	ZS_Rect cr;
	int xmin, ymin, x, y, xmax, ymax;
	if(!r)
	{
		r = &cr;
		cr.x = cr.y = 0;
		cr.w = s->w;
		cr.h = s->h;
	}
	r->x -= s->xo;
	r->y -= s->yo;
	if(!zs_Clip(r, s->w, s->h))
		return;
	xmin = r->x;
	ymin = r->y;
	xmax = r->x + r->w;
	ymax = r->y + r->h;
	channels &= ZS_CH_ALL;
	for(y = ymin; y < ymax; ++y)
	{
		ZS_Pixel *lp = zs_Pixel(s, 0, y);
		switch((int)channels)
		{
		  case ZS_CH_ALL:
			for(x = xmin; x < xmax; ++x)
				lp[x] = *p;
			break;
		  case ZS_CH_RGB:
			for(x = xmin; x < xmax; ++x)
			{
				lp[x].r = p->r;
				lp[x].g = p->g;
				lp[x].b = p->b;
			}
			break;
		  case ZS_CH_RGBA:
			for(x = xmin; x < xmax; ++x)
			{
				lp[x].r = p->r;
				lp[x].g = p->g;
				lp[x].b = p->b;
				lp[x].a = p->a;
			}
			break;
		  case ZS_CH_RGB | ZS_CH_Z:
			for(x = xmin; x < xmax; ++x)
			{
				lp[x].r = p->r;
				lp[x].g = p->g;
				lp[x].b = p->b;
				lp[x].z = p->z;
			}
			break;
		  default:
			/* Slow(er) catch-all */
			if(channels & ZS_CH_R)
				for(x = xmin; x < xmax; ++x)
					lp[x].r = p->r;
			if(channels & ZS_CH_G)
				for(x = xmin; x < xmax; ++x)
					lp[x].g = p->g;
			if(channels & ZS_CH_B)
				for(x = xmin; x < xmax; ++x)
					lp[x].b = p->b;
			if(channels & ZS_CH_A)
				for(x = xmin; x < xmax; ++x)
					lp[x].a = p->a;
			if(channels & ZS_CH_I)
				for(x = xmin; x < xmax; ++x)
					lp[x].i = p->i;
			if(channels & ZS_CH_Z)
				for(x = xmin; x < xmax; ++x)
					lp[x].z = p->z;
			break;
		}
	}
}


void zs_RawCopy(ZS_Channels channels, ZS_Surface *src, ZS_Rect *srcr,
		ZS_Surface *dst, int dx, int dy)
{
	ZS_Rect cr;
	int x, y;

	/* Source rect */
	if(!srcr)
	{
		/* Use full surface! */
		srcr = &cr;
		cr.x = cr.y = 0;
		cr.w = src->w;
		cr.h = src->h;
	}
	srcr->x -= src->xo;
	srcr->y -= src->yo;
	if(srcr->x < 0)
		dx -= srcr->x;
	if(srcr->y < 0)
		dy -= srcr->y;
	zs_Clip(srcr, src->w, src->h);

	/* Destination rect */
	dx -= dst->xo;
	dy -= dst->yo;
	if(dx < 0)
	{
		srcr->x -= dx;
		srcr->w += dx;
		dx = 0;
	}
	if(dx + srcr->w > dst->w)
		srcr->w = dst->w - dx;
	if(dy < 0)
	{
		srcr->y -= dy;
		srcr->h += dy;
		dy = 0;
	}
	if(dy + srcr->h > dst->h)
		srcr->h = dst->h - dy;

	if((srcr->w <= 0) || (srcr->h <= 0))
		return;		/* Nothing left to blit! */

	channels &= ZS_CH_ALL;
	for(y = 0; y < srcr->h; ++y)
	{
		ZS_Pixel *sp = zs_Pixel(src, srcr->x, srcr->y + y);
		ZS_Pixel *dp = zs_Pixel(dst, dx, dy + y);
		switch(channels)
		{
		  case ZS_CH_ALL:
		  {
			int bytes = sizeof(ZS_Pixel) * srcr->w;
			memcpy(dp, sp, bytes);
			break;
		  }
		  default:
			/* Slow(er) catch-all */
			if(channels & ZS_CH_R)
				for(x = 0; x < srcr->w; ++x)
					dp[x].r = sp[x].r;
			if(channels & ZS_CH_G)
				for(x = 0; x < srcr->w; ++x)
					dp[x].g = sp[x].g;
			if(channels & ZS_CH_B)
				for(x = 0; x < srcr->w; ++x)
					dp[x].b = sp[x].b;
			if(channels & ZS_CH_A)
				for(x = 0; x < srcr->w; ++x)
					dp[x].a = sp[x].a;
			if(channels & ZS_CH_I)
				for(x = 0; x < srcr->w; ++x)
					dp[x].i = sp[x].i;
			if(channels & ZS_CH_Z)
				for(x = 0; x < srcr->w; ++x)
					dp[x].z = sp[x].z;
			break;
		}
	}
}


void zs_WaterZ(ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p, int factor)
{
	ZS_Rect cr;
	int xmin, ymin, x, y, xmax, ymax;
	if(!r)
	{
		r = &cr;
		cr.x = cr.y = 0;
		cr.w = s->w;
		cr.h = s->h;
	}
	else if(!zs_Clip(r, s->w, s->h))
		return;
	xmin = r->x;
	ymin = r->y;
	xmax = r->x + r->w;
	ymax = r->y + r->h;
	for(y = ymin; y < ymax; ++y)
	{
		ZS_Pixel *lp = zs_Pixel(s, 0, y);
		for(x = xmin; x < xmax; ++x)
			if(p->z >= lp[x].z)
				lp[x].z += (p->z - lp[x].z) * factor >> 8;
	}
}


void zs_Fog(ZS_Surface *s, ZS_Rect *r, ZS_Pixel *p, int visibility)
{
	ZS_Rect cr;
	int xmin, ymin, x, y, xmax, ymax;
	int blur = visibility > 0 ? 65536 / visibility : 65536;
	if(!r)
	{
		r = &cr;
		cr.x = cr.y = 0;
		cr.w = s->w;
		cr.h = s->h;
	}
	else if(!zs_Clip(r, s->w, s->h))
		return;
	xmin = r->x;
	ymin = r->y;
	xmax = r->x + r->w;
	ymax = r->y + r->h;
	for(y = ymin; y < ymax; ++y)
	{
		ZS_Pixel *lp = zs_Pixel(s, 0, y);
		for(x = xmin; x < xmax; ++x)
			if(p->z >= lp[x].z)
			{
				int b1, b2;
				b1 = p->a + ((int)(p->z - lp[x].z) * blur >> 16);
				if(b1 > 256)
					b1 = 256;
				b2 = 256 - b1;
				lp[x].r = (p->r * b1 + lp[x].r * b2) >> 8;
				lp[x].g = (p->g * b1 + lp[x].g * b2) >> 8;
				lp[x].b = (p->b * b1 + lp[x].b * b2) >> 8;
				lp[x].i = (p->i * b1 + lp[x].i * b2) >> 8;
			}
	}
}


void zs_Block(ZS_Pipe *pipe, ZS_Surface *s,
		float x, float y, float w, float h, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	int ix, iy, sw;
	int ymin = y;
	int ymax = y + h;
	int xmin = x;
	int xmax = x + w;
	if(xmin < 0)
		xmin = 0;
	if(xmax > s->w)
		xmax = s->w;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	sw = xmax - xmin;
	for(ix = 0; ix < sw; ++ix)
		scratch[ix] = *px;
	for(iy = ymin; iy < ymax; ++iy)
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xmin, iy), sw);
}


void zs_Blit(ZS_Pipe *pipe, ZS_Surface *src, ZS_Rect *srcr,
		ZS_Surface *dst, float x, float y)
{
	int iy;
	int xmin = x - src->xo;
	int ymin = y - src->yo;
	int xmax, ymax;
	ZS_Pixel *scratch = src->state->scratch;
	ZS_Rect sr;
	
	/* Obtain and clip source rect. Note that this may nudge xmin/ymin! */
	if(srcr)
		sr = *srcr;
	else
	{
		sr.x = sr.y = 0;
		sr.w = src->w;
		sr.h = src->h;
	}
/*
printf("zs_Blit(%p, %p, [%d, %d, %d, %d], %p, %f, %f)\n", pipe, src,
sr.x, sr.y, sr.w, sr.h,		dst,	x, y);
printf("  xmin: %d\tymin: %d\n", xmin, ymin);
printf("  dest: %dx%d\n", dst->w, dst->h);
*/
	sr.x -= src->xo;
	sr.y -= src->yo;
	if(srcr->x < 0)
		xmin -= srcr->x;
	if(srcr->y < 0)
		ymin -= srcr->y;
	zs_Clip(&sr, src->w, src->h);

	/* Calculate and clip destination rect. */
	ymax = ymin + sr.h;
	xmax = xmin + sr.w;
	if(xmin < 0)
	{
		sr.x -= xmin;
		sr.w += xmin;
		xmin = 0;
	}
	if(xmax > dst->w)
		xmax = dst->w;
	if(ymin < 0)
	{
		sr.y -= ymin;
		sr.h += ymin;
		ymin = 0;
	}
	if(ymax > dst->h)
		ymax = dst->h;

	if((sr.w <= 0) || (sr.h <= 0) || (xmax <= xmin))
		return;		/* Nothing left to blit! */

	for(iy = ymin; iy < ymax; ++iy)
	{
		int sw = xmax - xmin;
		ZS_Pixel *spx = zs_Pixel(src, sr.x, sr.y + iy - ymin);
//printf("  %d (%d - %d)\n", sw, xmax, xmin);
		memcpy(scratch, spx, sizeof(ZS_Pixel) * sw);
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(dst, xmin, iy), sw);
	}
}


void zs_Cylinder(ZS_Pipe *pipe, ZS_Surface *s,
		float cx, float cy, float r, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	int x, y;
	int ymin = cy - r;
	int ymax = cy + r;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	for(y = ymin; y < ymax; ++y)
	{
		float dydy = (y - cy)*(y - cy);
		float hw = sqrtf(r*r - dydy);
		int xmin = cx - hw;
		int xmax = cx + hw;
		int w;
		if(xmin < 0)
			xmin = 0;
		if(xmax > s->w)
			xmax = s->w;
		w = xmax - xmin;
		for(x = 0; x < w; ++x)
			scratch[x] = *px;
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xmin, y), w);
	}
}


void zs_Cone(ZS_Pipe *pipe, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	float th = px->z + h;
	int x, y;
	float fx, fy;
	int ymin = cy - r;
	int ymax = cy + r;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	for(fy = y = ymin; y < ymax; ++y, ++fy)
	{
		float dydy = (fy - cy)*(fy - cy);
		float hw = sqrtf(r*r - dydy);
		int xmin = cx - hw;
		int xmax = cx + hw;
		int w;
		if(xmin < 0)
			xmin = 0;
		if(xmax > s->w)
			xmax = s->w;
		w = xmax - xmin;
		for(x = 0, fx = xmin; x < w; ++x, ++fx)
		{
			float dx = fx - cx;
			float d = sqrtf(dx*dx + dydy);
			int z = th - (h * d) / r;
			scratch[x] = *px;
			scratch[x].z = clamp88(z);
		}
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xmin, y), w);
	}
}


void zs_Beehive(ZS_Pipe *pipe, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	int x, y;
	float fx, fy;
	int ymin = cy - r;
	int ymax = cy + r;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	for(fy = y = ymin; y < ymax; ++y, ++fy)
	{
		float dydy = (fy - cy)*(fy - cy);
		float hw = sqrtf(r*r - dydy);
		int xmin = cx - hw;
		int xmax = cx + hw;
		int w;
		if(xmin < 0)
			xmin = 0;
		if(xmax > s->w)
			xmax = s->w;
		w = xmax - xmin;
		for(x = 0, fx = xmin; x < w; ++x, ++fx)
		{
			float dx = fx - cx;
			float d = sqrtf(dx*dx + dydy);
			int z = ((r - d * d / r) * h / r) * 256.0 + px->z;
			scratch[x] = *px;
			scratch[x].z = clamp88(z);
		}
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xmin, y), w);
	}
}


void zs_Dome(ZS_Pipe *pipe, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	int x, y;
	float fx, fy;
	float rr = r*r;
	int ymin = cy - r;
	int ymax = cy + r;
	float hf = h * 256.0 / r;
	float hz = px->z;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	for(fy = y = ymin; y < ymax; ++y, ++fy)
	{
		float dydy = (fy - cy)*(fy - cy);
		float hw = sqrtf(rr - dydy);
		int xmin = cx - hw;
		int xmax = cx + hw;
		int w;
		if(xmin < 0)
			xmin = 0;
		if(xmax > s->w)
			xmax = s->w;
		w = xmax - xmin;
		for(x = 0, fx = xmin; x < w; ++x, ++fx)
		{
			float dx = fx - cx;
			float d = sqrtf(dx*dx + dydy);
			int z = sqrtf(rr - d*d) * hf + hz;
			scratch[x] = *px;
			scratch[x].z = clamp88(z);
		}
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xmin, y), w);
	}
}


/*FIXME:*/
void zs_DomeRect(ZS_Pipe *pipe, ZS_Surface *s,
		float cx, float cy, float r, float h, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	int x, y;
	float fx, fy;
	float rr = r*r;
	int ymin = cy - r;
	int ymax = cy + r;
	float hf = h * 256.0 / r;
	float hz = px->z;
	if(ymin < 0)
		ymin = 0;
	if(ymax > s->h)
		ymax = s->h;
	for(fy = y = ymin; y < ymax; ++y, ++fy)
	{
		float dydy = (fy - cy)*(fy - cy);
		float hw = sqrtf(rr - dydy);
		int xmin = cx - hw;
		int xmax = cx + hw;
		int xxmin = cx - r;
		int xxmax = cx + r;
		if(xmin < 0)
			xmin = 0;
		if(xmax > s->w)
			xmax = s->w;
		if(xxmin < 0)
			xxmin = 0;
		if(xxmax > s->w)
			xxmax = s->w;
		for(x = xxmin; x < xmin; ++x)
			scratch[x] = *px;
		for(fx = xmin; x < xmax; ++x, ++fx)
		{
			float dx = fx - cx;
			float d = sqrtf(dx*dx + dydy);
			int z = sqrtf(rr - d*d) * hf + hz;
			scratch[x] = *px;
			scratch[x].z = clamp88(z);
		}
		for( ; x < xxmax; ++x)
			scratch[x] = *px;
		zs_PipeRun(pipe, 0, scratch, zs_Pixel(s, xxmin, y), xxmax - xxmin);
	}
}
/*FIXME:*/


/*---------------------------------------------------------------
	ZeeSpace toolkit - SDL rendering
---------------------------------------------------------------*/
void zs_Blit2SDL(ZS_Surface *from, SDL_Surface *to, int tx, int ty, int blend)
{
	int x, y, xmin, ymin, xmax, ymax;
	int bpp = to->format->BytesPerPixel;
	int ro, go, bo, ao = 0;
	if(to->format->Amask)
	{
#ifdef SDL_LITTLE_ENDIAN
		ro = 4 - to->format->Rshift / 8;
		go = 4 - to->format->Gshift / 8;
		bo = 4 - to->format->Bshift / 8;
		ao = 4 - to->format->Ashift / 8;
#else
		ro = to->format->Rshift / 8;
		go = to->format->Gshift / 8;
		bo = to->format->Bshift / 8;
		ao = to->format->Ashift / 8;
#endif
	}
	else
	{
#ifdef SDL_LITTLE_ENDIAN
		ro = 3 - to->format->Rshift / 8;
		go = 3 - to->format->Gshift / 8;
		bo = 3 - to->format->Bshift / 8;
#else
		ro = to->format->Rshift / 8;
		go = to->format->Gshift / 8;
		bo = to->format->Bshift / 8;
#endif
	}
	xmin = to->clip_rect.x;
	xmax = xmin + to->clip_rect.w;
	if(xmax > from->w)
		xmax = from->w;
	ymin = to->clip_rect.y;
	ymax = ymin + to->clip_rect.h;
	if(ymax > from->h)
		ymax = from->h;
	SDL_LockSurface(to);
	for(y = ymin; y < ymax; ++y)
	{
		ZS_Pixel *p = zs_Pixel(from, xmin, y);
		unsigned char *d = ((unsigned char *)to->pixels) +
				y * to->pitch + xmin * bpp;
		if(to->format->Amask)
			for(x = xmin; x < xmax; ++x)
			{
				d[ro] = p->r;
				d[go] = p->g;
				d[bo] = p->b;
				d[ao] = p->a;
				d += bpp;
				++p;
			}
		else if(blend)
			for(x = xmin; x < xmax; ++x)
			{
				int a1 = p->a;
				int a0 = 256 - a1;
				d[ro] = (d[ro] * a0 + p->r * a1) >> 8;
				d[go] = (d[go] * a0 + p->g * a1) >> 8;
				d[bo] = (d[bo] * a0 + p->b * a1) >> 8;
				d += bpp;
				++p;
			}
		else
			for(x = xmin; x < xmax; ++x)
			{
				d[ro] = p->r;
				d[go] = p->g;
				d[bo] = p->b;
				d += bpp;
				++p;
			}
	}
	SDL_UnlockSurface(to);
}


static inline void vline(SDL_Surface *s, ZS_Pixel *p, int x, int y, int y2,
		int ro, int go, int bo)
{
	unsigned char *d;
	int bpp = s->format->BytesPerPixel;
	int bal;
	int ymin = s->clip_rect.y;
	int ymax = ymin + s->clip_rect.h;
	int y2clipped;
	int r = p->r;
	int g = p->g;
	int b = p->b;
	y >>= 8;
	if(y < ymin)
		y = ymin;
	bal = y2 & 0xff;
	y2 >>= 8;
	d = ((unsigned char *)s->pixels) + y * s->pitch + x * bpp;
	if(y2 > ymax)
		y2clipped = ymax;
	else
		y2clipped = y2;
	for( ; y < y2clipped; ++y)
	{
		d[ro] = r;
		d[go] = g;
		d[bo] = b;
		d += s->pitch;
	}
	if((y < ymax) && (y <= y2))
	{
		d[ro] = (r * bal + d[ro] * (255 - bal)) >> 8;
		d[go] = (g * bal + d[go] * (255 - bal)) >> 8;
		d[bo] = (b * bal + d[bo] * (255 - bal)) >> 8;
	}
}

void zs_3D2SDL(ZS_Surface *from, SDL_Surface *to, float pitch)
{
	ZS_Pixel sky;
	ZS_Pixel *p;
	float a = pitch * 2 * M_PI / 360;
	unsigned tilt, scale, ty;
#ifdef SDL_LITTLE_ENDIAN
	int ro = 3 - to->format->Rshift / 8;
	int go = 3 - to->format->Gshift / 8;
	int bo = 3 - to->format->Bshift / 8;
#else
	int ro = to->format->Rshift / 8;
	int go = to->format->Gshift / 8;
	int bo = to->format->Bshift / 8;
#endif
	int *buf = (int *)from->state->scratch;
	int x, y, xmin, xmax, ymaxf;
	xmin = to->clip_rect.x;
	xmax = xmin + to->clip_rect.w;
	if(xmax > from->w)
		xmax = from->w;
	xmax -= 1;
	y = from->h - 1;
	ymaxf = y << 8;
	for(x = 0; x <= xmax; ++x)
		buf[x] = ymaxf + 256;
	tilt = sin(a) * 65536;
	scale = cos(a) * 65536;
	ty = 0;
	SDL_LockSurface(to);
	for( ; y >= 0; --y)
	{
		p = zs_Pixel(from, 0, y);
		for(x = xmin; x <= xmax; ++x)
		{
			int sz = p[x].z * scale >> 8;
			int tz = ymaxf - ((ty + sz) >> 8);
			if(tz < buf[x])
			{
				if(tz < 0)
					tz = 0;
				vline(to, &p[x], x, tz, buf[x], ro, go, bo);
				buf[x] = tz;
			}
		}
		ty += tilt;
	}
	sky.r = 64;
	sky.g = 64;
	sky.b = 96;
	sky.a = 255;
	for(x = xmin; x <= xmax; ++x)
		vline(to, &sky, x, 0, buf[x], ro, go, bo);
	SDL_UnlockSurface(to);
}


void zs_Graph2SDL(ZS_Surface *from, SDL_Surface *to, int scale)
{
	int x, y, xmax, ymax;
	int bpp = to->format->BytesPerPixel;
#ifdef SDL_LITTLE_ENDIAN
	int ro = 3 - to->format->Rshift / 8;
	int go = 3 - to->format->Gshift / 8;
	int bo = 3 - to->format->Bshift / 8;
#else
	int ro = to->format->Rshift / 8;
	int go = to->format->Gshift / 8;
	int bo = to->format->Bshift / 8;
#endif
	xmax = to->w;
	ymax = to->h;
	SDL_LockSurface(to);
	for(y = 0; y < ymax; ++y)
	{
		ZS_Pixel *p = zs_Pixel(from, 0, y / scale * scale);
		unsigned char *d = ((unsigned char *)to->pixels) + y * to->pitch;
		for(x = 0; x < xmax; ++x)
		{
			unsigned g, fy, sfy;
			fy = (65535 - p->z) * scale >> 8;
			sfy = fy >> 8;
			if(y % scale > sfy)
				g = 255;
			else if(y % scale < sfy)
				g = 0;
			else
				g = ((y % scale << 8) - fy) & 0xff;
			d[ro] = y % scale == scale - 1 ? 255 : 0;
			d[go] = g;
			d[bo] = y % scale == scale / 2 ? 255 : 0;
			d += bpp;
			++p;
		}
	}
	SDL_UnlockSurface(to);
}


void zs_Z2SDL(ZS_Surface *from, SDL_Surface *to, int scale)
{
	int x, y, xmax, ymax;
	int bpp = to->format->BytesPerPixel;
#ifdef SDL_LITTLE_ENDIAN
	int ro = 3 - to->format->Rshift / 8;
	int go = 3 - to->format->Gshift / 8;
	int bo = 3 - to->format->Bshift / 8;
#else
	int ro = to->format->Rshift / 8;
	int go = to->format->Gshift / 8;
	int bo = to->format->Bshift / 8;
#endif
	xmax = to->w;
	ymax = to->h;	
	SDL_LockSurface(to);
	for(y = 0; y < ymax; ++y)
	{
		ZS_Pixel *p = zs_Pixel(from, 0, y);
		unsigned char *d = ((unsigned char *)to->pixels) + y * to->pitch;
		for(x = 0; x < xmax; ++x, d += bpp, ++p)
		{
			int c;
			int rawc = p->z;
			rawc *= scale;
			rawc >>= 6;
			if(rawc > 1023)
			{
				d[ro] = 255;
				d[go] = 0;
				d[bo] = 255;
				continue;
			}
			c = rawc & 0xff;
			switch(rawc >> 8)
			{
			  case 0:
				d[ro] = 0;
				d[go] = 0;
				d[bo] = c;
				break;
			  case 1:
				d[ro] = 0;
				d[go] = c;
				d[bo] = 255 - c;
				break;
			  case 2:
				d[ro] = c;
				d[go] = 255;
				d[bo] = 0;
				break;
			  case 3:
				d[ro] = 255;
				d[go] = 255;
				d[bo] = c;
				break;
			}
		}
	}
	SDL_UnlockSurface(to);
}


void zs_I2SDL(ZS_Surface *from, SDL_Surface *to)
{
	int x, y, xmax, ymax;
	int bpp = to->format->BytesPerPixel;
#ifdef SDL_LITTLE_ENDIAN
	int ro = 3 - to->format->Rshift / 8;
	int go = 3 - to->format->Gshift / 8;
	int bo = 3 - to->format->Bshift / 8;
#else
	int ro = to->format->Rshift / 8;
	int go = to->format->Gshift / 8;
	int bo = to->format->Bshift / 8;
#endif
	xmax = to->w;
	ymax = to->h;	
	SDL_LockSurface(to);
	for(y = 0; y < ymax; ++y)
	{
		ZS_Pixel *p = zs_Pixel(from, 0, y);
		unsigned char *d = ((unsigned char *)to->pixels) + y * to->pitch;
		for(x = 0; x < xmax; ++x, d += bpp, ++p)
		{
			int i = p->i;
			if(i <= 255)
			{
				d[ro] = 0;
				d[go] = i;
				d[bo] = 0;
			}
			else if(i <= 511)
			{
				d[ro] = i;
				d[go] = 255;
				d[bo] = 0;
			}
			else
				d[ro] = d[go] = d[bo] = 255;
		}
	}
	SDL_UnlockSurface(to);
}


/*---------------------------------------------------------------
	Shadow casting
---------------------------------------------------------------*/

/* Do one diagonal ray */
static inline void do_ray(ZS_Surface *s, int startx, int starty, int zstep,
		int depth, int hardness)
{
	ZS_Pixel *p = zs_Pixel(s, startx, starty);
	int step = s->pitch + 1;
	int z = p->z;
	int i = s->w - startx;
	int ymax = s->h - starty;
	int dist = 0;
 	if(i > ymax)
		i = ymax;
	while(i--)
	{
		int pz = p->z;
		if(z <= pz)
		{
			z = pz;
			dist = 0;
		}
		else
		{
			unsigned sh = (z - pz) * hardness >> 8;
			if(sh > 256)
				sh = 256;
			sh = sh * depth >> 8;
			if(sh > p->i)
				p->i = 0;
			else
				p->i -= sh;
			++dist;
		}
		z += zstep;
		p += step;
	}
}

void zs_SimpleShadow(ZS_Surface *s, float altitude, float scale,
		float depth, float hardness)
{
	int xmin, ymin, x, y, xmax, ymax, zstep;
	int hard = 256 * hardness;
	int d = 256 * depth;
	if(altitude >= 90)
		return;
	zstep = -tan(altitude * 2 * M_PI / 360) * 256 * scale;
	xmin = 0;
	ymin = 0;
	xmax = s->w;
	ymax = s->h;
	for(y = ymin; y < ymax; ++y)
		do_ray(s, xmin, y, zstep, d, hard);
	for(x = ymin + 1; x < xmax; ++x)
		do_ray(s, x, ymin, zstep, d, hard);
	/* 2x2 filter */
	for(y = ymin + 1; y < ymax; ++y)
	{
		ZS_Pixel *pm1 = zs_Pixel(s, xmin, y - 1);
		ZS_Pixel *p = zs_Pixel(s, xmin, y);
		for(x = xmin + 1; x < xmax; ++x)
		{
			int i = p[x].i;
			i += pm1[x - 1].i;
			i += p[x - 1].i;
			i += pm1[x].i;
			i >>= 2;
			pm1[x - 1].i = i;
		}
	}
}


/*---------------------------------------------------------------
	Bump Mapping
---------------------------------------------------------------*/
void zs_BumpMap(ZS_Surface *s, float altitude, float scale, float depth)
{
	int x, y, xmax, ymax, abias;
	float scalei = 1.0f / scale;
//int scalei = 256 / scale;
	int d = 256 * depth;
	if(altitude < 1)
		altitude = 1;
	abias = tan((90 - altitude) * 2 * M_PI / 360) * 256 * scale;
	xmax = s->w - 2;
	ymax = s->h - 2;
	for(y = 0; y <= ymax; ++y)
	{
		ZS_Pixel *p = zs_Pixel(s, 0, y);
		int pitch = s->pitch;
		int z00 = p[0].z;
		int z10 = p[pitch].z;
		for(x = 0; x <= xmax; ++x)
		{
			float face, face2;
//			int face, face2;
			int iface;
			int z01 = p[x + 1].z;
			int z11 = p[x + pitch + 1].z;
#if 1
			face2 = fabs(z01 - z10) * scalei;
			face = fabs(z11 - z00 - abias) * scalei;
			iface = (int)(sqrtf(face*face + face2*face2) * .5f);
#else
			face2 = labs(z01 - z10) * scalei;
			face = labs(z11 - z00 - abias) * scalei;
			iface = (face + face2) >> 10;
#endif
			if(iface > 384)
				iface = 384;
			iface = (128 - iface) * d >> 8;
			iface = p[x].i * iface >> 8;
			iface = p[x].i + iface;
			if(iface < 0)
				p[x].i = 0;
			else if(iface > 65535)
				p[x].i = 65535;
			else
				p[x].i = iface;
			z00 = z01;
			z10 = z11;
		}
	}
}


/*---------------------------------------------------------------
	Inter-channel operations
---------------------------------------------------------------*/

void zs_ApplyIntensity(ZS_Surface *s, float rm, float gm, float bm)
{
/*TODO: Implement rm, gm, bm! */
	int x, y;
	for(y = 0; y < s->h; ++y)
	{
		ZS_Pixel *p = zs_Pixel(s, 0, y);
		for(x = 0; x < s->w; ++x)
		{
			int r = p[x].r * p[x].i >> 8;
			int g = p[x].g * p[x].i >> 8;
			int b = p[x].b * p[x].i >> 8;
			if(r > 255)
				r = 255;
			if(g > 255)
				g = 255;
			if(b > 255)
				b = 255;
			p[x].r = r;
			p[x].g = g;
			p[x].b = b;
		}
	}
}


/*---------------------------------------------------------------
	Perlin Noise Terrain Generator
---------------------------------------------------------------*/

//FIXME: These should be provided by the application, if desired!!!
#define	RND_SEEDS	8
static const unsigned rnd_seedtab[RND_SEEDS] = {
	1376513329UL,
	1382351539UL,
	1382351563UL,
	1391254213UL,
	1402751657UL,
	1402751687UL,
	1412302379UL,
	1422953557UL
};

/*
TODO:
	Configuration object!
		For each octave:
			* Amplitude
			* Random seed
			* Coordinate masks for rnd_2d()
			* Interpolation style
		Final output:
			* Output transform (linear, cube, S, ...)
			* Output operation (replace, add, sub, mul, ...)

		Use pipelines for output...? We could map Z or other values
		to RGB for interesting effects.

		Terrain and other Z generators as part of the pipeline, rather
		than being primitives...? Dome, beehive and similar primitives
		would be split into an <x,y,u,v> generator primitive part and a
		Z generator pipeline stage.
		  Or, we just treat shapes separately, and handle z like any
		other channel. Then we add tools for constructing and
		manipulating complex regions, and tools for "painting" through
		regions. Basic rendering would be done by tools like zs_Fill()
		(fill region with specified color) and zs_Texture() (fill region
		with transformed texture from ZeeSpace surface), and Dome,
		Beehive, PerlinTerrain etc would be in that class of tools,
		generating "texture" data on the fly, rather than reading from a
		pre-rendered surface.
*/

static inline unsigned rnd_2d(unsigned x, unsigned y, unsigned seed)
{
	x &= 0xffff;
	y &= 0xffff;
	x ^= x * 76607;
	y ^= y * 77347;
	return (x ^ y ^ 0xad4ac945) * seed;
}


/* 2D noise without interpolation (squares) */
static inline int noise_2d(unsigned x, unsigned y, int amp, unsigned seed)
{
	unsigned ix = x >> 16;
	unsigned iy = y >> 16;
	unsigned n = rnd_2d(ix, iy, seed) >> 16;
	return (n * (amp >> 2) >> 14) - amp / 2;
}


/* 2D noise with bilinear interpolation */
static inline int noise_2d_li(unsigned x, unsigned y, int amp, unsigned seed)
{
	unsigned fx = x & 0xffff;
	unsigned fy = y & 0xffff;
	unsigned ix = x >> 16;
	unsigned iy = y >> 16;
	unsigned n00 = rnd_2d(ix, iy, seed) >> 16;
	unsigned n10 = rnd_2d(ix + 1, iy, seed) >> 16;
	unsigned n01 = rnd_2d(ix, iy + 1, seed) >> 16;
	unsigned n11 = rnd_2d(ix + 1, iy + 1, seed) >> 16;
	n00 = (n10 * fx + n00 * (65536 - fx)) >> 16;
	n01 = (n11 * fx + n01 * (65536 - fx)) >> 16;
	n00 = (n01 * fy + n00 * (65536 - fy)) >> 16;
	return (n00 * (amp >> 2) >> 14) - amp / 2;
}


/* Straigt cubic interpolation (Hermite spline) */
static inline int cubint(int nm1, int n0, int n1, int n2, int frac)
{
	int a = (3 * (n0 - n1) - nm1 + n2) >> 1;
	int b = (n1 << 1) + nm1 - ((5 * n0 + n2) >> 1);
	int c = (n1 - nm1) >> 1;
	frac >>= 3;
	a = (a * frac) >> 13;
	a = (a + b) * frac >> 13;
	return n0 + ((a + c) * frac >> 13);
}


/* Two-stage Hermite; coefficient calculation */
static inline void cubint2c(int *n, int *c)
{
	int a = (3 * (n[1] - n[2]) - n[0] + n[3]) >> 1;
	int b = (n[2] << 1) + n[0] - ((5 * n[1] + n[3]) >> 1);
	c[3] = (n[2] - n[0]) >> 1;
	c[0] = a;
	c[1] = n[1];
	c[2] = b;
}

/* Two-stage Hermite; interpolation from coefficients */
static inline int cubintc(int *c, int frac)
{
	int a;
	frac >>= 3;
	a = (c[0] * frac) >> 13;
	a = (a + c[2]) * frac >> 13;
	return c[1] + ((a + c[3]) * frac >> 13);
}


#if 0
/* 2D noise with bicubic interpolation */
static inline int noise_2d_ci(unsigned x, unsigned y, int amp, unsigned seed)
{
	unsigned fx = x & 0xffff;
	unsigned fy = y & 0xffff;
	unsigned ix = x >> 16;
	unsigned iy = y >> 16;

	int n00 = rnd_2d(ix - 1, iy - 1, seed) >> 16;
	int n10 = rnd_2d(ix, iy - 1, seed) >> 16;
	int n20 = rnd_2d(ix + 1, iy - 1, seed) >> 16;
	int n30 = rnd_2d(ix + 2, iy - 1, seed) >> 16;

	int n01 = rnd_2d(ix - 1, iy, seed) >> 16;
	int n11 = rnd_2d(ix, iy, seed) >> 16;
	int n21 = rnd_2d(ix + 1, iy, seed) >> 16;
	int n31 = rnd_2d(ix + 2, iy, seed) >> 16;

	int n02 = rnd_2d(ix - 1, iy + 1, seed) >> 16;
	int n12 = rnd_2d(ix, iy + 1, seed) >> 16;
	int n22 = rnd_2d(ix + 1, iy + 1, seed) >> 16;
	int n32 = rnd_2d(ix + 2, iy + 1, seed) >> 16;

	int n03 = rnd_2d(ix - 1, iy + 2, seed) >> 16;
	int n13 = rnd_2d(ix, iy + 2, seed) >> 16;
	int n23 = rnd_2d(ix + 1, iy + 2, seed) >> 16;
	int n33 = rnd_2d(ix + 2, iy + 2, seed) >> 16;

	n10 = cubint(n00, n10, n20, n30, fx);
	n11 = cubint(n01, n11, n21, n31, fx);
	n12 = cubint(n02, n12, n22, n32, fx);
	n13 = cubint(n03, n13, n23, n33, fx);
	n11 = cubint(n10, n11, n12, n13, fy);

	return (n11 * (amp >> 2) >> 14) - amp / 2;
}
#endif

typedef struct ZS_PerlinCache
{
	int		n[4][4];
	unsigned	ix, iy;
	unsigned	seed;
} ZS_PerlinCache;

static void noise_2d_update_cache(ZS_PerlinCache *nc)
{
	unsigned ix = nc->ix;
	unsigned iy = nc->iy;
	unsigned seed = nc->seed;

	nc->n[0][0] = rnd_2d(ix - 1, iy - 1, seed) >> 16;
	nc->n[0][1] = rnd_2d(ix, iy - 1, seed) >> 16;
	nc->n[0][2] = rnd_2d(ix + 1, iy - 1, seed) >> 16;
	nc->n[0][3] = rnd_2d(ix + 2, iy - 1, seed) >> 16;
	cubint2c(nc->n[0], nc->n[0]);

	nc->n[1][0] = rnd_2d(ix - 1, iy, seed) >> 16;
	nc->n[1][1] = rnd_2d(ix, iy, seed) >> 16;
	nc->n[1][2] = rnd_2d(ix + 1, iy, seed) >> 16;
	nc->n[1][3] = rnd_2d(ix + 2, iy, seed) >> 16;
	cubint2c(nc->n[1], nc->n[1]);

	nc->n[2][0] = rnd_2d(ix - 1, iy + 1, seed) >> 16;
	nc->n[2][1] = rnd_2d(ix, iy + 1, seed) >> 16;
	nc->n[2][2] = rnd_2d(ix + 1, iy + 1, seed) >> 16;
	nc->n[2][3] = rnd_2d(ix + 2, iy + 1, seed) >> 16;
	cubint2c(nc->n[2], nc->n[2]);

	nc->n[3][0] = rnd_2d(ix - 1, iy + 2, seed) >> 16;
	nc->n[3][1] = rnd_2d(ix, iy + 2, seed) >> 16;
	nc->n[3][2] = rnd_2d(ix + 1, iy + 2, seed) >> 16;
	nc->n[3][3] = rnd_2d(ix + 2, iy + 2, seed) >> 16;
	cubint2c(nc->n[3], nc->n[3]);
}

static inline int noise_2d_ci_cached(ZS_PerlinCache *nc,
		unsigned x, unsigned y, int amp)
{
	unsigned fx = x & 0xffff;
	unsigned fy = y & 0xffff;
	int n10 = cubintc(nc->n[0], fx);
	int n11 = cubintc(nc->n[1], fx);
	int n12 = cubintc(nc->n[2], fx);
	int n13 = cubintc(nc->n[3], fx);
	n11 = cubint(n10, n11, n12, n13, fy);
	return (n11 * (amp >> 2) >> 14) - (amp >> 1);
}

static inline int noise_2d_ci_auto(ZS_PerlinCache *nc,
		unsigned x, unsigned y, int amp)
{
	unsigned ix = x >> 16;
	unsigned iy = y >> 16;
	if((ix != nc->ix) || (iy != nc->iy))
	{
		nc->ix = ix;
		nc->iy = iy;
		noise_2d_update_cache(nc);
	}
	return noise_2d_ci_cached(nc, x, y, amp);
}


static void perlin_run_nearest(int *scratch, unsigned w,
		unsigned su, unsigned sv, unsigned sdu, unsigned sdv,
		int a, unsigned seed)
{
	unsigned x;
	for(x = 0; x < w; ++x)
	{
		scratch[x] += noise_2d(su, sv, a, seed);
		su += sdu;
		sv += sdv;
	}
}


static void perlin_run_bilinear(int *scratch, unsigned w,
		unsigned su, unsigned sv, unsigned sdu, unsigned sdv,
		int a, unsigned seed)
{
	unsigned x;
	for(x = 0; x < w; ++x)
	{
		scratch[x] += noise_2d_li(su, sv, a, seed);
		su += sdu;
		sv += sdv;
	}
}


/*
 * Optimized for long runs, ie low detail levels
 *	Coarse:	2795 ms
 *	Fine:	4802 ms
 *
 * For reference:
 *	One layer with no perlin updates at all takes ~2600 ms.
 *	One layer with NEAREST (no interpolation) takes ~700 ms.
 */
static void perlin_run_bicubic_lo(int *scratch, unsigned w,
		unsigned su, unsigned sv, unsigned sdu, unsigned sdv,
		int a, unsigned seed)
{
	unsigned x;
	ZS_PerlinCache nc;
	nc.seed = seed;
//printf("[lo]");
	for(x = 0; x < w; )
	{
		unsigned rw;
		unsigned run = w - x;
		if(sdu)
		{
			unsigned r = (0xffff - (su & 0xffff) + sdu) / sdu;
			if(r < run)
				run = r;
		}
		if(sdv)
		{
			unsigned r = (0xffff - (sv & 0xffff) + sdv) / sdv;
			if(r < run)
				run = r;
		}
		if(!run)
			run = 1;
		nc.ix = su >> 16;
		nc.iy = sv >> 16;
		noise_2d_update_cache(&nc);
		for(rw = x + run; x < rw; ++x)
		{
			scratch[x] += noise_2d_ci_cached(&nc, su, sv, a);
			su += sdu;
			sv += sdv;
		}
	}
}

/*
 * Optimized for very short runs; ie medium detail levels
 *	Coarse:	2914 ms
 *	Fine:	4507 ms
 */
static void perlin_run_bicubic_mid(int *scratch, unsigned w,
		unsigned su, unsigned sv, unsigned sdu, unsigned sdv,
		int a, unsigned seed)
{
	unsigned x;
	ZS_PerlinCache nc;
//printf("[mid]");
	nc.seed = seed;
	nc.ix = ~(su >> 16);
	for(x = 0; x < w; ++x)
	{
		scratch[x] += noise_2d_ci_auto(&nc, su, sv, a);
		su += sdu;
		sv += sdv;
	}
}

#if 0
/*
 * Optimized for single pixels; ie high detail levels
 *	Coarse:	6449 ms
 *	Fine:	6370 ms
 */
static void perlin_run_bicubic_hi(int *scratch, unsigned w,
		unsigned su, unsigned sv, unsigned sdu, unsigned sdv,
		int a, unsigned seed)
{
	unsigned x;
	ZS_PerlinCache nc;
//printf("[hi]");
	nc.seed = seed;
	for(x = 0; x < w; ++x)
	{
		nc.ix = su >> 16;
		nc.iy = sv >> 16;
		noise_2d_update_cache(&nc);
		scratch[x] += noise_2d_ci_cached(&nc, su, sv, a);
		su += sdu;
		sv += sdv;
	}
}
#endif

void zs_PerlinTerrainZ(ZS_Surface *s, float base,
		float u, float v, float du, float dv,
		int namplitudes, float *amplitudes, int flags)
{
	int *scratch = (int *)s->state->scratch;
	int x, y, w, h;
	int ibase = (int)(base * 256);
//FIXME: Scale factor accuracy isn't all that great when zooming in close...
//FIXME: Since we want to use factors in the "several hundreds" for large scale
//FIXME: landscapes, we'll need to deal with this somehow. Tiled rendering...?
	unsigned iu0 = (unsigned)(u * 65536.0f + .5f);
	unsigned iv0 = (unsigned)(v * 65536.0f + .5f);
	int idu = (int)(du * 65536.0f + .5f);
	int idv = (int)(dv * 65536.0f + .5f);
	w = s->w;
	h = s->h;
//printf("width: %d\n", s->w);
	for(y = 0; y < h; ++y)	/* for each scanline */
//y = 0;
	{
		ZS_Pixel *p = zs_Pixel(s, 0, y);
		int oct;
		int a = namplitudes ? (int)(amplitudes[0] * 256) : 256;
		memset(scratch, 0, w * sizeof(int));
		for(oct = 0; ; ++oct)	/* for each octave of detail */
		{
			unsigned seed, su, sv;
			int absd, inter, sdu, sdv;
			if(oct < namplitudes)
				a = (int)(amplitudes[oct] * 256);
			else if((flags & ZS_PT__EXTENDMODE) == ZS_PT_STOP)
				break;		/* All done! */
			else switch(flags & ZS_PT__EXTENDMODE)
			{
			  case ZS_PT_EXTEND:
				break;
			  case ZS_PT_DIV1P5:
				a = 3 * a / 4;
				break;
			  case ZS_PT_DIV2:
				a /= 2;
				break;
			  case ZS_PT_DIV3:
				a /= 3;
				break;
			  case ZS_PT_DIV4:
				a /= 4;
				break;
			}
			if(abs(a) < 16)
			{
				if(oct >= namplitudes)
					break;		/* All done! */
				else
					continue;	/* Skip octave! */
			}

			/* Span start in "perlin space", and step size */
			su = iu0 << oct;
			sv = iv0 << oct;
			sdu = idu << oct;
			sdv = idv << oct;

			/* Fade out around the top octave to reduce "ringing" */
			absd = sqrt((float)sdu * sdu + (float)sdv * sdv);
			if(absd > 98304)
				break;		/* Main exit! */
			if(absd > 32768)
			{
				a = a * (49152 - (absd >> 1)) >> 15;
				if(abs(a) < 16)
					break;	/* Too low, and last octave! */
			}
//printf("octave %d; absd: %d:\n", oct, absd);

			/* Select noise generator seed */
			seed = rnd_seedtab[oct & (RND_SEEDS - 1)];

			/* Generate! */
			inter = flags & ZS_PT__INTERPOLATION;
			if(inter == ZS_PT_ADAPTIVE)
			{
				if((absd < 16384) || (absd > 32768))
					inter = ZS_PT_BICUBIC;
				else
					inter = ZS_PT_BILINEAR;
			}
			switch(inter)
			{
			  case ZS_PT_NEAREST:
				perlin_run_nearest(scratch, w, su, sv, sdu, sdv,
						a, seed);
				break;
			  case ZS_PT_BILINEAR:
				perlin_run_bilinear(scratch, w, su, sv, sdu, sdv,
						a, seed);
				break;
			  case ZS_PT_BICUBIC:
				if(absd < 0x4000)
					perlin_run_bicubic_lo(scratch, w,
							su, sv, sdu, sdv,
							a, seed);
#if 0
// Strangely, this appears to be slower than _mid in all cases...!
				else if(absd > 0x10000)
					perlin_run_bicubic_hi(scratch, w,
							su, sv, sdu, sdv,
							a, seed);
#endif
				else
					perlin_run_bicubic_mid(scratch, w,
							su, sv, sdu, sdv,
							a, seed);
				break;
			}
//printf("\n");
		}

		/* Apply output transform */
		switch(flags & ZS_PT__TRANSFORM)
		{
		  case ZS_PT_T_LINEAR:
			break;
		  case ZS_PT_T_CUBIC:
			for(x = 0; x < w; ++x)
			{
				int z = scratch[x] >> 1;
				scratch[x] = (z * z >> 14) * z >> 14;
			}
			break;
		  case ZS_PT_T_PCUBIC:
			for(x = 0; x < w; ++x)
			{
				int z = (scratch[x] >> 2) + 16384;
				z = (z * z >> 14) * z >> 14;
				scratch[x] = z - 32768;
			}
			break;
		  case ZS_PT_T_NCUBIC:
			for(x = 0; x < w; ++x)
			{
				int z = (scratch[x] >> 2) - 16384;
				z = (z * z >> 14) * z >> 14;
				scratch[x] = z + 32768;
			}
			break;
		}

		/* Add base height, clamp and apply to the Z channel */
		switch(flags & ZS_PT__OUTPUT)
		{
		  case ZS_PT_O_REPLACE:
			for(x = 0; x < w; ++x)
			{
				int z = scratch[x] + ibase;
				if(z < 0)
					z = 0;
				else if(z > 65535)
					z = 65535;
				p[x].z = z;
			}
			break;
		  case ZS_PT_O_ADD:
			for(x = 0; x < w; ++x)
			{
				int z = p[x].z + scratch[x] + ibase;
				if(z < 0)
					z = 0;
				else if(z > 65535)
					z = 65535;
				p[x].z = z;
			}
			break;
		  case ZS_PT_O_SUB:
			for(x = 0; x < w; ++x)
			{
				int z = p[x].z - (scratch[x] + ibase);
				if(z < 0)
					z = 0;
				else if(z > 65535)
					z = 65535;
				p[x].z = z;
			}
			break;
		  case ZS_PT_O_MUL:
			for(x = 0; x < w; ++x)
			{
				int z = ((scratch[x] + ibase) >> 2) *
						p[x].z >> 14;
				if(z < 0)
					z = 0;
				else if(z > 65535)
					z = 65535;
				p[x].z = z;
			}
			break;
		}

		/* Start point for next span */
		iu0 -= idv;
		iv0 += idu;
	}
//printf("usecount: %u (%f%% of total)\n", nc.usecount,
//		100.0f * nc.usecount / (nc.setcount + nc.usecount));
}


/*---------------------------------------------------------------
	Region based rendering
---------------------------------------------------------------*/

static inline unsigned lerp8(unsigned a, unsigned b, unsigned x)
{
	return (a * (256 - x) + b * x) >> 8;
}

void zs_Fill(ZS_Pipe *p, ZS_Region *r, ZS_Surface *s, ZS_Pixel *px)
{
	ZS_Pixel *scratch = s->state->scratch;
	ZS_Pixel *tmp = scratch + s->w;
	int i;
/*TODO: This only needs to cover the longest span in 'r' */
	for(i = 0; i < s->w; ++i)
		scratch[i] = *px;
	for(i = 0; i < r->nspans; ++i)
	{
		ZS_Span *sp = r->spans + i;
		ZS_Pixel *dst;
		int xmin, xmax;
//printf("span %d: %d, %d x %d, %d/%d\n", i, sp->x,  sp->y,  sp->w, sp->c0, sp->c1);
		if(sp->y >= s->h)
			break;
		xmin = sp->x;
		xmax = xmin + sp->w;
		if(xmin >= s->w)
			continue;
		if(xmax > s->w)
			xmax = s->w;
		dst = zs_Pixel(s, xmin, sp->y);
		if((sp->c0 == 256) && (sp->c1 == 256))
			zs_PipeRun(p, 0, scratch, dst, xmax - xmin);
		else
		{
			int x, c0, dc;
			int w = xmax - xmin;
			memcpy(tmp, dst, w * sizeof(ZS_Pixel));
			zs_PipeRun(p, 0, scratch, tmp, w);
			c0 = sp->c0 << 16;
			dc = ((sp->c1 - sp->c0) << 16) / sp->w;
/*TODO: Different ways of applying coverage...? */
			for(x = 0; x < w; ++x)
			{
				int cov = c0 >> 16;
				unsigned a = lerp8(0, tmp[x].a, cov) + dst[x].a;
				if(a > 255)
					dst[x].a = 255;
				else
					dst[x].a = a;
				dst[x].r = lerp8(dst[x].r, tmp[x].r, cov);
				dst[x].g = lerp8(dst[x].g, tmp[x].g, cov);
				dst[x].b = lerp8(dst[x].b, tmp[x].b, cov);
				dst[x].i = lerp8(dst[x].i, tmp[x].i, cov);
				dst[x].z = lerp8(dst[x].z, tmp[x].z, cov);
				c0 += dc;
			}
		}
	}
}
