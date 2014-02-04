/*(LGPLv2.1)
---------------------------------------------------------------------------
	eb_net.h - EEL SDL_net Binding
---------------------------------------------------------------------------
 * Copyright (C) 2005, 2007, 2009 David Olofson
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
