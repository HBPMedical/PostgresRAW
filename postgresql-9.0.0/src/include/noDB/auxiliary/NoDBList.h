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


#ifndef NODB_LIST_H
#define NODB_LIST_H

#include "noDB/auxiliary/NoDBArena.h"

typedef struct NoDBList_t {
    void                    *ptr;
    struct NoDBList_t       *next;
} NoDBList_t;

void            NoDBListDestroy(NoDBList_t *l);
NoDBList_t      *NoDBListAdd(NoDBList_t *l, void *ptr);
NoDBList_t      *NoDBListArenaAdd(NoDBList_t *l, NoDBArena_t a, void *ptr);
int             NoDBListSize(NoDBList_t *l);

typedef struct NoDBIntList_t {
    int                     v;
    struct NoDBIntList_t    *next;
} NoDBIntList_t;

void            NoDBIntListDestroy(NoDBIntList_t *l);
int             NoDBIntListContains(NoDBIntList_t *l, int v);
NoDBIntList_t   *NoDBIntListAdd(NoDBIntList_t *l, int v);
NoDBIntList_t   *NoDBIntListArenaAdd(NoDBIntList_t *l, NoDBArena_t a, int v);
NoDBIntList_t   *NoDBIntListArenaAddUnique(NoDBIntList_t *l, NoDBArena_t a, int v);
NoDBIntList_t   *NoDBIntListDelete(NoDBIntList_t *l);
int             NoDBIntListSize(NoDBIntList_t *l);
NoDBIntList_t   *NoDBIntListDuplicate(NoDBIntList_t *l);
NoDBIntList_t   *NoDBIntListArenaDuplicate(NoDBIntList_t *l, NoDBArena_t a);
int             *NoDBIntVectorFromList(NoDBIntList_t *l);
int             *NoDBIntVectorArenaFromList(NoDBIntList_t *l, NoDBArena_t a);
#endif	/* NODB_LIST_H */
