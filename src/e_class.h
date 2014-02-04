/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_class.h - EEL Class Definition
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009 David Olofson
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

#ifndef EEL_E_CLASS_H
#define EEL_E_CLASS_H

#include "EEL_register.h"

typedef struct
{
	EEL_object		*name;
	EEL_unregister_cb	unregister;
	void			*classdata;
	EEL_ctor_cb		construct;
	EEL_dtor_cb		destruct;
	EEL_rector_cb		reconstruct;
	EEL_mm_cb		mmethods[EEL_MM__COUNT];
	EEL_types		typeid;
	EEL_types		ancestor;
	int			registered;
} EEL_classdef;
EEL_MAKE_CAST(EEL_classdef)
void eel_cclass_register(EEL_vm *vm);

/*
 * Shortcut for bootstrapping the class subsystem.
 * (You can't use eel_o_construct() before the
   EEL_CCLASS class is registered! Catch 22.)
 */
EEL_object *eel_cclass_construct(EEL_vm *vm);

/*
 * Trap method to throw XNOMETAMETHOD, so we don't have
 * to check for NULL pointers in the normal case.
 */
EEL_xno eel_cclass_no_method(EEL_object *object,
		EEL_value *op1, EEL_value *op2);

#endif	/* EEL_E_CLASS_H */
