/*
---------------------------------------------------------------------------
	ec_symtab.h - Symbol Table/Tree
---------------------------------------------------------------------------
 * Copyright 2000, 2002, 2004-2006, 2009, 2011, 2014 David Olofson
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

#ifndef EEL_EC_SYMTAB_H
#define EEL_EC_SYMTAB_H

#include "EEL_types.h"
#include "EEL_value.h"
#include "e_register.h"

typedef enum
{
/*
FIXME: I think some cleaning up is needed here. Old cruft...
*/
	EEL_SUNDEFINED = 0,
	EEL_SKEYWORD,	/* Keyword. v.index = token */
	EEL_SVARIABLE,	/* Variable. (Local, result, arg,...)
			 * v.var.kind = kind of variable
			 * v.var.location = index
			 */
	EEL_SUPVALUE,	/* "Confirmed" upvalue variable.
			 * (Same arguments as SVARIABLE)
			 */
	EEL_SBODY,	/* Named {...} bodies. */
	EEL_SNAMESPACE,	/* Namespace. */
	EEL_SCONSTANT,	/* Named constant. v.value = value */
	EEL_SCLASS,	/* v.object = class def. object */
	EEL_SMODULE,	/* v.object -> CMODULE object */
	EEL_SFUNCTION,	/* v.object -> CFUNCTION object (EEL or C) */
	EEL_SOPERATOR	/* v.op -> operator */
} EEL_symtypes;

#define	EEL_SMASK(x)	EEL_SM##x = (1<<EEL_S##x)
enum
{
	EEL_SMASK(UNDEFINED),
	EEL_SMASK(KEYWORD),
	EEL_SMASK(VARIABLE),
	EEL_SMASK(UPVALUE),
	EEL_SMASK(BODY),
	EEL_SMASK(NAMESPACE),
	EEL_SMASK(CONSTANT),
	EEL_SMASK(CLASS),
	EEL_SMASK(MODULE),
	EEL_SMASK(FUNCTION),
	EEL_SMASK(OPERATOR)
};
#undef EEL_SMASK

/*
 * Find symbol of 'type' named 'name' in 'table'.
 * Returns NULL if not found.
 */
EEL_symbol *eel_s_find(EEL_state *es, EEL_symbol *table,
		const char *name, EEL_symtypes type);

/*
 * Add a new symbol of 'type' named 'name' as a child of symbol 'parent'.
 * NO CHECKING!
 */
EEL_symbol *eel_s_add(EEL_state *es, EEL_symbol *parent,
		const char *name, EEL_symtypes type);

/* Rename symbol 's' as 'name'. NO CHECKING! */
void eel_s_rename(EEL_state *es, EEL_symbol *s, const char *name);

/* Detach and free symbol 's' */
void eel_s_free(EEL_state *es, EEL_symbol *s);


/*------------------------------------------------
	Symbol table search tool
------------------------------------------------*/

/* Find by name, types and/or index. None ==> find next in table. */
#define	ESTF_NAME	0x00000001
#define	ESTF_TYPES	0x00000002

/* Recurse up to parent symtabs matching 'types'. */
#define	ESTF_UP		0x00000010

/* Recurse down into symtabs matching 'types'. */
#define	ESTF_DOWN	0x00000020

typedef struct EEL_finder
{
	EEL_state	*state;

	EEL_symbol	*start;		/* Start symbol table */
	EEL_symbol	*symtab;	/* Current symbol table */
	EEL_symbol	*symbol;	/* Current symbol */

	EEL_object	*name;		/* EEL_string object */
	int		types;		/* Mask of ESTM_* flags */
	int		index;

	int		flags;		/* For ESTF_* flags */
} EEL_finder;

/*
 * Init EEL_finder 'f' to search all of 'st' using the rules
 * defined by 'flags'.
 *
 * Any of the fields 'name', 'types' and 'index' needed for
 * the search must be initialized before the EEL_finder is
 * used, unless the defaults are appropriate.
 *
 * Default search pattern:
 *	name:	NULL	(matches anything)
 *	types:	-1	(matches any symbol types)
 *	index:	0
 */
void eel_finder_init(EEL_state *es, EEL_finder *f, EEL_symbol *st, int flags);

/*
 * Find the next symbol matching the search rules defined by 'f'.
 *
 * If 'f->symbol' is NULL, the search will start with the first
 * symbol of the current symbol table, otherwise, the search will
 * start at the symbol after 'f->symbol'.
 *
 * If 'f->flags' contains ESTF_DOWN, eel_finder_go() will
 * recursively search child symbols of any symbols matching
 * 'f->types'. Note that recursion is done directly as children
 * are found; it's not delayed until after the current table has
 * been scanned. (No branch stack...)
 *
 * If 'f->flags' contains ESTF_UP, eel_finder_go() will recurse
 * up and search parent symbol tables, as the end of symbol tables
 * are reached.
 *
 * NOTE: ESTF_UP and ESTF_DOWN cannot be used together!
 *
 * Returns NULL if there was no (further) match.
 */
EEL_symbol *eel_finder_go(EEL_finder *f);

/*
 * Variable kinds:
 *	STACK:
 *		Stored in a register in a register frame
 *		on the call stack.
 *
 *	STATIC:
 *		Stored in a table belonging to a function.
 *
 *	ARGUMENT:
 *		Stored in a register in the argument
 *		list in R[EEL_SF_ARGUMENTS].
 *
 *	OPTARG:
 *		Like ARGUMENT, but not necessarily in
 *		the LIST.
 *
 *	TUPARG:
 *		Similar to OPTARG, but indexed via LISTs
 *		with modulo.
 */
typedef enum
{
	EVK_STACK = 0,	/* index = register index */
	EVK_STATIC,	/* index = variable table index */
	EVK_ARGUMENT,	/* index = index in argument list */
	EVK_OPTARG,	/* index = index in argument list */
	EVK_TUPARG	/* index = index inside tuple */
} EEL_varkinds;


/*
 * Symbol table node
 *
 * NOTES:
 *	* The root symbol does not have a parent!
 *
 * IMPORTANT: Make sure eel_s_free() cleans up properly!
 */
struct EEL_symbol
{
	EEL_symbol	*parent;	/* Parent symbol, if any */
	EEL_symbol	*next, *prev;	/* Linked list */
	EEL_object	*name;		/* Symbol name */
	EEL_symtypes	type;		/* Symbol type */
	int		uvlevel;	/* Function nesting level */
	EEL_symbol	*symbols;	/* Linked list of children */
	EEL_symbol	*lastsym;	/* Last symbol (for adding) */
	union
	{
		/* KEYWORD */
		int		token;

		/* CLASS, FUNCTION, MODULE */
		EEL_object	*object;

		/* VARIABLE, UPVALUE */
		struct
		{
			EEL_varkinds	kind;
			int		location;	/* Heap/stack/... */
			int		defindex;	/* Constant index */
		} var;

		/* CONSTANT */
		EEL_value	value;

		/* OPERATOR */
		EEL_operator	*op;

		/* BODY */
		EEL_context	*context;
	} v;
};


/*
 * Generate an ASCII string listing the symbol types
 * marked with ESTM_* flags in 'flags'.
 *
 * NOTE: The result string has limited life time!
 */
const char *eel_estm_stringrep(EEL_state *es, int flags);

/*
 * Add a symbol 'name' of 'stype' to the root symbol table
 * of 'vm's state.
 *
 * NOTE:
 *	You must fill in the needed fields in .v, if any!
 */
EEL_symbol *eel__register(EEL_vm *vm, const char *name, EEL_symtypes stype);

/*
 * Grab the exports from module 'm' and add them to symbol table 'st'.
 */
int eel_s_get_exports(EEL_symbol *st, EEL_object *m);


/*------------------------------------------------
	Symbol table tools
------------------------------------------------*/

/*
 * Generate an ASCII string representation of 'sym'.
 *
 * NOTE: The result string has limited life time!
 */
const char *eel_s_stringrep(EEL_vm *vm, EEL_symbol *s);

/*
 * Dump the whole tree of symbols starting at 'st'
 * to stdout. If 'st' is NULL, the dump starts in the
 * root symbol table of 'es'. If 'code' is non zero,
 * VM code disassembly of functions is printed.
 */
void eel_s_dump(EEL_vm *vm, EEL_symbol *st, int code);

/*
 * Find symbol 'name' in 'scope'.
 *
 * If 'scope' is NULL, 'name' is assumed to be based
 * in the global root scope.
 *
 * Returns an EEL_symbol, or NULL, if 'name' was not found.
 */
EEL_symbol *eel_find_symbol(EEL_vm *vm, EEL_symbol *scope,
		const char *name);

/*
 * Returns a string describing the specified symbol
 * in a way that would complete the sentences:
 *	"This is ..."
 *	"Expected ..."
 */
const char *eel_symbol_is(EEL_symbol *s);

#endif /* EEL_EC_SYMTAB_H */
