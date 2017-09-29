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

#ifndef POLICY_H_
#define POLICY_H_


#include "postgres.h"
#include "snooping/bitmap.h"


typedef struct Record
{
	int attributeID;
} Record;

typedef struct UsageList
{
	Record *elements;
	int budget;

	int used_elements;
	int free_elements;

	int top;
	int bottom;
} UsageList;



void initializePolicy(UsageList *list, int budget, int natts);
int insertRecord(UsageList *list, Record newRecord);
void deleteRecord(UsageList *list, Record deleteRecord);
int* firstTimeLRU(UsageList *list, int *interestingAttributes, int natts);
void deleteRecordAtPos(UsageList *list, int pos);
int firstNotNeeded(UsageList *list, int *interestingAttributes, int natts);

void printUsageList(UsageList list);
int* applyPolicyCache(UsageList *list, BitMap *bitmap, int *neededColumns, int natts, int* deletedColumns);
int* applyPolicyPositionalMap(UsageList *list, BitMap *bitmap, int *neededMetapointers, int natts, int *deletedMetapointers);

void updateCounters(UsageList *list, int pos);



#endif /* POLICY_H_ */
