/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_dir.h - EEL Directory Handling
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

#ifndef	EEL_DIR_H
#define	EEL_DIR_H

#include <sys/types.h>
#include <dirent.h>
#include "EEL.h"
#include "EEL_types.h"

/*
 * directory
 */
typedef struct
{
	DIR		*handle;
} EEL_directory;
EEL_MAKE_CAST(EEL_directory)

/*
 * Register module 'io', containing 'file', 'memfile' and related functions.
 */
EEL_xno eel_dir_init(EEL_vm *vm);

#endif	/* EEL_DIR_H */
