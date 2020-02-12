/*
  Based on zlib license - see http://www.gzip.org/zlib/zlib_license.html

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  "Philip D. Bober" <wildfire1138@mchsi.com>

  David Olofson <david@olofson.net> 2010:
	* 'compression' argument renamed as 'flags'
	* IMG_COMPRESS_DEFAULT constant is now 0xff instead of -1
	* Added IMG_UPSIDEDOWN for saving OpenGL screenshots and the like

  David Olofson <david@olofson.net> 2017:
	* Renamed symbols to avoid conflicts with SDL2_image.
 */
#ifndef __EEL_SAVETOPNG_H__
#define __EEL_SAVETOPNG_H__

/* #include <SDL/begin_code.h> */

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_COMPRESS_MASK	0x000000ff
#define IMG_COMPRESS_OFF	0
#define IMG_COMPRESS_MAX	9
#define IMG_COMPRESS_DEFAULT	0xff
#define IMG_UPSIDEDOWN		0x00010000

/**
 * Takes a filename, a surface to save, and a compression level.  The
 * compression level can be 0(min) through 9(max), or -1(default).
 */
DECLSPEC int SDLCALL EEL_SavePNG(const char *file,
		SDL_Surface *surf, int flags);
/**
 * Takes a SDL_RWops pointer, a surface to save, and a compression level.
 * compression can be 0(min) through 9(max), or -1(default).
 */
DECLSPEC int SDLCALL EEL_SavePNG_RW(SDL_RWops *src,
		SDL_Surface *surf, int flags);
#ifdef __cplusplus
}
#endif

#endif/*__EEL_SAVETOPNG_H__*/
