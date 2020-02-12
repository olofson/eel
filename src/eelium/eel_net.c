/*
---------------------------------------------------------------------------
	eel_net.c - EEL SDL_net Binding
---------------------------------------------------------------------------
 * Copyright 2005, 2006, 2009, 2011, 2014, 2017, 2019 David Olofson
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

#include <string.h>
#include <stdlib.h>
#include "eel_net.h"
#include "net2.h"

typedef struct
{
	int		ipaddress_cid;
	int		net2_socket_cid;
} ENET_moduledata;

static ENET_moduledata md = { 0, 0 };


/*----------------------------------------------------------
	IPaddress class
----------------------------------------------------------*/
static EEL_xno ipa_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	IPaddress *ipa;
	EEL_object *eo = eel_o_alloc(vm, sizeof(IPaddress), cid);
	if(!eo)
		return EEL_XMEMORY;
	ipa = o2IPaddress(eo);
	switch(initc)
	{
	  case 0:
		ipa->host = INADDR_NONE;
		ipa->port = 9999;
		break;
	  case 1:
		if(EEL_CLASS(initv) == md.ipaddress_cid)
		{
			IPaddress *src = o2IPaddress(initv->objref.v);
			memcpy(ipa, src, sizeof(IPaddress));
		}
		else
		{
			/* Create server socket */
			if(SDLNet_ResolveHost(ipa, NULL, eel_v2l(initv)) != 0)
			{
				eel_o_free(eo);
				return EEL_XDEVICEOPEN;
			}
		}
		break;
	  case 2:
	  {
		const char *s;
		switch(EEL_CLASS(initv))
		{
		  case EEL_CSTRING:
		  case EEL_CDSTRING:
			s = eel_v2s(initv);
		  default:
			s = NULL;
		}
		/* Connect as client */
		if(SDLNet_ResolveHost(ipa, s, eel_v2l(initv + 1)) != 0)
		{
			eel_o_free(eo);
			return EEL_XDEVICEOPEN;
		}
		break;
	  }
	  default:
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno ipa_clone(EEL_vm *vm,
		const EEL_value *src, EEL_value *dst, EEL_classes cid)
{
	EEL_object *co = eel_o_alloc(vm, sizeof(IPaddress), cid);
	if(!co)
		return EEL_XMEMORY;
	memcpy(o2IPaddress(co), o2IPaddress(src->objref.v), sizeof(IPaddress));
	eel_o2v(dst, co);
	return 0;
}


static EEL_xno ipa_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	IPaddress *ipa = o2IPaddress(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strcmp(is, "host") == 0)
		op2->integer.v = ipa->host;
	else if(strcmp(is, "port") == 0)
		op2->integer.v = ipa->port;
	else
		return EEL_XWRONGINDEX;
	op2->classid = EEL_CINTEGER;
	return 0;
}


static EEL_xno ipa_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	IPaddress *ipa = o2IPaddress(eo);
	const char *is = eel_v2s(op1);
	int iv = eel_v2l(op2);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strcmp(is, "host") == 0)
		ipa->host = iv;
	else if(strcmp(is, "port") == 0)
		ipa->port = iv;
	else
		return EEL_XWRONGINDEX;
	return 0;
}


/*----------------------------------------------------------
	NET2_socket class
----------------------------------------------------------*/

static EEL_object **eel_sockets;


static EEL_xno n2s_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	REAL_ENET_socket *rs;
	ENET_socket *ens;
	EEL_object *eo;
	rs = malloc(sizeof(REAL_ENET_socket));
	if(!rs)
		return EEL_XMEMORY;
	eo = eel_o_alloc(vm, sizeof(ENET_socket), cid);
	if(!eo)
	{
		free(rs);
		return EEL_XMEMORY;
	}
	ens = o2ENET_socket(eo);
	ens->rs = rs;
	rs->pollperiod = 50;
	rs->sender = NULL;
	rs->fifo.buffer = NULL;
	rs->closed = 0;
	rs->status = EEL_XOK;
	switch(initc)
	{
	  case 1:
		if(initv->classid == EEL_CINTEGER)
		{
			rs->n2socket = eel_v2l(initv);
			if(rs->n2socket > NET2_MAX_SOCKETS)
			{
				eel_o_free(eo);
				return EEL_XHIGHINDEX;
			}
		}
		else
		{
			IPaddress *ipa;
			if(EEL_CLASS(initv) != md.ipaddress_cid)
			{
				eel_o_free(eo);
				return EEL_XWRONGTYPE;
			}
			ipa = o2IPaddress(initv->objref.v);
			rs->n2socket = NET2_TCPConnectToIP(ipa);
			if(rs->n2socket < 0)
			{
				eel_o_free(eo);
				return EEL_XDEVICEOPEN;
			}
		}
		break;
	  case 2:
		rs->n2socket = NET2_TCPConnectTo(eel_v2s(initv),
				eel_v2l(initv + 1));
		if(rs->n2socket < 0)
		{
			eel_o_free(eo);
			return EEL_XDEVICEOPEN;
		}
		break;
	  default:
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	if(eel_sockets && (rs->n2socket >= 0))
		eel_sockets[rs->n2socket] = eo;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno n2s_destruct(EEL_object *eo)
{
	ENET_socket *ens = o2ENET_socket(eo);
	if(!ens->rs)
		return 0;	/* Detached! We're done here. */
	if(ens->rs->sender)
	{
		ens->rs->closed = 1;
		SDL_WaitThread(ens->rs->sender, NULL);
		sfifo_close(&ens->rs->fifo);
	}
	NET2_TCPClose(ens->rs->n2socket);
	if(ens->rs->n2socket >= 0)
		if(eel_sockets)
			eel_sockets[ens->rs->n2socket] = NULL;
	free(ens->rs);
	return 0;
}


#define	N2S_SEND_FRAGMENT	4096

static int n2s_sender_thread(void *data)
{
	char buf[N2S_SEND_FRAGMENT];
	REAL_ENET_socket *rs = (REAL_ENET_socket *)data;
	while(!rs->closed && !rs->status)
	{
		int count;
		SDL_Delay(rs->pollperiod);
		count = sfifo_used(&rs->fifo);
		if(!count)
			continue;
		while(count)
		{
			int res;
			int n = count;
			if(n > N2S_SEND_FRAGMENT)
				n = N2S_SEND_FRAGMENT;
			sfifo_read(&rs->fifo, buf, n);
			res = NET2_TCPSend(rs->n2socket, buf, n);
			if(res < 0)
			{
				rs->status = EEL_XDEVICEWRITE;
				break;
			}
			count -= n;
		}
	}
	// Detach from the EEL wrapper object
	rs->sender = NULL;
	return 0;
}


static EEL_xno n2_tcp_setbuf(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ENET_socket *ens;

	if(EEL_CLASS(args) != md.net2_socket_cid)
		return EEL_XWRONGTYPE;
	ens = o2ENET_socket(args->objref.v);
	if(!ens->rs)
		return EEL_XDEVICECLOSED;

	if(ens->rs->sender)
	{
		/*
		 * Change buffer size!
		 * We need to close the sender thread, buffer etc,
		 * and set up new gear to do this safely. To avoid
		 * mixed up data, we wait for the old thread to
		 * finish before moving on.
		 */
		ens->rs->closed = 2;
		SDL_WaitThread(ens->rs->sender, NULL);
		sfifo_close(&ens->rs->fifo);
	}

	ens->rs->fifosize = eel_v2l(args + 1);

	if(vm->argc >= 3)
		ens->rs->pollperiod = eel_v2l(args + 2);

	if(sfifo_init(&ens->rs->fifo, ens->rs->fifosize) != 0)
		return EEL_XMEMORY;
	ens->rs->sender = SDL_CreateThread(n2s_sender_thread, "net2-sender",
			ens->rs);
	if(!ens->rs->sender)
	{
		sfifo_close(&ens->rs->fifo);
		return EEL_XTHREADCREATE;
	}
	return 0;
}


static EEL_xno n2s_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	ENET_socket *ens = o2ENET_socket(eo);
	const char *is;
	if(!ens->rs)
		return EEL_XDEVICECLOSED;
	is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strcmp(is, "index") == 0)
		op2->integer.v = ens->rs->n2socket;
	else if(strcmp(is, "buffer") == 0)
	{
		if(ens->rs->fifo.buffer)
			op2->integer.v = sfifo_space(&ens->rs->fifo);
		else
			op2->integer.v = 0;
	}
	else
		return EEL_XWRONGINDEX;
	op2->classid = EEL_CINTEGER;
	return 0;
}


EEL_object *eel_net_get_socket(EEL_vm *vm, int socket)
{
	if(socket < 0)
		return NULL;
	if(socket >= NET2_MAX_SOCKETS)
		return NULL;
	if(!eel_sockets[socket])
	{
		/*
		 * We need to automatically wrap new sockets from
		 * TCPACCEPTEVENTs and co, because they just "pop up",
		 * with no proper way for us to wrap them.
		 */
		EEL_xno x;
		EEL_value v, init;
		eel_l2v(&init, socket);
		x = eel_o_construct(vm, md.net2_socket_cid, &init, 1, &v);
		if(x)
			return NULL;
		--v.objref.v->refcount;	/* Socket table is not an owner! */
	}
	return eel_sockets[socket];
}


/*----------------------------------------------------------
	TCP calls
----------------------------------------------------------*/
static EEL_xno n2_tcp_accept_on(EEL_vm *vm)
{
	EEL_xno x;
	EEL_value init;
	EEL_value *args = vm->heap + vm->argv;
	int s;
	if(EEL_CLASS(args) == md.ipaddress_cid)
		s = NET2_TCPAcceptOnIP(o2IPaddress(args->objref.v));
	else
		s = NET2_TCPAcceptOn(eel_v2l(args));
	if(s < 0)
		return EEL_XDEVICEOPEN;
	eel_l2v(&init, s);
	x = eel_o_construct(vm, md.net2_socket_cid, &init, 1,
			vm->heap + vm->resv);
	if(x)
	{
		NET2_TCPClose(s);
		return x;
	}
	return 0;
}


static EEL_xno n2_tcp_send(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ENET_socket *ens;
	int i, count = 0;
	if(EEL_CLASS(args) != md.net2_socket_cid)
		return EEL_XWRONGTYPE;
	ens = o2ENET_socket(args->objref.v);
	if(!ens->rs)
		return EEL_XDEVICECLOSED;
	if(ens->rs->status)
		return ens->rs->status;
	for(i = 1; i < vm->argc; ++i)
	{
		void *buf;
		int bsize;
		EEL_value *v = args + i;
		switch(EEL_CLASS(v))
		{
		  case EEL_CREAL:
		  {
			int n;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			char lb[sizeof(EEL_real)];
			*((EEL_real *)&lb) = v->real.v;
#else
			union
			{
				EEL_real r;
				char c[sizeof(EEL_real)];
			} cvt;
			char lb[sizeof(EEL_real)];
			cvt.r = v->real.v;
			for(n = 0; n < sizeof(EEL_real); ++n)
				lb[n] = cvt.c[sizeof(EEL_real) - 1 - n];
#endif
			buf = &lb;
			bsize = sizeof(EEL_real);
			break;
		  }
		  case EEL_CINTEGER:
		  {
			EEL_uint32 cvt;
			SDLNet_Write32(eel_v2l(v), &cvt);
			buf = &cvt;
			bsize = 4;
			break;
		  }
		  case EEL_CSTRING:
		  case EEL_CDSTRING:
		  {
		  	buf = (void *)eel_v2s(v);
			bsize = eel_length(v->objref.v);
			break;
		  }
		  default:
			return EEL_XARGUMENTS;
		}
		if(ens->rs->sender)
		{
			/* Buffered, non-blocking */
			if(sfifo_space(&ens->rs->fifo) < bsize)
				return EEL_XBUFOVERFLOW;
			sfifo_write(&ens->rs->fifo, buf, bsize);
		}
		else
		{
			/* Direct, blocking */
			int n = NET2_TCPSend(ens->rs->n2socket, buf, bsize);
			if(n < 0)
				return EEL_XDEVICEWRITE;
		}
		count += bsize;
	}
	eel_l2v(vm->heap + vm->resv, count);
	return 0;
}


static EEL_xno n2_tcp_read(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ENET_socket *ens;
	EEL_object *so;
	int count;
	char buf[258];
	if(EEL_CLASS(args) != md.net2_socket_cid)
		return EEL_XWRONGTYPE;
	ens = o2ENET_socket(args->objref.v);
	if(!ens->rs)
		return EEL_XDEVICECLOSED;
	count = NET2_TCPRead(ens->rs->n2socket, buf, sizeof(buf) - 1);
	if(!count)
	{
		eel_nil2v(vm->heap + vm->resv);
		return 0;
	}
	buf[count] = 0;
	so = eel_ds_nnew(vm, buf, count);
	if(!so)
		return EEL_XMEMORY;
	eel_o2v(vm->heap + vm->resv, so);
	return 0;
}


static EEL_xno n2_tcp_close(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ENET_socket *ens;
	if(EEL_CLASS(args) != md.net2_socket_cid)
		return EEL_XWRONGTYPE;
	ens = o2ENET_socket(args->objref.v);
	if(!ens->rs)
		return EEL_XDEVICECLOSED;
	if(ens->rs->n2socket < 0)
		return EEL_XDEVICECLOSED;
	if(ens->rs->status)
		return ens->rs->status;
	if(eel_sockets)
		eel_sockets[ens->rs->n2socket] = NULL;
	if(ens->rs->sender)
	{
		ens->rs->closed = 1;
		ens->rs->status = EEL_XDEVICECLOSED;
	}
	else
	{
		NET2_TCPClose(ens->rs->n2socket);
		sfifo_close(&ens->rs->fifo);
		free(ens->rs);
		ens->rs = NULL;
	}
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/
static EEL_xno eel_net_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		eel_free(m->vm, eel_sockets);
		eel_sockets = NULL;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp n2_constants[] =
{
	/* NET2 event types */
	{"ERROREVENT",		SDL_USEREVENT + NET2_ERROREVENT},
	{"TCPACCEPTEVENT",	SDL_USEREVENT + NET2_TCPACCEPTEVENT},
	{"TCPRECEIVEEVENT",	SDL_USEREVENT + NET2_TCPRECEIVEEVENT},
	{"TCPCLOSEEVENT",	SDL_USEREVENT + NET2_TCPCLOSEEVENT},
	{"UDPRECEIVEEVENT",	SDL_USEREVENT + NET2_UDPRECEIVEEVENT},

	{NULL,	0}
};


EEL_xno eel_net_init(EEL_vm *vm)
{
	EEL_object *c;
	EEL_object *m;

	if(eel_sockets)
		return EEL_XDEVICEOPENED;

	m = eel_create_module(vm, "NET2", eel_net_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	eel_sockets = (EEL_object **)eel_malloc(vm,
			NET2_MAX_SOCKETS * sizeof(EEL_object *));
	if(!eel_sockets)
	{
		eel_disown(m);
		return EEL_XMODULEINIT;
	}
	memset(eel_sockets, 0, NET2_MAX_SOCKETS * sizeof(EEL_object *));

	/* Types */
	c = eel_export_class(m, "IPaddress", EEL_COBJECT,
			ipa_construct, NULL, NULL);
	md.ipaddress_cid = eel_class_cid(c);
	eel_set_metamethod(c, EEL_MM_GETINDEX, ipa_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, ipa_setindex);
	eel_set_casts(vm, md.ipaddress_cid, md.ipaddress_cid, ipa_clone);

	c = eel_export_class(m, "Socket", EEL_COBJECT,
			n2s_construct, n2s_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, n2s_getindex);
	md.net2_socket_cid = eel_class_cid(c);

	/* TCP functions */
	eel_export_cfunction(m, 1, "TCPAcceptOn", 1, 0, 0, n2_tcp_accept_on);
	eel_export_cfunction(m, 1, "TCPSend", 1, 0, 1, n2_tcp_send);
	eel_export_cfunction(m, 1, "TCPRead", 1, 0, 0, n2_tcp_read);
	eel_export_cfunction(m, 0, "TCPClose", 1, 0, 0, n2_tcp_close);
	eel_export_cfunction(m, 0, "TCPSetBuffer", 2, 1, 0, n2_tcp_setbuf);

	/* Constants and enums */
	eel_export_lconstants(m, n2_constants);

	eel_disown(m);
	return 0;
}
