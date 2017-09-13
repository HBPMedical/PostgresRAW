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

#ifndef METADATA_H
#define METADATA_H

#include <stdio.h>
#include <math.h>

#include "snooping/global.h"
#include "snooping/bitmap.h"
#include "snooping/positionalIndex.h"


/* Pointer for the end of each tuple */
typedef unsigned short int MetaPointer; //[0 - 65535]

/*typedef struct PointersDescriptor
{
	MetaPointer **position;     //Initialize for: INIT_POINTERS * VECTOR_SIZE
	int numberOfPointers;       //Number of active vectors
	int currentVector;          //Current active vector
	int currentVectorPos;       //Current position in active vector
	long currentFilePointer;

} PointersDescriptor;
*/

typedef struct EndOfTupleDescriptor
{
	MetaPointer *position; //Initialize for: VECTOR_SIZE
	int currentSize;
} EndOfTupleDescriptor;


/* Metapointers in the end of a file (pointer in the end of every tuple) */
typedef struct TupleFilePointers
{
	EndOfTupleDescriptor tuplePointer;
	long numberOfTuples;

	FILE *fp;
	bool done;

	char filename[MAX_FILENAME];
	char relation[MAX_RELATION_NAME];
} TupleFilePointers;


extern TupleFilePointers FD[NUMBER_OF_RELATIONS];


/*Functions for EOL pointers*/
void initializeMetadataModule(void);
int getTupleFilePointers(char *relation);

int getFreeTupleFilePointers(void);
int initializeTupleFilePointers(char *relation, FILE *fp);

void addMetaPointer(long newPosition, int pos);
void finalizeMetaPointer(int pos);
bool isFinalizedMetaPointer(int pos);

int computeBytesToRead(long currentTuple, long availableBytes, int pos, int *tuplesRead);
int getEndOfTuple(long currentTuple, int pos);
long  getNumberOfTuples(int pos);
void printMetadata(void);

#endif   /* METADATA_H */



