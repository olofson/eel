/*
---------------------------------------------------------------------------
	e_class.h - EEL Class Definition
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009 David Olofson
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

#ifndef EEL_E_CLASS_H
#define EEL_E_CLASS_H

#include "EEL_register.h"
#include "e_config.h"

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
