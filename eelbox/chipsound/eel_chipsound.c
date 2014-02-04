/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_chipsound.c - EEL ChipSound binding
---------------------------------------------------------------------------
 * Copyright (C) 2011-2012 David Olofson
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

#include "eel_chipsound.h"
#include "ChipSound/chipsound.h"

/* ChipSound has internal global state, so only one instance of the binding! */
static int loaded = 0;

/*---------------------------------------------------------
	Error handling
---------------------------------------------------------*/

static EEL_xno ecs_ErrorString(EEL_vm *vm)
{
	eel_s2v(vm, vm->heap + vm->resv, cs_ErrorString(
			eel_v2l(vm->heap + vm->argv)));
	return 0;
}


/*---------------------------------------------------------
	Open/close
---------------------------------------------------------*/

static EEL_xno ecs_Open(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int fs = vm->argc >= 1 ? eel_v2l(args) : 44100;
	int buffer = vm->argc >= 2 ? eel_v2l(args + 1) : 1024;
	int channels = vm->argc >= 3 ? eel_v2l(args + 2) : 2;
	int flags = vm->argc >= 4 ? eel_v2l(args + 3) : 0;
	eel_l2v(vm->heap + vm->resv, cs_Open(fs, buffer, channels, flags));
	return 0;
}

static EEL_xno ecs_Close(EEL_vm *vm)
{
	cs_Close();
	return 0;
}


/*---------------------------------------------------------
	Loading/unloading sounds
---------------------------------------------------------*/

static EEL_xno ecs_NewBank(EEL_vm *vm)
{
	const char *n = eel_v2s(vm->heap + vm->argv);
	if(!n)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, cs_NewBank(n));
	return 0;
}

static EEL_xno ecs_LoadString(EEL_vm *vm)
{
	const char *code = eel_v2s(vm->heap + vm->argv);
	const char *name = NULL;
	if(!code)
		return EEL_XWRONGTYPE;
	if(vm->argc >= 2)
		if(!(name = eel_v2s(vm->heap + vm->argv + 1)))
			return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, cs_LoadString(code, name));
	return 0;
}

static EEL_xno ecs_Load(EEL_vm *vm)
{
	const char *fn = eel_v2s(vm->heap + vm->argv);
	if(!fn)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, cs_Load(fn));
	return 0;
}

static EEL_xno ecs_FreeBank(EEL_vm *vm)
{
	cs_FreeBank(eel_v2l(vm->heap + vm->argv));
	return 0;
}


/*---------------------------------------------------------
	Waveform management
---------------------------------------------------------*/

static EEL_xno ecs_UploadWave(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *d = args + 4;
	const char *name = eel_v2s(args + 1);
	CS_sampleformats fmt;
	const void *data;
	unsigned size;
	if(!name || !EEL_IS_OBJREF(d->type))
		return EEL_XWRONGTYPE;
	size = eel_length(d->objref.v);
	data = eel_rawdata(d->objref.v);
	if(!data)
		return EEL_XWRONGTYPE;
	switch((EEL_classes)EEL_TYPE(d))
	{
	  case EEL_CSTRING:
	  case EEL_CDSTRING:
		fmt = CS_I8;
		data = eel_v2s(d);
		break;
	  case EEL_CVECTOR_S8:
		fmt = CS_I8;
		size = eel_length(d->objref.v);
		break;
	  case EEL_CVECTOR_S16:
		fmt = CS_I16;
		size *= 2;
		break;
	  case EEL_CVECTOR_S32:
		fmt = CS_I32;
		size *= 4;
		break;
	  case EEL_CVECTOR_F:
		fmt = CS_F32;
		size *= 4;
		break;
	  default:
		return EEL_XWRONGTYPE;
	}
	eel_l2v(vm->heap + vm->resv, cs_UploadWave(eel_v2l(args), name,
			eel_v2l(args + 2), eel_v2l(args + 3),
			fmt, data, size));
	return 0;
}


/*---------------------------------------------------------
	Finding exports
---------------------------------------------------------*/

static EEL_xno ecs_Find(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	const char *name = eel_v2s(args + 1);
	if(!name)
		return EEL_XWRONGTYPE;
	eel_l2v(vm->heap + vm->resv, cs_Find(eel_v2l(args), name));
	return 0;
}

static EEL_xno ecs_Get(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_Get(eel_v2l(args), eel_v2l(args + 1)));
	return 0;
}

static EEL_xno ecs_GetName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	const char *name = cs_GetName(eel_v2l(args), eel_v2l(args + 1));
	if(name)
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}

static EEL_xno ecs_GetType(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_GetType(eel_v2l(args)));
	return 0;
}

static EEL_xno ecs_TypeName(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	const char *name = cs_TypeName(eel_v2l(args));
	if(name)
		eel_s2v(vm, vm->heap + vm->resv, name);
	else
		eel_nil2v(vm->heap + vm->resv);
	return 0;
}


/*---------------------------------------------------------
	Playing and controlling
---------------------------------------------------------*/

static EEL_xno ecs_NewGroup(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_NewGroup(eel_v2l(args),
			eel_v2l(args + 1)));
	return 0;
}

static EEL_xno ecs_SendGroup(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int i, a[CS_MAXARGS];
	for(i = 0; i < vm->argc - 2; ++i)
		a[i] = eel_v2d(args + 2 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, cs_SendGroupa(eel_v2l(args),
			eel_v2l(args + 1), vm->argc - 2, a));
	return 0;
}

static EEL_xno ecs_DetachGroup(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_DetachGroup(eel_v2l(args)));
	return 0;
}

static EEL_xno ecs_Now(EEL_vm *vm)
{
	cs_Now();
	return 0;
}

static EEL_xno ecs_Wait(EEL_vm *vm)
{
	cs_Wait(eel_v2d(vm->heap + vm->argv));
	return 0;
}

static EEL_xno ecs_Start(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int i, a[CS_MAXARGS];
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, cs_Starta(eel_v2l(args), eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

static EEL_xno ecs_Send(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int i, a[CS_MAXARGS];
	for(i = 0; i < vm->argc - 3; ++i)
		a[i] = eel_v2d(args + 3 + i) * 65536.0f;
	eel_l2v(vm->heap + vm->resv, cs_Senda(eel_v2l(args), eel_v2l(args + 1),
			eel_v2l(args + 2), vm->argc - 3, a));
	return 0;
}

static EEL_xno ecs_Detach(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_Detach(eel_v2l(args), eel_v2l(args + 1)));
	return 0;
}

static EEL_xno ecs_Kill(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_Kill(eel_v2l(args), eel_v2l(args + 1)));
	return 0;
}


/*---------------------------------------------------------
	Utilities
---------------------------------------------------------*/

static EEL_xno ecs_F2P(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, cs_F2P(eel_v2d(args)));
	return 0;
}

static EEL_xno ecs_Rand(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_d2v(vm->heap + vm->resv, cs_Rand(eel_v2d(args)));
	return 0;
}


/*---------------------------------------------------------
	Object property interface
---------------------------------------------------------*/

static EEL_xno ecs_GetGlobalProp(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	eel_l2v(vm->heap + vm->resv, cs_GetGlobalProp(eel_v2l(args)));
	return 0;
}

static EEL_xno ecs_SetGlobalProp(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(cs_SetGlobalProp(eel_v2l(args), eel_v2l(args + 1)))
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno eel_chipsound_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close EELBox */
	if(closing)
	{
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp eel_chipsound_constants[] =
{
	/* Engine and API limits and information */
	{"MIDDLEC",		CS_MIDDLEC	},
	{"REGISTERS",		CS_REGISTERS	},
	{"MAXGIDS",		CS_MAXGIDS	},
	{"MAXVIDS",		CS_MAXVIDS	},
	{"MAXARGS",		CS_MAXARGS	},
	{"MAXEPS",		CS_MAXEPS	},
	{"MIPLEVELS",		CS_MIPLEVELS	},
	{"MAXFRAG",		CS_MAXFRAG	},
	{"MAXCHANNELS",		CS_MAXCHANNELS	},

	/* Object types */
	{"BANK",		CS_TBANK	},
	{"WAVE",		CS_TWAVE	},
	{"PROGRAM",		CS_TPROGRAM	},

	/* Flags for cs_Open() */
	{"MIDI",		CS_MIDI	},
	{"EXPORTALL",		CS_EXPORTALL	},
	{"TIMESTAMP",		CS_TIMESTAMP	},

	/* Flags for waveform uploading and rendering */
	{"LOOPED",		CS_LOOPED	},
	{"NORMALIZE",		CS_NORMALIZE	},
	{"WRAP",		CS_WRAP		},
	{"XFADE",		CS_XFADE	},
	{"REVMIX",		CS_REVMIX	},

	/* Properties: Voice control registers */
	{"PWAVE",		CS_PWAVE	},
	{"PPITCH",		CS_PPITCH	},
	{"PSPITCH",		CS_PSPITCH	},
	{"PAMPLITUDE",		CS_PAMPLITUDE	},
	{"PSAMPLITUDE",		CS_PSAMPLITUDE	},
	{"PTICK",		CS_PTICK	},

	/* Properties: Global settings and profiling information */
	{"PSAMPLERATE",		CS_PSAMPLERATE	},
	{"PBUFFER",		CS_PBUFFER	},
	{"PACTIVEVOICES",	CS_PACTIVEVOICES},
	{"PFREEVOICES",		CS_PFREEVOICES	},
	{"PTOTALVOICES",	CS_PTOTALVOICES	},
	{"PCPULOADAVG",		CS_PCPULOADAVG	},
	{"PCPULOADMAX",		CS_PCPULOADMAX	},
	{"PCPUTIMEAVG",		CS_PCPUTIMEAVG	},
	{"PCPUTIMEMAX",		CS_PCPUTIMEMAX	},
	{"PINSTRUCTIONS",	CS_PINSTRUCTIONS},
	{"PEXPORTALL",		CS_PEXPORTALL	},

	/* Properties: General properties */
	{"PCHANNELS",		CS_PCHANNELS	},
	{"PFLAGS",		CS_PFLAGS	},

	/* Properties: Wave properties */
	{"PLOOPED",		CS_PLOOPED	},

	{NULL, 0}
};


EEL_xno eel_chipsound_init(EEL_vm *vm)
{
	EEL_object *m;

	if(loaded)
		return EEL_XDEVICEOPENED;

	m = eel_create_module(vm, "ChipSound", eel_chipsound_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	/* Error handling */
	eel_export_cfunction(m, 1, "ErrorString", 1, 0, 0, ecs_ErrorString);

	/* Open/close */
	eel_export_cfunction(m, 1, "Open", 0, 4, 0, ecs_Open);
	eel_export_cfunction(m, 0, "Close", 0, 0, 0, ecs_Close);

	/* Loading/unloading sounds */
	eel_export_cfunction(m, 1, "NewBank", 1, 0, 0, ecs_NewBank);
	eel_export_cfunction(m, 1, "LoadString", 1, 1, 0, ecs_LoadString);
	eel_export_cfunction(m, 1, "Load", 1, 0, 0, ecs_Load);
	eel_export_cfunction(m, 0, "FreeBank", 1, 0, 0, ecs_FreeBank);

	/* Waveform management */
	eel_export_cfunction(m, 1, "UploadWave", 5, 0, 0, ecs_UploadWave);

	/* Finding exports */
	eel_export_cfunction(m, 1, "Find", 2, 0, 0, ecs_Find);
	eel_export_cfunction(m, 1, "Get", 2, 0, 0, ecs_Get);
	eel_export_cfunction(m, 1, "GetName", 2, 0, 0, ecs_GetName);
	eel_export_cfunction(m, 1, "GetType", 1, 0, 0, ecs_GetType);
	eel_export_cfunction(m, 1, "TypeName", 1, 0, 0, ecs_TypeName);

	/* Playing and controlling */
	eel_export_cfunction(m, 1, "NewGroup", 2, 0, 0, ecs_NewGroup);
	eel_export_cfunction(m, 1, "SendGroup", 2, 0, 1, ecs_SendGroup);
	eel_export_cfunction(m, 1, "DetachGroup", 1, 0, 0, ecs_DetachGroup);
	eel_export_cfunction(m, 0, "Now", 0, 0, 0, ecs_Now);
	eel_export_cfunction(m, 0, "Wait", 1, 0, 0, ecs_Wait);
	eel_export_cfunction(m, 1, "Start", 3, 0, 1, ecs_Start);
	eel_export_cfunction(m, 1, "Send", 3, 0, 1, ecs_Send);
	eel_export_cfunction(m, 1, "Detach", 2, 0, 0, ecs_Detach);
	eel_export_cfunction(m, 1, "Kill", 2, 0, 0, ecs_Kill);

	/* Utilities */
	eel_export_cfunction(m, 1, "F2P", 1, 0, 0, ecs_F2P);
	eel_export_cfunction(m, 1, "Rand", 1, 0, 0, ecs_Rand);

	/* Object property interface */
	eel_export_cfunction(m, 1, "GetGlobalProp", 1, 0, 0, ecs_GetGlobalProp);
	eel_export_cfunction(m, 0, "SetGlobalProp", 2, 0, 0, ecs_SetGlobalProp);

	/* Constants and enums */
	eel_export_lconstants(m, eel_chipsound_constants);

	loaded = 1;
	eel_disown(m);
	return 0;
}
