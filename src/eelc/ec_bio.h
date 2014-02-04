/*(LGPLv2.1)
---------------------------------------------------------------------------
	ec_bio.h - EEL Binary I/O
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

#ifndef EEL_EC_BIO_H
#define EEL_EC_BIO_H

#include "EEL_xno.h"


typedef struct
{
	/* Buffer */
	unsigned char	*data;
	int		len;

	/* Current position */
	int		pos;

	/* Line count cache */
	int		lcc_pos;
	int		lcc_line;
	int		lcc_col;
} EEL_bio;


/*
 * Create an EEL_bio object for reading from
 * the buffer 'data', which should be 'len'
 * bytes long.
 *
 * Note that the bio object DOES NOT assume
 * ownership of the buffer!
 */
EEL_bio *eel_bio_open(void *data, int len);


/*
 * Create a copy of 'bio', initialized to the
 * current state of 'bio'.
 */
EEL_bio *eel_bio_clone(EEL_bio *bio);


/*
 * Destroy 'bio'.
 *
 * Note that since the bio object does not own
 * the buffer, the buffer is not affected.
 */
void eel_bio_close(EEL_bio *bio);


/* Return last read character, or -1 if we're at the start. */
static inline int eel_bio_last(EEL_bio *bio)
{
	if(bio->pos)
		return bio->data[bio->pos - 1];
	else
		return -1;
}


/* Get the current character from the buffer, and advance. */
static inline int eel_bio_getchar(EEL_bio *bio)
{
	if(bio->pos < bio->len)
		return bio->data[bio->pos++];
	else
		return -1;
}


/* Push last character back. No depth limit. */
static inline void eel_bio_ungetc(EEL_bio *bio)
{
	if(bio->pos > 0)
		--bio->pos;
}


/* Get the current source buffer position. */
static inline int eel_bio_tell(EEL_bio *bio)
{
	return bio->pos;
}


/*
 * Set the current buffer position;
 *	eel_bio_seek()		Relative to current position
 *	eel_bio_seek_set()	Relative to start of buffer
 *	eel_bio_seek_end()	Relative to end of buffer
 * Returns new current position.
 */
static inline int eel_bio_seek_set(EEL_bio *bio, int offset)
{
	bio->pos = offset;
	if(bio->pos < 0)
		bio->pos = 0;
	else if(bio->pos > bio->len)
		bio->pos = bio->len;
	return bio->pos;
}

static inline int eel_bio_seek(EEL_bio *bio, int offset)
{
	return eel_bio_seek_set(bio, bio->pos + offset);
}

static inline int eel_bio_seek_end(EEL_bio *bio, int offset)
{
	return eel_bio_seek_set(bio, bio->len - offset);
}


/*
 * Read 'count' bytes from the buffer,
 * starting at the current position, into
 * 'buf'. Returns the number of bytes
 * actually read.
 */
int eel_bio_read(EEL_bio *bio, char *buf, int count);


/*
 * Read and parse a double precision floating
 * point number from the buffer.
 *
 * SUCCESS: The read position is moved to the
 *          first character that is not part of
 *          the number and 0 or EEL_XREALNUMBER
 *          is returned.
 * FAILURE: The read position is unchanged and
 *          an error code is returned.
 */
EEL_xno eel_bio_read_num(EEL_bio *bio, double *v);


/*
 * Calculate line and column of position 'pos' in the
 * buffer. Tabs count as jumps to the nearest higher
 * columnar position with 'tab' granularity.
 *    The implementation caches the last position and
 * works forward from there when possible.
 *
 * SUCCESS: Returns 0 and sets *line and *col to line and
 *          the column of 'pos', respectively.
 * FAILURE: Returns a negative value. *line and *col are
 *          unchanged.
 */
int eel_bio_linecount(EEL_bio *bio, int pos, int tab,
		int *line, int *col);

#endif
