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


/* Memory arena implementation based on LCC's arena by Christopher Fraser and David Hanson. */

// FIXME: Handle allocations bigger than block size (or fail gracefully).

#ifndef NODB_ARENA_H
#define NODB_ARENA_H

//#include "NoDB.h"
#include "stdio.h"


typedef enum NoDBArena_t {
    NODB_ARENA_STRATEGY = 0,
    NODB_ARENA_ITERATOR = 1,
    NODB_ARENA_QUERY    = 2,
    NODB_ARENA_CACHE    = 3
} NoDBArena_t;

void        *NoDBArenaAlloc(NoDBArena_t a, size_t size);
void        NoDBArenaFree(NoDBArena_t a);
void        NoDBArenaDestroy(NoDBArena_t a);
void        NoDBArenaDestroyAll();


#endif  /* NODB_ARENA_H */
