/*
---------------------------------------------------------------------------
@AUTO_MESSAGE@
---------------------------------------------------------------------------
	EEL_version.h - EEL Version Control (API)
---------------------------------------------------------------------------
 * Copyright 2003, 2005-2006, 2009, 2014 David Olofson
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

#ifndef	EEL_VERSION_H
#define	EEL_VERSION_H

/*
 * Note: eel_lib_version() is declared in eel.h.
 */

/*
 * EEL version codes are unsigned 32 bit integers. The
 * major version number is stored in the top 8 bits,
 * the minor version in the next lower 8 bits, and the
 * patch level in bits 15..8.
 *
 * The low byte is reserved for snapshot number, and
 * should generally not be considered by applications,
 * other than for diagnostic messages for bug reports.
 * (That's why EEL_MAKE_VERSION doesn't have an argument
 * for it! :-)
 */

#define	EEL_MAJOR_VERSION	@VERSION_MAJOR@
#define EEL_MINOR_VERSION	@VERSION_MINOR@
#define	EEL_MICRO_VERSION	@VERSION_PATCH@
#define	EEL_SNAPSHOT		@VERSION_BUILD@

/*
 * Make a version code out of arbitrary major, minor and
 * micro values. This is probably what you want if you
 * check library versions at run time!
 */
#define EEL_MAKE_VERSION(ma, mi, u)	((ma<<24)|(mi<<16)|(u<<8))

/*
 * Evaluates to the version of the library headers used.
 * NOTE: This version code includes the snapshot field!
 */
#define EEL_COMPILED_VERSION \
	(EEL_MAKE_VERSION(EEL_MAJOR_VERSION, EEL_MINOR_VERSION,	\
			EEL_MICRO_VERSION) | EEL_SNAPSHOT)

/*
 * Extracts individual fields from a version code, for
 * printing, or whatever special tests applications may
 * want to perform.
 */
#define	EEL_GET_MAJOR(ver)	(((ver)>>24) & 0xff)
#define	EEL_GET_MINOR(ver)	(((ver)>>16) & 0xff)
#define	EEL_GET_MICRO(ver)	(((ver)>>8) & 0xff)
#define	EEL_GET_SNAPSHOT(ver)	((ver) & 0xff)

#endif /* EEL_VERSION_H */
