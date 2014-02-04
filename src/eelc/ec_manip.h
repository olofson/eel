/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_manip.h - Argument Manipulator
---------------------------------------------------------------------------
 * Copyright (C) 2004-2006, 2009, 2011-2012 David Olofson
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

#ifndef	EEL_EC_MANIP_H
#define	EEL_EC_MANIP_H

#include "ec_coder.h"
#include "e_operate.h"

typedef enum
{
	EEL_MVOID = 0,		/* Nothing! No operations possible. */
	EEL_MCONSTANT,		/* Constant value or object. (RO) */
	EEL_MRESULT,		/* Result in temporary register. (RO) */
	EEL_MREGISTER,		/* Result in unmanaged register. (RO) */
	EEL_MSTATVAR,		/* Static variable. (R/W) */
	EEL_MVARIABLE,		/* Local or upvalue variable. (R/W) */
	EEL_MARGUMENT,		/* Local or upvalue argument. (R/W) */
	EEL_MOPTARG,		/* Local or upvalue optional argument. (R/W) */
	EEL_MTUPARG,		/* Local or upvalue tuple argument. (R/W) */
 	EEL_MOP,		/* Operator. (RO) */
 	EEL_MCAST,		/* Cast operator. (RO) */
 	EEL_MINDEX,		/* Indexed object. (R/W) */
 	EEL_MARGS,		/* Full argument list of current func (R/W) */
 	EEL_MTUPARGS		/* Tuple argument list of current func (R/W) */
} EEL_manipkinds;

struct EEL_manipulator
{
	EEL_mlist	*parent;
	EEL_manipulator	*prev, *next;
	EEL_coder	*coder;
	EEL_manipkinds	kind;
	int		refcount;	/* For operator trees... */
	union {
		int		reg;		/* Register index */
		struct
		{
			EEL_value	v;
			int		index;	/* Index, if it table */
		} constant;
		struct
		{
			EEL_symbol	*s;
			int		level;	/* Upvalue level */
			int		r;	/* Register index */
		} variable;
		struct
		{
			EEL_symbol	*s;
			int		index;	/* Variable table index */
		} statvar;
		struct
		{
			EEL_symbol	*s;
			int		level;	/* Upvalue level */
			int		arg;	/* Argument/tuple member index */
		} argument;
		struct
		{
			EEL_manipulator	*left;
			EEL_manipulator	*right;
			EEL_operators	op;
		} op;
		struct
		{
			EEL_manipulator	*object;
			EEL_classes	type;
		} cast;
		struct
		{
			EEL_manipulator	*object;
			EEL_manipulator	*index;
		} index;
	} v;
};


/*----------------------------------------------------------
	Manipulater list/tree building API
------------------------------------------------------------
 * These objects are generated by expressions and other
 * constructs that generate, or refer to objects that can
 * be read and/or written. A manipulator is an interface
 * that can generate code to access an object of any type
 * at any location.
 */

/*
 * Add constant (objref or value) 'v'.
 */
void eel_m_constant(EEL_mlist *ml, EEL_value *v);

/* Allocate and add a result register. Returns the register index. */
int eel_m_result(EEL_mlist *ml);

/* Add result register 'r' to 'ml'. THE REGISTER IS NOT MANAGED! */
void eel_m_register(EEL_mlist *ml, int r);

/* Add a local or upvalue variable. */
void eel_m_variable(EEL_mlist *ml, EEL_symbol *s, int level);

/* Add a static variable. */
void eel_m_statvar(EEL_mlist *ml, EEL_symbol *s);

/* Add a local or upvalue function argument. */
void eel_m_argument(EEL_mlist *ml, EEL_symbol *s, int level);

/* Add a local or upvalue optional function argument. */
void eel_m_optarg(EEL_mlist *ml, EEL_symbol *s, int level);

/*
 * Add a local or upvalue tuple argument tuple member. Unlike other arguments,
 * this cannot be read or written; only indexed.
 */
void eel_m_tuparg(EEL_mlist *ml, EEL_symbol *s, int level);

/* Add an object reference. */
void eel_m_object(EEL_mlist *ml, EEL_object *o);

/*
 * Add 'left' combined with 'right' using 'op' to 'ml'.
 * If 'op' is a unary operator, 'left' is not used and may be NULL.
 */
void eel_m_op(EEL_mlist *ml, EEL_manipulator *left, EEL_operators op,
		EEL_manipulator *right);

/* Add 'object' cast to 'type' to 'ml'. */
void eel_m_cast(EEL_mlist *ml, EEL_manipulator *object, EEL_types type);

/*
 * Add 'object' indexed by 'i' to 'ml'.
 */
void eel_m_index(EEL_mlist *ml, EEL_manipulator *object, EEL_manipulator *i);

/* Delete 'm' from it's argument list. */
void eel_m_delete(EEL_manipulator *m);

/* Detach manipulator from any list it's in */
void eel_m_detach(EEL_manipulator *m);

/* Transfer 'm' from it's argument list to 'ml'. */
void eel_m_transfer(EEL_manipulator *m, EEL_mlist *ml);

/* Add full or tuple argument list of current function. */
void eel_m_args(EEL_mlist *ml);
void eel_m_tupargs(EEL_mlist *ml);


/*----------------------------------------------------------
	Manipulator information
----------------------------------------------------------*/

/* Returns 1 if 'm' is writable. */
int eel_m_writable(EEL_manipulator *m);

/*
 * Returns the register for direct reading of 'm'
 * if possible, otherwise -1.
 *
 * Note:
 *	WILL return register indices of stack variables,
 *	since reading those requires no special steps.
 */
int eel_m_direct_read(EEL_manipulator *m);

/*
 * Returns the register for direct writing of 'm'
 * if possible, otherwise -1.
 *
 * Note:
 *	This will NOT return register indices of stack
 *	variables, since those must be written using the
 *	INIT or ASSIGN instructions.
 */
int eel_m_direct_write(EEL_manipulator *m);

/*
 * Returns the constant value of 'm', provided 'm' is an
 * integer constant in the [0, 255] range, otherwise -1.
 */
int eel_m_direct_uint8(EEL_manipulator *m);

/*
 * Returns the constant value of 'm', provided 'm' is an
 * integer constant in the [-32768, 32767] range,
 * otherwise -100000.
 */
int eel_m_direct_short(EEL_manipulator *m);

/*
 * Returns 0 or 1 if 'm' evaluates to a constant value that
 * would be considered falso or true respectively, by
 * conditional jump instructions. Returns -1 otherwise.
 */
int eel_m_direct_bool(EEL_manipulator *m);

/*
 * Returns 1 if 'm' is a constant value, available at compile
 * time. Otherwise, it returns 0.
 */
int eel_m_is_constant(EEL_manipulator *m);

/*
 * Read value of 'm' into 'v'. 'm' has to be a constant
 * that can be evaluated at compile time.
 */
void eel_m_get_constant(EEL_manipulator *m, EEL_value *v);


/*----------------------------------------------------------
	Code generation tools
----------------------------------------------------------*/

/*
 * Make sure constant is in the table if needed.
 * Returns the constant table index, if there is one,
 * otherwise -1.
 */
int eel_m_prepare_constant(EEL_manipulator *m);

/* Copy value from 'm' to R[r]. */
void eel_m_read(EEL_manipulator *m, int r);

/* Push value of 'm' onto the argument stack. */
void eel_m_push(EEL_manipulator *m);

/* Copy value from R[r] to 'm'. */
void eel_m_write(EEL_manipulator *m, int r);

/* Copy value from 'from' to 'to' */
void eel_m_copy(EEL_manipulator *from, EEL_manipulator *to);

/* Perform operation 'op' with 'from' on 'to' */
void eel_m_operate(EEL_manipulator *from, EEL_operators op, EEL_manipulator *to);

/* Perform operation 'op' with 'from' on 'to', in-place */
void eel_m_ipoperate(EEL_manipulator *from, EEL_operators op, EEL_manipulator *to);

/*
 * Make sure that 'm' is fully evaluated as if for reading,
 * but don't bother generating the final result. (Use this to
 * make sure functions actually get called and stuff.)
 */
void eel_m_evaluate(EEL_manipulator *m);

#endif
