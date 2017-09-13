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


/*
 * positionalIndex.c
 *
 *  Created on: Apr 21, 2011
 *      Author: yannis
 */

#include "postgres.h"

#include "snooping/positionalIndex.h"
#include "snooping/common.h"
#include "snooping/metadata.h"


//
//PositionalMap InternalPositionalMap[NUMBER_OF_RELATIONS];
//int internalMap_usedFiles = 0;
//int num_internal_metapointers = -1;
//
//
///*
// * Initialize Internal Metadata Module (UP to NUMBER_OF_RELATIONS input files can be processed)
// */
//void
//initializeInternalPositionalMapModule(void)
//{
//	int i;
//	for (i = 0; i < NUMBER_OF_RELATIONS; i++)
//	{
//		InternalPositionalMap[i].initialized =  false;
//		InternalPositionalMap[i].isReady =  false;
//		strcpy(InternalPositionalMap[i].filename, "\0");
//	}
//}
//
///*
// * Get position in InternalFilePointers for a specific file
// */
//int
//getInternalPositionalMap(char *relation)
//{
//	int i = 0;
//	for (i = 0; i < internalMap_usedFiles; i++)
//	{
//		if(strcmp(InternalPositionalMap[i].relation, relation) == 0)
//			return i;
//	}
//	return -1;
//}
//
//int
//getFreeInternalPositionalMap(void)
//{
//	if( internalMap_usedFiles == NUMBER_OF_RELATIONS){
//		write_stderr(_("Error in getFreeFilePointers() file positionalIndex.c: all pointers are used...\n"));
//	}
//	Assert( internalMap_usedFiles < NUMBER_OF_RELATIONS);
//	return internalMap_usedFiles;
//}
//
//int
//getPositionalMapID(char *relation)
//{
//	int pos;
//	Assert(relation != NULL);
//	pos = getInternalPositionalMap(relation);
//	if ( pos == -1)
//		pos = getFreeInternalPositionalMap();
//	return pos;
//}
//
//
///*
// * which: List of {1,0} for the needed metapointers for the interesting attributes
// * which length: (numberOfAttributes + 1)
// * For example:
// * If we have this schema |Attr1|Attr2|Attr3|Attr4|...|Attr(N-1)|, the interesting
// * attributes are Attr1 and Atrr2 and the policy is to collect pointers before and after
// * the interesting attribute, then we expect as input {1,1,1,0,0,...,0}
// */
//int
//initializeInternalPositionalMap(int pos, char *relation, FILE *fp, int numberOfAttributes, int *which)
//{
//	int i,j;
//	int how_many;
//	int *newCollectedElements;
//
//
//	/*Budget is limited*/
//	if ( num_internal_metapointers != -1 )
//	{
//		/*Initialize stats*/
//		initializePolicy(&InternalPositionalMap[pos].stats, num_internal_metapointers, numberOfAttributes);
//		newCollectedElements = firstTimeLRU(&InternalPositionalMap[pos].stats, which, numberOfAttributes);
//	}
//	else
//		newCollectedElements = which;
//
//	/*Initialize metadata struct for filename*/
//	strcpy(InternalPositionalMap[pos].filename, getInputFilename(relation));
//	strcpy(InternalPositionalMap[pos].relation, relation);
//
//	/* File is closed, then open it... */
//	if(fp == NULL)
//	{
//		if ((InternalPositionalMap[pos].fp = fopen(InternalPositionalMap[pos].filename, "r")) == NULL){
//			fprintf(stderr, "File %s not found... (metadata.c)",InternalPositionalMap[pos].filename);
//		}
//	}
//	else
//		InternalPositionalMap[pos].fp = fp;
//
//	InternalPositionalMap[pos].numberOfAttributes = numberOfAttributes;
//
//	InternalPositionalMap[pos].available = createBitMap(numberOfAttributes + 1);
//	setBit(InternalPositionalMap[pos].available, 0);
//	setBit(InternalPositionalMap[pos].available, numberOfAttributes);
//	for (i = 1 ; i < numberOfAttributes; i++)
//		setBitValue(InternalPositionalMap[pos].available, i, 0);
////	for (i = 1 ; i < numberOfAttributes; i++)
////		setBitValue(InternalPositionalMap[pos].available, i, which[i]);
////		setBitValue(InternalPositionalMap[pos].available, i, which[i - 1]);
//
////	Map is initially sorted, then it changes according to the order of the attributes we index
////	InternalPositionalMap[pos].activePointers = 0;
//
//	/* *
//	 * The MAP contains the position of the pointer in the data structure
//	 * MAP size is numAttr-1 since we don't care for the first and the last attribute
//	 * The size of which is numAttr+1
//	 * Example: |Pointer0|Pointer1|Pointer2|...|PointerN-1|PointerN|
//	 * Map can contain values for Pointer1,Pointer2,...,PointerN-1
//	 * Pointer0 is known
//	 * PointerN is known from the other structure (EOL pointers)
//	 *
//	 * IDs: Pointer0|Attr0|Pointer1|Attr1|Pointer2|...
//	 * Best case for Attr0 is having Pointer0 and Pointer1
//	 * Best case for Attr1 is having Pointer1 and Pointer2
//	 * */
//	InternalPositionalMap[pos].map = (int*) malloc ((numberOfAttributes - 1) * sizeof(int));
//	for (i = 0 ; i < ( numberOfAttributes - 1 ); i++)
//		InternalPositionalMap[pos].map[i] = -1;
//
//	j = 0;
//	for (i = 1 ; i <= ( numberOfAttributes - 1); i++)
//	{
//		if(newCollectedElements[i])
//		{/* We care for pointers 1...N-1 (The first is always zero - beginning of the tuple and the
//		    last one is specified from the other data structure with the end-of-tuple pointers) */
//			InternalPositionalMap[pos].map[j++] = i;
//			InternalPositionalMap[pos].activePointers++;
//		}
//	}
//
//	/* *
//	 * Inverse MAP is used to identify the positions of the pointers in the data structure.
//	 * For example, MAP=[3,2,5]  --> we have collected pointers with ID: 2,3,5
//	 * Then the inverse_MAP = [-1,-1, 1, 0, -1, 2] aka the fifth pointer is the second position
//	 * of the data structure.
//	 * The inverse map size is NumAttr+1. Be default position 0 and N-1 are -1 since we don't
//	 * include any value for these attributes
//	 * */
//	InternalPositionalMap[pos].inverse_map = (int*) malloc ((numberOfAttributes + 1) * sizeof(int));
//	for (i = 0 ; i < ( numberOfAttributes + 1); i++)
//		InternalPositionalMap[pos].inverse_map[i] = -1;
//
//	for (i = 0 ; i < ( numberOfAttributes - 1); i++)
//	{
//		if(InternalPositionalMap[pos].map[i] != -1) {//We care for pointers 1...N-1
//			InternalPositionalMap[pos].inverse_map[ InternalPositionalMap[pos].map[i] ] = i;
//		}
//	}
//
//
//	InternalPositionalMap[pos].newMetaPointers = (int*) malloc ((numberOfAttributes + 1) * sizeof(int));
//	if ( num_internal_metapointers != -1 )
//	{//Limited budget
//		InternalPositionalMap[pos].numOfnewMetaPointers = 0;
//		for (i = 0 ; i < ( numberOfAttributes + 1); i++)
//		{
//			InternalPositionalMap[pos].newMetaPointers[i] = newCollectedElements[i];
//			if ( newCollectedElements[i] )
//				InternalPositionalMap[pos].numOfnewMetaPointers++;
//		}
//		free(newCollectedElements);
//		newCollectedElements = NULL;
//	}
//	else
//	{
//		InternalPositionalMap[pos].numOfnewMetaPointers = 0;
//		for (i = 0 ; i < ( numberOfAttributes + 1); i++)
//		{
//			InternalPositionalMap[pos].newMetaPointers[i] = which[i];
//			if ( which[i] )
//				InternalPositionalMap[pos].numOfnewMetaPointers++;
//		}
//	}
//
//	InternalPositionalMap[pos].newDeletedPointers = (int*) malloc ( (numberOfAttributes + 1) * sizeof(int));
//	InternalPositionalMap[pos].numOfnewDeletedPointers = 0;
//	for (i = 0 ; i <(numberOfAttributes + 1); i++)
//		InternalPositionalMap[pos].newDeletedPointers[i] = 0;
//
//
//	/*
//	 * Create two PositionalMapInfo one with all the information and one only with the pointers we are going to collect
//	 * Initially, the global and the collect contain exactly the same information
//	 */
//	createGlobalPositionalMapInfo(pos);
//	createCollectPositionalMapInfo(pos);
//
//	initializeGlobalPositionalMapInfo(pos, InternalPositionalMap[pos].map, InternalPositionalMap[pos].inverse_map, InternalPositionalMap[pos].activePointers);
//	initializeCollectPositionalMapInfo(pos, InternalPositionalMap[pos].map, InternalPositionalMap[pos].inverse_map, InternalPositionalMap[pos].activePointers);
//
//	/*  Initialize data structure */
//	how_many = INIT_NUMBER_OF_STRUCTURES;
//	for ( i = 0 ; i < InternalPositionalMap[pos].globalMapInfo.length; i++ ) {
//		if (InternalPositionalMap[pos].globalMapInfo.whichDescriptor[i] >= how_many)
//			how_many += INIT_NUMBER_OF_STRUCTURES;
//	}
//
//	InternalPositionalMap[pos].internalPointers = (InternalMetaPointersDesc*) malloc (how_many * sizeof(InternalMetaPointersDesc));
//	for ( i = 0; i < how_many; i++) {
//		InternalPositionalMap[pos].internalPointers[i].position = (InternalMetaPointer*) malloc(INIT_TUPLES * INTERNAL_POINTERS * sizeof(InternalMetaPointer));
//		if (!InternalPositionalMap[pos].internalPointers[0].position) {
//			write_stderr(_("Error in initializeInternalPositionalMap() file positionalIndex.c\n"));
//			exit(1);
//		}
//		InternalPositionalMap[pos].internalPointers[i].currentSize = (INIT_TUPLES * INTERNAL_POINTERS);
//	}
//
//	InternalPositionalMap[pos].numberOfTuples = 0;
//	InternalPositionalMap[pos].numberOfTuplesCur = 0;
//
//	//Initialized!
//	InternalPositionalMap[pos].initialized = true;
//	InternalPositionalMap[pos].first = false;
//	InternalPositionalMap[pos].numberOfDesc = how_many;
//
//	internalMap_usedFiles++;
//	return pos;
//}
//
///*
// * Reorganize MapInfo
// * TODO: add a free method
// */
//void
//createGlobalPositionalMapInfo( int pos )
//{
//	int i;
//	//Size equal to the number of attributes in case we want to resize the initial choice ;-)
//	InternalPositionalMap[pos].globalMapInfo.pointerID 				= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].globalMapInfo.pointerPos 			= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].globalMapInfo.whichDescriptor 		= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].globalMapInfo.offset 				= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].globalMapInfo.inverse_map 			= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes + 1) * sizeof(int));
//
//	InternalPositionalMap[pos].globalMapInfo.length = 0;
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//	{
//		InternalPositionalMap[pos].globalMapInfo.pointerID[i] = -1;
//		InternalPositionalMap[pos].globalMapInfo.pointerPos[i] = -1;
//		InternalPositionalMap[pos].globalMapInfo.whichDescriptor[i] = -1;
//		InternalPositionalMap[pos].globalMapInfo.offset[i] = -1;
//	}
//
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes + 1); i++ )
//		InternalPositionalMap[pos].globalMapInfo.inverse_map[i] = -1;
//
//	InternalPositionalMap[pos].globalMapInfo.neededMapList 				= (int*) malloc ( 3 * (InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].globalMapInfo.defaultneededMapList 	= (int*) malloc ( 3 * (InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//}
//
//
//void
//createCollectPositionalMapInfo( int pos )
//{
//	int i;
//
//	InternalPositionalMap[pos].collectMapInfo.pointerID 			= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].collectMapInfo.pointerPos 			= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].collectMapInfo.whichDescriptor 		= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].collectMapInfo.offset 				= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].collectMapInfo.inverse_map 			= (int*) malloc ((InternalPositionalMap[pos].numberOfAttributes + 1) * sizeof(int));
//
//	InternalPositionalMap[pos].collectMapInfo.length = 0;
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//	{
//		InternalPositionalMap[pos].collectMapInfo.pointerID[i] = -1;
//		InternalPositionalMap[pos].collectMapInfo.pointerPos[i] = -1;
//		InternalPositionalMap[pos].collectMapInfo.whichDescriptor[i] = -1;
//		InternalPositionalMap[pos].collectMapInfo.offset[i] = -1;
//	}
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes + 1); i++ )
//		InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = -1;
//
//	InternalPositionalMap[pos].collectMapInfo.neededMapList 				= (int*) malloc ( 3 * (InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//	InternalPositionalMap[pos].collectMapInfo.defaultneededMapList 	= (int*) malloc ( 3 * (InternalPositionalMap[pos].numberOfAttributes - 1) * sizeof(int));
//}
//
//void
//initializeGlobalPositionalMapInfo(int pos, int* map, int *inverse_map, int activePointers)
//{
//	int i;
//	InternalPositionalMap[pos].globalMapInfo.length = activePointers;
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//	{
//		if ( map[i] != -1 )
//		{
//			InternalPositionalMap[pos].globalMapInfo.pointerPos[i] = i;
//			InternalPositionalMap[pos].globalMapInfo.pointerID[i] = map[i];
//			InternalPositionalMap[pos].globalMapInfo.whichDescriptor[i] = i / INTERNAL_POINTERS;
//			InternalPositionalMap[pos].globalMapInfo.offset[i] =
//					(InternalPositionalMap[pos].globalMapInfo.pointerPos[i] - ( InternalPositionalMap[pos].globalMapInfo.whichDescriptor[i] * INTERNAL_POINTERS) );
//		}
//	}
//
//
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes + 1); i++ )
//	{
//		if ( inverse_map[i] != -1 )
//			InternalPositionalMap[pos].globalMapInfo.inverse_map[i] = inverse_map[i];
//	}
//}
//
//void
//initializeCollectPositionalMapInfo(int pos, int* map, int *inverse_map, int activePointers)
//{
//	int i;
//	InternalPositionalMap[pos].collectMapInfo.length = activePointers;
//	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//	{
//		if ( map[i] != -1 )
//		{
//			InternalPositionalMap[pos].collectMapInfo.pointerPos[i] = i;
//			InternalPositionalMap[pos].collectMapInfo.pointerID[i] = map[i];
//			InternalPositionalMap[pos].collectMapInfo.whichDescriptor[i] = i / INTERNAL_POINTERS;
//			InternalPositionalMap[pos].collectMapInfo.offset[i] =
//					(InternalPositionalMap[pos].collectMapInfo.pointerPos[i] - ( InternalPositionalMap[pos].collectMapInfo.whichDescriptor[i] * INTERNAL_POINTERS) );
//		}
//		else
//		{
//			InternalPositionalMap[pos].collectMapInfo.pointerID[i] = -1;
//			InternalPositionalMap[pos].collectMapInfo.pointerPos[i] = -1;
//			InternalPositionalMap[pos].collectMapInfo.whichDescriptor[i] = -1;
//			InternalPositionalMap[pos].collectMapInfo.offset[i] = -1;
//		}
//	}
////	for ( i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes + 1); i++ )
////	{
////		if ( inverse_map[i] != -1 )
////			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = inverse_map[i];
////		else
////			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = -1;
////	}
//}
//
//
///*
// * We assume that the newMetapointers haven't been collected...
// * To double check we can use the BitMap...
// * */
//void
//updateGlobalPositionalMapInfo(int pos)
//{
//	int i;
//	//First available position is after the last active pointer
//	int position;
//
//	if(  InternalPositionalMap[pos].numOfnewMetaPointers > 0 )
//	{
//		position = InternalPositionalMap[pos].globalMapInfo.length;
//		//[1,N-1] --> new pointers
//		for ( i = 1 ; i <= (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//		{
//			if ( InternalPositionalMap[pos].newMetaPointers[i] )
//			{
//				InternalPositionalMap[pos].globalMapInfo.pointerPos[position] = position;
//				InternalPositionalMap[pos].globalMapInfo.pointerID[position] = i;
//				InternalPositionalMap[pos].globalMapInfo.whichDescriptor[position] = position / INTERNAL_POINTERS;
//				InternalPositionalMap[pos].globalMapInfo.offset[position] =
//						(InternalPositionalMap[pos].globalMapInfo.pointerPos[position] - ( InternalPositionalMap[pos].globalMapInfo.whichDescriptor[position] * INTERNAL_POINTERS) );
//				InternalPositionalMap[pos].globalMapInfo.inverse_map[i] = (position);
//				position++;
//			}
//		}
//	//	InternalPositionalMap[pos].activePointers += numNewMetapointers;
//		InternalPositionalMap[pos].globalMapInfo.length += InternalPositionalMap[pos].numOfnewMetaPointers;
//	}
//}
//
//
//void
//updateCollectPositionalMapInfo(int pos)
//{
//	int i;
//	int curPointer = 0;
//	int position;
//
//
//	if(  InternalPositionalMap[pos].numOfnewMetaPointers > 0 )
//	{
////		position = InternalPositionalMap[pos].activePointers;;
//		for ( i = 1 ; i <= (InternalPositionalMap[pos].numberOfAttributes - 1); i++ )
//		{
//			if ( InternalPositionalMap[pos].newMetaPointers[i] )
//			{
//				position = InternalPositionalMap[pos].inverse_map[i];
//
//				InternalPositionalMap[pos].collectMapInfo.pointerPos[curPointer] = position;
//				InternalPositionalMap[pos].collectMapInfo.pointerID[curPointer] = i;
//				InternalPositionalMap[pos].collectMapInfo.whichDescriptor[curPointer] = position / INTERNAL_POINTERS;
//				InternalPositionalMap[pos].collectMapInfo.offset[curPointer] =
//						(InternalPositionalMap[pos].collectMapInfo.pointerPos[curPointer] - ( InternalPositionalMap[pos].collectMapInfo.whichDescriptor[curPointer] * INTERNAL_POINTERS) );
//	//			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = curPointer;
//				curPointer++;
////				position++;
//			}
//		}
//		InternalPositionalMap[pos].collectMapInfo.length = InternalPositionalMap[pos].numOfnewMetaPointers;
//	}
//	else
//		InternalPositionalMap[pos].collectMapInfo.length = 0;
//	InternalPositionalMap[pos].numberOfTuples = 0;
//}
//
//
//void
//prepareGlobalPositionalMapInfo(int internalFilePointers_ID, int *neededMapPositions, int numOfneededMapPositions, int *defaultneededMapPositions, int numOfdefaultneededMapPositions)
//{
//	int whichAttribute;
//	int whichDescriptor;
//	int offset;
//	int temp;
//	int i;
//	int step = 3;
//
//	PositionalMap* tempMap = &InternalPositionalMap[internalFilePointers_ID];
//	PositionalMapInfo *mapInfo = &tempMap->globalMapInfo;
//
//	for (i = 0; i < numOfneededMapPositions; i++)
//	{
//		whichAttribute = neededMapPositions[i];
//		temp = mapInfo->inverse_map[whichAttribute];
//		whichDescriptor = mapInfo->whichDescriptor[temp];
//		offset = mapInfo->offset[temp];
//
//		mapInfo->neededMapList[i * step] = whichAttribute;
//		mapInfo->neededMapList[i * step + 1] = whichDescriptor;
//		mapInfo->neededMapList[i * step + 2 ] = offset;
//	}
//
//	for (i = 0; i < numOfdefaultneededMapPositions; i++)
//	{
//		whichAttribute = defaultneededMapPositions[i];
//		temp = mapInfo->inverse_map[whichAttribute];
//		whichDescriptor = mapInfo->whichDescriptor[temp];
//		offset = mapInfo->offset[temp];
//
//		mapInfo->defaultneededMapList[i * step] = whichAttribute;
//		mapInfo->defaultneededMapList[i * step + 1] = whichDescriptor;
//		mapInfo->defaultneededMapList[i * step + 2 ] = offset;
//	}
//}
//
//
//void
//prepareCollectPositionalMapInfo(int internalFilePointers_ID)
//{
//	int i;
//	int step = 3;
//	PositionalMapInfo *temp = &InternalPositionalMap[internalFilePointers_ID].collectMapInfo;
//
//	for (i = 0; i < temp->length; i++)
//	{
//		int id = temp->pointerID[i];
//		int desc_id = temp->whichDescriptor[i];
//		int offset = temp->offset[i];
//
//		temp->neededMapList[i * step] = id;
//		temp->neededMapList[i * step + 1] = desc_id;
//		temp->neededMapList[i * step + 2 ] = offset;
//	}
//}
//
//
///*
// * updateInternalPositionalMap with the pointers I'm going to collect
// * We allocate needed space for the needed  new pointers. Not in addInternalMapMetaPointer(s)
// * We don't update the BitMap but all the other structs
// */
//
//int
//updateInternalPositionalMap(int internalFilePointers_ID)
//{
//	int pos = internalFilePointers_ID;
//	int curPos;
//	int i;
//	int position;
//	int how_many;
//	void *tmp;
//	PositionalMapInfo temp;
//
//	/* Pointers should be deleted */
//	if(InternalPositionalMap[pos].numOfnewDeletedPointers > 0)
//	{/* We don't have to free any positions, just mark the positions in the Positional Map as unused */
//		/*Step 1: update map + inverse map*/
//		for (i = 1 ; i  < (InternalPositionalMap[pos].numberOfAttributes - 1); i++)
//		{//Use the inverse map to update the position that was freed above
//			if( InternalPositionalMap[pos].newDeletedPointers[i] )
//			{
//				InternalPositionalMap[pos].map[InternalPositionalMap[pos].inverse_map[i]] = -1;
//				InternalPositionalMap[pos].inverse_map[i] = -1;
//			}
//		}
//		InternalPositionalMap[pos].activePointers -= InternalPositionalMap[pos].numOfnewDeletedPointers;
//		//Re-initialize GlobalPositionalMapInfo
//		initializeGlobalPositionalMapInfo(pos, InternalPositionalMap[pos].map, InternalPositionalMap[pos].inverse_map, InternalPositionalMap[pos].activePointers);
//	}
//
//
//	/* Pointers will be collected */
//	if(InternalPositionalMap[pos].numOfnewMetaPointers > 0)
//	{
//		/*Step 1: update map*/
//		for (i = 1 ; i <= (InternalPositionalMap[pos].numberOfAttributes - 1); i++)
//		{
//			if( InternalPositionalMap[pos].newMetaPointers[i] )
//			{
//				position = getFirstFreeFromMap(InternalPositionalMap[pos].map, InternalPositionalMap[pos].numberOfAttributes - 1);
//				InternalPositionalMap[pos].map[position] = i;
//				InternalPositionalMap[pos].activePointers++;
//			}
//		}
//
//		/*Step 2: update inverse_map*/
//		for (i = 0 ; i < (InternalPositionalMap[pos].numberOfAttributes - 1); i++)
//		{
//			if(InternalPositionalMap[pos].map[i] != -1) {//We care for pointers 1...N-1
//				InternalPositionalMap[pos].inverse_map[ InternalPositionalMap[pos].map[i] ] = i;
//			}
//		}
//		//Update PositionalMapInfo both global and collect!
////		updateGlobalPositionalMapInfo(pos);
////		updateCollectPositionalMapInfo(pos);
//
//		initializeGlobalPositionalMapInfo(pos, InternalPositionalMap[pos].map, InternalPositionalMap[pos].inverse_map, InternalPositionalMap[pos].activePointers);
//		updateCollectPositionalMapInfo(pos);
//
//
//		//If after updating we have more decriptors than the pre-malloced, malloc new descriptors
//		/*TODO: Change to alloc space based on the number of tuples */
//		temp = InternalPositionalMap[pos].globalMapInfo;
//		how_many = (ceil ((double)temp.length / INTERNAL_POINTERS)) - InternalPositionalMap[pos].numberOfDesc;
//		if ( how_many > 0 )
//		{/* We have to realloc new descriptors - new level with chunks*/
//			curPos = InternalPositionalMap[pos].numberOfDesc;
//			InternalPositionalMap[pos].numberOfDesc += how_many;
//			tmp = (InternalMetaPointersDesc*) realloc (
//					InternalPositionalMap[pos].internalPointers,
//					InternalPositionalMap[pos].numberOfDesc * sizeof(InternalMetaPointersDesc));
//			if (tmp != NULL) {
//				InternalPositionalMap[pos].internalPointers = tmp;
//			}
//			for ( i = 0; i < how_many; i++) {
//				InternalPositionalMap[pos].internalPointers[curPos].position = (InternalMetaPointer*) malloc(INIT_TUPLES * INTERNAL_POINTERS * sizeof(InternalMetaPointer));
//				if (!InternalPositionalMap[pos].internalPointers[curPos].position) {
//					write_stderr(_("Error in initializeInternalPositionalMap() file positionalIndex.c\n"));
//					exit(1);
//				}
//				InternalPositionalMap[pos].internalPointers[curPos].currentSize = (INIT_TUPLES * INTERNAL_POINTERS);
//				curPos++;
//			}
//		}
//	}
//	else
//	{
//		//Update PositionalMapInfo both global and collect!
//		updateGlobalPositionalMapInfo(pos);
//		updateCollectPositionalMapInfo(pos);
//	}
//
//
//	return pos;
//}
//
//
//
///*
// * After collecting the pointers: Update BitMap
// */
//void
//updateInternalPositionalMapStatus(char *relation)
//{
//	int pos;
//	int i;
//	Assert(relation != NULL);
//
//	/* Update without initialization...*/
//	pos = getInternalPositionalMap(relation);
//	if ( pos == -1) {
//		fprintf(stderr, "Trying to update unknown relation (%s)",relation);
//		Assert(pos != -1);
//		return;
//	}
//
//	InternalPositionalMap[pos].activePointers = InternalPositionalMap[pos].globalMapInfo.length;
//
//	if (InternalPositionalMap[pos].activePointers > 0)
//		InternalPositionalMap[pos].isReady = true;
//	else
//		InternalPositionalMap[pos].isReady = false;
//
//
//	for (i = 1 ; i <= (InternalPositionalMap[pos].numberOfAttributes - 1); i++)
//	{
//		if( InternalPositionalMap[pos].newMetaPointers[i] )
//			setBitValue(InternalPositionalMap[pos].available, i, 1);
//	}
//
//	if (!InternalPositionalMap[pos].first)
//	{
//		InternalPositionalMap[pos].numberOfTuplesCur = InternalPositionalMap[pos].numberOfTuples;
//		InternalPositionalMap[pos].first = true;
//	}
//}
//
///*
// * After collecting the pointers: Update BitMap
// */
//void
//updateInternalPositionalMapStatus2(int pos)
//{
//	int i;
//
//	InternalPositionalMap[pos].activePointers = InternalPositionalMap[pos].globalMapInfo.length;
//
//	if (InternalPositionalMap[pos].activePointers > 0)
//		InternalPositionalMap[pos].isReady = true;
//	else
//		InternalPositionalMap[pos].isReady = false;
//
//
//	for (i = 1 ; i <= (InternalPositionalMap[pos].numberOfAttributes - 1); i++)
//	{
//		if( InternalPositionalMap[pos].newMetaPointers[i] )
//			setBitValue(InternalPositionalMap[pos].available, i, 1);
//	}
//
//	if (!InternalPositionalMap[pos].first)
//	{
//		InternalPositionalMap[pos].numberOfTuplesCur = InternalPositionalMap[pos].numberOfTuples;
//		InternalPositionalMap[pos].first = true;
//	}
//}
//
//
///*
// * For debug....
// */
//void
//printPositionalMapInfo(PositionalMapInfo mapInfo, int numAttrs)
//{
//	int i;
//	fprintf(stderr,"\n----------------------------------\n");
//	fprintf(stderr,"Length = %d\n",mapInfo.length);
//	fprintf(stderr,"pointerID = { ");
//	for (i = 0; i < mapInfo.length; i++)
//		fprintf(stderr,"%d ",mapInfo.pointerID[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"pointerPos = { ");
//	for (i = 0; i < mapInfo.length;i++)
//		fprintf(stderr,"%d ",mapInfo.pointerPos[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"whichDescriptor = { ");
//	for (i = 0; i < mapInfo.length;i++)
//		fprintf(stderr,"%d ",mapInfo.whichDescriptor[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"offset = { ");
//	for (i = 0; i < mapInfo.length;i++)
//		fprintf(stderr,"%d ",mapInfo.offset[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"inverse_map = { ");
//	for ( i = 0 ; i < (numAttrs + 1); i++ )
//		fprintf(stderr,"%d ",mapInfo.inverse_map[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"----------------------------------\n");
//
//}
//
////Add a new metapointer
////TODO:It does not work
////void
////addInternalMapMetaPointer(long newPosition, int whichRelation, int whichDescriptor, int attributePos, int offset)
////{
////	int whichPosition;
////	void *tmp;
////
////	whichPosition = ( InternalPositionalMap[whichRelation].numberOfTuples * INTERNAL_POINTERS) + offset;
////	if ( whichPosition >=  InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].currentSize)
////	{
////		InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].currentSize += (INIT_TUPLES * INTERNAL_POINTERS);
////		tmp = (InternalMetaPointer *) realloc(
////				 InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position,
////				(InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].currentSize) * sizeof(InternalMetaPointer) );
////		if (tmp != NULL) {
////			InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position = tmp;
////		}
////	}
////	InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition] = (InternalMetaPointer)(newPosition);
////}
//
///*
// * Add a set of metapointers in the positional map ;-)
// */
//
//void
//addInternalMapMetaPointers(InternalMetaPointer *attributeValues, int internalFilePointers_ID, int currentTuple)
//{
//	int step = 3;
//
//	int whichPosition, i;
//	void *tmp;
//	PositionalMapInfo *temp = &InternalPositionalMap[internalFilePointers_ID].collectMapInfo;
//	long start = (currentTuple * INTERNAL_POINTERS);
//
//	for (i = 0; i < temp->length; i++)
//	{
//		int pos = i * step;
////		int id = temp.pointerID[i];
////		int desc_id = temp.whichDescriptor[i];
////		int offset = temp.offset[i];
//
//		int id = temp->neededMapList[pos];
//		int desc_id = temp->neededMapList[pos + 1];
//		int offset = temp->neededMapList[pos + 2];
//
//		InternalMetaPointersDesc *pointersDesc = &InternalPositionalMap[internalFilePointers_ID].internalPointers[desc_id];
//		whichPosition = start + offset;
//		if ( whichPosition >= pointersDesc->currentSize)
//		{
//			pointersDesc->currentSize += (INIT_TUPLES * INTERNAL_POINTERS);
//			tmp = (InternalMetaPointer *) realloc(
//					pointersDesc->position,
//					(pointersDesc->currentSize) * sizeof(InternalMetaPointer) );
//			if (tmp != NULL) {
//				pointersDesc->position = tmp;
//			}
//		}
//		pointersDesc->position[whichPosition] = (InternalMetaPointer)(attributeValues[id - 1]);
//	}
//	InternalPositionalMap[internalFilePointers_ID].numberOfTuples++;
//}
//
////Add more than one metapointers ;-)
//void
//updateInternalMapMetaPointers(int *attributeValues, int internalFilePointers_ID, int currentTuple)
//{
//	int step = 3;
//	int whichPosition, i;
//	void *tmp;
//	PositionalMapInfo *temp = &InternalPositionalMap[internalFilePointers_ID].collectMapInfo;
//	long start = (currentTuple * INTERNAL_POINTERS);
//
//
//	for (i = 0; i < temp->length; i++)
//	{
//		int pos = i * step;
////		int id = temp.pointerID[i];
////		int desc_id = temp.whichDescriptor[i];
////		int offset = temp.offset[i];
//		int id = temp->neededMapList[pos];
//		int desc_id = temp->neededMapList[pos + 1];
//		int offset = temp->neededMapList[pos + 2];
//
//		InternalMetaPointersDesc *pointersDesc = &InternalPositionalMap[internalFilePointers_ID].internalPointers[desc_id];
//		whichPosition = start + offset;
//		if ( whichPosition >= pointersDesc->currentSize)
//		{
//			pointersDesc->currentSize += (INIT_TUPLES * INTERNAL_POINTERS);
//			tmp = (InternalMetaPointer *) realloc(
//					pointersDesc->position,
//					(pointersDesc->currentSize) * sizeof(InternalMetaPointer) );
//			if (tmp != NULL) {
//				pointersDesc->position = tmp;
//			}
//		}
//		pointersDesc->position[whichPosition] = (InternalMetaPointer)(attributeValues[id]);
//	}
//	InternalPositionalMap[internalFilePointers_ID].numberOfTuples++;
//}
//
//
//PositionalMapInfo
//getGlobalMapInfo(int whichRelation)
//{
//	return InternalPositionalMap[whichRelation].globalMapInfo;
//}
//
//PositionalMapInfo
//getCollectMapInfo(int whichRelation)
//{
//	return InternalPositionalMap[whichRelation].collectMapInfo;
//}
//
//BitMap*
//getBitMap(int whichRelation)
//{
//	return InternalPositionalMap[whichRelation].available;
//}
//
//void
//tupleProcessed(int whichRelation)
//{
//	InternalPositionalMap[whichRelation].numberOfTuples++;
//}
//
//int
//getNumberOfTuplesProcessed(int whichRelation)
//{
//	return InternalPositionalMap[whichRelation].numberOfTuples;
//}
//
//bool
//isInitializedInternalMapMetaPointers(int pos)
//{
//	return InternalPositionalMap[pos].initialized;
//}
//
//void
//setPositionalMapReady(int pos, bool val)
//{
//	InternalPositionalMap[pos].isReady = val;
//}
//
//bool
//isPositionalMapReady(int pos)
//{
//	return InternalPositionalMap[pos].isReady;
//}
//
//
//InternalMetaPointersDesc*
//getInternalMetaPointersDesc(int whichRelation, int whichDescriptor)
//{
//	return &InternalPositionalMap[whichRelation].internalPointers[whichDescriptor];
//}
//
//
//int*
//getAvailableInternalMapPointers(int whichRelation)
//{
//	return InternalPositionalMap[whichRelation].map;
//}
//
//
//void
//getInternalNeededMapMetapointers(long currentTuple, int whichRelation, int * attributes, int numOfneededMapPositions)
//{
//	int whichDescriptor;
//	int whichPosition;
//	int whichAttribute;
//	int i;
//	int step = 3;
//	long start = ( currentTuple * INTERNAL_POINTERS);
//
//	PositionalMap* tempMap = &InternalPositionalMap[whichRelation];
//	PositionalMapInfo *mapInfo = &tempMap->globalMapInfo;
//
//	for (i = 0; i < numOfneededMapPositions; i++)
//	{
//		int pos = i * step;
//		whichAttribute = mapInfo->neededMapList[pos];
//		whichDescriptor = mapInfo->neededMapList[pos + 1];
//		whichPosition = start + mapInfo->neededMapList[pos + 2];
//
////		attributes[whichAttribute] = InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition];
//		attributes[whichAttribute] = tempMap->internalPointers[whichDescriptor].position[whichPosition];
//	}
//}
//
//void
//getInternaldefaultNeededMapMetapointers(long currentTuple, int whichRelation, int * attributes, int numOfdefaultneededMapPositions)
//{
//	int whichDescriptor;
//	int whichPosition;
//	int whichAttribute;
//	int i;
//	int step = 3;
//	long start = ( currentTuple * INTERNAL_POINTERS);
//
//	PositionalMap* tempMap = &InternalPositionalMap[whichRelation];
//	PositionalMapInfo *mapInfo = &tempMap->globalMapInfo;
//
//	for (i = 0; i < numOfdefaultneededMapPositions; i++)
//	{
//		int pos = i * step;
//		whichAttribute = mapInfo->defaultneededMapList[pos];
//		whichDescriptor = mapInfo->defaultneededMapList[pos + 1];
//		whichPosition = start + mapInfo->defaultneededMapList[pos + 2];
//
////		attributes[whichAttribute] = InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition];
//		attributes[whichAttribute] = tempMap->internalPointers[whichDescriptor].position[whichPosition];
//	}
//}
//
//
//int
//getInternalMapMetapointer(long currentTuple, int whichRelation, int whichAttribute, PositionalMapInfo* mapInfo)
//{
//	int whichDescriptor;
//	int whichPosition;
//	int temp;
//
////	if (whichAttribute == 0)
////		return -1;
////	else if (whichAttribute == numAttributes)
////		//It costs a lot!!! To remove this...
//////		return FD[whichFD].tuplePointer.position[currentTuple]; //--> offset is already known from the previous call --> we don't need this call!!
////		return endOfTuple;
////	else
//	{//inverse_map seems to be a quick and dirty solution (To be checked in case we update the actual fields)
//		temp = mapInfo->inverse_map[whichAttribute];
//		whichDescriptor = mapInfo->whichDescriptor[temp];
//		whichPosition = ( currentTuple * INTERNAL_POINTERS) + mapInfo->offset[temp];
//
//		return InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition];
//	}
//}
//
//
///*
// * Input: List of interesting attributes
// * Output: List of positions before the interesting attributes
// */
//int*
//getNeededInterestingMetapointersBefore(int *interestingAttributes, int natts, int* numInterestingPointers)
//{
//	int i = 0;
//	int *which;
//	which = (int*) malloc((natts + 1) * sizeof(int));
//	*numInterestingPointers = 0;
//
//	for ( i = 0; i < (natts + 1); i++)
//		which[i] = 0;
//
//	for ( i = 0; i < natts; i++)
//	{
//		//If the attribute is interesting I want the i and (i+1) pointer..
//		if ( interestingAttributes[i] == 1 )
//			which[i] = 1;
//	}
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] )
//			(*numInterestingPointers)++;
//
//	return which;
//}
//
///*
// * Input: List of interesting attributes
// * Output: List of positions after the interesting attributes
// */
//int*
//getNeededInterestingMetapointersAfter(int *interestingAttributes, int natts, int* numInterestingPointers)
//{
//	int i = 0;
//	int *which;
//	which = (int*) malloc((natts + 1) * sizeof(int));
//	*numInterestingPointers = 0;
//
//	for ( i = 0; i < (natts + 1); i++)
//		which[i] = 0;
//
//	for ( i = 0; i < natts; i++)
//	{
//		//If the attribute is interesting I want the i and (i+1) pointer..
//		if ( interestingAttributes[i] == 1 )
//			which[i + 1] = 1;
//	}
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] )
//			(*numInterestingPointers)++;
//
//	return which;
//}
//
///*
// * Input: List of interesting attributes
// * Output: List of positions before and after the interesting attributes
// */
//int*
//getNeededInterestingMetapointersBoth(int *interestingAttributes, int natts, int* numInterestingPointers)
//{
//	int i = 0;
//	int *which;
//	which = (int*) malloc((natts + 1) * sizeof(int));
//	*numInterestingPointers = 0;
//
//	for ( i = 0; i < (natts + 1); i++)
//		which[i] = 0;
//
//	for ( i = 0; i < natts; i++)
//	{
//		//If the attribute is interesting I want the i and (i+1) pointer..
//		if ( interestingAttributes[i] == 1 )
//		{
//			which[i] = 1;
//			which[i + 1] = 1;
//		}
//	}
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] )
//			(*numInterestingPointers)++;
//
//	return which;
//}
//
//
//int*
//getQualAvailableMetapointers(int *qualAttributes, int natts, BitMap *available, int *count)
//{
//	int i = 0;
//	int *which;
//	which = (int*) malloc((natts + 1) * sizeof(int));
//	*count = 0;
//
//	for ( i = 0; i < (natts + 1); i++)
//		which[i] = 0;
//
//	for ( i = 0; i < natts; i++)
//	{
//		if ( qualAttributes[i] == 1 )
//		{
//			if( available->bitmap[i] == 1 )
//				which[i] = 1;
//			if( available->bitmap[i + 1] == 1 )
//				which[i + 1] = 1;
//		}
//	}
//	which[0] = 0;
//	which[natts] = 0;
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] )
//			(*count)++;
//
//	return which;
//}
//
//int*
//getParseNeededMetapointers(unsigned int numOftoBeParsed, unsigned int *toBeParsed, int natts, ParsingParameters *parameters, int *count)
//{
//	int i = 0;
//	int j = 0;
//	int *which;
//	which = (int*) malloc((natts + 1) * sizeof(int));
//	*count = 0;
//
//	for ( i = 0; i < (natts + 1); i++)
//		which[i] = 0;
//
//	for ( j = 0; j < numOftoBeParsed; j++ )
//	{
//		i = toBeParsed[j];
//		if (parameters[i].attribute_id != 0 && parameters[i].attribute_id != natts)
//			which[parameters[i].attribute_id] = 1;
//	}
//
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] )
//			(*count)++;
//
//	return which;
//}
//
//int*
//mergeLists(int *list1, int *list2, int natts, int *count)
//{
//	int *which;
//	int i = 0;
//	*count = 0;
//
//	which = (int*) malloc( (natts + 1) * sizeof(int));
//	for(i = 0 ; i <= natts; i++)
//		which[i] = -1;
//
//	for(i = 0 ; i <= natts; i++)
//	{
//		if (list1[i] == 1 || list2[i] == 1)
//			which[i] = 1;
//	}
//	for ( i = 0; i < (natts + 1); i++)
//		if ( which[i] == 1)
//			(*count)++;
//	return which;
//}
//
//int*
//updateMetapointerLists(int *neededMapPositions, int numOfneededMapPositions, int *defaultneededMapPositions, int numOfdefaultneededMapPositions, int natts)
//{
//	int i,j;
//	int found = 0;
//	int *which;
//	which = (int*) malloc( (natts + 1) * sizeof(int));
//	for(i = 0 ; i <= natts; i++)
//		which[i] = -1;
//
//	for(i = 0 ; i < numOfneededMapPositions; i++)
//	{
//		found = 0;
//		for(j = 0 ; j < numOfdefaultneededMapPositions; j++)
//		{
//			if (neededMapPositions[i] == defaultneededMapPositions[j])
//			{
//				found = 1;
//				break;
//			}
//		}
//		if( found )
//			neededMapPositions[i] = -1;
//	}
//	j = 0;
//	for(i = 0 ; i < numOfneededMapPositions; i++)
//	{
//		if (neededMapPositions[i] != -1)
//			which[j++] = neededMapPositions[i];
//	}
//	free(neededMapPositions);
//	neededMapPositions = NULL;
//
//	return which;
//}
//
//void
//getAvailableInterestingMetapointers(int* which, int* numInterestingPointers, int natts, BitMap *available, int* count)
//{
//	int i = 0;
//	int temp = 0;
//	*count = 0;
//
//	for(i = 0 ; i <= natts; i++)
//		which[i] = -1;
//
//	for(i = 0 ; i <= natts; i++)
//	{
//		if (numInterestingPointers[i] == 1 && available->bitmap[i] == 1)
//		{
//			if ( i != natts && i!= 0)
//				which[temp++] = i;
//		}
//	}
//	*count = temp;
//}
//
////Scan Map to find the right positions
//int*
//getInterestingPositions(int pos, int *interestingMetapointers, int natts, int* numInterestingPositions)
//{
//	int i, j, k;
//	int *which = NULL;
//	int count = 0;
//
//	//We exclude the first
//	for (i = 1; i < natts; i++)
//		if(interestingMetapointers[i])
//			count++;
//	//Position for the last attribute
//	count++;
//
//	if(count == 0)
//		return NULL;
//
//	which = (int*) malloc(count * sizeof(int));
//
//	//The first metapointer is known...
//	k = 0;
//	for (i = 1; i < natts; i++)
//	{
//		if(interestingMetapointers[i])
//		{
//			for ( j = 0; j < InternalPositionalMap[pos].globalMapInfo.length; j++)
//			{
//				if (InternalPositionalMap[pos].globalMapInfo.pointerID[j] == (i - 1))
//					break;
//			}
//			which[k++] = j;
//		}
//	}
//	if(interestingMetapointers[natts])
//		which[count - 1] = 1;
//	else
//		which[count - 1] = -1;
//	*numInterestingPositions = count;
//
//	return which;
//}
//
///* *
// * This function takes as input:
// * a) The interesting attributes to answer a query
// * b) The BitMap of available pointers
// * and computes:
// * a list with the pointers we have to collect and add in the data structure
// *
// * */
//void
//computeWhichPointersToCollect(int pos, BitMap *availablePointers, int natts, int *neededMetapointers)
//{
//	int i;
//	int *newCachedPointers;
//
//	PositionalMap * temp = &InternalPositionalMap[pos];
//
//	/*List with elements to be cached*/
//	temp->numOfnewMetaPointers = 0;
//	for ( i = 0 ; i < (natts + 1); i++)
//		temp->newMetaPointers[i] = 0;
//
//	if ( num_internal_metapointers == -1 )
//	{
//		/*Count needed but not cached columns*/
//		for ( i = 0 ; i < (temp->numberOfAttributes + 1); i++)
//		{
//			if( neededMetapointers[i] )
//				if( !getBit(temp->available,i) )
//				{
//					temp->newMetaPointers[i] = 1;
//					temp->numOfnewMetaPointers++;
//				}
//		}
//
//	}
//	else
//	{//Limited budget
//		//TODO check the pointers in the example...
//		newCachedPointers = applyPolicyPositionalMap(&temp->stats, temp->available, neededMetapointers, temp->numberOfAttributes, temp->newDeletedPointers);
//		temp->numOfnewMetaPointers = 0;
//		for (i = 1 ; i <= (temp->numberOfAttributes - 1); i++)
//		{
//			temp->newMetaPointers[i] = newCachedPointers[i];
//			if ( newCachedPointers[i] )
//				temp->numOfnewMetaPointers++;
//		}
//		free(newCachedPointers);
//		newCachedPointers = NULL;
//
//		/* Count the attributes that will be evicted from the positional map */
//		temp->numOfnewDeletedPointers = 0;
//		for (i = 1 ; i <= (temp->numberOfAttributes - 1); i++)
//		{
//			if ( temp->newDeletedPointers[i] )
//			{
//				temp->numOfnewDeletedPointers++;
//				setBitValue(temp->available, i, 0);
//			}
//		}
////		printUsageList(temp->stats);
//	}
//
//}
//
//
//void
//updateAttributeListUsingPositionaMap(int pos, int *interesting_attributes, int *numOfInterestingAtt, int *qual_attributes)
//{
//	int  i=0;
//	PositionalMap * temp = &InternalPositionalMap[pos];
//
//	//If already cached then remove it from the list ;-)
//	for( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if (interesting_attributes[i] && qual_attributes[i])
//		{
//			interesting_attributes[i] = 0;
//			(*numOfInterestingAtt)--;
//		}
//	}
//}
//
//
//
///*
// * For debug...
// */
//
//void
//printInternalMapMetadata(void)
//{
//	int i = 0,  z = 0, k = 0;
//	int step;
//	int valid;
//	int sum;
//	int count = 0;
//	fprintf(stderr,"\ninternalMap_usedFiles = %d\n",internalMap_usedFiles);
//	for( i = 0; i < internalMap_usedFiles; i++)
//	{
//		fprintf(stderr,"*****************************************\n");
//		fprintf(stderr,"File = %s\n",InternalPositionalMap[i].filename);
//		fprintf(stderr,"Relation = %s\n",InternalPositionalMap[i].relation);
//		fprintf(stderr,"Descriptors: %d\n",InternalPositionalMap[i].numberOfDesc);
//		fprintf(stderr,"Active Pointers: %d\n",InternalPositionalMap[i].activePointers);
//		fprintf(stderr,"INTERNAL_POINTERS: %d\n",INTERNAL_POINTERS);
//
//		valid = InternalPositionalMap[i].activePointers;
//		sum = 0;
//		for( k = 0; k < (InternalPositionalMap[i].numberOfDesc); k++)
//		{
//			count = 0;
//			if( InternalPositionalMap[i].internalPointers[k].currentSize > 0)
//			{
////				fprintf(stderr,"\nSize = %d\n",InternalPositionalMap[i].internalPointers[0].numberOfPointers);
//				/* Compute how many pointers are valid */
//				if ( sum < InternalPositionalMap[i].activePointers )
//				{
//					if (k == (InternalPositionalMap[i].numberOfDesc - 1))
//					{
//						step = valid % INTERNAL_POINTERS;
//						if ( step == 0)
//							step = INTERNAL_POINTERS;
//					}
//					else
//					{
//						if (INTERNAL_POINTERS > valid)
//							step = valid;
//						else
//							step = INTERNAL_POINTERS;
//					}
//					sum += step;
//					fprintf(stderr,"\nStep{%d} Vector {%d}: ",step,k);
//					if ( InternalPositionalMap[i].activePointers > 0 )
//					{
//						for( z = 0; z < InternalPositionalMap[i].numberOfTuplesCur * INTERNAL_POINTERS; z++)
//						{
//							if( z % INTERNAL_POINTERS < step )
//							{
//								count++;
//
//								if( InternalPositionalMap[i].internalPointers[k].position[z] != 0 )
//								{
//									if( z == InternalPositionalMap[i].internalPointers[k].currentSize ) {
//											fprintf(stderr,"%d \n",InternalPositionalMap[i].internalPointers[k].position[z]);
//									}
//									else {
//											fprintf(stderr,"%d ",InternalPositionalMap[i].internalPointers[k].position[z]);
//									}
//								}
//							}
//						}
//						fprintf(stderr,"Pointers: %d\n",count);
//					}
//				}
//			}
//		}
//		fprintf(stderr,"\n*****************************************\n\n");
//	}
//}
//
//
//void
//printPositionalMapTotal(int file_ID)
//{
//	int i;
//	PositionalMap *temp = &InternalPositionalMap[file_ID];
//
//	fprintf(stderr,"---------FILE_ID = %d-----------\n",file_ID);
//
//	fprintf(stderr,"initialized = %d\n",temp->initialized);
//	fprintf(stderr,"isReady = %d\n",temp->isReady);
////	fprintf(stderr,"Budget = %d\n",temp->budget);
//	fprintf(stderr,"numberOfTuples = %ld\n",temp->numberOfTuples);
//	fprintf(stderr,"Attributes = %d\n",temp->numberOfAttributes);
//	fprintf(stderr,"Descs = %d\n",temp->numberOfDesc);
//	fprintf(stderr,"ActivePointers = %d\n",temp->activePointers);
//
//	printBitMap(*(temp->available));
////	printPositionalMapInfo(temp->globalCacheInfo, temp->numberOfAttributes);
////	printPositionalMapInfo(temp->collectCacheInfo, temp->numberOfAttributes);
//
//	fprintf(stderr,"newMetaPointers = {%d}: { ",temp->numOfnewMetaPointers);
//	for ( i = 0; i < temp->numOfnewMetaPointers; i++ )
//		fprintf(stderr,"%d ",temp->newMetaPointers[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"----------------------------------\n");
//}



