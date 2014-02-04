/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_context.h - Compiler Context
---------------------------------------------------------------------------
 * Copyright (C) 2002-2004, 2009 David Olofson
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

#ifndef	EEL_CONTEXT_H
#define	EEL_CONTEXT_H

#include "EEL.h"
#include "e_util.h"
#include "ec_parser.h"

EEL_DLLIST(ej_, EEL_context, endjumps, lastej, EEL_codemark, prev, next);
EEL_DLLIST(cj_, EEL_context, contjumps, lastcj, EEL_codemark, prev, next);

#endif
