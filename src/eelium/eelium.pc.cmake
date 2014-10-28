# @EEL_AUTO_MESSAGE@

# eelium pkg-config source file

prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}
includedir=${prefix}/include/EEL

Name: EELIUM
Description: EEL bindings for SDL, OpenGL, Audiality 2 etc.
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
Requires:
Conflicts:
Libs: -L${libdir} -leelium
Libs.private:
Cflags: -I${includedir}
