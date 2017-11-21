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

#ifndef POSITIONALINDEX_H_
#define POSITIONALINDEX_H_


#include "snooping/global.h"
#include "snooping/bitmap.h"
#include "snooping/policy.h"
#include "snooping/common.h"

#include "noDB/NoDBScan.h"

/*
 * Positional Map
 * Data Structure with pointers per Tuple
 *
 * Initially we allocate space for INTERNAL_POINTERS * INIT_TUPLES pointers
 * a) The allocated space is increased for more tuples.
 * b) In case we need more pointers another chunk is allocated
 */
//
//#define INTERNAL_POINTERS 20                   //Chunk size
//#define INIT_TUPLES 1048576                    //1024 x 1024
//#define STEP INTERNAL_POINTERS * INIT_TUPLES   //Alloc step
//#define INIT_NUMBER_OF_STRUCTURES 20           //
//
//
//
////typedef unsigned short int InternalMetaPointer; //[0 - 65535]
//typedef struct InternalMetaPointersDesc
//{
//	InternalMetaPointer *position; //Initialize for: INTERNAL_POINTERS *  INIT_TUPLES
//	int currentSize;
//} InternalMetaPointersDesc;
//
///* PM descriptor*/
//typedef struct PositionalMapInfo
//{
//	int length;
//	int *pointerID;
//	int *pointerPos;
//	int *whichDescriptor;
//	int *offset;
//	int *inverse_map;
//
//	int *neededMapList; /*Pairs: {id, desc_id, offset}	*/
//	int *defaultneededMapList;
//
//} PositionalMapInfo;
//
//typedef struct PositionalMap
//{
//	bool initialized;
//	bool isReady;
//	bool first;
//
//	UsageList stats; // Used for LRU
//
//	long numberOfTuples;
//	long numberOfTuplesCur;
//
//	int numberOfAttributes;
//	int numberOfDesc;
//
//	BitMap *available;
//	int *map;
//	int *inverse_map;
//	int activePointers;
//	InternalMetaPointersDesc *internalPointers;
//
//	/*It is used while collecting internal metapointers*/
//	PositionalMapInfo globalMapInfo;
//	/*It is used while collecting internal metapointers*/
//	PositionalMapInfo collectMapInfo;
//
//	int *newMetaPointers; //Store the attributes that will be collected (adaptive mode)
//	int numOfnewMetaPointers;
//
//	int *newDeletedPointers; //Store the attributes that will be collected (adaptive mode)
//	int numOfnewDeletedPointers;
//
//	FILE *fp;
//	char filename[MAX_FILENAME];
//	char relation[MAX_RELATION_NAME];
//
//} PositionalMap;
//
///* Added for precomputing parsing steps */
//typedef struct ParsingParameters
//{
//	int how_many;
//	int attribute_id;
//	int direction;
//	int index;
//} ParsingParameters;
//
//
///* Positional map*/
//extern PositionalMap InternalPositionalMap[NUMBER_OF_RELATIONS];
//extern int internalMap_usedFiles;
//extern int num_internal_metapointers;
//
//void initializeInternalPositionalMapModule(void);
//int getInternalPositionalMap(char *relation);
//int getFreeInternalPositionalMap(void);
//int getPositionalMapID(char *relation);
//
//int initializeInternalPositionalMap(int pos, char *relation, FILE *fp, int numberOfAttributes, int *which);
//
//void createGlobalPositionalMapInfo(int pos);
//void createCollectPositionalMapInfo(int pos);
//
//void initializeGlobalPositionalMapInfo(int pos, int* map, int *inverse_map, int activePointers);
//void initializeCollectPositionalMapInfo(int pos, int* map, int *inverse_map, int activePointers);
//
//void updateGlobalPositionalMapInfo(int pos);
//void updateCollectPositionalMapInfo(int pos);
//
//void prepareGlobalPositionalMapInfo(int internalFilePointers_ID, int *neededMapPositions, int numOfneededMapPositions, int *defaultneededMapPositions, int numOfdefaultneededMapPositions);
//void prepareCollectPositionalMapInfo(int internalFilePointers_ID);
//
//
//
//int updateInternalPositionalMap(int internalFilePointers_ID);
//void printPositionalMapInfo(PositionalMapInfo mapInfo, int numAttrs);
//
//void updateInternalPositionalMapStatus(char *relation);
//void updateInternalPositionalMapStatus2(int pos);
//
//void addInternalMapMetaPointer(long newPosition, int whichRelation, int whichDescriptor, int attributePos, int offset);
//void addInternalMapMetaPointers(InternalMetaPointer *attributeValues, int internalFilePointers_ID, int currentTuple);
//void updateInternalMapMetaPointers(int *attributeValues, int internalFilePointers_ID, int currentTuple);
//
//void tupleProcessed(int whichRelation);
//int getNumberOfTuplesProcessed(int whichRelation);
//
//PositionalMapInfo getGlobalMapInfo(int whichRelation);
//PositionalMapInfo getCollectMapInfo(int whichRelation);
//
//
//BitMap* getBitMap(int whichRelation);
//int* getMetapointerPositions(int whichRelation);
//
//bool isInitializedInternalMapMetaPointers(int pos);
//void setPositionalMapReady(int pos, bool val);
//bool isPositionalMapReady(int pos);
//
//
//
//InternalMetaPointersDesc* getInternalMetaPointersDesc(int whichRelation, int whichDescriptor);
//
//
//int getInternalMapMetaPointer(int whichRelation, int attribute);
//void getInternalNeededMapMetapointers(long currentTuple, int whichRelation, int * attributes, int numOfneededMapPositions);
//void getInternaldefaultNeededMapMetapointers(long currentTuple, int whichRelation, int * attributes, int numOfdefaultneededMapPositions);
//
//int getInternalMapMetapointer(long currentTuple, int whichRelation, int whichAttribute, PositionalMapInfo* mapInfo);
//int* getAvailableInternalMapPointers(int whichRelation);
//
//int* getNeededInterestingMetapointersBefore(int *interesting_attributes, int natts, int* numInterestingPointers);
//int* getNeededInterestingMetapointersAfter(int *interesting_attributes, int natts, int* numInterestingPointers);
//int* getNeededInterestingMetapointersBoth(int *interesting_attributes, int natts, int* numInterestingPointers);
//int* getQualAvailableMetapointers(int *qualAttributes, int natts, BitMap *available, int *count);
//int* getParseNeededMetapointers(unsigned int numOftoBeParsed, unsigned int *toBeParsed, int natts, ParsingParameters *parameters, int *count);
//int* mergeLists(int *list1, int *list2, int natts, int *count);
//int* updateMetapointerLists(int *neededMapPositions, int numOfneededMapPositions, int *defaultneededMapPositions, int numOfdefaultneededMapPositions, int natts);
//
//
//void getAvailableInterestingMetapointers(int* which, int* numInterestingPointers, int natts, BitMap *available, int* count);
//int* getInterestingPositions(int pos, int *interestingMetapointers, int natts, int* numInterestingPositions);
//
//void computeWhichPointersToCollect(int pos, BitMap *availablePointers, int natts, int *neededMetapointers);
//void updateAttributeListUsingPositionaMap(int pos, int *interesting_attributes, int *numOfInterestingAtt, int *qual_attributes);
//
///* For DEBUG */
//void printInternalMapMetadata(void);
//void printPositionalMapTotal(int file_ID);



#endif /* POSITIONALINDEX_H_ */



