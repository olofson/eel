/*
---------------------------------------------------------------------------
	'eel' command line executive
---------------------------------------------------------------------------
 * Copyright 2005-2007, 2009-2012, 2014 David Olofson
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

#include "EEL.h"
#include "e_function.h"

#ifdef EEL_HAVE_EELBOX
# include "eelbox.h"
#endif

#define	READ_CHUNK_SIZE	256


/*----------------------------------------------------------
	main() with helpers
----------------------------------------------------------*/

static EEL_xno find_function(EEL_object *m, const char *name,
		EEL_object **fo)
{
	EEL_xno x;
	EEL_value f;
	if((x = eel_getsindex(m, name, &f)))
		return x;
	if((EEL_classes)f.objref.v->type != EEL_CFUNCTION)
		return EEL_XWRONGTYPE;
	*fo = f.objref.v;
	return EEL_XOK;
}


static EEL_xno args2args(EEL_object *fo, const char *sname,
		int argc, const char *argv[], int *resv)
{
	EEL_function *f = o2EEL_function(fo);
	EEL_xno x;
	int i;
	eel_argf(fo->vm, "*");
	if(f->common.results)
		if((x = eel_argf(fo->vm, "R", resv)))
			return x;
	if(f->common.reqargs || f->common.optargs || f->common.tupargs)
	{
		if((x = eel_argf(fo->vm, "s", sname)))
			return x;
		for(i = 0; i < argc; ++i)
			if( (x = eel_argf(fo->vm, "s", argv[i])) )
				return x;
	}
	else if(argc >= 1)
		return EEL_XMANYARGS;
	return EEL_XOK;
}


static void usage(const char *exename)
{
	EEL_uint32 v = eel_lib_version();
	fprintf(stderr, ".------------------------------------------------\n");
	fprintf(stderr, "| EEL (Extensible Embeddable Language) v%d.%d.%d\n",
			EEL_GET_MAJOR(v),
			EEL_GET_MINOR(v),
			EEL_GET_MICRO(v));
#ifdef EEL_HAVE_EELBOX
	fprintf(stderr, "| With EELBox (SDL, OpenGL, Audiality 2)\n");
#endif
	fprintf(stderr, "| Copyright 2005-2014 David Olofson\n");
	fprintf(stderr,	"|------------------------------------------------\n");
	fprintf(stderr, "| Usage: %s [switches] <file> [arguments]\n", 
			exename);
	fprintf(stderr, "| Switches:  -c        Compile only; don't run\n");
	fprintf(stderr, "|            -e        Fail on compiler warnings\n");
	fprintf(stderr, "|            -o <file> Write binary to \"file\"\n");
	fprintf(stderr, "|-           -l        List symbol tree\n");
	fprintf(stderr, "|--          -a        List VM assembly code\n");
	fprintf(stderr, "|---         -s        Read input from stdin\n");
	fprintf(stderr, "|----        -h        Help\n");
	fprintf(stderr, "'------------------------------------------------\n");
	exit(100);
}


/*
 * Read from 'f' until EOF, returning the buffer pointer and size through
 * 'buf' and 'len'. Returns 1 on success, 0 on failure.
 */
static int read_until_eof(FILE *f, char **buf, size_t *len)
{
	*len = 0;
	*buf = NULL;
	while(1)
	{
		size_t count;
		char *nb = realloc(*buf, *len + READ_CHUNK_SIZE);
		if(!nb)
			break;
		*buf = nb;
		count = fread(*buf + *len, 1, READ_CHUNK_SIZE, f);
		*len += count;
		if(count < READ_CHUNK_SIZE)
		{
			if(feof(f))
				return 1;
			break;
		}
	}
	/* Failure! */
	fprintf(stderr, "Could not read input!\n");
	free(*buf);
	*len = 0;
	*buf = NULL;
	return 0;
}


int main(int argc, const char *argv[])
{
	EEL_xno x;
	int result, i;
	EEL_object *m;
	EEL_vm *vm;
	int resv = -1;
	int flags = 0;
	int run = 1;
	int readstdin = 0;
#ifdef MAIN_AUTOSTART
	const char *defname = "main";
	const char *name = defname;
#else
	const char *name = NULL;
#endif
	const char *outname = NULL;
	int eelargc;
	const char **eelargv;

	fprintf(stderr,
		"\n"
		"#######################################################\n"
		"##                     WARNING!                      ##\n"
		"##    No operator precedence in EEL as of 0.3.7!     ##\n"
		"##   Expressions are evaluated from left to right.   ##\n"
		"## Order operations as intended, or use parentheses. ##\n"
		"#######################################################\n"
		"\n");

	for(i = 1; i < argc; ++i)
	{
		int j;
		if('-' != argv[i][0])
		{
			if(readstdin)
			{
				fprintf(stderr, "Specify file name or '-s' - "
						"not both!\n");
				usage(argv[0]);
			}
			name = argv[i];
			++i;
			break;	/* Pass the rest to the EEL script! */
		}
		for(j = 1; argv[i][j]; ++j)
			switch(argv[i][j])
			{
			  case 's':
				readstdin = 1;
				name = "<stdin>";
				break;
			  case 'c':
				run = 0;
				break;
			  case 'e':
				flags |= EEL_SF_WERROR;
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
					fprintf(stderr, "No output file "
							"name!\n");
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
	if(!name && !readstdin)
	{
		fprintf(stderr, "No source file name!\n");
		usage(argv[0]);
	}

	/* Init the EEL engine */
	vm = eel_open(argc, argv);
	if(!vm)
	{
		eel_perror(NULL, 1);
		fprintf(stderr, "Could not initialize EEL!\n");
		return 2;
	}

#ifdef EEL_HAVE_EELBOX
	if((x = eb_init_bindings(vm)))
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not initialize EELBox!\n");
		return 3;
	}
#endif
	/* Load the script */
	if(readstdin)
	{
		char *buf;
		size_t len;
		if(!read_until_eof(stdin, &buf, &len))
			return 4;
		m = eel_load_buffer(vm, buf, len, flags);
		free(buf);
	}
	else
		m = eel_load(vm, name, flags);
	if(!m)
	{
		eel_perror(vm, 1);
		fprintf(stderr, "Could not load script!\n");
		eel_close(vm);
		return 5;
	}

	if(run)
	{
		EEL_object *fo;
		if((x = find_function(m, "main", &fo)))
		{
			/*
			 * FIXME: We don't whine about there being no main<>
			 * when reading from stdin - but since that means any
			 * code is executing during module initialization, it
			 * all happens without initializing any subsystems!
			 */
			if(!readstdin)
				fprintf(stderr, "No main function found! "
					"(%s)\n", eel_x_name(vm, x));
			eel_disown(m);
			eel_close(vm);
			return 6;
		}

		/* Pass arguments and call the script's 'main' function */
		if((x = args2args(fo, name, eelargc, eelargv, &resv)))
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not pass arguments! (%s)\n",
					eel_x_name(vm, x));
			eel_disown(m);
			eel_close(vm);
			return 7;
		}

#ifdef EEL_HAVE_EELBOX
		if(eb_open_subsystems() < 0)
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Could not open subsystems!\n");
			eel_disown(m);
			eel_close(vm);
			return 8;
		}
#endif

		/* Run the script! */
		if((x = eel_call(vm, fo)))
		{
			eel_perror(vm, 1);
			fprintf(stderr, "Failure in main function! (%s)\n",
					eel_x_name(vm, x));
			eel_disown(m);
			eel_close(vm);
#ifdef EEL_HAVE_EELBOX
			eb_close_subsystems();
#endif
			return 9;
		}

		if(o2EEL_function(fo)->common.results)
		{
			/* Get the return value */
			result = eel_v2l(vm->heap + resv);
#ifdef DEBUG
			printf("Return value: %d (%s @ heap[%d])\n", result,
					eel_v_stringrep(vm, vm->heap + resv),
					resv);
#endif
		}
		else
			result = 0;
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
					eel_x_name(vm, x));
		}
	}

	eel_perror(vm, 0);

#ifdef MAIN_AUTOSTART
	if(result && name == defname)
	{
		fprintf(stderr, "Could not run default main program!\n");
		usage(argv[0]);
	}
#endif

	/* Clean up */
	if(m)
		eel_disown(m);
	eel_close(vm);
#ifdef EEL_HAVE_EELBOX
	eb_close_subsystems();
#endif
	return result;
}
