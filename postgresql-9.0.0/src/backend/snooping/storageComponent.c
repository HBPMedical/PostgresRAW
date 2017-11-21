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



#include <math.h>
#include "postgres.h"

#include "snooping/storageComponent.h"
#include "snooping/common.h"


Cache relCache[NUMBER_OF_RELATIONS];
int relCache_usedFiles = 0;

bool enable_caching = false;
int num_cached_columns = -1;


///*
// * Initialize Internal Metadata Module (UP to NUMBER_OF_RELATIONS input files can be processed concurrently)
// */
//void
//initializeStorageComponent(void)
//{
//	int i;
//	for (i = 0; i < NUMBER_OF_RELATIONS; i++)
//	{
//		relCache[i].initialized =  false;
//		relCache[i].isReady =  false;
////		relCache[i].budget =  -1;
//		strcpy(relCache[i].filename, "\0");
//	}
//}
//
//
///*
// * Get position in InternalFilePointers for a specific file
// */
//int
//getRelCache(char *relation)
//{
//	int i = 0;
//	for (i = 0; i < relCache_usedFiles; i++)
//	{
//		if(strcmp(relCache[i].relation, relation) == 0)
//			return i;
//	}
//	return -1;
//}
//
//
//int
//getFreeRelCache(void)
//{
//	if( relCache_usedFiles == NUMBER_OF_RELATIONS){
//		write_stderr(_("Error in getFreeRelCache() file storageComponent.c: all pointers are used...\n"));
//	}
//	Assert( relCache_usedFiles < NUMBER_OF_RELATIONS);
//	return relCache_usedFiles;
//}
//
//int
//getCacheID(char *relation)
//{
//	int pos;
//	Assert(relation != NULL);
//	pos = getRelCache(relation);
//	if ( pos == -1)
//		pos = getFreeRelCache();
//	return pos;
//}
//
//long
//getNumberOfCachedTuples(int pos)
//{
//	return relCache[pos].numberOfTuplesCur;
//}
//
//
///* *
// * which: List of {1,0} for the needed metapoitners for the interesting attributes
// * which length: (numberOfAttributes)
// * For example:
// * If we have this schema |Attr1|Attr2|Attr3|Attr4|...|Attr(N)|, the interesting
// * attributes are Attr1 and Atrr2 and the policy is to collect pointers before and after
// * the interesting attribute, then we expect as input {1,1,0,0,...,0}
// * */
//int
//initializeRelCache(int pos, char *relation, FILE *fp, int numberOfAttributes, int *list)
//{
//	int i,j;
//	int how_many;
//	int *which;
//	int *newCachedElements;
//	which = list;
//
//	/*Budget is limited*/
//	if ( num_cached_columns != -1 )
//	{
//		/*Initialize stats*/
//		initializePolicy(&relCache[pos].stats,num_cached_columns,numberOfAttributes);
//		newCachedElements = firstTimeLRU(&relCache[pos].stats,list,numberOfAttributes);
//	}
//	else
//		newCachedElements = which;
//
//	/*Initialize metadata struct for filename*/
//	strcpy(relCache[pos].filename, getInputFilename(relation));
//	strcpy(relCache[pos].relation, relation);
//
//	/* File is closed, then open it... */
//	if(fp == NULL)
//	{
//		if ((relCache[pos].fp = fopen(relCache[pos].filename, "r")) == NULL){
//			fprintf(stderr, "File %s not found... (metadata.c)",relCache[pos].filename);
//		}
//	}
//	else
//		relCache[pos].fp = fp;
//
//
//	relCache[pos].numberOfAttributes = numberOfAttributes;
//
//	/* Every bit is matched to an attribute */
//	relCache[pos].available = createBitMap(numberOfAttributes);
//	for (i = 0 ; i < numberOfAttributes; i++)
//		setBitValue(relCache[pos].available, i, 0);
//
//	/* *
//	 * The MAP contains the position of theattribute in the data structure
//	 * MAP size is numAttr
//	 * */
//	relCache[pos].map = (int*) malloc (numberOfAttributes  * sizeof(int));
//	for (i = 0 ; i < numberOfAttributes ; i++)
//		relCache[pos].map[i] = -1;
//
//	j = 0;
//	for (i = 0 ; i < numberOfAttributes ; i++)
//	{
//		if(newCachedElements[i])
//		{
//			relCache[pos].map[j++] = i;
//			relCache[pos].activePointers++;
//		}
//	}
//
//	/* *
//	 * Inverse MAP is used to identify the positions of the pointers in the data structure.
//	 * For example, MAP=[3,2,5]  --> we have collected pointers with ID: 2,3,5
//	 * Then the inverse_MAP = [-1,-1, 1, 0, -1, 2] aka the fifth pointer is the second position
//	 * of the data structure.
//	 * */
//	relCache[pos].inverse_map = (int*) malloc (numberOfAttributes  * sizeof(int));
//	for (i = 0 ; i <  numberOfAttributes ; i++)
//		relCache[pos].inverse_map[i] = -1;
//
//	for (i = 0 ; i < numberOfAttributes; i++)
//	{
//		if(relCache[pos].map[i] != -1) {//We care for pointers 0...N
//			relCache[pos].inverse_map[ relCache[pos].map[i] ] = i;
//		}
//	}
//
//	relCache[pos].newCachedElements = (int*) malloc (numberOfAttributes * sizeof(int));
//	if ( num_cached_columns != -1 )
//	{//Limited budget
//		relCache[pos].numOfnewCachedElements = 0;
//		for (i = 0 ; i < numberOfAttributes; i++)
//		{
//			relCache[pos].newCachedElements[i] = newCachedElements[i];
//			if ( newCachedElements[i] )
//				relCache[pos].numOfnewCachedElements++;
//		}
//		free(newCachedElements);
//		newCachedElements = NULL;
//	}
//	else
//	{
//		relCache[pos].numOfnewCachedElements = 0;
//		for (i = 0 ; i < numberOfAttributes; i++)
//		{
//			relCache[pos].newCachedElements[i] = which[i];
//			if ( which[i] )
//				relCache[pos].numOfnewCachedElements++;
//		}
//
//	}
//
//	relCache[pos].newDeletedElements = (int*) malloc (numberOfAttributes * sizeof(int));
//	relCache[pos].numOfnewDeletedElements = 0;
//	for (i = 0 ; i < numberOfAttributes; i++)
//		relCache[pos].newDeletedElements[i] = 0;
//
//
//
//	/*Positions of the elements to be loaded from the cache*/
//	relCache[pos].loadedElements = (int*) malloc (numberOfAttributes * sizeof(int));
//	for (i = 0 ; i < numberOfAttributes; i++)
//		relCache[pos].loadedElements[i] = 0;
//	relCache[pos].numOfloadedElements = 0;
//
//	/*Positions of the elements to be loaded from the cache for the where clause*/
//	relCache[pos].where_loadedElements = (int*) malloc (numberOfAttributes * sizeof(int));
//	for (i = 0 ; i < numberOfAttributes; i++)
//		relCache[pos].where_loadedElements[i] = 0;
//	relCache[pos].numOfwhere_loadedElements = 0;
//
//	/*
//	 * Create two PositionalMapInfo one with all the information and one only with the pointers we are going to collect
//	 * Initially, the global and the collect contain exactly the same information
//	 */
//	createGlobalRelCacheInfo(pos);
//	createCollectRelCacheInfo(pos);
//
//	initializeGlobalRelCacheInfo(pos, relCache[pos].map, relCache[pos].inverse_map, relCache[pos].activePointers);
//	initializeCollectRelCacheInfo(pos, relCache[pos].map, relCache[pos].inverse_map, relCache[pos].activePointers);
//
//	/*  Initialize data structure */
//	how_many = INIT_NUMBER_OF_LEVELS;
//	for ( i = 0 ; i < relCache[pos].globalCacheInfo.length; i++ ) {
//		if (relCache[pos].globalCacheInfo.whichLevel[i] >= how_many)
//			how_many += INIT_NUMBER_OF_LEVELS;
//	}
//
//	relCache[pos].cachedLevels = (CachedDatumDesc*) malloc (how_many * sizeof(CachedDatumDesc));
//	for ( i = 0; i < how_many; i++) {
//		relCache[pos].cachedLevels[i].position = (CachedElement*) malloc(INIT_CHUNKS * CHUNK_SIZE   * sizeof(CachedElement));
//		if (!relCache[pos].cachedLevels[0].position) {
//			write_stderr(_("Error in initializeInternalPositionalMap() file positionalIndex.c\n"));
//			exit(1);
//		}
//		relCache[pos].cachedLevels[i].currentSize = (INIT_CHUNKS * CHUNK_SIZE);
//	}
//
//	relCache[pos].numberOfTuples = 0;
//	relCache[pos].numberOfTuplesCur = 0;
//
//	//Initialized!!
//	relCache[pos].initialized = true;
//	relCache[pos].first = false;
//	relCache[pos].numberOfLevels = how_many;
//
//	relCache_usedFiles++;
//
//	return pos;
//}
//
//
///*
// * Reorganize MapInfo: add a free method somewhere...
// */
//void
//createGlobalRelCacheInfo( int pos )
//{
//	int i;
//	//Size equal to the number of attributes in case we want to resize the initial choice ;-)
//	relCache[pos].globalCacheInfo.elementID 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].globalCacheInfo.elementPos 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].globalCacheInfo.whichLevel 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].globalCacheInfo.offset 		= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].globalCacheInfo.inverse_map 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//
//	relCache[pos].globalCacheInfo.length = 0;
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//	{
//		relCache[pos].globalCacheInfo.elementID[i] = -1;
//		relCache[pos].globalCacheInfo.elementPos[i] = -1;
//		relCache[pos].globalCacheInfo.whichLevel[i] = -1;
//		relCache[pos].globalCacheInfo.offset[i] = -1;
//	}
//
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//		relCache[pos].globalCacheInfo.inverse_map[i] = -1;
//}
//
//void
//createCollectRelCacheInfo( int pos )
//{
//	int i;
//
//	relCache[pos].collectCacheInfo.elementID 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].collectCacheInfo.elementPos 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].collectCacheInfo.whichLevel 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].collectCacheInfo.offset 		= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//	relCache[pos].collectCacheInfo.inverse_map 	= (int*) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//
//	relCache[pos].collectCacheInfo.length = 0;
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes ; i++ )
//	{
//		relCache[pos].collectCacheInfo.elementID[i] = -1;
//		relCache[pos].collectCacheInfo.elementPos[i] = -1;
//		relCache[pos].collectCacheInfo.whichLevel[i] = -1;
//		relCache[pos].collectCacheInfo.offset[i] = -1;
//	}
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//		relCache[pos].collectCacheInfo.inverse_map[i] = -1;
//}
//
//void
//initializeGlobalRelCacheInfo(int pos, int* map, int *inverse_map, int activePointers)
//{
//	int i;
//	relCache[pos].globalCacheInfo.length = activePointers;
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//	{
//		if ( map[i] != -1 )
//		{
//			relCache[pos].globalCacheInfo.elementPos[i] = i;
//			relCache[pos].globalCacheInfo.elementID[i] = map[i];
//			relCache[pos].globalCacheInfo.whichLevel[i] = i / CHUNK_SIZE;
//			relCache[pos].globalCacheInfo.offset[i] =
//					(relCache[pos].globalCacheInfo.elementPos[i] - ( relCache[pos].globalCacheInfo.whichLevel[i] * CHUNK_SIZE) );
//		}
//	}
//
//
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//	{
//		if ( inverse_map[i] != -1 )
//			relCache[pos].globalCacheInfo.inverse_map[i] = inverse_map[i];
//	}
//}
//
//void
//initializeCollectRelCacheInfo(int pos, int* map, int *inverse_map, int activePointers)
//{
//	int i;
//	relCache[pos].collectCacheInfo.length = activePointers;
//	for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//	{
//		if ( map[i] != -1 )
//		{
//			relCache[pos].collectCacheInfo.elementPos[i] = i;
//			relCache[pos].collectCacheInfo.elementID[i] = map[i];
//			relCache[pos].collectCacheInfo.whichLevel[i] = i / CHUNK_SIZE;
//			relCache[pos].collectCacheInfo.offset[i] =
//					(relCache[pos].collectCacheInfo.elementPos[i] - ( relCache[pos].collectCacheInfo.whichLevel[i] * CHUNK_SIZE) );
//		}
//		else
//		{
//			relCache[pos].collectCacheInfo.elementID[i] = -1;
//			relCache[pos].collectCacheInfo.elementPos[i] = -1;
//			relCache[pos].collectCacheInfo.whichLevel[i] = -1;
//			relCache[pos].collectCacheInfo.offset[i] = -1;
//		}
//	}
////	for ( i = 0 ; i < InternalPositionalMap[pos].numberOfAttributes; i++ )
////	{
////		if ( inverse_map[i] != -1 )
////			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = inverse_map[i];
////		else
////			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = -1;
////	}
//}
//
///*
// * We assume that the newMetapointers haven't been collected...
// * To double check we can use the BitMap...
// * */
//void
//updateGlobalRelCacheInfo(int pos)
//{
//	int i;
//	//First available position is after the last active pointer
//	int position;
//
//	if(  relCache[pos].numOfnewCachedElements > 0 )
//	{
//		position = relCache[pos].globalCacheInfo.length;
//		for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//		{
//			if ( relCache[pos].newCachedElements[i] )
//			{
////				position = getFirstFreeFromMap(relCache[pos].map, relCache[pos].numberOfAttributes);
//
//				relCache[pos].globalCacheInfo.elementPos[position] = position;
//				relCache[pos].globalCacheInfo.elementID[position] = i;
//				relCache[pos].globalCacheInfo.whichLevel[position] = position / CHUNK_SIZE;
//				relCache[pos].globalCacheInfo.offset[position] =
//						(relCache[pos].globalCacheInfo.elementPos[position] - ( relCache[pos].globalCacheInfo.whichLevel[position] * CHUNK_SIZE) );
//				relCache[pos].globalCacheInfo.inverse_map[i] = (position);
//				position++;
//			}
//		}
//	//	InternalPositionalMap[pos].activePointers += numNewMetapointers;
//		relCache[pos].globalCacheInfo.length += relCache[pos].numOfnewCachedElements;
//	}
//}
//
//
//void
//updateCollectRelCacheInfo(int pos)
//{
//	int i;
//	int curPointer = 0;
//	int position;
//
//	if(  relCache[pos].numOfnewCachedElements > 0 )
//	{
////		position = relCache[pos].activePointers;;
//		for ( i = 0 ; i < relCache[pos].numberOfAttributes; i++ )
//		{
//			if ( relCache[pos].newCachedElements[i] )
//			{
//				position = relCache[pos].inverse_map[i];
//
//				relCache[pos].collectCacheInfo.elementPos[curPointer] = position;
//				relCache[pos].collectCacheInfo.elementID[curPointer] = i;
//				relCache[pos].collectCacheInfo.whichLevel[curPointer] = position / CHUNK_SIZE;
//				relCache[pos].collectCacheInfo.offset[curPointer] =
//						(relCache[pos].collectCacheInfo.elementPos[curPointer] - ( relCache[pos].collectCacheInfo.whichLevel[curPointer] * CHUNK_SIZE) );
//	//			InternalPositionalMap[pos].collectMapInfo.inverse_map[i] = curPointer;
//				curPointer++;
////				position++;
//			}
//		}
//		relCache[pos].collectCacheInfo.length = relCache[pos].numOfnewCachedElements;
//	}
//	else
//		relCache[pos].collectCacheInfo.length = 0;
//	relCache[pos].numberOfTuples = 0;
//}
//
//
///*
// * updateInternalPositionalMap with the pointers I'm going to collect
// * We allocate needed space for the needed  new pointers.
// * We don't update the BitMap but all the other structs
// */
//void
//updateRelCache(int cache_ID, Form_pg_attribute *attr)
//{
//	int pos;
//	int curPos;
//	int i;
//	int j;
//	int position;
//	int how_many;
//	void *tmp;
//	CacheInfo temp;
//
//	int whichDescriptor;
//	int whichPosition;
//	int whichAttribute;
//	int id;
//	long start;
//	Cache* tempCache;
//	CacheInfo *cacheInfo;
//
//	pos = cache_ID;
//
//	/*Initially remove all the attributes that */
//	if(relCache[pos].numOfnewDeletedElements > 0)
//	{
//		int *tobedeleted = (int *) malloc (relCache[pos].numberOfAttributes * sizeof(int));
//		j = 0;
//		for (i = 0; i < relCache[pos].numberOfAttributes; i++)
//		{
//			if(relCache[pos].newDeletedElements[i])
//			{
//				tobedeleted[j++] = i;
////				setBitValue(relCache[pos].available, i, 0);
//			}
//		}
//		//Delete from cache
//		tempCache = &relCache[pos];
//		cacheInfo = &tempCache->globalCacheInfo;
//
//		/*Step 1: Free Datum*/
//		for(j = 0 ; j < relCache[pos].numberOfTuplesCur; j++)
//		{
//			start = ( j * CHUNK_SIZE);
//			for (i = 0; i < relCache[pos].numOfnewDeletedElements; i++)
//			{
//				whichAttribute = tobedeleted[i];
//				if(!attr[whichAttribute]->attbyval)
//				{
//					id = cacheInfo->inverse_map[whichAttribute];
//					whichDescriptor = cacheInfo->whichLevel[id];
//					whichPosition = start + cacheInfo->offset[id];
//					{
//						Pointer s = DatumGetPointer(tempCache->cachedLevels[whichDescriptor].position[whichPosition]);
//						free(s);
//					}
//				}
//			}
//		}
//
//		/*Step 2: update map + inverse map*/
//		for (i = 0 ; i  < relCache[pos].numberOfAttributes; i++)
//		{//Use the inverse map to update the position that was freed above
//			if( relCache[pos].newDeletedElements[i] )
//			{
//				relCache[pos].map[relCache[pos].inverse_map[i]] = -1;
//				relCache[pos].inverse_map[i] = -1;
//			}
//		}
//		relCache[pos].activePointers -= relCache[pos].numOfnewDeletedElements;
//		//Re-initialize GlobalRelCacheInfo
//		initializeGlobalRelCacheInfo(pos, relCache[pos].map, relCache[pos].inverse_map, relCache[pos].activePointers);
//
//		free (tobedeleted);
//		tobedeleted = NULL;
//	}
//
//
//	if(relCache[pos].numOfnewCachedElements > 0)
//	{
//		/*Step 1: update map*/
//		for (i = 0 ; i  < relCache[pos].numberOfAttributes; i++)
//		{
//			if( relCache[pos].newCachedElements[i] )
//			{
//				position = getFirstFreeFromMap(relCache[pos].map, relCache[pos].numberOfAttributes);
//				relCache[pos].map[position] = i;
//				relCache[pos].activePointers++;
//			}
//		}
//
//		/*Step 2: update inverse_map*/
//		for (i = 0 ; i < relCache[pos].numberOfAttributes; i++)
//		{
//			if(relCache[pos].map[i] != -1) {
//				relCache[pos].inverse_map[ relCache[pos].map[i] ] = i;
//			}
//		}
//
//		//Update PositionalMapInfo both global and collect!
////		updateGlobalRelCacheInfo(pos);
////		updateCollectRelCacheInfo(pos);
//		//RE-init
//		initializeGlobalRelCacheInfo(pos, relCache[pos].map, relCache[pos].inverse_map, relCache[pos].activePointers);
//		updateCollectRelCacheInfo(pos);
////		initializeCollectRelCacheInfo(pos, relCache[pos].map, relCache[pos].inverse_map, relCache[pos].activePointers);
//
//
//
//		//If after updating we have more decriptors than the pre-malloced, malloc new descriptors
//		/*TODO: Change to alloc space based on the number of tuples */
//		temp = relCache[pos].globalCacheInfo;
//		//how_many = (ceil ((double)temp.length / CHUNK_SIZE)) - relCache[pos].numberOfLevels;
//		how_many = (ceil ((double)temp.length / CHUNK_SIZE)) - relCache[pos].numberOfLevels;
//
//		if ( how_many > 0 )
//		{/*We have to realloc new descriptors*/
//			curPos = relCache[pos].numberOfLevels;
//			relCache[pos].numberOfLevels += how_many;
//			tmp = (CachedDatumDesc*) realloc (
//					relCache[pos].cachedLevels,
//					relCache[pos].numberOfLevels * sizeof(CachedDatumDesc));
//			if (tmp != NULL) {
//				relCache[pos].cachedLevels = tmp;
//			}
//			for ( i = 0; i < how_many; i++) {
//				relCache[pos].cachedLevels[curPos].position = (CachedElement*) malloc(INIT_CHUNKS * CHUNK_SIZE * sizeof(CachedElement));
//				if (!relCache[pos].cachedLevels[curPos].position) {
//					write_stderr(_("Error in relCache() file storageComponent.c\n"));
//					exit(1);
//				}
//				relCache[pos].cachedLevels[curPos].currentSize = (INIT_CHUNKS * CHUNK_SIZE);
//				curPos++;
//			}
//		}
//	}
//	else
//	{
//		//Update relCacheInfo both global and collect!
//		updateGlobalRelCacheInfo(pos);
//		updateCollectRelCacheInfo(pos);
//	}
//}
//
//
//
///*
// * After collecting the pointers: Update BitMap
// */
//void
//updateRelCacheStatus(char *relation)
//{
//	int pos;
//	int i;
//	Assert(relation != NULL);
//
//	/* Update without initialization...*/
//	pos = getRelCache(relation);
//	if ( pos == -1) {
//		fprintf(stderr, "Trying to update unknown relation (%s)",relation);
//		Assert(pos != -1);
//		return;
//	}
//
//	relCache[pos].activePointers = relCache[pos].globalCacheInfo.length;
//
//	if (relCache[pos].activePointers > 0)
//		relCache[pos].isReady = true;
//	else
//		relCache[pos].isReady = false;
//
//
//	for (i = 0 ; i < relCache[pos].numberOfAttributes; i++)
//	{
//		if( relCache[pos].newCachedElements[i] )
//			setBitValue(relCache[pos].available, i, 1);
//	}
//	if (!relCache[pos].first)
//	{
//		relCache[pos].numberOfTuplesCur = relCache[pos].numberOfTuples;
//		relCache[pos].first = true;
//	}
//}
//
//void
//updateRelCacheStatus2(int pos)
//{
//	int i;
//
//	relCache[pos].activePointers = relCache[pos].globalCacheInfo.length;
//
//	if (relCache[pos].activePointers > 0)
//		relCache[pos].isReady = true;
//	else
//		relCache[pos].isReady = false;
//
//
//	for (i = 0 ; i < relCache[pos].numberOfAttributes; i++)
//	{
//		if( relCache[pos].newCachedElements[i] )
//			setBitValue(relCache[pos].available, i, 1);
//	}
//	if (!relCache[pos].first)
//	{
//		relCache[pos].numberOfTuplesCur = relCache[pos].numberOfTuples;
//		relCache[pos].first = true;
//	}
//}
//
//
///*
// * For debug....
// */
//void
//printRelCacheInfo(CacheInfo cacheInfo, int numAttrs)
//{
//	int i;
//	fprintf(stderr,"----------------------------------\n");
//	fprintf(stderr,"Length = %d\n",cacheInfo.length);
//	fprintf(stderr,"pointerID = { ");
//	for (i = 0; i < cacheInfo.length; i++)
//		fprintf(stderr,"%d ",cacheInfo.elementID[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"pointerPos = { ");
//	for (i = 0; i < cacheInfo.length;i++)
//		fprintf(stderr,"%d ",cacheInfo.elementPos[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"whichDescriptor = { ");
//	for (i = 0; i < cacheInfo.length;i++)
//		fprintf(stderr,"%d ",cacheInfo.whichLevel[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"offset = { ");
//	for (i = 0; i < cacheInfo.length;i++)
//		fprintf(stderr,"%d ",cacheInfo.offset[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"inverse_map = { ");
//	for ( i = 0 ; i < numAttrs; i++ )
//		fprintf(stderr,"%d ",cacheInfo.inverse_map[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"----------------------------------\n");
//
//}
//
//
///* Cache a set of DATUM */
//void
//addCacheDatum(CachedElement *attributeElements, Form_pg_attribute *attr, int cacheFile_ID, long currentTuple)
//{
//
//	int whichPosition, i;
//	void *tmp;
//	CacheInfo temp = relCache[cacheFile_ID].collectCacheInfo;
//	long start = (currentTuple * CHUNK_SIZE);
//
//	Datum datum;
//
//	for (i = 0; i < temp.length; i++)
//	{
//		int id = temp.elementID[i];
//		int desc_id = temp.whichLevel[i];
//		int offset = temp.offset[i];
//
//		CachedDatumDesc *datumDesc = &relCache[cacheFile_ID].cachedLevels[desc_id];
//		whichPosition = start + offset;
//		if ( whichPosition >= datumDesc->currentSize)
//		{
//			datumDesc->currentSize += (INIT_CHUNKS * CHUNK_SIZE);
//			tmp = (CachedElement *) realloc(
//					datumDesc->position,
//					(datumDesc->currentSize) * sizeof(CachedElement) );
//			if (tmp != NULL) {
//				datumDesc->position = tmp;
//			}
//		}
//		datum = datumCopy2(attributeElements[id],attr[id]->attbyval, attr[id]->attlen);
//
////		datumDesc->position[whichPosition] = (CachedElement)(attributeElements[id]);
//		datumDesc->position[whichPosition] = datum;
//	}
//	relCache[cacheFile_ID].numberOfTuples++;
//}
//
//
//
////Add more than one metapointers ;-)
//void
//updateCacheDatum(CachedElement *attributeElements, int cacheFile_ID, long currentTuple)
//{
//
//	int whichPosition, i;
//	void *tmp;
//	CacheInfo temp = relCache[cacheFile_ID].collectCacheInfo;
//	long start = (currentTuple * CHUNK_SIZE);
//
//	for (i = 0; i < temp.length; i++)
//	{
//		int id = temp.elementID[i];
//		int desc_id = temp.whichLevel[i];
//		int offset = temp.offset[i];
//
//		CachedDatumDesc *datumDesc = &relCache[cacheFile_ID].cachedLevels[desc_id];
//		whichPosition = start + offset;
//		if ( whichPosition >= datumDesc->currentSize)
//		{
//			datumDesc->currentSize += (INIT_CHUNKS * CHUNK_SIZE);
//			tmp = (CachedElement *) realloc(
//					datumDesc->position,
//					(datumDesc->currentSize) * sizeof(CachedElement) );
//			if (tmp != NULL) {
//				datumDesc->position = tmp;
//			}
//		}
//		datumDesc->position[whichPosition] = (CachedElement)(attributeElements[id]);
//	}
//	relCache[cacheFile_ID].numberOfTuples++;
//}
//
//
//void
//tupleCacheProcessed(int cache_ID)
//{
//	relCache[cache_ID].numberOfTuples++;
//}
//
//
//CacheInfo
//getGlobalCacheInfo(int whichLevel)
//{
//	return relCache[whichLevel].globalCacheInfo;
//}
//
//CacheInfo
//getCollectCacheInfo(int whichLevel)
//{
//	return relCache[whichLevel].collectCacheInfo;
//}
//
//bool
//isInitializedRelCache(int pos)
//{
//	return relCache[pos].initialized;
//}
//
//void
//setRelCacheReady(int pos, bool val)
//{
//	relCache[pos].isReady = val;
//}
//
//bool
//isRelCacheReady(int pos)
//{
//	return relCache[pos].isReady;
//}
//
//
///*
// * Examine
// */
//int*
//copyList(int *list, int natts)
//{
//	int i = 0;
//	int *new_list;
//	new_list = (int*) malloc(natts * sizeof(int));
//
//	for ( i = 0; i < natts; i++)
//		new_list[i] = list[i];
//
//	return new_list;
//}
//
//
//void
//updateInterestingAttributesUsingCache(int cacheFile_ID, int *interesting_attributes, int *numOfInterestingAtt)
//{
//	int  i=0;
//	Cache *temp = &relCache[cacheFile_ID];
//	if(!temp->isReady)
//		return;
//	for( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if (interesting_attributes[i])
//		{
//			/*If the column is cached then it is not an interesting attibute for the positional map*/
//			if(getBit(temp->available, i))
//			{
//				interesting_attributes[i] = 0;
//				(*numOfInterestingAtt)--;
//			}
//		}
//	}
//}
//
//void
//updateAttributeListUsingCache(int cacheFile_ID, int *interesting_attributes, int *numOfInterestingAtt, int *qual_attributes, int *numOfInterestingQualAtt)
//{
//	int  i=0;
//	Cache *temp = &relCache[cacheFile_ID];
//
//	//If already cached then remove it from the list ;-)
//	for( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if (interesting_attributes[i])
//		{
//			/*If the column is cached then it is not an interesting attibute for the positional map*/
//			if(getBit(temp->available, i))
//			{
//				interesting_attributes[i] = 0;
//				(*numOfInterestingAtt)--;
//			}
//		}
//		if (qual_attributes[i])
//		{
//			/*If the column is cached then it is not an interesting attibute for the positional map*/
//			if(getBit(temp->available, i))
//			{
//				qual_attributes[i] = 0;
//				(*numOfInterestingQualAtt)--;
//			}
//		}
//	}
//
//	/*Add in the list with qual the attributes to be cached*/
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if(temp->newCachedElements[i])
//		{
//			if (!qual_attributes[i] )//Then it's in interesting attributes
//			{
//				qual_attributes[i] = 1;
//				(*numOfInterestingQualAtt)++;
//
//				interesting_attributes[i] = 0;
//				(*numOfInterestingAtt)--;
//			}
//		}
//	}
//}
//
//
//
//void
//getCacheDatum(long currentTuple, int whichRelation, Datum *values)
//{
//	int whichDescriptor;
//	int whichPosition;
//	int whichAttribute;
//	int temp;
//	int i;
//	long start = ( currentTuple * CHUNK_SIZE);
//
//	Cache* tempCache = &relCache[whichRelation];
//	CacheInfo *cacheInfo = &tempCache->globalCacheInfo;
//
//	for (i = 0; i < tempCache->numOfloadedElements; i++)
//	{
//		whichAttribute = tempCache->loadedElements[i];
//		temp = cacheInfo->inverse_map[whichAttribute];
//		whichDescriptor = cacheInfo->whichLevel[temp];
//		whichPosition = start + cacheInfo->offset[temp];
////		attributes[whichAttribute] = InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition];
//		values[whichAttribute] = tempCache->cachedLevels[whichDescriptor].position[whichPosition];
//	}
//}
//
///*Load values for the where clause*/
//void
//get_whereCacheDatum(long currentTuple, int whichRelation, Datum *values)
//{
//	int whichDescriptor;
//	int whichPosition;
//	int whichAttribute;
//	int temp;
//	int i;
//	long start = ( currentTuple * CHUNK_SIZE);
//
//	Cache* tempCache = &relCache[whichRelation];
//	CacheInfo *cacheInfo = &tempCache->globalCacheInfo;
//
//	for (i = 0; i < tempCache->numOfwhere_loadedElements; i++)
//	{
//		whichAttribute = tempCache->where_loadedElements[i];
//		temp = cacheInfo->inverse_map[whichAttribute];
//		whichDescriptor = cacheInfo->whichLevel[temp];
//		whichPosition = start + cacheInfo->offset[temp];
////		attributes[whichAttribute] = InternalPositionalMap[whichRelation].internalPointers[whichDescriptor].position[whichPosition];
//		values[whichAttribute] = tempCache->cachedLevels[whichDescriptor].position[whichPosition];
//	}
//}
//
//
//
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
//computeWhichColumnsToCache(int cache_ID, int *neededColumns)
//{
//	int i;
//	int *newCachedElements;
//	//cache_ID
//	Cache * temp = &relCache[cache_ID];
//
//	/*List with elements to be cached*/
//	temp->numOfnewCachedElements = 0;
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//		temp->newCachedElements[i] = 0;
//
//	if ( num_cached_columns == -1 )
//	{
//		/*Count needed but not cached columns*/
//		for ( i = 0 ; i < temp->numberOfAttributes; i++)
//		{
//			if( neededColumns[i] )
//				if( !getBit(temp->available,i) )
//				{
//					temp->newCachedElements[i] = 1;
//					temp->numOfnewCachedElements++;
//				}
//		}
//	}
//	else
//	{//Limited budget
//		newCachedElements = applyPolicyCache(&temp->stats, temp->available, neededColumns, temp->numberOfAttributes, temp->newDeletedElements);
//		temp->numOfnewCachedElements = 0;
//		for (i = 0 ; i < temp->numberOfAttributes; i++)
//		{
//			temp->newCachedElements[i] = newCachedElements[i];
//			if ( newCachedElements[i] )
//				temp->numOfnewCachedElements++;
//		}
//		free(newCachedElements);
//		newCachedElements = NULL;
//
//		/* Count the attributes that will be evicted from the cache */
//		temp->numOfnewDeletedElements = 0;
//		for (i = 0 ; i < temp->numberOfAttributes; i++)
//		{
//			if ( temp->newDeletedElements[i] )
//			{
//				temp->numOfnewDeletedElements++;
//				setBitValue(temp->available, i, 0);
//			}
//		}
////		printUsageList(temp->stats);
//	}
//}
//
//
///*
// * Organize the columns we will access
// * */
//void
//computeWhichColumnsToLoad(int cache_ID, int *neededColumns, int* qualColumns)
//{
//	int i = 0;
//	int pos;
//	Cache * temp = &relCache[cache_ID];
//
//	/*Count needed but not cached columns*/
//	pos = 0;
//	temp->numOfloadedElements = 0;
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if( neededColumns[i] && !qualColumns[i])
//			if( getBit(temp->available,i) )
//			{
//				temp->loadedElements[pos++] = i;
//				temp->numOfloadedElements++;
//			}
//	}
//
//	pos = 0;
//	temp->numOfwhere_loadedElements = 0;
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if( qualColumns[i] )
//			if( getBit(temp->available,i) )
//			{
//				temp->where_loadedElements[pos++] = i;
//				temp->numOfwhere_loadedElements++;
//			}
//	}
//}
//
//void
//updateDataTransformLists(int cache_ID, int* interesting_attributes, int* qual_attributes, int* numOfQualAtt, int* numOfInterestingAtt)
//{
//
//	int i;
//
//	Cache *temp = &relCache[cache_ID];
//
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if (temp->newCachedElements[i] && !qual_attributes[i])
//		{
//			qual_attributes[i] = 1;
//			(*numOfQualAtt)++;
//		}
//	}
//
//	for ( i = 0 ; i < temp->numberOfAttributes; i++)
//	{
//		if (temp->newCachedElements[i] && interesting_attributes[i])
//		{
//			interesting_attributes[i] = 0;
//			(*numOfInterestingAtt)--;
//		}
//	}
//}
//
//
//
//void
//printRelCacheTotal(int cache_ID)
//{
//	int i;
//	Cache *temp = &relCache[cache_ID];
//
//	fprintf(stderr,"\n---------Cache_ID = %d-----------\n",cache_ID);
//
//	fprintf(stderr,"initialized = %d\n",temp->initialized);
//	fprintf(stderr,"isReady = %d\n",temp->isReady);
//	fprintf(stderr,"Budget = %d\n",temp->stats.budget);
//	fprintf(stderr,"numberOfTuples = %ld\n",temp->numberOfTuples);
//	fprintf(stderr,"Attributes = %d\n",temp->numberOfAttributes);
//	fprintf(stderr,"Levels = %d\n",temp->numberOfLevels);
//	fprintf(stderr,"ActivePointers = %d\n",temp->activePointers);
//
//	printBitMap(*(temp->available));
////	printRelCacheInfo(temp->globalCacheInfo, temp->numberOfAttributes);
////	printRelCacheInfo(temp->collectCacheInfo, temp->numberOfAttributes);
//
//	fprintf(stderr,"newCachedElements = {%d}: { ",temp->numOfnewCachedElements);
//	for ( i = 0; i < temp->numberOfAttributes; i++ )
//		fprintf(stderr,"%d ",temp->newCachedElements[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"loadedElements = {%d}: { ",temp->numOfloadedElements);
//	for ( i = 0; i < temp->numOfloadedElements; i++ )
//		fprintf(stderr,"%d ",temp->loadedElements[i]);
//	fprintf(stderr,"}\n");
//
//	fprintf(stderr,"where_loadedElements = {%d}: { ",temp->numOfwhere_loadedElements);
//	for ( i = 0; i < temp->numOfwhere_loadedElements; i++ )
//		fprintf(stderr,"%d ",temp->where_loadedElements[i]);
//	fprintf(stderr,"}\n");
//	fprintf(stderr,"---------------------------------\n");
//}











