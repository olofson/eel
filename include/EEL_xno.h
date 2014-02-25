/*
---------------------------------------------------------------------------
	EEL_xno.h - EEL Exception/Error codes (API)
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009, 2013-2014 David Olofson
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

#ifndef	EEL_XNO_H
#define	EEL_XNO_H

#include "EEL_export.h"
#include "EEL_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTE:
 *	Add new codes LAST, to preserve binary compatibility.
 *	When bumping the interface version to indicate broken
 *	binary compatibility, take the opportunity to put any
 *	new codes where they belong in the list.
 */
#define EEL_ALLEXCEPTIONS	\
\
  /* VM exceptions */		\
  EEL_DEFEX(XYIELD,		"Give up VM if there is other work")\
  EEL_DEFEX(XCOUNTER,		"Runlimit instruction counter exhausted")\
  EEL_DEFEX(XEND,		"Thread returns from top level")\
  EEL_DEFEX(XRETURN,		"Return from actual function")\
  EEL_DEFEX(XREFUSE,		"Object refused to destruct")\
  EEL_DEFEX(XOTHER,		"Other - check vm->exception")\
  EEL_DEFEX(XVMCHECK,		"Error detected due to EEL_VM_CHECKING")\
  EEL_DEFEX(XBADEXCEPTION,	"Bad exception value type")\
\
  /* Internal/VM/native errors */\
  EEL_DEFEX(XINTERNAL,		"Internal error in EEL")\
  EEL_DEFEX(XVMERROR,		"Unspecified VM error")\
\
  /* General VM and operator exceptions */\
  EEL_DEFEX(XILLEGAL,		"Illegal opcode")\
  EEL_DEFEX(XNOTIMPLEMENTED,	"Feature not implemented")\
  EEL_DEFEX(XCANTREAD,		"Object cannot be read")\
  EEL_DEFEX(XCANTWRITE,		"Object cannot be written")\
  EEL_DEFEX(XCANTINDEX,		"Object cannot be indexed")\
  EEL_DEFEX(XCANTINPLACE,	"Inplace operation not possible")\
  EEL_DEFEX(XUPVALUE,		"Upvalue(s) not accessible")\
  EEL_DEFEX(XCASTFAILED,	"Type cast failed")\
  EEL_DEFEX(XNOMETAMETHOD,	"Metamethod not implemented")\
  EEL_DEFEX(XNOCONSTRUCTOR,	"No constructor!")\
  EEL_DEFEX(XCONSTRUCTOR,	"Constructor failed")\
  EEL_DEFEX(XBADCONTEXT,	"Not possible in this context")\
\
  /* Argument and operand errors */\
  EEL_DEFEX(XARGUMENTS,		"Incorrect argument list")\
  EEL_DEFEX(XFEWARGS,		"Too few arguments")\
  EEL_DEFEX(XMANYARGS,		"Too manp arguments")\
  EEL_DEFEX(XTUPLEARGS,		"Incomplete argument tuple")\
  EEL_DEFEX(XNORESULT,		"No result available")\
  EEL_DEFEX(XNEEDREAL,		"Argument must be real type")\
  EEL_DEFEX(XNEEDINTEGER,	"Argument must be integer type")\
  EEL_DEFEX(XNEEDBOOLEAN,	"Argument must be boolean type")\
  EEL_DEFEX(XNEEDTYPEID,	"Argument must be typeid type")\
  EEL_DEFEX(XNEEDOBJECT,	"Argument must be object type")\
  EEL_DEFEX(XNEEDLIST,		"Argument must be LIST type")\
  EEL_DEFEX(XNEEDSTRING,	"Argument must be string type")\
  EEL_DEFEX(XNEEDDSTRING,	"Argument must be dstring type")\
  EEL_DEFEX(XNEEDARRAY,		"Argument must be array type")\
  EEL_DEFEX(XNEEDTABLE,		"Argument must be table type")\
  EEL_DEFEX(XNEEDMODULE,	"Argument must be a module")\
  EEL_DEFEX(XNEEDCALLABLE,	"Argument must be callable object")\
  EEL_DEFEX(XNEEDEVEN,		"Needs even number of items")\
  EEL_DEFEX(XWRONGTYPE,		"Wrong type")\
  EEL_DEFEX(XBADTYPE,		"Illegal type ID")\
  EEL_DEFEX(XBADCLASS,		"Illegal class type ID")\
  EEL_DEFEX(XLOWINDEX,		"Index out of range; too low")\
  EEL_DEFEX(XHIGHINDEX,		"Index out of range; too high")\
  EEL_DEFEX(XWRONGINDEX,	"Nonexistent index (index-by-name)")\
\
  /* Math and other operation errors */\
  EEL_DEFEX(XLOWVALUE,		"Value out of range; too low")\
  EEL_DEFEX(XHIGHVALUE,		"Value out of range; too high")\
  EEL_DEFEX(XBADVALUE,		"Incorrect value")\
  EEL_DEFEX(XDIVBYZERO,		"Division by zero")\
  EEL_DEFEX(XOVERFLOW,		"Too large value")\
  EEL_DEFEX(XUNDERFLOW,		"Too small value")\
  EEL_DEFEX(XDOMAIN,		"Math domain error")\
  EEL_DEFEX(XMATHERROR,		"Other math errors")\
  EEL_DEFEX(XILLEGALOPERATION,	"Illegal operation")\
\
  /* System errors */\
  EEL_DEFEX(XMEMORY,		"Out of memory")\
  EEL_DEFEX(XEOF,		"End of file")\
  EEL_DEFEX(XFILEOPEN,		"Error opening file")\
  EEL_DEFEX(XFILESEEK,		"Error seeking in file")\
  EEL_DEFEX(XFILEREAD,		"Error reading file")\
  EEL_DEFEX(XFILEWRITE,		"Error writing file")\
  EEL_DEFEX(XFILELOAD,		"Unspecified load error")\
  EEL_DEFEX(XFILESAVE,		"Unspecified save error")\
  EEL_DEFEX(XFILEOPENED,	"File is already open")\
  EEL_DEFEX(XFILECLOSED,	"File is closed")\
  EEL_DEFEX(XFILEERROR,		"Unspecified file I/O error")\
  EEL_DEFEX(XDEVICEOPEN,	"Error opening device")\
  EEL_DEFEX(XDEVICEREAD,	"Error reading from device")\
  EEL_DEFEX(XDEVICEWRITE,	"Error writing to device")\
  EEL_DEFEX(XDEVICECONTROL,	"Error controlling device")\
  EEL_DEFEX(XDEVICEOPENED,	"Device is already open")\
  EEL_DEFEX(XDEVICECLOSED,	"Device is closed")\
  EEL_DEFEX(XDEVICEERROR,	"Unspecified device I/O error")\
  EEL_DEFEX(XTHREADCREATE,	"Could not create thread")\
  EEL_DEFEX(XBUFOVERFLOW,	"Buffer overflow")\
\
  /* Lexer and parser exceptions */\
  EEL_DEFEX(XNONUMBER,		"Not a valid number")\
  EEL_DEFEX(XBADBASE,		"Bad base syntax")\
  EEL_DEFEX(XBIGBASE,		"Too big base")\
  EEL_DEFEX(XBADINTEGER,	"Bad integer part format")\
  EEL_DEFEX(XBADFRACTION,	"Bad fraction part format")\
  EEL_DEFEX(XBADEXPONENT,	"Bad exponent format")\
  EEL_DEFEX(XREALNUMBER,	"Enforce real value (lexer)")\
\
  /* Compiler and API errors */\
  EEL_DEFEX(XCOMPILE,		"Compile error")\
  EEL_DEFEX(XSYNTAX,		"Unspecified syntax error")\
  EEL_DEFEX(XNOTFOUND,		"Object not found (call by name)")\
  EEL_DEFEX(XMODULELOAD,	"Module loading failed")\
  EEL_DEFEX(XMODULEINIT,	"Module initialization failed")\
  EEL_DEFEX(XCANTSETMETHOD,	"Could not set (meta)method")\
\
  EEL_DEFEX(_XCOUNT,		"(Total number of exception codes)")

#define	EEL_DEFEX(x, y)	EEL_##x,
typedef enum EEL_xno
{
	EEL_XOK = 0,
	EEL_ALLEXCEPTIONS
} EEL_xno;
#undef	EEL_DEFEX


/*
 * Exception name strings.
 */

#if 0
/*TODO*/
/*
 * User exception registry
 */

/* Allocate a range of 'count' exception codes for module 'mo' */
EELAPI(unsigned)eel_x_alloc_range(EEL_object *mo, unsigned count);
#endif

/* Get exception description for 'x' */
EELAPI(const char *)eel_x_description(EEL_vm *vm, EEL_xno x);

/* Get the name of exception 'x' */
EELAPI(const char *)eel_x_name(EEL_vm *vm, EEL_xno x);

#ifdef __cplusplus
};
#endif

#endif /*  EEL_XNO_H */
