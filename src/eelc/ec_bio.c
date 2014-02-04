/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_bio.c - EEL Binary I/O
---------------------------------------------------------------------------
 * Copyright (C) 2002-2006, 2009 David Olofson
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ec_bio.h"

EEL_bio *eel_bio_open(void *data, int len)
{
	EEL_bio *bio = malloc(sizeof(EEL_bio));
	if(!bio)
		return NULL;
	bio->data = (unsigned char *)data;
	bio->len = len;
	bio->pos = 0;
	bio->lcc_pos = len + 1;
	return bio;
}


EEL_bio *eel_bio_clone(EEL_bio *bio)
{
	EEL_bio *c = malloc(sizeof(EEL_bio));
	if(!c)
		return NULL;
	*c = *bio;
	return c;
}


void eel_bio_close(EEL_bio *bio)
{
	free(bio);
}


int eel_bio_read(EEL_bio *bio, char *buf, int count)
{
	if(bio->pos + count > bio->len)
		count = bio->len - bio->pos;
	memcpy(buf, bio->data + bio->pos, count);
	bio->pos += count;
	return count;
}


#define	IS_NUM(x)	(((x) >= '0') && ((x) <= '9'))

/* Translate current char to a figure */
static inline int getfigure(EEL_bio *bio)
{
	int c = eel_bio_last(bio);
	if(IS_NUM(c))
		return c - '0';
	if(c < 'A')	/* Includes EOF and errors! */
		return -2;
	if(c >= 'a')
		c -= 'a' - 'A';
	if(c > 'Z')
		return -3;
	return c - 'A' + 10;
}


/* Grab figures in range according to 'base'. */
static inline int parse_num(EEL_bio *bio, int base, double *v)
{
	int figures = 0;
	*v = 0.0;
	while(1)
	{
		int f = getfigure(bio);
		if((f < 0) || (f >= base))
			break;
		eel_bio_getchar(bio);
		*v *= (double)base;
		*v += (double)f;
		++figures;
	}
	return figures;
}


static inline EEL_xno read_num(EEL_bio *bio, double *v)
{
	int has_integer = 0;
	int has_dot = 0;
	int has_fraction = 0;
	double sign = 1.0;
	double i = 0.0;
	double f = 0.0;
	double e = 0.0;
	int base = 10;
	int c = eel_bio_last(bio);

	if(!IS_NUM(c) && (c != '.') /*&& (c != '-')*/)
		return EEL_XNONUMBER;
#if 0
	if('-' == c)
	{
		sign = -1.0;
		c = eel_bio_getchar(bio);
		if(c < 0)
			return EEL_XNONUMBER;
	}
#endif
	/* Decode base, if specified */
	if('0' == c)
	{
		has_integer = 1;
		c = eel_bio_getchar(bio);
		if(c < 0)
		{
			*v = 0.0;
			return 0;
		}
		switch(c)
		{
#if 0
TODO:
		  case 's':
			/* sexagesimal, 00..59 */
			base = 60;
			eel_bio_getchar(bio);
			break;
#endif
		  case 'v':
			/* vigesimal, 0..[jJ] */
			base = 20;
			eel_bio_getchar(bio);
			break;
		  case 'x':
			/* [sh]exadecimal, 0..[fF] */
			base = 16;
			eel_bio_getchar(bio);
			break;
		  case 'd':
			c = eel_bio_getchar(bio);
			if(c < 0)
				return EEL_XNONUMBER;
			if('d' == c)
			{
				/* duodecimal, 0..[bB] */
				base = 12;
				c = eel_bio_getchar(bio);
			}
			else
				/* decimal, 0..9 */
				base = 10;
			break;
		  case 'o':
			/* octal, 0..7 */
			base = 8;
			eel_bio_getchar(bio);
			break;
		  case 'q':
			/* quartal, 0..3 */
			base = 4;
			eel_bio_getchar(bio);
			break;
		  case 'b':
			/* binal/binary, 0..1 */
			base = 2;
			eel_bio_getchar(bio);
			break;
		  case 'n':
			if(eel_bio_getchar(bio) != '(')
				return EEL_XBADBASE;
			base = 0;
			while(1)
			{
				c = eel_bio_getchar(bio);
				if(')' == c)
				{
					eel_bio_getchar(bio);
					break;
				}
				if((c < 0) || !IS_NUM(c))
					return EEL_XBADBASE;
				base *= 10;
				base += c - '0';
			}
			if(base < 2)
				return EEL_XBADBASE;
			break;
		  default:
			/* Nope, let's hand it to parse_num() below */
			eel_bio_ungetc(bio);
			break;
		}
		if(base > 36)
			return EEL_XBIGBASE;
	}

	/* Integer part */
	if(eel_bio_last(bio) != '.')
	{
		int figures = parse_num(bio, base, &i);
		if(figures < (has_integer ? 0 : 1))
			return EEL_XBADINTEGER;
		has_integer = 1;
	}
	else
		has_dot = 1;

	/* Fraction part */
	if(eel_bio_last(bio) == '.')
	{
		int figures;
		has_dot = 1;
		eel_bio_getchar(bio);
		figures = parse_num(bio, base, &f);
		if(figures)
			f /= pow(base, figures);
		has_fraction = figures > 0;
	}

	if(has_dot)
		if(!(has_integer || has_fraction))
			return EEL_XNONUMBER;

	/* Exponent part */
	switch(eel_bio_last(bio))
	{
	  /*
	   * Some of these will not be available when using
	   * big base formats, but that just means they'll
	   * be eaten by parse_num() before we get here.
	   */
	  case 'e':
	  case 'E':
	  case 'p':
	  case 'P':
	  case '@':
	  {
		double esign = 1.0;
		c = eel_bio_getchar(bio);
		if(c < 0)
			return EEL_XBADEXPONENT;
		switch(c)
		{
		  case '-':
			esign = -1.0;
			/* Fall through! */
		  case '+':
			eel_bio_getchar(bio);
			break;
		}
		if(!parse_num(bio, base, &e))
			return EEL_XBADEXPONENT;
		e *= esign;
		break;
	  }
	}

	*v = sign * (i + f) * pow(base, e);
	return has_dot ? EEL_XREALNUMBER : 0;
}


EEL_xno eel_bio_read_num(EEL_bio *bio, double *v)
{
	int startpos = bio->pos;
	EEL_xno result = read_num(bio, v);
	if(result && (result != EEL_XREALNUMBER))
		eel_bio_seek_set(bio, startpos);
	else
		eel_bio_ungetc(bio);
	return result;
}


int eel_bio_linecount(EEL_bio *bio, int pos, int tab,
		int *line, int *col)
{
	int l, c, p;
	if(pos >= bio->lcc_pos)
	{
		if(pos > bio->len)
			return -1;
		l = bio->lcc_line;
		c = bio->lcc_col;
		p = bio->lcc_pos;
	}
	else
	{
		if(pos < 0)
			return -1;
		l = c = 1;
		p = 0;
	}
	while(p < pos)
	{
		int ch = bio->data[p++];
		switch(ch)
		{
		  case '\n':
			++l;
			c = 1;
			break;
		  case '\t':
			c += tab + 1;
			c -= c % tab;
			break;
		  default:
			/* .ess compressed newlines */
			if((ch >= 0) && (ch <= 8))
			{
				l += ch;
				c = 1;
			}
			if(ch >= ' ')
				++c;
			break;
		}
	}
	*line = bio->lcc_line = l;
	*col = bio->lcc_col = c;
	bio->lcc_pos = p;
	return 0;
}
