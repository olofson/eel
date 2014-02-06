/*
---------------------------------------------------------------------------
	EEL_types.h - Commonly used types (API)
---------------------------------------------------------------------------
 * Copyright 2001-2004, 2006, 2009 David Olofson
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

#ifndef EEL_TYPES_H
#define EEL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Basic data types (stolen from libSDL) */
typedef unsigned char	EEL_uint8;
typedef signed char	EEL_int8;
typedef unsigned short	EEL_uint16;
typedef signed short	EEL_int16;
typedef unsigned int	EEL_uint32;
typedef signed int	EEL_int32;

/* Make sure the types really have the right sizes */
#define EEL_COMPILE_TIME_ASSERT(name, x)               \
       typedef int EEL_dummy_ ## name[(x) * 2 - 1]
EEL_COMPILE_TIME_ASSERT(uint8, sizeof(EEL_uint8) == 1);
EEL_COMPILE_TIME_ASSERT(sint8, sizeof(EEL_int8) == 1);
EEL_COMPILE_TIME_ASSERT(uint16, sizeof(EEL_uint16) == 2);
EEL_COMPILE_TIME_ASSERT(sint16, sizeof(EEL_int16) == 2);
EEL_COMPILE_TIME_ASSERT(uint32, sizeof(EEL_uint32) == 4);
EEL_COMPILE_TIME_ASSERT(sint32, sizeof(EEL_int32) == 4);
#undef EEl_COMPILE_TIME_ASSERT

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
 * Some types that are opaque to applications,
 * but show up in public interfaces.
 */
typedef struct EEL_vm EEL_vm;
typedef struct EEL_object EEL_object;
typedef union EEL_value EEL_value;

/*
 * EEL language data types
 */
typedef double EEL_real;	/* For 'real' values */
typedef EEL_int32 EEL_integer;	/* For 'integer' values */
typedef EEL_uint32 EEL_uinteger;/* For some ops on 'integer' values */
typedef	EEL_int32 EEL_index;	/* For indices in the heap, lists etc */

/* Size of the EEL_value struct in bytes */
#define	EEL_VALUE_SIZE		16


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

/*
 * List of integer constants, for eel_export_lconstants(),
 * eel_insert_lconstants() etc.
 * Terminate list with an item with a NULL name string!
 */
typedef struct
{
	const char	*name;
	long		value;
} EEL_lconstexp;

#ifdef __cplusplus
}
#endif

#endif /* EEL_TYPES_H */
