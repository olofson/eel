AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS = net2 midi zeespace zeedraw audiality2
DIST_SUBDIRS = $(SUBDIRS)

INCLUDES = -I../include -Inet2 -Imidi -Izeespace -Izeedraw -Iaudiality2 @LOCALCFLAGS@

bin_PROGRAMS = eelbox

eelbox_SOURCES = eelbox.c eb_sdl.c eb_image.c eb_net.c eb_opengl.c	\
		sfifo.c IMG_savepng.c eel_physics.c

eelbox_LDADD = net2/libnet2.a midi/libmidi.a zeespace/libzeespace.a	\
		 zeedraw/libzeedraw.a audiality2/libaudiality2.a @EELBOX_LDADD@

EXTRA_DIST = *.h *.eel *.png cfg-* *.a2s *.res

ACLOCAL_AMFLAGS = -I m4
