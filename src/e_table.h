/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_table.h - EEL Table Class
---------------------------------------------------------------------------
 * Copyright (C) 2005-2006, 2009-2011 David Olofson
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
