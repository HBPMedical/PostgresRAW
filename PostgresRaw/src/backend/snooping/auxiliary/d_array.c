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



#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "snooping/d_array.h"


//static void* GetObjectPos(D_Array *array, int pos);
static void IncreaseArraySize(D_Array *array, int additionalObjects);


/*Get pointer to object in position pos*/
//static void* GetObjectPos(D_Array *array, int pos)
void* GetObjectPos(D_Array *array, int pos)
{
	return ((char*)array->data + (array->objectSize * pos));
}

/*Allocate space for <additionalObjects> or ALLOC_UNIT new objects */
static void IncreaseArraySize(D_Array *array, int additionalObjects)
{
	assert(additionalObjects >= 0);
	array->currentSize += additionalObjects == 0 ? ALLOC_UNIT : additionalObjects;

	array->data = (void*) realloc(array->data, array->currentSize * array->objectSize);
	assert(array->data != NULL);
}

/*Initialize array with <size> objects of size <object_size>*/
void InitializeArray(D_Array *array, int size, int object_size)
{
	assert(size > 0);
	assert(object_size > 0);

	array->currentSize = size;
	array->objectSize = object_size;
	array->usedObjects = 0;
	array->data = (void*) malloc ( array->objectSize * size );
	assert(array->data != NULL);
}

/*Free array. Use <freeObject> if it is available*/
void FreeArray(D_Array *array, void (*freeObject)(void *))
{
	int i = 0;
	if( freeObject != NULL)
	{
		for ( i = 0; i < array->usedObjects; i++)
			freeObject(GetObjectPos(array, i));
	}
	array->usedObjects = 0;
	free(array->data);
}

/*Insert in the end of the array*/
void InsertToArray(D_Array *array, void* object)
{
	assert( array->usedObjects >= 0 );
	//Array is full so allocate space for more elements
	if( array->currentSize == array->usedObjects )
		IncreaseArraySize(array, 0);

	memcpy((void*)GetObjectPos(array, array->usedObjects), (void*)object, array->objectSize);
	array->usedObjects++;
}

/* Insert at a specific position of the array and shift the other elements*/
void InsertToArrayAt(D_Array *array, void* object, int pos)
{
	void *which;
	if ( pos < 0 && pos <= array->currentSize  )
		return;

	if( array->currentSize == array->usedObjects )
		IncreaseArraySize(array, 0);

	which = GetObjectPos(array, pos);
	memmove(GetObjectPos(array, pos + 1), which, array->objectSize * (array->usedObjects - pos));

	memcpy((void*)which, (void*)object, array->objectSize);
	array->usedObjects++;
}


void ReturnObject(D_Array *array, int pos, void* object)
{
	assert ( pos >=0 );
	if ( pos >= array->usedObjects ) {
		object = NULL;
		return;
	}
	else
		memcpy(object, GetObjectPos(array, pos), array->objectSize);
}


void RemoveObject(D_Array *array, int pos, void (*freeObject)(void *))
{
	void* which;
	assert( pos >= 0 && pos < array->usedObjects);

	which = GetObjectPos(array, pos);
	if(freeObject != NULL)
		freeObject(which);
	memmove(which, GetObjectPos(array, pos + 1), array->objectSize * (array->usedObjects - pos - 1));
	array->usedObjects--;
}

bool SearchArray(D_Array *array, void* object, bool (*compare)(void *, void *))
{
	int i = 0;
	if( compare == NULL)
		return false;

	for ( i = 0; i < array->usedObjects; i++)
		if(compare(GetObjectPos(array, i), object))
			return true;
	return false;
}



void PrintArray(D_Array *array, void (*printObject)(void *))
{
	int i = 0;
	if( printObject == NULL)
		return;
//	fprintf(stderr,"\n------------------------------\n");
//	fprintf(stderr,"Current Array Size = %d\n",(*array).currentSize);
//	fprintf(stderr,"# of Used Objects = %d\n",(*array).usedObjects);
//	fprintf(stderr,"Object Size = %d\n",(*array).objectSize);
	if( printObject != NULL) {
		for ( i = 0; i < array->usedObjects; i++)
			printObject(GetObjectPos(array, i));
	}
}


