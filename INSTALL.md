EEL
===

This file explains how to build and install EEL from source.

Installing
----------

* Install the dependencies. Most of these should actually be optional (build scripts need some work), but you'll want most of them for a fully functional Eelium executive anyway.
  * SDL 2.0
    * http://libsdl.org
  * SDL_net 2.0
    * https://www.libsdl.org/projects/SDL_net/
  * SDL_image 2.0
    * https://www.libsdl.org/projects/SDL_image/
  * libpng (needed for PNG support in SDL_image as well!)
    * http://www.libpng.org/pub/png/libpng.html
  * Audiality 2 1.9.2 or later
    * https://github.com/olofson/audiality2
  * MXE (optional, for cross-compiling Windows binaries)
    * https://mxe.cc/

* Download the source code.
  * GitHub/SSH
    * git clone git@github.com:olofson/eel.git
  * *Alternatively:* GitHub/HTTPS
    * git clone https://github.com/olofson/eel.git
  * Archive
    * http://eelang.org/

* Configure the source tree.
  * Option 1 (Un*x with bash or similar shell)
    * ./configure [*target*]
      * Currently available targets:
        * release
        * static
        * maintainer
        * debug
        * mingw-release
        * mingw-debug
        * mingw-64-release
        * mingw-64-debug
        * mingw-release-static
        * mingw-debug-static
        * mingw-64-release-static
        * mingw-64-debug-static
        * all *(all of the above)*
        * (Default if not specified: release)
  * Option 2 (Using CMake directly)
    * mkdir build
    * cd build
    * cmake ..
    * NOTE: Building this way will NOT update src/core/builtin.c if builtin.eel is modified! This is done by the configure script.

* Build and install.
  * ./make-all
    * The resulting executables are found in "build/<target>/src/"
  * *Alternatively:* Enter the desired build target directory.
    * make
    * *Optional:* sudo make install

* Cleaning up after building in-tree:
  * ./clean-all
    * Removes all build files generated by ./make-all
