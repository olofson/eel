/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_coder.h - EEL VM Code Generation Tools
---------------------------------------------------------------------------
 * Copyright (C) 2004-2006, 2009, 2011 David Olofson
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

#ifndef	EEL_EC_CODER_H
#define	EEL_EC_CODER_H

#include "e_eel.h"
#include "e_operate.h"

typedef struct EEL_mlist EEL_mlist;
typedef struct EEL_manipulator EEL_manipulator;
typedef struct EEL_regspec EEL_regspec;

/*----------------------------------------------------------
	EEL VM code generator
----------------------------------------------------------*/
struct EEL_coder
{
	EEL_state	*state;		/* Parent state */

	EEL_object	*f;		/* Where the code goes */

	/* Code */
	int		fragstart;	/* Start of current code fragment */
	int		maxcode;	/* Bytes of code allocated */
	int		peephole;	/* Enable peephole optimizer */
	int		codeonly;	/* Disable targets, optimizations etc */

	/* Debug info */
	int		fragline;	/* 'lines' index for fragment start */
	int		maxlines;

	/* Constants */
	int		maxconstants;	/* Number of constants allocated */

	/* Register allocation */
	int		maxregisters;	/* Size of table (elements) */
	EEL_regspec	*registers;	/* Table of regspecs */

	/* Argument manipulator lists */
	EEL_mlist	*firstml, *lastml;
};

/*
 * Open/close an EEL_CFUNCTION for code generation.
 * Return negative values in case of error.
 */
EEL_coder *eel_coder_open(EEL_state *es, EEL_object *fo);

/*
 * Flush resulting code to the pfunc and then die.
 */
int eel_coder_close(EEL_coder *cdr);

/*
 * Adjust sive of variable block etc
 */
void eel_coder_finalize_module(EEL_coder *cdr);


/*----------------------------------------------------------
	Code, constants, variables...
----------------------------------------------------------*/
/* Add value 'v' to the constant table. Returns it's index. */
int eel_coder_add_constant(EEL_coder *cdr, EEL_value *value);

/*
 * Add a variable to the variables table and initialize it
 * to 'value'. Returns the index of the variable.
 */
int eel_coder_add_variable(EEL_coder *cdr, EEL_value *value);


/*----------------------------------------------------------
	Register allocation
----------------------------------------------------------*/
typedef enum
{
	EEL_RUFREE = 0,		/* Not allocated! (Probably in the
				 * wrong place if you see this...) */
	EEL_RUTEMPORARY,	/* Temporary storage */
	EEL_RUVARIABLE		/* Local variable */
} EEL_reguse;

typedef enum
{
	EEL_RTDYNAMIC = 0	/* Any type allowed */
} EEL_regtype;

struct EEL_regspec
{
	EEL_reguse	use;	/* Variable, temporary etc */
	EEL_regtype	type;	/* Always EEL_RTDYNAMIC for now... */
};

/*
 * Allocate a contiguous group of stack registers for use
 * as indicated by 'use'.
 *
 * The *_top() version ensures that the allocated registers
 * occupy the highest allocated indices.
 *
 * Returns the register index of the first one.
 */
int eel_r_alloc(EEL_coder *cdr, int count, EEL_reguse use);
int eel_r_alloc_top(EEL_coder *cdr, int count, EEL_reguse use);

/*
 * Allocate register 'r'. If the register is not available, an
 * exception is thrown.
 */
void eel_r_alloc_reg(EEL_coder *cdr, int r, EEL_reguse use);

/*
 * Free a range of registers, or, if 'count' < 0,
 * all registers above and including R[first].
 */
void eel_r_free(EEL_coder *cdr, int first, int count);

/*
 * Get reginfo for 'r'. Throws an exception if 'r' is
 * not allocated, ie not for the compiler to touch.
 *
 * NOTE: Cannot return NULL.
 */
EEL_regspec *eel_r_spec(EEL_coder *cdr, int r);


/*----------------------------------------------------------
	Low level code generation
----------------------------------------------------------*/
/*
 * Instruction generators.
 * In some cases (branch instructions that need later fixup),
 * these return the position of the generated instruction,
 * or -1 if the instruction was not generated. (Dead code,
 * for example.)
 * For normal instructions, they return -1, since these
 * instructions may be moved around by the optimizer,
 * rendering the original positions pretty much useless.
 */
int eel_code0(EEL_coder *cdr, EEL_opcodes op);
int eel_codeA(EEL_coder *cdr, EEL_opcodes op, int a);
int eel_codeAx(EEL_coder *cdr, EEL_opcodes op, int a);
int eel_codeAB(EEL_coder *cdr, EEL_opcodes op, int a, int b);
int eel_codeABC(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c);
int eel_codeABCD(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d);
int eel_codesAx(EEL_coder *cdr, EEL_opcodes op, int a);
int eel_codeABx(EEL_coder *cdr, EEL_opcodes op, int a, int b);
int eel_codeAsBx(EEL_coder *cdr, EEL_opcodes op, int a, int b);
int eel_codeAxBx(EEL_coder *cdr, EEL_opcodes op, int a, int b);
int eel_codeAxsBx(EEL_coder *cdr, EEL_opcodes op, int a, int b);
int eel_codeABCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c);
int eel_codeABsCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c);
int eel_codeABxCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c);
int eel_codeABxsCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c);
int eel_codeABCDx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d);
int eel_codeABCsDx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d);

/*
 * Make the current code position a jump target, and return
 * the position. This may also performs optimizations on the
 * previous code fragment, prior to getting the position.
 */
int eel_code_target(EEL_coder *cdr);

/*
 * Adjust the target position of the jump instruction
 * at 'pos' so it lands at 'whereto'.
 */
void eel_code_setjump(EEL_coder *cdr, int pos, int whereto);


/*----------------------------------------------------------
	Tools
----------------------------------------------------------*/
/* Returns a disassembly of the instruction at 'pc' in function 'f'. */
const char *eel_i_stringrep(EEL_state *es, EEL_object *fo, int pc);

/* Returns a string with the name of opcode 'opcode'. */
const char *eel_i_name(int opcode);

/* Returns operand layout of opcode 'opcode'. */
EEL_operands eel_i_operands(int opcode);

/* Returns size of a complete instruction with 'opcode'. */
int eel_i_size(int opcode);

const char *eel_ol_name(EEL_operands oprs);

#endif	/* EEL_EC_CODER_H */
