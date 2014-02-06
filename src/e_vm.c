/*
---------------------------------------------------------------------------
	e_vm.c - EEL Virtual Machine
---------------------------------------------------------------------------
 * Copyright 2004-2013 David Olofson
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

#ifdef DEBUG
#	include <stdio.h>
#endif

#if defined(EEL_PROFILING) || defined(EEL_VM_PROFILING)
#	include <sys/io.h>
#	include <sys/time.h>
#	include <time.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "EEL.h"
#include "e_state.h"
#include "ec_coder.h"
#include "e_object.h"
#include "e_string.h"
#include "e_vector.h"
#include "e_operate.h"
#include "e_array.h"
#include "e_function.h"


/*
 * Set a VM exception.
 *
 * NOTE:
 *	Must be followed by a call to eel__scheduler() for anything to actually
 *	happen!
 */
static inline void eel__throw(EEL_vm *vm, EEL_xno x)
{
        eel_v_disown_nz(&VMP->exception);  /* Free any old object */
        VMP->exception.type = EEL_TINTEGER;
        VMP->exception.integer.v = x;
        DBG5B(eel_vmdump(vm, "(DEBUG: Exception %s thrown.)",
			eel_v_stringrep(vm, &VMP->exception));)
}


#if DBG4E(1)+0 == 1
static void dump_callframe(EEL_vm *vm, EEL_callframe *cf, const char *where)
{
	printf("   .--- Callframe %p - %s -------\n", cf, where);
	switch(cf->magic)
	{
	  case EEL_CALLFRAME_MAGIC_EEL:
		printf("   | (EEL function)\n");
		break;
	  case EEL_CALLFRAME_MAGIC_C:
		printf("   | (C function)\n");
		break;
	  case EEL_CALLFRAME_MAGIC_0:
		printf("   | (Root callframe)\n");
		break;
	  default:
		printf("   | !!! INCORRECT MAGIC %x !!!\n", cf->magic);
		break;
	}
	printf("   |   r_base = %u\n", cf->r_base);
	printf("   |     r_pc = %u\n", cf->r_pc);
	printf("   |     r_sp = %u\n", cf->r_sp);
	printf("   |  r_sbase = %u\n", cf->r_sbase);
	printf("   | - - - - - - - - - - - - - - - - - - - -\n");
	printf("   |        f = %p\n", cf->f);
	printf("   |    limbo = %p\n", cf->limbo);
	printf("   |     argv = %d\n", cf->argv);
	printf("   |     argc = %u\n", cf->argc);
	printf("   |   result = %d\n", cf->result);
	printf("   | cleantab = %u\n", cf->cleantab);
	printf("   | upvalues = %d\n", cf->upvalues);
	printf("   |    flags = %.8x\n", cf->flags);
	printf("   | - - - - - - - - - - - - - - - - - - - -\n");
	printf("   |  catcher = %p\n", cf->catcher);
	printf("   '------------------------------------------\n");
	printf("      |  base = %u\n", vm->base);
	printf("      |    pc = %u\n", vm->pc);
	printf("      |    sp = %u\n", vm->sp);
	printf("      | sbase = %u\n", vm->sbase);
	printf("      '------------------------------------\n");
}

static void check_callframe(EEL_vm *vm, EEL_callframe *cf)
{
	switch(cf->magic)
	{
	  case EEL_CALLFRAME_MAGIC_EEL:
	  case EEL_CALLFRAME_MAGIC_C:
	  case EEL_CALLFRAME_MAGIC_0:
		break;
	  default:
		eel__throw(vm, EEL_XINTERNAL);
		eel_vmdump(vm, "Corrupt callframe detected!");
		eel_perror(vm, 0);
		dump_callframe(vm, cf, "check_callframe()");
		break;
	}
}
#endif


static inline int get_uv_base(EEL_vm *vm, unsigned uvlevel)
{
	int b = vm->base;
	while(uvlevel--)
	{
		EEL_callframe *ucf;
#ifdef EEL_VM_CHECKING
		if(!b)
		{
			printf("get_base(): Tried to get callframe of initial "
					"state!\n");
			return -1;
		}
#endif
		ucf = (EEL_callframe *)(vm->heap + b - EEL_CFREGS);
		b = ucf->upvalues;
#ifdef EEL_VM_CHECKING
		if(b < 0)
		{
			printf("get_base(): uvlevel exhausted the call stack!\n");
			return -1;
		}
#endif
	}
	return b;
}

static inline EEL_callframe *b2callframe(EEL_vm *vm, int base)
{
	return (EEL_callframe *)(vm->heap + base - EEL_CFREGS);
}


/*
 * Backtrace call frames starting at 'frame', relocating
 * any limbo object lists from the old heap base address 'oldheap'.
 *
 * This may seem like an ugly hack, but I prefer it to having special
 * cases and stuff for limbo list heads - and a heap reallocation is
 * something that should never happen in a hard RT system anyway!
 */
static void relocate_limbo(EEL_vm *vm, EEL_uint32 frame, EEL_value *oldheap)
{
/*
FIXME: This 'long long' is only needed on platforms with 64 bit pointers.
FIXME: There should be a check to avoid it in 32 bit environments, which
FIXME: don't need it anyway.
*/
	long long offset = (char *)vm->heap - (char *)oldheap;
	while(frame)
	{
		EEL_callframe *cf = b2callframe(vm, frame);
		EEL_object *o = cf->limbo;
		if(!cf->f)
			return;
		if(o)
			o->lprev = (EEL_object *)((char *)o->lprev + offset);
		frame = cf->r_base;
	}
}


/*
 * Realocate heap to 'size' value elements.
 * Returns 1 if the heap was moved to a different address,
 * 0 if it's at the same address, or -1 in case of failure.
 */
static int set_heap(EEL_vm *vm, int size)
{
	EEL_value *oh = vm->heap;
	EEL_value *h = (EEL_value *)realloc(vm->heap, size * sizeof(EEL_value));
	if(!h)
		return -1;
#ifdef EEL_VM_CHECKING
	if(VMP->weakrefs)
		fprintf(stderr, "INTERNAL ERROR: %d weakrefs in heap while "
				"resizing!\n", VMP->weakrefs);
#endif
	vm->heap = h;
	vm->heapsize = size;
	if(oh && (vm->heap != oh))
	{
		relocate_limbo(vm, vm->base, oh);
		return 1;
	}
	return 0;
}


/*
 * Ensure that the heap is at least 'minsize' value elements.
 * Returns 1 if the heap was moved to a different address,
 * 0 if it's at the same address, or -1 in case of failure.
 */
static inline int grow_heap(EEL_vm *vm, int minsize)
{
	int size = vm->heapsize;
	if(minsize <= vm->heapsize)
		return 0;
#ifdef DEBUG
	fprintf(stderr, "Heap overflow! Need %d elements... ", minsize);
#endif
	while(size < minsize)
		size <<= 1;
	if(size > vm->heapsize)
	{
		int res = set_heap(vm, size);
#ifdef DEBUG
		if(res < 0)
			fprintf(stderr, "REALLOCATION FAILED!\n");
		else if(res)
			fprintf(stderr, "OK - Heap moved!\n");
		else
			fprintf(stderr, "OK!\n");
#endif
		return res;
	}
#ifdef DEBUG
	fprintf(stderr, "OK!\n");
#endif
	return 0;
}


/* Remove all argument stack items */
static inline void stack_clear(EEL_vm *vm)
{
	EEL_value *heap = vm->heap;
	DBG7S(printf("Flushing argument stack... (at heap[%d])\n", vm->sbase);)
	while(vm->sp > vm->sbase)
	{
		--vm->sp;
		DBG7S({
			const char *s = eel_v_stringrep(vm, &heap[vm->sp]);
			printf("  [%d] %s\n", vm->sp - vm->sbase, s);
			eel_sfree(VMP->state, s);
		})
		eel_v_disown_nz(&heap[vm->sp]);
	}
	DBG7S(printf("Done.\n");)
}


/* Release objects owned by local variables */
static inline void clean(EEL_vm *vm, unsigned char *ctab, int downto)
{
	EEL_value *heap;
	int base, i;
	heap = vm->heap;
	base = vm->base;
	i = ctab[0];
	DBG7(printf("Cleaning variables... (cleantab at heap[%ld])\n",
					(EEL_value *)ctab - heap);)
//printf("### cleantab: %p\n", ctab);
	while(i > downto)
	{
		DBG7({
			const char *s = eel_v_stringrep(vm, &heap[base + ctab[i]]);
			printf("  R[%d] %s\n", ctab[i], s);
			eel_sfree(VMP->state, s);
		})
		eel_v_disown_nz(&heap[base + ctab[i]]);
		--i;
	}
	DBG7(printf("Done.\n");)
	ctab[0] = downto;
}


/* Disown potential limbo objects */
static inline void limbo_clean(EEL_vm *vm, EEL_callframe *cf)
{
	DBG7(printf("Cleaning limbo objects...\n");)
	while(cf->limbo)
	{
		EEL_object *o = cf->limbo;
		DBG7({
			const char *s = eel_o_stringrep(o);
			printf("  %s\n", s);
			eel_sfree(VMP->state, s);
		})
		eel_limbo_unlink(o);
		DBG7(printf("    Unlinked...\n");)
		eel_o_disown_nz(o);
		DBG7(printf("    Disowned...\n");)
	}
	DBG7(printf("Done.\n");)
}


static inline EEL_xno get_function(EEL_vm *vm, EEL_value *ref, EEL_object **o)
{
	if(!EEL_IS_OBJREF(ref->type))
		return EEL_XNEEDOBJECT;
	*o = ref->objref.v;
	if((EEL_classes)(*o)->type != EEL_CFUNCTION)
		return EEL_XNEEDCALLABLE;
	return 0;
}


static inline EEL_xno check_args(EEL_vm *vm, EEL_object *fo)
{
	EEL_function *f = o2EEL_function(fo);
	int argc = vm->sp - vm->sbase;
	/* Check argument count */
	if(argc < f->common.reqargs)
		return EEL_XFEWARGS;
	if(f->common.tupargs)
	{
		if((argc - f->common.reqargs) % f->common.tupargs)
			return EEL_XTUPLEARGS;
	}
	else if(((f->common.optargs != 255) &&
			(argc > f->common.reqargs + f->common.optargs)) ||
			(!f->common.optargs && (argc > f->common.reqargs)))
		return EEL_XMANYARGS;
	return 0;
}


#if defined(EEL_PROFILING) || defined(EEL_VM_PROFILING)
#if 0
inline unsigned long long int rdtsc(void)
{
	unsigned long long int x;
	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	return x;
}
static inline long long getns(void)
{
#warning Using uncalibrated RDTSC for profiling!
	return rdtsc() / 3;
}
#else
static inline long long getns(void)
{
#if 0
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)tv.tv_usec * 1000 +
			(long long)tv.tv_sec * 1000000000LL;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long long)ts.tv_nsec + (long long)ts.tv_sec * 1000000000LL;
#endif
}
#endif
#endif


#ifdef	EEL_PROFILING
static inline void switch_function(EEL_vm *vm, EEL_object *f)
{
	long long t;
	if(VMP->p_current == f)
		return;

	/* Leave previous function */
	t = getns();
	if(VMP->p_current)
	{
		EEL_function *f = o2EEL_function(VMP->p_current);
		f->common.rtime += t - VMP->p_time;
		++f->common.ccount;
	}

	/* Enter new function */
	VMP->p_current = f;
	VMP->p_time = t;
}
#else
#  define	switch_function(vm, f)
#endif

#ifdef EEL_VM_PROFILING
static inline void vmprofile_out(EEL_vm *vm)
{
	long long dt;
	if(VMP->vmp_opcode == EEL_OILLEGAL_0)
		return;
	dt = getns() - VMP->vmp_time;
	if((dt >= 0) && (dt < 1000))
	{
		++VMP->vmprof[VMP->vmp_opcode].count;
		VMP->vmprof[VMP->vmp_opcode].time += dt;
	}
	VMP->vmp_opcode = EEL_OILLEGAL_0;
}

static inline void vmprofile_in(EEL_vm *vm, EEL_opcodes opcode)
{
	if(VMP->vmp_opcode != EEL_OILLEGAL_0)
		vmprofile_out(vm);
	VMP->vmp_opcode = opcode;
	VMP->vmp_time = getns();
}
#else
#  define	vmprofile_in(vm, opcode)
#  define	vmprofile_out(vm)
#endif


/*
 * Push a new call register frame for an EEL or C function call. This also sets
 * the return info and updates the VM special registers for the new frame.
 *
 *	cleansize	Cleantable size (entries)
 *	framesize	Size of register frame (registers)
 */
static inline EEL_xno push_frame(EEL_vm *vm, unsigned cleansize, unsigned framesize)
{
	EEL_callframe *ncf;
	int base, ctab;

	/* Start the new callframe right above the argument stack */
	base = vm->sp;

	/* ...that's where the clean table starts... */
	ctab = base;
	base += (cleansize + sizeof(EEL_value)) / sizeof(EEL_value);

	/* ...followed by the call frame stuct... */
	base += EEL_CFREGS;

	/* ...and next, the callie register frame. Realloc if needed. */
	if(grow_heap(vm, base + framesize + EEL_MINSTACK) < 0)
		return EEL_XMEMORY;
#ifdef EEL_VM_CHECKING
	{
		int i;
		for(i = 0; i < framesize; ++i)
			vm->heap[base + i].type = EEL_TILLEGAL;
	}
#endif

	/* Initialize the new callframe! */
	ncf = b2callframe(vm, base);

	/* Return info */
	ncf->r_pc = vm->pc;
	ncf->r_base = vm->base;
	ncf->r_sp = vm->sp;
	ncf->r_sbase = vm->sbase;

	/* Arguments */
	ncf->argv = vm->sbase;
	ncf->argc = vm->sp - vm->sbase;

	/* Initialize memory management */
	ncf->limbo = NULL;
	ncf->cleantab = ctab;
	((unsigned char *)(vm->heap + ctab))[0] = 0;

	/* Upvalues, level 0 (will be corrected by call_eel() if needed) */
	ncf->upvalues = vm->base;
	
	/* Point VM at the callie! */
	vm->base = base;
	vm->sp = vm->sbase = base + framesize;
/*
printf("========= push_frame() ===========\n");
printf("\targv: %d\n", ncf->argv);
printf("\targc: %d\n", ncf->argc);
printf("\tcleantab: %d\n", ncf->cleantab);
printf("\tbase: %d\n", vm->base);
printf("\tsbase: %d\n", vm->sbase);
printf("==================================\n");
*/
	return 0;
}


/* Call an EEL function */
static inline EEL_xno call_eel(EEL_vm *vm, EEL_object *fo, int result, int levels)
{
	EEL_callframe *cf;
	EEL_function *f = o2EEL_function(fo);
	EEL_xno x = push_frame(vm, f->e.cleansize, f->e.framesize);
	if(x)
		return x;

	cf = b2callframe(vm, vm->base);
	DBG4E(cf->magic = EEL_CALLFRAME_MAGIC_EEL;)

	/* Grab result ref and argument list */
	if(f->common.flags & EEL_FF_RESULTS)
	{
		cf->result = result;
#ifdef EEL_VM_CHECKING
		if(cf->result >= 0)
			vm->heap[cf->result].type = EEL_TILLEGAL;
#endif
	}
	else
		cf->result = -1;	/* Tell RETURNR to discard result! */

	/* Prepare upvalue access info */
	if(levels)
	{
		cf->upvalues = get_uv_base(vm, levels + 1);
		if(cf->upvalues < 0)
		{
			vm->base = cf->r_base;
			vm->sp = cf->r_sp;
			vm->sbase = cf->r_sbase;
			return EEL_XUPVALUE;
		}
	}

	cf->flags = 0;
	cf->f = fo;
	cf->catcher = NULL;
	vm->pc = 0;

	DBG4B(printf(".--- Entering function --------\n");)
	DBG4B(printf("|     heap = %p\n", vm->heap);)
	DBG4B(printf("|     base = %d\n", vm->base);)
	DBG4B(printf("| cleantab = %d\n", cf->cleantab);)
	DBG4B(printf("|------------------------------\n");)
	DBG4B(printf("|   r_base = %d\n", cf->r_base);)
	DBG4B(printf("'------------------------------\n");)

	DBG4D(printf("Entering EEL function '%s'\n", eel_o2s(f->common.name));)

	switch_function(vm, fo);
	return 0;
}


/* Call a C function */
static inline EEL_xno call_c(EEL_vm *vm, EEL_object *fo, int result, int levels)
{
	EEL_callframe *cf;
	EEL_function *f = o2EEL_function(fo);
	EEL_xno x;
	x = push_frame(vm, 0, 1);
	if(x)
		return x;

	cf = b2callframe(vm, vm->base);
	DBG4E(cf->magic = EEL_CALLFRAME_MAGIC_C;)
	vm->heap[vm->base].type = EEL_TNIL;

	/* Grab result ref */
	if(result >= 0)
		cf->result = result;
	else
		cf->result = vm->base;

	cf->flags = 0;
	cf->f = fo;
	cf->catcher = NULL;

	/* Prepare arguments */
/*FIXME: Kill these VM fields and pass them as arguments instead... */
	vm->resv = cf->result;
	vm->argv = cf->argv;
	vm->argc = cf->argc;
/*FIXME:*/

	/* Call C function */
	DBG4D(printf("Calling C function '%s'\n", eel_o2s(f->common.name));)
	switch_function(vm, fo);
	x = f->c.cb(vm);
#if 1
	if(x)
		stack_clear(vm);
#else
#  ifdef EEL_VM_CHECKING
	if(vm->sp != vm->sbase)
	{
		eel_vmdump(vm, "call_c(): Garbage left in callie argument stack!");
		return EEL_XVMCHECK;
	}
#  endif
#endif
	/* "Fake" return */
	vm->base = cf->r_base;
	vm->sp = cf->r_sp;
	vm->sbase = cf->r_sbase;

	/* Remove arguments! */
	stack_clear(vm);
	if(x)
	{
		switch_function(vm, eel__current_function(vm));
		return x;
	}

	/* Handle/discard result apropriately */
	if(f->common.flags & EEL_FF_RESULTS)
	{
#ifdef EEL_VM_CHECKING
		if((EEL_nontypes)vm->heap[cf->result].type == EEL_TILLEGAL)
		{
			eel_vmdump(vm, "C function forgot to return a result!");
			return EEL_XVMCHECK;
		}
#endif
		/* Receive or discard the result */
		if(result >= 0)
			eel_v_receive(vm->heap + cf->result);
		else
			eel_v_disown(vm->heap + cf->result);
	}
	switch_function(vm, eel__current_function(vm));
	return 0;
}


static inline EEL_xno call_f(EEL_vm *vm, EEL_object *fo, int result, int levels)
{
	EEL_function *f = o2EEL_function(fo);
	if((result >= 0) && !(f->common.flags & EEL_FF_RESULTS))
		return EEL_XNORESULT;
	if(f->common.flags & EEL_FF_CFUNC)
#ifdef EEL_VM_PROFILING
	{
		/* Subtract the C function execution time */
		EEL_xno res;
		long long t2, t1 = getns();
		res = call_c(vm, fo, result, levels);
		t2 = getns();
		VMP->vmp_time += t2 - t1 - VMP->vmp_overhead;
		return res;
	}
#else
		return call_c(vm, fo, result, levels);
#endif
	else
		return call_eel(vm, fo, result, levels);
}


static int call_catcher(EEL_vm *vm, EEL_object *catcher)
{
	EEL_xno x;
	EEL_value *R;
	EEL_callframe *cf = b2callframe(vm, vm->base);
#ifdef EEL_VM_CHECKING
	if(EEL_OTRY_AxBx != o2EEL_function(cf->f)->e.code[vm->pc -
			eel_i_size(EEL_OTRY_AxBx)])
	{
		eel_vmdump(vm, "Trying to call catcher by other means than TRY!");
		return EEL_XVMCHECK;
	}
#endif
/*
TODO: Fast shortcut instead of dummy/empty cathers!
*/
	/*
	 * This is basically an ordinary function call, except
	 * we let the catcher inherit the result index, and we...
	 */
	x = call_eel(vm, catcher, cf->result, 0);
	if(x)
		return x;
	/*
	 * ...poke the exception value into R[0], and set EEL_CFF_CATCHER.
	 * If the exception value is an object, it's ownership is hereby
	 * transferred from VMP->exception to the catcher.
	 */
	R = vm->heap + vm->base;
	eel_v_move(&R[0], &VMP->exception);
	VMP->exception.type = EEL_TNIL;
	((EEL_callframe *)(R - EEL_CFREGS))->flags |= EEL_CFF_CATCHER;
/*printf("### call_catcher(); exception: %s\tresult: %d\n",
		eel_v_stringrep(vm, &R[0]), cf->result);
*/
	return 0;
}

/* Unwind the call stack until we're at call frame 'target'. */
static void unwind(EEL_vm *vm, int target)
{
	DBG5(printf("|--- Unwinding calls... -------\n");)
	while(1)
	{
		EEL_callframe *cf = b2callframe(vm, vm->base);
		if(vm->base == target)
		{
			DBG5(printf("| Done!\n");)
			DBG5(printf("'------------------------------\n");)
			break;
		}

		/* Clean up after current function */
		DBG5(printf("| function '%s'\n",
				eel_o2s(o2EEL_function(cf->f)->common.name));)
		DBG5(printf("| base = %d\n", vm->base);)
		if(!(o2EEL_function(cf->f)->common.flags & EEL_FF_CFUNC))
		{
			clean(vm, (unsigned char *)(vm->heap + cf->cleantab), 0);
			limbo_clean(vm, cf);
		}
		vm->base = cf->r_base;
		vm->pc = cf->r_pc;
		stack_clear(vm);
		vm->sbase = cf->r_sbase;
		vm->sp = cf->r_sp;
		DBG5(printf("'------------------------------\n");)
	}
}


/* VM "work state" */
typedef struct
{
	unsigned char	*code;	/* Code buffer */
	EEL_callframe	*cf;	/* Call frame */
	EEL_value	*r;	/* Register frame */
	EEL_value	*sv;	/* Static variable array */
	unsigned char	*ctab;	/* Register variable cleanup table */
} EEL_vmstate;


static inline void reload_context(EEL_vm *vm, EEL_vmstate *vms)
{
	EEL_function *f;
	vms->r = vm->heap + vm->base;
	vms->cf = (EEL_callframe *)(vms->r - EEL_CFREGS);
	vms->ctab = (unsigned char *)(vm->heap + vms->cf->cleantab);
	f = o2EEL_function(vms->cf->f);
	vms->sv = o2EEL_module(f->common.module)->variables;
	if(!(f->common.flags & EEL_FF_CFUNC))
		vms->code = f->e.code;
	else
		vms->code = NULL;
	switch_function(vm, vms->cf->f);
	DBG4E(dump_callframe(vm, vms->cf, "reload_context()");)
}


/*
 * Reschedule. Handles the exception previously set by eel__throw().
 *
 * Returns a non-zero value if it's time to return from the VM loop,
 * 0 if the VM should just continue executing the next VM instruction.
 */
static EEL_xno eel__scheduler(EEL_vm *vm, EEL_vmstate *vms)
{
	int x;

	/* Figure out what the exception is about */
	switch(VMP->exception.type)
	{
	  case EEL_TNIL:
		x = EEL_XYIELD;
		break;
	  case EEL_TINTEGER:
		x = VMP->exception.integer.v;
		break;
	  case EEL_TREAL:
	  case EEL_TBOOLEAN:
	  case EEL_TTYPEID:
	  case EEL_TOBJREF:
	  case EEL_TWEAKREF:
		x = EEL_XOTHER;
		break;
	  default:
		return EEL_XBADEXCEPTION;
	}

	/* Figure out where the exception is to be handled */
	switch(x)
	{
	  case EEL_XNONE:
	  case EEL_XYIELD:
/*TODO: Reschedule, if other threads should run some. */
		break;
	  case EEL_XEND:
/*TODO: If there are other threads, reschedule! */
		return EEL_XEND;
	  case EEL_XRETURN:
	  {
		/* Leave current function from within try or except block */
		int base = vm->base;
		while(base)
		{
			EEL_callframe *cf = b2callframe(vm, base);
#ifdef EEL_VM_CHECKING
			if(!cf->f)
			{
				eel_vmdump(vm, "RETX or RETXR used in the wrong place!");
				return EEL_XVMCHECK;
			}
#endif
			/* Check for exception handler */
			if(!(cf->flags & (EEL_CFF_TRYBLOCK | EEL_CFF_CATCHER)))
			{
				int result;
				DBG5(printf(".------------------------------\n");)
				DBG5(printf("| XRETURN causes function '%s' "
						"to return.\n",
						eel_o2s(o2EEL_function(cf->f)->common.name));)
				DBG5(printf("|------------------------------\n");)
				/*
				 * Unroll, clean up etc...
				 */
				unwind(vm, base);
				VMP->exception.type = EEL_TNIL;
				clean(vm, (unsigned char *)(vm->heap + cf->cleantab), 0);
				limbo_clean(vm, cf);
				/*
				 * Get result index, if any.
				 */
				result = cf->result;
				/*
				 * Prepare to continue execution in caller.
				 */
				vm->base = cf->r_base;
				vm->pc = cf->r_pc;
				stack_clear(vm);
				vm->sbase = cf->r_sbase;
				vm->sp = cf->r_sp;
/*FIXME: Is this extra stack_clear() correct, and does it cover all cases...? */
				stack_clear(vm);
				cf = b2callframe(vm, vm->base);
				if(result >= 0)
				{
					/* Caller receives result! */
					DBGK4(printf("Expecting result in heap[%d].\n",
							result);)
#ifdef EEL_VM_CHECKING
					if((EEL_nontypes)vm->heap[result].type ==
							EEL_TILLEGAL)
					{
						eel_vmdump(vm, "Exception block "
							"forgot to return a result!");
						return EEL_XVMCHECK;
					}
#endif
					eel_v_receive(vm->heap + result);
				}
				if(!cf->f)
					return EEL_XEND;
				break;
			}
			base = cf->r_base;
		}
		break;
	  }
	  default:
	  {
		/* Try to find the nearest try...except catcher. */
		int base = vm->base;
//printf("---------- finding catcher... base: %d\n", base);
		while(base)
		{
			EEL_callframe *cf = b2callframe(vm, base);

			/* Nothing more to unwind? */
			if(!cf->f)
{
//printf("----------   No function!\n");
				break;
}

			if(o2EEL_function(cf->f)->common.flags & EEL_FF_CFUNC)
{
//printf("----------   C function!\n");
				break;
}

			if(cf->flags & EEL_CFF_UNTRY)
{
//printf("----------   EEL_CFF_UNTRY!\n");
				break;
}

			/* Check for exception handler */
			if(cf->catcher && x != EEL_XINTERNAL)
			{
				EEL_object *catcher = cf->catcher;
				DBG5(printf(".------------------------------\n");)
				DBG5(printf("| Handled by catcher '%s'\n",
						eel_o2s(o2EEL_function(catcher)->
								common.name));)
				DBG5(printf("|------------------------------\n");)
				base = cf->r_base;
				unwind(vm, base);
				x = call_catcher(vm, catcher);
				if(x)
				{
					eel__throw(vm, x);
					return x;
				}
				reload_context(vm, vms);
				/* Cleaning for exception value. */
//printf("### cleantab: %p\n", vms->ctab);
				vms->ctab[++vms->ctab[0]] = 0;
				return 0;
			}
			base = cf->r_base;

//printf("----------   next: %d\n", base);
		}

		/* No handler! Give up and dump. */
		DBG5(printf(".------------------------------\n");)
		DBG5(printf("| Exception not caught!\n");)
		DBG5(printf("|------------------------------\n");)
		eel_vmdump(vm, "Terminating EEL virtual machine!");
		unwind(vm, base);
		return x;
	  }
	}

	/* Get back to the VM loop. */
	reload_context(vm, vms);
	DBG5(printf(".--- Reschedule ---------------\n");)
	DBG5(printf("|  heap = %p\n", vm->heap);)
	DBG5(printf("|     f = '%s' (%s)\n",
			eel_o_stringrep(vms->cf->f),
			o2EEL_function(vms->cf->f)->common.flags & EEL_FF_CFUNC
					? "C" : "EEL");)
	DBG5(printf("|  code = %p\n", vms->code);)
	DBG5(printf("|  base = %d\n", vm->base);)
	DBG5(printf("| sbase = %d\n", vm->sbase);)
	DBG5(printf("|  ctab = %d\n", (EEL_value *)vms->ctab - vm->heap);)
	DBG5(printf("|    pc = %d\n", vm->pc);)
	DBG5(printf("|    sp = %d\n", vm->sp);)
	DBG5(printf("'------------------------------\n");)
	if(!vms->code)
		return EEL_XEND;
	else
		return 0;
}


#define	XCHECK(fn)			\
	({				\
		EEL_xno xxx = (fn);	\
		if(xxx)			\
			THROW(xxx);	\
	})

#define	PC		(vm->pc)
#define	R		(vms.r)
#define	S		(vm->heap + vm->sp)
#define	SV		(vms.sv)
#define	CALLFRAME	(vms.cf)
#define	CODE		(vms.code)
#define	CLEANTABLE	(vms.ctab)
#ifdef EEL_NOCLEAN
#  define	ADDCLEAN(x)
#else
#  define	ADDCLEAN(x)							\
	({								\
		DBG6(printf("Added R[%d] to cleaning table;"		\
				" position %d.\n", x, CLEANTABLE[0]+1);)\
		CLEANTABLE[++CLEANTABLE[0]] = x;			\
	})
#endif
#define CHECK_STACK(n)					\
({							\
	int res = grow_heap(vm, vm->sp + n);		\
	if(res)						\
	{						\
		if(res > 0)				\
			reload_context(vm, &vms);	\
		else					\
			RETURN(EEL_XMEMORY);		\
	}						\
})

/*
 * Various stuff to do right before executing a VM instruction.
 * (Might include NEXT and stuff, so DO NOT use inside NEXT!)
 */
#define	PREINSTRUCTION							\
	do {								\
		DBG4(							\
			const char *tmp = eel_i_stringrep(		\
					VMP->state, vms.cf->f, PC);	\
			printf("%6.1d: %s\n", PC, tmp);			\
			eel_sfree(VMP->state, tmp);			\
		)							\
		EEL_VMCHECK(						\
			if(!vms.cf->f)					\
				DUMP(EEL_XINTERNAL, "Bad context! "	\
						"No function.");	\
			if((EEL_classes)vms.cf->f->type != EEL_CFUNCTION)	\
				DUMP(EEL_XINTERNAL, "Bad context! "	\
						"Object is not a "	\
						"function.");		\
			if(PC >= o2EEL_function(vms.cf->f)->e.codesize)	\
				DUMP(EEL_XINTERNAL, "Bad context! "	\
						"PC out of range.");	\
		)							\
		DBG4E(check_callframe(vm, CALLFRAME);)			\
		DBG6B(++VMP->instructions;)				\
	} while(0)

EEL_xno eel_run(EEL_vm *vm)
{
	EEL_vmstate vms;

#ifdef EEL_VM_THREADED

/*--------------------------------------------------------------------*/
/*--- Threaded dispatcher (by means of computed goto) ----------------*/
/*--------------------------------------------------------------------*/

	/* Opcode ==> label address LUT for the threaded dispatcher */
#if EEL_OILLEGAL_0 != 0
#	error 'ILLEGAL' VM instruction opcode is not 0!
#endif
	static const void *gtab[] =
	{
#define	EEL_I(x, y)	&&lab_O##x##_##y,
		EEL_INSTRUCTIONS
		EEL_IILLEGAL
#undef	EEL_I
	};

#define	NEXT								\
	({								\
		vmprofile_out(vm);					\
		goto *gtab[(EEL_opcodes)vms.code[PC]];			\
	})

#define	BEGIN								\
	NEXT;								\
	lab_OILLEGAL_0:							\
	{								\
		PREINSTRUCTION;						\
		vmprofile_in(vm, (EEL_opcodes)vms.code[PC]);		\
		{							\
			++PC;						\
			THROW(EEL_XILLEGAL);				\
			{

#define	EEL_I(x, y)							\
				NEXT;					\
			}						\
		}							\
	}								\
	lab_O##x##_##y:							\
	{								\
		PREINSTRUCTION;						\
		vmprofile_in(vm, (EEL_opcodes)vms.code[PC]);		\
		{							\
			EEL_OPR_##y(vms.code + PC)			\
			PC += EEL_OSIZE_##y;				\
			{						\
				/* opcode implementation goes here */

#define	END								\
				NEXT;					\
			}						\
		}							\
	} /* <last code block> */					\
	/*								\
	 * We should not get here normally, as all instruction code	\
	 * blocks above end by dispatching the next instruction.	\
	 */								\
	THROW(EEL_XILLEGAL);

#else /* EEL_VM_THREADED */

/*--------------------------------------------------------------------*/
/*--- switch() based dispatcher --------------------------------------*/
/*--------------------------------------------------------------------*/

#define	NEXT								\
	({								\
		vmprofile_out(vm);					\
		continue;						\
	})

#define	BEGIN								\
	while(1)							\
	{								\
		PREINSTRUCTION;						\
		vmprofile_in(vm, (EEL_opcodes)vms.code[PC]);		\
		switch((EEL_opcodes)vms.code[PC])			\
		{							\
		  case EEL_OILLEGAL_0:					\
		  {							\
			++PC;						\
			THROW(EEL_XILLEGAL);				\
			{

#define	EEL_I(x, y)							\
				NEXT;					\
			}						\
		  }							\
		  case EEL_O##x##_##y:					\
		  {							\
			EEL_OPR_##y(vms.code + PC)			\
			PC += EEL_OSIZE_##y;				\
			{						\
				/* opcode implementation goes here */

#define	END								\
				NEXT;					\
			}						\
		  } /* <last case> */					\
		} /* switch((EEL_opcodes)vms.code[PC]) */		\
		/*							\
		 * We should not get here normally, as all instruction	\
		 * cases above end by dispatching the next instruction.	\
		 */							\
		THROW(EEL_XILLEGAL);					\
	} /* while(1) */

#endif /* EEL_VM_THREADED */

/*--------------------------------------------------------------------*/
/*--- Common dispatcher macros, based on the above -------------------*/
/*--------------------------------------------------------------------*/

/*--- Straight return to C caller - THE ONLY WAY OUT OF THE DISPATCH LOOP! ---*/
#define	RETURN(x)		\
	({			\
		return (x);	\
	})

/*--- Reschedule and continue execution, or leave the VM -------------*/
#define	RESCHEDULE					\
	({						\
		EEL_xno xx = eel__scheduler(vm, &vms);	\
		if(xx)					\
			RETURN(xx);			\
		else					\
			NEXT;				\
	})

/*--- Throw and handle a "silent" (normal) exception  ----------------*/
#define	THROW(x)			\
	({				\
		eel__throw(vm, x);	\
		RESCHEDULE;		\
	})

/*--- Throw an exception, dump message, handle  ----------------------*/
#define	DUMP(x, msg)			\
	({				\
		eel__throw(vm, x);	\
		eel_vmdump(vm, msg);	\
		RESCHEDULE;		\
	})

/*--- Throw an exception, dump message, handre - two arguments -------*/
#define	DUMP2(x, msg, arg)			\
	({					\
		eel__throw(vm, x);		\
		eel_vmdump(vm, msg, arg);	\
		RESCHEDULE;			\
	})

/*--------------------------------------------------------------------*/
/*--- (End of dispatcher macro horror.) ------------------------------*/
/*--------------------------------------------------------------------*/

	if(!vm->base)
		RETURN(EEL_XEND);

	/* Not interested in anything thrown outside the VM... */
	eel_v_disown_nz(&VMP->exception);
	VMP->exception.type = EEL_TNIL;

	reload_context(vm, &vms);
	DBG5(printf(">>>>>>>>>>>>>>>> Entering VM >>>>>>>>>>>>>>>>\n");)

#ifdef EEL_VM_PROFILING
	VMP->vmp_time = getns();
#endif
	BEGIN
	  /* Special instructions */
	  EEL_INOP

	  /* Local flow control */
	  EEL_IJUMP
#ifdef EEL_VM_CHECKING
		if(-eel_i_size(EEL_OJUMP_sAx) == A)
			DUMP(EEL_XARGUMENTS, "Illegal jump! (Infinite loop)");
#endif
		PC += A;

	  EEL_IJUMPZ
#ifdef EEL_VM_CHECKING
		if(-eel_i_size(EEL_OJUMPZ_AsBx) == B)
			DUMP(EEL_XARGUMENTS, "Illegal jump! (Infinite loop)");
#endif
		if(!eel_test_nz(vm, &R[A]))
			PC += B;

	  EEL_IJUMPNZ
#ifdef EEL_VM_CHECKING
		if(-eel_i_size(EEL_OJUMPNZ_AsBx) == B)
			DUMP(EEL_XARGUMENTS, "Illegal jump! (Infinite loop)");
#endif
		if(eel_test_nz(vm, &R[A]))
			PC += B;

	  EEL_ISWITCH
		EEL_value offs;
		EEL_xno x;
		EEL_function *f = o2EEL_function(CALLFRAME->f);
#ifdef EEL_VM_CHECKING
		if(-eel_i_size(EEL_OSWITCH_ABxsCx) == C)
			DUMP(EEL_XARGUMENTS, "Illegal SWITCH default jump!"
					" (Infinite loop)");
#endif
		x = eel_o__metamethod(f->e.constants[B].objref.v,
				EEL_MM_GETINDEX, &R[A], &offs);
		/*
		 * NOTE:
		 *	If we get anything but an integer here, the compiler
		 *	did something really stupid!
		 */
		if(!x)
			PC = offs.integer.v;
		else
			PC += C;

	  EEL_IPRELOOP
		/*
		 * Cast all values to real first, so we
		 * can save some cycles inside the loop.
		 */
		if(R[A].type != EEL_TREAL)
		{
			EEL_real v;
#ifdef DEBUG
			v = 0.0f;
#endif
			XCHECK(eel_get_realval(vm, &R[A], &v));
			eel_v_disown_nz(&R[A]);	/* R[A] is a variable! */
			R[A].type = EEL_TREAL;
			R[A].real.v = v;
		}
		if(R[B].type != EEL_TREAL)
		{
			EEL_real v;
#ifdef DEBUG
			v = 0.0f;
#endif
			XCHECK(eel_get_realval(vm, &R[B], &v));
			R[B].type = EEL_TREAL;
			R[B].real.v = v;
		}
		if(R[C].type != EEL_TREAL)
		{
			EEL_real v;
#ifdef DEBUG
			v = 0.0f;
#endif
			XCHECK(eel_get_realval(vm, &R[C], &v));
			R[C].type = EEL_TREAL;
			R[C].real.v = v;
		}
		/*
		 * Now for the important part: Killing this
		 * "one iteration no matter what" nonsense!
		 */
		if(R[B].real.v < 0.0)
		{
			if(R[A].real.v < R[C].real.v)
				PC += D;	/* Skip the loop */
		}
		else
			if(R[A].real.v > R[C].real.v)
				PC += D;	/* Skip the loop */

	  EEL_ILOOP
		/*
		 * R[A] is a variable, so someone might
		 * "damage" it from inside the loop!
		 */
		if(R[A].type != EEL_TREAL)
		{
			EEL_real v;
#ifdef DEBUG
			v = 0.0f;
#endif
			XCHECK(eel_get_realval(vm, &R[A], &v));
			eel_v_disown_nz(&R[A]);
			R[A].type = EEL_TREAL;
			R[A].real.v = v;
		}
		R[A].real.v += R[B].real.v;
		if(R[B].real.v < 0.0f)
		{
			if(R[A].real.v < R[C].real.v)
				NEXT;	/* Stop! */
		}
		else
			if(R[A].real.v > R[C].real.v)
				NEXT;	/* Stop! */
		PC += D;		/* Loop! */

	  /* Argument stack operations */
	  EEL_IPUSH
		CHECK_STACK(1);
		eel_v_copy(S, &R[A]);
		++vm->sp;

	  EEL_IPUSH2
		CHECK_STACK(2);
		eel_v_copy(S, &R[A]);
		eel_v_copy(S + 1, &R[B]);
		vm->sp += 2;

	  EEL_IPUSH3
		CHECK_STACK(3);
		eel_v_copy(S, &R[A]);
		eel_v_copy(S + 1, &R[B]);
		eel_v_copy(S + 2, &R[C]);
		vm->sp += 3;

	  EEL_IPUSH4
		CHECK_STACK(4);
		eel_v_copy(S, &R[A]);
		eel_v_copy(S + 1, &R[B]);
		eel_v_copy(S + 2, &R[C]);
		eel_v_copy(S + 3, &R[D]);
		vm->sp += 4;

	  EEL_IPUSHI
		CHECK_STACK(1);
		S[0].type = EEL_TINTEGER;
		S[0].integer.v = A;
		++vm->sp;

	  EEL_IPHTRUE
		CHECK_STACK(1);
		S[0].type = EEL_TBOOLEAN;
		S[0].integer.v = 1;
		++vm->sp;

	  EEL_IPHFALSE
		CHECK_STACK(1);
		S[0].type = EEL_TBOOLEAN;
		S[0].integer.v = 0;
		++vm->sp;

	  EEL_IPUSHNIL
		CHECK_STACK(1);
		S[0].type = EEL_TNIL;
		++vm->sp;

	  EEL_IPUSHC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		CHECK_STACK(1);
		eel_v_copy(S, &f->e.constants[A]);
		++vm->sp;

	  EEL_IPUSHC2
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		CHECK_STACK(2);
		eel_v_copy(S, &f->e.constants[A]);
		eel_v_copy(S + 1, &f->e.constants[B]);
		vm->sp += 2;

	  EEL_IPUSHIC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		CHECK_STACK(2);
		S[0].type = EEL_TINTEGER;
		S[0].integer.v = B;
		eel_v_copy(S + 1, &f->e.constants[A]);
		vm->sp += 2;

	  EEL_IPUSHCI
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		CHECK_STACK(2);
		eel_v_copy(S, &f->e.constants[A]);
		S[1].type = EEL_TINTEGER;
		S[1].integer.v = B;
		vm->sp += 2;

	  EEL_IPHVAR
		CHECK_STACK(1);
		eel_v_copy(S, &SV[A]);
		++vm->sp;

	  EEL_IPHUVAL
		EEL_value *rf;
		CHECK_STACK(1);
		rf = vm->heap + get_uv_base(vm, B);
		eel_v_copy(S, rf + A);
		++vm->sp;

	  EEL_IPHARGS
		if(CALLFRAME->argc)
		{
			int i;
			EEL_value *args = vm->heap + CALLFRAME->argv;
			CHECK_STACK(CALLFRAME->argc);
			for(i = 0; i < CALLFRAME->argc; ++i)
				eel_v_copy(S + i, args + i);
			vm->sp += CALLFRAME->argc;
		}

	  EEL_IPUSHTUP
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		int argc = CALLFRAME->argc - f->common.reqargs;
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "PUSHTUP in function with no tuple args!");
#  if 0
		/*
		 * This is actually ok - but the compiler should only allow it
		 * when using the 'tuples' keyword, or when there tuple size is
		 * one, as anything else would be most confusing!
		 */
		if(f->common.tupargs > 1)
			DUMP(EEL_XVMCHECK, "PUSHTUP in function with more than one tuple arg!");
#  endif
#endif
		if(argc)
		{
			int i;
			EEL_value *args = vm->heap + CALLFRAME->argv +
					f->common.reqargs;
			CHECK_STACK(argc);
			for(i = 0; i < argc; ++i)
				eel_v_copy(S + i, args + i);
			vm->sp += argc;
		}

	  /* Function calls */
	  EEL_ICALL
		EEL_object *f;
		DBG4E(dump_callframe(vm, CALLFRAME, "CALL");)
		XCHECK(get_function(vm, &R[A], &f));
		XCHECK(check_args(vm, f));
		XCHECK(call_f(vm, f, -1, 0));
		reload_context(vm, &vms);
//		RESCHEDULE;

	  EEL_ICALLR
		EEL_object *f;
		DBG4E(dump_callframe(vm, CALLFRAME, "CALLR");)
		XCHECK(get_function(vm, &R[A], &f));
		XCHECK(check_args(vm, f));
		XCHECK(call_f(vm, f, vm->base + B, 0));
		reload_context(vm, &vms);
//		RESCHEDULE;

	  EEL_ICCALL
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		DBG4E(dump_callframe(vm, CALLFRAME, "CCALL");)
#ifdef EEL_VM_CHECKING
		if(!EEL_IS_OBJREF(f->e.constants[B].type))
			DUMP(EEL_XARGUMENTS, "CCALL: Constant is not an object "
					"reference!");
		if((EEL_classes)f->e.constants[B].objref.v->type != EEL_CFUNCTION)
			DUMP(EEL_XARGUMENTS, "CCALL: Object is not a function!");
#endif
		XCHECK(call_f(vm, f->e.constants[B].objref.v, -1, A));
		reload_context(vm, &vms);
//		RESCHEDULE;

	  EEL_ICCALLR
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		DBG4E(dump_callframe(vm, CALLFRAME, "CCALLR");)
#ifdef EEL_VM_CHECKING
		if(!EEL_IS_OBJREF(f->e.constants[C].type))
			DUMP(EEL_XARGUMENTS, "CCALL: Constant is not an object "
					"reference!");
		if((EEL_classes)f->e.constants[C].objref.v->type != EEL_CFUNCTION)
			DUMP(EEL_XARGUMENTS, "CCALL: Object is not a function!");
#endif
		XCHECK(call_f(vm, f->e.constants[C].objref.v, vm->base + B, A));
		reload_context(vm, &vms);
//		RESCHEDULE;

	  EEL_IRETURN
		clean(vm, CLEANTABLE, 0);
		limbo_clean(vm, CALLFRAME);
		vm->base = CALLFRAME->r_base;
		PC = CALLFRAME->r_pc;
#ifdef EEL_VM_CHECKING
		if(vm->sp != vm->sbase)
			DUMP(EEL_XINTERNAL, "RETURN: Garbage left in argument "
					"stack when leaving function!");
#endif
		vm->sbase = CALLFRAME->r_sbase;
		vm->sp = CALLFRAME->r_sp;
		stack_clear(vm);
		DBG4E(dump_callframe(vm, CALLFRAME, "RETURN (from)");)
		if(!vm->base)
			RETURN(EEL_XEND);	/* Root ==> no callframe! */
		CALLFRAME = b2callframe(vm, vm->base);
		DBG4E(dump_callframe(vm, CALLFRAME, "RETURN (to)");)
		if((o2EEL_function(CALLFRAME->f)->common.flags & EEL_FF_CFUNC))
			RETURN(EEL_XEND);
		reload_context(vm, &vms);
		DBG6(printf("<=== (Returned to function %p)\n", CALLFRAME->f);)
//		RESCHEDULE;

	  EEL_IRETURNR
		int ri = CALLFRAME->result;
		DBG6(printf("Copying result from R[%d] to heap[%d]\n", A, ri);)
		/* Give the result to the caller! */
		if(ri >=0)
			eel_v_copy(vm->heap + ri, &R[A]);
		clean(vm, CLEANTABLE, 0);
		limbo_clean(vm, CALLFRAME);
		vm->base = CALLFRAME->r_base;
		PC = CALLFRAME->r_pc;
#ifdef EEL_VM_CHECKING
		if(vm->sp != vm->sbase)
			DUMP(EEL_XINTERNAL, "RETURN: Garbage left in argument "
					"stack when leaving function!");
#endif
		vm->sbase = CALLFRAME->r_sbase;
		vm->sp = CALLFRAME->r_sp;
		stack_clear(vm);
		DBG4E(dump_callframe(vm, CALLFRAME, "RETURNR (from)");)
		if(!vm->base)
			RETURN(EEL_XEND);	/* Root ==> no callframe! */
		CALLFRAME = b2callframe(vm, vm->base);
		DBG4E(dump_callframe(vm, CALLFRAME, "RETURNR (to)");)
		if((o2EEL_function(CALLFRAME->f)->common.flags & EEL_FF_CFUNC))
			RETURN(EEL_XEND);
		reload_context(vm, &vms);
		DBG6(printf("<=== (Returned to function %p)\n", CALLFRAME->f);)
		if(ri >=0)
			eel_v_receive(vm->heap + ri);
//		RESCHEDULE;

	  /* Memory management */
	  EEL_ICLEAN
		clean(vm, CLEANTABLE, A);

	  /* Optional/tuple argument checking */
	  EEL_IARGC
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = CALLFRAME->argc;

	  EEL_ITUPC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "TUPC in function with no tuple args!");
#endif
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = CALLFRAME->argc;
		R[A].integer.v -= f->common.reqargs;
		R[A].integer.v /= f->common.tupargs;

	  EEL_ISPEC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		R[B].type = EEL_TBOOLEAN;
		R[B].integer.v = A + f->common.reqargs < CALLFRAME->argc;

	  EEL_ITSPEC
		/*
		 * NOTE:
		 *	As it's illegal to pass incomplete tuples, there is no
		 *	need to check which actual argument we're talking about!
		 *	The compiler can just discard that information and pass
		 *	the tuple index to this instruction.
		 */
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		int tupc = CALLFRAME->argc;
		int ind = eel_get_indexval(vm, &R[A]);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "TSPEC in function with no tuple args!");
#endif
		if(ind < 0)
			THROW(EEL_XLOWINDEX);
		tupc -= f->common.reqargs;
		tupc /= f->common.tupargs;
		R[B].type = EEL_TBOOLEAN;
		R[B].integer.v = ind < tupc;

	  /* Immediate values, constants etc */
	  EEL_ILDI
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = B;

	  EEL_ILDTRUE
		R[A].type = EEL_TBOOLEAN;
		R[A].integer.v = 1;

	  EEL_ILDFALSE
		R[A].type = EEL_TBOOLEAN;
		R[A].integer.v = 0;

	  EEL_ILDNIL
		R[A].type = EEL_TNIL;

	  EEL_ILDC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		eel_v_qcopy(&R[A], &f->e.constants[B]);

	  /* Register access */
	  EEL_IMOVE
		eel_v_qcopy(&R[A], &R[B]);

	  /* Register variables */
	  EEL_IINIT
#ifdef EEL_NOCLEAN
		eel_v_qcopy(&R[A], &R[B]);
#else
		eel_v_copy(&R[A], &R[B]);
		ADDCLEAN(A);
#endif

	  EEL_IINITI
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = B;
		ADDCLEAN(A);

	  EEL_IINITNIL
		R[A].type = EEL_TNIL;
		ADDCLEAN(A);

	  EEL_IINITC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		eel_v_copy(&R[A], &f->e.constants[B]);
		ADDCLEAN(A);

	  EEL_IASSIGN
#ifdef EEL_NOCLEAN
		eel_v_qcopy(&R[A], &R[B]);
#else
		eel_v_disown_nz(&R[A]);
		eel_v_copy(&R[A], &R[B]);
#endif

	  EEL_IASSIGNI
#ifndef EEL_NOCLEAN
		eel_v_disown_nz(&R[A]);
#endif
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = B;

	  EEL_IASNNIL
#ifndef EEL_NOCLEAN
		eel_v_disown_nz(&R[A]);
#endif
		R[A].type = EEL_TNIL;

	  EEL_IASSIGNC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		eel_v_disown_nz(&R[A]);
		eel_v_copy(&R[A], &f->e.constants[B]);

	  /* Upvalues */
	  EEL_IGETUVAL
		EEL_value *rf = vm->heap + get_uv_base(vm, C);
		eel_v_qcopy(&R[A], rf + B);
		eel_v_grab(&R[A]);

	  EEL_ISETUVAL
		EEL_value *rf = vm->heap + get_uv_base(vm, C);
		eel_v_disown_nz(rf + B);
		eel_v_copy(rf + B, &R[A]);

	  /* Static variables */
	  EEL_IGETVAR
		eel_v_qcopy(&R[A], &SV[B]);
		eel_v_grab(&R[A]);

	  EEL_ISETVAR
		eel_v_disown_nz(&SV[B]);
#ifdef EEL_NOCLEAN
		eel_v_qcopy(&R[B], &R[A]);
#else
		eel_v_copy(&SV[B], &R[A]);
#endif

	  /* Indexed access (array/table)  */
	  EEL_IINDGETI
		switch(R[C].type)
		{
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
		  {
			EEL_value i;
			i.type = EEL_TINTEGER;
			i.integer.v = B;
			XCHECK(eel_o__metamethod(R[C].objref.v,
					EEL_MM_GETINDEX, &i, &R[A]));
			eel_v_receive(&R[A]);
			NEXT;
		  }
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  EEL_IINDSETI
		switch(R[C].type)
		{
		  case EEL_TWEAKREF:
		  case EEL_TOBJREF:
		  {
			EEL_value i;
			i.type = EEL_TINTEGER;
			i.integer.v = B;
			XCHECK(eel_o__metamethod(R[C].objref.v,
					EEL_MM_SETINDEX, &i, &R[A]));
			NEXT;
		  }
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  EEL_IINDGET
		switch(R[C].type)
		{
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_o__metamethod(R[C].objref.v,
					EEL_MM_GETINDEX, &R[B], &R[A]));
			eel_v_receive(&R[A]);
			NEXT;
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  EEL_IINDSET
		switch(R[C].type)
		{
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_o__metamethod(R[C].objref.v,
					EEL_MM_SETINDEX, &R[B], &R[A]));
			NEXT;
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  EEL_IINDGETC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		switch(R[B].type)
		{
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_o__metamethod(R[B].objref.v,
					EEL_MM_GETINDEX, &f->e.constants[C],
					&R[A]));
			eel_v_receive(&R[A]);
			NEXT;
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  EEL_IINDSETC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		switch(R[B].type)
		{
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_o__metamethod(R[B].objref.v,
					EEL_MM_SETINDEX, &f->e.constants[C],
					&R[A]));
			NEXT;
		  default:
			THROW(EEL_XCANTINDEX);
		}

	  /* Argument access */
	  EEL_IGETARGI
		EEL_value *arg;
		if(B >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + B;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_IPHARGI
		EEL_value *arg;
		if(A >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + A;
		CHECK_STACK(1);
		eel_v_copy(S, arg);
		++vm->sp;

	  EEL_IPHARGI2
		EEL_value *args = vm->heap + CALLFRAME->argv;
		if((A >= CALLFRAME->argc) || (B >= CALLFRAME->argc))
			THROW(EEL_XHIGHINDEX);
		CHECK_STACK(2);
		eel_v_copy(S, args + A);
		eel_v_copy(S + 1, args + B);
		vm->sp += 2;

	  EEL_ISETARGI
		EEL_value *arg;
		if(B >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + B;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  EEL_IGETARG
		EEL_value *arg;
		int ind = eel_get_indexval(vm, &R[B]);
		if(ind < 0)
			THROW(EEL_XWRONGTYPE);
		if(ind >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + ind;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_ISETARG
		EEL_value *arg;
		int ind = eel_get_indexval(vm, &R[B]);
		if(ind < 0)
			THROW(EEL_XWRONGTYPE);
		if(ind >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + ind;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  /* Tuple argument access */
	  EEL_IGETTARGI
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		int tup = eel_get_indexval(vm, &R[C]);
		if(tup < 0)
			THROW(EEL_XWRONGTYPE);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETTARGI in function with no tuple args!");
		if(B >= f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETTARGI with out-of-range tuple member!");
#endif
		B += f->common.reqargs + tup * f->common.tupargs;
		if(B >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + B;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_ISETTARGI
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		int tup = eel_get_indexval(vm, &R[C]);
		if(tup < 0)
			THROW(EEL_XWRONGTYPE);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "SETTARGI in function with no tuple args!");
		if(B >= f->common.tupargs)
			DUMP(EEL_XVMCHECK, "SETTARGI with out-of-range tuple member!");
#endif
		B += f->common.reqargs + tup * f->common.tupargs;
		if(B >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + B;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  EEL_IGETTARG
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		int ind = eel_get_indexval(vm, &R[B]);
		int tup = eel_get_indexval(vm, &R[C]);
		if((ind < 0) || (tup < 0))
			THROW(EEL_XWRONGTYPE);
		if(ind >= f->common.tupargs)
			THROW(EEL_XHIGHINDEX);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETTARG in function with no tuple args!");
#endif
		ind += f->common.reqargs + tup * f->common.tupargs;
		if(ind >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + ind;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_ISETTARG
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		int ind = eel_get_indexval(vm, &R[B]);
		int tup = eel_get_indexval(vm, &R[C]);
		if((ind < 0) || (tup < 0))
			THROW(EEL_XWRONGTYPE);
		if(ind >= f->common.tupargs)
			THROW(EEL_XHIGHINDEX);
#ifdef EEL_VM_CHECKING
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "SETTARG in function with no tuple args!");
#endif
		ind += f->common.reqargs + tup * f->common.tupargs;
		if(ind >= CALLFRAME->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + CALLFRAME->argv + ind;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  /* Upvalue argument access */
	  EEL_IGETUVARGI
		EEL_value *arg;
		EEL_callframe *cf = b2callframe(vm, get_uv_base(vm, C));
#ifdef EEL_VM_CHECKING
		if(!cf)
			THROW(EEL_XUPVALUE);
#endif
		if(B >= cf->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + cf->argv + B;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_ISETUVARGI
		EEL_value *arg;
		EEL_callframe *cf = b2callframe(vm, get_uv_base(vm, C));
#ifdef EEL_VM_CHECKING
		if(!cf)
			THROW(EEL_XUPVALUE);
#endif
		if(B >= cf->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + cf->argv + B;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  /* Upvalue tuple argument access */
	  EEL_IGETUVTARGI
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		EEL_callframe *cf = b2callframe(vm, get_uv_base(vm, D));
		int tup = eel_get_indexval(vm, &R[C]);
#ifdef EEL_VM_CHECKING
		if(!cf)
			THROW(EEL_XUPVALUE);
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETUVTARGI in function with no tuple args!");
		if(B >= f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETUVTARGI with out-of-range tuple member!");
#endif
		if(tup < 0)
			THROW(EEL_XWRONGTYPE);
		B += f->common.reqargs + tup * f->common.tupargs;
		if(B >= cf->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + cf->argv + B;
		eel_v_qcopy(&R[A], arg);
		eel_v_grab(&R[A]);

	  EEL_ISETUVTARGI
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		EEL_value *arg;
		EEL_callframe *cf = b2callframe(vm, get_uv_base(vm, D));
		int tup = eel_get_indexval(vm, &R[C]);
#ifdef EEL_VM_CHECKING
		if(!cf)
			THROW(EEL_XUPVALUE);
		if(!f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETUVTARGI in function with no tuple args!");
		if(B >= f->common.tupargs)
			DUMP(EEL_XVMCHECK, "GETUVTARGI with out-of-range tuple member!");
#endif
		if(tup < 0)
			THROW(EEL_XWRONGTYPE);
		B += f->common.reqargs + tup * f->common.tupargs;
		if(B >= cf->argc)
			THROW(EEL_XHIGHINDEX);
		arg = vm->heap + cf->argv + B;
		eel_v_disown_nz(arg);
		eel_v_copy(arg, &R[A]);

	  /* Operators */
	  EEL_IBOP
		XCHECK(eel_operate(&R[B], C, &R[D], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IPHBOP
		CHECK_STACK(1);
		XCHECK(eel_operate(&R[A], B, &R[C], S));
		++vm->sp;

	  EEL_IIPBOP
		XCHECK(eel_ipoperate(&R[B], C, &R[D], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IBOPS
		XCHECK(eel_operate(&R[B], C, &SV[D], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IIPBOPS
		XCHECK(eel_ipoperate(&R[B], C, &SV[D], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IBOPI
		EEL_value iv;
		iv.type = EEL_TINTEGER;
		iv.integer.v = D;
		XCHECK(eel_operate(&R[B], C, &iv, &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IPHBOPI
		EEL_value iv;
		iv.type = EEL_TINTEGER;
		iv.integer.v = C;
		CHECK_STACK(1);
		XCHECK(eel_operate(&R[A], B, &iv, S));
		++vm->sp;

	  EEL_IIPBOPI
		EEL_value iv;
		iv.type = EEL_TINTEGER;
		iv.integer.v = D;
		XCHECK(eel_ipoperate(&R[B], C, &iv, &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IBOPC
		EEL_function *f = o2EEL_function(CALLFRAME->f);
		XCHECK(eel_operate(&R[B], C, &f->e.constants[D], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_INEG
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TREAL:
			R[A].type = EEL_TREAL;
			R[A].real.v = -right->real.v;
			NEXT;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = -right->integer.v;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			THROW(EEL_XNOTIMPLEMENTED);
		  case EEL_TNIL:
		  case EEL_TTYPEID:
			THROW(EEL_XWRONGTYPE);
		}
		THROW(EEL_XWRONGTYPE);

	  EEL_IBNOT
		EEL_value *right = &R[B];
		if(EEL_TINTEGER != right->type)
			THROW(EEL_XWRONGTYPE);
		R[A].type = EEL_TINTEGER;
		R[A].integer.v = ~right->integer.v;

	  EEL_INOT
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TNIL:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = 1;
			NEXT;
		  case EEL_TREAL:
			R[A].type = EEL_TREAL;
			R[A].real.v = 0 == right->real.v;
			NEXT;
		  case EEL_TINTEGER:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = !right->integer.v;
			NEXT;
		  case EEL_TBOOLEAN:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = !right->integer.v;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
		  case EEL_TTYPEID:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = 0;
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ICASTR
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TNIL:
			R[A].type = EEL_TREAL;
			R[A].real.v = 0.0;
			NEXT;
		  case EEL_TREAL:
			R[A].type = EEL_TREAL;
			R[A].real.v = right->real.v;
			NEXT;
		  case EEL_TINTEGER:
		  case EEL_TTYPEID:
			R[A].type = EEL_TREAL;
			R[A].real.v = right->integer.v;
			NEXT;
		  case EEL_TBOOLEAN:
			R[A].type = EEL_TREAL;
			R[A].real.v = right->integer.v ? 1.0 : 0.0;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_cast(vm, right, &R[A], EEL_TREAL));
			eel_v_receive(&R[A]);
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ICASTI
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TNIL:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = 0;
			NEXT;
		  case EEL_TREAL:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = floor(right->real.v);
			NEXT;
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = right->integer.v;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_cast(vm, right, &R[A], EEL_TINTEGER));
			eel_v_receive(&R[A]);
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ICASTB
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TNIL:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = 0;
			NEXT;
		  case EEL_TREAL:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = 0.0 != right->real.v;
			NEXT;
		  case EEL_TINTEGER:
		  case EEL_TTYPEID:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = 0 != right->integer.v;
			NEXT;
		  case EEL_TBOOLEAN:
			R[A].type = EEL_TBOOLEAN;
			R[A].integer.v = right->integer.v;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_cast(vm, right, &R[A], EEL_TBOOLEAN));
			eel_v_receive(&R[A]);
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ICAST
		EEL_value *right = &R[B];
		switch(right->type)
		{
		  case EEL_TNIL:
		  case EEL_TREAL:
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			if(EEL_TTYPEID != R[C].type)
				THROW(EEL_XWRONGTYPE);
			XCHECK(eel_cast(vm, right, &R[A], R[C].integer.v));
			eel_v_receive(&R[A]);
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ITYPEOF
		switch(R[B].type)
		{
		  case EEL_TNIL:
			R[A].type = EEL_TNIL;
			NEXT;
		  case EEL_TREAL:
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			R[A].integer.v = R[B].type;
			R[A].type = EEL_TTYPEID;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			R[A].integer.v = R[B].objref.v->type;
			R[A].type = EEL_TTYPEID;
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_ISIZEOF
		switch(R[B].type)
		{
		  case EEL_TNIL:
		  case EEL_TREAL:
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			R[A].type = EEL_TINTEGER;
			R[A].integer.v = 1;
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_o__metamethod(R[B].objref.v,
					EEL_MM_LENGTH, NULL, &R[A]));
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  EEL_IWEAKREF
		if(R[B].type == EEL_TNIL)
			R[A].type = EEL_TNIL;
		else
		{
			if(R[B].type != EEL_TOBJREF)
				THROW(EEL_XNEEDOBJECT);
			eel_o2wr(&R[A], R[B].objref.v);
		}

	  EEL_IADD
		XCHECK(eel_op_add(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_ISUB
		XCHECK(eel_op_sub(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IMUL
		XCHECK(eel_op_mul(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IDIV
		XCHECK(eel_op_div(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IMOD
		XCHECK(eel_op_mod(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IPOWER
		XCHECK(eel_op_power(&R[B], &R[C], &R[A]));
		eel_v_receive(&R[A]);

	  EEL_IPHADD
		CHECK_STACK(1);
		XCHECK(eel_op_add(&R[A], &R[B], S));
		++vm->sp;

	  EEL_IPHSUB
		CHECK_STACK(1);
		XCHECK(eel_op_sub(&R[A], &R[B], S));
		++vm->sp;

	  EEL_IPHMUL
		CHECK_STACK(1);
		XCHECK(eel_op_mul(&R[A], &R[B], S));
		++vm->sp;

	  EEL_IPHDIV
		CHECK_STACK(1);
		XCHECK(eel_op_div(&R[A], &R[B], S));
		++vm->sp;

	  EEL_IPHMOD
		CHECK_STACK(1);
		XCHECK(eel_op_mod(&R[A], &R[B], S));
		++vm->sp;

	  EEL_IPHPOWER
		CHECK_STACK(1);
		XCHECK(eel_op_power(&R[A], &R[B], S));
		++vm->sp;

	  /* Constructors */
	  EEL_INEW
		XCHECK(eel_o__construct(vm, B,
				vm->heap + vm->sbase, vm->sp - vm->sbase,
				&R[A]));
		eel_v_receive(&R[A]);
		stack_clear(vm);

	  EEL_ICLONE
		switch(R[B].type)
		{
		  case EEL_TNIL:
		  case EEL_TREAL:
		  case EEL_TINTEGER:
		  case EEL_TBOOLEAN:
		  case EEL_TTYPEID:
			eel_v_qcopy(&R[A], &R[B]);
			NEXT;
		  case EEL_TOBJREF:
		  case EEL_TWEAKREF:
			XCHECK(eel_cast(vm, &R[B], &R[A], EEL_TYPE(&R[B])));
			eel_v_receive(&R[A]);
			NEXT;
		}
#ifdef EEL_VM_CHECKING
		THROW(EEL_XWRONGTYPE);
#endif

	  /* Exception handling */
	  EEL_ITRY
		EEL_function *f = o2EEL_function(CALLFRAME->f);
#ifdef EEL_VM_CHECKING
		if(!EEL_IS_OBJREF(f->e.constants[B].type))
			DUMP(EEL_XINTERNAL, "TRY: Try block is not an object!");
		if((EEL_classes)f->e.constants[B].objref.v->type != EEL_CFUNCTION)
			DUMP(EEL_XINTERNAL, "TRY: Try block is not a function!");
		if(!EEL_IS_OBJREF(f->e.constants[A].type))
			DUMP(EEL_XINTERNAL, "TRY: Catcher is not an object!");
		if((EEL_classes)f->e.constants[A].objref.v->type != EEL_CFUNCTION)
			DUMP(EEL_XINTERNAL, "TRY: Catcher is not a function!");
#endif
		DBGK4(printf("TRY passing on result index heap[%d].\n",
				CALLFRAME->result);)
		XCHECK(call_eel(vm, f->e.constants[B].objref.v,
				CALLFRAME->result, 0));
		reload_context(vm, &vms);
		CALLFRAME->catcher = f->e.constants[A].objref.v;
		CALLFRAME->flags |= EEL_CFF_TRYBLOCK;

	  EEL_IUNTRY
		EEL_function *f = o2EEL_function(CALLFRAME->f);
#ifdef EEL_VM_CHECKING
		if(!EEL_IS_OBJREF(f->e.constants[A].type))
			DUMP(EEL_XINTERNAL, "UNTRY: Try block is not an object!");
		if((EEL_classes)f->e.constants[A].objref.v->type != EEL_CFUNCTION)
			DUMP(EEL_XINTERNAL, "UNTRY: Try block is not a function!");
#endif
		DBGK4(printf("UNTRY passing on result index heap[%d].\n",
				CALLFRAME->result);)
		XCHECK(call_eel(vm, f->e.constants[A].objref.v,
				CALLFRAME->result, 0));
		reload_context(vm, &vms);
		CALLFRAME->flags |= EEL_CFF_TRYBLOCK | EEL_CFF_UNTRY;

	  EEL_ITHROW
	        eel_v_disown_nz(&VMP->exception);  /* Free any old object */
		eel_v_copy(&VMP->exception, &R[A]);
		DBG5(printf(">>>>>>>>>>> THROW <<<<<<<<<<<\n");)
		RESCHEDULE;

	  EEL_IRETRY
#ifdef EEL_VM_CHECKING
		if(!(CALLFRAME->flags & EEL_CFF_CATCHER))
			DUMP(EEL_XINTERNAL, "IRETRY used outside catcher!");
#endif
	  	clean(vm, CLEANTABLE, 0);
		limbo_clean(vm, CALLFRAME);
		vm->base = CALLFRAME->r_base;
#ifdef EEL_VM_CHECKING
		if(vm->sp != vm->sbase)
			DUMP(EEL_XINTERNAL, "RETRY: Garbage in argument stack!");
#endif
		vm->sbase = CALLFRAME->r_sbase;
		vm->sp = CALLFRAME->r_sp;
		/* Back up and rerun the TRY! */
		vm->pc = CALLFRAME->r_pc - EEL_OSIZE_AxBx;
		CALLFRAME = (EEL_callframe *)(vm->heap + vm->base - EEL_CFREGS);
#ifdef EEL_VM_CHECKING
		if(!CALLFRAME->f)
			THROW(EEL_XEND);
#endif
		reload_context(vm, &vms);

	  EEL_IRETX
#ifdef EEL_VM_CHECKING
		if(vm->sp != vm->sbase)
			DUMP(EEL_XINTERNAL, "RETX: Garbage left in argument "
					"stack when leaving block!");
/*FIXME: Should check the parent function prototype instead! */
		if(CALLFRAME->result >= 0)
			DUMP(EEL_XINTERNAL, "RETX (return with no result)"
					" in function!");
#endif
		THROW(EEL_XRETURN);

	  EEL_IRETXR
#ifdef EEL_VM_CHECKING
		if(vm->sp != vm->sbase)
			DUMP(EEL_XINTERNAL, "RETXR: Garbage left in argument "
					"stack when leaving block!");
#if 0
/*FIXME: Should check the parent function prototype instead! */
		if(CALLFRAME->result < 0)
			DUMP(EEL_XINTERNAL, "RETXR (return with result)"
					" in procedure!");
#endif
#endif
		if(CALLFRAME->result >= 0)
		{
			DBG6(printf("RETXR copying result from R[%d] to heap[%d]\n",
					A, CALLFRAME->result);)
			eel_v_copy(vm->heap + CALLFRAME->result, &R[A]);
		}
		THROW(EEL_XRETURN);
	END
	/* No exit! */
}
#undef	EEL_I


EEL_object *eel_current_function(EEL_vm *vm)
{
	return eel__current_function(vm);
}


EEL_object *eel_calling_function(EEL_vm *vm)
{
	int cbase;
	EEL_callframe *cf = b2callframe(vm, vm->base);
	if(!cf->f)
		return NULL;
	cbase = cf->r_base;
	cf = b2callframe(vm, cbase);
	return cf->f;
}


/*----------------------------------------------------------
	VM Memory Management
----------------------------------------------------------*/

static void *default_malloc(EEL_vm *vm, int size)
{
	return malloc(size);
}

static void *default_realloc(EEL_vm *vm, void *block, int size)
{
	return realloc(block, size);
}

static void default_free(EEL_vm *vm, void *block)
{
	return free(block);
}


/*----------------------------------------------------------
	EEL Virtual Machine API
----------------------------------------------------------*/

static inline int vm_init(EEL_state *es, EEL_vm *vm, int heap)
{
	if(es->vm)
	{
		/* Get memory manager from state VM */
		vm->malloc = es->vm->malloc;
		vm->realloc = es->vm->realloc;
		vm->free = es->vm->free;
	}
	else
	{
		/* Install default memory manager */
		vm->malloc = default_malloc;
		vm->realloc = default_realloc;
		vm->free = default_free;
	}
	VMP->state = es;

	/*
	 * Prepare heap and initialize a basic C function callframe.
	 *	What we want to achieve here is a minimal VM state with little
	 *	more than a result register and an argument stack. The actual
	 *	callframe that the top level function will run in will be set up
	 *	by eel_call(), so no code - EEL or C - will actually run in this
	 *	context.
	 */
	if(set_heap(vm, heap) < 0)
	{
		eel_vm_close(vm);
		return -1;
	}
	vm->sbase = vm->sp = 1;		/* Leave one register for the result! */
	vm->heap[vm->base].type = EEL_TNIL;	/* "Expect no result" mark */
	return 0;
}


EEL_vm *eel_vm_open(EEL_state *es, int heap)
{
#ifdef EEL_VM_PROFILING
	int i;
#endif
	EEL_vm *vm = (EEL_vm *)calloc(1, sizeof(EEL_vm) + sizeof(EEL_vm_private));
	if(!vm)
		return NULL;

	if(vm_init(es, vm, heap) < 0)
		return NULL;

#ifdef EEL_VM_PROFILING
	for(i = 0; i < EEL_VMP_POINTS; ++i)
	{
		VMP->vmprof[i].count = 0;
		VMP->vmprof[i].time = 0;
	}

	/* Calibrate for profiling overhead */
	VMP->vmp_overhead = 10000000;
	for(i = 0; i < 1000; ++i)
	{
		VMP->vmprof[EEL_ONOP_0].count = VMP->vmprof[EEL_ONOP_0].time = 0;
		vmprofile_in(vm, EEL_ONOP_0);
		vmprofile_out(vm);
		if(!VMP->vmprof[EEL_ONOP_0].count)
			continue;
		if(VMP->vmprof[EEL_ONOP_0].time < VMP->vmp_overhead)
			VMP->vmp_overhead = VMP->vmprof[EEL_ONOP_0].time;
	}
	VMP->vmprof[EEL_ONOP_0].count = VMP->vmprof[EEL_ONOP_0].time = 0;
	printf("Profiling timer overhead: %d ns\n", VMP->vmp_overhead);
#endif
	return vm;
}


void eel_vm_cleanup(EEL_vm *vm)
{
	eel_v_disown_nz(&VMP->exception);
	eel_ps_close(vm);
}


void eel_vm_close(EEL_vm *vm)
{
	/*
	 * WARNING! There isn't much you can do here, really.
	 * The class system and other stuff is gone when this
	 * function is called.
	 *
	 * Put VM cleanup stuff in eel_vm_cleanup() instead!
	 */
#ifdef EEL_VM_PROFILING
	int i;
	int used = 0;
	long long ttotal = 0;
	long long itotal = 0;
	EEL_vmpentry *p = VMP->vmprof;
	for(i = 0; i < EEL_VMP_POINTS; ++i)
	{
		ttotal += p[i].time;
		itotal += p[i].count;
	}
	printf(".----------------------------------"
			"----------------- -- -- - - -  -  -\n");
	printf("| EEL VM profiling results, sorted by total time\n");
	printf("|--- -- - - -  -  -\n");
	printf("|\topcode\tcount\ttime\ttime\tavg.t\n");
	printf("|\t\t\t(s)\t(%%)\t(s)\n");
	printf("|\t- - - - - - - - - - - - - - - - - - - -\n");
	for(i = 0; i < EEL_VMP_POINTS; ++i)
	{
		int j;
		long long tavg;

		/* Find maximum total time */
		int jmax = 0;
		long long tmax = -1;
		for(j = 0; j < EEL_VMP_POINTS; ++j)
			if(p[j].time > tmax)
			{
				jmax = j;
				tmax = p[j].time;
			}

		if(!tmax)
			break;
#if 0
		{
			/* If all remaining times are 0, find maximum count */
			long long cmax = 0;
			for(j = 0; j < EEL_VMP_POINTS; ++j)
				if(p[j].time >= 0)
					if(p[j].count > cmax)
					{
						jmax = j;
						cmax = p[j].count;
					}
		}
#endif
		/* Process the "winner" */
		tavg = p[jmax].count ? p[jmax].time / p[jmax].count : 0;
		tavg -= VMP->vmp_overhead;
		printf("|\t%s\t%lld%s\t%lld%s\t%.1f\t%lld%s\n",
				eel_i_name(jmax),
				p[jmax].count > 99999 ?
					p[jmax].count / 1000 :
					p[jmax].count,
				p[jmax].count > 99999 ? "k" : "",
				p[jmax].time > 99999 ?
					p[jmax].time / 1000 :
					p[jmax].time,
				p[jmax].time > 99999 ? "m" : "µ",
				ttotal ?
					(double)p[jmax].time * 100.0f / ttotal :
					0,
				tavg > 99999 ? tavg / 1000 : tavg,
				tavg > 99999 ? "µ" : "n");
		++used;
		
		/* Remove winner entry */
		p[jmax].time = -1;
	}
	printf("|--- -- - - -  -  -\n");
	printf("|      VM instructions executed: %lld\n", itotal);
	printf("| Time spent in VM instructions: %.2f s\n", ttotal * 0.000001f);
	printf("|--- -- - - -  -  -\n");
	printf("|       Opcodes available in VM: %d\n", EEL_O_LAST + 1);
	printf("|      Opcodes used in this run: %d\n", used);
	printf("'----------------------------------"
			"----------------- -- -- - - -  -  -\n");
#endif
	free(vm->heap);
	free(vm);
}


static EEL_object *find_function(EEL_vm *vm, EEL_object *module, const char *name)
{
	EEL_xno x;
	EEL_value n, f;
	if((EEL_classes)module->type != EEL_CMODULE)
		return NULL;

	n.type = EEL_TOBJREF;
	n.objref.v = eel_ps_new(vm, name);
	if(!n.objref.v)
		return NULL;

	x = eel_o__metamethod(module, EEL_MM_GETINDEX, &n, &f);
	if(x)
	{
		eel_v_disown_nz(&n);
		return NULL;
	}
/*
FIXME: If modules ever use weakrefs for functions, we need to handle that here!
*/
	if((EEL_classes)f.objref.v->type != EEL_CFUNCTION)
	{
		eel_v_disown_nz(&f);
		eel_v_disown_nz(&n);
		return NULL;
	}
	eel_v_disown_nz(&n);
	return f.objref.v;
}


static inline void reset_args(EEL_vm *vm)
{
	stack_clear(vm);
	eel_v_disown_nz(vm->heap + vm->base);
	vm->heap[vm->base].type = EEL_TNIL;
}


EEL_xno eel_vargf(EEL_vm *vm, const char *fmt, va_list args)
{
	int i, count = strlen(fmt);
	if(grow_heap(vm, vm->sp + count) < 0)
		return EEL_XMEMORY;
	for(i = 0; i < count; ++i)
		switch(fmt[i])
		{
		  case 0:
			return 0;
		  case '*':
			reset_args(vm);
			break;
		  case 'R':
		  {
			int *resv = va_arg(args, int *);
			/*
			 * We put a 'true' here to tell eel_call() that we expect
			 * a result! (Anything non-nil will do.)
			 */
			vm->heap[vm->base].type = EEL_TBOOLEAN;;
			vm->heap[vm->base].integer.v = 1;
			*resv = vm->base;
			break;
		  }
		  case 'n':
			S->type = EEL_TNIL;
			++vm->sp;
			break;
		  case 'i':
			S->type = EEL_TINTEGER;
			S->integer.v = va_arg(args, int);
			++vm->sp;
			break;
		  case 'r':
			S->type = EEL_TREAL;
			S->real.v = va_arg(args, EEL_real);
			++vm->sp;
			break;
		  case 'b':
			S->type = EEL_TBOOLEAN;
			S->integer.v = va_arg(args, int) != 0;
			++vm->sp;
			break;
		  case 's':
			S->type = EEL_TOBJREF;
			S->objref.v = eel_ps_new(vm, va_arg(args, const char *));
			if(!S->objref.v)
				return EEL_XCONSTRUCTOR;
			++vm->sp;
			break;
		  case 'o':
			S->type = EEL_TOBJREF;
			S->objref.v = va_arg(args, EEL_object *);
			eel_own(S->objref.v);
			++vm->sp;
			break;
		  case 'v':
			eel_v_copy(S, va_arg(args, EEL_value *));
			++vm->sp;
			break;
		}
	return 0;
}


EEL_xno eel_argf(EEL_vm *vm, const char *fmt, ...)
{
	EEL_xno x;
	va_list args;
	eel_clear_errors(VMP->state);
	va_start(args, fmt);
	x = eel_vargf(vm, fmt, args);
	va_end(args);
	return x;
}


static void call_msg(EEL_object *fo, EEL_emtype t, const char *fmt, ...)
{
	va_list args;
	EEL_vm *vm = fo->vm;
	EEL_function *f = o2EEL_function(fo);
	const char *fn = eel_o2s(f->common.name);
	eel_msg(VMP->state, t, "eel_call*() calling function '%s':\n", fn);
	va_start(args, fmt);
	eel_vmsg(VMP->state, -1, fmt, args);
	va_end(args);
}

static inline EEL_xno call_do_run(EEL_vm *vm)
{
	while(1)
	{
		EEL_xno x = eel_run(vm);
		switch(x)
		{
		  case EEL_XNONE:
		  case EEL_XYIELD:
			break;
		  case EEL_XEND:
			return 0;
		  default:
		  case EEL_XCOUNTER:
			return x;
		}
	}
}

EEL_xno eel_call(EEL_vm *vm, EEL_object *fo)
{
	EEL_xno x;
	EEL_function *f;
	int result;
/*FIXME:*/
	int save_resv = vm->resv;
	int save_argv = vm->argv;
	int save_argc = vm->argc;
/*FIXME:*/
	eel_clear_errors(VMP->state);
	if((EEL_classes)fo->type != EEL_CFUNCTION)
	{
		call_msg(fo, EEL_EM_VMERROR, "  Object is not callable!");
		return EEL_XNEEDCALLABLE;
	}
	f = o2EEL_function(fo);

	DBG4C(printf("---------- eel_call(%s) ----------\n", eel_o2s(f->common.name));)
	x = check_args(vm, fo);
	if(x)
	{
		const char *s;
		switch(x)
		{
		  case EEL_XFEWARGS:
			s = "  Too few arguments!";
			break;
		  case EEL_XTUPLEARGS:
			s = "  Incomplete tuples in tuple argument list!";
			break;
		  case EEL_XMANYARGS:
			s = "  Too many arguments!";
			break;
		  default:
			s = "  Incorrect arguments!";
			break;
		}
		call_msg(fo, EEL_EM_VMERROR, s);
		reset_args(vm);
/*FIXME:*/
		vm->resv = save_resv;
		vm->argv = save_argv;
		vm->argc = save_argc;
/*FIXME:*/
		return x;
	}

	/* Enter function */
	if(vm->heap[vm->base].type != EEL_TNIL)
		result = vm->base;
	else
		result = -1;
	x = call_f(vm, fo, result, 0);
	if(!x)
	{
		/* If it's an EEL function, we actually need to *run* it...! */
		if(!(f->common.flags & EEL_FF_CFUNC))
		{
			x = call_do_run(vm);
			if(x)
				call_msg(fo, EEL_EM_VMERROR, "  Function "
						"aborted with exception %s",
						eel_x_name(vm, x));
		}
	}
	else
		call_msg(fo, EEL_EM_VMERROR, "  Exception %s was thrown.",
				eel_x_name(vm, x));
/*FIXME:*/
	vm->resv = save_resv;
	vm->argv = save_argv;
	vm->argc = save_argc;
/*FIXME:*/
	return x;
}


EEL_xno eel_calln(EEL_vm *vm, EEL_object *m, const char *fn)
{
	EEL_xno x;
	EEL_object *f = find_function(vm, m, fn);
	if(!f)
	{
		eel_msg(VMP->state, EEL_EM_VMERROR,
				"eel_calln(): Function not found!");
		return EEL_XNOTFOUND;
	}
	x = eel_call(vm, f);
	eel_o_disown_nz(f);
	return x;
}


EEL_xno eel_callf(EEL_vm *vm, EEL_object *f, const char *fmt, ...)
{
	reset_args(vm);
	if(fmt)
	{
		EEL_xno x;
		va_list args;
		va_start(args, fmt);
		x = eel_vargf(vm, fmt, args);
		va_end(args);
		if(x)
			return x;
	}
	return eel_call(vm, f);
}


EEL_xno eel_callnf(EEL_vm *vm, EEL_object *m, const char *fn, const char *fmt, ...)
{
	EEL_xno x;
	EEL_object *f = find_function(vm, m, fn);
	if(!f)
	{
		eel_msg(VMP->state, EEL_EM_VMERROR,
				"eel_callnf(): Function not found!");
		return EEL_XNOTFOUND;
	}
	reset_args(vm);
	if(fmt)
	{
		va_list args;
		va_start(args, fmt);
		x = eel_vargf(vm, fmt, args);
		va_end(args);
		if(x)
			return x;
	}
	x = eel_call(vm, f);
	eel_o_disown_nz(f);
	return x;
}

#if 0
/*----------------------------------------------------------
	VM Context Stack
----------------------------------------------------------*/
EEL_xno eel_push_vm_context(EEL_vm *vm)
{
	EEL_vm_context *vmctx = eel_malloc(vm, sizeof(EEL_vm_context));
	if(!vmctx)
		return EEL_XMEMORY;
	vmctx->prev = VMP->ctxstack;
	VMP->ctxstack = vmctx;

	/* Save */
	vmctx->heapsize = vm->heapsize;
	vmctx->heap = vm->heap;
	vmctx->pc = vm->pc;
	vmctx->base = vm->base;
	vmctx->sp = vm->sp;
	vmctx->sbase = vm->sbase;
	vmctx->resv = vm->resv;
	vmctx->argv = vm->argv;
	vmctx->argc = vm->argc;
	vmctx->exception = VMP->exception;
	vmctx->limbo = VMP->limbo;

	/* Initialize */
	VMP->exception.type = EEL_TNIL;
	vm->heapsize = 0;
	vm->heap = NULL;
	vm_init(VMP->state, vm, EEL_INITHEAP);

	return 0;
}


void eel_pop_vm_context(EEL_vm *vm)
{
	EEL_vm_context *vmctx = VMP->ctxstack;
#ifdef EEL_VM_CHECKING
	if(!vmctx)
	{
		eel_vmdump(vm, "Tried to pop toplevel VM context!", vm);
		return;
	}
#endif
	VMP->ctxstack = vmctx->prev;

	/* Clean up */
	free(vm->heap);
	eel_v_disown_nz(&VMP->exception);

	/* Restore */
	vm->heapsize = vmctx->heapsize;
	vm->heap = vmctx->heap;
	vm->pc = vmctx->pc;
	vm->base = vmctx->base;
	vm->sp = vmctx->sp;
	vm->sbase = vmctx->sbase;
	vm->resv = vmctx->resv;
	vm->argv = vmctx->argv;
	vm->argc = vmctx->argc;
	VMP->exception = vmctx->exception;
	VMP->limbo = vmctx->limbo;
}
#endif
