/*(LGPLv2.1)
---------------------------------------------------------------------------
	eel_midi.c - EEL MIDI I/O module
---------------------------------------------------------------------------
 * Copyright (C) 2006, 2009, 2011 David Olofson
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
#include "eel_midi.h"

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

static int loaded = 0;

typedef enum
{
	EM_NOTEOFF = 0,
	EM_NOTEON,
	EM_AFTERTOUCH,
	EM_CONTROLCHANGE,
	EM_PROGRAMCHANGE,
	EM_CHANNELPRESSURE,
	EM_PITCHBEND,
	EM_SYSTEM,
	EM_RPN,
	EM_NRPN,
	EM_UNKNOWN
} EELMIDI_events;


/*--------------------------------------------------------------------
	Common state structure
--------------------------------------------------------------------*/

typedef struct
{
	int		rpn;	/* 0 ==> NRPN; 1 == RPN */
	unsigned	index;
	unsigned	data;
} RPN_state;

typedef struct
{
	EEL_vm		*vm;
	EEL_object	*msg;
	RPN_state	rpnstate[16];
#ifdef HAVE_ALSA
	int		port_id;
	snd_seq_t	*seq_handle;
#endif
} EELMIDI_data;


EELMIDI_data	emd;


static inline void em_seti(EEL_object *msg, const char *n, long val)
{
	EEL_value v;
	eel_l2v(&v, val);
	eel_setsindex(msg, n, &v);
}

#ifdef HAVE_ALSA

/*--------------------------------------------------------------------
	EEL message generator
--------------------------------------------------------------------*/

static void ev_off(EEL_object *t,
		unsigned ch, unsigned pitch, unsigned vel)
{
	em_seti(t, "type", EM_NOTEOFF);
	em_seti(t, "channel", ch);
	em_seti(t, "pitch", pitch);
	em_seti(t, "velocity", vel);
}

static void ev_on(EEL_object *t,
		unsigned ch, unsigned pitch, unsigned vel)
{
	em_seti(t, "type", EM_NOTEON);
	em_seti(t, "channel", ch);
	em_seti(t, "pitch", pitch);
	em_seti(t, "velocity", vel);
}

static void ev_aftertouch(EEL_object *t,
		unsigned ch, unsigned pitch, unsigned press)
{
	em_seti(t, "type", EM_AFTERTOUCH);
	em_seti(t, "channel", ch);
	em_seti(t, "pitch", pitch);
	em_seti(t, "pressure", press);
}

static void ev_control(EEL_object *t,
		unsigned ch, unsigned ctrl, unsigned amt)
{
	em_seti(t, "type", EM_CONTROLCHANGE);
	em_seti(t, "channel", ch);
	em_seti(t, "control", ctrl);
	em_seti(t, "value", amt);
}

static void ev_rpn(EEL_object *t,
		unsigned ch, unsigned ctrl, unsigned amt)
{
	em_seti(t, "type", EM_RPN);
	em_seti(t, "channel", ch);
	em_seti(t, "control", ctrl);
	em_seti(t, "value", amt);
}

static void ev_nrpn(EEL_object *t,
		unsigned ch, unsigned ctrl, unsigned amt)
{
	em_seti(t, "type", EM_NRPN);
	em_seti(t, "channel", ch);
	em_seti(t, "control", ctrl);
	em_seti(t, "value", amt);
}

static void ev_program(EEL_object *t,
		unsigned ch, unsigned prog)
{
	em_seti(t, "type", EM_PROGRAMCHANGE);
	em_seti(t, "channel", ch);
	em_seti(t, "program", prog);
}

static void ev_pressure(EEL_object *t,
		unsigned ch, unsigned press)
{
	em_seti(t, "type", EM_CHANNELPRESSURE);
	em_seti(t, "channel", ch);
	em_seti(t, "pressure", press);
}

static void ev_bend(EEL_object *t,
		unsigned ch, int amt)
{
	em_seti(t, "type", EM_PITCHBEND);
	em_seti(t, "channel", ch);
	em_seti(t, "value", amt);
}



/*--------------------------------------------------------------------
	alsaseq: ALSA Sequencer I/O driver
--------------------------------------------------------------------*/

static void alsaseq_close(EELMIDI_data *d)
{
	if(d->seq_handle)
	{
		snd_seq_close(d->seq_handle);
		d->seq_handle = NULL;
	}
}


static inline void alsaseq_do_rpn(EELMIDI_data *d, unsigned ch)
{
	if(16383 == d->rpnstate[ch].index)
		return;
	if(d->rpnstate[ch].rpn)
		ev_rpn(d->msg, ch, d->rpnstate[ch].index, d->rpnstate[ch].data);
	else
		ev_nrpn(d->msg, ch, d->rpnstate[ch].index, d->rpnstate[ch].data);
}


static EEL_xno alsaseq_read(EELMIDI_data *d)
{
	EEL_value v;
	EEL_xno x;
	snd_seq_event_t *ev;

	if(!d->seq_handle)
		return -1;
	
	if(snd_seq_event_input(d->seq_handle, &ev) < 0)
		return -1;

	x = eel_o_construct(d->vm, EEL_CTABLE, NULL, 0, &v);
	if(x)
		return x;
	d->msg = eel_v2o(&v);

	switch (ev->type)
	{
	  case SND_SEQ_EVENT_CONTROLLER:
	  {
		RPN_state *rs = &d->rpnstate[ev->data.control.channel];
		if(ev->data.control.channel >= 16)
		{
			ev_control(d->msg, ev->data.control.channel,
					ev->data.control.param,
					ev->data.control.value);
			break;
		}
		switch(ev->data.control.param)
		{
		  case 6:	/* Data Entry (coarse) */
			rs->data = ev->data.control.value << 7;
			break;
		  case 38:	/* Data Entry (fine) */
			rs->data |= ev->data.control.value;
			alsaseq_do_rpn(d, ev->data.control.channel);
			break;
		  case 98:	/* NRPN (fine) */
			rs->rpn = 0;
			rs->index |= ev->data.control.value;
			break;
		  case 99:	/* NRPN (coarse) */
			rs->rpn = 0;
			rs->index = ev->data.control.value << 7;
			break;
		  case 100:	/* RPN (fine) */
			rs->rpn = 1;
			rs->index |= ev->data.control.value;
			break;
		  case 101:	/* RPN (coarse) */
			rs->rpn = 1;
			rs->index = ev->data.control.value << 7;
			break;
		  default:
			ev_control(d->msg, ev->data.control.channel,
					ev->data.control.param,
					ev->data.control.value);
			break;
		}
		break;
	  }
	  case SND_SEQ_EVENT_NONREGPARAM:
		ev_nrpn(d->msg, ev->data.control.channel,
				ev->data.control.param,
				ev->data.control.value);
		break;
	  case SND_SEQ_EVENT_REGPARAM:
		ev_rpn(d->msg, ev->data.control.channel,
				ev->data.control.param,
				ev->data.control.value);
		break;
	  case SND_SEQ_EVENT_PGMCHANGE:
		ev_program(d->msg, ev->data.control.channel,
				ev->data.control.value);
		break;
	  case SND_SEQ_EVENT_NOTEON:
		ev_on(d->msg, ev->data.control.channel,
				ev->data.note.note,
				ev->data.note.velocity);
		break;
	  case SND_SEQ_EVENT_NOTEOFF:
		ev_off(d->msg, ev->data.control.channel,
				ev->data.note.note,
				ev->data.note.velocity);
		break;
	  case SND_SEQ_EVENT_KEYPRESS:
		ev_aftertouch(d->msg, ev->data.control.channel,
				ev->data.note.note,
				ev->data.note.velocity);
		break;
	  case SND_SEQ_EVENT_CHANPRESS:
		ev_pressure(d->msg, ev->data.control.channel,
				ev->data.control.value);
		break;
	  case SND_SEQ_EVENT_PITCHBEND:
		ev_bend(d->msg, ev->data.control.channel,
				ev->data.control.value);
		break;
	  default:
		em_seti(d->msg, "type", EM_UNKNOWN);
		break;
	}
	snd_seq_free_event(ev);
	return EEL_XNONE;
}


static int alsaseq_open(EELMIDI_data *d)
{
	int acaps = 0;
	const char *label = "EEL MIDI";
	acaps |= SND_SEQ_PORT_CAP_WRITE;
	acaps |= SND_SEQ_PORT_CAP_SUBS_WRITE;
	if(snd_seq_open(&d->seq_handle, "default", SND_SEQ_OPEN_DUPLEX,
			SND_SEQ_NONBLOCK) < 0)
	{
		fprintf(stderr, "alsaseq_open(): Error opening sequencer.\n");
		return -4;
	}
	snd_seq_set_client_name(d->seq_handle, label);
	d->port_id = snd_seq_create_simple_port(d->seq_handle, label, acaps,
					SND_SEQ_PORT_TYPE_SYNTH);
	if(d->port_id < 0)
	{
		fprintf(stderr, "alsaseq_open(): Error creating port.\n");
		snd_seq_close(d->seq_handle);
		d->seq_handle = NULL;
		return -5;
	}
	return 0;
}


#else /* HAVE_ALSA */


static void alsaseq_close(EELMIDI_data *d)
{
}

static int alsaseq_open(EELMIDI_data *d)
{
	return -1;
}

static int alsaseq_read(EELMIDI_data *d)
{
	return -1;
}


#endif /* HAVE_ALSA */


static EEL_xno em_OpenMIDI(EEL_vm *vm)
{
	alsaseq_close(&emd);
	if(alsaseq_open(&emd) < 0)
		return EEL_XDEVICEOPEN;
	return EEL_XNONE;
}


static EEL_xno em_CloseMIDI(EEL_vm *vm)
{
	alsaseq_close(&emd);
	return EEL_XNONE;
}


static EEL_xno em_ReadMIDI(EEL_vm *vm)
{
	EEL_xno x = alsaseq_read(&emd);
	if(x == -1)
	{
		eel_nil2v(vm->heap + vm->resv);
		return EEL_XNONE;
	}
	else if(x)
		return x;
	eel_o2v(vm->heap + vm->resv, emd.msg);
	emd.msg = NULL;
	return EEL_XNONE;
}


static EEL_xno em_EventName(EEL_vm *vm)
{
	const char *s;
	EEL_object *o;
	switch(eel_v2l(vm->heap + vm->argv))
	{
	  case EM_NOTEOFF:		s = "NOTEOFF"; break;
	  case EM_NOTEON:		s = "NOTEON"; break;
	  case EM_AFTERTOUCH:		s = "AFTERTOUCH"; break;
	  case EM_CONTROLCHANGE:	s = "CONTROLCHANGE"; break;
	  case EM_PROGRAMCHANGE:	s = "PROGRAMCHANGE"; break;
	  case EM_CHANNELPRESSURE:	s = "CHANNELPRESSURE"; break;
	  case EM_PITCHBEND:		s = "PITCHBEND"; break;
	  case EM_SYSTEM:		s = "SYSTEM"; break;
	  case EM_RPN:			s = "RPN"; break;
	  case EM_NRPN:			s = "NRPN"; break;
	  case EM_UNKNOWN:		s = "UNKNOWN"; break;
	  default:
		return EEL_XWRONGINDEX;
	}
	o = eel_ps_new(vm, s);
	if(!s)
		return EEL_XCONSTRUCTOR;
	eel_o2v(vm->heap + vm->resv, o);
	return EEL_XNONE;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno em_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		alsaseq_close(&emd);
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp em_constants[] =
{
	/* MIDI messages */
	{"NOTEOFF",		EM_NOTEOFF		},
	{"NOTEON",		EM_NOTEON		},
	{"AFTERTOUCH",		EM_AFTERTOUCH		},
	{"CONTROLCHANGE",	EM_CONTROLCHANGE	},
	{"PROGRAMCHANGE",	EM_PROGRAMCHANGE	},
	{"CHANNELPRESSURE",	EM_CHANNELPRESSURE	},
	{"PITCHBEND",		EM_PITCHBEND		},
	{"SYSTEM",		EM_SYSTEM		},
	{"RPN",			EM_RPN			},
	{"NRPN",		EM_NRPN			},
	{"UNKNOWN",		EM_UNKNOWN		},
	{NULL, 0}
};

EEL_xno eel_midi_init(EEL_vm *vm)
{
	EEL_object *m;
	if(loaded)
		return EEL_XDEVICEOPENED;

	memset(&emd, 0, sizeof(emd));
	emd.vm = vm;

	m = eel_create_module(vm, "midi", em_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	eel_export_cfunction(m, 0, "OpenMIDI", 0, 0, 0, em_OpenMIDI);
	eel_export_cfunction(m, 0, "CloseMIDI", 0, 0, 0, em_CloseMIDI);
	eel_export_cfunction(m, 1, "ReadMIDI", 0, 0, 0, em_ReadMIDI);

	eel_export_cfunction(m, 1, "EventName", 1, 0, 0, em_EventName);

	/* Constants and enums */
	eel_export_lconstants(m, em_constants);

	loaded = 1;
	eel_disown(m);
	return EEL_XNONE;
}
