/*
---------------------------------------------------------------------------
	eel_sdl.c - EEL SDL Binding
---------------------------------------------------------------------------
 * Copyright 2005-2020 David Olofson
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

#ifndef _WIN32
#include <sched.h>
#endif

#include <string.h>
#include "eel_sdl.h"
#include "eel_net.h"
#include "fastevents.h"
#include "net2.h"
#include "eel_opengl.h"
#include "SDL_revision.h"

/*
 * Since SDL is not fully thread safe, only one instance of
 * the EEL SDL library at a time is allowed in a process.
 * This has the advantage that the other Eelium modules can
 * get at the EEL SDL type IDs by simply looking at the
 * static fake "moduledata" struct.
 */
static int loaded = 0;

ESDL_moduledata esdl_md;


/*----------------------------------------------------------
	Versioning
----------------------------------------------------------*/

static EEL_xno esdl_HeaderVersion(EEL_vm *vm)
{
	EEL_object *a, *s;
	EEL_value v;
	SDL_version version;
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;

	SDL_VERSION(&version);
	eel_l2v(&v, version.major);
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, version.minor);
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, version.patch);
	eel_setlindex(a, 2, &v);

	if((s = eel_ps_new(vm, SDL_REVISION)))
		eel_o2v(&v, s);
	else
		eel_nil2v(&v);
	eel_setlindex(a, 3, &v);
	eel_disown(s);

	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}


static EEL_xno esdl_LinkedVersion(EEL_vm *vm)
{
	EEL_object *a, *s;
	EEL_value v;
	SDL_version version;
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;

	SDL_GetVersion(&version);
	eel_l2v(&v, version.major);
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, version.minor);
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, version.patch);
	eel_setlindex(a, 2, &v);

	if((s = eel_ps_new(vm, SDL_GetRevision())))
		eel_o2v(&v, s);
	else
		eel_nil2v(&v);
	eel_setlindex(a, 3, &v);
	eel_disown(s);

	eel_l2v(&v, SDL_GetRevisionNumber());
	eel_setlindex(a, 4, &v);

	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}


/*----------------------------------------------------------
	EEL utilities
----------------------------------------------------------*/

/* Set integer field 'n' of table 'io' to 'val' */
static inline void esdl_seti(EEL_object *io, const char *n, long val)
{
	EEL_value v;
	eel_l2v(&v, val);
	eel_setsindex(io, n, &v);
}


/* Set boolean field 'n' of table 'io' to 'val' */
static inline void esdl_setb(EEL_object *io, const char *n, int val)
{
	EEL_value v;
	eel_b2v(&v, val);
	eel_setsindex(io, n, &v);
}


/*----------------------------------------------------------
	Low level graphics utilities
----------------------------------------------------------*/

static inline void clip_rect(SDL_Rect *r, SDL_Rect *to)
{
	int dx1 = r->x;
	int dy1 = r->y;
	int dx2 = dx1 + r->w;
	int dy2 = dy1 + r->h;
	if(dx1 < to->x)
		dx1 = to->x;
	if(dy1 < to->y)
		dy1 = to->y;
	if(dx2 > to->x + to->w)
		dx2 = to->x + to->w;
	if(dy2 > to->y + to->h)
		dy2 = to->y + to->h;
	if(dx2 < dx1 || dy2 < dy1)
	{
		r->x = r->y = 0;
		r->w = r->h = 0;
	}
	else
	{
		r->x = dx1;
		r->y = dy1;
		r->w = dx2 - dx1;
		r->h = dy2 - dy1;
	}
}


/*
 * Get and lock (if necessary) an SDL surface from an argument, which can be a
 * Surface or a SurfaceLock. Returns 0 upon success, or an EEL exception code.
 */
static inline EEL_xno get_surface(EEL_value *arg, int *locked, SDL_Surface **s)
{
	*locked = 0;
	if(EEL_CLASS(arg) == esdl_md.surfacelock_cid)
	{
		EEL_object *so = o2ESDL_surfacelock(arg->objref.v)->surface;
		if(!so)
			return EEL_XARGUMENTS;	/* No surface! */
		*s = o2ESDL_surface(so)->surface;
		/* This one's locked by definition, so we're done! */
		return 0;
	}
	else if(EEL_CLASS(arg) == esdl_md.surface_cid)
	{
		*s = o2ESDL_surface(arg->objref.v)->surface;
		if(SDL_MUSTLOCK((*s)))
		{
			SDL_LockSurface(*s);
			*locked = 1;
		}
		return 0;
	}
	else
		return EEL_XWRONGTYPE;
}


/* Get pixel from 24 bpp surface, no clipping. */
static inline Uint32 getpixel24(SDL_Surface *s, int x, int y)
{
	Uint8 *p = (Uint8 *)s->pixels;
	Uint8 *p24 = p + y * s->pitch + x * 3;
#if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
	return p24[0] | ((Uint32)p24[1] << 8) | ((Uint32)p24[2] << 16);
#else
	return p24[2] | ((Uint32)p24[1] << 8) | ((Uint32)p24[0] << 16);
#endif
}


/* Get pixel from 32 bpp surface, no clipping. */
static inline Uint32 getpixel32(SDL_Surface *s, int x, int y)
{
	Uint32 *p = (Uint32 *)s->pixels;
	int ppitch = s->pitch / 4;
	return p[y * ppitch + x];
}


/* Get pixel from 24 bpp surface, with clipping. */
static inline Uint32 getpixel24c(SDL_Surface *s, int x, int y, Uint32 cc)
{
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
		return cc;
	else
		return getpixel24(s, x, y);
}


/* Get pixel from 32 bpp surface; with clipping. */
static inline Uint32 getpixel32c(SDL_Surface *s, int x, int y, Uint32 cc)
{
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
		return cc;
	else
		return getpixel32(s, x, y);
}


/*
 * Multiply the R/G/B channels from color value 'c' with weight 'w', and add
 * them to the respective accumulators. 'w' and the accumulators are 16:16
 * fixed point.
 */
static inline void maccrgb(Uint32 c, int w, int *r, int *g, int *b)
{
	*b += (c & 0xff) * w >> 8;
	*g += ((c >> 8) & 0xff) * w >> 8;
	*r += ((c >> 16) & 0xff) * w >> 8;
}


/*
 * Multiply the R/G/B/A channels from color value 'c' with weight 'w', and add
 * them to the respective accumulators. 'w' and the accumulators are 16:16
 * fixed point.
 */
static inline void maccrgba(Uint32 c, int w, int *r, int *g, int *b, int *a)
{
	*b += (c & 0xff) * w >> 8;
	*g += ((c >> 8) & 0xff) * w >> 8;
	*r += ((c >> 16) & 0xff) * w >> 8;
	*a += ((c >> 24) & 0xff) * w >> 8;
}


/* Assemble channels to ARGB color value. (No clamping or masking!) */
static inline Uint32 rgba2c(int r, int g, int b, int a)
{
	return a << 24 | r << 16 | g << 8 | b;
}


/*----------------------------------------------------------
	Rect class
----------------------------------------------------------*/
static EEL_xno r_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	SDL_Rect *r;
	EEL_object *eo = eel_o_alloc(vm, sizeof(SDL_Rect), cid);
	if(!eo)
		return EEL_XMEMORY;
	r = o2SDL_Rect(eo);
	if(!initc)
	{
		r->x = r->y = 0;
		r->w = r->h = 0;
	}
	else if(initc == 1)
	{
		SDL_Rect *sr;
		if(initv->objref.v->classid != esdl_md.rect_cid)
		{
			eel_o_free(eo);
			return EEL_XWRONGTYPE;
		}
		sr = o2SDL_Rect(initv->objref.v);
		memcpy(r, sr, sizeof(SDL_Rect));
	}
	else
	{
		int v;
		if(initc != 4)
		{
			eel_o_free(eo);
			return EEL_XARGUMENTS;
		}
		r->x = eel_v2l(initv);
		r->y = eel_v2l(initv + 1);
		v = eel_v2l(initv + 2);
		r->w = v >= 0 ? v : 0;
		v = eel_v2l(initv + 3);
		r->h = v >= 0 ? v : 0;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno r_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *co = eel_o_alloc(vm, sizeof(SDL_Rect), cid);
	if(!co)
		return EEL_XMEMORY;
	memcpy(o2SDL_Rect(co), o2SDL_Rect(src->objref.v), sizeof(SDL_Rect));
	eel_o2v(dst, co);
	return 0;
}


static EEL_xno r_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Rect *r = o2SDL_Rect(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		op2->integer.v = r->x;
		break;
	  case 'y':
		op2->integer.v = r->y;
		break;
	  case 'w':
		op2->integer.v = r->w;
		break;
	  case 'h':
		op2->integer.v = r->h;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	op2->classid = EEL_CINTEGER;
	return 0;
}


static EEL_xno r_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Rect *r = o2SDL_Rect(eo);
	const char *is = eel_v2s(op1);
	int iv = eel_v2l(op2);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		r->x = iv;
		break;
	  case 'y':
		r->y = iv;
		break;
	  case 'w':
		if(iv >= 0)
			r->w = iv;
		else
			r->w = 0;
		break;
	  case 'h':
		if(iv >= 0)
			r->h = iv;
		else
			r->h = 0;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	return 0;
}


/*----------------------------------------------------------
	Texture class
----------------------------------------------------------*/
static EEL_xno tx_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	SDL_Texture *texture = NULL;
	ESDL_texture *tx;
	EEL_object *eo;
	SDL_Renderer *renderer;
	if(initc < 1)
		return EEL_XARGUMENTS;
	if(EEL_CLASS(initv) != esdl_md.renderer_cid)
		return EEL_XWRONGTYPE;
	renderer = o2ESDL_renderer(initv[0].objref.v)->renderer;
	switch(initc)
	{
	  case 5:	/* renderer, format, access, width, height */
	  {
		Uint32 format = eel_v2l(initv + 1);
		int access = eel_v2l(initv + 2);
		int w = eel_v2l(initv + 3);
		int h = eel_v2l(initv + 4);
		texture = SDL_CreateTexture(renderer, format, access, w, h);
		break;
	  }
	  case 3:	/* renderer, surface, srcrect */
	  {
		Uint32 pf;
		SDL_Surface *s;
		SDL_Rect *r;
		if(EEL_CLASS(initv + 1) != esdl_md.surface_cid)
			return EEL_XWRONGTYPE;
		s = o2ESDL_surface(initv[1].objref.v)->surface;
		if(EEL_CLASS(initv + 2) != esdl_md.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2SDL_Rect(initv[2].objref.v);
		if((r->x < 0) || (r->y < 0))
			return EEL_XLOWVALUE;
		if((r->x + r->w > s->w) || (r->y + r->h > s->h))
			return EEL_XHIGHVALUE;
		pf = SDL_MasksToPixelFormatEnum(s->format->BitsPerPixel,
				s->format->Rmask, s->format->Gmask,
				s->format->Bmask, s->format->Amask);
		texture = SDL_CreateTexture(renderer, pf,
				SDL_TEXTUREACCESS_STATIC, r->w, r->h);
		if(!texture)
			return EEL_XDEVICEERROR;
		if(SDL_UpdateTexture(texture, NULL, (Uint8 *)s->pixels +
				s->format->BytesPerPixel * r->x +
				s->pitch * r->y,
				s->pitch) < 0)
		{
			SDL_DestroyTexture(texture);
			return EEL_XDEVICEWRITE;
		}
		break;
	  }
	  case 2:	/* renderer, surface */
		if(EEL_CLASS(initv + 1) != esdl_md.surface_cid)
			return EEL_XWRONGTYPE;
		texture = SDL_CreateTextureFromSurface(renderer,
				o2ESDL_surface(initv[1].objref.v)->surface);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!texture)
		return EEL_XDEVICEERROR;
	eo = eel_o_alloc(vm, sizeof(ESDL_texture), cid);
	if(!eo)
	{
		SDL_DestroyTexture(texture);
		return EEL_XMEMORY;
	}
	tx = o2ESDL_texture(eo);
	tx->texture = texture;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno tx_destruct(EEL_object *eo)
{
	SDL_DestroyTexture(o2ESDL_texture(eo)->texture);
	return 0;
}


static EEL_xno tx_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Texture *tx = o2ESDL_texture(eo)->texture;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) == 1)
		switch(is[0])
		{
		  case 'w':
			if(SDL_QueryTexture(tx, NULL, NULL,
					&op2->integer.v, NULL) < 0)
				return EEL_XDEVICEREAD;
			break;
		  case 'h':
			if(SDL_QueryTexture(tx, NULL, NULL, NULL,
					&op2->integer.v) < 0)
				return EEL_XDEVICEREAD;
			break;
		  default:
			return EEL_XWRONGINDEX;
		}
	else if(!strcmp(is, "format"))
	{
		Uint32 fmt;
		if(SDL_QueryTexture(tx, &fmt, NULL, NULL, NULL) < 0)
			return EEL_XDEVICEREAD;
		op2->integer.v = fmt;
	}
	else if(!strcmp(is, "access"))
	{
		if(SDL_QueryTexture(tx, NULL, &op2->integer.v, NULL, NULL) < 0)
			return EEL_XDEVICEREAD;
	}
	else if(!strcmp(is, "blendmode"))
	{
		SDL_BlendMode bm;
		if(SDL_GetTextureBlendMode(tx, &bm) < 0)
			return EEL_XDEVICEREAD;
		op2->integer.v = bm;
	}
	else
		return EEL_XWRONGINDEX;
	op2->classid = EEL_CINTEGER;
	return 0;
}


static EEL_xno tx_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Texture *tx = o2ESDL_texture(eo)->texture;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(!strcmp(is, "blendmode"))
	{
		if(SDL_SetTextureBlendMode(tx, eel_v2l(op2)) < 0)
			return EEL_XDEVICEWRITE;
	}
	else
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	Surface class
----------------------------------------------------------*/
static EEL_xno s_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_surface *s;
	EEL_object *eo;
	int w, h, bpp;
	Uint32	flags, rmask, gmask, bmask, amask;
	/* Get the specified parameters */
	amask = 0;
	switch(initc)
	{
	  case 8:	/* RGBA */
		amask = eel_v2l(initv + 7);
		/* Fall through! */
	  case 7:	/* RGB - no alpha */
		flags = eel_v2l(initv);
		w = eel_v2l(initv + 1);
		h = eel_v2l(initv + 2);
		bpp = eel_v2l(initv + 3);
		rmask = eel_v2l(initv + 4);
		gmask = eel_v2l(initv + 5);
		bmask = eel_v2l(initv + 6);
		break;
	  case 4:	/* Use "sensible" default masks for the bpp */
		flags = eel_v2l(initv);
		w = eel_v2l(initv + 1);
		h = eel_v2l(initv + 2);
		bpp = eel_v2l(initv + 3);
		switch(bpp)
		{
		  case 8:
			rmask = 0xe0;
			gmask = 0x1c;
			bmask = 0x03;
			amask = 0;
			break;
		  case 15:
			rmask = 0x7c00;
			gmask = 0x03e0;
			bmask = 0x001f;
			amask = 0;
			break;
		  case 16:
			rmask = 0xf800;
			gmask = 0x07e0;
			bmask = 0x001f;
			amask = 0;
			break;
		  case 24:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			rmask = 0x00ff0000;
			gmask = 0x0000ff00;
			bmask = 0x000000ff;
#else
			rmask = 0x000000ff;
			gmask = 0x0000ff00;
			bmask = 0x00ff0000;
#endif
			amask = 0;
			break;
		  case 32:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			rmask = 0xff000000;
			gmask = 0x00ff0000;
			bmask = 0x0000ff00;
			amask = 0x000000ff;
#else
			rmask = 0x000000ff;
			gmask = 0x0000ff00;
			bmask = 0x00ff0000;
			amask = 0xff000000;
#endif
			break;
		  default:
			return EEL_XARGUMENTS;
		}
		break;
	  case 0:
		// Create empty Surface object
		eo = eel_o_alloc(vm, sizeof(ESDL_surface), cid);
		if(!eo)
			return EEL_XMEMORY;
		s = o2ESDL_surface(eo);
		s->surface = NULL;
		eel_o2v(result, eo);
		return 0;
	  default:
		return EEL_XARGUMENTS;
	}
	eo = eel_o_alloc(vm, sizeof(ESDL_surface), cid);
	if(!eo)
		return EEL_XMEMORY;
	s = o2ESDL_surface(eo);
	s->surface = SDL_CreateRGBSurface(flags, w, h, bpp,
			rmask, gmask, bmask, amask);
	if(!s->surface)
	{
		eel_o_free(eo);
		return EEL_XDEVICEERROR;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno s_destruct(EEL_object *eo)
{
	ESDL_surface *s = o2ESDL_surface(eo);
	if(s->surface && !s->is_window_surface)
		SDL_FreeSurface(s->surface);
	return 0;
}


static EEL_xno s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Surface *s = o2ESDL_surface(eo)->surface;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) == 1)
		switch(is[0])
		{
		  case 'w':
			op2->integer.v = s->w;
			break;
		  case 'h':
			op2->integer.v = s->h;
			break;
		  default:
			return EEL_XWRONGINDEX;
		}
	else if(!strcmp(is, "flags"))
		op2->integer.v = s->flags;
	else if(!strcmp(is, "alpha"))
	{
		Uint8 am;
		SDL_GetSurfaceAlphaMod(s, &am);
		op2->integer.v = am;
	}
	else if(!strcmp(is, "colorkey"))
	{
		Uint32 ck;
		if(SDL_GetColorKey(s, &ck))
			return EEL_XWRONGINDEX;
		op2->integer.v = ck;
	}
	else if(!strcmp(is, "blendmode"))
	{
		SDL_BlendMode bm;
		if(SDL_GetSurfaceBlendMode(s, &bm))
			return EEL_XWRONGINDEX;
		op2->integer.v = bm;
	}
	else if(!strcmp(is, "palette"))
	{
		op2->integer.v = s->format->palette ? 1 : 0;
		op2->classid = EEL_CBOOLEAN;
		return 0;
	}
	else if(!strcmp(is, "BitsPerPixel"))
		op2->integer.v = s->format->BitsPerPixel;
	else if(!strcmp(is, "BytesPerPixel"))
		op2->integer.v = s->format->BytesPerPixel;
	else if(!strcmp(is, "Rmask"))
		op2->integer.v = s->format->Rmask;
	else if(!strcmp(is, "Gmask"))
		op2->integer.v = s->format->Gmask;
	else if(!strcmp(is, "Bmask"))
		op2->integer.v = s->format->Bmask;
	else if(!strcmp(is, "Amask"))
		op2->integer.v = s->format->Amask;
	else if(!strcmp(is, "PixelFormat"))
		op2->integer.v = SDL_MasksToPixelFormatEnum(
				s->format->BitsPerPixel,
				s->format->Rmask,
				s->format->Gmask,
				s->format->Bmask,
				s->format->Amask);
	else
		return EEL_XWRONGINDEX;
	op2->classid = EEL_CINTEGER;
	return 0;
}


static EEL_xno s_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Surface *s = o2ESDL_surface(eo)->surface;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(!strcmp(is, "blendmode"))
	{
		if(SDL_SetSurfaceBlendMode(s, eel_v2l(op2)) < 0)
			return EEL_XDEVICEWRITE;
	}
	else
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	SurfaceLock class
----------------------------------------------------------*/
static EEL_xno sl_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_surfacelock *sl;
	ESDL_surface *s;
	EEL_object *eo;
	if(initc != 1)
		return EEL_XARGUMENTS;
	if(EEL_CLASS(initv) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	eo = eel_o_alloc(vm, sizeof(ESDL_surfacelock), cid);
	if(!eo)
		return EEL_XMEMORY;
	sl = o2ESDL_surfacelock(eo);
	sl->surface = initv->objref.v;
	eel_own(sl->surface);
	s = o2ESDL_surface(sl->surface);
	SDL_LockSurface(s->surface);
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno sl_destruct(EEL_object *eo)
{
	ESDL_surface *s;
	ESDL_surfacelock *sl = o2ESDL_surfacelock(eo);
	if(!sl->surface)
		return 0;
	s = o2ESDL_surface(sl->surface);
	SDL_UnlockSurface(s->surface);
	eel_disown(sl->surface);
	return 0;
}


static EEL_xno sl_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ESDL_surfacelock *sl = o2ESDL_surfacelock(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 's':
		if(sl->surface)
		{
			eel_o2v(op2, sl->surface);
			eel_own(sl->surface);
		}
		else
			eel_nil2v(op2);
		return 0;
	  default:
		return EEL_XWRONGINDEX;
	}
}


static EEL_xno esdl_LockSurface(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_CLASS(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	return eel_o_construct(vm, esdl_md.surfacelock_cid, arg, 1,
			vm->heap + vm->resv);
}


static EEL_xno esdl_UnlockSurface(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	ESDL_surfacelock *sl;
	ESDL_surface *s;
	if(EEL_CLASS(arg) != esdl_md.surfacelock_cid)
		return EEL_XWRONGTYPE;
	sl = o2ESDL_surfacelock(arg->objref.v);
	if(!sl->surface)
		return 0;
	s = o2ESDL_surface(sl->surface);
	SDL_UnlockSurface(s->surface);
	eel_disown(sl->surface);
	sl->surface = NULL;
	return 0;
}


/*----------------------------------------------------------
	Joystick input
----------------------------------------------------------*/

static void esdl_detach_joysticks(void)
{
	ESDL_joystick *j = esdl_md.joysticks;
	while(j)
	{
		j->joystick = NULL;
		j = j->next;
	}
}

static EEL_xno esdl_DetectJoysticks(EEL_vm *vm)
{
	esdl_detach_joysticks();
	if(SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	eel_l2v(vm->heap + vm->resv, SDL_NumJoysticks());
	return 0;
}


static EEL_xno esdl_NumJoysticks(EEL_vm *vm)
{
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	eel_l2v(vm->heap + vm->resv, SDL_NumJoysticks());
	return 0;
}


static EEL_xno esdl_JoystickName(EEL_vm *vm)
{
	const char *s;
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	s = SDL_JoystickNameForIndex(eel_v2l(vm->heap + vm->argv));
	if(!s)
		return EEL_XWRONGINDEX;
	eel_s2v(vm, vm->heap + vm->resv, s);
	return 0;
}


static EEL_xno j_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_joystick *j;
	EEL_object *eo;
	int ind;
	if(!SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	switch(initc)
	{
	  case 0:
		ind = 0;
		break;
	  case 1:
		ind = eel_v2l(initv);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
#if 0
	if(SDL_JoystickOpened(ind))
		return EEL_XDEVICEOPENED;
#endif
	eo = eel_o_alloc(vm, sizeof(ESDL_joystick), cid);
	if(!eo)
		return EEL_XMEMORY;
	j = o2ESDL_joystick(eo);
	j->index = ind;
	j->joystick = SDL_JoystickOpen(j->index);
	j->next = esdl_md.joysticks;
	esdl_md.joysticks = j;
	if(!j->joystick)
	{
		eel_o_free(eo);
		return EEL_XDEVICEERROR;
	}
	j->name = eel_ps_new(vm, SDL_JoystickName(j->joystick));
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno j_destruct(EEL_object *eo)
{
	ESDL_joystick *j = o2ESDL_joystick(eo);
	ESDL_joystick *js = esdl_md.joysticks;
	while(js)
	{
		ESDL_joystick *jp;
		if(js == j)
		{
			if(js == esdl_md.joysticks)
				esdl_md.joysticks = js->next;
			else
				jp->next = js->next;
			break;
		}
		jp = js;
		js = js->next;
	}
	if(j->joystick && SDL_WasInit(SDL_INIT_JOYSTICK))
		SDL_JoystickClose(j->joystick);
	eel_disown(j->name);
	return 0;
}


static EEL_xno j_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ESDL_joystick *j = o2ESDL_joystick(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(!strcmp(is, "name"))
		eel_o2v(op2, j->name);
	else if(!strcmp(is, "axes"))
		eel_l2v(op2, SDL_JoystickNumAxes(j->joystick));
	else if(!strcmp(is, "buttons"))
		eel_l2v(op2, SDL_JoystickNumButtons(j->joystick));
	else if(!strcmp(is, "balls"))
		eel_l2v(op2, SDL_JoystickNumBalls(j->joystick));
	else
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	Window class
----------------------------------------------------------*/

static EEL_xno w_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_window *win;
	EEL_object *eo;
	const char *title;
	int x, y, w, h;
	Uint32 flags = 0;
	switch(initc)
	{
	  case 6:
		flags = eel_v2l(initv + 5);
		/* Fall-trough */
	  case 5:
		title = eel_v2s(initv);
		x = eel_v2l(initv + 1);
		y = eel_v2l(initv + 2);
		w = eel_v2l(initv + 3);
		h = eel_v2l(initv + 4);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(ESDL_window), cid)))
		return EEL_XMEMORY;
	win = o2ESDL_window(eo);
	if(!(win->window = SDL_CreateWindow(title, x, y, w, h, flags)))
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno w_destruct(EEL_object *eo)
{
	SDL_DestroyWindow(o2ESDL_window(eo)->window);
	return 0;
}


static EEL_xno w_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Window *win = o2ESDL_window(eo)->window;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(!strcmp(is, "surface"))
	{
		ESDL_surface *s;
		EEL_object *so = eel_o_alloc(eo->vm, sizeof(ESDL_surface),
				esdl_md.surface_cid);
		if(!so)
			return EEL_XMEMORY;
		s = o2ESDL_surface(so);
		s->is_window_surface = 1;
		s->surface = SDL_GetWindowSurface(win);
		if(!s->surface)
		{
			eel_o_free(so);
			return EEL_XDEVICEOPEN;
		}
		eel_o2v(op2, so);
		return 0;
	}
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		SDL_GetWindowPosition(win, &op2->integer.v, NULL);
		break;
	  case 'y':
		SDL_GetWindowPosition(win, NULL, &op2->integer.v);
		break;
	  case 'w':
		SDL_GetWindowSize(win, &op2->integer.v, NULL);
		break;
	  case 'h':
		SDL_GetWindowSize(win, NULL, &op2->integer.v);
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	op2->classid = EEL_CINTEGER;
	return 0;
}


/*----------------------------------------------------------
	Renderer class
----------------------------------------------------------*/

static EEL_xno rn_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_renderer *r;
	EEL_object *eo;
	ESDL_window *win;
	int drv = -1;
	Uint32 flags = 0;

	/* SDL_CreateSoftwareRenderer() */
	if((initc == 1) && (EEL_CLASS(initv) == esdl_md.surface_cid))
	{
		SDL_Surface *s = o2ESDL_surface(initv->objref.v)->surface;
		if(!(eo = eel_o_alloc(vm, sizeof(ESDL_renderer), cid)))
			return EEL_XMEMORY;
		r = o2ESDL_renderer(eo);
		if(!(r->renderer = SDL_CreateSoftwareRenderer(s)))
		{
			eel_o_free(eo);
			return EEL_XDEVICEOPEN;
		}
		eel_o2v(result, eo);
		return 0;
	}

	/* SDL_CreateRenderer() */
	switch(initc)
	{
	  case 3:
		flags = eel_v2l(initv + 2);
		/* Fall-trough */
	  case 2:
		drv = eel_v2l(initv + 1);
		/* Fall-trough */
	  case 1:
		if(EEL_CLASS(initv) != esdl_md.window_cid)
			return EEL_XWRONGTYPE;
		win = o2ESDL_window(initv->objref.v);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(ESDL_renderer), cid)))
		return EEL_XMEMORY;
	r = o2ESDL_renderer(eo);
	if(!(r->renderer = SDL_CreateRenderer(win->window, drv, flags)))
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno rn_destruct(EEL_object *eo)
{
	SDL_DestroyRenderer(o2ESDL_renderer(eo)->renderer);
	return 0;
}


static EEL_xno rn_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	SDL_Renderer *rn = o2ESDL_renderer(eo)->renderer;
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'w':
		if(SDL_GetRendererOutputSize(rn, &op2->integer.v, NULL) < 0)
			return EEL_XDEVICEREAD;
		break;
	  case 'h':
		if(SDL_GetRendererOutputSize(rn, NULL, &op2->integer.v) < 0)
			return EEL_XDEVICEREAD;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	op2->classid = EEL_CINTEGER;
	return 0;
}


/*----------------------------------------------------------
	GLContext class
----------------------------------------------------------*/

static EEL_xno glc_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	ESDL_glcontext *glc;
	EEL_object *eo;
	ESDL_window *win;
	switch(initc)
	{
	  case 1:
		if(EEL_CLASS(initv) != esdl_md.window_cid)
			return EEL_XWRONGTYPE;
		win = o2ESDL_window(initv->objref.v);
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(!(eo = eel_o_alloc(vm, sizeof(ESDL_glcontext), cid)))
		return EEL_XMEMORY;
	glc = o2ESDL_glcontext(eo);
	if(!(glc->context = SDL_GL_CreateContext(win->window)))
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno glc_destruct(EEL_object *eo)
{
	SDL_GL_DeleteContext(o2ESDL_glcontext(eo)->context);
	return 0;
}


/*----------------------------------------------------------
	OpenGL support calls
----------------------------------------------------------*/

static EEL_xno esdl_gl_setattribute(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int attr = eel_v2l(args);
	if(attr == GL_DOUBLEBUFFER)
		attr = SDL_GL_DOUBLEBUFFER;
	SDL_GL_SetAttribute(attr, eel_v2l(args + 1));
	return 0;
}


static EEL_xno esdl_gl_setswapinterval(EEL_vm *vm)
{
	SDL_GL_SetSwapInterval(eel_v2l(vm->heap + vm->argv));
	return 0;
}


static EEL_xno esdl_gl_swapwindow(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_GL_SwapWindow(o2ESDL_window(args->objref.v)->window);
	return 0;
}


/*----------------------------------------------------------
	SDL Calls
----------------------------------------------------------*/

static EEL_xno esdl_SetWindowTitle(EEL_vm *vm)
{
	SDL_Window *win;
	const char *title;
	ESDL_ARG_WINDOW(0, win)
	ESDL_ARG_STRING(1, title)
	SDL_SetWindowTitle(win, title);
	return 0;
}


static EEL_xno esdl_ShowCursor(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv, SDL_ShowCursor(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


static EEL_xno esdl_WarpMouse(EEL_vm *vm)
{
	int x, y;
	SDL_Window *win;
	ESDL_ARG_INTEGER(0, x)
	ESDL_ARG_INTEGER(1, y)
	ESDL_OPTARG_WINDOW(2, win, NULL);
	if(win)
		SDL_WarpMouseInWindow(win, x, y);
	else
		SDL_WarpMouseGlobal(x, y);
	return 0;
}


static EEL_xno esdl_SetWindowGrab(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ESDL_window *win;
	if(EEL_CLASS(args) != esdl_md.window_cid)
		return EEL_XWRONGTYPE;
	win = o2ESDL_window(args[0].objref.v);
	SDL_SetWindowGrab(win->window, eel_v2l(args + 1));
	return 0;
}


static EEL_xno esdl_GetTicks(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv, SDL_GetTicks());
	return 0;
}


static EEL_xno esdl_Delay(EEL_vm *vm)
{
	Uint32 d = eel_v2l(vm->heap + vm->argv);
#ifdef _WIN32
	SDL_Delay(d);
#else
	if(!d)
		sched_yield();
	else
		SDL_Delay(d);
#endif
	return 0;
}


static EEL_xno esdl_SetRenderTarget(EEL_vm *vm)
{
	SDL_Renderer *rn;
	SDL_Texture *tx;
	ESDL_ARG_RENDERER(0, rn)
	ESDL_OPTARGNIL_TEXTURE(1, tx, NULL)
	if(SDL_SetRenderTarget(rn, tx) < 0)
		return EEL_XDEVICECONTROL;
	return 0;
}


static EEL_xno esdl_RenderTargetSupported(EEL_vm *vm)
{
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	eel_b2v(vm->heap + vm->resv, SDL_RenderTargetSupported(rn));
	return 0;
}


static EEL_xno esdl_SetRenderDrawColor(EEL_vm *vm)
{
	SDL_Renderer *rn;
	int r, g, b, a;
	ESDL_ARG_RENDERER(0, rn)
	switch(vm->argc)
	{
	  case 2:
	  {
		Uint32 c;
		ESDL_ARG_INTEGER(1, c)
		r = c >> 16;
		g = c >> 8;
		b = c;
		a = 255;
		break;
	  }
	  case 4:
	  case 5:
		ESDL_ARG_INTEGER(1, r)
		ESDL_ARG_INTEGER(2, g)
		ESDL_ARG_INTEGER(3, b)
		ESDL_OPTARG_INTEGER(4, a, 255)
		if(r < 0)
			r = 0;
		else if(r > 255)
			r = 255;
		if(g < 0)
			g = 0;
		else if(g > 255)
			g = 255;
		if(b < 0)
			b = 0;
		else if(b > 255)
			b = 255;
		if(vm->argc >= 5)
		{
			if(a < 0)
				a = 0;
			else if(a > 255)
				a = 255;
		}
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	if(SDL_SetRenderDrawColor(rn, r, g, b, a) < 0)
		return EEL_XDEVICECONTROL;
	return 0;
}


static EEL_xno esdl_SetRenderDrawBlendMode(EEL_vm *vm)
{
	SDL_Renderer *rn;
	SDL_BlendMode bm;
	ESDL_ARG_RENDERER(0, rn)
	ESDL_ARG_INTEGER(1, bm)
	if(SDL_SetRenderDrawBlendMode(rn, bm) < 0)
		return EEL_XDEVICECONTROL;
	return 0;
}


static EEL_xno esdl_RenderSetClipRect(EEL_vm *vm)
{
	SDL_Rect *r;
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	ESDL_OPTARGNIL_RECT(1, r, NULL)
	SDL_RenderSetClipRect(rn, r);
	return 0;
}


static EEL_xno esdl_RenderDrawPoint(EEL_vm *vm)
{
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	if(vm->argc == 3)
	{
		// (renderer, x, y)
		int x, y;
		ESDL_ARG_INTEGER(1, x)
		ESDL_ARG_INTEGER(2, y)
		if(SDL_RenderDrawPoint(rn, x, y) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if((vm->argc >= 5) && (vm->argc & 1))
	{
		// (renderer)<x, y>
		int i;
		int n = (vm->argc - 1) >> 1;
		SDL_Point *points = (SDL_Point *)eel_scratch(vm,
				sizeof(SDL_Point) * n);
		if(!points)
			return EEL_XMEMORY;
		for(i = 0; i < n; ++i)
		{
			ESDL_ARG_INTEGER((i << 1) + 1, points[i].x)
			ESDL_ARG_INTEGER((i << 1) + 2, points[i].y)
		}
		if(SDL_RenderDrawPoints(rn, points, n) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if(vm->argc == 2)
	{
		// (renderer, indexable of <x, y>  tuples)
		// TODO: Optimizations for vector_s32 and maybe others
		EEL_value *args = vm->heap + vm->argv;
		int i;
		EEL_value iv;
		EEL_object *a;
		SDL_Point *points;
		int n;
		if(!EEL_IS_OBJREF(args[1].classid))
			return EEL_XCANTINDEX;
		a = args[1].objref.v;
		n = eel_length(a);
		if(n & 1)
			return EEL_XNEEDEVEN;
		n >>= 1;
		points = (SDL_Point *)eel_scratch(vm, sizeof(SDL_Point) * n);
		if(!points)
			return EEL_XMEMORY;
		eel_l2v(&iv, 0);
		for(i = 0; i < n; ++i)
		{
			EEL_value v;
			EEL_xno x;
			iv.integer.v = i << 1;
			if((x = eel_o_metamethod(a, EEL_MM_GETINDEX, &iv, &v)))
				return x;
			points[i].x = eel_v2l(&v);
			eel_v_disown(&v);
			++iv.integer.v;
			if((x = eel_o_metamethod(a, EEL_MM_GETINDEX, &iv, &v)))
				return x;
			points[i].y = eel_v2l(&v);
			eel_v_disown(&v);
		}
		if(SDL_RenderDrawPoints(rn, points, n) < 0)
			return EEL_XDEVICECONTROL;
	}
	else
		return EEL_XARGUMENTS;
	return 0;
}


static EEL_xno esdl_RenderDrawLine(EEL_vm *vm)
{
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	if(vm->argc == 5)
	{
		int x1, y1, x2, y2;
		ESDL_ARG_INTEGER(1, x1)
		ESDL_ARG_INTEGER(2, y1)
		ESDL_ARG_INTEGER(3, x2)
		ESDL_ARG_INTEGER(4, y2)
		if(SDL_RenderDrawLine(rn, x1, y1, x2, y2) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if((vm->argc >= 7) && (vm->argc & 1))
	{
		// (renderer)<x, y>
		int i;
		int n = (vm->argc - 1) >> 1;
		SDL_Point *points = (SDL_Point *)eel_scratch(vm,
				sizeof(SDL_Point) * n);
		if(!points)
			return EEL_XMEMORY;
		for(i = 0; i < n; ++i)
		{
			ESDL_ARG_INTEGER((i << 1) + 1, points[i].x)
			ESDL_ARG_INTEGER((i << 1) + 2, points[i].y)
		}
		if(SDL_RenderDrawLines(rn, points, n) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if(vm->argc == 2)
	{
		// (renderer, indexable of <x, y>  tuples)
		// TODO: Optimizations for vector_s32 and maybe others
		EEL_value *args = vm->heap + vm->argv;
		int i;
		EEL_value iv;
		EEL_object *a;
		SDL_Point *points;
		int n;
		if(!EEL_IS_OBJREF(args[1].classid))
			return EEL_XCANTINDEX;
		a = args[1].objref.v;
		n = eel_length(a);
		if(n & 1)
			return EEL_XNEEDEVEN;
		n >>= 1;
		points = (SDL_Point *)eel_scratch(vm, sizeof(SDL_Point) * n);
		if(!points)
			return EEL_XMEMORY;
		eel_l2v(&iv, 0);
		for(i = 0; i < n; ++i)
		{
			EEL_value v;
			EEL_xno x;
			iv.integer.v = i << 1;
			if((x = eel_o_metamethod(a, EEL_MM_GETINDEX, &iv, &v)))
				return x;
			points[i].x = eel_v2l(&v);
			eel_v_disown(&v);
			++iv.integer.v;
			if((x = eel_o_metamethod(a, EEL_MM_GETINDEX, &iv, &v)))
				return x;
			points[i].y = eel_v2l(&v);
			eel_v_disown(&v);
		}
		if(SDL_RenderDrawLines(rn, points, n) < 0)
			return EEL_XDEVICECONTROL;
	}
	else
		return EEL_XARGUMENTS;
	return 0;
}


static EEL_xno esdl_RenderClear(EEL_vm *vm)
{
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	if(SDL_RenderClear(rn) < 0)
		return EEL_XDEVICECONTROL;
	return 0;
}


static EEL_xno esdl_RenderFillRect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	if(vm->argc == 1)
	{
		if(SDL_RenderFillRect(rn, NULL) < 0)
			return EEL_XDEVICECONTROL;
		return 0;
	}
	else if(vm->argc == 5)
	{
		SDL_Rect r;
		ESDL_ARG_INTEGER(1, r.x)
		ESDL_ARG_INTEGER(2, r.y)
		ESDL_ARG_INTEGER(3, r.w)
		ESDL_ARG_INTEGER(4, r.h)
		if(SDL_RenderFillRect(rn, &r) < 0)
			return EEL_XDEVICECONTROL;
		return 0;
	}
	else if(vm->argc != 2)
		return EEL_XARGUMENTS;

	if(EEL_CLASS(args + 1) == esdl_md.rect_cid)
	{
		if(SDL_RenderFillRect(rn, o2SDL_Rect(args[1].objref.v)) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if(EEL_CLASS(args + 1) == EEL_CARRAY)
	{
		int i, len;
		EEL_value v;
		EEL_object *a = args[1].objref.v;
		SDL_Rect *ra;
		len = eel_length(a);
		if(len < 0)
			return EEL_XCANTINDEX;
		ra = eel_scratch(vm, len * sizeof(SDL_Rect));
		if(!ra)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
		{
			EEL_xno x = eel_getlindex(a, i, &v);
			if(x)
				return x;
			if(EEL_CLASS(&v) != esdl_md.rect_cid)
			{
				eel_v_disown(&v);
				continue;	/* Ignore... */
			}
			ra[i] = *o2SDL_Rect(v.objref.v);
			eel_disown(v.objref.v);
		}
		SDL_RenderFillRects(rn, ra, len);
	}
	else
		return EEL_XWRONGTYPE;
	return 0;
}


static EEL_xno esdl_RenderCopy(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Renderer *rn;
	SDL_Texture *tx;
	SDL_Rect *srcr, *dstr;
	ESDL_ARG_RENDERER(0, rn)
	ESDL_ARG_TEXTURE(1, tx)
	ESDL_OPTARGNIL_RECT(2, srcr, NULL)
	if((vm->argc == 5) && (EEL_CLASS(args + 3) == EEL_CINTEGER ||
			EEL_CLASS(args + 3) == EEL_CREAL))
	{
		SDL_Rect dr;
		if(srcr)
		{
			dr.w = srcr->w;
			dr.h = srcr->h;
		}
		else
		{
			if(SDL_QueryTexture(tx, NULL, NULL, &dr.w, &dr.h) < 0)
				return EEL_XDEVICEREAD;
		}
		ESDL_ARG_INTEGER(3, dr.x)
		ESDL_ARG_INTEGER(4, dr.y)
		if(SDL_RenderCopy(rn, tx, srcr, &dr) < 0)
			return EEL_XDEVICECONTROL;
	}
	else if(vm->argc <= 4)
	{
		ESDL_OPTARGNIL_RECT(3, dstr, NULL)
		if(SDL_RenderCopy(rn, tx, srcr, dstr) < 0)
			return EEL_XDEVICECONTROL;
	}
	else
	{
		double a;
		SDL_Point p;
		SDL_Point *pp;
		int flip;
		ESDL_OPTARGNIL_RECT(3, dstr, NULL)
		ESDL_OPTARG_REAL(4, a, 0.0f)
		ESDL_OPTARGNIL_INTEGER(5, p.x, 999999)
		ESDL_OPTARGNIL_INTEGER(6, p.y, 999999)
		ESDL_OPTARGNIL_INTEGER(7, flip, SDL_FLIP_NONE)
		if((p.x == 999999) && (p.y == 999999))
			pp = NULL;
		else
		{
			pp = &p;
			if((p.x == 999999) || (p.y == 999999))
			{
				int w, h;
				if(SDL_QueryTexture(tx, NULL, NULL,
						&w, &h) < 0)
					return EEL_XDEVICEREAD;
				if(p.x == 999999)
					p.x = w / 2;
				if(p.y == 999999)
					p.y = h / 2;
			}
		}
		if(SDL_RenderCopyEx(rn, tx, srcr, dstr, a, pp, flip) < 0)
			return EEL_XDEVICECONTROL;
	}
	return 0;
}


static EEL_xno esdl_RenderPresent(EEL_vm *vm)
{
	SDL_Renderer *rn;
	ESDL_ARG_RENDERER(0, rn)
	SDL_RenderPresent(rn);
	return 0;
}


static EEL_xno esdl_SetClipRect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Surface *s;
	SDL_Rect *r = NULL;
	if(EEL_CLASS(args) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(args->objref.v)->surface;
	if((vm->argc >= 2) && (EEL_CLASS(args + 1) != EEL_CNIL))
	{
		if(EEL_CLASS(args + 1) != esdl_md.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2SDL_Rect(args[1].objref.v);
	}
	SDL_SetClipRect(s, r);
	return 0;
}


static EEL_xno esdl_UpdateWindowSurface(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value *args = vm->heap + vm->argv;
	SDL_Window *win;
	ESDL_ARG_WINDOW(0, win)
	if(vm->argc == 1)
	{
		if(SDL_UpdateWindowSurface(win) < 0)
			return EEL_XDEVICECONTROL;
		return 0;
	}
	if(EEL_CLASS(args + 1) == esdl_md.rect_cid)
	{
		if(SDL_UpdateWindowSurfaceRects(win,
				o2SDL_Rect(args[1].objref.v), 1) < 0)
			return EEL_XDEVICECONTROL;
		return 0;
	}
	else if(EEL_CLASS(args + 1) == EEL_CARRAY)
	{
		int i, len;
		EEL_value v;
		EEL_object *a = args[1].objref.v;
		SDL_Rect *ra;
		len = eel_length(a);
		if(len < 0)
			return EEL_XCANTINDEX;
		ra = eel_scratch(vm, len * sizeof(SDL_Rect));
		if(!ra)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
		{
			x = eel_getlindex(a, i, &v);
			if(x)
				return x;
			if(EEL_CLASS(&v) != esdl_md.rect_cid)
			{
				eel_v_disown(&v);
				continue;	/* Ignore... */
			}
			ra[i] = *o2SDL_Rect(v.objref.v);
			eel_disown(v.objref.v);
		}
		SDL_UpdateWindowSurfaceRects(win, ra, len);
	}
	else
		return EEL_XWRONGTYPE;
	return 0;
}


static EEL_xno esdl_BlitSurface(EEL_vm *vm)
{
	SDL_Surface *from, *to;
	SDL_Rect *fromr, *tor;
	ESDL_ARG_SURFACE(0, from)
	ESDL_OPTARGNIL_RECT(1, fromr, NULL)
	ESDL_ARG_SURFACE(2, to)
	ESDL_OPTARGNIL_RECT(3, tor, NULL)
	switch(SDL_BlitSurface(from, fromr, to, tor))
	{
	  case -1:
		return EEL_XDEVICEWRITE;
	  case -2:
		return EEL_XEOF;	/* Lost surface! (DirectX) */
	}
	return 0;
}


static EEL_xno esdl_FillRect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Surface *to;
	int color;
	ESDL_ARG_SURFACE(0, to)
	ESDL_OPTARG_INTEGER(2, color, 0)
	if((vm->argc < 2) || (EEL_CLASS(args + 1) == EEL_CNIL))
	{
		if(SDL_FillRect(to, NULL, color) < 0)
			return EEL_XDEVICEWRITE;
	}
	else if(EEL_CLASS(args + 1) == esdl_md.rect_cid)
	{
		SDL_Rect *tor;
		ESDL_ARG_RECT(1, tor)
		if(SDL_FillRect(to, tor, color) < 0)
			return EEL_XDEVICEWRITE;
	}
	else if(EEL_CLASS(args + 1) == EEL_CARRAY)
	{
		int i, len;
		EEL_value v;
		EEL_object *a = args[1].objref.v;
		SDL_Rect *ra;
		len = eel_length(a);
		if(len < 0)
			return EEL_XCANTINDEX;
		ra = eel_scratch(vm, len * sizeof(SDL_Rect));
		if(!ra)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
		{
			EEL_xno x = eel_getlindex(a, i, &v);
			if(x)
				return x;
			if(EEL_CLASS(&v) != esdl_md.rect_cid)
			{
				eel_v_disown(&v);
				continue;	/* Ignore... */
			}
			ra[i] = *o2SDL_Rect(v.objref.v);
			eel_disown(v.objref.v);
		}
		i = SDL_FillRects(to, ra, len, color);
		if(i < 0)
			return EEL_XDEVICEWRITE;
	}
	else
		return EEL_XWRONGTYPE;
	return 0;
}


static EEL_xno esdl_SetSurfaceAlphaMod(EEL_vm *vm)
{
	SDL_Surface *s;
	Uint8 a;
	ESDL_ARG_SURFACE(0, s)
	ESDL_OPTARG_INTEGER(1, a, 255)
	if(SDL_SetSurfaceAlphaMod(s, a) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static EEL_xno esdl_SetSurfaceColorMod(EEL_vm *vm)
{
	SDL_Surface *s;
	Uint8 r, g, b;
	ESDL_ARG_SURFACE(0, s)
	ESDL_OPTARG_INTEGER(1, r, 255)
	ESDL_OPTARG_INTEGER(2, g, 255)
	ESDL_OPTARG_INTEGER(3, b, 255)
	if(SDL_SetSurfaceColorMod(s, r, g, b) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static EEL_xno esdl_SetColorKey(EEL_vm *vm)
{
	SDL_Surface *s;
	int flag;
	Uint32 key;
	ESDL_ARG_SURFACE(0, s)
	ESDL_ARG_INTEGER(1, flag)
	ESDL_ARG_INTEGER(2, key)
	if(SDL_SetColorKey(s, flag, key) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


static inline void esdl_raw2color(Uint32 r, SDL_Color *c)
{
	c->a = (r >> 24) & 0xff;
	c->r = (r >> 16) & 0xff;
	c->g = (r >> 8) & 0xff;
	c->b = r & 0xff;
}


/* function SetColors(surface, colors)[firstcolor] */
static EEL_xno esdl_SetColors(EEL_vm *vm)
{
	SDL_Surface *s;
	SDL_Palette *p;
	Uint32 first;
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *res = vm->heap + vm->resv;
	if(EEL_CLASS(args) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(args->objref.v)->surface;
	p = s->format->palette;
	if(!p)
		return EEL_XDEVICEWRITE;
	if(vm->argc >= 3)
		first = eel_v2l(args + 2);
	else
		first = 0;
	switch(EEL_CLASS(args + 1))
	{
	  case EEL_CINTEGER:
	  {
		/* Single color value */
		SDL_Color c;
		esdl_raw2color(args[1].integer.v, &c);
		eel_b2v(res, SDL_SetPaletteColors(p, &c, first, 1));
		return 0;
	  }
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  {
		/* Vector of color values */
		int i;
		SDL_Color *c;
		EEL_object *o = eel_v2o(args + 1);
		Uint32 *rd;
		int len = eel_length(o);
		if(len < 1)
			return EEL_XFEWITEMS;
		rd = eel_rawdata(o);
		if(!rd)
			return EEL_XCANTREAD;
		c = (SDL_Color *)eel_scratch(vm, sizeof(SDL_Color) * len);
		if(!c)
			return EEL_XMEMORY;
		for(i = 0; i < len; ++i)
			esdl_raw2color(rd[i], c + i);
		eel_b2v(res, SDL_SetPaletteColors(p, c, first, len));
		return 0;
	  }
	  default:
		return EEL_XWRONGTYPE;
	}
}


static EEL_xno esdl_MapColor(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	SDL_Surface *s;
	Uint32 color;
	int r, g, b;
	int a = -1;
	if(EEL_CLASS(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(arg->objref.v)->surface;
	if(vm->argc == 2)
	{
		// Packed "hex" ARGB color
		int c = eel_v2l(arg + 1);
		a = (c >> 24) & 0xff;
		r = (c >> 16) & 0xff;
		g = (c >> 8) & 0xff;
		b = c & 0xff;
	}
	else if(vm->argc >= 4)
	{
		// Individual R, G, B, and (optionally) A arguments
		r = eel_v2l(arg + 1);
		if(r < 0)
			r = 0;
		else if(r > 255)
			r = 255;
		g = eel_v2l(arg + 2);
		if(g < 0)
			g = 0;
		else if(g > 255)
			g = 255;
		b = eel_v2l(arg + 3);
		if(b < 0)
			b = 0;
		else if(b > 255)
			b = 255;
		if(vm->argc >= 5)
		{
			// A, if specified
			a = eel_v2l(arg + 4);
			if(a < 0)
				a = 0;
			else if(a > 255)
				a = 255;
		}
	}
	else
		return EEL_XARGUMENTS;
	if(a >= 0)
		color = SDL_MapRGBA(s->format, r, g, b, a);
	else
		color = SDL_MapRGB(s->format, r, g, b);
	vm->heap[vm->resv].classid = EEL_CINTEGER;
	vm->heap[vm->resv].integer.v = color;
	return 0;
}


static EEL_xno esdl_GetColor(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	SDL_Surface *s;
	Uint8 r, g, b, a;
	if(EEL_CLASS(arg) != esdl_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2ESDL_surface(arg->objref.v)->surface;
	SDL_GetRGBA(eel_v2l(arg + 1), s->format, &r, &g, &b, &a);
	vm->heap[vm->resv].classid = EEL_CINTEGER;
	vm->heap[vm->resv].integer.v = rgba2c(r, g, b, a);
	return 0;
}


static EEL_xno esdl_Plot(EEL_vm *vm)
{
	EEL_xno res;
	int i, xmin, ymin, xmax, ymax, locked;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	Uint32 color = eel_v2l(arg + 1);
	if((res = get_surface(arg, &locked, &s)))
		return res;
	xmin = s->clip_rect.x;
	ymin = s->clip_rect.y;
	xmax = xmin + s->clip_rect.w;
	ymax = ymin + s->clip_rect.h;
	switch(s->format->BytesPerPixel)
	{
	  case 1:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		int ppitch = s->pitch;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	  case 2:
	  {
		Uint16 *p = (Uint16 *)s->pixels;
		int ppitch = s->pitch / 2;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	  case 3:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		for(i = 2; i < vm->argc; i += 2)
		{
			Uint8 *p24;
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p24 = p + y * s->pitch + x * 3;
#if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
			p24[0] = color;
			p24[1] = color >> 8;
			p24[2] = color >> 16;
#else
			p24[0] = color >> 16;
			p24[1] = color >> 8;
			p24[2] = color;
#endif
		}
		break;
	  }
	  case 4:
	  {
		Uint32 *p = (Uint32 *)s->pixels;
		int ppitch = s->pitch / 4;
		for(i = 2; i < vm->argc; i += 2)
		{
			int x = eel_v2l(arg + i);
			int y = eel_v2l(arg + i + 1);
			if((x < xmin) || (y < ymin) || (x >= xmax) || (y >= ymax))
				continue;
			p[y * ppitch + x] = color;
		}
		break;
	  }
	}
	if(locked)
		SDL_UnlockSurface(s);
	return 0;
}


/* function GetPixel(surface, x, y)[clipreturn] */
static EEL_xno esdl_GetPixel(EEL_vm *vm)
{
	EEL_xno res;
	int x, y, locked;
	Uint32 color = 0;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	if((res = get_surface(arg, &locked, &s)))
		return res;
	x = eel_v2l(arg + 1);
	y = eel_v2l(arg + 2);
	if((x < 0) || (y < 0) || (x >= s->w) || (y >= s->h))
	{
		/* Clip! Fail, or return 'clipreturn'. */
		if(locked)
			SDL_UnlockSurface(s);
		if(vm->argc >= 4)
		{
			vm->heap[vm->resv] = arg[3];
			return 0;
		}
		else
			return EEL_XWRONGINDEX;
	}

	switch(s->format->BytesPerPixel)
	{
	  case 1:
	  {
		Uint8 *p = (Uint8 *)s->pixels;
		int ppitch = s->pitch;
		color = p[y * ppitch + x];
		break;
	  }
	  case 2:
	  {
		Uint16 *p = (Uint16 *)s->pixels;
		int ppitch = s->pitch / 2;
		color = p[y * ppitch + x];
		break;
	  }
	  case 3:
		color = getpixel24(s, x, y);
		break;
	  case 4:
		color = getpixel32(s, x, y);
		break;
	}
	if(locked)
		SDL_UnlockSurface(s);
	eel_l2v(vm->heap + vm->resv, color);
	return 0;
}


/*
 * function InterPixel(surface, x, y)[clipcolor]
 *
 *	'surface' needs to be a 24 (8:8:8) or 32 (8:8:8:8) bpp Surface[Lock].
 *
 *	'x' and 'y' can be integer or real coordinates.
 *
 *	'clipcolor' is the color used for off-surface pixels; 0 (black,
 *	transparent) if not specified.
 */
static EEL_xno esdl_InterPixel(EEL_vm *vm)
{
	EEL_xno res = 0;
	float x, y;
	int ix, iy, locked, w[4], r, g, b, a;
	Uint32 fx, fy, clipcolor;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	if((res = get_surface(arg, &locked, &s)))
		return res;
	x = eel_v2d(arg + 1);
	y = eel_v2d(arg + 2);
	if(vm->argc >= 4)
		clipcolor = eel_v2l(arg + 3);
	else
		clipcolor = 0;
	ix = floor(x);
	iy = floor(y);

	if((ix < -1) || (iy < -1) || (ix >= s->w - 1) || (iy >= s->h - 1))
	{
		/* Full clip! */
		if(locked)
			SDL_UnlockSurface(s);
		eel_l2v(vm->heap + vm->resv, clipcolor);
		return 0;
	}

	fx = (x - ix) * 32767.9f;
	fy = (y - iy) * 32767.9f;
	r = g = b = a = 0;
	w[0] = (32768 - fx) * (32768 - fy) >> 14;
	w[1] = fx * (32768 - fy) >> 14;
	w[2] = (32768 - fx) * fy >> 14;
	w[3] = fx * fy >> 14;

	if((ix >= 0) && (iy >= 0) && (ix < s->w - 1) && (iy < s->h - 1))
	{
		/* No clip! */
		switch(s->format->BytesPerPixel)
		{
		  case 3:
			maccrgb(getpixel24(s, x, y),
					w[0], &r, &g, &b);
			maccrgb(getpixel24(s, x + 1, y),
					w[1], &r, &g, &b);
			maccrgb(getpixel24(s, x, y + 1),
					w[2], &r, &g, &b);
			maccrgb(getpixel24(s, x + 1, y + 1),
					w[3], &r, &g, &b);
			break;
		  case 4:
			maccrgba(getpixel32(s, x, y),
					w[0], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x + 1, y),
					w[1], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x, y + 1),
					w[2], &r, &g, &b, &a);
			maccrgba(getpixel32(s, x + 1, y + 1),
					w[3], &r, &g, &b, &a);
			break;
		  default:
			if(locked)
				SDL_UnlockSurface(s);
			return EEL_XWRONGFORMAT;
		}
	}
	else
	{
		/* Partial clip! */
		switch(s->format->BytesPerPixel)
		{
		  case 3:
			maccrgb(getpixel24c(s, ix, iy, clipcolor),
					w[0], &r, &g, &b);
			maccrgb(getpixel24c(s, ix + 1, iy, clipcolor),
					w[1], &r, &g, &b);
			maccrgb(getpixel24c(s, ix, iy + 1, clipcolor),
					w[2], &r, &g, &b);
			maccrgb(getpixel24c(s, ix + 1, iy + 1, clipcolor),
					w[3], &r, &g, &b);
			break;
		  case 4:
			maccrgba(getpixel32c(s, ix, iy, clipcolor),
					w[0], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix + 1, iy, clipcolor),
					w[1], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix, iy + 1, clipcolor),
					w[2], &r, &g, &b, &a);
			maccrgba(getpixel32c(s, ix + 1, iy + 1, clipcolor),
					w[3], &r, &g, &b, &a);
			break;
		  default:
			if(locked)
				SDL_UnlockSurface(s);
			return EEL_XWRONGFORMAT;
		}
	}

	if(locked)
		SDL_UnlockSurface(s);

	eel_l2v(vm->heap + vm->resv, rgba2c(r >> 8, g >> 8, b >> 8, a >> 8));
	return 0;
}


/*
 * function FilterPixel(surface, x, y, filter)[clipcolor]
 *
 *	'surface' needs to be a 24 (8:8:8) or 32 (8:8:8:8) bpp Surface[Lock].
 *
 *	'x' and 'y' can be integer or real coordinates.
TODO: Actually implement real coordinates with interpolation?
 *
 *	'filter' is a vector_s32, where [0, 1] hold the width and height of the
 *	filter core, followed by the filter core itself, expressed as 16:16
 *	fixed point weights.
 *
 *	'clipcolor' is the color used for off-surface pixels; 0 (black,
 *	transparent) if not specified.
 */
static EEL_xno esdl_FilterPixel(EEL_vm *vm)
{
	EEL_xno res = 0;
	int x0, y0, x, y, locked, r, g, b, a;
	Uint32 clipcolor;
	Uint32 *filter;
	SDL_Surface *s;
	EEL_value *arg = vm->heap + vm->argv;
	x0 = eel_v2l(arg + 1);
	y0 = eel_v2l(arg + 2);
	if(EEL_CLASS(arg + 3) == EEL_CVECTOR_S32)
	{
		EEL_object *o = eel_v2o(arg + 3);
		int len = eel_length(o);
		if(len < 2)
			return EEL_XWRONGFORMAT;	/* Expected w, h! */
		filter = eel_rawdata(o);
		if(!filter)
			return EEL_XCANTREAD;	/* (Can this even happen?) */
		if(filter[0] * filter[1] > len + 2)
			return EEL_XFEWITEMS;	/* Incomplete filter core! */
	}
	else
		return EEL_XWRONGTYPE;
	if(vm->argc >= 5)
		clipcolor = eel_v2l(arg + 4);
	else
		clipcolor = 0;
	if((res = get_surface(arg, &locked, &s)))
		return res;

	/* No optimized cases here. Just brute-force per-pixel clipping. */
	x0 -= filter[0] >> 1;
	y0 -= filter[1] >> 1;
	r = g = b = a = 0;
	switch(s->format->BytesPerPixel)
	{
	  case 3:
		for(y = 0; y < filter[1]; ++y)
		{
			Uint32 *frow = filter + 2 + y * filter[0];
			for(x = 0; x < filter[0]; ++x)
				maccrgb(getpixel24c(s, x0 + x, y0 + y,
						clipcolor),
						frow[x], &r, &g, &b);
		}
		break;
	  case 4:
		for(y = 0; y < filter[1]; ++y)
		{
			Uint32 *frow = filter + 2 + y * filter[0];
			for(x = 0; x < filter[0]; ++x)
				maccrgba(getpixel32c(s, x0 + x, y0 + y,
						clipcolor),
						frow[x], &r, &g, &b, &a);
		}
		break;
	  default:
		if(locked)
			SDL_UnlockSurface(s);
		return EEL_XWRONGFORMAT;
	}

	if(locked)
		SDL_UnlockSurface(s);

	eel_l2v(vm->heap + vm->resv, rgba2c(r >> 8, g >> 8, b >> 8, a >> 8));
	return 0;
}


/*----------------------------------------------------------
	Event handling
----------------------------------------------------------*/

static EEL_xno esdl_PumpEvents(EEL_vm *vm)
{
	FE_PumpEvents();
	return 0;
}


static EEL_xno esdl_CheckEvent(EEL_vm *vm)
{
	eel_b2v(vm->heap + vm->resv, FE_PollEvent(NULL));
	return 0;
}


static inline void esdl_setsocket(EEL_object *io, SDL_Event *ev)
{
	EEL_value v;
	EEL_object *s = eel_net_get_socket(io->vm, NET2_GetSocket(ev));
	if(s)
		eel_o2v(&v, s);
	else
		eel_nil2v(&v);
	eel_setsindex(io, "socket", &v);
}

static EEL_xno esdl_decode_event(EEL_vm *vm, SDL_Event *ev)
{
	EEL_value v;
	EEL_object *t;
	EEL_xno x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return x;
	t = eel_v2o(&v);
	esdl_seti(t, "type", ev->type);
	esdl_seti(t, "timestamp", ev->common.timestamp);
	switch(ev->type)
	{
	  case SDL_WINDOWEVENT:
		esdl_seti(t, "windowID", ev->window.windowID);
		esdl_seti(t, "event", ev->window.event);
		switch(ev->window.event)
		{
		  case SDL_WINDOWEVENT_MOVED:
			esdl_seti(t, "x", ev->window.data1);
			esdl_seti(t, "y", ev->window.data2);
			break;
		  case SDL_WINDOWEVENT_RESIZED:
			esdl_seti(t, "w", ev->window.data1);
			esdl_seti(t, "h", ev->window.data2);
			break;
		}
		break;
	  case SDL_KEYDOWN:
	  case SDL_KEYUP:
		esdl_seti(t, "windowID", ev->key.windowID);
		esdl_seti(t, "state", ev->key.state);
		esdl_seti(t, "scancode", ev->key.keysym.scancode);
		esdl_seti(t, "sym", ev->key.keysym.sym);
		esdl_seti(t, "mod", ev->key.keysym.mod);
		break;
	  case SDL_TEXTEDITING:
		esdl_seti(t, "windowID", ev->edit.windowID);
		eel_table_setss(t, "text", ev->edit.text);
		esdl_seti(t, "start", ev->edit.start);
		esdl_seti(t, "length", ev->edit.length);
		break;
	  case SDL_TEXTINPUT:
		esdl_seti(t, "windowID", ev->edit.windowID);
		eel_table_setss(t, "text", ev->edit.text);
		break;
	  case SDL_MOUSEMOTION:
		esdl_seti(t, "windowID", ev->key.windowID);
		esdl_seti(t, "which", ev->motion.which);
		esdl_seti(t, "state", ev->motion.state);
		esdl_seti(t, "x", ev->motion.x);
		esdl_seti(t, "y", ev->motion.y);
		esdl_seti(t, "xrel", ev->motion.xrel);
		esdl_seti(t, "yrel", ev->motion.yrel);
		break;
	  case SDL_MOUSEWHEEL:
		esdl_seti(t, "windowID", ev->key.windowID);
		esdl_seti(t, "which", ev->motion.which);
		esdl_seti(t, "x", ev->motion.x);
		esdl_seti(t, "y", ev->motion.y);
		esdl_seti(t, "direction", ev->motion.xrel);
		break;
	  case SDL_MOUSEBUTTONDOWN:
	  case SDL_MOUSEBUTTONUP:
		esdl_seti(t, "windowID", ev->key.windowID);
		esdl_seti(t, "which", ev->button.which);
		esdl_seti(t, "button", ev->button.button);
		esdl_seti(t, "state", ev->button.state);
		esdl_seti(t, "x", ev->button.x);
		esdl_seti(t, "y", ev->button.y);
		break;
	  case SDL_JOYAXISMOTION:
		esdl_seti(t, "which", ev->jaxis.which);
		esdl_seti(t, "axis", ev->jaxis.axis);
		esdl_seti(t, "value", ev->jaxis.value);
		break;
	  case SDL_JOYBALLMOTION:
		esdl_seti(t, "which", ev->jball.which);
		esdl_seti(t, "ball", ev->jball.ball);
		esdl_seti(t, "xrel", ev->jball.xrel);
		esdl_seti(t, "yrel", ev->jball.yrel);
		break;
	  case SDL_JOYHATMOTION:
		esdl_seti(t, "which", ev->jhat.which);
		esdl_seti(t, "hat", ev->jhat.hat);
		esdl_seti(t, "value", ev->jhat.value);
		break;
	  case SDL_JOYBUTTONDOWN:
	  case SDL_JOYBUTTONUP:
		esdl_seti(t, "which", ev->jbutton.which);
		esdl_seti(t, "button", ev->jbutton.button);
		esdl_seti(t, "state", ev->jbutton.state);
		break;
	  case SDL_USEREVENT:
		switch(NET2_GetEventType(ev))
		{
		  case NET2_ERROREVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			eel_s2v(vm, &v, NET2_GetEventError(ev));
			eel_setsindex(t, "error", &v);
			break;
		  case NET2_TCPACCEPTEVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			esdl_seti(t, "port", NET2_GetEventData(ev));
			break;
		  case NET2_TCPRECEIVEEVENT:
		  case NET2_TCPCLOSEEVENT:
		  case NET2_UDPRECEIVEEVENT:
			esdl_seti(t, "type", SDL_USEREVENT + ev->user.code);
			esdl_setsocket(t, ev);
			break;
		  default:
			esdl_seti(t, "code", ev->user.code);
			break;
		}
		break;
	  case SDL_QUIT:
	  default:
		break;
	}
	eel_o2v(vm->heap + vm->resv, t);
	return 0;
}


static EEL_xno esdl_PollEvent(EEL_vm *vm)
{
	SDL_Event ev;
	if(!FE_PollEvent(&ev))
	{
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	return esdl_decode_event(vm, &ev);
}


static EEL_xno esdl_WaitEvent(EEL_vm *vm)
{
	SDL_Event ev;
	if(!FE_WaitEvent(&ev))
		return EEL_XDEVICEERROR;
	return esdl_decode_event(vm, &ev);
}


static EEL_xno esdl_EventState(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	Uint8 etype = eel_v2l(args);
	Uint8 res;
	if(vm->argc >= 2)
		res = SDL_EventState(etype, eel_v2l(args + 1));
	else
		res = SDL_EventState(etype, SDL_QUERY);
	eel_l2v(vm->heap + vm->resv, res);
	return 0;
}


/*----------------------------------------------------------
	Keyboard input
----------------------------------------------------------*/

static EEL_xno esdl_StartTextInput(EEL_vm *vm)
{
	SDL_StartTextInput();
	return 0;
}


static EEL_xno esdl_StopTextInput(EEL_vm *vm)
{
	SDL_StopTextInput();
	return 0;
}


static EEL_xno esdl_SetTextInputRect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	SDL_Rect *r = NULL;
	if((vm->argc >= 1) && (EEL_CLASS(args) != EEL_CNIL))
	{
		if(EEL_CLASS(args) != esdl_md.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2SDL_Rect(args[0].objref.v);
	}
	SDL_SetTextInputRect(r);
	return 0;
}


static EEL_xno esdl_IsTextInputActive(EEL_vm *vm)
{
	eel_b2v(vm->heap + vm->resv, SDL_IsTextInputActive());
	return 0;
}


static EEL_xno esdl_GetScancodeName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_s2v(vm, vm->heap + vm->resv, SDL_GetScancodeName(eel_v2l(args)));
	return 0;
}


static EEL_xno esdl_GetKeyName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_s2v(vm, vm->heap + vm->resv, SDL_GetKeyName(eel_v2l(args)));
	return 0;
}


/*----------------------------------------------------------
	Simple audio interface
----------------------------------------------------------*/

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
	int samples_out = len / 4;
	int samples_in = sfifo_used(&esdl_md.audiofifo) / 4;
	if(samples_in > samples_out)
		samples_in = samples_out;
	sfifo_read(&esdl_md.audiofifo, stream, samples_in * 4);
	if(samples_in < samples_out)
		memset(stream + samples_in * 4, 0, (samples_out - samples_in) * 4);
	esdl_md.audio_pos += samples_out;
}


/* procedure OpenAudio(samplerate, buffersize, fifosize); */
static EEL_xno esdl_OpenAudio(EEL_vm *vm)
{
	SDL_AudioSpec aspec;
	EEL_value *args = vm->heap + vm->argv;
	if(esdl_md.audio_open)
		return EEL_XDEVICEOPENED;
	if(sfifo_init(&esdl_md.audiofifo, eel_v2l(args + 2) * 4) < 0)
		return EEL_XDEVICEOPEN;
	aspec.freq = eel_v2l(args);
	aspec.format = AUDIO_S16SYS;
	aspec.channels = 2;
	aspec.samples = eel_v2l(args + 1);
	aspec.callback = audio_callback;
	if(SDL_OpenAudio(&aspec, NULL) < 0)
	{
		sfifo_close(&esdl_md.audiofifo);
		return EEL_XDEVICEOPEN;
	}
	esdl_md.audio_pos = 0;
	esdl_md.audio_open = 1;
	SDL_PauseAudio(0);
	return 0;
}


static void audio_close(void)
{
	if(!esdl_md.audio_open)
		return;
	SDL_CloseAudio();
	sfifo_close(&esdl_md.audiofifo);
	esdl_md.audio_open = 0;
}


static EEL_xno esdl_CloseAudio(EEL_vm *vm)
{
	audio_close();
	return 0;
}


static inline Sint16 get_sample(EEL_value *v)
{
	if(v->classid == EEL_CINTEGER)
		return v->integer.v >> 16;
	else
	{
		double s = eel_v2d(v) * 32768.0f;
		if(s < -32768.0f)
			return -32768;
		else if(s > 32767.0f)
			return 32767;
		else
			return (Sint16)s;
	}
}

static EEL_xno esdl_PlayAudio(EEL_vm *vm)
{
	Sint16 s[2];
	EEL_value *args = vm->heap + vm->argv;
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	if(sfifo_space(&esdl_md.audiofifo) < sizeof(s))
		return EEL_XDEVICEWRITE;
	if(EEL_IS_OBJREF(args[0].classid))
	{
		EEL_value iv;
		EEL_object *o0 = args[0].objref.v;
		long len = eel_length(o0);
		EEL_object *o1 = NULL;
		eel_l2v(&iv, 0);
		if(vm->argc >= 2)
		{
			if(!EEL_IS_OBJREF(args[1].classid))
				return EEL_XNEEDOBJECT;
			o1 = args[1].objref.v;
			if(eel_length(o1) < 0)
				return EEL_XWRONGTYPE;
		}
		for(iv.integer.v = 0; iv.integer.v < len; ++iv.integer.v)
		{
			EEL_value v;
			EEL_xno x = eel_o_metamethod(o0, EEL_MM_GETINDEX,
					&iv, &v);
			if(x)
				return x;
			s[0] = get_sample(&v);
			eel_v_disown(&v);
			if(o1)
			{
				x = eel_o_metamethod(o1, EEL_MM_GETINDEX,
						&iv, &v);
				if(x)
					return x;
				s[1] = get_sample(&v);
				eel_v_disown(&v);
			}
			else
				s[1] = s[0];
			sfifo_write(&esdl_md.audiofifo, &s, sizeof(s));
		}
	}
	else
	{
		s[0] = get_sample(args);
		if(vm->argc >= 2)
			s[1] = get_sample(args + 1);
		else
			s[1] = s[0];
		sfifo_write(&esdl_md.audiofifo, &s, sizeof(s));
	}
	return 0;
}


static EEL_xno esdl_AudioPosition(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, esdl_md.audio_pos);
	return 0;
}


static EEL_xno esdl_AudioBuffer(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, sfifo_used(&esdl_md.audiofifo) / 4);
	return 0;
}


static EEL_xno esdl_AudioSpace(EEL_vm *vm)
{
	if(!esdl_md.audio_open)
		return EEL_XDEVICECLOSED;
	eel_l2v(vm->heap + vm->resv, sfifo_space(&esdl_md.audiofifo) / 4);
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno esdl_sdl_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close the EEL state */
	if(closing)
	{
		esdl_detach_joysticks();
		if(esdl_md.audio_open)
			audio_close();
		eel_gl_dummy_calls();
		memset(&esdl_md, 0, sizeof(esdl_md));
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp esdl_constants[] =
{
	/* Endian handling */
	{"BYTEORDER",	SDL_BYTEORDER},
	{"BIG_ENDIAN",	SDL_BIG_ENDIAN},
	{"LIL_ENDIAN",	SDL_LIL_ENDIAN},

	/* Flags for windows */
	{"WINDOW_FULLSCREEN",		SDL_WINDOW_FULLSCREEN},
	{"WINDOW_FULLSCREEN_DESKTOP",	SDL_WINDOW_FULLSCREEN_DESKTOP},
	{"WINDOW_OPENGL",		SDL_WINDOW_OPENGL},
	{"WINDOW_HIDDEN",		SDL_WINDOW_HIDDEN},
	{"WINDOW_BORDERLESS",		SDL_WINDOW_BORDERLESS},
	{"WINDOW_RESIZABLE",		SDL_WINDOW_RESIZABLE},
	{"WINDOW_MINIMIZED",		SDL_WINDOW_MINIMIZED},
	{"WINDOW_MAXIMIZED",		SDL_WINDOW_MAXIMIZED},
	{"WINDOW_INPUT_GRABBED",	SDL_WINDOW_INPUT_GRABBED},
	{"WINDOW_ALLOW_HIGHDPI",	SDL_WINDOW_ALLOW_HIGHDPI},

	/* Special values for CreateWindow() */
	{"WINDOWPOS_CENTERED",		SDL_WINDOWPOS_CENTERED},
	{"WINDOWPOS_UNDEFINED",		SDL_WINDOWPOS_UNDEFINED},

	/* Flags for CreateRenderer() */
	{"RENDERER_SOFTWARE",		SDL_RENDERER_SOFTWARE},
	{"RENDERER_ACCELERATED",	SDL_RENDERER_ACCELERATED},
	{"RENDERER_PRESENTVSYNC",	SDL_RENDERER_PRESENTVSYNC},
	{"RENDERER_TARGETTEXTURE",	SDL_RENDERER_TARGETTEXTURE},

	/* Render blend modes */
	{"BLENDMODE_NONE",	SDL_BLENDMODE_NONE},
	{"BLENDMODE_BLEND",	SDL_BLENDMODE_BLEND},
	{"BLENDMODE_ADD",	SDL_BLENDMODE_ADD},
	{"BLENDMODE_MOD",	SDL_BLENDMODE_MOD},

	/* Flags for Surface */
	{"SWSURFACE",	SDL_SWSURFACE},

	/* Texture pixel formats */
	{"PIXELFORMAT_UNKNOWN",		SDL_PIXELFORMAT_UNKNOWN},
	{"PIXELFORMAT_INDEX1LSB",	SDL_PIXELFORMAT_INDEX1LSB},
	{"PIXELFORMAT_INDEX1MSB",	SDL_PIXELFORMAT_INDEX1MSB},
	{"PIXELFORMAT_INDEX4LSB",	SDL_PIXELFORMAT_INDEX4LSB},
	{"PIXELFORMAT_INDEX4MSB",	SDL_PIXELFORMAT_INDEX4MSB},
	{"PIXELFORMAT_INDEX8",		SDL_PIXELFORMAT_INDEX8},
	{"PIXELFORMAT_RGB332",		SDL_PIXELFORMAT_RGB332},
	{"PIXELFORMAT_RGB444",		SDL_PIXELFORMAT_RGB444},
	{"PIXELFORMAT_RGB555",		SDL_PIXELFORMAT_RGB555},
	{"PIXELFORMAT_BGR555",		SDL_PIXELFORMAT_BGR555},
	{"PIXELFORMAT_ARGB4444",	SDL_PIXELFORMAT_ARGB4444},
	{"PIXELFORMAT_RGBA4444",	SDL_PIXELFORMAT_RGBA4444},
	{"PIXELFORMAT_ABGR4444",	SDL_PIXELFORMAT_ABGR4444},
	{"PIXELFORMAT_BGRA4444",	SDL_PIXELFORMAT_BGRA4444},
	{"PIXELFORMAT_ARGB1555",	SDL_PIXELFORMAT_ARGB1555},
	{"PIXELFORMAT_RGBA5551",	SDL_PIXELFORMAT_RGBA5551},
	{"PIXELFORMAT_ABGR1555",	SDL_PIXELFORMAT_ABGR1555},
	{"PIXELFORMAT_BGRA5551",	SDL_PIXELFORMAT_BGRA5551},
	{"PIXELFORMAT_RGB565",		SDL_PIXELFORMAT_RGB565},
	{"PIXELFORMAT_BGR565",		SDL_PIXELFORMAT_BGR565},
	{"PIXELFORMAT_RGB24",		SDL_PIXELFORMAT_RGB24},
	{"PIXELFORMAT_BGR24",		SDL_PIXELFORMAT_BGR24},
	{"PIXELFORMAT_RGB888",		SDL_PIXELFORMAT_RGB888},
	{"PIXELFORMAT_RGBX8888",	SDL_PIXELFORMAT_RGBX8888},
	{"PIXELFORMAT_BGR888",		SDL_PIXELFORMAT_BGR888},
	{"PIXELFORMAT_BGRX8888",	SDL_PIXELFORMAT_BGRX8888},
	{"PIXELFORMAT_ARGB8888",	SDL_PIXELFORMAT_ARGB8888},
	{"PIXELFORMAT_RGBA8888",	SDL_PIXELFORMAT_RGBA8888},
	{"PIXELFORMAT_ABGR8888",	SDL_PIXELFORMAT_ABGR8888},
	{"PIXELFORMAT_BGRA8888",	SDL_PIXELFORMAT_BGRA8888},
	{"PIXELFORMAT_ARGB2101010",	SDL_PIXELFORMAT_ARGB2101010},
#ifdef SDL_PIXELFORMAT_RGBA32
	{"PIXELFORMAT_RGBA32",		SDL_PIXELFORMAT_RGBA32},
	{"PIXELFORMAT_ARGB32",		SDL_PIXELFORMAT_ARGB32},
	{"PIXELFORMAT_BGRA32",		SDL_PIXELFORMAT_BGRA32},
	{"PIXELFORMAT_ABGR32",		SDL_PIXELFORMAT_ABGR32},
#endif
	{"PIXELFORMAT_YV12",		SDL_PIXELFORMAT_YV12},
	{"PIXELFORMAT_IYUV",		SDL_PIXELFORMAT_IYUV},
	{"PIXELFORMAT_YUY2",		SDL_PIXELFORMAT_YUY2},
	{"PIXELFORMAT_UYVY",		SDL_PIXELFORMAT_UYVY},
	{"PIXELFORMAT_YVYU",		SDL_PIXELFORMAT_YVYU},
#ifdef SDL_PIXELFORMAT_NV12
	{"PIXELFORMAT_NV12",		SDL_PIXELFORMAT_NV12},
	{"PIXELFORMAT_NV21",		SDL_PIXELFORMAT_NV21},
#endif

	/* Texture access modes */
	{"TEXTUREACCESS_STATIC",	SDL_TEXTUREACCESS_STATIC},
	{"TEXTUREACCESS_STREAMING",	SDL_TEXTUREACCESS_STREAMING},
	{"TEXTUREACCESS_TARGET",	SDL_TEXTUREACCESS_TARGET},

	/* Flip actions for RenderCopy() */
	{"FLIP_NONE",		SDL_FLIP_NONE},
	{"FLIP_HORIZONTAL",	SDL_FLIP_HORIZONTAL},
	{"FLIP_VERTICAL",	SDL_FLIP_VERTICAL},

	/* Alpha constants */
	{"ALPHA_TRANSPARENT",	SDL_ALPHA_TRANSPARENT},
	{"ALPHA_OPAQUE",	SDL_ALPHA_OPAQUE},

	/* SDL event types */
	{"WINDOWEVENT",		SDL_WINDOWEVENT},
	{"KEYDOWN",		SDL_KEYDOWN},
	{"KEYUP",		SDL_KEYUP},
	{"TEXTEDITING",		SDL_TEXTEDITING},
	{"TEXTINPUT",		SDL_TEXTINPUT},
	{"MOUSEMOTION",		SDL_MOUSEMOTION},
	{"MOUSEWHEEL",		SDL_MOUSEWHEEL},
	{"MOUSEBUTTONDOWN",	SDL_MOUSEBUTTONDOWN},
	{"MOUSEBUTTONUP",	SDL_MOUSEBUTTONUP},
	{"JOYAXISMOTION",	SDL_JOYAXISMOTION},
	{"JOYBALLMOTION",	SDL_JOYBALLMOTION},
	{"JOYHATMOTION",	SDL_JOYHATMOTION},
	{"JOYBUTTONDOWN",	SDL_JOYBUTTONDOWN},
	{"JOYBUTTONUP",		SDL_JOYBUTTONUP},
	{"QUIT",		SDL_QUIT},
	{"USEREVENT",		SDL_USEREVENT},

	/* WINDOWEVENT IDs */
	{"WINDOWEVENT_SHOWN",		SDL_WINDOWEVENT_SHOWN},
	{"WINDOWEVENT_HIDDEN",		SDL_WINDOWEVENT_HIDDEN},
	{"WINDOWEVENT_EXPOSED",		SDL_WINDOWEVENT_EXPOSED},
	{"WINDOWEVENT_MOVED",		SDL_WINDOWEVENT_MOVED},
	{"WINDOWEVENT_RESIZED",		SDL_WINDOWEVENT_RESIZED},
	{"WINDOWEVENT_SIZE_CHANGED",	SDL_WINDOWEVENT_SIZE_CHANGED},
	{"WINDOWEVENT_MINIMIZED",	SDL_WINDOWEVENT_MINIMIZED},
	{"WINDOWEVENT_MAXIMIZED",	SDL_WINDOWEVENT_MAXIMIZED},
	{"WINDOWEVENT_RESTORED",	SDL_WINDOWEVENT_RESTORED},
	{"WINDOWEVENT_ENTER",		SDL_WINDOWEVENT_ENTER},
	{"WINDOWEVENT_LEAVE",		SDL_WINDOWEVENT_LEAVE},
	{"WINDOWEVENT_FOCUS_GAINED",	SDL_WINDOWEVENT_FOCUS_GAINED},
	{"WINDOWEVENT_FOCUS_LOST",	SDL_WINDOWEVENT_FOCUS_LOST},
	{"WINDOWEVENT_CLOSE",		SDL_WINDOWEVENT_CLOSE},
#ifdef SDL_WINDOWEVENT_TAKE_FOCUS
	{"WINDOWEVENT_TAKE_FOCUS",	SDL_WINDOWEVENT_TAKE_FOCUS},
#endif
#ifdef SDL_WINDOWEVENT_HIT_TEST
	{"WINDOWEVENT_HIT_TEST",	SDL_WINDOWEVENT_HIT_TEST},
#endif

	/* Symbolic key codes */
#define	ESDL_SDLK(x)	{"K"#x,		SDLK_##x},
	ESDL_SDLK(UNKNOWN)
	ESDL_SDLK(RETURN)
	ESDL_SDLK(ESCAPE)
	ESDL_SDLK(BACKSPACE)
	ESDL_SDLK(TAB)
	ESDL_SDLK(SPACE)
	ESDL_SDLK(EXCLAIM)
	ESDL_SDLK(QUOTEDBL)
	ESDL_SDLK(HASH)
	ESDL_SDLK(PERCENT)
	ESDL_SDLK(DOLLAR)
	ESDL_SDLK(AMPERSAND)
	ESDL_SDLK(QUOTE)
	ESDL_SDLK(LEFTPAREN)
	ESDL_SDLK(RIGHTPAREN)
	ESDL_SDLK(ASTERISK)
	ESDL_SDLK(PLUS)
	ESDL_SDLK(COMMA)
	ESDL_SDLK(MINUS)
	ESDL_SDLK(PERIOD)
	ESDL_SDLK(SLASH)
	ESDL_SDLK(0)
	ESDL_SDLK(1)
	ESDL_SDLK(2)
	ESDL_SDLK(3)
	ESDL_SDLK(4)
	ESDL_SDLK(5)
	ESDL_SDLK(6)
	ESDL_SDLK(7)
	ESDL_SDLK(8)
	ESDL_SDLK(9)
	ESDL_SDLK(COLON)
	ESDL_SDLK(SEMICOLON)
	ESDL_SDLK(LESS)
	ESDL_SDLK(EQUALS)
	ESDL_SDLK(GREATER)
	ESDL_SDLK(QUESTION)
	ESDL_SDLK(AT)
	ESDL_SDLK(LEFTBRACKET)
	ESDL_SDLK(BACKSLASH)
	ESDL_SDLK(RIGHTBRACKET)
	ESDL_SDLK(CARET)
	ESDL_SDLK(UNDERSCORE)
	ESDL_SDLK(BACKQUOTE)
	ESDL_SDLK(a)
	ESDL_SDLK(b)
	ESDL_SDLK(c)
	ESDL_SDLK(d)
	ESDL_SDLK(e)
	ESDL_SDLK(f)
	ESDL_SDLK(g)
	ESDL_SDLK(h)
	ESDL_SDLK(i)
	ESDL_SDLK(j)
	ESDL_SDLK(k)
	ESDL_SDLK(l)
	ESDL_SDLK(m)
	ESDL_SDLK(n)
	ESDL_SDLK(o)
	ESDL_SDLK(p)
	ESDL_SDLK(q)
	ESDL_SDLK(r)
	ESDL_SDLK(s)
	ESDL_SDLK(t)
	ESDL_SDLK(u)
	ESDL_SDLK(v)
	ESDL_SDLK(w)
	ESDL_SDLK(x)
	ESDL_SDLK(y)
	ESDL_SDLK(z)
	ESDL_SDLK(CAPSLOCK)
	ESDL_SDLK(F1)
	ESDL_SDLK(F2)
	ESDL_SDLK(F3)
	ESDL_SDLK(F4)
	ESDL_SDLK(F5)
	ESDL_SDLK(F6)
	ESDL_SDLK(F7)
	ESDL_SDLK(F8)
	ESDL_SDLK(F9)
	ESDL_SDLK(F10)
	ESDL_SDLK(F11)
	ESDL_SDLK(F12)
	ESDL_SDLK(PRINTSCREEN)
	ESDL_SDLK(SCROLLLOCK)
	ESDL_SDLK(PAUSE)
	ESDL_SDLK(INSERT)
	ESDL_SDLK(HOME)
	ESDL_SDLK(PAGEUP)
	ESDL_SDLK(DELETE)
	ESDL_SDLK(END)
	ESDL_SDLK(PAGEDOWN)
	ESDL_SDLK(RIGHT)
	ESDL_SDLK(LEFT)
	ESDL_SDLK(DOWN)
	ESDL_SDLK(UP)
	ESDL_SDLK(NUMLOCKCLEAR)
	ESDL_SDLK(KP_DIVIDE)
	ESDL_SDLK(KP_MULTIPLY)
	ESDL_SDLK(KP_MINUS)
	ESDL_SDLK(KP_PLUS)
	ESDL_SDLK(KP_ENTER)
	ESDL_SDLK(KP_1)
	ESDL_SDLK(KP_2)
	ESDL_SDLK(KP_3)
	ESDL_SDLK(KP_4)
	ESDL_SDLK(KP_5)
	ESDL_SDLK(KP_6)
	ESDL_SDLK(KP_7)
	ESDL_SDLK(KP_8)
	ESDL_SDLK(KP_9)
	ESDL_SDLK(KP_0)
	ESDL_SDLK(KP_PERIOD)
	ESDL_SDLK(APPLICATION)
	ESDL_SDLK(POWER)
	ESDL_SDLK(KP_EQUALS)
	ESDL_SDLK(F13)
	ESDL_SDLK(F14)
	ESDL_SDLK(F15)
	ESDL_SDLK(F16)
	ESDL_SDLK(F17)
	ESDL_SDLK(F18)
	ESDL_SDLK(F19)
	ESDL_SDLK(F20)
	ESDL_SDLK(F21)
	ESDL_SDLK(F22)
	ESDL_SDLK(F23)
	ESDL_SDLK(F24)
	ESDL_SDLK(EXECUTE)
	ESDL_SDLK(HELP)
	ESDL_SDLK(MENU)
	ESDL_SDLK(SELECT)
	ESDL_SDLK(STOP)
	ESDL_SDLK(AGAIN)
	ESDL_SDLK(UNDO)
	ESDL_SDLK(CUT)
	ESDL_SDLK(COPY)
	ESDL_SDLK(PASTE)
	ESDL_SDLK(FIND)
	ESDL_SDLK(MUTE)
	ESDL_SDLK(VOLUMEUP)
	ESDL_SDLK(VOLUMEDOWN)
	ESDL_SDLK(KP_COMMA)
	ESDL_SDLK(KP_EQUALSAS400)
	ESDL_SDLK(ALTERASE)
	ESDL_SDLK(SYSREQ)
	ESDL_SDLK(CANCEL)
	ESDL_SDLK(CLEAR)
	ESDL_SDLK(PRIOR)
	ESDL_SDLK(RETURN2)
	ESDL_SDLK(SEPARATOR)
	ESDL_SDLK(OUT)
	ESDL_SDLK(OPER)
	ESDL_SDLK(CLEARAGAIN)
	ESDL_SDLK(CRSEL)
	ESDL_SDLK(EXSEL)
	ESDL_SDLK(KP_00)
	ESDL_SDLK(KP_000)
	ESDL_SDLK(THOUSANDSSEPARATOR)
	ESDL_SDLK(DECIMALSEPARATOR)
	ESDL_SDLK(CURRENCYUNIT)
	ESDL_SDLK(CURRENCYSUBUNIT)
	ESDL_SDLK(KP_LEFTPAREN)
	ESDL_SDLK(KP_RIGHTPAREN)
	ESDL_SDLK(KP_LEFTBRACE)
	ESDL_SDLK(KP_RIGHTBRACE)
	ESDL_SDLK(KP_TAB)
	ESDL_SDLK(KP_BACKSPACE)
	ESDL_SDLK(KP_A)
	ESDL_SDLK(KP_B)
	ESDL_SDLK(KP_C)
	ESDL_SDLK(KP_D)
	ESDL_SDLK(KP_E)
	ESDL_SDLK(KP_F)
	ESDL_SDLK(KP_XOR)
	ESDL_SDLK(KP_POWER)
	ESDL_SDLK(KP_PERCENT)
	ESDL_SDLK(KP_LESS)
	ESDL_SDLK(KP_GREATER)
	ESDL_SDLK(KP_AMPERSAND)
	ESDL_SDLK(KP_DBLAMPERSAND)
	ESDL_SDLK(KP_VERTICALBAR)
	ESDL_SDLK(KP_DBLVERTICALBAR)
	ESDL_SDLK(KP_COLON)
	ESDL_SDLK(KP_HASH)
	ESDL_SDLK(KP_SPACE)
	ESDL_SDLK(KP_AT)
	ESDL_SDLK(KP_EXCLAM)
	ESDL_SDLK(KP_MEMSTORE)
	ESDL_SDLK(KP_MEMRECALL)
	ESDL_SDLK(KP_MEMCLEAR)
	ESDL_SDLK(KP_MEMADD)
	ESDL_SDLK(KP_MEMSUBTRACT)
	ESDL_SDLK(KP_MEMMULTIPLY)
	ESDL_SDLK(KP_MEMDIVIDE)
	ESDL_SDLK(KP_PLUSMINUS)
	ESDL_SDLK(KP_CLEAR)
	ESDL_SDLK(KP_CLEARENTRY)
	ESDL_SDLK(KP_BINARY)
	ESDL_SDLK(KP_OCTAL)
	ESDL_SDLK(KP_DECIMAL)
	ESDL_SDLK(KP_HEXADECIMAL)
	ESDL_SDLK(LCTRL)
	ESDL_SDLK(LSHIFT)
	ESDL_SDLK(LALT)
	ESDL_SDLK(LGUI)
	ESDL_SDLK(RCTRL)
	ESDL_SDLK(RSHIFT)
	ESDL_SDLK(RALT)
	ESDL_SDLK(RGUI)
	ESDL_SDLK(MODE)
	ESDL_SDLK(AUDIONEXT)
	ESDL_SDLK(AUDIOPREV)
	ESDL_SDLK(AUDIOSTOP)
	ESDL_SDLK(AUDIOPLAY)
	ESDL_SDLK(AUDIOMUTE)
	ESDL_SDLK(MEDIASELECT)
	ESDL_SDLK(WWW)
	ESDL_SDLK(MAIL)
	ESDL_SDLK(CALCULATOR)
	ESDL_SDLK(COMPUTER)
	ESDL_SDLK(AC_SEARCH)
	ESDL_SDLK(AC_HOME)
	ESDL_SDLK(AC_BACK)
	ESDL_SDLK(AC_FORWARD)
	ESDL_SDLK(AC_STOP)
	ESDL_SDLK(AC_REFRESH)
	ESDL_SDLK(AC_BOOKMARKS)
	ESDL_SDLK(BRIGHTNESSDOWN)
	ESDL_SDLK(BRIGHTNESSUP)
	ESDL_SDLK(DISPLAYSWITCH)
	ESDL_SDLK(KBDILLUMTOGGLE)
	ESDL_SDLK(KBDILLUMDOWN)
	ESDL_SDLK(KBDILLUMUP)
	ESDL_SDLK(EJECT)
	ESDL_SDLK(SLEEP)
#undef	ESDL_SDLK

	/* Keyboard modifiers (masks for or:ing) */
	{"KMOD_NONE",		KMOD_NONE},
	{"KMOD_SHIFT",		KMOD_SHIFT},
	{"KMOD_LSHIFT",		KMOD_LSHIFT},
	{"KMOD_RSHIFT",		KMOD_RSHIFT},
	{"KMOD_CTRL",		KMOD_CTRL},
	{"KMOD_LCTRL",		KMOD_LCTRL},
	{"KMOD_RCTRL",		KMOD_RCTRL},
	{"KMOD_ALT",		KMOD_ALT},
	{"KMOD_LALT",		KMOD_LALT},
	{"KMOD_RALT",		KMOD_RALT},
	{"KMOD_GUI",		KMOD_GUI},
	{"KMOD_LGUI",		KMOD_LGUI},
	{"KMOD_RGUI",		KMOD_RGUI},
	{"KMOD_NUM",		KMOD_NUM},
	{"KMOD_CAPS",		KMOD_CAPS},
	{"KMOD_MODE",		KMOD_MODE},
	{"KMOD_RESERVED",	KMOD_RESERVED},

	/* Button states */
	{"PRESSED",		SDL_PRESSED},
	{"RELEASED",		SDL_RELEASED},

	/* Mouse buttons */
	{"BUTTON_LEFT",		SDL_BUTTON_LEFT},
	{"BUTTON_MIDDLE",	SDL_BUTTON_MIDDLE},
	{"BUTTON_RIGHT",	SDL_BUTTON_RIGHT},

	/* Joystick hat positions */
	{"HAT_LEFTUP",		SDL_HAT_LEFTUP},
	{"HAT_UP",		SDL_HAT_UP},
	{"HAT_RIGHTUP",		SDL_HAT_RIGHTUP},
	{"HAT_LEFT",		SDL_HAT_LEFT},
	{"HAT_CENTERED",	SDL_HAT_CENTERED},
	{"HAT_RIGHT",		SDL_HAT_RIGHT},
	{"HAT_LEFTDOWN",	SDL_HAT_LEFTDOWN},
	{"HAT_DOWN",		SDL_HAT_DOWN},
	{"HAT_RIGHTDOWN",	SDL_HAT_RIGHTDOWN},

	/* SDL_EventState */
	{"IGNORE",	SDL_IGNORE},
	{"DISABLE",	SDL_DISABLE},
	{"ENABLE",	SDL_ENABLE},
	{"QUERY",	SDL_QUERY},

	{NULL, 0}
};


EEL_xno eel_sdl_init(EEL_vm *vm)
{
	EEL_object *c;
	EEL_object *m;

	if(loaded)
		return EEL_XDEVICEOPENED;

	m = eel_create_module(vm, "SDL", esdl_sdl_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	memset(&esdl_md, 0, sizeof(esdl_md));

	/* Types */
	c = eel_export_class(m, "Rect", EEL_COBJECT, r_construct, NULL, NULL);
	esdl_md.rect_cid = eel_class_cid(c);
	eel_set_metamethod(c, EEL_MM_GETINDEX, r_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, r_setindex);
	eel_set_casts(vm, esdl_md.rect_cid, esdl_md.rect_cid, r_clone);

	c = eel_export_class(m, "Window", EEL_COBJECT,
			w_construct, w_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, w_getindex);
	esdl_md.window_cid = eel_class_cid(c);

	c = eel_export_class(m, "Renderer", EEL_COBJECT,
			rn_construct, rn_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, rn_getindex);
	esdl_md.renderer_cid = eel_class_cid(c);

	c = eel_export_class(m, "GLContext", EEL_COBJECT,
			glc_construct, glc_destruct, NULL);
	esdl_md.glcontext_cid = eel_class_cid(c);

	c = eel_export_class(m, "Texture", EEL_COBJECT,
			tx_construct, tx_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, tx_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, tx_setindex);
	esdl_md.texture_cid = eel_class_cid(c);

	c = eel_export_class(m, "Surface", EEL_COBJECT,
			s_construct, s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, s_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, s_setindex);
	esdl_md.surface_cid = eel_class_cid(c);

	c = eel_export_class(m, "SurfaceLock", esdl_md.surface_cid,
			sl_construct, sl_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, sl_getindex);
	esdl_md.surfacelock_cid = eel_class_cid(c);

	c = eel_export_class(m, "Joystick", EEL_COBJECT,
			j_construct, j_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, j_getindex);
	esdl_md.joystick_cid = eel_class_cid(c);

	/* Versioning */
	eel_export_cfunction(m, 1, "HeaderVersion", 0, 0, 0,
			esdl_HeaderVersion);
	eel_export_cfunction(m, 1, "LinkedVersion", 0, 0, 0,
			esdl_LinkedVersion);

	/* Windows */
	eel_export_cfunction(m, 0, "SetWindowTitle", 2, 0, 0,
			esdl_SetWindowTitle);
	eel_export_cfunction(m, 0, "SetWindowGrab", 2, 0, 0,
			esdl_SetWindowGrab);
//TODO: Non-trivial, as we need to find the corresponding EEL Window object,
//	which may not exist, if the window was not created via EEL.
//	eel_export_cfunction(m, 1, "GetWindowGrab", 0, 0, 0,
//			esdl_GetWindowGrab);

	/* Rendering */
	eel_export_cfunction(m, 1, "RenderTargetSupported", 1, 0, 0,
			esdl_RenderTargetSupported);
	eel_export_cfunction(m, 0, "SetRenderTarget", 1, 1, 0,
			esdl_SetRenderTarget);
	eel_export_cfunction(m, 0, "SetRenderDrawColor", 2, 3, 0,
			esdl_SetRenderDrawColor);
	eel_export_cfunction(m, 0, "SetRenderDrawBlendMode", 2, 0, 0,
			esdl_SetRenderDrawBlendMode);
	eel_export_cfunction(m, 0, "RenderSetClipRect", 1, 1, 0, esdl_RenderSetClipRect);
	eel_export_cfunction(m, 0, "RenderDrawPoint", 2, 0, 1,
			esdl_RenderDrawPoint);
	eel_export_cfunction(m, 0, "RenderDrawLine", 2, 0, 1,
			esdl_RenderDrawLine);
	eel_export_cfunction(m, 0, "RenderClear", 1, 0, 0,
			esdl_RenderClear);
	eel_export_cfunction(m, 0, "RenderFillRect", 1, 4, 0,
			esdl_RenderFillRect);
	eel_export_cfunction(m, 0, "RenderCopy", 2, 5, 0,
			esdl_RenderCopy);
	eel_export_cfunction(m, 0, "RenderPresent", 1, 0, 0,
			esdl_RenderPresent);

	/* OpenGL support */
	eel_export_cfunction(m, 0, "GL_SetAttribute", 2, 0, 0,
			esdl_gl_setattribute);
	eel_export_cfunction(m, 0, "GL_SetSwapInterval", 1, 0, 0,
			esdl_gl_setswapinterval);
	eel_export_cfunction(m, 0, "GL_SwapWindow", 1, 0, 0,
			esdl_gl_swapwindow);

	/* Surfaces */
	eel_export_cfunction(m, 0, "SetClipRect", 0, 2, 0, esdl_SetClipRect);
	eel_export_cfunction(m, 0, "UpdateWindowSurface", 1, 1, 0,
			esdl_UpdateWindowSurface);
	eel_export_cfunction(m, 0, "BlitSurface", 3, 1, 0, esdl_BlitSurface);
	eel_export_cfunction(m, 0, "FillRect", 1, 2, 0, esdl_FillRect);
	eel_export_cfunction(m, 1, "LockSurface", 1, 0, 0, esdl_LockSurface);
	eel_export_cfunction(m, 0, "UnlockSurface", 1, 0, 0,
			esdl_UnlockSurface);
	eel_export_cfunction(m, 0, "SetSurfaceAlphaMod", 1, 1, 0,
			esdl_SetSurfaceAlphaMod);
	eel_export_cfunction(m, 0, "SetSurfaceColorMod", 1, 3, 0,
			esdl_SetSurfaceColorMod);
	eel_export_cfunction(m, 0, "SetColorKey", 3, 0, 0, esdl_SetColorKey);
	eel_export_cfunction(m, 1, "SetColors", 2, 1, 0, esdl_SetColors);

	/* Timing */
	eel_export_cfunction(m, 1, "GetTicks", 0, 0, 0, esdl_GetTicks);
	eel_export_cfunction(m, 0, "Delay", 1, 0, 0, esdl_Delay);

	/* Mouse control */
	eel_export_cfunction(m, 1, "ShowCursor", 1, 0, 0, esdl_ShowCursor);
	eel_export_cfunction(m, 0, "WarpMouse", 2, 1, 0, esdl_WarpMouse);

	/* Joystick input */
	eel_export_cfunction(m, 0, "DetectJoysticks", 0, 0, 0, esdl_DetectJoysticks);
	eel_export_cfunction(m, 1, "NumJoysticks", 0, 0, 0, esdl_NumJoysticks);
	eel_export_cfunction(m, 1, "JoystickName", 1, 0, 0, esdl_JoystickName);

	/* EEL SDL extensions */
	eel_export_cfunction(m, 1, "MapColor", 2, 3, 0, esdl_MapColor);
	eel_export_cfunction(m, 1, "GetColor", 2, 0, 0, esdl_GetColor);
	eel_export_cfunction(m, 0, "Plot", 2, 0, 2, esdl_Plot);
	eel_export_cfunction(m, 1, "GetPixel", 3, 1, 0, esdl_GetPixel);
	eel_export_cfunction(m, 1, "InterPixel", 3, 1, 0, esdl_InterPixel);
	eel_export_cfunction(m, 1, "FilterPixel", 4, 1, 0, esdl_FilterPixel);

	/* Event handling */
	eel_export_cfunction(m, 0, "PumpEvents", 0, 0, 0, esdl_PumpEvents);
	eel_export_cfunction(m, 1, "CheckEvent", 0, 0, 0, esdl_CheckEvent);
	eel_export_cfunction(m, 1, "PollEvent", 0, 0, 0, esdl_PollEvent);
	eel_export_cfunction(m, 1, "WaitEvent", 0, 0, 0, esdl_WaitEvent);
	eel_export_cfunction(m, 1, "EventState", 1, 1, 0, esdl_EventState);

	/* Keyboard input */
	eel_export_cfunction(m, 0, "StartTextInput", 0, 0, 0,
			esdl_StartTextInput);
	eel_export_cfunction(m, 0, "StopTextInput", 0, 0, 0,
			esdl_StopTextInput);
	eel_export_cfunction(m, 0, "SetTextInputRect", 0, 1, 0,
			esdl_SetTextInputRect);
	eel_export_cfunction(m, 1, "IsTextInputActive", 0, 0, 0,
			esdl_IsTextInputActive);
	eel_export_cfunction(m, 1, "GetScancodeName", 1, 0, 0,
			esdl_GetScancodeName);
	eel_export_cfunction(m, 1, "GetKeyName", 1, 0, 0,
			esdl_GetKeyName);

	/* Simple audio interface */
	eel_export_cfunction(m, 0, "OpenAudio", 3, 0, 0, esdl_OpenAudio);
	eel_export_cfunction(m, 0, "CloseAudio", 0, 0, 0, esdl_CloseAudio);
	eel_export_cfunction(m, 0, "PlayAudio", 1, 1, 0, esdl_PlayAudio);
	eel_export_cfunction(m, 1, "AudioPosition", 0, 0, 0, esdl_AudioPosition);
	eel_export_cfunction(m, 1, "AudioBuffer", 0, 0, 0, esdl_AudioBuffer);
	eel_export_cfunction(m, 1, "AudioSpace", 0, 0, 0, esdl_AudioSpace);

	/* Constants and enums */
	eel_export_lconstants(m, esdl_constants);

	loaded = 1;
	eel_disown(m);
	return 0;
}
