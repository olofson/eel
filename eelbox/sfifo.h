/*
------------------------------------------------------------
	SFIFO 1.4
------------------------------------------------------------
 * Simple portable lock-free FIFO
 * (c) 2000-2002, David Olofson
 *
 * Platform support:
 *	gcc / Linux / x86:		Works
 *	gcc / Linux / x86 kernel:	Works
 *	gcc / FreeBSD / x86:		Works
 *	gcc / NetBSD / x86:		Works
 *	gcc / Mac OS X / PPC:		Works
 *	gcc / Win32 / x86:		Works
 *	Borland C++ / DOS / x86RM:	Works
 *	Borland C++ / Win32 / x86PM16:	Untested
 *	? / Various Un*ces / ?:		Untested
 *	? / Mac OS / PPC:		Untested
 *	gcc / BeOS / x86:		Untested
 *	gcc / BeOS / PPC:		Untested
 *	? / ? / Alpha:			Untested
 *
 * 1.2:	* Max buffer size halved, to avoid problems with
 *	  the sign bit...
 *
 * 1.3:	* Critical buffer allocation bug fixed! For certain
 *	  requested buffer sizes, older version would
 *	  allocate a buffer of insufficient size, which
 *	  would result in memory thrashing. (Amazing that
 *	  I've manage to use this to the extent I have
 *	  without running into this... *heh*)
 *
 * 1.4: * Spinning read and write operations added:
 *		sfifo_read_spin()
 *		sfifo_read_user_spin()
 * 		sfifo_write_spin()
 *		sfifo_write_user_spin()
 *
 *	  NOTE: These calls actually *spin*!
 *		Yes, simple, "dirty", busy waiting.
 *
 *	  They are meant for situations where proper
 *	  blocking is not possible, or when the expected
 *	  average spinning time is so short that blocking
 *	  is not desirable.
 *
 *	  The former case refers to environments without
 *	  threads or processes, such as DOS applications
 *	  communicating with their ISRs. (That's actually
 *	  why these calls were implemented.)
 *
 *	  The latter would be special situations on SMP
 *	  systems, where you know you will never spin
 *	  waiting for a thread that's waiting for your
 *	  CPU. If you have one busy worker thread for
 *	  each CPU, the worker threads may communicate
 *	  this way, as long as the design implies that
 *	  they're not supposed to spin for significant
 *	  lengths of time during normal operation.
 *
 *	* Added 'volatile' to sfifo_atomic_t - just in
 *	  case someone actually finds a compiler that
 *	  manages to screw the synchronization up without
 *	  it...
 */

#ifndef	_SFIFO_H_
#define	_SFIFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

/*------------------------------------------------
	"Private" stuff
------------------------------------------------*/
/*
 * Porting note:
 *	Reads and writes of a variable of this type in memory
 *	must be *atomic*! 'int' is *not* atomic on all platforms.
 *	A safe type should be used, and  sfifo should limit the
 *	maximum buffer size accordingly.
 */
typedef volatile int sfifo_atomic_t;
#ifdef __TURBOC__
#	define	SFIFO_MAX_BUFFER_SIZE	0x7fff
#else /* Kludge: Assume 32 bit platform */
#	define	SFIFO_MAX_BUFFER_SIZE	0x7fffffff
#endif

typedef struct sfifo_t
{
	char *buffer;
	int size;			/* Number of bytes */
	sfifo_atomic_t readpos;		/* Read position */
	sfifo_atomic_t writepos;	/* Write position */
} sfifo_t;

#define SFIFO_SIZEMASK(x)	((x)->size - 1)


/*------------------------------------------------
	API
------------------------------------------------*/
int sfifo_init(sfifo_t *f, int size);
void sfifo_close(sfifo_t *f);
void sfifo_flush(sfifo_t *f);
int sfifo_write(sfifo_t *f, const void *buf, int len);
int sfifo_read(sfifo_t *f, void *buf, int len);
int sfifo_write_spin(sfifo_t *f, const void *buf, int len);
int sfifo_read_spin(sfifo_t *f, void *buf, int len);
#define sfifo_used(x)	(((x)->writepos - (x)->readpos) & SFIFO_SIZEMASK(x))
#define sfifo_space(x)	((x)->size - 1 - sfifo_used(x))


/*------------------------------------------------
	Linux kernel space interface
------------------------------------------------*/
#ifdef __KERNEL__
int sfifo_write_user(sfifo_t *f, const void *buf, int len);
int sfifo_read_user(sfifo_t *f, void *buf, int len);
int sfifo_write_user_spin(sfifo_t *f, const void *buf, int len);
int sfifo_read_user_spin(sfifo_t *f, void *buf, int len);
#else
#	define	sfifo_write_user	sfifo_write
#	define	sfifo_read_user		sfifo_read
#	define	sfifo_write_user_spin	sfifo_write_spin
#	define	sfifo_read_user_spin	sfifo_read_spin
#endif

#ifdef __cplusplus
};
#endif
#endif
