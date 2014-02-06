/*
---------------------------------------------------------------------------
	EEL_export.h - DLL exporting macros
---------------------------------------------------------------------------
 * Copyright 2002-2004, 2006, 2009 David Olofson
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
