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



#include "noDB/auxiliary/NoDBMap.h"

#include "noDB/auxiliary/NoDBMalloc.h"

NoDBMap_t *NoDBMapInit()
{
    NoDBMap_t *map = NoDBMalloc(sizeof(NoDBMap_t));
    map->head = NULL;
    map->len = 0;
    return map;
}

NoDBMap_t *NoDBMapArenaInit(NoDBArena_t a)
{
    NoDBMap_t *map = NoDBArenaAlloc(a, sizeof(NoDBMap_t));
    map->head = NULL;
    map->len = 0;
    return map;
}

void NoDBMapDestroy(NoDBMap_t *map)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        NoDBMapList_t *next = node->next;
        NoDBFree(node);
        node = next;
    }
    NoDBFree(map);
}

int NoDBMapHasPtr(NoDBMap_t *map, void *key)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        if (node->key.data.ptrData == key) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int NoDBMapSize(NoDBMap_t *map)
{
    return map->len;
}

int NoDBMapAddPtrInt(NoDBMap_t *map, void *key, int value)
{
    NoDBMapList_t *node;
    if (!map->head) {
        map->head = NoDBMalloc(sizeof(NoDBMapList_t));
        map->head->key.data.ptrData = key;
        map->head->value.data.intData = value;
        map->head->next = NULL;
        map->len = 1;
        return 1;
    }
    node = map->head;
    while (1) {
        if (node->key.data.ptrData == key) {
            return 0;
        }
        if (!node->next)
            break;
        node = node->next;
    }
    node->next = NoDBMalloc(sizeof(NoDBMapList_t));
    node->next->key.data.ptrData = key;
    node->next->value.data.intData = value;
    node->next->next = NULL;
    map->len++;
    return 1;
}

int NoDBMapArenaAddPtrInt(NoDBMap_t *map, NoDBArena_t a, void *key, int value)
{
    NoDBMapList_t *node;
    if (!map->head) {
        map->head = NoDBArenaAlloc(a, sizeof(NoDBMapList_t));
        map->head->key.data.ptrData = key;
        map->head->value.data.intData = value;
        map->head->next = NULL;
        map->len = 1;
        return 1;
    }
    node = map->head;
    while (1) {
        if (node->key.data.ptrData == key) {
            return 0;
        }
        if (!node->next)
            break;
        node = node->next;
    }
    node->next = NoDBArenaAlloc(a, sizeof(NoDBMapList_t));
    node->next->key.data.ptrData = key;
    node->next->value.data.intData = value;
    node->next->next = NULL;
    map->len++;
    return 1;
}

int NoDBMapGetPtrInt(NoDBMap_t *map, void *key, int *value)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        if (node->key.data.ptrData == key) {
            *value = node->value.data.intData;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int NoDBMapSetPtrInt(NoDBMap_t *map, void *key, int value)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        if (node->key.data.ptrData == key) {
            node->value.data.intData = value;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int NoDBMapAddPtrPtr(NoDBMap_t *map, void *key, void *value)
{
    NoDBMapList_t *node;
    if (!map->head) {
        map->head = NoDBMalloc(sizeof(NoDBMapList_t));
        map->head->key.data.ptrData = key;
        map->head->value.data.ptrData = value;
        map->head->next = NULL;
        map->len = 1;
        return 1;
    }
    node = map->head;
    while (1) {
        if (node->key.data.ptrData == key) {
            return 0;
        }
        if (!node->next)
            break;
        node = node->next;
    }
    node->next = NoDBMalloc(sizeof(NoDBMapList_t));
    node->next->key.data.ptrData = key;
    node->next->value.data.ptrData = value;
    node->next->next = NULL;
    map->len++;
    return 1;
}

int NoDBMapArenaAddPtrPtr(NoDBMap_t *map, NoDBArena_t a, void *key, void *value)
{
    NoDBMapList_t *node;
    if (!map->head) {
        map->head = NoDBArenaAlloc(a, sizeof(NoDBMapList_t));
        map->head->key.data.ptrData = key;
        map->head->value.data.ptrData = value;
        map->head->next = NULL;
        map->len = 1;
        return 1;
    }
    node = map->head;
    while (1) {
        if (node->key.data.ptrData == key) {
            return 0;
        }
        if (!node->next)
            break;
        node = node->next;
    }
    node->next = NoDBArenaAlloc(a, sizeof(NoDBMapList_t));
    node->next->key.data.ptrData = key;
    node->next->value.data.ptrData = value;
    node->next->next = NULL;
    map->len++;
    return 1;
}

int NoDBMapGetPtrPtr(NoDBMap_t *map, void *key, void **value)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        if (node->key.data.ptrData == key) {
            *value = node->value.data.ptrData;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

int NoDBMapSetPtrPtr(NoDBMap_t *map, void *key, void *value)
{
    NoDBMapList_t *node = map->head;
    while (node) {
        if (node->key.data.ptrData == key) {
            node->value.data.ptrData = value;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

NoDBMapIterator_t *NoDBMapIterator(NoDBMap_t *map)
{
    return map->head;
}

NoDBMapIterator_t *NoDBMapIteratorNext(NoDBMap_t *map, NoDBMapIterator_t *iterator)
{
    (void) map; // Silence compiler warnings over map being unused.
    return iterator->next;
}

void *NoDBMapIteratorKeyPtr(NoDBMap_t *map, NoDBMapIterator_t *iterator)
{
    (void) map; // Silence compiler warnings over map being unused.
    return iterator->key.data.ptrData;
}

int NoDBMapIteratorValueInt(NoDBMap_t *map, NoDBMapIterator_t *iterator)
{
    (void) map; // Silence compiler warnings over map being unused.
    return iterator->value.data.intData;
}

void *NoDBMapIteratorValuePtr(NoDBMap_t *map, NoDBMapIterator_t *iterator)
{
    (void) map; // Silence compiler warnings over map being unused.
    return iterator->value.data.ptrData;
}


