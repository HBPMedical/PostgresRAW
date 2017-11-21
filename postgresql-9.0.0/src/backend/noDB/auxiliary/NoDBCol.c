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


#include "noDB/auxiliary/NoDBCol.h"

#include "noDB/auxiliary/NoDBMalloc.h"

#include <stdlib.h>

NoDBColVector_t NoDBColVectorInit(int size)
{
    NoDBColVector_t v;
    v.size = size;
    if (size) {
        v.cols = NoDBMalloc(sizeof(NoDBCol_t) * size);
    } else {
        v.cols = NULL;
    }
    return v;
}

NoDBColVector_t NoDBColVectorArenaInit(NoDBArena_t a, int size)
{
    NoDBColVector_t v;
    v.size = size;
    if (size) {
        v.cols = NoDBArenaAlloc(a, sizeof(NoDBCol_t) * size);
    } else {
        v.cols = NULL;
    }
    return v;
}

void NoDBColVectorDestroy(NoDBColVector_t v)
{
    NoDBFree(v.cols);
}

int NoDBColVectorIndex(NoDBColVector_t v, NoDBCol_t col)
{
    int i;
    for (i = 0; i < v.size; i++) {
        if (v.cols[i] == col) {
            return i;
        }
    }
    return -1;
}

// Assumes vector contains sorted values.
int NoDBColVectorBinarySearch(NoDBColVector_t v, NoDBCol_t col)
{
    int min;
    int max;

    min = 0;
    max = v.size;

    while (max >= min) {
        int i = min + ((max - min) / 2);

        if (v.cols[i] < col) {
            min = i + 1;
        } else if (v.cols[i] > col) {
            max = i - 1;
        } else {
            return i;
        }
    }
    return -1;
}

NoDBCol_t NoDBColVectorGet(NoDBColVector_t v, int i)
{
    return v.cols[i];
}

void NoDBColVectorSet(NoDBColVector_t v, int i, NoDBCol_t col)
{
    v.cols[i] = col;
}

int NoDBColVectorSize(NoDBColVector_t v)
{
    return v.size;
}

int NoDBColVectorContains(NoDBColVector_t v, NoDBCol_t col)
{
    int i;
    for (i = 0; i < v.size; i++) {
        if (v.cols[i] == col) {
            return 1;
        }
    }
    return 0;
}

void NoDBColVectorApply(NoDBColVector_t v, void (*fn) (NoDBCol_t, void*), void *arg)
{
    int i;
    for (i = 0; i < v.size; i++) {
        (*fn) (v.cols[i], arg);
    }
}

static int comp(const void *a, const void *b)
{
    return ( *((NoDBCol_t *) a) - *((NoDBCol_t *)b) );
}

void NoDBColVectorSort(NoDBColVector_t v)
{
    NoDBQuickSort(v.cols, v.size, sizeof(NoDBCol_t), comp);
}

NoDBColVector_t NoDBColVectorFromList(NoDBColList_t *l)
{
    NoDBColVector_t v = NoDBColVectorInit(NoDBColListSize(l));
    int i = 0;
    while (l) {
        v.cols[i++] = l->col;
        l = l->next;
    }
    return v;
}

NoDBColVector_t NoDBColVectorArenaFromList(NoDBColList_t *l, NoDBArena_t a)
{
    NoDBColVector_t v = NoDBColVectorArenaInit(a, NoDBColListSize(l));
    int i = 0;
    while (l) {
        v.cols[i++] = l->col;
        l = l->next;
    }
    return v;
}

void NoDBColListDestroy(NoDBColList_t *l)
{
    while (l) {
        NoDBColList_t *next = l->next;
        NoDBFree(l);
        l = next;
    }
}

int NoDBColListContains(NoDBColList_t *l, NoDBCol_t col)
{
    while (l) {
        if (l->col == col) {
            return 1;
        }
        l = l->next;
    }
    return 0;
}

int NoDBColListContainsColList(NoDBColList_t *l, NoDBColList_t *subset)
{
    for (; subset; subset = subset->next) {
        if (!NoDBColListContains(l, subset->col)) {
            return 0;
        }
    }
    return 1;
}

NoDBColList_t *NoDBColListAdd(NoDBColList_t *l, NoDBCol_t col)
{
    NoDBColList_t *node = NoDBMalloc(sizeof(NoDBColList_t));
    node->col = col;
    node->next = l;
    return node;
}

NoDBColList_t *NoDBColListArenaAdd(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col)
{
    NoDBColList_t *node = NoDBArenaAlloc(a, sizeof(NoDBColList_t));
    node->col = col;
    node->next = l;
    return node;
}

NoDBColList_t *NoDBColListAddUnique(NoDBColList_t *l, NoDBCol_t col)
{
    if (NoDBColListContains(l, col)) {
        return l;
    }
    return NoDBColListAdd(l, col);
}

NoDBColList_t *NoDBColListArenaAddUnique(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col)
{
    if (NoDBColListContains(l, col)) {
        return l;
    }
    return NoDBColListArenaAdd(l, a, col);
}

NoDBColList_t *NoDBColListArenaAddSortUnique(NoDBColList_t *l, NoDBArena_t a, NoDBCol_t col)
{
    NoDBColList_t   *node;
    NoDBColList_t   *prev;
    NoDBColList_t   *cur;

    if (!l || (l && l->col > col)) {
        node = NoDBArenaAlloc(a, sizeof(NoDBColList_t));
        node->col = col;
        node->next = l;
        return node;
    }
    if (l->col == col) {
        return l;
    }

    prev = l;
    cur = l->next;
    while (cur) {
        if (cur->col == col) {
            return l;
        } else if (cur->col > col) {
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    node = NoDBArenaAlloc(a, sizeof(NoDBColList_t));
    node->col = col;
    node->next = cur;
    prev->next = node;
    return l;
}

NoDBColList_t *NoDBColListArenaDelete(NoDBColList_t *l, NoDBCol_t col)
{
    NoDBColList_t   *prev;
    NoDBColList_t   *cur;

    if (!l) {
        return l;
    }

    if (l->col == col) {
        return l->next;
    }

    prev = l;
    cur = l->next;
    for (; cur; cur = cur->next) {
        if (cur->col == col) {
            prev->next = cur->next;
            return l;
        }
        prev = cur;
    }
    return l;
}

int NoDBColListSize(NoDBColList_t *l)
{
    int n = 0;
    while (l) {
        n++;
        l = l->next;
    }
    return n;
}

NoDBColList_t *NoDBColListDuplicate(NoDBColList_t *l)
{
    NoDBColList_t *nl;

    nl = NULL;
    while (l) {
        nl = NoDBColListAdd(nl, l->col);
        l = l->next;
    }
    return nl;
}

NoDBColList_t *NoDBColListArenaDuplicate(NoDBColList_t *l, NoDBArena_t a)
{
    NoDBColList_t *nl;

    nl = NULL;
    while (l) {
        nl = NoDBColListArenaAdd(nl, a, l->col);
        l = l->next;
    }
    return nl;
}
