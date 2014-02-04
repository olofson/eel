/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_io.h - EEL File and Memory File Classes
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

#ifndef	EEL_IO_H
#define	EEL_IO_H

#include <stdio.h>
#include "EEL.h"
#include "EEL_types.h"

/*
 * file
 */
typedef struct
{
	FILE		*handle;	/* (C file handle) */
} EEL_file;
EEL_MAKE_CAST(EEL_file)

/*
 * memfile
 */
typedef struct
{
	EEL_object	*buffer;	/* (dstring) */
	int		position;	/* current position (bytes) */
} EEL_memfile;
EEL_MAKE_CAST(EEL_memfile)

/*
 * Register module 'io', containing 'file', 'memfile' and related functions.
 */
EEL_xno eel_io_init(EEL_vm *vm);

#endif	/* EEL_IO_H */
