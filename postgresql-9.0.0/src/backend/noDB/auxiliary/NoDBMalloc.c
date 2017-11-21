/*
                        NoDB Project 
        Query Processing On Raw Data Files using PostgresRAW

                   Copyright (c) 2011-2013
  Data Intensive Applications and Systems Labaratory (DIAS)
           Ecole Polytechnique Federale de Lausanne

                     All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright notice
and this permission notice appear in all copies of the software, derivative
works or modified versions, and any portions thereof, and that both notices
appear in supporting documentation.

This code is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. THE AUTHORS AND ECOLE POLYTECHNIQUE FEDERALE DE LAUSANNE
DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
USE OF THIS SOFTWARE.
*/


#include "noDB/auxiliary/NoDBMalloc.h"



#if defined(NODB_MYSQL)
#include <my_sys.h>
#include <m_string.h>
#else
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#endif

void *NoDBMalloc(size_t size)
{
#if defined(NODB_MYSQL)
    return my_malloc(size, MYF(MY_WME));
#else
    unsigned char *ptr = malloc(size);
    if (!ptr) {
        assert(0);
    }
    return ptr;
#endif
}

void *NoDBRealloc(void *ptr, size_t size)
{
#if defined(NODB_MYSQL)
    return my_realloc(ptr, size, MYF(MY_WME));
#else
    unsigned char *nptr = realloc(ptr, size);
    if (!nptr) {
        assert(0);
    }
    return nptr;
#endif
}

void NoDBFree(void *ptr)
{
    if (ptr) {
#if defined(NODB_MYSQL)
        my_free(ptr);
#else
        free(ptr);
#endif
    }
}

void *NoDBCopy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}

void *NoDBDup(const void *src, size_t size)
{
#if defined(NODB_MYSQL)
    return my_memdup(src, size, MYF(MY_WME));
#else
    unsigned char *dest = NoDBMalloc(size);
    return memcpy(dest, src, size);
#endif
}

size_t NoDBStringLen(const char *s)
{
    return strlen(s);
}
