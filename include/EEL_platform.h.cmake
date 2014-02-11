/*
---------------------------------------------------------------------------
@AUTO_MESSAGE@
---------------------------------------------------------------------------
	EEL_platform.h - Platform specific definitions (API)
---------------------------------------------------------------------------
 * Copyright 2007, 2014 David Olofson
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

#ifndef EEL_PLATFORM_H
#define EEL_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NULL (stolen from the GNU C Library)
 */
#ifndef NULL
#	if defined __GNUG__ &&					\
			(__GNUC__ > 2 ||			\
			(__GNUC__ == 2 && __GNUC_MINOR__ >= 8))
#		define NULL (__null)
#	else
#		if !defined(__cplusplus)
#			define NULL ((void*)0)
#		else
#			define NULL (0)
#		endif
#	endif
#endif


/*
 * Architecture
 */
#define	EEL_ARCH	"@EEL_ARCH@"
#define	EEL_SOEXT	"@EEL_SOEXT@"
#define	EEL_DIRSEP	'@EEL_DIRSEP@'

/*
 * 'inline'
 */
#ifndef inline
@EEL_PLATFORM_INLINE@
#endif


/* Figure out what endian this platform is. (Stolen from SDL.) */
#define EEL_LIL_ENDIAN  1234
#define EEL_BIG_ENDIAN  4321

/* Pardon the mess, I'm trying to determine the endianness of this host.
   I'm doing it by preprocessor defines rather than some sort of configure
   script so that application code can use this too.  The "right" way would
   be to dynamically generate this file on install, but that's a lot of work.
 */
#if  defined(__i386__) || defined(__ia64__) || defined(WIN32) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__SYMBIAN32__) || \
     defined(__x86_64__) || \
     defined(__LITTLE_ENDIAN__)
#define EEL_BYTEORDER   EEL_LIL_ENDIAN
#else
#define EEL_BYTEORDER   EEL_BIG_ENDIAN
#endif

#ifdef __cplusplus
}
#endif

#endif /* EEL_PLATFORM_H */
