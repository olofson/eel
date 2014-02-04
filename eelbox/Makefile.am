AUTOMAKE_OPTIONS = no-dependencies foreign

SUBDIRS = net2 midi zeespace zeedraw chipsound
DIST_SUBDIRS = $(SUBDIRS)

INCLUDES = -I../include -Inet2 -Imidi -Izeespace -Izeedraw -Ichipsound @LOCALCFLAGS@

bin_PROGRAMS = eelbox

eelbox_SOURCES = eelbox.c eb_sdl.c eb_image.c eb_net.c eb_opengl.c	\
		sfifo.c IMG_savepng.c eel_physics.c

eelbox_LDADD = net2/libnet2.a midi/libmidi.a zeespace/libzeespace.a	\
		 zeedraw/libzeedraw.a chipsound/libchipsound.a @EELBOX_LDADD@

EXTRA_DIST = *.h *.eel *.png cfg-* *.csl *.res

ACLOCAL_AMFLAGS = -I m4
