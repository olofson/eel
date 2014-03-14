/*
---------------------------------------------------------------------------
	e_vm.h - EEL Virtual Machine
---------------------------------------------------------------------------
 * Copyright 2004-2007, 2009-2012 David Olofson
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

#ifndef	EEL_E_VM_H
#define	EEL_E_VM_H

#include "e_eel.h"


/*----------------------------------------------------------
	The VM Instructions
----------------------------------------------------------*/
/*
 * VM instruction operands:
 *	A, B...	First, second etc operand
 *	A	Normal: 8 bits unsigned
 *	sA	'sX' ==> 8 bits signed
 *	Ax	'Xx' ==> 16 bits unsigned
 *	sAx	'sXx' ==> 16 bits signed
 *
 * VM addressing modes:
 *	R[x]	Register x (EEL_value) in the register
 *		frame.
 *	c[x]	Get constant (EEL_value) from constant
 *		table index x.
 *	sv[x]	Static variable x in the module static
 *		variable table.
 */
/* Special instructions */
#define	EEL_IILLEGAL	EEL_I(ILLEGAL, 0)
#define	EEL_INOP	EEL_I(NOP, 0)

/* Local flow control */
#define	EEL_IJUMP	EEL_I(JUMP, sAx)	/* PC += sAx; */
#define	EEL_IJUMPZ	EEL_I(JUMPZ, AsBx)	/* If R[A] then PC += sBx; */
#define	EEL_IJUMPNZ	EEL_I(JUMPNZ, AsBx)	/* If !R[A] then PC += sBx; */
#define	EEL_IJUMPEQ	EEL_I(JUMPEQ, ABsCx)	/* If R[A] == R[B] then PC += sCx; */
#define	EEL_IJUMPNE	EEL_I(JUMPNE, ABsCx)	/* If R[A] != R[B] then PC += sCx; */
#define	EEL_IJUMPGE	EEL_I(JUMPGE, ABsCx)	/* If R[A] >= R[B] then PC += sCx; */
#define	EEL_IJUMPLE	EEL_I(JUMPLE, ABsCx)	/* If R[A] <= R[B] then PC += sCx; */
#define	EEL_IJUMPGT	EEL_I(JUMPGT, ABsCx)	/* If R[A] > R[B] then PC += sCx; */
#define	EEL_IJUMPLT	EEL_I(JUMPLT, ABsCx)	/* If R[A] < R[B] then PC += sCx; */
#define	EEL_ISWITCH	EEL_I(SWITCH, ABxsCx)	/* try PC = c[Bx][R[A]]; */
						/* except PC += sCx; */
#define	EEL_IPRELOOP	EEL_I(PRELOOP, ABCsDx)
			/* R[A, B, C] = (real)R[A, B, C];
			 * if R[B] < 0 then
			 *	if R[A] < R[C] PC += sDx;
			 * else
			 *	if R[A] < R[C] PC += sDx;
			 */
#define	EEL_ILOOP	EEL_I(LOOP, ABCsDx)
			/* R[A] = (real)R[A] + R[B];
			 * if R[B] < 0 then
			 *	if R[A] >= R[C] PC += sDx;
			 * else
			 *	if R[A] <= R[C] PC += sDx;
			 */

/* Argument stack operations */
#define	EEL_IPUSH	EEL_I(PUSH, A)		/* push R[A]; */
#define	EEL_IPUSH2	EEL_I(PUSH2, AB)	/* push R[A]; push R[B]; */
#define	EEL_IPUSH3	EEL_I(PUSH3, ABC)	/* push R[A]; ... push R[C]; */
#define	EEL_IPUSH4	EEL_I(PUSH4, ABCD)	/* push R[A]; ... push R[D]; */
#define	EEL_IPUSHI	EEL_I(PUSHI, sAx)	/* push sAx; */
#define	EEL_IPHTRUE	EEL_I(PHTRUE, 0)	/* push true; */
#define	EEL_IPHFALSE	EEL_I(PHFALSE, 0)	/* push false; */
#define	EEL_IPUSHNIL	EEL_I(PUSHNIL, 0)	/* push nil; */
#define	EEL_IPUSHC	EEL_I(PUSHC, Ax)	/* push c[Ax]; */
#define	EEL_IPUSHC2	EEL_I(PUSHC2, AxBx)	/* push c[Ax]; push c[Bx]; */
#define	EEL_IPUSHIC	EEL_I(PUSHIC, AxsBx)	/* push sBx, c[Ax]; */
#define	EEL_IPUSHCI	EEL_I(PUSHCI, AxsBx)	/* push c[Ax], sBx; */
#define	EEL_IPHVAR	EEL_I(PHVAR, Ax)	/* push sv[Ax]; */
#define	EEL_IPHUVAL	EEL_I(PHUVAL, AB)	/* push R[A] B levels up; */
#define	EEL_IPHARGS	EEL_I(PHARGS, 0)	/* push args[]; */
#define	EEL_IPUSHTUP	EEL_I(PUSHTUP, 0)	/* push tupargs[]; */

/* Function calls */
#define	EEL_ICALL	EEL_I(CALL, A)
			/* Call object referred to by R[A]. Arguments are passed
			 * via the argument stack.
			 */
#define	EEL_ICALLR	EEL_I(CALLR, AB)
			/* Call object referred to by R[A] with R[B] being the
			 * result register. Arguments are passed via the
			 * argument stack.
			 */
#define	EEL_ICCALL	EEL_I(CCALL, ABx)
			/* Call func c[Bx], A being the scope nesting skip
			 * depth, where 0 means we're calling a local function,
			 * or that the function called does not use upvalues.
			 */
#define	EEL_ICCALLR	EEL_I(CCALLR, ABCx)
			/* Call func c[Cx] with R[B] being the result register
			 * (not touched unless there is a result), A being the
			 * scope nesting skip depth, where 0 means we're calling
			 * a local function, or that the function called does
			 * not use upvalues.
			 */
#define	EEL_IRETURN	EEL_I(RETURN, 0)	/* Clean up and return. */
#define	EEL_IRETURNR	EEL_I(RETURNR, A)	/* result = R[A]; CLEAN; RETURN; */

/* Memory management */
#define	EEL_ICLEAN	EEL_I(CLEAN, A)		/* Clean variables [A..top] */

/* Optional/tuple argument checking */
#define	EEL_IARGC	EEL_I(ARGC, A)		/* R[A] = argument count */
#define	EEL_ITUPC	EEL_I(TUPC, A)		/* R[A] = tuple count */
#define	EEL_ISPEC	EEL_I(SPEC, AB)		/* R[B] = specified(args[A]) */
#define	EEL_ITSPEC	EEL_I(TSPEC, AB)	/* R[B] = specified(tupargs[R[A]][]) */

/* Immediate values, constants etc */
#define	EEL_ILDI	EEL_I(LDI, AsBx)	/* R[A] = sBx; */
#define	EEL_ILDTRUE	EEL_I(LDTRUE, A)	/* R[A] = true; */
#define	EEL_ILDFALSE	EEL_I(LDFALSE, A)	/* R[A] = false; */
#define	EEL_ILDNIL	EEL_I(LDNIL, A)		/* R[A] = nil; */
#define	EEL_ILDC	EEL_I(LDC, ABx)		/* R[A] = c[Bx]; */

/* Register access */
#define	EEL_IMOVE	EEL_I(MOVE, AB)		/* R[A] = R[B]; */

/* Register variables */
#define	EEL_IINIT	EEL_I(INIT, AB)
			/* R[A] = R[B]; own R[A]; cleantable(R[A]) */
#define	EEL_IINITI	EEL_I(INITI, AsBx)	/* R[A] = sBx; cleantable(R[A]) */
#define	EEL_IINITNIL	EEL_I(INITNIL, A)	/* R[A] = nil; cleantable(R[A]) */
#define	EEL_IINITC	EEL_I(INITC, ABx)
			/* R[A] = c[Bx]; own R[A]; cleantable(R[A]) */
#define	EEL_IASSIGN	EEL_I(ASSIGN, AB)
			/* disown R[A]; R[A] = R[B]; own R[A] */
#define	EEL_IASSIGNI	EEL_I(ASSIGNI, AsBx)	/* disown R[A]; R[A] = sBx */
#define	EEL_IASNNIL	EEL_I(ASNNIL, A)	/* disown R[A]; R[A] = nil */
#define	EEL_IASSIGNC	EEL_I(ASSIGNC, ABx)
			/* disown R[A]; R[A] = c[Bx]; own R[A] */

/* Upvalues */
#define	EEL_IGETUVAL	EEL_I(GETUVAL, ABC)	/* R[A] = R[B] C levels up; */
#define	EEL_ISETUVAL	EEL_I(SETUVAL, ABC)
			/* disown R[B]; R[B] C levels up = R[A]; own R[B]; */

/* Static variables */
#define	EEL_IGETVAR	EEL_I(GETVAR, ABx)	/* R[A] = sv[Bx]; */
#define	EEL_ISETVAR	EEL_I(SETVAR, ABx)
			/* disown sv[Bx]; sv[Bx] = R[A]; own sv[Bx]; */

/* Indexed access (array/table)  */
#define	EEL_IINDGETI	EEL_I(INDGETI, ABC)	/* R[A] = R[C][B]; */
#define	EEL_IINDSETI	EEL_I(INDSETI, ABC)	/* R[C][B] = R[A]; */
#define	EEL_IINDGET	EEL_I(INDGET, ABC)	/* R[A] = R[C][R[B]]; */
#define	EEL_IINDSET	EEL_I(INDSET, ABC)	/* R[C][R[B]] = R[A]; */
#define	EEL_IINDGETC	EEL_I(INDGETC, ABCx)	/* R[A] = R[B][c[Cx]]; */
#define	EEL_IINDSETC	EEL_I(INDSETC, ABCx)	/* R[B][c[Cx]] = R[A]; */

/* Argument access */
#define	EEL_IGETARGI	EEL_I(GETARGI, AB)	/* R[A] = args[B]; */
#define	EEL_IPHARGI	EEL_I(PHARGI, A)	/* push args[A]; */
#define	EEL_IPHARGI2	EEL_I(PHARGI2, AB)	/* push args[A]; push args[B]; */
#define	EEL_ISETARGI	EEL_I(SETARGI, AB)	/* args[B] = R[A]; */
#if 0
#define	EEL_IGETARG	EEL_I(GETARG, AB)	/* R[A] = args[R[B]]; */
#define	EEL_ISETARG	EEL_I(SETARG, AB)	/* args[R[B]] = R[A]; */
#endif

/* Tuple argument access */
#define	EEL_IGETTARGI	EEL_I(GETTARGI, ABC)	/* R[A] = tupargs[R[C]][B]; */
#if 0
#define	EEL_ISETTARGI	EEL_I(SETTARGI, ABC)	/* tupargs[R[C]][B] = R[A]; */
#define	EEL_IGETTARG	EEL_I(GETTARG, ABC)	/* R[A] = tupargs[R[C]][R[B]]; */
#define	EEL_ISETTARG	EEL_I(SETTARG, ABC)	/* tupargs[R[C]][R[B]] = R[A]; */
#endif

/* Upvalue argument access */
#define	EEL_IGETUVARGI	EEL_I(GETUVARGI, ABC)	/* R[A] = (args uvlevel C)[B]; */
#define	EEL_ISETUVARGI	EEL_I(SETUVARGI, ABC)	/* (args uvlevel C)[B] = R[A]; */

/* Upvalue tuple argument access */
#define	EEL_IGETUVTARGI	EEL_I(GETUVTARGI, ABCD)	/* R[A] = (tupargs uvlevel D)[R[C]][B]; */
#if 0
#define	EEL_ISETUVTARGI	EEL_I(SETUVTARGI, ABCD)	/* (tupargs uvlevel D)[R[C]][B] = R[A]; */
#endif

/* Operators */
#define	EEL_IBOP	EEL_I(BOP, ABCD)	/* R[A] = R[B] op[C] R[D]; */
#define	EEL_IPHBOP	EEL_I(PHBOP, ABC)	/* push R[A] op[B] R[C]; */
#define	EEL_IIPBOP	EEL_I(IPBOP, ABCD)	/* R[A] = R[B].op[C](R[D]); */
#define	EEL_IBOPS	EEL_I(BOPS, ABCsDx)	/* R[A] = R[B] op[C] sv[D]; */
#define	EEL_IIPBOPS	EEL_I(IPBOPS, ABCsDx)	/* R[A] = R[B].op[C](sv[D]); */
#define	EEL_IBOPI	EEL_I(BOPI, ABCsDx)	/* R[A] = R[B] op[C] D; */
#define	EEL_IPHBOPI	EEL_I(PHBOPI, ABsCx)	/* push R[A] op[B] C; */
#define	EEL_IIPBOPI	EEL_I(IPBOPI, ABCsDx)	/* R[A] = R[B].op[C](D); */
#define	EEL_IBOPC	EEL_I(BOPC, ABCDx)	/* R[A] = R[B] op[C] c[Dx]; */

#define	EEL_INEG	EEL_I(NEG, AB)		/* R[A] = -R[B]; */
#define	EEL_IBNOT	EEL_I(BNOT, AB)		/* R[A] = bitwise not R[B]; */
#define	EEL_INOT	EEL_I(NOT, AB)		/* R[A] = not R[B]; */
#define	EEL_ICASTR	EEL_I(CASTR, AB)	/* R[A] = real R[B]; */
#define	EEL_ICASTI	EEL_I(CASTI, AB)	/* R[A] = integer R[B]; */
#define	EEL_ICASTB	EEL_I(CASTB, AB)	/* R[A] = boolean R[B]; */
#define	EEL_ICAST	EEL_I(CAST, ABC)	/* R[A] = (R[C]) R[B]; */
#define	EEL_ITYPEOF	EEL_I(TYPEOF, AB)	/* R[A] = typeof R[B]; */
#define	EEL_ISIZEOF	EEL_I(SIZEOF, AB)	/* R[A] = sizeof R[B]; */
#define	EEL_IWEAKREF	EEL_I(WEAKREF, AB)	/* R[A] = (weakref) R[B]; */

#define	EEL_IADD	EEL_I(ADD, ABC)		/* R[A] = R[B] + R[C]; */
#define	EEL_ISUB	EEL_I(SUB, ABC)		/* R[A] = R[B] - R[C]; */
#define	EEL_IMUL	EEL_I(MUL, ABC)		/* R[A] = R[B] * R[C]; */
#define	EEL_IDIV	EEL_I(DIV, ABC)		/* R[A] = R[B] / R[C]; */
#define	EEL_IMOD	EEL_I(MOD, ABC)		/* R[A] = R[B] % R[C]; */
#define	EEL_IPOWER	EEL_I(POWER, ABC)	/* R[A] = R[B] ** R[C]; */

#define	EEL_IPHADD	EEL_I(PHADD, AB)	/* push R[A] + R[B]; */
#define	EEL_IPHSUB	EEL_I(PHSUB, AB)	/* push R[A] - R[B]; */
#define	EEL_IPHMUL	EEL_I(PHMUL, AB)	/* push R[A] * R[B]; */
#define	EEL_IPHDIV	EEL_I(PHDIV, AB)	/* push R[A] / R[B]; */
#define	EEL_IPHMOD	EEL_I(PHMOD, AB)	/* push R[A] % R[B]; */
#define	EEL_IPHPOWER	EEL_I(PHPOWER, AB)	/* push R[A] ** R[B]; */

/* Constructors */
#define	EEL_INEW	EEL_I(NEW, AB)
			/* R[A] = instance of type B from argument stack */
#define	EEL_ICLONE	EEL_I(CLONE, AB)	/* R[A] = clone of R[B]; */

/* Exception handling */
#define	EEL_ITRY	EEL_I(TRY, AxBx)
			/* Call try block function c[Bx], using function c[Ax]
			 * as exception catcher. The try block function can have
			 * no arguments and no return value. Nor can the catcher
			 * function, but it gets the exception value in R[0]
			 * (read only!) when called. If the exception value is
			 * an object, it will be in in the limbo list, as if
			 * returned from the try block function.
			 */
#define	EEL_IUNTRY	EEL_I(UNTRY, Ax)
			/* Call try block function c[Ax], dumping on any
			 * exception, even if we're in an outer try block. The
			 * try block function can have no arguments and no
			 * return value.
			 */
#define	EEL_ITHROW	EEL_I(THROW, A)		/* Throw R[A] as an exception */
#define	EEL_IRETRY	EEL_I(RETRY, 0)
			/* Clean up and return from an exception catcher,
			 * adjusting pc to rerun the TRY instruction that
			 * invoked it. Of course, all hell will break lose if
			 * this is used in a normal function!
			 */
#define	EEL_IRETX	EEL_I(RETX, 0)		/* THROW XRETURN; */
#define	EEL_IRETXR	EEL_I(RETXR, A)		/* result = R[A]; THROW XRETURN; */

/*
 * List of all instruction macros (define EEL_I(name,args) first!)
 */
#define	EEL_INSTRUCTIONS						\
	EEL_IILLEGAL	EEL_INOP					\
	EEL_IJUMP	EEL_IJUMPZ	EEL_IJUMPNZ	EEL_ISWITCH	\
	EEL_IPRELOOP	EEL_ILOOP					\
	EEL_IPUSH	EEL_IPUSH2	EEL_IPUSH3	EEL_IPUSH4	\
	EEL_IPUSHI	EEL_IPHTRUE	EEL_IPHFALSE	EEL_IPUSHNIL	\
	EEL_IPUSHC	EEL_IPUSHC2	EEL_IPUSHIC	EEL_IPUSHCI	\
	EEL_IPHVAR	EEL_IPHUVAL	EEL_IPUSHTUP	EEL_IPHARGS	\
	EEL_ICALL	EEL_ICALLR	EEL_ICCALL	EEL_ICCALLR	\
	EEL_IRETURN	EEL_IRETURNR					\
	EEL_ICLEAN							\
	EEL_IARGC	EEL_ITUPC	EEL_ISPEC	EEL_ITSPEC	\
	EEL_ILDI	EEL_ILDTRUE	EEL_ILDFALSE	EEL_ILDNIL	\
	EEL_ILDC							\
	EEL_IMOVE							\
	EEL_IINIT	EEL_IINITI	EEL_IINITNIL	EEL_IINITC	\
	EEL_IASSIGN	EEL_IASSIGNI	EEL_IASNNIL	EEL_IASSIGNC	\
	EEL_IGETUVAL	EEL_ISETUVAL					\
	EEL_IGETVAR	EEL_ISETVAR					\
	EEL_IINDSETI	EEL_IINDGETI	EEL_IINDSET	EEL_IINDGET	\
	EEL_IINDSETC	EEL_IINDGETC					\
	EEL_IGETARGI	EEL_IPHARGI	EEL_IPHARGI2	EEL_ISETARGI	\
	EEL_IGETTARGI	EEL_IGETUVARGI	EEL_ISETUVARGI	EEL_IGETUVTARGI	\
	EEL_IBOP	EEL_IPHBOP	EEL_IIPBOP			\
	EEL_IBOPS	EEL_IIPBOPS					\
	EEL_IBOPI	EEL_IPHBOPI	EEL_IIPBOPI			\
	EEL_IBOPC							\
	EEL_INEG	EEL_IBNOT	EEL_INOT			\
	EEL_ICASTR	EEL_ICASTI	EEL_ICASTB	EEL_ICAST	\
	EEL_ITYPEOF	EEL_ISIZEOF	EEL_IWEAKREF			\
	EEL_IADD	EEL_ISUB	EEL_IMUL	EEL_IDIV	\
	EEL_IMOD	EEL_IPOWER					\
	EEL_IPHADD	EEL_IPHSUB	EEL_IPHMUL	EEL_IPHDIV	\
	EEL_IPHMOD	EEL_IPHPOWER					\
	EEL_INEW	EEL_ICLONE					\
	EEL_ITRY	EEL_IUNTRY	EEL_ITHROW	EEL_IRETRY	\
	EEL_IRETX	EEL_IRETXR

#define	EEL_I_LAST	EEL_IRETXR


/*
 * Operand decoding macros
 */
#define	EEL_O8(ins, i)		(ins)[i]
#define	EEL_OS8(ins, i)		(signed char)((ins)[i])
#define	EEL_O16(ins, i)		(ins)[i] | ((ins)[(i) + 1] << 8)
#define	EEL_OS16(ins, i)	(ins)[i] | ((signed char)(ins)[(i) + 1] << 8)

#define	EEL_OPR_0(ins)
#define	EEL_OSIZE_0		1

#define	EEL_OPR_A(ins)			\
	unsigned A = EEL_O8((ins), 1);
#define	EEL_OSIZE_A		2

#define	EEL_OPR_Ax(ins)		\
	unsigned A = EEL_O16((ins), 1);
#define	EEL_OSIZE_Ax		3

#define	EEL_OPR_AB(ins)			\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);
#define	EEL_OSIZE_AB		3

#define	EEL_OPR_ABC(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	unsigned C = EEL_O8((ins), 3);
#define	EEL_OSIZE_ABC		4

#define	EEL_OPR_ABCD(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	unsigned C = EEL_O8((ins), 3);	\
	unsigned D = EEL_O8((ins), 4);
#define	EEL_OSIZE_ABCD		5

#define	EEL_OPR_sAx(ins)		\
	int A = EEL_OS16((ins), 1);
#define	EEL_OSIZE_sAx		3

#define	EEL_OPR_ABx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O16((ins), 2);
#define	EEL_OSIZE_ABx		4

#define	EEL_OPR_AsBx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	int B = EEL_OS16((ins), 2);
#define	EEL_OSIZE_AsBx		4

#define	EEL_OPR_AxBx(ins)		\
	unsigned A = EEL_O16((ins), 1);	\
	unsigned B = EEL_O16((ins), 3);
#define	EEL_OSIZE_AxBx		5

#define	EEL_OPR_AxsBx(ins)		\
	unsigned A = EEL_O16((ins), 1);	\
	unsigned B = EEL_OS16((ins), 3);
#define	EEL_OSIZE_AxsBx		5

#define	EEL_OPR_ABCx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	unsigned C = EEL_O16((ins), 3);
#define	EEL_OSIZE_ABCx		5

#define	EEL_OPR_ABsCx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	int C = EEL_OS16((ins), 3);
#define	EEL_OSIZE_ABsCx		5

#define	EEL_OPR_ABxCx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O16((ins), 2);	\
	unsigned C = EEL_O16((ins), 4);
#define	EEL_OSIZE_ABxCx		6

#define	EEL_OPR_ABxsCx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O16((ins), 2);	\
	int C = EEL_OS16((ins), 4);
#define	EEL_OSIZE_ABxsCx	6

#define	EEL_OPR_ABCDx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	unsigned C = EEL_O8((ins), 3);	\
	unsigned D = EEL_O16((ins), 4);
#define	EEL_OSIZE_ABCDx		6

#define	EEL_OPR_ABCsDx(ins)		\
	unsigned A = EEL_O8((ins), 1);	\
	unsigned B = EEL_O8((ins), 2);	\
	unsigned C = EEL_O8((ins), 3);	\
	int D = EEL_OS16((ins), 4);
#define	EEL_OSIZE_ABCsDx	6

/*
 * Instruction operand layouts
 */
typedef enum
{
	EEL_OL_ILLEGAL = -1,
	EEL_OL_0 = 0,
	EEL_OL_A,
	EEL_OL_Ax,
	EEL_OL_AB,
	EEL_OL_ABC,
	EEL_OL_ABCD,
	EEL_OL_sAx,
	EEL_OL_ABx,
	EEL_OL_AsBx,
	EEL_OL_AxBx,
	EEL_OL_AxsBx,
	EEL_OL_ABCx,
	EEL_OL_ABsCx,
	EEL_OL_ABxCx,
	EEL_OL_ABxsCx,
	EEL_OL_ABCDx,
	EEL_OL_ABCsDx
} EEL_operands;

/*
 * Instruction opcodes
 */
#define	EEL_I(x,y)	EEL_O##x##_##y,
typedef enum EEL_opcodes
{
	EEL_INSTRUCTIONS
	EEL_O_LAST = EEL_I_LAST
} EEL_opcodes;
#undef EEL_I


/*----------------------------------------------------------
	EEL Virtual Machine
----------------------------------------------------------*/
/*
 * Function call conventions:
 *	* Caller pushes arguments onto its argument stack,
 *	  and optionally makes room for the result before
 *	  calling.
 *	* The CALL instructions allocate the callie register
 * 	  frame right above the argument stack.
 *	* The CALL and RETURN opcodes ensure that the VM
 *	  special registers are saved and restored.
 *
 * The EEL VM stack:
 *	The EEL VM is register based, and does not have a
 *	low level stack of the kind seen in most hardware
 *	CPUs and many other VMs. (The argument stack is a
 *	pure argument passing construct, and is part of
 *	each register frame - there is no global stack.)
 *
 *	However, the EEL VM has a special register that
 *	allows the register frame to be moved around in the
 *	heap. The CALL and RETURN instructions use this
 *	register to implement a function/closure level call
 *	stack, in the form of a linked list of function
 *	call frames.
 *
 * Function call register frame:
 *	R[-n-c]	Clean Table ("Pascal string" of unsigned char)
 *	  ..
 *	 R[-n-1]
 *	R[-n]	EEL_callframe structure;
 *	  ..	    pc		Previous PC
 *	 R[-1]	    base	Previous register base
 *		    f		Current function
 *		    upvalues	Frame that contains the
 *				"nearest level" upvalues.
 *		    result	Heap index of result
 *		    limbo	Linked list of new objects to
 *				check when leaving.
 *		    cleantab	Table of registers (variables)
 *				to clean when leaving.
 *		    catcher	Function to handle exceptions
 *				from functions called from here.
 *				('try' and 'except' blocks are
 *				actually functions.)
 *	R[0]		<Work registers start here!>
 *	  ..
 *	 R[m-1]
 *	R[m]	Argument stack for further calls starts here!
 *	  ..
 *
 *	The area before the call frame is used for memory
 *	management info; the "cleaning table". The number
 *	of registers allocated for this depends on the
 *	maximum size of the cleaning table, which is
 *	calculated by the compiler.
 *
 *	NOTE:	The registers used for the cleaning table
 *		and the call frame areas do not have valid
 *		type fields, and cannot be addressed as
 *		EEL_value elements!
 *
 *	The VM registers, starting at R[0], are used for
 *	local variables and temporary storage. The layout
 *	of this area is defined by the function code, and
 *	may change during the execution of the function.
 *
 *	Above the actual VM registers of the frame is the
 *	argument stack. This is actually a dynamic array of
 *	EEL values, and is used for passing arguments to
 *	functions, and initializers to constructors.
 *
 * Calling C functions from EEL:
 *	The register frame in place when the VM calls a C
 *	function is the same as when executing an EEL function,
 *	except that the clean table is unused (minimum size),
 *	and one work register, R[0], is allocated for use as
 *	return value if the C function calls EEL functions.
 *	The argument stack is used in the same way as by EEL
 *	functions, pushing arguments using eel_*argf().
 *
 * Upvalues:
 *	The current upvalue implementation deals only with
 *	static, compile time, scope relations, and does not
 *	support closure of "orphaned" functions using upvalues.
 *	(This will likely change in future versions.)
 *	   By means of the 'upvalues' field, it is still
 *	possible for functions to access their upvalues when
 *	called from within other functions (including try and
 *	except blocks) declared on the same or lower nesting
 *	level. 'upvalues' is essentially a shortcut through
 *	the callframe chain up to the "home" level register
 *	frame of the function.
 */

/* For 'flags' */
#define	EEL_CFF_TRYBLOCK	0x00000001
#define	EEL_CFF_CATCHER		0x00000002
#define	EEL_CFF_UNTRY		0x00000004

typedef struct
{
	/* Return info */
	EEL_uint32	r_base;		/* Return register frame base */
	EEL_uint32	r_pc;		/* Return PC */
	EEL_uint32	r_sp;		/* Return stack pointer */
	EEL_uint32	r_sbase;	/* Return stack base */

	/* Current context */
	EEL_object	*f;		/* Current function (NULL if none) */
	EEL_object	*limbo;		/* Limbo object list */
#if 0
/*TODO: For grabbing args directly out of arrays? Mind the implications...! */
	EEL_value	*args;		/* Arguments */
#endif
/*FIXME: Do we really need argv/argc? Might as well look at r_sbase/r_sp...? */
	EEL_int32	argv;		/* Heap index of first argument */
	EEL_uint32	argc;		/* Argument count */
	EEL_int32	result;		/* Heap index of result */
	EEL_uint32	cleantab;	/* Variables to clean */
	EEL_int32	upvalues;	/* Upvalue register frame */
	EEL_uint32	flags;

	/* Exception handling */
	EEL_object	*catcher;	/* Exception catcher, if any */

#if DBG4E(1)+0 == 1
#define	EEL_CALLFRAME_MAGIC_EEL	0xca11fee1
#define	EEL_CALLFRAME_MAGIC_C	0xca11fccc
#define	EEL_CALLFRAME_MAGIC_0	0xca11f000
	EEL_uint32	magic;
#endif
} EEL_callframe;


/* Number of registers (EEL_value structs) needed for a call frame */
#define	EEL_CFREGS	((sizeof(EEL_callframe) + sizeof(EEL_value) - 1) / \
					sizeof(EEL_value))

/* Highest addressable register in a callframe */
#define	EEL_MAXREG	255

EEL_vm *eel_vm_open(EEL_state *es, EEL_integer heap);
void eel_vm_cleanup(EEL_vm *vm);
void eel_vm_close(EEL_vm *vm);

/*
 * Initialize 'vm' to call 'fo' with vm->argc arguments.
 * The argument list is assumed to start at 'base'.
 *
 * Returns a negative value if an error occurs,
 * otherwise 0.
 */
int eel_vm_init_call(EEL_vm *vm, EEL_object *fo, int base);

/*
 * Call function 'fo' with the previously constructed
 * argument list.
 *
 * If there is a result, vm->resv points to it,
 * otherwise vm->resv is NULL.
 *
 * Returns a negative value if an error occurs,
 * otherwise 0.
 */
int eel_vm_call(EEL_vm *vm, EEL_object *fo);

#ifdef EEL_VM_PROFILING
/* Vector item representing one VM instruction opcode */
typedef struct
{
	long long count;	/* Number of times opcode has been executed */
	long long time;		/* Total time spent executing this opcode */
} EEL_vmpentry;

/* Extra test points for more fine grained, manual profiling */
typedef enum
{
	EEL_PROF1 = EEL_O_LAST + 1,
	EEL_PROF2,
	EEL_PROF3,
	EEL_PROF4,
	EEL_PROF5,
	EEL_PROF6,
	EEL_PROF7,
	EEL_PROF8,
	EEL_VMP_POINTS
} EEL_vmpxpoints;
#endif

/* Private stuff, not suitable for the public VM struct */
#if 0
typedef struct EEL_vm_context EEL_vm_context;
#endif

typedef struct
{
	EEL_state	*state;

	/* VM execution and exception control */
	EEL_value	exception;	/* nil = no exception */

	/* String pool with cache */
	EEL_object	**strings;	/* Array of buckets (lists) */
#ifdef EEL_CACHE_STRINGS
	EEL_object	*scache;	/* List of "dead" strings */
	EEL_object	*scache_last;	/* End of cache list */
	int		scache_size;	/* Current # of cached strings */
	int		scache_max;	/* Max # of cached strings */
#endif

	/* Memory management/accounting */
#if DBGM(1) + 0 == 1
	int		owns;		/* Refcount incs */
	int		disowns;	/* Refcount decs */
	int		created;
	int		destroyed;
	int		zombified;
	int		resurrected;
	EEL_object	*afirst, *alast;
#endif

#ifdef EEL_VM_CHECKING
	int		weakrefs;	/* Number of weakrefs in heap */
#endif
#if DBG6B(1)+0 == 1
	int		instructions;	/* # of VM instructions executed */
#endif

	int		is_closing;	/* Are we destroying the state? */

#ifdef	EEL_PROFILING
	EEL_object	*p_current;	/* Currently running function */
	long long	p_time;		/* Time of entering p_current */
#endif
#ifdef	EEL_VM_PROFILING
	long long	vmp_time;	/* Time for dispath of instruction */
	int		vmp_overhead;	/* Profiling overhead correction */
	EEL_opcodes	vmp_opcode;	/* Opcode currently being timed */
	EEL_vmpentry	vmprof[EEL_VMP_POINTS];
#endif
} EEL_vm_private;

/* Get the private part, which is right after the public interface. */
static inline EEL_vm_private *eel_vm2p(EEL_vm *vm)
{
	return (EEL_vm_private *)(vm + 1);
}
#define	VMP	eel_vm2p(vm)


static inline EEL_object *eel__current_function(EEL_vm *vm)
{
	EEL_callframe *cf = (EEL_callframe *)
			(vm->heap + vm->base - EEL_CFREGS);
	return cf->f;
}

#ifdef EEL_VM_CHECKING
# define	EEL_IN_HEAP(vm, v)					\
		((v >= vm->heap) && (v < vm->heap + vm->heapsize))
#endif

#if 0
/*
 * TEMPORARY HACK
 *	This structure is used as a LIFO stack node for pushing and
 *	popping VM execution contexts; ie the parts of EEL_vm and
 *	EEL_vm_private that need one instance per VM "thread".
 *	   This mechanism was originally used when dynamic loading
 *	of scripts results in the compiler reentering the VM to
 *	execute __init_module() procedures, but there's no need for
 *	that with the current call mechanism.
 * TEMPORARY HACK
 */
struct EEL_vm_context
{
	EEL_vm_context	*prev;
	int		heapsize;
	EEL_value	*heap;
	int		pc;
	int		base;
	int		sp;
	int		sbase;
	int		resv;
	int		argv;
	int		argc;
	EEL_value	exception;
	int		active;
	EEL_object	*limbo;
};

/*
FIXME: These should be reverted to the old design, like below, and should be
FIXME: used only for coroutines, microthreads and the like. There is no need for
FIXME: a decoupled VM state for plain EEL-from-C calls!
EEL_vm_context *eel_new_vm_context(EEL_vm *vm);
void eel_delete_vm_context(EEL_vm_context *vmctx);
*/
EEL_xno eel_push_vm_context(EEL_vm *vm);
void eel_pop_vm_context(EEL_vm *vm);
#endif

#endif /*  EEL_E_VM_H */
