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


#ifndef NODB_BITMAP_H
#define NODB_BITMAP_H

#include "noDB/auxiliary/NoDBArena.h"

typedef struct NoDBBitmap_t
{
    int *bits;
    int size;
} NoDBBitmap_t;

NoDBBitmap_t    *NoDBBitmapInit(int size);
NoDBBitmap_t    *NoDBBitmapArenaInit(NoDBArena_t a, int size);
void            NoDBBitmapDestroy(NoDBBitmap_t* bitmap);

void            NoDBBitmapSet(NoDBBitmap_t* bitmap, int p);
void            NoDBBitmapClear(NoDBBitmap_t* bitmap, int p);

int             NoDBBitmapIsSet(NoDBBitmap_t* bitmap, int p);
int             NoDBBitmapIsClear(NoDBBitmap_t* bitmap, int p);

void            NoDBBitmapSetAll(NoDBBitmap_t* bitmap);
void            NoDBBitmapClearAll(NoDBBitmap_t* bitmap);

int             NoDBBitmapIsAllSet(NoDBBitmap_t* bitmap);
int             NoDBBitmapIsAllClear(NoDBBitmap_t* bitmap);

void            NoDBBitmapOR(NoDBBitmap_t *orig, NoDBBitmap_t *toMerge);

#endif  /* NODB_BITMAP_H */
