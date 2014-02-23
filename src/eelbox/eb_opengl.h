/*
---------------------------------------------------------------------------
	eb_opengl.h - EEL OpenGL Binding
---------------------------------------------------------------------------
 * Copyright 2010-2012, 2014 David Olofson
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef EELBOX_OPENGL_H
#define EELBOX_OPENGL_H

/* Use vertex arrays where appropriate */
#undef EELBOX_USE_ARRAYS

/*
 * We're a "friend" module with SDL, in order to support SDL surfaces as an
 * interface to OpenGL textures.
 */
#include "eb_sdl.h"

#ifdef HAS_SDL_OPENGL_H
# include "SDL_opengl.h"
#else
# ifdef WIN32
#  include <windows.h>
# endif /* WIN32 */
# if defined(__APPLE__) && defined(__MACH__)
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
# else
#  include <GL/gl.h>
#  include <GL/glu.h>
# endif
#endif /* HAS_SDL_OPENGL_H */

#if defined(_WIN32) && !defined(APIENTRY) && \
		!defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
#include <windows.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

/*
 * Legacy OpenGL stuff
 */
#ifndef	GL_GENERATE_MIPMAP
#	define	GL_GENERATE_MIPMAP	0x8191
#endif
#ifndef	GL_GENERATE_MIPMAP_HINT
#	define	GL_GENERATE_MIPMAP_HINT	0x8192
#endif


/*
 * Texture management
 */

typedef enum
{
	EBGL_AUTOMIPMAP =	0x00000001,	/* Build mipmaps automatically */
	EBGL_HCLAMP =		0x00000002,	/* Clamp horizontally at edges */
	EBGL_VCLAMP =		0x00000004,	/* Clamy vertically at edges */
	EBGL_BUFFERED =		0x00010000	/* Keep off-context buffer */
} EBGL_texflags;

#if 0
typedef struct EBGL_texture EBGL_texture;
struct EBGL_texture
{
	EBGL_texture	*next, *prev;
	EBGL_texflags	flags;
	GLuint		name;
	SDL_Surface	*cache;
};
EEL_MAKE_CAST(EBGL_texture)
#endif

/*
 * OpenGL binding module data, with OpenGL entry points etc...
 */
typedef struct
{
	SDL_PixelFormat		RGBfmt;
	SDL_PixelFormat		RGBAfmt;

	int	version;	/* OpenGL version (MAJOR.MINOR) * 10 */

	/*
	 * OpenGL 1.1 core functions
	 */
	/* Miscellaneous */
	GLenum	(APIENTRY *GetError)(void);
	void	(APIENTRY *GetDoublev)(GLenum pname, GLdouble *params);
	const GLubyte* (APIENTRY *GetString)(GLenum name);
	void	(APIENTRY *Disable)(GLenum cap);
	void	(APIENTRY *Enable)(GLenum cap);
	void	(APIENTRY *BlendFunc)(GLenum, GLenum);
	void	(APIENTRY *ClearColor)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
	void	(APIENTRY *Clear)(GLbitfield mask);
	void	(APIENTRY *Flush)(void);
	void	(APIENTRY *Hint)(GLenum target, GLenum mode);
	void	(APIENTRY *DisableClientState)(GLenum cap);
	void	(APIENTRY *EnableClientState)(GLenum cap);

	/* Depth Buffer*/
	void	(APIENTRY *ClearDepth)(GLclampd depth);
	void	(APIENTRY *DepthFunc)(GLenum func);

	/* Transformation */
	void	(APIENTRY *MatrixMode)(GLenum mode);
	void	(APIENTRY *Ortho)(GLdouble left, GLdouble right, GLdouble bottom,
			GLdouble top, GLdouble zNear, GLdouble zFar);
	void	(APIENTRY *Frustum)(GLdouble left, GLdouble right, GLdouble bottom,
			GLdouble top, GLdouble zNear, GLdouble zFar);
	void	(APIENTRY *Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
	void	(APIENTRY *PushMatrix)(void);
	void	(APIENTRY *PopMatrix)(void);
	void	(APIENTRY *LoadIdentity)(void);
	void	(APIENTRY *LoadMatrixd)(const GLdouble *m);
	void	(APIENTRY *MultMatrixd)(const GLdouble *m);
	void	(APIENTRY *Rotated)(GLdouble, GLdouble, GLdouble, GLdouble);
	void	(APIENTRY *Scaled)(GLdouble, GLdouble, GLdouble);
	void	(APIENTRY *Translated)(GLdouble x, GLdouble y, GLdouble z);

	/* Textures */
	GLboolean (APIENTRY *IsTexture)(GLuint texture);
	void	(APIENTRY *GenTextures)(GLsizei n, GLuint *textures);
	void	(APIENTRY *BindTexture)(GLenum, GLuint);
	void	(APIENTRY *DeleteTextures)(GLsizei n, const GLuint *textures);
	void	(APIENTRY *TexImage2D)(GLenum target, GLint level, GLint internalformat,
			GLsizei width, GLsizei height, GLint border,
			GLenum format, GLenum type, const GLvoid *pixels);
	void	(APIENTRY *TexParameteri)(GLenum target, GLenum pname, GLint param);
	void	(APIENTRY *TexParameterf)(GLenum target, GLenum pname, GLfloat param);
	void	(APIENTRY *TexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
	void	(APIENTRY *TexSubImage2D)(GLenum target, GLint level,
			GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
			GLenum format, GLenum type, const GLvoid *pixels);

	/* Lighting */
	void	(APIENTRY *ShadeModel)(GLenum mode);

	/* Raster */
	void	(APIENTRY *PixelStorei)(GLenum pname, GLint param);
	void	(APIENTRY *ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height,
			GLenum format, GLenum type, GLvoid *pixels);

	/* Drawing */
	void	(APIENTRY *Begin)(GLenum);
	void	(APIENTRY *End)(void);
	void	(APIENTRY *Vertex2d)(GLdouble x, GLdouble y);
	void	(APIENTRY *Vertex3d)(GLdouble x, GLdouble y, GLdouble z);
	void	(APIENTRY *Vertex4d)(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
	void	(APIENTRY *Normal3d)(GLdouble nx, GLdouble ny, GLdouble nz);
	void	(APIENTRY *Color3d)(GLdouble red, GLdouble green, GLdouble blue);
	void	(APIENTRY *Color4d)(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
	void	(APIENTRY *TexCoord1d)(GLdouble s);
	void	(APIENTRY *TexCoord2d)(GLdouble s, GLdouble t);
	void	(APIENTRY *TexCoord3d)(GLdouble s, GLdouble t, GLdouble u);
	void	(APIENTRY *TexCoord4d)(GLdouble s, GLdouble t, GLdouble u, GLdouble v);

	/* Arrays */
	void	(APIENTRY *VertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
	void	(APIENTRY *TexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
	void	(APIENTRY *DrawArrays)(GLenum mode, GLint first, GLsizei count);

	/*
	 * Calls below this point may be missing - CHECK FOR NULL!!!
	 */

		/* OpenGL 1.2 core functions */
	void	(APIENTRY *_BlendEquation)(GLenum);

	/* 3.0+ Texture handling */
	void	(APIENTRY *_GenerateMipmap)(GLenum target);

	/* State cache */
	int	do_blend;
	int	do_texture;
	GLenum	sfactor, dfactor;
	GLuint	texture2d;

	
	
	/* True if Load() has been called. (Causes OpenGL reload as needed.) */
	int	was_loaded;

	/* OpenGL library path */
	char	*libname;
} EB_gl_md;

extern EB_gl_md ebgl_md;

EEL_xno ebgl_init(EEL_vm *vm);

/* Wire all core OpenGL calls to dummies, to avoid segfaults! */
void ebgl_dummy_calls(void);

EEL_xno ebgl_load(int force);


/*
 * OpenGL state control
 */

static inline void gl_Reset(void)
{
	ebgl_md.do_blend = -1;
	ebgl_md.do_texture = -1;
	ebgl_md.texture2d = -1;
	ebgl_md.sfactor = 0xffffffff;
	ebgl_md.dfactor = 0xffffffff;
}

static inline int *gl_GetState(GLenum cap)
{
	switch(cap)
	{
	  case GL_BLEND:
		return &ebgl_md.do_blend;
	  case GL_TEXTURE_2D:
		return &ebgl_md.do_texture;
	  default:
		return NULL;
	}
}

static inline void gl_Enable(GLenum cap)
{
	int *state = gl_GetState(cap);
	if(!state || (*state != 1))
	{
		ebgl_md.Enable(cap);
		if(state)
			*state = 1;
	}
}

static inline void gl_Disable(GLenum cap)
{
	int *state = gl_GetState(cap);
	if(!state || (*state != 0))
	{
		ebgl_md.Disable(cap);
		if(state)
			*state = 0;
	}
}

static inline void gl_BlendFunc(GLenum sfactor, GLenum dfactor)
{
	if((sfactor == ebgl_md.sfactor) && (dfactor == ebgl_md.dfactor))
		return;
	ebgl_md.BlendFunc(sfactor, dfactor);
	ebgl_md.sfactor = sfactor;
	ebgl_md.dfactor = dfactor;
}

static inline void gl_BindTexture(GLenum target, GLuint tx)
{
	if(target != GL_TEXTURE_2D)
	{
		ebgl_md.BindTexture(target, tx);
		return;
	}
	if(tx == ebgl_md.texture2d)
		return;
	ebgl_md.BindTexture(target, tx);
	ebgl_md.texture2d = tx;
}


#if 0
/*
 * Texture management
 */

static inline void gl_link_tex(EBGL_texture *tex)
{
	if(ebgl_md->last)
	{
		ebgl_md->last->next = tex;
		tex->prev = ebgl_md->last;
		tex->next = NULL;
		ebgl_md->last = tex;
	}
	else
	{
		ebgl_md->first = ebgl_md->last = tex;
		tex->next = tex->prev = NULL;
	}
}

static inline void gl_unlink_tex(EBGL_texture *tex)
{
	if(tex->prev)
		tex->prev->next = tex->next;
	else
		ebgl_md->first = tex->next;
	if(tex->next)
	{
		tex->next->prev = tex->prev;
		tex->next = NULL;
	}
	else
		ebgl_md->last = tex->prev;
	tex->prev = NULL;
}
#endif

#endif /* EELBOX_OPENGL_H */
