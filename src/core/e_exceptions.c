/*
---------------------------------------------------------------------------
	e_exceptions.c - EEL exception code registry
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
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
#include <stdlib.h>
#include <string.h>
#include "EEL_xno.h"
#include "e_exceptions.h"
#include "e_platform.h"


static EEL_mutex eel_x_mutex;


#define	EEL_DEFEX(x, y)	{ #x, y },
static const EEL_xentry e_core_xentries[] =
{
	{ "XOK", "<no exception>" },
	EEL_ALLEXCEPTIONS
};
#undef	EEL_DEFEX


static const EEL_xblock e_core_xblock =
{
	0,
	EEL__XCOUNT,
	e_core_xentries
};

static EEL_xblock **e_x_registry = NULL;


EEL_xno eel_x_open_registry(void)
{
	EEL_xno res;
	if((res = eel_mutex_open(&eel_x_mutex)))
		return res;
	e_x_registry = (EEL_xblock **)calloc(EEL_EXCEPTION_BLOCKS,
			sizeof(EEL_xblock *));
	if(!e_x_registry)
		return EEL_XMEMORY;
	e_x_registry[0] = (EEL_xblock *)&e_core_xblock;
	return EEL_XOK;
}


static void destroy_xblock(EEL_xblock *xb)
{
	int i;
	for(i = 0; i < xb->nentries; ++i)
	{
		if(xb->entries[i].name)
			free((void *)xb->entries[i].name);
		if(xb->entries[i].description)
			free((void *)xb->entries[i].description);
	}
	free(xb);
}


void eel_x_close_registry(void)
{
	int i;
	for(i = 1; i < EEL_EXCEPTION_BLOCKS; ++i)
		if(e_x_registry[i])
			destroy_xblock(e_x_registry[i]);
	free(e_x_registry);
	e_x_registry = NULL;
	eel_mutex_close(&eel_x_mutex);
}


static inline const EEL_xentry *get_xentry(EEL_vm *vm, EEL_xno x,
		const char **err)
{
	unsigned block = x >> EEL_EXCEPTION_BITS;
	unsigned entry = x & EEL_EXCEPTION_MAX;
	const EEL_xblock *xb;
	if(block >= EEL_EXCEPTION_BLOCKS)
	{
		*err = "<exception code out of range>";
		return NULL;
	}
	if(!(xb = e_x_registry[block]))
	{
		*err = "<undefined exception range>";
		return NULL;
	}
	if(entry >= xb->nentries)
	{
		*err = "<undefined exception>";
		return NULL;
	}
	return &xb->entries[entry];
}


const char *eel_x_name(EEL_vm *vm, EEL_xno x)
{
	const char *err;
	const EEL_xentry *xe = get_xentry(vm, x, &err);
	if(!xe)
		return err;
	if(xe->name)
		return xe->name;
	else
		return "<unnamed exception>";
}


const char *eel_x_description(EEL_vm *vm, EEL_xno x)
{
	const char *err;
	const EEL_xentry *xe = get_xentry(vm, x, &err);
	if(!xe)
		return err;
	if(xe->description)
		return xe->description;
	else
		return xe->name;
}


/*
 * Check if a block of exception codes is already registered.
 *
 * NOTE:
 *	If there are gaps in the block, we're just ignoring these (supposedly)
 *	unused entries, so we might actually accept a version of the block that
 *	has extra codes where the new one has gaps. Even if some module
 *	actually does remove entries, it's not going to break anything if
 *	they're still defined in the registry.
 */
static int already_registered(const EEL_xdef *xdefs, int min, int max,
		int count)
{
	int i;
	for(i = 1; i < EEL_EXCEPTION_BLOCKS; ++i)
	{
		int j;
		EEL_xblock *xb = e_x_registry[i];
		if(!xb)
			continue;	/* Nothing here! */
		if(xb->nentries != max - min + 1)
			continue;	/* Different number of entries! */
		if(xb->base != (i << EEL_EXCEPTION_BITS) - min)
			continue;	/* Different exception code base! */
		for(j = 0; j < count; ++j)
		{
			const EEL_xentry *xe = &xb->entries[j];
			if(strcmp(xe->name, xdefs[j].name) != 0)
				break;
			/* NOTE: We ignore the descriptions... */
		}
		if(j < count)
			continue;

		/* Everything matches! Return this one. */
		return i;
	}
	return 0;
}


int eel_x_register(EEL_vm *vm, const EEL_xdef *exceptions)
{
	EEL_xblock *xb;
	EEL_xentry *xe;
	int i;
	int count = 0;
	int min = 0x7fffffff;
	int max = 0x80000000;

	/* Count and verify the entries */
	while(exceptions[count].code || exceptions[count].name ||
			exceptions[count].description)
	{
		const EEL_xdef *xd = &exceptions[count];
		if(!xd->name)
		{
			fprintf(stderr, "eel_x_register(): No name specified "
					"for exception code %d! (%s)",
					xd->code,
					xd->description ? xd->description :
					"<no description>");
			return -EEL_XBADXCODE;
		}
		if((xd->code <= 0) || (xd->code > EEL_EXCEPTION_MAX))
		{
			fprintf(stderr, "eel_x_register(): Exception code %d "
					"is out of range! (%s; %s)\n",
					xd->code, xd->name,
					xd->description ? xd->description :
					"<no description>");
			return -EEL_XNEEDNAME;
		}
		if(xd->code < min)
			min = xd->code;
		if(xd->code > max)
			max = xd->code;
		++count;
	}
	if(!count)
	{
		fprintf(stderr, "eel_x_register(): No valid exception code "
				"entries found!\n");
		return -EEL_XFEWARGS;
	}
	if(max - min > EEL_EXCEPTION_MAX)
	{
		fprintf(stderr, "eel_x_register(): Exception code range %d..%d"
				" is too wide! (%s...)\n", min, max,
				exceptions->name);
		return -EEL_XWIDEXRANGE;
	}

	/* If an identical block is already registered, just return that! */
	if((i = already_registered(exceptions, min, max, count)))
		return e_x_registry[i]->base;

	/* Create and fill in exception block! */
	xb = (EEL_xblock *)calloc(1, sizeof(EEL_xblock));
	if(!xb)
		return -EEL_XMEMORY;
	xb->nentries = max - min + 1;
	xb->entries = xe = (EEL_xentry *)calloc(xb->nentries,
			sizeof(EEL_xentry));
	if(!xe)
	{
		free(xb);
		return -EEL_XMEMORY;
	}
	for(i = 0; i < count; ++i)
	{
		int ei = exceptions[i].code - min;
		if(exceptions[i].name)
		{
			xe[ei].name = strdup(exceptions[i].name);
			if(!xe[ei].name)
			{
				destroy_xblock(xb);
				return -EEL_XMEMORY;
			}
		}
		if(exceptions[i].description)
		{
			xe[ei].description = strdup(exceptions[i].description);
			if(!xe[ei].description)
			{
				destroy_xblock(xb);
				return -EEL_XMEMORY;
			}
		}
	}

	/* Install exception block! */
	eel_mutex_lock(&eel_x_mutex);
	for(i = 1; i < EEL_EXCEPTION_BLOCKS; ++i)
	{
		if(e_x_registry[i])
			continue;

		/* Set base, install, and return! */
		xb->base = (i << EEL_EXCEPTION_BITS) - min;
		e_x_registry[i] = xb;
		eel_mutex_unlock(&eel_x_mutex);
		return xb->base;
	}

	/* Oops... All blocks are in use! */
	eel_mutex_unlock(&eel_x_mutex);
	destroy_xblock(xb);
	return -EEL_XNOFREEBLOCKS;
}
