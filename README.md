EEL - Extensible Embeddable Language
====================================

EEL is a scripting and programming language, designed specifically for hard real time applications. The primary target areas of application are control engineering and audio synthesis, but EEL should also be suitable for game scripting and for adding scripting capabilities to real time multimedia applications.

The syntax is C-like, but the language is higher level than C, and "safe" in the sense that EEL programs should not be able to crash the virtual machine or the host application. EEL has dynamic typing, automatic memory management, exception handling and built-in high level data types such as vectors, arrays and tables. New such data types can be added at run time by host applications.

EEL compiles into byte-code that runs on a virtual machine, which means that no explicit support for specific achitectures is needed for portability. The implementation is deliberately very self contained and has few dependencies, so that it can be easily integrated into "strange" environments, such as embedded devices running real time operating systems.

Installing
----------

* Install the dependencies. Most of these should actually be optional (build scripts need some work), but you'll want most of them for a fully functional Eelium executive anyway.
  * SDL 1.2
    * http://libsdl.org
  * SDL_net 1.2
    * http://www.libsdl.org/projects/SDL_net/release-1.2.html
  * SDL_image 1.2
    * https://www.libsdl.org/projects/SDL_image/release-1.2.html
  * libpng (needed for PNG support in SDL_image as well!)
    * http://www.libpng.org/pub/png/libpng.html
  * Audiality 2
    * https://github.com/olofson/audiality2
  * MXE (optional; needed for cross-compiling Windows binaries)
    * http://mxe.cc/

* Download the source code.
  * Archive
    * http://eelang.org/
  * GitHub
    * git clone git@github.com:olofson/eel.git

* Configure the source tree.
  * Option 1 (Un*x with bash or similar shell)
    * ./cfg-all
  * Option 2 (Using CMake directly)
    * mkdir build
    * cd build
    * cmake ..
    * NOTE: Building this way will NOT update src/core/builtin.c if builtin.eel is modified! This is done by the cfg-all script.

* Build and install.
  * Enter the desired build directory. (cfg-all creates a few different ones under "build".)
  * make
  * sudo make install
