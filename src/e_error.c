/*(LGPLv2.1)
---------------------------------------------------------------------------
	e_error.c - EEL Compiler and VM Error Handling
---------------------------------------------------------------------------
 * Copyright (C) 2002-2006, 2008, 2010 David Olofson
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

#include <stdio.h>

#include "e_error.h"
#include "ec_symtab.h"
#include "ec_coder.h"
#include "e_state.h"
#include "e_util.h"
#include "e_function.h"
#include "e_string.h"
#include "e_table.h"

#ifndef va_copy
#  define       va_copy(x,y)    ((x) = (y))
#endif

struct EEL_emessage
{
	EEL_emessage	*next, *prev;
	char		*buffer;
	EEL_emtype	type;
};

EEL_DLLIST(eel_msg_, EEL_state, firstmsg, lastmsg, EEL_emessage, prev, next)

static const char *emtag(EEL_emtype t)
{
	switch(t)
	{
	  case EEL_EM_APPEND:	return NULL;
	  case EEL_EM_APPEND_NL:return NULL;
	  case EEL_EM_INFO:	return "";
	  case EEL_EM_CWARNING:	return "WARNING: ";
	  case EEL_EM_CERROR:	return "ERROR: ";
	  case EEL_EM_CDUMP:	return "";
	  case EEL_EM_VMWARNING:return "VM WARNING: ";
	  case EEL_EM_VMERROR:	return "VM ERROR: ";
	  case EEL_EM_VMDUMP:	return "";
	  case EEL_EM_SERROR:	return "SYSTEM ERROR: ";
	  case EEL_EM_IERROR:	return "INTERNAL ERROR: ";
	  case EEL_EM_FATAL:	return "FATAL ERROR: ";
	  case EEL_EM__COUNT:	break;
	}
	return "<ILLEGAL MESSAGE TYPE>";
}


static void eel__pmsg(EEL_state *es, EEL_emessage *em, const char *prefix)
{
	int i, si, oldc;
	int len = strlen(em->buffer);
	const char *tag = emtag(em->type);
	if(!tag)
		tag = "";
	for(si = i = 0; i < len + 1; ++i)
	{
		switch(em->buffer[i])
		{
		  case 0:
			i = len;
		  case '\n':
			if(si == i)
				break;
			oldc = em->buffer[i];
			em->buffer[i] = 0;
			fprintf(stderr, "%s%s%s\n", prefix,
					tag, em->buffer + si);
			em->buffer[i] = oldc;
			si = i + 1;
			break;
		}
	}
}


int eel_vmsg(EEL_state *es, EEL_emtype t, const char *format, va_list args)
{
	int count, bsize, start, append = 0;
	EEL_emessage *msg;
	if(!es)
	{
		/*
		 * Quick, dirty hack. Horrible formatting.
		 * Hopefully better than error messages
		 * vanishing in thin air, though.
		 */
		if(t <= EEL_EM_APPEND)
		{
			if(t == EEL_EM_APPEND_NL)
				fprintf(stderr, "\n");
			return vfprintf(stderr, format, args);
		}
		else
		{
			const char *tag = emtag(t);
			if(tag)
				count = fprintf(stderr, "(no state) %s", tag);
			else
				count = 0;
		}
		count += vfprintf(stderr, format, args);
		count += fprintf(stderr, "\n");
		return count;
	}
	switch(t)
	{
	  case EEL_EM_APPEND:
	  case EEL_EM_APPEND_NL:
		append = 1;
		msg = es->lastmsg;
		if(msg)
			break;
		eel_msg(es, EEL_EM_IERROR, "(Tried to append to "
				"non-existent error/warning message!)");
		eel_vmsg(es, EEL_EM_CERROR, format, args);
		return -1;
	  default:
		msg = (EEL_emessage *)calloc(1, sizeof(EEL_emessage));
		if(!msg)
			return -1;
		msg->type = t;
		break;
	}
	if(t == EEL_EM_APPEND_NL)
		if(msg->buffer[strlen(msg->buffer) - 1] != '\n')
			eel_msg(es, EEL_EM_APPEND, "\n");
	start = msg->buffer ? strlen(msg->buffer) : 0;
	bsize = start + 4;
	while(1)
	{
		va_list _args;
		char *nb = (char *)realloc(msg->buffer, bsize);
		if(!nb)
		{
			/* Don't kill old messages! */
			if(!append)
			{
				free(msg->buffer);
				free(msg);
			}
			fprintf(stderr, "FATAL ERROR: Could not log "
					"error/warning message!\n");
			return -1;
		}
		msg->buffer = nb;
		va_copy(_args, args);
		count = vsnprintf(msg->buffer + start, bsize - start,
				format, _args);
		if(count < 0)
			bsize *= 2;
		else if(start + count >= bsize)
			bsize = start + count + 1;
		else
			break;	/* Done! */
	}
	if(!append)
		eel_msg_link(es, msg);
	if((t == EEL_EM_FATAL) || (t == EEL_EM_IERROR))
		eel__pmsg(es, msg, "*** ");
#if (DBGZ(1+) DBGZ2(1+) 0) > 0
	else
		eel__pmsg(es, msg, "@@@ ");
#endif
	return count;
}


int eel_msg(EEL_state *es, EEL_emtype t, const char *format, ...)
{
	int count;
	va_list	args;
	va_start(args, format);
	count = eel_vmsg(es, t, format, args);
	va_end(args);
	return count;
}


/* Add message + current source position */
void eel_vmsg_src(EEL_state *es, EEL_emtype t,
		const char *format, va_list args)
{
	EEL_context *ctx = es->context;
	int spos, sline, scol;
	int epos, eline, ecol;
	if(!es)
	{
		const char *tag = emtag(t);
		if(tag)
			fprintf(stderr, "%s", tag);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		return;
	}
	if(!ctx || !ctx->module || !ctx->bio)
	{
		eel_vmsg(es, t, format, args);
		return;
	}

	DBGY2(eel_s_dump(es->vm, es->root_symtab, 1);)

	/* Figure out where we are */
/*
TODO: There should be an API that allows the parser to tell us
TODO: exactly which lexer item(s) the message is about. Just
TODO: store the values somewhere, together with the current ones,
TODO: and consider the hint invalid if the stored position doesn't
TODO: match the current.
*/
	spos = eel_lex_getpos(es, 2);
	if(spos < 0)
		spos = 0;
	epos = eel_lex_getpos(es, 0);
	if(epos < 0)
		epos = 0;
	eel_bio_linecount(ctx->bio, spos, EEL_TAB_SIZE, &sline, &scol);
	eel_bio_linecount(ctx->bio, epos, EEL_TAB_SIZE, &eline, &ecol);

	/* Position */
	if(ctx->module)
	{
		EEL_module *m = o2EEL_module(ctx->module);
		const char *fn = eel_module_filename(ctx->module);
		const char *mn = eel_module_modname(ctx->module);
		if(mn)
			eel_msg(es, t, "In module '%s' (\"%s\"),\n",
					mn, fn);
		else
			eel_msg(es, t, "In file \"%s\",\n", fn);
		es->last_module_id = m->id;
	}
	else
		eel_msg(es, t, "In some weird place (not a module),\n");
	if(sline == eline)
		eel_msg(es, -1, "  at line %d, in columns %d..%d:\n",
				sline, scol, ecol);
	else if(scol)
		eel_msg(es, -1, "  between line %d, column %d and "
				"line %d, column %d:\n",
				sline, scol, eline, ecol);
	else
		eel_msg(es, -1, "  near line %d, column %d:\n",
				eline, ecol);

	/* The actual message */
	eel_vmsg(es, -1, format, args);
}


/*
 * Find instruction that's occupying position 'pos' in 'f'.
 *
 * If 'line' is specified, this function also figures out
 * what source line this instruction originated from,
 * provided line number debug info is available.
 */
static int find_instruction(EEL_function *f, int pos, int *line)
{
	int i = 0;
	int pc = 0;
	if(pos < 0)
		pos = 0;
	if((f->common.flags & EEL_FF_CFUNC))
		return 0;
	if(pos >= f->e.codesize)
		pos = f->e.codesize - 1;
	while(1)
	{
		int size = eel_i_size(f->e.code[pc]);
		if(pc + size > pos)
			break;
		pc += size;
		++i;
		if(pc >= f->e.codesize)
		{
			pc = -1;
			break;
		}
	}
	if(line)
	{
		if(f->e.lines && (i < f->e.nlines))
			*line = f->e.lines[i];
		else
			*line = -1;
	}
	return pc;
}


/* Print message + current VM state, code listing etc */
static void vm_msg(EEL_vm *vm, EEL_emtype t,
		const char *format, va_list args, int statedump)
{
	int i, base, line, pc;
	EEL_state *es = VMP->state;
	EEL_callframe *cf = NULL;
	if(vm->base)
		cf = (EEL_callframe *)(vm->heap + vm->base - EEL_CFREGS);
	if(cf && cf->f && o2EEL_function(cf->f)->e.code)
	{
		EEL_function *f = o2EEL_function(cf->f);
		int cpc, last_line;

		/*
		 * The EEL VM has already advanced the PC
		 * when an exception is handled!
		 */
		cpc = find_instruction(f, vm->pc - 1, NULL);

		eel_msg(es, t, "At VM code position %d in function '%s',\n",
				cpc, eel_o2s(f->common.name));
		eel_msg(es, -1, "module \"%s\", file \"%s\":\n",
				eel_module_modname(f->common.module),
				eel_module_filename(f->common.module));
		switch(EEL_TYPE(&VMP->exception))
		{
		  case EEL_TINTEGER:
			eel_msg(es, -1, "Unhandled exception '%s'\n",
					eel_x_name(VMP->exception.integer.v));
			break;
		  case EEL_CSTRING:
			eel_msg(es, -1, "Unhandled exception \"%s\"\n",
					eel_v2s(&VMP->exception));
			break;
		  default:
		  {
			const char *xs = eel_v_stringrep(vm, &VMP->exception);
			eel_msg(es, -1, "Unhandled exception %s\n", xs);
			eel_sfree(es, xs);
			break;
		  }
		}
		eel_vmsg(es, -1, format, args);
		if(!statedump)
			return;

		if(cpc >= 0)
			eel_msg(es, EEL_EM_VMDUMP,
					"Registers:  PC:%6.1d  BASE:%6.1d\n",
					cpc, vm->base);
		else
			eel_msg(es, EEL_EM_VMDUMP,
					"Registers:  BASE:%6.1d\n", vm->base);

		eel_msg(es, -1, "Code:\n");
		if(cpc < 0)
		{
			eel_msg(es, -1, "   %6.1d: <Not available>\n", vm->pc);
			return;
		}

		/* Start 8 instructions back. */
		pc = cpc;
		for(i = 0; i < 8; ++i)
			pc = find_instruction(f, pc - 1, NULL);
 
		/* List 17 instructions, marking the one PC was at. */
		last_line = -1;
		for(i = 0; i < 17; ++i)
		{
			const char *tmp = eel_i_stringrep(es, cf->f, pc);
			line = -2;
			find_instruction(f, pc, &line);
			if((line >= 0) && (line != last_line))
			{
				eel_msg(es, -1, "[Line %d]\n", line);
				last_line = line;
			}
			if(pc == cpc)
			{
				eel_msg(es, -1, "==>%6.1d: %s\n", pc, tmp);
				i = 8;	/* Try to list 8 more. */
			}
			else
				eel_msg(es, -1, "   %6.1d: %s\n", pc, tmp);
			eel_sfree(es, tmp);
			pc += eel_i_size(f->e.code[pc]);
			if(pc >= f->e.codesize)
				break;
		}
	}
	else
	{
		if(cf && cf->f)
		{
			EEL_function *f = o2EEL_function(cf->f);
			eel_msg(es, t, "In function '%s':\n",
				eel_o2s(f->common.name));
		}
		else
			eel_msg(es, t, "In some place not a function:\n");
		eel_msg(es, -1, "Unhandled exception '%s'\n",
				(EEL_TINTEGER == VMP->exception.type) ?
				eel_x_name(VMP->exception.integer.v) :
				"<object>");
		eel_vmsg(es, -1, format, args);
		if(!statedump)
			return;

		eel_msg(es, -2, "Registers:  BASE:%6.1d\n", vm->base);
		eel_msg(es, -1, "(Code not available.)");
	}
	if(!cf || !cf->f)
		return;
	eel_msg(es, -2, "Call stack:\n");
	base = vm->base;
	pc = vm->pc;
	for(i = 0; i < 20; ++i)
	{
		EEL_function *f;
		char lbuf[32];
		if(!base)
		{
			eel_msg(es, -1, "    <bottom>\n");
			break;
		}
		cf = (EEL_callframe *)(vm->heap + base - EEL_CFREGS);
		f = o2EEL_function(cf->f);

		line = -1;
		find_instruction(f, pc, &line);
		if(line >= 0)
			snprintf(lbuf, sizeof(lbuf), " [Line %d]", line);

		eel_msg(es, -1, "%s %s()%s in module \"%s\", file \"%s\"\n",
				i ? "   " : "==>",
				eel_o2s(f->common.name),
				line >= 0 ? lbuf : "",
				eel_module_modname(f->common.module),
				eel_module_filename(f->common.module));

		base = cf->r_base;
		pc = cf->r_pc;
	}
}


void eel_cwarning(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg_src(es, EEL_EM_CWARNING, format, args);
	va_end(args);
}


void eel__cerror(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg_src(es, EEL_EM_CERROR, format, args);
	va_end(args);
	eel_cthrow(es);
}


void eel__serror(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg_src(es, EEL_EM_SERROR, format, args);
	va_end(args);
	eel_msg(es, EEL_EM_SERROR,
			"------------------------------------\n"
			"This is probably a result of system\n"
			"resources going low. If that's not a\n"
			"likely cause, please send a detailed\n"
			"bug report to the author of EEL at\n"
			"david@olofson.net\n"
			"------------------------------------");
	eel_cthrow(es);
}


void eel__ierror(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg_src(es, EEL_EM_IERROR, format, args);
	va_end(args);
	eel_msg(es, EEL_EM_IERROR,
			"------------------------------------\n"
			"Please send a detailed report to the\n"
			"author of EEL at david@olofson.net\n"
			"------------------------------------");
	DBGZ2(abort();)
	eel_cthrow(es);
}


void eel__ferror(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg_src(es, EEL_EM_FATAL, format, args);
	va_end(args);
	eel_msg(es, EEL_EM_FATAL,
			"------------------------------------\n"
			"Please send a detailed report to the\n"
			"author of EEL at david@olofson.net\n"
			"------------------------------------");
	abort();
}


void eel_info(EEL_state *es, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	eel_vmsg(es, EEL_EM_INFO, format, args);
	va_end(args);
}


void eel_vmdump(EEL_vm *vm, const char *format, ...)
{
	va_list	args;
	va_start(args, format);
	vm_msg(vm, EEL_EM_VMERROR, format, args, 1);
	va_end(args);
}


void eel_perror(EEL_vm *vm, int verbose)
{
	EEL_state *es;
	EEL_emessage *em;
	if(!vm)
	{
		fprintf(stderr, "eel_perror(): No VM! "
				"Can't print anything!\n");
		return;
	}
	es = VMP->state;
	if(!es)
	{
		fprintf(stderr, "eel_perror(): No state! "
				"Can't print anything!\n");
		return;
	}
	em = es->firstmsg;
	if(!verbose && !em)
		return;
	fprintf(stderr, ".----------------------------------"
			"----------------- -- -- - - -  -  -\n");
	if(!em)
		fprintf(stderr, "| EEL says: \"All is well!"
				" No errors, no warnings.\"\n"
				/*"| (So, why call eel_perror()?)\n"*/);
	for( ; em; em = em->next)
	{
		eel__pmsg(es, em, "| ");
		if(em->next)
			fprintf(stderr, "|--- -- - - -  -  -\n");
	}
	fprintf(stderr, "'----------------------------------"
			"----------------- -- -- - - -  -  -\n");
}


void eel_clear_errors(EEL_state *es)
{
	EEL_emessage *m = es->firstmsg;
	while(m)
	{
		EEL_emessage *nm = m->next;
		switch(m->type)
		{
		  default:
			eel_msg_unlink(es, m);
			free(m->buffer);
			free(m);
		  case EEL_EM_CWARNING:
		  case EEL_EM_VMWARNING:
			break;
		}
		m = nm;
	}
}


void eel_clear_warnings(EEL_state *es)
{
	EEL_emessage *m = es->firstmsg;
	while(m)
	{
		EEL_emessage *nm = m->next;
		switch(m->type)
		{
		  case EEL_EM_CWARNING:
		  case EEL_EM_VMWARNING:
			eel_msg_unlink(es, m);
			free(m->buffer);
			free(m);
		  default:
			break;
		}
		m = nm;
	}
}


void eel_clear_info(EEL_state *es)
{
	EEL_emessage *m = es->firstmsg;
	while(m)
	{
		EEL_emessage *nm = m->next;
		switch(m->type)
		{
		  case EEL_EM_INFO:
			eel_msg_unlink(es, m);
			free(m->buffer);
			free(m);
		  default:
			break;
		}
		m = nm;
	}
}
