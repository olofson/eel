/*
---------------------------------------------------------------------------
	e_exceptions.h - EEL exception code registry
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
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

#ifndef	EEL_E_EXCEPTIONS_H
#define	EEL_E_EXCEPTIONS_H

#include "EEL.h"

/*
 * Interface:
 *	* User exception codes should be positive integers smaller than
 *	  EEL_EXCEPTION_MAX.
 *
 *	* The register call returns an integer offset to add to user exception
 *	  codes before they're returned to the VM.
 *
 * Implementation notes:
 *	* The exception block index is encoded in the top bits, though the
 *	  MSB is always zero. (Exception codes should be positive integers!)
 *
 *	* Exception info is stored as arrays of EEL_xentry.
 */

#define	EEL_EXCEPTION_BITS	24
#define	EEL_EXCEPTION_MAX	((1 << EEL_EXCEPTION_BITS) - 1)
#define	EEL_EXCEPTION_BLOCKS	(1 << (31 - EEL_EXCEPTION_BITS))

typedef struct EEL_xentry
{
	const char	*name;		/* EEL symbol name */
	const char	*description;	/* For exception_description() */
} EEL_xentry;

typedef struct EEL_xblock
{
	int			base;	/* Code of first exception */
	unsigned		nentries;
	const EEL_xentry	*entries;
} EEL_xblock;

EEL_xno eel_x_open_registry(void);
void eel_x_close_registry(void);

#endif	/* EEL_E_EXCEPTIONS_H */
