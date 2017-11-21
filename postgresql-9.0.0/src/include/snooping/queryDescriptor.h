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

#ifndef QUERYDESCRIPTOR_H_
#define QUERYDESCRIPTOR_H_

#include "postgres.h"
#include "nodes/nodes.h"
#include "nodes/value.h"
#include "nodes/pg_list.h"

#include "snooping/d_array.h"
#include "snooping/global.h"

#include "access/tupdesc.h"

#include "noDB/auxiliary/NoDBCol.h"



typedef struct AliasMapEntry
{
	char tableName[MAX_RELATION_NAME];
	char aliasName[MAX_RELATION_NAME];
} AliasMapEntry;


typedef struct QueryDescriptor
{
	AliasMapEntry aliasMap[NUMBER_OF_RELATIONS];
	D_Array targetList;
	D_Array groupList;
	D_Array orderByList; //Order By
	D_Array whereList;
	D_Array havingList;
	D_Array filterList;
	D_Array allAttributes;
	long int limit;
} QueryDescriptor;


//TODO: add position in the table (attribute ID ;-)
typedef struct ColumnDescriptor
{
	char *columnName;
	char *tableName;
	char *alias;
	char *schemaName;
	char *catalogName;
	bool whole_row; // true if "*"
} ColumnDescriptor;

typedef enum ParseStatus
{
	PS_idle = 0,
//	PS_fromList,
	PS_targetList,
	PS_groupList,
	PS_orderByList,
	PS_whereList,
	PS_havingList,
	PS_filterList
} ParseStatus;



/*Add functions for initialize the struct...*/
void initializeQueryDescriptor(void);
void resetQueryDescriptor(void);
void changeParseStatus(ParseStatus newStatus);
ParseStatus getParseStatus(void);
void addColumnInformation(char* columnName, char* tableName, char* schemaName, char* catalogName, bool whole_row);
void addTableAliasInformation(char* tableName, char* alias);
char* getTableNameFromAlias(char* alias);
char* getAliasFromTableName(char* tableName);
int hasTableAlias(char* tableName,char* alias);


NoDBColVector_t getQueryFilterAttributes(TupleDesc tupDesc, char *relation);
NoDBColVector_t getQueryAttributes(TupleDesc tupDesc, char *relation);
NoDBColVector_t getQueryRestAttributes(NoDBColVector_t queryAtts, NoDBColVector_t filterAtts);
NoDBColVector_t mergeAttributeVectors(NoDBColVector_t a, NoDBColVector_t b);
void            setQueryLimit(long int limit);
long int        getQueryLimit(void);

void copyColumnDescriptor(ColumnDescriptor *old, ColumnDescriptor *new);
void printQueryDescriptor(void);
void printInterestingAttributes(void);


#endif /* QUERYDESCRIPTOR_H_ */



