/*
---------------------------------------------------------------------------
	ec_coder.c - EEL VM Code Generation Tools
---------------------------------------------------------------------------
 * Copyright 2004-2006, 2009-2012 David Olofson
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

#include <stdio.h>
#include <string.h>
#include "EEL.h"
#include "EEL_object.h"
#include "ec_coder.h"
#include "ec_mlist.h"
#include "e_state.h"
#include "e_object.h"
#include "e_function.h"
#include "e_module.h"
#include "e_string.h"

/*
 * Reallocate the code buffer of the function.
 * A positive value for 'size' is the total number of
 * bytes to make room for. A negative value for 'size'
 * is negated and added to codesize to produce the total
 * number of bytes to make room for.
 */
void eel_coder_realloc_code(EEL_coder *cdr, int size);

/* Reallocate the line number debug info table */
void eel_coder_realloc_lines(EEL_coder *cdr, int size);

/*
 * Like eel_coder_realloc_code(), but for the constants.
 * (Unit: 1 constant, as opposed to 1 byte.)
 */
void eel_coder_realloc_constants(EEL_coder *cdr, int size);

/*
 * Like eel_coder_realloc_constants(), but for the variables.
 */
void eel_coder_realloc_variables(EEL_coder *cdr, int size);


/* Operators */
const char *eel_opname(EEL_operators op)
{
	switch(op)
	{
	  case EEL_OP_ASSIGN:	return "ASSIGN";
	  case EEL_OP_WKASSN:	return "WKASSN";

	  case EEL_OP_POWER:	return "POWER";
	  case EEL_OP_MOD:	return "MOD";
	  case EEL_OP_DIV:	return "DIV";
	  case EEL_OP_MUL:	return "MUL";
	  case EEL_OP_SUB:	return "SUB";
	  case EEL_OP_ADD:	return "ADD";

	  case EEL_OP_VPOWER:	return "VPOWER";
	  case EEL_OP_VMOD:	return "VMOD";
	  case EEL_OP_VDIV:	return "VDIV";
	  case EEL_OP_VMUL:	return "VMUL";
	  case EEL_OP_VSUB:	return "VSUB";
	  case EEL_OP_VADD:	return "VADD";

	  case EEL_OP_BAND:	return "BAND";
	  case EEL_OP_BOR:	return "BOR";
	  case EEL_OP_BXOR:	return "BXOR";
	  case EEL_OP_SHL:	return "SHL";
	  case EEL_OP_SHR:	return "SHR";
	  case EEL_OP_ROL:	return "ROL";
	  case EEL_OP_ROR:	return "ROR";
	  case EEL_OP_BREV:	return "BREV";

	  case EEL_OP_AND:	return "AND";
	  case EEL_OP_OR:	return "OR";
	  case EEL_OP_XOR:	return "XOR";
	  case EEL_OP_EQ:	return "EQ";
	  case EEL_OP_NE:	return "NE";
	  case EEL_OP_GT:	return "GT";
	  case EEL_OP_GE:	return "GE";
	  case EEL_OP_LT:	return "LT";
	  case EEL_OP_LE:	return "LE";
	  case EEL_OP_IN:	return "IN";

	  case EEL_OP_MIN:	return "MIN";
	  case EEL_OP_MAX:	return "MAX";

	  case EEL_OP_NEG:	return "NEG";
	  case EEL_OP_CASTR:	return "CASTR";
	  case EEL_OP_CASTI:	return "CASTI";
	  case EEL_OP_CASTB:	return "CASTB";
	  case EEL_OP_TYPEOF:	return "TYPEOF";
	  case EEL_OP_SIZEOF:	return "SIZEOF";
	  case EEL_OP_CLONE:	return "CLONE";

	  case EEL_OP_NOT:	return "NOT";

	  case EEL_OP_BNOT:	return "BNOT";
	}
	return "<error>";
}


#define	EEL_I(x,y)	case EEL_O##x##_##y:	return #x; break;
const char *eel_i_name(int opcode)
{
	switch((EEL_opcodes)opcode)
	{
		EEL_INSTRUCTIONS
#ifdef EEL_VM_PROFILING
		case EEL_O_LAST + 1:	return "PROF1";	break;
		case EEL_O_LAST + 2:	return "PROF2";	break;
		case EEL_O_LAST + 3:	return "PROF3";	break;
		case EEL_O_LAST + 4:	return "PROF4";	break;
		case EEL_O_LAST + 5:	return "PROF5";	break;
		case EEL_O_LAST + 6:	return "PROF6";	break;
		case EEL_O_LAST + 7:	return "PROF7";	break;
		case EEL_O_LAST + 8:	return "PROF8";	break;
#endif
	}
	return "<BAD OP>";
}
#undef EEL_I


#define	EEL_I(x,y)	case EEL_O##x##_##y:	return EEL_OL_##y; break;
EEL_operands eel_i_operands(int opcode)
{
	switch((EEL_opcodes)opcode)
	{
		EEL_INSTRUCTIONS
	}
	return EEL_OL_ILLEGAL;
}
#undef EEL_I


int eel_i_size(int opcode)
{
	EEL_operands oprs = eel_i_operands(opcode);
	switch(oprs)
	{
	  case EEL_OL_ILLEGAL:	return 1;
	  case EEL_OL_0:	return EEL_OSIZE_0;
	  case EEL_OL_A:	return EEL_OSIZE_A;
	  case EEL_OL_Ax:	return EEL_OSIZE_Ax;
	  case EEL_OL_AB:	return EEL_OSIZE_AB;
	  case EEL_OL_ABC:	return EEL_OSIZE_ABC;
	  case EEL_OL_ABCD:	return EEL_OSIZE_ABCD;
	  case EEL_OL_sAx:	return EEL_OSIZE_sAx;
	  case EEL_OL_ABx:	return EEL_OSIZE_ABx;
	  case EEL_OL_AsBx:	return EEL_OSIZE_AsBx;
	  case EEL_OL_AxBx:	return EEL_OSIZE_AxBx;
	  case EEL_OL_AxsBx:	return EEL_OSIZE_AxsBx;
	  case EEL_OL_ABCx:	return EEL_OSIZE_ABCx;
	  case EEL_OL_ABsCx:	return EEL_OSIZE_ABsCx;
	  case EEL_OL_ABxCx:	return EEL_OSIZE_ABxCx;
	  case EEL_OL_ABxsCx:	return EEL_OSIZE_ABxsCx;
	  case EEL_OL_ABCDx:	return EEL_OSIZE_ABCDx;
	  case EEL_OL_ABCsDx:	return EEL_OSIZE_ABCsDx;
	}
	return 1;
}


const char *eel_ol_name(EEL_operands oprs)
{
	switch(oprs)
	{
	  case EEL_OL_ILLEGAL:
		break;
	  case EEL_OL_0:	return "<none>";
	  case EEL_OL_A:	return "A";
	  case EEL_OL_Ax:	return "Ax";
	  case EEL_OL_AB:	return "AB";
	  case EEL_OL_ABC:	return "ABC";
	  case EEL_OL_ABCD:	return "ABCD";
	  case EEL_OL_sAx:	return "sAx";
	  case EEL_OL_ABx:	return "ABx";
	  case EEL_OL_AsBx:	return "AsBx";
	  case EEL_OL_AxBx:	return "AxBx";
	  case EEL_OL_AxsBx:	return "AxsBx";
	  case EEL_OL_ABCx:	return "ABCx";
	  case EEL_OL_ABsCx:	return "ABsCx";
	  case EEL_OL_ABxCx:	return "ABxCx";
	  case EEL_OL_ABxsCx:	return "ABxsCx";
	  case EEL_OL_ABCDx:	return "ABCDx";
	  case EEL_OL_ABCsDx:	return "ABCsDx";
	}
	return "<BAD OPERANDS>";
}


#define	EEL_I(x,y)			\
		break;			\
	}				\
	case EEL_O##x##_##y:		\
	{				\
		EEL_OPR_##y(code + pc)	\
		pc += EEL_OSIZE_##y;

#define	BS		(EEL_SBUFSIZE - 12)
#define	MAXWIDTH	99
static const char *eel_i_stringrepx(EEL_state *es, EEL_object *fo, int pc,
		unsigned char *code)
{
	EEL_function *f;
	int count;
	const char *tmp = NULL;
	const char *tmp2 = NULL;
	char *rbuf, *buf;
	if((EEL_classes)fo->type != EEL_CFUNCTION)
		return "INTERNAL ERROR";
	f = o2EEL_function(fo);
	rbuf = eel_salloc(es);
	buf = rbuf + 12;
	snprintf(rbuf, 13, "%-12.12s", eel_i_name(code[pc]));
	switch((EEL_opcodes)code[pc])
	{
	  /* Special instructions */
	  case EEL_OILLEGAL_0:
	  case EEL_ONOP_0:
	  {

	  /* Local flow control */
	  EEL_IJUMP
		count = snprintf(buf, BS, "%d", pc + A);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", A);
	  EEL_IJUMPZ
		count = snprintf(buf, BS, "R%d, %d", A, pc + B);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", B);
	  EEL_IJUMPNZ
		count = snprintf(buf, BS, "R%d, %d", A, pc + B);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", B);
	  EEL_ISWITCH
		count = snprintf(buf, BS, "R%d, C%d, %d", A, B, pc + C);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", C);
	  EEL_IPRELOOP
		count = snprintf(buf, BS, "R%d, R%d, R%d, %d",
				A, B, C, pc + D);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", D);
	  EEL_ILOOP
		count = snprintf(buf, BS, "R%d, R%d, R%d, %d",
				A, B, C, pc + D);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (PC + %d)", D);

	  /* Argument stack operations */
	  EEL_IPUSH
		snprintf(buf, BS, "R%d", A);
	  EEL_IPUSH2
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPUSH3
		snprintf(buf, BS, "R%d, R%d, R%d", A, B, C);
	  EEL_IPUSH4
		snprintf(buf, BS, "R%d, R%d, R%d, R%d", A, B, C, D);
	  EEL_IPUSHI
		snprintf(buf, BS, "%d", A);
	  EEL_IPHTRUE
	  EEL_IPHFALSE
	  EEL_IPUSHNIL
	  EEL_IPUSHC
		count = snprintf(buf, BS, "C%d", A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[A]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", A, tmp);
	  EEL_IPUSHC2
		count = snprintf(buf, BS, "C%d, C%d", A, B);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[A]);
		tmp2 = eel_v_stringrep(es->vm, &f->e.constants[B]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; %s, %s", tmp, tmp2);
	  EEL_IPUSHCI
		count = snprintf(buf, BS, "C%d, %d", A, B);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[A]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; %s, %d", tmp, B);
	  EEL_IPUSHIC
		count = snprintf(buf, BS, "%d, C%d", B, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[A]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; %d, %s", B, tmp);
	  EEL_IPHVAR
		snprintf(buf, BS, "SV%d", A);
	  EEL_IPHUVAL
		snprintf(buf, BS, "R%d^%d", A, B);
	  EEL_IPHARGS
	  EEL_IPUSHTUP

	  /* Function calls */
	  EEL_ICALL
		snprintf(buf, BS, "R%d", A);
	  EEL_ICALLR
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_ICCALL
		count = snprintf(buf, BS, "C%d, %d", B, A);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (%s)",
				o2EEL_string(o2EEL_function(
				f->e.constants[B].objref.v)->
				common.name)->buffer);
	  EEL_ICCALLR
		count = snprintf(buf, BS, "C%d, R%d, %d", C, B, A);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (%s)",
				o2EEL_string(o2EEL_function(
				f->e.constants[C].objref.v)->
				common.name)->buffer);
	  EEL_IRETURN
	  EEL_IRETURNR
		snprintf(buf, BS, "R%d", A);

	  /* Memory management */
	  EEL_ICLEAN
		snprintf(buf, BS, "%d", A);

	  /* Optional/tuple argument checking */
	  EEL_IARGC
		snprintf(buf, BS, "R%d", A);
	  EEL_ITUPC
		snprintf(buf, BS, "R%d", A);
	  EEL_ISPEC
		snprintf(buf, BS, "ARGS[%d], R%d", A, B);
	  EEL_ITSPEC
		snprintf(buf, BS, "TUPARGS[R[%d]][], R%d", A, B);

	  /* Immediate values, constants etc */
	  EEL_ILDI
		snprintf(buf, BS, "%d, R%d", B, A);
	  EEL_ILDTRUE
		snprintf(buf, BS, "R%d", A);
	  EEL_ILDFALSE
		snprintf(buf, BS, "R%d", A);
	  EEL_ILDNIL
		snprintf(buf, BS, "R%d", A);
	  EEL_ILDC
		count = snprintf(buf, BS, "C%d, R%d", B, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[B]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", B, tmp);

	  /* Register access */
	  EEL_IMOVE
		snprintf(buf, BS, "R%d, R%d", B, A);

	  /* Register variables */
	  EEL_IINIT
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_IINITI
		snprintf(buf, BS, "%d, R%d", B, A);
	  EEL_IINITC
		count = snprintf(buf, BS, "C%d, R%d", B, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[B]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", B, tmp);
	  EEL_IINITNIL
		snprintf(buf, BS, "R%d", A);
	  EEL_IASSIGN
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_IASSIGNI
		snprintf(buf, BS, "%d, R%d", B, A);
	  EEL_IASSIGNC
		count = snprintf(buf, BS, "C%d, R%d", B, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[B]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", B, tmp);
	  EEL_IASNNIL
		snprintf(buf, BS, "R%d", A);

	  /* Upvalues */
	  EEL_IGETUVAL
		snprintf(buf, BS, "R%d^%d, R%d", B, C, A);
	  EEL_ISETUVAL
		snprintf(buf, BS, "R%d, R%d^%d", A, B, C);

	  /* Static variables */
	  EEL_IGETVAR
		snprintf(buf, BS, "SV%d, R%d", B, A);
	  EEL_ISETVAR
		snprintf(buf, BS, "R%d, SV%d", A, B);

	  /* Indexed access */
	  EEL_IINDGETI
		snprintf(buf, BS, "R%d[%d], R%d", C, B, A);
	  EEL_IINDSETI
		snprintf(buf, BS, "R%d, R%d[%d]", A, C, B);
	  EEL_IINDGET
		snprintf(buf, BS, "R%d[R%d], R%d", C, B, A);
	  EEL_IINDSET
		snprintf(buf, BS, "R%d, R%d[R%d]", A, C, B);
	  EEL_IINDGETC
		count = snprintf(buf, BS, "R%d[C%d], R%d", B, C, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[C]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", C, tmp);
	  EEL_IINDSETC
		count = snprintf(buf, BS, "R%d, R%d[C%d]", A, B, C);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[C]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", C, tmp);

	  /* Argument access */
	  EEL_IGETARGI
		snprintf(buf, BS, "ARGS[%d], R%d", B, A);
	  EEL_IPHARGI
		snprintf(buf, BS, "ARGS[%d]", A);
	  EEL_IPHARGI2
		snprintf(buf, BS, "ARGS[%d], ARGS[%d]", A, B);
	  EEL_ISETARGI
		snprintf(buf, BS, "R%d, ARGS[%d]", A, B);
	  EEL_IGETARG
		snprintf(buf, BS, "ARGS[R%d], R%d", B, A);
	  EEL_ISETARG
		snprintf(buf, BS, "R%d, ARGS[R%d]", A, B);

	  /* Tuple argument access */
	  EEL_IGETTARGI
		snprintf(buf, BS, "TUPARGS[R%d][%d], R%d", C, B, A);
	  EEL_ISETTARGI
		snprintf(buf, BS, "R%d, TUPARGS[R%d][%d]", A, C, B);
	  EEL_IGETTARG
		snprintf(buf, BS, "TUPARGS[R%d][R%d], R%d", C, B, A);
	  EEL_ISETTARG
		snprintf(buf, BS, "R%d, TUPARGS[R%d][R%d]", A, C, B);

	  /* Upvalue argument access */
	  EEL_IGETUVARGI
		snprintf(buf, BS, "ARGS[%d]^%d, R%d", B, C, A);
	  EEL_ISETUVARGI
		snprintf(buf, BS, "R%d, ARGS[%d]^%d", A, B, C);

	  /* Upvalue tuple argument access */
	  EEL_IGETUVTARGI
		snprintf(buf, BS, "TUPARGS[R%d][%d]^%d, R%d", C, B, D, A);
	  EEL_ISETUVTARGI
		snprintf(buf, BS, "R%d, TUPARGS[R%d][%d]^%d", A, C, B, D);

	  /* Operators */
	  EEL_IBOP
		snprintf(buf, BS, "R%d %s R%d, R%d", B, eel_opname(C), D, A);
	  EEL_IPHBOP
		snprintf(buf, BS, "R%d %s R%d", A, eel_opname(B), C);
	  EEL_IIPBOP
		snprintf(buf, BS, "R%d %s R%d, R%d", B, eel_opname(C), D, A);
	  EEL_IBOPS
		snprintf(buf, BS, "R%d %s SV%d, R%d", B, eel_opname(C), D, A);
	  EEL_IIPBOPS
		snprintf(buf, BS, "R%d %s SV%d, R%d", B, eel_opname(C), D, A);
	  EEL_IBOPI
		snprintf(buf, BS, "R%d %s %d, R%d", B, eel_opname(C), D, A);
	  EEL_IPHBOPI
		snprintf(buf, BS, "R%d %s %d", A, eel_opname(B), C);
	  EEL_IIPBOPI
		snprintf(buf, BS, "R%d %s %d, R%d", B, eel_opname(C), D, A);
	  EEL_IBOPC
		count = snprintf(buf, BS, "R%d %s C%d, R%d", B, eel_opname(C), D, A);
		tmp = eel_v_stringrep(es->vm, &f->e.constants[D]);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; C%d = %s", D, tmp);

	  EEL_INEG
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_IBNOT
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_INOT
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_ICASTR
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_ICASTI
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_ICASTB
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_ICAST
		snprintf(buf, BS, "(R%d)R%d, R%d", C, B, A);
	  EEL_ITYPEOF
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_ISIZEOF
		snprintf(buf, BS, "R%d, R%d", B, A);
	  EEL_IWEAKREF
		snprintf(buf, BS, "R%d, R%d", B, A);

	  EEL_IADD
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);
	  EEL_ISUB
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);
	  EEL_IMUL
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);
	  EEL_IDIV
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);
	  EEL_IMOD
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);
	  EEL_IPOWER
		snprintf(buf, BS, "R%d, R%d, R%d", B, C, A);

	  EEL_IPHADD
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPHSUB
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPHMUL
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPHDIV
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPHMOD
		snprintf(buf, BS, "R%d, R%d", A, B);
	  EEL_IPHPOWER
		snprintf(buf, BS, "R%d, R%d", A, B);

	  /* Constructors */
	  EEL_INEW
		snprintf(buf, BS, "%s, R%d", eel_typename(es->vm, B), A);
	  EEL_ICLONE
		snprintf(buf, BS, "R%d, R%d", B, A);

	  /* Exception handling */
	  EEL_ITRY
	  {
		const char *catcher = "<nil>";
		count = snprintf(buf, BS, "C%d, C%d", B, A);
		while(count < 24)
			buf[count++] = ' ';
		if(EEL_IS_OBJREF(f->e.constants[A].type))
			catcher = o2EEL_string(o2EEL_function(
					f->e.constants[A].objref.v)->
					common.name)->buffer;
		snprintf(buf + count, BS-24, "; (%s), (%s)",
				o2EEL_string(o2EEL_function(
				f->e.constants[B].objref.v)
				->common.name)->buffer,
				catcher);
	  }
	  EEL_IUNTRY
	  {
		count = snprintf(buf, BS, "C%d", A);
		while(count < 24)
			buf[count++] = ' ';
		snprintf(buf + count, BS-24, "; (%s)",
				o2EEL_string(o2EEL_function(
				f->e.constants[A].objref.v)
				->common.name)->buffer);
	  }
	  EEL_ITHROW
		snprintf(buf, BS, "R%d", A);
	  EEL_IRETRY
	  EEL_IRETX
	  EEL_IRETXR
		snprintf(buf, BS, "R%d", A);
		break;
	  }
	}
	if(!rbuf[0])
	{
		eel_sfree(es, rbuf);
		return "<ILLEGAL OPCODE>";
	}
	eel_sfree(es, tmp);
	eel_sfree(es, tmp2);
	if(rbuf[MAXWIDTH])
	{
		rbuf[MAXWIDTH-3] = rbuf[MAXWIDTH-2] = rbuf[MAXWIDTH-1] = '.';
		rbuf[MAXWIDTH] = 0;
	}
	return rbuf;
}
#undef	EEL_I
#undef	BS

const char *eel_i_stringrep(EEL_state *es, EEL_object *fo, int pc)
{
	EEL_function *f;
	if((EEL_classes)fo->type != EEL_CFUNCTION)
		return "INTERNAL ERROR";
	f = o2EEL_function(fo);
	if((pc < 0) || (pc >= f->e.codesize))
		return "<PC OUT OF RANGE>";
	return eel_i_stringrepx(es, fo, pc, f->e.code);
}


EEL_coder *eel_coder_open(EEL_state *es, EEL_object *fo)
{
	EEL_function *f;
	EEL_coder *cdr;
	if((EEL_classes)fo->type != EEL_CFUNCTION)
		eel_ierror(es, "Object is not a function!");
	f = o2EEL_function(fo);
	if(f->common.flags & EEL_FF_CFUNC)
		eel_ierror(es, "Function is not an EEL function!");
	if(!f->e.module)
		eel_ierror(es, "Function does not belong to a module!");
	cdr = (EEL_coder *)calloc(1, sizeof(EEL_coder));
	if(!cdr)
		eel_serror(es, "Could not create EEL_coder!");
	cdr->state = es;
	cdr->f = fo;
	cdr->maxcode = f->e.codesize;
	cdr->maxconstants = f->e.nconstants;
	cdr->peephole = 1;
	return cdr;
}


int eel_coder_close(EEL_coder *cdr)
{
	EEL_function *f = o2EEL_function(cdr->f);
	/* Free any unused space */
	eel_coder_realloc_code(cdr, f->e.codesize);
	eel_coder_realloc_lines(cdr, f->e.nlines);
	eel_coder_realloc_constants(cdr, f->e.nconstants);
	while(cdr->firstml)
		eel_ml_close(cdr->firstml);
	free(cdr->registers);
	free(cdr);
	return 0;
}


void eel_coder_realloc_code(EEL_coder *cdr, int size)
{
	unsigned char *nc;
	EEL_function *f = o2EEL_function(cdr->f);
	int ns = f->e.codesize;
	if(size >= 0)
		ns = size;
	else
	{
		size = ns - size;
		if(ns < 256)
			ns = 256;
		while(ns < size)
			ns *= 2;
	}
	if(ns == cdr->maxcode)
		return;

	nc = (unsigned char *)realloc(f->e.code, ns);
	if(!nc)
		eel_serror(cdr->state, "Could not reallocate code buffer!");

	f->e.code = nc;
	cdr->maxcode = ns;
}


void eel_coder_realloc_lines(EEL_coder *cdr, int size)
{
	EEL_int32 *nl;
	int ns = cdr->maxlines;
	EEL_function *f = o2EEL_function(cdr->f);
	if(size >= 0)
		ns = size;
	else
	{
		size = f->e.nlines - size;
		if(ns < 32)
			ns = 32;
		while(ns < size)
			ns *= 2;
	}
	if(ns == cdr->maxlines)
		return;

	nl = (EEL_int32 *)realloc(f->e.lines, sizeof(EEL_int32) * ns);
	if(!nl)
		eel_serror(cdr->state, "Could not reallocate line number table!");

	f->e.lines = nl;
	cdr->maxlines = ns;
}


void eel_coder_realloc_constants(EEL_coder *cdr, int size)
{
	EEL_value *nc;
	int ns = cdr->maxconstants;
	EEL_function *f = o2EEL_function(cdr->f);
	if(size >= 0)
		ns = size;
	else
	{
		size = f->e.nconstants - size;
		if(ns < 32)
			ns = 32;
		while(ns < size)
			ns *= 2;
	}
	if(ns == cdr->maxconstants)
		return;

	nc = (EEL_value *)realloc(f->e.constants, sizeof(EEL_value) * ns);
	if(!nc)
		eel_serror(cdr->state, "Could not reallocate constant table!");

	f->e.constants = nc;
	cdr->maxconstants = ns;
}


static int objects_equal(EEL_object *o1, EEL_object *o2)
{
	EEL_xno x;
	EEL_value v2, r;
	v2.type = EEL_TOBJREF;
	v2.objref.v = o2;
	x = eel_o__metamethod(o1, EEL_MM_COMPARE, &v2, &r);
	if(x)
		return 0;
	if(0 == r.integer.v)
		return 1;
	else
		return 0;
}


int eel_coder_add_constant(EEL_coder *cdr, EEL_value *value)
{
	int i;
	EEL_value *c;
	EEL_function *f = o2EEL_function(cdr->f);

	/* Do this first, in case we don't have a constant table! */
	eel_coder_realloc_constants(cdr, -1);
	c = f->e.constants;	/* Could be realloc()ed... */

	if(!value)
		eel_ierror(cdr->state, "Constant needs initializer!");

	if((EEL_nontypes)value->type > EEL_TLASTTYPE)
		eel_cerror(cdr->state, "Illegal constant type!");

	/* First check if we have one with this value already! */
/*FIXME: Can be time consuming when compiling large programs... */
	for(i = 0; i < f->e.nconstants; ++i)
	{
		if(c[i].type != value->type)
			continue;
		switch(value->type)
		{
		  case EEL_TNIL:
			return i;
		  case EEL_TREAL:
			if(c[i].real.v == value->real.v)
				return i;
			break;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			if(c[i].integer.v == value->integer.v)
				return i;
			break;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			if(c[i].objref.v == value->objref.v)
				return i;
			if(c[i].objref.v->type != value->objref.v->type)
				break;
			if(objects_equal(c[i].objref.v, value->objref.v))
				return i;
			break;
		}
	}

	/*
	 * Ok, add a new one...
	 */
	eel_v_qcopy(&c[f->e.nconstants], value);
	/*
	 * Constant tables take ownership of constant
	 * objects, except functions in the same module!
	 */
	if(EEL_IS_OBJREF(value->type) &&
			(((EEL_classes)value->objref.v->type != EEL_CFUNCTION) ||
			(o2EEL_function(value->objref.v)->common.module !=
					f->common.module)))
		eel_v_own(&c[f->e.nconstants]);

	return f->e.nconstants++;
}


void eel_coder_realloc_variables(EEL_coder *cdr, int size)
{
	EEL_value *nv;
	EEL_module *m = o2EEL_module(o2EEL_function(cdr->f)->e.module);
	int ns = m->maxvariables;
	if(size >= 0)
		ns = size;
	else
	{
		size = m->nvariables - size;
		if(ns < 32)
			ns = 32;
		while(ns <= size)
			ns *= 2;
	}
	if(ns == m->maxvariables)
		return;
	nv = (EEL_value *)realloc(m->variables, sizeof(EEL_value) * ns);
	if(!nv)
		eel_serror(cdr->state, "Could not reallocate variable table!");

	m->variables = nv;
	m->maxvariables = ns;
}


int eel_coder_add_variable(EEL_coder *cdr, EEL_value *value)
{
	EEL_value *v;
	EEL_module *m = o2EEL_module(o2EEL_function(cdr->f)->e.module);

	/* Do this first, in case we don't have a variable table! */
	eel_coder_realloc_variables(cdr, -1);
	v = m->variables + m->nvariables;

	if(value)
	{
		if((EEL_nontypes)value->type > EEL_TLASTTYPE)
			eel_cerror(cdr->state, "Illegal variable"
					" initializer type!");
		eel_v_copy(v, value);
	}
	else
		v->type = EEL_TNIL;

	return m->nvariables++;
}


void eel_coder_finalize_module(EEL_coder *cdr)
{
	EEL_module *m = o2EEL_module(o2EEL_function(cdr->f)->e.module);
	eel_coder_realloc_variables(cdr, m->nvariables);
}


/*----------------------------------------------------------
	Register allocator
----------------------------------------------------------*/

static void realloc_regs(EEL_coder *cdr)
{
	int i;
	int newsize = cdr->maxregisters ? cdr->maxregisters * 2 : 16;
	cdr->registers = (EEL_regspec *)realloc(cdr->registers,
			sizeof(EEL_regspec) * newsize);
	if(!cdr->registers)
		eel_serror(cdr->state, "Could not reallocate regspec table!");
	for(i = cdr->maxregisters; i < newsize; ++i)
		cdr->registers[i].use = EEL_RUFREE;
	cdr->maxregisters = newsize;
}


/* Bump clean table size if needed */
static void check_cleansize(EEL_coder *cdr)
{
	EEL_function *f = o2EEL_function(cdr->f);
	int i, clean;
	clean = 0;
	for(i = 0; i < cdr->maxregisters; ++i)
		if(EEL_RUVARIABLE == cdr->registers[i].use)
			++clean;
	if(clean > f->e.cleansize)
		f->e.cleansize = clean;
}


int eel_r_alloc(EEL_coder *cdr, int count, EEL_reguse use)
{
	EEL_function *f = o2EEL_function(cdr->f);
	/* Find range of 'count' free registers */
	int i;
	int first = 0;
	DBG9C(for(i = 0; i < cdr->maxregisters; ++i)
		printf("R%d:%d  ", i, cdr->registers[i].use);)
	while(1)
	{
		int ok = 1;
		for(i = first; i < first + count; ++i)
		{
			if(i >= cdr->maxregisters)
				realloc_regs(cdr);
			if(cdr->registers[i].use != EEL_RUFREE)
			{
				ok = 0;
				break;
			}
		}
		if(ok)
			break;
		++first;
	}

	/* Bump the function register frame size */
	if(first + count >= f->e.framesize)
		f->e.framesize = first + count + 1;

	/* Set the register usage codes */
	for(i = first; i < first + count; ++i)
		cdr->registers[i].use = use;

	check_cleansize(cdr);

	DBG9C(printf("\nALLOC R%d..R%d, use %d\n",
			first, first + count - 1, use);)
	return first;
}


int eel_r_alloc_top(EEL_coder *cdr, int count, EEL_reguse use)
{
	EEL_function *f = o2EEL_function(cdr->f);
	int first, i;
	for(first = cdr->maxregisters - 1; first >= 0; --first)
		if(EEL_RUFREE != cdr->registers[first].use)
			break;
	++first;
	while(first + count >= cdr->maxregisters)
		realloc_regs(cdr);

	/* Bump the function register frame size */
	if(first + count >= f->e.framesize)
		f->e.framesize = first + count + 1;

	/* Set the register usage codes */
	for(i = first; i < first + count; ++i)
		cdr->registers[i].use = use;

	check_cleansize(cdr);

	return first;
}


void eel_r_alloc_reg(EEL_coder *cdr, int r, EEL_reguse use)
{
	EEL_function *f = o2EEL_function(cdr->f);
	if(r >= cdr->maxregisters)
		realloc_regs(cdr);
	if(cdr->registers[r].use != EEL_RUFREE)
		eel_ierror(cdr->state, "Tried to allocate in-use register!");

	/* Bump the function register frame size */
	if(r >= f->e.framesize)
		f->e.framesize = r + 1;

	/* Set the register usage code */
	cdr->registers[r].use = use;

	check_cleansize(cdr);
}


void eel_r_free(EEL_coder *cdr, int first, int count)
{
	EEL_function *f = o2EEL_function(cdr->f);
	int max = count > 0 ? (first + count) : f->e.framesize - 1;
	if(max >= cdr->maxregisters)
		realloc_regs(cdr);
	DBG9C({int i;
	for(i = 0; i < cdr->maxregisters; ++i)
		printf("R%d:%d  ", i, cdr->registers[i].use);
	printf("\nFREE R%d..R%d\n", first, first + count - 1);})
	if(first < 0 || count <= 0)
		eel_ierror(cdr->state, "Someone tried to free %d registers"
				" starting at R%d!", count, first);
	while(first < max)
		cdr->registers[first++].use = EEL_RUFREE;
}


EEL_regspec *eel_r_spec(EEL_coder *cdr, int r)
{
	if(r < 0 || r >= cdr->maxregisters)
		eel_ierror(cdr->state, "Compiler asked stuff about an"
				" out of range register!");
	if(EEL_RUFREE == cdr->registers[r].use)
		eel_ierror(cdr->state, "Compiler asked stuff about a"
				" free register!");
	return &cdr->registers[r];
}


/*----------------------------------------------------------
	Low level code generation
----------------------------------------------------------*/
static int find_line(EEL_bio *b)
{
	int sline, scol;
	eel_bio_linecount(b, eel_bio_tell(b), EEL_TAB_SIZE, &sline, &scol);
	return sline;
}

/* Remove debug line info for 'count' instructions starting at 'start' */
static inline void eel_remove_lineinfo(EEL_coder *cdr, int start, int count)
{
	EEL_function *f = o2EEL_function(cdr->f);
	EEL_int32 *li = f->e.lines;
	int i = cdr->fragline;
	int pc = cdr->fragstart;
	while(pc < start)
	{
		pc += eel_i_size(f->e.code[pc]);
		++i;
	}
	memmove(li + i, li + i + count,
			(f->e.nlines - i - count) * sizeof(EEL_int32));
	f->e.nlines -= count;
}

/* Remove 'count' bytes, starting at position 'start' */
static inline void eel_code_remove_bytes(EEL_coder *cdr, int start, int count)
{
	EEL_function *f = o2EEL_function(cdr->f);
	unsigned char *c = f->e.code;
	memmove(c + start, c + start + count, f->e.codesize - start - count);
	f->e.codesize -= count;
}

#ifdef EEL_PEEPHOLE_OPTIMIZER
#define EEL_MKOPT(op1, op2)	((op1) * (EEL_O_LAST + 1) + (op2))
/*
 * Attempt to make a peephole substitution at the indicated position.
 * Returns 1 if a substitution was made.
 *
 *	To keep things simple and robust, the optimization is done in three
 *	steps:
 *		1. Analyze the input and generate the substitution code.
 *		2. Calculate size difference and move following code as needed.
 *		3. Move the generated code from the end of the block into the
 *		   target area.
 */
static inline int eel_peephole_subst(EEL_coder *cdr, int pc)
{
	EEL_function *f = o2EEL_function(cdr->f);
	unsigned old_len, new_len, old_end = f->e.codesize;
	int diff, icdiff;
	unsigned char *i1 = f->e.code + pc;
	int pc2 = pc + eel_i_size(i1[0]);
	unsigned char *i2 = f->e.code + pc2;
	int icountout, icountin = 2;
	int old_nlines = f->e.nlines;
	if(pc2 >= f->e.codesize)
		return 0;
	old_len = eel_i_size(i1[0]) + eel_i_size(i2[0]);
	switch(EEL_MKOPT(i1[0], i2[0]))
	{
	  case EEL_MKOPT(EEL_OPUSH_A, EEL_OPUSH_A):
		/*	Before:			After:
		 *	PUSH R[x]		PUSH2 R[x], R[y]
		 *	PUSH R[y]
		 */
		eel_codeAB(cdr, EEL_OPUSH2_AB, i1[1], i2[1]);
		break;
	  case EEL_MKOPT(EEL_OPUSH2_AB, EEL_OPUSH_A):
		/*
		 *	PUSH2 R[x], R[y]	PUSH3 R[x], R[y], R[z]
		 *	PUSH R[z]
		 */
		eel_codeABC(cdr, EEL_OPUSH3_ABC, i1[1], i1[2], i2[1]);
		break;
	  case EEL_MKOPT(EEL_OPUSH2_AB, EEL_OPUSH2_AB):
		/*
		 *	PUSH2 R[x], R[y]	PUSH4 R[x], R[y], R[z], R[w]
		 *	PUSH2 R[z], R[w]
		 */
		eel_codeABCD(cdr, EEL_OPUSH4_ABCD, i1[1], i1[2], i2[1], i2[2]);
		break;
/*FIXME: Can we ever get PUSH3; PUSH; or vice versa...? */
	  case EEL_MKOPT(EEL_OPUSHC_Ax, EEL_OPUSHC_Ax):
		/*	Before:			After:
		 *	PUSHC Cx		PUSHC2 Cx, Cy
		 *	PUSHC Cy
		 */
		eel_codeAxBx(cdr, EEL_OPUSHC2_AxBx, EEL_O16(i1, 1),
				EEL_O16(i2, 1));
		break;
	  case EEL_MKOPT(EEL_OPUSHC_Ax, EEL_OPUSHI_sAx):
		/*	Before:			After:
		 *	PUSHC Cx		PUSHCI Cx, y
		 *	PUSHI y
		 */
		eel_codeAxsBx(cdr, EEL_OPUSHCI_AxsBx, EEL_O16(i1, 1),
				EEL_OS16(i2, 1));
		break;
	  case EEL_MKOPT(EEL_OPUSHI_sAx, EEL_OPUSHC_Ax):
		/*	Before:			After:
		 *	PUSHI x			PUSHIC x, Cy
		 *	PUSHC Cy
		 */
		eel_codeAxsBx(cdr, EEL_OPUSHIC_AxsBx, EEL_O16(i2, 1),
				EEL_OS16(i1, 1));
		break;
	  case EEL_MKOPT(EEL_ONOT_AB, EEL_OJUMPZ_AsBx):
		/*
		 *	NOT R[?], R[x]		JUMPNZ R[x], ?
		 *	JUMPZ R[x], ?
		 */
		if(i1[1] != i2[1])
			return 0;
		eel_codeAsBx(cdr, EEL_OJUMPNZ_AsBx, i1[2], 0);
		break;
	  case EEL_MKOPT(EEL_ONOT_AB, EEL_OJUMPNZ_AsBx):
		/*
		 *	NOT R[?], R[x]		JUMPZ R[x], ?
		 *	JUMPNZ R[x], ?
		 */
		if(i1[1] != i2[1])
			return 0;
		eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, i1[2], 0);
		break;
	  case EEL_MKOPT(EEL_OLDI_AsBx, EEL_OINIT_AB):
		/*
		 *	LDI ?, R[x]		INITI ?, R[x]
		 *	INIT R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeAsBx(cdr, EEL_OINITI_AsBx, i2[1], EEL_OS16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDI_AsBx, EEL_OASSIGN_AB):
		/*
		 *	LDI ?, R[x]		ASSIGNI ?, R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeAsBx(cdr, EEL_OASSIGNI_AsBx, i2[1], EEL_OS16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDNIL_A, EEL_OINIT_AB):
		/*
		 *	LDNIL R[x]		INITNIL R[x]
		 *	INIT R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeA(cdr, EEL_OINITNIL_A, i2[1]);
		break;
	  case EEL_MKOPT(EEL_OLDNIL_A, EEL_OASSIGN_AB):
		/*
		 *	LDNIL R[x]		ASNNIL R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeA(cdr, EEL_OASNNIL_A, i2[1]);
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINIT_AB):
		/*
		 *	LDC C?, R[x]		INITC C?, R[x]
		 *	INIT R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeABx(cdr, EEL_OINITC_ABx, i2[1], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OASSIGN_AB):
		/*
		 *	LDC C?, R[x]		ASSIGNC C?, R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeABx(cdr, EEL_OASSIGNC_ABx, i2[1], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OGETARGI_AB, EEL_OPUSH_A):
		/*
		 *	GETARGI args[?], R[x]	PHARGI args[?]
		 *	PUSH R[x]
		 */
		if(i1[1] != i2[1])
			return 0;
		eel_codeA(cdr, EEL_OPHARGI_A, i1[2]);
		break;
	  case EEL_MKOPT(EEL_OPHARGI_A, EEL_OPHARGI_A):
		/*
		 *	PHARGI args[x]		PHARGI2 args[x], args[y]
		 *	PHARGI args[y]
		 */
		eel_codeAB(cdr, EEL_OPHARGI2_AB, i1[1], i2[1]);
		break;
	  case EEL_MKOPT(EEL_OBOP_ABCD, EEL_OPUSH_A):
		/*
		 *	BOP R[x] y R[z], R[w]	PHBOP R[x] y R[z]
		 *	PUSH R[w]
		 */
		if(i1[1] != i2[1])
			return 0;
		eel_codeABC(cdr, EEL_OPHBOP_ABC, i1[2], i1[3], i1[4]);
		break;
	  case EEL_MKOPT(EEL_OBOPI_ABCsDx, EEL_OPUSH_A):
		/*
		 *	BOPI R[x] y z, R[w]	PHBOPI R[x] y z
		 *	PUSH R[w]
		 */
		if(i1[1] != i2[1])
			return 0;
		eel_codeABsCx(cdr, EEL_OPHBOPI_ABsCx, i1[2], i1[3],
				EEL_OS16(i1, 4));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINDGET_ABC):
		/*
		 *	LDC Cx, R[y]		INDGETC R[z][Cx], R[w]
		 *	INDGET R[z][R[y]], R[w]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeABCx(cdr, EEL_OINDGETC_ABCx, i2[1], i2[3], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINDSET_ABC):
		/*
		 *	LDC Cx, R[y]		INDSETC R[w], R[z][Cx]
		 *	INDSET R[w], R[z][R[y]]
		 */
		if(i1[1] != i2[2])
			return 0;
		eel_codeABCx(cdr, EEL_OINDSETC_ABCx, i2[1], i2[3], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OBOP_ABCD):
		/*
		 *	LDC Cx, R[y]		BOPC R[z] w Cx, R[u]
		 *	BOP R[z] w R[y], R[u]
		 */
		if(i1[1] != i2[4])
			return 0;
		eel_codeABCDx(cdr, EEL_OBOPC_ABCDx, i2[1], i2[2], i2[3],
				EEL_O16(i1, 2));
		break;
	  default:
		/* Single instruction substitutions */
		icountin = 1;
		old_len = eel_i_size(i1[0]);
		switch(i1[0])
		{
		  case EEL_OBOP_ABCD:
		  {
			int op;
			switch(i1[3])
			{
			  case EEL_OP_ADD:	op = EEL_OADD_ABC; break;
			  case EEL_OP_SUB:	op = EEL_OSUB_ABC; break;
			  case EEL_OP_MUL:	op = EEL_OMUL_ABC; break;
			  case EEL_OP_DIV:	op = EEL_ODIV_ABC; break;
			  case EEL_OP_MOD:	op = EEL_OMOD_ABC; break;
			  case EEL_OP_POWER:	op = EEL_OPOWER_ABC; break;
			  default:
				return 0;
			}
			eel_codeABC(cdr, op, i1[1], i1[2], i1[4]);
			break;
		  }
		  case EEL_OPHBOP_ABC:
		  {
			int op;
			switch(i1[2])
			{
			  case EEL_OP_ADD:	op = EEL_OPHADD_AB; break;
			  case EEL_OP_SUB:	op = EEL_OPHSUB_AB; break;
			  case EEL_OP_MUL:	op = EEL_OPHMUL_AB; break;
			  case EEL_OP_DIV:	op = EEL_OPHDIV_AB; break;
			  case EEL_OP_MOD:	op = EEL_OPHMOD_AB; break;
			  case EEL_OP_POWER:	op = EEL_OPHPOWER_AB; break;
			  default:
				return 0;
			}
			eel_codeAB(cdr, op, i1[1], i1[3]);
			break;
		  }
		  default:
			return 0;
		}
		break;
	}
	new_len = f->e.codesize - old_end;	/* Size of new code */
	diff = old_len - new_len;
	icountout = f->e.nlines - old_nlines; /* Number of instructions issued */
	icdiff = icountin - icountout;
#ifdef DEBUG
	if(diff < 0)
		eel_ierror(cdr->state, "Peephole substitution larger than the "
				" original code!");
	if(icdiff < 0)
		eel_ierror(cdr->state, "Peephole substitution has more "
				"instructions than the original code!");
#endif
	eel_remove_lineinfo(cdr, pc, icdiff);	/* Remove excess lineinfo */
	old_nlines -= icdiff;
	eel_code_remove_bytes(cdr, pc, diff);	/* Remove excess bytes */
	old_end -= diff;
	memcpy(f->e.code + pc, f->e.code + old_end, new_len);	/* Paste! */
	f->e.nlines = old_nlines;
	eel_code_remove_bytes(cdr, old_end, new_len);	/* Clean up! */
	return 1;
}
#endif

int eel_code_target(EEL_coder *cdr)
{
#ifdef EEL_PEEPHOLE_OPTIMIZER
	EEL_function *f = o2EEL_function(cdr->f);
	DBG9D(int changed2 = 0;)
	DBG9D(unsigned char *origcode;)
	DBG9D(EEL_int32 *origlines;)
	DBG9D(int origsize = f->e.codesize;)
	DBG9D(printf("Optimizing fragment %d-%d of %s()...\n", cdr->fragstart,
			f->e.codesize, o2EEL_string(f->common.name)->buffer);)
	DBG9D(origcode = malloc(origsize);)
	DBG9D(memcpy(origcode, f->e.code, origsize);)
	DBG9D(origlines = malloc(f->e.nlines * sizeof(EEL_int32));)
	DBG9D(memcpy(origlines, f->e.lines, f->e.nlines * sizeof(EEL_int32));)
	while(cdr->peephole)
	{
#ifdef DEBUG
		int jmp;
#endif
		int pc = cdr->fragstart;
		int changed = 0;
		while(pc < f->e.codesize)
		{
			cdr->codeonly = 1;	/* Prevent infinite recursion! */
			changed += eel_peephole_subst(cdr, pc);
			cdr->codeonly = 0;
#ifdef DEBUG
			/*
			 * Sanity check: The optimizer doesn't relocate, so
			 * we'd better only try to move jumps that are to be
			 * "wired" later on!
			 */
			switch(f->e.code[pc])
			{
			  case EEL_OJUMP_sAx:
				jmp = EEL_OS16(f->e.code + pc, 1);
				break;
			  case EEL_OJUMPZ_AsBx:
			  case EEL_OJUMPNZ_AsBx:
				jmp = EEL_OS16(f->e.code + pc, 2);
				break;
			  default:
				jmp = 0;
				break;
			}
			if(jmp)
				eel_ierror(cdr->state, "Post-fixup jump found "
						"by peephole optimizer!");
#endif
			pc += eel_i_size(f->e.code[pc]);
		}
		DBG9D(changed2 += changed;)
		if(!changed)
			break;
	}
	DBG9D(if(changed2)
	{
		int pc = cdr->fragstart;
		int i = cdr->fragline;
		printf("  .- In ---------------------------------\n");
		while(pc < origsize)
		{
			const char *tmp = eel_i_stringrepx(cdr->state, cdr->f,
					pc, origcode);
			printf("  | [%6.1d]   %6.1d: %s\n", origlines[i], pc, tmp);
			eel_sfree(cdr->state, tmp);
			pc += eel_i_size(origcode[pc]);
			++i;
		}
		free(origcode);
		free(origlines);
		printf("  |- Out --------------------------------\n");
		pc = cdr->fragstart;
		i = cdr->fragline;
		while(pc < f->e.codesize)
		{
			const char *tmp = eel_i_stringrep(cdr->state, cdr->f, pc);
			printf("  | [%6.1d]   %6.1d: %s\n", f->e.lines[i], pc, tmp);
			eel_sfree(cdr->state, tmp);
			pc += eel_i_size(f->e.code[pc]);
			++i;
		}
		printf("  '--------------------------------------\n");
		if(i != f->e.nlines)
			printf("WARNING: Line number data does not match actual"
					" code size! (%d/%d)\n", f->e.nlines, i);
	})
#endif
#if DBG8(1) + 0 == 1
	printf("(%4.1d)%6.1d: --- new fragment ---\n",
			find_line(cdr->state->context->bio), f->e.codesize);
#endif
	cdr->fragstart = f->e.codesize;
	cdr->fragline = f->e.nlines;
	return f->e.codesize;
}


static inline int do_code(EEL_coder *cdr, unsigned char *ins, int len)
{
	EEL_function *f = o2EEL_function(cdr->f);
	int i, line = find_line(cdr->state->context->bio);
	int ipos = f->e.codesize;
#ifdef EEL_DEAD_CODE_ELIMINATION
	if(eel_test_exit(cdr->state) != EEL_EYES)
#endif
	{
		/* Bump the code buffer size, if needed */
		if(f->e.codesize + len >= cdr->maxcode)
			eel_coder_realloc_code(cdr, -len);
		/* Copy the instruction */
		for(i = 0; i < len; ++i)
			f->e.code[f->e.codesize++] = ins[i];

		/* Append line number debug info */
		if(f->e.nlines + 1 >= cdr->maxlines)
			eel_coder_realloc_lines(cdr, -1);
		f->e.lines[f->e.nlines++] = line;
	}
#ifdef EEL_DEAD_CODE_ELIMINATION
	else
	{
#  ifdef EEL_DEAD_CODE_ILLEGAL
		if(f->e.codesize + len >= cdr->maxcode)
			eel_coder_realloc_code(cdr, -len);
		if(f->e.nlines + len >= cdr->maxlines)
			eel_coder_realloc_lines(cdr, -len);
		f->e.code[f->e.codesize++] = EEL_OILLEGAL_0;
		f->e.lines[f->e.nlines++] = line;
		for(i = 1; i < len; ++i)
		{
			f->e.code[f->e.codesize++] = EEL_ONOP_0;
			f->e.lines[f->e.nlines++] = line;
		}
#  endif
		ipos = -ipos;
	}
#endif
	/* NOTE:
	 *	This debug code breaks if the dead code elimination is in
	 *	and ILLEGAL/NOP fill is disabled!
	 *	Also note that the printed code is the inserted code,
	 *	*before* code level optimizations, and as a result, the
	 *	positions may be way off when optimization is enabled.
	 */
#if !defined(EEL_DEAD_CODE_ELIMINATION) || defined(EEL_DEAD_CODE_ILLEGAL)
	DBG8({
		const char *tmp = eel_i_stringrep(cdr->state, cdr->f, ipos);
		DBG9B({
			switch(eel_test_exit(cdr->state))
			{
			  case EEL_ENO:		printf("       "); break;
			  case EEL_EMAYBE:	printf("(COND) "); break;
			  case EEL_EYES:	printf("(DEAD) "); break;
			}
		})
		DBGB2(eel_e_print(cdr->state);)
		printf("(%4.1d)%6.1d: %s\n", line, ipos, tmp);
		eel_sfree(cdr->state, tmp);
	})
#endif
	return ipos;
}


static inline int code(EEL_coder *cdr, unsigned char *ins, int len)
{
	int ipos = do_code(cdr, ins, len);
#ifdef DEBUG
	int offspos;
	switch(ins[0])
	{
	  case EEL_OJUMP_sAx:
		offspos = EEL_OSIZE_sAx - 2;
		break;
	  case EEL_OJUMPZ_AsBx:
	  case EEL_OJUMPNZ_AsBx:
		offspos = EEL_OSIZE_AsBx - 2;
		break;
	  case EEL_OSWITCH_ABxsCx:
		offspos = EEL_OSIZE_ABxsCx - 2;
		break;
	  case EEL_OPRELOOP_ABCsDx:
	  case EEL_OLOOP_ABCsDx:
		offspos = EEL_OSIZE_ABCsDx - 2;
		break;
	  default:
		offspos = 0;
		break;
	}
	if(offspos && (ins[offspos] || ins[offspos + 1]))
		eel_ierror(cdr->state, "Tried to code a branch offset directly!");
#endif
	if(cdr->codeonly)
		return ipos;
	switch((EEL_opcodes)ins[0])
	{
	  case EEL_OJUMP_sAx:
	  case EEL_OJUMPZ_AsBx:
	  case EEL_OJUMPNZ_AsBx:
	  case EEL_OSWITCH_ABxsCx:
	  case EEL_OPRELOOP_ABCsDx:
	  case EEL_OLOOP_ABCsDx:
	  case EEL_ORETURN_0:
	  case EEL_ORETURNR_A:
	  case EEL_OTHROW_A:
	  case EEL_ORETRY_0:
	  case EEL_ORETX_0:
	  case EEL_ORETXR_A:
		/*
		 * As of now, the optimizer never removes any of these
		 * instructions, so we can assume our new instruction
		 * is still the last instruction in the fragment, unless
		 * it was removed by dead code elimination.
		 */
		if(ipos < 0)
			return ipos;
		else
			return eel_code_target(cdr) - len;
	  default:
		/*
		 * No one's business where normal instructions go, as
		 * the optimizer will most likely move them around later!
		 */
		return -1;
	}
}


#define	EEL_RANGE(name, val, min, max)				\
	if(((val) < (min)) || ((val) > (max)))			\
	{							\
		res |= 1;					\
		eel_info(cdr->state, "Argument " #name		\
				" (%d) out of range!", val);	\
	}

#define	EEL_OPERANDS(expected)						\
	if(eel_i_operands(op) != (expected))				\
		eel_ierror(cdr->state, "Instruction %s with operand "	\
				"layout %s; should be %s!",		\
				eel_i_name(op),				\
				eel_ol_name(eel_i_operands(op)),	\
				eel_ol_name(expected));

#ifdef DEBUG
static inline const char *eel_rusename(EEL_reguse use)
{
	switch(use)
	{
	  case EEL_RUFREE:	return "FREE";
	  case EEL_RUTEMPORARY:	return "TEMPORARY";
	  case EEL_RUVARIABLE:	return "VARIABLE";
	  default:		return "<ILLEGAL>";
	}
}
/*
 * NOTE:
 *	If this one fires, try enabling DBG8 macros in e_config.h to see the
 *	code that triggers it.
 */
#define	EEL_REGUSE(op, r, u, opr)				\
	if(eel_r_spec(cdr, (r))->use != (u))			\
	{							\
		res |= 1;					\
		eel_ierror(cdr->state, "%s operand " #opr " "	\
				"refers to %s R[%d], "		\
				"but needs a %s register!",	\
				eel_i_name(op),			\
				eel_rusename(eel_r_spec(cdr, (r))->use), \
				(r), eel_rusename(u));		\
	}
#endif

int eel_code0(EEL_coder *cdr, EEL_opcodes op)
{
	unsigned char buf[EEL_OSIZE_0];
	EEL_OPERANDS(EEL_OL_0)
	buf[0] = op;
	return code(cdr, buf, EEL_OSIZE_0);
}


int eel_codeA(EEL_coder *cdr, EEL_opcodes op, int a)
{
	unsigned char buf[EEL_OSIZE_A];
	int res = 0;
	EEL_OPERANDS(EEL_OL_A)
	EEL_RANGE(A, a, 0, 255)
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OARGC_A:
	  case EEL_OTUPC_A:
	  case EEL_OLDTRUE_A:
	  case EEL_OLDFALSE_A:
	  case EEL_OLDNIL_A:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  case EEL_OINITNIL_A:
	  case EEL_OASNNIL_A:
		EEL_REGUSE(op, a, EEL_RUVARIABLE, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	return code(cdr, buf, EEL_OSIZE_A);
}


int eel_codeAx(EEL_coder *cdr, EEL_opcodes op, int a)
{
	unsigned char buf[EEL_OSIZE_Ax];
	int res = 0;
	EEL_OPERANDS(EEL_OL_Ax)
	EEL_RANGE(A, a, 0, 65535)
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a & 0xff;
	buf[2] = a >> 8;
	return code(cdr, buf, EEL_OSIZE_Ax);
}


int eel_codeAB(EEL_coder *cdr, EEL_opcodes op, int a, int b)
{
	unsigned char buf[EEL_OSIZE_AB];
	int res = 0;
	EEL_OPERANDS(EEL_OL_AB)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OCALLR_AB:
	  case EEL_OSPEC_AB:
	  case EEL_OTSPEC_AB:
		EEL_REGUSE(op, b, EEL_RUTEMPORARY, "B")
		break;
	  case EEL_OMOVE_AB:
	  case EEL_OGETARGI_AB:
	  case EEL_OGETARG_AB:
	  case EEL_ONEG_AB:
	  case EEL_OBNOT_AB:
	  case EEL_ONOT_AB:
	  case EEL_OCASTR_AB:
	  case EEL_OCASTI_AB:
	  case EEL_OCASTB_AB:
	  case EEL_OTYPEOF_AB:
	  case EEL_OSIZEOF_AB:
	  case EEL_OWEAKREF_AB:
	  case EEL_ONEW_AB:
	  case EEL_OCLONE_AB:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  case EEL_OINIT_AB:
	  case EEL_OASSIGN_AB:
		EEL_REGUSE(op, a, EEL_RUVARIABLE, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	return code(cdr, buf, EEL_OSIZE_AB);
}


int eel_codeABC(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c)
{
	unsigned char buf[EEL_OSIZE_ABC];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABC)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, 0, 255);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OGETUVAL_ABC:
	  case EEL_OINDGETI_ABC:
	  case EEL_OINDGET_ABC:
	  case EEL_OGETTARGI_ABC:
	  case EEL_OGETTARG_ABC:
	  case EEL_OGETUVARGI_ABC:
	  case EEL_OCAST_ABC:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c;
	return code(cdr, buf, EEL_OSIZE_ABC);
}


int eel_codeABCD(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d)
{
	unsigned char buf[EEL_OSIZE_ABCD];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABCD)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, 0, 255);
	EEL_RANGE(D, d, 0, 255);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OGETUVTARGI_ABCD:
	  case EEL_OBOP_ABCD:
	  case EEL_OIPBOP_ABCD:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c;
	buf[4] = d;
	return code(cdr, buf, EEL_OSIZE_ABCD);
}


int eel_codesAx(EEL_coder *cdr, EEL_opcodes op, int a)
{
	unsigned char buf[EEL_OSIZE_sAx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_sAx)
	EEL_RANGE(A, a, -32768, 32767);
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a & 0xff;
	buf[2] = a >> 8;
	return code(cdr, buf, EEL_OSIZE_sAx);
}


int eel_codeABx(EEL_coder *cdr, EEL_opcodes op, int a, int b)
{
	unsigned char buf[EEL_OSIZE_ABx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 65535);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OINITC_ABx:
	  case EEL_OASSIGNC_ABx:
		EEL_REGUSE(op, a, EEL_RUVARIABLE, "A")
		break;
	  case EEL_OLDC_ABx:
	  case EEL_OGETVAR_ABx:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b & 0xff;
	buf[3] = b >> 8;
	return code(cdr, buf, EEL_OSIZE_ABx);
}


int eel_codeAsBx(EEL_coder *cdr, EEL_opcodes op, int a, int b)
{
	unsigned char buf[EEL_OSIZE_AsBx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_AsBx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, -32768, 32767);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OINITI_AsBx:
	  case EEL_OASSIGNI_AsBx:
		EEL_REGUSE(op, a, EEL_RUVARIABLE, "A")
		break;
	  case EEL_OLDI_AsBx:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b & 0xff;
	buf[3] = b >> 8;
	return code(cdr, buf, EEL_OSIZE_AsBx);
}


int eel_codeAxBx(EEL_coder *cdr, EEL_opcodes op, int a, int b)
{
	unsigned char buf[EEL_OSIZE_AxBx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_AxBx)
	EEL_RANGE(A, a, 0, 65535);
	EEL_RANGE(B, b, 0, 65535);
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a & 0xff;
	buf[2] = a >> 8;
	buf[3] = b & 0xff;
	buf[4] = b >> 8;
	return code(cdr, buf, EEL_OSIZE_AxBx);
}


int eel_codeAxsBx(EEL_coder *cdr, EEL_opcodes op, int a, int b)
{
	unsigned char buf[EEL_OSIZE_AxBx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_AxsBx)
	EEL_RANGE(A, a, 0, 65535);
	EEL_RANGE(B, b, -32768, 32767);
	if(res)
		eel_ierror(cdr->state, "Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a & 0xff;
	buf[2] = a >> 8;
	buf[3] = b & 0xff;
	buf[4] = b >> 8;
	return code(cdr, buf, EEL_OSIZE_AxsBx);
}


int eel_codeABCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c)
{
	unsigned char buf[EEL_OSIZE_ABsCx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABCx)
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OCCALLR_ABCx:
		EEL_REGUSE(op, b, EEL_RUTEMPORARY, "B")
		break;
	  default:
		break;
	}
#endif
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, 0, 65535);
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c & 0xff;
	buf[4] = c >> 8;
	return code(cdr, buf, EEL_OSIZE_ABCx);
}


int eel_codeABsCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c)
{
	unsigned char buf[EEL_OSIZE_ABsCx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABsCx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, -32768, 32767);
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c & 0xff;
	buf[4] = c >> 8;
	return code(cdr, buf, EEL_OSIZE_ABsCx);
}


int eel_codeABxCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c)
{
	unsigned char buf[EEL_OSIZE_ABxCx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABxCx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 65535);
	EEL_RANGE(C, c, 0, 65535);
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b & 0xff;
	buf[3] = b >> 8;
	buf[4] = c & 0xff;
	buf[5] = c >> 8;
	return code(cdr, buf, EEL_OSIZE_ABxCx);
}


int eel_codeABxsCx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c)
{
	unsigned char buf[EEL_OSIZE_ABxsCx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABxsCx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 65535);
	EEL_RANGE(C, c, -32768, 32767);
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b & 0xff;
	buf[3] = b >> 8;
	buf[4] = c & 0xff;
	buf[5] = c >> 8;
	return code(cdr, buf, EEL_OSIZE_ABxsCx);
}


int eel_codeABCDx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d)
{
	unsigned char buf[EEL_OSIZE_ABCDx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABCDx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, 0, 255);
	EEL_RANGE(D, d, 0, 65535);
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c;
	buf[4] = d & 0xff;
	buf[5] = d >> 8;
	return code(cdr, buf, EEL_OSIZE_ABCDx);
}


int eel_codeABCsDx(EEL_coder *cdr, EEL_opcodes op, int a, int b, int c, int d)
{
	unsigned char buf[EEL_OSIZE_ABCsDx];
	int res = 0;
	EEL_OPERANDS(EEL_OL_ABCsDx)
	EEL_RANGE(A, a, 0, 255);
	EEL_RANGE(B, b, 0, 255);
	EEL_RANGE(C, c, 0, 255);
	EEL_RANGE(D, d, -32768, 32767);
#ifdef DEBUG
	switch(op)
	{
	  case EEL_OPRELOOP_ABCsDx:
	  case EEL_OLOOP_ABCsDx:
		EEL_REGUSE(op, a, EEL_RUVARIABLE, "A")
		break;
	  case EEL_OBOPS_ABCsDx:
	  case EEL_OIPBOPS_ABCsDx:
	  case EEL_OBOPI_ABCsDx:
	  case EEL_OIPBOPI_ABCsDx:
		EEL_REGUSE(op, a, EEL_RUTEMPORARY, "A")
		break;
	  default:
		break;
	}
#endif
	if(res)
		eel_ierror(cdr->state,"Could not generate %s instruction!",
				eel_i_name(op));
	buf[0] = op;
	buf[1] = a;
	buf[2] = b;
	buf[3] = c;
	buf[4] = d & 0xff;
	buf[5] = d >> 8;
	return code(cdr, buf, EEL_OSIZE_ABCsDx);
}


void eel_code_setjump(EEL_coder *cdr, int pos, int whereto)
{
	int isize, offspos;
	unsigned char *ins;
	if(pos < 0)
		return;		/* Code generation was disabled! --> */

	ins = o2EEL_function(cdr->f)->e.code + pos;
	DBG9(printf("setjump from %d to %d\n", pos, whereto);)
	switch(ins[0])
	{
	  case EEL_OJUMP_sAx:
		isize = EEL_OSIZE_sAx;
		offspos = EEL_OSIZE_sAx - 2;
		break;
	  case EEL_OJUMPZ_AsBx:
	  case EEL_OJUMPNZ_AsBx:
		isize = EEL_OSIZE_AsBx;
		offspos = EEL_OSIZE_AsBx - 2;
		break;
	  case EEL_OSWITCH_ABxsCx:
		isize = EEL_OSIZE_ABxsCx;
		offspos = EEL_OSIZE_ABxsCx - 2;
		break;
	  case EEL_OPRELOOP_ABCsDx:
	  case EEL_OLOOP_ABCsDx:
		isize = EEL_OSIZE_ABCsDx;
		offspos = EEL_OSIZE_ABCsDx - 2;
		break;
	  default:
		eel_ierror(cdr->state, "Tried to setjump() non-jump instruction!");
	}
	whereto -= pos + isize;
	if(whereto < -32768 || whereto > 32767)
		eel_ierror(cdr->state, "Relative jump out of range!");
	ins[offspos] = whereto & 0xff;
	ins[offspos + 1] = whereto >> 8;
}
