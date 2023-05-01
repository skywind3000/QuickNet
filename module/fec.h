/*
 * fec.c -- forward error correction based on Vandermonde matrices
 * 980614
 * (C) 1997-98 Luigi Rizzo (luigi@iet.unipi.it)
 *
 * Portions derived from code by Phil Karn (karn@ka9q.ampr.org),
 * Robert Morelos-Zaragoza (robert@spectra.eng.hawaii.edu) and Hari
 * Thirumoorthy (harit@spectra.eng.hawaii.edu), Aug 1995
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:

 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/*
 * The following parameter defines how many bits are used for
 * field elements. The code supports any value from 2 to 16
 * but fastest operation is achieved with 8 bit elements
 * This is the only parameter you may want to change.
 */

#ifndef __FEC_H__
#define __FEC_H__

#ifndef DEFS_H
#define DEFS_H

//#define  HAVE_CONFIG_H
//#ifdef HAVE_CONFIG_H
//#include "config.h"
/* Define if you don't have `vprintf' but do have `_doprnt.' */
#undef HAVE_DOPRNT

/* Define if you have the <errno.h> header file. */
#undef HAVE_ERRNO_H

/* Define if you have the `gettimeofday' function. */
#undef HAVE_GETTIMEOFDAY

/* Define if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if you have the <memory.h> header file. */
#define  HAVE_MEMORY_H

/* Define if you have the `memset' function. */
//#undef HAVE_MEMSET

/* Define if you have the <netinet/in.h> header file. */
//#undef HAVE_NETINET_IN_H

/* Define if you have the <stdint.h> header file. */
//#undef HAVE_STDINT_H

/* Define if you have the <stdlib.h> header file. */
#define  HAVE_STDLIB_H

/* Define if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define if the system has the type `u_char'. */
#undef HAVE_U_CHAR

/* Define if the system has the type `u_int32_t'. */
#undef HAVE_U_INT32_T

/* Define if you have the `vprintf' function. */
#undef HAVE_VPRINTF

/* The size of a `int', as computed by sizeof. */
#undef SIZEOF_INT

/* The size of a `long', as computed by sizeof. */
#undef SIZEOF_LONG

/* Define if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define if you can safely include both <sys/time.h> and <time.h>. */
#undef TIME_WITH_SYS_TIME

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
#undef inline

/* Define to `long' if <sys/types.h> does not define. */
#undef off_t

/* Define to `unsigned' if <sys/types.h> does not define. */
#undef size_t

//#endif

#include <stdio.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
 #ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
//#define  HAVE_MEMCPY
#if STDC_HEADERS
# include <string.h>
#else
//# if !HAVE_MEMCPY
//#  define memcpy(d, s, n) bcopy ((s), (d), (n))
//#  define memmove(d, s, n) bcopy ((s), (d), (n))
//# endif
#endif
#include <stdlib.h>

//#define HAVE_MEMSET
//#ifndef HAVE_BZERO
//#ifdef HAVE_MEMSET
#define bzero(d, siz) memset((d), 0, (siz))
#define bcopy(s, d, siz) memcpy((d), (s), (siz))
//#else
//#error I need bzero or memset!
//#endif
//#endif

#if HAVE_NETINET_IN_H
# include <netinet/in.h>        /* for htonl and ntohl */
#endif

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#if !HAVE_U_CHAR
	typedef unsigned char u_char;
#endif

#if !HAVE_U_INT32_T
# if SIZEOF_INT == 4
	typedef unsigned int u_int32_t;
# else
#  if SIZEOF_LONG == 4
	typedef unsigned long u_int32_t;
#  endif
# endif
#endif

#endif  /* DEFS_H */


#ifndef GF_BITS
#define GF_BITS  8      /* code over GF(2**GF_BITS) - change to suit */
#endif

#define GF_SIZE ((1 << GF_BITS) - 1)    /* powers of \alpha */

#ifdef __cplusplus
extern "C" {
#endif

void *fec_new(int k, int n);
void fec_free(void *p);

void fec_encode(void *code, u_char **src, u_char *dst, int index, int sz);
int fec_decode(void *code, u_char **pkt, int *index, int sz);


#ifdef __cplusplus
}
#endif

#endif


