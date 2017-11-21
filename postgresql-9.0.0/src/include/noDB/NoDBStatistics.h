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


#ifndef NODBSTATISTICS_H_
#define NODBSTATISTICS_H_

#include "postgres.h"
#include "utils/relcache.h"
#include "snooping/common.h"

#include "noDB/NoDBScan.h"

typedef struct ScalarItem
{
	Datum		value;			/* a data value */
	int			tupno;			/* position index for tuple it came from */
} ScalarItem;

typedef struct TrackItem
{
	Datum		value;
	int			count;
} TrackItem;

//
//typedef struct StatisticMap
//{
//	bool initialized;
//	bool isfirst;
//	BitMap *available;
//	Datum **sampleValues;
//	int *toBeProcessed;
//	int numOftoBeProcessed;
//
//	int *attstattarget;
//
//	int *samplerows;
//	int nrows;
//
//
//	int sampleSize;
//	char relation[MAX_RELATION_NAME];
//} StatisticMap;



void NoDB_analyze_rel(NoDBScanState_t cstate, Relation onerel);





#endif /* NODBSTATISTICS_H_ */
