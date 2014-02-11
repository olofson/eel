#ifndef _NET2SETS_H_
#define _NET2SETS_H_
/*
    NET2 is a threaded, event based, network IO library for SDL.
    Copyright 2002 Bob Pendleton

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

#include "SDL.h"
#include "SDL_net.h"

#ifdef __cplusplus
extern "C" {
#endif

  //------------------------------------------
  //
  // SocketSet routines
  //

  typedef struct
  {
    int itr;
    int last;
    int size;
    int *set;
  } SocketSet;

  int initSocketSet(SocketSet *s, int size);
  void finitSocketSet(SocketSet *s);
  int memberSocketSet(SocketSet *s, int *v);
  int *itemSocketSet(SocketSet *s, int *v);
  int addSocketSet(SocketSet *s, int *v);
  int delSocketSet(SocketSet *s, int *v);
  int firstSocketSet(SocketSet *s, int *v);
  int nextSocketSet(SocketSet *s, int *v);
  int NET2_TCPSendSet(SocketSet *s, char *buf, int len);   // Send data to a set of sockets

  //------------------------------------------
  //
  // IPSet Routines
  //

  typedef struct
  {
    int itr;
    int last;
    int size;
    IPaddress *set;
  } IPSet;

  int initIPSet(IPSet *s, int size);
  void finitIPSet(IPSet *s);
  int memberIPSet(IPSet *s, IPaddress *v);
  IPaddress *itemIPSet(IPSet *s, IPaddress *v);
  int addIPSet(IPSet *s, IPaddress *v);
  int delIPSet(IPSet *s, IPaddress *v);
  int firstIPSet(IPSet *s, IPaddress *v);
  int nextIPSet(IPSet *s, IPaddress *v);
  int NET2_UDPSendSet(IPSet *s, char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
