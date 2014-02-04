/*(PD)
---------------------------------------------------------------------------
	'eel' command line tool.
---------------------------------------------------------------------------
 * David Olofson 2004-2012
 *
 * This code is in the public domain. NO WARRANTY!
 */

#include <stdio.h>
#include <stdlib.h>
#include "EEL.h"

static int args2args(EEL_vm *vm, const char *sname,
		int argc, const char *argv[], int *resv)
{
	EEL_xno x;
	int i;
	x = eel_argf(vm, "*Rs", resv, sname);
	if(x)
		return x;
	for(i = 0; i < argc; ++i)
		if( (x = eel_argf(vm, "s", argv[i])) )
			return x;
	return 0;
}


static void usage(const char *exename)
{
	EEL_uint32 v = eel_lib_version();
	fprintf(stderr, "EEL (Extensible Embeddable Language) v%d.%d.%d\n",
			EEL_GET_MAJOR(v),
			EEL_GET_MINOR(v),
			EEL_GET_MICRO(v));
	fprintf(stderr, "Copyright (C) 2002-2012 David Olofson\n\n");
	fprintf(stderr, "Usage: %s [switches] <file> [arguments]\n\n", exename);
	fprintf(stderr, "Switches:  -c          Compile only; don't run\n");
	fprintf(stderr, "           -o <file>   Write binary to \"file\"\n");
	fprintf(stderr, "           -l          List symbol tree\n");
	fprintf(stderr, "           -a          List VM assembly code\n");
	fprintf(stderr, "           -s          Read input from stdin\n");
	fprintf(stderr, "           -h          Help\n\n");
	exit(1);
}


int main(int argc, const char *argv[])
{
	EEL_xno x;
	int result, i;
	EEL_vm *vm;
	EEL_object *m;
	int resv = -1;
	int flags = 0;
	int run = 1;
	const char *name = NULL;
	const char *outname = NULL;
	int eelargc;
	const char **eelargv;
	int readstdin = 0;

	for(i = 1; i < argc; ++i)
	{
		int j;
		if('-' != argv[i][0])
		{
			name = argv[i];
			++i;
			break;	/* Pass the rest to the EEL script! */
		}
		for(j = 1; argv[i][j]; ++j)
			switch(argv[i][j])
			{
			  case 's':
				readstdin = 1;
				break;
			  case 'c':
				run = 0;
				break;
			  case 'l':
				flags |= EEL_SF_LIST;
				break;
			  case 'a':
				flags |= EEL_SF_LISTASM;
				break;
			  case 'o':
				if(argc < i + 1)
				{
					fprintf(stderr, "No output file name!\n");
					usage(argv[0]);
				}
				++i;
				outname = argv[i];
				break;
			  case 'h':
			  default:
				usage(argv[0]);
			}
	}
	eelargv = argv + i;
	eelargc = argc - i;
	if(!name)
	{
		fprintf(stderr, "No source file name!\n");
		usage(argv[0]);
	}

	/* Init the EEL engine */
	vm = eel_open(argc, argv);
	if(!vm)
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not initialize EEL!\n");
		return 1;
	}

	/* Load the script */
	if(readstdin)
	{
		fprintf(stderr, "Not yet implemented!\n");
		return 1;
	}
	else
		m = eel_load(vm, name, flags);
	if(!m)
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not load script!\n");
		eel_close(vm);
		return 1;
	}

	if(run)
	{
		/* Pass arguments and call the script's 'main' function */
		x = args2args(vm, name, eelargc, eelargv, &resv);
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not pass arguments! (%s)\n",
					eel_x_name(x));
			eel_disown(m);
			eel_close(vm);
			return 1;
		}
		x = eel_calln(vm, m, "main");
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Failure in main function! (%s)\n",
					eel_x_name(x));
			eel_disown(m);
			eel_close(vm);
			return 1;
		}

		/* Get the return value */
		result = eel_v2l(vm->heap + resv);
#ifdef DEBUG
		printf("Return value: %d (%s @ heap[%d])\n", result,
				eel_v_stringrep(vm, vm->heap + resv), resv);
#endif
	}
	else
		result = 0;

	if(outname)
	{
		/* Serialize compiled module to file */
		x = eel_save(vm, m, outname, 0);
		if(x)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not save binary! (%s)\n",
					eel_x_name(x));
		}
	}

	/* In case there are warnings */
	eel_perror(vm, 0);

	/* Clean up */
	if(m)
		eel_disown(m);
	eel_close(vm);
	return result;
}
