
SUBDIRS = eelc system io dir math dsp

builtin.c:	builtin.eel text2c.sed
		sed -f text2c.sed builtin.eel > builtin.c

EEL_LIB = libEEL.la

lib_LTLIBRARIES = $(EEL_LIB)

INCLUDES = -I../include -Ieelc -Isystem -Iio -Idir -Imath -Idsp		\
	@LOCALCFLAGS@ -include e_config.h

BUILT_SOURCES = builtin.c
CLEANFILES = builtin.c

libEEL_la_SOURCES =							\
	e_state.c e_register.c e_util.c	e_error.c e_vm.c e_object.c	\
	e_operate.c e_cast.c e_builtin.c e_module.c e_function.c	\
	e_class.c e_vector.c e_array.c e_table.c e_string.c e_dstring.c

libEEL_la_LDFLAGS = @EEL_LDFLAGS@				\
	-release $(LT_RELEASE)					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libEEL_la_LIBADD = @SYSTEM_LIBS@ eelc/libeelc.la	\
	io/libeelio.la system/libeelsystem.la dir/libeeldir.la math/libeelmath.la dsp/libeeldsp.la

libEEL_la_DEPENDENCIES = eelc/libeelc.la	\
	io/libeelio.la system/libeelsystem.la dir/libeeldir.la math/libeelmath.la dsp/libeeldsp.la

bin_PROGRAMS = eel

eel_LDADD = libEEL.la

EXTRA_DIST = *.h *.eel *.sed
