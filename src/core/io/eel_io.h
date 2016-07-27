/*
---------------------------------------------------------------------------
	eel_io.h - EEL File and Memory File Classes
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009, 2016 David Olofson
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

#ifndef	EEL_IO_H
#define	EEL_IO_H

#include <stdio.h>
#include "EEL.h"
#include "EEL_types.h"

/*
 * Flags for EEL_file
 */
typedef enum
{
	EEL_FF_DONTCLOSE =	0x00000001
} EEL_fileflags;

/*
 * file
 */
typedef struct
{
	FILE		*handle;	/* (C file handle) */
	int		flags;		/* EEL_fileflags */
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
