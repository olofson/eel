/*
---------------------------------------------------------------------------
	eel_physics.h - EEL 2.5D Physics
---------------------------------------------------------------------------
 * Copyright 2011-2012 David Olofson
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

#ifndef EEL_PHYSICS_H
#define EEL_PHYSICS_H

#include <stdint.h>
#include "EEL.h"

/*
 * Don't touch this! Won't work with constraints at this point - and this is a
 * clumsy, limited design anyway.
#define CONSTRAINTS_NEED_IMPULSEFORCES
*/


/*----------------------------------------------------------
	Constants and types
----------------------------------------------------------*/

/* Set to 1 to enable domain checks that throw EEL exceptions */
#define	EPH_DOMAIN_CHECKS	1

/* HACK: Body bounding circle scale factor for culling. */
#define	EPH_CULLSCALE		1.5f

/* HACK: This should of course depend on map sun elevation... */
#define	EPH_MAXSHADOWDIST	150

/* Hard velocity limit, to contain stability issues, avoiding NaNs, infs etc  */
#define	EPH_MAX_VELOCITY	100

/* Maximum arch step between zmap test points (z map tiles) */
#define	EPH_ZTEST_DENSITY	4

typedef enum
{
#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
	EPH_AUTORESETFORCES =	0x00000001,	/* Reset forces before Frame() */
	EPH_IMPULSEFORCES =	0x00000002,	/* Reset after one frame */
#endif
	EPH_RESPONSE =		0x00000010,	/* Body/body hit autoresponse */
	EPH_ZRESPONSE =		0x00000020	/* Z hit autoresponse */
} EPH_bodyflags;

#ifdef CONSTRAINTS_NEED_IMPULSEFORCES
#define	EPH_DEFAULT_BODY_FLAGS	\
		(EPH_AUTORESETFORCES | EPH_RESPONSE | EPH_ZRESPONSE)
#else
#define	EPH_DEFAULT_BODY_FLAGS	\
		(EPH_RESPONSE | EPH_ZRESPONSE)
#endif

/* Default RNG seed */
#define	EPH_DEFAULTRNGSEED	16576

/* Returns a pseudo random number in the range [0, 65535] */
static inline int eph_Random(uint32_t *nstate)
{
	*nstate *= 1566083941UL;
	(*nstate)++;
	return *nstate * (*nstate >> 16) >> 16;
}

static inline double eph_Randomf(uint32_t *nstate, double max)
{
	return eph_Random(nstate) * max / 65536.0f;
}


/* Physics integrator modes */
typedef enum
{
	EPH_EULER = 0,
	EPH_VERLET
} EPH_integmodes;

/* Output interpolation modes */
typedef enum
{
	EPH_LATEST = 0,		/* Latest position - no interpolation (debug) */
	EPH_TWEEN,		/* Interpolation between last two logic frames */
	EPH_EXTRAPOLATE,	/* Velocity extrapolation from last logic frame */
	EPH_SEMIEXTRA		/* TWEEN + EXTRAPOLATE average */
} EPH_outputmodes;

/* Tests for FindFirst() and FindNext(). */
typedef enum
{
	EPH_INVIEW =		0x00000100,	/* Bodies in view */
	EPH_NOTINVIEW =		0x00000200	/* Bodies NOT in view */
#if 0
/*TODO:*/
	EPH_INMOTION =		0x00001000,	/* Bodies in motion */
	EPH_NOTINMOTION =	0x00002000,	/* Bodies NOT in motion */
#endif
} EPH_testflags;

typedef double EPH_f;
typedef struct EPH_zmap EPH_zmap;
typedef struct EPH_space EPH_space;
typedef struct EPH_body EPH_body;
typedef struct EPH_constraint EPH_constraint;

#define	EPH_X		0
#define	EPH_Y		1
#define	EPH_A		2
#define	EPH_DIMS	3


/*----------------------------------------------------------
	Math tools
----------------------------------------------------------*/

static inline void eph_Rotate(EPH_f x, EPH_f y, EPH_f a,
		EPH_f *xo, EPH_f *yo)
{
	EPH_f cosa = cos(a);
	EPH_f sina = sin(a);
	*xo = x * cosa - y * sina;
	*yo = x * sina + y * cosa;
}

static inline EPH_f eph_Dot(EPH_f xa, EPH_f ya, EPH_f xb, EPH_f yb)
{
	return xa * xb + ya * yb;
}

static inline EPH_f eph_Cross(EPH_f xa, EPH_f ya, EPH_f xb, EPH_f yb)
{
	return xa * yb - ya * xb;
}

static inline EPH_f eph_Distance(EPH_f xa, EPH_f ya, EPH_f xb, EPH_f yb)
{
	EPH_f dx = xa - xb;
	EPH_f dy = ya - yb;
	return sqrt(dx*dx + dy*dy);
}

static inline EPH_f eph_Direction(EPH_f xa, EPH_f ya, EPH_f xb, EPH_f yb)
{
	return atan2(yb - ya, xb - xa);
}


/*----------------------------------------------------------
	Doubly linked lists
----------------------------------------------------------*/

#define	EPH_LINK(s, b, first, last)		\
	if(s->last)				\
	{					\
		s->last->next = b;		\
		b->prev = s->last;		\
		b->next = NULL;			\
		s->last = b;			\
	}					\
	else					\
	{					\
		s->first = s->last = b;		\
		b->next = b->prev = NULL;	\
	}

#define	EPH_UNLINK(s, b, first, last)		\
	if(b->prev)				\
		b->prev->next = b->next;	\
	else					\
		s->first = b->next;		\
	if(b->next)				\
	{					\
		b->next->prev = b->prev;	\
		b->next = NULL;			\
	}					\
	else					\
		s->last = b->prev;		\
	b->prev = NULL;


/*----------------------------------------------------------
	Z-map
----------------------------------------------------------*/

struct EPH_zmap
{
	unsigned	w, h;
	double		scale;
	unsigned char	off;	/* Z substitute when off the map */
	unsigned char	*data;	/* Z map 2D array */
};

static inline int eph_setz_raw(EPH_zmap *m, int x, int y, int v)
{
	if(x < 0 || y < 0)
		return EEL_XLOWINDEX;
	if(x >= m->w || y >= m->h)
		return EEL_XHIGHINDEX;
	m->data[y * m->w + x] = v;
	return 0;
}

static inline int eph_getz_raw(EPH_zmap *m, int x, int y)
{
	if(x < 0 || y < 0 || x >= m->w || y >= m->h)
		return m->off;
	return m->data[y * m->w + x];
}

static inline float eph_getzi_raw(EPH_zmap *m, float x, float y)
{
	/*
	 * Probably not correct for negative x or y, but that's irrelevant now,
	 * as the Z map starts at (0, 0) and extends in the positive direction!
	 */
	int xx = (int)x;
	int yy = (int)y;
	float fx = x - xx;
	float fy = y - yy;
	float z00 = eph_getz_raw(m, xx, yy);
	float z10 = eph_getz_raw(m, xx + 1, yy);
	float z01 = eph_getz_raw(m, xx, yy + 1);
	float z11 = eph_getz_raw(m, xx + 1, yy + 1);
	z00 = z00 * (1.0 - fx) + z10 * fx;
	z01 = z01 * (1.0 - fx) + z11 * fx;
	return z00 * (1.0 - fy) + z01 * fy;
}


/*----------------------------------------------------------
	Space
----------------------------------------------------------*/

struct EPH_space
{
	/* Active bodies */
	EPH_body	*first, *last;

	/* Constraints */
	EPH_constraint	*firstc, *lastc;
	int		clean_constraints;	/* Bodies marked for destruction! */

	/* Integration control */
	EPH_integmodes	integration;

	/* Output interpolation control */
	EPH_outputmodes	outputmode;

	EEL_object	*table;		/* Table for non-core members */

	/* Map Z and collisions */
	EPH_zmap	zmap;

	/* Collision responses */
	float		impact_damage_min;
	float		overlap_correct_max;

	float		impact_absorb;
	float		impact_return;
	float		impact_damage;
	float		overlap_correct;

	float		impact_absorb_z;
	float		impact_return_z;
	float		impact_damage_z;
	float		overlap_correct_z;

	/* View */
	double		vx, vy;		/* View position (WU) */
	double		vxmin, vymin;	/* View culling limits (WU) */
	double		vxmax, vymax;

	/* Accounting */
	unsigned	active;		/* Currently active bodies */
	unsigned	constraints;	/* Currently active constraints */

	/* Random numbers */
	uint32_t	rngstate;	/* Local RNG state */
};
EEL_MAKE_CAST(EPH_space)
typedef enum
{
	EPH_SOFFMAPZ =	0,
	EPH_SZMAPSCALE,
	EPH_SVX,
	EPH_SVY,
	EPH_SVXMIN,
	EPH_SVYMIN,
	EPH_SVXMAX,
	EPH_SVYMAX,
	EPH_SACTIVE,
	EPH_SCONSTRAINTS,
	EPH_SINTEGRATION,
	EPH_SOUTPUTMODE,

	EPH_SIMPACT_DAMAGE_MIN,
	EPH_SOVERLAP_CORRECT_MAX,

	EPH_SIMPACT_ABSORB,
	EPH_SIMPACT_RETURN,
	EPH_SIMPACT_DAMAGE,
	EPH_SOVERLAP_CORRECT,

	EPH_SIMPACT_ABSORB_Z,
	EPH_SIMPACT_RETURN_Z,
	EPH_SIMPACT_DAMAGE_Z,
	EPH_SOVERLAP_CORRECT_Z,

	EPH_SRNGSEED
} EPH_spacefields;


/*----------------------------------------------------------
	Body
----------------------------------------------------------*/

typedef enum
{
	EPH_FRAME = 0,
	EPH_HIT,
	EPH_HITZ,
	EPH_IMPACT,
	EPH_RENDER,
	EPH_RENDERSHADOW,
	EPH_CLEANUP,
	EPH_NBODYMETHODS
} EPH_bodymethods;

struct EPH_body
{
	EPH_body	*next, *prev;	/* Doubly linked list under space */
	EPH_space	*space;		/* Parent/owner space */

	/* "Wired" methods/callbacks */
	EEL_object	*methods[EPH_NBODYMETHODS];

	EEL_object	*table;		/* Table for non-core members */

	/* Interpolation/"tweening" */
	EPH_f	i[EPH_DIMS];		/* Display position and heading */
	EPH_f	o[EPH_DIMS];		/* Previous c*, for interpolation */

	/* Physics vectors */
	EPH_f	c[EPH_DIMS];		/* Current position and heading */
	EPH_f	v[EPH_DIMS];		/* Velocity */
	EPH_f	f[EPH_DIMS];		/* External forces */
	EPH_f	rf[EPH_DIMS];		/* Collision response forces */

	/* Physics parameters */
	EPH_f	u[EPH_DIMS];		/* Friction coefficients */
	EPH_f	cd[EPH_DIMS];		/* Drag coefficients */
	EPH_f	m1[EPH_DIMS];		/* Inverse mass and inertia */

	/* Collision detection */
	EPH_f		z;		/* Body Z level, for Z map collisions */
	EPH_f		r;		/* Radius for coarse collision culling */
	EPH_f		shadowr;	/* Shadow radius */
	unsigned	group;		/* Group this body belongs to */
	unsigned	hitmask;	/* Groups this body can collide with */

	/* Collision response */
	EPH_f		e;		/* Elasticity */

	// Various flags
	unsigned	flags;
	int		killed;		/* Marked for destruction! */
};
EEL_MAKE_CAST(EPH_body)

typedef enum
{
	EPH_BSPACE = 0,
	EPH_BFLAGS,
	EPH_BGROUP,
	EPH_BHITMASK,
	EPH_BZ,
	EPH_BR,
	EPH_BSHADOWR,
	EPH_BE,
	EPH_BM,		/* Mass */
	EPH_BMI,	/* Moment of inertia */

	/* Methods */
	EPH_B_METHODS = 0x100,

	/* Specially encoded for vector addressing */
	EPH_B_VECTORS = 0x200,
	EPH_BIX = 0x200, EPH_BIY, EPH_BIA,	/* Display (tweened/extrapolated) */
#if TWEEN_INTERPOLATE == 1
	EPH_BOX = 0x300, EPH_BOY, EPH_BOA,	/* Previous position */
#endif
	EPH_BCX = 0x400, EPH_BCY, EPH_BCA,	/* Current position */
	EPH_BVX = 0x500, EPH_BVY, EPH_BVA,	/* Velocity */
	EPH_BRFX = 0x600, EPH_BRFY, EPH_BRFA,	/* Response forces */
	EPH_BFX = 0x700, EPH_BFY, EPH_BFA,	/* External forces */
	EPH_BUX = 0x800, EPH_BUY, EPH_BUA,	/* Friction coeffs */
	EPH_BCDX = 0x900, EPH_BCDY, EPH_BCDA	/* Drag coefficients */
} EPH_bodyfields;


static inline void eph_LinkBody(EPH_space *s, EPH_body *b)
{
	EPH_LINK(s, b, first, last)
}

static inline void eph_UnlinkBody(EPH_space *s, EPH_body *b)
{
	EPH_UNLINK(s, b, first, last)
}


static inline void eph_ForceV(EPH_body *b, EPH_f fx, EPH_f fy)
{
	b->f[EPH_X] += fx;
	b->f[EPH_Y] += fy;
}

static inline void eph_Force(EPH_body *b, EPH_f a, EPH_f f)
{
	b->f[EPH_X] += f * cos(a);
	b->f[EPH_Y] += f * sin(a);
}

static inline void eph_ForceRel(EPH_body *b, EPH_f a, EPH_f f)
{
	a += b->c[EPH_A];
	b->f[EPH_X] += f * cos(a);
	b->f[EPH_Y] += f * sin(a);
}

static inline void eph_ForceAtV(EPH_body *b, EPH_f x, EPH_f y, EPH_f fx, EPH_f fy)
{
	b->f[EPH_X] += fx;
	b->f[EPH_Y] += fy;
	b->f[EPH_A] += x * fy - y * fx;
}

static inline void eph_ForceAt(EPH_body *b, EPH_f x, EPH_f y, EPH_f a, EPH_f f)
{
	eph_ForceAtV(b, x, y, f * cos(a), f * sin(a));
}

static inline void eph_ForceAtRel(EPH_body *b, EPH_f x, EPH_f y, EPH_f a, EPH_f f)
{
	EPH_f fx, fy;
	EPH_f cosr = cos(b->c[EPH_A]);
	EPH_f sinr = sin(b->c[EPH_A]);
	a += b->c[EPH_A];
	fx = f * cos(a);
	fy = f * sin(a);
	b->f[EPH_X] += fx;
	b->f[EPH_Y] += fy;
	b->f[EPH_A] += (x * cosr - y * sinr) * fy - (x * sinr + y * cosr) * fx;
}


/*----------------------------------------------------------
	Constraints
----------------------------------------------------------*/

typedef enum
{
	EPH_DAMPEDSPRING = 0
} EPH_constraints;

typedef struct EPH_spring
{
	float	ax, ay;		/* Body 'a' anchor position */
	float	bx, by;		/* Body 'b' anchor position */
	float	d0;		/* Relaxed length */
	float	k, k2;		/* Linear and quadratic spring constants */
	float	kd;		/* Damping factor */
} EPH_spring;

struct EPH_constraint
{
	EPH_constraint	*next, *prev;	/* Global doubly linked list */
	EPH_body	*a, *b;		/* Bodies involved */
	EEL_object	*broken_cb;	/* Broken() callback */
	float		maxforce;	/* Breaking force */
	EPH_constraints	kind;		/* Kind of constraint */
	union {
		EPH_spring	spring;
	} p;
};
EEL_MAKE_CAST(EPH_constraint)

typedef enum
{
	/* Common fields */
	EPH_CA = 0,
	EPH_CB,
	EPH_CBROKEN,
	EPH_CMAXFORCE,
	EPH_CKIND,

	/* Parameters */
	EPH_CAX,
	EPH_CAY,
	EPH_CBX,
	EPH_CBY,
	EPH_CD0,
	EPH_CK,
	EPH_CK2,
	EPH_CKD
} EPH_constraintfields;

static inline void eph_LinkConstraint(EPH_space *s, EPH_constraint *c)
{
	EPH_LINK(s, c, firstc, lastc)
}

static inline void eph_UnlinkConstraint(EPH_space *s, EPH_constraint *c)
{
	EPH_UNLINK(s, c, firstc, lastc)
	/* NOTE:
	 *	The body links are weak, and will not be managed after the
	 *	constraint has been unlinked from the space!
	 */
	c->a = c->b = NULL;
}


/*
 * Returns 1 if the bodies that constraint 'c' is attached to are alive.
 *
 * NOTE:
 *	Constraint cleanup MUST be done before body cleanup, or there will be
 *	constraints with garbage pointers!
 *
 *	If 'b' is NULL, it is just assumed that the constraint does not use it,
 *	allowing the implementation of world/body constraints.
 */
static inline int eph_ConstraintIsAlive(EPH_constraint *c)
{
	if(!c->a || c->a->killed)	/* NOTE: Different from 'b'! */
		return 0;
	if(c->b && c->b->killed)
		return 0;
	return 1;
}


/*----------------------------------------------------------
	Init/cleanup
----------------------------------------------------------*/

EEL_xno eph_init(EEL_vm *vm);

#endif /* EEL_PHYSICS_H */
