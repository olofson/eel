/*
---------------------------------------------------------------------------
	e_platform.h - Platform specific details
---------------------------------------------------------------------------
 * Copyright 2014 David Olofson
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

#ifndef EEL_E_PLATFORM_H
#define EEL_E_PLATFORM_H

#include "EEL_xno.h"

#ifdef _WIN32
# if defined(_MSC_VER) && (_MSC_VER >= 1500)
#  include <intrin.h>
#  define HAVE_MSC_ATOMICS 1
# endif
# define WIN32_LEAN_AND_MEAN
# define STRICT
# include <windows.h>
# include <mmsystem.h>
#elif defined(__MACOSX__)
# include <libkern/OSAtomic.h>
# include <sched.h>
#else
# include <sched.h>
# include <sys/time.h>
# include <sys/wait.h>
# include <pthread.h>
# include <errno.h>
#endif

#ifdef _WIN32
char *strndup(const char *s, size_t size);
#endif


/*---------------------------------------------------------
	Atomics
---------------------------------------------------------*/

typedef int EEL_atomic;

static inline int eel_atomic_cas(EEL_atomic *a, int ov, int nv)
{
#ifdef HAVE_MSC_ATOMICS
	return (_InterlockedCompareExchange((long*)a, nv, ov) == ov);
#elif defined(_WIN32)
	return (InterlockedCompareExchange((long*)a, nv, ov) == ov);
#elif defined(__MACOSX__)
	return OSAtomicCompareAndSwap32Barrier(ov, nv, a);
#else
	/* Let's hope we have GCC atomics, then! */
	return __sync_bool_compare_and_swap(a, ov, nv);
#endif
}

static inline int eel_atomic_add(EEL_atomic *a, int v)
{
	while(1)
	{
		int ov = *a;
		if(eel_atomic_cas(a, ov, (ov + v)))
			return ov;
	}
}


/*---------------------------------------------------------
	Mutex
---------------------------------------------------------*/

typedef struct EEL_mutex
{
#ifdef _WIN32
	CRITICAL_SECTION	cs;
#else
	pthread_mutex_t		mutex;
#endif
} EEL_mutex;


/*
 * WIN32 implementation
 */
#ifdef _WIN32
static inline EEL_xno eel_mutex_open(EEL_mutex *mtx)
{
	InitializeCriticalSectionAndSpinCount(&mtx->cs, 2000);
	return EEL_XOK;
}

static inline int eel_mutex_try_lock(EEL_mutex *mtx)
{
	return TryEnterCriticalSection(&mtx->cs);
}

static inline void eel_mutex_lock(EEL_mutex *mtx)
{
	EnterCriticalSection(&mtx->cs);
}

static inline void eel_mutex_unlock(EEL_mutex *mtx)
{
	LeaveCriticalSection(&mtx->cs);
}

static inline void eel_mutex_close(EEL_mutex *mtx)
{
	DeleteCriticalSection(&mtx->cs);
}


/*
 * pthreads implementation
 */
#else	/* _WIN32 */
static inline EEL_xno eel_mutex_open(EEL_mutex *mtx)
{
	switch(pthread_mutex_init(&mtx->mutex, NULL))
	{
	  case 0:
		return EEL_XOK;
	  case EBUSY:
		return EEL_XDEVICEOPENED;
	  default:
		return EEL_XDEVICEOPEN;
	}
}

static inline int eel_mutex_try_lock(EEL_mutex *mtx)
{
	return (pthread_mutex_trylock(&mtx->mutex) == 0);
}

static inline void eel_mutex_lock(EEL_mutex *mtx)
{
	pthread_mutex_lock(&mtx->mutex);
}

static inline void eel_mutex_unlock(EEL_mutex *mtx)
{
	pthread_mutex_unlock(&mtx->mutex);
}

static inline void eel_mutex_close(EEL_mutex *mtx)
{
	pthread_mutex_destroy(&mtx->mutex);
}
#endif	/* _WIN32 */


/*---------------------------------------------------------
	CPU yield
---------------------------------------------------------*/

static inline void eel_yield(void)
{
#ifdef _WIN32
	/*
	 * NOTE: Windows XP and older actually only yield if other threads of
	 *       EQUAL PRIORITY is ready to run! Should we care...?
	 */
	Sleep(0);
#else
	sched_yield();
#endif
}

#endif /* EEL_E_PLATFORM_H */
