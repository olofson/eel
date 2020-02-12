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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "SDL.h"
#include "SDL_net.h"
#include "SDL_thread.h"

#include "net2.h"
#include "fastevents.h"
#include "queue.h"
#include "trace.h"

//----------------------------------------
//
// Configuration parameters
//

#define tcpQueLen     (1024)
#define udpQueLen     (tcpQueLen / (sizeof(UDPpacket *)))

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
// Forward Declarations
//

static int PumpNetworkEvents(void *); // thread

static __inline__ int InitSockets(int incr);
static __inline__ void FinitSockets(void);

static __inline__ int AllocSocket(int type);
static __inline__ void FreeSocket(int socket);

static __inline__ int raw_NET2_TCPAcceptOn(int port);
static __inline__ int raw_NET2_TCPAcceptOnIP(IPaddress *ip);
static __inline__ int raw_NET2_TCPConnectTo(char *host, int port);
static __inline__ int raw_NET2_TCPConnectToIP(IPaddress *ip);
static __inline__ void raw_NET2_TCPClose(int socket);
static __inline__ int raw_NET2_TCPSend(int socket, char *buf, int len);
static __inline__ int raw_NET2_TCPRead(int socket, char *buf, int len);
static __inline__ IPaddress *raw_NET2_TCPGetPeerAddress(int socket);

static __inline__ int sendEvent(Uint8 code, int data1, int data2);
static __inline__ int sendEventP(Uint8 code, int data1, void *data2);

static __inline__ void lockData(void);
static __inline__ void unlockData(void);
static __inline__ void lockSDLNet(void);
static __inline__ void unlockSDLNet(void);

//----------------------------------------
//
// Private Types
//

enum // socket types
{
  unusedSocket,        // ready for allocation

  TCPServerSocket,     // accept connections on these
  TCPClientSocket,     // send and receive data on these

  UDPServerSocket,     // accept packets on these
};

enum // socket states
{
  unusedState =  1,     // socket is not in use
  readyState  =  2,     // socket is ready for use
  dyingState  =  4,     // connection closed but input still available
  addState    =  8,     // needs to be added to the socket set
  delState    = 16,     // needs to be deleted from the socket set
};

typedef struct
{
  int len;
  Uint8 buf[tcpQueLen];
} CharQue;

QUEUETYPE(Packet, UDPpacket *, udpQueLen);            // create type PacketQue
QUEUECODE(static __inline__, Packet, UDPpacket *, udpQueLen);

typedef struct
{
  Uint8 type;
  Uint8 state;
  union
  {
    int tcpPort;
    int udpLen;
  }p;
  union
  {
    TCPsocket            tcpSocket;
    UDPsocket            udpSocket;
    SDLNet_GenericSocket genSocket;
  }s;
  union
  {
    CharQue tb;
    PacketQue ub;
  }q;
} NET2_Socket;

//----------------------------------------
//
// Static Variables
//

static int lastHeapSocket = 0;
static int freeHeapSocket = 0;

static NET2_Socket *socketHeap[NET2_MAX_SOCKETS];
static SDLNet_SocketSet socketSet = NULL;

static int initialized = 0;

static UDPsocket udpSendSocket = NULL;

static const char *error = 0;
static int errorSocket = -1;

//----------------------------------------
//
//
//

static __inline__ void setError(const char *err, int socket)
{
  error = err;
  errorSocket = socket;
}

static __inline__ int sendError(const char *err, int socket)
{
  int val = -1;

  //printf("ERROR: %s\n", err); fflush(NULL);
  val = sendEventP(NET2_ERROREVENT, socket, (void *)(long)err);

  return val;
}

//----------------------------------------
//
// Threads, mutexs, thread utils, and
// thread safe wrappers
//

static int dataLocked = 0;
static SDL_mutex *dataLock = NULL;
static SDL_cond *dataWait = NULL;
static SDL_mutex *sdlNetLock = NULL;
static SDL_Thread *processSockets = NULL;
static int doneYet = 0;
static int waitForRead = 0;

static __inline__ int sendEventP(Uint8 code, int data1, void *data2)
{
  SDL_Event event;

  event.type = SDL_USEREVENT;
  event.user.code = code;
  event.user.data1 = (void *)(long)data1;
  event.user.data2 = data2;

  if (dataLocked)
  {
    unlockData();
    FE_PushEvent(&event);
    lockData();
  }
  else
  {
    //printf("this should not happen\n"); fflush(NULL);
    exit(1);
  }

  return 0;
}

static __inline__ int sendEvent(Uint8 code, int data1, int data2)
{
  return sendEventP(code, data1, (void *)(long)data2);
}

static __inline__ void signalRead(void)
{
  waitForRead = 0;
  SDL_CondSignal(dataWait);
}

static __inline__ void waitUntilRead(void)
{
  dataLocked = 0;
  SDL_CondWait(dataWait, dataLock);
  dataLocked = 1;
}

static __inline__ void lockData(void)
{
  if (-1 == SDL_LockMutex(dataLock))
  {
    setError("NET2: can't lock Data mutex", -1);
  }
  dataLocked = 1;
}

static __inline__ void unlockData(void)
{
  dataLocked = 0;
  if (-1 == SDL_UnlockMutex(dataLock))
  {
    setError("NET2: can't unlock Data mutex", -1);
  }
}

static __inline__ void lockSDLNet(void)
{
  if (-1 == SDL_LockMutex(sdlNetLock))
  {
    setError("NET2: can't lock SDLNet mutex", -1);
  }
}

static __inline__ void unlockSDLNet(void)
{
  if (-1 == SDL_UnlockMutex(sdlNetLock))
  {
    setError("NET2: can't unlock SDLNet mutex", -1);
  }
}

static __inline__ void snFreeSocketSet(SDLNet_SocketSet ss)
{
  lockSDLNet();
  SDLNet_FreeSocketSet(ss);
  unlockSDLNet();
}

static __inline__ SDLNet_SocketSet snAllocSocketSet(int size)
{
  SDLNet_SocketSet ss = NULL;

  lockSDLNet();
  ss = SDLNet_AllocSocketSet(size);
  unlockSDLNet();

  return ss;
}

static __inline__ int snTCPAddSocket(TCPsocket s)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_TCP_AddSocket(socketSet, s);
  unlockSDLNet();

  return val;
}

static __inline__ int snTCPDelSocket(TCPsocket s)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_TCP_DelSocket(socketSet, s);
  unlockSDLNet();

  return val;
}

static __inline__ int snUDPAddSocket(UDPsocket s)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_UDP_AddSocket(socketSet, s);
  unlockSDLNet();

  return val;
}

static __inline__ int snUDPDelSocket(UDPsocket s)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_UDP_DelSocket(socketSet, s);
  unlockSDLNet();

  return val;
}

static __inline__ TCPsocket snTCPAccept(TCPsocket s)
{
  TCPsocket socket = NULL;

  lockSDLNet();
  socket = SDLNet_TCP_Accept(s);
  unlockSDLNet();

  return socket;
}

static __inline__ void snTCPClose(TCPsocket s)
{
  lockSDLNet();
  SDLNet_TCP_Close(s);
  unlockSDLNet();
}

static __inline__ TCPsocket snTCPOpen(IPaddress *ip)
{
  TCPsocket socket = NULL ;

  lockSDLNet();
  socket = SDLNet_TCP_Open(ip);
  unlockSDLNet();

  return socket;
}

static __inline__ void snUDPClose(UDPsocket s)
{
  lockSDLNet();
  SDLNet_UDP_Close(s);
  unlockSDLNet();
}

static __inline__ UDPsocket snUDPOpen(int port)
{
  UDPsocket socket = NULL ;

  lockSDLNet();
  socket = SDLNet_UDP_Open(port);
  unlockSDLNet();

  return socket;
}

static __inline__ int snUDPSend(UDPsocket s, int c, UDPpacket *p)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_UDP_Send(s, c, p);
  unlockSDLNet();

  return val;
}

static __inline__ int snUDPRecv(UDPsocket s, UDPpacket *p)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_UDP_Recv(s, p);
  unlockSDLNet();

  return val;
}

static __inline__ UDPpacket *snUDPAllocPacket(int size)
{
  UDPpacket *val = 0;

  lockSDLNet();
  val = SDLNet_AllocPacket(size);
  unlockSDLNet();

  return val;
}

static __inline__ void snUDPFreePacket(UDPpacket *p)
{
  lockSDLNet();
  SDLNet_FreePacket(p);
  unlockSDLNet();
}

static __inline__ int snCheckSockets(SDLNet_SocketSet ss, int wait)
{
  int val = 0;

  val = SDLNet_CheckSockets(ss, wait);
  return val;
}

static __inline__ int snTCPRead(TCPsocket s, Uint8 *buf, int max)
{
  int val = -1;

  lockSDLNet();
  val = SDLNet_TCP_Recv(s, buf, max); // read what we can
  unlockSDLNet();

  return val;
}

static __inline__ int snTCPSend(TCPsocket s, Uint8 *buf, int len)
{
  int val = -1;

  lockSDLNet();
  val = SDLNet_TCP_Send(s, buf, len);
  unlockSDLNet();

  return val;
}

static __inline__ int snResolveHost(IPaddress *ip, char *name, int port)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_ResolveHost(ip, name, port);
  unlockSDLNet();

  return val;
}

static __inline__ int snSocketReady(SDLNet_GenericSocket s)
{
  int val = 0;

  lockSDLNet();
  val = SDLNet_SocketReady(s);
  unlockSDLNet();

  return val;
}

static __inline__ IPaddress *snTCPGetPeerAddress(TCPsocket s)
{
  IPaddress *val = NULL;

  lockSDLNet();
  val = SDLNet_TCP_GetPeerAddress(s);
  unlockSDLNet();

  return val;
}

//----------------------------------------
//
// API routines
//
// to make coding easier most APIs have a
// raw version that does the work and a
// thread safe wrapper.
//

//----------------------------------------
//
//
//

int NET2_ResolveHost(IPaddress *ip, char *name, int port)
{
  return snResolveHost(ip, name, port);
}


//----------------------------------------
//
//
//

void NET2_UDPFreePacket(UDPpacket *p)
{
  snUDPFreePacket(p);
}

//----------------------------------------
//
//
//

int NET2_GetEventType(SDL_Event *e)
{
  return e->user.code;
}

//----------------------------------------
//
//
//

int NET2_GetSocket(SDL_Event *e)
{
  return (int)(long)e->user.data1;
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_UDPAcceptOn(int port, int size)
{
  int s = -1;

  s = AllocSocket(UDPServerSocket);
  if (-1 == s)
  {
    setError("NET2: out of memory", s);
    return -1;
  }

  socketHeap[s]->s.udpSocket = snUDPOpen(port);
  if (NULL == socketHeap[s]->s.udpSocket)
  {
    setError("NET2: can't open a socket", s);
    FreeSocket(s);
    return -1;
  }

  socketHeap[s]->p.udpLen = size;
  socketHeap[s]->state = addState;

  return s;
}

int NET2_UDPAcceptOn(int port, int size)
{
  int val = -1;

  lockData();
  val = raw_NET2_UDPAcceptOn(port, size);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_UDPSend(IPaddress *ip, char *buf, int len)
{
  UDPpacket p;

  p.channel = -1;
  p.data = (Uint8 *)buf;
  p.len = len;
  p.maxlen = len;
  p.address = *ip;

  if (0 == snUDPSend(udpSendSocket, -1, &p))
  {
    setError("NET2: UDP send failed", -1);
    return -1;
  }

  return 0;
}

int NET2_UDPSend(IPaddress *ip, const char *buf, int len)
{
  int val = -1;

  lockData();
  val = raw_NET2_UDPSend(ip, (char *)buf, len);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static UDPpacket *raw_NET2_UDPRead(int s)
{
  UDPpacket *p = NULL;
  PacketQue *ub = NULL;

  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type == UDPServerSocket))
  {
    ub = &socketHeap[s]->q.ub;

    if (-1 == DequePacket(ub, &p))
    {
      return NULL;
    }
  }

  return p;
}

UDPpacket *NET2_UDPRead(int socket)
{
  UDPpacket *val = NULL;

  lockData();
  val = raw_NET2_UDPRead(socket);
  unlockData();
  signalRead();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ void raw_NET2_UDPClose(int s)
{
  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type == UDPServerSocket))
  {
    socketHeap[s]->state = delState;
  }
}

void NET2_UDPClose(int socket)
{
  lockData();
  raw_NET2_UDPClose(socket);
  unlockData();
}

//----------------------------------------
//
//
//

int NET2_GetEventData(SDL_Event *e)
{
  return (int)(long)e->user.data2;
}

//----------------------------------------
//
//
//

const char *NET2_GetError(void)
{
  return error;
}

const char *NET2_GetEventError(SDL_Event *e)
{
//  int ed = NET2_GetEventData(e);
//  return (const char *)ed;
  return (const char *)e->user.data2;
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_TCPAcceptOnIP(IPaddress *ip)
{
  int s = -1;

  s = AllocSocket(TCPServerSocket);
  if (-1 == s)
  {
    setError("NET2: out of memory", -1);
    return -1;
  }

  socketHeap[s]->s.tcpSocket = snTCPOpen(ip);
  if (NULL == socketHeap[s]->s.tcpSocket)
  {
    setError("NET2: can't open a socket", -1);
    FreeSocket(s);
    return -1;
  }

  socketHeap[s]->p.tcpPort = ip->port;
  socketHeap[s]->state = addState;

  return s;
}

static __inline__ int raw_NET2_TCPAcceptOn(int port)
{
  int s = -1;
  IPaddress ip;

  if (-1 == snResolveHost(&ip, NULL, port))
  {
    setError("NET2: can't resolve that host name or address", -1);
    return -1;
  }
  //printIPaddress(&ip);

  s = raw_NET2_TCPAcceptOnIP(&ip);
  if (-1 != s)
  {
    //printf("port=%d\n", port);
    socketHeap[s]->p.tcpPort = port;
  }

  return s;
}

int NET2_TCPAcceptOn(int port)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPAcceptOn(port);
  unlockData();

  return val;
}

int NET2_TCPAcceptOnIP(IPaddress *ip)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPAcceptOnIP(ip);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_TCPConnectToIP(IPaddress *ip)
{
  int s = -1;

  s = AllocSocket(TCPClientSocket);
  if (-1 == s)
  {
    setError("NET2: out of memory", -1);
    return -1;
  }

  socketHeap[s]->s.tcpSocket = snTCPOpen(ip);
  if (NULL == socketHeap[s]->s.tcpSocket)
  {
    setError("NET2: can't open a socket", -1);
    FreeSocket(s);
    return -1;
  }

  socketHeap[s]->state = addState;

  return s;
}

static __inline__ int raw_NET2_TCPConnectTo(char *host, int port)
{
  int s = -1;
  IPaddress ip;

  if (-1 == snResolveHost(&ip, host, port))
  {
    setError("NET2: can't find that host name or address", -1);
    return -1;
  }
  //printIPaddress(&ip);

  s = raw_NET2_TCPConnectToIP(&ip);
  return s;
}

int NET2_TCPConnectTo(const char *host, int port)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPConnectTo((char *)host, port);
  unlockData();

  return val;
}

int NET2_TCPConnectToIP(IPaddress *ip)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPConnectToIP(ip);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ void raw_NET2_TCPClose(int s)
{
  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      ((socketHeap[s]->type == TCPServerSocket) ||
       (socketHeap[s]->type == TCPClientSocket)))
  {
    socketHeap[s]->state = delState; // stop reading and writing
  }
}

void NET2_TCPClose(int socket)
{
  lockData();
  raw_NET2_TCPClose(socket);
  unlockData();
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_TCPSend(int s, char *buf, int len)
{
  int val = 0;

  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type == TCPClientSocket) &&
      (0 != (socketHeap[s]->state & (addState | readyState))))
  {
    if (0 < len)
    {
      val = snTCPSend(socketHeap[s]->s.tcpSocket, (Uint8 *)buf, len);
      if (val != len)
      {
        socketHeap[s]->state = dyingState;
        setError("NET2: can't send TCP data, expect a close event", s);
        sendEvent(NET2_TCPCLOSEEVENT, s, -1);
        return -1;
      }
    }
    return val;
  }

  setError("NET2: you can't do a TCP send on that socket", s);
  return -1;
}

int NET2_TCPSend(int socket, const char *buf, int len)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPSend(socket, (char *)buf, len);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ int raw_NET2_TCPRead(int s, char *buf, int len)
{
  CharQue *tb = NULL;
  int nlen = 0;

  if ((0 < len) &&
      (s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type == TCPClientSocket))
  {
    tb = &socketHeap[s]->q.tb;

    nlen = min(tb->len, len);
    memmove(&buf[0], &tb->buf[0], nlen);
    tb->len -= nlen;
    memmove(&tb->buf[0], &tb->buf[nlen], tb->len);
  }

  return nlen;
}

int NET2_TCPRead(int socket, char *buf, int len)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPRead(socket, buf, len);
  unlockData();
  signalRead();

  return val;
}

//----------------------------------------
//
//
//
#if 0
static __inline__ int raw_NET2_TCPReceived(int s)
{
  CharQue *tb = NULL;
  int nlen = 0;

  if ((s >= 0) && (s < lastHeapSocket) &&
      (socketHeap[s]->type == TCPClientSocket))
  {
    tb = &socketHeap[s]->q.tb;
    nlen = tb->len;
  }

  return nlen;
}

static int NET2_TCPReceived(int socket)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPReceived(socket);
  unlockData();

  return val;
}
#endif
//----------------------------------------
//
//
//

static __inline__ int raw_NET2_TCPStrLen(int s)
{
  CharQue *tb = NULL;
  int nlen = 0;

  if ((s >= 0) && (s < lastHeapSocket) &&
      (socketHeap[s]->type == TCPClientSocket))
  {
    tb = &socketHeap[s]->q.tb;
    for( ; (nlen < tb->len) && (!tb->buf[nlen]); ++nlen)
      ;
  }

  return nlen;
}

int NET2_TCPStrLen(int socket)
{
  int val = 0;

  lockData();
  val = raw_NET2_TCPStrLen(socket);
  unlockData();

  return val;
}

//----------------------------------------
//
//
//

static __inline__ IPaddress *raw_NET2_TCPGetPeerAddress(int s)
{
  IPaddress *ip = NULL;

  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type == TCPClientSocket))
  {
    ip = snTCPGetPeerAddress(socketHeap[s]->s.tcpSocket);
  }

  return ip;
}

IPaddress *NET2_TCPGetPeerAddress(int s)
{
  IPaddress *ip = NULL;

  lockData();
  ip = raw_NET2_TCPGetPeerAddress(s);
  unlockData();

  return ip;
}

//----------------------------------------
//
// NET2 initialization and finalization
//

int NET2_Init()
{
  if (initialized)
  {
    return 0;
  }

#ifndef WIN32

  // SIGPIPE has to be ignored so that NET2 can handle broken
  // connections without raising a program wide exception. SDL_net
  // "should" do this so that it can handle exception properly, but it
  // doesn't.

  signal(SIGPIPE, SIG_IGN); // work around for bug in SDL_net
#endif

  error = 0;
  errorSocket = -1;

  doneYet = 0;

  dataLock = SDL_CreateMutex();
  if (NULL == dataLock)
  {
    setError("NET2: can't create a mutex", -1);
    return -1;
  }

  sdlNetLock = SDL_CreateMutex();
  if (NULL == sdlNetLock)
  {
    setError("NET2: can't create a mutex", -1);
    return -1;
  }

  dataWait = SDL_CreateCond();
  if (NULL == dataWait)
  {
    setError("NET2: can't create a condition variable", -1);
    return -1;
  }

  InitSockets(NET2_MAX_SOCKETS);

  udpSendSocket = snUDPOpen(0);
  if (NULL == udpSendSocket)
  {
    setError("NET2: can't open the UDP send socket", -1);
    return -1;
  }

  processSockets = SDL_CreateThread(PumpNetworkEvents, "NET2::PumpNetworkEvents", NULL);
  if (NULL == processSockets)
  {
    setError("NET2: can't start the network thread", -1);
    return -1;
  }
  //printf("root=%d thread=%d\n", SDL_ThreadID(), SDL_GetThreadID(processSockets)); fflush(NULL);

  initialized = 1;
  return 0;
}

void NET2_Quit()
{
  SDL_Event event;

  if (!initialized)
  {
    return;
  }

  doneYet = 1; // tell pumpNetworkEvents thread to die
  while (0 < FE_PollEvent(&event))  // it might be waiting on the queue
  {
  }
  signalRead();
  SDL_WaitThread(processSockets, NULL); // wait for it to die

  FinitSockets();

  snUDPClose(udpSendSocket);

  SDL_DestroyMutex(dataLock);
  SDL_DestroyMutex(sdlNetLock);
  SDL_DestroyCond(dataWait);

  dataLock = NULL;
  sdlNetLock = NULL;
  dataWait = NULL;

  error = 0;
  errorSocket = -1;

  initialized = 0;
}

//----------------------------------------
//
// Network pump thread. This handles
// input and converts it to events.
//

static int PumpNetworkEvents(void *nothing)
{
  int i = 0;

#define timeOut (10)

  while (!doneYet)
  {
    if (-1 == snCheckSockets(socketSet, timeOut))
    {
      setError("NET2: the CheckSockets call failed", -1);
    }

    lockData();
    while ((!doneYet) && waitForRead)
    {
      waitUntilRead();
    }

    for (i = 0; ((!doneYet) && (i < lastHeapSocket)); i++)
    {
      if (addState == socketHeap[i]->state)
      {
        switch(socketHeap[i]->type)
        {
        case unusedSocket:
          sendError("NET2: trying to add an unused socket", i);
          break;

        case TCPServerSocket:
        case TCPClientSocket:
          if (-1 != snTCPAddSocket(socketHeap[i]->s.tcpSocket))
          {
            socketHeap[i]->state = readyState;
          }
          else
          {
            socketHeap[i]->state = delState;
            sendError("NET2: can't add a TCP socket to the socket set", i);
          }
          break;

        case UDPServerSocket:
          if (-1 != snUDPAddSocket(socketHeap[i]->s.udpSocket))
          {
            socketHeap[i]->state = readyState;
          }
          else
          {
            socketHeap[i]->state = delState;
            sendError("NET2: can't add a UDP socket to the socket set", i);
          }
          break;

        default:
          sendError("NET2: invalid socket type, this should never happen", i);
          break;
        }
      }
      else if (delState == socketHeap[i]->state)
      {
        switch(socketHeap[i]->type)
        {
        case unusedSocket:
          sendError("NET2: trying to delete an unused socket", i);
          break;

        case TCPServerSocket:
        case TCPClientSocket:
          if (-1 == snTCPDelSocket(socketHeap[i]->s.tcpSocket))
          {
            sendError("NET2: can't delete a TCP socket from the socket set", i);
          }
          snTCPClose(socketHeap[i]->s.tcpSocket);
          FreeSocket(i);
          break;

        case UDPServerSocket:
          if (-1 == snUDPDelSocket(socketHeap[i]->s.udpSocket))
          {
            sendError("NET2: can't delete a UDP socket from the socket set", i);
          }
          snUDPClose(socketHeap[i]->s.udpSocket);
          FreeSocket(i);
          break;

        default:
          sendError("NET2: invalid socket type, this should never happen", i);
          break;
        }
      }
      else if ((TCPServerSocket == socketHeap[i]->type) &&
               (snSocketReady(socketHeap[i]->s.genSocket)))
      {
        TCPsocket socket = NULL;
        socket = snTCPAccept(socketHeap[i]->s.tcpSocket);

        if (NULL != socket)
        {
          int s = -1;

          s = AllocSocket(TCPClientSocket);
          if (-1 != s)
          {
            //printf("got a connection!\n");

            socketHeap[s]->s.tcpSocket = socket;
            socketHeap[s]->state = addState;
            sendEvent(NET2_TCPACCEPTEVENT,  s, socketHeap[i]->p.tcpPort);
          }
          else // can't handle the connection, so close it.
          {
            snTCPClose(socket);
            sendError("NET2: a TCP accept failed", i); // let the app know
          }
        }
      }
      else if ((TCPClientSocket == socketHeap[i]->type) &&
               (readyState == socketHeap[i]->state) &&
               (snSocketReady(socketHeap[i]->s.genSocket)))
      {
        int len;
        CharQue *tb = &socketHeap[i]->q.tb;

        if (tcpQueLen <= tb->len)
        {
          waitForRead = 1;
        }
        else
        {
          len = snTCPRead(socketHeap[i]->s.tcpSocket,
                          &tb->buf[tb->len],
                          tcpQueLen - tb->len);

          if (0 < len)
          {
            int oldlen = tb->len;
            tb->len += len;
            if (0 == oldlen)
            {
              sendEvent(NET2_TCPRECEIVEEVENT, i, -1);
            }
          }
          else // no byte, must be dead.
          {
            socketHeap[i]->state = dyingState;
            sendEvent(NET2_TCPCLOSEEVENT, i, -1);
          }
        }
      }
      else if ((UDPServerSocket == socketHeap[i]->type) &&
               (readyState == socketHeap[i]->state) &&
               (snSocketReady(socketHeap[i]->s.genSocket)))
      {
        int recv = 0;
        UDPpacket *p = NULL;
        PacketQue *ub = &socketHeap[i]->q.ub;

        if (PacketQueFull(ub))
        {
          waitForRead = 1;
        }
        else
        {
          while ((!PacketQueFull(ub)) &&
                 (NULL != (p = snUDPAllocPacket(socketHeap[i]->p.udpLen))) &&
                 (1 == (recv = snUDPRecv(socketHeap[i]->s.udpSocket, p))))
          {
            if (PacketQueEmpty(ub))
            {
              EnquePacket(ub, &p);
              sendEvent(NET2_UDPRECEIVEEVENT, i, -1);
            }
            else
            {
              EnquePacket(ub, &p);
            }
          }

          // unravel terminating conditions and free left over memory
          // if we need to
          if (!PacketQueFull(ub)) // if the packet que is full then everything is fine
          {
            if (NULL != p) // couldn't alloc a packet
            {
              if (0 >= recv) // ran out of packets
              {
                snUDPFreePacket(p);
              }
            }
            else
            {
              sendError("NET2: out of memory", i);
            }
          }
        }
      }
    }
    unlockData();
  }

  return 0;
}

//----------------------------------------
//
// Socket heap utilities
//

static __inline__ int InitSockets(int incr)
{
  int i = 0;

  freeHeapSocket = 0;
  lastHeapSocket = 0;

  socketSet = snAllocSocketSet(NET2_MAX_SOCKETS);
  if (NULL == socketSet)
  {
    setError("NET2: out of memory", -1);
    return -1;
  }

  memset(socketHeap, 0, sizeof(socketHeap));
  for (i = 0; i < NET2_MAX_SOCKETS; i++)
  {
    //printf("%d\n", i);
    socketHeap[i] = NULL;
  }

  return 0;
}

//
// close all sockets and free resources
//
// MUST NOT be called while PumpNetworkEvents
// thread is running.
//

static __inline__ void FinitSockets()
{
  int i = 0;

  for (i = 0; i < lastHeapSocket; i++)
  {
    if (NULL != socketHeap[i])
    {
      if ((TCPServerSocket == socketHeap[i]->type) ||
          (TCPClientSocket == socketHeap[i]->type))
      {
        //printf("Used=%d\n", i);
        snTCPClose(socketHeap[i]->s.tcpSocket);
      }

      if (UDPServerSocket == socketHeap[i]->type)
      {
        //printf("Used=%d\n", i);
        snUDPClose(socketHeap[i]->s.udpSocket);
      }

      free(socketHeap[i]);
      socketHeap[i] = NULL;
    }
  }

  if (NULL != socketSet)
  {
    snFreeSocketSet(socketSet);
    socketSet = NULL;
  }
}

//
// free a socket
//
static __inline__ void FreeSocket(int s)
{
  if ((s >= 0) &&
      (s < lastHeapSocket) &&
      (socketHeap[s]->type != unusedSocket))
  {
    if (UDPServerSocket == socketHeap[s]->type)
    {
      UDPpacket *p;
      PacketQue *ub = &socketHeap[s]->q.ub;

      while (-1 != DequePacket(ub, &p))
      {
        SDLNet_FreePacket(p);
      }
    }

    socketHeap[s]->type = unusedSocket;
    socketHeap[s]->state = unusedState;
    socketHeap[s]->s.genSocket = NULL;
  }
}

//
// allocate a socket
//

static __inline__ void initBuffer(int s, int type)
{
  switch (type)
  {
  case TCPClientSocket:
    socketHeap[s]->q.tb.len = 0;
    break;

  case UDPServerSocket:
    InitPacketQue(&socketHeap[s]->q.ub);
    break;
  }
}

static __inline__ int AllocSocket(int type)
{
  int i = 0;
  int s = -1;

  //printf("last=%d\n", lastHeapSocket);
  for (i = 0; i < lastHeapSocket; i++)
  {
    //printf("(%d)\n", freeHeapSocket);
    s = freeHeapSocket;
    if (unusedSocket == socketHeap[s]->type)
    {
      initBuffer(s, type);
      socketHeap[s]->type = type;
      socketHeap[s]->state = unusedState;
      socketHeap[s]->s.genSocket = NULL;

      return s;
    }

    freeHeapSocket = (freeHeapSocket + 1) % lastHeapSocket;
  }

  if (lastHeapSocket < NET2_MAX_SOCKETS)
  {
    s = lastHeapSocket;

    socketHeap[s] = malloc(sizeof(NET2_Socket));
    if (NULL == socketHeap[s])
    {
      return -1;
    }

    initBuffer(s, type);
    socketHeap[s]->type = type;
    socketHeap[s]->state = unusedState;
    socketHeap[s]->s.genSocket = NULL;

    lastHeapSocket++;
    return s;
  }

  return -1;
}
