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

#include "noDB/auxiliary/NoDBArena.h"

#include "noDB/auxiliary/NoDBMalloc.h"

typedef struct ArenaBlock_t {
    char                *limit;
    char                *avail;
    struct ArenaBlock_t *next;
} ArenaBlock_t;

typedef union Align_t {
    long            l;
    char            *p;
    double          d;
    int             (*f) (void);
} Align_t;

typedef union Header_t {
    ArenaBlock_t    b;
    Align_t         a;
} Header_t;

static ArenaBlock_t first[] =   {   {NULL, NULL, NULL},
                                    {NULL, NULL, NULL},
                                    {NULL, NULL, NULL},
                                    {NULL, NULL, NULL} };
static ArenaBlock_t *arena[] =  {   &first[NODB_ARENA_STRATEGY],
                                    &first[NODB_ARENA_ITERATOR],
                                    &first[NODB_ARENA_QUERY],
                                    &first[NODB_ARENA_CACHE] };
int arenaBlockSize[] = {    (1024*1024),
                            (1024*1024),
                            (1024*1024),
                            (16*1024*1024) };

static ArenaBlock_t *freeBlocks = NULL;

static size_t RoundUp(size_t x, size_t n)
{
    return ( x + (n-1)) & (~(n - 1));
}

void *NoDBArenaAlloc(NoDBArena_t a, size_t size)
{
    ArenaBlock_t    *blk;

    blk = arena[a];
    size = RoundUp(size, sizeof(Align_t));
    while ((blk->avail + size) > blk->limit) {
        if ((blk->next = freeBlocks) != NULL) {
            freeBlocks = freeBlocks->next;
            blk = blk->next;
        } else {
            size_t  m;

            m = sizeof(Header_t) + size + arenaBlockSize[a];
            blk->next = NoDBMalloc(m);
            blk = blk->next;
            blk->limit = (char *) blk + m;
        }
        blk->avail = (char *) ((Header_t *) blk + 1);
        blk->next = NULL;
        arena[a] = blk;
    }
    blk->avail += size;
    return blk->avail - size;
}

void NoDBArenaFree(NoDBArena_t a)
{
    arena[a]->next = freeBlocks;
    freeBlocks = first[a].next;
    first[a].next = NULL;
    arena[a] = &first[a];
}

void Destroy(ArenaBlock_t *blk)
{
    while (blk) {
        ArenaBlock_t    *next;

        next = blk->next;
        NoDBFree(blk);
        blk = next;
    }
}

void NoDBArenaDestroy(NoDBArena_t a)
{
    Destroy(arena[a]);
}

void NoDBArenaDestroyAll()
{
    NoDBArenaFree(0);
    Destroy(freeBlocks);
}

