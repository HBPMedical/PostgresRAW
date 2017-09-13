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


#ifndef NODB_ROW_H
#define NODB_ROW_H

#include "noDB/auxiliary/NoDBArena.h"

typedef unsigned long NoDBRow_t;

typedef struct {
    int                             size;
    NoDBRow_t                       *rows;
} NoDBRowVector_t;

typedef struct NoDBRowList_t {
    NoDBRow_t                       row;
    struct NoDBRowList_t            *next;
} NoDBRowList_t;

NoDBRowVector_t         NoDBRowVectorInit(int size);
NoDBRowVector_t         NoDBRowVectorArenaInit(NoDBArena_t a, int size);
void                    NoDBRowVectorDestroy(NoDBRowVector_t v);
NoDBRow_t               NoDBRowVectorGet(NoDBRowVector_t v, int i);
void                    NoDBRowVectorSet(NoDBRowVector_t v, int i, NoDBRow_t row);
int                     NoDBRowVectorSize(NoDBRowVector_t v);
void                    NoDBRowVectorApply(NoDBRowVector_t v, void (*fn) (NoDBRow_t, void *), void *arg);
void                    NoDBRowVectorSort(NoDBRowVector_t v);
NoDBRowVector_t         NoDBRowVectorFromList(NoDBRowList_t *l);
NoDBRowVector_t         NoDBRowVectorArenaFromList(NoDBRowList_t *l, NoDBArena_t a);

void                    NoDBRowListDestroy(NoDBRowList_t *l);
NoDBRowList_t           *NoDBRowListAddUnique(NoDBRowList_t *l, NoDBRow_t row);
NoDBRowList_t           *NoDBRowListArenaAddUnique(NoDBRowList_t *l, NoDBArena_t a, NoDBRow_t row);
NoDBRowList_t           *NoDBRowListArenaAddSortUnique(NoDBRowList_t *l, NoDBArena_t a, NoDBRow_t row);
int                     NoDBRowListSize(NoDBRowList_t *l);


#endif /* NODB_ROW_H */
