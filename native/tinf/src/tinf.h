/*
 * tinf  -  tiny inflate library (inflate, gzip, zlib)
 *
 * version 1.00
 *
 * Copyright (c) 2003 by Joergen Ibsen / Jibz
 * All Rights Reserved
 *
 * http://www.ibsensoftware.com/
 */

#include <stdlib.h>

#ifndef TINF_H_INCLUDED
#define TINF_H_INCLUDED

/* calling convention */
#ifndef TINFCC
 #ifdef __WATCOMC__
  #define TINFCC __cdecl
 #else
  #define TINFCC
 #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TINF_OK             0
#define TINF_DATA_ERROR    (-3)

/* function prototypes */

void TINFCC tinf_init();

size_t tinf_data_size();

int TINFCC tinf_uncompress(void *dest, unsigned int *destLen,
                           const void *source, unsigned int sourceLen, void *v_tinf_data);

int TINFCC tinf_gzip_uncompress(void *dest, unsigned int *destLen,
                                const void *source, unsigned int sourceLen);

int TINFCC tinf_zlib_uncompress(void *dest, unsigned int *destLen,
                                const void *source, unsigned int sourceLen, void *v_tinf_data);

unsigned int TINFCC tinf_adler32(const void *data, unsigned int length);

unsigned int TINFCC tinf_crc32(const void *data, unsigned int length);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TINF_H_INCLUDED */
