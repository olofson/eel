AUTOMAKE_OPTIONS = 1.4 foreign

SUBDIRS = src include docs tools

DIST_SUBDIRS = $(SUBDIRS)

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = eel.pc

EXTRA_DIST = eel.m4 cfg-* BUGS BRAINSTORM COPYING*

CLEANFILES = eel.pc eel-config

bin_SCRIPTS = eel-config

m4datadir = $(datadir)/aclocal
m4data_DATA = eel.m4

ACLOCAL_AMFLAGS = -I m4 --install

#
# Various extra rules (some stolen from SDL)
#

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to install the libraries only - prevent rebuilding apps
install-lib:
	$(MAKE) install-libLTLIBRARIES

# Run ldconfig after installing the library:
install-exec-hook:
	-ldconfig

# Grab the test programs and docs for the distribution:
dist-hook:
	if test -f test/Makefile; then (cd test; make distclean); fi
	cp -rp $(srcdir)/test $(distdir)
	cp -rp $(srcdir)/eelbox $(distdir)
	cp -rp $(srcdir)/include $(distdir)
	cp -rp $(srcdir)/docs $(distdir)
	rm -rf `find $(distdir) -type d -name CVS -print`

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
