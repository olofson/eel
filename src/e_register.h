/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_register.h - EEL compiler registry
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2004-2006, 2009, 2011 David Olofson
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

#ifndef	EEL_E_REGISTER_H
#define	EEL_E_REGISTER_H

#include <stdlib.h>
#include "EEL.h"
#include "e_operate.h"

/*----------------------------------------------------------
	ESS file support
----------------------------------------------------------*/
void eel_register_essx(EEL_vm *vm, EEL_types t, EEL_symbol *s);
void eel_register_essxop(EEL_vm *vm, int op, EEL_symbol *s);

/*----------------------------------------------------------
	Operator (C)
----------------------------------------------------------*/
/* Flags */
#define	EOPF_UNARY	0x10000000
#define	EOPF_BINARY	0x20000000

typedef struct EEL_operator
{
	/* Binary */
	EEL_operators	op;
	int		lpri, rpri;

	/* Unary */
	EEL_operators	un_op;
	int		un_pri;

	int		flags;		/* EOPF_* flags */
} EEL_operator;

static inline EEL_operator *eel_operator_new(void)
{
	return (EEL_operator *)calloc(1, sizeof(EEL_operator));
}

static inline void eel_operator_free(EEL_operator *op)
{
	free(op);
}

/*----------------------------------------------------------
	Other stuff
----------------------------------------------------------*/
/*
 * Register class 'name' with ID 'cid', ancestor 'ancestor',
 * constructor 'construct', destructor 'destruct' and
 * reconstructor ("stream deserializer") 'reconstruct'.
 *
 * If 'cid' is -1, the first free type ID is used.
 *
 * Unless 'name' starts with a '\001' character, 'name' is
 * also registered as a TK_SYM_TYPE in the compiler symbol
 * table, so the name can be used as a type name in EEL
 * source code.
 *
 * The class is marked as a descendant from class 'ancestor',
 * where -1 means the class is a root class.
 *
 * SUCCESS: Returns the type EEL_classdef object of the new class.
 * FAILURE: Returns NULL.
 */
EEL_object *eel_register_class(EEL_vm *vm,
		EEL_classes cid, const char *name, EEL_classes ancestor,
		EEL_ctor_cb construct, EEL_dtor_cb destruct,
		EEL_rector_cb reconstruct);

/*
 * Mark class 'classdef' as unused, so that it will be
 * destroyed as soon as there are no more instances of it.
 */
void eel_unregister_class(EEL_object *classdef);


/*
 * Register C function 'func' as 'name', taking 'args'
 * fixed arguments, and up to 'optargs' optional arguments
 * OR any number of groups of 'tupargs' arguments, and
 * returning 'results' results.
 *
 * 'results', 'args' and 'tupargs' must be values of 0 or
 * higher.
 *
 * 'optargs' may have the value of -1, meaning "any number
 * of optional arguments"; that is, roughly equivalent to
 * '...' in C/C++ function declarations.
 *
 * SUCCESS: Returns the EEL_function object.
 * FAILURE: Returns NULL.
 */
EEL_object *eel_register_cfunction(EEL_vm *vm,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func);

#endif	/* EEL_E_REGISTER_H */
