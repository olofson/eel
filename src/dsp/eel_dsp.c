/*
 * DSP Module - KISS FFT library binding + other DSP tools
 *
 * Copyright (C) 2006-2007, 2010 David Olofson
 */

#include <math.h>
#include "eel_dsp.h"
#include "kfc.h"
#include "e_vector.h"		/* For 'vector' internals */
#include "e_operate.h"		/* For inline metamethod calling */


/*-------------------------------------------------------------------
	Internal tools
-------------------------------------------------------------------*/

typedef enum
{
	OP_GET = 0,
	OP_SET,
	OP_ADD,
	OP_SUB,
	OP_MUL
} OPS;

static EEL_xno do_op2(EEL_object *o, int ind, double *avals, OPS op)
{
	double vvals[2];
	EEL_vector *vec = NULL;
	switch(o->type)
	{
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
	  case EEL_CVECTOR_D:
		vec = o2EEL_vector(o);
		if(ind < 0)
			return EEL_XLOWINDEX;
		else if(ind + 1 >= vec->length)
			return EEL_XHIGHINDEX;
		break;
	  default:
		break;
	}
	if(op != OP_SET)
	{
		switch(o->type)
		{
		  case EEL_CVECTOR_U8:
			vvals[0] = vec->buffer.u8[ind];
			vvals[1] = vec->buffer.u8[ind + 1];
			break;
		  case EEL_CVECTOR_S8:
			vvals[0] = vec->buffer.s8[ind];
			vvals[1] = vec->buffer.s8[ind + 1];
			break;
		  case EEL_CVECTOR_U16:
			vvals[0] = vec->buffer.u16[ind];
			vvals[1] = vec->buffer.u16[ind + 1];
			break;
		  case EEL_CVECTOR_S16:
			vvals[0] = vec->buffer.s16[ind];
			vvals[1] = vec->buffer.s16[ind + 1];
			break;
		  case EEL_CVECTOR_U32:
			vvals[0] = vec->buffer.u32[ind];
			vvals[1] = vec->buffer.u32[ind + 1];
			break;
		  case EEL_CVECTOR_S32:
			vvals[0] = vec->buffer.s32[ind];
			vvals[1] = vec->buffer.s32[ind + 1];
			break;
		  case EEL_CVECTOR_F:
			vvals[0] = vec->buffer.f[ind];
			vvals[1] = vec->buffer.f[ind + 1];
			break;
		  case EEL_CVECTOR_D:
			vvals[0] = vec->buffer.d[ind];
			vvals[1] = vec->buffer.d[ind + 1];
			break;
		  default:
		  {
			EEL_xno x;
			EEL_value v;
			eel_l2v(&v, ind);
			x = eel_o__metamethod(o, EEL_MM_GETINDEX, &v, &v);
			if(x)
				return x;
			vvals[0] = eel_v2d(&v);
			eel_v_disown_nz(&v);
			eel_l2v(&v, ind + 1);
			x = eel_o__metamethod(o, EEL_MM_GETINDEX, &v, &v);
			if(x)
				return x;
			vvals[1] = eel_v2d(&v);
			eel_v_disown_nz(&v);
			break;
		  }
		}
	}
	switch(op)
	{
	  case OP_GET:
		avals[0] = vvals[0];
		avals[1] = vvals[1];
		return EEL_XNONE;
	  case OP_SET:
		vvals[0] = avals[0];
		vvals[1] = avals[1];
		break;
	  case OP_ADD:
		vvals[0] += avals[0];
		vvals[1] += avals[1];
		break;
	  case OP_SUB:
		vvals[0] -= avals[0];
		vvals[1] -= avals[1];
		break;
	  case OP_MUL:
		vvals[0] *= avals[0];
		vvals[1] *= avals[1];
		break;
	}
	switch(o->type)
	{
	  case EEL_CVECTOR_U8:
		vec->buffer.u8[ind] = vvals[0];
		vec->buffer.u8[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_S8:
		vec->buffer.s8[ind] = vvals[0];
		vec->buffer.s8[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_U16:
		vec->buffer.u16[ind] = vvals[0];
		vec->buffer.u16[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_S16:
		vec->buffer.s16[ind] = vvals[0];
		vec->buffer.s16[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_U32:
		vec->buffer.u32[ind] = vvals[0];
		vec->buffer.u32[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_S32:
		vec->buffer.s32[ind] = vvals[0];
		vec->buffer.s32[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_F:
		vec->buffer.f[ind] = vvals[0];
		vec->buffer.f[ind + 1] = vvals[1];
		break;
	  case EEL_CVECTOR_D:
		vec->buffer.d[ind] = vvals[0];
		vec->buffer.d[ind + 1] = vvals[1];
		break;
	  default:
	  {
		EEL_value iv, v;
		eel_l2v(&iv, ind);
		eel_d2v(&v, vvals[0]);
		eel_o__metamethod(o, EEL_MM_SETINDEX, &iv, &v);
		eel_l2v(&iv, ind + 1);
		eel_d2v(&v, vvals[1]);
		return eel_o__metamethod(o, EEL_MM_SETINDEX, &iv, &v);
	  }
	}
	return EEL_XNONE;
}


/*-------------------------------------------------------------------
	Statistics
-------------------------------------------------------------------*/

static EEL_xno do_sum(EEL_vm *vm, int *count)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_vector *vec;
	int first = 0;
	int last;
	int stride = 1;
	int i;
	double sum = 0.0f;
	*count = 0;

	switch(EEL_TYPE(args))
	{
	  case EEL_CVECTOR_U8:
	  case EEL_CVECTOR_S8:
	  case EEL_CVECTOR_U16:
	  case EEL_CVECTOR_S16:
	  case EEL_CVECTOR_U32:
	  case EEL_CVECTOR_S32:
	  case EEL_CVECTOR_F:
	  case EEL_CVECTOR_D:
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	vec = o2EEL_vector(args[0].objref.v);

	if(!vec->length)
	{
		eel_d2v(vm->heap + vm->resv, sum);
		return EEL_XNONE;
	}
	last = vec->length - 1;

	switch(vm->argc)
	{
	  case 4:
		stride = eel_v2l(args + 3);
		if(stride <= 0)
			return EEL_XLOWVALUE;
		/* Fall through! */
	  case 3:
		last = eel_v2l(args + 2);
		if(last < 0)
			return EEL_XLOWINDEX;
		else if(last >= vec->length)
			return EEL_XHIGHINDEX;
		/* Fall through! */
	  case 2:
		first = eel_v2l(args + 1);
		if(first < 0)
			return EEL_XLOWINDEX;
		else if(first >= vec->length)
			return EEL_XHIGHINDEX;
		/* Fall through! */
	  case 1:
		break;
	}

	if(last < first)
		return EEL_XWRONGINDEX;

	switch(EEL_TYPE(args))
	{
	  case EEL_CVECTOR_U8:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.u8[i];
		break;
	  case EEL_CVECTOR_S8:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.s8[i];
		break;
	  case EEL_CVECTOR_U16:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.u16[i];
		break;
	  case EEL_CVECTOR_S16:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.s16[i];
		break;
	  case EEL_CVECTOR_U32:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.u32[i];
		break;
	  case EEL_CVECTOR_S32:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.s32[i];
		break;
	  case EEL_CVECTOR_F:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.f[i];
		break;
	  case EEL_CVECTOR_D:
		for(i = first; i <= last; i += stride)
			sum += vec->buffer.d[i];
		break;
	  default:
		return EEL_XWRONGTYPE;
	}

	*count = 1 + (last - first) / stride;
	eel_d2v(vm->heap + vm->resv, sum);
	return EEL_XNONE;
}


// function sum(v)[first, last, stride];
static EEL_xno dsp_sum(EEL_vm *vm)
{
	int count;
	return do_sum(vm, &count);
}


// function average(v)[first, last, stride];
static EEL_xno dsp_average(EEL_vm *vm)
{
	int count;
	EEL_xno x = do_sum(vm, &count);
	if(!x && count)
		vm->heap[vm->resv].real.v /= count;
	return x;
}


/*-------------------------------------------------------------------
	Function renderers
-------------------------------------------------------------------*/

/* function polynomial(count)<coeffs>; */
static EEL_xno do_polynomial(EEL_vm *vm, int add, int inclusive)
{
	EEL_value v, iv;
	EEL_xno x;
	EEL_value *args = vm->heap + vm->argv;
	EEL_object *o = NULL;
	EEL_vector *vec;
	int ncoeffs = vm->argc - 1;
	double *coeffs;
	double fx, dfx;
	int count;
	int path;

	if(add)
	{
		o = eel_v2o(args);
		if(!o)
			return EEL_XNEEDOBJECT;
		x = eel_o__metamethod(o, EEL_MM_LENGTH, NULL, &v);
		if(x)
			return x;
		count = eel_v2l(&v);
	}
	else
		count = eel_v2l(args);

	if(ncoeffs)
	{
		int i;
		coeffs = eel_malloc(vm, sizeof(double) * ncoeffs);
		if(!coeffs)
			return EEL_XMEMORY;
		for(i = 0; i < ncoeffs; ++i)
			coeffs[i] = eel_v2d(args + 1 + i);
	}
	else
	{
		coeffs = eel_malloc(vm, sizeof(double));
		if(!coeffs)
			return EEL_XMEMORY;
		coeffs[0] = 0.0f;
	}

	if(!add)
	{
		o = eel_new_indexable(vm, EEL_CVECTOR_D, count);
		if(!o)
		{
			eel_free(vm, coeffs);
			return EEL_XCONSTRUCTOR;
		}
	}


	eel_l2v(&iv, 0);
	fx = 0.0f;
	if(inclusive & (count > 1))
		dfx = 1.0f / (double)(count - 1);
	else
		dfx = 1.0f / (double)count;
	vec = o2EEL_vector(o);
	if(add)
		switch(o->type)
		{
		  case EEL_CVECTOR_U8:	path = 1; break;
		  case EEL_CVECTOR_S8:	path = 2; break;
		  case EEL_CVECTOR_U16:	path = 3; break;
		  case EEL_CVECTOR_S16:	path = 4; break;
		  case EEL_CVECTOR_U32:	path = 5; break;
		  case EEL_CVECTOR_S32:	path = 6; break;
		  case EEL_CVECTOR_F:	path = 7; break;
		  case EEL_CVECTOR_D:	path = 8; break;
		  default:		path = 9; break;
		}
	else
		path = 0;
	for( ; iv.integer.v < count; ++iv.integer.v)
	{
		double out = coeffs[0];
		double fxx = fx;
		int c;
		for(c = 1; c < ncoeffs; ++c)
		{
			out += fxx * coeffs[c];
			fxx *= fx;
		}
		switch(path)
		{
		  case 0:
			vec->buffer.d[iv.integer.v] = out;
			break;
		  case 1:
			vec->buffer.u8[iv.integer.v] += out;
			break;
		  case 2:
			vec->buffer.s8[iv.integer.v] += out;
			break;
		  case 3:
			vec->buffer.u16[iv.integer.v] += out;
			break;
		  case 4:
			vec->buffer.s16[iv.integer.v] += out;
			break;
		  case 5:
			vec->buffer.u32[iv.integer.v] += out;
			break;
		  case 6:
			vec->buffer.s32[iv.integer.v] += out;
			break;
		  case 7:
			vec->buffer.f[iv.integer.v] += out;
			break;
		  case 8:
			vec->buffer.d[iv.integer.v] += out;
			break;
		  default:
		  {
			EEL_value rv;
			x = eel_o__metamethod(o, EEL_MM_GETINDEX, &iv, &v);
			if(x)
			{
				eel_free(vm, coeffs);
				return x;
			}
			eel_d2v(&rv, eel_v2d(&v) + out);
			eel_v_disown_nz(&v);
			x = eel_o__metamethod(o, EEL_MM_SETINDEX, &iv, &rv);
			if(x)
			{
				eel_free(vm, coeffs);
				return x;
			}
			break;
		  }
		}
		fx += dfx;
	}

	if(!add)
		eel_o2v(vm->heap + vm->resv, o);
	eel_free(vm, coeffs);
	return EEL_XNONE;
}


// function polynomial(count)<coeffs>;
static EEL_xno dsp_polynomial(EEL_vm *vm)
{
	return do_polynomial(vm, 0, 0);
}


// procedure add_polynomial(v)<coeffs>;
static EEL_xno dsp_add_polynomial(EEL_vm *vm)
{
	return do_polynomial(vm, 1, 0);
}


// function polynomial_i(count)<coeffs>;
static EEL_xno dsp_polynomial_i(EEL_vm *vm)
{
	return do_polynomial(vm, 0, 1);
}


// procedure add_polynomial_i(v)<coeffs>;
static EEL_xno dsp_add_polynomial_i(EEL_vm *vm)
{
	return do_polynomial(vm, 1, 1);
}


/*-------------------------------------------------------------------
	FFT
-------------------------------------------------------------------*/

static EEL_xno dsp_fft(EEL_vm *vm)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno dsp_ifft(EEL_vm *vm)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno dsp_fft_real(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	EEL_object *to, *fo;
	EEL_vector *tv, *fv;
	int i;
	double scale;
/*TODO: Use an intermediate buffer for other types! */
	if(EEL_TYPE(arg) != EEL_CVECTOR_D)
		return EEL_XWRONGTYPE;
	to = eel_v2o(arg);
	tv = o2EEL_vector(to);
	if(tv->length & 1)
		return EEL_XNEEDEVEN;
	fo = eel_new_indexable(vm, EEL_CVECTOR_D, tv->length + 2);
	if(!fo)
		return EEL_XCONSTRUCTOR;
	fv = o2EEL_vector(fo);
	kfc_fftr(tv->length, tv->buffer.d, (kiss_fft_cpx *)fv->buffer.d);
	scale = 1.0f / (double)tv->length;
	fv->buffer.d[0] *= scale;
	scale *= 2.0f;
	for(i = 1; i < fv->length - 1; i += 2)
	{
		fv->buffer.d[i] *= scale;
		fv->buffer.d[i + 1] *= scale;
	}
	fv->buffer.d[fv->length - 2] *= 0.5f;
	fv->buffer.d[fv->length - 1] *= 0.5f;
	eel_o2v(vm->heap + vm->resv, fo);
	return EEL_XNONE;
}


static EEL_xno dsp_ifft_real(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
	EEL_vector *tv, *fv;
	EEL_object *to, *fo;
	int nfft;
	int i;
	double save[3];
/*TODO: Use an intermediate buffer for other types! */
	if(EEL_TYPE(arg) != EEL_CVECTOR_D)
		return EEL_XWRONGTYPE;
	fo = eel_v2o(arg);
	fv = o2EEL_vector(fo);
	if((fv->length & 3) != 2)
		return EEL_XNEEDEVEN;
	nfft = fv->length - 2;
	to = eel_new_indexable(vm, EEL_CVECTOR_D, nfft);
	if(!to)
		return EEL_XCONSTRUCTOR;
	tv = o2EEL_vector(to);
	save[0] = tv->buffer.d[0];
	save[1] = tv->buffer.d[tv->length - 2];
	save[2] = tv->buffer.d[tv->length - 1];
	fv->buffer.d[0] *= 2.0f;
	fv->buffer.d[fv->length - 2] *= 2.0f;
	fv->buffer.d[fv->length - 1] *= 2.0f;
	kfc_ifftr(nfft, (kiss_fft_cpx *)fv->buffer.d, tv->buffer.d);
	for(i = 0; i < tv->length; i += 2)
	{
		tv->buffer.d[i] *= 0.5f;
		tv->buffer.d[i + 1] *= 0.5f;
	}
	fv->buffer.d[0] = save[0];
	fv->buffer.d[fv->length - 2] = save[1];
	fv->buffer.d[fv->length - 1] = save[2];
	eel_o2v(vm->heap + vm->resv, to);
	return EEL_XNONE;
}


static EEL_xno dsp_fft_cleanup(EEL_vm *vm)
{
	kfc_cleanup();
	kiss_fft_cleanup();
	return EEL_XNONE;
}


static EEL_xno dsp_c_abs(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value *args = vm->heap + vm->argv;
	double v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	x = do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_GET);
	if(x)
		return x;
	eel_d2v(vm->heap + vm->resv, sqrt(v[0]*v[0] + v[1]*v[1]));
	return EEL_XNONE;
}


static EEL_xno dsp_c_arg(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value *args = vm->heap + vm->argv;
	double v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	x = do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_GET);
	if(x)
		return x;
	eel_d2v(vm->heap + vm->resv, atan2(v[1], v[0]));
	return EEL_XNONE;
}


static EEL_xno dsp_c_set(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	v[0] = eel_v2d(args + 2);
	v[1] = eel_v2d(args + 3);
	/* NOTE: Exceptions are ignored and lead to NOP! */
	do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_SET);
	return EEL_XNONE;
}


static EEL_xno dsp_c_add(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	v[0] = eel_v2d(args + 2);
	v[1] = eel_v2d(args + 3);
	/* NOTE: Exceptions are ignored and lead to NOP! */
	do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_ADD);
	return EEL_XNONE;
}


static EEL_xno dsp_c_set_polar(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double m, v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	m = eel_v2d(args + 2);
	v[1] = eel_v2d(args + 3);
	v[0] = cos(v[1]) * m;
	v[1] = sin(v[1]) * m;
	/* NOTE: Exceptions are ignored and lead to NOP! */
	do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_SET);
	return EEL_XNONE;
}


static EEL_xno dsp_c_add_polar(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double m, v[2];
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	m = eel_v2d(args + 2);
	v[1] = eel_v2d(args + 3);
	v[0] = cos(v[1]) * m;
	v[1] = sin(v[1]) * m;
	/* NOTE: Exceptions are ignored and lead to NOP! */
	do_op2(eel_v2o(args), eel_v2l(args + 1) * 2, v, OP_ADD);
	return EEL_XNONE;
}


static EEL_xno dsp_c_add_i(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double re, im, frac, v[2];
	int i;
	OPS op;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	re = eel_v2d(args + 2);
	im = eel_v2d(args + 3);
	frac = eel_v2d(args + 1);
	i = 2 * (int)frac;
	frac -= floor(frac);
	if(i & 2)
		op = OP_SUB;
	else
		op = OP_ADD;
	v[0] = re * (1.0f - frac);
	v[1] = im * (1.0f - frac);
	do_op2(eel_v2o(args), i, v, op);
	v[0] = -re * frac;
	v[1] = -im * frac;
	do_op2(eel_v2o(args), i + 2, v, op);
	return EEL_XNONE;
}


static EEL_xno dsp_c_add_polar_i(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double re, im, frac, v[2];
	int i;
	OPS op;
	if(!EEL_IS_OBJREF(args->type))
		return EEL_XNEEDOBJECT;
	v[0] = eel_v2d(args + 2);
	v[1] = eel_v2d(args + 3);
	re = cos(v[1]) * v[0];
	im = sin(v[1]) * v[0];
	frac = eel_v2d(args + 1);
	i = 2 * (int)frac;
	frac -= floor(frac);
	if(i & 2)
		op = OP_SUB;
	else
		op = OP_ADD;
	v[0] = re * (1.0f - frac);
	v[1] = im * (1.0f - frac);
	do_op2(eel_v2o(args), i, v, op);
	v[0] = -re * frac;
	v[1] = -im * frac;
	do_op2(eel_v2o(args), i + 2, v, op);
	return EEL_XNONE;
}


/*-------------------------------------------------------------------
	Generators
-------------------------------------------------------------------*/

static EEL_xno gen_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	const char *tn;
	EDSP_generator *g;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EDSP_generator), type);
	if(!eo)
		return EEL_XMEMORY;
	g = o2EDSP_generator(eo);
	if(initc != 1)
	{
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	tn = eel_v2s(initv);
	if(!tn)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
/*......................*/
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno gen_destruct(EEL_object *eo)
{
//	EDSP_generator *g = o2EDSP_generator(eo);
	return 0;
}


static EEL_xno gen_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return EEL_XNOTIMPLEMENTED;
}


static EEL_xno gen_cast(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	return EEL_XNOTIMPLEMENTED;
}


/*-------------------------------------------------------------------
	Loading/unloading
-------------------------------------------------------------------*/

static EEL_xno dsp_unload(EEL_object *m, int closing)
{
	kfc_cleanup();
	kiss_fft_cleanup();
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


EEL_xno eel_dsp_init(EEL_vm *vm)
{
	EEL_object *m;
	EEL_object *c;

	/* Create module */
	m = eel_create_module(vm, "dsp", dsp_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	/* Statistics */
	eel_export_cfunction(m, 1, "sum", 1, 3, 0, dsp_sum);
	eel_export_cfunction(m, 1, "average", 1, 3, 0, dsp_average);

	/* Polynomials */
	eel_export_cfunction(m, 1, "polynomial", 1, 0, 1, dsp_polynomial);
	eel_export_cfunction(m, 0, "add_polynomial", 1, 0, 1,
			dsp_add_polynomial);
	eel_export_cfunction(m, 1, "polynomial_i", 1, 0, 1, dsp_polynomial_i);
	eel_export_cfunction(m, 0, "add_polynomial_i", 1, 0, 1,
			dsp_add_polynomial_i);

	/* FFT */
	eel_export_cfunction(m, 1, "fft", 1, 0, 0, dsp_fft);
	eel_export_cfunction(m, 1, "ifft", 1, 0, 0, dsp_ifft);
	eel_export_cfunction(m, 1, "fft_real", 1, 0, 0, dsp_fft_real);
	eel_export_cfunction(m, 1, "ifft_real", 1, 0, 0, dsp_ifft_real);
	eel_export_cfunction(m, 0, "fft_cleanup", 0, 0, 0,
			dsp_fft_cleanup);

	/* FFT/complex number vector tools */
	eel_export_cfunction(m, 1, "c_abs", 2, 0, 0, dsp_c_abs);
	eel_export_cfunction(m, 1, "c_arg", 2, 0, 0, dsp_c_arg);
	eel_export_cfunction(m, 0, "c_set", 4, 0, 0, dsp_c_set);
	eel_export_cfunction(m, 0, "c_add", 4, 0, 0, dsp_c_add);
	eel_export_cfunction(m, 0, "c_set_polar", 4, 0, 0, dsp_c_set_polar);
	eel_export_cfunction(m, 0, "c_add_polar", 4, 0, 0, dsp_c_add_polar);
	eel_export_cfunction(m, 0, "c_add_i", 4, 0, 0, dsp_c_add_i);
	eel_export_cfunction(m, 0, "c_add_polar_i", 4, 0, 0, dsp_c_add_polar_i);

	/* Register class 'generator' */
	c = eel_export_class(m, "generator", -1, gen_construct, gen_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, gen_getindex);
	eel_set_metamethod(c, EEL_MM_CAST, gen_cast);

	eel_disown(m);
	return 0;
}
