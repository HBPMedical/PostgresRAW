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

#ifndef D_ARRAY_H_
#define D_ARRAY_H_

#include "postgres.h"


#define INIT_SIZE   256
#define ALLOC_UNIT  32

typedef struct d_array{
	void *data;
	int currentSize;
	int usedObjects;
	int objectSize;
} D_Array;


void* GetObjectPos(D_Array *array, int pos);

void InitializeArray(D_Array *array, int size, int item_size);
void InsertToArray(D_Array *array, void* object);
void InsertToArrayAt(D_Array *array, void* object, int pos);
void RemoveObject(D_Array *array, int pos, void (*freeObject)(void *));
void ReturnObject(D_Array *array, int pos, void* object);
void FreeArray(D_Array *array, void (*freeObject)(void *));
bool SearchArray(D_Array *array, void* object, bool (*compare)(void *, void *));
void PrintArray(D_Array *array, void (*printObject)(void *));




#endif /* D_ARRAY_H_ */
