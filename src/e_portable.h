/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_portable.h - "snprintf and stuff"
---------------------------------------------------------------------------
 * Copyright (C) 2002-2003, 2009 David Olofson
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

#ifndef	EEL_PORTABLE_H
#define	EEL_PORTABLE_H

#include "config.h"

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
