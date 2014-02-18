/*
---------------------------------------------------------------------------
	eel_system.c - EEL system/platform information module
---------------------------------------------------------------------------
 * Copyright 2007, 2009, 2014 David Olofson
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

/*
TODO:
	* Try to get the executable path from various platform
	  specific APIs. Only use argv[0] as a last resort, as
	  there's no guarantee there actually is a path, or that
	  an empty path still gets you to where the executable
	  is once it's up running.
	
	* Get home path on platforms that support it, or some
	  sensible fallback (usually exepath) for others.

	* Get per-user configuration path where applicable.
	  exepath is probably the fallback as usual...

	* System wide configuration path...?
*/

#include <stdlib.h>
#include <string.h>
#include "eel_system.h"
#include "EEL_register.h"
#ifdef _WIN32
#include <windows.h>
#endif


/*
 * function getenv(name);
 *
 *	Returns the value (string), or nil, if no variable 'name' exists.
 */
static EEL_xno s_getenv(EEL_vm *vm)
{
	EEL_object *s;
	const char *value;
	const char *name = eel_v2s(vm->heap + vm->argv);
	if(!name)
		return EEL_XNEEDSTRING;
	if(!(value = getenv(name)))
	{
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	if(!(s = eel_ps_new(vm, value)))
		return EEL_XMEMORY;
	eel_o2v(vm->heap + vm->resv, s);
	return 0;
}


/*
 * function setenv(name, value)[overwrite];
 *
 *	Adds a variable 'name' and sets it to 'value', or if 'value' is nil,
 *	delete variable 'name' if it exists.
 *
 *	If 'overwrite' is true, an existing variable 'name' will be overwritten
 *	with 'value'. If 'overwrite' is false, no action is performed if a
 *	variable 'name' already exists.
 *
 *	'overwrite' must be true (or not specified) if 'value' is nil, because
 *	deleting a variable without overwriting doesn't make sense.
 *
 *	Returns true if a variable was added, overwritten or deleted.
 *
 * NOTE: 'overwrite' defaults to true if not specified!
 */
static EEL_xno s_setenv(EEL_vm *vm)
{
	EEL_value *arg = vm->heap + vm->argv;
#ifdef _WIN32
	char buf[1];
#endif
	const char *value;
	const char *name = eel_v2s(arg);
	int overwrite = vm->argc >= 3 ? eel_v2l(arg + 2) : 1;
	int added = 1;
	value = EEL_TYPE(arg + 1) == EEL_TNIL ? NULL : eel_v2s(arg + 1);
	if(!value && !overwrite)
		return EEL_XARGUMENTS;	/* Delete, but don't overwrite? o.O */
#ifdef _WIN32
	if(overwrite || (GetEnvironmentVariableA(name, buf, sizeof(buf) > 0))
		added = !SetEnvironmentVariableA(name, value ? value : NULL);
#else
	if(value)
		added = (setenv(name, value, overwrite) == 0);
	else
		added = (unsetenv(name) == 0);
#endif
	eel_b2v(vm->heap + vm->resv, added);
	return 0;
}


static EEL_xno s_unload(EEL_object *m, int closing)
{
	if(closing)
		return 0;
	else
		return EEL_XREFUSE;
}


EEL_xno eel_system_init(EEL_vm *vm, int argc, const char *argv[])
{
	EEL_object *m;
	const char dirsep[] = { EEL_DIRSEP, 0 };
	char *exepath = NULL;
	char *exename = NULL;
/* TODO:
#ifdef WIN32
	... = GetModuleFileName();
...
#else
*/
	if(argc)
	{
		char *s;
		char *buf = strdup(argv[0]);
		s = strrchr(buf, EEL_DIRSEP);
		if(s)
		{
			*s = 0;
			exepath = strdup(buf);
			exename = strdup(s + 1);
		}
		else
		{
			exepath = strdup("");
			exename = strdup(buf);
		}
		free(buf);
	}
	else
	{
		exepath = strdup("");
		exename = strdup("exename_unknown");
	}
	if(!exepath || !exename)
		return EEL_XMODULEINIT;

	m = eel_create_module(vm, "system", s_unload, NULL);
	if(!m)
	{
		free(exepath);
		free(exename);
		return EEL_XMODULEINIT;
	}

	eel_export_sconstant(m, "ARCH", EEL_ARCH);
	eel_export_sconstant(m, "SOEXT", EEL_SOEXT);
	eel_export_sconstant(m, "EXENAME", exename);
	eel_export_sconstant(m, "EXEPATH", exepath);
	eel_export_sconstant(m, "DIRSEP", dirsep);
#ifdef WIN32
/* FIXME: */	eel_export_sconstant(m, "CFGPATH", exepath);
/* FIXME: */	eel_export_sconstant(m, "HOMEPATH", exepath);
#elif defined MACOS
/* FIXME: */	eel_export_sconstant(m, "CFGPATH", exepath);
/* FIXME: */	eel_export_sconstant(m, "HOMEPATH", exepath);
#else
	eel_export_sconstant(m, "CFGPATH", "~");
	eel_export_sconstant(m, "HOMEPATH", "~");
#endif
	free(exepath);
	free(exename);

	eel_export_cfunction(m, 1, "getenv", 1, 0, 0, s_getenv);
	eel_export_cfunction(m, 1, "setenv", 2, 1, 0, s_setenv);

	eel_disown(m);
	return 0;
}
