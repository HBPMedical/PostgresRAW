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


/*-------------------------------------------------------------------------
 *
 * metadata.c
 *		metadata information
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/snooping/metadata.c $
 * Author: juan
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "snooping/metadata.h"
#include "snooping/common.h"
//#include "snooping/coordinator.h"


//Metadata pointers in the input file (pointer in the end of file)
TupleFilePointers FD[NUMBER_OF_RELATIONS];
int usedFiles = 0;


/*
 * Initialize Metadata Module (UP to NUMBER_OF_RELATIONS input files can be processed concurrently)
 */
void
initializeMetadataModule(void)
{
	int i;
	for (i = 0; i < NUMBER_OF_RELATIONS; i++)
	{
		FD[i].done =  false;
		strcpy(FD[i].filename, "\0");
	}
}

/*
 * Get position in FilePointer for a specific file
 */
int
getTupleFilePointers(char *relation)
{
	int i = 0;
	for (i = 0; i < usedFiles; i++)
	{
		if(strcmp(FD[i].relation, relation) == 0)
			return i;
	}
	return -1;
}

int
getFreeTupleFilePointers(void)
{
	if( usedFiles == NUMBER_OF_RELATIONS){
		write_stderr(_("Error in getTupleFilePointers() file metadata.c: all pointers are used...\n"));
	}
	Assert( usedFiles < NUMBER_OF_RELATIONS);
	return usedFiles;
}

int
initializeTupleFilePointers(char *relation, FILE *fp)
{
	int pos;
	Assert(relation != NULL);

	pos = getTupleFilePointers(relation);
	if ( pos == -1)
		pos = getFreeTupleFilePointers();
	else
	{
		if(FD[pos].done == true)
			return pos;
	}

	/* Initialize metadata struct for filename */
	strcpy(FD[pos].filename, getInputFilename(relation));
	strcpy(FD[pos].relation, relation);

	/* File is closed, then open it... */
	if(fp == NULL)
	{
		if ((FD[pos].fp = fopen(FD[pos].filename, "r")) == NULL){
			fprintf(stderr, "File %s not found... (metadata.c)",FD[pos].filename);
		}
	}
	else
		FD[pos].fp = fp;

	/* Initialize Array of pointers (default init size: VECTOR_SIZE)*/
	FD[pos].tuplePointer.position = (MetaPointer *) malloc(VECTOR_SIZE * sizeof(MetaPointer));
	if (!FD[pos].tuplePointer.position)
	{
		write_stderr(_("Error in initializeFilePointers() file metadata.c\n"));
		exit(1);
	}

	FD[pos].tuplePointer.currentSize = VECTOR_SIZE;
	FD[pos].numberOfTuples = 0;
	FD[pos].done = false;

	/* Increase number of files for which we keep pointers */
	usedFiles++;
	return pos;
}


//void
//addMetaPointer(long newPosition, int pos)
//{
//	int x;
//	int y;
//	if(FD[pos].done)
//		return;
//
//	/*allocate memory for another Vector*/
//	if(FD[pos].tuplePointer.currentVectorPos == VECTOR_SIZE)//End of vector
//	{
//		FD[pos].tuplePointer.currentVector++;
//		if(FD[pos].tuplePointer.currentVector == FD[pos].tuplePointer.numberOfPointers)//End of array
//		{
//			FD[pos].tuplePointer.position = (MetaPointer **) realloc(FD[pos].tuplePointer.position, ( FD[pos].tuplePointer.numberOfPointers + INIT_POINTERS) * sizeof(MetaPointer *) );
//			FD[pos].tuplePointer.numberOfPointers += INIT_POINTERS;
//		}
//
//		FD[pos].tuplePointer.position[FD[pos].tuplePointer.currentVector] = (MetaPointer *) malloc(VECTOR_SIZE * sizeof(MetaPointer));
//		FD[pos].tuplePointer.currentVectorPos = 0;
//	}
////	FD[pos].pointers[FD[pos].currentVector][FD[pos].currentVectorPos] = (MetaPointer)(newPosition - FD[pos].currentFilePointer);//keep offset from previous '\n'
//	x = FD[pos].tuplePointer.currentVector;
//	y = FD[pos].tuplePointer.currentVectorPos;
//	FD[pos].tuplePointer.position[x][y] = (MetaPointer)(newPosition);//keep offset from previous '\n'
//
//	FD[pos].tuplePointer.currentFilePointer += newPosition;
//	FD[pos].tuplePointer.currentVectorPos++;
//	FD[pos].numberOfTuples++;
//}

/* Add an EOL metapointer*/
void
addMetaPointer(long newPosition, int pos)
{
//	int x;
//	int y;
//	if(FD[pos].done)
//		return;

	if ( FD[pos].numberOfTuples >=  FD[pos].tuplePointer.currentSize)
	{
		FD[pos].tuplePointer.currentSize += VECTOR_SIZE;
		FD[pos].tuplePointer.position = (MetaPointer*) realloc(FD[pos].tuplePointer.position, ( FD[pos].tuplePointer.currentSize * sizeof(MetaPointer *) ));
	}
	FD[pos].tuplePointer.position[FD[pos].numberOfTuples] = (MetaPointer)(newPosition);
	FD[pos].numberOfTuples++;
}


void
finalizeMetaPointer(int pos)
{
	FD[pos].done = true;
}

bool
isFinalizedMetaPointer(int pos)
{
	return FD[pos].done;
}

/* Compute bytes to be read in the next block (used after the EOL pointers have been collected) */
int
computeBytesToRead(long currentTuple, long availableBytes, int pos, int *tuplesRead)
{
	int result = 0 ;
	int i;
	*tuplesRead = 0;

	for( i = currentTuple; i < FD[pos].numberOfTuples; i++)
	{
			result += FD[pos].tuplePointer.position[i];
			(*tuplesRead)++;
			if(result > availableBytes) {
				result -= FD[pos].tuplePointer.position[i];
				(*tuplesRead)--;
				break;
			}
	}
	return result;
}


int
getEndOfTuple(long currentTuple, int pos)
{
	return FD[pos].tuplePointer.position[currentTuple];
}

long
getNumberOfTuples(int pos)
{
	return FD[pos].numberOfTuples;
}

/* For DEBUG */
void
printMetadata(void)
{
	int i=0, z = 0;
	fprintf(stderr,"\nUsedFiles = %d\n",usedFiles);
	for( i = 0; i < usedFiles; i++)
	{
		fprintf(stderr,"*****************************************\n");
		fprintf(stderr,"File = %s\n",FD[i].filename);
		fprintf(stderr,"Relation = %s\n",FD[i].relation);
		fprintf(stderr,"Records = %ld\n",FD[i].numberOfTuples);
		fprintf(stderr,"Vector {%d}:",z);
		for( z = 0; z < FD[i].numberOfTuples; z++)
		{
				fprintf(stderr,"%d ",FD[i].tuplePointer.position[z]);
		}
		fprintf(stderr,"\n");
		fprintf(stderr,"*****************************************\n\n");
	}

}


