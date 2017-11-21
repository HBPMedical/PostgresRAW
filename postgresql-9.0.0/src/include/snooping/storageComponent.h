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

#ifndef STORAGECOMPONENT_H_
#define STORAGECOMPONENT_H_

#include "catalog/pg_attribute.h"
#include "utils/datum.h"


#include "snooping/global.h"
#include "snooping/bitmap.h"
#include "snooping/policy.h"
#include "snooping/common.h"


#define CHUNK_SIZE 20
#define INIT_CHUNKS 1048576 					//1024 x 1024
#define CHUNK_STEP CHUNK_SIZE * INIT_CHUNKS

#define INIT_NUMBER_OF_LEVELS 20


typedef Datum CachedElement; //[0 - 65535]
typedef struct CachedElementsDesc
{
	CachedElement *position; //Initialize for: INIT_CHUNKS *  CHUNK_STEP
	int currentSize;
} CachedDatumDesc;

/* Similar with PM*/
typedef struct CacheInfo
{
	int length;
	int *elementID;
	int *elementPos;
	int *whichLevel;
	int *offset;
	int *inverse_map;
} CacheInfo;

typedef struct Cache
{
	bool initialized;
	bool isReady;
	bool first;

	/*Maximun number of columns that can be cached. If -1, then budget = inf*/
//	int budget;
	UsageList stats;

	long numberOfTuples;
	long numberOfTuplesCur;

	int numberOfAttributes;
	int numberOfLevels;

	BitMap *available;

	int *map;
	int *inverse_map;
	int activePointers;

	CachedDatumDesc *cachedLevels;
	/*It is used while collecting internal metapointers*/
	CacheInfo globalCacheInfo;
	/*It is used while collecting internal metapointers*/
	CacheInfo collectCacheInfo;

	int *newCachedElements; //Store the attributes that will be collected (adaptive mode)
	int numOfnewCachedElements;

	int *newDeletedElements; //Store the attributes that will be collected (adaptive mode)
	int numOfnewDeletedElements;

	int *loadedElements; //Store the attributes that will be loaded from the cache
	int numOfloadedElements;

	int *where_loadedElements; //Store the attributes that will be loaded from the cache for the where clause
	int numOfwhere_loadedElements;


	FILE *fp;
	char filename[MAX_FILENAME];
	char relation[MAX_RELATION_NAME];

} Cache;


extern bool enable_caching;
extern int num_cached_columns;

void initializeStorageComponent(void);
int getRelCache(char *relation);
int getFreeRelCache(void);
int getCacheID(char *relation);
long getNumberOfCachedTuples(int pos);
int initializeRelCache(int pos, char *relation, FILE *fp, int numberOfAttributes, int *list);

void createGlobalRelCacheInfo( int pos );
void createCollectRelCacheInfo( int pos );

void initializeGlobalRelCacheInfo(int pos, int* map, int *inverse_map, int activePointers);
void initializeCollectRelCacheInfo(int pos, int* map, int *inverse_map, int activePointers);

void updateGlobalRelCacheInfo(int pos);
void updateCollectRelCacheInfo(int pos);
void updateRelCache(int cache_ID, Form_pg_attribute *attr);

void updateRelCacheStatus(char *relation);
void updateRelCacheStatus2(int pos);

void printRelCacheInfo(CacheInfo cacheInfo, int numAttrs);
//void addCacheDatum(CachedElement *attributeElements, int cacheFile_ID, int currentTuple);
void addCacheDatum(CachedElement *attributeElements, Form_pg_attribute *attr, int cacheFile_ID, long currentTuple);
void updateCacheDatum(CachedElement *attributeElements, int cacheFile_ID, long currentTuple);

void tupleCacheProcessed(int cache_ID);

CacheInfo getGlobalCacheInfo(int whichLevel);
CacheInfo getCollectCacheInfo(int whichLevel);

bool isInitializedRelCache(int pos);
void setRelCacheReady(int pos, bool val);
bool isRelCacheReady(int pos);


int* copyList(int *list, int natts);
void getCacheDatum(long currentTuple, int whichRelation, Datum *values);
void get_whereCacheDatum(long currentTuple, int whichRelation, Datum *values);
void updateInterestingAttributesUsingCache(int cacheFile_ID, int *interesting_attributes, int *numOfInterestingAtt);
void updateAttributeListUsingCache(int cacheFile_ID, int *interesting_attributes, int *numOfInterestingAtt, int *qual_attributes, int *numOfInterestingQualAtt);

void computeWhichColumnsToCache(int cache_ID, int *neededColumns);
void computeWhichColumnsToLoad(int cache_ID, int *neededColumns, int* qual_attributes);

void updateDataTransformLists(int cache_ID, int* interesting_attributes, int* qual_attributes, int* numOfQualAtt, int* numOfInterestingAtt);

void printRelCacheTotal(int cache_ID);



#endif /* STORAGECOMPONENT_H_ */




