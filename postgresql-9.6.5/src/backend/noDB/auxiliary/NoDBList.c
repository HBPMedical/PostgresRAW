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


#include "noDB/auxiliary/NoDBList.h"

#include "noDB/auxiliary/NoDBMalloc.h"

void NoDBListDestroy(NoDBList_t *l)
{
    while (l) {
        NoDBList_t *next = l->next;
        NoDBFree(l);
        l = next;
    }
}

NoDBList_t* NoDBListAdd(NoDBList_t *l, void *ptr)
{
    NoDBList_t *node = NoDBMalloc(sizeof(NoDBList_t));
    node->ptr = ptr;
    node->next = l;
    return node;
}

NoDBList_t* NoDBListArenaAdd(NoDBList_t *l, NoDBArena_t a, void *ptr)
{
    NoDBList_t *node = NoDBArenaAlloc(a, sizeof(NoDBList_t));
    node->ptr = ptr;
    node->next = l;
    return node;
}

int NoDBListSize(NoDBList_t *l)
{
    int n = 0;
    while (l) {
        n++;
        l = l->next;
    }
    return n;
}

void NoDBIntListDestroy(NoDBIntList_t *l)
{
    while (l) {
        NoDBIntList_t *next = l->next;
        NoDBFree(l);
        l = next;
    }
}

int NoDBIntListContains(NoDBIntList_t *l, int v)
{
    while (l) {
        if (l->v == v) {
            return 1;
        }
        l = l->next;
    }
    return 0;
}

NoDBIntList_t *NoDBIntListAdd(NoDBIntList_t *l, int v)
{
    NoDBIntList_t *node = NoDBMalloc(sizeof(NoDBIntList_t));
    node->v = v;
    node->next = l;
    return node;
}

NoDBIntList_t *NoDBIntListArenaAdd(NoDBIntList_t *l, NoDBArena_t a, int v)
{
    NoDBIntList_t *node = NoDBArenaAlloc(a, sizeof(NoDBIntList_t));
    node->v = v;
    node->next = l;
    return node;
}

NoDBIntList_t *NoDBIntListArenaAddUnique(NoDBIntList_t *l, NoDBArena_t a, int v)
{
    if (NoDBIntListContains(l, v)) {
        return l;
    }
    return NoDBIntListArenaAdd(l, a, v);
}


NoDBIntList_t *NoDBIntListDelete(NoDBIntList_t *l)
{
    NoDBIntList_t *next = l->next;
    NoDBFree(l);
    return next;
}

int NoDBIntListSize(NoDBIntList_t *l)
{
    int n = 0;
    while (l) {
        n++;
        l = l->next;
    }
    return n;
}

NoDBIntList_t *NoDBIntListDuplicate(NoDBIntList_t *l)
{
    NoDBIntList_t *nl;

    nl = NULL;
    while (l) {
        nl = NoDBIntListAdd(nl, l->v);
        l = l->next;
    }
    return nl;
}

NoDBIntList_t *NoDBIntListArenaDuplicate(NoDBIntList_t *l, NoDBArena_t a)
{
    NoDBIntList_t *nl;

    nl = NULL;
    while (l) {
        nl = NoDBIntListArenaAdd(nl, a, l->v);
        l = l->next;
    }
    return nl;
}

int *NoDBIntVectorFromList(NoDBIntList_t *l)
{
    int *v = NoDBMalloc(sizeof(int) * NoDBIntListSize(l));
    int i = 0;
    while (l) {
        v[i++] = l->v;
        l = l->next;
    }
    return v;
}

int *NoDBIntVectorArenaFromList(NoDBIntList_t *l, NoDBArena_t a)
{
    int *v = NoDBArenaAlloc(a, sizeof(int) * NoDBIntListSize(l));
    int i = 0;
    while (l) {
        v[i++] = l->v;
        l = l->next;
    }
    return v;
}

