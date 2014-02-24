/*
---------------------------------------------------------------------------
	e_portable.h - "snprintf and stuff"
---------------------------------------------------------------------------
 * Copyright 2002-2003, 2009 David Olofson
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

#ifndef	EEL_PORTABLE_H
#define	EEL_PORTABLE_H

#include "e_config.h"

/*
 * Windoze calls it _snprintf - just to piss off anyone who likes to
 * use non "ANSI C" calls... (How about C99?)
 */
#ifndef HAVE_SNPRINTF
#	ifndef HAVE__SNPRINTF
#		error No snprintf() or _snprintf()!
#	else
#		define snprintf _snprintf
#	endif
#endif


/*
 * On some platforms, assignments of these "opaque objects" are
 * illegal - while on others, the macro to handle that can be
 * missing... Oh, and this is really only in C99, of course. :-/
 */
#include <stdarg.h>
#ifndef va_copy
#	ifdef __va_copy
#		define va_copy(to, from) __va_copy(to, from)
#	else
#		define va_copy(to, from) (to) = (from)
#	endif
#endif


#endif /* EEL_PORTABLE_H */
