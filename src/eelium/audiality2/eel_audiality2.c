/*
---------------------------------------------------------------------------
	eel_audiality2.h - EEL Audiality 2 binding
---------------------------------------------------------------------------
 * Copyright 2011-2012, 2014 David Olofson
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

#include <string.h>
#include "eel_audiality2.h"


typedef struct
{
	/* Class type IDs */
	int		state_cid;

	EEL_object	*statefields;
} A2_moduledata;

/*FIXME: Can't get moduledata from within constructors... */
static A2_moduledata a2_md;


static EEL_xno ea2_translate_error(A2_errors a2e)
{
	switch(a2e)
	{
	  case A2_REFUSE:	return EEL_XREFUSE;
	  case A2_OOMEMORY:	return EEL_XMEMORY;
	  case A2_OOHANDLES:	return EEL_XMEMORY;
	  /*...*/
	  default:		return EEL_XDEVICEERROR;
	}
}

/*----------------------------------------------------------
	Audiality 2 state class
----------------------------------------------------------*/

/* initv [sample_rate, buffer_size, channels, flags] */
static EEL_xno a2s_construct(EEL_vm *vm, EEL_types type,
		EEL_value *initv, int initc, EEL_value *result)
{
	EA2_state *ea2s;
	A2_state *st;
	EEL_value v;
	EEL_xno xno;
	EEL_object *eo;
	A2_config *cfg = a2_OpenConfig(
		initc >= 1 ? eel_v2l(initv) : -1,	/* sample rate */
		initc >= 2 ? eel_v2l(initv + 1) : -1,	/* buffering */
		initc >= 3 ? eel_v2l(initv + 2) : -1,	/* channels */
		initc >= 4 ? eel_v2l(initv + 3) : -1	/* flags */
	);
	if(!cfg)
		return EEL_XDEVICEOPEN;
	if(initc >= 5)
	{
		const char *adname = eel_v2s(initv + 4);
		if(adname)
		{
			A2_errors ae;
			A2_driver *ad = a2_NewDriver(A2_AUDIODRIVER, adname);
			if(!ad)
			{
				a2_CloseConfig(cfg);
				return EEL_XNOTFOUND;
			}
			if((ae = a2_AddDriver(cfg, ad)))
			{
				a2_CloseConfig(cfg);
				return ea2_translate_error(ae);
			}
		}
	}
	/* Hand it over to the state, so we don't have to keep track of it! */
	cfg->flags |= A2_STATECLOSE;
	if(!(st = a2_Open(cfg)))
		return EEL_XDEVICEOPEN;
	if(!(eo = eel_o_alloc(vm, sizeof(EA2_state), type)))
	{
		a2_Close(st);
		return EEL_XMEMORY;
	}
	ea2s = o2EA2_state(eo);
	memset(ea2s, 0, sizeof(EA2_state));
	ea2s->state = st;
	if((xno = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v)))
	{
		eel_o_free(eo);
		a2_Close(st);
		return xno;
	}
	ea2s->table = eel_v2o(&v);
	eel_o2v(result, eo);
	return 0;
}

static EEL_xno a2s_destruct(EEL_object *eo)
{
	EA2_state *ea2s = o2EA2_state(eo);
	a2_Close(ea2s->state);
	ea2s->state = NULL;
	eel_disown(ea2s->table);
	ea2s->table = NULL;
	return 0;
}


static EEL_xno a2s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EA2_state *ea2s = o2EA2_state(eo);
	/* Passive content only for now, so we just read the table directly! */
	if(eel_table_get(a2_md.statefields, op1, op2) != EEL_XOK)
	{
		/* No hit! Fall through to extension table. */
		EEL_xno x = eel_table_get(ea2s->table, op1, op2);
		if(x)
			return x;
		eel_v_own(op2);
		return 0;
	}
	eel_v_own(op2);
	return 0;
}


static EEL_xno a2s_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_value v;
	EA2_state *ea2s = o2EA2_state(eo);
	if(eel_table_get(a2_md.statefields, op1, &v) == EEL_XOK)
		return EEL_XCANTWRITE;
	return eel_o_metamethod(ea2s->table, EEL_MM_SETINDEX, op1, op2);
}


/*---------------------------------------------------------
	Error handling
---------------------------------------------------------*/

static EEL_xno ea2_LastError(EEL_vm *vm)
{
	eel_l2v(vm->heap + vm->resv, a2_LastError());
	return 0;
}


static EEL_xno ea2_ErrorString(EEL_vm *vm)
{
	eel_s2v(vm, vm->heap + vm->resv, a2_ErrorString(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


/*---------------------------------------------------------
	Handle management
---------------------------------------------------------*/

/* RootVoice(state) */
static EEL_xno ea2_RootVoice(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_RootVoice(ea2s->state));
	return 0;
}

/* TypeOf(state, handle) */
static EEL_xno ea2_TypeOf(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_TypeOf(ea2s->state, eel_v2l(args + 1)));
	return 0;
}

/* TypeName(state, typecode) */
static EEL_xno ea2_TypeName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *name;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if((name = a2_TypeName(ea2s->state, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* String(state, handle) */
static EEL_xno ea2_String(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if((s = a2_String(ea2s->state, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, s);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* Name(state, handle) */
static EEL_xno ea2_Name(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *name;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if((name = a2_Name(ea2s->state, eel_v2l(args + 1))))
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

/* Retain(state, handle) */
static EEL_xno ea2_Retain(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_Retain(ea2s->state, eel_v2l(args + 1)));
	return 0;
}

/* Release(state, handle) */
static EEL_xno ea2_Release(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_Release(ea2s->state, eel_v2l(args + 1)));
	return 0;
}

/* Assign(state, owner, handle) */
static EEL_xno ea2_Assign(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_Assign(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2)));
	return 0;
}

/* Export(state, owner, handle)[name] */
static EEL_xno ea2_Export(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *name;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(vm->argc >= 4)
		name = eel_v2s(args + 3);
	else
		name = NULL;
	eel_l2v(vm->heap + vm->resv, a2_Export(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), name));
	return 0;
}


/*---------------------------------------------------------
	Object loading/creation
---------------------------------------------------------*/

/* NewBank(state, name)[flags] */
static EEL_xno ea2_NewBank(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *n;
	int flags = 0;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(!(n = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		flags = eel_v2l(args + 2);
	eel_l2v(vm->heap + vm->resv, a2_NewBank(ea2s->state, n, flags));
	return 0;
}

/* LoadString(state, code, name) */
static EEL_xno ea2_LoadString(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *code;
	const char *name = NULL;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(!(code = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	if(vm->argc >= 3)
		if(!(name = eel_v2s(args + 2)))
			return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_LoadString(ea2s->state, code, name));
	return 0;
}

/* Load(state, filename) */
static EEL_xno ea2_Load(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	const char *fn;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	if(!(fn = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv,
			a2_Load(o2EA2_state(args->objref.v)->state, fn));
	return 0;
}

/* NewString(state, string) */
static EEL_xno ea2_NewString(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(!(s = eel_v2s(args + 1)))
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_NewString(ea2s->state, s));
	return 0;
}

/* UnloadAll(state) */
static EEL_xno ea2_UnloadAll(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_UnloadAll(ea2s->state));
	return 0;
}


/*---------------------------------------------------------
	Objects and exports
---------------------------------------------------------*/

/* Get(state, node, path) */
static EEL_xno ea2_Get(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *path;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(!(path = eel_v2s(args + 2)))
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, a2_Get(ea2s->state, eel_v2l(args + 1),
			path));
	return 0;
}

/* GetExport(state, node, index) */
static EEL_xno ea2_GetExport(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_GetExport(ea2s->state,
			eel_v2l(args + 1), eel_v2l(args + 2)));
	return 0;
}

/* GetExportName(state, node, index) */
static EEL_xno ea2_GetExportName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	const char *s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	s = a2_GetExportName(ea2s->state, eel_v2l(args + 1), eel_v2l(args + 2));
	if(s)
		eel_s2v(vm, vm->heap + vm->resv, s);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}


/*---------------------------------------------------------
	Playing and controlling
---------------------------------------------------------*/

/* Now(state) */
static EEL_xno ea2_Now(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	a2_Now(o2EA2_state(args->objref.v)->state);
	return 0;
}

/* Wait(state, dt) */
static EEL_xno ea2_Wait(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	a2_Wait(o2EA2_state(args->objref.v)->state, eel_v2d(args + 1));
	return 0;
}

/* NewGroup(state, parent) */
static EEL_xno ea2_NewGroup(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_NewGroup(ea2s->state, eel_v2l(args + 1)));
	return 0;
}

/* Start(state, parent, program)<args> */
static EEL_xno ea2_Start(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	int i, a[A2_MAXARGS];
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Starta(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* Play(state, parent, program)<args> */
static EEL_xno ea2_Play(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	int i, a[A2_MAXARGS];
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Playa(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* Send(state, voice, entrypoint)<args> */
static EEL_xno ea2_Send(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	int i, a[A2_MAXARGS];
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_Senda(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* SendSub(state, voice, entrypoint)<args> */
static EEL_xno ea2_SendSub(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	int i, a[A2_MAXARGS];
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, a2_SendSuba(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

/* Kill(state, voice) */
static EEL_xno ea2_Kill(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_Kill(ea2s->state, eel_v2l(args + 1)));
	return 0;
}

/* KillSub(state, voice) */
static EEL_xno ea2_KillSub(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_l2v(vm->heap + vm->resv, a2_KillSub(ea2s->state, eel_v2l(args + 1)));
	return 0;
}


/*---------------------------------------------------------
	Waveform management
---------------------------------------------------------*/

/* WaveUpload(state, wavetype, period, flags, data) */
static EEL_xno ea2_WaveUpload(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	EEL_value *d = args + 4;
	A2_sampleformats fmt;
	const void *data;
	unsigned size;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	size = eel_length(d->objref.v);
	data = eel_rawdata(d->objref.v);
	if(!data)
		return EEL_XWRONGTYPE;
	switch((EEL_classes)EEL_TYPE(d))
	{
	  case EEL_CSTRING:
	  case EEL_CDSTRING:
		fmt = A2_I8;
		data = eel_v2s(d);
		break;
	  case EEL_CVECTOR_S8:
		fmt = A2_I8;
		size = eel_length(d->objref.v);
		break;
	  case EEL_CVECTOR_S16:
		fmt = A2_I16;
		size *= 2;
		break;
	  case EEL_CVECTOR_S32:
		fmt = A2_I32;
		size *= 4;
		break;
	  case EEL_CVECTOR_F:
		fmt = A2_F32;
		size *= 4;
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	eel_l2v(vm->heap + vm->resv, a2_WaveUpload(ea2s->state,
			eel_v2l(args + 1), eel_v2l(args + 3), eel_v2l(args + 4),
			fmt, data, size));
	return 0;
}


/*---------------------------------------------------------
	Utilities
---------------------------------------------------------*/

static EEL_xno ea2_F2P(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, a2_F2P(eel_v2d(args)));
	return 0;
}

static EEL_xno ea2_Rand(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	eel_d2v(vm->heap + vm->resv, a2_Rand(ea2s->state, eel_v2d(args + 1)));
	return 0;
}


/*---------------------------------------------------------
	Object property interface
---------------------------------------------------------*/

/* GetProperty(state, handle, property) */
static EEL_xno ea2_GetProperty(EEL_vm *vm)
{
	A2_errors ae;
	int v;
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	ae = a2_GetProperty(ea2s->state, eel_v2l(args + 1),
			eel_v2l(args + 2), &v);
	if(ae)
		return ea2_translate_error(ae);
	eel_l2v(vm->heap + vm->resv, v);
	return 0;
}

/* GetProperty(state, handle, property) */
/* SetProperty(state, handle, property, value) */
static EEL_xno ea2_SetProperty(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EA2_state *ea2s;
	if(EEL_TYPE(args) != a2_md.state_cid)
		return EEL_XWRONGTYPE;
	ea2s = o2EA2_state(args->objref.v);
	if(a2_SetProperty(ea2s->state, eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3)))
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	Versioning
----------------------------------------------------------*/

static EEL_xno ea2_HeaderVersion(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	unsigned ver = a2_HeaderVersion();
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, A2_MAJOR(ver));
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, A2_MINOR(ver));
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, A2_MICRO(ver));
	eel_setlindex(a, 2, &v);
	if(A2_BUILD(ver))
	{
		eel_l2v(&v, A2_BUILD(ver));
		eel_setlindex(a, 3, &v);
	}
	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}

static EEL_xno ea2_LinkedVersion(EEL_vm *vm)
{
	EEL_object *a;
	EEL_value v;
	unsigned ver = a2_LinkedVersion();
	EEL_xno x = eel_o_construct(vm, EEL_CARRAY, NULL, 0, &v);
	if(x)
		return x;
	a = v.objref.v;
	eel_l2v(&v, A2_MAJOR(ver));
	eel_setlindex(a, 0, &v);
	eel_l2v(&v, A2_MINOR(ver));
	eel_setlindex(a, 1, &v);
	eel_l2v(&v, A2_MICRO(ver));
	eel_setlindex(a, 2, &v);
	if(A2_BUILD(ver))
	{
		eel_l2v(&v, A2_BUILD(ver));
		eel_setlindex(a, 3, &v);
	}
	eel_o2v(vm->heap + vm->resv, a);
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno eel_a2_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close the EEL state */
	if(closing)
	{
		if(a2_md.statefields)
			eel_disown(a2_md.statefields);
		memset(&a2_md, 0, sizeof(a2_md));
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp eel_a2_constants[] =
{
	/* Engine and API limits and information */
	{"MIDDLEC",		A2_MIDDLEC	},
	{"REGISTERS",		A2_REGISTERS	},
	{"MAXARGS",		A2_MAXARGS	},
	{"MAXEPS",		A2_MAXEPS	},
	{"MIPLEVELS",		A2_MIPLEVELS	},
	{"MAXFRAG",		A2_MAXFRAG	},
	{"MAXCHANNELS",		A2_MAXCHANNELS	},

	/* Object types */
	{"BANK",		A2_TBANK	},
	{"WAVE",		A2_TWAVE	},
	{"PROGRAM",		A2_TPROGRAM	},

	/* Flags for a2_Open() */
	{"EXPORTALL",		A2_EXPORTALL	},
	{"TIMESTAMP",		A2_TIMESTAMP	},
	{"NOAUTOCNX",		A2_NOAUTOCNX	},
	{"REALTIME",		A2_REALTIME	},

	/* Wave types */
	{"WOFF",		A2_WOFF		},
	{"WNOISE",		A2_WNOISE	},
	{"WWAVE",		A2_WWAVE	},
	{"WMIPWAVE",		A2_WMIPWAVE	},

	/* Flags for waveform uploading and rendering */
	{"LOOPED",		A2_LOOPED	},
	{"NORMALIZE",		A2_NORMALIZE	},
	{"XFADE",		A2_XFADE	},
	{"REVMIX",		A2_REVMIX	},
	{"CLEAR",		A2_CLEAR	},
	{"UNPREPARED",		A2_UNPREPARED	},

	/* Properties: Global settings and profiling information */
	{"PSAMPLERATE",		A2_PSAMPLERATE	},
	{"PBUFFER",		A2_PBUFFER	},
	{"PACTIVEVOICES",	A2_PACTIVEVOICES},
	{"PFREEVOICES",		A2_PFREEVOICES	},
	{"PTOTALVOICES",	A2_PTOTALVOICES	},
	{"PCPULOADAVG",		A2_PCPULOADAVG	},
	{"PCPULOADMAX",		A2_PCPULOADMAX	},
	{"PCPUTIMEAVG",		A2_PCPUTIMEAVG	},
	{"PCPUTIMEMAX",		A2_PCPUTIMEMAX	},
	{"PINSTRUCTIONS",	A2_PINSTRUCTIONS},
	{"PEXPORTALL",		A2_PEXPORTALL	},

	/* Properties: General properties */
	{"PCHANNELS",		A2_PCHANNELS	},
	{"PFLAGS",		A2_PFLAGS	},

	/* Properties: Wave properties */
	{"PLOOPED",		A2_PLOOPED	},

	{NULL, 0}
};


static void addfunc(EEL_object *m, EEL_object *t,
		int results, const char *name,
		int reqargs, int optargs, int tupargs,
		EEL_cfunc_cb func)
{
	EEL_value v;
	v.type = EEL_TOBJREF;
	v.objref.v = eel_export_cfunction(m, results, name,
			reqargs, optargs, tupargs, func);
	if(!v.objref.v)
		return;
	eel_table_sets(t, name, &v);
}


EEL_xno eel_audiality2_init(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value v;
	EEL_object *t;
	EEL_object *c;
	EEL_object *m = eel_create_module(vm, "Audiality2", eel_a2_unload, &a2_md);
	memset(&a2_md, 0, sizeof(a2_md));
	if(!m)
		return EEL_XMODULEINIT;

	/* Register class 'a2state' */
	c = eel_export_class(m, "a2state", -1, a2s_construct, a2s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, a2s_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, a2s_setindex);
	a2_md.state_cid = eel_class_typeid(c);

	x = eel_o_construct(vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return EEL_XMODULEINIT;
	t = a2_md.statefields = eel_v2o(&v);

	/* Error handling */
	eel_export_cfunction(m, 1, "LastError", 0, 0, 0, ea2_LastError);
	eel_export_cfunction(m, 1, "ErrorString", 1, 0, 0, ea2_ErrorString);

	/* Handle management */
	addfunc(m, t, 1, "RootVoice", 1, 0, 0, ea2_RootVoice);
	addfunc(m, t, 1, "TypeOf", 2, 0, 0, ea2_TypeOf);
	addfunc(m, t, 1, "TypeName", 2, 0, 0, ea2_TypeName);
	addfunc(m, t, 1, "String", 2, 0, 0, ea2_String);
	addfunc(m, t, 1, "Name", 2, 0, 0, ea2_Name);
	addfunc(m, t, 1, "Retain", 2, 0, 0, ea2_Retain);
	addfunc(m, t, 1, "Release", 2, 0, 0, ea2_Release);
	addfunc(m, t, 1, "Assign", 3, 0, 0, ea2_Assign);
	addfunc(m, t, 1, "Export", 3, 1, 0, ea2_Export);

	/* Object loading/creation */
	addfunc(m, t, 1, "NewBank", 2, 1, 0, ea2_NewBank);
	addfunc(m, t, 1, "LoadString", 2, 1, 0, ea2_LoadString);
	addfunc(m, t, 1, "Load", 2, 0, 0, ea2_Load);
	addfunc(m, t, 1, "NewString", 2, 0, 0, ea2_NewString);
	addfunc(m, t, 1, "UnloadAll", 2, 0, 0, ea2_UnloadAll);

	/* Objects and exports */
	addfunc(m, t, 1, "Get", 3, 0, 0, ea2_Get);
	addfunc(m, t, 1, "GetExport", 3, 0, 0, ea2_GetExport);
	addfunc(m, t, 1, "GetExportName", 3, 0, 0, ea2_GetExportName);

	/* Playing and controlling */
	addfunc(m, t, 0, "Now", 1, 0, 0, ea2_Now);
	addfunc(m, t, 0, "Wait", 2, 0, 0, ea2_Wait);
	addfunc(m, t, 1, "NewGroup", 2, 0, 0, ea2_NewGroup);
	addfunc(m, t, 1, "Start", 3, 0, 1, ea2_Start);
	addfunc(m, t, 1, "Play", 3, 0, 1, ea2_Play);
	addfunc(m, t, 1, "Send", 3, 0, 1, ea2_Send);
	addfunc(m, t, 1, "SendSub", 3, 0, 1, ea2_SendSub);
	addfunc(m, t, 1, "Kill", 2, 0, 0, ea2_Kill);
	addfunc(m, t, 1, "KillSub", 2, 0, 0, ea2_KillSub);

	/* Waveform management (waves.h) */
	addfunc(m, t, 1, "WaveUpload", 5, 0, 0, ea2_WaveUpload);

	/* Utilities */
	eel_export_cfunction(m, 1, "F2P", 1, 0, 0, ea2_F2P);
	addfunc(m, t, 1, "Rand", 2, 0, 0, ea2_Rand);

	/* Object property interface */
	addfunc(m, t, 1, "GetProperty", 3, 0, 0, ea2_GetProperty);
	addfunc(m, t, 0, "SetProperty", 4, 0, 0, ea2_SetProperty);

	/* Versioning */
	eel_export_cfunction(m, 1, "HeaderVersion", 0, 0, 0, ea2_HeaderVersion);
	eel_export_cfunction(m, 1, "LinkedVersion", 0, 0, 0, ea2_LinkedVersion);

	/* Constants and enums */
	eel_export_lconstants(m, eel_a2_constants);

	eel_disown(m);
	return 0;
}
