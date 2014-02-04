#include "kfc.h"

/*
Copyright (c) 2003-2004, Mark Borgerding
Copyright (c) 2006 David Olofson (Real FFT caching)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the author nor the names of any contributors may be used
      to endorse or promote products derived from this software without
      specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

/*
TODO:
	* Merge cached_fft and cached_fftr into one polymorphic struct.

	* Remove the 'inverse' field and use separate caches instead,
	  for quicker lookups.
*/

typedef struct cached_fft cached_fft;

struct cached_fft
{
    int nfft;
    int inverse;
    kiss_fft_cfg cfg;
    cached_fft *next;
};

static cached_fft *cache_root = NULL;
static int ncached=0;


typedef struct cached_fftr cached_fftr;

struct cached_fftr
{
    int nfft;
    int inverse;
    kiss_fftr_cfg cfg;
    cached_fftr *next;
};

static cached_fftr *r_cache_root = NULL;
static int r_ncached=0;


static kiss_fft_cfg find_cached_fft(int nfft,int inverse)
{
    size_t len = 0;
    cached_fft *cur = cache_root;
    cached_fft *prev = NULL;
    while ( cur ) {
        if ( cur->nfft == nfft && inverse == cur->inverse )
            break;/*found the right node*/
        prev = cur;
        cur = prev->next;
    }
    if (cur== NULL) {
        /* no cached node found, need to create a new one*/
        kiss_fft_alloc(nfft,inverse,0,&len);
        cur = (cached_fft *)KISS_FFT_MALLOC((sizeof(struct cached_fft) + len ));
        if (cur == NULL)
            return NULL;
        cur->cfg = (kiss_fft_cfg)(cur+1);
        kiss_fft_alloc(nfft,inverse,cur->cfg,&len);
        cur->nfft=nfft;
        cur->inverse=inverse;
        cur->next = NULL;
        if ( prev )
            prev->next = cur;
        else
            cache_root = cur;
        ++ncached;
    }
    return cur->cfg;
}


static kiss_fftr_cfg find_cached_fftr(int nfft,int inverse)
{
    size_t len = 0;
    cached_fftr *cur = r_cache_root;
    cached_fftr *prev = NULL;
    while ( cur ) {
        if ( cur->nfft == nfft && inverse == cur->inverse )
            break;/*found the right node*/
        prev = cur;
        cur = prev->next;
    }
    if (cur== NULL) {
        /* no cached node found, need to create a new one*/
        kiss_fftr_alloc(nfft,inverse,0,&len);
        cur = (cached_fftr *)KISS_FFT_MALLOC((sizeof(struct cached_fftr) + len ));
        if (cur == NULL)
            return NULL;
        cur->cfg = (kiss_fftr_cfg)(cur+1);
        kiss_fftr_alloc(nfft,inverse,cur->cfg,&len);
        cur->nfft=nfft;
        cur->inverse=inverse;
        cur->next = NULL;
        if ( prev )
            prev->next = cur;
        else
            r_cache_root = cur;
        ++ncached;
    }
    return cur->cfg;
}


void kfc_cleanup(void)
{
    cached_fft *cur = cache_root;
    cached_fft *next = NULL;

    cached_fftr *r_cur = r_cache_root;
    cached_fftr *r_next = NULL;

    while (cur){
        next = cur->next;
        free(cur);
        cur=next;
    }
    ncached=0;
    cache_root = NULL;

    while (r_cur){
        r_next = r_cur->next;
        free(r_cur);
        r_cur=r_next;
    }
    r_ncached=0;
    r_cache_root = NULL;
}


void kfc_fft(int nfft, const kiss_fft_cpx * fin,kiss_fft_cpx * fout)
{
    kiss_fft( find_cached_fft(nfft,0),fin,fout );
}


void kfc_ifft(int nfft, const kiss_fft_cpx * fin,kiss_fft_cpx * fout)
{
    kiss_fft( find_cached_fft(nfft,1),fin,fout );
}


void kfc_fftr(int nfft, const kiss_fft_scalar *timedata, kiss_fft_cpx *fout)
{
    kiss_fftr( find_cached_fftr(nfft,0),timedata,fout);
}


void kfc_ifftr(int nfft, const kiss_fft_cpx *fin, kiss_fft_scalar *timedata)
{
    kiss_fftri( find_cached_fftr(nfft,1),fin,timedata);
}


#ifdef KFC_TEST
static void check(int nc)
{
    if (ncached != nc) {
        fprintf(stderr,"ncached should be %d,but it is %d\n",nc,ncached);
        exit(1);
    }
}

int main(void)
{
    kiss_fft_cpx buf1[1024],buf2[1024];
    memset(buf1,0,sizeof(buf1));
    check(0);
    kfc_fft(512,buf1,buf2);
    check(1);
    kfc_fft(512,buf1,buf2);
    check(1);
    kfc_ifft(512,buf1,buf2);
    check(2);
    kfc_cleanup();
    check(0);
    return 0;
}
#endif
