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


#ifndef NODBCACHEWORLD_H_
#define NODBCACHEWORLD_H_

#include "noDB/NoDBCache.h"

#define MAX_RELATION_NAME    64
#define NELLEM_ALLOC         5

typedef struct NoDBEOLCache_t
{
	char relation[MAX_RELATION_NAME];
	struct NoDBCache_t *EOLCache;
} NoDBEOLCache_t;


typedef struct NoDBEOLCacheWorld_t
{
	int	size;					/* Current size */
	struct NoDBEOLCache_t *vector; 	/* List of caches */
} NoDBEOLCacheWorld_t;


NoDBCache_t *NoDBInitEOLCache(char* relation, NoDBRow_t rows);
NoDBCache_t *NoDBGetEOLCache(char* relation);
void NoDBFreeEOLCache(char* relation);


#endif /* NODBCACHEWORLD_H_ */
