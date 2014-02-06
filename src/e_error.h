/*
---------------------------------------------------------------------------
	e_error.h - EEL Compiler and VM Error Handling
---------------------------------------------------------------------------
 * Copyright 2002-2005, 2009, 2013 David Olofson
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

#ifndef EEL_E_ERROR_H
#define EEL_E_ERROR_H

#include <stdlib.h>
#include <string.h>
#include "e_vm.h"


/* Compiler and VM error messages */
typedef enum
{
	EEL_EM_APPEND_NL = -2,	/* Append to last, starting at new line */
	EEL_EM_APPEND = -1,	/* Append text to last message */
	EEL_EM_INFO = 0,	/* Info, what we're doing etc... */
	EEL_EM_CWARNING,	/* Compile Warning */
	EEL_EM_CERROR,		/* Compile Error */
	EEL_EM_CDUMP,		/* Compiler symbol tree dump */
	EEL_EM_VMWARNING,	/* VM Warning */
	EEL_EM_VMERROR,		/* VM Error */
	EEL_EM_VMDUMP,		/* VM registers, code etc */
	EEL_EM_IERROR,		/* Internal Error */
	EEL_EM_SERROR,		/* System Error */
	EEL_EM_FATAL,		/* Fatal Error (instant printout!) */
	EEL_EM__COUNT
} EEL_emtype;

/* Format and add a message of type 't' */
int eel_msg(EEL_state *es, EEL_emtype t, const char *format, ...);
int eel_vmsg(EEL_state *es, EEL_emtype t, const char *format, va_list args);

/* Formatted messages with added source location info */
void eel_vmsg_src(EEL_state *es, EEL_emtype t,
		const char *format, va_list args);

/*
 * Print a nice warning or error "prompt" with file name,
 * line # and column, followed by your formatted error message,
 * followed by a newline ('\n') where appropriate. (Don't
 * add this yourself, unless you need multiple lines.)
 *
 * eel_cerror() is meant for compiler errors, and will
 * print the error message along with source line and
 * position info if possible.
 *
 * eel_ierror() is meant for internal errors, and will
 * add a "contact the author" message.
 *
 * eel_serror() is for system errors, such as running
 * out of memory, "strange" I/O errors and the like.
 * This should be used only for errors that are most
 * likely caused by the system going low on resources,
 * file system corruption and the like.
 *
 * eel_cerror(), eel_ierror() and eel_serror() do not return!
 */
void eel_cwarning(EEL_state *es, const char *format, ...);

/*
 * Some ugly macro tricks to get any compiler to (sort of)
 * understand that eel_cerror(), eel_ierror(), eel_serror()
 * and eel_cthrow() never return.
 *
 * Please add cases for other compilers to avoid the exit()
 * hack as far as possible! (The __VA_ARGS__ thingy is C99.)
 */
#ifdef __GNUC__
	/* Tell the compiler that these never return. */
	void eel__cerror(EEL_state *es, const char *format, ...)
			__attribute__((format (printf, 2, 3)))
			__attribute__((noreturn));
	void eel__serror(EEL_state *es, const char *format, ...)
			__attribute__((format (printf, 2, 3)))
			__attribute__((noreturn));
	void eel__ierror(EEL_state *es, const char *format, ...)
			__attribute__((format (printf, 2, 3)))
			__attribute__((noreturn));
	void eel__ferror(EEL_state *es, const char *format, ...)
			__attribute__((format (printf, 2, 3)))
			__attribute__((noreturn));
	void eel__cthrow(EEL_state *es)
			__attribute__((noreturn));
#	define	eel_cerror	eel__cerror
#	define	eel_serror	eel__serror
#	define	eel_ierror	eel__ierror
#	define	eel_ferror	eel__ferror
#	define	eel_cthrow	eel__cthrow
#elif defined(HAVE_C99_VAMACROS)
	/* Let's hope that exit() is marked "noreturn". */
	void eel__cerror(EEL_state *es, const char *format, ...);
	void eel__serror(EEL_state *es, const char *format, ...);
	void eel__ierror(EEL_state *es, const char *format, ...);
	void eel__ferror(EEL_state *es, const char *format, ...);
	void eel__cthrow(EEL_state *es);
#	define	eel_cerror(...)	({ eel__cerror(__VA_ARGS__); exit(1); })
#	define	eel_serror(...)	({ eel__serror(__VA_ARGS__); exit(1); })
#	define	eel_ierror(...)	({ eel__ierror(__VA_ARGS__); exit(1); })
#	define	eel_ferror(...)	({ eel__ferror(__VA_ARGS__); exit(1); })
#	define	eel_cthrow(x)	({ eel__cthrow(x); exit(1); })
#else
	/* Hopefully, we'll just get some warnings... */
#	define	eel_cerror	eel__cerror
#	define	eel_serror	eel__serror
#	define	eel_ierror	eel__ierror
#	define	eel_ferror	eel__ferror
#	define	eel_cthrow	eel__cthrow
#endif

/* Add info message to log */
void eel_info(EEL_state *es, const char *format, ...);

/* Clear all messages except warnings */
void eel_clear_errors(EEL_state *es);

/* Clear warning messages */
void eel_clear_warnings(EEL_state *es);

/* Clear info messages */
void eel_clear_info(EEL_state *es);

/* Get name of metamethod 'mm' */
const char *eel_mm_name(EEL_mmindex mm);

/*
 * Dump current state of VM, with exception info, custom
 * error message and disassembly.
 */
void eel_vmdump(EEL_vm *vm, const char *format, ...);

#endif /*EEL_E_UTIL_H*/
