#ifndef _NET2_H_
#define _NET2_H_
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

#include "SDL.h"
#include "SDL_net.h"

#define NET2_MAX_SOCKETS    (1024)

#ifdef __cplusplus
extern "C" {
#endif

  /* NET2 event types */
  enum
  {
    NET2_ERROREVENT,

    NET2_TCPACCEPTEVENT,
    NET2_TCPRECEIVEEVENT,
    NET2_TCPCLOSEEVENT,

    NET2_UDPRECEIVEEVENT,
  };

  //------------------------------------------
  //
  // General NET2 routines
  //

  int NET2_Init(void);                                     // Initialize net2
  void NET2_Quit(void);                                    // Shutdown net2

  int NET2_GetEventType(SDL_Event *event);                 // get the event type from a net2 event
  int NET2_GetSocket(SDL_Event *event);                    // get the socket from an event
  int NET2_GetEventData(SDL_Event *event);                 // get the auxiliar data from an event

  const char *NET2_GetError(void);                         // get a string describing the last error
  const char *NET2_GetEventError(SDL_Event *event);        // get the string for an error event

  int NET2_ResolveHost(IPaddress *ip, char *name, int port); // get the IP address for a host name

  //------------------------------------------
  //
  // TCP routines
  //

  int NET2_TCPAcceptOn(int port);                          // Accept incoming connections
  int NET2_TCPAcceptOnIP(IPaddress *ip);                   // Accept incoming connections
  int NET2_TCPConnectTo(const char *host, int port);       // Connect to an IP address/port
  int NET2_TCPConnectToIP(IPaddress *ip);                  // Connect to an IP address/port
  void NET2_TCPClose(int socket);                          // close a TCP socket
  int NET2_TCPSend(int socket, const char *buf, int len);  // Send data to a socket
  int NET2_TCPRead(int socket, char *buf, int len);        // Read data from a socket
  IPaddress *NET2_TCPGetPeerAddress(int socket);           // Get the IP address of a socket
  int NET2_TCPStrLen(int socket);                          // Get number of bytes until next
                                                           // '\0', or end of queue.

  //------------------------------------------
  //
  // UDP routines
  //

  int NET2_UDPAcceptOn(int port, int size);
  void NET2_UDPClose(int socket);
  int NET2_UDPSend(IPaddress *addr, const char *buf, int len);
  UDPpacket *NET2_UDPRead(int socket);
  void NET2_UDPFreePacket(UDPpacket *p);

#ifdef __cplusplus
}
#endif

#endif
