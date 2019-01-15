/*
---------------------------------------------------------------------------
	eel_io.c - EEL File and Memory File Classes
---------------------------------------------------------------------------
 * Copyright 2005-2006, 2009, 2012, 2014, 2016, 2019 David Olofson
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

#include <string.h>
#include <stdio.h>
#include "eel_io.h"
#include "e_object.h"
#include "e_string.h"
#include "e_dstring.h"


typedef struct
{
	/* Class Type IDs */
	int		file_cid;
	int		memfile_cid;

	/* "Static" objects */
	EEL_object	*stdin_file;
	EEL_object	*stdout_file;
	EEL_object	*stderr_file;
} IO_moduledata;


/*----------------------------------------------------------
	file class
----------------------------------------------------------*/

static EEL_xno f_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	const char *fn, *mode;
	EEL_file *f;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_file), cid);
	if(!eo)
		return EEL_XMEMORY;
	f = o2EEL_file(eo);
	if(!initc)
	{
		f->handle = NULL;
		eel_o2v(result, eo);
		return 0;
	}
	if(initc != 2)
	{
		eel_o_free(eo);
		return EEL_XARGUMENTS;
	}
	fn = eel_v2s(initv);
	mode = eel_v2s(initv + 1);
	if(!fn || !mode)
	{
		eel_o_free(eo);
		return EEL_XWRONGTYPE;
	}
	f->handle = fopen(fn, mode);
	if(!f->handle)
	{
		eel_o_free(eo);
		return EEL_XFILEOPEN;
	}
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno f_destruct(EEL_object *eo)
{
	EEL_file *f = o2EEL_file(eo);
	if(f->handle && !(f->flags & EEL_FF_DONTCLOSE))
		fclose(f->handle);
	return 0;
}


static EEL_xno f_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_file *f = o2EEL_file(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	switch(is[0])
	{
	  case 'p':
		if(strcmp(is, "position") == 0)
		{
			long pos = ftell(f->handle);
			if(pos < 0)
				return EEL_XFILEERROR;
			eel_l2v(op2, pos);
			return 0;
		}
		break;
/*	  case 'r':
		if(strcmp(is, "read") == 0)
		{
			eel_o2v(op2, f->read);
			return 0;
		}
		break;
	  case 'w':
		if(strcmp(is, "write") == 0)
		{
			eel_o2v(op2, f->write);
			return 0;
		}
		break;
	  case 'c':
		if(strcmp(is, "close") == 0)
		{
			eel_o2v(op2, f->close);
			return 0;
		}
		break;
*/	}
	return EEL_XWRONGINDEX;
}


static EEL_xno f_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_file *f = o2EEL_file(eo);
	const char *is = eel_v2s(op1);
	int iv = eel_v2l(op2);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strcmp(is, "position") == 0)
	{
		int res = fseek(f->handle, iv, SEEK_SET);
		if(res < 0)
			return EEL_XFILESEEK;
	}
	else
		return EEL_XWRONGINDEX;
	return 0;
}


static EEL_xno f_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	long res, pos;
	EEL_file *f = o2EEL_file(eo);
	pos = ftell(f->handle);
	if(pos < 0)
		return EEL_XFILEERROR;
	res = fseek(f->handle, 0, SEEK_END);
	if(res < 0)
		return EEL_XFILESEEK;
	res = ftell(f->handle);
	if(res < 0)
		return EEL_XFILEERROR;
	fseek(f->handle, pos, SEEK_SET);
	op2->classid = EEL_CINTEGER;
	op2->integer.v = res;
	return 0;
}


/*----------------------------------------------------------
	memfile class
----------------------------------------------------------*/

static EEL_xno mf_construct(EEL_vm *vm, EEL_classes cid,
		EEL_value *initv, int initc, EEL_value *result)
{
	EEL_memfile *mf;
	EEL_object *eo = eel_o_alloc(vm, sizeof(EEL_memfile), cid);
	if(!eo)
		return EEL_XMEMORY;
	mf = o2EEL_memfile(eo);
	if(!initc)
	{
		mf->buffer = eel_ds_new(vm, "");
		if(!mf->buffer)
		{
			eel_o_free(eo);
			return EEL_XMEMORY;
		}
	}
	else
	{
		if((initc != 1) || (EEL_CLASS(initv) != EEL_CDSTRING))
		{
			eel_o_free(eo);
			return EEL_XARGUMENTS;
		}
		mf->buffer = initv->objref.v;
		eel_o_own(mf->buffer);
	}
	mf->position = 0;
	eel_o2v(result, eo);
	return 0;
}


static EEL_xno mf_destruct(EEL_object *eo)
{
	EEL_memfile *mf = o2EEL_memfile(eo);
	if(mf->buffer)
		eel_disown(mf->buffer);
	return 0;
}


static EEL_xno mf_getindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_memfile *mf = o2EEL_memfile(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	switch(is[0])
	{
	  case 'p':
		if(strcmp(is, "position") == 0)
		{
			if(!mf->buffer)
				return EEL_XFILECLOSED;
			eel_l2v(op2, mf->position);
			return 0;
		}
		break;
	  case 'b':
		if(strcmp(is, "buffer") == 0)
		{
			if(!mf->buffer)
				return EEL_XFILECLOSED;
			eel_own(mf->buffer);
			eel_o2v(op2, mf->buffer);
			return 0;
		}
		break;
/*	  case 'r':
		if(strcmp(is, "read") == 0)
		{
			eel_o2v(op2, f->read);
			return 0;
		}
		break;
	  case 'w':
		if(strcmp(is, "write") == 0)
		{
			eel_o2v(op2, f->write);
			return 0;
		}
		break;
	  case 'c':
		if(strcmp(is, "close") == 0)
		{
			eel_o2v(op2, f->close);
			return 0;
		}
		break;
*/	}
	return EEL_XWRONGINDEX;
}


static EEL_xno mf_setindex(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_memfile *mf = o2EEL_memfile(eo);
	const char *is = eel_v2s(op1);
	if(!is)
		return EEL_XWRONGTYPE;
	if(strcmp(is, "position") == 0)
	{
		EEL_dstring *fb;
		int iv = eel_v2l(op2);
		if(!mf->buffer)
			return EEL_XFILECLOSED;
		fb = o2EEL_dstring(mf->buffer);
		if(iv < 0)
			return EEL_XFILESEEK;
		if(iv > fb->length)
			return EEL_XFILESEEK;
		mf->position = iv;
	}
	else if(strcmp(is, "buffer") == 0)
	{
		if(EEL_CLASS(op2) != EEL_CDSTRING)
			return EEL_XNEEDDSTRING;
		if(mf->buffer)
			eel_disown(mf->buffer);
		mf->buffer = op2->objref.v;
		eel_own(mf->buffer);
		mf->position = 0;
	}
	else
		return EEL_XWRONGINDEX;
	return 0;
}


static EEL_xno mf_length(EEL_object *eo, EEL_value *op1, EEL_value *op2)
{
	EEL_dstring *buf;
	EEL_memfile *mf = o2EEL_memfile(eo);
	if(!mf->buffer)
		return EEL_XFILECLOSED;
	buf = o2EEL_dstring(mf->buffer);
	op2->classid = EEL_CINTEGER;
	op2->integer.v = buf->length;
	return 0;
}


/*----------------------------------------------------------
	stdio file handle functions
----------------------------------------------------------*/

static EEL_xno io_get_stdin(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	if(!md->stdin_file)
		return EEL_XFILECLOSED;
	eel_own(md->stdin_file);
	eel_o2v(vm->heap + vm->resv, md->stdin_file);
	return 0;
}


static EEL_xno io_get_stdout(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	if(!md->stdout_file)
		return EEL_XFILECLOSED;
	eel_own(md->stdout_file);
	eel_o2v(vm->heap + vm->resv, md->stdout_file);
	return 0;
}


static EEL_xno io_get_stderr(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	if(!md->stderr_file)
		return EEL_XFILECLOSED;
	eel_own(md->stderr_file);
	eel_o2v(vm->heap + vm->resv, md->stderr_file);
	return 0;
}


/*----------------------------------------------------------
	file/memfile functions
----------------------------------------------------------*/

/*
 * Deserialize one instance of the specified type. If there
 * is not enough data in the file, an XEOF exception is thrown.
 */
static EEL_xno read_type(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *args = vm->heap + vm->argv;
	EEL_value *r = vm->heap + vm->resv;
	char buf[8];
	int count;

	/* Figure out how many bytes to read */
	if(EEL_CLASS(args + 1) != EEL_CCLASSID)
		return EEL_XARGUMENTS;
	switch(args[1].integer.v)
	{
	  case EEL_CREAL:
		count = 8;
		break;
	  case EEL_CINTEGER:
		count = 4;
		break;
	  default:
		return EEL_XNOTIMPLEMENTED;
	}

	/* Read! */
	if(EEL_CLASS(args) == md->file_cid)
	{
		int res;
		EEL_file *f = o2EEL_file(args->objref.v);
		if(!f->handle)
			return EEL_XFILECLOSED;
		res = fread(buf, 1, count, f->handle);
		if(res != count)
		{
			if(ferror(f->handle))
				return EEL_XFILEERROR;
			else if((res <= 0) && feof(f->handle))
				return EEL_XEOF;
			else
				count = res;
		}
	}
	else if(EEL_CLASS(args) == md->memfile_cid)
	{
		EEL_memfile *mf = o2EEL_memfile(args->objref.v);
		EEL_dstring *fb;
		if(!mf->buffer)
			return EEL_XFILECLOSED;
		fb = o2EEL_dstring(mf->buffer);
		if(mf->position + count > fb->length)
			return EEL_XEOF;
		memcpy(buf, fb->buffer + mf->position, count);
		mf->position += count;
	}
	else if(EEL_CLASS(args) == EEL_CSTRING)
	{
		EEL_string *fb = o2EEL_string(args->objref.v);
		if(count > fb->length)
			return EEL_XEOF;
		memcpy(buf, fb->buffer, count);
	}
	else if(EEL_CLASS(args) == EEL_CDSTRING)
	{
		EEL_dstring *fb = o2EEL_dstring(args->objref.v);
		if(count > fb->length)
			return EEL_XEOF;
		memcpy(buf, fb->buffer, count);
	}
	else
		return EEL_XWRONGTYPE;

	/* Decode! */
	switch(args[1].integer.v)
	{
	  case EEL_CREAL:
	  {
		union {
			char		c[sizeof(EEL_real)];
			EEL_real	r;
		} cvt;
#if EEL_BYTEORDER == EEL_BIG_ENDIAN
		memcpy(&cvt.c, buf, sizeof(EEL_real));
#else
		int n;
		for(n = 0; n < sizeof(EEL_real); ++n)
			cvt.c[sizeof(EEL_real) - 1 - n] = buf[n];
#endif
		r->real.v = cvt.r;
		r->classid = EEL_CREAL;
		return 0;
	  }
	  case EEL_CINTEGER:
	  {
		union {
			char		c[4];
			EEL_int32	i;
		} cvt;
#if EEL_BYTEORDER == EEL_BIG_ENDIAN
		memcpy(&cvt.c, buf, sizeof(cvt.i));
#else
		cvt.c[3] = buf[0];
		cvt.c[2] = buf[1];
		cvt.c[1] = buf[2];
		cvt.c[0] = buf[3];
#endif
		r->integer.v = cvt.i;
		r->classid = EEL_CINTEGER;
		return 0;
	  }
	  default:
		return EEL_XINTERNAL;	/* How the f*** did we get here!? */
	}
}


static EEL_xno io_read(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *args = vm->heap + vm->argv;
	EEL_object *so;
	EEL_dstring *ds;
	int count;

	/* # of bytes to read */
	if(vm->argc >= 2)
	{
		switch(EEL_CLASS(args + 1))
		{
		  case EEL_CINTEGER:
		  case EEL_CREAL:
			count = eel_v2l(args + 1);
			if(count < 0)
				return EEL_XLOWVALUE;
			break;
		  default:
			return read_type(vm);
		}
	}
	else
		count = 1;	/* Default: One byte. */

	/* Create dstring for result */
	so = eel_ds_nnew(vm, NULL, count);
	if(!so)
		return EEL_XMEMORY;
	ds = o2EEL_dstring(so);

	/* Read! */
	if(EEL_CLASS(args) == md->file_cid)
	{
		int res;
		EEL_file *f = o2EEL_file(args->objref.v);
		if(!f->handle)
		{
			eel_o_disown_nz(so);
			return EEL_XFILECLOSED;
		}
		if(count)
		{
			res = fread(ds->buffer, 1, count, f->handle);
			if(res != count)
			{
				if(ferror(f->handle))
				{
					eel_o_disown_nz(so);
					return EEL_XFILEERROR;
				}
				else if((res <= 0) && feof(f->handle))
				{
					eel_o_disown_nz(so);
					return EEL_XEOF;
				}
				else
					count = res;
			}
		}
		ds->length = count;
	}
	else if(EEL_CLASS(args) == md->memfile_cid)
	{
		EEL_memfile *mf = o2EEL_memfile(args->objref.v);
		EEL_dstring *fb;
		if(!mf->buffer)
		{
			eel_o_disown_nz(so);
			return EEL_XFILECLOSED;
		}
		if(count)
		{
			fb = o2EEL_dstring(mf->buffer);
			if(mf->position >= fb->length)
			{
				eel_o_disown_nz(so);
				return EEL_XEOF;
			}
			if(mf->position + count > fb->length)
				count = fb->length - mf->position;
			memcpy(ds->buffer, fb->buffer + mf->position, count);
			mf->position += count;
		}
		ds->length = count;
	}
	else
	{
		eel_o_disown_nz(so);
		return EEL_XWRONGTYPE;
	}

	eel_o2v(vm->heap + vm->resv, so);
	return 0;
}


typedef struct IO_writedesc IO_writedesc;
struct IO_writedesc
{
	EEL_file	*f;
	EEL_memfile	*mf;
	int		count;
	EEL_xno (*write)(EEL_vm *vm, IO_writedesc *wrd, const char *buf, int len);
};


static EEL_xno io_write_file(EEL_vm *vm, IO_writedesc *wrd,
		const char *buf, int len)
{
	int res = fwrite(buf, 1, len, wrd->f->handle);
	if(res != len)
		return EEL_XFILEERROR;
	wrd->count += res;
	return 0;
}


static EEL_xno io_write_memfile(EEL_vm *vm, IO_writedesc *wrd,
		const char *buf, int len)
{
	EEL_xno res = eel_ds_write(wrd->mf->buffer, wrd->mf->position,
			buf, len);
	if(res)
		return res;
	wrd->count += len;
	wrd->mf->position += len;
	return 0;
}


static EEL_xno io_write(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	int i;
	IO_writedesc wrd;
	EEL_value *args = vm->heap + vm->argv;

	/* Prepare for writing */
	wrd.f = NULL;
	wrd.mf = NULL;
	wrd.count = 0;
	if(EEL_CLASS(args) == md->file_cid)
	{
		wrd.f = o2EEL_file(args->objref.v);
		if(!wrd.f->handle)
			return EEL_XFILECLOSED;
		wrd.write = io_write_file;
	}
	else if(EEL_CLASS(args) == md->memfile_cid)
	{
		wrd.mf = o2EEL_memfile(args->objref.v);
		if(!wrd.mf->buffer)
			return EEL_XFILECLOSED;
		wrd.write = io_write_memfile;
	}
	else
		return EEL_XWRONGTYPE;

	/* Serialize and write arguments! */
	for(i = 1; i < vm->argc; ++i)
	{
		EEL_value *v = args + i;
		EEL_xno x;
		switch(EEL_CLASS(v))
		{
		  case EEL_CREAL:
		  {
			union {
				char		c[sizeof(EEL_real)];
				EEL_real	r;
			} cvt;
#if EEL_BYTEORDER == EEL_BIG_ENDIAN
			cvt.r = v->real.v;
			x = wrd.write(vm, &wrd, cvt.c, sizeof(EEL_real));
#else
			char lb[sizeof(EEL_real)];
			int n;
			cvt.r = v->real.v;
			for(n = 0; n < sizeof(EEL_real); ++n)
				lb[n] = cvt.c[sizeof(EEL_real) - 1 - n];
			x = wrd.write(vm, &wrd, lb, sizeof(EEL_real));
#endif
			if(x)
				return x;
			break;
		  }
		  case EEL_CINTEGER:
		  {
			union {
				char		c[4];
				EEL_int32	i;
			} cvt;
#if EEL_BYTEORDER == EEL_BIG_ENDIAN
			cvt.i = v->integer.v;
			x = wrd.write(vm, &wrd, cvt.c, 4);
#else
			char lb[4];
			cvt.i = v->integer.v;
			lb[0] = cvt.c[3];
			lb[1] = cvt.c[2];
			lb[2] = cvt.c[1];
			lb[3] = cvt.c[0];
			x = wrd.write(vm, &wrd, lb, 4);
#endif
			if(x)
				return x;
			break;
		  }
		  case EEL_CSTRING:
		  case EEL_CDSTRING:
		  {
		  	const char *s = eel_v2s(v);
			int len = eel_length(v->objref.v);
			x = wrd.write(vm, &wrd, s, len);
			if(x)
				return x;
			break;
		  }
		  default:
			return EEL_XARGUMENTS;
		}
	}

	eel_l2v(vm->heap + vm->resv, wrd.count);
	return 0;
}


static EEL_xno io_flush(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *arg = vm->heap + vm->argv;
	if(!vm->argc)
	{
		if(fflush(NULL))
			return EEL_XFILEERROR;
		return 0;
	}
	if(EEL_CLASS(arg) == md->file_cid)
	{
		EEL_file *f = o2EEL_file(arg->objref.v);
		if(!f->handle)
			return EEL_XFILECLOSED;
		if(fflush(f->handle))
			return EEL_XFILEERROR;
		return 0;
	}
	else if(EEL_CLASS(arg) == md->memfile_cid)
		return 0;
	else
		return EEL_XWRONGTYPE;
	return 0;
}


static EEL_xno io_close(EEL_vm *vm)
{
	IO_moduledata *md = (IO_moduledata *)eel_get_current_moduledata(vm);
	EEL_value *arg = vm->heap + vm->argv;
	if(EEL_CLASS(arg) == md->file_cid)
	{
		EEL_file *f = o2EEL_file(arg->objref.v);
		if(!f->handle)
			return EEL_XFILECLOSED;
		fclose(f->handle);
		f->handle = NULL;
	}
	else if(EEL_CLASS(arg) == md->memfile_cid)
	{
		EEL_memfile *mf = o2EEL_memfile(arg->objref.v);
		if(!mf->buffer)
			return EEL_XFILECLOSED;
		eel_free(vm, mf->buffer);
		mf->buffer = NULL;
		mf->position = 0;
	}
	else
		return EEL_XWRONGTYPE;
	return 0;
}


/*----------------------------------------------------------
	Unloading
----------------------------------------------------------*/

static EEL_xno io_unload(EEL_object *m, int closing)
{
	if(closing)
	{
		IO_moduledata *md = (IO_moduledata *)eel_get_moduledata(m);
		eel_disown(md->stdin_file);
		eel_disown(md->stdout_file);
		eel_disown(md->stderr_file);
		eel_free(m->vm, md);
		return 0;
	}
	else
		return EEL_XREFUSE;
}


/*----------------------------------------------------------
	Initialization
----------------------------------------------------------*/

static EEL_object *io_create_fh_wrapper(EEL_vm *vm, int cid, FILE *f)
{
	EEL_value v;
	if(eel_o_construct(vm, cid, NULL, 0, &v))
		return NULL;
	o2EEL_file(eel_v2o(&v))->handle = f;
	o2EEL_file(eel_v2o(&v))->flags |= EEL_FF_DONTCLOSE;
	return eel_v2o(&v);
}


EEL_xno eel_io_init(EEL_vm *vm)
{
	EEL_object *m;
	EEL_object *c;
	IO_moduledata *md = (IO_moduledata *)eel_malloc(vm,
			sizeof(IO_moduledata));
	if(!md)
		return EEL_XMEMORY;

	m = eel_create_module(vm, "io", io_unload, md);
	if(!m)
	{
		eel_free(vm, md);
		return EEL_XMODULEINIT;
	}

	/* Types */
	c = eel_export_class(m, "file", -1, f_construct, f_destruct, NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, f_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, f_setindex);
	eel_set_metamethod(c, EEL_MM_LENGTH, f_length);
	md->file_cid = eel_class_cid(c);

	c = eel_export_class(m, "memfile", -1, mf_construct, mf_destruct,
			NULL);
	eel_set_metamethod(c, EEL_MM_GETINDEX, mf_getindex);
	eel_set_metamethod(c, EEL_MM_SETINDEX, mf_setindex);
	eel_set_metamethod(c, EEL_MM_LENGTH, mf_length);
	md->memfile_cid = eel_class_cid(c);

	/* Functions */
	eel_export_cfunction(m, 1, "stdin", 0, 0, 0, io_get_stdin);
	eel_export_cfunction(m, 1, "stdout", 0, 0, 0, io_get_stdout);
	eel_export_cfunction(m, 1, "stderr", 0, 0, 0, io_get_stderr);

	eel_export_cfunction(m, 1, "read", 1, 1, 0, io_read);
	eel_export_cfunction(m, 1, "write", 1, 0, 1, io_write);
	eel_export_cfunction(m, 0, "flush", 0, 1, 0, io_flush);
	eel_export_cfunction(m, 0, "close", 1, 0, 0, io_close);

	/* "Static" objects */
	md->stdin_file = io_create_fh_wrapper(vm, md->file_cid, stdin);
	md->stdout_file = io_create_fh_wrapper(vm, md->file_cid, stdout);
	md->stderr_file = io_create_fh_wrapper(vm, md->file_cid, stderr);

	SETNAME(m, "EEL Built-in IO Module");
	eel_disown(m);
	return 0;
}
