/*(LGPLv2.1)
---------------------------------------------------------------------------
	EEL_export.h - DLL exporting macros
---------------------------------------------------------------------------
 * Copyright (C) 2002-2004, 2006, 2009 David Olofson
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

#ifndef	EEL_EXPORT_H
#define	EEL_EXPORT_H

/*
 * EEL__EXP and EEL_CALL were stolen from libSDL 1.2.4
 * (Renamed to avoid collisions and confusion.)
 */

/* Some compilers use a special export keyword */
#ifndef EEL__EXP
# ifdef __BEOS__
#  if defined(__GNUC__)
#   define EEL__EXP	__declspec(dllexport)
#  else
#   define EEL__EXP	__declspec(export)
#  endif
# else
# ifdef WIN32
#  ifdef __BORLANDC__
/* This one's for anyone who cares to port to BCC. */
#   ifdef BUILD_EEL
#    define EEL__EXP	__declspec(dllexport)
#   else
#    define EEL__EXP	__declspec(dllimport)
#   endif
#  else
#   define EEL__EXP	__declspec(dllexport)
#  endif
# else
#  define EEL__EXP
# endif
# endif
#endif

/* Removed EEL__EXP on Symbian OS. No DLLs on EPOC...? */
#ifdef __SYMBIAN32__
#undef	EEL__EXP
#define	EEL__EXP
#endif /* __SYMBIAN32__ */

/* C calling convention */
#ifndef EEL__CALL
#ifdef WIN32
#define EEL__CALL	__cdecl
#else
#define EEL__CALL
#endif
#endif /* EEL__CALL */

/* Nice and short wrapper macro */
#define	EELAPI(rt)	extern EEL__EXP rt EEL__CALL

#endif /* EEL_EXPORT_H */
