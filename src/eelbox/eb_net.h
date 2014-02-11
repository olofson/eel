/*
---------------------------------------------------------------------------
	eb_net.h - EEL SDL_net Binding
---------------------------------------------------------------------------
 * Copyright 2005, 2007, 2009 David Olofson
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

#ifndef EELBOX_NET_H
#define EELBOX_NET_H

#include "EEL.h"
#include "SDL_thread.h"
#include "SDL_net.h"
#include "sfifo.h"

/* IPaddress */
EEL_MAKE_CAST(IPaddress)

/* NET2 socket + nonblocking buffered sending */

/*
 * The actual socket object. This is separate from the EEL wrapper
 * object so that it can be detached and handed over to the thread,
 * which stays around to send all data before the real socket is
 * actually closed.
 */
typedef struct
{
	int		n2socket;	/* NET2 socket index */
	int		pollperiod;	/* 0 ==> direct TCPSend() */
	int		fifosize;	/* Size of FIFO (init) */
	SDL_Thread	*sender;	/* Polling sender thread */
	sfifo_t		fifo;		/* Lock-free FIFO buffer */
	int		closed;		/* 1 ==> dies after emptying the FIFO */
	EEL_xno		status;		/* Sender status */
} REAL_EB_socket;

/* The EEL wrapper object - owns the real socket object until closed */
typedef struct
{
	REAL_EB_socket	*rs;
} EB_socket;
EEL_MAKE_CAST(EB_socket)

/* Returns socket object for NET2 socket handle 'socket', or NULL. */
EEL_object *eb_net_get_socket(EEL_vm *vm, int socket);

EEL_xno eb_net_init(EEL_vm *vm);

#endif /* EELBOX_NET_H */
