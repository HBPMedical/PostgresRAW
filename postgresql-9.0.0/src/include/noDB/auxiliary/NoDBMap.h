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


#ifndef NODB_MAP_H
#define NODB_MAP_H

#include "noDB/auxiliary/NoDBArena.h"

typedef struct NoDBMapData_t {
    union {
        void                *ptrData;
        int                 intData;
    } data;
} NoDBMapData_t;

typedef struct NoDBMapList_t {
    NoDBMapData_t           key;
    NoDBMapData_t           value;
    struct NoDBMapList_t    *next;
} NoDBMapList_t;

typedef struct NoDBMap_t {
    NoDBMapList_t           *head;
    int                     len;
} NoDBMap_t;

NoDBMap_t               *NoDBMapInit();
NoDBMap_t               *NoDBMapArenaInit(NoDBArena_t a);
void                    NoDBMapDestroy(NoDBMap_t *map);
int                     NoDBMapHasPtr(NoDBMap_t *map, void *key);
int                     NoDBMapSize(NoDBMap_t *map);
int                     NoDBMapAddPtrInt(NoDBMap_t *map, void *key, int value);
int                     NoDBMapArenaAddPtrInt(NoDBMap_t *map, NoDBArena_t a, void *key, int value);
int                     NoDBMapGetPtrInt(NoDBMap_t *map, void *key, int *value);
int                     NoDBMapSetPtrInt(NoDBMap_t *map, void *key, int value);
int                     NoDBMapAddPtrPtr(NoDBMap_t *map, void *key, void *value);
int                     NoDBMapArenaAddPtrPtr(NoDBMap_t *map, NoDBArena_t a, void *key, void *value);
int                     NoDBMapGetPtrPtr(NoDBMap_t *map, void *key, void **value);
int                     NoDBMapSetPtrPtr(NoDBMap_t *map, void *key, void *value);

typedef NoDBMapList_t   NoDBMapIterator_t;

NoDBMapIterator_t       *NoDBMapIterator(NoDBMap_t *map);
NoDBMapIterator_t       *NoDBMapIteratorNext(NoDBMap_t *map, NoDBMapIterator_t *iterator);
void                    *NoDBMapIteratorKeyPtr(NoDBMap_t *map, NoDBMapIterator_t *iterator);
int                     NoDBMapIteratorValueInt(NoDBMap_t *map, NoDBMapIterator_t *iterator);
void                    *NoDBMapIteratorValuePtr(NoDBMap_t *map, NoDBMapIterator_t *iterator);

#endif /* NODB_MAP_H */
