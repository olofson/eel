/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_physics.c - EEL 2D Physics
---------------------------------------------------------------------------
 * Copyright (C) 2011-2012 David Olofson
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

#include <stdio.h>


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "eel_physics.h"
#include "EEL_object.h"
#include "../src/e_vector.h"

typedef struct
{
	/* Class type IDs */
	int		space_cid;
	int		body_cid;
	int		constraint_cid;

	/* Private string lookup tables */
	EEL_object	*spacefields;
	EEL_object	*bodyfields;
	EEL_object	*constraintfields;

	EEL_value	cx;
	EEL_value	cy;
	EEL_value	ca;
} EPH_moduledata;

/*FIXME: Can't get moduledata from within constructors... */
static EPH_moduledata eph_md;


static inline int is_pot(unsigned x)
{
	return ((x != 0) && !(x & (x - 1)));
}

/* NOTE: Input must be a power of two! */
static const int bitpos[32] = 
{
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
static inline int bit_index(unsigned x)
{
	return bitpos[(EEL_uint32)(x * 0x077CB531U) >> 27];
}

static inline EEL_xno kill_body(EPH_body *body, int warndead)
{
	if(body->killed)
	{
#if 0
/*
FIXME: I'm seeing "too many" of these after the native constraints update.
FIXME: Might be something to look into...
 */
		if(warndead)
			printf("WARNING: Tried to kill dead body %p!\n", body);
#endif
		return 0;
	}
	if(body->space)
	{
		--body->space->active;
		body->space->clean_constraints = 1;
	}
	body->killed = 1;
	if(body->methods[EPH_CLEANUP])
	{
		EEL_object *bo = EPH_body2o(body);
		return eel_callf(bo->vm, body->methods[EPH_CLEANUP], "o", bo);
	}
	else
		return 0;
}


/*----------------------------------------------------------
	space class
----------------------------------------------------------*/

static EEL_xno space_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EPH_space *space;
	EEL_value v;
	EEL_xno xno;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EPH_space), type);
	if(!eo)
		return EEL_XMEMORY;
	space = o2EPH_space(eo);
	memset(space, 0, sizeof(EPH_space));
	xno = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(xno)
		return xno;
	space->table = eel_v2o(&v);
	space->vxmin = space->vymin = -1e10f;
	space->vxmax = space->vymax = 1e10f;
	space->integration = EPH_VERLET;
	space->outputmode = EPH_SEMIEXTRA;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno space_destruct(EEL_object *eo)
{
	EPH_space *space = o2EPH_space(eo);
	EPH_body *body;
	while(space->firstc)
		eph_UnlinkConstraint(space, space->firstc);
	/* Disable APIs that need 'space'! */
	for(body = space->first; body; body = body->next)
		body->space = NULL;
	body = space->first;
	while(body)
	{
		EPH_body *nb = body->next;
		eph_UnlinkBody(space, body);
		kill_body(body, 0);
		eel_disown(EPH_body2o(body));
		body = nb;
	}
	eel_disown(space->table);
	space->table = NULL;
	return 0;
}


static EEL_xno space_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EPH_space *space = o2EPH_space(eo);
	if(eel_table_get(eph_md.spacefields, op1, &v) != EEL_XNONE)
	{
		/* No hit! Fall through to extension table. */
		EEL_xno x = eel_table_get(space->table, op1, op2);
		if(x)
			return x;
		eel_v_own(op2);
		return 0;
	}
	switch(v.integer.v)
	{
	  case EPH_SOFFMAPZ:	eel_l2v(op2, space->zmap.off);	return 0;
	  case EPH_SZMAPSCALE:	eel_d2v(op2, space->zmap.scale);return 0;
	  case EPH_SVX:		eel_d2v(op2, space->vx);	return 0;
	  case EPH_SVY:		eel_d2v(op2, space->vy);	return 0;
	  case EPH_SVXMIN:	eel_d2v(op2, space->vxmin);	return 0;
	  case EPH_SVYMIN:	eel_d2v(op2, space->vymin);	return 0;
	  case EPH_SVXMAX:	eel_d2v(op2, space->vxmax);	return 0;
	  case EPH_SVYMAX:	eel_d2v(op2, space->vymax);	return 0;
	  case EPH_SACTIVE:	eel_l2v(op2, space->active);	return 0;
	  case EPH_SCONSTRAINTS:eel_l2v(op2, space->constraints);return 0;
	  case EPH_SINTEGRATION:eel_l2v(op2, space->integration);return 0;
	  case EPH_SOUTPUTMODE:	eel_l2v(op2, space->outputmode);return 0;

	  case EPH_SIMPACT_DAMAGE_MIN:
			eel_d2v(op2, space->impact_damage_min);	return 0;
	  case EPH_SOVERLAP_CORRECT_MAX:
			eel_d2v(op2, space->overlap_correct_max);return 0;

	  case EPH_SIMPACT_ABSORB:
			eel_d2v(op2, space->impact_absorb);	return 0;
	  case EPH_SIMPACT_RETURN:
			eel_d2v(op2, space->impact_return);	return 0;
	  case EPH_SIMPACT_DAMAGE:
			eel_d2v(op2, space->impact_damage);	return 0;
	  case EPH_SOVERLAP_CORRECT:
			eel_d2v(op2, space->overlap_correct);	return 0;

	  case EPH_SIMPACT_ABSORB_Z:
			eel_d2v(op2, space->impact_absorb_z);	return 0;
	  case EPH_SIMPACT_RETURN_Z:
			eel_d2v(op2, space->impact_return_z);	return 0;
	  case EPH_SIMPACT_DAMAGE_Z:
			eel_d2v(op2, space->impact_damage_z);	return 0;
	  case EPH_SOVERLAP_CORRECT_Z:
			eel_d2v(op2, space->overlap_correct_z);	return 0;
	}
	return EEL_XWRONGINDEX;
}


static EEL_xno space_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EPH_space *space = o2EPH_space(eo);
	if(eel_table_get(eph_md.spacefields, op1, &v) != EEL_XNONE)
		return eel_o_metamethod(space->table, EEL_MM_SETINDEX, op1, op2);
	switch(v.integer.v)
	{
	  case EPH_SOFFMAPZ:	space->zmap.off = eel_v2l(op2);	return 0;
	  case EPH_SZMAPSCALE:	space->zmap.scale = eel_v2d(op2);return 0;
	  case EPH_SVX:		space->vx = eel_v2d(op2);	return 0;
	  case EPH_SVY:		space->vy = eel_v2d(op2);	return 0;
	  case EPH_SVXMIN:	space->vxmin = eel_v2d(op2);	return 0;
	  case EPH_SVYMIN:	space->vymin = eel_v2d(op2);	return 0;
	  case EPH_SVXMAX:	space->vxmax = eel_v2d(op2);	return 0;
	  case EPH_SVYMAX:	space->vymax = eel_v2d(op2);	return 0;
	  case EPH_SACTIVE:	return EEL_XCANTWRITE;
	  case EPH_SCONSTRAINTS:return EEL_XCANTWRITE;
	  case EPH_SINTEGRATION:space->integration = eel_v2l(op2);return 0;
	  case EPH_SOUTPUTMODE:	space->outputmode = eel_v2l(op2);return 0;

	  case EPH_SIMPACT_DAMAGE_MIN:
			space->impact_damage_min = eel_v2d(op2); return 0;
	  case EPH_SOVERLAP_CORRECT_MAX:
			space->overlap_correct_max = eel_v2d(op2); return 0;

	  case EPH_SIMPACT_ABSORB:
			space->impact_absorb = eel_v2d(op2);	return 0;
	  case EPH_SIMPACT_RETURN:
			space->impact_return = eel_v2d(op2);	return 0;
	  case EPH_SIMPACT_DAMAGE:
			space->impact_damage = eel_v2d(op2);	return 0;
	  case EPH_SOVERLAP_CORRECT:
			space->overlap_correct = eel_v2d(op2);	return 0;

	  case EPH_SIMPACT_ABSORB_Z:
			space->impact_absorb_z = eel_v2d(op2);	return 0;
	  case EPH_SIMPACT_RETURN_Z:
			space->impact_return_z = eel_v2d(op2);	return 0;
	  case EPH_SIMPACT_DAMAGE_Z:
			space->impact_damage_z = eel_v2d(op2);	return 0;
	  case EPH_SOVERLAP_CORRECT_Z:
			space->overlap_correct_z = eel_v2d(op2); return 0;
	}
	return EEL_XWRONGINDEX;
}


static EEL_xno eph_initz(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_zmap *zmap;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	zmap = &space->zmap;
	zmap->w = eel_v2l(args + 1);
	zmap->h = eel_v2l(args + 2);
	free(zmap->data);
	zmap->data = calloc(zmap->w, zmap->h);
	if(!zmap->data)
		return EEL_XMEMORY;
	zmap->scale = eel_v2d(args + 3);
	if(zmap->scale)
		zmap->scale = 1.0f / zmap->scale;
	else
		zmap->scale = 1.0f;
	return 0;
}


static EEL_xno eph_setz(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	return eph_setz_raw(&space->zmap, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3));
}


static EEL_xno eph_getz(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_zmap *zmap;
	float x = eel_v2d(args + 1);
	float y = eel_v2d(args + 2);
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	zmap = &space->zmap;
	eel_d2v(vm->heap + vm->resv, eph_getzi_raw(zmap,
			x * zmap->scale, zmap->h - y * zmap->scale));
	return 0;
}


/*
 * function TestLineZ(space, z, xa, ya, xb, yb, miss)
 *
 * Check the line (xa, ya)-(xb, yb) for Z map collisions. Returns distance from
 * (x0, y0) to point of collision, or 'miss' if there's no collision.
 */
static EEL_xno eph_testlinez(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_zmap *zmap;
	float z, xa, ya, xb, yb;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	zmap = &space->zmap;
	z = eel_v2d(args + 1);
	xa = eel_v2d(args + 2) * zmap->scale;
	ya = eel_v2d(args + 3) * zmap->scale;
	xb = eel_v2d(args + 4) * zmap->scale;
	yb = eel_v2d(args + 5) * zmap->scale;
	if(fabsf(xa - xb) > fabsf(ya - yb))
	{
		// Horizontal scan
		float x, y, dy;
		if(fabsf(xa - xb) > 1.0f)
			dy = (ya - yb) / (xa - xb);
		else
			dy = 0;
		y = ya;
		if(xb > xa)
			for(x = xa; x < xb; x += 1.0f, y += dy)
			{
				float zz = eph_getzi_raw(zmap, x, zmap->h - y);
				if(zz >= z)
				{
					x -= xa;
					y -= ya;
					eel_d2v(vm->heap + vm->resv,
							sqrt(x*x + y*y) /
							zmap->scale);
					return 0;
				}
			}
		else
			for(x = xa; x > xb; x -= 1.0f, y -= dy)
			{
				float zz = eph_getzi_raw(zmap, x, zmap->h - y);
				if(zz >= z)
				{
					x -= xa;
					y -= ya;
					eel_d2v(vm->heap + vm->resv,
							sqrt(x*x + y*y) /
							zmap->scale);
					return 0;
				}
			}
	}
	else
	{
		// Vertical scan
		float x, y, dx;
		if(fabsf(ya - yb) > 1.0f)
			dx = (xa - xb) / (ya - yb);
		else
			dx = 0;
		x = xa;
		if(yb > ya)
			for(y = ya; y < yb; y += 1.0f, x += dx)
			{
				float zz = eph_getzi_raw(zmap, x, zmap->h - y);
				if(zz >= z)
				{
					x -= xa;
					y -= ya;
					eel_d2v(vm->heap + vm->resv,
							sqrt(x*x + y*y) /
							zmap->scale);
					return 0;
				}
			}
		else
			for(y = ya; y > yb; y -= 1.0f, x -= dx)
			{
				float zz = eph_getzi_raw(zmap, x, zmap->h - y);
				if(zz >= z)
				{
					x -= xa;
					y -= ya;
					eel_d2v(vm->heap + vm->resv,
							sqrt(x*x + y*y) /
							zmap->scale);
					return 0;
				}
			}
	}
	eel_copy(vm->heap + vm->resv, args + 6);
	return 0;
}


/*----------------------------------------------------------
	body class
----------------------------------------------------------*/

static inline void advance(EPH_body *body)
{
	int i;
	EPH_f tmp[EPH_DIMS];
	EPH_f a[EPH_DIMS];
	if(body->killed)
		return;

	/* Shift coordinates for interpolation */
	memcpy(body->o, body->c, sizeof(body->o));

	/* Apply forces to acceleration */
	for(i = 0; i < EPH_DIMS; ++i)
		a[i] = (body->f[i] + body->rf[i]) * body->m1[i];

	/*
	 * Apply frictional forces to acceleration
	 * NOTE:
	 *	We assume that friction changes linearly with
	 *	mass, so mass is ignored here!
	 */
	for(i = 0; i < EPH_DIMS; ++i)
		a[i] -= body->v[i] * body->u[i];

	/* Apply drag forces to acceleration */
	for(i = 0; i < EPH_DIMS; ++i)
		a[i] -= body->v[i] * fabs(body->v[i]) * body->cd[i];

	/* Backup velocities */
	if(body->space->integration == EPH_VERLET)
		memcpy(tmp, body->v, sizeof(tmp));

	/* Apply acceleration to velocity */
	for(i = 0; i < EPH_DIMS; ++i)
	{
		body->v[i] += a[i];
		if(body->v[i] > EPH_MAX_VELOCITY)
			body->v[i] = EPH_MAX_VELOCITY;
		else if(body->v[i] < -EPH_MAX_VELOCITY)
			body->v[i] = -EPH_MAX_VELOCITY;
	}

	/* Apply velocity to position */
	if(body->space->integration == EPH_VERLET)
		for(i = 0; i < EPH_DIMS; ++i)
			body->c[i] += (body->v[i] + tmp[i]) * 0.5f;
	else
		for(i = 0; i < EPH_DIMS; ++i)
			body->c[i] += body->v[i];

#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
	/* EPH_IMPULSEFORCES ==> forces reset after being applied once */
	if(body->flags & EPH_IMPULSEFORCES)
#endif
		for(i = 0; i < EPH_DIMS; ++i)
			body->f[i] = 0.0f;

	/* Response forces are always impulses here */
	for(i = 0; i < EPH_DIMS; ++i)
		body->rf[i] = 0.0f;
}


static inline void om_latest(EPH_body *body)
{
	memcpy(body->i, body->c, sizeof(body->i));
}


static inline void om_tween(EPH_body *body, float weight)
{
	int i;
	float w1 = 1.0f - weight;
	for(i = 0; i < EPH_DIMS; ++i)
		body->i[i] = body->o[i] * w1 + body->c[i] * weight;
}


static inline void om_extrapolate(EPH_body *body, float weight)
{
	int i;
	for(i = 0; i < EPH_DIMS; ++i)
		body->i[i] = body->c[i] + body->v[i] * weight;
}


static inline void om_semiextra(EPH_body *body, float weight)
{
	int i;
	float w1 = 1.0f - weight;
	for(i = 0; i < EPH_DIMS; ++i)
		body->i[i] = (body->c[i] + body->v[i] * weight +
				body->o[i] * w1 + body->c[i] * weight) * .5f;
}


static EEL_xno eph_advance(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_TYPE(arg) == eph_md.body_cid)
	{
		EPH_body *body = o2EPH_body(arg->objref.v);
		if(!body->space)
			return EEL_XDEVICECLOSED;
		advance(body);
		return 0;
	}
	else if(EEL_TYPE(arg) == eph_md.space_cid)
	{
		EPH_space *space = o2EPH_space(arg->objref.v);
		EPH_body *body;
#if EPH_DOMAIN_CHECKS == 1
		for(body = space->first; body; body = body->next)
		{
			int i;
			advance(body);
			for(i = 0; i < EPH_DIMS; ++i)
			{
				if(!isfinite(body->v[i]))
				{
					printf("eph_advance(): DOMAIN ERROR: "
							"v%c = %f\n", 'x' + i,
							body->v[i]);
					return EEL_XDOMAIN;
				}
				if(!isfinite(body->c[i]))
				{
					printf("eph_advance(): DOMAIN ERROR: "
							"c%c = %f\n", 'x' + i,
							body->c[i]);
					return EEL_XDOMAIN;
				}
			}
		}
#else
		for(body = space->first; body; body = body->next)
			advance(body);
#endif
		return 0;
	}
	else
		return EEL_XWRONGTYPE;
}


static EEL_xno eph_tween(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f w = eel_v2d(args + 1);
	if(w < 0.0f)
		w = 0.0f;
	else if(w > 1.0f)
		w = 1.0f;
#if 0
	if(EEL_TYPE(args) == eph_md.body_cid)
	{
		EPH_body *body = o2EPH_body(args->objref.v);
		tween(body, w);
		return 0;
	}
	else
#endif
	if(EEL_TYPE(args) == eph_md.space_cid)
	{
		EPH_space *space = o2EPH_space(args->objref.v);
		EPH_body *body;
		switch(space->outputmode)
		{
		  case EPH_LATEST:
		  default:
			for(body = space->first; body; body = body->next)
				om_latest(body);
			break;
		  case EPH_TWEEN:
			for(body = space->first; body; body = body->next)
				om_tween(body, w);
			break;
		  case EPH_EXTRAPOLATE:
			for(body = space->first; body; body = body->next)
				om_extrapolate(body, w);
			break;
		  case EPH_SEMIEXTRA:
			for(body = space->first; body; body = body->next)
				om_semiextra(body, w);
			break;
		}
		return 0;
	}
	else
		return EEL_XWRONGTYPE;
}


static EEL_xno eph_frame(EEL_vm *vm)
{
	EPH_space *space;
	EPH_body *b;
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	for(b = space->first; b; b = b->next)
	{
#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
		int i;
#endif
		if(b->killed)
			continue;
#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
		if(b->flags & EPH_AUTORESETFORCES)
			for(i = 0; i < EPH_DIMS; ++i)
				b->f[i] = 0.0f;
#endif
		if(b->methods[EPH_FRAME])
		{
			EEL_xno x = eel_callf(vm, b->methods[EPH_FRAME],
					"o", EPH_body2o(b));
			if(x)
				return x;
		}
	}
	return 0;
}


/* Add response forces for one contact point. Return total absolute force. */
static inline float apply_z_response(EEL_vm *vm, EPH_body *b, EPH_zmap *m,
		float x, float y, float z)
{
	EPH_space *space = b->space;
	float dir = atan2(y - b->c[EPH_Y], x - b->c[EPH_X]);
	float ftotal = 0.0f;
	float damage = 0.0f;
	float overlap;

	/* Corrective forces for absorbing impact energy */
	float f = (b->v[EPH_X] * (x - b->c[EPH_X]) +
			b->v[EPH_Y] * (y - b->c[EPH_Y])) / b->r;
	if(f > 0)
	{
		ftotal += f / b->m1[EPH_X] * space->impact_absorb_z;
		damage = ftotal;
	}
	else
		ftotal += f / b->m1[EPH_X] * space->impact_return_z;

	/*
	 * Overlap corrective forces
	 *	Not quite right, and may cause problems with steep walls, but
	 *	we'd need a lot heavier collision detection to get exact
	 *	contact points.
	 */
	overlap = z - b->z;
//	local overlap = self.r - phys.Distance(self, point);
	if(overlap > space->overlap_correct_max)
		overlap = space->overlap_correct_max;
	ftotal += overlap * space->overlap_correct_z / b->m1[EPH_X];

	/* Apply! */
	b->rf[EPH_X] -= ftotal * cos(dir);
	b->rf[EPH_Y] -= ftotal * sin(dir);

	return damage;
}

static inline EEL_xno check_z_collisions(EEL_vm *vm, EPH_space *space,
		EPH_body *b, EPH_zmap *m)
{
	float damage = 0.0f;
	float xa = b->c[EPH_X] * m->scale;
	float ya = b->c[EPH_Y] * m->scale;
	float r = b->r * m->scale;
	float scale_inv = 1.0f / m->scale;
	int i;
	float da;
	int points = ceil(r * 2.0f * M_PI / EPH_ZTEST_DENSITY);

	/* Test and handle hits around the perimeter of the bounding circle */
	if(points < 4)
	{
		if(b->flags & EPH_ZRESPONSE)
			points = 4;
		else
			points = 1, r = 0;
	}
	da = 2.0f * M_PI / points;
	for(i = 0; i < points; ++i)
	{
		float a = i * da;
		float x = xa + r * cos(a);
		float y = ya + r * sin(a);
		float mz = eph_getzi_raw(m, x, m->h - y);
		if(b->z > mz)
			continue;
		if(b->flags & EPH_ZRESPONSE)
			damage += apply_z_response(vm, b, m,
					x * scale_inv, y * scale_inv, mz);
		if(b->methods[EPH_HITZ])
		{
/*TODO: Collect contact points and pass them all as arguments to one callback. */
			EEL_xno ex = eel_callf(vm, b->methods[EPH_HITZ],
					"orrr", EPH_body2o(b),
					(EEL_real)x * scale_inv,
					(EEL_real)y * scale_inv,
					(EEL_real)mz);
			if(ex)
				return ex;
			if(b->killed)
				return 0;
		}
	}

	/* Call Impact() method if there is enough damage */
	damage *= space->impact_damage_z;
	if(b->methods[EPH_IMPACT] && (damage > space->impact_damage_min))
	{
		EEL_xno ex;
		damage -= space->impact_damage_min;
		ex = eel_callf(vm, b->methods[EPH_IMPACT], "orn",
			       		EPH_body2o(b), (EEL_real)damage);
		if(ex)
			return ex;
	}
	return 0;
}

static EEL_xno eph_collidez(EEL_vm *vm)
{
	EPH_space *space;
	EPH_body *b;
	EEL_value *args = vm->heap + vm->argv;
	unsigned mask = eel_v2l(args + 1);
	EPH_zmap *m;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	m = &space->zmap;
	for(b = space->first; b; b = b->next)
	{
		EEL_xno x;
		if(b->killed || (!b->methods[EPH_HITZ] &
				!(b->flags & EPH_ZRESPONSE)))
			continue;
		if(!(b->group & mask) && b->methods[EPH_HITZ])
		{
			/*
			 * Hack for keeping things from falling forever with
			 * "noclip"; we pretend we're off-map all the time...
			 */
			if(b->z > m->off)
				continue;
			x = eel_callf(vm, b->methods[EPH_HITZ],
					"orrr", EPH_body2o(b),
					(EEL_real)b->c[EPH_X],
					(EEL_real)b->c[EPH_Y],
					(EEL_real)m->off);
			if(x)
				return x;
			if(b->killed)
				continue;
		}
		x = check_z_collisions(vm, space, b, m);
		if(x)
			return x;
	}
	return 0;
}


static inline EEL_xno apply_response(EEL_vm *vm, EPH_body *b1, EPH_body *b2)
{
	EPH_space *space = b1->space;
	float dir, f;
	float ftotal = 0.0f;
	float damage = 0.0f;
	float dx = b2->c[EPH_X] - b1->c[EPH_X];
	float dy = b2->c[EPH_Y] - b1->c[EPH_Y];
	float dvx = b2->v[EPH_X] - b1->v[EPH_X];
	float dvy = b2->v[EPH_Y] - b1->v[EPH_Y];
	float m1 = 1.0f / b1->m1[EPH_X];
	float m2 = 1.0f / b2->m1[EPH_X];
	float overlap = b1->r + b2->r - sqrt(dx * dx + dy * dy);
	if(overlap <= .00001f)
		return 0;
	dir = atan2(dy, dx);

	// Corrective forces for absorbing impact energy
	f = -(dvx * dx + dvy * dy) / (b1->r + b2->r);
	if(f > 0)
	{
		ftotal += f * (m1 < m2 ? m1 : m2) * space->impact_absorb;
		damage = ftotal;
	}
	else
		ftotal += f * (m1 < m2 ? m1 : m2) * space->impact_return;

	// Overlap corrective forces
	ftotal += (overlap < space->overlap_correct_max ? overlap :
				space->overlap_correct_max) *
				space->overlap_correct * (m1 < m2 ? m1 : m2);

	// Apply!
	b1->rf[EPH_X] -= ftotal * cos(dir);
	b1->rf[EPH_Y] -= ftotal * sin(dir);
	b2->rf[EPH_X] += ftotal * cos(dir);
	b2->rf[EPH_Y] += ftotal * sin(dir);

	// Call Impact() methods if there is damage
	damage *= space->impact_damage;
	if(damage > space->impact_damage_min)
	{
		EEL_xno x;
		damage -= space->impact_damage_min;
		if(b1->methods[EPH_IMPACT])
		{
			x = eel_callf(vm, b1->methods[EPH_IMPACT], "oro",
			       		EPH_body2o(b1), (EEL_real)damage,
					EPH_body2o(b2));
			if(x)
				return x;
		}
		if(b2->methods[EPH_IMPACT])
		{
			if(!b1->killed)
				x = eel_callf(vm, b2->methods[EPH_IMPACT], "oro",
				       		EPH_body2o(b2), (EEL_real)damage,
						EPH_body2o(b1));
			else
				x = eel_callf(vm, b2->methods[EPH_IMPACT], "orn",
				       		EPH_body2o(b2), (EEL_real)damage);
			if(x)
				return x;
		}
	}
	return 0;
}

static inline EEL_xno check_collision(EEL_vm *vm, EPH_body *b1, EPH_body *b2)
{
	EEL_xno x;
	EPH_f dx, dy, dist, rsum;
	int hitb1 = b2->group & b1->hitmask;
	int hitb2 = b1->group & b2->hitmask;
	if(b1->killed || b2->killed)
		return 0;
	rsum = b1->r + b2->r;
	dx = b1->c[EPH_X] - b2->c[EPH_X];
	dy = b1->c[EPH_Y] - b2->c[EPH_Y];
	dist = dx * dx + dy * dy;
	if(dist > rsum * rsum)
		return 0;	/* No intersection! --> */
	if(!hitb1 && !hitb2)
		return 0;	/* All masked out! --> */
	if(b1->flags & b2->flags & EPH_RESPONSE)
	{
		x = apply_response(vm, b1, b2);
		if(x)
			return x;
	}
	if(b1->methods[EPH_HIT] && hitb1)
	{
		x = eel_callf(vm, b1->methods[EPH_HIT],
				"oo", EPH_body2o(b1), EPH_body2o(b2));
		if(x)
			return x;
	}
	if(b1->killed || b2->killed)
		return 0;
	if(b2->methods[EPH_HIT] && hitb2)
	{
		x = eel_callf(vm, b2->methods[EPH_HIT],
				"oo", EPH_body2o(b2), EPH_body2o(b1));
		if(x)
			return x;
	}
	return 0;
}


static EEL_xno eph_collide(EEL_vm *vm)
{
	EPH_space *space;
	EPH_body *b1, *b2;
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	for(b1 = space->first; b1; b1 = b1->next)
		for(b2 = b1->next; b2; b2 = b2->next)
			if(b2->group & b1->hitmask || b1->group & b2->hitmask)
			{
				EEL_xno x = check_collision(vm, b1, b2);
				if(x)
					return x;
			}
	return 0;
}


static EEL_xno eph_rendershadows(EEL_vm *vm)
{
	float xmin, ymin, xmax, ymax;
	EPH_space *space;
	EPH_body *b;
	EEL_value *args = vm->heap + vm->argv;
	unsigned mask = eel_v2l(args + 1);
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	xmin = space->vx + space->vxmin - EPH_MAXSHADOWDIST;
	ymin = space->vy + space->vymin;
	xmax = space->vx + space->vxmax;
	ymax = space->vy + space->vymax + EPH_MAXSHADOWDIST;
	for(b = space->first; b; b = b->next)
	{
		EEL_xno x;
		float r;
		if(b->killed || !b->methods[EPH_RENDERSHADOW] || !(b->group & mask))
			continue;
		r = b->shadowr;
		if(b->i[EPH_X] - r > xmax ||
				b->i[EPH_X] + r < xmin ||
				b->i[EPH_Y] - r > ymax ||
				b->i[EPH_Y] + r < ymin)
			continue;
		x = eel_callf(vm, b->methods[EPH_RENDERSHADOW], "o", EPH_body2o(b));
		if(x)
			return x;
	}
	return 0;
}


static EEL_xno eph_render(EEL_vm *vm)
{
	float xmin, ymin, xmax, ymax;
	EPH_space *space;
	EPH_body *b;
	EEL_value *args = vm->heap + vm->argv;
	unsigned mask = eel_v2l(args + 1);
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	xmin = space->vx + space->vxmin;
	ymin = space->vy + space->vymin;
	xmax = space->vx + space->vxmax;
	ymax = space->vy + space->vymax;
	for(b = space->first; b; b = b->next)
	{
		EEL_xno x;
		float r;
		if(b->killed || !b->methods[EPH_RENDER] || !(b->group & mask))
			continue;
		r = b->r * EPH_CULLSCALE;
		if(b->i[EPH_X] - r > xmax ||
				b->i[EPH_X] + r < xmin ||
				b->i[EPH_Y] - r > ymax ||
				b->i[EPH_Y] + r < ymin)
			continue;
		x = eel_callf(vm, b->methods[EPH_RENDER], "o", EPH_body2o(b));
		if(x)
			return x;
	}
	return 0;
}


static EEL_xno eph_kill(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_TYPE(arg) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	return kill_body(o2EPH_body(arg->objref.v), 1);
}


static EEL_xno eph_killall(EEL_vm *vm)
{
	EEL_xno x, xx;
	EPH_space *space;
	EPH_body *b;
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_TYPE(arg) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(arg->objref.v);
	/*
	 * NOTE:
	 *	If there are multiple exceptions thrown, we only return the
	 *	last one! This even bothering to check for exceptions is just a
	 *	debug tool anyway.
	 */
//printf("killall\n");
	for(x = 0, b = space->first; b; b = b->next)
		if((xx = kill_body(b, 0)))
			x = xx;
//printf("/killall\n");
	return x;
}


static EEL_xno eph_clean(EEL_vm *vm)
{
	EPH_space *space;
	EPH_body *b;
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_TYPE(arg) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(arg->objref.v);
	if(space->clean_constraints)
	{
//printf("--- cleaning constraints ---\n");
		EPH_constraint *c = space->firstc;
		while(c)
		{
			EPH_constraint *cnext = c->next;
			if(!eph_ConstraintIsAlive(c))
			{
				eph_UnlinkConstraint(space, c);
				--space->constraints;
//printf("### deleted constraint %p\n", c);
			}
//else
//printf("leaving constraint %p\n", c);
			c = cnext;
		}
		space->clean_constraints = 0;
	}
//printf("--- cleaning bodies ---\n");
	b = space->first;
	while(b)
	{
		EPH_body *bnext = b->next;
		if(b->killed)
		{
			eph_UnlinkBody(space, b);
			eel_disown(EPH_body2o(b));
//printf("### deleted body %p\n", b);
		}
//else
//printf("leaving body %p\n", b);
		b = bnext;
	}
//printf("--- cleaning done! ---\n");
	return 0;
}


// Init: space, x, y, r, mass, inertia, group
static EEL_xno body_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EPH_space *space;
	EPH_f r = 1.0f;
	EPH_f mass = 1.0f;
	EPH_f inertia = 1.0f;
	EEL_value v;
	EEL_xno xno;
	EPH_body *body;
	EEL_object *eo;
	if(initc < 1)
		return EEL_XARGUMENTS;
	if(!(eo = eel_o_alloc(vm, sizeof(EPH_body), type)))
		return EEL_XMEMORY;
	body = o2EPH_body(eo);
	memset(body, 0, sizeof(EPH_body));
	if(EEL_TYPE(initv) != eph_md.space_cid)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
	space = o2EPH_space(initv->objref.v);
	body->space = space;
	switch(initc)
	{
	  case 7:
	  {
		body->group = eel_v2l(initv + 6);
		if(!is_pot(body->group))
		{
			/*
			 * Future versions will require that a body is a member
			 * of exactly one group!
			 */
			eel_o_free(eo);
			return EEL_XBADVALUE;
		}
	  }
	  case 6:
		inertia = eel_v2d(initv + 5);
	  case 5:
		mass = eel_v2d(initv + 4);
	  case 4:
		r = eel_v2d(initv + 3);
	  case 3:
		body->i[EPH_Y] = body->c[EPH_Y] =
#if TWEEN_INTERPOLATE == 1
				body->o[EPH_Y] =
#endif
				eel_v2d(initv + 2);
	  case 2:
		body->i[EPH_X] = body->c[EPH_X] =
#if TWEEN_INTERPOLATE == 1
				body->o[EPH_X] =
#endif
				eel_v2d(initv + 1);
		break;
	  default:
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	if(mass <= 0.0f || inertia <= 0.0f)
	{
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	body->r = r;
	body->m1[EPH_X] = body->m1[EPH_Y] = 1.0f / mass;
	body->m1[EPH_A] = 1.0f / inertia;
	body->e = 0.0f;
	body->flags = EPH_DEFAULT_BODY_FLAGS;
	xno = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(xno)
	{
		eel_disown(eo);
		return xno;
	}
	body->table = eel_v2o(&v);
	eph_LinkBody(space, body);
	++space->active;
	eel_own(eo);	/* Owned by space! */
	eel_o2v(result, eo);
//printf("created body %p\n", body);
	return 0;
}


static EEL_xno body_destruct(EEL_object *eo)
{
	int i;
	EPH_body *body = o2EPH_body(eo);
//printf("destroying body %p\n", eo);
	if(!body->killed)
	{
		printf("WARNING: Physics body %p destroyed while active!\n",
				body);
		kill_body(body, 0);
		eph_UnlinkBody(body->space, body);
	}
	for(i = 0; i < EPH_NBODYMETHODS; ++i)
		if(body->methods[i])
			eel_disown(body->methods[i]);
	eel_disown(body->table);
//printf("destroyed body %p\n", eo);
	return 0;
}


/* This cannot fail unless the field LUT is screwed up! */
static inline EPH_f *body_get_vector(EPH_body *body, unsigned fi)
{
	switch(fi & 0xff00)
	{
	  case EPH_BIX:		return body->i;
#if TWEEN_INTERPOLATE == 1
	  case EPH_BOX:		return body->o;
#endif
	  case EPH_BCX:		return body->c;
	  case EPH_BVX:		return body->v;
	  case EPH_BRFX:	return body->rf;
	  case EPH_BFX:		return body->f;
	  case EPH_BUX:		return body->u;
	  case EPH_BCDX:	return body->cd;
	}
	return NULL;
}


static EEL_xno body_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int ind;
	EEL_value v;
	EPH_body *body = o2EPH_body(eo);
	if(eel_table_get(eph_md.bodyfields, op1, &v) != EEL_XNONE)
	{
		/* No hit! Fall through to extension table. */
		EEL_xno x = eel_table_get(body->table, op1, op2);
		if(x)
			return x;
		eel_v_own(op2);
		return 0;
	}
	ind = v.integer.v & 0xff;
	if((v.integer.v & 0xff00) == EPH_B_METHODS)
	{
		if(body->methods[ind])
		{
			eel_own(body->methods[ind]);
			eel_o2v(op2, body->methods[ind]);
		}
		else
			eel_nil2v(op2);
		return 0;
	}
	if((v.integer.v & 0xff00) >= EPH_B_VECTORS)
	{
		EPH_f *f = body_get_vector(body, v.integer.v);
		eel_d2v(op2, f[ind]);
		return 0;
	}
	switch(ind)
	{
	  case EPH_BSPACE:
	  {
		if(!body->space)
			return EEL_XDEVICECLOSED;
		eel_own(EPH_space2o(body->space));
		eel_o2v(op2, EPH_space2o(body->space));
		return 0;
	  }
	  case EPH_BFLAGS:	eel_l2v(op2, body->flags);	return 0;
	  case EPH_BGROUP:	eel_l2v(op2, body->group);	return 0;
	  case EPH_BHITMASK:	eel_l2v(op2, body->hitmask);	return 0;
	  case EPH_BSHADOWR:	eel_d2v(op2, body->shadowr);	return 0;
	  case EPH_BZ:		eel_d2v(op2, body->z);		return 0;
	  case EPH_BR:		eel_d2v(op2, body->r);		return 0;
	  case EPH_BE:		eel_d2v(op2, body->e);		return 0;
	  case EPH_BM:		eel_d2v(op2, 1.0f / body->m1[EPH_X]);	return 0;
	  case EPH_BMI:		eel_d2v(op2, 1.0f / body->m1[EPH_A]);	return 0;
	}
	/* Only happens if the field table doesn't match the implementation! */
	return EEL_XINTERNAL;
}


static EEL_xno body_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	int ind;
	EEL_value v;
	EPH_body *body = o2EPH_body(eo);
	if(eel_table_get(eph_md.bodyfields, op1, &v) != EEL_XNONE)
		return eel_o_metamethod(body->table, EEL_MM_SETINDEX, op1, op2);
	ind = v.integer.v & 0xff;
	if((v.integer.v & 0xff00) == EPH_B_METHODS)
	{
		if(op2->type == EEL_TNIL)
			body->methods[ind] = NULL;
		else
		{
			if(EEL_TYPE(op2) != (EEL_types)EEL_CFUNCTION)
				return EEL_XNEEDCALLABLE;
			body->methods[ind] = op2->objref.v;
			eel_own(body->methods[ind]);
		}
		return 0;
	}
	if((v.integer.v & 0xff00) >= EPH_B_VECTORS)
	{
		EPH_f *f = body_get_vector(body, v.integer.v);
		f[ind] = eel_v2d(op2);
#if EPH_DOMAIN_CHECKS == 1
		if(!isfinite(f[ind]))
		{
			printf("body_setindex(): DOMAIN ERROR: %f\n", f[ind]);
			return EEL_XDOMAIN;
		}
#endif
		return 0;
	}
	switch(ind)
	{
	  case EPH_BSPACE:	return EEL_XCANTWRITE;	/* Read-only! */
	  case EPH_BFLAGS:	body->flags = eel_v2l(op2);	return 0;
	  case EPH_BGROUP:
	  {
		int g = eel_v2l(op2);
		if(!is_pot(g))
		{
			/*
			 * Future versions will require that a body is a member
			 * of exactly one group!
			 */
			return EEL_XBADVALUE;
		}
		body->group = g;
		return 0;
	  }
	  case EPH_BHITMASK:	body->hitmask = eel_v2l(op2);	return 0;
	  case EPH_BSHADOWR:	body->shadowr = eel_v2d(op2);	return 0;
	  case EPH_BZ:		body->z = eel_v2d(op2);		return 0;
	  case EPH_BR:		body->r = eel_v2d(op2);		return 0;
	  case EPH_BE:		body->e = eel_v2d(op2);		return 0;
	  case EPH_BM:
	  {
		EPH_f m = eel_v2d(op2);
		if(m <= 0.0f)
			return EEL_XARGUMENTS;
		body->m1[EPH_X] = body->m1[EPH_Y] = 1.0f / m;
		return 0;
	  }
	  case EPH_BMI:
	  {
		EPH_f mi = eel_v2d(op2);
		if(mi <= 0.0f)
			return EEL_XARGUMENTS;
		body->m1[EPH_A] = 1.0f / mi;
		return 0;
	  }
	}
	/* Only happens if the field table doesn't match the implementation! */
	return EEL_XINTERNAL;
}


static EEL_xno eph_force(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f a = eel_v2d(args + 1);
	EPH_f f = eel_v2d(args + 2);
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	eph_Force(o2EPH_body(args->objref.v), f, a);
	return 0;
}

static EEL_xno eph_forceat(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f x = eel_v2d(args + 1);
	EPH_f y = eel_v2d(args + 2);
	EPH_f a = eel_v2d(args + 3);
	EPH_f f = eel_v2d(args + 4);
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	eph_ForceAt(o2EPH_body(args->objref.v), x, y, a, f);
	return 0;
}

static EEL_xno eph_forceatv(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f x = eel_v2d(args + 1);
	EPH_f y = eel_v2d(args + 2);
	EPH_f fx = eel_v2d(args + 3);
	EPH_f fy = eel_v2d(args + 4);
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	eph_ForceAtV(o2EPH_body(args->objref.v), x, y, fx, fy);
	return 0;
}

static EEL_xno eph_forcerel(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f a = eel_v2d(args + 1);
	EPH_f f = eel_v2d(args + 2);
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	eph_ForceRel(o2EPH_body(args->objref.v), a, f);
	return 0;
}

static EEL_xno eph_forceatrel(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f x = eel_v2d(args + 1);
	EPH_f y = eel_v2d(args + 2);
	EPH_f a = eel_v2d(args + 3);
	EPH_f f = eel_v2d(args + 4);
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	eph_ForceAtRel(o2EPH_body(args->objref.v), x, y, a, f);
	return 0;
}


static EEL_xno eph_velocity(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_body *body;
	EPH_f a, v;
	if(EEL_TYPE(args) != eph_md.body_cid)
		return EEL_XWRONGTYPE;
	body = o2EPH_body(args->objref.v);
	a = eel_v2d(args + 1);
	v = eel_v2d(args + 2);
	body->v[EPH_X] += v * cos(a);
	body->v[EPH_Y] += v * sin(a);
	if(vm->argc >= 4)
		body->v[EPH_A] += eel_v2d(args + 3);
	return 0;
}


/*----------------------------------------------------------
	Constraints
----------------------------------------------------------*/

// Init: space, kind, maxforce, <params>
//	If 'maxforce' is 0, the constraint will never break.
//
//	DAMPEDSPRING:	a, ax, ay,	b, bx, by,	d0, k, k2, kd
// TODO:	If 'b' is nil, the 'b end' is attached to the world at (bx, by).
//
static EEL_xno constraint_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EPH_space *space;
	EPH_constraint *c;
	EEL_object *eo;
	if(initc != 13)
		return EEL_XARGUMENTS;
	if(EEL_TYPE(initv) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(initv->objref.v);
	eo = eel_o_alloc(vm, sizeof(EPH_constraint), type);
	if(!eo)
		return EEL_XMEMORY;
	c = o2EPH_constraint(eo);
	c->kind = (EPH_constraints)eel_v2l(initv + 1);
	if(EEL_TYPE(initv + 3) != eph_md.body_cid)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
	c->a = o2EPH_body(eel_v2o(initv + 3));
	if(EEL_TYPE(initv + 6) != eph_md.body_cid)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
	c->b = o2EPH_body(eel_v2o(initv + 6));
	c->broken_cb = NULL;
	c->maxforce = eel_v2d(initv + 2);
	switch(c->kind)
	{
	  case EPH_DAMPEDSPRING:
		c->p.spring.ax = eel_v2d(initv + 4);
		c->p.spring.ay = eel_v2d(initv + 5);
		c->p.spring.bx = eel_v2d(initv + 7);
		c->p.spring.by = eel_v2d(initv + 8);
		c->p.spring.d0 = eel_v2d(initv + 9);
		c->p.spring.k = eel_v2d(initv + 10);
		c->p.spring.k2 = eel_v2d(initv + 11);
		c->p.spring.kd = eel_v2d(initv + 12);
		break;
	  default:
		eel_o_free(eo);
		return EEL_XBADVALUE;
	}
	eph_LinkConstraint(space, c);
	++space->constraints;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno constraint_destruct(EEL_object *eo)
{
	EPH_constraint *c = o2EPH_constraint(eo);
	if(c->a)
	{
		--c->a->space->constraints;
		eph_UnlinkConstraint(c->a->space, c);
	}
	if(c->broken_cb)
		eel_disown(c->broken_cb);
	return 0;
}


static EEL_xno constraint_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EPH_constraint *c = o2EPH_constraint(eo);
	EPH_spring *s = &c->p.spring;
	if(eel_table_get(eph_md.constraintfields, op1, &v) != EEL_XNONE)
		return EEL_XWRONGINDEX;
	switch(v.integer.v)
	{
	  case EPH_CA:
		if(c->a)
		{
			eel_own(EPH_body2o(c->a));
			eel_o2v(op2, EPH_body2o(c->a));
		}
		else
			eel_nil2v(op2);
		return 0;
	  case EPH_CB:
		if(c->b)
		{
			eel_own(EPH_body2o(c->b));
			eel_o2v(op2, EPH_body2o(c->b));
		}
		else
			eel_nil2v(op2);
		return 0;
	  case EPH_CBROKEN:
		if(c->broken_cb)
		{
			eel_own(c->broken_cb);
			eel_o2v(op2, c->broken_cb);
		}
		else
			eel_nil2v(op2);
		return 0;
	  case EPH_CMAXFORCE:	eel_d2v(op2, c->maxforce);	return 0;
	  case EPH_CKIND:	eel_l2v(op2, c->kind);		return 0;
	  case EPH_CAX:		eel_d2v(op2, s->ax);		return 0;
	  case EPH_CAY:		eel_d2v(op2, s->ay);		return 0;
	  case EPH_CBX:		eel_d2v(op2, s->bx);		return 0;
	  case EPH_CBY:		eel_d2v(op2, s->by);		return 0;
	  case EPH_CD0:		eel_d2v(op2, s->d0);		return 0;
	  case EPH_CK:		eel_d2v(op2, s->k);		return 0;
	  case EPH_CK2:		eel_d2v(op2, s->k2);		return 0;
	  case EPH_CKD:		eel_d2v(op2, s->kd);		return 0;
	}
	return EEL_XWRONGINDEX;
}


static EEL_xno constraint_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EPH_constraint *c = o2EPH_constraint(eo);
	EPH_spring *s = &c->p.spring;
	if(eel_table_get(eph_md.constraintfields, op1, &v) != EEL_XNONE)
		return EEL_XWRONGINDEX;
	switch(v.integer.v)
	{
	  case EPH_CBROKEN:
		if(op2->type == EEL_TNIL)
			c->broken_cb = NULL;
		else
		{
			if(EEL_TYPE(op2) != (EEL_types)EEL_CFUNCTION)
				return EEL_XNEEDCALLABLE;
			c->broken_cb = op2->objref.v;
			eel_own(c->broken_cb);
		}
		return 0;
	  case EPH_CMAXFORCE:	c->maxforce = eel_v2d(op2);	return 0;
	  case EPH_CAX:		s->ax = eel_v2d(op2);		return 0;
	  case EPH_CAY:		s->ay = eel_v2d(op2);		return 0;
	  case EPH_CBX:		s->bx = eel_v2d(op2);		return 0;
	  case EPH_CBY:		s->by = eel_v2d(op2);		return 0;
	  case EPH_CD0:		s->d0 = eel_v2d(op2);		return 0;
	  case EPH_CK:		s->k = eel_v2d(op2);		return 0;
	  case EPH_CK2:		s->k2 = eel_v2d(op2);		return 0;
	  case EPH_CKD:		s->kd = eel_v2d(op2);		return 0;
	  default:	return EEL_XCANTWRITE;
	}
	return EEL_XWRONGINDEX;
}


static inline float apply_damped_spring(EPH_constraint *c)
{
	EPH_spring *s = &c->p.spring;
	EPH_body *a = c->a;
	EPH_body *b = c->b;
	EPH_f rax, ray, rbx, rby;	/* Anchor a/b, rotated (x, y) */
	EPH_f raxw, rayw, rbxw, rbyw;	/* Anchors, world coordinates */
	EPH_f nx, ny;			/* Normalized spring direction */
	EPH_f d;			/* Distance between anchors */
	EPH_f vax, vay, vbx, vby;	/* Anchor velocities */
	EPH_f pdv;			/* Projected anchor velocity diff */
	EPH_f dd;			/* Deviation from relaxed length */
	EPH_f f, maxf;			/* Force, maximum force limit */

	/* Anchor positions */
	eph_Rotate(s->ax, s->ay, a->c[EPH_A], &rax, &ray);
	eph_Rotate(s->bx, s->by, b->c[EPH_A], &rbx, &rby);
	raxw = rax + a->c[EPH_X];
	rayw = ray + a->c[EPH_Y];
	rbxw = rbx + b->c[EPH_X];
	rbyw = rby + b->c[EPH_Y];
	
	/* Direction + distance */
	nx = rbxw - raxw;
	ny = rbyw - rayw;
	d = sqrt(nx*nx + ny*ny);
	if(!d)
		return 0.0f;	/* No direction of forces in the x/y plane! */

	/* Normalize the spring direction vector */
	nx /= d;
	ny /= d;

	/* Anchor velocities */
	vax = a->v[EPH_X] - ray * a->v[EPH_A];
	vay = a->v[EPH_Y] + rax * a->v[EPH_A];
	vbx = b->v[EPH_X] - rby * b->v[EPH_A];
	vby = b->v[EPH_Y] + rbx * b->v[EPH_A];

	/* Anchor velocity difference projected onto the spring */
	pdv = eph_Dot(vbx - vax, vby - vay, nx, ny);

	/* Spring force */
	dd = d - s->d0;
	f = dd * s->k + dd * fabs(dd) * s->k2;

	/* Damping force (A bit dangerous with strong damping...) */
	f += pdv * s->kd;

	/* Limit and apply total force! */
	maxf = 1.0f / (a->m1[EPH_X] < b->m1[EPH_X] ? a->m1[EPH_X] : b->m1[EPH_X]);
	if(f < -maxf)
		f = -maxf;
	else if(f > maxf)
		f = maxf;
	eph_ForceAtV(a, rax, ray, nx * f, ny * f);
	eph_ForceAtV(b, rbx, rby, -nx * f, -ny * f);
	return f;
}

static EEL_xno eph_applyconstraints(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_constraint *c;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(eel_v2o(args));
	c = space->firstc;
	space->clean_constraints = 0;	/* No need for eph_clean() to do it now! */
	while(c)
	{
		float f;
		EPH_constraint *cnext = c->next;
		if(!eph_ConstraintIsAlive(c))
		{
			eph_UnlinkConstraint(space, c);
			--space->constraints;
			c = cnext;
			continue;
		}
		f = apply_damped_spring(c);
		if(c->maxforce && (fabs(f) > c->maxforce))
		{
			
			if(c->broken_cb)
			{
				EEL_xno x = eel_callf(vm, c->broken_cb,
						"o", EPH_constraint2o(c));
				if(x)
					return x;
			}
		}
		c = cnext;
	}
	return 0;
}

static EEL_xno eph_firstconstraint(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_constraint *c;
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(eel_v2o(args));
	for(c = space->firstc; c; c = c->next)
	{
		if(!eph_ConstraintIsAlive(c))
			continue;
		eel_own(EPH_constraint2o(c));
		eel_o2v(vm->heap + vm->resv, EPH_constraint2o(c));
		return 0;
	}
	eel_nil2v(vm->heap + vm->resv);
	return 0;
}

static EEL_xno eph_nextconstraint(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_constraint *c;
	if(EEL_TYPE(args) != eph_md.constraint_cid)
		return EEL_XWRONGTYPE;
	for(c = o2EPH_constraint(eel_v2o(args))->next; c; c = c->next)
	{
		if(!eph_ConstraintIsAlive(c))
			continue;
		eel_own(EPH_constraint2o(c));
		eel_o2v(vm->heap + vm->resv, EPH_constraint2o(c));
		return 0;
	}
	eel_nil2v(vm->heap + vm->resv);
	return 0;
}


/*----------------------------------------------------------
	Utilities
----------------------------------------------------------*/

static EEL_xno eph_rotate(EEL_vm *vm)
{
	EEL_object *o = eel_cv_new_noinit(vm, EEL_CVECTOR_D, 2);
	EEL_value *args = vm->heap + vm->argv;
	double *d;
	if(!o)
		return EEL_XMEMORY;
	d = o2EEL_vector(o)->buffer.d;
	eph_Rotate(eel_v2d(args), eel_v2d(args + 1), eel_v2d(args + 2),
			d, d + 1);
	eel_o2v(vm->heap + vm->resv, o);
	return 0;
}

static EEL_xno eph_dot(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, eph_Dot(eel_v2d(args), eel_v2d(args + 1),
			eel_v2d(args + 2), eel_v2d(args + 3)));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_dot(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}

static EEL_xno eph_cross(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, eph_Cross(eel_v2d(args), eel_v2d(args + 1),
			eel_v2d(args + 2), eel_v2d(args + 3)));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_cross(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}


static inline EEL_xno grab_xy(EEL_value *arg, EPH_f *cx, EPH_f *cy)
{
	if(EEL_TYPE(arg) == eph_md.body_cid)
	{
		EPH_body *b = o2EPH_body(arg->objref.v);
		*cx = b->c[EPH_X];
		*cy = b->c[EPH_Y];
	}
	else if(EEL_TYPE(arg) == EEL_CVECTOR_D)
	{
		EEL_vector *v = o2EEL_vector(arg->objref.v);
		if(v->length < 2)
			return EEL_XHIGHINDEX;
		*cx = v->buffer.d[0];
		*cy = v->buffer.d[1];
	}
	else
	{
		EEL_xno x;
		EEL_value v;
		EEL_object *o = eel_v2o(arg);
		if(!o)
			return EEL_XNEEDOBJECT;
		x = eel_getindex(o, &eph_md.cx, &v);
		if(x)
			return x;
		*cx = eel_v2d(&v);
		x = eel_getindex(o, &eph_md.cy, &v);
		if(x)
			return x;
		*cy = eel_v2d(&v);
	}
	return 0;
}

static inline EEL_xno grab_xya(EEL_value *arg, EPH_f *cx, EPH_f *cy, EPH_f *ca)
{
	if(EEL_TYPE(arg) == eph_md.body_cid)
	{
		EPH_body *b = o2EPH_body(arg->objref.v);
		*cx = b->c[EPH_X];
		*cy = b->c[EPH_Y];
		*ca = b->c[EPH_A];
	}
	else if(EEL_TYPE(arg) == EEL_CVECTOR_D)
	{
		EEL_vector *v = o2EEL_vector(arg->objref.v);
		if(v->length < 3)
			return EEL_XHIGHINDEX;
		*cx = v->buffer.d[0];
		*cy = v->buffer.d[1];
		*ca = v->buffer.d[2];
	}
	else
	{
		EEL_xno x;
		EEL_value v;
		EEL_object *o = eel_v2o(arg);
		if(!o)
			return EEL_XNEEDOBJECT;
		if((x = eel_getindex(o, &eph_md.cx, &v)))
			return x;
		*cx = eel_v2d(&v);
		if((x = eel_getindex(o, &eph_md.cy, &v)))
			return x;
		*cy = eel_v2d(&v);
		if((x = eel_getindex(o, &eph_md.ca, &v)))
			return x;
		*ca = eel_v2d(&v);
	}
	return 0;
}

static EEL_xno eph_distance(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f xa, ya, xb, yb;
	EEL_xno x = grab_xy(args, &xa, &ya);
	if(x)
		return x;
	x = grab_xy(args + 1, &xb, &yb);
	if(x)
		return x;
	eel_d2v(vm->heap + vm->resv, eph_Distance(xa, ya, xb, yb));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_distance(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}

/*
export function PointDistance(a, xa, ya, b, xb, yb)
{
	return Distance(vector [a.(cx, cy) + Rotate(xa, ya, a.ca)[0, 1]],
			vector [b.(cx, cy) + Rotate(xb, yb, b.ca)[0, 1]]);
}
 */
static EEL_xno eph_pointdistance(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f xa, ya, aa, xb, yb, ab, xr, yr;
	EEL_xno x = grab_xya(args, &xa, &ya, &aa);
	if(x)
		return x;
	x = grab_xya(args + 3, &xb, &yb, &ab);
	if(x)
		return x;
	eph_Rotate(eel_v2d(args + 1), eel_v2d(args + 2), aa, &xr, &yr);
	xa += xr;
	ya += yr;
	eph_Rotate(eel_v2d(args + 4), eel_v2d(args + 5), ab, &xr, &yr);
	xb += xr;
	yb += yr;
	eel_d2v(vm->heap + vm->resv, eph_Distance(xa, ya, xb, yb));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_distance(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}

static EEL_xno eph_direction(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f xa, ya, xb, yb;
	EEL_xno x = grab_xy(args, &xa, &ya);
	if(x)
		return x;
	x = grab_xy(args + 1, &xb, &yb);
	if(x)
		return x;
	eel_d2v(vm->heap + vm->resv, eph_Direction(xa, ya, xb, yb));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_direction(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}


static inline EEL_xno find_body(EEL_vm *vm, EPH_space *space, EPH_body *b,
		unsigned mask, unsigned testflags)
{
	float xmin, ymin, xmax, ymax;
	int test_view = (~testflags) & (EPH_INVIEW | EPH_NOTINVIEW);
	int test_view_in = testflags & EPH_INVIEW;
	if(!mask)
		return 0;
	if(test_view == (EPH_INVIEW | EPH_NOTINVIEW))
		return 0;	/* Exclude both in + out ==> no hits! */
	if(test_view)
	{
		xmin = space->vx + space->vxmin;
		ymin = space->vy + space->vymin;
		xmax = space->vx + space->vxmax;
		ymax = space->vy + space->vymax;
	}
	for( ; b; b = b->next)
	{
		if(b->killed || !(b->group & mask))
			continue;
		if(test_view)
		{
			float r = b->r * EPH_CULLSCALE;
			if(b->i[EPH_X] - r > xmax ||
					b->i[EPH_X] + r < xmin ||
					b->i[EPH_Y] - r > ymax ||
					b->i[EPH_Y] + r < ymin)
			{
				if(test_view_in)
					continue;
			}
			else
				if(!test_view_in)
					continue;
		}
		eel_own(EPH_body2o(b));
		eel_o2v(vm->heap + vm->resv, EPH_body2o(b));
		return 0;
	}
	eel_nil2v(vm->heap + vm->resv);
	return 0;
}

static EEL_xno eph_findnext(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_body *b;
	unsigned mask = -1;
	unsigned testflags = -1;
	if(EEL_TYPE(args) == eph_md.body_cid)
		b = o2EPH_body(args->objref.v)->next;
	else if(EEL_TYPE(args) == eph_md.space_cid)
		b = o2EPH_space(args->objref.v)->first;
	else
		return EEL_XWRONGTYPE;
	if(!b)
	{
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	if(vm->argc >= 2)
		mask = eel_v2l(args + 1);
	if(vm->argc >= 3)
		testflags = eel_v2l(args + 2);
	return find_body(vm, b->space, b, mask, testflags);
}

static EEL_xno eph_findat(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_space *space;
	EPH_body *b;
	unsigned mask = -1;
	EPH_f x = eel_v2d(args + 1);
	EPH_f y = eel_v2d(args + 2);
	if(EEL_TYPE(args) != eph_md.space_cid)
		return EEL_XWRONGTYPE;
	space = o2EPH_space(args->objref.v);
	if(vm->argc >= 4)
		mask = eel_v2l(args + 3);
	for(b = space->first; b; b = b->next)
	{
		EPH_f dx, dy;
		if(b->killed || !(b->group & mask))
			continue;
		dx = b->c[EPH_X] - x;
		dy = b->c[EPH_Y] - y;
		if(dx*dx + dy*dy > b->r*b->r)
			continue;
		eel_own(EPH_body2o(b));
		eel_o2v(vm->heap + vm->resv, EPH_body2o(b));
		return 0;
	}
	eel_nil2v(vm->heap + vm->resv);
	return 0;
}


static EEL_xno eph_circleinertia(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f m = eel_v2d(args);
	EPH_f r1 = eel_v2d(args + 1);
	EPH_f r2 = eel_v2d(args + 2);
	eel_d2v(vm->heap + vm->resv, .5f * m * (r1*r1 + r2*r2));
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_dot(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}


static EEL_xno eph_diskinertia(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EPH_f m = eel_v2d(args);
	EPH_f r = eel_v2d(args + 1);
	eel_d2v(vm->heap + vm->resv, .5f * m * r*r);
#if EPH_DOMAIN_CHECKS == 1
	if(!isfinite(vm->heap[vm->resv].real.v))
	{
		printf("eph_dot(): DOMAIN ERROR: %f\n", vm->heap[vm->resv].real.v);
		return EEL_XDOMAIN;
	}
#endif
	return 0;
}


/*----------------------------------------------------------
	Constants and fields
----------------------------------------------------------*/

static const EEL_lconstexp eph_constants[] =
{
	/* Endian handling */
	{"MAXSHADOWDIST",	EPH_MAXSHADOWDIST	},
	
	/* Body flags */
#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
	{"AUTORESETFORCES",	EPH_AUTORESETFORCES	},
	{"IMPULSEFORCES",	EPH_IMPULSEFORCES	},
#endif
	{"RESPONSE",		EPH_RESPONSE		},
	{"ZRESPONSE",		EPH_ZRESPONSE		},
	{"DEFAULT_BODY_FLAGS",	EPH_DEFAULT_BODY_FLAGS	},

	/* Constraint types */
	{"DAMPEDSPRING",	EPH_DAMPEDSPRING	},

	{NULL, 0}
};

static const EEL_lconstexp eph_spacefields[] =
{
	{"offmapz",		EPH_SOFFMAPZ		},
	{"zmapscale",		EPH_SZMAPSCALE		},
	{"vx",			EPH_SVX			},
	{"vy",			EPH_SVY			},
	{"vxmin",		EPH_SVXMIN		},
	{"vymin",		EPH_SVYMIN		},
	{"vxmax",		EPH_SVXMAX		},
	{"vymax",		EPH_SVYMAX		},
	{"active",		EPH_SACTIVE		},
	{"constraints",		EPH_SCONSTRAINTS	},
	{"integration",		EPH_SINTEGRATION	},
	{"outputmode",		EPH_SOUTPUTMODE		},

	{"impact_damage_min",	EPH_SIMPACT_DAMAGE_MIN	},
	{"overlap_correct_max",	EPH_SOVERLAP_CORRECT_MAX},

	{"impact_absorb",	EPH_SIMPACT_ABSORB	},
	{"impact_return",	EPH_SIMPACT_RETURN	},
	{"impact_damage",	EPH_SIMPACT_DAMAGE	},
	{"overlap_correct",	EPH_SOVERLAP_CORRECT	},

	{"impact_absorb_z",	EPH_SIMPACT_ABSORB_Z	},
	{"impact_return_z",	EPH_SIMPACT_RETURN_Z	},
	{"impact_damage_z",	EPH_SIMPACT_DAMAGE_Z	},
	{"overlap_correct_z",	EPH_SOVERLAP_CORRECT_Z	},

	{NULL, 0}
};

static const EEL_lconstexp eph_bodyfields[] =
{
	{"space",		EPH_BSPACE		},
	{"flags",		EPH_BFLAGS		},
	{"group",		EPH_BGROUP		},
	{"hitmask",		EPH_BHITMASK		},
	{"z",			EPH_BZ			},
	{"r",			EPH_BR			},
	{"shadowr",		EPH_BSHADOWR		},
	{"e",			EPH_BE			},
	{"m",			EPH_BM			},
	{"mi",			EPH_BMI			},

	/* Methods */
	{"Frame",		EPH_B_METHODS | EPH_FRAME	},
	{"Hit",			EPH_B_METHODS | EPH_HIT		},
	{"HitZ",		EPH_B_METHODS | EPH_HITZ	},
	{"Impact",		EPH_B_METHODS | EPH_IMPACT	},
	{"Render",		EPH_B_METHODS | EPH_RENDER	},
	{"RenderShadow",	EPH_B_METHODS | EPH_RENDERSHADOW},
	{"Cleanup",		EPH_B_METHODS | EPH_CLEANUP	},

	/* Specially encoded for vector addressing */
	{"ix",	EPH_BIX},	{"iy", EPH_BIY},	{"ia", EPH_BIA},
#if TWEEN_INTERPOLATE == 1
	{"ox",	EPH_BOX},	{"oy", EPH_BOY},	{"oa", EPH_BOA},
#endif
	{"cx",	EPH_BCX},	{"cy", EPH_BCY},	{"ca", EPH_BCA},
	{"vx",	EPH_BVX},	{"vy", EPH_BVY},	{"va", EPH_BVA},
	{"fx",	EPH_BFX},	{"fy", EPH_BFY},	{"fa", EPH_BFA},
	{"rfx",	EPH_BRFX},	{"rfy", EPH_BRFY},	{"rfa", EPH_BRFA},
	{"ux",	EPH_BUX},	{"uy", EPH_BUY},	{"ua", EPH_BUA},
	{"cdx",	EPH_BCDX},	{"cdy", EPH_BCDY},	{"cda", EPH_BCDA},
	
	{NULL, 0}
};

static const EEL_lconstexp eph_constraintfields[] =
{
	{"a",		EPH_CA		},
	{"b",		EPH_CB		},
	{"Broken",	EPH_CBROKEN	},
	{"maxforce",	EPH_CMAXFORCE	},
	{"kind",	EPH_CKIND	},

	{"ax",		EPH_CAX		},
	{"ay",		EPH_CAY		},
	{"bx",		EPH_CBX		},
	{"by",		EPH_CBY		},
	{"d0",		EPH_CD0		},
	{"k",		EPH_CK		},
	{"k2",		EPH_CK2		},
	{"kd",		EPH_CKD		},

	{NULL, 0}
};


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/
static EEL_xno eph_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		if(eph_md.spacefields)
			eel_disown(eph_md.spacefields);
		if(eph_md.bodyfields)
			eel_disown(eph_md.bodyfields);
		if(eph_md.constraintfields)
			eel_disown(eph_md.constraintfields);
		eel_v_disown(&eph_md.cx);
		eel_v_disown(&eph_md.cy);
		eel_v_disown(&eph_md.ca);
		memset(&eph_md, 0, sizeof(eph_md));
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static EEL_object *fieldtable(EEL_vm *vm, const EEL_lconstexp *c)
{
	EEL_value v;
	EEL_object *t;
	EEL_xno x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return NULL;
	t = eel_v2o(&v);
	if(eel_insert_lconstants(t, c))
		return NULL;
	return t;
}

EEL_xno eph_init(EEL_vm *vm)
{
	EEL_object *c;
	EEL_object *m = eel_create_module(vm, "physicsbase", eph_unload, &eph_md);
	memset(&eph_md, 0, sizeof(eph_md));
	if(!m)
		return EEL_XMODULEINIT;

	/* Get EEL strings for quick indexing */
	eel_s2v(vm, &eph_md.cx, "cx");
	eel_s2v(vm, &eph_md.cy, "cy");
	eel_s2v(vm, &eph_md.ca, "ca");

	/* Internal "prototype" tables for core object fields */
	eph_md.spacefields = fieldtable(vm, eph_spacefields);
	eph_md.bodyfields = fieldtable(vm, eph_bodyfields);
	eph_md.constraintfields = fieldtable(vm, eph_constraintfields);
	if(!eph_md.spacefields || !eph_md.bodyfields || !eph_md.constraintfields)
	{
		eel_disown(m);
		return EEL_XMEMORY;
	}

	/* Register class 'space' */
	c = eel_export_class(m, "physspace", -1, space_construct, space_destruct,
			NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, space_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, space_setindex);
	eph_md.space_cid = eel_class_typeid(c);

	/* Register class 'body' */
	c = eel_export_class(m, "physbody", -1, body_construct, body_destruct,
			NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, body_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, body_setindex);
	eph_md.body_cid = eel_class_typeid(c);

	/* Register class 'constraint' */
	c = eel_export_class(m, "physconstraint", -1, constraint_construct,
			constraint_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, constraint_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, constraint_setindex);
	eph_md.constraint_cid = eel_class_typeid(c);

	/* Low level functions */
	eel_export_cfunction(m, 0, "Advance", 1, 0, 0, eph_advance);
	eel_export_cfunction(m, 0, "Tween", 2, 0, 0, eph_tween);
	eel_export_cfunction(m, 0, "Frame", 1, 0, 0, eph_frame);
	eel_export_cfunction(m, 0, "Collide", 1, 0, 0, eph_collide);
	eel_export_cfunction(m, 0, "RenderShadows", 2, 0, 0, eph_rendershadows);
	eel_export_cfunction(m, 0, "Render", 2, 0, 0, eph_render);
	eel_export_cfunction(m, 0, "Kill", 1, 0, 0, eph_kill);
	eel_export_cfunction(m, 0, "KillAll", 1, 0, 0, eph_killall);
	eel_export_cfunction(m, 0, "Clean", 1, 0, 0, eph_clean);
	eel_export_cfunction(m, 0, "Velocity", 3, 1, 0, eph_velocity);

	/* Forces */
	eel_export_cfunction(m, 0, "Force", 3, 0, 0, eph_force);
	eel_export_cfunction(m, 0, "ForceAt", 5, 0, 0, eph_forceat);
	eel_export_cfunction(m, 0, "ForceAtV", 5, 0, 0, eph_forceatv);
	eel_export_cfunction(m, 0, "ForceRel", 3, 0, 0, eph_forcerel);
	eel_export_cfunction(m, 0, "ForceAtRel", 5, 0, 0, eph_forceatrel);

	/* Constraints */
	eel_export_cfunction(m, 0, "ApplyConstraints", 1, 0, 0, eph_applyconstraints);
	eel_export_cfunction(m, 1, "FirstConstraint", 1, 0, 0, eph_firstconstraint);
	eel_export_cfunction(m, 1, "NextConstraint", 1, 0, 0, eph_nextconstraint);

	/* Z map */
	eel_export_cfunction(m, 0, "InitZ", 4, 0, 0, eph_initz);
	eel_export_cfunction(m, 0, "SetZ", 4, 0, 0, eph_setz);
	eel_export_cfunction(m, 1, "GetZ", 3, 0, 0, eph_getz);
	eel_export_cfunction(m, 0, "CollideZ", 2, 0, 0, eph_collidez);
	eel_export_cfunction(m, 1, "TestLineZ", 7, 0, 0, eph_testlinez);

	/* Utilities */
	eel_export_cfunction(m, 1, "Rotate", 3, 0, 0, eph_rotate);
	eel_export_cfunction(m, 1, "Dot", 4, 0, 0, eph_dot);
	eel_export_cfunction(m, 1, "Cross", 4, 0, 0, eph_cross);
	eel_export_cfunction(m, 1, "Distance", 2, 0, 0, eph_distance);
	eel_export_cfunction(m, 1, "PointDistance", 6, 0, 0, eph_pointdistance);
	eel_export_cfunction(m, 1, "Direction", 2, 0, 0, eph_direction);
	eel_export_cfunction(m, 1, "FindFirst", 1, 2, 0, eph_findnext);
	eel_export_cfunction(m, 1, "FindNext", 1, 2, 0, eph_findnext);
	eel_export_cfunction(m, 1, "FindAt", 3, 1, 0, eph_findat);
	eel_export_cfunction(m, 1, "CircleInertia", 3, 0, 0, eph_circleinertia);
	eel_export_cfunction(m, 1, "DiskInertia", 2, 0, 0, eph_diskinertia);

	/* Constants and enums */
	eel_export_lconstants(m, eph_constants);

	eel_disown(m);
	return 0;
}
