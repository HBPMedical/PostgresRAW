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


#ifndef NODB_COL_H
#define NODB_COL_H

#include "noDB/auxiliary/NoDBArena.h"


typedef int NoDBCol_t;

typedef struct NoDBColVector_t {
    int                     size;
    NoDBCol_t               *cols;
} NoDBColVector_t;

typedef struct NoDBColList_t {
    NoDBCol_t               col;
    struct NoDBColList_t    *next;
} NoDBColList_t;

NoDBColVector_t NoDBColVectorInit(int size);
NoDBColVector_t NoDBColVectorArenaInit(NoDBArena_t a, int size);
void            NoDBColVectorDestroy(NoDBColVector_t v);
int             NoDBColVectorIndex(NoDBColVector_t v, NoDBCol_t col);
int             NoDBColVectorBinarySearch(NoDBColVector_t v, NoDBCol_t col);
NoDBCol_t       NoDBColVectorGet(NoDBColVector_t v, int i);
void            NoDBColVectorSet(NoDBColVector_t v, int i, NoDBCol_t col);
int             NoDBColVectorSize(NoDBColVector_t v);
int             NoDBColVectorContains(NoDBColVector_t v, NoDBCol_t col);
void            NoDBColVectorApply(NoDBColVector_t v, void (*fn) (NoDBCol_t, void *), void *arg);
void            NoDBColVectorSort(NoDBColVector_t v);
NoDBColVector_t NoDBColVectorFromList(NoDBColList_t *l);
NoDBColVector_t NoDBColVectorArenaFromList(NoDBColList_t *l, NoDBArena_t a);

void            NoDBColListDestroy(NoDBColList_t *l);
int             NoDBColListContains(NoDBColList_t *l, NoDBCol_t col);
int             NoDBColListContainsColList(NoDBColList_t *l, NoDBColList_t *subset);
NoDBColList_t   *NoDBColListAdd(NoDBColList_t *l, NoDBCol_t col);
NoDBColList_t   *NoDBColListArenaAdd(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col);
NoDBColList_t   *NoDBColListAddUnique(NoDBColList_t *l, NoDBCol_t col);
NoDBColList_t   *NoDBColListArenaAddUnique(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col);
NoDBColList_t   *NoDBColListArenaAddSortUnique(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col);
NoDBColList_t   *NoDBColListArenaDelete(NoDBColList_t *l, NoDBCol_t col);
int             NoDBColListSize(NoDBColList_t *l);
NoDBColList_t   *NoDBColListDuplicate(NoDBColList_t *l);
NoDBColList_t   *NoDBColListArenaDuplicate(NoDBColList_t *l, NoDBArena_t a);

#endif /* NODB_COL_H */
