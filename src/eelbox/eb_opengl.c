/*
---------------------------------------------------------------------------
	eb_opengl.c - EEL OpenGL Binding
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

#include "eb_opengl.h"
#include <math.h>

EB_gl_md ebgl_md;

/* Module loaded? */
static int loaded = 0;

/*
 * DEBUGGING:
 *	When this is set, glGetError() is called after every OpenGL call, and
 *	if there is an error, an EEL exceptions is thrown.
 *
 *	NOTE:
 *		This is a debugging feature that adds significant overhead!
 */
static int exceptions = 0;

static int getglexception(void)
{
/*
FIXME: With some later EEL version, specific exceptions should be registered
FIXME: for these!
*/
	switch(ebgl_md.GetError())
	{
	  case GL_NO_ERROR:
		return 0;
	  case GL_INVALID_ENUM:
		return EEL_XARGUMENTS;
	  case GL_INVALID_VALUE:
		return EEL_XBADVALUE;
	  case GL_INVALID_OPERATION:
		return EEL_XILLEGALOPERATION;
	  case GL_STACK_OVERFLOW:
		return EEL_XOVERFLOW;
	  case GL_STACK_UNDERFLOW:
		return EEL_XUNDERFLOW;
	  case GL_OUT_OF_MEMORY:
		return EEL_XMEMORY;
	  default:
		return EEL_XDEVICEERROR;
	}
}

static inline int glexception(void)
{
	if(!exceptions)
		return 0;
	return getglexception();
}



/*----------------------------------------------------------
	OpenGL library loading/unloading
----------------------------------------------------------*/

static struct
{
	const char	*name;
	void		**fn;
} glfuncs[] = {
	/*
	 * OpenGL 1.1 core functions - if not present, we bail out!
	 */

	/* Miscellaneous */
	{"glGetError", (void *)&ebgl_md.GetError },
	{"glGetDoublev", (void *)&ebgl_md.GetDoublev },
	{"glGetString", (void *)&ebgl_md.GetString },
	{"glDisable", (void *)&ebgl_md.Disable },
	{"glEnable", (void *)&ebgl_md.Enable },
	{"glBlendFunc", (void *)&ebgl_md.BlendFunc },
	{"glClearColor", (void *)&ebgl_md.ClearColor },
	{"glClear", (void *)&ebgl_md.Clear },
	{"glFlush", (void *)&ebgl_md.Flush },
	{"glHint", (void *)&ebgl_md.Hint },
	{"glDisableClientState", (void *)&ebgl_md.DisableClientState },
	{"glEnableClientState", (void *)&ebgl_md.EnableClientState },

	/* Depth Buffer */
	{"glClearDepth", (void *)&ebgl_md.ClearDepth },
	{"glDepthFunc", (void *)&ebgl_md.DepthFunc },

	/* Transformation */
	{"glMatrixMode", (void *)&ebgl_md.MatrixMode },
	{"glOrtho", (void *)&ebgl_md.Ortho },
	{"glFrustum", (void *)&ebgl_md.Frustum },
	{"glViewport", (void *)&ebgl_md.Viewport },
	{"glPushMatrix", (void *)&ebgl_md.PushMatrix },
	{"glPopMatrix", (void *)&ebgl_md.PopMatrix },
	{"glLoadIdentity", (void *)&ebgl_md.LoadIdentity },
	{"glLoadMatrixd", (void *)&ebgl_md.LoadMatrixd },
	{"glMultMatrixd", (void *)&ebgl_md.MultMatrixd },
	{"glRotated", (void *)&ebgl_md.Rotated },
	{"glScaled", (void *)&ebgl_md.Scaled },
	{"glTranslated", (void *)&ebgl_md.Translated },

	/* Textures */
	{"glIsTexture", (void *)&ebgl_md.IsTexture },
	{"glGenTextures", (void *)&ebgl_md.GenTextures },
	{"glBindTexture", (void *)&ebgl_md.BindTexture },
	{"glDeleteTextures", (void *)&ebgl_md.DeleteTextures },
	{"glTexImage2D", (void *)&ebgl_md.TexImage2D },
	{"glTexParameteri", (void *)&ebgl_md.TexParameteri },
	{"glTexParameterf", (void *)&ebgl_md.TexParameterf },
	{"glTexParameterfv", (void *)&ebgl_md.TexParameterfv },
	{"glTexSubImage2D", (void *)&ebgl_md.TexSubImage2D },

	/* Lighting */
	{"glShadeModel", (void *)&ebgl_md.ShadeModel },
	
	/* Raster */
	{"glPixelStorei", (void *)&ebgl_md.PixelStorei },
	{"glReadPixels", (void *)&ebgl_md.ReadPixels },

	/* Drawing */
	{"glBegin", (void *)&ebgl_md.Begin },
	{"glEnd", (void *)&ebgl_md.End },
	{"glVertex2d", (void *)&ebgl_md.Vertex2d },
	{"glVertex3d", (void *)&ebgl_md.Vertex3d },
	{"glVertex4d", (void *)&ebgl_md.Vertex4d },
	{"glNormal3d", (void *)&ebgl_md.Normal3d },
	{"glColor3d", (void *)&ebgl_md.Color3d },
	{"glColor4d", (void *)&ebgl_md.Color4d },
	{"glTexCoord1d", (void *)&ebgl_md.TexCoord1d },
	{"glTexCoord2d", (void *)&ebgl_md.TexCoord2d },
	{"glTexCoord3d", (void *)&ebgl_md.TexCoord3d },
	{"glTexCoord4d", (void *)&ebgl_md.TexCoord4d },

	/* Arrays */
	{"glVertexPointer", (void *)&ebgl_md.VertexPointer },
	{"glTexCoordPointer", (void *)&ebgl_md.TexCoordPointer },
	{"glDrawArrays", (void *)&ebgl_md.DrawArrays },

	{NULL, NULL },

	/*
	 * Calls below this point may be missing - CHECK FOR NULL!!!
	 */

	/* OpenGL 1.2 core functions */
	{"glBlendEquation", (void *)&ebgl_md._BlendEquation },

	/* 3.0+ mipmap generation */
	{"glGenerateMipmap", (void *)&ebgl_md._GenerateMipmap },
	
	{NULL, NULL }
};


static int gl_dummy(void)
{
	printf("WARNING: OpenGL call with no OpenGL driver loaded!\n");
	return -1;
}


void ebgl_dummy_calls(void)
{
	int i;
	for(i = 0; glfuncs[i].name; ++i)
		*glfuncs[i].fn = gl_dummy;
}


static int get_gl_calls(void)
{
	int i;

	/* Get core functions */
	for(i = 0; glfuncs[i].name; ++i)
	{
		*glfuncs[i].fn = SDL_GL_GetProcAddress(glfuncs[i].name);
		if(!*glfuncs[i].fn)
		{
			printf("ERROR: Required OpenGL call '%s' missing!\n",
					glfuncs[i].name);
			return -1;
		}
	}

	/* Try to get newer functions and extensions */
	for(++i; glfuncs[i].name; ++i)
	{
		*glfuncs[i].fn = SDL_GL_GetProcAddress(glfuncs[i].name);
		if(!*glfuncs[i].fn)
			printf("NOTE: %s not available.\n", glfuncs[i].name);
	}

	return 0;
}


static void get_gl_version(void)
{
	const char *s = (const char *)ebgl_md.GetString(GL_VERSION);
	ebgl_md.version = 0;
	if(s)
	{
		int major, minor;
		if(sscanf(s, "%d.%d", &major, &minor) == 2)
			ebgl_md.version = major * 10 + minor;
	}
}


static inline int gl_version(void)
{
	if(!ebgl_md.version)
		get_gl_version();
	return ebgl_md.version;
}


/*----------------------------------------------------------
	Management and utilities
----------------------------------------------------------*/

/* Create a surface of the prefered OpenGL RGB texture format */
static SDL_Surface *CreateRGBSurface(int w, int h)
{
	Uint32 rmask, gmask, bmask;
	int bits = 24;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0x00ff0000;
	gmask = 0x0000ff00;
	bmask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
#endif
	return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
			bits, rmask, gmask, bmask, 0);
}

/* Create a surface of the prefered OpenGL RGBA texture format */
static SDL_Surface *CreateRGBASurface(int w, int h)
{
	Uint32 rmask, gmask, bmask, amask;
	int bits = 32;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif
	return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
			bits, rmask, gmask, bmask, amask);
}

static void init_formats(void)
{
	SDL_Surface *s = CreateRGBSurface(1, 1);
	if(!s)
		return;
	ebgl_md.RGBfmt = *(s->format);
	SDL_FreeSurface(s);

	s = CreateRGBASurface(1, 1);
	if(!s)
		return;
	ebgl_md.RGBAfmt = *(s->format);
	SDL_FreeSurface(s);
}

EEL_xno ebgl_load(int force)
{
	gl_Reset();
	ebgl_md.version = 0;
	ebgl_dummy_calls();
	if(!force && !ebgl_md.was_loaded)
		return 0;
	if(!SDL_GL_GetProcAddress("glGetError"))
	{
		if(SDL_GL_LoadLibrary(ebgl_md.libname) < 0)
			return EEL_XDEVICEOPEN;
	}
	if(get_gl_calls() < 0)
		return EEL_XDEVICEOPEN;
	init_formats();
	ebgl_md.was_loaded = 1;
	return 0;
}

static EEL_xno gl_load(EEL_vm *vm)
{
	if(ebgl_md.libname)
	{
		free(ebgl_md.libname);
		ebgl_md.libname = NULL;
	}
	if(vm->argc)
	{
		const char *ln = eel_v2s(vm->heap + vm->argv);
		if(!ln)
			return EEL_XWRONGTYPE;
		ebgl_md.libname = strdup(ln);
	}
	return ebgl_load(1);
}


static EEL_xno gl_exceptions(EEL_vm *vm)
{
	exceptions = (eel_v2l(vm->heap + vm->argv) != 0);
	return 0;
}


static EEL_xno gl_setattribute(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int attr = eel_v2l(args);
	if(attr == GL_DOUBLEBUFFER)
		attr = SDL_GL_DOUBLEBUFFER;
	SDL_GL_SetAttribute(attr, eel_v2l(args + 1));
	return 0;
}


static EEL_xno gl_perspective(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	GLdouble fovy, aspect, znear, zfar;
	GLdouble xmin, xmax, ymin, ymax;
	fovy = eel_v2d(args);
	aspect = eel_v2d(args + 1);
	znear = eel_v2d(args + 2);
	zfar = eel_v2d(args + 3);
	ymax = znear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;
	ebgl_md.Frustum(xmin, xmax, ymin, ymax, znear, zfar);
	return glexception();
}


static EEL_xno gl_swapbuffers(EEL_vm *vm)
{
	SDL_GL_SwapBuffers();
	return 0;
}


static EEL_xno gl_uploadtexture(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	GLuint tex;
	int flags = 0;
	SDL_Surface *s, *tmp = NULL;

	/* Get the source SDL surface */
	if(EEL_TYPE(args) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	s = o2EB_surface(args->objref.v)->surface;
	/* Any flags? */
	if(vm->argc >= 2)
		flags = eel_v2l(args + 1);

	/* New texture, or replace an existing one? */
	if(vm->argc >= 3)
		tex = eel_v2l(args + 2);
	else
		ebgl_md.GenTextures(1, &tex);

	/* Convert */
/* TODO: Optimization: Convert only when OpenGL can't handle it! */
	tmp = SDL_ConvertSurface(s,
			s->format->Amask || s->flags & SDL_SRCCOLORKEY ?
					&ebgl_md.RGBAfmt : &ebgl_md.RGBfmt,
			SDL_SWSURFACE);

	/* Setup... */
	ebgl_md.BindTexture(GL_TEXTURE_2D, tex);
	ebgl_md.PixelStorei(GL_UNPACK_ROW_LENGTH,
			tmp->pitch / tmp->format->BytesPerPixel);

	ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if(flags & EBGL_AUTOMIPMAP)
	{
		/* If mipmaps requested, try to handle it one way or another. */
		ebgl_md.Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR_MIPMAP_LINEAR);
		if(ebgl_md._GenerateMipmap)
		{
			/* The nice, efficient OpenGL 3.0 way */
			ebgl_md.Enable(GL_TEXTURE_2D);	/* For the ATI bug! */
		}
		else if((gl_version() >= 14) && (gl_version() < 31))
		{
			/* The OpenGL 1.4 way */
			ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP,
					GL_TRUE);
		}
		else
		{
/*TODO: Custom mipmap generator, to cover any OpenGL version! */
			/* Give up! No mipmapping... */
			ebgl_md.TexParameteri(GL_TEXTURE_2D,
					GL_TEXTURE_MIN_FILTER,
					GL_LINEAR);
		}
	}
	else
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				GL_LINEAR);

	if(flags & EBGL_HCLAMP)
	{
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
				GL_CLAMP_TO_EDGE);
	}
	else
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

	if(flags & EBGL_VCLAMP)
	{
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
				GL_CLAMP_TO_EDGE);
	}
	else
		ebgl_md.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	/* Upload! */
	SDL_LockSurface(tmp);
	ebgl_md.TexImage2D(GL_TEXTURE_2D, 0,
			tmp->format->Amask ? GL_RGBA8 : GL_RGB8,
			tmp->w, tmp->h, 0,
			tmp->format->Amask ? GL_RGBA : GL_RGB,
			GL_UNSIGNED_BYTE, (char *)tmp->pixels);
	SDL_UnlockSurface(tmp);

	SDL_FreeSurface(tmp);

	if(flags & EBGL_AUTOMIPMAP)
		if(ebgl_md._GenerateMipmap)
			ebgl_md._GenerateMipmap(GL_TEXTURE_2D);

	/* Return the texture name */
	eel_l2v(vm->heap + vm->resv, tex);
	return glexception();
}


/*----------------------------------------------------------
	Miscellaneous
----------------------------------------------------------*/

static EEL_xno gl_getstring(EEL_vm *vm)
{
	const GLubyte *s = ebgl_md.GetString(eel_v2l(vm->heap + vm->argv));
	if(s)
		eel_s2v(vm, vm->heap + vm->resv, (const char *)s);
	else
		eel_nil2v(vm->heap + vm->resv);
	return glexception();
}


static EEL_xno gl_disable(EEL_vm *vm)
{
	gl_Disable(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


static EEL_xno gl_enable(EEL_vm *vm)
{
	gl_Enable(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


static EEL_xno gl_blendfunc(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	gl_BlendFunc(eel_v2l(args), eel_v2l(args + 1));
	return glexception();
}


static EEL_xno gl_blendequation(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(!ebgl_md._BlendEquation)
		return EEL_XNOTIMPLEMENTED;
	ebgl_md._BlendEquation(eel_v2l(args));
	return glexception();
}


static EEL_xno gl_clearcolor(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.ClearColor(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2), eel_v2d(args + 3));
	return glexception();
}


static EEL_xno gl_clear(EEL_vm *vm)
{
	ebgl_md.Clear(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


static EEL_xno gl_hint(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Hint(eel_v2l(args), eel_v2l(args + 1));
	return glexception();
}


static EEL_xno gl_readpixels(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	int sx, sy, dx, dy, w, h;
	SDL_Surface *dst;
	int vw = o2EB_surface(eb_md.video_surface)->surface->w;
	int vh = o2EB_surface(eb_md.video_surface)->surface->h;
	if(EEL_TYPE(args) == EEL_TNIL)
	{
		sx = sy = 0;
		w = vw;
		h = vh;
	}
	else if(EEL_TYPE(args) == eb_md.rect_cid)
	{
		SDL_Rect *r = o2SDL_Rect(args->objref.v);
		sx = r->x;
		sy = r->y;
		w = r->w;
		h = r->h;
		if(w > vw)
			w = vw;
		if(h > vh)
			h = vh;
	}
	else
		return EEL_XWRONGTYPE;

	if(EEL_TYPE(args + 1) != eb_md.surface_cid)
		return EEL_XWRONGTYPE;
	dst = o2EB_surface(args[1].objref.v)->surface;

	if((vm->argc < 3) || (EEL_TYPE(args + 2) == EEL_TNIL))
		dx = dy = 0;
	else if(EEL_TYPE(args + 2) == eb_md.rect_cid)
	{
		SDL_Rect *r = o2SDL_Rect(args[2].objref.v);
		dx = r->x;
		dy = r->y;
		if(dx >= vw)
			return 0;
		if(dy >= vh)
			return 0;
		if(dx < 0)
		{
			w += dx;
			sx -= dx;
			dx = 0;
		}
		if(dy < 0)
		{
			h += dy;
			sy -= dy;
			dy = 0;
		}
	}
	else
		return EEL_XWRONGTYPE;

	if((w <= 0) || (h <= 0))
		return 0;
/*
FIXME: This should support any SDL pixel format...
*/
	if(dst->format->BytesPerPixel != 3)
		return EEL_XWRONGTYPE;

	SDL_LockSurface(dst);
	ebgl_md.PixelStorei(GL_PACK_ALIGNMENT, 4);
	ebgl_md.PixelStorei(GL_PACK_ROW_LENGTH, dst->pitch /
			dst->format->BytesPerPixel);
	ebgl_md.ReadPixels(sx, sy, w, h, GL_RGB, GL_UNSIGNED_BYTE,
			dst->pixels + dx * dst->format->BytesPerPixel +
			dy * dst->pitch);
	SDL_UnlockSurface(dst);
	return 0;
}



/*----------------------------------------------------------
	Depth Buffer
----------------------------------------------------------*/

static EEL_xno gl_cleardepth(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.ClearDepth(eel_v2d(args));
	return glexception();
}


static EEL_xno gl_depthfunc(EEL_vm *vm)
{
	ebgl_md.DepthFunc(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


/*----------------------------------------------------------
	Transformation
----------------------------------------------------------*/

static EEL_xno gl_matrixmode(EEL_vm *vm)
{
	ebgl_md.MatrixMode(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


static EEL_xno gl_ortho(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Ortho(eel_v2d(args), eel_v2d(args + 1),
			eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5));
	return glexception();
}


static EEL_xno gl_frustum(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Frustum(eel_v2d(args), eel_v2d(args + 1),
			eel_v2d(args + 2), eel_v2d(args + 3),
			eel_v2d(args + 4), eel_v2d(args + 5));
	return glexception();
}


static EEL_xno gl_viewport(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Viewport(eel_v2l(args), eel_v2l(args + 1), eel_v2l(args + 2),
			eel_v2l(args + 3));
	return glexception();
}


static EEL_xno gl_pushmatrix(EEL_vm *vm)
{
	ebgl_md.PushMatrix();
	return glexception();
}


static EEL_xno gl_popmatrix(EEL_vm *vm)
{
	ebgl_md.PopMatrix();
	return glexception();
}


static EEL_xno gl_loadidentity(EEL_vm *vm)
{
	ebgl_md.LoadIdentity();
	return glexception();
}


static EEL_xno gl_rotate(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Rotated(eel_v2d(args), eel_v2d(args + 1), eel_v2d(args + 2),
			eel_v2d(args + 3));
	return glexception();
}


static EEL_xno gl_scale(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Scaled(eel_v2d(args), eel_v2d(args + 1), eel_v2d(args + 2));
	return glexception();
}


static EEL_xno gl_translate(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Translated(eel_v2d(args), eel_v2d(args + 1), eel_v2d(args + 2));
	return glexception();
}


/*----------------------------------------------------------
	Textures
----------------------------------------------------------*/

static EEL_xno gl_istexture(EEL_vm *vm)
{
	eel_b2v(vm->heap + vm->resv,
			ebgl_md.IsTexture(eel_v2l(vm->heap + vm->argv)));
	return glexception();
}


static EEL_xno gl_bindtexture(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	gl_BindTexture(eel_v2l(args), eel_v2l(args + 1));
	return glexception();
}


static EEL_xno gl_texparameter(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	GLenum t = eel_v2l(args);
	GLenum pn = eel_v2l(args + 1);
	if(vm->argc == 3)
	{
		if(EEL_TYPE(args + 2) == EEL_TREAL)
			ebgl_md.TexParameterf(t, pn, eel_v2d(args + 2));
		else
			ebgl_md.TexParameteri(t, pn, eel_v2l(args + 2));
	}
	else
	{
/*
FIXME: Is there a defined upper limit here? Either way, we should check count
FIXME: against parameter, to avoid math exceptions on the OpenGL side.
*/
		GLfloat v[8];
		int i;
		if(vm->argc > 2 + 8)
			return EEL_XMANYARGS;
		for(i = 0; i + 2 < vm->argc - 1; ++i)
			v[i] = eel_v2d(args + 2 + i);
		ebgl_md.TexParameterfv(t, pn, v);
	}
	return glexception();
}


static EEL_xno gl_deletetexture(EEL_vm *vm)
{
	GLuint tex;
	tex = eel_v2l(vm->heap + vm->argv);
	ebgl_md.DeleteTextures(1, &tex);
	return glexception();
}


/*----------------------------------------------------------
	Lighting
----------------------------------------------------------*/

static EEL_xno gl_shademodel(EEL_vm *vm)
{
	ebgl_md.ShadeModel(eel_v2l(vm->heap + vm->argv));
	return glexception();
}


/*----------------------------------------------------------
	Drawing
----------------------------------------------------------*/

static EEL_xno gl_begin(EEL_vm *vm)
{
	ebgl_md.Begin(eel_v2l(vm->heap + vm->argv));
	return 0;
}


static EEL_xno gl_end(EEL_vm *vm)
{
	ebgl_md.End();
	return glexception();
}

static EEL_xno gl_vertex(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	switch(vm->argc)
	{
	  case 2:
		ebgl_md.Vertex2d(eel_v2d(args), eel_v2d(args + 1));
		break;
	  case 3:
		ebgl_md.Vertex3d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2));
		break;
	  case 4:
		ebgl_md.Vertex4d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2), eel_v2d(args + 3));
		break;
	}
	return 0;
}


static EEL_xno gl_normal(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	ebgl_md.Normal3d(eel_v2d(args), eel_v2d(args + 1), eel_v2d(args + 2));
	return 0;
}


static EEL_xno gl_color(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	if(vm->argc == 3)
		ebgl_md.Color3d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2));
	else
		ebgl_md.Color4d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2), eel_v2d(args + 3));
	return 0;
}


static EEL_xno gl_texcoord(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	switch(vm->argc)
	{
	  case 1:
		ebgl_md.TexCoord1d(eel_v2d(args));
		break;
	  case 2:
		ebgl_md.TexCoord2d(eel_v2d(args), eel_v2d(args + 1));
		break;
	  case 3:
		ebgl_md.TexCoord3d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2));
		break;
	  case 4:
		ebgl_md.TexCoord4d(eel_v2d(args), eel_v2d(args + 1),
				eel_v2d(args + 2), eel_v2d(args + 3));
		break;
	}
	return 0;
}


/*----------------------------------------------------------
	Drawing tools
----------------------------------------------------------*/

static EEL_xno gl_drawrect(EEL_vm *vm)
{
	EEL_value *args = vm->heap + vm->argv;
	double tx1, ty1, tx2, ty2;
	double x = eel_v2d(args + 1);
	double y = eel_v2d(args + 2);
	double x2 = eel_v2d(args + 3);
	double y2 = eel_v2d(args + 4);
	double z = 0;
	if((vm->argc == 6) || (vm->argc == 10))
		z = eel_v2d(args + 5);
	else if((vm->argc != 5) && (vm->argc != 9))
		return EEL_XARGUMENTS;
#ifdef EELBOX_USE_ARRAYS
#warning gl_drawrect() does not implement the vertex z argument with arrays!
	GLfloat vertices[8];
	GLfloat texcoords[8];
	vertices[0] = vertices[6] = x;
	vertices[1] = vertices[3] = y;
	vertices[2] = vertices[4] = x2;
	vertices[5] = vertices[7] = y2;
#endif
	/* 'true' or 'nil' keeps the current texture, if any */
	if(EEL_TYPE(args) == EEL_TBOOLEAN)
	{
		if(args[0].integer.v == 0)
		{
			/* 'false' ==> no texture! */
			gl_Disable(GL_TEXTURE_2D);
#ifdef EELBOX_USE_ARRAYS
			ebgl_md.EnableClientState(GL_VERTEX_ARRAY);
			ebgl_md.VertexPointer(2, GL_FLOAT, 0, vertices);
			ebgl_md.DrawArrays(GL_QUADS, 0, 8);
			ebgl_md.DisableClientState(GL_VERTEX_ARRAY);
#else
			ebgl_md.Begin(GL_QUADS);
			ebgl_md.Vertex2d(x, y);
			ebgl_md.Vertex2d(x2, y);
			ebgl_md.Vertex2d(x2, y2);
			ebgl_md.Vertex2d(x, y2);
			ebgl_md.End();
#endif
			return 0;
		}
	}
	else if(EEL_TYPE(args) != EEL_TNIL)
	{
		gl_Enable(GL_TEXTURE_2D);
		gl_BindTexture(GL_TEXTURE_2D, eel_v2l(args));
	}
	if(vm->argc == 9)
	{
		tx1 = eel_v2d(args + 5);
		ty1 = eel_v2d(args + 6);
		tx2 = eel_v2d(args + 7);
		ty2 = eel_v2d(args + 8);
	}
	else if(vm->argc == 10)
	{
		tx1 = eel_v2d(args + 6);
		ty1 = eel_v2d(args + 7);
		tx2 = eel_v2d(args + 8);
		ty2 = eel_v2d(args + 9);
	}
	else
	{
		tx1 = ty1 = 0.0f;
		tx2 = ty2 = 1.0f;
	}
#ifdef EELBOX_USE_ARRAYS
	texcoords[0] = texcoords[6] = tx1;
	texcoords[1] = texcoords[3] = ty2;
	texcoords[2] = texcoords[4] = tx2;
	texcoords[5] = texcoords[7] = ty1;
	ebgl_md.EnableClientState(GL_VERTEX_ARRAY);
	ebgl_md.EnableClientState(GL_TEXTURE_COORD_ARRAY);
	ebgl_md.VertexPointer(2, GL_FLOAT, 0, vertices);
	ebgl_md.TexCoordPointer(2, GL_FLOAT, 0, texcoords);
	ebgl_md.DrawArrays(GL_QUADS, 0, 8);
	ebgl_md.DisableClientState(GL_TEXTURE_COORD_ARRAY);
	ebgl_md.DisableClientState(GL_VERTEX_ARRAY);
#else
	ebgl_md.Begin(GL_QUADS);
	if(z)
	{
		ebgl_md.TexCoord2d(tx1, ty2);
		ebgl_md.Vertex3d(x, y, z);
		ebgl_md.TexCoord2d(tx2, ty2);
		ebgl_md.Vertex3d(x2, y, z);
		ebgl_md.TexCoord2d(tx2, ty1);
		ebgl_md.Vertex3d(x2, y2, z);
		ebgl_md.TexCoord2d(tx1, ty1);
		ebgl_md.Vertex3d(x, y2, z);
	}
	else
	{
		ebgl_md.TexCoord2d(tx1, ty2);
		ebgl_md.Vertex2d(x, y);
		ebgl_md.TexCoord2d(tx2, ty2);
		ebgl_md.Vertex2d(x2, y);
		ebgl_md.TexCoord2d(tx2, ty1);
		ebgl_md.Vertex2d(x2, y2);
		ebgl_md.TexCoord2d(tx1, ty1);
		ebgl_md.Vertex2d(x, y2);
	}
	ebgl_md.End();
#endif
	return 0;
}


static EEL_xno gl_drawcircle(EEL_vm *vm)
{
	int i, d;
	float da, a;
	EEL_value *args = vm->heap + vm->argv;
	float x = eel_v2d(args);
	float y = eel_v2d(args + 1);
	float r = eel_v2d(args + 2);
	if(r <= 10)
		d = 12;
	else if(r >= 500)
		d = 135;
	else
		d = 16 + r * .25;
	da = 2.0f * M_PI / d;
	ebgl_md.Begin(GL_LINE_LOOP);
	for(i = 0, a = 0.0f; i < d; ++i, a += da)
		ebgl_md.Vertex2d(x + r * cosf(a), y + r * sinf(a));
	ebgl_md.End();
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno ebgl_unload(EEL_object *m, int closing)
{
	/* Stick around until we explicitly close EELBox */
	if(closing)
	{
		memset(&ebgl_md, 0, sizeof(ebgl_md));
		loaded = 0;
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static const EEL_lconstexp ebgl_constants[] =
{
	/* UploadTexture() flags */
	{"AUTOMIPMAP",			EBGL_AUTOMIPMAP			},
	{"HCLAMP",			EBGL_HCLAMP			},
	{"VCLAMP",			EBGL_VCLAMP			},
	{"HVCLAMP",			EBGL_HCLAMP | EBGL_VCLAMP	},

	/* SDL  OpenGL Attributes */
	{"RED_SIZE",			SDL_GL_RED_SIZE			},
	{"GREEN_SIZE",			SDL_GL_GREEN_SIZE		},
	{"BLUE_SIZE",			SDL_GL_BLUE_SIZE		},
/*	{"DOUBLEBUFFER",		SDL_GL_DOUBLEBUFFER		},*/
	{"SWAP_CONTROL",		SDL_GL_SWAP_CONTROL		},

	/* Error codes */
	{"NO_ERROR",			GL_NO_ERROR			},
	{"INVALID_ENUM",		GL_INVALID_ENUM			},
	{"INVALID_VALUE",		GL_INVALID_VALUE		},
	{"INVALID_OPERATION",		GL_INVALID_OPERATION		},
	{"STACK_OVERFLOW",		GL_STACK_OVERFLOW		},
	{"STACK_UNDERFLOW",		GL_STACK_UNDERFLOW		},
	{"OUT_OF_MEMORY",		GL_OUT_OF_MEMORY		},

	/* Primitives */
	{"POINTS",			GL_POINTS			},
	{"LINES",			GL_LINES			},
	{"LINE_LOOP",			GL_LINE_LOOP			},
	{"LINE_STRIP",			GL_LINE_STRIP			},
	{"TRIANGLES",			GL_TRIANGLES			},
	{"TRIANGLE_STRIP",		GL_TRIANGLE_STRIP		},
	{"TRIANGLE_FAN",		GL_TRIANGLE_FAN			},
	{"QUADS",			GL_QUADS			},
	{"QUAD_STRIP",			GL_QUAD_STRIP			},
	{"POLYGON",			GL_POLYGON			},

	/* Vertex Arrays */
	{"VERTEX_ARRAY",		GL_VERTEX_ARRAY			},
	{"NORMAL_ARRAY",		GL_NORMAL_ARRAY			},
	{"COLOR_ARRAY",			GL_COLOR_ARRAY			},
	{"INDEX_ARRAY",			GL_INDEX_ARRAY			},
	{"TEXTURE_COORD_ARRAY",		GL_TEXTURE_COORD_ARRAY		},
	{"EDGE_FLAG_ARRAY",		GL_EDGE_FLAG_ARRAY		},
	{"VERTEX_ARRAY_SIZE",		GL_VERTEX_ARRAY_SIZE		},
	{"VERTEX_ARRAY_TYPE",		GL_VERTEX_ARRAY_TYPE		},
	{"VERTEX_ARRAY_STRIDE",		GL_VERTEX_ARRAY_STRIDE		},
	{"NORMAL_ARRAY_TYPE",		GL_NORMAL_ARRAY_TYPE		},
	{"NORMAL_ARRAY_STRIDE",		GL_NORMAL_ARRAY_STRIDE		},
	{"COLOR_ARRAY_SIZE",		GL_COLOR_ARRAY_SIZE		},
	{"COLOR_ARRAY_TYPE",		GL_COLOR_ARRAY_TYPE		},
	{"COLOR_ARRAY_STRIDE",		GL_COLOR_ARRAY_STRIDE		},
	{"INDEX_ARRAY_TYPE",		GL_INDEX_ARRAY_TYPE		},
	{"INDEX_ARRAY_STRIDE",		GL_INDEX_ARRAY_STRIDE		},
	{"TEXTURE_COORD_ARRAY_SIZE",	GL_TEXTURE_COORD_ARRAY_SIZE	},
	{"TEXTURE_COORD_ARRAY_TYPE",	GL_TEXTURE_COORD_ARRAY_TYPE	},
	{"TEXTURE_COORD_ARRAY_STRIDE",	GL_TEXTURE_COORD_ARRAY_STRIDE	},
	{"EDGE_FLAG_ARRAY_STRIDE",	GL_EDGE_FLAG_ARRAY_STRIDE	},
	{"VERTEX_ARRAY_POINTER",	GL_VERTEX_ARRAY_POINTER		},
	{"NORMAL_ARRAY_POINTER",	GL_NORMAL_ARRAY_POINTER		},
	{"COLOR_ARRAY_POINTER",		GL_COLOR_ARRAY_POINTER		},
	{"INDEX_ARRAY_POINTER",		GL_INDEX_ARRAY_POINTER		},
	{"TEXTURE_COORD_ARRAY_POINTER",	GL_TEXTURE_COORD_ARRAY_POINTER	},
	{"EDGE_FLAG_ARRAY_POINTER",	GL_EDGE_FLAG_ARRAY_POINTER	},
	{"V2F",				GL_V2F				},
	{"V3F",				GL_V3F				},
	{"C4UB_V2F",			GL_C4UB_V2F			},
	{"C4UB_V3F",			GL_C4UB_V3F			},
	{"C3F_V3F",			GL_C3F_V3F			},
	{"N3F_V3F",			GL_N3F_V3F			},
	{"C4F_N3F_V3F",			GL_C4F_N3F_V3F			},
	{"T2F_V3F",			GL_T2F_V3F			},
	{"T4F_V4F",			GL_T4F_V4F			},
	{"T2F_C4UB_V3F",		GL_T2F_C4UB_V3F			},
	{"T2F_C3F_V3F",			GL_T2F_C3F_V3F			},
	{"T2F_N3F_V3F",			GL_T2F_N3F_V3F			},
	{"T2F_C4F_N3F_V3F",		GL_T2F_C4F_N3F_V3F		},
	{"T4F_C4F_N3F_V4F",		GL_T4F_C4F_N3F_V4F		},

	/* Matrix Mode */
	{"MATRIX_MODE",			GL_MATRIX_MODE			},
	{"MODELVIEW",			GL_MODELVIEW			},
	{"PROJECTION",			GL_PROJECTION			},
	{"TEXTURE",			GL_TEXTURE			},

	/* Points */
	{"POINT_SMOOTH",		GL_POINT_SMOOTH			},
	{"POINT_SIZE",			GL_POINT_SIZE			},
	{"POINT_SIZE_GRANULARITY",	GL_POINT_SIZE_GRANULARITY 	},
	{"POINT_SIZE_RANGE",		GL_POINT_SIZE_RANGE		},

	/* Lines */
	{"LINE_SMOOTH",			GL_LINE_SMOOTH			},
	{"LINE_STIPPLE",		GL_LINE_STIPPLE			},
	{"LINE_STIPPLE_PATTERN",	GL_LINE_STIPPLE_PATTERN		},
	{"LINE_STIPPLE_REPEAT",		GL_LINE_STIPPLE_REPEAT		},
	{"LINE_WIDTH",			GL_LINE_WIDTH			},
	{"LINE_WIDTH_GRANULARITY",	GL_LINE_WIDTH_GRANULARITY	},
	{"LINE_WIDTH_RANGE",		GL_LINE_WIDTH_RANGE		},

	/* Polygons */
	{"POINT",			GL_POINT			},
	{"LINE",			GL_LINE				},
	{"FILL",			GL_FILL				},
	{"CW",				GL_CW				},
	{"CCW",				GL_CCW				},
	{"FRONT",			GL_FRONT			},
	{"BACK",			GL_BACK				},
	{"POLYGON_MODE",		GL_POLYGON_MODE			},
	{"POLYGON_SMOOTH",		GL_POLYGON_SMOOTH		},
	{"POLYGON_STIPPLE",		GL_POLYGON_STIPPLE		},
	{"EDGE_FLAG",			GL_EDGE_FLAG			},
	{"CULL_FACE",			GL_CULL_FACE			},
	{"CULL_FACE_MODE",		GL_CULL_FACE_MODE		},
	{"FRONT_FACE",			GL_FRONT_FACE			},
	{"POLYGON_OFFSET_FACTOR",	GL_POLYGON_OFFSET_FACTOR	},
	{"POLYGON_OFFSET_UNITS",	GL_POLYGON_OFFSET_UNITS		},
	{"POLYGON_OFFSET_POINT",	GL_POLYGON_OFFSET_POINT		},
	{"POLYGON_OFFSET_LINE",		GL_POLYGON_OFFSET_LINE		},
	{"POLYGON_OFFSET_FILL",		GL_POLYGON_OFFSET_FILL		},

	/* Display Lists */
	{"COMPILE",			GL_COMPILE			},
	{"COMPILE_AND_EXECUTE",		GL_COMPILE_AND_EXECUTE		},
	{"LIST_BASE",			GL_LIST_BASE			},
	{"LIST_INDEX",			GL_LIST_INDEX			},
	{"LIST_MODE",			GL_LIST_MODE			},

	/* Depth Buffer */
	{"NEVER",			GL_NEVER			},
	{"LESS",			GL_LESS				},
	{"EQUAL",			GL_EQUAL			},
	{"LEQUAL",			GL_LEQUAL			},
	{"GREATER",			GL_GREATER			},
	{"NOTEQUAL",			GL_NOTEQUAL			},
	{"GEQUAL",			GL_GEQUAL			},
	{"ALWAYS",			GL_ALWAYS			},
	{"DEPTH_TEST",			GL_DEPTH_TEST			},
	{"DEPTH_BITS",			GL_DEPTH_BITS			},
	{"DEPTH_CLEAR_VALUE",		GL_DEPTH_CLEAR_VALUE		},
	{"DEPTH_FUNC",			GL_DEPTH_FUNC			},
	{"DEPTH_RANGE",			GL_DEPTH_RANGE			},
	{"DEPTH_WRITEMASK",		GL_DEPTH_WRITEMASK		},
	{"DEPTH_COMPONENT",		GL_DEPTH_COMPONENT		},

	/* Lighting */
	{"LIGHTING",			GL_LIGHTING			},
	{"LIGHT0",			GL_LIGHT0			},
	{"LIGHT1",			GL_LIGHT1			},
	{"LIGHT2",			GL_LIGHT2			},
	{"LIGHT3",			GL_LIGHT3			},
	{"LIGHT4",			GL_LIGHT4			},
	{"LIGHT5",			GL_LIGHT5			},
	{"LIGHT6",			GL_LIGHT6			},
	{"LIGHT7",			GL_LIGHT7			},
	{"SPOT_EXPONENT",		GL_SPOT_EXPONENT		},
	{"SPOT_CUTOFF",			GL_SPOT_CUTOFF			},
	{"CONSTANT_ATTENUATION",	GL_CONSTANT_ATTENUATION		},
	{"LINEAR_ATTENUATION",		GL_LINEAR_ATTENUATION		},
	{"QUADRATIC_ATTENUATION	",	GL_QUADRATIC_ATTENUATION	},
	{"AMBIENT",			GL_AMBIENT			},
	{"DIFFUSE",			GL_DIFFUSE			},
	{"SPECULAR",			GL_SPECULAR			},
	{"SHININESS",			GL_SHININESS			},
	{"EMISSION",			GL_EMISSION			},
	{"POSITION",			GL_POSITION			},
	{"SPOT_DIRECTION",		GL_SPOT_DIRECTION		},
	{"AMBIENT_AND_DIFFUSE",		GL_AMBIENT_AND_DIFFUSE		},
	{"COLOR_INDEXES",		GL_COLOR_INDEXES		},
	{"LIGHT_MODEL_TWO_SIDE",	GL_LIGHT_MODEL_TWO_SIDE		},
	{"LIGHT_MODEL_LOCAL_VIEWER",	GL_LIGHT_MODEL_LOCAL_VIEWER	},
	{"LIGHT_MODEL_AMBIENT",		GL_LIGHT_MODEL_AMBIENT		},
	{"FRONT_AND_BACK",		GL_FRONT_AND_BACK		},
	{"SHADE_MODEL",			GL_SHADE_MODEL			},
	{"FLAT",			GL_FLAT				},
	{"SMOOTH",			GL_SMOOTH			},
	{"COLOR_MATERIAL",		GL_COLOR_MATERIAL		},
	{"COLOR_MATERIAL_FACE",		GL_COLOR_MATERIAL_FACE		},
	{"COLOR_MATERIAL_PARAMETER",	GL_COLOR_MATERIAL_PARAMETER	},
	{"NORMALIZE",			GL_NORMALIZE			},

	/* User Clipping Planes */
	{"CLIP_PLANE0",			GL_CLIP_PLANE0			},
	{"CLIP_PLANE1",			GL_CLIP_PLANE1			},
	{"CLIP_PLANE2",			GL_CLIP_PLANE2			},
	{"CLIP_PLANE3",			GL_CLIP_PLANE3			},
	{"CLIP_PLANE4",			GL_CLIP_PLANE4			},
	{"CLIP_PLANE5",			GL_CLIP_PLANE5			},

	/* Accumulation Buffer */
	{"ACCUM_RED_BITS",		GL_ACCUM_RED_BITS		},
	{"ACCUM_GREEN_BITS",		GL_ACCUM_GREEN_BITS		},
	{"ACCUM_BLUE_BITS",		GL_ACCUM_BLUE_BITS		},
	{"ACCUM_ALPHA_BITS",		GL_ACCUM_ALPHA_BITS		},
	{"ACCUM_CLEAR_VALUE",		GL_ACCUM_CLEAR_VALUE		},
	{"ACCUM",			GL_ACCUM			},
	{"ADD",				GL_ADD				},
	{"LOAD",			GL_LOAD				},
	{"MULT",			GL_MULT				},
	{"RETURN",			GL_RETURN			},

	/* Alpha Testing */
	{"ALPHA_TEST",			GL_ALPHA_TEST			},
	{"ALPHA_TEST_REF",		GL_ALPHA_TEST_REF		},
	{"ALPHA_TEST_FUNC",		GL_ALPHA_TEST_FUNC		},

	/* Blending */
	{"BLEND",			GL_BLEND			},
	{"BLEND_SRC",			GL_BLEND_SRC			},
	{"BLEND_DST",			GL_BLEND_DST			},
	{"ZERO",			GL_ZERO				},
	{"ONE",				GL_ONE				},
	{"SRC_COLOR",			GL_SRC_COLOR			},
	{"ONE_MINUS_SRC_COLOR",		GL_ONE_MINUS_SRC_COLOR		},
	{"SRC_ALPHA",			GL_SRC_ALPHA			},
	{"ONE_MINUS_SRC_ALPHA",		GL_ONE_MINUS_SRC_ALPHA		},
	{"DST_ALPHA",			GL_DST_ALPHA			},
	{"ONE_MINUS_DST_ALPHA",		GL_ONE_MINUS_DST_ALPHA		},
	{"DST_COLOR",			GL_DST_COLOR			},
	{"ONE_MINUS_DST_COLOR",		GL_ONE_MINUS_DST_COLOR		},
	{"SRC_ALPHA_SATURATE",		GL_SRC_ALPHA_SATURATE		},

	/* Blend Equation */
	{"BLEND_EQUATION",		GL_BLEND_EQUATION		},
	{"MIN",				GL_MIN				},
	{"MAX",				GL_MAX				},
	{"FUNC_ADD",			GL_FUNC_ADD			},
	{"FUNC_SUBTRACT",		GL_FUNC_SUBTRACT		},
	{"FUNC_REVERSE_SUBTRACT",	GL_FUNC_REVERSE_SUBTRACT	},
	{"BLEND_COLOR",			GL_BLEND_COLOR 			},

	/* Render Mode */
	{"FEEDBACK",			GL_FEEDBACK			},
	{"RENDER",			GL_RENDER			},
	{"SELECT",			GL_SELECT			},

	/* Feedback */
	{"2D",				GL_2D				},
	{"3D",				GL_3D				},
	{"3D_COLOR",			GL_3D_COLOR			},
	{"3D_COLOR_TEXTURE",		GL_3D_COLOR_TEXTURE		},
	{"4D_COLOR_TEXTURE",		GL_4D_COLOR_TEXTURE		},
	{"POINT_TOKEN",			GL_POINT_TOKEN			},
	{"LINE_TOKEN",			GL_LINE_TOKEN			},
	{"LINE_RESET_TOKEN",		GL_LINE_RESET_TOKEN		},
	{"POLYGON_TOKEN",		GL_POLYGON_TOKEN		},
	{"BITMAP_TOKEN",		GL_BITMAP_TOKEN			},
	{"DRAW_PIXEL_TOKEN",		GL_DRAW_PIXEL_TOKEN		},
	{"COPY_PIXEL_TOKEN",		GL_COPY_PIXEL_TOKEN		},
	{"PASS_THROUGH_TOKEN",		GL_PASS_THROUGH_TOKEN		},
	{"FEEDBACK_BUFFER_POINTER",	GL_FEEDBACK_BUFFER_POINTER	},
	{"FEEDBACK_BUFFER_SIZE",	GL_FEEDBACK_BUFFER_SIZE		},
	{"FEEDBACK_BUFFER_TYPE",	GL_FEEDBACK_BUFFER_TYPE		},

	/* Selection */
	{"SELECTION_BUFFER_POINTER",	GL_SELECTION_BUFFER_POINTER	},
	{"SELECTION_BUFFER_SIZE",	GL_SELECTION_BUFFER_SIZE	},

	/* Fog */
	{"FOG",				GL_FOG				},
	{"FOG_MODE",			GL_FOG_MODE			},
	{"FOG_DENSITY",			GL_FOG_DENSITY			},
	{"FOG_COLOR",			GL_FOG_COLOR			},
	{"FOG_INDEX",			GL_FOG_INDEX			},
	{"FOG_START",			GL_FOG_START			},
	{"FOG_END",			GL_FOG_END			},
	{"LINEAR",			GL_LINEAR			},
	{"EXP",				GL_EXP				},
	{"EXP2",			GL_EXP2				},

	/* Logic Operations */
	{"LOGIC_OP",			GL_LOGIC_OP			},
	{"INDEX_LOGIC_OP",		GL_INDEX_LOGIC_OP		},
	{"COLOR_LOGIC_OP",		GL_COLOR_LOGIC_OP		},
	{"LOGIC_OP_MODE",		GL_LOGIC_OP_MODE		},
	{"CLEAR",			GL_CLEAR			},
	{"SET",				GL_SET				},
	{"COPY",			GL_COPY				},
	{"COPY_INVERTED",		GL_COPY_INVERTED		},
	{"NOOP",			GL_NOOP				},
	{"INVERT",			GL_INVERT			},
	{"AND",				GL_AND				},
	{"NAND",			GL_NAND				},
	{"OR",				GL_OR				},
	{"NOR",				GL_NOR				},
	{"XOR",				GL_XOR				},
	{"EQUIV",			GL_EQUIV			},
	{"AND_REVERSE",			GL_AND_REVERSE			},
	{"AND_INVERTED",		GL_AND_INVERTED			},
	{"OR_REVERSE",			GL_OR_REVERSE			},
	{"OR_INVERTED",			GL_OR_INVERTED			},

	/* Stencil */
	{"STENCIL_BITS",		GL_STENCIL_BITS			},
	{"STENCIL_TEST",		GL_STENCIL_TEST			},
	{"STENCIL_CLEAR_VALUE",		GL_STENCIL_CLEAR_VALUE		},
	{"STENCIL_FUNC",		GL_STENCIL_FUNC			},
	{"STENCIL_VALUE_MASK",		GL_STENCIL_VALUE_MASK		},
	{"STENCIL_FAIL",		GL_STENCIL_FAIL			},
	{"STENCIL_PASS_DEPTH_FAIL",	GL_STENCIL_PASS_DEPTH_FAIL	},
	{"STENCIL_PASS_DEPTH_PASS",	GL_STENCIL_PASS_DEPTH_PASS	},
	{"STENCIL_REF",			GL_STENCIL_REF			},
	{"STENCIL_WRITEMASK",		GL_STENCIL_WRITEMASK		},
	{"STENCIL_INDEX",		GL_STENCIL_INDEX		},
	{"KEEP",			GL_KEEP				},
	{"REPLACE",			GL_REPLACE			},
	{"INCR",			GL_INCR				},
	{"DECR",			GL_DECR				},

	/* Pixel Buffers */
	{"NONE",			GL_NONE				},
	{"LEFT",			GL_LEFT				},
	{"RIGHT",			GL_RIGHT			},
	{"FRONT_LEFT",			GL_FRONT_LEFT			},
	{"FRONT_RIGHT",			GL_FRONT_RIGHT			},
	{"BACK_LEFT",			GL_BACK_LEFT			},
	{"BACK_RIGHT",			GL_BACK_RIGHT			},
	{"AUX0",			GL_AUX0				},
	{"AUX1",			GL_AUX1				},
	{"AUX2",			GL_AUX2				},
	{"AUX3",			GL_AUX3				},
	{"COLOR_INDEX",			GL_COLOR_INDEX			},
	{"RED",				GL_RED				},
	{"GREEN",			GL_GREEN			},
	{"BLUE",			GL_BLUE				},
	{"ALPHA",			GL_ALPHA			},
	{"LUMINANCE",			GL_LUMINANCE			},
	{"LUMINANCE_ALPHA",		GL_LUMINANCE_ALPHA		},
	{"ALPHA_BITS",			GL_ALPHA_BITS			},
	{"RED_BITS",			GL_RED_BITS			},
	{"GREEN_BITS",			GL_GREEN_BITS			},
	{"BLUE_BITS",			GL_BLUE_BITS			},
	{"INDEX_BITS",			GL_INDEX_BITS			},
	{"SUBPIXEL_BITS",		GL_SUBPIXEL_BITS		},
	{"AUX_BUFFERS",			GL_AUX_BUFFERS			},
	{"READ_BUFFER",			GL_READ_BUFFER			},
	{"DRAW_BUFFER",			GL_DRAW_BUFFER			},
	{"DOUBLEBUFFER",		GL_DOUBLEBUFFER			},
	{"STEREO",			GL_STEREO			},
	{"BITMAP",			GL_BITMAP			},
	{"COLOR",			GL_COLOR			},
	{"DEPTH",			GL_DEPTH			},
	{"STENCIL",			GL_STENCIL			},
	{"DITHER",			GL_DITHER			},
	{"RGB",				GL_RGB				},
	{"RGBA",			GL_RGBA				},

	/* Implementation Limits */
	{"MAX_LIST_NESTING",		GL_MAX_LIST_NESTING		},
	{"MAX_EVAL_ORDER",		GL_MAX_EVAL_ORDER		},
	{"MAX_LIGHTS",			GL_MAX_LIGHTS			},
	{"MAX_CLIP_PLANES",		GL_MAX_CLIP_PLANES		},
	{"MAX_TEXTURE_SIZE",		GL_MAX_TEXTURE_SIZE		},
	{"MAX_PIXEL_MAP_TABLE",		GL_MAX_PIXEL_MAP_TABLE		},
	{"MAX_ATTRIB_STACK_DEPTH",	GL_MAX_ATTRIB_STACK_DEPTH	},
	{"MAX_MODELVIEW_STACK_DEPTH",	GL_MAX_MODELVIEW_STACK_DEPTH	},
	{"MAX_NAME_STACK_DEPTH",	GL_MAX_NAME_STACK_DEPTH		},
	{"MAX_PROJECTION_STACK_DEPTH",	GL_MAX_PROJECTION_STACK_DEPTH	},
	{"MAX_TEXTURE_STACK_DEPTH",	GL_MAX_TEXTURE_STACK_DEPTH	},
	{"MAX_VIEWPORT_DIMS",		GL_MAX_VIEWPORT_DIMS		},
	{"MAX_CLIENT_ATTRIB_STACK_DEPTH",GL_MAX_CLIENT_ATTRIB_STACK_DEPTH},

	/* Gets */
	{"ATTRIB_STACK_DEPTH",		GL_ATTRIB_STACK_DEPTH		},
	{"CLIENT_ATTRIB_STACK_DEPTH",	GL_CLIENT_ATTRIB_STACK_DEPTH	},
	{"COLOR_CLEAR_VALUE",		GL_COLOR_CLEAR_VALUE		},
	{"COLOR_WRITEMASK",		GL_COLOR_WRITEMASK		},
	{"CURRENT_INDEX",		GL_CURRENT_INDEX		},
	{"CURRENT_COLOR",		GL_CURRENT_COLOR		},
	{"CURRENT_NORMAL",		GL_CURRENT_NORMAL		},
	{"CURRENT_RASTER_COLOR",	GL_CURRENT_RASTER_COLOR		},
	{"CURRENT_RASTER_DISTANCE",	GL_CURRENT_RASTER_DISTANCE	},
	{"CURRENT_RASTER_INDEX",	GL_CURRENT_RASTER_INDEX		},
	{"CURRENT_RASTER_POSITION",	GL_CURRENT_RASTER_POSITION	},
	{"CURRENT_RASTER_TEXTURE_COORDS",GL_CURRENT_RASTER_TEXTURE_COORDS},
	{"CURRENT_RASTER_POSITION_VALID",GL_CURRENT_RASTER_POSITION_VALID},
	{"CURRENT_TEXTURE_COORDS",	GL_CURRENT_TEXTURE_COORDS	},
	{"INDEX_CLEAR_VALUE",		GL_INDEX_CLEAR_VALUE		},
	{"INDEX_MODE",			GL_INDEX_MODE			},
	{"INDEX_WRITEMASK",		GL_INDEX_WRITEMASK		},
	{"MODELVIEW_MATRIX",		GL_MODELVIEW_MATRIX		},
	{"MODELVIEW_STACK_DEPTH",	GL_MODELVIEW_STACK_DEPTH	},
	{"NAME_STACK_DEPTH",		GL_NAME_STACK_DEPTH		},
	{"PROJECTION_MATRIX",		GL_PROJECTION_MATRIX		},
	{"PROJECTION_STACK_DEPTH",	GL_PROJECTION_STACK_DEPTH	},
	{"RENDER_MODE",			GL_RENDER_MODE			},
	{"RGBA_MODE",			GL_RGBA_MODE			},
	{"TEXTURE_MATRIX",		GL_TEXTURE_MATRIX		},
	{"TEXTURE_STACK_DEPTH",		GL_TEXTURE_STACK_DEPTH		},
	{"VIEWPORT",			GL_VIEWPORT			},

	/* Evaluators */
	{"AUTO_NORMAL",			GL_AUTO_NORMAL			},
	{"MAP1_COLOR_4",		GL_MAP1_COLOR_4			},
	{"MAP1_INDEX",			GL_MAP1_INDEX			},
	{"MAP1_NORMAL",			GL_MAP1_NORMAL			},
	{"MAP1_TEXTURE_COORD_1",	GL_MAP1_TEXTURE_COORD_1		},
	{"MAP1_TEXTURE_COORD_2",	GL_MAP1_TEXTURE_COORD_2		},
	{"MAP1_TEXTURE_COORD_3",	GL_MAP1_TEXTURE_COORD_3		},
	{"MAP1_TEXTURE_COORD_4",	GL_MAP1_TEXTURE_COORD_4		},
	{"MAP1_VERTEX_3",		GL_MAP1_VERTEX_3		},
	{"MAP1_VERTEX_4",		GL_MAP1_VERTEX_4		},
	{"MAP2_COLOR_4",		GL_MAP2_COLOR_4			},
	{"MAP2_INDEX",			GL_MAP2_INDEX			},
	{"MAP2_NORMAL",			GL_MAP2_NORMAL			},
	{"MAP2_TEXTURE_COORD_1",	GL_MAP2_TEXTURE_COORD_1		},
	{"MAP2_TEXTURE_COORD_2",	GL_MAP2_TEXTURE_COORD_2		},
	{"MAP2_TEXTURE_COORD_3",	GL_MAP2_TEXTURE_COORD_3		},
	{"MAP2_TEXTURE_COORD_4",	GL_MAP2_TEXTURE_COORD_4		},
	{"MAP2_VERTEX_3",		GL_MAP2_VERTEX_3		},
	{"MAP2_VERTEX_4",		GL_MAP2_VERTEX_4		},
	{"MAP1_GRID_DOMAIN",		GL_MAP1_GRID_DOMAIN		},
	{"MAP1_GRID_SEGMENTS",		GL_MAP1_GRID_SEGMENTS		},
	{"MAP2_GRID_DOMAIN",		GL_MAP2_GRID_DOMAIN		},
	{"MAP2_GRID_SEGMENTS",		GL_MAP2_GRID_SEGMENTS		},
	{"COEFF",			GL_COEFF			},
	{"ORDER",			GL_ORDER			},
	{"DOMAIN",			GL_DOMAIN			},

	/* Hints */
	{"PERSPECTIVE_CORRECTION_HINT",	GL_PERSPECTIVE_CORRECTION_HINT	},
	{"POINT_SMOOTH_HINT",		GL_POINT_SMOOTH_HINT		},
	{"LINE_SMOOTH_HINT",		GL_LINE_SMOOTH_HINT		},
	{"POLYGON_SMOOTH_HINT",		GL_POLYGON_SMOOTH_HINT		},
	{"FOG_HINT",			GL_FOG_HINT			},
	{"DONT_CARE",			GL_DONT_CARE			},
	{"FASTEST",			GL_FASTEST			},
	{"NICEST",			GL_NICEST			},

	/* Scissor Box */
	{"GL_SCISSOR_BOX",		GL_SCISSOR_BOX			},
	{"GL_SCISSOR_TEST",		GL_SCISSOR_TEST			},

	/* Pixel Mode/Transfer */
	{"MAP_COLOR",			GL_MAP_COLOR			},
	{"MAP_STENCIL",			GL_MAP_STENCIL			},
	{"INDEX_SHIFT",			GL_INDEX_SHIFT			},
	{"INDEX_OFFSET",		GL_INDEX_OFFSET			},
	{"RED_SCALE",			GL_RED_SCALE			},
	{"RED_BIAS",			GL_RED_BIAS			},
	{"GREEN_SCALE",			GL_GREEN_SCALE			},
	{"GREEN_BIAS",			GL_GREEN_BIAS			},
	{"BLUE_SCALE",			GL_BLUE_SCALE			},
	{"BLUE_BIAS",			GL_BLUE_BIAS			},
	{"ALPHA_SCALE",			GL_ALPHA_SCALE			},
	{"ALPHA_BIAS",			GL_ALPHA_BIAS			},
	{"DEPTH_SCALE",			GL_DEPTH_SCALE			},
	{"DEPTH_BIAS",			GL_DEPTH_BIAS			},
	{"PIXEL_MAP_S_TO_S_SIZE",	GL_PIXEL_MAP_S_TO_S_SIZE	},
	{"PIXEL_MAP_I_TO_I_SIZE",	GL_PIXEL_MAP_I_TO_I_SIZE	},
	{"PIXEL_MAP_I_TO_R_SIZE",	GL_PIXEL_MAP_I_TO_R_SIZE	},
	{"PIXEL_MAP_I_TO_G_SIZE",	GL_PIXEL_MAP_I_TO_G_SIZE	},
	{"PIXEL_MAP_I_TO_B_SIZE",	GL_PIXEL_MAP_I_TO_B_SIZE	},
	{"PIXEL_MAP_I_TO_A_SIZE",	GL_PIXEL_MAP_I_TO_A_SIZE	},
	{"PIXEL_MAP_R_TO_R_SIZE",	GL_PIXEL_MAP_R_TO_R_SIZE	},
	{"PIXEL_MAP_G_TO_G_SIZE",	GL_PIXEL_MAP_G_TO_G_SIZE	},
	{"PIXEL_MAP_B_TO_B_SIZE",	GL_PIXEL_MAP_B_TO_B_SIZE	},
	{"PIXEL_MAP_A_TO_A_SIZE",	GL_PIXEL_MAP_A_TO_A_SIZE	},
	{"PIXEL_MAP_S_TO_S",		GL_PIXEL_MAP_S_TO_S		},
	{"PIXEL_MAP_I_TO_I",		GL_PIXEL_MAP_I_TO_I		},
	{"PIXEL_MAP_I_TO_R",		GL_PIXEL_MAP_I_TO_R		},
	{"PIXEL_MAP_I_TO_G",		GL_PIXEL_MAP_I_TO_G		},
	{"PIXEL_MAP_I_TO_B",		GL_PIXEL_MAP_I_TO_B		},
	{"PIXEL_MAP_I_TO_A",		GL_PIXEL_MAP_I_TO_A		},
	{"PIXEL_MAP_R_TO_R",		GL_PIXEL_MAP_R_TO_R		},
	{"PIXEL_MAP_G_TO_G",		GL_PIXEL_MAP_G_TO_G		},
	{"PIXEL_MAP_B_TO_B",		GL_PIXEL_MAP_B_TO_B		},
	{"PIXEL_MAP_A_TO_A",		GL_PIXEL_MAP_A_TO_A		},
	{"PACK_ALIGNMENT",		GL_PACK_ALIGNMENT		},
	{"PACK_LSB_FIRST",		GL_PACK_LSB_FIRST		},
	{"PACK_ROW_LENGTH",		GL_PACK_ROW_LENGTH		},
	{"PACK_SKIP_PIXELS",		GL_PACK_SKIP_PIXELS		},
	{"PACK_SKIP_ROWS",		GL_PACK_SKIP_ROWS		},
	{"PACK_SWAP_BYTES",		GL_PACK_SWAP_BYTES		},
	{"UNPACK_ALIGNMENT",		GL_UNPACK_ALIGNMENT		},
	{"UNPACK_LSB_FIRST",		GL_UNPACK_LSB_FIRST		},
	{"UNPACK_ROW_LENGTH",		GL_UNPACK_ROW_LENGTH		},
	{"UNPACK_SKIP_PIXELS",		GL_UNPACK_SKIP_PIXELS		},
	{"UNPACK_SKIP_ROWS",		GL_UNPACK_SKIP_ROWS		},
	{"UNPACK_SWAP_BYTES",		GL_UNPACK_SWAP_BYTES		},
	{"ZOOM_X",			GL_ZOOM_X			},
	{"ZOOM_Y",			GL_ZOOM_Y			},

	/* Texture Mapping */
	{"TEXTURE_ENV",			GL_TEXTURE_ENV			},
	{"TEXTURE_ENV_MODE",		GL_TEXTURE_ENV_MODE		},
	{"TEXTURE_1D",			GL_TEXTURE_1D			},
	{"TEXTURE_2D",			GL_TEXTURE_2D			},
	{"TEXTURE_WRAP_S",		GL_TEXTURE_WRAP_S		},
	{"TEXTURE_WRAP_T",		GL_TEXTURE_WRAP_T		},
	{"TEXTURE_MAG_FILTER",		GL_TEXTURE_MAG_FILTER		},
	{"TEXTURE_MIN_FILTER",		GL_TEXTURE_MIN_FILTER		},
	{"TEXTURE_ENV_COLOR",		GL_TEXTURE_ENV_COLOR		},
	{"TEXTURE_GEN_S",		GL_TEXTURE_GEN_S		},
	{"TEXTURE_GEN_T",		GL_TEXTURE_GEN_T		},
	{"TEXTURE_GEN_MODE",		GL_TEXTURE_GEN_MODE		},
	{"TEXTURE_BORDER_COLOR",	GL_TEXTURE_BORDER_COLOR		},
	{"TEXTURE_WIDTH",		GL_TEXTURE_WIDTH		},
	{"TEXTURE_HEIGHT",		GL_TEXTURE_HEIGHT		},
	{"TEXTURE_BORDER",		GL_TEXTURE_BORDER		},
	{"TEXTURE_COMPONENTS",		GL_TEXTURE_COMPONENTS		},
	{"TEXTURE_RED_SIZE",		GL_TEXTURE_RED_SIZE		},
	{"TEXTURE_GREEN_SIZE",		GL_TEXTURE_GREEN_SIZE		},
	{"TEXTURE_BLUE_SIZE",		GL_TEXTURE_BLUE_SIZE		},
	{"TEXTURE_ALPHA_SIZE",		GL_TEXTURE_ALPHA_SIZE		},
	{"TEXTURE_LUMINANCE_SIZE",	GL_TEXTURE_LUMINANCE_SIZE	},
	{"TEXTURE_INTENSITY_SIZE",	GL_TEXTURE_INTENSITY_SIZE	},
	{"NEAREST_MIPMAP_NEAREST",	GL_NEAREST_MIPMAP_NEAREST	},
	{"NEAREST_MIPMAP_LINEAR",	GL_NEAREST_MIPMAP_LINEAR	},
	{"LINEAR_MIPMAP_NEAREST",	GL_LINEAR_MIPMAP_NEAREST	},
	{"LINEAR_MIPMAP_LINEAR",	GL_LINEAR_MIPMAP_LINEAR		},
	{"OBJECT_LINEAR",		GL_OBJECT_LINEAR		},
	{"OBJECT_PLANE",		GL_OBJECT_PLANE			},
	{"EYE_LINEAR",			GL_EYE_LINEAR			},
	{"EYE_PLANE",			GL_EYE_PLANE			},
	{"SPHERE_MAP",			GL_SPHERE_MAP			},
	{"DECAL",			GL_DECAL			},
	{"MODULATE",			GL_MODULATE			},
	{"NEAREST",			GL_NEAREST			},
	{"REPEAT",			GL_REPEAT			},
	{"CLAMP",			GL_CLAMP			},
	{"S",				GL_S				},
	{"T",				GL_T				},
	{"R",				GL_R				},
	{"Q",				GL_Q				},
	{"TEXTURE_GEN_R",		GL_TEXTURE_GEN_R		},
	{"TEXTURE_GEN_Q",		GL_TEXTURE_GEN_Q		},

	/* Utility */
	{"VENDOR",			GL_VENDOR			},
	{"RENDERER",			GL_RENDERER			},
	{"VERSION",			GL_VERSION			},
	{"EXTENSIONS",			GL_EXTENSIONS			},

	/* glPush/PopAttrib Bits */
	{"CURRENT_BIT",			GL_CURRENT_BIT			},
	{"POINT_BIT",			GL_POINT_BIT			},
	{"LINE_BIT",			GL_LINE_BIT			},
	{"POLYGON_BIT",			GL_POLYGON_BIT			},
	{"POLYGON_STIPPLE_BIT",		GL_POLYGON_STIPPLE_BIT		},
	{"PIXEL_MODE_BIT",		GL_PIXEL_MODE_BIT		},
	{"LIGHTING_BIT",		GL_LIGHTING_BIT			},
	{"FOG_BIT",			GL_FOG_BIT			},
	{"DEPTH_BUFFER_BIT",		GL_DEPTH_BUFFER_BIT		},
	{"ACCUM_BUFFER_BIT",		GL_ACCUM_BUFFER_BIT		},
	{"STENCIL_BUFFER_BIT",		GL_STENCIL_BUFFER_BIT		},
	{"VIEWPORT_BIT",		GL_VIEWPORT_BIT			},
	{"TRANSFORM_BIT",		GL_TRANSFORM_BIT		},
	{"ENABLE_BIT",			GL_ENABLE_BIT			},
	{"COLOR_BUFFER_BIT",		GL_COLOR_BUFFER_BIT		},
	{"HINT_BIT",			GL_HINT_BIT			},
	{"EVAL_BIT",			GL_EVAL_BIT			},
	{"LIST_BIT",			GL_LIST_BIT			},
	{"TEXTURE_BIT",			GL_TEXTURE_BIT			},
	{"SCISSOR_BIT",			GL_SCISSOR_BIT			},
	{"ALL_ATTRIB_BITS",		GL_ALL_ATTRIB_BITS		},

	/* OpenGL 1.1 */
	{"PROXY_TEXTURE_1D",		GL_PROXY_TEXTURE_1D		},
	{"PROXY_TEXTURE_2D",		GL_PROXY_TEXTURE_2D		},
	{"TEXTURE_PRIORITY",		GL_TEXTURE_PRIORITY		},
	{"TEXTURE_RESIDENT",		GL_TEXTURE_RESIDENT		},
	{"TEXTURE_BINDING_1D",		GL_TEXTURE_BINDING_1D		},
	{"TEXTURE_BINDING_2D",		GL_TEXTURE_BINDING_2D		},
	{"TEXTURE_INTERNAL_FORMAT",	GL_TEXTURE_INTERNAL_FORMAT	},
	{"ALPHA4",			GL_ALPHA4			},
	{"ALPHA8",			GL_ALPHA8			},
	{"ALPHA12",			GL_ALPHA12			},
	{"ALPHA16",			GL_ALPHA16			},
	{"LUMINANCE4",			GL_LUMINANCE4			},
	{"LUMINANCE8",			GL_LUMINANCE8			},
	{"LUMINANCE12",			GL_LUMINANCE12			},
	{"LUMINANCE16",			GL_LUMINANCE16			},
	{"LUMINANCE4_ALPHA4",		GL_LUMINANCE4_ALPHA4		},
	{"LUMINANCE6_ALPHA2",		GL_LUMINANCE6_ALPHA2		},
	{"LUMINANCE8_ALPHA8",		GL_LUMINANCE8_ALPHA8		},
	{"LUMINANCE12_ALPHA4",		GL_LUMINANCE12_ALPHA4		},
	{"LUMINANCE12_ALPHA12",		GL_LUMINANCE12_ALPHA12		},
	{"LUMINANCE16_ALPHA16",		GL_LUMINANCE16_ALPHA16		},
	{"INTENSITY",			GL_INTENSITY			},
	{"INTENSITY4",			GL_INTENSITY4			},
	{"INTENSITY8",			GL_INTENSITY8			},
	{"INTENSITY12",			GL_INTENSITY12			},
	{"INTENSITY16",			GL_INTENSITY16			},
	{"R3_G3_B2",			GL_R3_G3_B2			},
	{"RGB4",			GL_RGB4				},
	{"RGB5",			GL_RGB5				},
	{"RGB8",			GL_RGB8				},
	{"RGB10",			GL_RGB10			},
	{"RGB12",			GL_RGB12			},
	{"RGB16",			GL_RGB16			},
	{"RGBA2",			GL_RGBA2			},
	{"RGBA4",			GL_RGBA4			},
	{"RGB5_A1",			GL_RGB5_A1			},
	{"RGBA8",			GL_RGBA8			},
	{"RGB10_A2",			GL_RGB10_A2			},
	{"RGBA12",			GL_RGBA12			},
	{"RGBA16",			GL_RGBA16			},
	{"CLIENT_PIXEL_STORE_BIT",	GL_CLIENT_PIXEL_STORE_BIT	},
	{"CLIENT_VERTEX_ARRAY_BIT",	GL_CLIENT_VERTEX_ARRAY_BIT	},
	{"ALL_CLIENT_ATTRIB_BITS",	GL_ALL_CLIENT_ATTRIB_BITS 	},
	{"CLIENT_ALL_ATTRIB_BITS",	GL_CLIENT_ALL_ATTRIB_BITS 	},

	{NULL, 0}
};


EEL_xno ebgl_init(EEL_vm *vm)
{
	EEL_object *m;

	if(loaded)
		return EEL_XDEVICEOPENED;

	m = eel_create_module(vm, "OpenGL", ebgl_unload, NULL);
	if(!m)
		return EEL_XMODULEINIT;

	memset(&ebgl_md, 0, sizeof(ebgl_md));
	gl_Reset();
	ebgl_dummy_calls();

	/* Management and utilities */
	eel_export_cfunction(m, 0, "Load", 0, 1, 0, gl_load);
	eel_export_cfunction(m, 0, "Exceptions", 1, 0, 0, gl_exceptions);
	eel_export_cfunction(m, 0, "SetAttribute", 2, 0, 0, gl_setattribute);
	eel_export_cfunction(m, 0, "Perspective", 4, 0, 0, gl_perspective);
	eel_export_cfunction(m, 0, "SwapBuffers", 0, 0, 0, gl_swapbuffers);
	eel_export_cfunction(m, 1, "UploadTexture", 1, 2, 0, gl_uploadtexture);

	/* Miscellaneous */
	eel_export_cfunction(m, 1, "GetString", 1, 0, 0, gl_getstring);
	eel_export_cfunction(m, 0, "Disable", 1, 0, 0, gl_disable);
	eel_export_cfunction(m, 0, "Enable", 1, 0, 0, gl_enable);
	eel_export_cfunction(m, 0, "BlendFunc", 2, 0, 0, gl_blendfunc);
	eel_export_cfunction(m, 0, "BlendEquation", 1, 0, 0, gl_blendequation);
	eel_export_cfunction(m, 0, "ClearColor", 4, 0, 0, gl_clearcolor);
	eel_export_cfunction(m, 0, "Clear", 1, 0, 0, gl_clear);
	eel_export_cfunction(m, 0, "Hint", 2, 0, 0, gl_hint);
	eel_export_cfunction(m, 0, "ReadPixels", 2, 1, 0, gl_readpixels);

	/* Depth Buffer */
	eel_export_cfunction(m, 0, "ClearDepth", 1, 0, 0, gl_cleardepth);
	eel_export_cfunction(m, 0, "DepthFunc", 1, 0, 0, gl_depthfunc);

	/* Transformation */
	eel_export_cfunction(m, 0, "MatrixMode", 1, 0, 0, gl_matrixmode);
	eel_export_cfunction(m, 0, "Ortho", 6, 0, 0, gl_ortho);
	eel_export_cfunction(m, 0, "Frustum", 6, 0, 0, gl_frustum);
	eel_export_cfunction(m, 0, "Viewport", 4, 0, 0, gl_viewport);
	eel_export_cfunction(m, 0, "PushMatrix", 0, 0, 0, gl_pushmatrix);
	eel_export_cfunction(m, 0, "PopMatrix", 0, 0, 0, gl_popmatrix);
	eel_export_cfunction(m, 0, "LoadIdentity", 0, 0, 0, gl_loadidentity);
	eel_export_cfunction(m, 0, "Rotate", 4, 0, 0, gl_rotate);
	eel_export_cfunction(m, 0, "Scale", 3, 0, 0, gl_scale);
	eel_export_cfunction(m, 0, "Translate", 3, 0, 0, gl_translate);

	/* Textures */
	eel_export_cfunction(m, 1, "IsTexture", 1, 0, 0, gl_istexture);
	eel_export_cfunction(m, 0, "BindTexture", 2, 0, 0, gl_bindtexture);
	/*
	 * NOTE: Theoretically, args should be 2, 0, 1, but the current
	 *       implementation actually has a limit at 8, so we may as well
	 *       allow this to be trapped at compile time.
	 */
	eel_export_cfunction(m, 0, "TexParameter", 2, 8, 0, gl_texparameter);
	eel_export_cfunction(m, 0, "DeleteTexture", 1, 0, 0, gl_deletetexture);

	/* Lighting */
	eel_export_cfunction(m, 0, "ShadeModel", 1, 0, 0, gl_shademodel);

	/* Raster */

	/* Drawing */
	eel_export_cfunction(m, 0, "Begin", 1, 0, 0, gl_begin);
	eel_export_cfunction(m, 0, "End", 0, 0, 0, gl_end);
	eel_export_cfunction(m, 0, "Vertex", 2, 2, 0, gl_vertex);
	eel_export_cfunction(m, 0, "Normal", 3, 0, 0, gl_normal);
	eel_export_cfunction(m, 0, "Color", 3, 1, 0, gl_color);
	eel_export_cfunction(m, 0, "TexCoord", 1, 3, 0, gl_texcoord);

	/* Drawing tools (simple performance hacks) */
	eel_export_cfunction(m, 0, "DrawRect", 5, 5, 0, gl_drawrect);
	eel_export_cfunction(m, 0, "DrawCircle", 3, 0, 0, gl_drawcircle);

	/* Constants and enums */
	eel_export_lconstants(m, ebgl_constants);

	loaded = 1;
	eel_disown(m);
	return 0;
}
