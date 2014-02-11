/*
---------------------------------------------------------------------------
	ec_manip.c - Argument Manipulator
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2012 David Olofson
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

#include "ec_mlist.h"
#include "ec_symtab.h"
#include "ec_event.h"
#include "e_util.h"
#include "e_object.h"
#include "e_string.h"
#include "e_function.h"

static void do_operate(EEL_manipulator *m, int r, int ip);
static void do_cast(EEL_manipulator *m, int r);
static void do_read_index(EEL_manipulator *m, int r);
static void do_write_index(EEL_manipulator *m, int r);

/*----------------------------------------------------------
	Linked list operators
----------------------------------------------------------*/
EEL_DLLIST(m_, EEL_mlist, args, lastarg, EEL_manipulator, prev, next)


static inline EEL_manipulator *m_open(EEL_mlist *ml, EEL_manipkinds kind)
{
	EEL_manipulator *m = (EEL_manipulator *)
			calloc(1, sizeof(EEL_manipulator));
	if(!m)
		eel_ierror(ml->coder->state,
				"Could not allocate memory for manipulator!");
	m->refcount = 1;
	m->kind = kind;

	/* Add as last argument */
	m->parent = ml;
	m->coder = ml->coder;
	m_link(ml, m);
	++ml->length;
	return m;
}


/* Release without messing with the list */
static void m_release(EEL_manipulator *m)
{
	if(1 == m->refcount)
		eel_m_delete(m);
	else
		--m->refcount;
}



/*----------------------------------------------------------
	Manipulater list/tree building API
----------------------------------------------------------*/

void eel_m_constant(EEL_mlist *ml, EEL_value *v)
{
	EEL_manipulator *m = m_open(ml, EEL_MCONSTANT);
	eel_v_copy(&m->v.constant.v, v);
	m->v.constant.index = -1;
}


int eel_m_result(EEL_mlist *ml)
{
	EEL_manipulator *m = m_open(ml, EEL_MRESULT);
	m->v.reg = eel_r_alloc(ml->coder, 1, EEL_RUTEMPORARY);
	return m->v.reg;
}


void eel_m_register(EEL_mlist *ml, int r)
{
	EEL_manipulator *m = m_open(ml, EEL_MREGISTER);
	m->v.reg = r;
}


void eel_m_variable(EEL_mlist *ml, EEL_symbol *s, int level)
{
	EEL_manipulator *m = m_open(ml, EEL_MVARIABLE);
	m->v.variable.s = s;
	m->v.variable.level = level;
	m->v.variable.r = s->v.var.location;
}


void eel_m_statvar(EEL_mlist *ml, EEL_symbol *s)
{
	EEL_manipulator *m = m_open(ml, EEL_MSTATVAR);
	m->v.statvar.s = s;
	m->v.statvar.index = s->v.var.location;
}


void eel_m_argument(EEL_mlist *ml, EEL_symbol *s, int level)
{
	EEL_manipulator *m = m_open(ml, EEL_MARGUMENT);
	m->v.argument.s = s;
	m->v.argument.level = level;
	m->v.argument.arg = s->v.var.location;
}


void eel_m_optarg(EEL_mlist *ml, EEL_symbol *s, int level)
{
	EEL_manipulator *m = m_open(ml, EEL_MOPTARG);
	m->v.argument.s = s;
	m->v.argument.level = level;
	m->v.argument.arg = s->v.var.location;
}


void eel_m_tuparg(EEL_mlist *ml, EEL_symbol *s, int level)
{
	EEL_manipulator *m = m_open(ml, EEL_MTUPARG);
	m->v.argument.s = s;
	m->v.argument.level = level;
	m->v.argument.arg = s->v.var.location;
}


void eel_m_object(EEL_mlist *ml, EEL_object *o)
{
	EEL_value v;
	v.type = EEL_TOBJREF;
	v.objref.v = o;
	eel_m_constant(ml, &v);
}


void eel_m_op(EEL_mlist *ml, EEL_manipulator *left, EEL_operators op,
		EEL_manipulator *right)
{
	EEL_manipulator *m = m_open(ml, EEL_MOP);
	m->v.op.left = left;
	m->v.op.right = right;
	if(left)
		++left->refcount;
	++right->refcount;
	m->v.op.op = op;
}


void eel_m_cast(EEL_mlist *ml, EEL_manipulator *object, EEL_types type)
{
	EEL_manipulator *m = m_open(ml, EEL_MCAST);
	m->v.cast.object = object;
	m->v.cast.type = type;
	++object->refcount;
}


void eel_m_index(EEL_mlist *ml, EEL_manipulator *object, EEL_manipulator *i)
{
	EEL_manipulator *m = m_open(ml, EEL_MINDEX);
	m->v.index.object = object;
	m->v.index.index = i;
	++object->refcount;
	++i->refcount;
}


void eel_m_args(EEL_mlist *ml)
{
	m_open(ml, EEL_MARGS);
}


void eel_m_tupargs(EEL_mlist *ml)
{
	m_open(ml, EEL_MTUPARGS);
}


/*
 * Detach manipulator from any list it's in.
 * The refcount is either taken over from the list,
 * or incremented.
 */
void eel_m_detach(EEL_manipulator *m)
{
	if(m->parent)
	{
		EEL_mlist *ml = m->parent;
		m_unlink(ml, m);
		--ml->length;
		m->parent = NULL;
	}
	else
		++m->refcount;
}


/* Delete from the manipulator list */
void eel_m_delete(EEL_manipulator *m)
{
	if(m->parent)
	{
		EEL_mlist *ml = m->parent;
		m_unlink(ml, m);
		--ml->length;
		m->parent = NULL;
	}
	if(--m->refcount)
		return;

	switch(m->kind)
	{
	  case EEL_MVOID:
		break;
	  case EEL_MCONSTANT:
		eel_v_disown(&m->v.constant.v);
		break;
	  case EEL_MRESULT:
		eel_r_free(m->coder, m->v.reg, 1);
		break;
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
		break;
	  case EEL_MOP:
		if(m->v.op.left)
			m_release(m->v.op.left);
		m_release(m->v.op.right);
		break;
	  case EEL_MCAST:
		m_release(m->v.cast.object);
		break;
	  case EEL_MINDEX:
		m_release(m->v.index.object);
		m_release(m->v.index.index);
		break;
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	free(m);
}


void eel_m_transfer(EEL_manipulator *m, EEL_mlist *ml)
{
	eel_m_detach(m);

	/* Add as last argument */
	m->parent = ml;
	m->coder = ml->coder;
	m_link(ml, m);
	++ml->length;
}


/*----------------------------------------------------------
	Manipulator information
----------------------------------------------------------*/

int eel_m_writable(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MVOID:
	  case EEL_MCONSTANT:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
		return 0;
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
#if 0
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
#endif
	  case EEL_MINDEX:	/* Can't know at compile time, actually... */
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		return 1;
	}
	return 0;
}


int eel_m_direct_read(EEL_manipulator *m)
{
	EEL_coder *cdr = m->coder;
	switch(m->kind)
	{
	  case EEL_MVOID:
	  case EEL_MCONSTANT:
		return -1;
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
		return m->v.reg;
	  case EEL_MVARIABLE:
	  {
	  	EEL_symbol *s;
		if(m->v.variable.level)
			return -1;
		s = m->v.variable.s;
		switch(eel_test_init(cdr->state, s))
		{
		  case EEL_ENO:
			eel_cerror(cdr->state, "Reading uninitialized "
					"variable '%s'!\n"
					"Maybe you misspelled the name?",
					eel_o2s(s->name));
		  case EEL_EYES:
			break;
		  case EEL_EMAYBE:
			eel_cerror(cdr->state, "Reading variable"
					" '%s', which may be"
					" uninitialized at this"
					" point!", eel_o2s(s->name));
		}
		return m->v.variable.r;
	  }
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		return -1;
	}
	return -1;
}


int eel_m_direct_write(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MVOID:
	  case EEL_MCONSTANT:
		return -1;
	  case EEL_MRESULT:
		return m->v.reg;
	  case EEL_MVARIABLE:
/*
FIXME: Apply type tracking logic here!
FIXME: Initializing variables with simple values is ok as well.
*/
		return -1;
	  case EEL_MREGISTER:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		return -1;
	}
	return -1;
}


int eel_m_direct_uint8(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MCONSTANT:
		if(m->v.constant.v.type != EEL_TINTEGER)
			return -1;
		if(m->v.constant.v.integer.v < 0)
			return -1;
		if(m->v.constant.v.integer.v > 255)
			return -1;
		return m->v.constant.v.integer.v;
	  case EEL_MVOID:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	return -1;
}


int eel_m_direct_short(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MCONSTANT:
		if(m->v.constant.v.type != EEL_TINTEGER)
			return -100000;
		if(m->v.constant.v.integer.v < -32768)
			return -100000;
		if(m->v.constant.v.integer.v > 32767)
			return -100000;
		return m->v.constant.v.integer.v;
	  case EEL_MVOID:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	return -100000;
}


int eel_m_direct_bool(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MCONSTANT:
		switch(m->v.constant.v.type)
		{
		  case EEL_TNIL:
			return 0;
		  case EEL_TREAL:
			return m->v.constant.v.real.v != 0.0;
		  case EEL_TINTEGER:
			return m->v.constant.v.integer.v != 0;
		  case EEL_TBOOLEAN:
			return m->v.constant.v.integer.v;
		  case EEL_TTYPEID:
			return 1;
		  case EEL_TOBJREF:	/* An object is "something". (nil is nothing.) */
		  case EEL_TWEAKREF:
			return 1;
		}
		return -1;
	  case EEL_MVOID:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	return -1;
}


int eel_m_is_constant(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MCONSTANT:
		return 1;
	  case EEL_MVOID:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	return 0;
}


void eel_m_get_constant(EEL_manipulator *m, EEL_value *v)
{
	EEL_coder *cdr = m->coder;
	switch(m->kind)
	{
	  case EEL_MCONSTANT:
		eel_v_copy(v, &m->v.constant.v);
		return;
	  case EEL_MVOID:
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
	  case EEL_MVARIABLE:
	  case EEL_MSTATVAR:
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		break;
	}
	eel_ierror(cdr->state, "eel_m_get_constant() used on something"
			" not a constant!");
}


/*----------------------------------------------------------
	Code generation tools
----------------------------------------------------------*/

static void do_operate(EEL_manipulator *m, int r, int ip)
{
	EEL_coder *cdr = m->coder;
	int lr, rr, rv, ins = 0;
	switch(m->v.op.op)
	{
	  case EEL_OP_POWER:
	  case EEL_OP_MOD:
	  case EEL_OP_DIV:
	  case EEL_OP_MUL:
	  case EEL_OP_SUB:
	  case EEL_OP_ADD:
	  case EEL_OP_VPOWER:
	  case EEL_OP_VMOD:
	  case EEL_OP_VDIV:
	  case EEL_OP_VMUL:
	  case EEL_OP_VSUB:
	  case EEL_OP_VADD:
	  case EEL_OP_BAND:
	  case EEL_OP_BOR:
	  case EEL_OP_BXOR:
	  case EEL_OP_SHL:
	  case EEL_OP_SHR:
	  case EEL_OP_ROL:
	  case EEL_OP_ROR:
	  case EEL_OP_BREV:
	  case EEL_OP_AND:
	  case EEL_OP_OR:
	  case EEL_OP_XOR:
	  case EEL_OP_EQ:
	  case EEL_OP_NE:
	  case EEL_OP_GT:
	  case EEL_OP_GE:
	  case EEL_OP_LT:
	  case EEL_OP_LE:
	  case EEL_OP_IN:
	  case EEL_OP_MIN:
	  case EEL_OP_MAX:
		if(!m->v.op.left)
			eel_ierror(cdr->state, "Left hand operand to binary"
					" operator missing!");
		/* Code binary operators */
		lr = eel_m_direct_read(m->v.op.left);
		if(lr < 0)
		{
			lr = r;
			eel_m_read(m->v.op.left, lr);
		}
		rr = eel_m_direct_read(m->v.op.right);
		rv = eel_m_direct_short(m->v.op.right);
		if(rr >= 0)
			eel_codeABCD(cdr, ip ? EEL_OIPBOP_ABCD : EEL_OBOP_ABCD,
					r, lr, m->v.op.op, rr);
		else if(rv != -100000)
			eel_codeABCsDx(cdr, ip ? EEL_OIPBOPI_ABCsDx :
					EEL_OBOPI_ABCsDx,
					r, lr, m->v.op.op, rv);
		else if(m->v.op.right->kind == EEL_MSTATVAR)
			eel_codeABCsDx(cdr, ip ? EEL_OIPBOPS_ABCsDx :
					EEL_OBOPS_ABCsDx,
					r, lr, m->v.op.op,
					m->v.op.right->v.statvar.index);
		else
		{
			rr = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(m->v.op.right, rr);
			eel_codeABCD(cdr, ip ? EEL_OIPBOP_ABCD :
					EEL_OBOP_ABCD,
					r, lr, m->v.op.op, rr);
			eel_r_free(cdr, rr, 1);
		}
		return;
	  case EEL_OP_NEG:
		ins = EEL_ONEG_AB;
		break;
	  case EEL_OP_NOT:
		ins = EEL_ONOT_AB;
	  	break;
	  case EEL_OP_CASTR:
		ins = EEL_OCASTR_AB;
	  	break;
	  case EEL_OP_CASTI:
		ins = EEL_OCASTI_AB;
	  	break;
	  case EEL_OP_CASTB:
		ins = EEL_OCASTB_AB;
	  	break;
	  case EEL_OP_TYPEOF:
		ins = EEL_OTYPEOF_AB;
	  	break;
	  case EEL_OP_SIZEOF:
		/*
		 * Special handling for 'sizeof tuple_argument', as arguments
		 * are no longer accessed via LISTs. (No LISTs as of 0.3.0.)
		 */
		if(m->v.op.right->kind == EEL_MTUPARG)
		{
			eel_codeA(cdr, EEL_OTUPC_A, r);
			return;
		}
		ins = EEL_OSIZEOF_AB;
	  	break;
	  case EEL_OP_CLONE:
		ins = EEL_OCLONE_AB;
	  	break;
	  case EEL_OP_BNOT:
		ins = EEL_OBNOT_AB;
	  	break;
	  case EEL_OP_ASSIGN:
	  case EEL_OP_WKASSN:
		eel_ierror(cdr->state, "ASSIGN and WKASSN operators not (yet) "
				"handled by do_operate()! Use eel_m_ipop().");
	}
	if(!ins)
		eel_ierror(cdr->state, "Operator not implemented!");

	/* Code unary operators */
	rr = eel_m_direct_read(m->v.op.right);
	if(rr < 0)
	{
		rr = r;
		eel_m_read(m->v.op.right, rr);
	}
	eel_codeAB(cdr, ins, r, rr);
}


static void do_cast(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	int dr = eel_m_direct_read(m->v.cast.object);
	if(dr < 0)
	{
		dr = r;
		eel_m_read(m->v.cast.object, dr);
	}
	switch((EEL_types)m->v.cast.type)
	{
	  case EEL_TREAL:
		eel_codeAB(cdr, EEL_OCASTR_AB, r, dr);
	  	break;
	  case EEL_TINTEGER:
		eel_codeAB(cdr, EEL_OCASTI_AB, r, dr);
	  	break;
	  case EEL_TBOOLEAN:
		eel_codeAB(cdr, EEL_OCASTB_AB, r, dr);
	  	break;
	  default:
	  {
		int cr = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
#if 0
	  	if(m->v.cast.type <= 255)
		{
			eel_codeAB(cdr, EEL_OLDTYPE_AB, cr, m->v.cast.type);
			eel_codeABC(cdr, EEL_OCAST_ABC, r, dr, cr);
		}
		else
#endif
		{
			EEL_value v;
			int c;
			v.type = EEL_TTYPEID;
			v.integer.v = m->v.cast.type;
			c = eel_coder_add_constant(cdr, &v);
			eel_codeABx(cdr, EEL_OLDC_ABx, cr, c);
			eel_codeABC(cdr, EEL_OCAST_ABC, r, dr, cr);
		}
		eel_r_free(cdr, cr, 1);
		break;
	  }
	}
}


static void do_read_index_obj(EEL_manipulator *m, int r)
{
	int or, ir, iv;
	EEL_coder *cdr = m->coder;
	EEL_manipulator *om = m->v.index.object;
	EEL_manipulator *im = m->v.index.index;
	or = eel_m_direct_read(om);
	if(or < 0)
	{
		/* Use r as intermediate reg for OBJECT */
		eel_m_read(om, r);
		or = r;
	}
	ir = eel_m_direct_read(im);
	iv = eel_m_direct_uint8(im);
	if(ir >= 0)
		eel_codeABC(cdr, EEL_OINDGET_ABC, r, ir, or);
	else if(iv >= 0)
		eel_codeABC(cdr, EEL_OINDGETI_ABC, r, iv, or);
	else
	{
		ir = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(im, ir);
		eel_codeABC(cdr, EEL_OINDGET_ABC, r, ir, or);
		eel_r_free(cdr, ir, 1);
	}
}

static void do_read_index_tuparg(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	EEL_manipulator *tam = m->v.index.object;
	EEL_manipulator *im = m->v.index.index;
	int ir = eel_m_direct_read(im);
	if(tam->v.argument.level)
	{
		if(ir >= 0)
			eel_codeABCD(cdr, EEL_OGETUVTARGI_ABCD, r,
					tam->v.argument.arg,
				     	ir,
					tam->v.argument.level);
		else
		{
			ir = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(im, ir);
			eel_codeABCD(cdr, EEL_OGETUVTARGI_ABCD, r,
					tam->v.argument.arg,
					ir,
					tam->v.argument.level);
			eel_r_free(cdr, ir, 1);
		}
	}
	else
	{
		if(ir >= 0)
			eel_codeABC(cdr, EEL_OGETTARGI_ABC, r,
					tam->v.argument.arg, ir);
		else
		{
			ir = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(im, ir);
			eel_codeABC(cdr, EEL_OGETTARGI_ABC, r,
					tam->v.argument.arg, ir);
			eel_r_free(cdr, ir, 1);
		}
	}
}

static inline void do_read_index(EEL_manipulator *m, int r)
{
	if(m->v.index.object->kind == EEL_MTUPARG)
		do_read_index_tuparg(m, r);
	else
		do_read_index_obj(m, r);
}


static void do_write_index_tuparg(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	eel_cerror(cdr->state, "Writing tuple arguments not yet implemented!");
}

static void do_write_index_obj(EEL_manipulator *m, int r)
{
	int or, ir, iv;
	EEL_coder *cdr = m->coder;
	EEL_manipulator *om = m->v.index.object;
	EEL_manipulator *im = m->v.index.index;

	/* Read object reference or whatever */
	or = eel_m_direct_read(om);
	if(or < 0)
	{
		or = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(om, or);
	}

	ir = eel_m_direct_read(im);
	iv = eel_m_direct_uint8(im);
	if(ir >= 0)
		eel_codeABC(cdr, EEL_OINDSET_ABC, r, ir, or);
	else if(iv >= 0)
		eel_codeABC(cdr, EEL_OINDSETI_ABC, r, iv, or);
	else
	{
		ir = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(im, ir);
		eel_codeABC(cdr, EEL_OINDSET_ABC, r, ir, or);
		eel_r_free(cdr, ir, 1);
	}

	if(eel_m_direct_read(om) < 0)
		eel_r_free(cdr, or, 1);
}

static inline void do_write_index(EEL_manipulator *m, int r)
{
	if(m->v.index.object->kind == EEL_MTUPARG)
		do_write_index_tuparg(m, r);
	else
		do_write_index_obj(m, r);
}


int eel_m_prepare_constant(EEL_manipulator *m)
{
	EEL_value *cv;
	int *ci;
	EEL_coder *cdr = m->coder;
	if(m->kind != EEL_MCONSTANT)
		eel_ierror(cdr->state, "eel_m_prepare_constant(): Hey, "
				"that's no constant!");
	cv = &m->v.constant.v;
	ci = &m->v.constant.index;
	if(*ci >= 0)
		return *ci;
	switch(cv->type)
	{
	  case EEL_TNIL:
	  case EEL_TBOOLEAN:
		return -1;
	  case EEL_TINTEGER:
		if((cv->integer.v >= -32768) && (cv->integer.v <= 32767))
			return -1;
		else
			return (*ci = eel_coder_add_constant(cdr, cv));
	  case EEL_TTYPEID:
#if 0
		if(cv->integer.v <= 255)
			return -1;
		/* ...else fall through! */
#endif
	  case EEL_TREAL:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		/* Add/find constant and try again! */
		return (*ci = eel_coder_add_constant(cdr, cv));
	}
	eel_ierror(cdr->state, "CONSTANT manipulator with illegal value type!");
}


static void read_constant(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	EEL_value *cv = &m->v.constant.v;
	int *ci = &m->v.constant.index;

	if(*ci >= 0)
	{
		/* Ok, it's in the constant table; just grab it... */
		eel_codeABx(cdr, EEL_OLDC_ABx, r, *ci);
		return;
	}
	else if(eel_m_prepare_constant(m) >= 0)
	{
		read_constant(m, r);	/* Try again! */
		return;
	}

	/*
	 * No constant in table!
	 * Lets figure out some nice way to
	 * generate this value or something.
	 */
	switch(cv->type)
	{
	  case EEL_TNIL:
		eel_codeA(cdr, EEL_OLDNIL_A, r);
		return;
	  case EEL_TINTEGER:
		if((cv->integer.v >= -32768) && (cv->integer.v <= 32767))
		{
			eel_codeAsBx(cdr, EEL_OLDI_AsBx, r, cv->integer.v);
			return;
		}
	  case EEL_TBOOLEAN:
		if(cv->integer.v)
			eel_codeA(cdr, EEL_OLDTRUE_A, r);
		else
			eel_codeA(cdr, EEL_OLDFALSE_A, r);
		return;
#if 0
	  case EEL_TTYPEID:
		if(cv->integer.v <= 255)
		{
			eel_codeAB(cdr, EEL_OLDTYPE_AB, r, cv->integer.v);
			return;
		}
#endif
	  case EEL_TTYPEID:
	  case EEL_TREAL:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	}
	eel_ierror(cdr->state, "CONSTANT manipulator failed to generate value!");
}


static void push_constant(EEL_manipulator *m)
{
	EEL_coder *cdr = m->coder;
	EEL_value *cv = &m->v.constant.v;
	int *ci = &m->v.constant.index;

	if(*ci >= 0)
	{
		/* Ok, it's in the constant table; just grab it... */
		eel_codeAx(cdr, EEL_OPUSHC_Ax, *ci);
		return;
	}
	else if(eel_m_prepare_constant(m) >= 0)
	{
		push_constant(m);	/* Try again! */
		return;
	}

	/*
	 * No constant in table!
	 * Lets figure out some nice way to
	 * generate this value or something.
	 */
	switch(cv->type)
	{
	  case EEL_TNIL:
		eel_code0(cdr, EEL_OPUSHNIL_0);
		return;
	  case EEL_TINTEGER:
		if((cv->integer.v >= -32768) && (cv->integer.v <= 32767))
		{
			eel_codesAx(cdr, EEL_OPUSHI_sAx, cv->integer.v);
			return;
		}
	  case EEL_TBOOLEAN:
		if(cv->integer.v)
			eel_code0(cdr, EEL_OPHTRUE_0);
		else
			eel_code0(cdr, EEL_OPHFALSE_0);
		return;
#if 0
	  case EEL_TTYPEID:
		if(cv->integer.v <= 255)
		{
			eel_codeAB(cdr, EEL_OLDTYPE_AB, r, cv->integer.v);
			return;
		}
#endif
	  case EEL_TTYPEID:
	  case EEL_TREAL:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		break;
	}
	eel_ierror(cdr->state, "CONSTANT manipulator failed to generate value!");
}


void eel_m_read(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	EEL_function *f = o2EEL_function(cdr->f);
	switch(m->kind)
	{
	  case EEL_MVOID:
		eel_ierror(cdr->state, "Tried to read a void manipulator!");
	  case EEL_MCONSTANT:
		read_constant(m, r);
		break;
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
		eel_codeAB(cdr, EEL_OMOVE_AB, r, m->v.reg);
		break;
	  case EEL_MVARIABLE:
	  {
		EEL_symbol *s = m->v.variable.s;
		if(m->v.variable.level)
			eel_codeABC(cdr, EEL_OGETUVAL_ABC, r,
					m->v.variable.r, m->v.variable.level);
		else
		{
			switch(eel_test_init(cdr->state, s))
			{
			  case EEL_ENO:
				eel_cerror(cdr->state, "Reading uninitialized "
						"variable '%s'!\n"
						"Maybe you misspelled the name?",
						eel_o2s(s->name));
			  case EEL_EYES:
				break;
			  case EEL_EMAYBE:
				eel_cerror(cdr->state, "Reading variable"
						" '%s', which may be"
						" uninitialized at this"
						" point!", eel_o2s(s->name));
			}
			eel_codeAB(cdr, EEL_OMOVE_AB, r, m->v.variable.r);
		}
		break;
	  }
	  case EEL_MSTATVAR:
		eel_codeABx(cdr, EEL_OGETVAR_ABx, r, m->v.statvar.index);
		break;
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  {
		int arg = m->v.argument.arg;
		if(m->kind == EEL_MOPTARG)
			arg += f->common.reqargs;
		if(m->v.argument.level)
			eel_codeABC(cdr, EEL_OGETUVARGI_ABC, r, arg,
					m->v.argument.level);
		else
			eel_codeAB(cdr, EEL_OGETARGI_AB, r, arg);
 		break;
	  }
	  case EEL_MTUPARG:
		eel_cerror(cdr->state, "Tried to read tuparg array '%s'! "
				"(Can only be indexed.)",
				eel_o2s(m->v.variable.s->name));
	  case EEL_MOP:
		do_operate(m, r, 0);
		break;
	  case EEL_MCAST:
		do_cast(m, r);
		break;
	  case EEL_MINDEX:
		do_read_index(m, r);
		break;
	  case EEL_MARGS:
		eel_ierror(cdr->state, "Tried to read argument list into "
				"register!");
	  case EEL_MTUPARGS:
		eel_ierror(cdr->state, "Tried to read tuple argument list into "
				"register!");
	}
}


void eel_m_push(EEL_manipulator *m)
{
	EEL_coder *cdr = m->coder;
	switch(m->kind)
	{
	  case EEL_MVOID:
		eel_ierror(cdr->state, "Tried to push a void manipulator!");
	  case EEL_MCONSTANT:
		push_constant(m);
		break;
	  case EEL_MRESULT:
	  case EEL_MREGISTER:
		eel_codeA(cdr, EEL_OPUSH_A, m->v.reg);
		break;
	  case EEL_MVARIABLE:
	  {
		EEL_symbol *s = m->v.variable.s;
		if(m->v.variable.level)
			eel_codeAB(cdr, EEL_OPHUVAL_AB,
					m->v.variable.r, m->v.variable.level);
		else
		{
			switch(eel_test_init(cdr->state, s))
			{
			  case EEL_ENO:
				eel_cerror(cdr->state, "Pushing uninitialized "
						"variable '%s'!\n"
						"Maybe you misspelled the name?",
						eel_o2s(s->name));
			  case EEL_EYES:
				break;
			  case EEL_EMAYBE:
				eel_cerror(cdr->state, "Pushing variable"
						" '%s', which may be"
						" uninitialized at this"
						" point!", eel_o2s(s->name));
			}
			eel_codeA(cdr, EEL_OPUSH_A, m->v.variable.r);
		}
		break;
	  }
	  case EEL_MSTATVAR:
		eel_codeAx(cdr, EEL_OPHVAR_Ax, m->v.statvar.index);
		break;
	  case EEL_MTUPARG:
		eel_cerror(cdr->state, "Tried to push tuparg array '%s'! "
				"(Can only be indexed.)",
				eel_o2s(m->v.variable.s->name));
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MOP:
	  case EEL_MCAST:
	  case EEL_MINDEX:
	  {
/*
FIXME: Maybe we should just do all of them this way, and have the
FIXME: peephole optimizer make use of the shortcut instructions?
*/
		int r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(m, r);
		eel_codeA(cdr, EEL_OPUSH_A, r);
		eel_r_free(cdr, r, 1);
		break;
	  }
	  case EEL_MARGS:
		eel_code0(cdr, EEL_OPHARGS_0);
		break;
	  case EEL_MTUPARGS:
		eel_code0(cdr, EEL_OPUSHTUP_0);
		break;
	}
}


void eel_m_evaluate(EEL_manipulator *m)
{
/*
FIXME: This will be needed if/when "everything becomes a manipulator", to
FIXME: enforce evaluation of unused values when there are side effects.
*/
}


void eel_m_write(EEL_manipulator *m, int r)
{
	EEL_coder *cdr = m->coder;
	EEL_function *f = o2EEL_function(cdr->f);
	switch(m->kind)
	{
	  case EEL_MVOID:
		eel_ierror(cdr->state, "Tried to write a void manipulator!");
	  case EEL_MCONSTANT:
		eel_cerror(cdr->state, "Assignment to constant!");
	  case EEL_MREGISTER:
		eel_cerror(cdr->state, "Assignment to read-only term!");
	  case EEL_MRESULT:
		eel_codeAB(cdr, EEL_OMOVE_AB, m->v.reg, r);
		break;
	  case EEL_MVARIABLE:
	  {
		EEL_symbol *s = m->v.variable.s;
		if(m->v.variable.level)
			eel_codeABC(cdr, EEL_OSETUVAL_ABC, r,
					m->v.variable.r,
					m->v.variable.level);
		else
		{
			switch(eel_test_init(cdr->state, s))
			{
			  case EEL_ENO:
				eel_codeAB(cdr, EEL_OINIT_AB,
						m->v.variable.r, r);
				eel_e_init(cdr->state, s);
				break;
			  case EEL_EYES:
				eel_codeAB(cdr, EEL_OASSIGN_AB,
					m->v.variable.r, r);
				break;
			  case EEL_EMAYBE:
/*
FIXME: Is this actually an internal error now...?
*/
				eel_cerror(cdr->state, "Cannot tell if "
						"variable '%s' is "
						"initialized at this point!",
						eel_o2s(s->name));
				break;
			}
		}
		break;
	  }
	  case EEL_MSTATVAR:
		eel_codeABx(cdr, EEL_OSETVAR_ABx, r, m->v.statvar.index);
		break;
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  {
		int arg = m->v.argument.arg;
		if(m->kind == EEL_MOPTARG)
			arg += f->common.reqargs;
		if(m->v.argument.level)
			eel_codeABC(cdr, EEL_OSETUVARGI_ABC, r, arg,
					m->v.argument.level);
		else
			eel_codeAB(cdr, EEL_OSETARGI_AB, r, arg);
		break;
	  }
	  case EEL_MTUPARG:
		eel_cerror(cdr->state, "Tried to write to tuparg array '%s'! "
				"(Can only be indexed.)",
				eel_o2s(m->v.variable.s->name));
	  case EEL_MOP:
	  case EEL_MCAST:
		eel_cerror(cdr->state, "Assignment to read-only expression!");
	  case EEL_MINDEX:
		do_write_index(m, r);
		break;
	  case EEL_MARGS:
		eel_ierror(cdr->state, "Tried to write to argument list!");
	  case EEL_MTUPARGS:
		eel_ierror(cdr->state, "Tried to write to tuple argument list!");
	}
}


static int can_write_weakref(EEL_manipulator *m)
{
	switch(m->kind)
	{
	  case EEL_MVOID:
	  case EEL_MCONSTANT:
	  case EEL_MREGISTER:
	  case EEL_MRESULT:
	  case EEL_MVARIABLE:
		return 0;
	  case EEL_MSTATVAR:
		return 1;
	  case EEL_MARGUMENT:
	  case EEL_MOPTARG:
	  case EEL_MTUPARG:
	  case EEL_MOP:
	  case EEL_MCAST:
		return 0;
	  case EEL_MINDEX:
		return 1;
	  case EEL_MARGS:
	  case EEL_MTUPARGS:
		return 0;
	}
	return 0;
}


void eel_m_copy(EEL_manipulator *from, EEL_manipulator *to)
{
	EEL_coder *cdr = from->coder;
	int dr = eel_m_direct_read(from);
	int dw = eel_m_direct_write(to);
	if(dr >= 0)
		eel_m_write(to, dr);
	else if(dw >= 0)
		eel_m_read(from, dw);
	else
	{
		/* Fallback - use a temporary register */
		int r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		eel_m_read(from, r);
		eel_m_write(to, r);
		eel_r_free(cdr, r, 1);
	}
}


static void apply_op(EEL_manipulator *from, EEL_operators op,
		EEL_manipulator *to, int inplace)
{
	EEL_coder *cdr = from->coder;
	EEL_mlist *ml = eel_ml_open(cdr);
	EEL_manipulator *m;
	int dw = eel_m_direct_write(to);
	eel_m_op(ml, to, op, from);
	m = eel_ml_get(ml, 0);
	if(dw >= 0)
		do_operate(m, dw, inplace);
	else
	{
		int r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		do_operate(m, r, inplace);
		if(!inplace)
			eel_m_write(to, r);
		eel_r_free(cdr, r, 1);
	}
	eel_ml_close(ml);
}


void eel_m_operate(EEL_manipulator *from, EEL_operators op, EEL_manipulator *to)
{
	EEL_coder *cdr = from->coder;
	EEL_state *es = cdr->state;
	switch(op)
	{
	  case EEL_OP_ASSIGN:
		eel_m_copy(from, to);
		break;
	  case EEL_OP_WKASSN:
	  {
		int dr;
		int wr = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
		if(!can_write_weakref(to))
			eel_cerror(es, "Weak assignment to target that cannot "
					"receive weak references!");
		dr = eel_m_direct_read(from);
		if(dr >= 0)
			eel_codeAB(cdr, EEL_OWEAKREF_AB, wr, dr);
		else
		{
			int r = eel_r_alloc(cdr, 1, EEL_RUTEMPORARY);
			eel_m_read(from, r);
			eel_codeAB(cdr, EEL_OWEAKREF_AB, wr, r);
			eel_r_free(cdr, r, 1);
		}
		eel_m_write(to, wr);
		eel_r_free(cdr, wr, 1);
		break;
	  }
	  default:
		apply_op(from, op, to, 0);
		break;
	}
}


void eel_m_ipoperate(EEL_manipulator *from, EEL_operators op, EEL_manipulator *to)
{
	if(op == EEL_OP_ASSIGN)
		eel_m_copy(from, to);
	else
		apply_op(from, op, to, 1);
}
