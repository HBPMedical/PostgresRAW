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


#include "noDB/auxiliary/NoDBBitmap.h"

#include "noDB/auxiliary/NoDBMalloc.h"

NoDBBitmap_t *NoDBBitmapInit(int size)
{
    NoDBBitmap_t *bitmap = NoDBMalloc(sizeof(NoDBBitmap_t));
    bitmap->bits = NoDBMalloc(sizeof(int) * size);
    bitmap->size = size;
    NoDBBitmapClearAll(bitmap);
    return bitmap;
}

NoDBBitmap_t *NoDBBitmapArenaInit(NoDBArena_t a, int size)
{
    NoDBBitmap_t *bitmap = NoDBArenaAlloc(a, sizeof(NoDBBitmap_t));
    bitmap->bits = NoDBArenaAlloc(a, sizeof(int) * size);
    bitmap->size = size;
    NoDBBitmapClearAll(bitmap);
    return bitmap;
}

void NoDBBitmapDestroy(NoDBBitmap_t* bitmap)
{
    NoDBFree(bitmap->bits);
    NoDBFree(bitmap);
}

void NoDBBitmapSet(NoDBBitmap_t* bitmap, int p)
{
    bitmap->bits[p] = 1;
}

void NoDBBitmapClear(NoDBBitmap_t* bitmap, int p)
{
    bitmap->bits[p] = 0;
}

int NoDBBitmapIsSet(NoDBBitmap_t* bitmap, int p)
{
    return bitmap->bits[p] == 1;
}

int NoDBBitmapIsClear(NoDBBitmap_t* bitmap, int p)
{
    return bitmap->bits[p] == 0;
}

void NoDBBitmapSetAll(NoDBBitmap_t* bitmap)
{
    int i;
    for (i = 0; i < bitmap->size; i++) {
        bitmap->bits[i] = 1;
    }
}

void NoDBBitmapClearAll(NoDBBitmap_t* bitmap)
{
    int i;
    for (i = 0; i < bitmap->size; i++) {
        bitmap->bits[i] = 0;
    }
}

int NoDBBitmapIsAllSet(NoDBBitmap_t* bitmap)
{
    int i;
    for (i = 0; i < bitmap->size; i++) {
        if (!bitmap->bits[i]) {
            return 0;
        }
    }
    return 1;
}

int NoDBBitmapIsAllClear(NoDBBitmap_t* bitmap)
{
    int i;
    for (i = 0; i < bitmap->size; i++) {
        if (bitmap->bits[i]) {
            return 0;
        }
    }
    return 1;
}

void NoDBBitmapOR(NoDBBitmap_t *orig, NoDBBitmap_t *toMerge)
{
    int i;
    for (i = 0; i < orig->size; i++) {
        if (NoDBBitmapIsSet(toMerge, i)) {
            NoDBBitmapSet(orig, i);
        }
    }
}
