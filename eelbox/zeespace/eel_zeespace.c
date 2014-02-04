/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_zeespace.c - EEL ZeeSpace binding
---------------------------------------------------------------------------
 * Copyright (C) 2010-2011 David Olofson
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

#include "eel_zeespace.h"
#include "eb_sdl.h"

/*FIXME: Only one global ZeeSpace state per process for now... */
static int loaded = 0;

// For the surface constructor
#define EZS_WINDOW	0x7ee594c0
#define EZS_VIEW	0x7ee594c1

typedef struct
{
	/* Class Type IDs */
	int		pixel_cid;
	int		rect_cid;
	int		pipe_cid;
	int		region_cid;
	int		surface_cid;

	EEL_vm		*vm;
	ZS_State	*state;
} EZS_moduledata;

EZS_moduledata	zsmd;


/*----------------------------------------------------------
	Pixel class
----------------------------------------------------------*/
static EEL_xno zsp_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ZS_Pixel *p;
	EEL_object *eo = eel_o_alloc(vm, sizeof(ZS_Pixel), type);
	if(!eo)
		return EEL_XMEMORY;
	p = o2ZS_Pixel(eo);
	if(initc == 1)
	{
		ZS_Pixel *sp;
		if(initv->objref.v->type != zsmd.pixel_cid)
		{
			eel_o_free(eo);
			return EEL_XWRONGTYPE;
		}
		sp = o2ZS_Pixel(initv->objref.v);
		memcpy(p, sp, sizeof(ZS_Pixel));
	}
	else
	{
		memset(p, 0, sizeof(ZS_Pixel));
		switch(initc)
		{
		  case 6:
			p->z = (int)(eel_v2d(initv + 5) * 256.0f);
		  case 5:
			p->i = (int)(eel_v2d(initv + 4) * 256.0f);
		  case 4:
			p->a = eel_v2l(initv + 3);
		  case 3:
			p->b = eel_v2l(initv + 2);
			p->g = eel_v2l(initv + 1);
			p->r = eel_v2l(initv);
		  case 0:
			break;
		  default:
			eel_o_free(eo);
			return EEL_XARGUMENTS;
		}
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno zsp_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *co = eel_o_alloc(vm, sizeof(ZS_Pixel), t);
	if(!co)
		return EEL_XMEMORY;
	memcpy(o2ZS_Pixel(co), o2ZS_Pixel(src->objref.v), sizeof(ZS_Pixel));
	eel_o2v(dst, co);
	return 0;
}


static EEL_xno zsp_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ZS_Pixel *p = o2ZS_Pixel(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'r':
		eel_l2v(op2, p->r);
		break;
	  case 'g':
		eel_l2v(op2, p->g);
		break;
	  case 'b':
		eel_l2v(op2, p->b);
		break;
	  case 'a':
		eel_l2v(op2, p->a);
		break;
	  case 'i':
		eel_d2v(op2, p->i / 256.0f);
		break;
	  case 'z':
		eel_d2v(op2, p->z / 256.0f);
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	return 0;
}


static EEL_xno zsp_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ZS_Pixel *p = o2ZS_Pixel(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'r':
		p->r = eel_v2l(op2);
		break;
	  case 'g':
		p->g = eel_v2l(op2);
		break;
	  case 'b':
		p->b = eel_v2l(op2);
		break;
	  case 'a':
		p->a = eel_v2l(op2);
		break;
	  case 'i':
		p->i = (int)(eel_v2d(op2) * 256.0f);
		break;
	  case 'z':
		p->z = (int)(eel_v2d(op2) * 256.0f);
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	return 0;
}


/*----------------------------------------------------------
	Rect class
----------------------------------------------------------*/
static EEL_xno zsr_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	ZS_Rect *r;
	EEL_object *eo = eel_o_alloc(vm, sizeof(ZS_Rect), type);
	if(!eo)
		return EEL_XMEMORY;
	r = o2ZS_Rect(eo);
	if(!initc)
	{
		r->x = r->y = 0;
		r->w = r->h = 0;
	}
	else if(initc == 1)
	{
		ZS_Rect *sr;
		if(initv->objref.v->type != zsmd.rect_cid)
		{
			eel_o_free(eo);
			return EEL_XWRONGTYPE;
		}
		sr = o2ZS_Rect(initv->objref.v);
		memcpy(r, sr, sizeof(ZS_Rect));
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


static EEL_xno zsr_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_types t)
{
	EEL_object *co = eel_o_alloc(vm, sizeof(ZS_Rect), t);
	if(!co)
		return EEL_XMEMORY;
	memcpy(o2ZS_Rect(co), o2ZS_Rect(src->objref.v), sizeof(ZS_Rect));
	eel_o2v(dst, co);
	return 0;
}


static EEL_xno zsr_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ZS_Rect *r = o2ZS_Rect(eo);
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
	op2->type = EEL_TINTEGER;
	return 0;
}


static EEL_xno zsr_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ZS_Rect *r = o2ZS_Rect(eo);
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
	Surface class
----------------------------------------------------------*/
static EEL_xno zss_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EZS_surface *s;
	EEL_object *eo;
	switch(initc)
	{
	  case 2:
		// New surface
		break;
	  case 3:
		// Window or region
		if(EEL_TYPE(initv) != zsmd.surface_cid)
			return EEL_XWRONGTYPE;
		if(EEL_TYPE(initv + 1) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		break;
	  default:
		return EEL_XARGUMENTS;
	}
	eo = eel_o_alloc(vm, sizeof(EZS_surface), type);
	if(!eo)
		return EEL_XMEMORY;
	s = o2EZS_surface(eo);
	if(initc == 3)
	{
		EZS_surface *ss = o2EZS_surface(initv->objref.v);
		ZS_Rect *r = o2ZS_Rect(initv[1].objref.v);
		switch(eel_v2l(initv + 2))
		{
		  case EZS_WINDOW:
			s->surface = zs_Window(ss->surface, r);
			break;
		  case EZS_VIEW:
			s->surface = zs_View(ss->surface, r);
			break;
		  default:
			eel_o_free(eo);
			return EEL_XARGUMENTS;
		}
	}
	else
		s->surface = zs_NewSurface(zsmd.state,
				eel_v2l(initv), eel_v2l(initv + 1));
	if(!s->surface)
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno zss_destruct(EEL_object *eo)
{
	EZS_surface *s = o2EZS_surface(eo);
	if(s->surface)
		zs_FreeSurface(s->surface);
	return 0;
}


static EEL_xno zss_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EZS_surface *s = o2EZS_surface(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strlen(is) != 1)
		return EEL_XWRONGINDEX;
	switch(is[0])
	{
	  case 'x':
		op2->integer.v = s->surface->xo;
		break;
	  case 'y':
		op2->integer.v = s->surface->yo;
		break;
	  case 'w':
		op2->integer.v = s->surface->w;
		break;
	  case 'h':
		op2->integer.v = s->surface->h;
		break;
	  default:
		return EEL_XWRONGINDEX;
	}
	op2->type = EEL_TINTEGER;
	return 0;
}


/*----------------------------------------------------------
	Region class
----------------------------------------------------------*/
static EEL_xno zsrg_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EZS_region *rg;
	EEL_object *eo;
	if(initc != 0)
		return EEL_XARGUMENTS;
	eo = eel_o_alloc(vm, sizeof(EZS_region), type);
	if(!eo)
		return EEL_XMEMORY;
	rg = o2EZS_region(eo);
	rg->region = zs_NewRegion(0);
	if(!rg->region)
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno zsrg_destruct(EEL_object *eo)
{
	EZS_region *rg = o2EZS_region(eo);
	if(rg->region)
		zs_FreeRegion(rg->region);
	return 0;
}


/*----------------------------------------------------------
	Pipe class
----------------------------------------------------------*/
static EEL_xno zspp_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EZS_pipe *pp;
	EEL_object *eo;
	if(initc != 0)
		return EEL_XARGUMENTS;
	eo = eel_o_alloc(vm, sizeof(EZS_pipe), type);
	if(!eo)
		return EEL_XMEMORY;
	pp = o2EZS_pipe(eo);
	pp->pipe = zs_NewPipe(zsmd.state);
	if(!pp->pipe)
	{
		eel_o_free(eo);
		return EEL_XDEVICEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno zspp_destruct(EEL_object *eo)
{
	EZS_pipe *pp = o2EZS_pipe(eo);
	if(pp->pipe)
		zs_FreePipe(pp->pipe);
	return 0;
}


/*----------------------------------------------------------
	Pipe functions
----------------------------------------------------------*/

static EEL_xno ezs_pipez(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *pp;
	float z;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	pp = o2EZS_pipe(args->objref.v);
	if(vm->argc >= 3)
		z = eel_v2d(args + 2);
	else
		z = 0.0f;
	if(zs_PipeZ(pp->pipe, eel_v2l(args + 1), z) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}

static EEL_xno ezs_pipecolor(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *pp;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	pp = o2EZS_pipe(args->objref.v);
	if(zs_PipeColor(pp->pipe, eel_v2l(args + 1)) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}

static EEL_xno ezs_pipealpha(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *pp;
	float alpha;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		alpha = eel_v2d(args + 2);
	else
		alpha = 1.0f;
	pp = o2EZS_pipe(args->objref.v);
	if(zs_PipeAlpha(pp->pipe, eel_v2l(args + 1), alpha) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}

static EEL_xno ezs_pipeintensity(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *pp;
	float intensity;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		intensity = eel_v2d(args + 2);
	else
		intensity = 1.0f;
	pp = o2EZS_pipe(args->objref.v);
	if(zs_PipeIntensity(pp->pipe, eel_v2l(args + 1), intensity) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}

static EEL_xno ezs_pipewrite(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *pp;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	pp = o2EZS_pipe(args->objref.v);
	if(zs_PipeWrite(pp->pipe, eel_v2l(args + 1)) < 0)
		return EEL_XDEVICEERROR;
	return 0;
}


/*----------------------------------------------------------
	Pixel functions
----------------------------------------------------------*/

static EEL_xno ezs_getpixel(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Pixel *p, *sp;
	int x = eel_v2l(args + 1);
	int y = eel_v2l(args + 2);
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args->objref.v);
	if(vm->argc >= 4)
	{
		if(EEL_TYPE(args + 3) != zsmd.pixel_cid)
			return EEL_XWRONGTYPE;
		p = o2ZS_Pixel(eel_v2o(args + 3));
		sp = zs_PixelClip(s->surface, x, y);
		if(sp)
			memcpy(p, sp, sizeof(ZS_Pixel));
		else
			memset(p, 0, sizeof(ZS_Pixel));
		eel_nil2v(vm->heap + vm->resv);
	}
	else
	{
		EEL_xno xn;
		sp = zs_PixelClip(s->surface, x, y);
		if(!sp)
		{
			eel_nil2v(vm->heap + vm->resv);
			return 0;
		}
		xn = eel_o_construct(vm, zsmd.pixel_cid, NULL, 0,
				vm->heap + vm->resv);
		if(xn)
			return xn;
		p = o2ZS_Pixel(eel_v2o(vm->heap + vm->resv));
		memcpy(p, sp, sizeof(ZS_Pixel));
	}
	return 0;
}

static EEL_xno ezs_getpixelclamp(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Pixel *p, *sp;
	int x = eel_v2l(args + 1);
	int y = eel_v2l(args + 2);
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args->objref.v);
	if(vm->argc >= 4)
	{
		if(EEL_TYPE(args + 3) != zsmd.pixel_cid)
			return EEL_XWRONGTYPE;
		p = o2ZS_Pixel(eel_v2o(args + 3));
	}
	else
	{
		EEL_xno xn;
		sp = zs_PixelClamp(s->surface, x, y);
		xn = eel_o_construct(vm, zsmd.pixel_cid, NULL, 0,
				vm->heap + vm->resv);
		if(xn)
			return xn;
		p = o2ZS_Pixel(eel_v2o(vm->heap + vm->resv));
	}
	sp = zs_PixelClamp(s->surface, x, y);
	memcpy(p, sp, sizeof(ZS_Pixel));
	eel_nil2v(vm->heap + vm->resv);
	return 0;
}

static EEL_xno ezs_setpixel(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Pixel *p, *dp;
	int x = eel_v2l(args + 1);
	int y = eel_v2l(args + 2);
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args->objref.v);
	if(EEL_TYPE(args + 3) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	p = o2ZS_Pixel(eel_v2o(args + 3));
	dp = zs_PixelClip(s->surface, x, y);
	if(dp)
		memcpy(dp, p, sizeof(ZS_Pixel));
	return 0;
}


/*----------------------------------------------------------
	Pseudo-random number generator
----------------------------------------------------------*/

static EEL_xno ezs_rndreset(EEL_vm *vm)
{
	zs_RndReset(zsmd.state, eel_v2d(vm->heap + vm->argv));
	return 0;
}

static EEL_xno ezs_rndget(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv,
			zs_RndGet(zsmd.state, eel_v2l(vm->heap + vm->argv)));
	return 0;
}


/*----------------------------------------------------------
	Surface/Window/Region management
----------------------------------------------------------*/

static EEL_xno ezs_window(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value initv[3];
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	eel_o2v(initv, args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.rect_cid)
		return EEL_XWRONGTYPE;
	eel_o2v(initv + 1, args[1].objref.v);
	eel_l2v(initv + 2, EZS_WINDOW);
	return eel_o_construct(vm, zsmd.surface_cid, initv, 3,
			vm->heap + vm->resv);
}

static EEL_xno ezs_view(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value initv[3];
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	eel_o2v(initv, args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.rect_cid)
		return EEL_XWRONGTYPE;
	eel_o2v(initv + 1, args[1].objref.v);
	eel_l2v(initv + 2, EZS_VIEW);
	return eel_o_construct(vm, zsmd.surface_cid, initv, 3,
			vm->heap + vm->resv);
}


/*----------------------------------------------------------
	Raw rendering primitives (No pipelines!)
----------------------------------------------------------*/

static EEL_xno ezs_rawfill(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Rect *r;
	ZS_Pixel *p;

	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	
	if(EEL_TYPE(args + 2) == EEL_TNIL)
		r = NULL;
	else
	{
		if(EEL_TYPE(args + 2) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2ZS_Rect(args[2].objref.v);
	}

	if(EEL_TYPE(args + 3) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	p = o2ZS_Pixel(args[3].objref.v);

	zs_RawFill(eel_v2l(args), s->surface, r, p);
	return 0;
}

static EEL_xno ezs_rawcopy(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *src;
	EZS_surface *dst;
	ZS_Rect *srcr;

	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	src = o2EZS_surface(args[1].objref.v);

	if(EEL_TYPE(args + 2) == EEL_TNIL)
		srcr = NULL;
	else
	{
		if(EEL_TYPE(args + 2) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		srcr = o2ZS_Rect(args[2].objref.v);
	}

	if(EEL_TYPE(args + 3) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	dst = o2EZS_surface(args[3].objref.v);

	zs_RawCopy(eel_v2l(args), src->surface, srcr, dst->surface,
			eel_v2l(args + 4), eel_v2l(args + 5));
	return 0;
}


/*---------------------------------------------------------------
	Region functions
---------------------------------------------------------------*/

static EEL_xno ezs_regionrect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_region *r;
	ZS_Errors res;
	ZS_Operations op = ZS_UNION;
	if(EEL_TYPE(args) != zsmd.region_cid)
		return EEL_XWRONGTYPE;
	r = o2EZS_region(args[0].objref.v);
	if(vm->argc >= 6)
		op = eel_v2l(args + 5);
	res = zs_RegionRect(r->region, eel_v2d(args + 1), eel_v2d(args + 2),
			eel_v2d(args + 3), eel_v2d(args + 4), op);
	switch(res)
	{
	  case 0:
		return 0;
	  case ZS_EMEMORY:
		return EEL_XMEMORY;
	  case ZS_EILLEGAL:
		return EEL_XILLEGALOPERATION;
	  case ZS_ENOTIMPL:
		return EEL_XNOTIMPLEMENTED;
	  case ZS_EINTERNAL:
	  default:
		return EEL_XDEVICEERROR;
	}
}


/*---------------------------------------------------------------
	Region based rendering
---------------------------------------------------------------*/

static EEL_xno ezs_fill(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_region *r;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.region_cid)
		return EEL_XWRONGTYPE;
	r = o2EZS_region(args[1].objref.v);
	if(EEL_TYPE(args + 2) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[2].objref.v);
	if(EEL_TYPE(args + 3) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[3].objref.v);
	zs_Fill(p->pipe, r->region, s->surface, px);
	return 0;
}


/*----------------------------------------------------------
	2.5D rendering primitives (Using pipelines)
----------------------------------------------------------*/
static EEL_xno ezs_block(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 6) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[6].objref.v);
	zs_Block(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5), px);
	return 0;
}

static EEL_xno ezs_blit(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *src;
	EZS_surface *dst;
	ZS_Rect *r;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	src = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 2) == EEL_TNIL)
		r = NULL;
	else
	{
		if(EEL_TYPE(args + 2) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2ZS_Rect(args[2].objref.v);
	}
	if(EEL_TYPE(args + 3) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	dst = o2EZS_surface(args[3].objref.v);
	zs_Blit(p->pipe, src->surface, r, dst->surface,
			eel_v2d(args + 4), eel_v2d(args + 5));
	return 0;
}

static EEL_xno ezs_cylinder(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 5) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[5].objref.v);
	zs_Cylinder(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), px);
	return 0;
}

static EEL_xno ezs_cone(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 6) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[6].objref.v);
	zs_Cone(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5), px);
	return 0;
}

static EEL_xno ezs_beehive(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 6) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[6].objref.v);
	zs_Beehive(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5), px);
	return 0;
}

static EEL_xno ezs_dome(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 6) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[6].objref.v);
	zs_Dome(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5), px);
	return 0;
}

/*FIXME:*/
static EEL_xno ezs_domerect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_pipe *p;
	EZS_surface *s;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.pipe_cid)
		return EEL_XWRONGTYPE;
	p = o2EZS_pipe(args[0].objref.v);
	if(EEL_TYPE(args + 1) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[1].objref.v);
	if(EEL_TYPE(args + 6) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[6].objref.v);
	zs_DomeRect(p->pipe, s->surface, eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5), px);
	return 0;
}
/*FIXME:*/


/*---------------------------------------------------------------
	Inter-channel operations
---------------------------------------------------------------*/

static EEL_xno ezs_applyintensity(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	float rm, gm, bm;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	rm = gm = bm = 1.0f;
	switch(vm->argc)
	{
	  case 4:
		bm = eel_v2d(args + 3);
	  case 3:
		gm = eel_v2d(args + 2);
	  case 2:
		rm = eel_v2d(args + 1);
	}
	zs_ApplyIntensity(s->surface, rm, gm, bm);
	return 0;
}


/*----------------------------------------------------------
	SDL rendering utilities
----------------------------------------------------------*/
static EEL_xno ezs_blit2sdl(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *from;
	EB_surface *to;
	int blend = 0;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	from = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	to = o2EB_surface(args[1].objref.v);
	if(vm->argc >= 3)
		blend = eel_v2l(args + 2);
	zs_Blit2SDL(from->surface, to->surface, 0, 0, blend);
	return 0;
}

static EEL_xno ezs_render3d2sdl(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *from;
	EB_surface *to;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	from = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	to = o2EB_surface(args[1].objref.v);
	zs_3D2SDL(from->surface, to->surface, eel_v2d(args + 2));
	return 0;
}

static EEL_xno ezs_graph2sdl(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *from;
	EB_surface *to;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	from = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	to = o2EB_surface(args[1].objref.v);
	zs_Graph2SDL(from->surface, to->surface, eel_v2l(args + 2));
	return 0;
}

static EEL_xno ezs_z2sdl(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *from;
	EB_surface *to;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	from = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	to = o2EB_surface(args[1].objref.v);
	zs_Z2SDL(from->surface, to->surface, eel_v2l(args + 2));
	return 0;
}

static EEL_xno ezs_i2sdl(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *from;
	EB_surface *to;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	from = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	to = o2EB_surface(args[1].objref.v);
	zs_I2SDL(from->surface, to->surface);
	return 0;
}



/*----------------------------------------------------------
	Lighting tools
----------------------------------------------------------*/
static EEL_xno ezs_simpleshadow(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	zs_SimpleShadow(s->surface, eel_v2d(args + 1), eel_v2d(args + 2),
			eel_v2d(args + 3), eel_v2d(args + 4));
	return 0;
}

static EEL_xno ezs_bumpmap(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	zs_BumpMap(s->surface, eel_v2d(args + 1), eel_v2d(args + 2),
			eel_v2d(args + 3));
	return 0;
}


/*----------------------------------------------------------
	Terrain tools
----------------------------------------------------------*/
static EEL_xno ezs_perlinterrainz(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	long namplitudes;
	float *amplitudes;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 6) == EEL_TNIL)
	{
		namplitudes = 0;
		amplitudes = NULL;
	}
	else
	{
		EEL_object *o;
		if(!EEL_IS_OBJREF(args[6].type))
			return EEL_XWRONGTYPE;
		o = args[6].objref.v;
		namplitudes = eel_length(o);
		if(namplitudes < 0)
			return EEL_XARGUMENTS;
		if(namplitudes == 0)
			amplitudes = NULL;
		else
		{
			int i;
			amplitudes = (float *)malloc(namplitudes * sizeof(float));
			if(!amplitudes)
				return EEL_XMEMORY;
			for(i = 0; i < namplitudes; ++i)
			{
				EEL_value v;
				EEL_xno x;
				x = eel_getlindex(o, i, &v);
				if(x)
				{
					free(amplitudes);
					return x;
				}
				amplitudes[i] = eel_v2d(&v);
			}
		}
	}
	zs_PerlinTerrainZ(s->surface, eel_v2d(args + 1),
			eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5),
			namplitudes, amplitudes, eel_v2l(args + 7));
	free(amplitudes);
	return 0;
}

static EEL_xno ezs_waterz(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Rect *r;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) == EEL_TNIL)
		r = NULL;
	else
	{
		if(EEL_TYPE(args + 1) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2ZS_Rect(args[1].objref.v);
	}
	if(EEL_TYPE(args + 2) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[2].objref.v);
	zs_WaterZ(s->surface, r, px, eel_v2l(args + 3));
	return 0;
}

static EEL_xno ezs_fog(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EZS_surface *s;
	ZS_Rect *r;
	ZS_Pixel *px;
	if(EEL_TYPE(args) != zsmd.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EZS_surface(args[0].objref.v);
	if(EEL_TYPE(args + 1) == EEL_TNIL)
		r = NULL;
	else
	{
		if(EEL_TYPE(args + 1) != zsmd.rect_cid)
			return EEL_XWRONGTYPE;
		r = o2ZS_Rect(args[1].objref.v);
	}
	if(EEL_TYPE(args + 2) != zsmd.pixel_cid)
		return EEL_XWRONGTYPE;
	px = o2ZS_Pixel(args[2].objref.v);
	zs_Fog(s->surface, r, px, eel_v2l(args + 3));
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno ezs_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		zs_Close(zsmd.state);
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp ezs_constants[] =
{
	/* Flags for the Raw*() calls */
	{"R",			ZS_CH_R			},
	{"G",			ZS_CH_G			},
	{"B",			ZS_CH_B			},
	{"A",			ZS_CH_A			},
	{"I",			ZS_CH_I			},
	{"Z",			ZS_CH_Z			},
	{"RGB",			ZS_CH_RGB		},
	{"RGBA",		ZS_CH_RGBA		},
	{"IZ",			ZS_CH_IZ		},
	{"ALL",			ZS_CH_ALL		},

	/* Perlin terrain generator flags */
	{"NEAREST",		ZS_PT_NEAREST		},
	{"BILINEAR",		ZS_PT_BILINEAR		},
	{"BICUBIC",		ZS_PT_BICUBIC		},
	{"ADAPTIVE",		ZS_PT_ADAPTIVE		},

	{"STOP",		ZS_PT_STOP		},
	{"EXTEND",		ZS_PT_EXTEND		},
	{"DIV1P5",		ZS_PT_DIV1P5		},
	{"DIV2",		ZS_PT_DIV2		},
	{"DIV3",		ZS_PT_DIV3		},
	{"DIV4",		ZS_PT_DIV4		},

	{"T_LINEAR",		ZS_PT_T_LINEAR		},
	{"T_CUBIC",		ZS_PT_T_CUBIC		},
	{"T_PCUBIC",		ZS_PT_T_PCUBIC		},
	{"T_NCUBIC",		ZS_PT_T_NCUBIC		},

	{"O_REPLACE",		ZS_PT_O_REPLACE		},
	{"O_ADD",		ZS_PT_O_ADD		},
	{"O_SUB",		ZS_PT_O_SUB		},
	{"O_MUL",		ZS_PT_O_MUL		},

	/* Pipeline modes */
	{"Z_NORMAL",		ZS_Z_NORMAL		},
	{"Z_ADD", 		ZS_Z_ADD 		},
	{"Z_SUBTRACT",		ZS_Z_SUBTRACT		},
	{"Z_MULTIPLY",		ZS_Z_MULTIPLY		},
	{"ZB_ALL", 		ZS_ZB_ALL 		},
	{"ZB_ABOVE", 		ZS_ZB_ABOVE 		},
	{"ZB_BELOW", 		ZS_ZB_BELOW 		},
	{"C_NORMAL",		ZS_C_NORMAL		},
	{"C_ADD",		ZS_C_ADD		},
	{"C_SUBTRACT",		ZS_C_SUBTRACT		},
	{"C_MULTIPLY",		ZS_C_MULTIPLY		},
	{"C_DIVIDE",		ZS_C_DIVIDE		},
	{"C_LIGHTEN",		ZS_C_LIGHTEN		},
	{"C_DARKEN",		ZS_C_DARKEN		},
	{"C_CONTRAST",		ZS_C_CONTRAST		},
	{"C_INVERT",		ZS_C_INVERT		},
	{"C_HUE",		ZS_C_HUE		},
	{"C_SATURATE",		ZS_C_SATURATE		},
	{"C_COLOR",		ZS_C_COLOR		},
	{"C_VALUE",		ZS_C_VALUE		},
	{"A_FIXED",		ZS_A_FIXED		},
	{"A_SOURCE",		ZS_A_SOURCE		},
	{"A_DEST",		ZS_A_DEST		},
	{"I_FIXED",		ZS_I_FIXED		},
	{"I_SOURCE",		ZS_I_SOURCE		},
	{"I_DEST",		ZS_I_DEST		},
	{"CW_R",		ZS_CW_R			},
	{"CW_G",		ZS_CW_G			},
	{"CW_B",		ZS_CW_B			},
	{"CW_RGB",		ZS_CW_RGB		},
	{"AW_OFF",		ZS_AW_OFF		},
	{"AW_COPY",		ZS_AW_COPY		},
	{"AW_ADD",		ZS_AW_ADD		},
	{"AW_SUBTRACT",		ZS_AW_SUBTRACT		},
	{"AW_MULTIPLY",		ZS_AW_MULTIPLY		},
	{"IW_OFF",		ZS_IW_OFF		},
	{"IW_COPY",		ZS_IW_COPY		},
	{"IW_ADD",		ZS_IW_ADD		},
	{"IW_SUBTRACT",		ZS_IW_SUBTRACT		},
	{"IW_MULTIPLY",		ZS_IW_MULTIPLY		},
	{"IW_BLEND",		ZS_IW_BLEND		},
	{"ZW_OFF", 		ZS_ZW_OFF 		},
	{"ZW_ON", 		ZS_ZW_ON 		},

	/* Region boolean operators */
	{"UNION", 		ZS_UNION 		},
	{"DIFFERENCE", 		ZS_DIFFERENCE 		},
	{"INTERSECTION", 	ZS_INTERSECTION		},

	{NULL, 0}
};

EEL_xno eel_zeespace_init(EEL_vm *vm)
{
	EEL_object *c;
	EEL_object *m;

	if(loaded)
		return EEL_XDEVICEOPENED;

	memset(&zsmd, 0, sizeof(zsmd));
	zsmd.vm = vm;

	zsmd.state = zs_Open();
	if(!zsmd.state)
		return EEL_XDEVICEOPEN;

	m = eel_create_module(vm, "ZeeSpace", ezs_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	c = eel_export_class(m, "ZS_Pixel", EEL_COBJECT, zsp_construct, NULL, NULL);
	zsmd.pixel_cid = eel_class_typeid(c);
	eel_set_metamethod(c, EEL_MM_GETINDEX, zsp_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, zsp_setindex);
	eel_set_casts(vm, zsmd.pixel_cid, zsmd.pixel_cid, zsp_clone);

	c = eel_export_class(m, "ZS_Rect", EEL_COBJECT, zsr_construct, NULL, NULL);
	zsmd.rect_cid = eel_class_typeid(c);
	eel_set_metamethod(c, EEL_MM_GETINDEX, zsr_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, zsr_setindex);
	eel_set_casts(vm, zsmd.rect_cid, zsmd.rect_cid, zsr_clone);

	c = eel_export_class(m, "ZS_Surface", EEL_COBJECT,
			zss_construct, zss_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, zss_getindex);
	zsmd.surface_cid = eel_class_typeid(c);

	c = eel_export_class(m, "ZS_Region", EEL_COBJECT,
			zsrg_construct, zsrg_destruct, NULL);
	zsmd.region_cid = eel_class_typeid(c);
	
	c = eel_export_class(m, "ZS_Pipe", EEL_COBJECT,
			zspp_construct, zspp_destruct, NULL);
	zsmd.pipe_cid = eel_class_typeid(c);

	/* Region functions */
	eel_export_cfunction(m, 0, "RegionRect", 5, 1, 0, ezs_regionrect);

	/* Pipe functions */
	eel_export_cfunction(m, 0, "PipeZ", 2, 1, 0, ezs_pipez);
	eel_export_cfunction(m, 0, "PipeColor", 2, 0, 0, ezs_pipecolor);
	eel_export_cfunction(m, 0, "PipeAlpha", 2, 1, 0, ezs_pipealpha);
	eel_export_cfunction(m, 0, "PipeIntensity", 2, 1, 0, ezs_pipeintensity);
	eel_export_cfunction(m, 0, "PipeWrite", 2, 0, 0, ezs_pipewrite);

	/* Pixel functions */
	eel_export_cfunction(m, 1, "GetPixel", 3, 1, 0, ezs_getpixel);
	eel_export_cfunction(m, 1, "GetPixelClamp", 3, 1, 0, ezs_getpixelclamp);
	eel_export_cfunction(m, 0, "SetPixel", 4, 0, 0, ezs_setpixel);

	/* Pseudo-random number generator */
	eel_export_cfunction(m, 0, "RndReset", 1, 0, 0, ezs_rndreset);
	eel_export_cfunction(m, 1, "RndGet", 1, 0, 0, ezs_rndget);

	/* Surface/Window/Region management */
	eel_export_cfunction(m, 1, "Window", 2, 0, 0, ezs_window);
	eel_export_cfunction(m, 1, "View", 2, 0, 0, ezs_view);

	/* Raw rendering primitives (No pipelines!) */
	eel_export_cfunction(m, 0, "RawFill", 4, 0, 0, ezs_rawfill);
	eel_export_cfunction(m, 0, "RawCopy", 6, 0, 0, ezs_rawcopy);

	/* 2.5D rendering primitives (Using pipelines) */
	eel_export_cfunction(m, 0, "Block", 7, 0, 0, ezs_block);
	eel_export_cfunction(m, 0, "Blit", 6, 0, 0, ezs_blit);
	eel_export_cfunction(m, 0, "Cylinder", 6, 0, 0, ezs_cylinder);
	eel_export_cfunction(m, 0, "Cone", 7, 0, 0, ezs_cone);
	eel_export_cfunction(m, 0, "Beehive", 7, 0, 0, ezs_beehive);
	eel_export_cfunction(m, 0, "Dome", 7, 0, 0, ezs_dome);
/*FIXME:*/
eel_export_cfunction(m, 0, "DomeRect", 7, 0, 0, ezs_domerect);

	/* Region based rendering */
	eel_export_cfunction(m, 0, "Fill", 4, 0, 0, ezs_fill);

	/* Inter-channel operations */
	eel_export_cfunction(m, 0, "ApplyIntensity", 1, 3, 0, ezs_applyintensity);

	/* SDL rendering utilities */
	eel_export_cfunction(m, 0, "Blit2SDL", 2, 1, 0, ezs_blit2sdl);
	eel_export_cfunction(m, 0, "Render3D2SDL", 3, 0, 0, ezs_render3d2sdl);
	eel_export_cfunction(m, 0, "Graph2SDL", 3, 0, 0, ezs_graph2sdl);
	eel_export_cfunction(m, 0, "Z2SDL", 3, 0, 0, ezs_z2sdl);
	eel_export_cfunction(m, 0, "I2SDL", 2, 0, 0, ezs_i2sdl);

	/* Lighting tools */
	eel_export_cfunction(m, 0, "SimpleShadow", 5, 0, 0, ezs_simpleshadow);
	eel_export_cfunction(m, 0, "BumpMap", 4, 0, 0, ezs_bumpmap);

	/* Terrain tools */
	eel_export_cfunction(m, 0, "PerlinTerrainZ", 8, 0, 0, ezs_perlinterrainz);
	eel_export_cfunction(m, 0, "WaterZ", 4, 0, 0, ezs_waterz);
	eel_export_cfunction(m, 0, "Fog", 4, 0, 0, ezs_fog);

	/* Constants and enums */
	eel_export_lconstants(m, ezs_constants);

	loaded = 1;
	eel_disown(m);
	return EEL_XNONE;
}
