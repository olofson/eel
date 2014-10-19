/*
---------------------------------------------------------------------------
	ec_optimizer.c - EEL Bytecode Optimizer
---------------------------------------------------------------------------
 * Copyright 2005-2014 David Olofson
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

#include "ec_optimizer.h"
#include "ec_coder.h"
#include "e_function.h"


#ifdef EEL_PEEPHOLE_OPTIMIZER
/*
 * Attempt to make a peephole substitution at the indicated position.
 * Returns 1 if a substitution was made.
 *
 *	To keep things simple and robust, the optimization is done in three
 *	steps:
 *	  1. Analyze the input and generate the substitution code as usual;
 *	     that is, after the end of the current fragment.
 *	  2. Calculate size difference and move subsequent code as needed.
 *	  3. Move the generated code from the end of the block into the target
 *	     area.
 */
#define EEL_MKOPT(op1, op2)	((op1) * (EEL_O_LAST + 1) + (op2))
static inline int eel_peephole_subst(EEL_coder *cdr, int pc, unsigned flags)
{
	EEL_function *f = o2EEL_function(cdr->f);
	int keepregs = (flags & EEL_OPTIMIZE_KEEP_REGISTERS);
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
		if(keepregs || (i1[1] != i2[1]))
			return 0;
		eel_codeAsBx(cdr, EEL_OJUMPNZ_AsBx, i1[2], 0);
		break;
	  case EEL_MKOPT(EEL_ONOT_AB, EEL_OJUMPNZ_AsBx):
		/*
		 *	NOT R[?], R[x]		JUMPZ R[x], ?
		 *	JUMPNZ R[x], ?
		 */
		if(keepregs || (i1[1] != i2[1]))
			return 0;
		eel_codeAsBx(cdr, EEL_OJUMPZ_AsBx, i1[2], 0);
		break;
	  case EEL_MKOPT(EEL_OLDI_AsBx, EEL_OINIT_AB):
		/*
		 *	LDI ?, R[x]		INITI ?, R[x]
		 *	INIT R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeAsBx(cdr, EEL_OINITI_AsBx, i2[1], EEL_OS16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDI_AsBx, EEL_OASSIGN_AB):
		/*
		 *	LDI ?, R[x]		ASSIGNI ?, R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeAsBx(cdr, EEL_OASSIGNI_AsBx, i2[1], EEL_OS16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDNIL_A, EEL_OINIT_AB):
		/*
		 *	LDNIL R[x]		INITNIL R[x]
		 *	INIT R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeA(cdr, EEL_OINITNIL_A, i2[1]);
		break;
	  case EEL_MKOPT(EEL_OLDNIL_A, EEL_OASSIGN_AB):
		/*
		 *	LDNIL R[x]		ASNNIL R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeA(cdr, EEL_OASNNIL_A, i2[1]);
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINIT_AB):
		/*
		 *	LDC C?, R[x]		INITC C?, R[x]
		 *	INIT R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeABx(cdr, EEL_OINITC_ABx, i2[1], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OASSIGN_AB):
		/*
		 *	LDC C?, R[x]		ASSIGNC C?, R[x]
		 *	ASSIGN R[x], R[?]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeABx(cdr, EEL_OASSIGNC_ABx, i2[1], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OGETARGI_AB, EEL_OPUSH_A):
		/*
		 *	GETARGI args[?], R[x]	PHARGI args[?]
		 *	PUSH R[x]
		 */
		if(keepregs || (i1[1] != i2[1]))
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
		if(keepregs || (i1[1] != i2[1]))
			return 0;
		eel_codeABC(cdr, EEL_OPHBOP_ABC, i1[2], i1[3], i1[4]);
		break;
	  case EEL_MKOPT(EEL_OBOPI_ABCsDx, EEL_OPUSH_A):
		/*
		 *	BOPI R[x] y z, R[w]	PHBOPI R[x] y z
		 *	PUSH R[w]
		 */
		if(keepregs || (i1[1] != i2[1]))
			return 0;
		eel_codeABsCx(cdr, EEL_OPHBOPI_ABsCx, i1[2], i1[3],
				EEL_OS16(i1, 4));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINDGET_ABC):
		/*
		 *	LDC Cx, R[y]		INDGETC R[z][Cx], R[w]
		 *	INDGET R[z][R[y]], R[w]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeABCx(cdr, EEL_OINDGETC_ABCx, i2[1], i2[3], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OINDSET_ABC):
		/*
		 *	LDC Cx, R[y]		INDSETC R[w], R[z][Cx]
		 *	INDSET R[w], R[z][R[y]]
		 */
		if(keepregs || (i1[1] != i2[2]))
			return 0;
		eel_codeABCx(cdr, EEL_OINDSETC_ABCx, i2[1], i2[3], EEL_O16(i1, 2));
		break;
	  case EEL_MKOPT(EEL_OLDC_ABx, EEL_OBOP_ABCD):
		/*
		 *	LDC Cx, R[y]		BOPC R[z] w Cx, R[u]
		 *	BOP R[z] w R[y], R[u]
		 */
		if(keepregs || (i1[1] != i2[4]))
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
	icountout = f->e.nlines - old_nlines;	/* # of instructions issued */
	icdiff = icountin - icountout;
#ifdef DEBUG
	if(diff < 0)
		eel_ierror(cdr->state, "Peephole substitution larger than the "
				" original code!");
	if(icdiff < 0)
		eel_ierror(cdr->state, "Peephole substitution has more "
				"instructions than the original code!");
#endif
	eel_code_remove_lineinfo(cdr, pc, icdiff); /* Remove excess lineinfo */
	old_nlines -= icdiff;
	eel_code_remove_bytes(cdr, pc, diff);	/* Remove excess bytes */
	old_end -= diff;
	memcpy(f->e.code + pc, f->e.code + old_end, new_len);	/* Paste! */
	f->e.nlines = old_nlines;
	eel_code_remove_bytes(cdr, old_end, new_len);	/* Clean up! */
	return 1;
}
#endif /* EEL_PEEPHOLE_OPTIMIZER */


void eel_optimize(EEL_coder *cdr, unsigned flags)
{
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
		int pc = cdr->fragstart;
		int changed = 0;
		while(pc < f->e.codesize)
		{
			/*
			 * Branch instructions and the like will trigger the
			 * peephole optimizer, so we need this to prevent
			 * infinite recursion!
			 */
			int codeonly_save = cdr->codeonly;
			cdr->codeonly = 1;
			changed += eel_peephole_subst(cdr, pc, flags);
			cdr->codeonly = codeonly_save;
#ifdef DEBUG
			/*
			 * Sanity check: The optimizer doesn't relocate, so
			 * we'd better only try to move jumps that are to be
			 * "wired" later on!
			 */
			if(eel_code_getjump(cdr, pc) >= 0)
				eel_ierror(cdr->state, "Post-fixup branch "
						"instruction found by "
						"peephole optimizer!");
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
			const char *tmp = eel_i_stringrep(cdr->state, cdr->f,
					pc, origcode);
			printf("  | [%6.1d]   %6.1d: %s\n", origlines[i], pc,
					tmp);
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
			const char *tmp = eel_i_stringrep(cdr->state, cdr->f,
					pc, NULL);
			printf("  | [%6.1d]   %6.1d: %s\n", f->e.lines[i],
					pc, tmp);
			eel_sfree(cdr->state, tmp);
			pc += eel_i_size(f->e.code[pc]);
			++i;
		}
		printf("  '--------------------------------------\n");
		if(i != f->e.nlines)
			printf("WARNING: Line number data does not match "
					"actual code size! (%d/%d)\n",
					f->e.nlines, i);
	})
#if DBG8(1) + 0 == 1
	printf("(%4.1d)%6.1d: --- new fragment ---\n",
			eel_code_find_line(cdr), f->e.codesize);
#endif
	cdr->fragstart = f->e.codesize;
	cdr->fragline = f->e.nlines;
}
