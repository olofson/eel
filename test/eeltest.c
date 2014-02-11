/*
---------------------------------------------------------------------------
	Minimal client that loads and runs a test script.
---------------------------------------------------------------------------
 * David Olofson 2002, 2004, 2005, 2007, 2014
 *
 * This code is in the public domain. NO WARRANTY!
 */

#include <stdio.h>
#include "EEL.h"

int main(int argc, const char *argv[])
{
	EEL_object *m;
	EEL_vm *vm = eel_open(argc, argv);
	if(!vm)
	{
		fprintf(stderr, "Could not initialize EEL!\n");
		return 1;
	}
	m = eel_load(vm, "test.eel", 0);
	if(!m)
	{
		fprintf(stderr, "Could not load script!\n");
		return 1;
	}
	eel_callnf(vm, m, "main", "*");
	eel_disown(m);
	eel_close(vm);
	return 0;
}
