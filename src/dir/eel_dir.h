/*
---------------------------------------------------------------------------
	eel_dir.h - EEL Directory Handling
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
