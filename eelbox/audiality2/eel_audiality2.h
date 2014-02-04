/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_audiality2.h - EEL Audiality 2 binding
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

#ifndef EEL_AUDIALITY2_H
#define EEL_AUDIALITY2_H

#include "EEL.h"
#include "Audiality2/audiality2.h"
#include "Audiality2/waves.h"

/* state */
typedef struct
{
	A2_state	*state;
	EEL_object	*table;
} EA2_state;
EEL_MAKE_CAST(EA2_state)

EEL_xno eel_audiality2_init(EEL_vm *vm);

#endif /* EEL_AUDIALITY2_H */
