/*
---------------------------------------------------------------------------
	e_table.h - EEL Table Class
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009-2011 David Olofson
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

#ifndef	EEL_E_TABLE_H
#define	EEL_E_TABLE_H

#include "EEL.h"
#include "EEL_types.h"

typedef struct EEL_tableitem EEL_tableitem;

typedef struct
{
	int		length;		/* # of items */
	int		asize;		/* Current size of array */
	EEL_tableitem	*items;
} EEL_table;

EEL_MAKE_CAST(EEL_table)
void eel_ctable_register(EEL_vm *vm);

/*
 * Handy wrappers for the EEL internals
 *
 * NOTE:
 *	These drop any reference ownerships of returned objects,
 *	and weakrefs are turned into "weak" objrefs! You need to
 *	explicitly take ownership if you want to keep the references. 
 */

/* Iteration */
EEL_tableitem *eel_table_get_item(EEL_object *to, int i);

/* Item access */
EEL_value *eel_table_get_key(EEL_tableitem *ti);
EEL_value *eel_table_get_value(EEL_tableitem *ti);

/* (Remaining tools moved to EEL_object.h)*/

#endif	/* EEL_E_TABLE_H */
