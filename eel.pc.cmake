# eel pkg-config source file

prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}
includedir=${prefix}/include/EEL

Name: EEL
Description: The Extensible Embeddable Language, a programming/scripting language for realtime applications.
Version: @VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@
Requires:
Conflicts:
Libs: -L${libdir} -leel
Libs.private:
Cflags: -I${includedir}
