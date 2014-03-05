/*
---------------------------------------------------------------------------
	ec_lexer.c - EEL lexer
---------------------------------------------------------------------------
 * Copyright 2002-2006, 2010, 2012 David Olofson
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ec_lexer.h"
#include "ec_symtab.h"
#include "e_state.h"
#include "e_string.h"
#include "e_object.h"

/*----------------------------------------------------------
	"Lexer Value"
----------------------------------------------------------*/

static void eel_lval_cleanup(EEL_lval *lval)
{
	switch(lval->type)
	{
	  case ELVT_NONE:
	  case ELVT_REAL:
	  case ELVT_INTEGER:
		break;
	  case ELVT_STRING:
		free(lval->v.s.buf);
		lval->v.s.buf = NULL;
		lval->v.s.len = 0;
		break;
	  case ELVT_SYMREF:
		break;
	}
	lval->type = ELVT_NONE;
}


void eel_lval_copy_string(EEL_lval *lval, const char *s, int len)
{
	if(lval->type == ELVT_STRING)
		free(lval->v.s.buf);
	lval->type = ELVT_STRING;
	lval->v.s.buf = malloc(len + 1);
	memcpy(lval->v.s.buf, s, len);
	lval->v.s.buf[len] = 0;
	lval->v.s.len = len;
}


/*----------------------------------------------------------
	Lexer
----------------------------------------------------------*/

static inline int lex_getchar(EEL_state *es)
{
	EEL_bio *bio = es->context->bio;
	int c = eel_bio_getchar(bio);
	if(c >= ' ')
		return c;
	if(c < 0)
		return c;	/* EOF */
	switch(c)
	{
	  case 1:
	  case 2:
	  case 3:
	  case 4:
	  case 5:
	  case 6:
	  case 7:
	  case 8:
		c = '\n';
		break;
	  case '\n':
	  case '\r':
	  case '\t':
		break;
	  default:
		eel_cerror(es, "Illegal character! Binary file?");
	}
	return c;
}

static void lexitem_cleanup(EEL_lexitem *li)
{
	eel_lval_cleanup(&li->lval);
	li->token = TK_EOF;
	li->pos = -1;
}


static void lexbuf_realloc(EEL_state *es, int len)
{
	char *nsb;
	es->lexbuf_length = len * 5 / 4;
	nsb = (char *)realloc(es->lexbuf, es->lexbuf_length + 1);
	if(!nsb)
		eel_serror(es, "EEL lexer out of memory!");
	es->lexbuf = nsb;
}


static inline void lexbuf_bump(EEL_state *es, int len)
{
	if(len >= es->lexbuf_length)
		lexbuf_realloc(es, len);
}


/*
 * Ignore whitespace, including comments, and return the first nonwhite
 * character. If 'report_eoln' is 0, EOLN is treated as whitespace, whereas if
 * 'report_eoln' is 0, EOLN is returned as a nonwhite character.
 */
static int skipwhite(EEL_state *es, int report_eoln)
{
	while(1)
	{
		int c = lex_getchar(es);
		switch(c)
		{
		  case ' ':
		  case '\t':
		  case '\r':
			continue;
		  case 1:
		  case 2:
		  case 3:
		  case 4:
		  case 5:
		  case 6:
		  case 7:
		  case 8:
		  case '\n':
			if(report_eoln)
				return '\n';
			continue;
		  case '/':
			switch((c = lex_getchar(es)))
			{
			  case '/':	/* C++ style single line comment */
				while((c = lex_getchar(es)) >= 0)
					if(c == '\n')
						break;
				continue;
			  case '*':	/* C style comment */
			  {
				int prevc = 0;
				while((c = lex_getchar(es)) >= 0)
				{
					if((prevc == '*') && (c == '/'))
						break;
					else
						prevc = c;
				}
				continue;
			  }
			  default:
				eel_bio_ungetc(es->context->bio);
				return '/';
			}
		}
		return c;
	}
}


static inline int get_figure(EEL_state *es, int base)
{
	int n = lex_getchar(es);
	if(n >= '0' && n <= '9')
		n -= '0';
	else if(n >= 'a' && n <= 'z')
/* FIXME: This can't be right! */
		n -= 'a';
	else if(n >= 'A' && n <= 'Z')
		n -= 'A';
	else
		return -1;
	if(n >= base)
		return -1;
	return n;
}


/* Simple base-n integer number parser for string escapes */
static int get_num(EEL_state *es, int base, int figures)
{
	int value = 0;
	while(figures--)
	{
		int n = get_figure(es, base);
		if(n < 0)
			return n;
		value *= base;
		value += n;
	}
	return value;
}


/*
 * Parse quoted string.
 *
 * C printf "backslash codes" are supported,
 * as well as "\dXXX", for decimal codes.
 *
 * Strings can span multiple lines, either
 * by using "inline" newlines (\n, \r and \t
 * are filtered out!), or by unquoting, adding
 * the desired whitespace characters, and then
 * beginning a new quoted string (C style).
 *
 * A compiler warning is issued if a string is
 * continued using the C style, unless there is
 * at least one newline character in the "hole"
 * between two parts of a string.
 */
static int parse_string(EEL_state *es, int delim)
{
	int c, len = 0;
	while(1)
	{
		c = lex_getchar(es);
		switch(c)
		{
		  case -1:
			if(delim == '"')
				eel_cerror(es, "End of file inside string"
						" literal!");
			else
				eel_cerror(es, "End of file inside character"
						" literal!");
		  case '\\':
			c = lex_getchar(es);
			switch(c)
			{
			  case -1:
				eel_cerror(es, "End of file inside string "
						"escape sequence!");
			  case '0':
			  case '1':
			  case '2':
			  case '3':
				c = get_num(es, 8, 2);
				if(c < 0)
					eel_cerror(es, "Illegal octal number!");
				break;
				c += 64 * (c - '0');
				break;
			  case 'a':
				c = '\a';
				break;
			  case 'b':
				c = '\b';
				break;
			  case 'c':
				c = '\0';
				break;
			  case 'd':
				c = get_num(es, 10, 2);
				if(c < 0)
					eel_cerror(es, "Illegal decimal number!");
				break;
			  case 'f':
				c = '\f';
				break;
			  case 'n':
				c = '\n';
				break;
			  case 'r':
				c = '\r';
				break;
			  case 't':
				c = '\t';
				break;
			  case 'v':
				c = '\v';
				break;
			  case 'x':
				c = get_num(es, 16, 2);
				if(c < 0)
					eel_cerror(es, "Illegal hex number!");
				break;
			  default:
				lexbuf_bump(es, len + 1);
				es->lexbuf[len++] = c;
				continue;
			}
			break;
		  case '\n':
		  case '\r':
		  case '\t':
			continue;
		  default:
			break;
		}
		if(c == delim)
		{
			/* Skip whitespace, looking for newlines */
			int got_nl = 0;
			while(1)
			{
				c = skipwhite(es, 1);
				if(c == '\n')
					got_nl = 1;
				else
					break;
			}
			if(c != delim)
			{
				/* Nope, no continuation --> */
				eel_bio_ungetc(es->context->bio);
				break;
			}
			/*
			 * C style string continuation! Let's throw
			 * a warning if this is done without a newline.
			 * (That's usually a "missing separator" typo.)
			 */
			if(!got_nl)
				eel_cwarning(es, "C style string continuation"
						" with no newline! Typo?");
			continue;
		}
		lexbuf_bump(es, len + 1);
		es->lexbuf[len++] = c;
	}
	eel_lval_copy_string(&es->lval, es->lexbuf, len);
	return TK_STRING;
}


/*
 * Grab a "word" of valid operator characters.
 */
static int grab_operator(EEL_state *es, int c)
{
	int len = 0;
	while(1)
	{
		if(len > 16)
			eel_cerror(es, "That's a pretty long"
					" operator token...!");
		switch(c)
		{
		  case '!':
		  case '#':
		  case '%':
		  case '&':
		  case '*':
		  case '+':
		  case '-':
		  case '/':
		  case ':':
		  case '<':
		  case '=':
		  case '>':
		  case '?':
		  case '@':
		  case '^':
		  case '|':
		  case '~':
			es->lexbuf[len++] = c;
			break;
		  default:
			if(len)
				eel_bio_ungetc(es->context->bio);
			es->lexbuf[len] = 0;
			return len;
		}
		c = lex_getchar(es);
	}
}


/* Check if we got a (=) (weak assignment) operator. */
static int check_weakassign(EEL_state *es, int c)
{
	if(c == '(')
	{
		if(lex_getchar(es) == '=')
		{
			if(lex_getchar(es) == ')')
			{
				es->lexbuf[0] = '(';
				es->lexbuf[1] = '=';
				es->lexbuf[2] = ')';
				es->lexbuf[3] = 0;
				return 1;
			}
			eel_bio_ungetc(es->context->bio);
		}
		eel_bio_ungetc(es->context->bio);
		es->lexbuf[0] = 0;
	}
	return 0;
}


static inline int token(EEL_state *es, int tk, int exit_no)
{
#if DBGT(1)+0 == 1
	const char *s = "";
	switch(es->lval.type)
	{
	  case 	ELVT_NONE:
		break;
	  case	ELVT_REAL:
		break;
	  case	ELVT_INTEGER:
		break;
	  case	ELVT_STRING:
		s = es->lval.v.s.buf;
		break;
	  case	ELVT_SYMREF:
		s = eel_s_stringrep(es->vm, es->lval.v.symbol);
		break;
	}
	if((tk >= 32) && (tk < 256))
		printf("################ [token '%c'/<%s> at exit #%d]\n",
				tk, s, exit_no);
	else
		printf("################ [token %d/<%s> at exit #%d]\n",
				tk, s, exit_no);
#endif
	es->token = tk;
	return tk;
}


int eel_lex(EEL_state *es, int flags)
{
	EEL_finder f;
	EEL_symbol *sym;
	EEL_symbol *st = es->context->symtab;
	int c;
	EEL_bio *bio = es->context->bio;

	/* Push current state for unlex() */
	lexitem_cleanup(&es->ls[1]);	/* Drop oldest item */
	es->ls[1] = es->ls[0];		/* Shift */
	es->ls[0].lval = es->lval;	/* Store current state */
	es->ls[0].token = es->token;
	es->ls[0].pos = eel_bio_tell(bio);
	es->lval.type = ELVT_NONE;	/* Init new state */

	if(flags & ELF_NO_SKIPWHITE)
		c = lex_getchar(es);
	else
		c = skipwhite(es, (flags & ELF_REPORT_EOLN));
	if(c < 0)
		return token(es, TK_EOF, 1);

	/* Pre-tokenized keyword? (.ess files) */
	if((c >= ESS_TOKEN_BASE) && (c < ESS_TOKEN_BASE + ESS_TOKENS))
	{
		int tk;
		sym = es->tokentab[c - ESS_TOKEN_BASE];
		if(!sym)
			eel_ierror(es, "Undefined ESS token %d!", c);
//printf("%d ==> %p\t(%d, %d)\n", c, sym, ESS_TOKEN_BASE, ESS_TOKENS);
		switch(sym->type)
		{
		  case EEL_SKEYWORD:	tk = sym->v.token; break;
		  case EEL_SCLASS:	tk = TK_SYM_CLASS; break;
		  case EEL_SOPERATOR:	tk = TK_SYM_OPERATOR; break;
		  default:
			eel_ierror(es, "ESS token %d gave unsupported symbol "
					"type!", c);
		}
		/*
		 * Instead of having the strip tool understand context properly,
		 * we just take the fact that it occasionally tokenizes field
		 * names and stuff, and turn them back into TK_NAMEs here!
		 */
		if(flags & ELF_LOCALS_ONLY)
		{
			const char *n = eel_o2s(sym->name);
			eel_lval_copy_string(&es->lval, n, strlen(n));
			return token(es, TK_NAME, 2);
		}
		es->lval.type = ELVT_SYMREF;
		es->lval.v.symbol = sym;
		return token(es, tk, 3);
	}

	if((flags & ELF_CHARACTERS) && (c > ' ') && (c <= 127))
		return token(es, c, 4);

	/*
	 * (Multi)character literal (cast to integer)
	 * Note that the endian is fixed (the last byte
	 * always ends up in the lowest bits), so it's
	 * safe to use for "FourCC" identifiers and the
	 * like.
	 */
	if(c == '\'')
	{
		int i, val = 0;
		parse_string(es, '\'');
		if(es->lval.v.s.len > sizeof(EEL_integer))
			eel_cerror(es, "Character literal too long for the "
					"integer type!");
		for(i = 0; i < es->lval.v.s.len; ++i)
		{
			c = es->lval.v.s.buf[i];
			val <<= 8;
			val |= c;
		}
		free(es->lval.v.s.buf);
		es->lval.type = ELVT_INTEGER;
		es->lval.v.i = val;
		return token(es, TK_INUM, 5);
	}

	/* String literal */
	if(c == '"')
		return token(es, parse_string(es, '"'), 6);

	/* Char starts an identifier or keyword => read the name. */
	while(isalpha(c) || (c == '_') || (c == '$'))
	{
		int len = 1;
		int start = eel_bio_tell(bio) - 1;
		while((isalnum(c = lex_getchar(es)) ||
				(c == '_')) && (c >= 0))
			++len;
		lexbuf_bump(es, len);
		eel_bio_seek_set(bio, start);
		eel_bio_read(bio, es->lexbuf, len);
		es->lexbuf[len] = '\0';

		eel_finder_init(es, &f, st, ESTF_NAME | ESTF_TYPES |
				((flags & ELF_LOCALS_ONLY) ? 0 : ESTF_UP));
		f.name = eel_ps_new(es->vm, es->lexbuf);
		f.types =	EEL_SMKEYWORD |
				EEL_SMVARIABLE |
				EEL_SMUPVALUE |
				EEL_SMBODY |
				EEL_SMNAMESPACE |
				EEL_SMCONSTANT |
				EEL_SMCLASS |
				EEL_SMFUNCTION |
				EEL_SMOPERATOR;
		sym = eel_finder_go(&f);
		eel_o_disown_nz(f.name);
		if(sym)
		{
			int tk;
			es->lval.type = ELVT_SYMREF;
			es->lval.v.symbol = sym;
			switch(sym->type)
			{
			  case EEL_SKEYWORD:	tk = sym->v.token; break;
			  case EEL_SVARIABLE:	tk = TK_SYM_VARIABLE; break;
			  case EEL_SUPVALUE:	tk = TK_SYM_VARIABLE; break;
			  case EEL_SBODY:	tk = TK_SYM_BODY; break;
			  case EEL_SNAMESPACE:
			  {
				c = skipwhite(es, (flags & ELF_REPORT_EOLN));
				if(c != '.')
				{
					eel_bio_ungetc(bio);
					tk = TK_SYMBOL;
					break;
				}
				st = sym;
				flags |= ELF_LOCALS_ONLY;
				c = skipwhite(es, (flags & ELF_REPORT_EOLN));
				continue;
			  }
			  case EEL_SCONSTANT:	tk = TK_SYM_CONSTANT; break;
			  case EEL_SCLASS:	tk = TK_SYM_CLASS; break;
			  case EEL_SFUNCTION:	tk = TK_SYM_FUNCTION; break;
			  case EEL_SOPERATOR:	tk = TK_SYM_OPERATOR; break;
			  default:		tk = TK_SYMBOL; break;
			}
			return token(es, tk, 7);
		}
		else
		{
			eel_lval_copy_string(&es->lval, es->lexbuf, len);
			return token(es, TK_NAME, 8);
		}
	}

	/* Look for operators */
	if(!(flags & ELF_NO_OPERATORS))
	{
/*
FIXME: In order to safely support buffered file I/O (as opposed to the current
FIXME: "load the whole file right away" approach), we'll need to specify how
FIXME: far back data must be kept!
*/
		int pos = eel_bio_tell(bio);
		int removed_eq = 0;
		int len;
		if(check_weakassign(es, c))
			return token(es, TK_WEAKASSIGN, 9);
		len = grab_operator(es, c);
		while(len)
		{
			eel_finder_init(es, &f, st, ESTF_NAME | ESTF_TYPES |
					((flags & ELF_LOCALS_ONLY) ? 0
					: ESTF_UP));
			f.name = eel_ps_new(es->vm, es->lexbuf);
			f.types = EEL_SMOPERATOR;
			sym = eel_finder_go(&f);
			eel_o_disown_nz(f.name);
			if(sym)
			{
				es->lval.type = ELVT_SYMREF;
				es->lval.v.symbol = sym;
				if(sym->v.op->flags & EOPF_NOSHORT)
					return token(es, TK_SYM_OPERATOR, 10);
				else if(removed_eq)
				{
					lex_getchar(es);
					return token(es, TK_SYM_SHORTOP, 11);
				}
				else
					return token(es, TK_SYM_OPERATOR, 12);
			}
			--len;
			removed_eq = (es->lexbuf[len] == '=');
			es->lexbuf[len] = 0;
			eel_bio_ungetc(bio);
		}
		eel_bio_seek_set(bio, pos);
	}

	/* Look for numeric literal */
	switch(eel_bio_read_num(bio, &es->lval.v.r))
	{
	  case 0:
	  {
		/* See if we can interpret this as an integer */
		EEL_real fr = (EEL_integer)(es->lval.v.r);
#if 1
		EEL_real ufr = (EEL_uinteger)(es->lval.v.r);
#endif
		if(fr == es->lval.v.r)
		{
			es->lval.type = ELVT_INTEGER;
			es->lval.v.i = (EEL_int32)fr;
			return token(es, TK_INUM, 21);
		}
#if 1
		else if(ufr == es->lval.v.r)
		{
			es->lval.type = ELVT_INTEGER;
			es->lval.v.i = (EEL_uint32)ufr;
			return token(es, TK_INUM, 22);
		}
#endif
		/* else
			fall through */
	  }
	  case EEL_XREALNUMBER:
		es->lval.type = ELVT_REAL;
		return token(es, TK_RNUM, 14);
	  case EEL_XNONUMBER:
		c = eel_bio_last(bio);
		break;
	  case EEL_XBADBASE:
		eel_cerror(es, "Bad base syntax in numeric literal!");
	  case EEL_XBIGBASE:
		eel_cerror(es, "Too big base in numeric literal!");
	  case EEL_XBADINTEGER:
		eel_cerror(es, "Bad integer part in numeric literal!");
	  case EEL_XBADFRACTION:
		eel_cerror(es, "Bad fractional part in numeric literal!");
	  case EEL_XBADEXPONENT:
		eel_cerror(es, "Bad exponent in numeric literal!");
	  default:
		eel_cerror(es, "Unknown error parsing numeric literal!");
	}

	/* Return character as token */
	return token(es, c, 15);
}


void eel_unlex(EEL_state *es)
{
	DBGT(printf("UNLEX <<<<<<<<<<<<\n");)
	if(es->ls[0].pos < 0)
		eel_ierror(es, " Too deep unlex()ing!");

	/* Ditch the current state */
	eel_lval_cleanup(&es->lval);

	/* Restore state */
	eel_bio_seek_set(es->context->bio, es->ls[0].pos);
	es->lval = es->ls[0].lval;
	es->token = es->ls[0].token;

	/* Shift */
	es->ls[0] = es->ls[1];

	/* Init new "oldest" item */
	es->ls[1].lval.type = ELVT_NONE;
	es->ls[1].token = TK_EOF;
	es->ls[1].pos = -1;
}


int eel_relex(EEL_state *es, int flags)
{
	eel_unlex(es);
	return eel_lex(es, flags);
}


int eel_lexer_init(EEL_state *es)
{
	lexbuf_bump(es, 128);
	if(!es->lexbuf)
		return -1;
	eel_lexer_invalidate(es);
	return 0;
}


void eel_lexer_cleanup(EEL_state *es)
{
	eel_lexer_invalidate(es);
	free(es->lexbuf);
	es->lexbuf = NULL;
	es->lexbuf_length = 0;
}


int eel_lexer_start(EEL_state *es, EEL_object *mo)
{
	EEL_module *m;
	if((EEL_classes)mo->type != EEL_CMODULE)
		eel_ierror(es, "Object is not a module!");
	m = o2EEL_module(mo);

	/* Get the source code */
	es->context->module = mo;
	if(es->context->bio && (es->context->flags & ECTX_OWNS_BIO))
		eel_bio_close(es->context->bio);
	es->context->bio = eel_bio_open(m->source, m->len);
	if(!es->context->bio)
		eel_ierror(es, "Could not create bio object for reading script!");
	es->context->flags |= ECTX_OWNS_BIO;

	/* Bootstrap the parser */
	eel_lexer_invalidate(es);
	eel_lex(es, 0);
	return 0;
}


void eel_lexer_invalidate(EEL_state *es)
{
	/* Invalidate the lexer stack! */
	eel_lval_cleanup(&es->lval);
	lexitem_cleanup(&es->ls[0]);
	lexitem_cleanup(&es->ls[1]);
}


int eel_lex_getpos(EEL_state *es, int item)
{
	switch(item)
	{
	  case 0:
		return eel_bio_tell(es->context->bio);
	  case 1:
	  case 2:
		return es->ls[item - 1].pos;
	  default:
		return -2;
	}
}
