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



#include "noDB/auxiliary/NoDBRow.h"

#include "noDB/auxiliary/NoDBMalloc.h"
#include "noDB/auxiliary/NoDBSort.h"

#include <stdlib.h>



NoDBRowVector_t NoDBRowVectorInit(int size)
{
    NoDBRowVector_t v;
    v.size = size;
    if (size) {
        v.rows = NoDBMalloc(sizeof(NoDBRow_t) * size);
    } else {
        v.rows = NULL;
    }
    return v;
}

NoDBRowVector_t NoDBRowVectorArenaInit(NoDBArena_t a, int size)
{
    NoDBRowVector_t v;
    v.size = size;
    if (size) {
        v.rows = NoDBArenaAlloc(a, sizeof(NoDBRow_t) * size);
    } else {
        v.rows = NULL;
    }
    return v;
}

void NoDBRowVectorDestroy(NoDBRowVector_t v)
{
    NoDBFree(v.rows);
}

NoDBRow_t NoDBRowVectorGet(NoDBRowVector_t v, int i)
{
    return v.rows[i];
}

void NoDBRowVectorSet(NoDBRowVector_t v, int i, NoDBRow_t row)
{
    v.rows[i] = row;
}

int NoDBRowVectorSize(NoDBRowVector_t v)
{
    return v.size;
}

void NoDBRowVectorApply(NoDBRowVector_t v, void (*fn) (NoDBRow_t, void*), void *arg)
{
    int i;
    for (i = 0; i < v.size; i++) {
        (*fn) (v.rows[i], arg);
    }
}

static int comp(const void *a, const void *b)
{
    return ( *((NoDBRow_t *) a) - *((NoDBRow_t *)b) );
}

void NoDBRowVectorSort(NoDBRowVector_t v)
{
    NoDBQuickSort(v.rows, v.size, sizeof(NoDBRow_t), comp);
}

NoDBRowVector_t NoDBRowVectorFromList(NoDBRowList_t *l)
{
    NoDBRowVector_t v = NoDBRowVectorInit(NoDBRowListSize(l));
    int i = 0;
    while (l) {
        v.rows[i++] = l->row;
        l = l->next;
    }
    return v;
}

NoDBRowVector_t NoDBRowVectorArenaFromList(NoDBRowList_t *l, NoDBArena_t a)
{
    NoDBRowVector_t v = NoDBRowVectorArenaInit(a, NoDBRowListSize(l));
    int i = 0;
    while (l) {
        v.rows[i++] = l->row;
        l = l->next;
    }
    return v;
}

void NoDBRowListDestroy(NoDBRowList_t *l)
{
    while (l) {
        NoDBRowList_t *next = l->next;
        NoDBFree(l);
        l = next;
    }
}

NoDBRowList_t *NoDBRowListAddUnique(NoDBRowList_t *l, NoDBRow_t row)
{
    NoDBRowList_t *node = l;
    while (node) {
        if (node->row == row) {
            return l;
        }
        node = node->next;
    }
    node = NoDBMalloc(sizeof(NoDBRowList_t));
    node->row = row;
    node->next = l;
    return node;
}

NoDBRowList_t *NoDBRowListArenaAddUnique(NoDBRowList_t *l, NoDBArena_t a, NoDBRow_t row)
{
    NoDBRowList_t *node = l;
    while (node) {
        if (node->row == row) {
            return l;
        }
        node = node->next;
    }
    node = NoDBArenaAlloc(a, sizeof(NoDBRowList_t));
    node->row = row;
    node->next = l;
    return node;
}

NoDBRowList_t *NoDBRowListArenaAddSortUnique(NoDBRowList_t *l, NoDBArena_t a, NoDBRow_t row)
{
    NoDBRowList_t   *node;
    NoDBRowList_t   *prev;
    NoDBRowList_t   *cur;

    if (!l || (l && l->row > row)) {
        node = NoDBArenaAlloc(a, sizeof(NoDBRowList_t));
        node->row = row;
        node->next = l;
        return node;
    }
    if (l->row == row) {
        return l;
    }

    prev = l;
    cur = l->next;
    while (cur) {
        if (cur->row == row) {
            return l;
        } else if (cur->row > row) {
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    node = NoDBArenaAlloc(a, sizeof(NoDBRowList_t));
    node->row = row;
    node->next = cur;
    prev->next = node;
    return l;
}

int NoDBRowListSize(NoDBRowList_t *l)
{
    int n = 0;
    while (l) {
        n++;
        l = l->next;
    }
    return n;
}
