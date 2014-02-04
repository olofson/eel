/*
    NET2 is a threaded, event based, network IO library for SDL.
    Copyright (C) 2002 Bob Pendleton

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License
    as published by the Free Software Foundation; either version 2.1
    of the License, or (at your option) any later version.
    
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
    
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA

    If you do not wish to comply with the terms of the LGPL please
    contact the author as other terms are available for a fee.
    
    Bob Pendleton
    Bob@Pendleton.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_net.h"

#include "net2.h"
#include "net2sets.h"
#include "set.h"
#include "trace.h"

//----------------------------------------
//
// handy stuff
//

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define abs(a) (((a)<0) ? -(a) : (a))
#define sign(a) (((a)<0) ? -1 : (a)>0 ? 1 : 0)

//----------------------------------------
// 
// Sets used to keep track of sockets
// and IP addresses.
//

SETCODE(, Socket, int, 10, (*(a) == *(b)));

static __inline__ int ipEqual(IPaddress *a, IPaddress *b)
{
  return (a->host == b->host) && (a->port == b->port);
}

SETCODE(, IP, IPaddress, 10, ipEqual(a, b));

static char *error = 0; 
static int errorSocket = -1; 

//----------------------------------------
//
//
//

static __inline__ void setError(char *err, int socket)
{
  error = err;
  errorSocket = socket;
}

int NET2_UDPSendSet(IPSet *s, char *buf, int len)
{
  int val = 0;
  int done = 0;
  int status = 0;
  IPaddress ip;

  done = firstIPSet(s, &ip);
  while (-1 != done)
  {
    status = NET2_UDPSend(&ip, buf, len);
    val = min(val, status);

    done = nextIPSet(s, &ip);
  }

  return val;
}

//----------------------------------------
//
//
//

int NET2_TCPSendSet(SocketSet *s, char *buf, int len)
{
  int val = 0;
  int done = 0;
  int status = 0;
  int socket = 0;

  done = firstSocketSet(s, &socket);
  while (-1 != done)
  {
    status = NET2_TCPSend(socket, buf, len);
    val = min(val, status);

    done = nextSocketSet(s, &socket);
  }

  return val;
}

