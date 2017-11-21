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


#ifndef NODB_MALLOC_H
#define NODB_MALLOC_H

#include <stddef.h>

void        *NoDBMalloc(size_t size);
void        *NoDBRealloc(void *ptr, size_t size);
void        NoDBFree(void *ptr);

void        *NoDBCopy(void *dest, const void *src, size_t n);
void        *NoDBDup(const void *src, size_t n);

size_t      NoDBStringLen(const char *s);

#endif	/* NODB_MALLOC_H */
